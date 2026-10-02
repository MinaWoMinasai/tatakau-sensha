#pragma once
#include "DirectXCommon.h"

namespace cg2 {

class BloomConstantBuffer
{
public:
    void Initialize(DirectXCommon* dxCommon);
    void Update(const BloomParam& param);
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const;

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    BloomParam* mappedData_ = nullptr;
};

} // namespace cg2
