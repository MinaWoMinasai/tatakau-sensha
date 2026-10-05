#pragma once
#include <algorithm>
#include <cmath>

enum class CombatFlowState { Playing, BossDefeatSequence, StageClear, GameOver };
enum class CombatDeathOutcome { None, PlayerDeath, BossDefeat };

struct CombatDeathInput {
    bool expedition = false;
    bool playerDead = false;
    bool playerDeathHandled = false;
    bool bossActive = false;
    bool bossDead = false;
    bool bossDefeatHandled = false;
    bool bossRoom = false;
};

// Expedition gives the player's simultaneous death priority; legacy combat
// gives boss defeat priority. Neither renderer nor animation makes this choice.
inline CombatDeathOutcome SelectCombatDeathOutcome(const CombatDeathInput& input) {
    if (input.expedition && input.playerDead && !input.playerDeathHandled)
        return CombatDeathOutcome::PlayerDeath;
    if (input.bossActive && input.bossDead && !input.bossDefeatHandled &&
        (!input.expedition || input.bossRoom))
        return CombatDeathOutcome::BossDefeat;
    if (input.playerDead && !input.playerDeathHandled)
        return CombatDeathOutcome::PlayerDeath;
    return CombatDeathOutcome::None;
}

struct CombatFlowEvents {
    bool bossImpact = false;
    bool bossResultReady = false;
    bool gameOverTextReady = false;
    // The existing flow returns even on the exact frame a timer reaches zero.
    bool waitForPresentation = false;
};

// Owns result timing only. Scene remains responsible for room/currency state,
// screen effects, sound, UI and actual scene transition requests.
class CombatFlowController {
public:
    void Reset() { *this = CombatFlowController{}; }
    void BeginBossDefeat(float dissolveSpeed, float impactDelay) {
        state_ = CombatFlowState::BossDefeatSequence;
        timer_ = (std::clamp)(2.30f / (std::max)(0.10f, dissolveSpeed), 1.10f, 2.20f);
        bossDefeatDuration_ = timer_;
        impactDelay_ = impactDelay;
        impactTriggered_ = false;
    }
    void BeginGameOver(float duration) {
        state_ = CombatFlowState::GameOver;
        timer_ = duration;
    }
    void EnterResult(bool stageClear) {
        state_ = stageClear ? CombatFlowState::StageClear : CombatFlowState::GameOver;
        timer_ = 0.0f;
    }
    CombatFlowEvents Update(float baseDeltaTime, bool pendingCredits) {
        CombatFlowEvents events;
        // The engine supplies fixed positive dt. A paused or invalid caller
        // cannot wind the timer backwards or introduce non-finite state.
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
    CombatFlowState GetState() const { return state_; }
    float GetTimer() const { return timer_; }
    float GetBossDefeatDuration() const { return bossDefeatDuration_; }

private:
    CombatFlowState state_ = CombatFlowState::Playing;
    float timer_ = 0.0f;
    float bossDefeatDuration_ = 1.55f;
    float impactDelay_ = 0.0f;
    bool impactTriggered_ = false;
};
