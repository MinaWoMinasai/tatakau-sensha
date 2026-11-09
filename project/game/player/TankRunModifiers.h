#pragma once
#include <array>
#include <algorithm>
#include "../run/TankCardId.h"

/// @brief ランで選ぶコアの種類。
enum class TankRunCore {
    None = 0,
    Ricochet,
    Assault,
    Drone
};

/// @brief 育つ戦車の試作の部品による性能の倍率。既定はすべて1で、何も変えない。
struct TankPartScales {
    float maxHp = 1.0f;
    float bodyDamage = 1.0f;
    float bulletDamage = 1.0f;
    float reloadInterval = 1.0f;
    float moveSpeed = 1.0f;
    /// @brief スタミナ（ダッシュの回数）の上限に足す量。
    float extraStamina = 0.0f;
};

// その遠征の間だけの選択。空の値なら、元のゲームのルールは変わらない。
/// @brief 遠征で取得した強化効果と補正値を保持する。
struct TankRunModifiers {
    bool enabled = false;
    bool expedition = false;
    /// @brief 育つ戦車の試作。遠征の射撃スタイル向けの効果（貫通・連鎖放電・撃破バースト）を、部品で使えるようにする。
    bool survivor = false;
    /// @brief 育つ戦車の試作で、主砲の弾が壁で跳ね返る回数（反射砲）。0 なら跳ね返らない。
    int survivorWallBounces = 0;
    bool ricochet = false;
    bool heavy = false;
    bool rapid = false;
    bool thrusters = false;
    bool capacitor = false;
    bool repair = false;
    bool drones = false;
    bool pierce = false;
    TankRunCore core = TankRunCore::None;
    bool scatterShot = false;
    bool homing = false;
    bool dashBurst = false;
    bool overdrive = false;
    bool meleeBlade = false;
    bool bladeReach = false;
    bool impactDrive = false;
    bool perfectDodge = false;
    bool droneFocus = false;
    bool droneGuard = false;
    bool meleeTempo = false;
    bool finisherCharge = false;
    bool railCannon = false;
    bool droneLaserLink = false;
    bool slashWave = false;
    bool parryBlade = false;
    bool extraBarrel1 = false;
    bool extraBarrel2 = false;
    bool fanMount = false;
    bool alternatingFire = false;
    bool heavyDroneCore = false;
    bool lightBladeActuator = false;
    bool heavyBladeEdge = false;
    bool chainLightning = false;
    bool markDetonation = false;
    bool boomerangShell = false;
    bool killBurst = false;
    bool droneCharge = false;
    bool droneRebuildBomb = false;
    bool targetPainter = false;
    bool autonomousSpread = false;
    bool dashSlash = false;
    bool spinBlade = false;
    bool wallSmash = false;
    TankPartScales partScales{};
    std::array<float, tankrun::CardCount> effectPower = [] {
        std::array<float, tankrun::CardCount> p{};
        p.fill(1);
        return p;
    }();
};

/// @brief 指定した強化効果の強度を返す。
inline constexpr float TankEffectPower(const TankRunModifiers& modifiers, tankrun::CardId card)
{
    const size_t index = tankrun::CardIndex(card);
    const float p = index < modifiers.effectPower.size() ? modifiers.effectPower[index] : 1.0f;
    return p == p ? (std::clamp)(p, 0.1f, 5.0f) : 1.0f;
}

/// @brief 指定した強化効果の取得数を返す。
inline constexpr int TankEffectCount(const TankRunModifiers& modifiers, tankrun::CardId card, int base = 1)
{
    return (std::max)(1, static_cast<int>(static_cast<float>(base) * TankEffectPower(modifiers, card) + 0.5f));
}

/// @brief 遠征中の行動に基づく成長値を集計する。
struct TankRunGrowth {
    float hp = 0.15f;
    float damage = 0.25f;
    float bulletSpeed = 0.20f;
    float reload = 0.25f;
    float move = 0.12f;
};

