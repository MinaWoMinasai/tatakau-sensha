#include "ParticleManager.h"
#include <algorithm>
#include "Object3dCommon.h"

ParticleManager* ParticleManager::GetInstance()
{
    static ParticleManager instance;
    return &instance;
}

void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    // 1. モデルの取得（ModelManagerを使用）
    ModelManager::GetInstance()->LoadModel("triangleParticle.obj");
    model_ = ModelManager::GetInstance()->FindModel("triangleParticle.obj");

    // 2. インスタンシング用リソースの作成
    instancingResource_ = texture.CreateBufferResource(dxCommon_->GetDevice(), sizeof(ModelParticleTransformationMatrix) * kMaxInstance);
    instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instancingData_));

    // 3. SRVの作成 (SrvManagerを活用！)
    srvIndex_ = srvManager_->Allocate(); // 空き番号を自動取得
    srvManager_->CreateSRVforStructuredBuffer(
        srvIndex_,
        instancingResource_.Get(),
        kMaxInstance,
        sizeof(ModelParticleTransformationMatrix)
    );

    // 用のModelParticleTransformationMatrix用のリソースを作る。Matrix4x41つ分のサイズを用意する
    transformationMatrixResource = texture.CreateBufferResource(dxCommon_->GetDevice(), sizeof(ModelParticleTransformationMatrix));
    // 書き込むためのアドレスを取得
    transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData));
    // 単位行列を書き込んでおく
    transformationMatrixData->WVP = MakeIdentity4x4();
    transformationMatrixData->World = MakeIdentity4x4();

    // マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
    materialResource_ = texture.CreateBufferResource(dxCommon_->GetDevice(), sizeof(Material));
    // マテリアルにデータを書き込む
    materialData_ = nullptr;
    // 書き込むためのアドレスを取得
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    // 赤を書き込む
    materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    // ライティングを有効にする
    materialData_->enableLighting = true;
    materialData_->lightingMode = false;
    materialData_->uvTransform = MakeIdentity4x4();
    materialData_->shininess = 32.0f;

    // 平行光源用のリソースを作る
    directionalLightResource = resource.CreatedirectionalLight(texture, dxCommon_->GetDevice());
    // マテリアルにデータを書き込む
    directionalLightData = nullptr;
    // 書き込むためのアドレスを取得
    directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
    // デフォルト値
    directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
    directionalLightData->direction = Normalize(directionalLightData->direction);
    directionalLightData->intensity = 1.0f;

    // カメラ用 CBV を作成
    cameraResource_ = texture.CreateBufferResource(
        dxCommon_->GetDevice(),
        sizeof(CameraData)
    );

    // マップ
    cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));

    // 初期値（とりあえず原点）
    cameraData_->worldPosition = { 0.0f, 0.0f, 0.0f };
    cameraData_->padding = 0.0f;

}

void ParticleManager::Emit(const Particle& particle) {
    if (particles_.size() >= kMaxInstance) return;

    particles_.push_back(particle);
}

void ParticleManager::Update(float deltaTime, Camera* camera, DebugCamera* debugCamera) {
   
    uint32_t instanceCount = 0;
    for (auto it = particles_.begin(); it != particles_.end(); ) {
        it->currentTime += deltaTime;
        if (it->currentTime >= it->lifeTime) {
            it = particles_.erase(it);
            continue;
        }

        // 移動
        it->transform.translate += it->velocity * deltaTime;

        // 2. 回転：回転速度ベクトルを加算 (氷がくるくる回る動き)
        it->transform.rotate.x += it->angularVelocity.x * deltaTime;
        it->transform.rotate.y += it->angularVelocity.y * deltaTime;
        it->transform.rotate.z += it->angularVelocity.z * deltaTime;

        // GPUデータ更新
        if (instanceCount < kMaxInstance) {
            Matrix4x4 world = MakeAffineMatrix(it->transform.scale, it->transform.rotate, it->transform.translate);
            if (Object3dCommon::GetInstance()->GetIsDebugCamera()) {
                instancingData_[instanceCount].WVP = world * debugCamera->GetViewMatrix() * debugCamera->GetProjectionMatrix();
                cameraData_->worldPosition = debugCamera->GetEyePosition();
            } else {
                instancingData_[instanceCount].WVP = world * camera->GetViewMatrix() * camera->GetProjectionMatrix();
                cameraData_->worldPosition = camera->GetTranslate();
			}

            Matrix4x4 worldInverseTranspose = Transpose(Inverse(world));
            instancingData_[instanceCount].World = world;
            instancingData_[instanceCount].WorldInverseTranspose = worldInverseTranspose;

            // フェードアウトなどの色計算
            float alpha = 1.0f - (it->currentTime / it->lifeTime);
            instancingData_[instanceCount].color = it->color;
            instancingData_[instanceCount].color.w *= alpha;

            instanceCount++;
        }
        ++it;
    }
}

void ParticleManager::Draw() {
    if (particles_.empty()) return;

    auto commandList = dxCommon_->GetList();

    // 1. シグネチャとPSOの設定
    commandList->SetGraphicsRootSignature(dxCommon_->GetPSOModelParticle().root_.GetSignature().Get());
    commandList->SetPipelineState(dxCommon_->GetPSOModelParticle().graphicsState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 2. 頂点バッファの設定
    commandList->IASetVertexBuffers(0, 1, &model_->GetVertexBufferView());

    // --- ここからルートパラメータのセット (InitalizeForModelParticleの順番に合わせる) ---

    // Index 0: Material (b0) - CBV
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    // Index 1: DirectionalLight (b1) - CBV
    commandList->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());

    // Index 2: Camera (b2) - CBV
    commandList->SetGraphicsRootConstantBufferView(2, cameraResource_->GetGPUVirtualAddress());

    // Index 3: Instancing Buffer (t1) - DescriptorTable
    // SrvManagerからGPUハンドルを取得してセットします
    commandList->SetGraphicsRootDescriptorTable(3, srvManager_->GetGPUDescriptorHandle(srvIndex_));

    // Index 4: Texture (t0) - DescriptorTable
    commandList->SetGraphicsRootDescriptorTable(4, TextureManager::GetInstance()->GetSrvHandleGPU(model_->GetModelData().material.textureFilePath));

    // 3. 描画実行
    commandList->DrawInstanced(
        (UINT)model_->GetModelData().vertices.size(),
        (UINT)particles_.size(),
        0, 0
    );
}