#include "game/player/progression/PlayerProgression.h"
#include "game/player/combat/PlayerSystems.h"
#include "game/player/ui/PlayerPresentation.h"
#include "game/weapon/CombatTypes.h"
#include "Player.h"
#include "game/player/PlayerMovement.h"
#include "PlayerUiHelpers.h"
#include "game/enemy/visual/NeonDepthPlacement.h"
#include "StartupTrace.h"
#include "Stage.h"
#include "game/exp/ExpEnemy.h"
#include "game/enemy/actor/Enemy.h"
#include "game/exp/EnemyManager.h"
#include "Audio.h"
#include "game/ui/TankButtonUI.h"
#include "game/ui/NeonTextEffect.h"
#include "ObjectPostEffect.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void PlayerProgression::AddExp(int amount)
{
    if (player_.runCurrencyMode_) {
        player_.runCurrencyEarned_ = (std::min)(1000000, player_.runCurrencyEarned_ + tankcontent::CreditsFromExperience(amount));
        return;
    }
    if (player_.level_ >= player_.kMaxLevel)
        return;

    player_.exp_ += amount;
    // レベルアップ判定（余剰分も考慮してループ）
    while (player_.exp_ >= player_.nextLevelExp_) {
        const int previousRank = GetRankFromLevel(player_.level_);
        player_.exp_ -= player_.nextLevelExp_;
        player_.level_++;
        player_.skillPoints_++;

        // 次の必要経験値を再計算（例: レベル * 100 + 補正）
        player_.nextLevelExp_ = GetNextLevelExp();

        if (player_.ui_->arenaUiEnabled_ && GetRankFromLevel(player_.level_) > previousRank &&
            !(player_.runModifiers_.enabled && player_.runCheckpointEvolution_)) {
            player_.isChangeMode = true;
            // AddExpはPlayer::Update後の衝突処理から呼ばれる場合があるため、
            // 同じフレームの初回描画より先に遅延フォント更新を完了させる。
            player_.playerEvolution_->PrepareStaticEvolutionTextTextures();
            player_.playerEvolution_->PrepareEvolutionCircuitTextTextures();
        }

        if (player_.level_ >= player_.kMaxLevel) {
            player_.exp_ = player_.nextLevelExp_; // カンスト表示用
            break;
        }
    }
}

void PlayerProgression::SetRunCurrencyMode(bool enabled)
{
    if (player_.runCurrencyMode_ == enabled)
        return;
    player_.runCurrencyMode_ = enabled;
    player_.runCurrencyEarned_ = 0;
    if (enabled) {
        player_.level_ = 1;
        player_.exp_ = 0;
        player_.skillPoints_ = 0;
        player_.nextLevelExp_ = GetNextLevelExp();
        player_.isChangeMode = false;
    }
}

void PlayerProgression::InstallRunAuthoredClasses(const tankcontent::Catalog& catalog)
{
    std::string error;
    if (!tankcontent::ValidateCatalog(catalog, error))
        return;
    player_.runAuthoredClasses_.clear();
    player_.runAuthoredChoices_.clear();
    player_.runAuthoredStyles_.clear();
    for (const auto& authored : catalog.players) {
        const auto* base = GetClassConfig(authored.baseClass);
        if (!base || base->barrels.empty())
            continue;
        PlayerClassConfig config = *base;
        config.id = authored.id;
        config.displayName = authored.name;
        config.requiredRank = 1;
        config.reloadScale = authored.reloadScale;
        config.bulletDamageScale = authored.damageScale;
        config.bulletSpeedScale = authored.bulletSpeedScale;
        config.bulletCount = 1;
        config.usesDrone = authored.style == tankbuild::Style::Drone;
        config.maxDrones = authored.drones;
        config.reflect = authored.reflect;
        config.penetrate = authored.penetrate;
        config.alternateBarrels = authored.alternate;
        config.fireAllBarrels = !authored.alternate;
        config.randomSpread = false;
        config.spreadAngleDeg = 10;
        config.bodyShape = static_cast<BodyShape>(authored.bodyShape);
        config.bodyOutlineColor = {authored.color[0], authored.color[1], authored.color[2], authored.color[3]};
        config.bodyFillColor = {authored.color[0] * 0.12f, authored.color[1] * 0.12f, authored.color[2] * 0.12f, 0.65f};
        const WeaponMountConfig prototype = base->barrels.front();
        config.barrels.assign(static_cast<std::size_t>(authored.barrels), prototype);
        for (std::size_t i = 0; i < config.barrels.size(); ++i) {
            auto& barrel = config.barrels[i];
            const float position = static_cast<float>(i) - static_cast<float>(config.barrels.size() - 1) * 0.5f;
            barrel.offset = {0.72f, position * 0.50f, 0};
            barrel.angleDeg = position * authored.fanAngle;
            barrel.fires = true;
            barrel.weaponType = WeaponType::Projectile;
            barrel.fireGroup = 0;
            barrel.reloadScale = 1;
            barrel.damageScale = 1;
            barrel.projectileSpeedScale = 1;
        }
        player_.runAuthoredStyles_[authored.id] = authored.style;
        player_.runAuthoredClasses_.emplace(authored.id, std::move(config));
        player_.runAuthoredChoices_.push_back({authored.id, authored.name, authored.description});
    }
    if (player_.runEvolutionActive_ && player_.runAuthoredEvolutionActive_) {
        const auto active = player_.runAuthoredClasses_.find(player_.runEvolutionConfig_.id);
        const auto style = player_.runAuthoredStyles_.find(player_.runEvolutionConfig_.id);
        if (active != player_.runAuthoredClasses_.end() &&
            (!player_.expeditionCombatStyleSelected_ ||
             (style != player_.runAuthoredStyles_.end() && style->second == player_.expeditionCombatStyle_))) {
            player_.runEvolutionConfig_ = active->second;
            player_.InitializeBarrels();
            player_.UpdateBarrelLayout();
            EnsureExpeditionDrones();
            for (auto& drone : player_.drones_)
                player_.ConfigureRunDrone(*drone);
        }
    }
}

