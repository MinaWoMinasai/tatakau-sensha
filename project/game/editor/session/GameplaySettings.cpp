#include "game/editor/session/GameplaySettings.h"
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

void GameplaySettings::LoadBalanceEditorFromJson(const nlohmann::json& balanceJson)
{
    if (!balanceJson.is_object()) {
        return;
    }

    world_.resources.balanceEditor_.defaultRandomSpawnEnabled =
        ReadCustomBool(balanceJson, "defaultRandomSpawnEnabled", world_.resources.balanceEditor_.defaultRandomSpawnEnabled);
    if (balanceJson.contains("player") && balanceJson["player"].is_object()) {
        const nlohmann::json& playerJson = balanceJson["player"];
        world_.resources.balanceEditor_.playerMaxHp = ReadCustomInt(playerJson, "maxHp", world_.resources.balanceEditor_.playerMaxHp);
        world_.resources.balanceEditor_.playerReloadSpeed =
            ReadCustomFloat(playerJson, "reloadSpeed", world_.resources.balanceEditor_.playerReloadSpeed);
        world_.resources.balanceEditor_.playerBulletDamage =
            ReadCustomFloat(playerJson, "bulletDamage", world_.resources.balanceEditor_.playerBulletDamage);
        world_.resources.balanceEditor_.playerBulletSpeed =
            ReadCustomFloat(playerJson, "bulletSpeed", world_.resources.balanceEditor_.playerBulletSpeed);
        world_.resources.balanceEditor_.playerMoveSpeed =
            ReadCustomFloat(playerJson, "moveSpeed", world_.resources.balanceEditor_.playerMoveSpeed);
        world_.resources.balanceEditor_.playerStaminaRecovery =
            ReadCustomFloat(playerJson, "staminaRecovery", world_.resources.balanceEditor_.playerStaminaRecovery);
        world_.resources.balanceEditor_.playerMaxStamina =
            ReadCustomFloat(playerJson, "maxStamina", world_.resources.balanceEditor_.playerMaxStamina);
        world_.resources.balanceEditor_.maxHpUpgradeAmount =
            ReadCustomFloat(playerJson, "maxHpUpgradeAmount", world_.resources.balanceEditor_.maxHpUpgradeAmount);
        world_.resources.balanceEditor_.playerBodyDamage =
            ReadCustomInt(playerJson, "bodyDamage", world_.resources.balanceEditor_.playerBodyDamage);
        world_.resources.balanceEditor_.healToFull = ReadCustomBool(playerJson, "healToFull", world_.resources.balanceEditor_.healToFull);
    }
    if (balanceJson.contains("playerUpgrades") && balanceJson["playerUpgrades"].is_object()) {
        const nlohmann::json& upgradeJson = balanceJson["playerUpgrades"];
        world_.resources.balanceEditor_.playerHealthRegenUpgrade =
            ReadCustomFloat(upgradeJson, "healthRegen", world_.resources.balanceEditor_.playerHealthRegenUpgrade);
        world_.resources.balanceEditor_.maxHpUpgradeAmount =
            ReadCustomFloat(upgradeJson, "maxHp", world_.resources.balanceEditor_.maxHpUpgradeAmount);
        world_.resources.balanceEditor_.playerBodyDamageUpgrade =
            ReadCustomFloat(upgradeJson, "bodyDamage", world_.resources.balanceEditor_.playerBodyDamageUpgrade);
        world_.resources.balanceEditor_.playerBulletSpeedUpgrade =
            ReadCustomFloat(upgradeJson, "bulletSpeed", world_.resources.balanceEditor_.playerBulletSpeedUpgrade);
        world_.resources.balanceEditor_.playerBulletDamageUpgrade =
            ReadCustomFloat(upgradeJson, "bulletDamage", world_.resources.balanceEditor_.playerBulletDamageUpgrade);
        world_.resources.balanceEditor_.playerReloadUpgrade =
            ReadCustomFloat(upgradeJson, "reloadSpeed", world_.resources.balanceEditor_.playerReloadUpgrade);
        world_.resources.balanceEditor_.playerMoveSpeedUpgrade =
            ReadCustomFloat(upgradeJson, "moveSpeed", world_.resources.balanceEditor_.playerMoveSpeedUpgrade);
        world_.resources.balanceEditor_.playerMinReloadSpeed =
            ReadCustomFloat(upgradeJson, "minReloadSpeed", world_.resources.balanceEditor_.playerMinReloadSpeed);
    }
    if (balanceJson.contains("damage") && balanceJson["damage"].is_object()) {
        const nlohmann::json& damageJson = balanceJson["damage"];
        world_.resources.balanceEditor_.damageBlock = ReadCustomInt(damageJson, "damageBlock", world_.resources.balanceEditor_.damageBlock);
        world_.resources.balanceEditor_.bossContact = ReadCustomInt(damageJson, "bossContact", world_.resources.balanceEditor_.bossContact);
        world_.resources.balanceEditor_.expEnemyContact =
            ReadCustomInt(damageJson, "expEnemyContact", world_.resources.balanceEditor_.expEnemyContact);
        world_.resources.balanceEditor_.shooterContact =
            ReadCustomInt(damageJson, "shooterContact", world_.resources.balanceEditor_.shooterContact);
        world_.resources.balanceEditor_.shooterBullet =
            ReadCustomInt(damageJson, "shooterBullet", world_.resources.balanceEditor_.shooterBullet);
        world_.resources.balanceEditor_.shooterDetectionRadius =
            ReadCustomFloat(damageJson, "shooterDetectionRadius", world_.resources.balanceEditor_.shooterDetectionRadius);
        world_.resources.balanceEditor_.shooterTurnSpeed =
            ReadCustomFloat(damageJson, "shooterTurnSpeed", world_.resources.balanceEditor_.shooterTurnSpeed);
        world_.resources.balanceEditor_.shooterFireInterval =
            ReadCustomFloat(damageJson, "shooterFireInterval", world_.resources.balanceEditor_.shooterFireInterval);
        world_.resources.balanceEditor_.shooterBulletSpeed =
            ReadCustomFloat(damageJson, "shooterBulletSpeed", world_.resources.balanceEditor_.shooterBulletSpeed);
    }
    if (balanceJson.contains("bossAttackDefault") && balanceJson["bossAttackDefault"].is_object()) {
        const nlohmann::json& attackJson = balanceJson["bossAttackDefault"];
        world_.resources.balanceEditor_.bossBulletSpeed =
            ReadCustomFloat(attackJson, "bulletSpeed", world_.resources.balanceEditor_.bossBulletSpeed);
        world_.resources.balanceEditor_.bossBulletCount =
            ReadCustomInt(attackJson, "bulletCount", world_.resources.balanceEditor_.bossBulletCount);
        world_.resources.balanceEditor_.bossSpreadAngleDeg =
            ReadCustomFloat(attackJson, "spreadAngleDeg", world_.resources.balanceEditor_.bossSpreadAngleDeg);
        world_.resources.balanceEditor_.bossCooldown =
            ReadCustomFloat(attackJson, "cooldown", world_.resources.balanceEditor_.bossCooldown);
        world_.resources.balanceEditor_.bossBulletDamage =
            ReadCustomInt(attackJson, "damage", world_.resources.balanceEditor_.bossBulletDamage);
        world_.resources.balanceEditor_.bossBulletHp =
            ReadCustomFloat(attackJson, "bulletHp", world_.resources.balanceEditor_.bossBulletHp);
        world_.resources.balanceEditor_.bossBulletPenetration =
            ReadCustomFloat(attackJson, "bulletPenetration", world_.resources.balanceEditor_.bossBulletPenetration);
        world_.resources.balanceEditor_.bossRandomSpread =
            ReadCustomBool(attackJson, "randomSpread", world_.resources.balanceEditor_.bossRandomSpread);
        world_.resources.balanceEditor_.bossAttackPattern =
            (std::clamp)(ReadCustomInt(attackJson, "pattern", world_.resources.balanceEditor_.bossAttackPattern), 0, 3);
    }
    if (balanceJson.contains("enemySystem") && balanceJson["enemySystem"].is_object()) {
        const nlohmann::json& enemySystemJson = balanceJson["enemySystem"];
        world_.resources.balanceEditor_.expEnemyHostileToBoss =
            ReadCustomBool(enemySystemJson, "expEnemyHostileToBoss", world_.resources.balanceEditor_.expEnemyHostileToBoss);
        world_.resources.balanceEditor_.bossExpEnemyDamage =
            ReadCustomInt(enemySystemJson, "bossExpEnemyDamage", world_.resources.balanceEditor_.bossExpEnemyDamage);
        world_.resources.balanceEditor_.bossHealOnExpEnemyKill =
            ReadCustomInt(enemySystemJson, "bossHealOnExpEnemyKill", world_.resources.balanceEditor_.bossHealOnExpEnemyKill);
        world_.resources.balanceEditor_.bossKillsPerLevel =
            ReadCustomInt(enemySystemJson, "bossKillsPerLevel", world_.resources.balanceEditor_.bossKillsPerLevel);
        world_.resources.balanceEditor_.bossMaxHpGainPerLevel =
            ReadCustomInt(enemySystemJson, "bossMaxHpGainPerLevel", world_.resources.balanceEditor_.bossMaxHpGainPerLevel);
        world_.resources.balanceEditor_.bossDamageGainPerLevel =
            ReadCustomInt(enemySystemJson, "bossDamageGainPerLevel", world_.resources.balanceEditor_.bossDamageGainPerLevel);
        world_.resources.balanceEditor_.bossLevelingModeEnabled =
            ReadCustomBool(enemySystemJson, "bossLevelingModeEnabled", world_.resources.balanceEditor_.bossLevelingModeEnabled);
        world_.resources.balanceEditor_.bossLevelingEnterDistance =
            ReadCustomFloat(enemySystemJson, "bossLevelingEnterDistance", world_.resources.balanceEditor_.bossLevelingEnterDistance);
        world_.resources.balanceEditor_.bossLevelingExitDistance =
            ReadCustomFloat(enemySystemJson, "bossLevelingExitDistance", world_.resources.balanceEditor_.bossLevelingExitDistance);
        world_.resources.balanceEditor_.bossLevelingSearchRadius =
            ReadCustomFloat(enemySystemJson, "bossLevelingSearchRadius", world_.resources.balanceEditor_.bossLevelingSearchRadius);
        world_.resources.balanceEditor_.bossAimTurnHalfSeconds =
            ReadCustomFloat(enemySystemJson, "bossAimTurnHalfSeconds", world_.resources.balanceEditor_.bossAimTurnHalfSeconds);
    }
    world_.resources.balanceEditor_.initialized = true;
}

