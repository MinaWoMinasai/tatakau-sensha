#pragma once
#include <algorithm>
#include <array>
#include <cmath>

/// @brief 遠征ライバルの照準・残弾・行動時計を保持し、連射・突進・再装填の状態と発射要求を計算する。
/// @note 描画・弾生成・経路探索・位置更新・衝突処理はEnemy側の担当。
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
    /// @brief 発射の有無、パターン、固定照準角（ラジアン）、弾倉内の発射回番号を保持する。位置や弾性能は含まない。
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
    /// @brief 現在の射撃パターンを返す。
    Pattern GetPattern() const
    {
        return pattern_;
    }
    /// @brief HP低下後、弾倉の切り替え時に第2段階へ移行済みかを返す。
    bool IsPhaseTwo() const
    {
        return phaseTwo_;
    }
    /// @brief 残弾数を返す。
    int GetAmmo() const
    {
        return ammo_;
    }
    /// @brief 現在の弾倉で要求する発射回数の上限を返す。1回の弾数はProjectileCountで別に決まる。
    int GetCapacity() const
    {
        return capacity_;
    }
    /// @brief Reset以降に発射要求を返した回数を返す。生成された弾の総数ではない。
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
    /// @brief 追尾・固定に使う照準角（ラジアン）を返す。
    float GetAimAngle() const
    {
        return aimAngle_;
    }
    /// @brief 再装填完了の周期ごとに交互となる、横移動方向の符号（+1または-1）を返す。
    float GetStrafeSign() const
    {
        return cycle_ % 2 == 0 ? 1.0f : -1.0f;
    }
    /// @brief 進行度を返す。
    float GetProgress() const
    {
        return (std::clamp)(elapsed_ / Duration(), 0.0f, 1.0f);
    }
    /// @brief 現在の突進設定が接近する弾への反応による回避かを返す。行動段階の判定とは別。
    bool IsReactiveDash() const
    {
        return reactiveDash_;
    }
    /// @brief 突進の行動時計上の継続時間（秒）を返す。
    float GetDashDuration() const
    {
        return phaseTwo_ ? 0.30f : 0.28f;
    }
    /// @brief 突進速度（60FPS基準の1フレーム当たりのワールド単位）を返す。
    float GetDashSpeed() const
    {
        return phaseTwo_ ? 0.62f : 0.55f;
    }
    /// @brief 突進時間・速度を60FPS基準で掛けた移動距離（ワールド単位）を返す。実際の通行可能距離はEnemy側で制限する。
    float GetDashDistance() const
    {
        return GetDashDuration() * GetDashSpeed() * 60.0f;
    }
    /// @brief 突進中なら、回避突進は位置取りへ、通常突進は再装填へ移る。その他の段階では何もしない。
    void FinishDash()
    {
        if (phase_ != Phase::Dash)
            return;
        Enter(reactiveDash_ ? Phase::Reposition : Phase::Reload);
    }

    /// @brief 行動時計・HP低下の予約・状態を更新し、1回分の発射要求を返す。fireがfalseでも状態は変わり得る。
    /// @param dt 経過秒数。有限の正値だけを使い、1回に進める時間は0.10秒まで。
    /// @param targetAngle 追尾に使う照準角（ラジアン）。非有限値では現在の照準角を使う。
    /// @note HP比率が0.5以下なら第2段階への移行を予約する。照準固定後の発射はlineOfSightがfalseでも進む。
    Shot Step(float dt, bool lineOfSight, float targetAngle, float hpRatio, bool incomingThreat = false)
    {
        Shot result{};
        if (!std::isfinite(dt) || dt <= 0.0f)
            return result;
        // 1回の経過時間を0.10秒に制限し、その呼び出しの行動状態だけを処理する。長いフレームで連射をまとめない。
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
    /// @brief 固定照準から各弾へ加える角度差（度）を返す。発射位置の差分ではない。
    /// @note projectileは0～ProjectileCount(pattern)-1、roundは弾倉内の発射回番号。これらの範囲は関数内で制限しない。
    static float ProjectileOffset(Pattern pattern, int projectile, int round, int capacity)
    {
        if (pattern == Pattern::AimedBurst)
            return projectile == 0 ? -3.0f : 3.0f;
        if (pattern == Pattern::FanBurst) {
            // 3本の角度を20度間隔にし、発射回ごとに全体を左右5度ずらす。
            return -20.0f + 20.0f * static_cast<float>(projectile) + (round % 2 == 0 ? -5.0f : 5.0f);
        }
        return -32.0f + 64.0f * static_cast<float>(round) / static_cast<float>((std::max)(1, capacity - 1)) +
               (static_cast<float>(projectile) - 1.0f) * 5.0f;
    }
    /// @brief 攻撃予告の扇形の半角（度）を返す。
    static float WarningHalfAngle(Pattern pattern)
    {
        return pattern == Pattern::AimedBurst ? 5.0f : (pattern == Pattern::FanBurst ? 27.0f : 39.0f);
    }
    /// @brief 相対位置と速度から、27基準フレーム以内の最近接点が半径内に入る弾かを判定する。
    /// @note 位置・半径はワールド単位、速度は60FPS基準の1フレーム当たり。離れる弾やほぼ停止した弾は除く。
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
        /// @brief 派生した行動状態を基底型から破棄するための仮想デストラクタ。
        virtual ~State() = default;
        /// @brief 行動状態を更新し、発射要求を返す。移動・描画・弾生成は行わない。
        virtual Shot Update(RivalBossCombat& boss, bool visible, float aim, bool threat) const = 0;
    };
    /// @brief RivalBossCombatで距離を取り直す状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class RepositionState final : public State {
    public:
        /// @brief 接近弾への回避、または見通しがある場合の弾倉準備・照準追尾への遷移を行う。
        Shot Update(RivalBossCombat& boss, bool visible, float aim, bool threat) const override
        {
            if (threat && boss.dodgeCooldown_ <= 0.0f && visible) {
                boss.reactiveDash_ = true;
                boss.dodgeCooldown_ = 5.5f;
                boss.Enter(Phase::DashWarning);
            } else if (boss.elapsed_ >= boss.Duration() && visible) {
                // 第2段階への移行は新しい弾倉の開始時に反映し、連射途中では残弾を増やさない。
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
        /// @brief 照準を追尾し、遮蔽なら位置取りへ、追尾時間終了なら照準固定へ移る。
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
        /// @brief 固定予告の時間終了で連射へ移り、最初の発射要求を返す。
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
        /// @brief 発射間隔ごとに残弾を消費して発射要求を返し、弾切れなら突進予告へ移る。
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
        /// @brief 突進予告の時間終了で突進へ移る。
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
        /// @brief 突進時間の終了を判定し、終了後の行動へ移る。位置は更新しない。
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
        /// @brief 再装填時間の終了で周期を増やして残弾を回復し、位置取りへ戻る。
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
    /// @brief 段階を設定して経過時間を0に戻す。突進/再装填へ入る場合は集計回数も増やす。
    void Enter(Phase next)
    {
        phase_ = next;
        elapsed_ = 0.0f;
        if (next == Phase::Dash)
            ++dashCount_;
        if (next == Phase::Reload)
            ++reloadCount_;
    }
    /// @brief 現在の行動時計上の継続時間（秒）を返す。
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
    /// @brief 残弾を1減らし、発射回数を増やして待ち時間を0に戻し、弾倉内番号を含む発射要求を返す。弾は生成しない。
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
