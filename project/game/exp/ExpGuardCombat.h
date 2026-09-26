#pragma once
#include "ExpEnemyCombatCycle.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

// Small, allocation-free combat rules also exercised by the enemy tests.
namespace expguard {
inline constexpr float kShieldHalfAngle = 55.0f * 3.14159265f / 180.0f;
inline constexpr float kBladeHalfAngle = 55.0f * 3.14159265f / 180.0f;
inline constexpr float kBladeReach = 3.3f;

inline bool InFacingCone(float facingX, float facingY, float offsetX, float offsetY,
    float halfAngle, float targetRadius = 0.0f) {
    const float distance = std::hypot(offsetX, offsetY);
    const float facingLength = std::hypot(facingX, facingY);
    if (!std::isfinite(distance) || !std::isfinite(facingLength) || facingLength < 0.0001f) return false;
    if (distance < 0.0001f) return true;
    const float angularPadding = std::asin((std::clamp)(targetRadius / distance, 0.0f, 1.0f));
    return (facingX * offsetX + facingY * offsetY) / (facingLength * distance) >=
        std::cos(halfAngle + angularPadding);
}

inline uint32_t ShieldDamage(uint32_t amount, float facingX, float facingY,
    float sourceOffsetX, float sourceOffsetY, bool melee = false) {
    // Area damage exactly at the centre has no front/back direction.
    if (std::hypot(sourceOffsetX, sourceOffsetY) < 0.0001f ||
        !InFacingCone(facingX, facingY, sourceOffsetX, sourceOffsetY, kShieldHalfAngle)) return amount;
    return static_cast<uint32_t>(std::ceil(static_cast<double>(amount) * (melee ? 0.40 : 0.15)));
}

class BladeCycle {
public:
    static constexpr float kWarningSeconds = 0.45f;
    static constexpr float kActiveSeconds = 0.14f;
    static constexpr float kRecoverySeconds = 0.90f;
    void Reset(float delay = 0.35f) {
        phase_ = ExpEnemyCombatPhase::Cooldown;
        remaining_ = (std::max)(0.0f, delay);
        hitConsumed_ = false;
        swingCount_ = 0;
        recoverySeconds_ = kRecoverySeconds;
    }
    void SetIntervalScale(float scale) {
        // F6 may slow this enemy, but cannot remove its mandatory punish window.
        recoverySeconds_ = kRecoverySeconds * (std::clamp)(scale, 1.0f, 4.0f);
    }
    bool Advance(float seconds, bool inAttackRange) {
        if (!std::isfinite(seconds) || seconds <= 0.0f) return false;
        remaining_ = (std::max)(0.0f, remaining_ - seconds);
        if (remaining_ > 0.0f) return false;
        switch (phase_) {
        case ExpEnemyCombatPhase::Cooldown:
            if (inAttackRange) Enter(ExpEnemyCombatPhase::Locked, kWarningSeconds);
            break;
        case ExpEnemyCombatPhase::Locked:
            hitConsumed_ = false;
            ++swingCount_;
            Enter(ExpEnemyCombatPhase::Active, kActiveSeconds);
            return true;
        case ExpEnemyCombatPhase::Active:
            Enter(ExpEnemyCombatPhase::Recovery, recoverySeconds_);
            break;
        case ExpEnemyCombatPhase::Recovery:
            Enter(ExpEnemyCombatPhase::Cooldown, 0.15f);
            break;
        default: break;
        }
        return false;
    }
    bool TryHit(float facingX, float facingY, float offsetX, float offsetY, float targetRadius, bool visible) {
        if (phase_ != ExpEnemyCombatPhase::Active || hitConsumed_ || !visible ||
            std::hypot(offsetX, offsetY) > kBladeReach + targetRadius ||
            !InFacingCone(facingX, facingY, offsetX, offsetY, kBladeHalfAngle, targetRadius)) return false;
        hitConsumed_ = true;
        return true;
    }
    ExpEnemyCombatPhase GetPhase() const { return phase_; }
    bool IsCommitted() const { return phase_ == ExpEnemyCombatPhase::Locked || phase_ == ExpEnemyCombatPhase::Active; }
    float WarningRatio() const { return phase_ == ExpEnemyCombatPhase::Locked ?
        (std::clamp)(1.0f - remaining_ / kWarningSeconds, 0.0f, 1.0f) : 0.0f; }
    float SweepRatio() const { return phase_ == ExpEnemyCombatPhase::Active ?
        (std::clamp)(1.0f - remaining_ / kActiveSeconds, 0.0f, 1.0f) : 0.0f; }
    float RecoveryRatio() const { return phase_ == ExpEnemyCombatPhase::Recovery ?
        (std::clamp)(remaining_ / recoverySeconds_, 0.0f, 1.0f) : 0.0f; }
    uint32_t SwingCount() const { return swingCount_; }
private:
    void Enter(ExpEnemyCombatPhase phase, float seconds) { phase_ = phase; remaining_ = seconds; }
    ExpEnemyCombatPhase phase_ = ExpEnemyCombatPhase::Cooldown;
    float remaining_ = 0.35f;
    float recoverySeconds_ = kRecoverySeconds;
    bool hitConsumed_ = false;
    uint32_t swingCount_ = 0;
};
}