nlohmann::json GameplaySettings::BuildBalanceJsonFromEditor() const
{
    nlohmann::json balance = nlohmann::json::object();
    balance["defaultRandomSpawnEnabled"] = world_.resources.balanceEditor_.defaultRandomSpawnEnabled;
    balance["player"] = {{"maxHp", (std::max)(1, world_.resources.balanceEditor_.playerMaxHp)},
                         {"reloadSpeed", (std::max)(0.05f, world_.resources.balanceEditor_.playerReloadSpeed)},
                         {"bulletDamage", (std::max)(0.1f, world_.resources.balanceEditor_.playerBulletDamage)},
                         {"bulletSpeed", (std::max)(0.01f, world_.resources.balanceEditor_.playerBulletSpeed)},
                         {"moveSpeed", (std::max)(0.01f, world_.resources.balanceEditor_.playerMoveSpeed)},
                         {"staminaRecovery", (std::max)(0.0f, world_.resources.balanceEditor_.playerStaminaRecovery)},
                         {"maxStamina", (std::max)(0.0f, world_.resources.balanceEditor_.playerMaxStamina)},
                         {"bodyDamage", (std::max)(1, world_.resources.balanceEditor_.playerBodyDamage)},
                         {"healToFull", world_.resources.balanceEditor_.healToFull}};
    balance["playerUpgrades"] = {{"healthRegen", (std::max)(0.0f, world_.resources.balanceEditor_.playerHealthRegenUpgrade)},
                                 {"maxHp", (std::max)(0.0f, world_.resources.balanceEditor_.maxHpUpgradeAmount)},
                                 {"bodyDamage", (std::max)(0.0f, world_.resources.balanceEditor_.playerBodyDamageUpgrade)},
                                 {"bulletSpeed", world_.resources.balanceEditor_.playerBulletSpeedUpgrade},
                                 {"bulletDamage", (std::max)(0.0f, world_.resources.balanceEditor_.playerBulletDamageUpgrade)},
                                 {"reloadSpeed", (std::max)(0.0f, world_.resources.balanceEditor_.playerReloadUpgrade)},
                                 {"moveSpeed", world_.resources.balanceEditor_.playerMoveSpeedUpgrade},
                                 {"minReloadSpeed", (std::max)(0.05f, world_.resources.balanceEditor_.playerMinReloadSpeed)}};
    balance["damage"] = {{"damageBlock", (std::max)(1, world_.resources.balanceEditor_.damageBlock)},
                         {"bossContact", (std::max)(1, world_.resources.balanceEditor_.bossContact)},
                         {"expEnemyContact", (std::max)(1, world_.resources.balanceEditor_.expEnemyContact)},
                         {"shooterContact", (std::max)(1, world_.resources.balanceEditor_.shooterContact)},
                         {"shooterBullet", (std::max)(1, world_.resources.balanceEditor_.shooterBullet)},
                         {"shooterDetectionRadius", (std::max)(1.0f, world_.resources.balanceEditor_.shooterDetectionRadius)},
                         {"shooterTurnSpeed", (std::max)(0.1f, world_.resources.balanceEditor_.shooterTurnSpeed)},
                         {"shooterFireInterval", (std::max)(0.1f, world_.resources.balanceEditor_.shooterFireInterval)},
                         {"shooterBulletSpeed", (std::max)(0.01f, world_.resources.balanceEditor_.shooterBulletSpeed)}};
    balance["bossAttackDefault"] = {{"bulletSpeed", (std::max)(0.01f, world_.resources.balanceEditor_.bossBulletSpeed)},
                                    {"bulletCount", (std::max)(1, world_.resources.balanceEditor_.bossBulletCount)},
                                    {"spreadAngleDeg", (std::clamp)(world_.resources.balanceEditor_.bossSpreadAngleDeg, 0.0f, 360.0f)},
                                    {"cooldown", (std::max)(0.05f, world_.resources.balanceEditor_.bossCooldown)},
                                    {"damage", (std::max)(1, world_.resources.balanceEditor_.bossBulletDamage)},
                                    {"bulletHp", (std::max)(0.0f, world_.resources.balanceEditor_.bossBulletHp)},
                                    {"bulletPenetration", (std::max)(0.0f, world_.resources.balanceEditor_.bossBulletPenetration)},
                                    {"randomSpread", world_.resources.balanceEditor_.bossRandomSpread},
                                    {"pattern", (std::clamp)(world_.resources.balanceEditor_.bossAttackPattern, 0, 3)}};
    balance["enemySystem"] = {{"expEnemyHostileToBoss", world_.resources.balanceEditor_.expEnemyHostileToBoss},
                              {"bossExpEnemyDamage", (std::max)(1, world_.resources.balanceEditor_.bossExpEnemyDamage)},
                              {"bossHealOnExpEnemyKill", (std::max)(0, world_.resources.balanceEditor_.bossHealOnExpEnemyKill)},
                              {"bossKillsPerLevel", (std::max)(1, world_.resources.balanceEditor_.bossKillsPerLevel)},
                              {"bossMaxHpGainPerLevel", (std::max)(0, world_.resources.balanceEditor_.bossMaxHpGainPerLevel)},
                              {"bossDamageGainPerLevel", (std::max)(0, world_.resources.balanceEditor_.bossDamageGainPerLevel)},
                              {"bossLevelingModeEnabled", world_.resources.balanceEditor_.bossLevelingModeEnabled},
                              {"bossLevelingEnterDistance", (std::max)(1.0f, world_.resources.balanceEditor_.bossLevelingEnterDistance)},
                              {"bossLevelingExitDistance", (std::clamp)(world_.resources.balanceEditor_.bossLevelingExitDistance, 0.5f,
                                                                        world_.resources.balanceEditor_.bossLevelingEnterDistance)},
                              {"bossLevelingSearchRadius", (std::max)(1.0f, world_.resources.balanceEditor_.bossLevelingSearchRadius)},
                              {"bossAimTurnHalfSeconds", (std::max)(0.05f, world_.resources.balanceEditor_.bossAimTurnHalfSeconds)}};
    if (world_.resources.currentLevelData_.balance.is_object() && world_.resources.currentLevelData_.balance.contains("notes") &&
        world_.resources.currentLevelData_.balance["notes"].is_string()) {
        balance["notes"] = world_.resources.currentLevelData_.balance["notes"];
    } else {
        balance["notes"] = "Level AI-ditor Balance Labで調整した値。";
    }
    return balance;
}

