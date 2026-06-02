#include "RenderTexture.h"

void RenderTexture::Initialize(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    RtvManager* rtvManager,
    uint32_t width,
    uint32_t height,
    std::array<float, 4> clearColor,
    bool createDepth
) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    rtvManager_ = rtvManager;

    // RenderTarget用テクスチャ作成
    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    clearValue.Color[0] = clearColor[0];
    clearValue.Color[1] = clearColor[1];
    clearValue.Color[2] = clearColor[2];
    clearValue.Color[3] = clearColor[3];

    resource_ = dxCommon_->CreateTextureResource(
        width,
        height,
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        &clearValue
    );

    // RTV（← ここが RtvManager）
    uint32_t rtvIndex = rtvManager_->Allocate();
    rtvHandle_ = rtvManager_->GetHandle(rtvIndex);

    dxCommon_->GetDevice()->CreateRenderTargetView(
        resource_.Get(),
        nullptr,
        rtvManager_->GetHandle(rtvIndex)
    );

    // SRV（← ここが SrvManager）
    srvIndex_ = srvManager_->Allocate();
    srvManager_->CreateSRVforTexture2D(
        srvIndex_,
        resource_.Get(),
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        1
    );

    if (createDepth) {
        // --- 1. 深度リソース (ID3D12Resource) の作成 ---
        D3D12_RESOURCE_DESC depthDesc = {};
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.MipLevels = 1;
        depthDesc.DepthOrArraySize = 1;
        // DSVとして書き込みつつ、SRVとして深度値を読めるように typeless で確保する。
        depthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_CLEAR_VALUE depthClearValue = {};
        depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthClearValue.DepthStencil.Depth = 1.0f;

        HRESULT hr = dxCommon->GetDevice()->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &depthDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue,
            IID_PPV_ARGS(&depthResource_)
        );
        assert(SUCCEEDED(hr));

        // --- 2. DSVハンドルを取得して View を作成 ---
        dsvHandle_ = dxCommon->GetNewDsvHandle();

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
        dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dxCommon->GetDevice()->CreateDepthStencilView(depthResource_.Get(), &dsvDesc, dsvHandle_);

        depthSrvIndex_ = srvManager_->Allocate();
        srvManager_->CreateSRVforShadowMap(depthSrvIndex_, depthResource_.Get());
    }

    // --- ここを追加 ---
    // 生成直後は RENDER_TARGET 状態であることが多いため、
    // 最初の使用に備えて PIXEL_SHADER_RESOURCE に遷移させておく
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; // 生成時の状態
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    dxCommon_->GetList()->ResourceBarrier(1, &barrier);

    if (depthResource_) {
        D3D12_RESOURCE_BARRIER depthBarrier{};
        depthBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        depthBarrier.Transition.pResource = depthResource_.Get();
        depthBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        depthBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        depthBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        dxCommon_->GetList()->ResourceBarrier(1, &depthBarrier);
    }

    // このコマンドを即座に実行するか、あるいはコマンドリストを Close/Execute する仕組みが必要です
    // TextureManager などと同様に、初期化用のコマンドリスト実行を呼んでください
    dxCommon_->ExecuteCommandListAndWait();
}

D3D12_GPU_DESCRIPTOR_HANDLE RenderTexture::GetGPUHandle()
{
	return srvManager_->GetGPUDescriptorHandle(srvIndex_);
}

D3D12_GPU_DESCRIPTOR_HANDLE RenderTexture::GetDepthGPUHandle()
{
    assert(depthResource_ != nullptr);
    return srvManager_->GetGPUDescriptorHandle(depthSrvIndex_);
}
