#pragma once
#include <algorithm>
#include <array>
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
        const State& state = GetState(phase_);
        return state.Update(*this, deltaTime, canTrackTarget);
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
    // Immutable state objects are shared; each enemy keeps its own timers here.
    // Nested classes use the private transition API without friend declarations.
    class State {
    public:
        virtual ~State() = default;
        virtual bool Update(ExpEnemyCombatCycle& cycle, float dt, bool canTrack) const = 0;
    };
    class CooldownState final : public State {
    public:
        bool Update(ExpEnemyCombatCycle& cycle, float dt, bool canTrack) const override {
            if (cycle.Elapse(dt) && canTrack)
                cycle.Enter(ExpEnemyCombatPhase::Tracking, cycle.timing_.tracking);
            return false;
        }
    };
    class TrackingState final : public State {
    public:
        bool Update(ExpEnemyCombatCycle& cycle, float dt, bool canTrack) const override {
            if (!canTrack) cycle.Enter(ExpEnemyCombatPhase::Cooldown, 0.40f);
            else if (cycle.Elapse(dt)) cycle.Enter(ExpEnemyCombatPhase::Locked, cycle.timing_.locked);
            return false;
        }
    };
    class LockedState final : public State {
    public:
        bool Update(ExpEnemyCombatCycle& cycle, float dt, bool) const override {
            if (!cycle.Elapse(dt)) return false;
            cycle.Enter(ExpEnemyCombatPhase::Active, cycle.timing_.active);
            return true;
        }
    };
    class ActiveState final : public State {
    public:
        bool Update(ExpEnemyCombatCycle& cycle, float dt, bool) const override {
            if (cycle.Elapse(dt)) cycle.EnterRecovery();
            return false;
        }
    };
    class RecoveryState final : public State {
    public:
        bool Update(ExpEnemyCombatCycle& cycle, float dt, bool) const override {
            if (cycle.Elapse(dt))
                cycle.Enter(ExpEnemyCombatPhase::Cooldown, cycle.timing_.cooldown * cycle.recoveryScale_);
            return false;
        }
    };
    static const State& GetState(ExpEnemyCombatPhase phase) {
        static const CooldownState cooldown;
        static const TrackingState tracking;
        static const LockedState locked;
        static const ActiveState active;
        static const RecoveryState recovery;
        // Indexed by ExpEnemyCombatPhase; behavior stays in the State classes.
        static const std::array<const State*, 5> states{&cooldown, &tracking, &locked, &active, &recovery};
        static_assert(states.size() == static_cast<std::size_t>(ExpEnemyCombatPhase::Recovery) + 1);
        const auto index = static_cast<std::size_t>(phase);
        return *states[index < states.size() ? index : 0];
    }
    bool Elapse(float dt) {
        remaining_ = (std::max)(0.0f, remaining_ - dt);
        return remaining_ <= 0.0f;
    }
    void Enter(ExpEnemyCombatPhase phase, float duration) {
        phase_ = phase;
        remaining_ = (std::max)(0.001f, duration);
    }
    Timing timing_{};
    ExpEnemyCombatPhase phase_ = ExpEnemyCombatPhase::Cooldown;
    float remaining_ = 0.0f;
    float recoveryScale_ = 1.0f;
};
