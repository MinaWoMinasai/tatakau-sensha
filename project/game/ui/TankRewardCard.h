#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>
#include "Sprite.h"
#include "TextLabel.h"
#include "game/run/TankBuildStyle.h"
#include "game/run/TankRunDirector.h"
#include "TankRewardCardDemo.h"
#include "TankRewardPreviewRenderer.h"
#include "game/player/TankCombatStyleBalance.h"

/// @brief 報酬カードの表示内容・価格・選択状態を表す。
struct TankRewardCardModel {
    std::string id, title, description, footer, authoredVariant;
    tankbuild::Style style = tankbuild::Style::Shooter;
    int rarity = 0;
    std::vector<tankrun::CardId> effects;
    tankrun::CardCounts ownedEffects{};
    bool styleChoice = false;
    // Authored visual characteristics are supplied explicitly by the scene.
    // They do not read/instantiate the player's class or authored JSON catalog.
    int barrels = 1, drones = 3;
    bool reflect = false, penetrate = false;
    int currentDrones = 3, currentBarrels = 1;
    bool currentReflect = false, currentPenetrate = false;
    float fanAngle = 0, currentFanAngle = 0;
    bool alternate = false, currentAlternate = false;
    float damageScale = 1.0f, reloadScale = 1.0f, bulletSpeedScale = 1.0f;
    float currentDamageScale = 1.0f, currentReloadScale = 1.0f, currentBulletSpeedScale = 1.0f;
    bool previewKnown = true;
    TankCombatStyleProfile profile{};
    TankRunGrowth growth{};
    decltype(TankRunModifiers{}.effectPower) effectPower = TankRunModifiers{}.effectPower;
    decltype(TankRunModifiers{}.effectPower) ownedEffectPower = TankRunModifiers{}.effectPower;
};

/// @brief 報酬カードの表示資源と入力・選択演出を管理する。
class TankRewardCard {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(cg2::SpriteCommon* spriteCommon);
    /// @brief プレビューを初期化する。
    void InitializePreview(cg2::SrvManager* srvManager);
    /// @brief プレビュー外観を設定する。
    void SetPreviewAppearance(const TankRewardPreviewAppearance& appearance);
    /// @brief プレビュー描画を利用前に準備する。
    void PreparePreviewRender();
    /// @brief プレビュー描画件数を返す。
    uint64_t GetPreviewRenderCount() const
    {
        return preview_ ? preview_->GetRenderCount() : 0;
    }
    /// @brief プレビュー頂点件数を返す。
    uint32_t GetPreviewVertexCount() const
    {
        return preview_ ? preview_->GetGeometryVertexCount() : 0;
    }
    // Safe to call each Update; an equal model does not rebuild any text.
    /// @brief モデルを設定する。
    void SetModel(const TankRewardCardModel& model);
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(const cg2::Vector2& center, const cg2::Vector2& size, float deltaTime, bool hovered, bool enabled = true);
    /// @brief Acquireを再生する。
    void PlayAcquire();
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw();
    // Optional scene bloom pass. Draw() already includes the small edge glow
    // textures; an additional full-screen effect is not required for these cards.
    /// @brief ブルーム元データを描画する。
    void DrawBloomSource();
    /// @brief モデルを返す。
    const TankRewardCardModel& GetModel() const
    {
        return model_;
    }
    /// @brief デモ状態の写しを返す。
    const tankreward::DemoSnapshot& GetDemoSnapshot(bool after = true) const
    {
        return after ? after_ : before_;
    }
    /// @brief デモ時間を返す。
    float GetDemoTime() const
    {
        return clock_.Elapsed();
    }
    /// @brief デモ進行件数を返す。
    std::uint64_t GetDemoAdvanceCount() const
    {
        return clock_.AdvanceCount();
    }
    /// @brief AcquireAnimatingであるか判定する。
    bool IsAcquireAnimating() const
    {
        return acquireTime_ >= 0.0f && acquireTime_ < 0.62f;
    }

private:
    static constexpr std::size_t kSolidCapacity = 420, kGlowCapacity = 80, kLabelCount = 8;
    /// @brief スプライトを必要な状態を用意する。
    void EnsureSprite(std::unique_ptr<cg2::Sprite>& sprite, const char* texture);
    /// @brief 文字を最新の内容へ更新する。
    void RefreshText();
    /// @brief フレームを組み立てる。
    void BuildFrame();
    /// @brief デモを組み立てる。
    void BuildDemo();
    /// @brief Laneを描画する。
    void DrawLane(const tankreward::DemoSnapshot& state, const cg2::Vector2& origin, const cg2::Vector2& size, bool after);
    /// @brief 指定領域を矩形として描画する。
    void Rect(cg2::Vector2 center, cg2::Vector2 size, const cg2::Vector4& color, float rotation = 0.0f);
    /// @brief 2点を結ぶ線を描画する。
    void Line(cg2::Vector2 a, cg2::Vector2 b, float width, const cg2::Vector4& color);
    /// @brief 指定範囲の発光を描画する。
    void Glow(cg2::Vector2 center, cg2::Vector2 size, const cg2::Vector4& color, float rotation = 0.0f);
    /// @brief デモ用の戦車の姿勢を描画する。
    void Tank(cg2::Vector2 center, float radius, float angle, const cg2::Vector4& color, bool drone = false, int barrels = 1);
    /// @brief デモ用の説明文字を描画する。
    void Label(std::size_t index, cg2::Vector2 position, float maxWidth, float maxHeight, float alpha = 1.0f);
    /// @brief 報酬の種類に対応するデモ設定を返す。
    tankreward::DemoConfig DemoConfig(bool after) const;
    cg2::SpriteCommon* spriteCommon_ = nullptr;
    TankRewardCardModel model_{};
    std::array<std::unique_ptr<cg2::Sprite>, kSolidCapacity> solids_;
    std::array<std::unique_ptr<cg2::Sprite>, kGlowCapacity> glows_;
    std::array<std::unique_ptr<cg2::TextLabel>, kLabelCount> labels_;
    std::unique_ptr<TankRewardPreviewRenderer> preview_;
    std::unique_ptr<cg2::Sprite> previewSprite_;
    std::size_t solidCount_ = 0, glowCount_ = 0;
    std::array<bool, kLabelCount> labelVisible_{};
    tankreward::DemoClock clock_;
    tankreward::DemoSnapshot before_{}, after_{};
    cg2::Vector2 center_{}, size_{300, 410};
    cg2::Vector4 accent_{0.9f, 0.96f, 1.0f, 1.0f};
    float hoverBlend_ = 0.0f, visualTime_ = 0.0f, acquireTime_ = -1.0f, alpha_ = 1.0f;
    bool dirty_ = true, hovered_ = false, enabled_ = true;
    bool previewDirty_ = true;
};
