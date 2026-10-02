#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// Expedition rival decisions only. Angles and timers are locked here; rendering,
// navigation and collision remain the actor's responsibility.
/// @brief ライバルボスの照準・連射・突進・再装填を状態で管理する。
class RivalBossCombat {
public:
    enum class Phase {
        Reposition,
        Tracking,
        Locked,
        Volley,
        DashWarning,
        Dash,
        Reload
    };
    enum class Pattern {
        AimedBurst,
        FanBurst,
        Sweep
    };
    /// @brief 1回の攻撃で発生させる弾の位置・方向・性能を表す。
    struct Shot {
        bool fire = false;
        Pattern pattern = Pattern::AimedBurst;
        float angle = 0.0f;
        int round = 0;
    };
    /// @brief 状態と集計値を初期状態へ戻す。
    void Reset()
    {
        *this = RivalBossCombat{};
    }
    /// @brief 段階を返す。
    Phase GetPhase() const
    {
        return phase_;
    }
    /// @brief Patternを返す。
    Pattern GetPattern() const
    {
        return pattern_;
    }
    /// @brief 段階Twoであるか判定する。
    bool IsPhaseTwo() const
    {
        return phaseTwo_;
    }
    /// @brief 残弾数を返す。
    int GetAmmo() const
    {
        return ammo_;
    }
    /// @brief Capacityを返す。
    int GetCapacity() const
    {
        return capacity_;
    }
    /// @brief 射撃発射済みを返す。
    unsigned GetShotsFired() const
    {
        return shotsFired_;
    }
    /// @brief ダッシュ件数を返す。
    unsigned GetDashCount() const
    {
        return dashCount_;
    }
    /// @brief 再装填件数を返す。
    unsigned GetReloadCount() const
    {
        return reloadCount_;
    }
    /// @brief 照準角度を返す。
    float GetAimAngle() const
    {
        return aimAngle_;
    }
    /// @brief StrafeSignを返す。
    float GetStrafeSign() const
    {
        return cycle_ % 2 == 0 ? 1.0f : -1.0f;
    }
    /// @brief 進行度を返す。
    float GetProgress() const
    {
        return (std::clamp)(elapsed_ / Duration(), 0.0f, 1.0f);
    }
    /// @brief Reactiveダッシュであるか判定する。
    bool IsReactiveDash() const
    {
        return reactiveDash_;
    }
    /// @brief ダッシュ継続時間を返す。
    float GetDashDuration() const
    {
        return phaseTwo_ ? 0.30f : 0.28f;
    }
    /// @brief ダッシュ速度を返す。
    float GetDashSpeed() const
    {
        return phaseTwo_ ? 0.62f : 0.55f;
    } // units / 60Hz frame
    /// @brief ダッシュ距離を返す。
    float GetDashDistance() const
    {
        return GetDashDuration() * GetDashSpeed() * 60.0f;
    }
    /// @brief ダッシュを終了する。
    void FinishDash()
    {
        if (phase_ != Phase::Dash)
            return;
        Enter(reactiveDash_ ? Phase::Reposition : Phase::Reload);
    }

    /// @brief 現在の状態を1段階更新する。
    /// @param dt この処理で進める経過時間（秒）。
    Shot Step(float dt, bool lineOfSight, float targetAngle, float hpRatio, bool incomingThreat = false)
    {
        Shot result{};
        if (!std::isfinite(dt) || dt <= 0.0f)
            return result;
        // Never consume more than one state transition / projectile volley in a hitch.
        const float step = (std::min)(dt, 0.10f);
        elapsed_ += step;
        dodgeCooldown_ = (std::max)(0.0f, dodgeCooldown_ - step);
        if (std::isfinite(hpRatio) && hpRatio <= 0.5f)
            phaseTwoPending_ = true;
        const float aim = std::isfinite(targetAngle) ? targetAngle : aimAngle_;
        const State& state = GetState(phase_);
        return state.Update(*this, lineOfSight, aim, incomingThreat);
    }

