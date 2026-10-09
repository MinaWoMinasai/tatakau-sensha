#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief LevelRuntimeの処理を担当し、同じプレイの共有状態を借用する。
class LevelRuntime {
public:
    /// @brief 借用するワールドを設定する。
    explicit LevelRuntime(GameWorld& world) : world_(world) {}
    /// @brief 戦車遠征部屋形状を現在の状態へ適用する。
    void ApplyTankExpeditionRoomGeometry();

    /// @brief レベルファイルを読み込む。
    bool LoadLevelFile(LevelData& outLevel) const;

    /// @brief レベルデータを再読込または再装填する。
    void ReloadLevelData(bool resetSpawnPositions);

    /// @brief Appliedレベルデータを消去する。
    void ClearAppliedLevelData();

    /// @brief レベルデータを現在の状態へ適用する。
    void ApplyLevelData(const LevelData& levelData);

    /// @brief レベル性能調整を現在の状態へ適用する。
    void ApplyLevelBalance(const nlohmann::json& balanceJson);

    /// @brief レベル項目を追加する。
    bool AddLevelItem(const LevelObject& levelObject);

    /// @brief レベル項目を更新する。
    void UpdateLevelItems();

    /// @brief レベル項目を描画する。
    void DrawLevelItems();

private:
    GameWorld& world_;
};
} // namespace gameplay
