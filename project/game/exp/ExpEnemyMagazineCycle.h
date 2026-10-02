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
        const State& state = GetState(phase_);
        return state.Update(*this, dt, canTrack);
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
    class State {
    public:
        virtual ~State() = default;
        virtual bool Update(ExpEnemyMagazineCycle& cycle, float dt, bool canTrack) const = 0;
    };
    class CooldownState final : public State {
    public:
        bool Update(ExpEnemyMagazineCycle& cycle, float dt, bool canTrack) const override {
            if (cycle.Elapse(dt) && canTrack)
                cycle.Enter(ExpEnemyCombatPhase::Tracking, cycle.timing_.tracking);
            return false;
        }
    };
    class TrackingState final : public State {
    public:
        bool Update(ExpEnemyMagazineCycle& cycle, float dt, bool canTrack) const override {
            if (!canTrack) cycle.Enter(ExpEnemyCombatPhase::Cooldown, 0.14f);
            else if (cycle.Elapse(dt)) cycle.Enter(ExpEnemyCombatPhase::Locked, cycle.timing_.locked);
            return false;
        }
    };
    class LockedState final : public State {
    public:
        bool Update(ExpEnemyMagazineCycle& cycle, float dt, bool) const override {
            if (!cycle.Elapse(dt) || cycle.ammo_ <= 0) return false;
            --cycle.ammo_;
            cycle.Enter(ExpEnemyCombatPhase::Active,
                (std::max)(0.08f, cycle.timing_.cadence * cycle.intervalScale_));
            return true;
        }
    };
    class ActiveState final : public State {
    public:
        bool Update(ExpEnemyMagazineCycle& cycle, float dt, bool) const override {
            if (!cycle.Elapse(dt)) return false;
            if (cycle.ammo_ == 0) {
                ++cycle.reloadCount_;
                cycle.reloadDuration_ = (std::max)(0.80f, cycle.timing_.reload * cycle.intervalScale_);
                cycle.Enter(ExpEnemyCombatPhase::Recovery, cycle.reloadDuration_);
            } else cycle.Enter(ExpEnemyCombatPhase::Cooldown, 0.01f);
            return false;
        }
    };
    class RecoveryState final : public State {
    public:
        bool Update(ExpEnemyMagazineCycle& cycle, float dt, bool) const override {
            if (cycle.Elapse(dt)) {
                cycle.ammo_ = cycle.timing_.rounds;
                cycle.Enter(ExpEnemyCombatPhase::Cooldown, 0.10f);
            }
            return false;
        }
    };
    static const State& GetState(ExpEnemyCombatPhase phase) {
        static const CooldownState cooldown;
        static const TrackingState tracking;
        static const LockedState locked;
        static const ActiveState active;
        static const RecoveryState recovery;
        switch (phase) {
        case ExpEnemyCombatPhase::Tracking: return tracking;
        case ExpEnemyCombatPhase::Locked: return locked;
        case ExpEnemyCombatPhase::Active: return active;
        case ExpEnemyCombatPhase::Recovery: return recovery;
        default: return cooldown;
        }
    }
    bool Elapse(float dt) {
        remaining_ = (std::max)(0.0f, remaining_ - dt);
        return remaining_ <= 0.0f;
    }
    void Enter(ExpEnemyCombatPhase phase, float time) { phase_ = phase; remaining_ = time; }
    Timing timing_{};
    ExpEnemyCombatPhase phase_ = ExpEnemyCombatPhase::Cooldown;
    float remaining_ = 0, intervalScale_ = 1, reloadDuration_ = 1;
    int ammo_ = 3;
    unsigned reloadCount_ = 0;
};
