#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// Pure timing contract shared by expedition enemies. Every attack has a visible
// tracking window, a non-tracking commitment window, and a punishable recovery.
enum class ExpEnemyCombatPhase {
    Cooldown,
    Tracking,
    Locked,
    Active,
    Recovery
};

/// @brief 予告・照準固定・攻撃・回復を時間で進め、敵の攻撃周期を管理する。
class ExpEnemyCombatCycle {
public:
    /// @brief 攻撃周期の各状態を継続する時間を指定する。
    struct Timing {
        float tracking = 0.60f;
        float locked = 0.30f;
        float active = 0.42f;
        float recovery = 1.10f;
        float cooldown = 0.55f;
    };

    /// @brief 状態と集計値を初期状態へ戻す。
    void Reset(const Timing& timing, float initialDelay)
    {
        timing_ = timing;
        phase_ = ExpEnemyCombatPhase::Cooldown;
        remaining_ = (std::max)(0.0f, initialDelay);
    }

    // True only on entering Active. A long frame never skips the commitment
    // phase or emits multiple shots; each phase retains its complete duration.
    /// @brief 経過時間に応じて現在の状態を進める。
    /// @param deltaTime この処理で進める経過時間（秒）。
    bool Advance(float deltaTime, bool canTrackTarget)
    {
        if (!std::isfinite(deltaTime) || deltaTime <= 0.0f)
            return false;
        const State& state = GetState(phase_);
        return state.Update(*this, deltaTime, canTrackTarget);
    }

    /// @brief 攻撃後の回復時間の倍率を設定する。
    void SetRecoveryScale(float scale)
    {
        recoveryScale_ = (std::clamp)(scale, 0.20f, 8.0f);
    }
    /// @brief 回復を開始する。
    void EnterRecovery()
    {
        Enter(ExpEnemyCombatPhase::Recovery, timing_.recovery * recoveryScale_);
    }
    /// @brief 段階を返す。
    ExpEnemyCombatPhase GetPhase() const
    {
        return phase_;
    }
    /// @brief 照準固定であるか判定する。
    bool IsAimLocked() const
    {
        return phase_ == ExpEnemyCombatPhase::Locked || phase_ == ExpEnemyCombatPhase::Active;
    }
    /// @brief 予告比率を返す。
    float GetWarningRatio() const
    {
        if (phase_ == ExpEnemyCombatPhase::Locked)
            return 1.0f;
        if (phase_ != ExpEnemyCombatPhase::Tracking)
            return 0.0f;
        return (std::clamp)(1.0f - remaining_ / (std::max)(0.001f, timing_.tracking), 0.0f, 1.0f);
    }
    /// @brief 回復比率を返す。
    float GetRecoveryRatio() const
    {
        return phase_ == ExpEnemyCombatPhase::Recovery
                   ? (std::clamp)(remaining_ / (std::max)(0.001f, timing_.recovery * recoveryScale_), 0.0f, 1.0f)
                   : 0.0f;
    }

private:
    // Immutable state objects are shared; each enemy keeps its own timers here.
    // Nested classes use the private transition API without friend declarations.
    /// @brief ExpEnemyCombatCycleの状態遷移の共通契約を定義する。具体的な状態は同じクラス内の派生型で表す。
    class State {
    public:
        /// @brief この型の終了処理を行う。所有している資源の寿命を終了させる。
        virtual ~State() = default;
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        /// @param dt この処理で進める経過時間（秒）。
        virtual bool Update(ExpEnemyCombatCycle& cycle, float dt, bool canTrack) const = 0;
    };
    /// @brief ExpEnemyCombatCycleで次の攻撃まで待機する状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class CooldownState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        /// @param dt この処理で進める経過時間（秒）。
        bool Update(ExpEnemyCombatCycle& cycle, float dt, bool canTrack) const override
        {
            if (cycle.Elapse(dt) && canTrack)
                cycle.Enter(ExpEnemyCombatPhase::Tracking, cycle.timing_.tracking);
            return false;
        }
    };
    /// @brief ExpEnemyCombatCycleで対象を追尾する状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class TrackingState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        /// @param dt この処理で進める経過時間（秒）。
        bool Update(ExpEnemyCombatCycle& cycle, float dt, bool canTrack) const override
        {
            if (!canTrack)
                cycle.Enter(ExpEnemyCombatPhase::Cooldown, 0.40f);
            else if (cycle.Elapse(dt))
                cycle.Enter(ExpEnemyCombatPhase::Locked, cycle.timing_.locked);
            return false;
        }
    };
    /// @brief ExpEnemyCombatCycleで照準を固定する状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class LockedState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        /// @param dt この処理で進める経過時間（秒）。
        bool Update(ExpEnemyCombatCycle& cycle, float dt, bool) const override
        {
            if (!cycle.Elapse(dt))
                return false;
            cycle.Enter(ExpEnemyCombatPhase::Active, cycle.timing_.active);
            return true;
        }
    };
    /// @brief ExpEnemyCombatCycleで攻撃が有効な状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class ActiveState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        /// @param dt この処理で進める経過時間（秒）。
        bool Update(ExpEnemyCombatCycle& cycle, float dt, bool) const override
        {
            if (cycle.Elapse(dt))
                cycle.EnterRecovery();
            return false;
        }
    };
    /// @brief ExpEnemyCombatCycleで攻撃後に回復する状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class RecoveryState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        /// @param dt この処理で進める経過時間（秒）。
        bool Update(ExpEnemyCombatCycle& cycle, float dt, bool) const override
        {
            if (cycle.Elapse(dt))
                cycle.Enter(ExpEnemyCombatPhase::Cooldown, cycle.timing_.cooldown * cycle.recoveryScale_);
            return false;
        }
    };
    /// @brief 状態を返す。
    static const State& GetState(ExpEnemyCombatPhase phase)
    {
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
    /// @brief 経過時間を内部タイマーへ反映する。
    /// @param dt この処理で進める経過時間（秒）。
    bool Elapse(float dt)
    {
        remaining_ = (std::max)(0.0f, remaining_ - dt);
        return remaining_ <= 0.0f;
    }
    /// @brief 指定状態へ入る際の時間と条件を初期化する。
    void Enter(ExpEnemyCombatPhase phase, float duration)
    {
        phase_ = phase;
        remaining_ = (std::max)(0.001f, duration);
    }
    Timing timing_{};
    ExpEnemyCombatPhase phase_ = ExpEnemyCombatPhase::Cooldown;
    float remaining_ = 0.0f;
    float recoveryScale_ = 1.0f;
};
