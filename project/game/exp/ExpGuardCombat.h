#pragma once
#include "ExpEnemyCombatCycle.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

// 敵戦闘の既存テストからも利用する、描画資源を持たない小さな戦闘規則。
namespace expguard {
inline constexpr float kShieldHalfAngle = 55.0f * 3.14159265f / 180.0f;
inline constexpr float kBladeHalfAngle = 55.0f * 3.14159265f / 180.0f;
inline constexpr float kBladeReach = 3.3f;
inline constexpr float kReflectHalfAngle = 50.0f * 3.14159265f / 180.0f;
inline constexpr float kEmpRadius = 8.0f;
inline constexpr int kSummonAliveLimit = 3;
inline constexpr int kSummonLifetimeLimit = 6;
inline constexpr float kSummonLifetimeSeconds = 16.0f;

/// @brief パルスの待ち時間と予告時間を保持し、予告終了時の発動を呼び出し側へ通知する。
class PulseCycle {
public:
    /// @brief 秒単位の周期・予告・初回待ちを設定し、予告中フラグを解除する。
    /// @note 予告は最低0.25秒、周期から予告を引いた待ち時間は最低0.5秒、初回待ちは最低0秒。
    void Reset(float period, float warning, float initialDelay)
    {
        warningSeconds_ = (std::max)(0.25f, warning);
        cooldownSeconds_ = (std::max)(0.5f, period - warningSeconds_);
        remaining_ = (std::max)(0.0f, initialDelay);
        warning_ = false;
    }
    /// @brief 待ち/予告の時間を進め、予告終了時に待ち状態へ戻してtrueを返す。
    /// @param dt 秒数。非有限値・0以下なら状態を変えずfalse。
    /// @param canStart 待ち時間終了後に予告を開始できるか。開始済みの予告はfalseでも最後まで進む。
    /// @note falseでも時間や予告状態は変わり得る。余った時間は繰り越さず、長い更新でも予告全体を飛ばさない。
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
/// @brief 同じ指揮官の生存数と生成累計から、同時上限3・累計上限6までの残り枠数を計算する。生成は行わない。
inline int SummonSlots(int alive, int total)
{
    return (std::max)(0, (std::min)(kSummonAliveLimit - alive, kSummonLifetimeLimit - total));
}

/// @brief XYの向きと対象までの相対位置から、扇形の境界を含む接触条件を判定する。
/// @param halfAngle 扇形の半角（ラジアン）。
/// @param targetRadius 対象のワールド半径。距離に応じて半角へ余裕を加える。
/// @note 向きがほぼ0や長さが非有限ならfalse。対象がほぼ中心にあればtrue。
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

/// @brief 正面盾の扇形内なら元のダメージを近接40%、その他15%へ減らして切り上げ、扇形外なら元の値を返す。
/// @note 対象のHPや盾の演出は変更しない。攻撃元がほぼ中心の場合は方向を決めず、元の値を返す。
inline uint32_t ShieldDamage(uint32_t amount, float facingX, float facingY, float sourceOffsetX, float sourceOffsetY, bool melee = false)
{
    // 中心とほぼ同じ攻撃元には正面/背面の向きがないため、盾による減衰を適用しない。
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
    /// @brief 次の回復時間へ使う倍率を1〜4に制限する。予告/攻撃の長さと開始済み回復の残り時間は変えない。
    void SetIntervalScale(float scale)
    {
        // 制作時の調整でも、回復時間を既定値より短くしない。
        recoverySeconds_ = kRecoverySeconds * (std::clamp)(scale, 1.0f, 4.0f);
    }
    /// @brief 秒数を進め、予告終了からActiveへ入る呼び出しだけtrueを返す。非有限値・0以下は対象外。
    /// @note inAttackRangeは待ち終了後の予告開始だけに使う。falseの戻り値でも時間や段階は変わり得る。
    /// 1呼び出しに1段階だけ進み、実際のダメージはTryHitの後に呼び出し側が適用する。
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
    /// @brief Active中の未消費の振りで、射程・扇形・visibleを満たしたら受付を消費しtrueを返す。
    /// @note HPは変更しない。呼び出し側がダメージを適用できなくても、同じ振りは再受付しない。falseでは受付を消費しない。
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
    /// @brief 予告の照準固定中または攻撃実行中かを返す。
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
    /// @brief Recoveryの残り時間の割合を0〜1で返す。終了へ向かうほど小さくなり、それ以外は0。
    float RecoveryRatio() const
    {
        return phase_ == ExpEnemyCombatPhase::Recovery ? (std::clamp)(remaining_ / recoverySeconds_, 0.0f, 1.0f) : 0.0f;
    }
    /// @brief Activeへ入り斬撃を開始した累計回数を返す。命中回数ではない。
    uint32_t SwingCount() const
    {
        return swingCount_;
    }

private:
    /// @brief expguard::BladeCycleの状態遷移の共通契約を定義する。具体的な状態は同じクラス内の派生型で表す。
    class State {
    public:
        /// @brief 派生状態を基底型経由で破棄できるようにする。
        virtual ~State() = default;
        /// @brief 現在の段階の終了時に次段階へ遷移する。Activeへ入った場合だけtrueを返す。
        virtual bool Update(BladeCycle& cycle, bool inRange) const = 0;
    };
    /// @brief expguard::BladeCycleで次の攻撃まで待機する状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class CooldownState final : public State {
    public:
        /// @brief 現在の段階の終了時に次段階へ遷移する。Activeへ入った場合だけtrueを返す。
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
        /// @brief 現在の段階の終了時に次段階へ遷移する。Activeへ入った場合だけtrueを返す。
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
        /// @brief 現在の段階の終了時に次段階へ遷移する。Activeへ入った場合だけtrueを返す。
        bool Update(BladeCycle& cycle, bool) const override
        {
            cycle.Enter(ExpEnemyCombatPhase::Recovery, cycle.recoverySeconds_);
            return false;
        }
    };
    /// @brief expguard::BladeCycleで攻撃後に回復する状態を表す。状態ごとの更新と次状態への遷移を担当する。
    class RecoveryState final : public State {
    public:
        /// @brief 現在の段階の終了時に次段階へ遷移する。Activeへ入った場合だけtrueを返す。
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
