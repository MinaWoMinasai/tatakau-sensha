#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief CombatFramePipelineの処理を担当し、同じプレイの共有状態を借用する。
class CombatFramePipeline {
public:
    /// @brief 借用するワールドを設定する。
    explicit CombatFramePipeline(GameWorld& world) : world_(world) {}
    /// @brief シーンの状態に応じて戦闘・演出・進行を更新する。戦闘の呼び出し順は実装内に記載する。
    void Update();

private:
    GameWorld& world_;
};
} // namespace gameplay