std::vector<RunEvolutionChoice> PlayerProgression::GetRunAuthoredEvolutionChoices() const
{
    if (!player_.runModifiers_.enabled || player_.isDead_)
        return {};
    std::vector<RunEvolutionChoice> result;
    for (const auto& choice : player_.runAuthoredChoices_) {
        const auto style = player_.runAuthoredStyles_.find(choice.id);
        if (player_.expeditionCombatStyleSelected_ &&
            (style == player_.runAuthoredStyles_.end() || style->second != player_.expeditionCombatStyle_))
            continue;
        if (!player_.runEvolutionActive_ || choice.id != player_.runEvolutionConfig_.id)
            result.push_back(choice);
    }
    return result;
}

bool PlayerProgression::ChooseRunAuthoredClass(const std::string& id)
{
    if (!player_.runModifiers_.enabled || player_.isDead_)
        return false;
    const auto it = player_.runAuthoredClasses_.find(id);
    if (it == player_.runAuthoredClasses_.end())
        return false;
    const auto style = player_.runAuthoredStyles_.find(id);
    if (player_.expeditionCombatStyleSelected_ &&
        (style == player_.runAuthoredStyles_.end() || style->second != player_.expeditionCombatStyle_))
        return false;
    const int previousHp = player_.hp_;
    player_.runEvolutionConfig_ = it->second;
    player_.runEvolutionActive_ = true;
    player_.runAuthoredEvolutionActive_ = true;
    player_.runEvolutionPrepared_ = false;
    player_.railCharge_.Reset();
    player_.specialMeleeElapsed_ = -1;
    player_.specialParriedBullets_.clear();
    player_.droneLaserLinks_.clear();
    player_.pendingSpecialCombatEvents_.clear();
    player_.bulletCoolTime = 0;
    player_.shootBarrelIndex_ = 0;
    player_.shootGroupIndex_ = 0;
    player_.weaponGroupCooldowns_.clear();
    player_.drones_.clear();
    player_.runSupportDroneTimer_ = 0;
    player_.isChangeMode = false;
    RecalculateStatsFromBase(false);
    player_.hp_ = (std::clamp)(previousHp, 0, player_.GetMaxHp());
    player_.InitializeBarrels();
    player_.UpdateBarrelLayout();
    EnsureExpeditionDrones();
    player_.evolutionConfirmedEvent_ = true;
    return true;
}

void PlayerProgression::ApplyBalanceConfig(const BalanceConfig& config)
{
    const int previousHp = player_.hp_;
    player_.baseStats_.maxHp = static_cast<float>((std::max)(1, config.maxHp));
    player_.baseStats_.reloadSpeed = (std::max)(0.05f, config.reloadSpeed);
    player_.baseStats_.bulletDamage = (std::max)(0.1f, config.bulletDamage);
    player_.baseStats_.bulletSpeed = (std::max)(0.01f, config.bulletSpeed);
    player_.baseStats_.moveSpeed = (std::max)(0.01f, config.moveSpeed);
    player_.baseStats_.staminaRecovery = (std::max)(0.0f, config.staminaRecovery);
    player_.baseStats_.maxStamina = (std::max)(0.0f, config.maxStamina);
    player_.baseStats_.bodyDamage = static_cast<float>((std::max)(1u, config.bodyDamage));

    player_.healthRegenUpgradeRate_ = (std::max)(0.0f, config.healthRegenUpgrade);
    player_.maxHpUpgradeRate_ = (std::max)(0.0f, config.maxHpUpgradeAmount);
    player_.bodyDamageUpgradeRate_ = (std::max)(0.0f, config.bodyDamageUpgrade);
    player_.bulletSpeedUpgradeRate_ = config.bulletSpeedUpgrade;
    player_.bulletDamageUpgradeRate_ = (std::max)(0.0f, config.bulletDamageUpgrade);
    player_.reloadUpgradeRate_ = (std::max)(0.0f, config.reloadUpgrade);
    player_.moveSpeedUpgradeRate_ = config.moveSpeedUpgrade;
    player_.minReloadSpeed_ = (std::max)(0.05f, config.minReloadSpeed);

    player_.runGrowth_ = {player_.maxHpUpgradeRate_, player_.bulletDamageUpgradeRate_, player_.bulletSpeedUpgradeRate_,
                          (std::min)(0.90f, player_.reloadUpgradeRate_), player_.moveSpeedUpgradeRate_};
    RecalculateStatsFromBase(config.healToFull);
    // Editing a running expedition must not grant a free heal when the old
    // maximum was full. Card acquisition keeps its separate growth/heal rule.
    if (player_.runCheckpointEvolution_ && !config.healToFull)
        player_.hp_ = (std::clamp)(previousHp, 0, player_.GetMaxHp());
}

void PlayerProgression::SetRunModifiers(const TankRunModifiers& modifiers)
{
    const bool wasEnabled = player_.runModifiers_.enabled;
    const int previousHp = player_.hp_;
    player_.runModifiers_ = modifiers;
    player_.RefreshAdditiveArmaments();
    if (!modifiers.enabled || !modifiers.railCannon)
        player_.railCharge_.Reset();
    if (!wasEnabled || !player_.runModifiers_.enabled) {
        player_.ResetAdditionalAbilities();
        player_.expeditionCombatStyleSelected_ = false;
        player_.expeditionCombatStyle_ = tankbuild::Style::Shooter;
        player_.runMaintenance_ = {};
        player_.runEvolutionActive_ = false;
        player_.runAuthoredEvolutionActive_ = false;
        player_.runEvolutionPrepared_ = false;
    }
    if (player_.runModifiers_.enabled) {
        player_.ui_->upgradeHudListVisibility_ = 0.0f;
        player_.ui_->upgradeHudMouseCaptured_ = false;
    }
    RecalculateStatsFromBase(false);
    // Refitting edited modules is not healing. A purchased repair grants its
    // explicit recovery in the scene, including before the style is selected.
    if (wasEnabled && player_.runModifiers_.enabled && player_.runModifiers_.expedition)
        player_.hp_ = (std::clamp)(previousHp, 0, player_.GetMaxHp());
    if (wasEnabled && !player_.runModifiers_.enabled) {
        SetRunCurrencyMode(false);
        // A run's support units and attack configuration must not escape its mode.
        player_.drones_.clear();
        player_.runSupportDroneTimer_ = 0.0f;
        player_.runDashAttackTimer_ = 0.0f;
        player_.runOverdriveTimer_ = 0.0f;
        player_.runOverdriveCooldown_ = 0.0f;
        player_.runDashBurstPending_ = false;
        player_.runRoomAwaitInputRelease_ = false;
        player_.runCheckpointEvolution_ = false;
        player_.runHomingTargets_.clear();
        if (player_.object_) {
            player_.InitializeBarrels();
            player_.UpdateBarrelLayout();
        }
    }
}

