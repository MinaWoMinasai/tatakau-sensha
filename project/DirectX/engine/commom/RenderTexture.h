#pragma once
#include "SrvManager.h"
#include "RtvManager.h"

class RenderTexture {
public:
    void Initialize(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        RtvManager* rtvManager,
        uint32_t width,
        uint32_t height
    );

    uint32_t GetSrvIndex() const { return srvIndex_; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const { return rtvHandle_; }

    SrvManager* GetSrvManager() { return srvManager_; }

    ID3D12Resource* GetResource() { return resource_.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    
    uint32_t srvIndex_;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    RtvManager* rtvManager_ = nullptr;
};

