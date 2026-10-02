#pragma once
#include "SrvManager.h"
#include "RtvManager.h"
#include <array>

namespace cg2 {

class RenderTexture {
public:
	~RenderTexture();

    void Initialize(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        RtvManager* rtvManager,
        uint32_t width,
        uint32_t height,
        std::array<float, 4> clearColor = { 0.0f, 0.0f, 0.0f, 1.0f },
        bool createDepth = true,
        DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
    );

    uint32_t GetSrvIndex() const { return srvIndex_; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const { return rtvHandle_; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const { return dsvHandle_; }

    SrvManager* GetSrvManager() { return srvManager_; }

    ID3D12Resource* GetResource() { return resource_.Get(); }
    ID3D12Resource* GetDepthResource() { return depthResource_.Get(); }

    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle();
    D3D12_GPU_DESCRIPTOR_HANDLE GetDepthGPUHandle();

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    
	uint32_t srvIndex_ = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    RtvManager* rtvManager_ = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_;
    Microsoft::WRL::ComPtr<ID3D12Resource> depthResource_;
    uint32_t depthSrvIndex_ = 0;
};

} // namespace cg2
