#include "ShadowMap.h"

void ShadowMap::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height) {
    auto device = dxCommon->GetDevice();

    // 1. リソース作成 (R32_TYPELESS)
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1; // ★必須：1 以上にする必要があります
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R32_TYPELESS;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0; // ★明示的に 0 を指定
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; // ★明示的に指定
    desc.Alignment = 0;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&resource_));
    
    assert(SUCCEEDED(hr));
    assert(resource_ != nullptr);
    
    // 2. DSV作成 (書き込み用)
    // ※dxCommon側でシャドウ用のDSVヒープ管理が必要。
    // ここでは簡略化のためdxCommonからハンドルをもらう想定
    dsvHandle_ = dxCommon->GetNewDsvHandle();

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(resource_.Get(), &dsvDesc, dsvHandle_);

    // 3. SRV作成 (読み取り用)
    srvIndex_ = srvManager->Allocate();
    srvManager->CreateSRVforShadowMap(srvIndex_, resource_.Get());

}