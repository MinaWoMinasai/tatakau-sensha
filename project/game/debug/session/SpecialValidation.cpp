#include "game/debug/session/SpecialValidation.h"
#include "game/session/GameplaySystems.h"
#include "Enemy.h"
#include <fstream>
#include <iomanip>

namespace gameplay {

namespace {
constexpr const char* kSpecialDirectory = "generated/special_validation/";
constexpr std::array<const char*, 15> kSpecialNames{"RailCannon",     "DroneLaserLink",   "SlashWave", "ParryBlade",  "ChainLightning",
                                                    "MarkDetonation", "BoomerangShell",   "KillBurst", "DroneCharge", "DroneRebuildBomb",
                                                    "TargetPainter",  "AutonomousSpread", "DashSlash", "SpinBlade",   "WallSmash"};
constexpr std::array<tankrun::CardId, 15> kSpecialEffects{
    tankrun::CardId::RailCannon,     tankrun::CardId::DroneLaserLink,   tankrun::CardId::SlashWave,      tankrun::CardId::ParryBlade,
    tankrun::CardId::ChainLightning, tankrun::CardId::MarkDetonation,   tankrun::CardId::BoomerangShell, tankrun::CardId::KillBurst,
    tankrun::CardId::DroneCharge,    tankrun::CardId::DroneRebuildBomb, tankrun::CardId::TargetPainter,  tankrun::CardId::AutonomousSpread,
    tankrun::CardId::DashSlash,      tankrun::CardId::SpinBlade,        tankrun::CardId::WallSmash};
/// @brief 検証用の対象配置を計算する。
cg2::Vector3 ProbePosition(int index, size_t actor, float age)
{
    switch (index) {
    case 0:
        return {38 + 4 * static_cast<float>(actor), 29, 0};
    case 1:
        return {age > 1.2f ? 28.25f : 25.0f, 29, 0};
    case 2:
        return {38.6f, 29, 0};
    case 4:
        return actor == 0 ? cg2::Vector3{38, 29, 0} : actor == 1 ? cg2::Vector3{41, 32, 0} : cg2::Vector3{44, 31, 0};
    case 7:
        return actor == 0 ? cg2::Vector3{38, 29, 0} : actor == 1 ? cg2::Vector3{38, 32, 0} : cg2::Vector3{38, 26, 0};
    case 11:
        return {38, 25 + 4 * static_cast<float>(actor), 0};
    case 13:
        return actor == 0 ? cg2::Vector3{33, 29, 0} : cg2::Vector3{27, 29, 0};
    case 14:
        return actor == 0 ? cg2::Vector3{65.5f, 29, 0} : cg2::Vector3{65, 31.5f, 0};
    default:
        return {38, 29, 0};
    }
}
} // namespace

void SpecialValidation::InitializeSpecialValidationFixture()
{
    wchar_t flag[8]{};
    world_.run.specialValidationEnabled_ =
        !world_.demo.titleDemo_ && GetEnvironmentVariableW(L"CG2_TANK_SPECIAL_AUTOTEST", flag, 8) > 0 && flag[0] == L'1';
    if (!world_.run.specialValidationEnabled_)
        return;
    world_.run.specialValidation_ = {{"completed", false},
                                     {"testMode", true},
                                     {"forcedDamage", false},
                                     {"scriptedInputAndTargets", true},
                                     {"invulnerablePlayer", true},
                                     {"elapsed", 0.0},
                                     {"probe", -1},
                                     {"age", 0.0},
                                     {"requested", false},
                                     {"errors", nlohmann::json::array()},
                                     {"captures", nlohmann::json::array()},
                                     {"probes", nlohmann::json::array()}};
    world_.run.tankRunAutoTest_ = world_.run.expeditionMapAutoTest_ = world_.run.combatValidationEnabled_ = false;
    world_.validation.experienceValidationVariant_ = 0;
    world_.run.expeditionGuideActive_ = false;
    world_.run.expeditionBuildChosen_ = true;
    world_.run.expeditionBuildChoice_ = false;
    world_.run.tankExpeditionTutorial_.Skip();
    world_.combat.tutorialConfig_.enabled = false;
    world_.presentation.debugPlayerNoDamage_ = true;
    world_.run.tankExpeditionMusicEnabled_ = world_.run.tankExpeditionEffectsEnabled_ = false;
    world_.run.tankExpeditionAudio_.SetMusicVolume(0);
    world_.run.tankExpeditionAudio_.SetEffectsVolume(0);
    world_.run.expeditionContent_ = tankcontent::DefaultCatalog();
    tankcontent::Enemy target;
    target.id = "special_target";
    target.name = "特殊能力・検証標的";
    target.behavior = tankcontent::EnemyBehavior::Square;
    target.hp = 5000;
    target.contactDamage = 1;
    target.moveSpeedScale = .1f;
    target.creditDrop = 0;
    target.color = {1.0f, .40f, .25f, 1};
    world_.run.expeditionContent_.enemies.push_back(target);
    auto room = tankexp::MakeEmptyRoom("special_probe", "特殊能力・実動作検証");
    room.objective = "boss";
    room.playerStart = {30, 29};
    room.objectiveTargets = {{56, 29}};
    room.spawns.clear();
    world_.run.expeditionRooms_.rooms = {room};
    world_.run.expeditionMapDefinition_.startingCurrency = 0;
    world_.run.expeditionMapDefinition_.startNodes = {"special_probe"};
    world_.run.expeditionMapDefinition_.nodes = {
        {"special_probe", "特殊能力・実動作検証", tankexp::NodeKind::Boss, 0, 2, 0, "special_probe", 0, 0, {}}};
    std::filesystem::create_directories(kSpecialDirectory);
    std::ofstream(std::string(kSpecialDirectory) + "validation.json") << std::setw(2) << world_.run.specialValidation_ << '\n';
}

bool SpecialValidation::UpdateSpecialValidation(float dt)
{
    if (!world_.run.specialValidationEnabled_)
        return false;
    auto write = [&] {
        std::ofstream(std::string(kSpecialDirectory) + "validation.json") << std::setw(2) << world_.run.specialValidation_ << '\n';
    };
    auto capture = [&](const std::string& name) {
        auto& names = world_.run.specialValidation_["captures"];
        if (!world_.run.tankRunCapturePath_.empty() || std::find(names.begin(), names.end(), nlohmann::json(name)) != names.end())
            return;
        names.push_back(name);
        world_.run.tankRunCapturePath_ = std::string(kSpecialDirectory) + name + ".png";
    };
    auto fail = [&](const std::string& reason) {
        world_.run.specialValidation_["errors"].push_back(reason);
    };
    const double elapsed = world_.run.specialValidation_.value("elapsed", 0.0) + dt;
    world_.run.specialValidation_["elapsed"] = elapsed;
    if (elapsed > 140) {
        fail("Special combat runtime timed out");
        write();
        PostQuitMessage(8);
        return true;
    }
    const bool transitioning = world_.run.expeditionTransition_.IsActive();
    world_.expeditionMapController->UpdateExpeditionPresentation(dt);
    if (transitioning) {
        world_.expeditionController->RefreshTankExpeditionUi();
        return true;
    }
    if (!world_.run.specialValidation_.value("requested", false)) {
        if (elapsed > .3) {
            world_.run.specialValidation_["requested"] = true;
            world_.expeditionMapController->RequestExpeditionMapNode("special_probe");
        }
        world_.expeditionController->RefreshTankExpeditionUi();
        return true;
    }
    if (!world_.run.tankExpedition_.IsCombat())
        return true;
    int index = world_.run.specialValidation_.value("probe", -1);
    if (index >= static_cast<int>(kSpecialNames.size())) {
        if (world_.run.tankRunCapturePath_.empty()) {
            world_.run.specialValidation_["completed"] = world_.run.specialValidation_["errors"].empty();
            write();
            PostQuitMessage(world_.run.specialValidation_["completed"].get<bool>() ? 0 : 8);
        }
        return true;
    }
    if (index < 0 || world_.run.specialValidation_.value("beginNext", false)) {
        index = index < 0 ? 0 : index + 1;
        world_.run.specialValidation_["probe"] = index;
        world_.run.specialValidation_["beginNext"] = false;
        if (index >= static_cast<int>(kSpecialNames.size()))
            return true;
        world_.resources.enemyManager_->ClearRunActors();
        world_.resources.bulletManager_->ClearAll();
        world_.resources.enemy_->SetRunEncounterEnabled(false);
        world_.run.tankExpeditionRivalActive_ = false;
        world_.presentation.playerMeleeSlashes_.clear();
        world_.presentation.playerLaserBeams_.clear();
        world_.presentation.playerMines_.clear();
        world_.run.expeditionHitSparks_.clear();
        if (world_.resources.playerMeleeTrailManager_)
            world_.resources.playerMeleeTrailManager_->ClearInstances();
        world_.resources.player_->ResetRunRoomState(index == 14 ? cg2::Vector3{62, 29, 0} : cg2::Vector3{30, 29, 0});
        world_.resources.player_->SetDebugNoDamage(true);
        world_.run.tankRun_.Reset(20260927);
        world_.run.tankRun_.ChooseLoadout(0);
        world_.run.tankRun_.ChooseCore(0);
        if (!world_.run.tankRun_.GrantExpeditionModules({kSpecialEffects[static_cast<size_t>(index)]}))
            fail("Could not grant actual expedition module");
        if (index == 12)
            world_.run.tankRun_.GrantExpeditionModules({tankrun::CardId::SlashWave});
        if (index == 13)
            world_.run.tankRun_.GrantExpeditionModules({tankrun::CardId::ParryBlade});
        if (index == 14)
            world_.run.tankRun_.GrantExpeditionModules({tankrun::CardId::ImpactDrive});
        world_.arenaRunController->ApplyTankRunCards();
        world_.run.expeditionBuildStyle_ = index == 0 || (index >= 4 && index <= 7)    ? tankbuild::Style::Shooter
                                           : index == 1 || (index >= 8 && index <= 11) ? tankbuild::Style::Drone
                                                                                       : tankbuild::Style::Melee;
        if (!world_.resources.player_->SetExpeditionCombatStyle(world_.run.expeditionBuildStyle_))
            fail("Could not equip style");
        world_.resources.player_->SetDemoInput(true, {}, index == 14 ? cg2::Vector3{68, 29, 0} : cg2::Vector3{50, 29, 0}, false, false);
        const auto stats = world_.resources.player_->GetSpecialCombatStats();
        world_.run.specialValidation_["before"] = {{"railShots", stats.railShots},
                                                   {"linkTicks", stats.linkTicks},
                                                   {"slashWaves", stats.slashWaves},
                                                   {"parries", stats.parries},
                                                   {"perfectParries", stats.perfectParries},
                                                   {"droneCharges", stats.droneCharges},
                                                   {"droneChargeHits", stats.droneChargeHits},
                                                   {"droneBombs", stats.droneBombs},
                                                   {"droneRebuilds", stats.droneRebuilds},
                                                   {"targetLocks", stats.targetLocks},
                                                   {"dashSlashes", stats.dashSlashes},
                                                   {"dashSlashHits", stats.dashSlashHits},
                                                   {"spinTicks", stats.spinTicks},
                                                   {"wallSmashes", stats.wallSmashes}};
        world_.run.specialValidation_["age"] = 0.0;
        world_.run.specialValidation_["minHp"] = index == 7 ? 6 : 5000;
        world_.run.specialValidation_["secondMinHp"] = 5000;
        world_.run.specialValidation_["normalInjected"] = false;
        world_.run.specialValidation_["perfectInjected"] = false;
        world_.run.specialValidation_["armoredRemaining"] = 12.0f;
        world_.run.specialValidation_["maxCharge"] = 0.0f;
        world_.run.specialValidation_["maxLinks"] = 0;
        world_.run.specialValidation_["waveSamples"] = 0;
        world_.run.specialValidation_["railSamples"] = 0;
        world_.run.specialValidation_["dashInjected"] = false;
        world_.run.specialValidation_["minAvailable"] = 99;
        world_.run.specialValidation_["returnedEscort"] = false;
        world_.run.specialValidation_["thirdMinHp"] = 5000;
        world_.run.specialValidation_["spinCutInjected"] = false;
        const int count = index == 3 ? 0 : index == 0 || index == 13 || index == 14 ? 2 : index == 4 || index == 7 || index == 11 ? 3 : 1;
        for (int n = 0; n < count; ++n)
            world_.resources.enemyManager_->SpawnLevelEnemy(ProbePosition(index, static_cast<size_t>(n), 0), "special_target",
                                                            index == 7 && n == 0 ? 6 : 5000);
        world_.combatFlow->SetEventCallout(std::string("SPECIAL VALIDATION / ") + kSpecialNames[static_cast<size_t>(index)], 1.2f);
        write();
    }
    const float age = static_cast<float>(world_.run.specialValidation_.value("age", 0.0) + dt);
    world_.run.specialValidation_["age"] = age;
    // Keep the wave probe outside direct sword reach despite attack follow-through.
    if (index == 2 || index == 6 || index == 13) {
        world_.resources.player_->SetWorldPosition({30, 29, 0});
        world_.resources.player_->SetVelocity({});
    }
    const auto actors = world_.resources.enemyManager_->GetEnemyPtrs();
    // Position fixtures isolate damage from movement/knockback; no HP or damage is injected.
    for (size_t i = 0; i < actors.size(); ++i) {
        if (index != 14 && index != 7)
            actors[i]->SetWorldPosition(ProbePosition(index, i, age));
        const char* field = i == 0 ? "minHp" : i == 1 ? "secondMinHp" : "thirdMinHp";
        world_.run.specialValidation_[field] = (std::min)(world_.run.specialValidation_.value(field, 5000), actors[i]->GetHp());
    }
    const auto stats = world_.resources.player_->GetSpecialCombatStats();
    const auto& before = world_.run.specialValidation_["before"];
    world_.run.specialValidation_["maxCharge"] =
        (std::max)(world_.run.specialValidation_.value("maxCharge", 0.0f), world_.resources.player_->GetRailChargeRatio());
    world_.run.specialValidation_["maxLinks"] = (std::max)(world_.run.specialValidation_.value("maxLinks", 0),
                                                           static_cast<int>(world_.resources.player_->GetDroneLaserLinks().size()));
    for (auto* bullet : world_.resources.bulletManager_->GetBulletPtrs()) {
        if (bullet->GetSpecialKind() == Bullet::SpecialKind::Rail) {
            world_.run.specialValidation_["railSamples"] = world_.run.specialValidation_.value("railSamples", 0) + 1;
            capture("rail_fire");
        }
        if (bullet->GetSpecialKind() == Bullet::SpecialKind::SlashWave) {
            world_.run.specialValidation_["waveSamples"] = world_.run.specialValidation_.value("waveSamples", 0) + 1;
            if (cg2::Length(bullet->GetWorldPosition() - world_.resources.player_->GetWorldPosition()) > 4.5f)
                capture("slash_wave");
        }
        // Latch the first cut: a later, separately timed sword swing may finish it.
        if (index == 3 && bullet->GetOwner() == kEnemy && bullet->GetDamage() == 13 && bullet->GetBulletHp() < 12 &&
            world_.run.specialValidation_.value("armoredRemaining", 12.0f) == 12)
            world_.run.specialValidation_["armoredRemaining"] = bullet->GetBulletHp();
    }
    bool shoot = false;
    if (index == 0) {
        shoot = age > .3f && age < 1.5f;
        if (world_.resources.player_->GetRailChargeRatio() >= .999f)
            capture("rail_charge");
    }
    if (index == 1 && std::any_of(world_.resources.player_->GetDroneLaserLinks().begin(),
                                  world_.resources.player_->GetDroneLaserLinks().end(), [](const auto& link) {
                                      return link.contact;
                                  }))
        capture("drone_link");
    if (index >= 2)
        shoot = age > .35f;
    if (index == 12)
        shoot = age > .42f;
    if (index == 4 || index == 6 || index == 7)
        shoot = age > .35f && age < .46f;
    bool dash = false;
    if ((index == 12 || index == 14) && age > .42f && !world_.run.specialValidation_.value("dashInjected", false)) {
        dash = true;
        world_.run.specialValidation_["dashInjected"] = true;
    }
    world_.resources.player_->SetDemoInput(true, {},
                                           index == 14                 ? cg2::Vector3{68, 29, 0}
                                           : index >= 8 && index <= 11 ? cg2::Vector3{38, 29, 0}
                                                                       : cg2::Vector3{50, 29, 0},
                                           shoot, dash);
    if (index >= 8 && index <= 11) {
        int available = 0;
        bool escort = true;
        for (auto* drone : world_.resources.player_->GetDronePtrs()) {
            if (drone->IsRunAvailable())
                ++available;
            if (drone->GetRunMission().GetPhase() != tankspecial::DronePhase::Escort)
                escort = false;
        }
        world_.run.specialValidation_["minAvailable"] = (std::min)(world_.run.specialValidation_.value("minAvailable", 99), available);
        if (escort && stats.droneChargeHits > before.value("droneChargeHits", 0u))
            world_.run.specialValidation_["returnedEscort"] = true;
    }
    if (index == 13 && world_.resources.player_->GetSpinBladeRatio() > .35f &&
        !world_.run.specialValidation_.value("spinCutInjected", false)) {
        auto shot = std::make_unique<Bullet>();
        shot->Initialize(world_.resources.player_->GetWorldPosition() + cg2::Vector3{-2.5f, 0, 0}, {.01f, 0, 0}, 7, kEnemy, false, 6, 1);
        shot->ConfigureGrowth(0, 0, 0);
        world_.resources.bulletManager_->Add(std::move(shot));
        world_.run.specialValidation_["spinCutInjected"] = true;
        world_.run.specialValidation_["perfectAtSpin"] = stats.perfectParries;
        world_.run.specialValidation_["parryAtSpin"] = stats.parries;
    }
    const auto shooterStats = world_.resources.bulletManager_->GetShooterStats();
    if (index == 4 && shooterStats.chainHits > 0)
        capture("chain_lightning");
    if (index == 5 && shooterStats.detonations > 0)
        capture("mark_detonation");
    if (index == 6 && shooterStats.returns > 0)
        capture("boomerang_shell");
    if (index == 7 && shooterStats.burstTriggers > 0)
        capture("kill_burst");
    if (index == 8 && stats.droneChargeHits > before.value("droneChargeHits", 0u))
        capture("drone_charge");
    if (index == 9 && stats.droneBombs > before.value("droneBombs", 0u))
        capture("drone_bomb");
    if (index == 9 && stats.droneRebuilds > before.value("droneRebuilds", 0u))
        capture("drone_rebuild");
    if (index == 10 && stats.targetLocks > before.value("targetLocks", 0u))
        capture("target_painter");
    if (index == 11 && age > 1.8f)
        capture("autonomous_spread");
    if (index == 12 && stats.dashSlashes > before.value("dashSlashes", 0u))
        capture("dash_slash");
    if (index == 13 && world_.resources.player_->GetSpinBladeRatio() > .3f)
        capture("spin_blade");
    if (index == 14 && stats.wallSmashes > before.value("wallSmashes", 0u))
        capture("wall_smash");
    if (index == 3) {
        const auto spawn = [&](float hp, uint32_t damage, const cg2::Vector3& offset) {
            auto b = std::make_unique<Bullet>();
            b->Initialize(world_.resources.player_->GetWorldPosition() + offset, {-.02f, 0, 0}, damage, kEnemy, false, hp, 1);
            b->ConfigureGrowth(0, 0, 0);
            world_.resources.bulletManager_->Add(std::move(b));
        };
        for (const auto& slash : world_.presentation.playerMeleeSlashes_) {
            const float active = slash.elapsed - slash.windupDuration;
            if (!world_.run.specialValidation_.value("normalInjected", false) && slash.finisher && active > .125f && active < .165f) {
                spawn(tankspecial::kOrdinaryEnemyBulletHp, 7, {2.5f, 0, 0});
                world_.run.specialValidation_["normalInjected"] = true;
            }
            if (world_.run.specialValidation_.value("normalInjected", false) &&
                !world_.run.specialValidation_.value("perfectInjected", false) && stats.parries > before.value("parries", 0u) &&
                active >= 0 && active < .04f) {
                spawn(tankspecial::kOrdinaryEnemyBulletHp, 9, {2.5f, 0, 0});
                spawn(tankspecial::kBossEnemyBulletHp, 13, {2.8f, .2f, 0});
                world_.run.specialValidation_["perfectInjected"] = true;
            }
        }
        if (stats.parries > before.value("parries", 0u) && stats.perfectParries == before.value("perfectParries", 0u))
            capture("parry_normal");
        if (stats.perfectParries > before.value("perfectParries", 0u))
            capture("parry_perfect");
    }
    const float duration = index == 9                                                               ? 11.5f
                           : index == 8                                                             ? 5.4f
                           : index == 3 || index == 10 || index == 12 || index == 13 || index == 14 ? 5.0f
                                                                                                    : 4.0f;
    if (age > duration && world_.run.tankRunCapturePath_.empty()) {
        nlohmann::json result = {{"id", kSpecialNames[static_cast<size_t>(index)]},
                                 {"damage", (index == 7 ? 6 : 5000) - world_.run.specialValidation_.value("minHp", 5000)},
                                 {"secondDamage", 5000 - world_.run.specialValidation_.value("secondMinHp", 5000)},
                                 {"railShots", stats.railShots - before.value("railShots", 0u)},
                                 {"linkTicks", stats.linkTicks - before.value("linkTicks", 0u)},
                                 {"slashWaves", stats.slashWaves - before.value("slashWaves", 0u)},
                                 {"parries", stats.parries - before.value("parries", 0u)},
                                 {"perfectParries", stats.perfectParries - before.value("perfectParries", 0u)},
                                 {"maxCharge", world_.run.specialValidation_["maxCharge"]},
                                 {"maxLinks", world_.run.specialValidation_["maxLinks"]},
                                 {"waveSamples", world_.run.specialValidation_["waveSamples"]},
                                 {"railSamples", world_.run.specialValidation_["railSamples"]},
                                 {"armoredRemaining", world_.run.specialValidation_["armoredRemaining"]},
                                 {"chainHits", shooterStats.chainHits},
                                 {"detonations", shooterStats.detonations},
                                 {"returns", shooterStats.returns},
                                 {"burstTriggers", shooterStats.burstTriggers},
                                 {"burstChildren", shooterStats.burstChildren},
                                 {"droneCharges", stats.droneCharges - before.value("droneCharges", 0u)},
                                 {"droneChargeHits", stats.droneChargeHits - before.value("droneChargeHits", 0u)},
                                 {"droneBombs", stats.droneBombs - before.value("droneBombs", 0u)},
                                 {"droneRebuilds", stats.droneRebuilds - before.value("droneRebuilds", 0u)},
                                 {"targetLocks", stats.targetLocks - before.value("targetLocks", 0u)},
                                 {"minAvailable", world_.run.specialValidation_["minAvailable"]},
                                 {"returnedEscort", world_.run.specialValidation_["returnedEscort"]},
                                 {"dashSlashes", stats.dashSlashes - before.value("dashSlashes", 0u)},
                                 {"dashSlashHits", stats.dashSlashHits - before.value("dashSlashHits", 0u)},
                                 {"spinTicks", stats.spinTicks - before.value("spinTicks", 0u)},
                                 {"wallSmashes", stats.wallSmashes - before.value("wallSmashes", 0u)},
                                 {"thirdDamage", 5000 - world_.run.specialValidation_.value("thirdMinHp", 5000)}};
        if (index == 0 && (result["railShots"].get<int>() != 1 || result["damage"].get<int>() < 25 ||
                           result["secondDamage"].get<int>() < 25 || result["maxCharge"].get<float>() < .99f))
            fail("Rail charge/release/pierce actual target damage incomplete");
        if (index == 1 && (result["linkTicks"].get<int>() < 2 || result["damage"].get<int>() < 2 || result["maxLinks"].get<int>() != 3))
            fail("Actual drone laser contact/damage incomplete");
        if (index == 2 && (result["slashWaves"].get<int>() < 1 || result["damage"].get<int>() < 1 || result["waveSamples"].get<int>() < 1))
            fail("Actual finisher wave failed to hit outside melee reach");
        if (index == 3 && (result["parries"].get<int>() < 3 || result["perfectParries"].get<int>() < 2 ||
                           result["armoredRemaining"].get<float>() <= 0 || result["armoredRemaining"].get<float>() >= 12))
            fail("Normal/perfect cut and armored bullet survival incomplete");
        if (index == 4 && (shooterStats.chainHits != 2 || result["secondDamage"].get<int>() < 1 || result["thirdDamage"].get<int>() < 1))
            fail("Chain lightning did not hit exactly two additional targets");
        if (index == 5 && shooterStats.detonations < 1)
            fail("Actual repeated projectile hits did not detonate mark");
        if (index == 6 && (shooterStats.returns != 1 || result["damage"].get<int>() < 12))
            fail("One boomerang did not return and hit outward/inward");
        if (index == 7 && (shooterStats.burstTriggers != 1 || shooterStats.burstChildren != 6))
            fail("Kill burst missing or recursively triggered");
        if (index == 8 && (result["droneCharges"].get<int>() < 1 || result["droneChargeHits"].get<int>() < 1 ||
                           !world_.run.specialValidation_.value("returnedEscort", false)))
            fail("Drone charge hit/return incomplete");
        if (index == 9 && (result["droneBombs"].get<int>() < 1 || result["droneRebuilds"].get<int>() < 1 ||
                           world_.run.specialValidation_.value("minAvailable", 99) != 2))
            fail("Drone bomb offline/rebuild incomplete");
        if (index == 10 && result["targetLocks"].get<int>() < 1)
            fail("Distinct drone attacks did not build target LOCK");
        if (index == 11 &&
            (result["damage"].get<int>() < 1 || result["secondDamage"].get<int>() < 1 || result["thirdDamage"].get<int>() < 1))
            fail("Autonomous drones did not damage distinct targets");
        if (index == 12 &&
            (result["dashSlashes"].get<int>() < 1 || result["dashSlashHits"].get<int>() < 1 || result["slashWaves"].get<int>() < 1))
            fail("Dash slash hit/combo finisher connection incomplete");
        if (index == 13 && (result["spinTicks"].get<int>() < 4 || result["secondDamage"].get<int>() < 1 ||
                            stats.parries <= world_.run.specialValidation_.value("parryAtSpin", stats.parries) ||
                            stats.perfectParries != world_.run.specialValidation_.value("perfectAtSpin", stats.perfectParries)))
            fail("Spin damage or non-perfect bullet cut incomplete");
        if (index == 14 && result["wallSmashes"].get<int>() < 1)
            fail("Actual knockback/wall collision did not create WallSmash");
        world_.run.specialValidation_["probes"].push_back(result);
        world_.run.specialValidation_["beginNext"] = true;
        write();
    }
    world_.expeditionController->RefreshTankExpeditionUi();
    return true;
}

} // namespace gameplay
