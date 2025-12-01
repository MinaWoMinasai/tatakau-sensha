#include "Object3dCommon.h"

void Object3dCommon::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;

}

void Object3dCommon::PreDraw()
{

	dxCommon_->GetList()->SetGraphicsRootSignature(dxCommon_->GetPSOObject().root_.GetSignature().Get());
	dxCommon_->GetList()->SetPipelineState(dxCommon_->GetPSOObject().graphicsState_.Get()); // PSOを設定
	dxCommon_->GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

}