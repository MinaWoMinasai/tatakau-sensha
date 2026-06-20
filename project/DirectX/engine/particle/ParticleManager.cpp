#include "ParticleManager.h"
#include "Calculation.h"
#include "TextureManager.h"
#include "DebugCamera.h"
#include "Object3dCommon.h"
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstdio>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

ParticleManager* ParticleManager::GetInstance() {
    static ParticleManager instance;
    return &instance;
}

nlohmann::json ParticleEmitterConfig::ToJson() const {
    return {
        {"position", {position.x, position.y, position.z}},
        {"speedMin", speedMin},
        {"speedMax", speedMax},
        {"lifeTimeMin", lifeTimeMin},
        {"lifeTimeMax", lifeTimeMax},
        {"gravity", {gravity.x, gravity.y, gravity.z}},
        {"startScale", startScale},
        {"endScale", endScale},
        {"startScaleMin", {startScaleMin.x, startScaleMin.y, startScaleMin.z}},
        {"startScaleMax", {startScaleMax.x, startScaleMax.y, startScaleMax.z}},
        {"endScaleMin", {endScaleMin.x, endScaleMin.y, endScaleMin.z}},
        {"endScaleMax", {endScaleMax.x, endScaleMax.y, endScaleMax.z}},
        {"startColor", {startColor.x, startColor.y, startColor.z, startColor.w}},
        {"endColor", {endColor.x, endColor.y, endColor.z, endColor.w}},
        {"modelPath", modelPath},
        {"emitterShape", static_cast<uint32_t>(emitterShape)},
        {"shapeSize", {shapeSize.x, shapeSize.y, shapeSize.z}},
        {"easingType", static_cast<uint32_t>(easingType)},
        {"isBillboard", isBillboard},
        {"alignToVelocity", alignToVelocity}
    };
}

void ParticleEmitterConfig::FromJson(const nlohmann::json& j) {
    auto readVector3 = [](const nlohmann::json& value, const Vector3& fallback) {
        if (!value.is_array() || value.size() < 3) {
            return fallback;
        }
        return Vector3{ value[0].get<float>(), value[1].get<float>(), value[2].get<float>() };
    };
    auto readVector4 = [](const nlohmann::json& value, const Vector4& fallback) {
        if (!value.is_array() || value.size() < 4) {
            return fallback;
        }
        return Vector4{ value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>() };
    };

    position = readVector3(j.value("position", nlohmann::json::array()), position);
    speedMin = j.value("speedMin", speedMin);
    speedMax = j.value("speedMax", speedMax);
    lifeTimeMin = j.value("lifeTimeMin", lifeTimeMin);
    lifeTimeMax = j.value("lifeTimeMax", lifeTimeMax);
    gravity = readVector3(j.value("gravity", nlohmann::json::array()), gravity);
    startScale = j.value("startScale", startScale);
    endScale = j.value("endScale", endScale);
    startScaleMin = readVector3(j.value("startScaleMin", nlohmann::json::array()), Vector3{ startScale, startScale, startScale });
    startScaleMax = readVector3(j.value("startScaleMax", nlohmann::json::array()), Vector3{ startScale, startScale, startScale });
    endScaleMin = readVector3(j.value("endScaleMin", nlohmann::json::array()), Vector3{ endScale, endScale, endScale });
    endScaleMax = readVector3(j.value("endScaleMax", nlohmann::json::array()), Vector3{ endScale, endScale, endScale });
    startColor = readVector4(j.value("startColor", nlohmann::json::array()), startColor);
    endColor = readVector4(j.value("endColor", nlohmann::json::array()), endColor);
    modelPath = j.value("modelPath", modelPath);
    emitterShape = static_cast<EmitterShape>(j.value("emitterShape", static_cast<uint32_t>(emitterShape)));
    shapeSize = readVector3(j.value("shapeSize", nlohmann::json::array()), shapeSize);
    easingType = static_cast<EasingType>(j.value("easingType", static_cast<uint32_t>(easingType)));
    isBillboard = j.value("isBillboard", isBillboard);
    alignToVelocity = j.value("alignToVelocity", alignToVelocity);
}

