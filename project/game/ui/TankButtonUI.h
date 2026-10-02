#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "Sprite.h"
#include "TextLabel.h"
#include "game/weapon/WeaponMount.h"

namespace cg2 {
class ObjectPostEffect;
}

enum class TankButtonBodyShape {
    Circle = 0,
    Box,
    Triangle,
    Pentagon,
};

enum class TankButtonState {
    Normal = 0,
    Hover,
    Selected,
    Locked,
};

/// @brief 戦車を模したボタンの形状と表示情報を表す。
struct TankButtonVisualData {
    std::string classId = "Basic";
    std::string hiraganaName = "べーしっく";
    int rank = 1;
    TankButtonBodyShape bodyShape = TankButtonBodyShape::Circle;
    cg2::Vector2 bodyScale{1.0f, 1.0f};
    cg2::Vector4 bodyFillColor{0.10f, 0.22f, 0.30f, 0.95f};
    cg2::Vector4 bodyOutlineColor{0.58f, 1.0f, 0.72f, 1.0f};
    std::vector<WeaponMountConfig> weaponMounts;
    bool usesDrone = false;
};

/// @brief 戦車ボタンの通常・選択・無効時の外観を指定する。
struct TankButtonUiStyle {
    float buttonWidth = 150.0f;
    float buttonHeight = 150.0f;
    float cornerRadius = 16.0f;
    float borderWidth = 1.6f;

    float glowWidth = 10.0f;
    float glowIntensity = 0.78f;
    float bloomBoost = 0.22f;
    float normalGlowMultiplier = 0.42f;
    float hoverGlowMultiplier = 0.78f;
    float selectedGlowMultiplier = 1.15f;
    float lockedGlowMultiplier = 0.10f;

    cg2::Vector4 fillColor{0.012f, 0.026f, 0.052f, 1.0f};
    float fillOpacity = 0.94f;
    std::array<cg2::Vector4, 4> borderColors{
        cg2::Vector4{0.58f, 1.0f, 0.28f, 1.0f},
        cg2::Vector4{0.22f, 0.76f, 1.0f, 1.0f},
        cg2::Vector4{0.76f, 0.36f, 1.0f, 1.0f},
        cg2::Vector4{1.0f, 0.78f, 0.24f, 1.0f},
    };
    std::array<cg2::Vector4, 4> glowColors{
        cg2::Vector4{0.48f, 1.0f, 0.24f, 1.0f},
        cg2::Vector4{0.14f, 0.70f, 1.0f, 1.0f},
        cg2::Vector4{0.70f, 0.24f, 1.0f, 1.0f},
        cg2::Vector4{1.0f, 0.66f, 0.12f, 1.0f},
    };
    cg2::Vector4 lockedTint{0.38f, 0.44f, 0.50f, 0.62f};

    float iconScale = 1.0f;
    float iconOffsetY = -22.0f;
    float labelOffsetY = 49.0f;
    float labelFontSize = 17.0f;
    std::string labelFontFamily = "Meiryo";
    std::string labelFontPath;
    int labelFontWeight = 400;
    cg2::Vector4 labelColor{0.94f, 0.98f, 1.0f, 1.0f};
    cg2::Vector4 labelOutlineColor{0.0f, 0.02f, 0.05f, 0.82f};
    float labelOutlineWidth = 0.45f;

    float buttonSpacingX = 22.0f;
    float buttonSpacingY = 28.0f;
    cg2::Vector2 previewAreaOffset{240.0f, 132.0f};
    int previewAreaColumns = 4;
};

/// @brief 戦車ボタンUI外観を読み込む。
bool LoadTankButtonUiStyle(TankButtonUiStyle& style, const std::string& path = "resources/configs/tankButtonUiStyle.json",
                           std::string* status = nullptr);
/// @brief 戦車ボタンUI外観を保存する。
bool SaveTankButtonUiStyle(const TankButtonUiStyle& style, const std::string& path = "resources/configs/tankButtonUiStyle.json",
                           std::string* status = nullptr);

