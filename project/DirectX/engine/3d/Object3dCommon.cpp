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

}

void Object3dCommon::Update() {
	// ライトを少し遠ざけて、全体を俯瞰するようにする
	Vector3 lightPos = { -30.0f, 60.0f, -30.0f };
	// 地面付近（0, -15, 0）を見るように調整
	Matrix4x4 lightView = MakeLookAtMatrix(lightPos, { 0, -15.0f, 0 }, { 0, 1, 0 });
	// 範囲を少し広げる (-50 ~ 50)
	Matrix4x4 lightOrtho = MakeOrthographicMatrix(-50.0f, 50.0f, 50.0f, -50.0f, 0.1f, 200.0f);
	lightViewProjection_ = Multiply(lightView, lightOrtho);
}

void Object3dCommon::PreDraw(BlendMode blendMode)
{

	dxCommon_->GetList()->SetGraphicsRootSignature(dxCommon_->GetPSOObject(blendMode).root_.GetSignature().Get());
	dxCommon_->GetList()->SetPipelineState(dxCommon_->GetPSOObject(blendMode).graphicsState_.Get()); // PSOを設定
	dxCommon_->GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	blendMode_ = blendMode;
}