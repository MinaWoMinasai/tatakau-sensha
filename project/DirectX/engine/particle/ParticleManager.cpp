#include "ParticleManager.h"
#include "Calculation.h"
#include "TextureManager.h"
#include "DebugCamera.h"
#include "Object3dCommon.h"
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace cg2 {

float ParticleManager::Rand(float min, float max) {
    return std::uniform_real_distribution<float>(min, max)(presentationRandom_);
}
Vector3 ParticleManager::Rand(const Vector3& min, const Vector3& max) {
    return {Rand(min.x, max.x), Rand(min.y, max.y), Rand(min.z, max.z)};
}
Vector3 ParticleManager::RandomUnitVector() {
    const float z = Rand(-1.0f, 1.0f), angle = Rand(0.0f, 2.0f * pi);
    const float radius = std::sqrt((std::max)(0.0f, 1.0f - z * z));
    return {radius * std::cos(angle), radius * std::sin(angle), z};
}

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
        {"alignToVelocity", alignToVelocity},
        {"neonRadiance", neonRadiance},
        {"neonShape", static_cast<uint32_t>(neonShape)}
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
    neonRadiance = j.value("neonRadiance", neonRadiance);
    neonShape = static_cast<NeonParticleShape>((std::min)(5u, j.value("neonShape", static_cast<uint32_t>(neonShape))));
}

