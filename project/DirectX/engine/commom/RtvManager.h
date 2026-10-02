#pragma once
#include "DirectXCommon.h"

namespace cg2 {

/// @brief レンダーターゲット用のディスクリプターヒープと割り当てを管理する。
class RtvManager {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dxCommon);

    /// @brief ディスクリプターなどの利用枠を確保して返す。
    uint32_t Allocate();
    /// @brief ハンドルを返す。
    D3D12_CPU_DESCRIPTOR_HANDLE GetHandle(uint32_t index);

    // 最大rtv数
    static const uint32_t kMaxRtvCount;

private:
    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
    uint32_t descriptorSize_ = 0;
    uint32_t useIndex_ = 0;
};

} // namespace cg2
