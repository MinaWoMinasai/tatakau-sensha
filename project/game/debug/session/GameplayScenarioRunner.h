#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief GameplayScenarioRunnerの処理を担当し、同じプレイの共有状態を借用する。
class GameplayScenarioRunner {
public:
    /// @brief 借用するワールドを設定する。
    explicit GameplayScenarioRunner(GameWorld& world) : world_(world) {}
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    void InitializeGameplayScenario();
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    bool PrepareGameplayScenarioFrame();
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    void RecordGameplayScenarioFrame();
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    void RecordGameplayScenarioCapture(cg2::DirectXCommon& dx);
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    gameplaytest::Snapshot MakeGameplayScenarioSnapshot() const;
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    void QueueGameplayScenarioCapture(const std::string& name, bool continuousRecording = false);
#endif
private:
    GameWorld& world_;
};
} // namespace gameplay
