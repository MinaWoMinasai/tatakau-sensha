#include "ParticleManager.h"
#include "Calculation.h"
#include "TextureManager.h"
#include "DebugCamera.h"
#include <fstream>
#include <algorithm>

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
        {"startColor", {startColor.x, startColor.y, startColor.z, startColor.w}},
        {"endColor", {endColor.x, endColor.y, endColor.z, endColor.w}},
        {"modelPath", modelPath},
        {"emitterShape", static_cast<uint32_t>(emitterShape)},
        {"shapeSize", {shapeSize.x, shapeSize.y, shapeSize.z}},
        {"easingType", static_cast<uint32_t>(easingType)},
        {"isBillboard", isBillboard}
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
    startColor = readVector4(j.value("startColor", nlohmann::json::array()), startColor);
    endColor = readVector4(j.value("endColor", nlohmann::json::array()), endColor);
    modelPath = j.value("modelPath", modelPath);
    emitterShape = static_cast<EmitterShape>(j.value("emitterShape", static_cast<uint32_t>(emitterShape)));
    shapeSize = readVector3(j.value("shapeSize", nlohmann::json::array()), shapeSize);
    easingType = static_cast<EasingType>(j.value("easingType", static_cast<uint32_t>(easingType)));
    isBillboard = j.value("isBillboard", isBillboard);
}

void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    auto device = dxCommon_->GetDevice();

    // モデル読み込み
    ModelManager::GetInstance()->LoadModel("triangleParticle.obj");
    model_ = ModelManager::GetInstance()->FindModel("triangleParticle.obj");

    // 1. インスタンシング用(UAV/SRV)
    instancingResource_ = dxCommon_->CreateUAVBufferResource(sizeof(ModelParticleTransformationMatrix) * kMaxInstance, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    uavIndexRenderData_ = srvManager_->Allocate();
    // デバイスから直接UAV作成 (SrvManagerにUAV作成がない場合)
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = kMaxInstance;
    uavDesc.Buffer.StructureByteStride = sizeof(ModelParticleTransformationMatrix);
    device->CreateUnorderedAccessView(instancingResource_.Get(), nullptr, &uavDesc, srvManager_->GetCPUDescriptorHandle(uavIndexRenderData_));

    srvIndexInstancing_ = srvManager_->Allocate();
    srvManager_->CreateSRVforStructuredBuffer(srvIndexInstancing_, instancingResource_.Get(), kMaxInstance, sizeof(ModelParticleTransformationMatrix));

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
}

void ParticleManager::Update(float deltaTime, Camera* camera, DebugCamera*) {
    (void)deltaTime;
    (void)camera;

    // The compute-driven GPU update path is still being migrated.
    // Avoid issuing incomplete GPU commands from the frame loop.
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

void ParticleManager::Emit(const std::string& effectName, const Vector3& position, uint32_t count) {
    if (effectLibrary_.find(effectName) == effectLibrary_.end()) return;
    auto& config = effectLibrary_[effectName];
    config.position = position;

    std::vector<Particle> particles;
    for (uint32_t i = 0; i < count; ++i) particles.push_back(MakeParticle(config));
    EmitBatch(particles);
}

void ParticleManager::Emit(const ::Particle& particle) {
    Particle converted{};
    converted.transform = particle.transform;
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

    EmitBatch({ converted });
}

void ParticleManager::EmitBatch(const std::vector<Particle>& particles) {
    if (particles.empty()) return;
    uint32_t count = static_cast<uint32_t>((std::min)(particles.size(), size_t{ 1000 }));
    if (freeIndex_ + count >= kMaxInstance) freeIndex_ = 0;

    void* mappedPtr = nullptr;
    emitStagingResource_->Map(0, nullptr, &mappedPtr);
    ParticleGPU* dest = static_cast<ParticleGPU*>(mappedPtr) + freeIndex_;

    for (uint32_t i = 0; i < count; ++i) {
        dest[i].position = particles[i].transform.translate;
        dest[i].velocity = particles[i].velocity;
        dest[i].acceleration = particles[i].acceleration;
        dest[i].angularVelocity = particles[i].angularVelocity;
        dest[i].currentTime = 0.0f;
        dest[i].lifeTime = particles[i].lifeTime;
        dest[i].startScale = particles[i].startScale;
        dest[i].endScale = particles[i].endScale;
        dest[i].startColor = particles[i].startColor;
        dest[i].endColor = particles[i].endColor;
        dest[i].rotate = particles[i].transform.rotate;
        dest[i].isActive = 1;
        dest[i].easingType = static_cast<uint32_t>(particles[i].easingType);
        dest[i].isBillboard = particles[i].isBillboard ? 1 : 0;
    }
    emitStagingResource_->Unmap(0, nullptr);

    dxCommon_->GetList()->CopyBufferRegion(particleResource_.Get(), freeIndex_ * sizeof(ParticleGPU), emitStagingResource_.Get(), freeIndex_ * sizeof(ParticleGPU), count * sizeof(ParticleGPU));
    freeIndex_ += count;
}

ParticleManager::Particle ParticleManager::MakeParticle(const ParticleEmitterConfig& config) {
    Particle p;
    p.transform.translate = config.position; // 形状に応じた計算をここに
    p.velocity = Multiply(Rand(config.speedMin, config.speedMax), RandomUnitVector());
    p.acceleration = config.gravity;
    p.angularVelocity = Rand(Vector3{ -5,-5,-5 }, Vector3{ 5,5,5 });
    p.lifeTime = Rand(config.lifeTimeMin, config.lifeTimeMax);
    p.currentTime = 0.0f;
    p.startScale = config.startScale;
    p.endScale = config.endScale;
    p.startColor = config.startColor;
    p.endColor = config.endColor;
    p.easingType = config.easingType;
    p.isBillboard = config.isBillboard;
    return p;
}

void ParticleManager::Draw() {
    if (!dxCommon_ || !srvManager_ || !model_ || !commandSignature_) {
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
    srvManager_->SetGraphicsRootDescriptorTable(3, srvIndexInstancing_);
    commandList->SetGraphicsRootDescriptorTable(4, TextureManager::GetInstance()->GetSrvHandleGPU(model_->GetModelData().material.textureFilePath));

    commandList->ExecuteIndirect(commandSignature_.Get(), 1, drawArgsResource_.Get(), 0, nullptr, 0);
}
