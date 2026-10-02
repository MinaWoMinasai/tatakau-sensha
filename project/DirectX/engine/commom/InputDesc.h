#pragma once
#include <d3d12.h>
#include <span>

namespace cg2 {

/// @brief 頂点入力の要素配置を保持し、パイプライン作成時に提供する。
class InputDesc {

public:
    /// @brief 初期化
    void Initialize();

    /// @brief 用の軌跡を初期化する。
    void InitializeForTrail();
    /// @brief 用のスキニングを初期化する。
    void InitializeForSkinning();

    /// @brief Element記述情報を返す。
    std::span<const D3D12_INPUT_ELEMENT_DESC> GetElementDescs() const
    {
        return std::span{ElementDescs_, _countof(ElementDescs_)};
    }
    /// @brief 配置を返す。
    D3D12_INPUT_LAYOUT_DESC GetLayout()
    {
        return Layout_;
    }

private:
    // InputLayout
    D3D12_INPUT_ELEMENT_DESC ElementDescs_[6] = {};
    D3D12_INPUT_LAYOUT_DESC Layout_{};
};

} // namespace cg2
