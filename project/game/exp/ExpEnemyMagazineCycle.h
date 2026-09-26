#pragma once
#include "ExpEnemyCombatCycle.h"

// A finite magazine, not an infinite periodic gun. Cover cancels tracking, but
// committed shots keep their aim. Reload always runs to completion, even when
// the target disappears. No frame may emit more than one volley.
class ExpEnemyMagazineCycle {
public:
    struct Timing {
        int rounds = 3;
        float tracking = 0.26f;
        float locked = 0.16f;
        float cadence = 0.16f;
        float reload = 1.50f;
    };
    void Reset(const Timing& timing, float delay) {
        timing_ = timing;
        timing_.rounds = (std::clamp)(timing.rounds, 1, 8);
        timing_.tracking = (std::max)(0.12f, timing.tracking);
        timing_.locked = (std::max)(0.12f, timing.locked);
        timing_.cadence = (std::max)(0.08f, timing.cadence);
        timing_.reload = (std::max)(0.80f, timing.reload);
        ammo_ = timing_.rounds;
        reloadCount_ = 0;
        Enter(ExpEnemyCombatPhase::Cooldown, (std::max)(0.0f, delay));
    }
    void SetIntervalScale(float scale) { intervalScale_ = (std::clamp)(scale, 0.3f, 4.0f); }
    bool Advance(float dt, bool canTrack) {
        if (!std::isfinite(dt) || dt <= 0) return false;
        if (phase_ == ExpEnemyCombatPhase::Tracking && !canTrack) {
            Enter(ExpEnemyCombatPhase::Cooldown, 0.14f);
            return false;
        }
        remaining_ = (std::max)(0.0f, remaining_ - dt);
        if (remaining_ > 0) return false;
        switch (phase_) {
        case ExpEnemyCombatPhase::Cooldown:
            if (canTrack) Enter(ExpEnemyCombatPhase::Tracking, timing_.tracking);
            break;
        case ExpEnemyCombatPhase::Tracking:
            Enter(ExpEnemyCombatPhase::Locked, timing_.locked);
            break;
        case ExpEnemyCombatPhase::Locked:
            if (ammo_ > 0) {
                --ammo_;
                Enter(ExpEnemyCombatPhase::Active, (std::max)(0.08f, timing_.cadence * intervalScale_));
                return true;
            }
            break;
        case ExpEnemyCombatPhase::Active:
            if (ammo_ == 0) {
                ++reloadCount_;
                reloadDuration_ = (std::max)(0.80f, timing_.reload * intervalScale_);
                Enter(ExpEnemyCombatPhase::Recovery, reloadDuration_);
            } else Enter(ExpEnemyCombatPhase::Cooldown, 0.01f);
            break;
        case ExpEnemyCombatPhase::Recovery:
            ammo_ = timing_.rounds;
            Enter(ExpEnemyCombatPhase::Cooldown, 0.10f);
            break;
        }
        return false;
    }
    ExpEnemyCombatPhase GetPhase() const { return phase_; }
    bool IsAimLocked() const { return phase_ == ExpEnemyCombatPhase::Locked || phase_ == ExpEnemyCombatPhase::Active; }
    bool IsReloading() const { return phase_ == ExpEnemyCombatPhase::Recovery; }
    int GetAmmo() const { return ammo_; }
    int GetCapacity() const { return timing_.rounds; }
    unsigned GetReloadCount() const { return reloadCount_; }
    float GetWarningRatio() const {
        if (phase_ == ExpEnemyCombatPhase::Locked) return 1;
        return phase_ == ExpEnemyCombatPhase::Tracking ? (std::clamp)(1 - remaining_ / timing_.tracking, 0.0f, 1.0f) : 0;
    }
    float GetReloadProgress() const {
        return IsReloading() ? (std::clamp)(1 - remaining_ / reloadDuration_, 0.0f, 1.0f) : 0;
    }
private:
    void Enter(ExpEnemyCombatPhase phase, float time) { phase_ = phase; remaining_ = time; }
    Timing timing_{};
    ExpEnemyCombatPhase phase_ = ExpEnemyCombatPhase::Cooldown;
    float remaining_ = 0, intervalScale_ = 1, reloadDuration_ = 1;
    int ammo_ = 3;
    unsigned reloadCount_ = 0;
};
