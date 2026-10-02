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
inline constexpr float kReflectHalfAngle = 50.0f * 3.14159265f / 180.0f;
inline constexpr float kEmpRadius = 8.0f;
inline constexpr int kSummonAliveLimit = 3;
inline constexpr int kSummonLifetimeLimit = 6;
inline constexpr float kSummonLifetimeSeconds = 16.0f;

// Fixed update timing: a long frame cannot skip the entire warning or emit
// several waves. No render resources are associated with this ability state.
/// @brief パルス攻撃の周期と発動タイミングを管理する。
class PulseCycle {
public:
    /// @brief 状態と集計値を初期状態へ戻す。
    void Reset(float period, float warning, float initialDelay)
    {
        warningSeconds_ = (std::max)(0.25f, warning);
        cooldownSeconds_ = (std::max)(0.5f, period - warningSeconds_);
        remaining_ = (std::max)(0.0f, initialDelay);
        warning_ = false;
    }
    /// @brief 経過時間に応じて現在の状態を進める。
    /// @param dt この処理で進める経過時間（秒）。
    bool Advance(float dt, bool canStart)
    {
        if (!std::isfinite(dt) || dt <= 0)
            return false;
        remaining_ = (std::max)(0.0f, remaining_ - dt);
        if (remaining_ > 0)
            return false;
        if (warning_) {
            warning_ = false;
            remaining_ = cooldownSeconds_;
            return true;
        }
        if (canStart) {
            warning_ = true;
            remaining_ = warningSeconds_;
        }
        return false;
    }
    /// @brief 予告であるか判定する。
    bool IsWarning() const
    {
        return warning_;
    }
    /// @brief 予告時間に対する現在の進行度を返す。
    float WarningRatio() const
    {
        return warning_ ? (std::clamp)(1 - remaining_ / warningSeconds_, 0.0f, 1.0f) : 0;
    }

private:
    float remaining_ = 0, warningSeconds_ = 1, cooldownSeconds_ = 4;
    bool warning_ = false;
};
/// @brief 召喚可能な枠数を返す。
inline int SummonSlots(int alive, int total)
{
    return (std::max)(0, (std::min)(kSummonAliveLimit - alive, kSummonLifetimeLimit - total));
}

/// @brief 対象が向いている方向の扇形範囲内か判定する。
inline bool InFacingCone(float facingX, float facingY, float offsetX, float offsetY, float halfAngle, float targetRadius = 0.0f)
{
    const float distance = std::hypot(offsetX, offsetY);
    const float facingLength = std::hypot(facingX, facingY);
    if (!std::isfinite(distance) || !std::isfinite(facingLength) || facingLength < 0.0001f)
        return false;
    if (distance < 0.0001f)
        return true;
    const float angularPadding = std::asin((std::clamp)(targetRadius / distance, 0.0f, 1.0f));
    return (facingX * offsetX + facingY * offsetY) / (facingLength * distance) >= std::cos(halfAngle + angularPadding);
}

/// @brief 盾に適用するダメージを計算する。
inline uint32_t ShieldDamage(uint32_t amount, float facingX, float facingY, float sourceOffsetX, float sourceOffsetY, bool melee = false)
{
    // Area damage exactly at the centre has no front/back direction.
    if (std::hypot(sourceOffsetX, sourceOffsetY) < 0.0001f ||
        !InFacingCone(facingX, facingY, sourceOffsetX, sourceOffsetY, kShieldHalfAngle))
        return amount;
    return static_cast<uint32_t>(std::ceil(static_cast<double>(amount) * (melee ? 0.40 : 0.15)));
}

