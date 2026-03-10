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
	Vector3 lightPos = { -10.0f, 20.0f, -10.0f }; // ライトの擬似的な位置
	Matrix4x4 lightView = MakeLookAtMatrix(lightPos, { 0,0,0 }, { 0,1,0 });
	Matrix4x4 lightOrtho = MakeOrthographicMatrix(-20.0f, 20.0f, 20.0f, -20.0f, 0.1f, 100.0f);
	lightViewProjection_ = Multiply(lightView, lightOrtho);
}

void Object3dCommon::PreDraw(BlendMode blendMode)
{

	dxCommon_->GetList()->SetGraphicsRootSignature(dxCommon_->GetPSOObject(blendMode).root_.GetSignature().Get());
	dxCommon_->GetList()->SetPipelineState(dxCommon_->GetPSOObject(blendMode).graphicsState_.Get()); // PSOを設定
	dxCommon_->GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	blendMode_ = blendMode;
}