void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    auto device = dxCommon_->GetDevice();

    // A padded quad lets the PS integrate the contour at pixel width and draw a
    // soft halo outside the old hard-rasterized, double-sided strip geometry.
    // The triangle itself still has radius one; the padding is visual only.
    ModelData contourData{};
    contourData.vertices = {
        {{-1.22f, -0.86f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{-1.22f,  1.36f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{ 1.22f, -0.86f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{ 1.22f,  1.36f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}
    };
    contourData.indices = {0, 1, 2, 1, 3, 2};
    contourData.material.textureFilePath = "resources/white512x512.png";
    contourModelCommon_ = std::make_unique<ModelCommon>();
    contourModelCommon_->Initialize(dxCommon_);
    contourModel_ = std::make_unique<Model>();
    contourModel_->InitializeFromModelData(contourModelCommon_.get(), contourData);
    model_ = contourModel_.get();

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
    *materialData_ = MakeDefaultMaterial();
    materialData_->shininess = 1.0f;
    // ModelParticle alone interprets Material's reserved byte-28 float as the
    // procedural-frame selector. No shared CB field/offset or root binding changes.
    materialData_->padding = 1.0f;

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

    // GPU FreeList。空きParticle indexの取得・返却をCS内の不可分操作で行う。
    freeListIndexResource_ = dxCommon_->CreateUAVBufferResource(sizeof(int32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    uavIndexFreeListIndex_ = srvManager_->Allocate();
    srvManager_->CreateUAVforStructuredBuffer(uavIndexFreeListIndex_, freeListIndexResource_.Get(), 1, sizeof(int32_t));
    freeListResource_ = dxCommon_->CreateUAVBufferResource(sizeof(uint32_t) * kMaxInstance, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    uavIndexFreeList_ = srvManager_->Allocate();
    srvManager_->CreateUAVforStructuredBuffer(uavIndexFreeList_, freeListResource_.Get(), kMaxInstance, sizeof(uint32_t));

    // 3. 間接描画 (Indirect Args)
    drawArgsResource_ = dxCommon_->CreateUAVBufferResource(sizeof(D3D12_DRAW_INDEXED_ARGUMENTS), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    uavIndexDrawArgs_ = srvManager_->Allocate();
    srvManager_->CreateUAVforRawBuffer(uavIndexDrawArgs_, drawArgsResource_.Get());

    // 4. 生存インデックス
    aliveIndicesResource_ = dxCommon_->CreateUAVBufferResource(sizeof(uint32_t) * kMaxInstance, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    uavIndexAliveIndices_ = srvManager_->Allocate();
    srvManager_->CreateUAVforStructuredBuffer(uavIndexAliveIndices_, aliveIndicesResource_.Get(), kMaxInstance, sizeof(uint32_t));

    // 5. 定数バッファ
    computeConfigResource_ = dxCommon_->CreateBufferResource(sizeof(GlobalConfig));
    computeSceneResource_ = dxCommon_->CreateBufferResource(sizeof(SceneConfig));
    initializeConfigResource_ = dxCommon_->CreateBufferResource(sizeof(GlobalConfig));
    emitterConfigResource_ = dxCommon_->CreateBufferResource(sizeof(GlobalConfig));
    batchConfigResource_ = dxCommon_->CreateBufferResource(sizeof(GlobalConfig));

    emitterRequestResource_ = dxCommon_->CreateBufferResource(sizeof(GpuEmitterRequest) * kMaxEmitRequests);
    srvIndexEmitterRequests_ = srvManager_->Allocate();
    srvManager_->CreateSRVforStructuredBuffer(
        srvIndexEmitterRequests_, emitterRequestResource_.Get(), kMaxEmitRequests, sizeof(GpuEmitterRequest));

    emitStagingResource_ = dxCommon_->CreateBufferResource(sizeof(ParticleGPU) * kMaxInstance);
    srvIndexPrebuiltParticles_ = srvManager_->Allocate();
    srvManager_->CreateSRVforStructuredBuffer(
        srvIndexPrebuiltParticles_, emitStagingResource_.Get(), kMaxInstance, sizeof(ParticleGPU));

    resetResource_ = dxCommon_->CreateBufferResource(sizeof(uint32_t));
    uint32_t zero = 0;
    void* pReset = nullptr;
    resetResource_->Map(0, nullptr, &pReset);
    memcpy(pReset, &zero, 4);
    resetResource_->Unmap(0, nullptr);

    drawArgsInitResource_ = dxCommon_->CreateBufferResource(sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
    D3D12_DRAW_INDEXED_ARGUMENTS* drawArgs = nullptr;
    drawArgsInitResource_->Map(0, nullptr, reinterpret_cast<void**>(&drawArgs));
    drawArgs->IndexCountPerInstance = static_cast<UINT>(model_->GetModelData().indices.size());
    drawArgs->InstanceCount = 0;
    drawArgs->StartIndexLocation = 0;
    drawArgs->BaseVertexLocation = 0;
    drawArgs->StartInstanceLocation = 0;
    drawArgsInitResource_->Unmap(0, nullptr);
    ResetDrawArgs();
    Transition(particleResource_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Transition(freeListIndexResource_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Transition(freeListResource_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Transition(gpuInstancingResource_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Transition(aliveIndicesResource_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    InitializeGpuParticles();

    // コマンドシグネチャ
    D3D12_INDIRECT_ARGUMENT_DESC argDesc = {};
    argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
    D3D12_COMMAND_SIGNATURE_DESC sigDesc{};
    sigDesc.ByteStride = static_cast<UINT>(sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
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
        if (particle.neonRadiance) particle.velocity *= std::exp(-NeonParticleDrag(particle.neonShape) * deltaTime);
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
        const float scaleT = particle.neonRadiance ? t : easedT;
        Vector3 scale = LerpVector3(particle.startScaleVector, particle.endScaleVector, scaleT);
        Vector4 color = LerpColor(particle.startColor, particle.endColor, particle.neonRadiance ? t : easedT);
        if (particle.neonRadiance) {
            const float fade = NeonParticleFade(t, particle.neonShape);
            color.w = particle.startColor.w + (particle.endColor.w - particle.startColor.w) * fade;
        }

        Vector3 rotate = particle.transform.rotate;
        if (particle.isBillboard && (particle.alignToVelocity || (particle.neonRadiance && particle.neonShape == NeonParticleShape::Spark)) && Length(particle.velocity) > 0.0001f) {
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
            Vector3 right = Normalize(Cross(up, forward));
            up = Normalize(Cross(forward, right));

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
        // This homogeneous element is unused by the shader's normal 3x3.
        instancingData_[instanceCount_].WorldInverseTranspose.m[3][3] = particle.neonRadiance ? 2.0f + static_cast<float>(particle.neonShape) : 1.0f;
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
    conf->time = gpuTime_;
    conf->itemCount = kMaxInstance;
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

    gpuTime_ += deltaTime;
    DispatchGpuEmitters();

    Transition(drawArgsResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
    commandList->CopyBufferRegion(drawArgsResource_.Get(), 4, resetResource_.Get(), 0, sizeof(uint32_t));
    Transition(drawArgsResource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    GlobalConfig* config = nullptr;
    computeConfigResource_->Map(0, nullptr, reinterpret_cast<void**>(&config));
    config->deltaTime = deltaTime;
    config->maxParticles = kMaxInstance;
    config->time = gpuTime_;
    config->itemCount = kMaxInstance;
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
    commandList->SetComputeRootDescriptorTable(6, srvManager_->GetGPUDescriptorHandle(uavIndexFreeListIndex_));
    commandList->SetComputeRootDescriptorTable(7, srvManager_->GetGPUDescriptorHandle(uavIndexFreeList_));

    commandList->Dispatch((kMaxInstance + 1023) / 1024, 1, 1);

    AddUavBarrier({ particleResource_.Get(), gpuInstancingResource_.Get(), drawArgsResource_.Get(),
        freeListIndexResource_.Get(), freeListResource_.Get() });

    Transition(gpuInstancingResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Transition(drawArgsResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    gpuDrawReady_ = true;
}

void ParticleManager::InitializeGpuParticles() {
    GlobalConfig* config = nullptr;
    initializeConfigResource_->Map(0, nullptr, reinterpret_cast<void**>(&config));
    *config = { 0.0f, kMaxInstance, 0.0f, kMaxInstance };
    initializeConfigResource_->Unmap(0, nullptr);

    srvManager_->PreDraw();
    auto commandList = dxCommon_->GetList();
    auto& pso = dxCommon_->GetPSOInitializeParticle();
    commandList->SetComputeRootSignature(pso.root_.GetSignature().Get());
    commandList->SetPipelineState(pso.computeState_.Get());
    commandList->SetComputeRootConstantBufferView(0, initializeConfigResource_->GetGPUVirtualAddress());
    commandList->SetComputeRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(uavIndexParticles_));
    commandList->SetComputeRootDescriptorTable(3, srvManager_->GetGPUDescriptorHandle(uavIndexFreeListIndex_));
    commandList->SetComputeRootDescriptorTable(4, srvManager_->GetGPUDescriptorHandle(uavIndexFreeList_));
    commandList->Dispatch((kMaxInstance + 1023) / 1024, 1, 1);
    AddUavBarrier({ particleResource_.Get(), freeListIndexResource_.Get(), freeListResource_.Get() });
}

void ParticleManager::DispatchGpuEmitters() {
    auto commandList = dxCommon_->GetList();
    srvManager_->PreDraw();

    if (!pendingGpuEmitters_.empty()) {
        const uint32_t count = static_cast<uint32_t>((std::min)(pendingGpuEmitters_.size(), size_t{ kMaxEmitRequests }));
        void* mapped = nullptr;
        emitterRequestResource_->Map(0, nullptr, &mapped);
        std::memcpy(mapped, pendingGpuEmitters_.data(), sizeof(GpuEmitterRequest) * count);
        emitterRequestResource_->Unmap(0, nullptr);

        GlobalConfig* config = nullptr;
        emitterConfigResource_->Map(0, nullptr, reinterpret_cast<void**>(&config));
        *config = { 0.0f, kMaxInstance, gpuTime_, count };
        emitterConfigResource_->Unmap(0, nullptr);

        auto& pso = dxCommon_->GetPSOEmitParticle();
        commandList->SetComputeRootSignature(pso.root_.GetSignature().Get());
        commandList->SetPipelineState(pso.computeState_.Get());
        commandList->SetComputeRootConstantBufferView(0, emitterConfigResource_->GetGPUVirtualAddress());
        commandList->SetComputeRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(uavIndexParticles_));
        commandList->SetComputeRootDescriptorTable(3, srvManager_->GetGPUDescriptorHandle(uavIndexFreeListIndex_));
        commandList->SetComputeRootDescriptorTable(4, srvManager_->GetGPUDescriptorHandle(uavIndexFreeList_));
        commandList->SetComputeRootDescriptorTable(8, srvManager_->GetGPUDescriptorHandle(srvIndexEmitterRequests_));
        commandList->Dispatch((count + 63) / 64, 1, 1);
        AddUavBarrier({ particleResource_.Get(), freeListIndexResource_.Get(), freeListResource_.Get() });
    }

    if (!pendingGpuParticles_.empty()) {
        const uint32_t count = static_cast<uint32_t>((std::min)(pendingGpuParticles_.size(), size_t{ kMaxInstance }));
        void* mapped = nullptr;
        emitStagingResource_->Map(0, nullptr, &mapped);
        std::memcpy(mapped, pendingGpuParticles_.data(), sizeof(ParticleGPU) * count);
        emitStagingResource_->Unmap(0, nullptr);

        GlobalConfig* config = nullptr;
        batchConfigResource_->Map(0, nullptr, reinterpret_cast<void**>(&config));
        *config = { 0.0f, kMaxInstance, gpuTime_, count };
        batchConfigResource_->Unmap(0, nullptr);

        auto& pso = dxCommon_->GetPSOEmitBatchParticle();
        commandList->SetComputeRootSignature(pso.root_.GetSignature().Get());
        commandList->SetPipelineState(pso.computeState_.Get());
        commandList->SetComputeRootConstantBufferView(0, batchConfigResource_->GetGPUVirtualAddress());
        commandList->SetComputeRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(uavIndexParticles_));
        commandList->SetComputeRootDescriptorTable(3, srvManager_->GetGPUDescriptorHandle(uavIndexFreeListIndex_));
        commandList->SetComputeRootDescriptorTable(4, srvManager_->GetGPUDescriptorHandle(uavIndexFreeList_));
        commandList->SetComputeRootDescriptorTable(8, srvManager_->GetGPUDescriptorHandle(srvIndexPrebuiltParticles_));
        commandList->Dispatch((count + 63) / 64, 1, 1);
        AddUavBarrier({ particleResource_.Get(), freeListIndexResource_.Get(), freeListResource_.Get() });
    }

    pendingGpuEmitters_.clear();
    pendingGpuParticles_.clear();
}

void ParticleManager::AddUavBarrier(std::initializer_list<ID3D12Resource*> resources) {
    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    barriers.reserve(resources.size());
    for (ID3D12Resource* resource : resources) {
        if (!resource) {
            continue;
        }
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = resource;
        barriers.push_back(barrier);
    }
    if (!barriers.empty()) {
        dxCommon_->GetList()->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    }
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
    commandList->CopyBufferRegion(
        drawArgsResource_.Get(), 0, drawArgsInitResource_.Get(), 0,
        sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
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
    auto it = effectLibrary_.find(effectName);
    if (it == effectLibrary_.end() || count == 0) return;

    if (useGpuUpdate_) {
        QueueGpuEmitter(it->second, position, count);
        return;
    }

    ParticleEmitterConfig config = it->second;
    config.position = position;

    std::vector<Particle> particles;
    for (uint32_t i = 0; i < count; ++i) particles.push_back(MakeParticle(config));
    EmitBatch(particles);
}

void ParticleManager::QueueGpuEmitter(const ParticleEmitterConfig& config, const Vector3& position, uint32_t count) {
    const uint32_t available = kMaxEmitRequests - static_cast<uint32_t>((std::min)(pendingGpuEmitters_.size(), size_t{ kMaxEmitRequests }));
    count = (std::min)(count, available);
    pendingGpuEmitters_.reserve(pendingGpuEmitters_.size() + count);

    const auto maxComponent = [](const Vector3& value) {
        return (std::max)({ value.x, value.y, value.z });
    };
    for (uint32_t i = 0; i < count; ++i) {
        GpuEmitterRequest request{};
        request.position = position;
        request.radius = config.shapeSize.x;
        request.boxSize = config.shapeSize;
        request.shape = static_cast<uint32_t>(config.emitterShape);
        request.speedMin = config.speedMin;
        request.speedMax = config.speedMax;
        request.lifeTimeMin = config.lifeTimeMin;
        request.lifeTimeMax = config.lifeTimeMax;
        request.acceleration = config.gravity;
        request.startScaleMin = maxComponent(config.startScaleMin);
        request.startScaleMax = maxComponent(config.startScaleMax);
        request.endScaleMin = maxComponent(config.endScaleMin);
        request.endScaleMax = maxComponent(config.endScaleMax);
        request.easingType = static_cast<uint32_t>(config.easingType);
        request.startColor = config.startColor;
        request.endColor = config.endColor;
        request.isBillboard = config.isBillboard ? 1u : 0u;
        request.seed = static_cast<float>(emitterSeed_++);
        request.padding[0] = config.neonRadiance ? 1.0f : 0.0f;
        request.padding[1] = static_cast<float>(config.neonShape);
        pendingGpuEmitters_.push_back(request);
    }
}

void ParticleManager::EmitHitEffect(const Vector3& position) {
    auto it = effectLibrary_.find("HitSpark");
    if (it == effectLibrary_.end()) {
        return;
    }

    ParticleEmitterConfig config = it->second;
    constexpr uint32_t kSparkCount = 18;
	const bool useOutline = neonTriangleEffectMode_ != NeonTriangleEffectMode::LegacyModel;
	const bool useLegacy = neonTriangleEffectMode_ != NeonTriangleEffectMode::Outline;
	std::vector<Particle> legacyParticles;
	legacyParticles.reserve(useLegacy ? kSparkCount : 0u);

    for (uint32_t i = 0; i < kSparkCount; ++i) {
        float angle = (2.0f * pi * static_cast<float>(i) / static_cast<float>(kSparkCount)) + Rand(-0.18f, 0.18f);
        Vector3 radial = { std::cos(angle), std::sin(angle), 0.0f };
        Vector3 dir = Normalize(radial * Rand(0.45f, 1.0f) + RandomUnitVector() * Rand(0.55f, 1.15f));
		if (useLegacy) {
			Particle particle = MakeParticle(config);
			particle.velocity = dir * Rand(config.speedMin, config.speedMax);
			particle.transform.translate = position + Rand(
				Vector3{ -0.18f, -0.18f, -0.04f }, Vector3{ 0.18f, 0.18f, 0.04f });
			particle.transform.rotate.z = angle - (pi * 0.5f);
			particle.angularVelocity = { 0.0f, 0.0f, Rand(-10.0f, 10.0f) };
			particle.startScaleVector *= 1.8f;
			particle.endScaleVector *= 1.8f;
			legacyParticles.push_back(particle);
		}
		if (!useOutline) {
			continue;
		}
		NeonTriangleEvent event{};
		event.position = position + Rand(Vector3{ -0.18f, -0.18f, -0.04f }, Vector3{ 0.18f, 0.18f, 0.04f });
		event.velocity = dir * Rand(config.speedMin, config.speedMax);
		event.radius = Rand(0.12f, 0.28f);
		event.rotation = angle - (pi * 0.5f);
		event.angularVelocity = Rand(-10.0f, 10.0f);
		event.lineWidth = Rand(0.03f, 0.055f);
		event.lifeTime = Rand(config.lifeTimeMin, config.lifeTimeMax);
		event.tiltRad = Rand(-0.95f, 0.95f);
		event.trailCopies = 0;
		event.isBillboard = false;
		event.color = config.startColor;
		neonTriangleEvents_.push_back(event);
    }
	EmitBatch(legacyParticles);
}

void ParticleManager::EmitNeonDeathEffect(
    const Vector3& position, const Vector4& primaryColor, const Vector4& secondaryColor, float strength) {
    strength = std::clamp(strength, 0.2f, 2.0f);
    // A single compact ignition point precedes the burst. It replaces the
    // overlapping giant triangle charges and leaves the target's position clear.
    const float size = 0.6f + strength * 0.65f;
    if (neonTriangleEffectMode_ != NeonTriangleEffectMode::Outline) {
        ParticleEmitterConfig config{};
        config.position = position;
        config.gravity = {};
        config.lifeTimeMin = config.lifeTimeMax = 0.065f;
        config.startScaleMin = config.startScaleMax = {size * 0.20f, size * 0.20f, size * 0.20f};
        config.endScaleMin = config.endScaleMax = {size * 0.72f, size * 0.72f, size * 0.72f};
        config.startColor = {1.7f, 1.7f, 1.7f, 1.0f};
        config.endColor = {primaryColor.x, primaryColor.y, primaryColor.z, 0.0f};
        config.neonRadiance = true;
        config.neonShape = NeonParticleShape::Flash;
        config.isBillboard = true;
        Particle particle = MakeParticle(config);
        particle.velocity = particle.angularVelocity = {};
        EmitBatch({particle});
    }
    if (neonTriangleEffectMode_ != NeonTriangleEffectMode::LegacyModel) {
        NeonTriangleEvent event{};
        event.position = position;
        event.radius = size * 0.20f;
        event.endRadius = size * 0.72f;
        event.lifeTime = 0.065f;
        event.color = {1.7f, 1.7f, 1.7f, 1.0f};
        event.shape = NeonParticleShape::Flash;
        event.isBillboard = true;
        neonTriangleEvents_.push_back(event);
    }
    pendingDeathBursts_.push_back({position, primaryColor, secondaryColor, strength, 0.045f});
}

void ParticleManager::EmitNeonDeathBurstNow(
    const Vector3& position, const Vector4& primaryColor, const Vector4& secondaryColor, float strength) {
    const bool useOutline = neonTriangleEffectMode_ != NeonTriangleEffectMode::LegacyModel;
    const bool useModel = neonTriangleEffectMode_ != NeonTriangleEffectMode::Outline;
    const float size = 0.6f + strength * 0.65f;
    std::vector<Particle> particles;
    // Both render modes consume the same choreography, palette and silhouettes.
    auto emit = [&](NeonParticleShape shape, const Vector3& velocity, float startSize, float endSize,
                    float life, float rotation, float spin, const Vector4& color, bool billboard) {
        if (useModel) {
            ParticleEmitterConfig config{};
            config.position = position;
            config.gravity = {};
            config.lifeTimeMin = config.lifeTimeMax = life;
            config.startScaleMin = config.startScaleMax = {startSize, startSize, startSize};
            config.endScaleMin = config.endScaleMax = {endSize, endSize, endSize};
            config.startColor = color;
            config.endColor = {color.x * 0.65f, color.y * 0.65f, color.z * 0.65f, 0.0f};
            config.neonRadiance = true;
            config.neonShape = shape;
            config.isBillboard = billboard;
            Particle particle = MakeParticle(config);
            particle.velocity = velocity;
            particle.transform.rotate = {billboard ? 0.0f : Rand(-0.55f, 0.55f), 0.0f, rotation};
            particle.angularVelocity = {0.0f, 0.0f, spin};
            particles.push_back(particle);
        }
        if (useOutline) {
            NeonTriangleEvent event{};
            event.position = position;
            event.velocity = velocity;
            event.radius = startSize;
            event.endRadius = endSize;
            event.lifeTime = life;
            event.rotation = rotation;
            event.angularVelocity = spin;
            event.lineWidth = shape == NeonParticleShape::Ring ? 0.048f : 0.030f;
            event.color = color;
            event.shape = shape;
            event.isBillboard = billboard;
            event.tiltRad = billboard ? 0.0f : Rand(-0.55f, 0.55f);
            neonTriangleEvents_.push_back(event);
        }
    };
    const Vector4 white{1.8f, 1.8f, 1.8f, 1.0f};
    Vector4 ringColor = primaryColor;
    ringColor.w = 0.75f;
    emit(NeonParticleShape::Flash, {}, size * 0.80f, size * 2.35f, 0.14f, 0.0f, 0.0f, white, true);
    emit(NeonParticleShape::Ring, {}, size * 0.75f, size * 5.2f, 0.42f, 0.0f, 0.0f, ringColor, true);
    const uint32_t count = static_cast<uint32_t>(std::lround(16.0f + 24.0f * strength));
    for (uint32_t i = 0; i < count; ++i) {
        const float angle = 2.0f * pi * static_cast<float>(i) / count + Rand(-0.13f, 0.13f);
        const Vector3 direction{std::cos(angle), std::sin(angle), Rand(-0.10f, 0.10f)};
        const bool spark = i % 4 == 0;
        const bool sliver = i % 4 == 1;
        const NeonParticleShape shape = spark ? NeonParticleShape::Spark :
            sliver ? NeonParticleShape::Sliver : NeonParticleShape::Shard;
        Vector4 color = i % 5 == 0 ? secondaryColor : primaryColor;
        color.w = spark ? 0.85f : 0.95f;
        const float radius = spark ? Rand(0.34f, 0.70f) : Rand(0.28f, 0.74f) * size;
        emit(shape, direction * (spark ? Rand(12.0f, 23.0f) : Rand(6.5f, 18.0f)), radius,
            radius * (spark ? 0.05f : 0.58f), spark ? Rand(0.12f, 0.23f) : Rand(0.56f, 1.12f),
            spark ? angle - pi * 0.5f : Rand(0.0f, 2.0f * pi), spark ? 0.0f : Rand(-8.0f, 8.0f), color, spark);
    }
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
			if (pending.strength >= 0.75f) screenPulseEvents_.push_back({ pending.position, pending.strength });
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

std::vector<ParticleManager::NeonTriangleEvent> ParticleManager::ConsumeNeonTriangleEvents() {
	std::vector<NeonTriangleEvent> events = std::move(neonTriangleEvents_);
	neonTriangleEvents_.clear();
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
	const bool useOutline = neonTriangleEffectMode_ != NeonTriangleEffectMode::LegacyModel;
	const bool useLegacy = neonTriangleEffectMode_ != NeonTriangleEffectMode::Outline;
	if (useLegacy) {
		ParticleEmitterConfig legacyConfig = it->second;
		legacyConfig.position = position;
		legacyConfig.startColor = color;
		legacyConfig.endColor = { color.x * 0.35f, color.y * 0.35f, color.z * 0.35f, 0.0f };
		legacyConfig.startScaleMin *= 1.8f;
		legacyConfig.startScaleMax *= 1.8f;
		legacyConfig.endScaleMin *= 1.8f;
		legacyConfig.endScaleMax *= 1.8f;

		Vector3 legacyNormal = impactNormal;
		legacyNormal.z = 0.0f;
		legacyNormal = Length(legacyNormal) < 0.0001f
			? Vector3{ 1.0f, 0.0f, 0.0f } : Normalize(legacyNormal);
		const Vector3 legacyTangent = { -legacyNormal.y, legacyNormal.x, 0.0f };
		std::vector<Particle> particles;
		particles.reserve(count);
		for (uint32_t i = 0; i < count; ++i) {
			Particle particle = MakeParticle(legacyConfig);
			Vector3 direction = legacyNormal * Rand(0.55f, 1.35f) +
				legacyTangent * Rand(-0.95f, 0.95f) + Vector3{ 0.0f, 0.0f, Rand(-0.18f, 0.18f) };
			direction = Normalize(direction);
			particle.velocity = direction * Rand(legacyConfig.speedMin, legacyConfig.speedMax);
			particle.transform.translate += direction * Rand(0.0f, 0.18f);
			particle.transform.rotate.z = std::atan2(direction.y, direction.x) + Rand(-0.6f, 0.6f);
			particle.angularVelocity = { 0.0f, 0.0f, i % 4 == 0 ? Rand(-8.0f, 8.0f) : 0.0f };
            particle.neonShape = i % 4 == 0 ? NeonParticleShape::Sliver : NeonParticleShape::Spark;
			particles.push_back(particle);
		}
		EmitBatch(particles);
	}
	if (!useOutline) {
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
	const uint32_t triangleCount = (std::min)(count, 18u);
	for (uint32_t i = 0; i < triangleCount; ++i) {
		Vector3 direction = normal * Rand(0.55f, 1.25f) + tangent * Rand(-0.85f, 0.85f);
		if (Length(direction) < 0.0001f) {
			direction = normal;
		}
		direction = Normalize(direction);
		NeonTriangleEvent event{};
		event.position = position + Rand(Vector3{ -0.12f, -0.12f, -0.03f }, Vector3{ 0.12f, 0.12f, 0.03f });
		event.velocity = direction * Rand(5.5f, 14.0f);
		event.radius = i % 4 == 0 ? Rand(0.24f, 0.42f) : Rand(0.30f, 0.64f);
        event.endRadius = event.radius * 0.10f;
        event.shape = i % 4 == 0 ? NeonParticleShape::Sliver : NeonParticleShape::Spark;
		event.rotation = std::atan2(direction.y, direction.x) - pi * 0.5f;
        event.angularVelocity = i % 4 == 0 ? Rand(-8.0f, 8.0f) : 0.0f;
		event.lineWidth = Rand(0.028f, 0.072f);
		event.lifeTime = Rand(0.18f, 0.46f);
		event.tiltRad = Rand(-0.85f, 0.85f);
		event.trailCopies = 0;
		event.isBillboard = false;
		event.color = color;
		neonTriangleEvents_.push_back(event);
	}
}

void ParticleManager::EmitNeonMovementEffect(const Vector3& position, const Vector3& movementDirection) {
	const bool useOutline = neonTriangleEffectMode_ != NeonTriangleEffectMode::LegacyModel;
	const bool useLegacy = neonTriangleEffectMode_ != NeonTriangleEffectMode::Outline;
	Vector3 backward = movementDirection;
	backward.z = 0.0f;
	if (Length(backward) < 0.0001f) {
		backward = { 0.0f, 1.0f, 0.0f };
	} else {
		backward = Normalize(backward) * -1.0f;
	}
	const Vector3 side = { -backward.y, backward.x, 0.0f };
	if (useLegacy) {
		auto it = effectLibrary_.find("DashTrail");
		if (it != effectLibrary_.end()) {
			ParticleEmitterConfig config = it->second;
			config.position = position;
			Particle particle = MakeParticle(config);
			particle.transform.translate += side * Rand(-0.32f, 0.32f);
			particle.velocity = backward * Rand(0.8f, 2.4f) + side * Rand(-1.2f, 1.2f);
			particle.startScaleVector *= 2.35f;
			particle.endScaleVector *= 2.35f;
			particle.transform.rotate.z = std::atan2(backward.y, backward.x) - (pi * 0.5f) + Rand(-0.45f, 0.45f);
			particle.angularVelocity = { 0.0f, 0.0f, Rand(-5.0f, 5.0f) };
			EmitBatch({ particle });
		}
	}
	if (!useOutline) {
		return;
	}
	NeonTriangleEvent event{};
	event.position = position + side * Rand(-0.32f, 0.32f) + Vector3{ 0.0f, 0.0f, Rand(-0.08f, 0.08f) };
	event.velocity = backward * Rand(0.8f, 2.4f) + side * Rand(-1.2f, 1.2f);
	event.radius = Rand(0.10f, 0.22f);
	event.rotation = Rand(0.0f, pi * 2.0f);
	event.angularVelocity = Rand(-5.0f, 5.0f);
	event.lineWidth = Rand(0.018f, 0.034f);
	event.lifeTime = Rand(0.16f, 0.30f);
	event.tiltRad = Rand(-1.0f, 1.0f);
	event.trailCopies = 0;
	event.isBillboard = false;
	event.color = { 0.20f, 0.95f, 1.15f, 0.30f };
	neonTriangleEvents_.push_back(event);
}

void ParticleManager::Emit(const cg2::Particle& particle) {
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
        return;
    }

    if (!particleResource_ || particles.empty()) {
        return;
    }

    const size_t available = kMaxInstance - (std::min)(pendingGpuParticles_.size(), size_t{ kMaxInstance });
    const uint32_t count = static_cast<uint32_t>((std::min)(particles.size(), available));
    pendingGpuParticles_.reserve(pendingGpuParticles_.size() + count);

    for (uint32_t i = 0; i < count; ++i) {
        const Particle& src = particles[i];
        float startScale = (std::max)({ src.startScaleVector.x, src.startScaleVector.y, src.startScaleVector.z, src.startScale });
        float endScale = (std::max)({ src.endScaleVector.x, src.endScaleVector.y, src.endScaleVector.z, src.endScale });
        if (src.neonRadiance) {
            // The scalar defaults to one even when a small vector range is
            // authored. Match CPU fragment sizes instead of inflating each shard.
            startScale = (std::max)({ src.startScaleVector.x, src.startScaleVector.y, src.startScaleVector.z });
            endScale = (std::max)({ src.endScaleVector.x, src.endScaleVector.y, src.endScaleVector.z });
        }
        ParticleGPU particle{};
        particle.position = src.transform.translate;
        particle.velocity = src.velocity;
        particle.acceleration = src.acceleration;
        particle.angularVelocity = src.angularVelocity;
        particle.currentTime = src.currentTime;
        particle.lifeTime = src.lifeTime;
        particle.startScale = startScale;
        particle.endScale = endScale;
        particle.startColor = src.startColor;
        particle.endColor = src.endColor;
        particle.rotate = src.transform.rotate;
        particle.isActive = 1;
        particle.easingType = static_cast<uint32_t>(src.easingType);
        particle.isBillboard = src.isBillboard ? 1u : 0u;
        particle.padding1 = src.neonRadiance ? 1.0f : 0.0f;
        particle.padding2 = static_cast<float>(src.neonShape);
        pendingGpuParticles_.push_back(particle);
    }
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
    p.neonRadiance = config.neonRadiance;
    p.neonShape = config.neonShape;
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
    auto& pso = dxCommon_->GetPSOModelParticleForScene(); // SceneRT向けのModelParticlePSOを使用
    commandList->SetGraphicsRootSignature(pso.root_.GetSignature().Get());
    commandList->SetPipelineState(pso.graphicsState_.Get());

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &model_->GetVertexBufferView());
    commandList->IASetIndexBuffer(&model_->GetIndexBufferView());

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
        commandList->DrawIndexedInstanced(
            static_cast<UINT>(model_->GetModelData().indices.size()), instanceCount_, 0, 0, 0);
    }
}

uint32_t ParticleManager::GetActiveCount() const {
    return static_cast<uint32_t>(activeParticles_.size());
}

bool ParticleManager::HasDrawableParticles() const {
    return useGpuUpdate_ ? gpuDrawReady_ : instanceCount_ > 0;
}

void ParticleManager::SetUseGpuUpdate(bool useGpuUpdate) {
    if (useGpuUpdate_ == useGpuUpdate) {
        return;
    }
    useGpuUpdate_ = useGpuUpdate;
    activeParticles_.clear();
    pendingGpuEmitters_.clear();
    pendingGpuParticles_.clear();
    instanceCount_ = 0;
    if (useGpuUpdate_ && particleResource_) {
        InitializeGpuParticles();
    }
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
    hit.neonRadiance = true;
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
    burst.neonRadiance = true;
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
    deathCharge.neonRadiance = true;
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
    deathFlash.neonRadiance = true;
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
    deathShard.neonRadiance = true;
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
    impact.neonRadiance = true;
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

        ImGui::Text("CPU Active: %u", GetActiveCount());
        ImGui::TextDisabled("GPU: Initialize CS -> Emit CS -> Update CS -> ExecuteIndirect");
        ImGui::TextDisabled("GPU FreeList: enabled / max %u", kMaxInstance);
        bool gpuUpdate = useGpuUpdate_;
        if (ImGui::Checkbox("GPU Update", &gpuUpdate)) {
            SetUseGpuUpdate(gpuUpdate);
        }
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
        ImGui::Checkbox("Neon Radiance", &editorConfig_.neonRadiance);

        if (ImGui::Button("Apply")) {
            RegisterEffect(editorEffectName_, editorConfig_);
        }
        ImGui::SameLine();
        if (ImGui::Button("GPU Emitter x2048")) {
            RegisterEffect(editorEffectName_, editorConfig_);
            Emit(editorEffectName_, { 0.0f, 0.0f, 0.0f }, 2048);
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

} // namespace cg2
