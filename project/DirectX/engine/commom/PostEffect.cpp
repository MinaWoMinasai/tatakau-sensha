#include "PostEffect.h"

void PostEffect::Initialize(DirectXCommon* dxCommon, BloomConstantBuffer* bloomCB) {
	dxCommon_ = dxCommon;
	bloomCB_ = bloomCB;
}

void PostEffect::Draw(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV, BlendMode blendMode)
{
	dxCommon_->GetList()->SetGraphicsRootSignature(dxCommon_->GetPSOObject(blendMode).root_.GetSignature().Get());
	dxCommon_->GetList()->SetPipelineState(dxCommon_->GetPSOObject(blendMode).graphicsState_.Get());
	
	dxCommon_->GetList()->SetGraphicsRootConstantBufferView(0, bloomCB_->GetGPUAddress());

	dxCommon_->GetList()->SetGraphicsRootDescriptorTable(1, inputSRV);

	// 全画面三角形
	dxCommon_->GetList()->DrawInstanced(3, 1, 0, 0);

}

void PostEffect::DrawComposite( D3D12_GPU_DESCRIPTOR_HANDLE sceneSRV, D3D12_GPU_DESCRIPTOR_HANDLE bloomSRV){
    
    dxCommon_->GetList()->SetGraphicsRootSignature(dxCommon_->GetPSOObject(kAdd_Bloom_Composite).root_.GetSignature().Get());

    dxCommon_->GetList()->SetPipelineState(dxCommon_->GetPSOObject(kAdd_Bloom_Composite).graphicsState_.Get());

    dxCommon_->GetList()->SetGraphicsRootConstantBufferView(0, bloomCB_->GetGPUAddress());

    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(1, sceneSRV);

    dxCommon_->GetList()->DrawInstanced(3, 1, 0, 0);
}