bool GameplaySettings::SaveBalanceEditorToLevelFile(const std::string& filePath)
{
    nlohmann::json levelJson = nlohmann::json::object();
    {
        std::ifstream in(filePath);
        if (in.is_open()) {
            try {
                in >> levelJson;
            }
            catch (const std::exception& e) {
                world_.resources.balanceEditor_.statusMessage = std::string("Failed to parse level JSON: ") + e.what();
                return false;
            }
        }
    }
    if (!levelJson.is_object()) {
        levelJson = nlohmann::json::object();
    }

    levelJson["balance"] = BuildBalanceJsonFromEditor();
    std::ofstream out(filePath);
    if (!out.is_open()) {
        world_.resources.balanceEditor_.statusMessage = "Failed to open level JSON for writing.";
        return false;
    }
    out << levelJson.dump(2) << std::endl;
    world_.resources.currentLevelData_.balance = levelJson["balance"];
    world_.resources.balanceEditor_.statusMessage = "Saved balance to " + filePath;
    return true;
}

bool GameplaySettings::WriteBalanceAIHandoff(const std::string& filePath) const
{
    std::ofstream out(filePath);
    if (!out.is_open()) {
        return false;
    }

    out << "# Level AI-ditor Balance Handoff\n\n"
        << "このファイルは、ゲーム内のBalance Labで調整した値をAIに渡すためのメモです。\n\n"
        << "## 依頼例\n\n"
        << "- 現在のプレイ感:\n"
        << "- 困っていること:\n"
        << "- もっと強くしたい要素:\n"
        << "- もっと弱くしたい要素:\n"
        << "- 残したい体験:\n\n"
        << "## 現在のBalance JSON\n\n"
        << "```json\n"
        << BuildBalanceJsonFromEditor().dump(2) << "\n```\n\n"
        << "## AIへのルール\n\n"
        << "- C++の固定値変更ではなく、まず `resources/levels/level_test.json` の `balance` を調整してください。\n"
        << "- プレイヤーHP、DamageBlock、Shooter弾、ボス通常弾は `balance` 内で調整してください。\n"
        << "- ボスHPフェーズ中の弾性能だけは `bossPhases[].customProperties.bossAttack` で調整してください。\n"
        << "- 変更後の `balance` JSONと、なぜその値にしたかを短く説明してください。\n";
    return true;
}

