#pragma once
#include <algorithm>
#include <cmath>

// Pure timing contract shared by expedition enemies. Every attack has a visible
// tracking window, a non-tracking commitment window, and a punishable recovery.
enum class ExpEnemyCombatPhase { Cooldown, Tracking, Locked, Active, Recovery };

class ExpEnemyCombatCycle {
public:
    struct Timing {
        float tracking = 0.60f;
        float locked = 0.30f;
        float active = 0.42f;
        float recovery = 1.10f;
        float cooldown = 0.55f;
    };

    void Reset(const Timing& timing, float initialDelay) {
        timing_ = timing;
        phase_ = ExpEnemyCombatPhase::Cooldown;
        remaining_ = (std::max)(0.0f, initialDelay);
    }

    // True only on entering Active. A long frame never skips the commitment
    // phase or emits multiple shots; each phase retains its complete duration.
    bool Advance(float deltaTime, bool canTrackTarget) {
        if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) return false;
        if (phase_ == ExpEnemyCombatPhase::Tracking && !canTrackTarget) {
            Enter(ExpEnemyCombatPhase::Cooldown, 0.40f);
            return false;
        }
        remaining_ = (std::max)(0.0f, remaining_ - deltaTime);
        if (remaining_ > 0.0f) return false;
        switch (phase_) {
        case ExpEnemyCombatPhase::Cooldown:
            if (canTrackTarget) Enter(ExpEnemyCombatPhase::Tracking, timing_.tracking);
            break;
        case ExpEnemyCombatPhase::Tracking:
            Enter(ExpEnemyCombatPhase::Locked, timing_.locked);
            break;
        case ExpEnemyCombatPhase::Locked:
            Enter(ExpEnemyCombatPhase::Active, timing_.active);
            return true;
        case ExpEnemyCombatPhase::Active:
            EnterRecovery();
            break;
        case ExpEnemyCombatPhase::Recovery:
            Enter(ExpEnemyCombatPhase::Cooldown, timing_.cooldown * recoveryScale_);
            break;
        }
        return false;
    }

    void SetRecoveryScale(float scale) { recoveryScale_=(std::clamp)(scale,0.20f,8.0f); }
    void EnterRecovery() { Enter(ExpEnemyCombatPhase::Recovery, timing_.recovery * recoveryScale_); }
    ExpEnemyCombatPhase GetPhase() const { return phase_; }
    bool IsAimLocked() const {
        return phase_ == ExpEnemyCombatPhase::Locked || phase_ == ExpEnemyCombatPhase::Active;
    }
    float GetWarningRatio() const {
        if (phase_ == ExpEnemyCombatPhase::Locked) return 1.0f;
        if (phase_ != ExpEnemyCombatPhase::Tracking) return 0.0f;
        return (std::clamp)(1.0f - remaining_ / (std::max)(0.001f, timing_.tracking), 0.0f, 1.0f);
    }
    float GetRecoveryRatio() const {
        return phase_ == ExpEnemyCombatPhase::Recovery
            ? (std::clamp)(remaining_ / (std::max)(0.001f, timing_.recovery * recoveryScale_), 0.0f, 1.0f) : 0.0f;
    }

private:
    void Enter(ExpEnemyCombatPhase phase, float duration) {
        phase_ = phase;
        remaining_ = (std::max)(0.001f, duration);
    }
    Timing timing_{};
    ExpEnemyCombatPhase phase_ = ExpEnemyCombatPhase::Cooldown;
    float remaining_ = 0.0f;
    float recoveryScale_ = 1.0f;
};
