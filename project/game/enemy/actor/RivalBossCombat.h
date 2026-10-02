#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// Expedition rival decisions only. Angles and timers are locked here; rendering,
// navigation and collision remain the actor's responsibility.
class RivalBossCombat {
public:
    enum class Phase { Reposition, Tracking, Locked, Volley, DashWarning, Dash, Reload };
    enum class Pattern { AimedBurst, FanBurst, Sweep };
    struct Shot {
        bool fire = false;
        Pattern pattern = Pattern::AimedBurst;
        float angle = 0.0f;
        int round = 0;
    };
    void Reset() { *this = RivalBossCombat{}; }
    Phase GetPhase() const { return phase_; }
    Pattern GetPattern() const { return pattern_; }
    bool IsPhaseTwo() const { return phaseTwo_; }
    int GetAmmo() const { return ammo_; }
    int GetCapacity() const { return capacity_; }
    unsigned GetShotsFired() const { return shotsFired_; }
    unsigned GetDashCount() const { return dashCount_; }
    unsigned GetReloadCount() const { return reloadCount_; }
    float GetAimAngle() const { return aimAngle_; }
    float GetStrafeSign() const { return cycle_ % 2 == 0 ? 1.0f : -1.0f; }
    float GetProgress() const { return (std::clamp)(elapsed_ / Duration(), 0.0f, 1.0f); }
    bool IsReactiveDash() const { return reactiveDash_; }
    float GetDashDuration() const { return phaseTwo_ ? 0.30f : 0.28f; }
    float GetDashSpeed() const { return phaseTwo_ ? 0.62f : 0.55f; } // units / 60Hz frame
    float GetDashDistance() const { return GetDashDuration() * GetDashSpeed() * 60.0f; }
    void FinishDash() {
        if (phase_ != Phase::Dash) return;
        Enter(reactiveDash_ ? Phase::Reposition : Phase::Reload);
    }

    Shot Step(float dt, bool lineOfSight, float targetAngle, float hpRatio, bool incomingThreat = false) {
        Shot result{};
        if (!std::isfinite(dt) || dt <= 0.0f) return result;
        // Never consume more than one state transition / projectile volley in a hitch.
        const float step = (std::min)(dt, 0.10f);
        elapsed_ += step;
        dodgeCooldown_ = (std::max)(0.0f, dodgeCooldown_ - step);
        if (std::isfinite(hpRatio) && hpRatio <= 0.5f) phaseTwoPending_ = true;
        const float aim = std::isfinite(targetAngle) ? targetAngle : aimAngle_;
        const State& state = GetState(phase_);
        return state.Update(*this, lineOfSight, aim, incomingThreat);
    }