void PlayerProgression::ApplyCombatStyleBalance(const TankCombatStyleBalances& profiles)
{
    const int hp = player_.hp_;
    const float stamina = player_.stats_.stamina;
    const auto defaults = DefaultTankCombatStyleBalances();
    for (size_t i = 0; i < profiles.size(); ++i)
        player_.combatStyleBalances_[i] = SanitizeTankCombatStyleProfile(profiles[i], defaults[i]);
    RecalculateStatsFromBase(false);
    player_.hp_ = (std::clamp)(hp, 0, player_.GetMaxHp());
    player_.stats_.stamina = (std::clamp)(stamina, 0.0f, player_.stats_.maxStamina);
    EnsureExpeditionDrones();
    for (auto& drone : player_.drones_)
        player_.ConfigureRunDrone(*drone);
}

float PlayerProgression::GetRunBaseReloadFrames() const
{
    return (std::max)(0.05f, player_.runModifiers_.enabled && player_.expeditionCombatStyleSelected_
                                 ? player_.GetCombatStyleProfile(player_.expeditionCombatStyle_).attackIntervalSeconds * 60.0f
                                 : player_.baseStats_.reloadSpeed);
}

bool PlayerProgression::SetExpeditionCombatStyle(tankbuild::Style style)
{
    if (!player_.runModifiers_.enabled || !player_.runModifiers_.expedition || player_.isDead_ || !tankbuild::Valid(style))
        return false;
    if (player_.expeditionCombatStyleSelected_ && player_.expeditionCombatStyle_ == style)
        return true;
    const auto* basic = GetClassConfig("Basic");
    if (!basic || basic->barrels.empty())
        return false;
    // Changing equipment must never rebuild the run or refill health/stamina.
    player_.runStarterConfig_ = *basic;
    player_.runStarterConfig_.displayName = tankbuild::Name(style);
    player_.runStarterConfig_.usesDrone = style == tankbuild::Style::Drone;
    player_.runStarterConfig_.maxDrones = style == tankbuild::Style::Drone ? 3 : 0;
    player_.runStarterConfig_.bulletCount = 1;
    player_.runStarterConfig_.reloadScale = player_.runStarterConfig_.bulletDamageScale = player_.runStarterConfig_.bulletSpeedScale = 1.0f;
    player_.runStarterConfig_.randomSpread = false;
    player_.runStarterConfig_.spreadAngleDeg = 0;
    player_.runStarterConfig_.reflect = player_.runStarterConfig_.penetrate = false;
    player_.runStarterConfig_.barrels.resize(1);
    auto& mount = player_.runStarterConfig_.barrels.front();
    mount.fires = style == tankbuild::Style::Shooter;
    mount.weaponType = WeaponType::Projectile;
    mount.angleDeg = 0;
    mount.offset = {.72f, 0, 0};
    mount.damageScale = mount.reloadScale = mount.projectileSpeedScale = 1.0f;
    const int previousHp = player_.hp_;
    const float previousStamina = player_.stats_.stamina;
    player_.expeditionCombatStyle_ = style;
    player_.expeditionCombatStyleSelected_ = true;
    player_.ResetAdditionalAbilities();
    player_.RefreshAdditiveArmaments();
    player_.railCharge_.Reset();
    player_.specialMeleeElapsed_ = -1;
    player_.specialParriedBullets_.clear();
    player_.droneLaserLinks_.clear();
    player_.pendingSpecialCombatEvents_.clear();
    RecalculateStatsFromBase(false);
    player_.hp_ = (std::clamp)(previousHp, 0, player_.GetMaxHp());
    player_.stats_.stamina = (std::min)(previousStamina, player_.stats_.maxStamina);
    player_.runEvolutionActive_ = player_.runAuthoredEvolutionActive_ = player_.runEvolutionPrepared_ = false;
    player_.bulletCoolTime = 0;
    player_.meleeComboStep_ = 0;
    player_.meleeComboTimer_ = 0;
    player_.shootBarrelIndex_ = player_.shootGroupIndex_ = 0;
    player_.weaponGroupCooldowns_.clear();
    player_.drones_.clear();
    player_.runSupportDroneTimer_ = 0;
    player_.pendingMeleeSlashes_.clear();
    player_.pendingLaserShots_.clear();
    player_.pendingMineDrops_.clear();
    if (player_.object_) {
        player_.InitializeBarrels();
        player_.UpdateBarrelLayout();
        EnsureExpeditionDrones();
    }
    return true;
}

int PlayerProgression::GetExpeditionDroneLimit() const
{
    if (!player_.IsDroneBuild())
        return 0;
    const auto* config = GetCurrentClassConfig();
    return TankCombatStyleDroneCount(player_.GetCombatStyleProfile(tankbuild::Style::Drone).droneCount, config ? config->maxDrones : 3,
                                     player_.runEvolutionActive_,
                                     player_.runModifiers_.drones ? TankEffectCount(player_.runModifiers_, 6, 2) : 0);
}

void PlayerProgression::EnsureExpeditionDrones()
{
    if (!player_.IsDroneBuild() || !player_.object_ || !player_.runBulletManager_ || player_.isDead_)
        return;
    const size_t limit = static_cast<size_t>(GetExpeditionDroneLimit());
    if (player_.drones_.size() > limit)
        player_.drones_.resize(limit);
    while (player_.drones_.size() < limit) {
        auto drone = std::make_unique<PlayerDrone>();
        // Spawn at the owner; normal wall-aware movement forms the escort.
        drone->Initialize(player_.GetWorldPosition(), {});
        drone->SetAttackControllerBulletManager(player_.runBulletManager_);
        player_.ConfigureRunDrone(*drone);
        drone->SetRunInput(player_.runAimWorld_, false);
        player_.drones_.push_back(std::move(drone));
    }
}