    /// @brief 攻撃1回に使う投射物の数を返す。
    static int ProjectileCount(Pattern pattern)
    {
        return pattern == Pattern::AimedBurst ? 2 : 3;
    }
    /// @brief 投射物ごとの発射位置の差分を返す。
    static float ProjectileOffset(Pattern pattern, int projectile, int round, int capacity)
    {
        if (pattern == Pattern::AimedBurst)
            return projectile == 0 ? -3.0f : 3.0f;
        if (pattern == Pattern::FanBurst) {
            // Alternating lanes; there is always space between the three rays.
            return -20.0f + 20.0f * static_cast<float>(projectile) + (round % 2 == 0 ? -5.0f : 5.0f);
        }
        return -32.0f + 64.0f * static_cast<float>(round) / static_cast<float>((std::max)(1, capacity - 1)) +
               (static_cast<float>(projectile) - 1.0f) * 5.0f;
    }
    /// @brief 攻撃予告の扇形の半角を返す。
    static float WarningHalfAngle(Pattern pattern)
    {
        return pattern == Pattern::AimedBurst ? 5.0f : (pattern == Pattern::FanBurst ? 27.0f : 39.0f);
    }
    // Projection rejects bullets travelling away or crossing well outside the
    // body. This allows pressure to trigger a limited dodge, without mind reading.
    /// @brief Incoming脅威であるか判定する。
    static bool IsIncomingThreat(float relativeX, float relativeY, float velocityX, float velocityY, float radius)
    {
        const float speedSq = velocityX * velocityX + velocityY * velocityY;
        if (!std::isfinite(speedSq) || speedSq < 0.0001f)
            return false;
        const float frames = -(relativeX * velocityX + relativeY * velocityY) / speedSq;
        if (frames < 0.0f || frames > 27.0f)
            return false;
        const float x = relativeX + velocityX * frames, y = relativeY + velocityY * frames;
        return x * x + y * y <= radius * radius;
    }

private:
    /// @brief RivalBossCombatの状態遷移の共通契約を定義する。具体的な状態は同じクラス内の派生型で表す。
    class State {
    public:
        /// @brief この型の終了処理を行う。所有している資源の寿命を終了させる。
        virtual ~State() = default;
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        virtual Shot Update(RivalBossCombat& boss, bool visible, float aim, bool threat) const = 0;
    };
    /// @brief RivalBossCombatで距離を取り直す状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class RepositionState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        Shot Update(RivalBossCombat& boss, bool visible, float aim, bool threat) const override
        {
            if (threat && boss.dodgeCooldown_ <= 0.0f && visible) {
                boss.reactiveDash_ = true;
                boss.dodgeCooldown_ = 5.5f;
                boss.Enter(Phase::DashWarning);
            } else if (boss.elapsed_ >= boss.Duration() && visible) {
                // Upgrade only at a magazine boundary, never refill mid-volley.
                boss.phaseTwo_ = boss.phaseTwoPending_;
                boss.capacity_ = boss.phaseTwo_ ? 5 : 4;
                boss.ammo_ = boss.capacity_;
                boss.pattern_ = boss.cycle_ % 3 == 1 ? Pattern::FanBurst : (boss.cycle_ % 3 == 2 ? Pattern::Sweep : Pattern::AimedBurst);
                boss.aimAngle_ = aim;
                boss.Enter(Phase::Tracking);
            }
            return {};
        }
    };
    /// @brief RivalBossCombatで対象を追尾する状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class TrackingState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        Shot Update(RivalBossCombat& boss, bool visible, float aim, bool) const override
        {
            boss.aimAngle_ = aim;
            if (!visible)
                boss.Enter(Phase::Reposition);
            else if (boss.elapsed_ >= boss.Duration())
                boss.Enter(Phase::Locked);
            return {};
        }
    };
    /// @brief RivalBossCombatで照準を固定する状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class LockedState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        Shot Update(RivalBossCombat& boss, bool, float, bool) const override
        {
            if (boss.elapsed_ < boss.Duration())
                return {};
            boss.Enter(Phase::Volley);
            return boss.FireRound();
        }
    };
    /// @brief RivalBossCombatで連射する状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class VolleyState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        Shot Update(RivalBossCombat& boss, bool, float, bool) const override
        {
            if (boss.elapsed_ < boss.Duration())
                return {};
            if (boss.ammo_ > 0)
                return boss.FireRound();
            boss.reactiveDash_ = false;
            boss.dodgeCooldown_ = (std::max)(boss.dodgeCooldown_, 2.0f);
            boss.Enter(Phase::DashWarning);
            return {};
        }
    };
    /// @brief RivalBossCombatで突進を予告する状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class DashWarningState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        Shot Update(RivalBossCombat& boss, bool, float, bool) const override
        {
            if (boss.elapsed_ >= boss.Duration())
                boss.Enter(Phase::Dash);
            return {};
        }
    };
    /// @brief RivalBossCombatで突進する状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class DashState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        Shot Update(RivalBossCombat& boss, bool, float, bool) const override
        {
            if (boss.elapsed_ >= boss.Duration())
                boss.FinishDash();
            return {};
        }
    };
    /// @brief RivalBossCombatで再装填する状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class ReloadState final : public State {
    public:
        /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
        Shot Update(RivalBossCombat& boss, bool, float, bool) const override
        {
            if (boss.elapsed_ >= boss.Duration()) {
                ++boss.cycle_;
                boss.ammo_ = boss.capacity_;
                boss.Enter(Phase::Reposition);
            }
            return {};
        }
    };
    /// @brief 状態を返す。
    static const State& GetState(Phase phase)
    {
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
    /// @brief 指定状態へ入る際の時間と条件を初期化する。
    void Enter(Phase next)
    {
        phase_ = next;
        elapsed_ = 0.0f;
        if (next == Phase::Dash)
            ++dashCount_;
        if (next == Phase::Reload)
            ++reloadCount_;
    }
    /// @brief 現在の動作の継続時間を返す。
    float Duration() const
    {
        switch (phase_) {
        case Phase::Reposition:
            return phaseTwo_ ? 0.48f : 0.62f;
        case Phase::Tracking:
            return phaseTwo_ ? 0.42f : 0.52f;
        case Phase::Locked:
            return 0.24f;
        case Phase::Volley:
            return pattern_ == Pattern::FanBurst ? 0.29f : 0.23f;
        case Phase::DashWarning:
            return reactiveDash_ ? 0.28f : 0.38f;
        case Phase::Dash:
            return GetDashDuration();
        case Phase::Reload:
            return phaseTwo_ ? 1.15f : 1.35f;
        }
        return 1.0f;
    }
    /// @brief Roundを発射する。
    Shot FireRound()
    {
        const int round = capacity_ - ammo_;
        --ammo_;
        ++shotsFired_;
        elapsed_ = 0.0f;
        return {true, pattern_, aimAngle_, round};
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
