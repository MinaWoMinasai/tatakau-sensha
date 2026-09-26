#include "GameScene.h"
#include "Enemy.h"
#include <cmath>
#include <fstream>
#include <iomanip>

namespace {
constexpr const char* kCombatProbeDirectory = "generated/combat_validation/";
constexpr std::array<const char*, 6> kCombatProbeNames{
    "Charger", "Sniper", "Skirmisher", "Flanker", "Suppressor", "Rival"
};
bool OverlapsWall(const Stage& stage, const AABB& body) {
    // Resolved bodies have 0.01-unit separation. Ignore only floating point noise.
    for (const auto& row : stage.GetBlocks()) for (const auto& block : row) {
        if (!block.isActive) continue;
        const float x = (std::min)(body.max.x, block.aabb.max.x) - (std::max)(body.min.x, block.aabb.min.x);
        const float y = (std::min)(body.max.y, block.aabb.max.y) - (std::max)(body.min.y, block.aabb.min.y);
        if (x > 0.035f && y > 0.035f) return true;
    }
    return false;
}
}

void GameScene::InitializeCombatValidationFixture() {
    wchar_t flag[8]{};
    combatValidationEnabled_ = !titleDemo_ && GetEnvironmentVariableW(L"CG2_TANK_COMBAT_AUTOTEST", flag, 8) > 0 && flag[0] == L'1';
    if (!combatValidationEnabled_) return;
    combatValidationRequested_ = combatValidationPhase2Injected_ = false;
    combatValidationElapsed_ = 0; combatValidationIndex_ = -1;
    combatValidationResults_ = nlohmann::json::array();
    combatValidationErrors_.clear(); combatValidationCaptures_.clear();
    tankRunAutoTest_ = expeditionMapAutoTest_ = false;
    tankExpeditionTutorial_.Skip(); tutorialConfig_.enabled = false;
    debugPlayerNoDamage_ = true;
    tankExpeditionMusicEnabled_ = tankExpeditionEffectsEnabled_ = false;
    tankExpeditionAudio_.SetMusicVolume(0); tankExpeditionAudio_.SetEffectsVolume(0);
    // All fixture changes are in memory; user-authored JSON is never rewritten.
    expeditionContent_ = tankcontent::DefaultCatalog();
    tankexp::RoomDefinition room;
    room.id = "ai_probe"; room.name = "AI / 実動作検証"; room.objective = "boss";
    room.playerStart = { 30, 29 }; room.objectiveTargets = { { 56, 29 } };
    room.grid.assign(tankexp::kRoomColumns * tankexp::kRoomRows, 0);
    for (int row = tankexp::kRoomTop; row <= tankexp::kRoomBottom; ++row)
        for (int col = tankexp::kRoomLeft; col <= tankexp::kRoomRight; ++col)
            if (row == tankexp::kRoomTop || row == tankexp::kRoomBottom || col == tankexp::kRoomLeft || col == tankexp::kRoomRight)
                room.grid[static_cast<size_t>(row * tankexp::kRoomColumns + col)] = 1;
    // Offset cover exercises navigation, without blocking the initial firing lane.
    for (const tankexp::RoomPoint p : { tankexp::RoomPoint{44, 36}, tankexp::RoomPoint{44, 22} })
        room.grid[static_cast<size_t>(tankexp::RoomPointCell(p))] = 1;
    expeditionRooms_.rooms = { room };
    expeditionMapDefinition_.startingCurrency = 0;
    expeditionMapDefinition_.startNodes = { "ai_probe" };
    expeditionMapDefinition_.nodes = { {"ai_probe", "AI / 実動作検証", tankexp::NodeKind::Boss, 0, 2, 0, "ai_probe", 0, 0, {}} };
    std::string error;
    if (!tankexp::ValidateRoomCatalog(expeditionRooms_, error) || !tankexp::ValidateExpeditionMap(expeditionMapDefinition_, error))
        combatValidationErrors_.push_back("Invalid fixture: " + error);
    std::filesystem::create_directories(kCombatProbeDirectory);
    WriteCombatValidationReport(false);
}