void PlayerProgression::ConfigurePrototypeLoadout(int archetype)
{
    if (!player_.runModifiers_.enabled) {
        return;
    }
    player_.runMaintenance_ = {};
    player_.expeditionCombatStyleSelected_ = false;
    player_.expeditionCombatStyle_ = tankbuild::Style::Shooter;
    player_.runEvolutionActive_ = false;
    player_.runAuthoredEvolutionActive_ = false;
    player_.runEvolutionPrepared_ = false;
    RecalculateStatsFromBase(false);
    if (player_.runCheckpointEvolution_) {
        const char* branches[] = {"Twin", "MachineGun", "Overseer"};
        player_.runDashExplosionsEmitted_ = 0;
        player_.runStarterBranch_ = branches[(std::clamp)(archetype, 0, 2)];
        EvolveById("Basic");
        if (const auto* basic = GetClassConfig("Basic"))
            player_.runStarterConfig_ = *basic;
        player_.runStarterConfig_.displayName = "ベーシック";
        player_.runStarterConfig_.usesDrone = false;
        player_.runStarterConfig_.bulletCount = 1;
        player_.runStarterConfig_.reflect = false;
        player_.runStarterConfig_.penetrate = false;
        player_.runStarterConfig_.spreadAngleDeg = 0;
        player_.runStarterConfig_.randomSpread = false;
        player_.runStarterConfig_.reloadScale = 1;
        player_.runStarterConfig_.bulletDamageScale = 1;
        player_.runStarterConfig_.bulletSpeedScale = 1;
        player_.runStarterConfig_.specialActionId = "perfect_dodge";
        player_.runStarterConfig_.alternateBarrels = false;
        player_.runStarterConfig_.fireAllBarrels = false;
        if (!player_.runStarterConfig_.barrels.empty()) {
            player_.runStarterConfig_.barrels.resize(1);
            auto& barrel = player_.runStarterConfig_.barrels.front();
            barrel.fires = true;
            barrel.weaponType = WeaponType::Projectile;
            barrel.angleDeg = 0;
            barrel.offset = {0.72f, 0, 0};
            barrel.reloadScale = 1;
            barrel.damageScale = 1;
            barrel.projectileSpeedScale = 1;
        }
        player_.level_ = 1;
        player_.exp_ = 0;
        player_.skillPoints_ = 0;
        player_.upgradeLevels_.fill(0);
        player_.nextLevelExp_ = GetNextLevelExp();
        player_.drones_.clear();
        RecalculateStatsFromBase(true);
        player_.InitializeBarrels();
        player_.UpdateBarrelLayout();
        return;
    }
    // Starting loadouts are rank two. Their next evolution becomes available
    // at rank three, rather than opening a locked menu at the first rank-up.
    if (player_.level_ < 5) {
        player_.level_ = 5;
        player_.exp_ = 0;
        player_.nextLevelExp_ = GetNextLevelExp();
    }
    const char* classIds[] = {"Twin", "MachineGun", "Overseer"};
    EvolveById(classIds[(std::clamp)(archetype, 0, 2)]);
}

void PlayerProgression::HealRunPlayer(int amount)
{
    if (!player_.runModifiers_.enabled || player_.isDead_ || amount <= 0) {
        return;
    }
    player_.hp_ += (std::min)(amount, (std::max)(0, player_.GetMaxHp() - player_.hp_));
}

bool PlayerProgression::SpendRunHealth(int amount)
{
    if (!player_.runModifiers_.enabled || player_.isDead_ || amount <= 0 || player_.hp_ <= amount)
        return false;
    // A chosen event cost is not an incoming hit and cannot be dodged or lethal.
    player_.hp_ -= amount;
    return true;
}

std::vector<RunEvolutionChoice> PlayerProgression::GetRunEvolutionChoices() const
{
    std::vector<RunEvolutionChoice> choices;
    if (!player_.runModifiers_.enabled || player_.isDead_)
        return choices;
    if (player_.expeditionCombatStyleSelected_ && player_.expeditionCombatStyle_ != tankbuild::Style::Shooter)
        return choices;
    if (player_.runCheckpointEvolution_) {
        if (!player_.runEvolutionPrepared_ || player_.runEvolutionActive_)
            return choices;
        for (const auto& specialization : kTankExpeditionSpecializations) {
            if (player_.expeditionCombatStyleSelected_ ? specialization.drones == 0 : player_.runStarterBranch_ == specialization.starter) {
                choices.push_back({specialization.id, specialization.name, specialization.description});
            }
        }
        return choices;
    }
    const PlayerClassConfig* current = GetCurrentClassConfig();
    if (!current)
        return choices;
    const auto firingMode = [](const PlayerClassConfig& config) {
        if (config.usesDrone)
            return "ドローン上限" + std::to_string(config.maxDrones) + "機";
        const auto guns = std::count_if(config.barrels.begin(), config.barrels.end(), [](const WeaponMountConfig& mount) {
            return mount.fires && mount.weaponType == WeaponType::Projectile;
        });
        return std::to_string(guns) + "砲" + (guns > 1 ? (config.alternateBarrels && !config.fireAllBarrels ? "交互" : "同時") : "射撃");
    };
    const auto scale = [](float value) {
        char text[24]{};
        std::snprintf(text, sizeof(text), "x%.2f", value);
        return std::string(text);
    };
    for (const std::string& id : player_.classCatalog_.OrderedIds()) {
        // The editor's temporary copy has no firing advantage over Twin. Keep it
        // in the original C tree, but omit it from expedition checkpoint rewards.
        if (id == "Triple_Copy" || !CanEvolveTo(id))
            continue;
        const PlayerClassConfig* config = GetClassConfig(id);
        if (!config)
            continue;
        std::string description = "機体の基本性能を比較\n" + firingMode(*current) + "→" + firingMode(*config);
        description += "\n威力 " + scale(current->bulletDamageScale) + "→" + scale(config->bulletDamageScale);
        description += "\n発射間隔 " + scale(current->reloadScale) + "→" + scale(config->reloadScale);
        if (current->reflect != config->reflect) {
            description += std::string("\n壁反射 ") + (current->reflect ? "あり→なし" : "なし→あり");
        } else if (current->bulletCount != config->bulletCount) {
            description += "\n砲ごとの弾数 " + std::to_string(current->bulletCount) + "→" + std::to_string(config->bulletCount);
        } else if (current->bulletSpeedScale != config->bulletSpeedScale) {
            description += "\n弾速 " + scale(current->bulletSpeedScale) + "→" + scale(config->bulletSpeedScale);
        }
        description += "\n主軸・改造は引き継ぎ";
        choices.push_back({config->id, config->displayName, std::move(description)});
        if (choices.size() == 3)
            break;
    }
    return choices;
}

