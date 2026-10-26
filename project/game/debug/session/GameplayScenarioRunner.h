#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief GameplayScenarioRunnerの処理を担当し、同じプレイの共有状態を借用する。
class GameplayScenarioRunner {
public:
    explicit GameplayScenarioRunner(GameWorld& world) : world_(world) {}
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief シナリオ検証を初期化する。
    void InitializeGameplayScenario();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief シナリオ検証のフレーム前の準備を行う。続行できればtrueを返す。
    bool PrepareGameplayScenarioFrame();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief シナリオ検証のフレーム結果を記録する。
    void RecordGameplayScenarioFrame();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief シナリオ検証で要求されたキャプチャを記録する。
    void RecordGameplayScenarioCapture(myEngine::DirectXCommon& dx);
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief 現在のゲーム状態からシナリオ検証用のスナップショットを作る。
    gameplaytest::Snapshot MakeGameplayScenarioSnapshot() const;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief シナリオ検証のキャプチャを予約する。
    void QueueGameplayScenarioCapture(const std::string& name, bool continuousRecording = false);
#endif
private:
    GameWorld& world_;
};
} // namespace gameplay