// 変更のたびに初期値から組み立て直すので、何度適用しても効果は重ならない。
/// @brief 遠征全体の強化倍率や制限値を指定する。
struct TankRunTuning {
    float damage = 1.0f;
    float bulletSpeed = 1.0f;
    float reloadInterval = 1.0f;
    float moveSpeed = 1.0f;
    float staminaRecovery = 1.0f;
    float maxHp = 1.0f;
    float dashSpeed = 1.0f;
    float dashCooldown = 1.0f;
    float bulletHp = 0.0f;
    float bulletInterception = 0.0f;
    float bodyDamage = 1.0f;
    float extraStamina = 0.0f;
    int extraProjectiles = 0;
    float minimumSpread = 0.0f;
    bool reflects = false;
    int maxWallBounces = -1;
    int actorPierceCount = 0;
    int impactSplitCount = 0;
};

/// @brief 戦車遠征調整値を作成して返す。
inline constexpr TankRunTuning MakeTankRunTuning(const TankRunModifiers& modifiers, const TankRunGrowth& growth = {})
{
    TankRunTuning result{};
    if (!modifiers.enabled) {
        return result;
    }
    // 敵弾の打ち消しは、敵に与えたダメージとは独立している。
    result.bulletHp = 1.0f;
    result.bulletInterception = modifiers.pierce ? 1.0f + 2.0f * TankEffectPower(modifiers, tankrun::CardId::Pierce) : 1.0f;
    result.dashSpeed = 1.85f;
    result.reflects = modifiers.ricochet || modifiers.core == TankRunCore::Ricochet;
    if (modifiers.expedition) {
        result.maxWallBounces = modifiers.core == TankRunCore::Ricochet
                                    ? 2 + (modifiers.ricochet ? TankEffectCount(modifiers, tankrun::CardId::Ricochet, 2) : 0)
                                    : (modifiers.ricochet ? TankEffectCount(modifiers, tankrun::CardId::Ricochet) : 0);
        result.actorPierceCount = modifiers.pierce ? TankEffectCount(modifiers, tankrun::CardId::Pierce) : 0;
        result.impactSplitCount = 0; // 旧来の拡散データで弾数が増えることはもう無い。
    } else if (modifiers.survivor) {
        result.actorPierceCount = modifiers.pierce ? TankEffectCount(modifiers, tankrun::CardId::Pierce) : 0;
    }
    if (modifiers.core == TankRunCore::Ricochet) {
        result.damage *= 1.20f;
        result.bulletSpeed *= 1.25f;
        if (modifiers.ricochet) {
            result.bulletSpeed *= 1.0f + 0.2f * TankEffectPower(modifiers, tankrun::CardId::Ricochet);
        }
    } else if (modifiers.core == TankRunCore::Assault) {
        result.damage *= 1.80f;
        result.bulletSpeed *= 0.85f;
        result.reloadInterval *= 1.6f;
    }
    if (modifiers.heavy) {
        const float p = TankEffectPower(modifiers, tankrun::CardId::Heavy);
        result.damage *= 1.0f + (modifiers.expedition ? growth.damage : 0.7f) * p;
        result.bulletSpeed *= modifiers.expedition ? 1.0f + growth.bulletSpeed * p : (std::max)(0.05f, 1.0f - 0.28f * p);
        result.reloadInterval *= 1.0f + (modifiers.expedition ? 0.1f : 0.2f) * p;
    }
    if (modifiers.rapid) {
        const float p = TankEffectPower(modifiers, tankrun::CardId::Rapid);
        result.damage *= modifiers.expedition ? 1.0f : (std::max)(0.05f, 1.0f - 0.2f * p);
        result.reloadInterval *= (std::max)(0.05f, 1.0f - (modifiers.expedition ? growth.reload : 0.35f) * p);
    }
    if (modifiers.thrusters) {
        const float p = TankEffectPower(modifiers, tankrun::CardId::Thrusters);
        result.moveSpeed = 1.0f + (modifiers.expedition ? growth.move : 0.18f) * p;
        result.staminaRecovery = 1.0f + 0.25f * p;
        result.dashSpeed *= 1.0f + 0.25f * p;
        result.dashCooldown = (std::max)(0.05f, 1.0f - 0.2f * p);
    }
    if (modifiers.repair) {
        result.maxHp = 1.0f + (modifiers.expedition ? growth.hp : 0.25f) * TankEffectPower(modifiers, tankrun::CardId::Repair);
    }
    // 試作の部品の倍率は最後に掛ける。既定の1を掛けても値は変わらない。
    result.maxHp *= modifiers.partScales.maxHp;
    result.damage *= modifiers.partScales.bulletDamage;
    result.reloadInterval *= modifiers.partScales.reloadInterval;
    result.moveSpeed *= modifiers.partScales.moveSpeed;
    result.bodyDamage = modifiers.partScales.bodyDamage;
    result.extraStamina = modifiers.partScales.extraStamina;
    return result;
}

