#include "BloomConstantBuffer.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cg2 {

void BloomConstantBuffer::Initialize(DirectXCommon* dxCommon) {
    dxCommon_ = dxCommon;
    frameFence_ = dxCommon_->GetFenceValue();
    Update(BloomParam{});
}

void BloomConstantBuffer::Update(const BloomParam& param) {
    const auto fence = dxCommon_->GetFence();
    const uint64_t fenceValue = dxCommon_->GetFenceValue();
    // Recycle only after DirectXCommon's submitted frame has completed.
    // Updates in one command list retain independent immutable GPU snapshots.
    if (fence && fenceValue > frameFence_ && fence->GetCompletedValue() >= fenceValue) {
        frameFence_ = fenceValue;
        nextSnapshot_ = 0;
    }
    if (nextSnapshot_ == snapshots_.size()) {
        Snapshot snapshot;
        const auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        const auto desc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(BloomParam) + 255u) & ~255u);
        HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, IID_PPV_ARGS(&snapshot.resource));
        if (FAILED(hr)) throw std::runtime_error("Bloom constant snapshot allocation failed");
        const D3D12_RANGE noRead{0, 0};
        hr = snapshot.resource->Map(0, &noRead, reinterpret_cast<void**>(&snapshot.mapped));
        if (FAILED(hr)) throw std::runtime_error("Bloom constant snapshot mapping failed");
        snapshots_.push_back(std::move(snapshot));
    }
    currentSnapshot_ = nextSnapshot_++;
    BloomParam safe = param;
    const auto finiteRange = [](float value, float fallback, float high) {
        return std::isfinite(value) ? std::clamp(value, 0.0f, high) : fallback;
    };
    safe.bloomMode = safe.bloomMode <= 3 ? safe.bloomMode : 2;
    safe.bloomSoftKnee = finiteRange(safe.bloomSoftKnee, 0.5f, 1.0f);
    safe.bloomScatter = finiteRange(safe.bloomScatter, 0.3f, 0.9f);
    safe.bloomRadius = (std::max)(0.25f, finiteRange(safe.bloomRadius, 0.75f, 2.0f));
    safe.bloomGain = finiteRange(safe.bloomGain, 0.15f, 4.0f);
    safe.bloomOutputToHdr = safe.bloomOutputToHdr != 0;
    *snapshots_[currentSnapshot_].mapped = safe;
}

D3D12_GPU_VIRTUAL_ADDRESS BloomConstantBuffer::GetGPUAddress() const {
    return snapshots_.at(currentSnapshot_).resource->GetGPUVirtualAddress();
}

} // namespace cg2