void GameScene::CaptureCombatValidation(const std::string& name) {
    if (!tankRunCapturePath_.empty() || std::find(combatValidationCaptures_.begin(), combatValidationCaptures_.end(), name) != combatValidationCaptures_.end()) return;
    combatValidationCaptures_.push_back(name);
    tankRunCapturePath_ = std::string(kCombatProbeDirectory) + name + ".png";
}

void GameScene::BeginCombatValidationProbe(int index) {
    combatValidationIndex_ = index;
    combatValidationProbe_ = {};
    combatValidationProbe_.id = kCombatProbeNames[static_cast<size_t>(index)];
    enemyManager_->ClearRunActors(); bulletManager_->ClearAll();
    enemy_->SetRunEncounterEnabled(false); tankExpeditionRivalActive_ = false;
    player_->ResetRunRoomState({30,29,0}); player_->SetDebugNoDamage(true);
    player_->SetDemoInput(true, {}, {56,29,0}, false, false);
    if (index < 5) {
        if (!enemyManager_->SpawnLevelEnemy({54,29,0}, combatValidationProbe_.id, 50000))
            combatValidationErrors_.push_back("Could not spawn " + combatValidationProbe_.id);
    } else {
        tankExpeditionRivalActive_ = true;
        enemy_->ResetRunEncounter({56,29,0}, 10000, 1, false);
        enemy_->EnableExpeditionRival(true);
    }
    previousPlayerHp_ = player_->GetHp(); previousBossHp_ = enemy_->GetHp();
    SetEventCallout("AI VALIDATION / " + combatValidationProbe_.id, 1.5f);
    WriteCombatValidationReport(false);
}

void GameScene::FinishCombatValidationProbe() {
    const auto& p = combatValidationProbe_;
    const bool boss = combatValidationIndex_ == 5, charger = combatValidationIndex_ == 0;
    auto fail = [&](const std::string& text) { combatValidationErrors_.push_back(p.id + ": " + text); };
    if (p.pathLength < 1.0f) fail("No meaningful movement");
    if (p.wallIntersections) fail("Actor intersects solid wall");
    if (p.tunnelingViolations) fail("Dash crossed solid wall or implausible movement step");
    if (p.reloadViolations) fail("Weapon fired/consumed ammo while reloading");
    if (p.playerBulletSamples) fail("Validation player fired unexpectedly");
    if (charger) { if (p.dashes < 1) fail("No committed charge"); }
    else {
        if (p.shots < 2 || p.projectileSamples == 0) fail("No real projectile emission");
        if (p.reloads < 1 || p.reloadSeconds < 0.5f) fail("No finite-magazine reload opening");
    }
    if ((combatValidationIndex_ == 2 || combatValidationIndex_ == 3 || boss) && p.dashes < 1) fail("No dash");
    if (boss && (!p.phase2 || p.patterns != 7u)) fail("Boss did not reach phase two/all attack patterns");
    combatValidationResults_.push_back({
        {"id", p.id}, {"seconds", p.age}, {"pathLength", p.pathLength}, {"maxStep", p.maxStep},
        {"shots", p.shots}, {"dashes", p.dashes}, {"reloads", p.reloads}, {"reloadSeconds", p.reloadSeconds},
        {"phaseMask", p.phases}, {"patternMask", p.patterns}, {"phase2", p.phase2},
        {"wallIntersections", p.wallIntersections}, {"tunnelingViolations", p.tunnelingViolations}, {"reloadViolations", p.reloadViolations},
        {"projectileSamples", p.projectileSamples}, {"maxProjectiles", p.maxProjectiles}, {"playerBulletSamples", p.playerBulletSamples}, {"trace", p.trace}
    });
    WriteCombatValidationReport(false);
}