void PlayerProgression::PrepareRunEvolution()
{
    if (!player_.runModifiers_.enabled || player_.isDead_)
        return;
    player_.isChangeMode = false;
    player_.evolutionConfirmedEvent_ = false;
    player_.evolutionCancelledEvent_ = false;
    if (player_.runCheckpointEvolution_) {
        // A checkpoint specialization is independent of the arena's level/tree.
        player_.runEvolutionPrepared_ = !player_.runEvolutionActive_;
        return;
    }
    const PlayerClassConfig* current = GetCurrentClassConfig();
    if (!current || !player_.ui_->evolutionCircuitLoaded_)
        return;
    const int targetRank = current->requiredRank + 1;
    if (targetRank > GetRankFromLevel(player_.kMaxLevel))
        return;
    const bool hasSuccessor =
        std::any_of(player_.classCatalog_.OrderedIds().begin(), player_.classCatalog_.OrderedIds().end(), [&](const std::string& id) {
            if (id == "Triple_Copy")
                return false; // Match the checkpoint candidate filter above.
            const PlayerClassConfig* config = GetClassConfig(id);
            return config && config->requiredRank == targetRank && HasEvolutionEdge(current->id, id) && IsRunCompatibleClass(*config);
        });
    if (!hasSuccessor)
        return;
    // Grant only the checkpoint's required rank. HP, partial XP, and the build
    // remain untouched; normal level-up rewards and the legacy C popup do not run.
    while (player_.level_ < player_.kMaxLevel && GetRankFromLevel(player_.level_) < targetRank)
        ++player_.level_;
    player_.nextLevelExp_ = GetNextLevelExp();
}

bool PlayerProgression::ChooseRunEvolution(const std::string& id)
{
    if (!player_.runModifiers_.enabled || player_.isDead_)
        return false;
    const auto choices = GetRunEvolutionChoices();
    if (std::none_of(choices.begin(), choices.end(), [&](const RunEvolutionChoice& choice) {
            return choice.id == id;
        })) {
        return false;
    }
    if (player_.runCheckpointEvolution_) {
        const auto* specialization = FindTankExpeditionSpecialization(id);
        const auto* starter =
            GetClassConfig(player_.expeditionCombatStyleSelected_ && specialization ? specialization->starter : player_.runStarterBranch_);
        if (!specialization || !starter || starter->barrels.empty())
            return false;
        player_.runEvolutionConfig_ = *starter;
        auto& config = player_.runEvolutionConfig_;
        config.id = specialization->id;
        config.displayName = specialization->name;
        config.requiredRank = 3;
        config.usesDrone = specialization->drones > 0;
        config.maxDrones = specialization->drones;
        config.reloadScale *= specialization->reloadScale;
        config.bulletDamageScale *= specialization->damageScale;
        config.bulletSpeedScale *= specialization->speedScale;
        config.bulletCount = 1;
        config.spreadAngleDeg = specialization->randomSpread;
        config.randomSpread = specialization->randomSpread > 0.0f;
        config.reflect = starter->reflect || specialization->reflect;
        config.alternateBarrels = specialization->alternate;
        config.fireAllBarrels = !specialization->alternate;
        const WeaponMountConfig prototype = starter->barrels.front();
        config.barrels.assign(static_cast<size_t>(specialization->barrels), prototype);
        for (size_t i = 0; i < config.barrels.size(); ++i) {
            auto& barrel = config.barrels[i];
            const float position = static_cast<float>(i) - static_cast<float>(config.barrels.size() - 1) * 0.5f;
            barrel.offset = {0.72f, position * 0.50f, 0.0f};
            barrel.angleDeg = position * specialization->fanAngle;
            barrel.fires = true;
            barrel.weaponType = WeaponType::Projectile;
            barrel.fireGroup = 0;
            barrel.reloadScale = 1.0f;
            barrel.damageScale = 1.0f;
            barrel.projectileSpeedScale = 1.0f;
            if (specialization->speedScale > 1.2f)
                barrel.scale.x *= 1.25f;
        }
        player_.runEvolutionActive_ = true;
        player_.runAuthoredEvolutionActive_ = false;
        player_.runEvolutionPrepared_ = false;
        player_.bulletCoolTime = 0.0f;
        player_.shootBarrelIndex_ = 0;
        player_.shootGroupIndex_ = 0;
        player_.weaponGroupCooldowns_.clear();
        player_.drones_.clear();
        player_.runSupportDroneTimer_ = 0.0f;
        player_.InitializeBarrels();
        player_.UpdateBarrelLayout();
        player_.isChangeMode = false;
        player_.evolutionConfirmedEvent_ = true;
        return true;
    }
    return TryConfirmEvolutionById(id);
}

bool PlayerProgression::AwardRunMaintenancePoint(int clearedRoom)
{
    return player_.runModifiers_.enabled && player_.runCheckpointEvolution_ && !player_.isDead_ &&
           player_.runMaintenance_.AwardRoom(clearedRoom);
}

std::array<RunMaintenanceChoice, 3> PlayerProgression::GetRunMaintenanceChoices() const
{
    std::array<RunMaintenanceChoice, 3> choices{
        {{0, "機動整備", "1段階ごとに移動速度 +12%\n敵の射線を外しやすくする"},
         {1, "装填整備", "1段階ごとに発射間隔 -14%\n主砲とドローンの両方に有効"},
         {2, "装甲整備", "1段階ごとに被ダメージ -8%\n最大24%・最低1ダメージ\nイベントのHP支払いは対象外"}}};
    for (auto& choice : choices) {
        choice.rank = player_.runMaintenance_.Rank(choice.id);
        choice.canSpend = player_.runModifiers_.enabled && player_.runCheckpointEvolution_ && !player_.isDead_ &&
                          player_.runMaintenance_.Points() > 0 && choice.rank < choice.maxRank;
    }
    return choices;
}

bool PlayerProgression::SpendRunMaintenancePoint(int stat)
{
    if (!player_.runModifiers_.enabled || !player_.runCheckpointEvolution_ || player_.isDead_ || !player_.runMaintenance_.Spend(stat))
        return false;
    const int previousHp = player_.hp_;
    RecalculateStatsFromBase(false);
    // Maintenance changes never heal, including while the player is at full HP.
    player_.hp_ = (std::min)(previousHp, player_.GetMaxHp());
    return true;
}

bool PlayerProgression::RefundRunMaintenancePoint(int stat)
{
    if (!player_.runModifiers_.enabled || !player_.runCheckpointEvolution_ || player_.isDead_ || !player_.runMaintenance_.Refund(stat))
        return false;
    const int previousHp = player_.hp_;
    RecalculateStatsFromBase(false);
    player_.hp_ = (std::min)(previousHp, player_.GetMaxHp());
    return true;
}

