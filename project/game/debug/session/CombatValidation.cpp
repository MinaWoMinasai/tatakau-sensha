#include "game/debug/session/CombatValidation.h"
#include "game/session/GameplaySystems.h"
#include "Enemy.h"
#include <cmath>
#include <fstream>
#include <iomanip>

namespace gameplay {

namespace {
constexpr const char* kCombatProbeDirectory = "generated/combat_validation/";
constexpr std::array<const char*, 6> kCombatProbeNames{"Charger", "Sniper", "Skirmisher", "Flanker", "Suppressor", "Rival"};
/// @brief 指定した形状が地形の壁と重なるか判定する。
bool OverlapsWall(const Stage& stage, const cg2::AABB& body)
{
    // Resolved bodies have 0.01-unit separation. Ignore only floating point noise.
    for (const auto& row : stage.GetBlocks())
        for (const auto& block : row) {
            if (!block.isActive)
                continue;
            const float x = (std::min)(body.max.x, block.aabb.max.x) - (std::max)(body.min.x, block.aabb.min.x);
            const float y = (std::min)(body.max.y, block.aabb.max.y) - (std::max)(body.min.y, block.aabb.min.y);
            if (x > 0.035f && y > 0.035f)
                return true;
        }
    return false;
}
} // namespace

void CombatValidation::InitializeCombatValidationFixture()
{
    wchar_t flag[8]{};
    world_.run.combatValidationEnabled_ =
        !world_.demo.titleDemo_ && GetEnvironmentVariableW(L"CG2_TANK_COMBAT_AUTOTEST", flag, 8) > 0 && flag[0] == L'1';
    if (!world_.run.combatValidationEnabled_)
        return;
    world_.run.combatValidationRequested_ = world_.run.combatValidationPhase2Injected_ = false;
    world_.run.combatValidationElapsed_ = 0;
    world_.run.combatValidationIndex_ = -1;
    world_.run.combatValidationResults_ = nlohmann::json::array();
    world_.run.combatValidationErrors_.clear();
    world_.run.combatValidationCaptures_.clear();
    world_.run.tankRunAutoTest_ = world_.run.expeditionMapAutoTest_ = false;
    world_.run.tankExpeditionTutorial_.Skip();
    world_.combat.tutorialConfig_.enabled = false;
    world_.presentation.debugPlayerNoDamage_ = true;
    world_.run.tankExpeditionMusicEnabled_ = world_.run.tankExpeditionEffectsEnabled_ = false;
    world_.run.tankExpeditionAudio_.SetMusicVolume(0);
    world_.run.tankExpeditionAudio_.SetEffectsVolume(0);
    // All fixture changes are in memory; user-authored JSON is never rewritten.
    world_.run.expeditionContent_ = tankcontent::DefaultCatalog();
    tankexp::RoomDefinition room;
    room.id = "ai_probe";
    room.name = "AI / 実動作検証";
    room.objective = "boss";
    room.playerStart = {30, 29};
    room.objectiveTargets = {{56, 29}};
    room.grid.assign(tankexp::kRoomColumns * tankexp::kRoomRows, 0);
    for (int row = tankexp::kRoomTop; row <= tankexp::kRoomBottom; ++row)
        for (int col = tankexp::kRoomLeft; col <= tankexp::kRoomRight; ++col)
            if (row == tankexp::kRoomTop || row == tankexp::kRoomBottom || col == tankexp::kRoomLeft || col == tankexp::kRoomRight)
                room.grid[static_cast<size_t>(row * tankexp::kRoomColumns + col)] = 1;
    // Offset cover exercises navigation, without blocking the initial firing lane.
    for (const tankexp::RoomPoint p : {tankexp::RoomPoint{44, 36}, tankexp::RoomPoint{44, 22}})
        room.grid[static_cast<size_t>(tankexp::RoomPointCell(p))] = 1;
    world_.run.expeditionRooms_.rooms = {room};
    world_.run.expeditionMapDefinition_.startingCurrency = 0;
    world_.run.expeditionMapDefinition_.startNodes = {"ai_probe"};
    world_.run.expeditionMapDefinition_.nodes = {{"ai_probe", "AI / 実動作検証", tankexp::NodeKind::Boss, 0, 2, 0, "ai_probe", 0, 0, {}}};
    std::string error;
    if (!tankexp::ValidateRoomCatalog(world_.run.expeditionRooms_, error) ||
        !tankexp::ValidateExpeditionMap(world_.run.expeditionMapDefinition_, error))
        world_.run.combatValidationErrors_.push_back("Invalid fixture: " + error);
    std::filesystem::create_directories(kCombatProbeDirectory);
    WriteCombatValidationReport(false);
}

void CombatValidation::CaptureCombatValidation(const std::string& name)
{
    if (!world_.run.tankRunCapturePath_.empty() ||
        std::find(world_.run.combatValidationCaptures_.begin(), world_.run.combatValidationCaptures_.end(), name) !=
            world_.run.combatValidationCaptures_.end())
        return;
    world_.run.combatValidationCaptures_.push_back(name);
    world_.run.tankRunCapturePath_ = std::string(kCombatProbeDirectory) + name + ".png";
}

void CombatValidation::BeginCombatValidationProbe(int index)
{
    world_.run.combatValidationIndex_ = index;
    world_.run.combatValidationProbe_ = {};
    world_.run.combatValidationProbe_.id = kCombatProbeNames[static_cast<size_t>(index)];
    world_.resources.enemyManager_->ClearRunActors();
    world_.resources.bulletManager_->ClearAll();
    world_.resources.enemy_->SetRunEncounterEnabled(false);
    world_.run.tankExpeditionRivalActive_ = false;
    world_.resources.player_->ResetRunRoomState({30, 29, 0});
    world_.resources.player_->SetDebugNoDamage(true);
    world_.resources.player_->SetDemoInput(true, {}, {56, 29, 0}, false, false);
    if (index < 5) {
        if (!world_.resources.enemyManager_->SpawnLevelEnemy({54, 29, 0}, world_.run.combatValidationProbe_.id, 50000))
            world_.run.combatValidationErrors_.push_back("Could not spawn " + world_.run.combatValidationProbe_.id);
    } else {
        world_.run.tankExpeditionRivalActive_ = true;
        world_.resources.enemy_->ResetRunEncounter({56, 29, 0}, 10000, 1, false);
        world_.resources.enemy_->EnableExpeditionRival(true);
    }
    world_.combat.previousPlayerHp_ = world_.resources.player_->GetHp();
    world_.combat.previousBossHp_ = world_.resources.enemy_->GetHp();
    world_.combatFlow->SetEventCallout("AI VALIDATION / " + world_.run.combatValidationProbe_.id, 1.5f);
    WriteCombatValidationReport(false);
}

void CombatValidation::FinishCombatValidationProbe()
{
    const auto& p = world_.run.combatValidationProbe_;
    const bool boss = world_.run.combatValidationIndex_ == 5, charger = world_.run.combatValidationIndex_ == 0;
    auto fail = [&](const std::string& text) {
        world_.run.combatValidationErrors_.push_back(p.id + ": " + text);
    };
    if (p.pathLength < 1.0f)
        fail("No meaningful movement");
    if (p.wallIntersections)
        fail("Actor intersects solid wall");
    if (p.tunnelingViolations)
        fail("Dash crossed solid wall or implausible movement step");
    if (p.reloadViolations)
        fail("Weapon fired/consumed ammo while reloading");
    if (p.playerBulletSamples)
        fail("Validation player fired unexpectedly");
    if (charger) {
        if (p.dashes < 1)
            fail("No committed charge");
    } else {
        if (p.shots < 2 || p.projectileSamples == 0)
            fail("No real projectile emission");
        if (p.reloads < 1 || p.reloadSeconds < 0.5f)
            fail("No finite-magazine reload opening");
    }
    if ((world_.run.combatValidationIndex_ == 2 || world_.run.combatValidationIndex_ == 3 || boss) && p.dashes < 1)
        fail("No dash");
    if (boss && (!p.phase2 || p.patterns != 7u))
        fail("Boss did not reach phase two/all attack patterns");
    world_.run.combatValidationResults_.push_back({{"id", p.id},
                                                   {"seconds", p.age},
                                                   {"pathLength", p.pathLength},
                                                   {"maxStep", p.maxStep},
                                                   {"shots", p.shots},
                                                   {"dashes", p.dashes},
                                                   {"reloads", p.reloads},
                                                   {"reloadSeconds", p.reloadSeconds},
                                                   {"phaseMask", p.phases},
                                                   {"patternMask", p.patterns},
                                                   {"phase2", p.phase2},
                                                   {"wallIntersections", p.wallIntersections},
                                                   {"tunnelingViolations", p.tunnelingViolations},
                                                   {"reloadViolations", p.reloadViolations},
                                                   {"projectileSamples", p.projectileSamples},
                                                   {"maxProjectiles", p.maxProjectiles},
                                                   {"playerBulletSamples", p.playerBulletSamples},
                                                   {"trace", p.trace}});
    WriteCombatValidationReport(false);
}

void CombatValidation::WriteCombatValidationReport(bool completed)
{
    nlohmann::json report = {
        {"completed", completed},
        {"testMode", true},
        {"forcedCombatClear", false},
        {"invulnerablePlayer", true},
        {"playerShoots", false},
        {"fixtureHp", 50000},
        {"bossFixtureHp", 10000},
        {"phase2HpInjection", world_.run.combatValidationPhase2Injected_},
        {"phase2InjectionDescription", "At 14 seconds of the rival probe, apply 6000 damage to test its <=50% HP phase; no forced kills."},
        {"elapsed", world_.run.combatValidationElapsed_},
        {"currentProbe", world_.run.combatValidationIndex_},
        {"errors", world_.run.combatValidationErrors_},
        {"probes", world_.run.combatValidationResults_},
        {"captures", world_.run.combatValidationCaptures_}};
    std::ofstream(std::string(kCombatProbeDirectory) + "validation.json") << std::setw(2) << report << '\n';
}

bool CombatValidation::UpdateCombatValidation(float dt)
{
    if (!world_.run.combatValidationEnabled_)
        return false;
    world_.run.combatValidationElapsed_ += dt;
    if (world_.run.combatValidationElapsed_ > 95.0f) {
        world_.run.combatValidationErrors_.push_back("Runtime probe timed out");
        WriteCombatValidationReport(false);
        PostQuitMessage(6);
        return true;
    }
    // The ordinary presentation gate also freezes this probe's actors.
    const bool transitioning = world_.run.expeditionTransition_.IsActive();
    world_.expeditionMapController->UpdateExpeditionPresentation(dt);
    if (transitioning) {
        world_.expeditionController->RefreshTankExpeditionUi();
        return true;
    }
    if (!world_.run.combatValidationRequested_) {
        if (world_.run.combatValidationElapsed_ > 0.2f)
            CaptureCombatValidation("map");
        if (world_.run.combatValidationElapsed_ > 0.7f && world_.run.tankRunCapturePath_.empty()) {
            world_.run.combatValidationRequested_ = true;
            world_.expeditionMapController->RequestExpeditionMapNode("ai_probe");
        }
        world_.expeditionController->RefreshTankExpeditionUi();
        return true;
    }
    if (!world_.run.tankExpedition_.IsCombat()) {
        world_.expeditionController->RefreshTankExpeditionUi();
        return true;
    }
    if (world_.run.combatValidationIndex_ < 0)
        BeginCombatValidationProbe(0);
    if (world_.run.combatValidationIndex_ >= 6) {
        if (world_.run.tankRunCapturePath_.empty()) {
            const bool pass = world_.run.combatValidationErrors_.empty() && world_.run.combatValidationResults_.size() == 6;
            WriteCombatValidationReport(pass);
            PostQuitMessage(pass ? 0 : 6);
        }
        return true;
    }
    auto& p = world_.run.combatValidationProbe_;
    p.age += dt;
    p.sampleAge += dt;
    const bool boss = world_.run.combatValidationIndex_ == 5;
    // Normal Player movement, with no teleporting or auto-fire during a probe.
    const cg2::Vector3 target{32.0f + 3.0f * std::sin(p.age * 0.55f), 29.0f + 6.0f * std::sin(p.age * 0.70f), 0};
    cg2::Vector3 input = target - world_.resources.player_->GetWorldPosition();
    if (cg2::Length(input) > 1.0f)
        input = cg2::Normalize(input);
    world_.resources.player_->SetDemoInput(true, {input.x, input.y}, {56, 29, 0}, false, false);
    world_.presentation.debugPlayerNoDamage_ = true;
    cg2::Vector3 position{};
    cg2::AABB body{};
    unsigned shots = 0, dashes = 0, reloads = 0;
    bool reloading = false, dashing = false;
    int ammo = 0, phase = 0;
    bool actorFound = false;
    if (boss) {
        actorFound = world_.resources.enemy_->IsExpeditionRivalEnabled() && !world_.resources.enemy_->IsDead();
        const auto status = world_.resources.enemy_->GetRivalCombatStatus();
        position = world_.resources.enemy_->GetWorldPosition();
        body = world_.resources.enemy_->GetAABB();
        shots = status.shotsFired;
        dashes = status.dashCount;
        reloads = status.reloadCount;
        phase = static_cast<int>(status.phase);
        ammo = status.ammo;
        reloading = status.phase == RivalBossCombat::Phase::Reload;
        dashing = status.phase == RivalBossCombat::Phase::Dash;
        p.patterns |= 1u << static_cast<unsigned>(status.pattern);
        p.phase2 |= status.phase2;
        if (p.age > 14.0f && !world_.run.combatValidationPhase2Injected_) {
            world_.resources.enemy_->TakeDamage(6000);
            world_.run.combatValidationPhase2Injected_ = true;
        }
        if (dashing)
            CaptureCombatValidation("Rival_dash");
        if (reloading)
            CaptureCombatValidation("Rival_reload");
        if (status.phase2 && status.phase == RivalBossCombat::Phase::Locked)
            CaptureCombatValidation("Rival_phase2");
    } else {
        const auto actors = world_.resources.enemyManager_->GetEnemyPtrs();
        if (!actors.empty() && actors.front() && !actors.front()->IsDead()) {
            auto* actor = actors.front();
            actorFound = true;
            position = actor->GetWorldPosition();
            body = actor->GetAABB();
            shots = actor->GetCombatShotsFired();
            dashes = actor->GetCombatDashCount();
            reloads = actor->GetCombatReloadCount();
            reloading = actor->IsReloading();
            dashing = actor->IsDashing();
            ammo = actor->GetAmmoRemaining();
            phase = static_cast<int>(actor->GetCombatPhase());
        }
    }
    if (!actorFound) {
        world_.run.combatValidationErrors_.push_back(p.id + ": probe actor missing/dead");
        WriteCombatValidationReport(false);
        PostQuitMessage(6);
        return true;
    }
    if (p.hasPrevious) {
        const float movement = cg2::Length(position - p.previousPosition);
        p.pathLength += movement;
        p.maxStep = (std::max)(p.maxStep, movement);
        if (movement > 1.25f)
            ++p.tunnelingViolations;
        if (dashing) {
            // Sweep the straight dash's center so endpoint checks cannot miss a wall.
            const int steps = (std::max)(1, static_cast<int>(movement / 0.12f));
            for (int i = 1; i < steps; ++i) {
                const cg2::Vector3 sample = p.previousPosition + (position - p.previousPosition) * (static_cast<float>(i) / steps);
                if (world_.resources.stage_->IsCollisionWithAnyBlock(sample, 0.05f)) {
                    ++p.tunnelingViolations;
                    break;
                }
            }
        }
        if (p.previousReload && reloading && (shots != p.shots || ammo < p.previousAmmo))
            ++p.reloadViolations;
    }
    if (OverlapsWall(*world_.resources.stage_, body))
        ++p.wallIntersections;
    if (reloading)
        p.reloadSeconds += dt;
    p.phases |= 1u << static_cast<unsigned>(phase);
    p.previousPosition = position;
    p.previousAmmo = ammo;
    p.previousReload = reloading;
    p.hasPrevious = true;
    p.shots = shots;
    p.dashes = dashes;
    p.reloads = reloads;
    const auto bullets = world_.resources.bulletManager_->GetBulletCounts();
    const size_t enemyBullets = bullets.enemy + bullets.hostileExpEnemy;
    p.projectileSamples += enemyBullets;
    p.maxProjectiles = (std::max)(p.maxProjectiles, enemyBullets);
    p.playerBulletSamples += static_cast<int>(bullets.player);
    if (p.sampleAge >= 0.25f) {
        p.sampleAge = 0;
        p.trace.push_back({{"t", p.age},
                           {"x", position.x},
                           {"y", position.y},
                           {"phase", phase},
                           {"ammo", ammo},
                           {"shots", shots},
                           {"bullets", enemyBullets}});
    }
    if (p.age > 2.2f)
        CaptureCombatValidation(p.id);
    world_.run.tankExpedition_.Update(dt);
    world_.run.tankRun_.Update(dt);
    world_.run.tankExpeditionArrival_ += dt;
    world_.run.tankRunHudTimer_ -= dt;
    if (world_.run.tankRunHudTimer_ <= 0) {
        world_.expeditionController->RefreshTankExpeditionUi();
        world_.run.tankRunHudTimer_ = 0.10f;
    }
    if (p.age >= (boss ? 25.0f : 11.0f) && world_.run.tankRunCapturePath_.empty()) {
        FinishCombatValidationProbe();
        const int next = world_.run.combatValidationIndex_ + 1;
        if (next < 6)
            BeginCombatValidationProbe(next);
        else
            world_.run.combatValidationIndex_ = next;
    }
    return true;
}

} // namespace gameplay
