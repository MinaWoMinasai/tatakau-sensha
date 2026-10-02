#pragma once
#include "DirectXCommon.h"
#include <vector>

namespace cg2 {

/// @brief SRV/UAV用のディスクリプターヒープと割り当てを管理する。
class SrvManager {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dxCommon);

    /// @brief ディスクリプターなどの利用枠を確保して返す。
    uint32_t Allocate();
    /// @brief 確保した資源を解放する。
    void Free(uint32_t index);
    /// @brief Allocated件数を返す。
    uint32_t GetAllocatedCount() const
    {
        return allocatedCount_;
    }
    /// @brief 高精度水面マーキングを返す。
    uint32_t GetHighWaterMark() const
    {
        return highWaterMark_;
    }

    /// @brief CPUディスクリプターハンドルを返す。
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
    /// @brief GPUディスクリプターハンドルを返す。
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

    // SRV生成(テクスチャ用)
    void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT MipLevels);
    // SRV生成(Structured Buffer用)
    void CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResouce, UINT numElements, UINT structureBytrStrike);
    // SRV生成(シャドウマッピング用)
    void CreateSRVforShadowMap(uint32_t srvIndex, ID3D12Resource* pResource);
    // SRV生成(テクスチャキューブ用)
    void CreateSRVforTextureCube(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT MipLevels);
    // UAV生成(Structured Buffer用)
    void CreateUAVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);
    // UAV生成(RWByteAddressBuffer用)
    void CreateUAVforRawBuffer(uint32_t srvIndex, ID3D12Resource* pResource);

    /// @brief 後続の描画で使う描画先・パイプラインを準備する。対応するPostDrawと組にして使う。
    void PreDraw();

    /// @brief GraphicsルートディスクリプターTableを設定する。
    void SetGraphicsRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex);

    /// @brief SRVヒープを返す。
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetSrvHeap()
    {
        return descriptorHeap_;
    };
    // 最大srv数
    static const uint32_t kMaxSrvCount;

private:
    DirectXCommon* dxCommon_ = nullptr;

    // デスクリプタサイズ
    uint32_t descriptorSize;
    // SRV用デスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;

    // 次に使用するSRVインデックス
    uint32_t useIndex_ = 0;
    uint32_t allocatedCount_ = 0;
    uint32_t highWaterMark_ = 0;
    std::vector<uint32_t> freeIndices_;
    std::vector<bool> allocatedIndices_;
};

} // namespace cg2
