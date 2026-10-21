#include "DynamicTexture.h"
#include "DirectXCommon.h"
#include "SrvManager.h"

#include <cstring>

namespace myEngine {

namespace {

constexpr uint32_t kBytesPerPixel = 4;

/// @brief ヒープの種類だけを指定した、既定のヒープ設定。
D3D12_HEAP_PROPERTIES HeapProperties(D3D12_HEAP_TYPE type)
{
    D3D12_HEAP_PROPERTIES properties{};
    properties.Type = type;
    return properties;
}

/// @brief 指定した大きさのバッファの説明。
D3D12_RESOURCE_DESC BufferDesc(UINT64 size)
{
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return desc;
}

} // namespace

DynamicTexture::~DynamicTexture()
{
    if (uploadBuffer_ && uploadData_) {
        uploadBuffer_->Unmap(0, nullptr);
    }
    // まだ実行していないコピーやバリアが残っているかもしれないので、GPU が終わるまで資源を預ける。
    if (dxCommon_) {
        if (texture_) {
            dxCommon_->ReleaseAfterExecution(std::move(texture_));
        }
        if (uploadBuffer_) {
            dxCommon_->ReleaseAfterExecution(std::move(uploadBuffer_));
        }
    }
    if (hasSrv_ && srvManager_) {
        srvManager_->Free(srvIndex_);
    }
}

bool DynamicTexture::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height, bool srgb)
{
    if (!dxCommon || !srvManager || width == 0 || height == 0 || texture_) {
        return false;
    }
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    width_ = width;
    height_ = height;
    ID3D12Device* device = dxCommon_->GetDevice().Get();
    const DXGI_FORMAT format = srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;

    D3D12_RESOURCE_DESC textureDesc{};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = format;
    textureDesc.SampleDesc.Count = 1;
    const D3D12_HEAP_PROPERTIES defaultHeap = HeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    if (FAILED(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                               IID_PPV_ARGS(&texture_)))) {
        return false;
    }
    state_ = D3D12_RESOURCE_STATE_COPY_DEST;

    // 送り用のバッファは、GPU の求める行の並び（行の先頭を 256 バイト境界にそろえる）で確保する。
    device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint_, nullptr, nullptr, &totalBytes_);
    const D3D12_HEAP_PROPERTIES uploadHeap = HeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    const D3D12_RESOURCE_DESC uploadDesc = BufferDesc(totalBytes_);
    if (FAILED(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                               IID_PPV_ARGS(&uploadBuffer_)))) {
        texture_.Reset();
        return false;
    }
    uploadBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&uploadData_));

    srvIndex_ = srvManager_->Allocate();
    hasSrv_ = true;
    srvManager_->CreateSRVforTexture2D(srvIndex_, texture_.Get(), format, 1);

    // 中身が決まっていない状態で使われないよう、透明な黒で埋めておく。
    const std::vector<uint8_t> clear(size_t(width) * height * kBytesPerPixel, 0);
    return Update(clear);
}

bool DynamicTexture::Update(std::span<const uint8_t> rgba)
{
    if (!texture_ || rgba.size() != size_t(width_) * height_ * kBytesPerPixel) {
        return false;
    }
    const size_t rowBytes = size_t(width_) * kBytesPerPixel;
    uint8_t* destination = uploadData_ + footprint_.Offset;
    for (uint32_t row = 0; row < height_; ++row) {
        std::memcpy(destination + size_t(row) * footprint_.Footprint.RowPitch, rgba.data() + row * rowBytes, rowBytes);
    }

    Transition(D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION destinationLocation{};
    destinationLocation.pResource = texture_.Get();
    destinationLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destinationLocation.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION sourceLocation{};
    sourceLocation.pResource = uploadBuffer_.Get();
    sourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    sourceLocation.PlacedFootprint = footprint_;
    dxCommon_->GetList()->CopyTextureRegion(&destinationLocation, 0, 0, 0, &sourceLocation, nullptr);
    Transition(D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    return true;
}

bool DynamicTexture::ReadBack(std::vector<uint8_t>& rgba)
{
    if (!texture_) {
        return false;
    }
    Microsoft::WRL::ComPtr<ID3D12Resource> readback;
    const D3D12_HEAP_PROPERTIES readbackHeap = HeapProperties(D3D12_HEAP_TYPE_READBACK);
    const D3D12_RESOURCE_DESC readbackDesc = BufferDesc(totalBytes_);
    if (FAILED(dxCommon_->GetDevice()->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &readbackDesc,
                                                               D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)))) {
        return false;
    }
    const D3D12_RESOURCE_STATES previous = state_;
    Transition(D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION destinationLocation{};
    destinationLocation.pResource = readback.Get();
    destinationLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    destinationLocation.PlacedFootprint = footprint_;
    D3D12_TEXTURE_COPY_LOCATION sourceLocation{};
    sourceLocation.pResource = texture_.Get();
    sourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    sourceLocation.SubresourceIndex = 0;
    dxCommon_->GetList()->CopyTextureRegion(&destinationLocation, 0, 0, 0, &sourceLocation, nullptr);
    Transition(previous);
    // ここまでに積んだコマンドを実行して、終わるのを待つ（読み込み時のテクスチャ転送と同じ方法）。
    dxCommon_->ExecuteCommandListAndWait();

    uint8_t* mapped = nullptr;
    const D3D12_RANGE readRange{0, static_cast<SIZE_T>(totalBytes_)};
    if (FAILED(readback->Map(0, &readRange, reinterpret_cast<void**>(&mapped)))) {
        return false;
    }
    const size_t rowBytes = size_t(width_) * kBytesPerPixel;
    rgba.resize(rowBytes * height_);
    for (uint32_t row = 0; row < height_; ++row) {
        std::memcpy(rgba.data() + row * rowBytes, mapped + footprint_.Offset + size_t(row) * footprint_.Footprint.RowPitch, rowBytes);
    }
    const D3D12_RANGE writtenRange{0, 0};
    readback->Unmap(0, &writtenRange);
    return true;
}

D3D12_GPU_DESCRIPTOR_HANDLE DynamicTexture::GetSrvHandleGPU() const
{
    return srvManager_->GetGPUDescriptorHandle(srvIndex_);
}

void DynamicTexture::Transition(D3D12_RESOURCE_STATES after)
{
    if (state_ == after) {
        return;
    }
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = texture_.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = state_;
    barrier.Transition.StateAfter = after;
    dxCommon_->GetList()->ResourceBarrier(1, &barrier);
    state_ = after;
}

} // namespace myEngine