void PlayerProgression::ResetRunRoomState(const cg2::Vector3& position)
{
    if (!player_.runModifiers_.enabled || player_.isDead_)
        return;
    player_.ResetAdditionalAbilities();
    player_.railCharge_.Reset();
    player_.linkDamageClock_ = {};
    player_.droneLaserLinks_.clear();
    player_.pendingSpecialCombatEvents_.clear();
    player_.specialMeleeElapsed_ = -1;
    player_.specialParriedBullets_.clear();
    player_.velocity_ = {};
    player_.move_ = {};
    player_.inputDir_ = {};
    player_.bulletCoolTime = 0.0f;
    player_.shootBarrelIndex_ = 0;
    player_.shootGroupIndex_ = 0;
    player_.weaponGroupCooldowns_.clear();
    player_.pendingLaserShots_.clear();
    player_.pendingMineDrops_.clear();
    player_.pendingMeleeSlashes_.clear();
    player_.pendingDashImpacts_.clear();
    player_.dashImpactTargets_.clear();
    player_.drones_.clear();
    player_.runHomingTargets_.clear();
    player_.runSupportDroneTimer_ = 0.0f;
    player_.runDashAttackTimer_ = 0.0f;
    player_.runOverdriveTimer_ = 0.0f;
    player_.runOverdriveCooldown_ = 0.0f;
    player_.runDashBurstPending_ = false;
    player_.runRoomAwaitInputRelease_ = true;
    player_.isDashing_ = false;
    player_.dashTimer_ = 0.0f;
    player_.dashCooldown_ = 0.0f;
    player_.isJustEvaded_ = false;
    player_.isBuffActive_ = false;
    player_.buffTimer_ = 0.0f;
    player_.requestSlow_ = false;
    player_.isSmash_ = false;
    player_.smashCharge_ = 0.0f;
    player_.smashDir_ = {};
    player_.meleeComboStep_ = 0;
    player_.meleeComboTimer_ = 0.0f;
    player_.saberCounterTimer_ = 0.0f;
    player_.isStealth_ = false;
    player_.stealthTimer_ = 0.0f;
    player_.stealthAlpha_ = 1.0f;
    player_.summonTimer_ = 0.0f;
    player_.stats_.stamina = player_.stats_.maxStamina;
    player_.invincibleTimer_ = 0.45f;
    player_.damageFeedbackTimer_ = 0.0f;
    player_.movementParticleTimer_ = 0.0f;
    player_.primaryAttackPerformedEvent_ = false;
    player_.dashStartedEvent_ = false;
    player_.statUpgradePerformedEvent_ = false;
    player_.evolutionConfirmedEvent_ = false;
    player_.evolutionCancelledEvent_ = false;
    player_.isChangeMode = false;
    player_.ui_->upgradeHudMouseCaptured_ = false;
    for (auto& barrel : player_.barrels_) {
        barrel.recoilOffset = 0.0f;
        barrel.muzzleFlashTimer = 0.0f;
    }
    player_.SetWorldPosition(position);
    player_.UpdateBarrelLayout();
    EnsureExpeditionDrones();
}

Player::RunCombatSnapshot PlayerProgression::GetRunCombatSnapshot() const
{
    RunCombatSnapshot result{};
    const auto* config = GetCurrentClassConfig();
    if (!config)
        return result;
    result.style = player_.expeditionCombatStyle_;
    result.hasStyle = player_.expeditionCombatStyleSelected_;
    result.melee = player_.IsMeleeBuild();
    result.droneLimit = GetExpeditionDroneLimit();
    result.classId = config->id;
    result.classDamageScale = config->bulletDamageScale;
    result.classReloadScale = config->reloadScale;
    result.classBulletSpeedScale = config->bulletSpeedScale;
    result.classReflects = config->reflect;
    result.classPenetrates = config->penetrate;
    result.isAuthored = player_.runEvolutionActive_ && player_.runAuthoredEvolutionActive_;
    result.classDroneCount = config->usesDrone || result.isAuthored ? (std::max)(0, config->maxDrones) : 0;
    result.baseDroneCount = player_.GetCombatStyleProfile(tankbuild::Style::Drone).droneCount;
    result.barrels = player_.IsDroneBuild() || player_.IsMeleeBuild() ? 0 : static_cast<int>(config->barrels.size());
    result.activeDrones = static_cast<int>(player_.drones_.size());
    AttackParam attack{};
    attack.bulletCount = config->bulletCount;
    attack.reflect = config->reflect;
    player_.ApplyRunProjectileRules(attack);
    result.projectilesPerBarrel = attack.bulletCount;
    result.maxWallBounces = attack.maxWallBounces;
    result.actorPierceCount = attack.actorPierceCount;
    result.impactSplitCount = attack.impactSplitCount;
    result.reflects = attack.reflect || player_.isBuffActive_;
    result.homing = player_.runModifiers_.enabled && player_.runModifiers_.homing;
    result.dashBurst = player_.runModifiers_.enabled && player_.runModifiers_.dashBurst;
    const auto synergy = MakeTankRunSynergy(player_.runModifiers_, player_.runOverdriveTimer_ > 0.0f);
    result.dashExplosion = synergy.dashExplosion;
    result.homingTurnRate = synergy.homingTurnRate;
    result.dashExplosionsEmitted = player_.runDashExplosionsEmitted_;
    result.shotDamage = (std::max)(1.0f, std::round(player_.stats_.bulletDamage * config->bulletDamageScale));
    if (!player_.IsDroneBuild() && !player_.IsMeleeBuild() && !config->barrels.empty())
        result.shotDamage = (std::max)(1.0f, std::round(result.shotDamage * config->barrels.front().damageScale));
    result.reloadSeconds = player_.stats_.reloadSpeed / 60.0f * config->reloadScale * player_.GetRunFireIntervalScale() *
                           (player_.isBuffActive_ ? 0.7f : 1.0f);
    if (player_.IsDroneBuild()) {
        const auto tuning = MakeTankDroneTuning(player_.runModifiers_, true);
        result.shotDamage = (std::max)(1.0f, player_.stats_.bulletDamage * config->bulletDamageScale * tuning.damageScale *
                                                 (player_.runModifiers_.autonomousSpread
                                                      ? (std::max)(.2f, 1.0f - .18f * TankEffectPower(player_.runModifiers_, 38))
                                                      : 1.0f));
        result.reloadSeconds =
            (std::max)(.05f, player_.GetCombatStyleProfile(tankbuild::Style::Drone).attackIntervalSeconds * tuning.reloadSeconds / .5f *
                                 player_.stats_.reloadSpeed / GetRunBaseReloadFrames() * config->reloadScale *
                                 player_.GetRunFireIntervalScale() * (player_.isBuffActive_ ? .7f : 1.0f) *
                                 (player_.runModifiers_.core == TankRunCore::Drone ? .8f : 1.0f) *
                                 (player_.empJammerTimer_ > 0 ? 1.5f : 1.0f));
    } else if (player_.IsMeleeBuild()) {
        const auto combo = MakeTankMeleeCombo(0, player_.runModifiers_);
        result.shotDamage = (std::max)(1.0f, std::round(player_.stats_.bulletDamage * config->bulletDamageScale * 3.8f * combo.damage));
        result.reloadSeconds =
            (std::max)(.05f, (combo.windup + combo.duration + combo.recovery) * player_.stats_.reloadSpeed / GetRunBaseReloadFrames() *
                                 config->reloadScale * player_.GetRunFireIntervalScale() *
                                 (player_.expeditionCombatStyleSelected_
                                      ? player_.GetCombatStyleProfile(tankbuild::Style::Melee).attackIntervalSeconds / .33f
                                      : 1.0f));
    }
    return result;
}

