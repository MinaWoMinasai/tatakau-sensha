#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief StageNeonRendererの処理を担当し、同じプレイの共有状態を借用する。
class StageNeonRenderer {
public:
    /// @brief 借用するワールドを設定する。
    explicit StageNeonRenderer(GameWorld& world) : world_(world) {}
    /// @brief ネオン格子描画パスを描画する。
    void DrawNeonGridPass(bool includeStageBlockOutlines = true);

    /// @brief ステージ地形ブロックネオン描画パスを描画する。
    void DrawStageBlockNeonPass();

    /// @brief ステージ地形ブロックネオンOutlinesを後で処理するために予約する。
    void QueueStageBlockNeonOutlines();

    /// @brief ステージ後処理キャッシュUV差分を返す。
    cg2::Vector2 GetStagePostCacheUvOffset(const cg2::Vector3& currentCameraPos) const;

private:
    GameWorld& world_;
};
} // namespace gameplay