// アクターと、描画を使わない回帰テストで共有する。勢いは両方の軸で保つので、横向きの
// ダッシュは、動いている戦車の進路を急に切り替えずに曲げる。
/// @brief 強化状態に応じたダッシュの勢いを返す。
inline constexpr float TankDashMomentum(float previous, float direction, float speed)
{
    return previous * 0.20f + direction * speed;
}

/// @brief 強化状態がジャスト回避を使用できる条件か判定する。
inline constexpr bool TankCanPerfectDodge(bool enabled, bool enemyBullet, float elapsed, float power = 1.0f)
{
    return enabled && enemyBullet && elapsed >= 0.0f && elapsed <= (std::min)(0.30f, 0.20f * power);
}

/// @brief 近接コンボの各段の速度・範囲・威力を指定する。
struct TankMeleeComboTuning {
    float damage = 1.0f, range = 1.0f, arc = 120.0f;
    float windup = 0.05f, duration = 0.14f, recovery = 0.14f;
    float knockback = 0.16f;
};

/// @brief 近接攻撃の連撃の各段の性能を作って返す。
inline constexpr TankMeleeComboTuning MakeTankMeleeCombo(int step, bool reach, bool impact, bool tempo = false, bool finisherCharge = false)
{
    TankMeleeComboTuning result{};
    if (step == 1) {
        result.damage = 1.10f;
        result.arc = 145.0f;
        result.knockback = 0.18f;
    }
    if (step == 2) {
        result.damage = 1.70f;
        result.range = 1.10f;
        result.arc = 200.0f;
        result.windup = 0.10f;
        result.duration = 0.20f;
        result.recovery = 0.21f;
        result.knockback = 0.36f;
    }
    if (reach) {
        result.range *= 1.30f;
        result.damage *= 1.20f;
    }
    if (impact) {
        result.knockback *= 1.50f;
    }
    if (tempo) {
        result.windup *= 0.82f;
        result.duration *= 0.82f;
        result.recovery *= 0.82f;
    }
    if (finisherCharge && step == 2) {
        result.damage *= 1.50f;
        result.knockback *= 1.20f;
    }
    return result;
}

/// @brief 強化の状態から、近接攻撃の連撃の各段の性能を作って返す。
inline constexpr TankMeleeComboTuning MakeTankMeleeCombo(int step, const TankRunModifiers& modifiers)
{
    auto result = MakeTankMeleeCombo(step, false, false);
    if (!modifiers.enabled) {
        return result;
    }
    if (modifiers.bladeReach) {
        const float p = TankEffectPower(modifiers, tankrun::CardId::BladeReach);
        result.range *= 1.0f + .3f * p;
        result.damage *= 1.0f + .2f * p;
    }
    if (modifiers.impactDrive) {
        result.knockback *= 1.0f + .5f * TankEffectPower(modifiers, tankrun::CardId::ImpactDrive);
    }
    if (modifiers.meleeTempo) {
        const float rate = (std::max)(.05f, 1.0f - .18f * TankEffectPower(modifiers, tankrun::CardId::MeleeTempo));
        result.windup *= rate;
        result.duration *= rate;
        result.recovery *= rate;
    }
    if (modifiers.finisherCharge && step == 2) {
        const float p = TankEffectPower(modifiers, tankrun::CardId::FinisherCharge);
        result.damage *= 1.0f + .5f * p;
        result.knockback *= 1.0f + .2f * p;
    }
    if (modifiers.lightBladeActuator) {
        const float p = TankEffectPower(modifiers, tankrun::CardId::LightBladeActuator);
        result.damage *= (std::max)(.1f, 1 - .05f * p);
        const float speed = (std::max)(.2f, 1 - .15f * p);
        result.windup *= speed;
        result.duration *= speed;
        result.recovery *= speed;
    }
    if (modifiers.heavyBladeEdge) {
        const float p = TankEffectPower(modifiers, tankrun::CardId::HeavyBladeEdge);
        result.damage *= 1 + .45f * p;
        const float speed = 1 + .18f * p;
        result.windup *= speed;
        result.duration *= speed;
        result.recovery *= speed;
    }
    return result;
}

