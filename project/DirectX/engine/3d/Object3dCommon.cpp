#include "Object3dCommon.h"

Object3dCommon* Object3dCommon::GetInstance()
{
	static Object3dCommon instance;
	return &instance;
}

void Object3dCommon::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, ShadowMap* shadowMap)
{

	if (dxCommon_ != nullptr) {
		return;
	}
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	shadowMap_ = shadowMap;
    lightDir_ = Normalize({ 0.01f, -0.99f, 0.1f });
}

void Object3dCommon::Update() {
    // 1. ライトの向き（DirectionalLightと同じ向きにする）
    // 例：斜め下 45度方向

#ifdef USE_IMGUI

    ImGui::Begin("light");
    ImGui::DragFloat3("dir", &lightDir_.x, 0.01f);
    ImGui::End();

    lightDir_ = Normalize(lightDir_);

#endif // USE_IMGUI

    // 2. ライトの位置
    // ライトの向きの逆方向に、十分離れた場所にカメラを置く
    float distance = 300.0f; // 十分な距離をとる
    Vector3 lightPos = { lightDir_.x * distance, -lightDir_.y * distance, lightDir_.z * distance };

    // 3. ライトビュー行列
    // 原点付近を映す。もしキャラが動くならここをキャラの座標にする
    Matrix4x4 lightView = MakeLookAtMatrix(lightPos, { 0, 0, 0 }, { 0, 1, 0 });

    // 4. プロジェクション行列（ここが重要！）
    // 地面全体（緑色の範囲）を広げるために range を大きくする
    float range = 500.0f; // ステージ全体を覆うくらいのサイズ
    // 奥行き（FarZ）も十分に確保する
    Matrix4x4 lightOrtho = MakeOrthographicMatrix(-range, range, range, -range, 0.1f, 1000.0f);

    lightViewProjection_ = Multiply(lightView, lightOrtho);
}

void Object3dCommon::PreDraw(BlendMode blendMode)
{

	dxCommon_->GetList()->SetGraphicsRootSignature(dxCommon_->GetPSOObject(blendMode).root_.GetSignature().Get());
	dxCommon_->GetList()->SetPipelineState(dxCommon_->GetPSOObject(blendMode).graphicsState_.Get()); // PSOを設定
	dxCommon_->GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	blendMode_ = blendMode;
}