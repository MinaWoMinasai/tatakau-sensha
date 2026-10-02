#pragma once
#include "DirectXCommon.h"

namespace cg2 {

/// @brief 静的モデル描画で共用するパイプライン・カメラ・ライティングを管理する。
class ModelCommon {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dxCommon);

    /// @brief DirectXの共通基盤を返す。
    DirectXCommon* GetDxCommon() const
    {
        return dxCommon_;
    }

private:
    DirectXCommon* dxCommon_;
};

} // namespace cg2