void GameScene::WriteCombatValidationReport(bool completed) {
    nlohmann::json report = {
        {"completed", completed}, {"testMode", true}, {"forcedCombatClear", false},
        {"invulnerablePlayer", true}, {"playerShoots", false}, {"fixtureHp", 50000}, {"bossFixtureHp", 10000},
        {"phase2HpInjection", combatValidationPhase2Injected_},
        {"phase2InjectionDescription", "At 14 seconds of the rival probe, apply 6000 damage to test its <=50% HP phase; no forced kills."},
        {"elapsed", combatValidationElapsed_}, {"currentProbe", combatValidationIndex_},
        {"errors", combatValidationErrors_}, {"probes", combatValidationResults_}, {"captures", combatValidationCaptures_}
    };
    std::ofstream(std::string(kCombatProbeDirectory) + "validation.json") << std::setw(2) << report << '\n';
}

bool GameScene::UpdateCombatValidation(float dt) {
    if (!combatValidationEnabled_) return false;
    combatValidationElapsed_ += dt;
    if (combatValidationElapsed_ > 95.0f) {
        combatValidationErrors_.push_back("Runtime probe timed out");
        WriteCombatValidationReport(false); PostQuitMessage(6); return true;
    }
    // The ordinary presentation gate also freezes this probe's actors.
    const bool transitioning = expeditionTransition_.IsActive();
    UpdateExpeditionPresentation(dt);
    if (transitioning) { RefreshTankExpeditionUi(); return true; }
    if (!combatValidationRequested_) {
        if (combatValidationElapsed_ > 0.2f) CaptureCombatValidation("map");
        if (combatValidationElapsed_ > 0.7f && tankRunCapturePath_.empty()) {
            combatValidationRequested_ = true; RequestExpeditionMapNode("ai_probe");
        }
        RefreshTankExpeditionUi(); return true;
    }
    if (!tankExpedition_.IsCombat()) { RefreshTankExpeditionUi(); return true; }
    if (combatValidationIndex_ < 0) BeginCombatValidationProbe(0);
    if (combatValidationIndex_ >= 6) {
        if (tankRunCapturePath_.empty()) {
            const bool pass = combatValidationErrors_.empty() && combatValidationResults_.size() == 6;
            WriteCombatValidationReport(pass); PostQuitMessage(pass ? 0 : 6);
        }
        return true;
    }
    auto& p = combatValidationProbe_;
    p.age += dt; p.sampleAge += dt;
    const bool boss = combatValidationIndex_ == 5;
    // Normal Player movement, with no teleporting or auto-fire during a probe.
    const Vector3 target{32.0f + 3.0f * std::sin(p.age * 0.55f), 29.0f + 6.0f * std::sin(p.age * 0.70f), 0};
    Vector3 input = target - player_->GetWorldPosition();
    if (Length(input) > 1.0f) input = Normalize(input);
    player_->SetDemoInput(true, {input.x,input.y}, {56,29,0}, false, false);
    debugPlayerNoDamage_ = true;
    Vector3 position{}; AABB body{};
    unsigned shots = 0, dashes = 0, reloads = 0;
    bool reloading = false, dashing = false; int ammo = 0, phase = 0;
    bool actorFound = false;
    if (boss) {
        actorFound = enemy_->IsExpeditionRivalEnabled() && !enemy_->IsDead();
        const auto status = enemy_->GetRivalCombatStatus();
        position = enemy_->GetWorldPosition(); body = enemy_->GetAABB();
        shots = status.shotsFired; dashes = status.dashCount; reloads = status.reloadCount;
        phase = static_cast<int>(status.phase); ammo = status.ammo;
        reloading = status.phase == RivalBossCombat::Phase::Reload;
        dashing = status.phase == RivalBossCombat::Phase::Dash;
        p.patterns |= 1u << static_cast<unsigned>(status.pattern); p.phase2 |= status.phase2;
        if (p.age > 14.0f && !combatValidationPhase2Injected_) {
            enemy_->TakeDamage(6000); combatValidationPhase2Injected_ = true;
        }
        if (dashing) CaptureCombatValidation("Rival_dash");
        if (reloading) CaptureCombatValidation("Rival_reload");
        if (status.phase2 && status.phase == RivalBossCombat::Phase::Locked) CaptureCombatValidation("Rival_phase2");
    } else {
        const auto actors = enemyManager_->GetEnemyPtrs();
        if (!actors.empty() && actors.front() && !actors.front()->IsDead()) {
            auto* actor = actors.front(); actorFound = true;
            position = actor->GetWorldPosition(); body = actor->GetAABB();
            shots = actor->GetCombatShotsFired(); dashes = actor->GetCombatDashCount(); reloads = actor->GetCombatReloadCount();
            reloading = actor->IsReloading(); dashing = actor->IsDashing();
            ammo = actor->GetAmmoRemaining(); phase = static_cast<int>(actor->GetCombatPhase());
        }
    }
    if (!actorFound) {
        combatValidationErrors_.push_back(p.id + ": probe actor missing/dead");
        WriteCombatValidationReport(false); PostQuitMessage(6); return true;
    }
    if (p.hasPrevious) {
        const float movement = Length(position - p.previousPosition);
        p.pathLength += movement; p.maxStep = (std::max)(p.maxStep, movement);
        if (movement > 1.25f) ++p.tunnelingViolations;
        if (dashing) {
            // Sweep the straight dash's center so endpoint checks cannot miss a wall.
            const int steps = (std::max)(1, static_cast<int>(movement / 0.12f));
            for (int i = 1; i < steps; ++i) {
                const Vector3 sample = p.previousPosition + (position - p.previousPosition) * (static_cast<float>(i) / steps);
                if (stage_->IsCollisionWithAnyBlock(sample, 0.05f)) { ++p.tunnelingViolations; break; }
            }
        }
        if (p.previousReload && reloading && (shots != p.shots || ammo < p.previousAmmo)) ++p.reloadViolations;
    }
    if (OverlapsWall(*stage_, body)) ++p.wallIntersections;
    if (reloading) p.reloadSeconds += dt;
    p.phases |= 1u << static_cast<unsigned>(phase);
    p.previousPosition = position; p.previousAmmo = ammo; p.previousReload = reloading; p.hasPrevious = true;
    p.shots = shots; p.dashes = dashes; p.reloads = reloads;
    const auto bullets = bulletManager_->GetBulletCounts();
    const size_t enemyBullets = bullets.enemy + bullets.hostileExpEnemy;
    p.projectileSamples += enemyBullets; p.maxProjectiles = (std::max)(p.maxProjectiles, enemyBullets);
    p.playerBulletSamples += static_cast<int>(bullets.player);
    if (p.sampleAge >= 0.25f) {
        p.sampleAge = 0;
        p.trace.push_back({{"t",p.age},{"x",position.x},{"y",position.y},{"phase",phase},{"ammo",ammo},{"shots",shots},{"bullets",enemyBullets}});
    }
    if (p.age > 2.2f) CaptureCombatValidation(p.id);
    tankExpedition_.Update(dt); tankRun_.Update(dt); tankExpeditionArrival_ += dt;
    tankRunHudTimer_ -= dt;
    if (tankRunHudTimer_ <= 0) { RefreshTankExpeditionUi(); tankRunHudTimer_ = 0.10f; }
    if (p.age >= (boss ? 25.0f : 11.0f) && tankRunCapturePath_.empty()) {
        FinishCombatValidationProbe();
        const int next = combatValidationIndex_ + 1;
        if (next < 6) BeginCombatValidationProbe(next);
        else combatValidationIndex_ = next;
    }
    return true;
}
