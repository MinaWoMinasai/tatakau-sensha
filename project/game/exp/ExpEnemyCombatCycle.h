#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// 敵の攻撃周期の時間状態。追尾予告、照準固定、実行、回復を分け、実際の移動や攻撃は呼び出し側が行う。
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

    /// @brief 各段階の時間設定をコピーし、初回待ちを最低0秒としてCooldownへ戻す。回復倍率は保持する。
    void Reset(const Timing& timing, float initialDelay)
    {
        timing_ = timing;
        phase_ = ExpEnemyCombatPhase::Cooldown;
        remaining_ = (std::max)(0.0f, initialDelay);
    }

    /// @brief 現在の段階を進め、Activeへ入った呼び出しだけtrueを返す。falseでもタイマーや段階は変わり得る。
    /// @param deltaTime 進める秒数。非有限値・0以下なら何もせずfalse。
    /// @param canTrackTarget 待ち終了後の追尾開始と、Trackingの継続を許可するか。Locked以降は参照しない。
    /// @note 1呼び出しで進む段階は最大1つ。余った時間を次段階へ繰り越さないため、長い更新でも照準固定を飛ばさない。
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
    /// @brief Recoveryの残り時間の割合を0〜1で返す。それ以外は0。経過した時間の割合ではない。
    float GetRecoveryRatio() const
    {
        return phase_ == ExpEnemyCombatPhase::Recovery
                   ? (std::clamp)(remaining_ / (std::max)(0.001f, timing_.recovery * recoveryScale_), 0.0f, 1.0f)
                   : 0.0f;
    }

private:
    // 状態オブジェクトは共有するが、段階とタイマーは各周期インスタンスに保持する。
    /// @brief ExpEnemyCombatCycleの状態遷移の共通契約を定義する。具体的な状態は同じクラス内の派生型で表す。
    class State {
    public:
        /// @brief 派生状態を基底型経由で破棄できるようにする。
        virtual ~State() = default;
        /// @brief この段階のタイマーと遷移を処理する。Activeへ入った場合だけtrueを返す。
        /// @param dt この処理で進める経過時間（秒）。
        virtual bool Update(ExpEnemyCombatCycle& cycle, float dt, bool canTrack) const = 0;
    };
    /// @brief ExpEnemyCombatCycleで次の攻撃まで待機する状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class CooldownState final : public State {
    public:
        /// @brief この段階のタイマーと遷移を処理する。Activeへ入った場合だけtrueを返す。
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
        /// @brief この段階のタイマーと遷移を処理する。Activeへ入った場合だけtrueを返す。
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
        /// @brief この段階のタイマーと遷移を処理する。Activeへ入った場合だけtrueを返す。
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
        /// @brief この段階のタイマーと遷移を処理する。Activeへ入った場合だけtrueを返す。
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
        /// @brief この段階のタイマーと遷移を処理する。Activeへ入った場合だけtrueを返す。
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
        // 列挙値を共有状態オブジェクトへ対応させる。範囲外の値はCooldownを使う。
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
