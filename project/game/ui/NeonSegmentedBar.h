#pragma once

#include <memory>
#include <vector>

#include "game/ui/NeonProgressBar.h"

// 任意の最大強化段数を、小さな区切りセルとして表示するHUD部品。
/// @brief ネオンの分割バーの区切り・色・発光を指定する。
struct NeonSegmentedBarStyle {
    cg2::Vector4 backgroundColor{0.012f, 0.030f, 0.052f, 0.90f};
    cg2::Vector4 emptyColor{0.055f, 0.100f, 0.145f, 0.94f};
    cg2::Vector4 filledColor{0.38f, 1.00f, 0.62f, 0.98f};
    cg2::Vector4 outlineColor{0.40f, 0.94f, 1.00f, 0.92f};
    float innerPadding = 2.0f;
    float segmentGap = 2.0f;
    float bloomBrightness = 1.75f;
    float bloomAlpha = 0.62f;
    float backdropBloomBrightness = 1.65f;
    float backdropBloomAlpha = 0.16f;
    bool roundedFrame = true;
};

/// @brief 値と最大値に応じたネオンの分割バーを更新・描画する。
class NeonSegmentedBar {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(cg2::SpriteCommon* spriteCommon);
    /// @brief 外観を設定する。
    void SetStyle(const NeonSegmentedBarStyle& style);
    /// @brief 境界を設定する。
    void SetBounds(const cg2::Vector2& position, const cg2::Vector2& size);
    /// @brief 区切り件数を設定する。
    void SetSegmentCount(int count);
    /// @brief FilledSegmentsを設定する。
    void SetFilledSegments(int count);
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    void Update();
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw();
    /// @brief ブルーム元データを描画する。
    void DrawBloomSource();

private:
    /// @brief 区切りスプライトを必要な状態を用意する。
    void EnsureSegmentSprites();
    /// @brief 色の明るさへ倍率を掛ける。
    static cg2::Vector4 MultiplyBrightness(const cg2::Vector4& color, float brightness, float alpha);

    cg2::SpriteCommon* spriteCommon_ = nullptr;
    NeonSegmentedBarStyle style_{};
    cg2::Vector2 position_{};
    cg2::Vector2 size_{};
    int segmentCount_ = 1;
    int filledSegments_ = 0;
    bool dirty_ = true;
    std::unique_ptr<NeonProgressBar> frame_;
    std::unique_ptr<cg2::Sprite> bloomBackdrop_;
    std::vector<std::unique_ptr<cg2::Sprite>> segments_;
    std::vector<std::unique_ptr<cg2::Sprite>> bloomSegments_;
};
