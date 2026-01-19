#include "SpriteCommon.h"

SpriteCommon* SpriteCommon::GetInstance()
{
	static SpriteCommon instance;
	return &instance;
}

void SpriteCommon::Initialize(DirectXCommon* dxCommon)
{
	if (dxCommon_ != nullptr) {
		return;
	}
	dxCommon_ = dxCommon;

}

void SpriteCommon::PreDraw(BlendMode blendMode)
{

	dxCommon_->GetList()->SetGraphicsRootSignature(dxCommon_->GetPSOObject(blendMode).root_.GetSignature().Get());
	dxCommon_->GetList()->SetPipelineState(dxCommon_->GetPSOObject(blendMode).graphicsState_.Get()); // PSOを設定
	dxCommon_->GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

}