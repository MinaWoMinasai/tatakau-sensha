#include "game/weapon/CombatTypes.h"
#include "GameScene.h"
#include "game/player/TankRunModifiers.h"
#include "game/run/TankExpeditionEncounters.h"

#include <cmath>
#include <queue>
#include <fstream>

namespace {
constexpr uint32_t kTitleSeed = 20260925u;
constexpr float kDemoStageSeconds = 20.0f;

bool ClearShot(Stage& stage, const cg2::Vector3& from, const cg2::Vector3& to) {
    const cg2::Vector3 delta = to - from;
    const int steps = (std::max)(1, static_cast<int>(cg2::Length(delta) / 0.6f));
    for (int i = 1; i < steps; ++i)
        if (stage.IsCollisionWithAnyBlock(from + delta * (static_cast<float>(i) / steps), 0.35f)) return false;
    return true;
}

// A tiny fixed-grid path is enough to prevent the attract player from spending
// its whole scene shooting a wall. Combat, bullets and collisions remain real.
std::vector<cg2::Vector3> FindDemoPath(Stage& stage, const cg2::Vector3& from, const cg2::Vector3& target) {
    constexpr int width = 44, height = 28, count = width * height;
    const auto position = [](int cell) { return cg2::Vector3{1.0f + 2.0f * (cell % width), 1.0f + 2.0f * (cell / width), 0}; };
    const auto index = [](const cg2::Vector3& p) {
        return (std::clamp)(static_cast<int>(std::round((p.y - 1) / 2)), 0, height - 1) * width +
            (std::clamp)(static_cast<int>(std::round((p.x - 1) / 2)), 0, width - 1);
    };
    std::array<int, count> parent; parent.fill(-1);
    const int start = index(from); parent[start] = start;
    std::queue<int> pending; pending.push(start);
    int best = start; float bestDistance = cg2::Length(position(start) - target);
    while (!pending.empty()) {
        const int cell = pending.front(); pending.pop();
        const int x = cell % width, y = cell / width;
        const std::array<int, 4> adjacent{{x > 0 ? cell - 1 : -1, x + 1 < width ? cell + 1 : -1,
            y > 0 ? cell - width : -1, y + 1 < height ? cell + width : -1}};
        for (int next : adjacent) {
            if (next < 0 || parent[next] >= 0 || stage.IsCollisionWithAnyBlock(position(next), 0.85f)) continue;
            parent[next] = cell; pending.push(next);
            const float distance = cg2::Length(position(next) - target);
            if (distance < bestDistance) { best = next; bestDistance = distance; }
        }
    }
    std::vector<cg2::Vector3> path;
    while (best != start) { path.push_back(position(best)); best = parent[best]; }
    std::reverse(path.begin(), path.end());
    return path;
}
}

void GameScene::InitializeTitleDemo() {
    if (!titleDemo_) return;
    tankRunAutoTest_ = false;
    tutorialConfig_.enabled = false;
    debugPlayerNoDamage_ = true; // Attract mode cannot terminate on player death.
    tankExpeditionMusicEnabled_ = false;
    tankExpeditionEffectsEnabled_ = false;
    tankExpeditionAudio_.SetMusicVolume(0);
    tankExpeditionAudio_.SetEffectsVolume(0);
    titleDemoStatus_ = {};
    ResetTitleDemoStage(0);
}