void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    auto device = dxCommon_->GetDevice();

    // 全パーティクルを同じGPU間接描画にまとめるため、ネオン枠線モデルに統一する。
    ModelManager::GetInstance()->LoadModel("plane.obj");
    ModelManager::GetInstance()->LoadModel("triangleParticle.obj");
    ModelManager::GetInstance()->LoadModel("neonTriangleParticle.obj");
    model_ = ModelManager::GetInstance()->FindModel("neonTriangleParticle.obj");

    // 1. インスタンシング用。CPU更新した行列を毎フレーム書き込むためUploadにする。
    instancingResource_ = dxCommon_->CreateBufferResource(sizeof(ModelParticleTransformationMatrix) * kMaxInstance);
    instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instancingData_));

    srvIndexInstancing_ = srvManager_->Allocate();
    srvManager_->CreateSRVforStructuredBuffer(srvIndexInstancing_, instancingResource_.Get(), kMaxInstance, sizeof(ModelParticleTransformationMatrix));

    gpuInstancingResource_ = dxCommon_->CreateUAVBufferResource(sizeof(ModelParticleTransformationMatrix) * kMaxInstance, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    uavIndexRenderData_ = srvManager_->Allocate();
    srvManager_->CreateUAVforStructuredBuffer(uavIndexRenderData_, gpuInstancingResource_.Get(), kMaxInstance, sizeof(ModelParticleTransformationMatrix));
    srvIndexGpuInstancing_ = srvManager_->Allocate();
    srvManager_->CreateSRVforStructuredBuffer(srvIndexGpuInstancing_, gpuInstancingResource_.Get(), kMaxInstance, sizeof(ModelParticleTransformationMatrix));

    materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = false;
    materialData_->lightingMode = false;
    materialData_->environmentCoefficient = 0.0f;
    materialData_->padding = 0.0f;
    materialData_->uvTransform = MakeIdentity4x4();
    materialData_->shininess = 1.0f;

    directionalLightResource_ = dxCommon_->CreateBufferResource(sizeof(DirectionalLight));
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));
    directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
    directionalLightData_->intensity = 1.0f;

    cameraResource_ = dxCommon_->CreateBufferResource(sizeof(CameraData));
    cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));
    cameraData_->worldPosition = { 0.0f, 0.0f, 0.0f };
    cameraData_->padding = 0.0f;

    // 2. 物理バッファ (Particle Data)
    particleResource_ = dxCommon_->CreateUAVBufferResource(sizeof(ParticleGPU) * kMaxInstance, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    uavIndexParticles_ = srvManager_->Allocate();
    srvManager_->CreateUAVforStructuredBuffer(uavIndexParticles_, particleResource_.Get(), kMaxInstance, sizeof(ParticleGPU));

    // 3. 間接描画 (Indirect Args)
    drawArgsResource_ = dxCommon_->CreateUAVBufferResource(sizeof(D3D12_DRAW_ARGUMENTS), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    uavIndexDrawArgs_ = srvManager_->Allocate();
    srvManager_->CreateUAVforRawBuffer(uavIndexDrawArgs_, drawArgsResource_.Get());

    // 4. 生存インデックス
    aliveIndicesResource_ = dxCommon_->CreateUAVBufferResource(sizeof(uint32_t) * kMaxInstance, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    uavIndexAliveIndices_ = srvManager_->Allocate();
    srvManager_->CreateUAVforStructuredBuffer(uavIndexAliveIndices_, aliveIndicesResource_.Get(), kMaxInstance, sizeof(uint32_t));

    // 5. 定数バッファ
    computeConfigResource_ = dxCommon_->CreateBufferResource(sizeof(GlobalConfig));
    computeSceneResource_ = dxCommon_->CreateBufferResource(sizeof(SceneConfig));
    emitStagingResource_ = dxCommon_->CreateBufferResource(sizeof(ParticleGPU) * kMaxInstance);

    // Default heapの初期内容は未定義なので、GPU更新を始める前に全スロットを無効化する。
    void* initialParticles = nullptr;
    emitStagingResource_->Map(0, nullptr, &initialParticles);
    std::memset(initialParticles, 0, sizeof(ParticleGPU) * kMaxInstance);
    emitStagingResource_->Unmap(0, nullptr);
    Transition(particleResource_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    dxCommon_->GetList()->CopyBufferRegion(
        particleResource_.Get(), 0, emitStagingResource_.Get(), 0,
        sizeof(ParticleGPU) * kMaxInstance);
    Transition(particleResource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    resetResource_ = dxCommon_->CreateBufferResource(sizeof(uint32_t));
    uint32_t zero = 0;
    void* pReset = nullptr;
    resetResource_->Map(0, nullptr, &pReset);
    memcpy(pReset, &zero, 4);
    resetResource_->Unmap(0, nullptr);

    drawArgsInitResource_ = dxCommon_->CreateBufferResource(sizeof(D3D12_DRAW_ARGUMENTS));
    D3D12_DRAW_ARGUMENTS* drawArgs = nullptr;
    drawArgsInitResource_->Map(0, nullptr, reinterpret_cast<void**>(&drawArgs));
    drawArgs->VertexCountPerInstance = static_cast<UINT>(model_->GetModelData().vertices.size());
    drawArgs->InstanceCount = 0;
    drawArgs->StartVertexLocation = 0;
    drawArgs->StartInstanceLocation = 0;
    drawArgsInitResource_->Unmap(0, nullptr);
    ResetDrawArgs();
    Transition(gpuInstancingResource_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Transition(aliveIndicesResource_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    // コマンドシグネチャ
    D3D12_INDIRECT_ARGUMENT_DESC argDesc = {};
    argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
    D3D12_COMMAND_SIGNATURE_DESC sigDesc{};
    sigDesc.ByteStride = static_cast<UINT>(sizeof(D3D12_DRAW_ARGUMENTS));
    sigDesc.NumArgumentDescs = 1;
    sigDesc.pArgumentDescs = &argDesc;
    device->CreateCommandSignature(&sigDesc, nullptr, IID_PPV_ARGS(&commandSignature_));

    CreateDefaultEffects();
    editorConfig_ = effectLibrary_["HitSpark"];
}

float ParticleManager::ApplyEasing(float t, EasingType type) const {
    t = std::clamp(t, 0.0f, 1.0f);
    switch (type) {
    case EasingType::EaseIn:
        return t * t;
    case EasingType::EaseOut:
        return 1.0f - ((1.0f - t) * (1.0f - t));
    case EasingType::Linear:
    default:
        return t;
    }
}

Vector4 ParticleManager::LerpColor(const Vector4& a, const Vector4& b, float t) const {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t,
    };
}

Vector3 ParticleManager::LerpVector3(const Vector3& a, const Vector3& b, float t) const {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
    };
}

Matrix4x4 ParticleManager::MakeBillboardMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate, Camera* camera, DebugCamera* debugCamera) const {
    Vector3 cameraPos = camera ? camera->GetTranslate() : Vector3{ 0.0f, 0.0f, -1.0f };
    if (debugCamera) {
        cameraPos = debugCamera->GetEyePosition();
    }

    Vector3 toCamera = cameraPos - translate;
    if (Length(toCamera) < 0.0001f) {
        toCamera = { 0.0f, 0.0f, -1.0f };
    }
    Vector3 forward = Normalize(toCamera);
    Vector3 up = { 0.0f, 1.0f, 0.0f };
    if (std::fabs(Dot(forward, up)) > 0.98f) {
        up = { 1.0f, 0.0f, 0.0f };
    }
    Vector3 right = Normalize(Cross(forward, up));
    up = Normalize(Cross(right, forward));

    const float c = std::cos(rotate.z);
    const float s = std::sin(rotate.z);
    Vector3 spunRight = right * c + up * s;
    Vector3 spunUp = up * c - right * s;

    Matrix4x4 result = MakeIdentity4x4();
    result.m[0][0] = spunRight.x * scale.x;
    result.m[0][1] = spunRight.y * scale.x;
    result.m[0][2] = spunRight.z * scale.x;
    result.m[1][0] = spunUp.x * scale.y;
    result.m[1][1] = spunUp.y * scale.y;
    result.m[1][2] = spunUp.z * scale.y;
    result.m[2][0] = forward.x * scale.z;
    result.m[2][1] = forward.y * scale.z;
    result.m[2][2] = forward.z * scale.z;
    result.m[3][0] = translate.x;
    result.m[3][1] = translate.y;
    result.m[3][2] = translate.z;
    return result;
}

void ParticleManager::Update(float deltaTime, Camera* camera, DebugCamera* debugCamera) {
    if (!camera) {
        return;
    }

	UpdatePendingDeathBursts(deltaTime);

    const bool useDebugCamera = debugCamera && Object3dCommon::GetInstance()->GetIsDebugCamera();
    const Matrix4x4& viewProjection = useDebugCamera ? debugCamera->GetViewProjectionMatrix() : camera->GetViewProjectionMatrix();
    const Vector3 cameraPosition = useDebugCamera ? debugCamera->GetEyePosition() : camera->GetTranslate();

    if (cameraData_) {
        cameraData_->worldPosition = cameraPosition;
    }

    if (useGpuUpdate_) {
        DispatchGpu(deltaTime, viewProjection, cameraPosition);
        return;
    }

    if (!instancingData_) {
        return;
    }

    for (auto& active : activeParticles_) {
        Particle& particle = active.particle;
        particle.currentTime += deltaTime;
        particle.velocity += particle.acceleration * deltaTime;
        particle.transform.translate += particle.velocity * deltaTime;
        particle.transform.rotate += particle.angularVelocity * deltaTime;
    }

    std::erase_if(activeParticles_, [](const ActiveParticle& active) {
        return active.particle.currentTime >= active.particle.lifeTime;
    });

    instanceCount_ = 0;
    for (const auto& active : activeParticles_) {
        if (instanceCount_ >= kMaxInstance) {
            break;
        }

        const Particle& particle = active.particle;
        float t = particle.lifeTime > 0.0f ? particle.currentTime / particle.lifeTime : 1.0f;
        float easedT = ApplyEasing(t, particle.easingType);
        Vector3 scale = LerpVector3(particle.startScaleVector, particle.endScaleVector, easedT);
        Vector4 color = LerpColor(particle.startColor, particle.endColor, easedT);

        Vector3 rotate = particle.transform.rotate;
        if (particle.isBillboard && particle.alignToVelocity && Length(particle.velocity) > 0.0001f) {
            Vector3 cameraPos = cameraPosition;
            Vector3 toCamera = cameraPos - particle.transform.translate;
            if (Length(toCamera) < 0.0001f) {
                toCamera = { 0.0f, 0.0f, -1.0f };
            }

            Vector3 forward = Normalize(toCamera);
            Vector3 up = { 0.0f, 1.0f, 0.0f };
            if (std::fabs(Dot(forward, up)) > 0.98f) {
                up = { 1.0f, 0.0f, 0.0f };
            }
            Vector3 right = Normalize(Cross(forward, up));
            up = Normalize(Cross(right, forward));

            float velocityX = Dot(particle.velocity, right);
            float velocityY = Dot(particle.velocity, up);
            if (std::fabs(velocityX) > 0.0001f || std::fabs(velocityY) > 0.0001f) {
                rotate.z = std::atan2(velocityY, velocityX) - (pi * 0.5f);
            }
        }

        Matrix4x4 worldMatrix = particle.isBillboard
            ? MakeBillboardMatrix(scale, rotate, particle.transform.translate, camera, useDebugCamera ? debugCamera : nullptr)
            : MakeAffineMatrix(scale, rotate, particle.transform.translate);
        Matrix4x4 worldInverseTransposeMatrix = Transpose(Inverse(worldMatrix));
        Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, viewProjection);

        instancingData_[instanceCount_].WVP = worldViewProjectionMatrix;
        instancingData_[instanceCount_].World = worldMatrix;
        instancingData_[instanceCount_].WorldInverseTranspose = worldInverseTransposeMatrix;
        instancingData_[instanceCount_].color = color;
        ++instanceCount_;
    }
}

void ParticleManager::Dispatch(float deltaTime, Camera* camera) {
    if (!dxCommon_ || !srvManager_ || !camera) {
        return;
    }

    auto commandList = dxCommon_->GetList();
    // カウンターリセット
    commandList->CopyBufferRegion(drawArgsResource_.Get(), 4, resetResource_.Get(), 0, 4);

    // 定数バッファ更新
    GlobalConfig* conf;
    computeConfigResource_->Map(0, nullptr, (void**)&conf);
    conf->deltaTime = deltaTime;
    conf->maxParticles = kMaxInstance;
    computeConfigResource_->Unmap(0, nullptr);

    SceneConfig* scene;
    computeSceneResource_->Map(0, nullptr, (void**)&scene);
    scene->viewProjection = camera->GetViewProjectionMatrix();
    scene->cameraPosition = camera->GetTranslate();
    computeSceneResource_->Unmap(0, nullptr);

    srvManager_->PreDraw();
    // Compute PSO/root signature is not wired into DirectXCommon yet.
    // Keep Update safe while the GPU particle path is being migrated.
}

void ParticleManager::DispatchGpu(float deltaTime, const Matrix4x4& viewProjection, const Vector3& cameraPosition) {
    if (!dxCommon_ || !srvManager_) {
        return;
    }

    auto commandList = dxCommon_->GetList();

    Transition(drawArgsResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
    commandList->CopyBufferRegion(drawArgsResource_.Get(), 4, resetResource_.Get(), 0, sizeof(uint32_t));
    Transition(drawArgsResource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    GlobalConfig* config = nullptr;
    computeConfigResource_->Map(0, nullptr, reinterpret_cast<void**>(&config));
    config->deltaTime = deltaTime;
    config->maxParticles = kMaxInstance;
    computeConfigResource_->Unmap(0, nullptr);

    SceneConfig* scene = nullptr;
    computeSceneResource_->Map(0, nullptr, reinterpret_cast<void**>(&scene));
    scene->viewProjection = viewProjection;
    scene->cameraPosition = cameraPosition;
    scene->scenePadding = 0.0f;
    computeSceneResource_->Unmap(0, nullptr);

    srvManager_->PreDraw();
    auto& pso = dxCommon_->GetPSOComputeParticle();
    commandList->SetComputeRootSignature(pso.root_.GetSignature().Get());
    commandList->SetPipelineState(pso.computeState_.Get());
    commandList->SetComputeRootConstantBufferView(0, computeConfigResource_->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(1, computeSceneResource_->GetGPUVirtualAddress());
    commandList->SetComputeRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(uavIndexParticles_));
    commandList->SetComputeRootDescriptorTable(3, srvManager_->GetGPUDescriptorHandle(uavIndexRenderData_));
    commandList->SetComputeRootDescriptorTable(4, srvManager_->GetGPUDescriptorHandle(uavIndexAliveIndices_));
    commandList->SetComputeRootDescriptorTable(5, srvManager_->GetGPUDescriptorHandle(uavIndexDrawArgs_));

    commandList->Dispatch((kMaxInstance + 1023) / 1024, 1, 1);

    D3D12_RESOURCE_BARRIER uavBarriers[3]{};
    uavBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarriers[0].UAV.pResource = particleResource_.Get();
    uavBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarriers[1].UAV.pResource = gpuInstancingResource_.Get();
    uavBarriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarriers[2].UAV.pResource = drawArgsResource_.Get();
    commandList->ResourceBarrier(3, uavBarriers);

    Transition(gpuInstancingResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Transition(drawArgsResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    gpuDrawReady_ = true;
}

void ParticleManager::Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    if (!resource || before == after) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    dxCommon_->GetList()->ResourceBarrier(1, &barrier);
}

void ParticleManager::ResetDrawArgs() {
    auto commandList = dxCommon_->GetList();
    Transition(drawArgsResource_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    commandList->CopyBufferRegion(drawArgsResource_.Get(), 0, drawArgsInitResource_.Get(), 0, sizeof(D3D12_DRAW_ARGUMENTS));
    Transition(drawArgsResource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void ParticleManager::RegisterEffect(const std::string& effectName, const std::string& jsonPath) {
    ParticleEmitterConfig config;
    std::ifstream file(jsonPath);
    if (file) {
        nlohmann::json json;
        file >> json;
        config.FromJson(json);
    }
    effectLibrary_[effectName] = config;
}

void ParticleManager::RegisterEffect(const std::string& effectName, const ParticleEmitterConfig& config) {
    effectLibrary_[effectName] = config;
}

void ParticleManager::SaveEffect(const std::string& effectName, const std::string& jsonPath) const {
    auto it = effectLibrary_.find(effectName);
    if (it == effectLibrary_.end()) {
        return;
    }

    std::ofstream file(jsonPath);
    if (!file) {
        return;
    }
    file << it->second.ToJson().dump(4);
}

void ParticleManager::Emit(const std::string& effectName, const Vector3& position, uint32_t count) {
    if (effectLibrary_.find(effectName) == effectLibrary_.end()) return;
    auto& config = effectLibrary_[effectName];
    config.position = position;

    std::vector<Particle> particles;
    for (uint32_t i = 0; i < count; ++i) particles.push_back(MakeParticle(config));
    EmitBatch(particles);
}

void ParticleManager::EmitHitEffect(const Vector3& position) {
    auto it = effectLibrary_.find("HitSpark");
    if (it == effectLibrary_.end()) {
        return;
    }

    ParticleEmitterConfig config = it->second;
    config.position = position;

    std::vector<Particle> particles;
    constexpr uint32_t kSparkCount = 18;
    particles.reserve(kSparkCount);

    for (uint32_t i = 0; i < kSparkCount; ++i) {
        float angle = (2.0f * pi * static_cast<float>(i) / static_cast<float>(kSparkCount)) + Rand(-0.18f, 0.18f);
        Vector3 radial = { std::cos(angle), std::sin(angle), 0.0f };
        Vector3 dir = Normalize(radial * Rand(0.45f, 1.0f) + RandomUnitVector() * Rand(0.55f, 1.15f));
        float speed = Rand(config.speedMin, config.speedMax);

        Particle particle = MakeParticle(config);
        particle.transform.translate = position + Rand(Vector3{ -0.18f, -0.18f, -0.04f }, Vector3{ 0.18f, 0.18f, 0.04f });
        particle.transform.rotate = { 0.0f, 0.0f, angle - (pi * 0.5f) };
        particle.velocity = dir * speed;
        particle.startScaleVector = Rand(config.startScaleMin, config.startScaleMax);
        particle.endScaleVector = Rand(config.endScaleMin, config.endScaleMax);
        particle.alignToVelocity = config.alignToVelocity;
        particles.push_back(particle);
    }

    EmitBatch(particles);
}

void ParticleManager::EmitNeonDeathEffect(
    const Vector3& position,
    const Vector4& primaryColor,
    const Vector4& secondaryColor,
    float strength) {
    strength = std::clamp(strength, 0.2f, 2.0f);

	if (strength >= 0.75f) {
		auto chargeIt = effectLibrary_.find("NeonDeathCharge");
		if (chargeIt != effectLibrary_.end()) {
			ParticleEmitterConfig charge = chargeIt->second;
			charge.position = position;
			const float chargeScale = 0.75f + strength * 0.35f;
			charge.startScaleMin *= chargeScale;
			charge.startScaleMax *= chargeScale;
			charge.endScaleMin *= chargeScale;
			charge.endScaleMax *= chargeScale;
			std::vector<Particle> chargeParticles;
			const uint32_t chargeCount = static_cast<uint32_t>(std::lround(5.0f * strength));
			chargeParticles.reserve(chargeCount);
			for (uint32_t i = 0; i < chargeCount; ++i) {
				Particle particle = MakeParticle(charge);
				particle.transform.rotate.z = (2.0f * pi * static_cast<float>(i)) /
					static_cast<float>((std::max)(1u, chargeCount));
				particle.angularVelocity = { 0.0f, 0.0f, (i % 2 == 0 ? 1.0f : -1.0f) * Rand(2.0f, 5.0f) };
				chargeParticles.push_back(particle);
			}
			EmitBatch(chargeParticles);
		}

		pendingDeathBursts_.push_back({ position, primaryColor, secondaryColor, strength, 0.28f });
		return;
	}

	EmitNeonDeathBurstNow(position, primaryColor, secondaryColor, strength);
}

void ParticleManager::EmitNeonDeathBurstNow(
	const Vector3& position,
	const Vector4& primaryColor,
	const Vector4& secondaryColor,
	float strength) {

    std::vector<Particle> particles;
    auto emitLayer = [&](const char* effectName, uint32_t baseCount, float scaleMultiplier) {
        auto it = effectLibrary_.find(effectName);
        if (it == effectLibrary_.end()) {
            return;
        }

        ParticleEmitterConfig config = it->second;
        config.position = position;
        config.shapeSize *= scaleMultiplier;
        config.startScaleMin *= scaleMultiplier;
        config.startScaleMax *= scaleMultiplier;
        config.endScaleMin *= scaleMultiplier;
        config.endScaleMax *= scaleMultiplier;
        config.startColor = primaryColor;
        config.endColor = secondaryColor;

        const uint32_t count = (std::max)(1u, static_cast<uint32_t>(std::lround(baseCount * strength)));
        particles.reserve(particles.size() + count);
        for (uint32_t i = 0; i < count; ++i) {
            Particle particle = MakeParticle(config);
            particle.angularVelocity *= Rand(0.75f, 1.65f);
            particles.push_back(particle);
        }
    };

    const float scaleMultiplier = 0.72f + strength * 0.38f;
    emitLayer("NeonDeathFlash", 7, scaleMultiplier);
    emitLayer("NeonDeathShard", 54, scaleMultiplier);
    emitLayer("NeonDeathFragment", 20, scaleMultiplier);
    EmitBatch(particles);
}

void ParticleManager::UpdatePendingDeathBursts(float deltaTime) {
	for (PendingDeathBurst& pending : pendingDeathBursts_) {
		pending.timer -= deltaTime;
		if (pending.timer <= 0.0f) {
			EmitNeonDeathBurstNow(
				pending.position,
				pending.primaryColor,
				pending.secondaryColor,
				pending.strength);
			screenPulseEvents_.push_back({ pending.position, pending.strength });
		}
	}

	std::erase_if(pendingDeathBursts_, [](const PendingDeathBurst& pending) {
		return pending.timer <= 0.0f;
	});
}

std::vector<ParticleManager::ScreenPulseEvent> ParticleManager::ConsumeScreenPulseEvents() {
	std::vector<ScreenPulseEvent> events = std::move(screenPulseEvents_);
	screenPulseEvents_.clear();
	return events;
}

void ParticleManager::EmitNeonImpactEffect(
    const Vector3& position,
    const Vector3& impactNormal,
    const Vector4& color,
    uint32_t count) {
    auto it = effectLibrary_.find("NeonImpactSpark");
    if (it == effectLibrary_.end() || count == 0) {
        return;
    }

    ParticleEmitterConfig config = it->second;
    config.position = position;
    config.startColor = color;
    config.endColor = { color.x * 0.35f, color.y * 0.35f, color.z * 0.35f, 0.0f };

    Vector3 normal = impactNormal;
    normal.z = 0.0f;
    if (Length(normal) < 0.0001f) {
        normal = { 1.0f, 0.0f, 0.0f };
    } else {
        normal = Normalize(normal);
    }
    const Vector3 tangent = { -normal.y, normal.x, 0.0f };

    std::vector<Particle> particles;
    particles.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        Particle particle = MakeParticle(config);
        Vector3 direction = normal * Rand(0.55f, 1.35f) +
            tangent * Rand(-0.95f, 0.95f) + Vector3{ 0.0f, 0.0f, Rand(-0.18f, 0.18f) };
        direction = Normalize(direction);
        particle.velocity = direction * Rand(config.speedMin, config.speedMax);
        particle.transform.translate += direction * Rand(0.0f, 0.18f);
        particle.transform.rotate.z = std::atan2(direction.y, direction.x) + Rand(-0.6f, 0.6f);
        particle.angularVelocity = { 0.0f, 0.0f, Rand(-12.0f, 12.0f) };
        particles.push_back(particle);
    }
    EmitBatch(particles);
}

void ParticleManager::Emit(const ::Particle& particle) {
    Particle converted{};
    converted.transform = particle.transform;
    converted.startScaleVector = particle.transform.scale;
    converted.endScaleVector = { 0.0f, 0.0f, 0.0f };
    converted.velocity = particle.velocity;
    converted.acceleration = particle.acceleration;
    converted.angularVelocity = particle.angularVelocity;
    converted.lifeTime = particle.lifeTime;
    converted.currentTime = particle.currentTime;
    converted.startScale = particle.transform.scale.x;
    converted.endScale = 0.0f;
    converted.startColor = particle.color;
    converted.endColor = { particle.color.x, particle.color.y, particle.color.z, 0.0f };
    converted.easingType = EasingType::Linear;
    converted.isBillboard = false;
    converted.alignToVelocity = false;

    EmitBatch({ converted });
}

void ParticleManager::EmitBatch(const std::vector<Particle>& particles) {
    if (!useGpuUpdate_) {
        for (const Particle& particle : particles) {
            if (activeParticles_.size() >= kMaxInstance) {
                activeParticles_.erase(activeParticles_.begin());
            }
            activeParticles_.push_back({ particle });
        }
    }

    if (!particleResource_ || !emitStagingResource_ || particles.empty()) {
        return;
    }

    uint32_t count = static_cast<uint32_t>((std::min)(particles.size(), size_t{ 1000 }));
    if (freeIndex_ + count >= kMaxInstance) {
        freeIndex_ = 0;
    }

    void* mappedPtr = nullptr;
    emitStagingResource_->Map(0, nullptr, &mappedPtr);
    ParticleGPU* dest = static_cast<ParticleGPU*>(mappedPtr) + freeIndex_;

    for (uint32_t i = 0; i < count; ++i) {
        const Particle& src = particles[i];
        float startScale = (std::max)({ src.startScaleVector.x, src.startScaleVector.y, src.startScaleVector.z, src.startScale });
        float endScale = (std::max)({ src.endScaleVector.x, src.endScaleVector.y, src.endScaleVector.z, src.endScale });
        dest[i].position = src.transform.translate;
        dest[i].velocity = src.velocity;
        dest[i].acceleration = src.acceleration;
        dest[i].angularVelocity = src.angularVelocity;
        dest[i].currentTime = src.currentTime;
        dest[i].lifeTime = src.lifeTime;
        dest[i].startScale = startScale;
        dest[i].endScale = endScale;
        dest[i].startColor = src.startColor;
        dest[i].endColor = src.endColor;
        dest[i].rotate = src.transform.rotate;
        dest[i].isActive = 1;
        dest[i].easingType = static_cast<uint32_t>(src.easingType);
        dest[i].isBillboard = src.isBillboard ? 1 : 0;
    }
    emitStagingResource_->Unmap(0, nullptr);

    Transition(particleResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
    dxCommon_->GetList()->CopyBufferRegion(
        particleResource_.Get(),
        freeIndex_ * sizeof(ParticleGPU),
        emitStagingResource_.Get(),
        freeIndex_ * sizeof(ParticleGPU),
        count * sizeof(ParticleGPU));
    Transition(particleResource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    freeIndex_ += count;
}

ParticleManager::Particle ParticleManager::MakeParticle(const ParticleEmitterConfig& config) {
    Particle p;
    p.transform.translate = config.position;
    switch (config.emitterShape) {
    case EmitterShape::Sphere:
        p.transform.translate += RandomUnitVector() * Rand(0.0f, config.shapeSize.x);
        break;
    case EmitterShape::Box:
        p.transform.translate += Rand(config.shapeSize * -0.5f, config.shapeSize * 0.5f);
        break;
    case EmitterShape::Point:
    default:
        break;
    }

    p.velocity = Multiply(Rand(config.speedMin, config.speedMax), RandomUnitVector());
    p.acceleration = config.gravity;
    p.angularVelocity = Rand(Vector3{ -5,-5,-5 }, Vector3{ 5,5,5 });
    p.lifeTime = Rand(config.lifeTimeMin, config.lifeTimeMax);
    p.currentTime = 0.0f;
    p.startScale = config.startScale;
    p.endScale = config.endScale;
    p.startScaleVector = Rand(config.startScaleMin, config.startScaleMax);
    p.endScaleVector = Rand(config.endScaleMin, config.endScaleMax);
    p.transform.scale = p.startScaleVector;
    p.transform.rotate = { 0.0f, 0.0f, Rand(0.0f, 2.0f * pi) };
    p.startColor = config.startColor;
    p.endColor = config.endColor;
    p.easingType = config.easingType;
    p.isBillboard = config.isBillboard;
    p.alignToVelocity = config.alignToVelocity;
    if (config.alignToVelocity && Length(p.velocity) > 0.0001f) {
        p.transform.rotate.z = std::atan2(p.velocity.y, p.velocity.x) - (pi * 0.5f);
    }
    return p;
}

void ParticleManager::Draw() {
    if (!dxCommon_ || !srvManager_ || !model_ || instanceCount_ == 0) {
        if (!useGpuUpdate_ || !gpuDrawReady_) {
            return;
        }
    }

    auto commandList = dxCommon_->GetList();
    auto& pso = dxCommon_->GetPSOModelParticle(); // 共通のModelParticlePSOを使用
    commandList->SetGraphicsRootSignature(pso.root_.GetSignature().Get());
    commandList->SetPipelineState(pso.graphicsState_.Get());

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &model_->GetVertexBufferView());

    // RootParameter [0]:Material, [1]:Light, [2]:Camera, [3]:InstancingSRV, [4]:Texture
    // 各エンジンのCBVリソースから取得してセット
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, directionalLightResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(2, cameraResource_->GetGPUVirtualAddress());
    srvManager_->SetGraphicsRootDescriptorTable(3, useGpuUpdate_ ? srvIndexGpuInstancing_ : srvIndexInstancing_);
    commandList->SetGraphicsRootDescriptorTable(4, TextureManager::GetInstance()->GetSrvHandleGPU(model_->GetModelData().material.textureFilePath));

    if (useGpuUpdate_) {
        commandList->ExecuteIndirect(commandSignature_.Get(), 1, drawArgsResource_.Get(), 0, nullptr, 0);
        Transition(gpuInstancingResource_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Transition(drawArgsResource_.Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        gpuDrawReady_ = false;
    } else {
        commandList->DrawInstanced(static_cast<UINT>(model_->GetModelData().vertices.size()), instanceCount_, 0, 0);
    }
}

uint32_t ParticleManager::GetActiveCount() const {
    return static_cast<uint32_t>(activeParticles_.size());
}

void ParticleManager::SetUseGpuUpdate(bool useGpuUpdate) {
    if (useGpuUpdate_ == useGpuUpdate) {
        return;
    }
    useGpuUpdate_ = useGpuUpdate;
    activeParticles_.clear();
    instanceCount_ = 0;
}

void ParticleManager::CreateDefaultEffects() {
    ParticleEmitterConfig hit{};
    hit.speedMin = 18.0f;
    hit.speedMax = 34.0f;
    hit.lifeTimeMin = 0.12f;
    hit.lifeTimeMax = 0.24f;
    hit.gravity = { 0.0f, -8.0f, 0.0f };
    hit.startScale = 1.0f;
    hit.endScale = 0.0f;
    hit.startScaleMin = { 0.10f, 0.10f, 0.10f };
    hit.startScaleMax = { 0.30f, 0.30f, 0.30f };
    hit.endScaleMin = { 0.01f, 0.01f, 0.01f };
    hit.endScaleMax = { 0.04f, 0.04f, 0.04f };
    hit.startColor = { 1.0f, 0.78f, 0.26f, 1.0f };
    hit.endColor = { 1.0f, 0.08f, 0.02f, 0.0f };
    hit.modelPath = "neonTriangleParticle.obj";
    hit.emitterShape = EmitterShape::Point;
    hit.easingType = EasingType::EaseOut;
    hit.isBillboard = true;
    hit.alignToVelocity = true;
    RegisterEffect("HitSpark", hit);

    ParticleEmitterConfig burst{};
    burst.speedMin = 14.0f;
    burst.speedMax = 42.0f;
    burst.lifeTimeMin = 0.35f;
    burst.lifeTimeMax = 0.75f;
    burst.gravity = { 0.0f, -1.2f, 0.0f };
    burst.startScaleMin = { 0.16f, 0.16f, 0.16f };
    burst.startScaleMax = { 0.42f, 0.42f, 0.42f };
    burst.endScaleMin = { 0.02f, 0.02f, 0.02f };
    burst.endScaleMax = { 0.05f, 0.05f, 0.05f };
    burst.startColor = { 0.08f, 1.0f, 0.95f, 1.0f };
    burst.endColor = { 1.0f, 0.10f, 0.85f, 0.0f };
    burst.modelPath = "triangleParticle.obj";
    burst.emitterShape = EmitterShape::Sphere;
    burst.shapeSize = { 0.8f, 0.8f, 0.2f };
    burst.easingType = EasingType::EaseOut;
    burst.isBillboard = false;
    burst.alignToVelocity = false;
    RegisterEffect("PlayerDeathBurst", burst);

    ParticleEmitterConfig debris = burst;
    debris.speedMin = 10.0f;
    debris.speedMax = 30.0f;
    debris.lifeTimeMin = 0.28f;
    debris.lifeTimeMax = 0.62f;
    debris.gravity = { 0.0f, -0.6f, 0.0f };
    debris.startScaleMin = { 0.12f, 0.12f, 0.12f };
    debris.startScaleMax = { 0.34f, 0.34f, 0.34f };
    debris.startColor = { 1.0f, 0.12f, 0.35f, 1.0f };
    debris.endColor = { 0.15f, 0.85f, 1.0f, 0.0f };
    RegisterEffect("EnemyDeathBurst", debris);

	ParticleEmitterConfig deathCharge{};
	deathCharge.speedMin = 0.0f;
	deathCharge.speedMax = 1.5f;
	deathCharge.lifeTimeMin = 0.28f;
	deathCharge.lifeTimeMax = 0.32f;
	deathCharge.gravity = { 0.0f, 0.0f, 0.0f };
	deathCharge.startScaleMin = { 0.28f, 0.28f, 0.28f };
	deathCharge.startScaleMax = { 0.55f, 0.55f, 0.55f };
	deathCharge.endScaleMin = { 2.5f, 2.5f, 2.5f };
	deathCharge.endScaleMax = { 4.0f, 4.0f, 4.0f };
	deathCharge.startColor = { 2.2f, 2.2f, 2.2f, 0.95f };
	deathCharge.endColor = { 1.6f, 1.6f, 1.6f, 0.0f };
	deathCharge.modelPath = "neonTriangleParticle.obj";
	deathCharge.emitterShape = EmitterShape::Sphere;
	deathCharge.shapeSize = { 0.18f, 0.18f, 0.05f };
	deathCharge.easingType = EasingType::EaseOut;
	deathCharge.isBillboard = true;
	RegisterEffect("NeonDeathCharge", deathCharge);

    ParticleEmitterConfig deathFlash{};
    deathFlash.speedMin = 1.5f;
    deathFlash.speedMax = 7.0f;
    deathFlash.lifeTimeMin = 0.16f;
    deathFlash.lifeTimeMax = 0.30f;
    deathFlash.gravity = { 0.0f, 0.0f, 0.0f };
    deathFlash.startScaleMin = { 0.75f, 0.75f, 0.75f };
    deathFlash.startScaleMax = { 1.45f, 1.45f, 1.45f };
    deathFlash.endScaleMin = { 2.8f, 2.8f, 2.8f };
    deathFlash.endScaleMax = { 5.2f, 5.2f, 5.2f };
    deathFlash.modelPath = "neonTriangleParticle.obj";
    deathFlash.emitterShape = EmitterShape::Sphere;
    deathFlash.shapeSize = { 0.35f, 0.35f, 0.12f };
    deathFlash.easingType = EasingType::EaseOut;
    deathFlash.isBillboard = true;
    RegisterEffect("NeonDeathFlash", deathFlash);

    ParticleEmitterConfig deathShard{};
    deathShard.speedMin = 13.0f;
    deathShard.speedMax = 40.0f;
    deathShard.lifeTimeMin = 0.34f;
    deathShard.lifeTimeMax = 0.86f;
    deathShard.gravity = { 0.0f, -1.1f, 0.0f };
    deathShard.startScaleMin = { 0.12f, 0.12f, 0.12f };
    deathShard.startScaleMax = { 0.38f, 0.38f, 0.38f };
    deathShard.endScaleMin = { 0.015f, 0.015f, 0.015f };
    deathShard.endScaleMax = { 0.07f, 0.07f, 0.07f };
    deathShard.modelPath = "neonTriangleParticle.obj";
    deathShard.emitterShape = EmitterShape::Sphere;
    deathShard.shapeSize = { 0.65f, 0.65f, 0.18f };
    deathShard.easingType = EasingType::EaseOut;
    deathShard.isBillboard = false;
    RegisterEffect("NeonDeathShard", deathShard);

    ParticleEmitterConfig deathFragment = deathShard;
    deathFragment.speedMin = 4.0f;
    deathFragment.speedMax = 15.0f;
    deathFragment.lifeTimeMin = 0.72f;
    deathFragment.lifeTimeMax = 1.35f;
    deathFragment.gravity = { 0.0f, -0.45f, 0.0f };
    deathFragment.startScaleMin = { 0.34f, 0.34f, 0.34f };
    deathFragment.startScaleMax = { 0.82f, 0.82f, 0.82f };
    deathFragment.endScaleMin = { 0.05f, 0.05f, 0.05f };
    deathFragment.endScaleMax = { 0.18f, 0.18f, 0.18f };
    deathFragment.shapeSize = { 0.9f, 0.9f, 0.25f };
    RegisterEffect("NeonDeathFragment", deathFragment);

    ParticleEmitterConfig impact{};
    impact.speedMin = 9.0f;
    impact.speedMax = 24.0f;
    impact.lifeTimeMin = 0.14f;
    impact.lifeTimeMax = 0.32f;
    impact.gravity = { 0.0f, -2.0f, 0.0f };
    impact.startScaleMin = { 0.10f, 0.10f, 0.10f };
    impact.startScaleMax = { 0.28f, 0.28f, 0.28f };
    impact.endScaleMin = { 0.01f, 0.01f, 0.01f };
    impact.endScaleMax = { 0.04f, 0.04f, 0.04f };
    impact.modelPath = "neonTriangleParticle.obj";
    impact.emitterShape = EmitterShape::Point;
    impact.easingType = EasingType::EaseOut;
    impact.isBillboard = true;
    RegisterEffect("NeonImpactSpark", impact);

    ParticleEmitterConfig smoke{};
    smoke.speedMin = 1.0f;
    smoke.speedMax = 4.0f;
    smoke.lifeTimeMin = 0.32f;
    smoke.lifeTimeMax = 0.62f;
    smoke.gravity = { 0.0f, 0.4f, 0.0f };
    smoke.startScaleMin = { 0.35f, 0.35f, 0.35f };
    smoke.startScaleMax = { 0.85f, 0.85f, 0.85f };
    smoke.endScaleMin = { 0.9f, 0.9f, 0.9f };
    smoke.endScaleMax = { 1.5f, 1.5f, 1.5f };
    smoke.startColor = { 0.10f, 0.95f, 1.0f, 0.36f };
    smoke.endColor = { 0.95f, 0.05f, 1.0f, 0.0f };
    smoke.modelPath = "plane.obj";
    smoke.emitterShape = EmitterShape::Sphere;
    smoke.shapeSize = { 0.6f, 0.6f, 0.2f };
    smoke.easingType = EasingType::EaseOut;
    smoke.isBillboard = true;
    smoke.alignToVelocity = false;
    RegisterEffect("DeathSmoke", smoke);

    ParticleEmitterConfig dash{};
    dash.speedMin = 1.0f;
    dash.speedMax = 4.0f;
    dash.lifeTimeMin = 0.12f;
    dash.lifeTimeMax = 0.25f;
    dash.gravity = { 0.0f, 0.0f, 0.0f };
    dash.startScaleMin = { 0.08f, 0.08f, 0.08f };
    dash.startScaleMax = { 0.18f, 0.18f, 0.18f };
    dash.endScaleMin = { 0.02f, 0.02f, 0.02f };
    dash.endScaleMax = { 0.04f, 0.04f, 0.04f };
    dash.startColor = { 0.3f, 1.0f, 1.0f, 0.8f };
    dash.endColor = { 0.0f, 0.45f, 0.65f, 0.0f };
    dash.modelPath = "plane.obj";
    dash.emitterShape = EmitterShape::Sphere;
    dash.shapeSize = { 0.4f, 0.4f, 0.1f };
    dash.easingType = EasingType::EaseOut;
    dash.isBillboard = true;
    RegisterEffect("DashDust", dash);

    ParticleEmitterConfig casing{};
    casing.speedMin = 3.0f;
    casing.speedMax = 7.0f;
    casing.lifeTimeMin = 0.25f;
    casing.lifeTimeMax = 0.55f;
    casing.gravity = { 0.0f, -6.0f, 0.0f };
    casing.startScaleMin = { 0.05f, 0.22f, 0.05f };
    casing.startScaleMax = { 0.08f, 0.36f, 0.08f };
    casing.endScaleMin = { 0.02f, 0.04f, 0.02f };
    casing.endScaleMax = { 0.03f, 0.08f, 0.03f };
    casing.startColor = { 1.0f, 0.82f, 0.22f, 1.0f };
    casing.endColor = { 0.9f, 0.25f, 0.03f, 0.0f };
    casing.modelPath = "plane.obj";
    casing.emitterShape = EmitterShape::Point;
    casing.easingType = EasingType::EaseOut;
    casing.isBillboard = true;
    casing.alignToVelocity = true;
    RegisterEffect("CasingSpark", casing);
}

void ParticleManager::DrawImGuiEditor() {
#ifdef USE_IMGUI
    if (ImGui::Begin("Particle Editor")) {
        char nameBuffer[64]{};
        std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", editorEffectName_.c_str());
        if (ImGui::InputText("Effect Name", nameBuffer, sizeof(nameBuffer))) {
            editorEffectName_ = nameBuffer;
        }

        ImGui::Text("Active: %u", GetActiveCount());
        ImGui::Checkbox("GPU Update", &useGpuUpdate_);
        ImGui::DragFloat("Speed Min", &editorConfig_.speedMin, 0.1f, 0.0f, 200.0f);
        ImGui::DragFloat("Speed Max", &editorConfig_.speedMax, 0.1f, 0.0f, 200.0f);
        ImGui::DragFloat("Life Min", &editorConfig_.lifeTimeMin, 0.01f, 0.01f, 10.0f);
        ImGui::DragFloat("Life Max", &editorConfig_.lifeTimeMax, 0.01f, 0.01f, 10.0f);
        ImGui::DragFloat3("Gravity", &editorConfig_.gravity.x, 0.05f);
        ImGui::DragFloat3("Start Scale Min", &editorConfig_.startScaleMin.x, 0.01f, 0.0f, 20.0f);
        ImGui::DragFloat3("Start Scale Max", &editorConfig_.startScaleMax.x, 0.01f, 0.0f, 20.0f);
        ImGui::DragFloat3("End Scale Min", &editorConfig_.endScaleMin.x, 0.01f, 0.0f, 20.0f);
        ImGui::DragFloat3("End Scale Max", &editorConfig_.endScaleMax.x, 0.01f, 0.0f, 20.0f);
        ImGui::ColorEdit4("Start Color", &editorConfig_.startColor.x);
        ImGui::ColorEdit4("End Color", &editorConfig_.endColor.x);
        ImGui::Checkbox("Billboard", &editorConfig_.isBillboard);
        ImGui::Checkbox("Align To Velocity", &editorConfig_.alignToVelocity);

        if (ImGui::Button("Apply")) {
            RegisterEffect(editorEffectName_, editorConfig_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Sample Hit")) {
            RegisterEffect("HitSpark", editorConfig_);
            EmitHitEffect({ 0.0f, 0.0f, 0.0f });
        }
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            RegisterEffect(editorEffectName_, editorConfig_);
            SaveEffect(editorEffectName_, "resources/effects/hit_spark.json");
        }
    }
    ImGui::End();
#endif
}
