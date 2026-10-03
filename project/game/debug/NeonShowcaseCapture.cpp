#include "NeonShowcaseCapture.h"
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "externals/DirectXTex/DirectXTex.h"
#include <wincodec.h>
#include <fstream>
#include <stdexcept>

namespace {
void Check(HRESULT hr, const char* message) {
    if (FAILED(hr)) throw std::runtime_error(std::string(message) + " HRESULT=" + std::to_string(hr));
}
}

void NeonShowcaseCapture::Request(const std::filesystem::path& directory, const std::string& name, nlohmann::json metadata) {
    if (IsBusy()) return;
    lastCaptureSucceeded_ = false;
    if (name.empty() || std::filesystem::path(name).filename().string() != name) {
        status_ = "Capture requires a local filename."; return;
    }
    path_ = directory / name;
    metadata_ = std::move(metadata);
    requested_ = true;
}

void NeonShowcaseCapture::Record(cg2::DirectXCommon& dx) {
    if (!requested_ || pending_) return;
    try {
        Check(dx.GetSwapChain()->GetBuffer(dx.GetSwapChain()->GetCurrentBackBufferIndex(), IID_PPV_ARGS(&source_)), "Backbuffer capture");
        const auto desc = source_->GetDesc();
        if (desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM || desc.SampleDesc.Count != 1)
            throw std::runtime_error("Showcase capture requires RGBA8 single-sample backbuffer.");
        width_ = static_cast<UINT>(desc.Width); height_ = desc.Height;
        dx.GetDevice()->GetCopyableFootprints(&desc, 0, 1, 0, &footprint_, nullptr, nullptr, &bytes_);
        D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC buffer{};
        buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; buffer.Width = bytes_;
        buffer.Height = 1; buffer.DepthOrArraySize = 1; buffer.MipLevels = 1;
        buffer.SampleDesc.Count = 1; buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        Check(dx.GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback_)), "Readback allocation");
        D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = source_.Get(); barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        dx.GetList()->ResourceBarrier(1, &barrier);
        D3D12_TEXTURE_COPY_LOCATION target{}; target.pResource = readback_.Get();
        target.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; target.PlacedFootprint = footprint_;
        D3D12_TEXTURE_COPY_LOCATION source{}; source.pResource = source_.Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dx.GetList()->CopyTextureRegion(&target, 0, 0, 0, &source, nullptr);
        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        dx.GetList()->ResourceBarrier(1, &barrier);
        fence_ = dx.GetFenceValue() + 1;
        pending_ = true; requested_ = false;
        metadata_["resolution"] = {width_, height_};
        metadata_["resolutionSource"] = "Actual copied backbuffer resource; sceneViewportResolution records the Scene viewport.";
        metadata_["capture"] = "Engine final backbuffer before ImGui; unmodified sRGB-encoded RGBA8";
        status_ = "Waiting for the frame fence.";
    } catch (const std::exception& error) { requested_ = false; status_ = error.what(); }
}

void NeonShowcaseCapture::Resolve(cg2::DirectXCommon& dx) {
    if (!pending_ || dx.GetFence()->GetCompletedValue() < fence_) return;
    uint8_t* bytes = nullptr;
    try {
        D3D12_RANGE range{0, static_cast<SIZE_T>(bytes_)};
        Check(readback_->Map(0, &range, reinterpret_cast<void**>(&bytes)), "Readback Map");
        DirectX::Image image{};
        image.width = width_; image.height = height_; image.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        image.rowPitch = footprint_.Footprint.RowPitch; image.slicePitch = image.rowPitch * height_;
        image.pixels = bytes + footprint_.Offset;
        std::filesystem::create_directories(path_.parent_path());
        auto png = path_; png += ".png";
        Check(DirectX::SaveToWICFile(image, DirectX::WIC_FLAGS_NONE, GUID_ContainerFormatPng, png.c_str()), "PNG save");
        D3D12_RANGE written{0,0}; readback_->Unmap(0, &written); bytes = nullptr;
        auto json = path_; json += ".json";
        std::ofstream stream(json, std::ios::binary); stream << metadata_.dump(2) << '\n';
        if (!stream) throw std::runtime_error("Capture JSON save failed.");
        lastCaptureSucceeded_ = true;
        status_ = "Saved " + png.generic_string();
    } catch (const std::exception& error) {
        if (bytes) { D3D12_RANGE written{0,0}; readback_->Unmap(0,&written); }
        status_ = error.what();
    }
    pending_ = false; source_.Reset(); readback_.Reset();
}
#endif
