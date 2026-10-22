#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief GameplayEditorの処理を担当し、同じプレイの共有状態を借用する。
class GameplayEditor {
public:
    /// @brief 借用するワールドを設定する。
    explicit GameplayEditor(GameWorld& world) : world_(world) {}

    /// @brief ゲームシーンのデバッグ用ImGuiを表示する。
    void DrawGameplayDebugUi();

    /// @brief レベルの敵AIと性能を調整する画面を表示する。
    void DrawLevelAiEditorBalanceLab(bool embedded = false);

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

    /// @brief レベルの演出のプリセットを反映する。
    void ApplyLevelEffectPreset(const nlohmann::json& effectJson);

    /// @brief レベル編集のプレビューを描画待ちに積む。
    void QueueLevelEditorPreview();

    /// @brief レベルのオブジェクトのプレビューを描画待ちに積む。
    void QueueLevelObjectPreview(const LevelObject& levelObject, const myEngine::Vector4& color);

    /// @brief レベルの出現範囲のプレビューを描画待ちに積む。
    void QueueLevelSpawnAreaPreview(const LevelSpawnArea& spawnArea, const myEngine::Vector4& color);

    /// @brief 弾の状態をデバッグ用に画面へ重ねて表示する。
    void DrawBulletStatusDebugOverlay();

    /// @brief 弾の状態をデバッグ用の表で表示する。
    void DrawBulletStatusDebugTable();

private:
    GameWorld& world_;
};
} // namespace gameplay
