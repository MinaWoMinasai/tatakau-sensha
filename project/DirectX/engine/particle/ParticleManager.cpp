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
    ModelManager::GetInstance()->LoadModel("plane.obj");
    model_ = ModelManager::GetInstance()->FindModel("plane.obj");

    // 2. インスタンシング用リソースの作成
    instancingResource_ = texture.CreateBufferResource(dxCommon_->GetDevice(), sizeof(ParticleForGPU) * kMaxInstance);
    instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instancingData_));

    // 3. SRVの作成 (SrvManagerを活用！)
    srvIndex_ = srvManager_->Allocate(); // 空き番号を自動取得
    srvManager_->CreateSRVforStructuredBuffer(
        srvIndex_,
        instancingResource_.Get(),
        kMaxInstance,
        sizeof(ParticleForGPU)
    );

    // 用のTransformationMatrix用のリソースを作る。Matrix4x41つ分のサイズを用意する
    transformationMatrixResource = texture.CreateBufferResource(dxCommon_->GetDevice(), sizeof(TransformationMatrix));
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
    materialData_->enableLighting = false;
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
   
    Matrix4x4 billboardMatrix;
    
    // ビルボード計算
    if (Object3dCommon::GetInstance()->GetIsDebugCamera()) {
        billboardMatrix = Inverse(debugCamera->GetViewMatrix());
    } else {
        billboardMatrix = Inverse(camera->GetViewMatrix());
    }
    billboardMatrix.m[3][0] = 0.0f;
    billboardMatrix.m[3][1] = 0.0f;
    billboardMatrix.m[3][2] = 0.0f;

    uint32_t instanceCount = 0;
    for (auto it = particles_.begin(); it != particles_.end(); ) {
        it->currentTime += deltaTime;
        if (it->currentTime >= it->lifeTime) {
            it = particles_.erase(it);
            continue;
        }

        // 移動
        it->transform.translate += it->velocity;

        // GPUデータ更新
        if (instanceCount < kMaxInstance) {
            Matrix4x4 world = MakeScaleMatrix(it->transform.scale) * billboardMatrix * MakeTranslateMatrix(it->transform.translate);
            if (Object3dCommon::GetInstance()->GetIsDebugCamera()) {
                instancingData_[instanceCount].WVP = world * debugCamera->GetViewMatrix() * debugCamera->GetProjectionMatrix();
            } else {
                instancingData_[instanceCount].WVP = world * camera->GetViewMatrix() * camera->GetProjectionMatrix();
			}
            instancingData_[instanceCount].World = world;

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

    dxCommon_->GetList()->SetGraphicsRootSignature(dxCommon_->GetPSOParticle().root_.GetSignature().Get());
    dxCommon_->GetList()->SetPipelineState(dxCommon_->GetPSOParticle().graphicsState_.Get()); // PSOを設定
    dxCommon_->GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 1. マテリアル (b0)
    dxCommon_->GetList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    // 2. テクスチャ (t0) をセット 
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(1, TextureManager::GetInstance()->GetSrvHandleGPU(model_->GetModelData().material.textureFilePath));

    // 3. インスタンシングバッファ (t1) をセット
    srvManager_->SetGraphicsRootDescriptorTable(2, srvIndex_);

    // 描画実行
    dxCommon_->GetList()->IASetVertexBuffers(0, 1, &model_->GetVertexBufferView());
    dxCommon_->GetList()->DrawInstanced((UINT)model_->GetModelData().vertices.size(), (UINT)particles_.size(), 0, 0);
}