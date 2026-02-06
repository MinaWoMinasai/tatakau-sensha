#pragma once
#include "DirectXCommon.h"

class RtvManager {
public:
    void Initialize(DirectXCommon* dxCommon);

    uint32_t Allocate();
    D3D12_CPU_DESCRIPTOR_HANDLE GetHandle(uint32_t index);
    
    // 最大rtv数
    static const uint32_t kMaxRtvCount;

private:
    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
    uint32_t descriptorSize_ = 0;
    uint32_t useIndex_ = 0;
};

