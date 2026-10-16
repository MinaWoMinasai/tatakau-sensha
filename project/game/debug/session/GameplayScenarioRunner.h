#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief GameplayScenarioRunnerの処理を担当し、同じプレイの共有状態を借用する。
class GameplayScenarioRunner {
public:
    /// @brief 借用するワールドを設定する。
    explicit GameplayScenarioRunner(GameWorld& world) : world_(world) {}
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    void InitializeGameplayScenario();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    bool PrepareGameplayScenarioFrame();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    void RecordGameplayScenarioFrame();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    void RecordGameplayScenarioCapture(myEngine::DirectXCommon& dx);
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    gameplaytest::Snapshot MakeGameplayScenarioSnapshot() const;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    void QueueGameplayScenarioCapture(const std::string& name, bool continuousRecording = false);
#endif
private:
    GameWorld& world_;
};
} // namespace gameplay