void GameScene::ResetTitleDemoStage(int stage) {
    titleDemoStatus_.stage = stage % 4;
    titleDemoStatus_.stageSeconds = 0;
    titleDemoStatus_.stagesVisitedMask |= 1u << titleDemoStatus_.stage;
    titleDemoPreviousDash_ = false;
    titleDemoPreviousBulletCount_ = 0;
    titleDemoNavigationTimer_ = 0;
    titleDemoPath_.clear();
    tankRunPaused_ = false;
    phase_ = Phase::kMain;
    gameFlowState_ = GameFlowState::Playing;
    gameFlowTimer_ = 0;
    bossDefeatHandled_ = playerDeathHandled_ = false;
    bossEntryTriggered_ = true;
    timeScale_ = 1;
    screenEffectDirector_.LoadConfig("resources/configs/screenEffects.json");
    // A normal expedition has four room rewards plus at most one event card.
    // Do not let a late showcase collect a sixth, unattainable modification.
    tankrun::Config config; config.combatSeconds = 1000000; config.maxDrafts = 5;
    tankRun_ = tankrun::RunDirector(kTitleSeed, config);
    tankRun_.ChooseLoadout(0);
    tankRun_.ChooseCore(0);
    TankRunModifiers neutral{}; neutral.enabled = true; neutral.expedition = true;
    player_->SetRunModifiers(neutral);
    player_->ConfigurePrototypeLoadout(0);
    player_->SetRunCheckpointEvolution(true);
    // Each showcase starts from a reproducible build, then plays normal combat.
    const std::array<tankrun::CardId, 5> lateCards{{tankrun::CardId::Rapid, tankrun::CardId::Ricochet,
        tankrun::CardId::Heavy, tankrun::CardId::Homing, tankrun::CardId::DashBurst}};
    const std::array<tankrun::CardId, 5> bossCards{{tankrun::CardId::Heavy, tankrun::CardId::Homing,
        tankrun::CardId::Overdrive, tankrun::CardId::Pierce, tankrun::CardId::DashBurst}};
    const auto& cards = stage == 3 ? bossCards : lateCards;
    const int cardCount = stage == 0 ? 0 : stage == 1 ? 3 : 5;
    for (int i = 0; i < cardCount; ++i) {
        if (tankRun_.OpenRewardDraft(false, cards[i])) tankRun_.ChooseCard(0);
    }
    if (stage > 0) ApplyTankRunCards();
    if (stage >= 2) {
        player_->PrepareRunEvolution();
        const auto choices = player_->GetRunEvolutionChoices();
        if (!choices.empty()) player_->ChooseRunEvolution(choices.front().id);
    }
    player_->HealRunPlayer(player_->GetMaxHp());
    tankExpedition_.Reset(); tankExpedition_.Start();
    const int targetRoom = stage == 0 ? 0 : stage == 1 ? 2 : stage == 2 ? 3 : 4;
    // Restore a room snapshot for the showcase; these setup transitions are not
    // counted as played clears, rewards or routes in the runtime evidence.
    for (int step = 0; tankExpedition_.GetRoomIndex() < targetRoom && step < 30; ++step) {
        switch (tankExpedition_.GetPhase()) {
        case tankexp::Phase::Combat: tankExpedition_.CompleteRoom(); break;
        case tankexp::Phase::Reward: tankExpedition_.ChooseRewardDone(); break;
        case tankexp::Phase::Route: tankExpedition_.ChooseRoute(tankExpedition_.GetRouteRound() == 0 ? 1 : 0); break;
        case tankexp::Phase::Event: tankExpedition_.ChooseEvent(1); break;
        case tankexp::Phase::Evolution: tankExpedition_.CompleteEvolution(); break;
        default: break;
        }
    }
    StartTankExpeditionRoom();
    player_->SetDebugNoDamage(true);
    player_->SetDemoInput(true, {}, player_->GetWorldPosition() + cg2::Vector3{1,0,0}, false, false);
    tankRunMenuAge_ = 0;
    tankExpeditionAudio_.SetMusicVolume(0); tankExpeditionAudio_.SetEffectsVolume(0);
}

