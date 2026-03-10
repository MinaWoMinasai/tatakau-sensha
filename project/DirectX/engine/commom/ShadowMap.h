#pragma once
#include "DirectXCommon.h"
#include "SrvManager.h"
#include <wrl.h>

class ShadowMap {
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height);

    ID3D12Resource* GetResource() { return resource_.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() { return dsvHandle_; }
    uint32_t GetSrvIndex() const { return srvIndex_; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_;
    uint32_t srvIndex_;
};