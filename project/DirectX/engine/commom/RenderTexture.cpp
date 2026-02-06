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
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    clearValue.Color[0] = 0.0f;
    clearValue.Color[1] = 0.0f;
    clearValue.Color[2] = 0.0f;
    clearValue.Color[3] = 1.0f;

    resource_ = dxCommon_->CreateTextureResource(
        width,
        height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
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
        DXGI_FORMAT_R8G8B8A8_UNORM,
        1
    );
}