    static int ProjectileCount(Pattern pattern) { return pattern == Pattern::AimedBurst ? 2 : 3; }
    static float ProjectileOffset(Pattern pattern, int projectile, int round, int capacity) {
        if (pattern == Pattern::AimedBurst) return projectile == 0 ? -3.0f : 3.0f;
        if (pattern == Pattern::FanBurst) {
            // Alternating lanes; there is always space between the three rays.
            return -20.0f + 20.0f * static_cast<float>(projectile) + (round % 2 == 0 ? -5.0f : 5.0f);
        }
        return -32.0f + 64.0f * static_cast<float>(round) / static_cast<float>((std::max)(1, capacity - 1))
            + (static_cast<float>(projectile) - 1.0f) * 5.0f;
    }
    static float WarningHalfAngle(Pattern pattern) {
        return pattern == Pattern::AimedBurst ? 5.0f : (pattern == Pattern::FanBurst ? 27.0f : 39.0f);
    }
    // Projection rejects bullets travelling away or crossing well outside the
    // body. This allows pressure to trigger a limited dodge, without mind reading.
    static bool IsIncomingThreat(float relativeX, float relativeY, float velocityX, float velocityY, float radius) {
        const float speedSq = velocityX * velocityX + velocityY * velocityY;
        if (!std::isfinite(speedSq) || speedSq < 0.0001f) return false;
        const float frames = -(relativeX * velocityX + relativeY * velocityY) / speedSq;
        if (frames < 0.0f || frames > 27.0f) return false;
        const float x = relativeX + velocityX * frames, y = relativeY + velocityY * frames;
        return x * x + y * y <= radius * radius;
    }

private:
    class State {
    public:
        virtual ~State() = default;
        virtual Shot Update(RivalBossCombat& boss, bool visible, float aim, bool threat) const = 0;
    };
    class RepositionState final : public State {
    public:
        Shot Update(RivalBossCombat& boss, bool visible, float aim, bool threat) const override {
            if (threat && boss.dodgeCooldown_ <= 0.0f && visible) {
                boss.reactiveDash_ = true;
                boss.dodgeCooldown_ = 5.5f;
                boss.Enter(Phase::DashWarning);
            } else if (boss.elapsed_ >= boss.Duration() && visible) {
                // Upgrade only at a magazine boundary, never refill mid-volley.
                boss.phaseTwo_ = boss.phaseTwoPending_;
                boss.capacity_ = boss.phaseTwo_ ? 5 : 4;
                boss.ammo_ = boss.capacity_;
                boss.pattern_ = boss.cycle_ % 3 == 1 ? Pattern::FanBurst :
                    (boss.cycle_ % 3 == 2 ? Pattern::Sweep : Pattern::AimedBurst);
                boss.aimAngle_ = aim;
                boss.Enter(Phase::Tracking);
            }
            return {};
        }
    };
    class TrackingState final : public State {
    public:
        Shot Update(RivalBossCombat& boss, bool visible, float aim, bool) const override {
            boss.aimAngle_ = aim;
            if (!visible) boss.Enter(Phase::Reposition);
            else if (boss.elapsed_ >= boss.Duration()) boss.Enter(Phase::Locked);
            return {};
        }
    };
    class LockedState final : public State {
    public:
        Shot Update(RivalBossCombat& boss, bool, float, bool) const override {
            if (boss.elapsed_ < boss.Duration()) return {};
            boss.Enter(Phase::Volley);
            return boss.FireRound();
        }
    };
    class VolleyState final : public State {
    public:
        Shot Update(RivalBossCombat& boss, bool, float, bool) const override {
            if (boss.elapsed_ < boss.Duration()) return {};
            if (boss.ammo_ > 0) return boss.FireRound();
            boss.reactiveDash_ = false;
            boss.dodgeCooldown_ = (std::max)(boss.dodgeCooldown_, 2.0f);
            boss.Enter(Phase::DashWarning);
            return {};
        }
    };
    class DashWarningState final : public State {
    public:
        Shot Update(RivalBossCombat& boss, bool, float, bool) const override {
            if (boss.elapsed_ >= boss.Duration()) boss.Enter(Phase::Dash);
            return {};
        }
    };
    class DashState final : public State {
    public:
        Shot Update(RivalBossCombat& boss, bool, float, bool) const override {
            if (boss.elapsed_ >= boss.Duration()) boss.FinishDash();
            return {};
        }
    };
    class ReloadState final : public State {
    public:
        Shot Update(RivalBossCombat& boss, bool, float, bool) const override {
            if (boss.elapsed_ >= boss.Duration()) {
                ++boss.cycle_;
                boss.ammo_ = boss.capacity_;
                boss.Enter(Phase::Reposition);
            }
            return {};
        }
    };
    static const State& GetState(Phase phase) {
        static const RepositionState reposition;
        static const TrackingState tracking;
        static const LockedState locked;
        static const VolleyState volley;
        static const DashWarningState dashWarning;
        static const DashState dash;
        static const ReloadState reload;
        static const std::array<const State*, 7> states{&reposition, &tracking, &locked, &volley, &dashWarning, &dash, &reload};
        static_assert(states.size() == static_cast<std::size_t>(Phase::Reload) + 1);
        const auto index = static_cast<std::size_t>(phase);
        return *states[index < states.size() ? index : 0];
    }
    void Enter(Phase next) {
        phase_ = next; elapsed_ = 0.0f;
        if (next == Phase::Dash) ++dashCount_;
        if (next == Phase::Reload) ++reloadCount_;
    }
    float Duration() const {
        switch (phase_) {
        case Phase::Reposition: return phaseTwo_ ? 0.48f : 0.62f;
        case Phase::Tracking: return phaseTwo_ ? 0.42f : 0.52f;
        case Phase::Locked: return 0.24f;
        case Phase::Volley: return pattern_ == Pattern::FanBurst ? 0.29f : 0.23f;
        case Phase::DashWarning: return reactiveDash_ ? 0.28f : 0.38f;
        case Phase::Dash: return GetDashDuration();
        case Phase::Reload: return phaseTwo_ ? 1.15f : 1.35f;
        }
        return 1.0f;
    }
    Shot FireRound() {
        const int round = capacity_ - ammo_;
        --ammo_;
        ++shotsFired_;
        elapsed_ = 0.0f;
        return { true, pattern_, aimAngle_, round };
    }
    Phase phase_ = Phase::Reposition;
    Pattern pattern_ = Pattern::AimedBurst;
    float elapsed_ = 0.0f;
    float aimAngle_ = 0.0f;
    float dodgeCooldown_ = 2.0f;
    int cycle_ = 0;
    int capacity_ = 4;
    int ammo_ = 4;
    unsigned shotsFired_ = 0, dashCount_ = 0, reloadCount_ = 0;
    bool reactiveDash_ = false;
    bool phaseTwoPending_ = false;
    bool phaseTwo_ = false;
};
