#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief SessionBootstrapの処理を担当し、同じプレイの共有状態を借用する。
class SessionBootstrap {
public:
    /// @brief 借用するワールドを設定する。
    explicit SessionBootstrap(GameWorld& world) : world_(world) {}
    /// @brief 初期化
    void Initialize();

private:
    GameWorld& world_;
};
} // namespace gameplay
