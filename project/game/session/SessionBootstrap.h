#pragma once
#include "game/session/GameWorld.h"
#include "InitializationTask.h"

namespace gameplay {
/// @brief SessionBootstrapの処理を担当し、同じプレイの共有状態を借用する。
class SessionBootstrap {
public:
    /// @brief 借用するワールドを設定する。
    explicit SessionBootstrap(GameWorld& world) : world_(world) {}
    /// @brief 初期化
    void Initialize();
    bool InitializeStep();

private:
    cg2::InitializationTask Build();
    cg2::InitializationTask task_;
    GameWorld& world_;
};
} // namespace gameplay
