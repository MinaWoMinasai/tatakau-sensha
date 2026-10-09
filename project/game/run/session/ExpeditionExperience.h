#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief ExpeditionExperienceの処理を担当し、同じプレイの共有状態を借用する。
class ExpeditionExperience {
public:
    /// @brief 借用するワールドを設定する。
    explicit ExpeditionExperience(GameWorld& world) : world_(world) {}
    /// @brief 遠征経験値を初期化する。
    void InitializeExpeditionExperience();

    /// @brief 遠征通貨を出現させる。
    void SpawnExpeditionCredits(const cg2::Vector3& position, int amount, bool flyImmediately = false);

    /// @brief 遠征通貨を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateExpeditionCredits(float dt, bool collectAll = false);

    /// @brief 遠征通貨を描画する。
    void DrawExpeditionCredits();

    /// @brief 遠征Vitalsを描画する。
    void DrawExpeditionVitals();

    /// @brief ガイド付き遠征を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateGuidedExpedition(float dt);

    /// @brief 遠征ガイドの現在の説明を確認済みにして次へ進む。
    void AcknowledgeGuidedExpedition();

    /// @brief ガイド付き遠征を描画する。
    void DrawGuidedExpedition();

    /// @brief ガイド付き遠征UIを最新の内容へ更新する。
    void RefreshGuidedExpeditionUi();

    /// @brief ガイド付き遠征Pausedであるか判定する。
    bool IsGuidedExpeditionPaused() const;

    /// @brief 遠征衝撃を後で処理するために予約する。
    void QueueExpeditionImpact(const cg2::Vector3& position, const cg2::Vector3& direction, bool finisher);

    /// @brief 遠征ポインターを描画する。
    void DrawExpeditionPointer(cg2::Vector2 target, bool right = true);

    /// @brief 通貨Iconを描画する。
    void DrawCurrencyIcon(cg2::Vector2 center, float size = 30);

    /// @brief 遠征Pointersを必要な状態を用意する。
    void EnsureExpeditionPointers(size_t count);

private:
    GameWorld& world_;
};
} // namespace gameplay