void GameScene::UpdateTitleDemo(float dt) {
    if (!titleDemo_) return;
    // Attract mode bypasses the ordinary expedition update (including burst cleanup).
    for(auto& burst:tankRunBursts_) burst.age+=dt;
    std::erase_if(tankRunBursts_,[](const RunBurst& b){return b.age>(b.resource?0.7f:0.35f);});
    titleDemoRoomFade_=(std::clamp)(titleDemoRoomFade_+(tankExpedition_.IsCombat()?-dt:dt)/0.5f,0.0f,1.0f);
    const size_t playerBullets = bulletManager_->GetBulletCounts().player;
    if (playerBullets > titleDemoPreviousBulletCount_)
        titleDemoStatus_.shots += static_cast<int>(playerBullets - titleDemoPreviousBulletCount_);
    titleDemoPreviousBulletCount_ = playerBullets;
    titleDemoStatus_.maxPlayerBullets = (std::max)(titleDemoStatus_.maxPlayerBullets, static_cast<int>(playerBullets));
    if (player_->IsDashing() && !titleDemoPreviousDash_) ++titleDemoStatus_.dashes;
    titleDemoPreviousDash_ = player_->IsDashing();
    titleDemoStatus_.kills = defeatedEnemies_;
    titleDemoStatus_.stageSeconds += dt; titleDemoStatus_.totalSeconds += dt;
    if (titleDemoStatus_.stageSeconds >= kDemoStageSeconds)
        ResetTitleDemoStage((titleDemoStatus_.stage + 1) % 4);
    if (gameFlowState_ != GameFlowState::Playing) return;
    tankRunMenuAge_ += dt;
    const auto phase = tankExpedition_.GetPhase();
    if (phase != tankexp::Phase::Combat) {
        player_->SetDemoInput(true, {}, player_->GetWorldPosition() + cg2::Vector3{1,0,0}, false, false);
        if (tankRunMenuAge_ > 0.35f && titleDemoRoomFade_>=1.0f) {
            const int option = phase == tankexp::Phase::Route && tankExpedition_.GetRouteRound() == 0 ? 1 : 0;
            SelectTankExpeditionOption(option);
            if (phase == tankexp::Phase::Reward) ++titleDemoStatus_.rewards;
            if (phase == tankexp::Phase::Route) ++titleDemoStatus_.routes;
        }
        return;
    }
    const auto room = tankExpedition_.GetRoomKind();
    int threats = 0;
    for (auto* actor : enemyManager_->GetEnemyPtrs()) if (actor && actor->IsCombatThreat()) ++threats;
    if (room == tankexp::RoomKind::Resource && !tankExpeditionResourceReleased_ && !threats && tankExpeditionSpawned_ > 0) {
        auto& node = tankRunResources_[0];
        node.active = enemyManager_->SpawnRunResource(node.position, 90, [this](bool owned) { OnTankRunResourceClaim(0,owned); });
        tankExpeditionResourceReleased_ = node.active;
    }
    if (room != tankexp::RoomKind::Boss && tankExpeditionSpawned_ > 0 &&
        tankexp::IsRoomObjectiveComplete(room, threats, tankExpeditionNodes_, tankExpeditionRoomPending_, enemy_->IsDead())) {
        FinishTankExpeditionRoom(); return;
    }
    tankExpedition_.Update(dt); tankRun_.Update(dt); tankExpeditionArrival_ += dt;
    const cg2::Vector3 origin = player_->GetWorldPosition();
    cg2::Vector3 aim = origin + cg2::Vector3{10,0,0}; float distance = 10000; bool found = false;
    const auto consider = [&](const cg2::Vector3& point, bool threat) {
        const float actualDistance = cg2::Length(point - origin);
        const float score = actualDistance + (ClearShot(*stage_, origin, point) ? 0.0f : 25.0f) + (threat ? 0.0f : 100.0f);
        if (score < distance) { distance = score; aim = point; found = true; }
    };
    for (auto* actor : enemyManager_->GetEnemyPtrs()) if (actor && !actor->IsDead())
        consider(actor->GetWorldPosition(), actor->IsCombatThreat() || actor->IsRunResource());
    if (IsRunRivalActive() && !enemy_->IsDead()) consider(enemy_->GetWorldPosition(), true);
    cg2::Vector3 move{};
    if (found) {
        const cg2::Vector3 toTarget = aim - origin; const float range = cg2::Length(toTarget);
        const cg2::Vector3 heading = range > 0.01f ? toTarget * (1.0f / range) : cg2::Vector3{1,0,0};
        const bool visible = ClearShot(*stage_, origin, aim);
        titleDemoNavigationTimer_ -= dt;
        if (titleDemoNavigationTimer_ <= 0) {
            titleDemoPath_ = FindDemoPath(*stage_, origin, aim);
            titleDemoNavigationTimer_ = 0.45f;
        }
        if ((!visible || range > 9) && !titleDemoPath_.empty()) {
            while (titleDemoPath_.size() > 1 && cg2::Length(titleDemoPath_.front() - origin) < 1.1f) titleDemoPath_.erase(titleDemoPath_.begin());
            move = titleDemoPath_.front() - origin;
        } else {
            const float orbit = static_cast<int>(titleDemoStatus_.stageSeconds / 3.0f) % 2 ? -1.0f : 1.0f;
            move = cg2::Vector3{-heading.y, heading.x, 0} * orbit + heading * (range < 5.0f ? -0.8f : 0.15f);
        }
        const float magnitude = cg2::Length(move); if (magnitude > 0.01f) move = move * (1.0f / magnitude);
        if (stage_->IsCollisionWithAnyBlock(origin + move * 1.6f, 0.9f)) {
            const cg2::Vector3 tangent{-move.y, move.x, 0};
            move = !stage_->IsCollisionWithAnyBlock(origin + tangent * 1.6f, 0.9f) ? tangent : tangent * -1.0f;
        }
    }
    bool danger = false;
    for (auto* bullet : bulletManager_->GetBulletPtrs()) if (bullet && !bullet->IsDead() && bullet->GetOwner() != kPlayer &&
        cg2::Length(bullet->GetWorldPosition() - origin) < 4.0f) { danger = true; break; }
    player_->SetDemoInput(true, {move.x, move.y}, aim, found, danger && !player_->IsDashing());
}

