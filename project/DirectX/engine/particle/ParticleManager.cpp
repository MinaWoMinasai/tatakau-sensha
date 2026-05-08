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

    // モデル読み込み。細長く伸ばしてヒットエフェクトに使いやすい plane を標準にする。
    ModelManager::GetInstance()->LoadModel("plane.obj");
    ModelManager::GetInstance()->LoadModel("triangleParticle.obj");
    model_ = ModelManager::GetInstance()->FindModel("plane.obj");

    // 1. インスタンシング用。CPU更新した行列を毎フレーム書き込むためUploadにする。
    instancingResource_ = dxCommon_->CreateBufferResource(sizeof(ModelParticleTransformationMatrix) * kMaxInstance);
    instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instancingData_));

    srvIndexInstancing_ = srvManager_->Allocate();
    srvManager_->CreateSRVforStructuredBuffer(srvIndexInstancing_, instancingResource_.Get(), kMaxInstance, sizeof(ModelParticleTransformationMatrix));

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

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = kMaxInstance;

    // 2. 物理バッファ (Particle Data)
    particleResource_ = dxCommon_->CreateUAVBufferResource(sizeof(ParticleGPU) * kMaxInstance, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    uavIndexParticles_ = srvManager_->Allocate();
    uavDesc.Buffer.StructureByteStride = sizeof(ParticleGPU);
    device->CreateUnorderedAccessView(particleResource_.Get(), nullptr, &uavDesc, srvManager_->GetCPUDescriptorHandle(uavIndexParticles_));

    // 3. 間接描画 (Indirect Args)
    drawArgsResource_ = dxCommon_->CreateUAVBufferResource(sizeof(D3D12_DRAW_ARGUMENTS), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    uavIndexDrawArgs_ = srvManager_->Allocate();
    D3D12_UNORDERED_ACCESS_VIEW_DESC rawUavDesc{};
    rawUavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    rawUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    rawUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
    rawUavDesc.Buffer.NumElements = sizeof(D3D12_DRAW_ARGUMENTS) / 4;
    device->CreateUnorderedAccessView(drawArgsResource_.Get(), nullptr, &rawUavDesc, srvManager_->GetCPUDescriptorHandle(uavIndexDrawArgs_));

    // 4. 生存インデックス
    aliveIndicesResource_ = dxCommon_->CreateUAVBufferResource(sizeof(uint32_t) * kMaxInstance, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    uavIndexAliveIndices_ = srvManager_->Allocate();
    uavDesc.Buffer.StructureByteStride = sizeof(uint32_t);
    device->CreateUnorderedAccessView(aliveIndicesResource_.Get(), nullptr, &uavDesc, srvManager_->GetCPUDescriptorHandle(uavIndexAliveIndices_));

    // 5. 定数バッファ
    computeConfigResource_ = dxCommon_->CreateBufferResource(sizeof(GlobalConfig));
    computeSceneResource_ = dxCommon_->CreateBufferResource(sizeof(SceneConfig));
    emitStagingResource_ = dxCommon_->CreateBufferResource(sizeof(ParticleGPU) * kMaxInstance);
    resetResource_ = dxCommon_->CreateBufferResource(sizeof(uint32_t));
    uint32_t zero = 0;
    void* pReset = nullptr;
    resetResource_->Map(0, nullptr, &pReset);
    memcpy(pReset, &zero, 4);
    resetResource_->Unmap(0, nullptr);

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
    if (!instancingData_ || !camera) {
        return;
    }

    const bool useDebugCamera = debugCamera && Object3dCommon::GetInstance()->GetIsDebugCamera();

    if (cameraData_) {
        cameraData_->worldPosition = useDebugCamera ? debugCamera->GetEyePosition() : camera->GetTranslate();
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
    const Matrix4x4& viewProjection = useDebugCamera ? debugCamera->GetViewProjectionMatrix() : camera->GetViewProjectionMatrix();
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
            Vector3 cameraPos = useDebugCamera ? debugCamera->GetEyePosition() : camera->GetTranslate();
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
    for (const Particle& particle : particles) {
        if (activeParticles_.size() >= kMaxInstance) {
            activeParticles_.erase(activeParticles_.begin());
        }
        activeParticles_.push_back({ particle });
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
    if (config.alignToVelocity && Length(p.velocity) > 0.0001f) {
        p.transform.rotate.z = std::atan2(p.velocity.y, p.velocity.x) - (pi * 0.5f);
    }
    return p;
}

void ParticleManager::Draw() {
    if (!dxCommon_ || !srvManager_ || !model_ || instanceCount_ == 0) {
        return;
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
    srvManager_->SetGraphicsRootDescriptorTable(3, srvIndexInstancing_);
    commandList->SetGraphicsRootDescriptorTable(4, TextureManager::GetInstance()->GetSrvHandleGPU(model_->GetModelData().material.textureFilePath));

    commandList->DrawInstanced(static_cast<UINT>(model_->GetModelData().vertices.size()), instanceCount_, 0, 0);
}

uint32_t ParticleManager::GetActiveCount() const {
    return static_cast<uint32_t>(activeParticles_.size());
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
    hit.startScaleMin = { 0.08f, 1.1f, 0.08f };
    hit.startScaleMax = { 0.16f, 2.1f, 0.16f };
    hit.endScaleMin = { 0.02f, 0.10f, 0.02f };
    hit.endScaleMax = { 0.04f, 0.24f, 0.04f };
    hit.startColor = { 1.0f, 0.78f, 0.26f, 1.0f };
    hit.endColor = { 1.0f, 0.08f, 0.02f, 0.0f };
    hit.modelPath = "plane.obj";
    hit.emitterShape = EmitterShape::Point;
    hit.easingType = EasingType::EaseOut;
    hit.isBillboard = true;
    hit.alignToVelocity = true;
    RegisterEffect("HitSpark", hit);
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