/// @brief 刃攻撃の予告・攻撃・回復の周期を管理する。
class BladeCycle {
public:
    static constexpr float kWarningSeconds = 0.45f;
    static constexpr float kActiveSeconds = 0.14f;
    static constexpr float kRecoverySeconds = 0.90f;
    /// @brief 状態と集計値を初期状態へ戻す。
    void Reset(float delay = 0.35f)
    {
        phase_ = ExpEnemyCombatPhase::Cooldown;
        remaining_ = (std::max)(0.0f, delay);
        hitConsumed_ = false;
        swingCount_ = 0;
        recoverySeconds_ = kRecoverySeconds;
    }
    /// @brief 間隔倍率を設定する。
    void SetIntervalScale(float scale)
    {
        // F6 may slow this enemy, but cannot remove its mandatory punish window.
        recoverySeconds_ = kRecoverySeconds * (std::clamp)(scale, 1.0f, 4.0f);
    }
    /// @brief 経過時間に応じて現在の状態を進める。
    bool Advance(float seconds, bool inAttackRange)
    {
        if (!std::isfinite(seconds) || seconds <= 0.0f)
            return false;
        remaining_ = (std::max)(0.0f, remaining_ - seconds);
        if (remaining_ > 0.0f)
            return false;
        const State& state = GetState(phase_);
        return state.Update(*this, inAttackRange);
    }
    /// @brief 攻撃の有効時間と対象を確認して命中を処理する。
    bool TryHit(float facingX, float facingY, float offsetX, float offsetY, float targetRadius, bool visible)
    {
        if (phase_ != ExpEnemyCombatPhase::Active || hitConsumed_ || !visible ||
            std::hypot(offsetX, offsetY) > kBladeReach + targetRadius ||
            !InFacingCone(facingX, facingY, offsetX, offsetY, kBladeHalfAngle, targetRadius))
            return false;
        hitConsumed_ = true;
        return true;
    }
    /// @brief 段階を返す。
    ExpEnemyCombatPhase GetPhase() const
    {
        return phase_;
    }
    /// @brief Committedであるか判定する。
    bool IsCommitted() const
    {
        return phase_ == ExpEnemyCombatPhase::Locked || phase_ == ExpEnemyCombatPhase::Active;
    }
    /// @brief 予告時間に対する現在の進行度を返す。
    float WarningRatio() const
    {
        return phase_ == ExpEnemyCombatPhase::Locked ? (std::clamp)(1.0f - remaining_ / kWarningSeconds, 0.0f, 1.0f) : 0.0f;
    }
    /// @brief 斬撃の振りに対する現在の進行度を返す。
    float SweepRatio() const
    {
        return phase_ == ExpEnemyCombatPhase::Active ? (std::clamp)(1.0f - remaining_ / kActiveSeconds, 0.0f, 1.0f) : 0.0f;
    }
    /// @brief 攻撃後の回復時間に対する進行度を返す。
    float RecoveryRatio() const
    {
        return phase_ == ExpEnemyCombatPhase::Recovery ? (std::clamp)(remaining_ / recoverySeconds_, 0.0f, 1.0f) : 0.0f;
    }
    /// @brief 連続攻撃で使う振りの回数を返す。
    uint32_t SwingCount() const
    {
        return swingCount_;
    }

private:
    /// @brief expguard::BladeCycleの状態遷移の共通契約を定義する。具体的な状態は同じクラス内の派生型で表す。
    class State {
    public:
        /// @brief この型の終了処理を行う。所有している資源の寿命を終了させる。
        virtual ~State() = default;
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        virtual bool Update(BladeCycle& cycle, bool inRange) const = 0;
    };
    /// @brief expguard::BladeCycleで次の攻撃まで待機する状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class CooldownState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        bool Update(BladeCycle& cycle, bool inRange) const override
        {
            if (inRange)
                cycle.Enter(ExpEnemyCombatPhase::Locked, kWarningSeconds);
            return false;
        }
    };
    /// @brief expguard::BladeCycleで照準を固定する状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class LockedState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        bool Update(BladeCycle& cycle, bool) const override
        {
            cycle.hitConsumed_ = false;
            ++cycle.swingCount_;
            cycle.Enter(ExpEnemyCombatPhase::Active, kActiveSeconds);
            return true;
        }
    };
    /// @brief expguard::BladeCycleで攻撃が有効な状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class ActiveState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        bool Update(BladeCycle& cycle, bool) const override
        {
            cycle.Enter(ExpEnemyCombatPhase::Recovery, cycle.recoverySeconds_);
            return false;
        }
    };
    /// @brief expguard::BladeCycleで攻撃後に回復する状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class RecoveryState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        bool Update(BladeCycle& cycle, bool) const override
        {
            cycle.Enter(ExpEnemyCombatPhase::Cooldown, 0.15f);
            return false;
        }
    };
    /// @brief 状態を返す。
    static const State& GetState(ExpEnemyCombatPhase phase)
    {
        static const CooldownState cooldown;
        static const LockedState locked;
        static const ActiveState active;
        static const RecoveryState recovery;
        static const std::array<const State*, 5> states{&cooldown, &cooldown, &locked, &active, &recovery};
        static_assert(states.size() == static_cast<std::size_t>(ExpEnemyCombatPhase::Recovery) + 1);
        const auto index = static_cast<std::size_t>(phase);
        return *states[index < states.size() ? index : 0];
    }
    /// @brief 指定状態へ入る際の時間と条件を初期化する。
    void Enter(ExpEnemyCombatPhase phase, float seconds)
    {
        phase_ = phase;
        remaining_ = seconds;
    }
    ExpEnemyCombatPhase phase_ = ExpEnemyCombatPhase::Cooldown;
    float remaining_ = 0.35f;
    float recoverySeconds_ = kRecoverySeconds;
    bool hitConsumed_ = false;
    uint32_t swingCount_ = 0;
};
} // namespace expguard