int PlayerProgression::GetNextLevelExp() const
{
    // 簡易的な計算式（必要に応じて調整）
    return player_.level_ * 10 + 20;
}

void PlayerProgression::Evolve(ClassType newClass)
{
    EvolveById(ClassTypeToString(newClass));
}

void PlayerProgression::EvolveById(const std::string& classId)
{
    const PlayerClassConfig* config = GetClassConfig(classId);
    if (!config || !IsRunCompatibleClass(*config)) {
        return;
    }

    player_.currentClassId_ = config->id;
    player_.currentClass_ = config->type;
    player_.bulletCoolTime = 0.0f;
    player_.shootBarrelIndex_ = 0;
    player_.shootGroupIndex_ = 0;
    player_.weaponGroupCooldowns_.clear();
    if (player_.runModifiers_.enabled) {
        player_.drones_.clear();
        player_.runSupportDroneTimer_ = 0.0f;
    }

    // 進化時に特殊状態をリセットする
    player_.isSmash_ = false;
    player_.smashCharge_ = 0.0f;
    player_.isStealth_ = false;

    // 旧機体の切替用に残っている互換分岐。現在の砲塔・外観の反映は下の再構築で行う。
    switch (player_.currentClass_) {
    case ClassType::Twin:

        break;
    }
    player_.InitializeBarrels();
    player_.UpdateBarrelLayout();

    player_.isChangeMode = false;
}

bool PlayerProgression::IsRunCompatibleClass(const PlayerClassConfig& config) const
{
    if (!player_.runModifiers_.enabled)
        return true;
    // Smasher bypasses the configured guns and always performs a melee attack.
    if (config.type == ClassType::Smasher)
        return false;
    if (config.usesDrone)
        return true;
    return std::any_of(config.barrels.begin(), config.barrels.end(), [](const WeaponMountConfig& mount) {
        return mount.fires && mount.weaponType == WeaponType::Projectile;
    });
}

bool PlayerProgression::IsEvolutionClassVisible(const std::string& classId) const
{
    // Keep the current node visible if an editor changes its weapon configuration.
    if (!player_.runModifiers_.enabled || classId == player_.currentClassId_)
        return true;
    const PlayerClassConfig* config = GetClassConfig(classId);
    return config && IsRunCompatibleClass(*config);
}

bool PlayerProgression::CanEvolveTo(const std::string& classId) const
{
    const PlayerClassConfig* currentConfig = GetCurrentClassConfig();
    const PlayerClassConfig* targetConfig = GetClassConfig(classId);
    if (!currentConfig || !targetConfig || targetConfig->id == currentConfig->id || !IsRunCompatibleClass(*targetConfig)) {
        return false;
    }
    if (!player_.ui_->evolutionCircuitLoaded_ || !HasEvolutionEdge(currentConfig->id, targetConfig->id) ||
        targetConfig->requiredRank != currentConfig->requiredRank + 1) {
        return false;
    }
    return GetRankFromLevel(player_.level_) >= targetConfig->requiredRank;
}

bool PlayerProgression::HasEvolutionEdge(const std::string& from, const std::string& to) const
{
    return std::any_of(player_.ui_->evolutionCircuitEdges_.begin(), player_.ui_->evolutionCircuitEdges_.end(),
                       [&](const PlayerUiState::EvolutionCircuitEdgeDefinition& edge) {
                           return edge.from == from && edge.to == to;
                       });
}

bool PlayerProgression::TryConfirmEvolutionById(const std::string& classId)
{
    if (!CanEvolveTo(classId)) {
        return false;
    }
    const std::string previousClassId = player_.currentClassId_;
    EvolveById(classId);
    if (player_.ui_->evolutionHistory_.empty() || player_.ui_->evolutionHistory_.back() != previousClassId) {
        player_.ui_->evolutionHistory_.clear();
        player_.ui_->evolutionHistory_.push_back(previousClassId);
    }
    player_.ui_->evolutionHistory_.push_back(classId);
    player_.evolutionConfirmedEvent_ = true;
    return true;
}

bool PlayerProgression::LoadPlayerClassConfigs(const std::string& path)
{
    PlayerClassCatalog loadedCatalog;
    if (!loadedCatalog.Load(path))
        return false;
    if (!loadedCatalog.Find(player_.currentClassId_)) {
        std::cerr << "[PlayerClass] Reload failed: current class is missing: " << player_.currentClassId_ << std::endl;
        return false;
    }
    // 現在の機体が残ることを確認してから確定する。失敗時はHP・装備・設定をそのまま保つ。
    player_.classCatalog_.Swap(loadedCatalog);
    return true;
}

