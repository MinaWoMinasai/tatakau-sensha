#include "ShadowMap.h"

void ShadowMap::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height) {
    auto device = dxCommon->GetDevice();

    // 1. リソース作成 (R24G8_TYPELESS)
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    // PSOの D24_S8 と互換性のある TYPELESS フォーマット
    desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Alignment = 0;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    // クリア値の設定 (D24_S8)
    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, // 初期状態は読み取り用にしておくのが安全
        &clearValue, IID_PPV_ARGS(&resource_));

    assert(SUCCEEDED(hr));

    // 2. DSV作成 (書き込み用)
    dsvHandle_ = dxCommon->GetNewDsvHandle();

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // PSOと一致させる
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    device->CreateDepthStencilView(resource_.Get(), &dsvDesc, dsvHandle_);

    // 3. SRV作成 (読み取り用)
    srvIndex_ = srvManager->Allocate();
    // SRV作成関数内で DXGI_FORMAT_R24_UNORM_X8_TYPELESS を指定しているか確認が必要
    srvManager->CreateSRVforShadowMap(srvIndex_, resource_.Get());
}