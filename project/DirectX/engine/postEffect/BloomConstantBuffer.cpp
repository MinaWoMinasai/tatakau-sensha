#include "BloomConstantBuffer.h"

void BloomConstantBuffer::Initialize(DirectXCommon* dxCommon)
{
    auto device = dxCommon->GetDevice();

    // 定数バッファは 256byte 境界
    uint32_t size =
        (sizeof(BloomParam) + 0xff) & ~0xff;

    CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC resourceDesc =
        CD3DX12_RESOURCE_DESC::Buffer(size);

    HRESULT hr = device->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&resource_));

    assert(SUCCEEDED(hr));

    // Map（1回だけ）
    resource_->Map(0, nullptr,
        reinterpret_cast<void**>(&mappedData_));
}

void BloomConstantBuffer::Update(const BloomParam& param)
{
    *mappedData_ = param;
}

D3D12_GPU_VIRTUAL_ADDRESS BloomConstantBuffer::GetGPUAddress() const
{
    return resource_->GetGPUVirtualAddress();
}