// アクターとカードのデモで、同じ攻撃のトレードオフを使う。これらの倍率は、
// 持ち主の攻撃力・弾速・リロードの強化と掛け合わせる。
/// @brief ドローンの移動・攻撃・特殊能力の調整値を指定する。
struct TankDroneTuning {
    float damageScale = 0.35f, reloadSeconds = 0.75f, spreadDegrees = 6.0f;
    float bulletHp = 1.0f, interception = 1.0f;
    float sizeScale = 1.0f, trailScale = 1.0f;
};

/// @brief 戦車ドローン調整値を作成して返す。
inline constexpr TankDroneTuning MakeTankDroneTuning(const TankRunModifiers& modifiers, bool primaryStyle)
{
    TankDroneTuning result{};
    if (primaryStyle) {
        result.damageScale = 1.0f;
        result.reloadSeconds = 0.5f;
        result.spreadDegrees = 0.0f;
    }
    if (!modifiers.enabled) {
        return result;
    }
    if (modifiers.droneFocus) {
        const float p = TankEffectPower(modifiers, tankrun::CardId::DroneFocus);
        result.damageScale *= 1.0f + (modifiers.expedition ? .15f : .4f) * p;
        if (!modifiers.expedition) {
            result.reloadSeconds *= 1.0f + .15f * p;
        }
        result.spreadDegrees = 0;
    }
    if (modifiers.heavyDroneCore) {
        const float p = TankEffectPower(modifiers, tankrun::CardId::HeavyDroneCore);
        result.damageScale *= 1.0f + .45f * p;
        result.reloadSeconds *= 1.0f + .2f * p;
        result.sizeScale = 1 + .2f * p;
        result.trailScale = 1 + .25f * p;
    }
    if (modifiers.droneGuard) {
        result.bulletHp = 1.0f + 2.0f * TankEffectPower(modifiers, tankrun::CardId::DroneGuard);
        result.interception = result.bulletHp;
    }
    return result;
}

// カードをまたぐ効果は、発射・ダッシュの瞬間に組み合わせる。一時的な Overdrive の状態を
// 分けておくことで、追尾の強化が永続してしまうのを防ぐ。
/// @brief 取得済みの強化の組み合わせによる相乗効果を表す。
struct TankRunSynergy {
    float homingTurnRate = 0.0f;
    bool dashExplosion = false;
    float explosionRadius = 3.2f;
    float explosionDamageScale = 2.0f;
};

/// @brief 強化の組み合わせによる相乗効果を作って返す。
inline constexpr TankRunSynergy MakeTankRunSynergy(const TankRunModifiers& modifiers, bool overdriveActive)
{
    TankRunSynergy result{};
    if (!modifiers.enabled) {
        return result;
    }
    if (modifiers.homing) {
        result.homingTurnRate = (modifiers.expedition && modifiers.overdrive && overdriveActive ? 3.2f : 1.6f) *
                                TankEffectPower(modifiers, tankrun::CardId::Homing);
    }
    result.dashExplosion = modifiers.expedition && modifiers.heavy && modifiers.dashBurst;
    result.explosionDamageScale = (result.dashExplosion ? 2.0f : 1.4f) * TankEffectPower(modifiers, tankrun::CardId::DashBurst);
    return result;
}
