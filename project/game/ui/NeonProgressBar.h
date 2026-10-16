#pragma once

#include <memory>

#include "Sprite.h"

/// @brief ネオンの連続バーの色・太さ・発光を指定する。
struct NeonProgressBarStyle {
    myEngine::Vector4 backgroundColor{0.025f, 0.040f, 0.075f, 0.88f};
    myEngine::Vector4 delayedFillColor{0.30f, 0.80f, 1.00f, 0.48f};
    myEngine::Vector4 fillColor{0.96f, 0.83f, 0.24f, 0.96f};
    myEngine::Vector4 outlineColor{0.90f, 1.00f, 0.72f, 0.92f};
    float fillResponse = 13.0f;
    float delayedResponse = 4.0f;
    float bloomBrightness = 2.15f;
    float bloomAlpha = 0.72f;
    float outlineWidth = 1.5f;
    // 端だけを等倍率で描くため、長さによって端部が太らない。
    bool roundedEnds = false;
};

/// @brief 進行度に応じたネオンの連続バーを更新・描画する。
class NeonProgressBar {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(myEngine::SpriteCommon* spriteCommon);
    /// @brief 外観を設定する。
    void SetStyle(const NeonProgressBarStyle& style);
    /// @brief 境界を設定する。
    void SetBounds(const myEngine::Vector2& position, const myEngine::Vector2& size);
    /// @brief 対象を設定する。
    void SetTarget(float normalizedValue);
    /// @brief 表示値を指定値へ即座に合わせる。
    void SnapTo(float normalizedValue);
    // レベルアップ時は満タンまで補間してから新しいレベルの経験値へ移る。
    void BeginRollover(float normalizedValue);
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(float deltaTime);
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw();
    /// @brief ブルーム元データを描画する。
    void DrawBloomSource();

    /// @brief 表示値を返す。
    float GetDisplayValue() const
    {
        return displayValue_;
    }

private:
    /// @brief スプライトを更新する。
    void UpdateSprites();
    /// @brief 色の明るさへ倍率を掛ける。
    static myEngine::Vector4 MultiplyBrightness(const myEngine::Vector4& color, float brightness, float alpha);

    myEngine::SpriteCommon* spriteCommon_ = nullptr;
    NeonProgressBarStyle style_{};
    myEngine::Vector2 position_{};
    myEngine::Vector2 size_{};
    float targetValue_ = 0.0f;
    float displayValue_ = 0.0f;
    float delayedValue_ = 0.0f;
    float rolloverTargetValue_ = 0.0f;
    bool hasValue_ = false;
    bool rolloverActive_ = false;
    bool dirty_ = true;

    std::unique_ptr<myEngine::Sprite> background_;
    std::unique_ptr<myEngine::Sprite> delayedFill_;
    std::unique_ptr<myEngine::Sprite> fill_;
    std::unique_ptr<myEngine::Sprite> outlineTop_;
    std::unique_ptr<myEngine::Sprite> outlineBottom_;
    std::unique_ptr<myEngine::Sprite> outlineLeft_;
    std::unique_ptr<myEngine::Sprite> outlineRight_;
    std::unique_ptr<myEngine::Sprite> bloomFill_;
    std::unique_ptr<myEngine::Sprite> bloomOutlineTop_;
    std::unique_ptr<myEngine::Sprite> bloomOutlineBottom_;
    std::unique_ptr<myEngine::Sprite> bloomOutlineLeft_;
    std::unique_ptr<myEngine::Sprite> bloomOutlineRight_;

    // カプセル版: 中央は矩形、左右だけをマスクから等倍率で描く。
    std::unique_ptr<myEngine::Sprite> roundedBackgroundLeft_;
    std::unique_ptr<myEngine::Sprite> roundedBackgroundRight_;
    std::unique_ptr<myEngine::Sprite> roundedDelayedLeft_;
    std::unique_ptr<myEngine::Sprite> roundedDelayedRight_;
    std::unique_ptr<myEngine::Sprite> roundedFillLeft_;
    std::unique_ptr<myEngine::Sprite> roundedFillRight_;
    std::unique_ptr<myEngine::Sprite> roundedOutlineLeft_;
    std::unique_ptr<myEngine::Sprite> roundedOutlineCenter_;
    std::unique_ptr<myEngine::Sprite> roundedOutlineRight_;
    std::unique_ptr<myEngine::Sprite> roundedBloomFillLeft_;
    std::unique_ptr<myEngine::Sprite> roundedBloomFillRight_;
    std::unique_ptr<myEngine::Sprite> roundedBloomOutlineLeft_;
    std::unique_ptr<myEngine::Sprite> roundedBloomOutlineCenter_;
    std::unique_ptr<myEngine::Sprite> roundedBloomOutlineRight_;
};
