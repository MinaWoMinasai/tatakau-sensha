#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief ClassConfigWatcherの処理を担当し、同じプレイの共有状態を借用する。
class ClassConfigWatcher {
public:
    /// @brief 借用するワールドを設定する。
    explicit ClassConfigWatcher(GameWorld& world) : world_(world) {}
    /// @brief 自機機体設定監視を初期化する。
    void InitializePlayerClassConfigWatch();

    /// @brief 自機機体設定監視を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdatePlayerClassConfigWatch(float deltaTime);

    /// @brief 自機機体設定を再読込または再装填する。
    bool ReloadPlayerClassConfig(bool automatic);

private:
    GameWorld& world_;
};
} // namespace gameplay
