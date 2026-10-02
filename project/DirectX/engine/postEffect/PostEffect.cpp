#include "PostEffect.h"

namespace cg2 {

void PostEffect::Initialize(DirectXCommon* dxCommon, BloomConstantBuffer* bloomCB) {
	dxCommon_ = dxCommon;
	bloomCB_ = bloomCB;
}

void PostEffect::Draw(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV, BlendMode blendMode, bool outputToHdr)
{
    auto& pso = outputToHdr
        ? dxCommon_->GetPSOObjectForScene(blendMode)
        : dxCommon_->GetPSOObject(blendMode);
	dxCommon_->GetList()->SetGraphicsRootSignature(pso.root_.GetSignature().Get());
	dxCommon_->GetList()->SetPipelineState(pso.graphicsState_.Get());
	dxCommon_->GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	dxCommon_->GetList()->SetGraphicsRootConstantBufferView(0, bloomCB_->GetGPUAddress());

	dxCommon_->GetList()->SetGraphicsRootDescriptorTable(1, inputSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(2, inputSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(3, inputSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(4, inputSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(5, inputSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(6, inputSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(7, inputSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(8, inputSRV);

	// 全画面三角形
	dxCommon_->GetList()->DrawInstanced(3, 1, 0, 0);

}

void PostEffect::DrawBloomExtract(
    D3D12_GPU_DESCRIPTOR_HANDLE sceneSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE materialSRV)
{
    auto& pso = dxCommon_->GetPSOObjectForScene(kAdd_Bloom_Extract);
    dxCommon_->GetList()->SetGraphicsRootSignature(pso.root_.GetSignature().Get());
    dxCommon_->GetList()->SetPipelineState(pso.graphicsState_.Get());
    dxCommon_->GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    dxCommon_->GetList()->SetGraphicsRootConstantBufferView(0, bloomCB_->GetGPUAddress());
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(1, sceneSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(2, sceneSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(3, sceneSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(4, sceneSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(5, sceneSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(6, materialSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(7, sceneSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(8, sceneSRV);
    dxCommon_->GetList()->DrawInstanced(3, 1, 0, 0);
}

void PostEffect::DrawComposite(
    D3D12_GPU_DESCRIPTOR_HANDLE sceneSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE bloomSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE depthSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE normalSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE ssrSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE materialSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE ssaoSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE motionVectorSRV) {

    dxCommon_->GetList()->SetGraphicsRootSignature(dxCommon_->GetPSOObject(kAdd_Bloom_Composite).root_.GetSignature().Get());

    dxCommon_->GetList()->SetPipelineState(dxCommon_->GetPSOObject(kAdd_Bloom_Composite).graphicsState_.Get());
    dxCommon_->GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    dxCommon_->GetList()->SetGraphicsRootConstantBufferView(0, bloomCB_->GetGPUAddress());

    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(1, sceneSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(2, bloomSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(3, depthSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(4, normalSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(5, ssrSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(6, materialSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(7, ssaoSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(8, motionVectorSRV);

    dxCommon_->GetList()->DrawInstanced(3, 1, 0, 0);
}

void PostEffect::DrawMotionVectorResolve(
    D3D12_GPU_DESCRIPTOR_HANDLE depthSRV) {

    auto& pso = dxCommon_->GetPSOObject(kAdd_MotionVector_Resolve);
    dxCommon_->GetList()->SetGraphicsRootSignature(pso.root_.GetSignature().Get());
    dxCommon_->GetList()->SetPipelineState(pso.graphicsState_.Get());
    dxCommon_->GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    dxCommon_->GetList()->SetGraphicsRootConstantBufferView(0, bloomCB_->GetGPUAddress());
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(1, depthSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(2, depthSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(3, depthSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(4, depthSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(5, depthSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(6, depthSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(7, depthSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(8, depthSRV);

    dxCommon_->GetList()->DrawInstanced(3, 1, 0, 0);
}

void PostEffect::DrawTemporalResolve(
    D3D12_GPU_DESCRIPTOR_HANDLE currentSceneSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE historySceneSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE depthSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE normalSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE materialSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE motionVectorSRV) {

    auto& pso = dxCommon_->GetPSOObjectForScene(kAdd_Temporal_Resolve);
    dxCommon_->GetList()->SetGraphicsRootSignature(pso.root_.GetSignature().Get());
    dxCommon_->GetList()->SetPipelineState(pso.graphicsState_.Get());
    dxCommon_->GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    dxCommon_->GetList()->SetGraphicsRootConstantBufferView(0, bloomCB_->GetGPUAddress());
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(1, currentSceneSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(2, historySceneSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(3, depthSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(4, normalSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(5, currentSceneSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(6, materialSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(7, currentSceneSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(8, motionVectorSRV);

    dxCommon_->GetList()->DrawInstanced(3, 1, 0, 0);
}

void PostEffect::DrawSSAOResolve(
    D3D12_GPU_DESCRIPTOR_HANDLE depthSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE normalSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE materialSRV) {

    auto& pso = dxCommon_->GetPSOObject(kAdd_SSAO_Resolve);
    dxCommon_->GetList()->SetGraphicsRootSignature(pso.root_.GetSignature().Get());
    dxCommon_->GetList()->SetPipelineState(pso.graphicsState_.Get());
    dxCommon_->GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    dxCommon_->GetList()->SetGraphicsRootConstantBufferView(0, bloomCB_->GetGPUAddress());
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(1, depthSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(2, depthSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(3, depthSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(4, normalSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(5, depthSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(6, materialSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(7, depthSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(8, depthSRV);

    dxCommon_->GetList()->DrawInstanced(3, 1, 0, 0);
}

void PostEffect::DrawSSAODenoise(
    D3D12_GPU_DESCRIPTOR_HANDLE ssaoSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE depthSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE normalSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE materialSRV) {

    auto& pso = dxCommon_->GetPSOObject(kAdd_SSAO_Denoise);
    dxCommon_->GetList()->SetGraphicsRootSignature(pso.root_.GetSignature().Get());
    dxCommon_->GetList()->SetPipelineState(pso.graphicsState_.Get());
    dxCommon_->GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    dxCommon_->GetList()->SetGraphicsRootConstantBufferView(0, bloomCB_->GetGPUAddress());
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(1, ssaoSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(2, ssaoSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(3, depthSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(4, normalSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(5, ssaoSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(6, materialSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(7, ssaoSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(8, ssaoSRV);

    dxCommon_->GetList()->DrawInstanced(3, 1, 0, 0);
}

void PostEffect::DrawSSRResolve(
    D3D12_GPU_DESCRIPTOR_HANDLE sceneSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE depthSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE normalSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE materialSRV) {

    auto& pso = dxCommon_->GetPSOObjectForScene(kAdd_SSR_Resolve);
    dxCommon_->GetList()->SetGraphicsRootSignature(pso.root_.GetSignature().Get());
    dxCommon_->GetList()->SetPipelineState(pso.graphicsState_.Get());
    dxCommon_->GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    dxCommon_->GetList()->SetGraphicsRootConstantBufferView(0, bloomCB_->GetGPUAddress());
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(1, sceneSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(2, sceneSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(3, depthSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(4, normalSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(5, sceneSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(6, materialSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(7, sceneSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(8, sceneSRV);

    dxCommon_->GetList()->DrawInstanced(3, 1, 0, 0);
}

void PostEffect::DrawSSRDenoise(
    D3D12_GPU_DESCRIPTOR_HANDLE ssrSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE depthSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE normalSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE materialSRV) {

    auto& pso = dxCommon_->GetPSOObjectForScene(kAdd_SSR_Denoise);
    dxCommon_->GetList()->SetGraphicsRootSignature(pso.root_.GetSignature().Get());
    dxCommon_->GetList()->SetPipelineState(pso.graphicsState_.Get());
    dxCommon_->GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    dxCommon_->GetList()->SetGraphicsRootConstantBufferView(0, bloomCB_->GetGPUAddress());
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(1, ssrSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(2, ssrSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(3, depthSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(4, normalSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(5, ssrSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(6, materialSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(7, ssrSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(8, ssrSRV);

    dxCommon_->GetList()->DrawInstanced(3, 1, 0, 0);
}

void PostEffect::DrawObjectComposite(D3D12_GPU_DESCRIPTOR_HANDLE objectSRV, D3D12_GPU_DESCRIPTOR_HANDLE bloomSRV, bool outputToHdr) {
    auto& pso = outputToHdr
        ? dxCommon_->GetPSOObjectForScene(kAdd_ObjectPost_Composite)
        : dxCommon_->GetPSOObject(kAdd_ObjectPost_Composite);
    dxCommon_->GetList()->SetGraphicsRootSignature(pso.root_.GetSignature().Get());
    dxCommon_->GetList()->SetPipelineState(pso.graphicsState_.Get());
    dxCommon_->GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    dxCommon_->GetList()->SetGraphicsRootConstantBufferView(0, bloomCB_->GetGPUAddress());
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(1, objectSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(2, bloomSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(3, objectSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(4, objectSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(5, objectSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(6, objectSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(7, objectSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(8, objectSRV);

    dxCommon_->GetList()->DrawInstanced(3, 1, 0, 0);
}

void PostEffect::DrawObjectOutlineAdd(D3D12_GPU_DESCRIPTOR_HANDLE objectSRV, D3D12_GPU_DESCRIPTOR_HANDLE bloomSRV, bool outputToHdr) {
    auto& pso = outputToHdr
        ? dxCommon_->GetPSOObjectForScene(kAdd_ObjectPost_OutlineAdd)
        : dxCommon_->GetPSOObject(kAdd_ObjectPost_OutlineAdd);
    dxCommon_->GetList()->SetGraphicsRootSignature(pso.root_.GetSignature().Get());
    dxCommon_->GetList()->SetPipelineState(pso.graphicsState_.Get());
    dxCommon_->GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    dxCommon_->GetList()->SetGraphicsRootConstantBufferView(0, bloomCB_->GetGPUAddress());
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(1, objectSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(2, bloomSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(3, objectSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(4, objectSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(5, objectSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(6, objectSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(7, objectSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(8, objectSRV);

    dxCommon_->GetList()->DrawInstanced(3, 1, 0, 0);
}

void PostEffect::DrawObjectBloomAdd(D3D12_GPU_DESCRIPTOR_HANDLE bloomSRV, bool outputToHdr) {
    auto& pso = outputToHdr
        ? dxCommon_->GetPSOObjectForScene(kAdd_ObjectPost_BloomAdd)
        : dxCommon_->GetPSOObject(kAdd_ObjectPost_BloomAdd);
    dxCommon_->GetList()->SetGraphicsRootSignature(pso.root_.GetSignature().Get());
    dxCommon_->GetList()->SetPipelineState(pso.graphicsState_.Get());
    dxCommon_->GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    dxCommon_->GetList()->SetGraphicsRootConstantBufferView(0, bloomCB_->GetGPUAddress());
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(1, bloomSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(2, bloomSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(3, bloomSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(4, bloomSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(5, bloomSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(6, bloomSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(7, bloomSRV);
    dxCommon_->GetList()->SetGraphicsRootDescriptorTable(8, bloomSRV);

    dxCommon_->GetList()->DrawInstanced(3, 1, 0, 0);
}

} // namespace cg2
