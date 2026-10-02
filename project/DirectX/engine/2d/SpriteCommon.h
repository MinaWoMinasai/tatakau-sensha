#pragma once
#include "DirectXCommon.h"

namespace cg2 {

/// @brief スプライト描画で共有するパイプラインと描画先を管理する。
class SpriteCommon {
public:
    // シングルトン
    static SpriteCommon* GetInstance();

    // 初期化
    void Initialize(DirectXCommon* dxCommon);

    // 共通描画設定
    void PreDraw(BlendMode blendMode = kNone);
    /// @brief シーンを向けの描画前処理を行う。
    void PreDrawForScene(BlendMode blendMode = kNone);

    /// @brief DirectXの共通基盤を返す。
    DirectXCommon* GetDxCommon()
    {
        return dxCommon_;
    }

private:
    DirectXCommon* dxCommon_ = nullptr;
};

} // namespace cg2
