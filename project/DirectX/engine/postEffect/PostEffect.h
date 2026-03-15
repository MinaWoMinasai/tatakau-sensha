#pragma once
#include "DirectXCommon.h"
#include "BloomConstantBuffer.h"

class PostEffect {
public:
    void Initialize(DirectXCommon* dxCommon, BloomConstantBuffer* bloomCB);
    void Draw(
        D3D12_GPU_DESCRIPTOR_HANDLE inputSRV, BlendMode blendMode
    );
    void DrawComposite(D3D12_GPU_DESCRIPTOR_HANDLE sceneSRV, D3D12_GPU_DESCRIPTOR_HANDLE bloomSRV);

private:
    DirectXCommon* dxCommon_ = nullptr;
    BloomConstantBuffer* bloomCB_;

};
