#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief ExpeditionExperienceの処理を担当し、同じプレイの共有状態を借用する。
class ExpeditionExperience {
public:
    explicit ExpeditionExperience(GameWorld& world) : world_(world) {}

    /// @brief 遠征経験値を初期化する。
    void InitializeExpeditionExperience();

    /// @brief 遠征通貨を出現させる。
    void SpawnExpeditionCredits(const myEngine::Vector3& position, int amount, bool flyImmediately = false);

    /// @brief 遠征通貨を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateExpeditionCredits(float dt, bool collectAll = false);

    /// @brief 遠征通貨を描画する。
    void DrawExpeditionCredits();

    /// @brief 遠征中の体力などの表示を描画する。
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

    /// @brief ガイド付きの遠征が一時停止中かを返す。
    bool IsGuidedExpeditionPaused() const;

    /// @brief 遠征中の衝撃の演出を描画待ちに積む。
    void QueueExpeditionImpact(const myEngine::Vector3& position, const myEngine::Vector3& direction, bool finisher);

    /// @brief 遠征ポインターを描画する。
    void DrawExpeditionPointer(myEngine::Vector2 target, bool right = true);

    /// @brief 通貨のアイコンを描画する。
    void DrawCurrencyIcon(myEngine::Vector2 center, float size = 30);

    /// @brief 方向を示す矢印のスプライトを、必要な数まで作っておく。
    void EnsureExpeditionPointers(size_t count);

private:
    GameWorld& world_;
};
} // namespace gameplay
