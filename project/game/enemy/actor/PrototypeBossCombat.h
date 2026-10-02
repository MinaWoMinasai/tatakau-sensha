#pragma once
#include <algorithm>
#include <cmath>

// Pure timing/pattern state: no renderer, actor, or wall-clock dependencies.
/// @brief 試作ボスの攻撃予告と攻撃の進行を計算する。
class PrototypeBossCombat {
public:
    enum class AttackType {
        AimedSpread,
        GapRing,
        Sweep
    };
    enum class Phase {
        Recovery,
        Telegraph,
        Attack
    };
    /// @brief 1回の攻撃で発生させる弾の位置・方向・性能を表す。
    struct Shot {
        bool fire = false;
        AttackType type = AttackType::AimedSpread;
        float angleRadians = 0.0f;
        int pressure = 0;
    };

    /// @brief 状態と集計値を初期状態へ戻す。
    void Reset()
    {
        *this = PrototypeBossCombat{};
    }
    /// @brief 段階を返す。
    Phase GetPhase() const
    {
        return phase_;
    }
    /// @brief 攻撃種類を返す。
    AttackType GetAttackType() const
    {
        return type_;
    }
    /// @brief 照準角度を返す。
    float GetAimAngle() const
    {
        return aimAngle_;
    }
    /// @brief 進行度を返す。
    float GetProgress() const
    {
        return phase_ == Phase::Telegraph ? (std::clamp)(elapsed_ / telegraphDuration_, 0.0f, 1.0f) : 0.0f;
    }
    /// @brief 拡散角度度を返す。
    float GetSpreadAngleDeg() const
    {
        // For GapRing this is the width of the safe opening, not the danger arc.
        return type_ == AttackType::GapRing ? 70.0f : (type_ == AttackType::Sweep ? 90.0f : 44.0f);
    }
    /// @brief 現在の行動がその場に留まる状態か判定する。
    bool HoldsPosition() const
    {
        return phase_ != Phase::Recovery;
    }

    /// @brief 現在の状態を1段階更新する。
    /// @param dt この処理で進める経過時間（秒）。
    Shot Step(float dt, bool canBeginAttack, float targetAngle, float hpRatio, int pressure, bool foraging, float recoveryScale = 1.0f)
    {
        Shot shot{};
        if (!std::isfinite(dt) || dt <= 0.0f)
            return shot;
        // A hitch must not skip the visible warning or release a burst of shots.
        elapsed_ += (std::min)(dt, 0.10f);
        if (phase_ == Phase::Recovery) {
            if (elapsed_ < recoveryDuration_ * (std::clamp)(recoveryScale, 0.20f, 10.0f) || !canBeginAttack)
                return shot;
            pressure_ = (std::clamp)(pressure, 0, 4);
            aimAngle_ = std::isfinite(targetAngle) ? targetAngle : 0.0f;
            type_ = AttackType::AimedSpread;
            if (!foraging && cycleIndex_ % 2 == 1 && (hpRatio <= 0.75f || pressure_ > 0)) {
                type_ = AttackType::GapRing;
            } else if (!foraging && cycleIndex_ % 4 == 2 && hpRatio <= 0.40f) {
                type_ = AttackType::Sweep;
            }
            ++cycleIndex_;
            telegraphDuration_ = (type_ == AttackType::AimedSpread ? 0.95f : 1.20f) - 0.035f * pressure_;
            phase_ = Phase::Telegraph;
            elapsed_ = 0.0f;
            return shot;
        }
        if (phase_ == Phase::Telegraph) {
            if (elapsed_ < telegraphDuration_)
                return shot;
            phase_ = Phase::Attack;
            elapsed_ = 0.0f;
            sweepShotIndex_ = 0;
        } else if (type_ == AttackType::Sweep) {
            if (elapsed_ < 0.16f)
                return shot;
            elapsed_ = 0.0f;
        }

        shot = {true, type_, aimAngle_, pressure_};
        if (type_ == AttackType::Sweep) {
            constexpr float radiansPerDegree = 3.1415926535f / 180.0f;
            shot.angleRadians += (-45.0f + 15.0f * sweepShotIndex_) * radiansPerDegree;
            ++sweepShotIndex_;
            if (sweepShotIndex_ < 7)
                return shot;
        }
        phase_ = Phase::Recovery;
        elapsed_ = 0.0f;
        recoveryDuration_ = type_ == AttackType::AimedSpread ? 1.10f : 1.45f;
        return shot;
    }

    /// @brief 投射物件数を返す。
    static int GetProjectileCount(AttackType type)
    {
        return type == AttackType::GapRing ? 16 : (type == AttackType::AimedSpread ? 5 : 1);
    }
    /// @brief 投射物差分度を返す。
    static float GetProjectileOffsetDeg(AttackType type, int index)
    {
        if (type == AttackType::GapRing)
            return 35.0f + 290.0f * index / 15.0f;
        if (type == AttackType::AimedSpread)
            return -22.0f + 11.0f * index;
        return 0.0f;
    }

private:
    Phase phase_ = Phase::Recovery;
    AttackType type_ = AttackType::AimedSpread;
    float elapsed_ = 0.0f;
    float recoveryDuration_ = 1.20f;
    float telegraphDuration_ = 0.95f;
    float aimAngle_ = 0.0f;
    unsigned int cycleIndex_ = 0;
    int sweepShotIndex_ = 0;
    int pressure_ = 0;
};
