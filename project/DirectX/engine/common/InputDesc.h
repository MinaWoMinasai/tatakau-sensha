#pragma once
#include <d3d12.h>
#include <span>

namespace myEngine {

/// @brief 頂点入力の要素配置を保持し、パイプライン作成時に提供する。
class InputDesc {
public:
    void Initialize();

    /// @brief 軌跡用の頂点レイアウトにする。
    void InitializeForTrail();
    /// @brief スキニング用の頂点レイアウトにする。
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

} // namespace myEngine
