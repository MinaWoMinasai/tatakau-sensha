#pragma once
#include "DirectXCommon.h"
#include "BloomConstantBuffer.h"

class PostEffect {
public:
    void Initialize(DirectXCommon* dxCommon, BloomConstantBuffer* bloomCB);
    void Draw(
        D3D12_GPU_DESCRIPTOR_HANDLE inputSRV,
        BlendMode blendMode,
        bool outputToHdr = true
    );
    void DrawBloomExtract(
        D3D12_GPU_DESCRIPTOR_HANDLE sceneSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE materialSRV);
    void DrawComposite(
        D3D12_GPU_DESCRIPTOR_HANDLE sceneSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE bloomSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE depthSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE normalSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE ssrSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE materialSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE ssaoSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE motionVectorSRV);
    void DrawMotionVectorResolve(
        D3D12_GPU_DESCRIPTOR_HANDLE depthSRV);
    void DrawTemporalResolve(
        D3D12_GPU_DESCRIPTOR_HANDLE currentSceneSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE historySceneSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE depthSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE normalSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE materialSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE motionVectorSRV);
    void DrawSSAOResolve(
        D3D12_GPU_DESCRIPTOR_HANDLE depthSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE normalSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE materialSRV);
    void DrawSSAODenoise(
        D3D12_GPU_DESCRIPTOR_HANDLE ssaoSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE depthSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE normalSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE materialSRV);
    void DrawSSRResolve(
        D3D12_GPU_DESCRIPTOR_HANDLE sceneSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE depthSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE normalSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE materialSRV);
    void DrawSSRDenoise(
        D3D12_GPU_DESCRIPTOR_HANDLE ssrSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE depthSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE normalSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE materialSRV);
    void DrawObjectComposite(D3D12_GPU_DESCRIPTOR_HANDLE objectSRV, D3D12_GPU_DESCRIPTOR_HANDLE bloomSRV, bool outputToHdr = true);
    void DrawObjectOutlineAdd(D3D12_GPU_DESCRIPTOR_HANDLE objectSRV, D3D12_GPU_DESCRIPTOR_HANDLE bloomSRV, bool outputToHdr = true);
    void DrawObjectBloomAdd(D3D12_GPU_DESCRIPTOR_HANDLE bloomSRV, bool outputToHdr = true);

private:
    DirectXCommon* dxCommon_ = nullptr;
    BloomConstantBuffer* bloomCB_;

};
