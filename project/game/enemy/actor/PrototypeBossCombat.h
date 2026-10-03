#pragma once
#include <algorithm>
#include <cmath>

/// @brief 試作ボスの攻撃時計・パターン・固定照準を保持し、発射要求を計算する。
/// @note 描画・弾生成・移動は担当せず、渡された時間だけで進行する。
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
    /// @brief 発射の有無、パターン、照準角（ラジアン）、攻撃圧力を保持する。位置・弾性能は呼び出し側で決める。
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
    /// @brief 固定した照準角（ラジアン）を返す。
    float GetAimAngle() const
    {
        return aimAngle_;
    }
    /// @brief 進行度を返す。
    float GetProgress() const
    {
        return phase_ == Phase::Telegraph ? (std::clamp)(elapsed_ / telegraphDuration_, 0.0f, 1.0f) : 0.0f;
    }
    /// @brief 予告範囲の全幅（度）を返す。GapRingは安全な開口幅、それ以外は危険な扇形の幅。
    float GetSpreadAngleDeg() const
    {
        return type_ == AttackType::GapRing ? 70.0f : (type_ == AttackType::Sweep ? 90.0f : 44.0f);
    }
    /// @brief 現在の行動がその場に留まる状態か判定する。
    bool HoldsPosition() const
    {
        return phase_ != Phase::Recovery;
    }

    /// @brief 攻撃時計・状態を進め、弾生成の要求を返す。fireがfalseでも時計・状態は変わり得る。
    /// @param dt 経過秒数。有限の正値だけを使い、1回に進める時間は0.10秒まで。
    /// @param targetAngle 攻撃開始時に固定する照準角（ラジアン）。非有限値は0に置き換える。
    /// @note 予告/攻撃中はcanBeginAttackがfalseでも進む。pressureは0～4、recoveryScaleは0.20～10に制限する。
    Shot Step(float dt, bool canBeginAttack, float targetAngle, float hpRatio, int pressure, bool foraging, float recoveryScale = 1.0f)
    {
        Shot shot{};
        if (!std::isfinite(dt) || dt <= 0.0f)
            return shot;
        // 長い1回のdtを0.10秒に制限し、予告を飛ばしたり掃射をまとめて要求したりしない。
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

    /// @brief 1回の発射要求から生成する弾数を返す。
    static int GetProjectileCount(AttackType type)
    {
        return type == AttackType::GapRing ? 16 : (type == AttackType::AimedSpread ? 5 : 1);
    }
    /// @brief 基準照準から各弾へ加える角度差（度）を返す。
    /// @param index 0以上、GetProjectileCount(type)未満の弾番号。関数内では範囲を制限しない。
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
