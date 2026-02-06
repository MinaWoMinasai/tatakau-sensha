#include "Object3dCommon.h"

Object3dCommon* Object3dCommon::GetInstance()
{
	static Object3dCommon instance;
	return &instance;
}

void Object3dCommon::Initialize(DirectXCommon* dxCommon)
{

	if (dxCommon_ != nullptr) {
		return;
	}
	dxCommon_ = dxCommon;

}

void Object3dCommon::PreDraw(BlendMode blendMode)
{

	dxCommon_->GetList()->SetGraphicsRootSignature(dxCommon_->GetPSOObject(blendMode).root_.GetSignature().Get());
	dxCommon_->GetList()->SetPipelineState(dxCommon_->GetPSOObject(blendMode).graphicsState_.Get()); // PSOを設定
	dxCommon_->GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

}