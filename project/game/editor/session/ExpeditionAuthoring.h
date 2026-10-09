#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief ExpeditionAuthoringの処理を担当し、同じプレイの共有状態を借用する。
class ExpeditionAuthoring {
public:
    /// @brief 借用するワールドを設定する。
    explicit ExpeditionAuthoring(GameWorld& world) : world_(world) {}
    /// @brief 遠征制作データHubを更新する。
    void UpdateExpeditionAuthoringHub();

private:
    GameWorld& world_;
};
} // namespace gameplay
