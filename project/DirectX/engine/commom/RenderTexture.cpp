#include "RenderTexture.h"

void RenderTexture::Initialize(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    RtvManager* rtvManager,
    uint32_t width,
    uint32_t height
) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    rtvManager_ = rtvManager;

    // RenderTarget用テクスチャ作成
    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    clearValue.Color[0] = 0.0f;
    clearValue.Color[1] = 0.0f;
    clearValue.Color[2] = 0.0f;
    clearValue.Color[3] = 1.0f;

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

    // --- 1. 深度リソース (ID3D12Resource) の作成 ---
    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // PSOと合わせる
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE depthClearValue = {};
    depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthClearValue.DepthStencil.Depth = 1.0f;

    dxCommon->GetDevice()->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue,
        IID_PPV_ARGS(&depthResource_) // RenderTextureのメンバに追加
    );

    // --- 2. DSVハンドルを取得して View を作成 ---
    // ここであなたの作った関数を活用！
    dsvHandle_ = dxCommon->GetNewDsvHandle();

    dxCommon->GetDevice()->CreateDepthStencilView(
        depthResource_.Get(), nullptr, dsvHandle_
    );

}