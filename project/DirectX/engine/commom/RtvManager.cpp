#include "RtvManager.h"

const uint32_t RtvManager::kMaxRtvCount = 16;

void RtvManager::Initialize(DirectXCommon* dxCommon) {
    
    dxCommon_ = dxCommon;

    heap_ = dxCommon_->CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        kMaxRtvCount,
        false
    );
    descriptorSize_ = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV
    );
}

uint32_t RtvManager::Allocate() {
    assert(useIndex_ < kMaxRtvCount);
    return useIndex_++;
}

D3D12_CPU_DESCRIPTOR_HANDLE RtvManager::GetHandle(uint32_t index) {
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        heap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += descriptorSize_ * index;
    return handle;
}