bool PlayerProgression::ReloadPlayerClassConfigs(const std::string& path)
{
    const std::string activeClassId = player_.currentClassId_;
    if (!LoadPlayerClassConfigs(path)) {
        return false;
    }

    const PlayerClassConfig* config = GetClassConfig(activeClassId);
    if (!config) {
        std::cerr << "[PlayerClass] Reload failed: current class is unavailable." << std::endl;
        return false;
    }
    player_.currentClassId_ = activeClassId;
    player_.currentClass_ = config->type;
    player_.shootBarrelIndex_ = 0;
    player_.shootGroupIndex_ = 0;
    player_.weaponGroupCooldowns_.clear();
    player_.InitializeBarrels();
    player_.UpdateBarrelLayout();
    std::cerr << "[PlayerClass] Reload succeeded. Current class: " << player_.currentClassId_ << std::endl;
    return true;
}

Player::PlayerClassConfig PlayerProgression::CreateDefaultClassConfig(ClassType type) const
{
    return PlayerClassCatalog::CreateDefaultConfig(type);
}

void PlayerProgression::SavePlayerClassConfigs(const std::string& path) const
{
    player_.classCatalog_.Save(path);
}

const Player::PlayerClassConfig* PlayerProgression::GetClassConfig(ClassType type) const
{
    return player_.classCatalog_.Find(type);
}

const Player::PlayerClassConfig* PlayerProgression::GetClassConfig(const std::string& classId) const
{
    return player_.classCatalog_.Find(classId);
}

const Player::PlayerClassConfig* PlayerProgression::GetCurrentClassConfig() const
{
    if (player_.runModifiers_.enabled && player_.runEvolutionActive_)
        return &player_.runEvolutionConfig_;
    if (player_.runModifiers_.enabled && player_.expeditionCombatStyleSelected_)
        return &player_.runStarterConfig_;
    if (player_.runModifiers_.enabled && player_.runCheckpointEvolution_ && !player_.runStarterConfig_.barrels.empty())
        return &player_.runStarterConfig_;
    return GetClassConfig(player_.currentClassId_);
}

Player::PlayerClassConfig* PlayerProgression::GetMutableClassConfig(const std::string& classId)
{
    return player_.classCatalog_.FindMutable(classId);
}

int PlayerProgression::GetRankFromLevel(int level) const
{
    if (level >= 15)
        return 4;
    if (level >= 10)
        return 3;
    if (level >= 5)
        return 2;
    return 1;
}

int PlayerProgression::GetUpgradeLevel(int index) const
{
    if (index < 0 || index >= static_cast<int>(player_.upgradeLevels_.size())) {
        return 0;
    }
    return player_.upgradeLevels_[index];
}

const char* PlayerProgression::GetCurrentClassName() const
{
    if (const PlayerClassConfig* config = GetCurrentClassConfig()) {
        return config->displayName.c_str();
    }
    switch (player_.currentClass_) {
    case ClassType::Basic:
        return "Basic";
    case ClassType::Twin:
        return "Twin";
    case ClassType::MachineGun:
        return "MachineGun";
    case ClassType::Overseer:
        return "Overseer";
    case ClassType::Triple:
        return "Triple";
    case ClassType::Assassin:
        return "Assassin";
    case ClassType::Bounder:
        return "Bounder";
    case ClassType::Ninja:
        return "Ninja";
    case ClassType::Smasher:
        return "Smasher";
    case ClassType::Summoner:
        return "Summoner";
    }
    return "Unknown";
}

bool PlayerProgression::ApplyStatUpgrade(int index)
{
    if (player_.runModifiers_.enabled) {
        return false;
    }
    if (player_.skillPoints_ <= 0 || index < 0 || index >= static_cast<int>(player_.upgradeLevels_.size())) {
        return false;
    }
    if (player_.upgradeLevels_[index] >= player_.maxEnhancePoint) {
        return false;
    }

    player_.upgradeLevels_[index]++;
    player_.skillPoints_--;

    switch (index) {
    case 0:
        break;
    case 1:
        break;
    case 2:
        break;
    case 3:
        break;
    case 4:
        break;
    case 5:
        break;
    case 6:
        break;
    }

    RecalculateStatsFromBase(false);
    player_.statUpgradePerformedEvent_ = true;
    return true;
}

bool PlayerProgression::RefundStatUpgrade(int index)
{
    if (player_.runModifiers_.enabled) {
        return false;
    }
    if (index < 0 || index >= static_cast<int>(player_.upgradeLevels_.size())) {
        return false;
    }
    if (player_.upgradeLevels_[index] <= 0) {
        return false;
    }

    player_.upgradeLevels_[index]--;
    player_.skillPoints_++;
    RecalculateStatsFromBase(false);
    return true;
}

void PlayerProgression::RecalculateStatsFromBase(bool healToFull)
{
    const int oldMaxHp = player_.GetMaxHp();
    PlayerDerivedStatsInput derivedStatsInput{};
    derivedStatsInput.base = player_.baseStats_;
    derivedStatsInput.upgradeLevels = player_.upgradeLevels_;
    derivedStatsInput.upgradeRates = {player_.healthRegenUpgradeRate_, player_.maxHpUpgradeRate_,        player_.bodyDamageUpgradeRate_,
                                      player_.bulletSpeedUpgradeRate_, player_.bulletDamageUpgradeRate_, player_.reloadUpgradeRate_,
                                      player_.moveSpeedUpgradeRate_,   player_.minReloadSpeed_};
    derivedStatsInput.runEnabled = player_.runModifiers_.enabled;
    derivedStatsInput.runTuning = MakeTankRunTuning(player_.runModifiers_, player_.runGrowth_);
    derivedStatsInput.combatStyleSelected = player_.expeditionCombatStyleSelected_;
    derivedStatsInput.combatStyle = player_.expeditionCombatStyle_;
    if (derivedStatsInput.runEnabled && derivedStatsInput.combatStyleSelected)
        derivedStatsInput.combatStyleProfile = player_.GetCombatStyleProfile(player_.expeditionCombatStyle_);
    derivedStatsInput.maintenanceMoveScale = player_.runMaintenance_.MoveScale();
    derivedStatsInput.maintenanceReloadScale = player_.runMaintenance_.ReloadScale();
    derivedStatsInput.previousStamina = player_.stats_.stamina;

    player_.stats_ = CalculatePlayerDerivedStats(derivedStatsInput);
    player_.SetDamage(static_cast<uint32_t>((std::max)(1.0f, player_.stats_.bodyDamage)));
    player_.hp_ = ResolvePlayerRecalculatedHp(player_.hp_, oldMaxHp, player_.GetMaxHp(), healToFull);
}
