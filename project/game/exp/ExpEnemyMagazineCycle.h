#pragma once
#include "ExpEnemyCombatCycle.h"

// A finite magazine, not an infinite periodic gun. Cover cancels tracking, but
// committed shots keep their aim. Reload always runs to completion, even when
// the target disappears. No frame may emit more than one volley.
/// @brief 複数弾の連射と再装填を含む敵の攻撃周期を管理する。
class ExpEnemyMagazineCycle {
public:
    /// @brief 攻撃周期の各状態を継続する時間を指定する。
    struct Timing {
        int rounds = 3;
        float tracking = 0.26f;
        float locked = 0.16f;
        float cadence = 0.16f;
        float reload = 1.50f;
    };
    /// @brief 状態と集計値を初期状態へ戻す。
    void Reset(const Timing& timing, float delay)
    {
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
    /// @brief 間隔倍率を設定する。
    void SetIntervalScale(float scale)
    {
        intervalScale_ = (std::clamp)(scale, 0.3f, 4.0f);
    }
    /// @brief 経過時間に応じて現在の状態を進める。
    /// @param dt この処理で進める経過時間（秒）。
    bool Advance(float dt, bool canTrack)
    {
        if (!std::isfinite(dt) || dt <= 0)
            return false;
        const State& state = GetState(phase_);
        return state.Update(*this, dt, canTrack);
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
    /// @brief Reloadingであるか判定する。
    bool IsReloading() const
    {
        return phase_ == ExpEnemyCombatPhase::Recovery;
    }
    /// @brief 残弾数を返す。
    int GetAmmo() const
    {
        return ammo_;
    }
    /// @brief Capacityを返す。
    int GetCapacity() const
    {
        return timing_.rounds;
    }
    /// @brief 再装填件数を返す。
    unsigned GetReloadCount() const
    {
        return reloadCount_;
    }
    /// @brief 予告比率を返す。
    float GetWarningRatio() const
    {
        if (phase_ == ExpEnemyCombatPhase::Locked)
            return 1;
        return phase_ == ExpEnemyCombatPhase::Tracking ? (std::clamp)(1 - remaining_ / timing_.tracking, 0.0f, 1.0f) : 0;
    }
    /// @brief 再装填進行度を返す。
    float GetReloadProgress() const
    {
        return IsReloading() ? (std::clamp)(1 - remaining_ / reloadDuration_, 0.0f, 1.0f) : 0;
    }

private:
    /// @brief ExpEnemyMagazineCycleの状態遷移の共通契約を定義する。具体的な状態は同じクラス内の派生型で表す。
    class State {
    public:
        /// @brief この型の終了処理を行う。所有している資源の寿命を終了させる。
        virtual ~State() = default;
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        /// @param dt この処理で進める経過時間（秒）。
        virtual bool Update(ExpEnemyMagazineCycle& cycle, float dt, bool canTrack) const = 0;
    };
    /// @brief ExpEnemyMagazineCycleで次の攻撃まで待機する状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class CooldownState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        /// @param dt この処理で進める経過時間（秒）。
        bool Update(ExpEnemyMagazineCycle& cycle, float dt, bool canTrack) const override
        {
            if (cycle.Elapse(dt) && canTrack)
                cycle.Enter(ExpEnemyCombatPhase::Tracking, cycle.timing_.tracking);
            return false;
        }
    };
    /// @brief ExpEnemyMagazineCycleで対象を追尾する状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class TrackingState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        /// @param dt この処理で進める経過時間（秒）。
        bool Update(ExpEnemyMagazineCycle& cycle, float dt, bool canTrack) const override
        {
            if (!canTrack)
                cycle.Enter(ExpEnemyCombatPhase::Cooldown, 0.14f);
            else if (cycle.Elapse(dt))
                cycle.Enter(ExpEnemyCombatPhase::Locked, cycle.timing_.locked);
            return false;
        }
    };
    /// @brief ExpEnemyMagazineCycleで照準を固定する状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class LockedState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        /// @param dt この処理で進める経過時間（秒）。
        bool Update(ExpEnemyMagazineCycle& cycle, float dt, bool) const override
        {
            if (!cycle.Elapse(dt) || cycle.ammo_ <= 0)
                return false;
            --cycle.ammo_;
            cycle.Enter(ExpEnemyCombatPhase::Active, (std::max)(0.08f, cycle.timing_.cadence * cycle.intervalScale_));
            return true;
        }
    };
    /// @brief ExpEnemyMagazineCycleで攻撃が有効な状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class ActiveState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        /// @param dt この処理で進める経過時間（秒）。
        bool Update(ExpEnemyMagazineCycle& cycle, float dt, bool) const override
        {
            if (!cycle.Elapse(dt))
                return false;
            if (cycle.ammo_ == 0) {
                ++cycle.reloadCount_;
                cycle.reloadDuration_ = (std::max)(0.80f, cycle.timing_.reload * cycle.intervalScale_);
                cycle.Enter(ExpEnemyCombatPhase::Recovery, cycle.reloadDuration_);
            } else
                cycle.Enter(ExpEnemyCombatPhase::Cooldown, 0.01f);
            return false;
        }
    };
    /// @brief ExpEnemyMagazineCycleで攻撃後に回復する状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class RecoveryState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        /// @param dt この処理で進める経過時間（秒）。
        bool Update(ExpEnemyMagazineCycle& cycle, float dt, bool) const override
        {
            if (cycle.Elapse(dt)) {
                cycle.ammo_ = cycle.timing_.rounds;
                cycle.Enter(ExpEnemyCombatPhase::Cooldown, 0.10f);
            }
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
    void Enter(ExpEnemyCombatPhase phase, float time)
    {
        phase_ = phase;
        remaining_ = time;
    }
    Timing timing_{};
    ExpEnemyCombatPhase phase_ = ExpEnemyCombatPhase::Cooldown;
    float remaining_ = 0, intervalScale_ = 1, reloadDuration_ = 1;
    int ammo_ = 3;
    unsigned reloadCount_ = 0;
};
