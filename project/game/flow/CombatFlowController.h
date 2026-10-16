#pragma once
#include <algorithm>
#include <cmath>

/// @brief 戦闘シーンの進行状態。
enum class CombatFlowState {
    Playing,
    BossDefeatSequence,
    StageClear,
    GameOver
};
/// @brief 死亡判定の結果。
enum class CombatDeathOutcome {
    None,
    PlayerDeath,
    BossDefeat
};

/// @brief 死亡判定に使う戦闘側の値。
struct CombatDeathInput {
    bool expedition = false;
    bool playerDead = false;
    bool playerDeathHandled = false;
    bool bossActive = false;
    bool bossDead = false;
    bool bossDefeatHandled = false;
    bool bossRoom = false;
};

// 遠征ではプレイヤーとの同時死亡時にプレイヤーの死亡を優先し、旧来の戦闘では
// ボス撃破を優先する。描画やアニメーションがこの判断をすることはない。
inline CombatDeathOutcome SelectCombatDeathOutcome(const CombatDeathInput& input)
{
    if (input.expedition && input.playerDead && !input.playerDeathHandled) {
        return CombatDeathOutcome::PlayerDeath;
    }
    if (input.bossActive && input.bossDead && !input.bossDefeatHandled && (!input.expedition || input.bossRoom)) {
        return CombatDeathOutcome::BossDefeat;
    }
    if (input.playerDead && !input.playerDeathHandled) {
        return CombatDeathOutcome::PlayerDeath;
    }
    return CombatDeathOutcome::None;
}

/// @brief 1回の更新で発生した進行イベント。
struct CombatFlowEvents {
    bool bossImpact = false;
    bool bossResultReady = false;
    bool gameOverTextReady = false;
    // 既存のフローは、タイマーがちょうど0になったフレームでも戻る。
    bool waitForPresentation = false;
};

// 結果のタイミングだけを担当する。部屋や通貨の状態、画面効果、サウンド、UI、
// 実際のシーン遷移の要求は引き続きSceneが担当する。
class CombatFlowController {
public:
    /// @brief 初期状態に戻す。
    void Reset()
    {
        *this = CombatFlowController{};
    }

    // minimumDurationにより、段階的なフィナーレ（カメラの寄り、連続爆発、タイトル）を結果の前に終えられる。
    void BeginBossDefeat(float dissolveSpeed, float impactDelay, float minimumDuration = 0.0f)
    {
        state_ = CombatFlowState::BossDefeatSequence;
        timer_ = (std::max)((std::clamp)(2.30f / (std::max)(0.10f, dissolveSpeed), 1.10f, 2.20f),
                            std::isfinite(minimumDuration) ? (std::min)(minimumDuration, 8.0f) : 0.0f);
        bossDefeatDuration_ = timer_;
        impactDelay_ = impactDelay;
        impactTriggered_ = false;
    }

    /// @brief ゲームオーバー演出を開始する。
    void BeginGameOver(float duration)
    {
        state_ = CombatFlowState::GameOver;
        timer_ = duration;
    }

    /// @brief 結果画面の状態へ移る。
    void EnterResult(bool stageClear)
    {
        state_ = stageClear ? CombatFlowState::StageClear : CombatFlowState::GameOver;
        timer_ = 0.0f;
    }

    /// @brief タイマーを進め、発生した進行イベントを返す。
    CombatFlowEvents Update(float baseDeltaTime, bool pendingCredits)
    {
        CombatFlowEvents events;
        // エンジンは固定の正のdtを渡す。一時停止中や不正な呼び出し側が
        // タイマーを巻き戻したり、有限でない状態を持ち込んだりはできない。
        const float dt = std::isfinite(baseDeltaTime) && baseDeltaTime > 0.0f ? baseDeltaTime : 0.0f;
        if (state_ == CombatFlowState::BossDefeatSequence) {
            events.waitForPresentation = true;
            if (!impactTriggered_) {
                impactDelay_ = (std::max)(0.0f, impactDelay_ - dt);
                if (impactDelay_ <= 0.0f) {
                    impactTriggered_ = true;
                    events.bossImpact = true;
                }
            }
            timer_ = (std::max)(0.0f, timer_ - dt);
            events.bossResultReady = timer_ <= 0.0f && !pendingCredits;
        } else if (state_ == CombatFlowState::GameOver && timer_ > 0.0f) {
            events.waitForPresentation = true;
            timer_ = (std::max)(0.0f, timer_ - dt);
            events.gameOverTextReady = timer_ <= 0.0f;
        }
        return events;
    }

    CombatFlowState GetState() const
    {
        return state_;
    }

    float GetTimer() const
    {
        return timer_;
    }

    float GetBossDefeatDuration() const
    {
        return bossDefeatDuration_;
    }

private:
    CombatFlowState state_ = CombatFlowState::Playing;
    float timer_ = 0.0f;
    float bossDefeatDuration_ = 1.55f;
    float impactDelay_ = 0.0f;
    bool impactTriggered_ = false;
};
