#include "Skybox.h"

void Skybox::Initialize(const std::string& textureFilePath) {
    
    object3dCommon_ = Object3dCommon::GetInstance();
    dxCommon_ = object3dCommon_->GetDxCommon();
    srvManager_ = object3dCommon_->GetSrvManager();

    filePath_ = textureFilePath;

    // 1. テクスチャの読み込み (DDS対応済みTextureManagerを使用)
    TextureManager::GetInstance()->LoadTexture(textureFilePath);
    textureIndex_ = TextureManager::GetInstance()->GetSrvIndex(textureFilePath);

    // 2. 内部でObject3dを生成して立方体モデルを割り当て
    object_ = std::make_unique<Object3d>();
    object_->Initialize();
    object_->SetModel("cube.obj"); // Skybox用の立方体モデル
}

void Skybox::Update(Camera* camera, DebugCamera* debugCamera) {

    // 3. カメラの位置に追従させる（空が遠くに見えるように）
    if (object3dCommon_->GetIsDebugCamera()) {
        object_->SetTranslate(debugCamera->GetEyePosition());
        object_->Update();
    } else {
        object_->SetTranslate(camera->GetTranslate());
        object_->Update();
    }
}

void Skybox::Draw() {
    auto list = dxCommon_->GetList();
    auto& pso = dxCommon_->GetPSOSkybox();

    // 1. Skybox専用のシグネチャとPSOをセット
    list->SetGraphicsRootSignature(pso.root_.GetSignature().Get());
    list->SetPipelineState(pso.graphicsState_.Get());

    // 2. SkyboxのRootSignature設定に合わせて順番にセット
    // [0]: Material (b0)
    list->SetGraphicsRootConstantBufferView(0, object_->GetMaterialResource()->GetGPUVirtualAddress());

    // [1]: TransformationMatrix (b0)
    list->SetGraphicsRootConstantBufferView(1, object_->GetTransformationResource()->GetGPUVirtualAddress());

    // [2]: DescriptorTable (t0: CubeMap)
    srvManager_->SetGraphicsRootDescriptorTable(2, textureIndex_);

    // 3. 描画実行
    // object_->Draw() を呼ぶと Index 3 以降にアクセスしてクラッシュするため、
    // モデルの頂点バッファセットと描画コマンドだけを直接実行する
    if (object_->GetModel()) {
        // Modelクラスに実装されている「メッシュのみ描画」を呼び出す
        object_->GetModel()->DrawOnlyMesh();
    }
}