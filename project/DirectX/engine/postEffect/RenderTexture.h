#pragma once
#include "SrvManager.h"
#include "RtvManager.h"
#include <array>

namespace cg2 {

/// @brief 描画先テクスチャとRTV/SRVを管理する。利用時は対応するリソース状態へ遷移させる。
class RenderTexture {
public:
    /// @brief この型の終了処理を行う。所有している資源の寿命を終了させる。
    ~RenderTexture();

    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, RtvManager* rtvManager, uint32_t width, uint32_t height,
                    std::array<float, 4> clearColor = {0.0f, 0.0f, 0.0f, 1.0f}, bool createDepth = true,
                    DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

    /// @brief SRV添字を返す。
    uint32_t GetSrvIndex() const
    {
        return srvIndex_;
    }
    /// @brief RTVハンドルを返す。
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const
    {
        return rtvHandle_;
    }
    /// @brief DSVハンドルを返す。
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const
    {
        return dsvHandle_;
    }

    /// @brief SRV管理を返す。
    SrvManager* GetSrvManager()
    {
        return srvManager_;
    }

    /// @brief リソースを返す。
    ID3D12Resource* GetResource()
    {
        return resource_.Get();
    }
    /// @brief 深度リソースを返す。
    ID3D12Resource* GetDepthResource()
    {
        return depthResource_.Get();
    }

    /// @brief GPUハンドルを返す。
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle();
    /// @brief 深度GPUハンドルを返す。
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
