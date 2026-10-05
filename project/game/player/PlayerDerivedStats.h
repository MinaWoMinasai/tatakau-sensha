#pragma once
#include <algorithm>
#include <array>
#include "TankCombatStyleBalance.h"
#include "TankRunModifiers.h"

// Actor-independent runtime values. Player keeps a source-compatible alias.
struct TankPlayerStats {
    float reloadSpeed = 10.0f;
    float bulletDamage = 1.0f;
    float bulletSpeed = 0.3f;
    float moveSpeed = 0.2f;
    float maxHp = 10000.0f;
    float staminaRecovery = 1.0f;
    float stamina = 3.0f;
    float maxStamina = 3.0f;
    float bodyDamage = 3.0f;
};

struct PlayerUpgradeRates {
    float staminaRecovery = 0.08f, maxHp = 0.10f, bodyDamage = 0.10f;
    float bulletSpeed = 0.08f, bulletDamage = 0.10f, reload = 0.07f, moveSpeed = 0.06f;
    float minimumReloadFrames = 3.0f;
};

// Only values used for composition cross this boundary. No actor, input,
// resource or UI access occurs here. The profile is copied when the style is selected.
struct PlayerDerivedStatsInput {
    TankPlayerStats base{};
    std::array<int, 7> upgradeLevels{};
    PlayerUpgradeRates upgradeRates{};
    TankRunTuning runTuning{};
    TankCombatStyleProfile combatStyleProfile{};
    tankbuild::Style combatStyle = tankbuild::Style::Shooter;
    bool combatStyleSelected = false;
    bool runEnabled = false;
    float maintenanceMoveScale = 1.0f, maintenanceReloadScale = 1.0f;
    float previousStamina = 3.0f;
};

// Preserve composition order: class base -> upgrades -> run -> maintenance -> bounds.
inline TankPlayerStats CalculatePlayerDerivedStats(const PlayerDerivedStatsInput& input)
{
    TankPlayerStats stats = input.base;
    if (input.runEnabled && input.combatStyleSelected) {
        const auto& profile = input.combatStyleProfile;
        stats.maxHp = profile.maxHp; stats.moveSpeed = profile.moveSpeed;
        stats.maxStamina = profile.maxStamina; stats.staminaRecovery = profile.staminaRecovery;
        stats.bodyDamage = profile.bodyDamage; stats.bulletSpeed = profile.bulletSpeed;
        stats.bulletDamage = profile.attackDamage / (input.combatStyle == tankbuild::Style::Melee ? 3.8f : 1.0f);
        stats.reloadSpeed = profile.attackIntervalSeconds * 60.0f;
    }
    stats.staminaRecovery *= 1.0f + input.upgradeRates.staminaRecovery * static_cast<float>(input.upgradeLevels[0]);
    stats.maxHp *= 1.0f + input.upgradeRates.maxHp * static_cast<float>(input.upgradeLevels[1]);
    stats.bodyDamage *= 1.0f + input.upgradeRates.bodyDamage * static_cast<float>(input.upgradeLevels[2]);
    stats.bulletSpeed *= 1.0f + input.upgradeRates.bulletSpeed * static_cast<float>(input.upgradeLevels[3]);
    stats.bulletDamage *= 1.0f + input.upgradeRates.bulletDamage * static_cast<float>(input.upgradeLevels[4]);
    stats.reloadSpeed *= (std::max)(0.05f, 1.0f - input.upgradeRates.reload * static_cast<float>(input.upgradeLevels[5]));
    if (!input.combatStyleSelected) stats.reloadSpeed = (std::max)(input.upgradeRates.minimumReloadFrames, stats.reloadSpeed);
    stats.moveSpeed *= 1.0f + input.upgradeRates.moveSpeed * static_cast<float>(input.upgradeLevels[6]);
    stats.bulletDamage *= input.runTuning.damage;
    stats.bulletSpeed *= input.runTuning.bulletSpeed;
    stats.reloadSpeed *= input.runTuning.reloadInterval;
    stats.moveSpeed *= input.runTuning.moveSpeed;
    stats.staminaRecovery *= input.runTuning.staminaRecovery;
    stats.maxHp *= input.runTuning.maxHp;
    if (input.runEnabled) {
        stats.moveSpeed *= input.maintenanceMoveScale;
        stats.reloadSpeed *= input.maintenanceReloadScale;
        stats.stamina = input.previousStamina;
    }
    stats.maxHp = (std::max)(1.0f, stats.maxHp);
    stats.reloadSpeed = (std::max)(0.05f, stats.reloadSpeed);
    stats.bulletDamage = (std::max)(0.1f, stats.bulletDamage);
    stats.bulletSpeed = (std::max)(0.01f, stats.bulletSpeed);
    stats.moveSpeed = (std::max)(0.01f, stats.moveSpeed);
    stats.maxStamina = (std::max)(0.0f, stats.maxStamina);
    stats.stamina = (std::min)(stats.stamina, stats.maxStamina);
    return stats;
}

// Recalculation has its legacy full-HP growth rule. Explicit no-heal callers
// keep restoring their previous HP after this rule, just as before extraction.
inline int ResolvePlayerRecalculatedHp(int hp, int oldMaximum, int newMaximum, bool healToFull)
{
    const bool wasFullHp = oldMaximum > 0 && hp >= oldMaximum;
    return healToFull || wasFullHp ? newMaximum : (std::clamp)(hp, 0, newMaximum);
}