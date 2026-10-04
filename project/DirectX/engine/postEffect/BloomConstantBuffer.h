#pragma once
#include "DirectXCommon.h"
#include <vector>

namespace cg2 {

/// @brief ブルームなどの画面効果のパラメーターをGPUへ転送する。
class BloomConstantBuffer {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dxCommon);
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    void Update(const BloomParam& param);
    /// @brief GPUAddressを返す。
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const;

private:
    struct Snapshot {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        BloomParam* mapped = nullptr;
    };
    DirectXCommon* dxCommon_ = nullptr;
    std::vector<Snapshot> snapshots_;
    size_t nextSnapshot_ = 0;
    size_t currentSnapshot_ = 0;
    uint64_t frameFence_ = 0;
};

} // namespace cg2
