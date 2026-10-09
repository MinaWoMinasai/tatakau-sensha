#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief GameplayEditorの処理を担当し、同じプレイの共有状態を借用する。
class GameplayEditor {
public:
    /// @brief 借用するワールドを設定する。
    explicit GameplayEditor(GameWorld& world) : world_(world) {}
    /// @brief ゲームシーン開発表示ImGUIを描画する。
    void DrawGameplayDebugUi();

    /// @brief レベルAIDitor性能調整Labを描画する。
    void DrawLevelAIDitorBalanceLab(bool embedded = false);

    /// @brief レベルオブジェクトを現在の状態へ適用する。
    void ApplyLevelObject(const LevelObject& levelObject, bool allowBossSpawn);

    /// @brief レベル出現範囲を追加する。
    void AddLevelSpawnArea(const LevelSpawnArea& spawnArea);

    /// @brief レベル出現範囲からのオブジェクトを追加する。
    void AddLevelSpawnAreaFromObject(const LevelObject& levelObject);

    /// @brief HP比が開始閾値以下になった未発動ボス段階を配列順に適用し、追加配置を生成する。
    void UpdateLevelBossPhases();

    /// @brief ボス段階調整値を現在の状態へ適用する。
    void ApplyBossPhaseTuning(const LevelBossPhase& phase);

    /// @brief レベル演出Presetを現在の状態へ適用する。
    void ApplyLevelEffectPreset(const nlohmann::json& effectJson);

    /// @brief レベル編集画面プレビューを後で処理するために予約する。
    void QueueLevelEditorPreview();

    /// @brief レベルオブジェクトプレビューを後で処理するために予約する。
    void QueueLevelObjectPreview(const LevelObject& levelObject, const cg2::Vector4& color);

    /// @brief レベル出現範囲プレビューを後で処理するために予約する。
    void QueueLevelSpawnAreaPreview(const LevelSpawnArea& spawnArea, const cg2::Vector4& color);

    /// @brief 弾状態開発表示重ね表示を描画する。
    void DrawBulletStatusDebugOverlay();

    /// @brief 弾状態開発表示Tableを描画する。
    void DrawBulletStatusDebugTable();

private:
    GameWorld& world_;
};
} // namespace gameplay
