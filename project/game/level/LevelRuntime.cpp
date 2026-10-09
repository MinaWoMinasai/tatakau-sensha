#include "game/level/LevelRuntime.h"
#include "game/session/GameplaySystems.h"
#include "game/weapon/CombatTypes.h"
#include "RuntimeProfiler.h"
#include "StartupTrace.h"
#include "GameStartMode.h"
#include "game/ui/TankCombatNeonGeometry.h"
#include "game/effects/TankSpecialNeonGeometry.h"
#include "CollisionConfig.h"
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include "game/session/GameplayHelpers.h"

namespace gameplay {

using namespace detail;

void LevelRuntime::ApplyTankExpeditionRoomGeometry()
{
    // Learn movement and single shots on an open floor before cover and hazards.
    if (world_.run.expeditionRun_ && world_.run.tankExpedition_.GetRoomIndex() == 0) {
        world_.resources.stage_->LoadRunMap("resources/maps/expedition_outskirts.csv");
        world_.presentation.stagePostCacheValid_ = false;
    }
}

bool LevelRuntime::LoadLevelFile(LevelData& outLevel) const
{
    return LevelLoader().Load(world_.run.prototypeRun_ ? "resources/levels/tank_run.json" : "resources/levels/level_test.json", outLevel);
}

void LevelRuntime::ReloadLevelData(bool resetSpawnPositions)
{
    LevelData levelData;
    if (!LoadLevelFile(levelData)) {
        std::cerr << "[Level AI-ditor] Reload failed." << std::endl;
        return;
    }

    ClearAppliedLevelData();

    if (resetSpawnPositions) {
        for (const LevelObject& object : levelData.objects) {
            if (object.type == "PlayerSpawn" && world_.resources.player_) {
                world_.resources.player_->SetWorldPosition(object.transform.translate);
            } else if (object.type == "BossSpawn" && world_.resources.enemy_) {
                world_.resources.enemy_->SetWorldPosition(object.transform.translate);
            }
        }
    }

    ApplyLevelData(levelData);
    world_.presentation.stagePostCacheValid_ = false;
    std::cerr << "[Level AI-ditor] Reloaded: " << levelData.levelName << std::endl;
}

void LevelRuntime::ClearAppliedLevelData()
{
    world_.resources.levelItems_.clear();
    world_.resources.levelBossPhases_.clear();
    if (world_.resources.stage_) {
        world_.resources.stage_->ClearLevelObstacles();
    }
    if (world_.resources.enemyManager_) {
        world_.resources.enemyManager_->ClearLevelData();
    }
    if (world_.resources.enemy_) {
        world_.resources.enemy_->SetBossAttackConfig(Enemy::BossAttackConfig{});
    }
}

void LevelRuntime::ApplyLevelData(const LevelData& levelData)
{
    world_.resources.currentLevelData_ = levelData;
    if (levelData.balance.is_object()) {
        const bool randomSpawnEnabled = ReadCustomBool(levelData.balance, "defaultRandomSpawnEnabled", true);
        world_.resources.enemyManager_->SetDefaultRandomSpawnEnabled(randomSpawnEnabled);
        world_.gameplaySettings->LoadBalanceEditorFromJson(levelData.balance);
        ApplyLevelBalance(levelData.balance);
    }

    for (const LevelObject& object : levelData.objects) {
        world_.gameplayEditor->ApplyLevelObject(object, false);
    }
    if (!world_.gameplayQueries->IsTutorialCombatSuppressed()) {
        for (const LevelSpawnArea& spawnArea : levelData.spawnAreas) {
            world_.gameplayEditor->AddLevelSpawnArea(spawnArea);
        }
    }
    world_.resources.levelBossPhases_.clear();
    world_.resources.levelBossPhases_.reserve(levelData.bossPhases.size());
    for (const LevelBossPhase& phase : levelData.bossPhases) {
        world_.resources.levelBossPhases_.push_back({phase, false});
    }
}

void LevelRuntime::ApplyLevelBalance(const nlohmann::json& balanceJson)
{
    if (!balanceJson.is_object()) {
        return;
    }

    if (world_.resources.player_ && balanceJson.contains("player") && balanceJson["player"].is_object()) {
        const nlohmann::json& playerJson = balanceJson["player"];
        Player::BalanceConfig config{};
        config.maxHp = ReadCustomInt(playerJson, "maxHp", world_.resources.player_->GetMaxHp());
        config.reloadSpeed = ReadCustomFloat(playerJson, "reloadSpeed", config.reloadSpeed);
        config.bulletDamage = ReadCustomFloat(playerJson, "bulletDamage", config.bulletDamage);
        config.bulletSpeed = ReadCustomFloat(playerJson, "bulletSpeed", config.bulletSpeed);
        config.moveSpeed = ReadCustomFloat(playerJson, "moveSpeed", config.moveSpeed);
        config.staminaRecovery = ReadCustomFloat(playerJson, "staminaRecovery", config.staminaRecovery);
        config.maxStamina = ReadCustomFloat(playerJson, "maxStamina", config.maxStamina);
        config.maxHpUpgradeAmount = ReadCustomFloat(playerJson, "maxHpUpgradeAmount", 0.10f);
        config.bodyDamage = static_cast<uint32_t>(
            (std::max)(1, ReadCustomInt(playerJson, "bodyDamage", static_cast<int>(world_.resources.player_->GetDamage()))));
        if (balanceJson.contains("playerUpgrades") && balanceJson["playerUpgrades"].is_object()) {
            const nlohmann::json& upgradeJson = balanceJson["playerUpgrades"];
            config.healthRegenUpgrade = ReadCustomFloat(upgradeJson, "healthRegen", config.healthRegenUpgrade);
            config.maxHpUpgradeAmount = ReadCustomFloat(upgradeJson, "maxHp", config.maxHpUpgradeAmount);
            config.bodyDamageUpgrade = ReadCustomFloat(upgradeJson, "bodyDamage", config.bodyDamageUpgrade);
            config.bulletSpeedUpgrade = ReadCustomFloat(upgradeJson, "bulletSpeed", config.bulletSpeedUpgrade);
            config.bulletDamageUpgrade = ReadCustomFloat(upgradeJson, "bulletDamage", config.bulletDamageUpgrade);
            config.reloadUpgrade = ReadCustomFloat(upgradeJson, "reloadSpeed", config.reloadUpgrade);
            config.moveSpeedUpgrade = ReadCustomFloat(upgradeJson, "moveSpeed", config.moveSpeedUpgrade);
            config.minReloadSpeed = ReadCustomFloat(upgradeJson, "minReloadSpeed", config.minReloadSpeed);
        }
        config.healToFull = ReadCustomBool(playerJson, "healToFull", false);
        world_.resources.player_->ApplyBalanceConfig(config);
    }

    if (balanceJson.contains("damage") && balanceJson["damage"].is_object()) {
        const nlohmann::json& damageJson = balanceJson["damage"];
        if (world_.resources.stage_) {
            world_.resources.stage_->SetDamageBlockDamage(
                static_cast<uint32_t>((std::max)(1, ReadCustomInt(damageJson, "damageBlock", 75))));
        }
        if (world_.resources.enemy_) {
            world_.resources.enemy_->SetDamage(static_cast<uint32_t>(
                (std::max)(1, ReadCustomInt(damageJson, "bossContact", static_cast<int>(world_.resources.enemy_->GetDamage())))));
        }

        ExpEnemy::BalanceConfig expConfig{};
        expConfig.contactDamage = static_cast<uint32_t>((std::max)(1, ReadCustomInt(damageJson, "expEnemyContact", 12)));
        expConfig.shooterContactDamage = static_cast<uint32_t>((std::max)(1, ReadCustomInt(damageJson, "shooterContact", 20)));
        expConfig.shooterBulletDamage = static_cast<uint32_t>((std::max)(1, ReadCustomInt(damageJson, "shooterBullet", 15)));
        expConfig.shooterDetectionRadius = ReadCustomFloat(damageJson, "shooterDetectionRadius", expConfig.shooterDetectionRadius);
        expConfig.shooterTurnSpeed = ReadCustomFloat(damageJson, "shooterTurnSpeed", expConfig.shooterTurnSpeed);
        expConfig.shooterFireInterval = ReadCustomFloat(damageJson, "shooterFireInterval", expConfig.shooterFireInterval);
        expConfig.shooterBulletSpeed = ReadCustomFloat(damageJson, "shooterBulletSpeed", expConfig.shooterBulletSpeed);
        ExpEnemy::SetBalanceConfig(expConfig);
    }

    if (world_.resources.enemy_ && balanceJson.contains("bossAttackDefault") && balanceJson["bossAttackDefault"].is_object()) {
        const nlohmann::json& attackJson = balanceJson["bossAttackDefault"];
        Enemy::BossAttackConfig config = world_.resources.enemy_->GetBossAttackConfig();
        config.bulletSpeed = ReadCustomFloat(attackJson, "bulletSpeed", config.bulletSpeed);
        config.bulletCount = ReadCustomInt(attackJson, "bulletCount", config.bulletCount);
        config.spreadAngleDeg = ReadCustomFloat(attackJson, "spreadAngleDeg", config.spreadAngleDeg);
        config.cooldown = ReadCustomFloat(attackJson, "cooldown", config.cooldown);
        config.damage = static_cast<uint32_t>((std::max)(1, ReadCustomInt(attackJson, "damage", static_cast<int>(config.damage))));
        config.bulletHp = ReadCustomFloat(attackJson, "bulletHp", config.bulletHp);
        config.bulletPenetration = ReadCustomFloat(attackJson, "bulletPenetration", config.bulletPenetration);
        config.randomSpread = ReadCustomBool(attackJson, "randomSpread", config.randomSpread);
        config.pattern = static_cast<Enemy::BossAttackConfig::Pattern>(
            (std::clamp)(ReadCustomInt(attackJson, "pattern", static_cast<int>(config.pattern)), 0, 3));
        world_.resources.enemy_->SetBossAttackConfig(config);
    }

    if (world_.resources.enemyManager_ || world_.resources.enemy_) {
        const nlohmann::json emptyEnemySystem = nlohmann::json::object();
        const nlohmann::json& enemySystemJson =
            (balanceJson.contains("enemySystem") && balanceJson["enemySystem"].is_object()) ? balanceJson["enemySystem"] : emptyEnemySystem;
        const bool hostile = ReadCustomBool(enemySystemJson, "expEnemyHostileToBoss", false);
        if (world_.resources.enemyManager_) {
            world_.resources.enemyManager_->SetExpEnemyHostileToBoss(hostile);
        }
        if (world_.resources.enemy_) {
            Enemy::EnemyProgressConfig config = world_.resources.enemy_->GetEnemyProgressConfig();
            config.expEnemyHostile = hostile;
            config.expEnemyContactDamage = static_cast<uint32_t>(
                (std::max)(1, ReadCustomInt(enemySystemJson, "bossExpEnemyDamage", static_cast<int>(config.expEnemyContactDamage))));
            config.healOnExpEnemyKill = ReadCustomInt(enemySystemJson, "bossHealOnExpEnemyKill", config.healOnExpEnemyKill);
            config.killsPerLevel = ReadCustomInt(enemySystemJson, "bossKillsPerLevel", config.killsPerLevel);
            config.maxHpGainPerLevel = ReadCustomInt(enemySystemJson, "bossMaxHpGainPerLevel", config.maxHpGainPerLevel);
            config.damageGainPerLevel = static_cast<uint32_t>(
                (std::max)(0, ReadCustomInt(enemySystemJson, "bossDamageGainPerLevel", static_cast<int>(config.damageGainPerLevel))));
            config.levelingModeEnabled = ReadCustomBool(enemySystemJson, "bossLevelingModeEnabled", config.levelingModeEnabled);
            config.levelingEnterPlayerDistance =
                ReadCustomFloat(enemySystemJson, "bossLevelingEnterDistance", config.levelingEnterPlayerDistance);
            config.levelingExitPlayerDistance =
                ReadCustomFloat(enemySystemJson, "bossLevelingExitDistance", config.levelingExitPlayerDistance);
            config.levelingSearchRadius = ReadCustomFloat(enemySystemJson, "bossLevelingSearchRadius", config.levelingSearchRadius);
            config.aimTurnHalfSeconds = ReadCustomFloat(enemySystemJson, "bossAimTurnHalfSeconds", config.aimTurnHalfSeconds);
            world_.resources.enemy_->SetEnemyProgressConfig(config);
        }
    }
}

bool LevelRuntime::AddLevelItem(const LevelObject& levelObject)
{
    std::string model;
    if (!TryGetItemModel(levelObject.prefab, model)) {
        std::cerr << "[LevelLoader] Unsupported Item prefab: " << levelObject.prefab << std::endl;
        return false;
    }

    LevelVisualObject visual{};
    visual.name = levelObject.name;
    visual.object = std::make_unique<cg2::Object3d>();
    visual.object->Initialize();
    visual.object->SetModel(model);
    visual.object->SetTransform(levelObject.transform);
    visual.object->SetColor({0.35f, 1.0f, 0.75f, 1.0f});
    visual.object->Update();
    world_.resources.levelItems_.push_back(std::move(visual));
    return true;
}

void LevelRuntime::UpdateLevelItems()
{
    for (LevelVisualObject& item : world_.resources.levelItems_) {
        if (item.object) {
            item.object->Update();
        }
    }
}

void LevelRuntime::DrawLevelItems()
{
    for (LevelVisualObject& item : world_.resources.levelItems_) {
        if (item.object) {
            item.object->Draw();
        }
    }
}
} // namespace gameplay
