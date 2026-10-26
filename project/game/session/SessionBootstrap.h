#pragma once
#include "game/session/GameWorld.h"
#include "InitializationTask.h"

namespace gameplay {
/// @brief SessionBootstrapの処理を担当し、同じプレイの共有状態を借用する。
class SessionBootstrap {
public:
    explicit SessionBootstrap(GameWorld& world) : world_(world) {}

    void Initialize();
    /// @brief 初期化を1ステップ進める。完了したらtrueを返す。
    bool InitializeStep();

private:
    /// @brief 初期化を分割して行うコルーチンを作る。
    myEngine::InitializationTask Build();
    myEngine::InitializationTask task_;
    GameWorld& world_;
};
} // namespace gameplay
