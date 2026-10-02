#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include "../run/TankBuildStyle.h"

// Direct base values for the three expedition families. Evolution and purchased
// modifiers compose on top; they never overwrite this authoring configuration.
/// @brief 戦闘系統ごとの基礎性能と操作上の特徴を指定する。
struct TankCombatStyleProfile {
    float maxHp = 120.0f, moveSpeed = 0.23f, maxStamina = 3.0f, staminaRecovery = 0.9f, bodyDamage = 3.0f;
    float attackDamage = 6.0f, attackIntervalSeconds = 0.30f, bulletSpeed = 0.27f;
    float meleeRange = 4.3f, meleeKnockback = 0.16f;
    int droneCount = 3;
    float droneFollowSpeed = 0.25f, droneCatchupSpeed = 0.62f, droneResponse = 5.0f, droneFormationRadius = 1.5f;
};
using TankCombatStyleBalances = std::array<TankCombatStyleProfile, 3>;
/// @brief 戦車戦闘外観Balancesの既定値を作成して返す。
inline constexpr TankCombatStyleBalances DefaultTankCombatStyleBalances()
{
    TankCombatStyleBalances profiles{};
    profiles[1].attackDamage = 3.0f;
    profiles[1].attackIntervalSeconds = 0.5f;
    profiles[2].attackDamage = 15.2f;
    profiles[2].attackIntervalSeconds = 0.33f;
    return profiles;
}
/// @brief 戦闘系統の設定値を対応範囲内へ補正する。
inline TankCombatStyleProfile SanitizeTankCombatStyleProfile(TankCombatStyleProfile p, const TankCombatStyleProfile& defaults = {})
{
    auto safe = [](float value, float fallback, float low, float high) {
        return std::isfinite(value) ? (std::clamp)(value, low, high) : fallback;
    };
    p.maxHp = safe(p.maxHp, defaults.maxHp, 1, 9999);
    p.moveSpeed = safe(p.moveSpeed, defaults.moveSpeed, 0.01f, 2);
    p.maxStamina = safe(p.maxStamina, defaults.maxStamina, 0, 50);
    p.staminaRecovery = safe(p.staminaRecovery, defaults.staminaRecovery, 0, 50);
    p.bodyDamage = safe(p.bodyDamage, defaults.bodyDamage, 1, 999);
    p.attackDamage = safe(p.attackDamage, defaults.attackDamage, 0.1f, 999);
    p.attackIntervalSeconds = safe(p.attackIntervalSeconds, defaults.attackIntervalSeconds, 0.05f, 10);
    p.bulletSpeed = safe(p.bulletSpeed, defaults.bulletSpeed, 0.01f, 4);
    p.meleeRange = safe(p.meleeRange, defaults.meleeRange, 0.5f, 20);
    p.meleeKnockback = safe(p.meleeKnockback, defaults.meleeKnockback, 0, 0.95f);
    p.droneCount = (std::clamp)(p.droneCount, 1, 12);
    p.droneFollowSpeed = safe(p.droneFollowSpeed, defaults.droneFollowSpeed, 0.01f, 2);
    p.droneCatchupSpeed = safe(p.droneCatchupSpeed, defaults.droneCatchupSpeed, p.droneFollowSpeed, 4);
    p.droneResponse = safe(p.droneResponse, defaults.droneResponse, 0.1f, 30);
    p.droneFormationRadius = safe(p.droneFormationRadius, defaults.droneFormationRadius, 0, 8);
    return p;
}
/// @brief 戦闘系統に対応するドローン数を返す。
inline constexpr int TankCombatStyleDroneCount(int baseCount, int authoredCount, bool evolved, int supportCount)
{
    return (std::clamp)(baseCount + (evolved ? authoredCount - 3 : 0) + supportCount, 1, 12);
}