/// @brief 戦車を模したボタンの入力状態とネオン描画を管理する。
class TankButtonUI {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(cg2::SpriteCommon* spriteCommon);
    /// @brief 表示データを設定する。
    void SetVisualData(const TankButtonVisualData& visualData);
    /// @brief ランクを設定する。
    void SetRank(int rank);
    /// @brief 状態を設定する。
    void SetState(TankButtonState state);
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    void Update(const cg2::Vector2& center, const TankButtonUiStyle& style);
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw();
    /// @brief ブルーム元データを描画する。
    void DrawBloomSource();
    /// @brief 文字を返す。
    cg2::TextLabel* GetLabel() const
    {
        return label_.get();
    }

private:
    static constexpr size_t kFrameSegmentCount = 68;
    static constexpr size_t kFrameLayerCount = 3;
    static constexpr size_t kIconSpriteCount = 96;
    static constexpr size_t kFillSpriteCount = 2;

    /// @brief フレームを更新する。
    void UpdateFrame(const cg2::Vector2& center, const TankButtonUiStyle& style);
    /// @brief Iconを更新する。
    void UpdateIcon(const cg2::Vector2& center, const TankButtonUiStyle& style);
    /// @brief 文字を更新する。
    void UpdateLabel(const cg2::Vector2& center, const TankButtonUiStyle& style);

    cg2::SpriteCommon* spriteCommon_ = nullptr;
    TankButtonVisualData visualData_{};
    int rank_ = 1;
    TankButtonState state_ = TankButtonState::Normal;
    std::array<std::unique_ptr<cg2::Sprite>, kFillSpriteCount> fillSprites_;
    std::array<std::array<std::unique_ptr<cg2::Sprite>, kFrameSegmentCount>, kFrameLayerCount> frameSprites_;
    std::array<std::unique_ptr<cg2::Sprite>, kIconSpriteCount> iconSprites_;
    std::array<std::unique_ptr<cg2::Sprite>, kIconSpriteCount> iconBloomSprites_;
    std::unique_ptr<cg2::TextLabel> label_;
};

/// @brief 戦車ボタンの見本を開発画面へ表示する。
class TankButtonGallery {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(cg2::SpriteCommon* spriteCommon, const std::array<TankButtonVisualData, 4>& classVisuals);
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    void Update();
    /// @brief 後の後処理演出を描画する。
    void DrawAfterPostEffects();
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw();
    /// @brief ImGUIを描画する。
    void DrawImGui();
    /// @brief 表示中を設定する。
    void SetVisible(bool visible)
    {
        visible_ = visible;
    }
    /// @brief 表示中であるか判定する。
    bool IsVisible() const
    {
        return visible_;
    }

private:
    static constexpr size_t kSampleCount = 12;
    /// @brief Samplesを組み立てる。
    void BuildSamples(const std::array<TankButtonVisualData, 4>& classVisuals);
    /// @brief 文字表示を更新する。
    void UpdateLabels();

    cg2::SpriteCommon* spriteCommon_ = nullptr;
    bool visible_ = true;
    TankButtonUiStyle style_{};
    std::string styleStatus_;
    std::unique_ptr<cg2::Sprite> backdrop_;
    std::unique_ptr<cg2::ObjectPostEffect> bloomEffect_;
    std::array<std::unique_ptr<TankButtonUI>, kSampleCount> buttons_;
    std::array<TankButtonVisualData, kSampleCount> sampleVisuals_{};
    std::array<int, kSampleCount> sampleRanks_{};
    std::array<TankButtonState, kSampleCount> sampleStates_{};
    std::unique_ptr<cg2::TextLabel> titleLabel_;
    std::array<std::unique_ptr<cg2::TextLabel>, 3> rowLabels_;
    std::unique_ptr<cg2::TextLabel> noteLabel_;
};