void GameplaySettings::DrawPostEffectParamControls(const char* labelPrefix, cg2::BloomParam& param)
{
#ifdef USE_IMGUI
    ImGui::PushID(labelPrefix);
    const char* bloomModes[] = {"OFF", "Legacy", "Quality", "Light"};
    int bloomMode = static_cast<int>(param.bloomMode);
    if (ImGui::Combo("カテゴリのBloom方式", &bloomMode, bloomModes, 4))
        param.bloomMode = static_cast<uint32_t>(bloomMode);
    ImGui::DragFloat("Legacyブルーム強度", &param.intensity, 0.01f, 0.0f, 8.0f);
    ImGui::DragFloat("QualityブルームGain", &param.bloomGain, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("Quality Soft knee", &param.bloomSoftKnee, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Quality Scatter", &param.bloomScatter, 0.01f, 0.0f, 0.8f);
    ImGui::DragFloat("Quality Radius", &param.bloomRadius, 0.01f, 0.25f, 2.0f);
    ImGui::DragFloat("ブルームしきい値", &param.threshold, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("歪み量", &param.distortionAmount, 0.001f, 0.0f, 0.2f);
    ImGui::DragFloat("色収差", &param.chromAbAmount, 0.001f, 0.0f, 0.2f);
    ImGui::DragFloat("グリッチ", &param.glitchAmount, 0.001f, 0.0f, 0.2f);
    ImGui::DragFloat("ディゾルブ", &param.dissolveThreshold, 0.01f, 0.0f, 1.0f);
    ImGui::ColorEdit3("ディゾルブ境界色", &param.dissolveEdgeColor.x);
    ImGui::DragFloat("ディゾルブ境界幅", &param.dissolveEdgeWidth, 0.001f, 0.0f, 0.25f);
    ImGui::DragFloat("ディゾルブノイズ密度", &param.dissolveNoiseScale, 1.0f, 1.0f, 400.0f);
    ImGui::DragFloat("ディゾルブノイズ速度", &param.dissolveNoiseSpeed, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("アウトライン幅", &param.outlineWidth, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("アウトラインしきい値", &param.outlineThreshold, 0.01f, 0.0f, 1.0f);
    ImGui::ColorEdit3("アウトライン色", &param.outlineColor.x);
    ImGui::DragFloat("アウトライン発光強度", &param.outlineBloomIntensity, 0.01f, 0.0f, 8.0f);
    ImGui::DragFloat("アウトライン発光幅", &param.outlineBloomWidth, 0.1f, 0.0f, 30.0f);
    ImGui::PopID();
#else
    (void)labelPrefix;
    (void)param;
#endif
}

nlohmann::json GameplaySettings::BuildGamePostEffectConfig() const
{
    auto makeEntry = [](bool enabled, const cg2::ObjectPostEffect* effect) {
        nlohmann::json entry = nlohmann::json::object();
        entry["enabled"] = enabled;
        if (effect) {
            entry["param"] = WriteBloomParamJson(effect->GetParam());
        }
        return entry;
    };

    nlohmann::json config = nlohmann::json::object();
    config["version"] = 1;
    config["player"] = makeEntry(world_.combat.enablePlayerPostEffect_, world_.resources.playerPostEffect_.get());
    config["bossEnemy"] = makeEntry(world_.combat.enableEnemyPostEffect_, world_.resources.enemyPostEffect_.get());
    config["expEnemy"] = makeEntry(world_.presentation.enableExpEnemyPostEffect_, world_.resources.expEnemyPostEffect_.get());
    config["stage"] = makeEntry(world_.presentation.enableStagePostEffect_, world_.resources.stagePostEffect_.get());
    config["grid"] = makeEntry(world_.presentation.enableNeonGridPostEffect_, world_.resources.neonGridPostEffect_.get());
    config["bulletTrail"] = makeEntry(world_.presentation.enableBulletTrailPostEffect_, world_.resources.bulletTrailPostEffect_.get());
    config["particles"] = makeEntry(world_.presentation.enableParticlePostEffect_, world_.resources.particlePostEffect_.get());
    config["sharedObjectBloom"] = {{"param", world_.resources.sharedObjectBloomPostEffect_
                                                 ? WriteBloomParamJson(world_.resources.sharedObjectBloomPostEffect_->GetParam())
                                                 : nlohmann::json::object()}};
    config["expEnemyPostCull"] = {{"halfWidth", world_.presentation.expEnemyPostVisibleHalfWidth_},
                                  {"halfHeight", world_.presentation.expEnemyPostVisibleHalfHeight_}};
    config["slowMotionPlayerPost"] = {{"keepPlayerColor", world_.presentation.keepPlayerColorDuringSlow_},
                                      {"chromAbAmount", world_.presentation.slowPlayerChromAbAmount_},
                                      {"distortionAmount", world_.presentation.slowPlayerDistortionAmount_},
                                      {"glitchAmount", world_.presentation.slowPlayerGlitchAmount_}};
    config["deathPulse"] = {{"enabled", world_.presentation.enableDeathPostPulse_},
                            {"duration", world_.presentation.deathPostPulseDuration_},
                            {"bloomBoost", world_.presentation.deathPostBloomBoost_},
                            {"chromAbAmount", world_.presentation.deathPostChromAbAmount_},
                            {"shockwaveStrength", world_.presentation.deathPostShockwaveStrength_},
                            {"shockwaveWidth", world_.presentation.deathPostShockwaveWidth_},
                            {"shockwaveMaxRadius", world_.presentation.deathPostShockwaveMaxRadius_}};
    return config;
}

void GameplaySettings::ApplyGamePostEffectConfig(const nlohmann::json& configJson)
{
    if (!configJson.is_object()) {
        return;
    }

    auto applyEntry = [&](const char* key, bool* enabled, cg2::ObjectPostEffect* effect) {
        if (!configJson.contains(key) || !configJson[key].is_object()) {
            return;
        }
        const nlohmann::json& entry = configJson[key];
        if (enabled) {
            *enabled = ReadCustomBool(entry, "enabled", *enabled);
        }
        if (effect && entry.contains("param")) {
            cg2::BloomParam param = effect->GetParam();
            ReadBloomParamJson(entry["param"], param);
            effect->SetParam(param);
        }
    };

    applyEntry("player", &world_.combat.enablePlayerPostEffect_, world_.resources.playerPostEffect_.get());
    applyEntry("bossEnemy", &world_.combat.enableEnemyPostEffect_, world_.resources.enemyPostEffect_.get());
    applyEntry("expEnemy", &world_.presentation.enableExpEnemyPostEffect_, world_.resources.expEnemyPostEffect_.get());
    applyEntry("stage", &world_.presentation.enableStagePostEffect_, world_.resources.stagePostEffect_.get());
    applyEntry("grid", &world_.presentation.enableNeonGridPostEffect_, world_.resources.neonGridPostEffect_.get());
    applyEntry("bulletTrail", &world_.presentation.enableBulletTrailPostEffect_, world_.resources.bulletTrailPostEffect_.get());
    applyEntry("particles", &world_.presentation.enableParticlePostEffect_, world_.resources.particlePostEffect_.get());
    applyEntry("sharedObjectBloom", nullptr, world_.resources.sharedObjectBloomPostEffect_.get());

    if (configJson.contains("expEnemyPostCull") && configJson["expEnemyPostCull"].is_object()) {
        const nlohmann::json& cullJson = configJson["expEnemyPostCull"];
        world_.presentation.expEnemyPostVisibleHalfWidth_ =
            ReadCustomFloat(cullJson, "halfWidth", world_.presentation.expEnemyPostVisibleHalfWidth_);
        world_.presentation.expEnemyPostVisibleHalfHeight_ =
            ReadCustomFloat(cullJson, "halfHeight", world_.presentation.expEnemyPostVisibleHalfHeight_);
    }
    if (configJson.contains("slowMotionPlayerPost") && configJson["slowMotionPlayerPost"].is_object()) {
        const nlohmann::json& slowJson = configJson["slowMotionPlayerPost"];
        world_.presentation.keepPlayerColorDuringSlow_ =
            ReadCustomBool(slowJson, "keepPlayerColor", world_.presentation.keepPlayerColorDuringSlow_);
        world_.presentation.slowPlayerChromAbAmount_ =
            ReadCustomFloat(slowJson, "chromAbAmount", world_.presentation.slowPlayerChromAbAmount_);
        world_.presentation.slowPlayerDistortionAmount_ =
            ReadCustomFloat(slowJson, "distortionAmount", world_.presentation.slowPlayerDistortionAmount_);
        world_.presentation.slowPlayerGlitchAmount_ =
            ReadCustomFloat(slowJson, "glitchAmount", world_.presentation.slowPlayerGlitchAmount_);
    }
    if (configJson.contains("deathPulse") && configJson["deathPulse"].is_object()) {
        const nlohmann::json& pulseJson = configJson["deathPulse"];
        world_.presentation.enableDeathPostPulse_ = ReadCustomBool(pulseJson, "enabled", world_.presentation.enableDeathPostPulse_);
        world_.presentation.deathPostPulseDuration_ = ReadCustomFloat(pulseJson, "duration", world_.presentation.deathPostPulseDuration_);
        world_.presentation.deathPostBloomBoost_ = ReadCustomFloat(pulseJson, "bloomBoost", world_.presentation.deathPostBloomBoost_);
        world_.presentation.deathPostChromAbAmount_ =
            ReadCustomFloat(pulseJson, "chromAbAmount", world_.presentation.deathPostChromAbAmount_);
        world_.presentation.deathPostShockwaveStrength_ =
            ReadCustomFloat(pulseJson, "shockwaveStrength", world_.presentation.deathPostShockwaveStrength_);
        world_.presentation.deathPostShockwaveWidth_ =
            ReadCustomFloat(pulseJson, "shockwaveWidth", world_.presentation.deathPostShockwaveWidth_);
        world_.presentation.deathPostShockwaveMaxRadius_ =
            ReadCustomFloat(pulseJson, "shockwaveMaxRadius", world_.presentation.deathPostShockwaveMaxRadius_);
    }

    world_.presentation.stagePostCacheValid_ = false;
}

bool GameplaySettings::LoadGamePostEffectConfig(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        world_.presentation.postEffectConfigStatus_ = "ポスト設定ファイルが見つからないため、初期値を使用します。";
        return false;
    }

    nlohmann::json configJson;
    try {
        file >> configJson;
    }
    catch (...) {
        world_.presentation.postEffectConfigStatus_ = "ポスト設定JSONの読み込みに失敗しました。";
        return false;
    }

    ApplyGamePostEffectConfig(configJson);
    world_.presentation.postEffectConfigStatus_ = "ポスト設定を読み込みました: " + filePath;
    return true;
}

bool GameplaySettings::SaveGamePostEffectConfig(const std::string& filePath) const
{
    std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    file << BuildGamePostEffectConfig().dump(2);
    return true;
}

nlohmann::json GameplaySettings::BuildGameVisualConfig() const
{
    nlohmann::json config = nlohmann::json::object();
    config["version"] = 1;
    config["textAppearance"] = {{"fontMode", world_.combat.gameTextFontMode_},
                                {"neonEnabled", world_.combat.gameTextNeonEnabled_},
                                {"outlineEnabled", world_.combat.gameTextOutlineEnabled_},
                                {"outlineColor", WriteJsonVector4(world_.combat.gameTextOutlineColor_)},
                                {"outlineThickness", world_.combat.gameTextOutlineThickness_},
                                {"glowColor", WriteJsonVector4(world_.combat.gameTextNeonStyle_.glowColor)},
                                {"sourceBrightness", world_.combat.gameTextNeonStyle_.sourceBrightness},
                                {"threshold", world_.combat.gameTextNeonStyle_.threshold},
                                {"innerIntensity", world_.combat.gameTextNeonStyle_.innerIntensity},
                                {"outerIntensity", world_.combat.gameTextNeonStyle_.outerIntensity}};
    config["neonGrid"] = {{"showWorldGrid", world_.presentation.showNeonGrid_},
                          {"showActorLocalGrid", world_.presentation.showActorLocalGrid_},
                          {"worldSpacing", world_.presentation.worldGridSpacing_},
                          {"worldLineWidth", world_.presentation.worldGridLineWidth_},
                          {"worldColor", WriteJsonVector4(world_.presentation.worldGridColor_)},
                          {"actorRadius", world_.presentation.actorGridRadius_},
                          {"actorSpacing", world_.presentation.actorGridSpacing_},
                          {"actorLineWidth", world_.presentation.actorGridLineWidth_},
                          {"lineSoftEdgeRatio", world_.presentation.neonLineSoftEdgeRatio_},
                          {"lineCoreIntensity", world_.presentation.neonLineCoreIntensity_},
                          {"particleTriangleGlowWidthScale", world_.presentation.neonParticleTriangleGlowWidthScale_},
                          {"particleTriangleCoreWidthScale", world_.presentation.neonParticleTriangleCoreWidthScale_},
                          {"particleTriangleBrightness", world_.presentation.neonParticleTriangleBrightness_},
                          {"particleTriangleTrailSpacing", world_.presentation.neonParticleTriangleTrailSpacing_},
                          {"particleTriangleBirthScale", world_.presentation.neonParticleTriangleBirthScale_},
                          {"particleTriangleTrailCopies", world_.presentation.neonParticleTriangleTrailCopies_},
                          {"triangleEffectMode", world_.presentation.neonTriangleEffectMode_},
                          {"playerColor", WriteJsonVector4(world_.presentation.playerGridColor_)},
                          {"bossEnemyColor", WriteJsonVector4(world_.presentation.enemyGridColor_)},
                          {"expEnemyColor", WriteJsonVector4(world_.presentation.expEnemyGridColor_)},
                          {"showStageBlockNeonOutlines", world_.presentation.showStageBlockNeonOutlines_},
                          {"showStageNormalBlockBodies", world_.presentation.showStageNormalBlockBodies_},
                          {"stageBlockNeonLineWidth", world_.presentation.stageBlockNeonLineWidth_},
                          {"stageBlockNeonDepthBias", world_.presentation.stageBlockNeonDepthBias_},
                          {"stageBlockNeonColor", WriteJsonVector4(world_.presentation.stageBlockNeonColor_)},
                          {"showStageDamageBlockNeonOutlines", world_.presentation.showStageDamageBlockNeonOutlines_},
                          {"stageDamageBlockNeonColor", WriteJsonVector4(world_.presentation.stageDamageBlockNeonColor_)},
                          {"stageDamageBlockPulseSpeed", world_.presentation.stageDamageBlockPulseSpeed_},
                          {"stageDamageBlockPulseMin", world_.presentation.stageDamageBlockPulseMin_},
                          {"stageDamageBlockPulseMax", world_.presentation.stageDamageBlockPulseMax_},
                          {"cullActorLocalGrid", world_.presentation.cullActorLocalGrid_},
                          {"maxExpEnemyLocalGrids", world_.presentation.maxExpEnemyLocalGrids_},
                          {"expEnemyNeonRenderMode", world_.presentation.expEnemyNeonRenderMode_},
                          {"expEnemyNeonSquareSize", world_.presentation.expEnemyNeonSquareSize_},
                          {"expEnemyNeonTriangleRadius", world_.presentation.expEnemyNeonTriangleRadius_},
                          {"expEnemyNeonPentagonRadius", world_.presentation.expEnemyNeonPentagonRadius_},
                          {"expEnemyNeonShooterRadius", world_.presentation.expEnemyNeonShooterRadius_},
                          {"expEnemyNeonLineWidth", world_.presentation.expEnemyNeonLineWidth_},
                          {"playerNeonRenderMode", world_.presentation.playerNeonRenderMode_},
                          {"bossNeonRenderMode", world_.presentation.bossNeonRenderMode_},
                          {"playerNeonBillboardRadius", world_.presentation.playerNeonBillboardRadius_},
                          {"bossNeonBillboardRadius", world_.presentation.bossNeonBillboardRadius_},
                          {"actorNeonBillboardLineWidth", world_.presentation.actorNeonBillboardLineWidth_},
                          {"playerNeonEmission", world_.presentation.playerNeonEmission_},
                          {"bossNeonEmission", world_.presentation.bossNeonEmission_},
                          {"bossNeonBarrelForwardOffset", world_.presentation.bossNeonBarrelForwardOffset_},
                          {"bossNeonBarrelSideOffset", world_.presentation.bossNeonBarrelSideOffset_},
                          {"bossNeonBarrelLengthScale", world_.presentation.bossNeonBarrelLengthScale_},
                          {"bossNeonBarrelWidthScale", world_.presentation.bossNeonBarrelWidthScale_},
                          {"bossNeonBarrelAngleDeg", world_.presentation.bossNeonBarrelAngleDeg_},
                          {"fillActorNeonBodies", world_.presentation.fillActorNeonBodies_},
                          {"actorNeonBodyFillColor", WriteJsonVector4(world_.presentation.actorNeonBodyFillColor_)},
                          {"playerDashCurrentAlpha", world_.presentation.playerDashCurrentAlpha_},
                          {"playerAfterimageAlpha", world_.presentation.playerAfterimageAlpha_},
                          {"playerAfterimageInterval", world_.presentation.playerAfterimageInterval_},
                          {"playerAfterimageLifetime", world_.presentation.playerAfterimageLifetime_},
                          {"showPlayerIdleMeleeSaber", world_.presentation.showPlayerIdleMeleeSaber_},
                          {"enablePlayerMeleeRibbonTrail", world_.presentation.enablePlayerMeleeRibbonTrail_},
                          {"playerIdleSaberSideOffset", world_.presentation.playerIdleSaberSideOffset_},
                          {"playerIdleSaberForwardOffset", world_.presentation.playerIdleSaberForwardOffset_},
                          {"playerIdleSaberLength", world_.presentation.playerIdleSaberLength_},
                          {"playerIdleSaberAngleDeg", world_.presentation.playerIdleSaberAngleDeg_},
                          {"playerIdleSaberHiltLength", world_.presentation.playerIdleSaberHiltLength_},
                          {"playerIdleSaberBladeWidth", world_.presentation.playerIdleSaberBladeWidth_},
                          {"playerIdleSaberOuterWidthScale", world_.presentation.playerIdleSaberOuterWidthScale_},
                          {"playerIdleSaberCoreWidthScale", world_.presentation.playerIdleSaberCoreWidthScale_},
                          {"playerMeleeBladeOuterWidthScale", world_.presentation.playerMeleeBladeOuterWidthScale_},
                          {"playerMeleeBladeHaloWidthScale", world_.presentation.playerMeleeBladeHaloWidthScale_},
                          {"playerMeleeBladeCoreWidthScale", world_.presentation.playerMeleeBladeCoreWidthScale_},
                          {"playerMeleeTrailWidthScale", world_.presentation.playerMeleeTrailWidthScale_},
                          {"playerMeleeTrailAlphaScale", world_.presentation.playerMeleeTrailAlphaScale_},
                          {"playerMeleeAfterimageAlphaScale", world_.presentation.playerMeleeAfterimageAlphaScale_},
                          {"showTriangleDemo", world_.presentation.showNeonTriangleDemo_},
                          {"triangleCenter", WriteJsonVector3(world_.presentation.neonTriangleDemoCenter_)},
                          {"triangleRadius", world_.presentation.neonTriangleDemoRadius_},
                          {"triangleLineWidth", world_.validation.neonTriangleDemoLineWidth_},
                          {"triangleRotateSpeed", world_.validation.neonTriangleDemoRotateSpeed_},
                          {"triangleColor", WriteJsonVector4(world_.validation.neonTriangleDemoColor_)}};
    config["neonGrid"]["meleeComboVisuals"] = nlohmann::json::array();
    for (const MeleeComboVisualProfile& profile : world_.presentation.playerMeleeComboVisuals_) {
        config["neonGrid"]["meleeComboVisuals"].push_back({{"startAngleDeg", profile.startAngleDeg},
                                                           {"endAngleDeg", profile.endAngleDeg},
                                                           {"durationScale", profile.durationScale},
                                                           {"bladeLengthScale", profile.bladeLengthScale},
                                                           {"bladeWidthScale", profile.bladeWidthScale},
                                                           {"hiltSideOffset", profile.hiltSideOffset},
                                                           {"windupAngleDeg", profile.windupAngleDeg},
                                                           {"returnAngleDeg", profile.returnAngleDeg},
                                                           {"colorScale", WriteJsonVector4(profile.colorScale)}});
    }
    if (world_.resources.bulletManager_) {
        config["bulletTrail"] = WriteBulletTrailSettingsJson(world_.resources.bulletManager_->GetTrailSettings());
    }
    return config;
}

void GameplaySettings::ApplyGameVisualConfig(const nlohmann::json& configJson)
{
    if (!configJson.is_object()) {
        return;
    }
    if (configJson.contains("textAppearance") && configJson["textAppearance"].is_object()) {
        const nlohmann::json& textJson = configJson["textAppearance"];
        world_.combat.gameTextFontMode_ = (std::clamp)(ReadCustomInt(textJson, "fontMode", world_.combat.gameTextFontMode_), 0, 1);
        world_.combat.gameTextNeonEnabled_ = ReadCustomBool(textJson, "neonEnabled", world_.combat.gameTextNeonEnabled_);
        world_.combat.gameTextOutlineEnabled_ = ReadCustomBool(textJson, "outlineEnabled", world_.combat.gameTextOutlineEnabled_);
        if (textJson.contains("outlineColor")) {
            world_.combat.gameTextOutlineColor_ = ReadJsonVector4(textJson["outlineColor"], world_.combat.gameTextOutlineColor_);
        }
        world_.combat.gameTextOutlineThickness_ =
            (std::max)(0.0f, ReadCustomFloat(textJson, "outlineThickness", world_.combat.gameTextOutlineThickness_));
        if (textJson.contains("glowColor")) {
            world_.combat.gameTextNeonStyle_.glowColor = ReadJsonVector4(textJson["glowColor"], world_.combat.gameTextNeonStyle_.glowColor);
        }
        world_.combat.gameTextNeonStyle_.sourceBrightness =
            ReadCustomFloat(textJson, "sourceBrightness", world_.combat.gameTextNeonStyle_.sourceBrightness);
        world_.combat.gameTextNeonStyle_.threshold = ReadCustomFloat(textJson, "threshold", world_.combat.gameTextNeonStyle_.threshold);
        world_.combat.gameTextNeonStyle_.innerIntensity =
            ReadCustomFloat(textJson, "innerIntensity", world_.combat.gameTextNeonStyle_.innerIntensity);
        world_.combat.gameTextNeonStyle_.outerIntensity =
            ReadCustomFloat(textJson, "outerIntensity", world_.combat.gameTextNeonStyle_.outerIntensity);
    }
    world_.gameplayHud->ApplyGameTextAppearance();
    if (configJson.contains("neonGrid") && configJson["neonGrid"].is_object()) {
        const nlohmann::json& gridJson = configJson["neonGrid"];
        world_.presentation.showNeonGrid_ = ReadCustomBool(gridJson, "showWorldGrid", world_.presentation.showNeonGrid_);
        world_.presentation.showActorLocalGrid_ = ReadCustomBool(gridJson, "showActorLocalGrid", world_.presentation.showActorLocalGrid_);
        world_.presentation.worldGridSpacing_ = ReadCustomFloat(gridJson, "worldSpacing", world_.presentation.worldGridSpacing_);
        world_.presentation.worldGridLineWidth_ = ReadCustomFloat(gridJson, "worldLineWidth", world_.presentation.worldGridLineWidth_);
        if (gridJson.contains("worldColor"))
            world_.presentation.worldGridColor_ = ReadJsonVector4(gridJson["worldColor"], world_.presentation.worldGridColor_);
        world_.presentation.actorGridRadius_ = ReadCustomFloat(gridJson, "actorRadius", world_.presentation.actorGridRadius_);
        world_.presentation.actorGridSpacing_ = ReadCustomFloat(gridJson, "actorSpacing", world_.presentation.actorGridSpacing_);
        world_.presentation.actorGridLineWidth_ = ReadCustomFloat(gridJson, "actorLineWidth", world_.presentation.actorGridLineWidth_);
        world_.presentation.neonLineSoftEdgeRatio_ =
            ReadCustomFloat(gridJson, "lineSoftEdgeRatio", world_.presentation.neonLineSoftEdgeRatio_);
        world_.presentation.neonLineCoreIntensity_ =
            ReadCustomFloat(gridJson, "lineCoreIntensity", world_.presentation.neonLineCoreIntensity_);
        world_.presentation.neonParticleTriangleGlowWidthScale_ =
            (std::max)(1.0f, ReadCustomFloat(gridJson, "particleTriangleGlowWidthScale",
                                             world_.presentation.neonParticleTriangleGlowWidthScale_));
        world_.presentation.neonParticleTriangleCoreWidthScale_ =
            (std::max)(0.01f, ReadCustomFloat(gridJson, "particleTriangleCoreWidthScale",
                                              world_.presentation.neonParticleTriangleCoreWidthScale_));
        world_.presentation.neonParticleTriangleBrightness_ =
            (std::max)(0.0f, ReadCustomFloat(gridJson, "particleTriangleBrightness", world_.presentation.neonParticleTriangleBrightness_));
        world_.presentation.neonParticleTriangleTrailSpacing_ =
            (std::max)(0.0f,
                       ReadCustomFloat(gridJson, "particleTriangleTrailSpacing", world_.presentation.neonParticleTriangleTrailSpacing_));
        world_.presentation.neonParticleTriangleBirthScale_ =
            (std::clamp)(ReadCustomFloat(gridJson, "particleTriangleBirthScale", world_.presentation.neonParticleTriangleBirthScale_),
                         0.05f, 1.0f);
        world_.presentation.neonParticleTriangleTrailCopies_ =
            (std::clamp)(ReadCustomInt(gridJson, "particleTriangleTrailCopies", world_.presentation.neonParticleTriangleTrailCopies_), 0,
                         8);
        world_.presentation.neonTriangleEffectMode_ =
            (std::clamp)(ReadCustomInt(gridJson, "triangleEffectMode", world_.presentation.neonTriangleEffectMode_), 0, 2);
        cg2::ParticleManager::GetInstance()->SetNeonTriangleEffectMode(
            static_cast<cg2::ParticleManager::NeonTriangleEffectMode>(world_.presentation.neonTriangleEffectMode_));
        if (gridJson.contains("playerColor"))
            world_.presentation.playerGridColor_ = ReadJsonVector4(gridJson["playerColor"], world_.presentation.playerGridColor_);
        if (gridJson.contains("bossEnemyColor"))
            world_.presentation.enemyGridColor_ = ReadJsonVector4(gridJson["bossEnemyColor"], world_.presentation.enemyGridColor_);
        if (gridJson.contains("expEnemyColor"))
            world_.presentation.expEnemyGridColor_ = ReadJsonVector4(gridJson["expEnemyColor"], world_.presentation.expEnemyGridColor_);
        world_.presentation.showStageBlockNeonOutlines_ =
            ReadCustomBool(gridJson, "showStageBlockNeonOutlines", world_.presentation.showStageBlockNeonOutlines_);
        world_.presentation.showStageNormalBlockBodies_ =
            ReadCustomBool(gridJson, "showStageNormalBlockBodies", world_.presentation.showStageNormalBlockBodies_);
        world_.presentation.stageBlockNeonLineWidth_ =
            ReadCustomFloat(gridJson, "stageBlockNeonLineWidth", world_.presentation.stageBlockNeonLineWidth_);
        world_.presentation.stageBlockNeonDepthBias_ =
            ReadCustomFloat(gridJson, "stageBlockNeonDepthBias", world_.presentation.stageBlockNeonDepthBias_);
        if (gridJson.contains("stageBlockNeonColor"))
            world_.presentation.stageBlockNeonColor_ =
                ReadJsonVector4(gridJson["stageBlockNeonColor"], world_.presentation.stageBlockNeonColor_);
        world_.presentation.showStageDamageBlockNeonOutlines_ =
            ReadCustomBool(gridJson, "showStageDamageBlockNeonOutlines", world_.presentation.showStageDamageBlockNeonOutlines_);
        if (gridJson.contains("stageDamageBlockNeonColor"))
            world_.presentation.stageDamageBlockNeonColor_ =
                ReadJsonVector4(gridJson["stageDamageBlockNeonColor"], world_.presentation.stageDamageBlockNeonColor_);
        world_.presentation.stageDamageBlockPulseSpeed_ =
            ReadCustomFloat(gridJson, "stageDamageBlockPulseSpeed", world_.presentation.stageDamageBlockPulseSpeed_);
        world_.presentation.stageDamageBlockPulseMin_ =
            ReadCustomFloat(gridJson, "stageDamageBlockPulseMin", world_.presentation.stageDamageBlockPulseMin_);
        world_.presentation.stageDamageBlockPulseMax_ =
            ReadCustomFloat(gridJson, "stageDamageBlockPulseMax", world_.presentation.stageDamageBlockPulseMax_);
        if (world_.presentation.stageDamageBlockPulseMin_ > world_.presentation.stageDamageBlockPulseMax_) {
            std::swap(world_.presentation.stageDamageBlockPulseMin_, world_.presentation.stageDamageBlockPulseMax_);
        }
        world_.presentation.cullActorLocalGrid_ = ReadCustomBool(gridJson, "cullActorLocalGrid", world_.presentation.cullActorLocalGrid_);
        world_.presentation.maxExpEnemyLocalGrids_ =
            ReadCustomInt(gridJson, "maxExpEnemyLocalGrids", world_.presentation.maxExpEnemyLocalGrids_);
        world_.presentation.expEnemyNeonRenderMode_ =
            ReadCustomInt(gridJson, "expEnemyNeonRenderMode", world_.presentation.expEnemyNeonRenderMode_);
        if (gridJson.contains("useExpEnemyNeonBillboards") && !gridJson.contains("expEnemyNeonRenderMode")) {
            world_.presentation.expEnemyNeonRenderMode_ = ReadCustomBool(gridJson, "useExpEnemyNeonBillboards", false) ? 1 : 0;
        }
        world_.presentation.expEnemyNeonRenderMode_ = (std::clamp)(world_.presentation.expEnemyNeonRenderMode_, 0, 3);
        world_.presentation.expEnemyNeonSquareSize_ =
            ReadCustomFloat(gridJson, "expEnemyNeonSquareSize", world_.presentation.expEnemyNeonSquareSize_);
        world_.presentation.expEnemyNeonTriangleRadius_ =
            ReadCustomFloat(gridJson, "expEnemyNeonTriangleRadius", world_.presentation.expEnemyNeonTriangleRadius_);
        world_.presentation.expEnemyNeonPentagonRadius_ =
            ReadCustomFloat(gridJson, "expEnemyNeonPentagonRadius", world_.presentation.expEnemyNeonPentagonRadius_);
        world_.presentation.expEnemyNeonShooterRadius_ =
            ReadCustomFloat(gridJson, "expEnemyNeonShooterRadius", world_.presentation.expEnemyNeonShooterRadius_);
        world_.presentation.expEnemyNeonLineWidth_ =
            ReadCustomFloat(gridJson, "expEnemyNeonLineWidth", world_.presentation.expEnemyNeonLineWidth_);
        world_.presentation.playerNeonRenderMode_ =
            (std::clamp)(ReadCustomInt(gridJson, "playerNeonRenderMode", world_.presentation.playerNeonRenderMode_), 0, 1);
        world_.presentation.bossNeonRenderMode_ =
            (std::clamp)(ReadCustomInt(gridJson, "bossNeonRenderMode", world_.presentation.bossNeonRenderMode_), 0, 1);
        world_.presentation.playerNeonBillboardRadius_ =
            ReadCustomFloat(gridJson, "playerNeonBillboardRadius", world_.presentation.playerNeonBillboardRadius_);
        world_.presentation.bossNeonBillboardRadius_ =
            ReadCustomFloat(gridJson, "bossNeonBillboardRadius", world_.presentation.bossNeonBillboardRadius_);
        world_.presentation.actorNeonBillboardLineWidth_ =
            ReadCustomFloat(gridJson, "actorNeonBillboardLineWidth", world_.presentation.actorNeonBillboardLineWidth_);
        world_.presentation.playerNeonEmission_ =
            (std::clamp)(ReadCustomFloat(gridJson, "playerNeonEmission", world_.presentation.playerNeonEmission_), 0.0f, 4.0f);
        world_.presentation.bossNeonEmission_ =
            (std::clamp)(ReadCustomFloat(gridJson, "bossNeonEmission", world_.presentation.bossNeonEmission_), 0.0f, 4.0f);
        world_.presentation.bossNeonBarrelForwardOffset_ =
            ReadCustomFloat(gridJson, "bossNeonBarrelForwardOffset", world_.presentation.bossNeonBarrelForwardOffset_);
        world_.presentation.bossNeonBarrelSideOffset_ =
            ReadCustomFloat(gridJson, "bossNeonBarrelSideOffset", world_.presentation.bossNeonBarrelSideOffset_);
        world_.presentation.bossNeonBarrelLengthScale_ =
            ReadCustomFloat(gridJson, "bossNeonBarrelLengthScale", world_.presentation.bossNeonBarrelLengthScale_);
        world_.presentation.bossNeonBarrelWidthScale_ =
            ReadCustomFloat(gridJson, "bossNeonBarrelWidthScale", world_.presentation.bossNeonBarrelWidthScale_);
        world_.presentation.bossNeonBarrelAngleDeg_ =
            ReadCustomFloat(gridJson, "bossNeonBarrelAngleDeg", world_.presentation.bossNeonBarrelAngleDeg_);
        world_.presentation.fillActorNeonBodies_ =
            ReadCustomBool(gridJson, "fillActorNeonBodies", world_.presentation.fillActorNeonBodies_);
        if (gridJson.contains("actorNeonBodyFillColor"))
            world_.presentation.actorNeonBodyFillColor_ =
                ReadJsonVector4(gridJson["actorNeonBodyFillColor"], world_.presentation.actorNeonBodyFillColor_);
        world_.presentation.playerDashCurrentAlpha_ =
            ReadCustomFloat(gridJson, "playerDashCurrentAlpha", world_.presentation.playerDashCurrentAlpha_);
        world_.presentation.playerAfterimageAlpha_ =
            ReadCustomFloat(gridJson, "playerAfterimageAlpha", world_.presentation.playerAfterimageAlpha_);
        world_.presentation.playerAfterimageInterval_ =
            (std::max)(0.01f, ReadCustomFloat(gridJson, "playerAfterimageInterval", world_.presentation.playerAfterimageInterval_));
        world_.presentation.playerAfterimageLifetime_ =
            (std::max)(0.05f, ReadCustomFloat(gridJson, "playerAfterimageLifetime", world_.presentation.playerAfterimageLifetime_));
        world_.presentation.showPlayerIdleMeleeSaber_ =
            ReadCustomBool(gridJson, "showPlayerIdleMeleeSaber", world_.presentation.showPlayerIdleMeleeSaber_);
        world_.presentation.enablePlayerMeleeRibbonTrail_ =
            ReadCustomBool(gridJson, "enablePlayerMeleeRibbonTrail", world_.presentation.enablePlayerMeleeRibbonTrail_);
        world_.presentation.playerIdleSaberSideOffset_ =
            ReadCustomFloat(gridJson, "playerIdleSaberSideOffset", world_.presentation.playerIdleSaberSideOffset_);
        world_.presentation.playerIdleSaberForwardOffset_ =
            ReadCustomFloat(gridJson, "playerIdleSaberForwardOffset", world_.presentation.playerIdleSaberForwardOffset_);
        world_.presentation.playerIdleSaberLength_ =
            (std::max)(0.05f, ReadCustomFloat(gridJson, "playerIdleSaberLength", world_.presentation.playerIdleSaberLength_));
        world_.presentation.playerIdleSaberAngleDeg_ =
            ReadCustomFloat(gridJson, "playerIdleSaberAngleDeg", world_.presentation.playerIdleSaberAngleDeg_);
        world_.presentation.playerIdleSaberHiltLength_ =
            (std::max)(0.02f, ReadCustomFloat(gridJson, "playerIdleSaberHiltLength", world_.presentation.playerIdleSaberHiltLength_));
        world_.presentation.playerIdleSaberBladeWidth_ =
            (std::max)(0.005f, ReadCustomFloat(gridJson, "playerIdleSaberBladeWidth", world_.presentation.playerIdleSaberBladeWidth_));
        world_.presentation.playerIdleSaberOuterWidthScale_ =
            (std::max)(0.05f,
                       ReadCustomFloat(gridJson, "playerIdleSaberOuterWidthScale", world_.presentation.playerIdleSaberOuterWidthScale_));
        world_.presentation.playerIdleSaberCoreWidthScale_ =
            (std::max)(0.01f,
                       ReadCustomFloat(gridJson, "playerIdleSaberCoreWidthScale", world_.presentation.playerIdleSaberCoreWidthScale_));
        world_.presentation.playerMeleeBladeOuterWidthScale_ =
            (std::max)(0.05f,
                       ReadCustomFloat(gridJson, "playerMeleeBladeOuterWidthScale", world_.presentation.playerMeleeBladeOuterWidthScale_));
        world_.presentation.playerMeleeBladeHaloWidthScale_ =
            (std::max)(0.05f,
                       ReadCustomFloat(gridJson, "playerMeleeBladeHaloWidthScale", world_.presentation.playerMeleeBladeHaloWidthScale_));
        world_.presentation.playerMeleeBladeCoreWidthScale_ =
            (std::max)(0.01f,
                       ReadCustomFloat(gridJson, "playerMeleeBladeCoreWidthScale", world_.presentation.playerMeleeBladeCoreWidthScale_));
        world_.presentation.playerMeleeTrailWidthScale_ =
            (std::max)(0.05f, ReadCustomFloat(gridJson, "playerMeleeTrailWidthScale", world_.presentation.playerMeleeTrailWidthScale_));
        world_.presentation.playerMeleeTrailAlphaScale_ =
            (std::max)(0.0f, ReadCustomFloat(gridJson, "playerMeleeTrailAlphaScale", world_.presentation.playerMeleeTrailAlphaScale_));
        world_.presentation.playerMeleeAfterimageAlphaScale_ =
            (std::max)(0.0f,
                       ReadCustomFloat(gridJson, "playerMeleeAfterimageAlphaScale", world_.presentation.playerMeleeAfterimageAlphaScale_));
        if (gridJson.contains("meleeComboVisuals") && gridJson["meleeComboVisuals"].is_array()) {
            std::vector<MeleeComboVisualProfile> loadedProfiles;
            for (const nlohmann::json& profileJson : gridJson["meleeComboVisuals"]) {
                if (!profileJson.is_object()) {
                    continue;
                }
                MeleeComboVisualProfile profile{};
                profile.startAngleDeg = ReadCustomFloat(profileJson, "startAngleDeg", profile.startAngleDeg);
                profile.endAngleDeg = ReadCustomFloat(profileJson, "endAngleDeg", profile.endAngleDeg);
                profile.durationScale = (std::max)(0.05f, ReadCustomFloat(profileJson, "durationScale", profile.durationScale));
                profile.bladeLengthScale = (std::max)(0.05f, ReadCustomFloat(profileJson, "bladeLengthScale", profile.bladeLengthScale));
                profile.bladeWidthScale = (std::max)(0.05f, ReadCustomFloat(profileJson, "bladeWidthScale", profile.bladeWidthScale));
                profile.hiltSideOffset = ReadCustomFloat(profileJson, "hiltSideOffset", profile.hiltSideOffset);
                profile.windupAngleDeg = ReadCustomFloat(profileJson, "windupAngleDeg", profile.windupAngleDeg);
                profile.returnAngleDeg = ReadCustomFloat(profileJson, "returnAngleDeg", profile.returnAngleDeg);
                if (profileJson.contains("colorScale"))
                    profile.colorScale = ReadJsonVector4(profileJson["colorScale"], profile.colorScale);
                loadedProfiles.push_back(profile);
            }
            if (!loadedProfiles.empty()) {
                world_.presentation.playerMeleeComboVisuals_ = std::move(loadedProfiles);
            }
        }
        world_.presentation.showNeonTriangleDemo_ = ReadCustomBool(gridJson, "showTriangleDemo", world_.presentation.showNeonTriangleDemo_);
        if (gridJson.contains("triangleCenter"))
            world_.presentation.neonTriangleDemoCenter_ =
                ReadJsonVector3(gridJson["triangleCenter"], world_.presentation.neonTriangleDemoCenter_);
        world_.presentation.neonTriangleDemoRadius_ =
            ReadCustomFloat(gridJson, "triangleRadius", world_.presentation.neonTriangleDemoRadius_);
        world_.validation.neonTriangleDemoLineWidth_ =
            ReadCustomFloat(gridJson, "triangleLineWidth", world_.validation.neonTriangleDemoLineWidth_);
        world_.validation.neonTriangleDemoRotateSpeed_ =
            ReadCustomFloat(gridJson, "triangleRotateSpeed", world_.validation.neonTriangleDemoRotateSpeed_);
        if (gridJson.contains("triangleColor"))
            world_.validation.neonTriangleDemoColor_ = ReadJsonVector4(gridJson["triangleColor"], world_.validation.neonTriangleDemoColor_);
    }
    if (world_.resources.bulletManager_ && configJson.contains("bulletTrail")) {
        auto trail = world_.resources.bulletManager_->GetTrailSettings();
        ReadBulletTrailSettingsJson(configJson["bulletTrail"], trail);
        world_.resources.bulletManager_->SetTrailSettings(trail);
    }
}

bool GameplaySettings::LoadGameVisualConfig(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        world_.presentation.visualConfigStatus_ = "見た目設定ファイルが見つからないため、初期値を使用します。";
        return false;
    }

    nlohmann::json configJson;
    try {
        file >> configJson;
    }
    catch (...) {
        world_.presentation.visualConfigStatus_ = "見た目設定JSONの読み込みに失敗しました。";
        return false;
    }

    ApplyGameVisualConfig(configJson);
    world_.presentation.visualConfigStatus_ = "見た目設定を読み込みました: " + filePath;
    return true;
}

bool GameplaySettings::SaveGameVisualConfig(const std::string& filePath) const
{
    std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    file << BuildGameVisualConfig().dump(2);
    return true;
}
} // namespace gameplay