void GameScene::RequestTitleDemoCapture(const std::string& name) {
    if (!titleDemo_ || !tankRunCapturePath_.empty()) return;
    std::filesystem::create_directories("generated/title_demo");
    tankRunCapturePath_ = "generated/title_demo/" + name + ".png";
}
void GameScene::CopyTitleDemoCapture() { if (titleDemo_) CopyTankRunCapture(); }
void GameScene::FlushTitleDemoCapture() { if (titleDemo_) FinishTankRunCapture(); }

nlohmann::json GameScene::GetTitleDemoBuild() const {
    float oldestBurstAge=0;for(const auto& burst:tankRunBursts_)oldestBurstAge=(std::max)(oldestBurstAge,burst.age);
    const auto combat = player_->GetRunCombatSnapshot();
    const auto growth = bulletManager_->GetGrowthStats();
    return {{"oldestBurstAge",oldestBurstAge},{"stage",titleDemoStatus_.stage}, {"class",player_->GetCurrentClassName()},
        {"cards",tankRun_.GetDraftCount()}, {"barrels",combat.barrels},
        {"projectilesPerBarrel",combat.projectilesPerBarrel}, {"activeDrones",combat.activeDrones},
        {"reflects",combat.reflects}, {"homing",combat.homing}, {"dashBurst",combat.dashBurst},
        {"maxWallBounces",combat.maxWallBounces}, {"actorPierceCount",combat.actorPierceCount},
        {"impactSplitCount",combat.impactSplitCount}, {"dashExplosion",combat.dashExplosion},
        {"homingTurnRate",combat.homingTurnRate}, {"dashExplosionsEmitted",combat.dashExplosionsEmitted},
        {"wallBouncesObserved",growth.wallBounces}, {"actorPiercesObserved",growth.actorPierces},
        {"splitChildrenObserved",growth.splitChildrenSpawned},
        {"shotDamage",combat.shotDamage}, {"reloadSeconds",combat.reloadSeconds}};
}

