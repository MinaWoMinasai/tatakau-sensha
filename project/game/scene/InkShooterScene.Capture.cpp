#include "InkShooterScene.h"
#include "Object3dCommon.h"
#include "externals/DirectXTex/DirectXTex.h"
#include <filesystem>
#include <string>

void InkShooterScene::RequestCapture(const std::string& name) {
    if (!capturePath_.empty()) return;
    const std::string directory=extendedReplay_?"generated/ink_phase2":fidelityReplay_?"generated/ink_phase3":weaponsReplay_?"generated/ink_phase4":"generated/ink_phase5";
    std::filesystem::create_directories(directory);
    capturePath_=directory+"/"+name+".png";
}

// A photo copies the presented scene once, after its HUD. It never reads the
// ownership mask. Normal frames read only the separate 32-byte GPU timer.
void InkShooterScene::CopyCapture() {
    if (capturePath_.empty() || captureCopied_) return;
    auto dx=Object3dCommon::GetInstance()->GetDxCommon();
    Microsoft::WRL::ComPtr<ID3D12Resource> source;
    if (FAILED(dx->GetSwapChain()->GetBuffer(dx->GetSwapChain()->GetCurrentBackBufferIndex(),IID_PPV_ARGS(&source)))) return;
    auto desc=source->GetDesc();
    UINT64 size=0;
    dx->GetDevice()->GetCopyableFootprints(&desc,0,1,0,&captureLayout_,nullptr,nullptr,&size);
    D3D12_HEAP_PROPERTIES heap{}; heap.Type=D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC buffer{}; buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width=size; buffer.Height=1; buffer.DepthOrArraySize=1; buffer.MipLevels=1;
    buffer.SampleDesc.Count=1; buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(dx->GetDevice()->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,
        D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&captureReadback_)))) {
        capturePath_.clear(); return;
    }
    D3D12_RESOURCE_BARRIER barrier{}; barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource=source.Get(); barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_RENDER_TARGET; barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
    auto list=dx->GetList(); list->ResourceBarrier(1,&barrier);
    D3D12_TEXTURE_COPY_LOCATION from{},to{};
    from.pResource=source.Get(); from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    to.pResource=captureReadback_.Get(); to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    to.PlacedFootprint=captureLayout_;
    list->CopyTextureRegion(&to,0,0,0,&from,nullptr);
    std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter);
    list->ResourceBarrier(1,&barrier); captureCopied_=true;
}

void InkShooterScene::FinishCapture() {
    // DirectXCommon::PostDraw waits for the submitted frame fence. Therefore
    // next Update can encode the completed readback without another GPU stall.
    if (!captureCopied_) return;
    void* pixels=nullptr;
    if (SUCCEEDED(captureReadback_->Map(0,nullptr,&pixels))) {
        const auto& layout=captureLayout_.Footprint;
        DirectX::Image photo{};
        photo.width=layout.Width; photo.height=layout.Height;
        photo.format=layout.Format; photo.rowPitch=layout.RowPitch;
        photo.slicePitch=photo.rowPitch*photo.height;
        photo.pixels=static_cast<uint8_t*>(pixels)+captureLayout_.Offset;
        const auto output=std::filesystem::path(capturePath_);
        const HRESULT result=DirectX::SaveToWICFile(photo,DirectX::WIC_FLAGS_NONE,
            DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),output.c_str());
        if (FAILED(result)) OutputDebugStringA("[Ink] screenshot PNG encoding failed.\n");
        D3D12_RANGE writes{0,0}; captureReadback_->Unmap(0,&writes);
    }
    captureReadback_.Reset(); capturePath_.clear(); captureCopied_=false;
}