void GameScene::VerifyTitleDemoTransition(float dt) {
    if (titleDemo_ || !expeditionRun_ || titleDemoTransitionVerified_) return;
    wchar_t test[8]{};
    if (!(GetEnvironmentVariableW(L"CG2_TITLE_AUTOTEST", test, 8) > 0 && test[0] == L'1')) return;
    titleDemoTransitionTimer_ += dt;
    if (titleDemoTransitionTimer_ < 2.0f) return;
    const std::filesystem::path path("generated/title_demo/validation.json");
    std::ifstream input(path);
    if (!input) return;
    auto report = nlohmann::json::parse(input, nullptr, false);
    if (report.is_discarded() || !report.value("fadeComplete", false) ||
        report.value("transitionRequested", std::string{}) != "TANK_EXPEDITION") return;
    if (titleDemoTransitionTimer_ < 2.0f + dt * 1.5f && tankRunCapturePath_.empty())
        tankRunCapturePath_ = "generated/title_demo/new_game.png";
    if (titleDemoTransitionTimer_ < 3.0f) return; // Finish the image copy on a later frame/fence.
    const bool freshStart = expeditionMapEnabled_ ?
        tankExpedition_.GetPhase() == tankexp::Phase::Map && expeditionMapRun_.IsChoosing() &&
        expeditionMapRun_.GetChosenNodeIds().empty() && expeditionMapRun_.GetVisitedNodeIds().empty() &&
        expeditionMapRun_.GetCurrency() == expeditionMapRun_.GetDefinition().startingCurrency :
        tankExpedition_.GetPhase() == tankexp::Phase::Dormant;
    const bool fresh = !player_->IsDemoInputEnabled() && !player_->IsDebugNoDamage() &&
        tankRun_.GetDraftCount() == 0 && freshStart &&
        bulletManager_->GetBulletCount() == 0 && player_->GetHp() == player_->GetMaxHp();
    report["freshGame"] = {{"valid", fresh}, {"demoInput", player_->IsDemoInputEnabled()},
        {"invulnerable", player_->IsDebugNoDamage()}, {"drafts", tankRun_.GetDraftCount()},
        {"hp", player_->GetHp()}, {"maxHp", player_->GetMaxHp()}, {"level", player_->GetLevel()},
        {"class", player_->GetCurrentClassName()}, {"bullets", bulletManager_->GetBulletCount()}};
    bool buildsValid = false, allStagesShoot = false, activeSynergiesObserved = false, projectileGrowthObserved = false;
    bool singleProjectilePerBarrel = false, noSplitInDemoBuilds = false;
    if (report.contains("stageSamples") && report["stageSamples"].is_array() && report["stageSamples"].size() == 4) {
        const auto& samples = report["stageSamples"];
        const auto& early = samples[0]["build"]; const auto& late = samples[2]["build"]; const auto& boss = samples[3]["build"];
        // The current game has one shot per barrel; this showcase gains barrels
        // through evolution and changes projectile behavior through its cards.
        // None of its fixed loadouts contains the retired split/multishot card.
        singleProjectilePerBarrel = std::all_of(samples.begin(), samples.end(), [](const auto& sample) {
            return sample["build"].value("projectilesPerBarrel", 0) == 1;
        });
        noSplitInDemoBuilds = std::all_of(samples.begin(), samples.end(), [](const auto& sample) {
            return sample["build"].value("impactSplitCount", -1) == 0 &&
                sample.value("splitChildrenObserved", uint64_t{1}) == 0;
        });
        buildsValid = singleProjectilePerBarrel && noSplitInDemoBuilds &&
            early.value("cards", -1) == 0 && early.value("barrels", 0) == 1 && !early.value("reflects", true) &&
            !early.value("homing", true) && !early.value("dashBurst", true) &&
            late.value("cards", 0) == 5 && late.value("barrels", 0) > early.value("barrels", 0) &&
            late.value("reflects", false) && late.value("homing", false) && late.value("dashBurst", false) &&
            late.value("maxWallBounces", 0) >= 4 && boss.value("cards", 0) == 5 &&
            boss.value("barrels", 0) > early.value("barrels", 0) &&
            boss.value("actorPierceCount", 0) >= 1 && boss.value("dashExplosion", false);
        activeSynergiesObserved = samples[3].value("maxHomingTurnRate", 0.0f) >= 3.0f &&
            samples[3].value("maxDashExplosionsEmitted", 0u) > 0 && samples[3].value("dashes", 0) > 0;
        projectileGrowthObserved = samples[2].value("wallBouncesObserved", uint64_t{0}) > 0 &&
            late.value("barrels", 0) > early.value("barrels", 0) &&
            samples[3].value("actorPiercesObserved", uint64_t{0}) > 0;
        allStagesShoot = std::all_of(samples.begin(), samples.end(), [](const auto& sample) {
            return sample.value("projectileEmissionSamples", 0) > 0;
        });
    }
    report["fixedBuildsValid"] = buildsValid; report["allStagesShoot"] = allStagesShoot;
    report["singleProjectilePerBarrel"] = singleProjectilePerBarrel;
    report["noSplitInDemoBuilds"] = noSplitInDemoBuilds;
    report["activeSynergiesObserved"] = activeSynergiesObserved;
    report["projectileGrowthObserved"] = projectileGrowthObserved;
    const bool completed = fresh && report.value("stagesVisited", 0) == 15 && report.value("capturedStages", 0) == 15 &&
        buildsValid && allStagesShoot && activeSynergiesObserved && projectileGrowthObserved &&
        report.value("demoFrozenDuringFade", false) && report.value("projectileEmissionSamples", 0) > 0 &&
        report.value("kills", 0) > 0 && report.value("dashes", 0) > 0 &&
        report.value("rewards", 0) > 0 && report.value("routes", 0) > 0;
    report["completed"] = completed;
    std::ofstream output(path); output << report.dump(2) << '\n'; output.close();
    titleDemoTransitionVerified_ = true;
    PostQuitMessage(completed ? 0 : 1);
}

float GameScene::GetTitleDemoFade() const {
    const float t=titleDemoStatus_.stageSeconds;
    const float edge=(std::max)((std::clamp)((0.55f-t)/0.55f,0.0f,1.0f),(std::clamp)((t-19.45f)/0.55f,0.0f,1.0f));
    const float a=(std::max)(edge,titleDemoRoomFade_);
    return a*a*(3-2*a);
}
