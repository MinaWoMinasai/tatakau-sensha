#include "game/demo/TitleDemoController.h"
#include "game/session/GameplaySystems.h"
#include "game/weapon/CombatTypes.h"
#include "game/player/TankRunModifiers.h"
#include "game/run/TankExpeditionEncounters.h"
#include <cmath>
#include <queue>
#include <fstream>

namespace gameplay {

namespace {
constexpr uint32_t kTitleSeed = 20260925u;
constexpr float kDemoStageSeconds = 20.0f;

/// @brief 射撃を消去する。
bool ClearShot(Stage& stage, const cg2::Vector3& from, const cg2::Vector3& to)
{
    const cg2::Vector3 delta = to - from;
    const int steps = (std::max)(1, static_cast<int>(cg2::Length(delta) / 0.6f));
    for (int i = 1; i < steps; ++i)
        if (stage.IsCollisionWithAnyBlock(from + delta * (static_cast<float>(i) / steps), 0.35f))
            return false;
    return true;
}

// A tiny fixed-grid path is enough to prevent the attract player from spending
// its whole scene shooting a wall. Combat, bullets and collisions remain real.
/// @brief デモパスを検索する。
std::vector<cg2::Vector3> FindDemoPath(Stage& stage, const cg2::Vector3& from, const cg2::Vector3& target)
{
    constexpr int width = 44, height = 28, count = width * height;
    const auto position = [](int cell) {
        return cg2::Vector3{1.0f + 2.0f * (cell % width), 1.0f + 2.0f * (cell / width), 0};
    };
    const auto index = [](const cg2::Vector3& p) {
        return (std::clamp)(static_cast<int>(std::round((p.y - 1) / 2)), 0, height - 1) * width +
               (std::clamp)(static_cast<int>(std::round((p.x - 1) / 2)), 0, width - 1);
    };
    std::array<int, count> parent;
    parent.fill(-1);
    const int start = index(from);
    parent[start] = start;
    std::queue<int> pending;
    pending.push(start);
    int best = start;
    float bestDistance = cg2::Length(position(start) - target);
    while (!pending.empty()) {
        const int cell = pending.front();
        pending.pop();
        const int x = cell % width, y = cell / width;
        const std::array<int, 4> adjacent{
            {x > 0 ? cell - 1 : -1, x + 1 < width ? cell + 1 : -1, y > 0 ? cell - width : -1, y + 1 < height ? cell + width : -1}};
        for (int next : adjacent) {
            if (next < 0 || parent[next] >= 0 || stage.IsCollisionWithAnyBlock(position(next), 0.85f))
                continue;
            parent[next] = cell;
            pending.push(next);
            const float distance = cg2::Length(position(next) - target);
            if (distance < bestDistance) {
                best = next;
                bestDistance = distance;
            }
        }
    }
    std::vector<cg2::Vector3> path;
    while (best != start) {
        path.push_back(position(best));
        best = parent[best];
    }
    std::reverse(path.begin(), path.end());
    return path;
}
} // namespace

void TitleDemoController::InitializeTitleDemo()
{
    if (!world_.demo.titleDemo_)
        return;
    world_.run.tankRunAutoTest_ = false;
    world_.combat.tutorialConfig_.enabled = false;
    world_.presentation.debugPlayerNoDamage_ = true; // Attract mode cannot terminate on player death.
    world_.run.tankExpeditionMusicEnabled_ = false;
    world_.run.tankExpeditionEffectsEnabled_ = false;
    world_.run.tankExpeditionAudio_.SetMusicVolume(0);
    world_.run.tankExpeditionAudio_.SetEffectsVolume(0);
    world_.demo.titleDemoStatus_ = {};
    ResetTitleDemoStage(0);
}

void TitleDemoController::ResetTitleDemoStage(int stage)
{
    world_.demo.titleDemoStatus_.stage = stage % 4;
    world_.demo.titleDemoStatus_.stageSeconds = 0;
    world_.demo.titleDemoStatus_.stagesVisitedMask |= 1u << world_.demo.titleDemoStatus_.stage;
    world_.demo.titleDemoPreviousDash_ = false;
    world_.demo.titleDemoPreviousBulletCount_ = 0;
    world_.demo.titleDemoNavigationTimer_ = 0;
    world_.demo.titleDemoPath_.clear();
    world_.run.tankRunPaused_ = false;
    world_.combat.phase_ = Phase::kMain;
    world_.combat.combatFlow_.Reset();
    world_.combat.bossDefeatHandled_ = world_.combat.playerDeathHandled_ = false;
    world_.combat.bossEntryTriggered_ = true;
    world_.combat.timeScale_ = 1;
    world_.combat.screenEffectDirector_.LoadConfig("resources/configs/screenEffects.json");
    // A normal expedition has four room rewards plus at most one event card.
    // Do not let a late showcase collect a sixth, unattainable modification.
    tankrun::Config config;
    config.combatSeconds = 1000000;
    config.maxDrafts = 5;
    world_.run.tankRun_ = tankrun::RunDirector(kTitleSeed, config);
    world_.run.tankRun_.ChooseLoadout(0);
    world_.run.tankRun_.ChooseCore(0);
    TankRunModifiers neutral{};
    neutral.enabled = true;
    neutral.expedition = true;
    world_.resources.player_->SetRunModifiers(neutral);
    world_.resources.player_->ConfigurePrototypeLoadout(0);
    world_.resources.player_->SetRunCheckpointEvolution(true);
    // Each showcase starts from a reproducible build, then plays normal combat.
    const std::array<tankrun::CardId, 5> lateCards{
        {tankrun::CardId::Rapid, tankrun::CardId::Ricochet, tankrun::CardId::Heavy, tankrun::CardId::Homing, tankrun::CardId::DashBurst}};
    const std::array<tankrun::CardId, 5> bossCards{
        {tankrun::CardId::Heavy, tankrun::CardId::Homing, tankrun::CardId::Overdrive, tankrun::CardId::Pierce, tankrun::CardId::DashBurst}};
    const auto& cards = stage == 3 ? bossCards : lateCards;
    const int cardCount = stage == 0 ? 0 : stage == 1 ? 3 : 5;
    for (int i = 0; i < cardCount; ++i) {
        if (world_.run.tankRun_.OpenRewardDraft(false, cards[i]))
            world_.run.tankRun_.ChooseCard(0);
    }
    if (stage > 0)
        world_.arenaRunController->ApplyTankRunCards();
    if (stage >= 2) {
        world_.resources.player_->PrepareRunEvolution();
        const auto choices = world_.resources.player_->GetRunEvolutionChoices();
        if (!choices.empty())
            world_.resources.player_->ChooseRunEvolution(choices.front().id);
    }
    world_.resources.player_->HealRunPlayer(world_.resources.player_->GetMaxHp());
    world_.run.tankExpedition_.Reset();
    world_.run.tankExpedition_.Start();
    const int targetRoom = stage == 0 ? 0 : stage == 1 ? 2 : stage == 2 ? 3 : 4;
    // Restore a room snapshot for the showcase; these setup transitions are not
    // counted as played clears, rewards or routes in the runtime evidence.
    for (int step = 0; world_.run.tankExpedition_.GetRoomIndex() < targetRoom && step < 30; ++step) {
        switch (world_.run.tankExpedition_.GetPhase()) {
        case tankexp::Phase::Combat:
            world_.run.tankExpedition_.CompleteRoom();
            break;
        case tankexp::Phase::Reward:
            world_.run.tankExpedition_.ChooseRewardDone();
            break;
        case tankexp::Phase::Route:
            world_.run.tankExpedition_.ChooseRoute(world_.run.tankExpedition_.GetRouteRound() == 0 ? 1 : 0);
            break;
        case tankexp::Phase::Event:
            world_.run.tankExpedition_.ChooseEvent(1);
            break;
        case tankexp::Phase::Evolution:
            world_.run.tankExpedition_.CompleteEvolution();
            break;
        default:
            break;
        }
    }
    world_.expeditionController->StartTankExpeditionRoom();
    world_.resources.player_->SetDebugNoDamage(true);
    world_.resources.player_->SetDemoInput(true, {}, world_.resources.player_->GetWorldPosition() + cg2::Vector3{1, 0, 0}, false, false);
    world_.run.tankRunMenuAge_ = 0;
    world_.run.tankExpeditionAudio_.SetMusicVolume(0);
    world_.run.tankExpeditionAudio_.SetEffectsVolume(0);
}

void TitleDemoController::UpdateTitleDemo(float dt)
{
    if (!world_.demo.titleDemo_)
        return;
    // Attract mode bypasses the ordinary expedition update (including burst cleanup).
    for (auto& burst : world_.run.tankRunBursts_)
        burst.age += dt;
    std::erase_if(world_.run.tankRunBursts_, [](const RunBurst& b) {
        return b.age > (b.resource ? 0.7f : 0.35f);
    });
    world_.demo.titleDemoRoomFade_ =
        (std::clamp)(world_.demo.titleDemoRoomFade_ + (world_.run.tankExpedition_.IsCombat() ? -dt : dt) / 0.5f, 0.0f, 1.0f);
    const size_t playerBullets = world_.resources.bulletManager_->GetBulletCounts().player;
    if (playerBullets > world_.demo.titleDemoPreviousBulletCount_)
        world_.demo.titleDemoStatus_.shots += static_cast<int>(playerBullets - world_.demo.titleDemoPreviousBulletCount_);
    world_.demo.titleDemoPreviousBulletCount_ = playerBullets;
    world_.demo.titleDemoStatus_.maxPlayerBullets =
        (std::max)(world_.demo.titleDemoStatus_.maxPlayerBullets, static_cast<int>(playerBullets));
    if (world_.resources.player_->IsDashing() && !world_.demo.titleDemoPreviousDash_)
        ++world_.demo.titleDemoStatus_.dashes;
    world_.demo.titleDemoPreviousDash_ = world_.resources.player_->IsDashing();
    world_.demo.titleDemoStatus_.kills = world_.combat.defeatedEnemies_;
    world_.demo.titleDemoStatus_.stageSeconds += dt;
    world_.demo.titleDemoStatus_.totalSeconds += dt;
    if (world_.demo.titleDemoStatus_.stageSeconds >= kDemoStageSeconds)
        ResetTitleDemoStage((world_.demo.titleDemoStatus_.stage + 1) % 4);
    if (world_.combat.combatFlow_.GetState() != GameFlowState::Playing)
        return;
    world_.run.tankRunMenuAge_ += dt;
    const auto phase = world_.run.tankExpedition_.GetPhase();
    if (phase != tankexp::Phase::Combat) {
        world_.resources.player_->SetDemoInput(true, {}, world_.resources.player_->GetWorldPosition() + cg2::Vector3{1, 0, 0}, false,
                                               false);
        if (world_.run.tankRunMenuAge_ > 0.35f && world_.demo.titleDemoRoomFade_ >= 1.0f) {
            const int option = phase == tankexp::Phase::Route && world_.run.tankExpedition_.GetRouteRound() == 0 ? 1 : 0;
            world_.expeditionController->SelectTankExpeditionOption(option);
            if (phase == tankexp::Phase::Reward)
                ++world_.demo.titleDemoStatus_.rewards;
            if (phase == tankexp::Phase::Route)
                ++world_.demo.titleDemoStatus_.routes;
        }
        return;
    }
    const auto room = world_.run.tankExpedition_.GetRoomKind();
    int threats = 0;
    for (auto* actor : world_.resources.enemyManager_->GetEnemyPtrs())
        if (actor && actor->IsCombatThreat())
            ++threats;
    if (room == tankexp::RoomKind::Resource && !world_.run.tankExpeditionResourceReleased_ && !threats &&
        world_.run.tankExpeditionSpawned_ > 0) {
        auto& node = world_.run.tankRunResources_[0];
        node.active = world_.resources.enemyManager_->SpawnRunResource(node.position, 90, [this](bool owned) {
            world_.arenaRunController->OnTankRunResourceClaim(0, owned);
        });
        world_.run.tankExpeditionResourceReleased_ = node.active;
    }
    if (room != tankexp::RoomKind::Boss && world_.run.tankExpeditionSpawned_ > 0 &&
        tankexp::IsRoomObjectiveComplete(room, threats, world_.run.tankExpeditionNodes_, world_.run.tankExpeditionRoomPending_,
                                         world_.resources.enemy_->IsDead())) {
        world_.expeditionController->FinishTankExpeditionRoom();
        return;
    }
    world_.run.tankExpedition_.Update(dt);
    world_.run.tankRun_.Update(dt);
    world_.run.tankExpeditionArrival_ += dt;
    const cg2::Vector3 origin = world_.resources.player_->GetWorldPosition();
    cg2::Vector3 aim = origin + cg2::Vector3{10, 0, 0};
    float distance = 10000;
    bool found = false;
    const auto consider = [&](const cg2::Vector3& point, bool threat) {
        const float actualDistance = cg2::Length(point - origin);
        const float score = actualDistance + (ClearShot(*world_.resources.stage_, origin, point) ? 0.0f : 25.0f) + (threat ? 0.0f : 100.0f);
        if (score < distance) {
            distance = score;
            aim = point;
            found = true;
        }
    };
    for (auto* actor : world_.resources.enemyManager_->GetEnemyPtrs())
        if (actor && !actor->IsDead())
            consider(actor->GetWorldPosition(), actor->IsCombatThreat() || actor->IsRunResource());
    if (world_.gameplayQueries->IsRunRivalActive() && !world_.resources.enemy_->IsDead())
        consider(world_.resources.enemy_->GetWorldPosition(), true);
    cg2::Vector3 move{};
    if (found) {
        const cg2::Vector3 toTarget = aim - origin;
        const float range = cg2::Length(toTarget);
        const cg2::Vector3 heading = range > 0.01f ? toTarget * (1.0f / range) : cg2::Vector3{1, 0, 0};
        const bool visible = ClearShot(*world_.resources.stage_, origin, aim);
        world_.demo.titleDemoNavigationTimer_ -= dt;
        if (world_.demo.titleDemoNavigationTimer_ <= 0) {
            world_.demo.titleDemoPath_ = FindDemoPath(*world_.resources.stage_, origin, aim);
            world_.demo.titleDemoNavigationTimer_ = 0.45f;
        }
        if ((!visible || range > 9) && !world_.demo.titleDemoPath_.empty()) {
            while (world_.demo.titleDemoPath_.size() > 1 && cg2::Length(world_.demo.titleDemoPath_.front() - origin) < 1.1f)
                world_.demo.titleDemoPath_.erase(world_.demo.titleDemoPath_.begin());
            move = world_.demo.titleDemoPath_.front() - origin;
        } else {
            const float orbit = static_cast<int>(world_.demo.titleDemoStatus_.stageSeconds / 3.0f) % 2 ? -1.0f : 1.0f;
            move = cg2::Vector3{-heading.y, heading.x, 0} * orbit + heading * (range < 5.0f ? -0.8f : 0.15f);
        }
        const float magnitude = cg2::Length(move);
        if (magnitude > 0.01f)
            move = move * (1.0f / magnitude);
        if (world_.resources.stage_->IsCollisionWithAnyBlock(origin + move * 1.6f, 0.9f)) {
            const cg2::Vector3 tangent{-move.y, move.x, 0};
            move = !world_.resources.stage_->IsCollisionWithAnyBlock(origin + tangent * 1.6f, 0.9f) ? tangent : tangent * -1.0f;
        }
    }
    bool danger = false;
    for (auto* bullet : world_.resources.bulletManager_->GetBulletPtrs())
        if (bullet && !bullet->IsDead() && bullet->GetOwner() != kPlayer && cg2::Length(bullet->GetWorldPosition() - origin) < 4.0f) {
            danger = true;
            break;
        }
    world_.resources.player_->SetDemoInput(true, {move.x, move.y}, aim, found, danger && !world_.resources.player_->IsDashing());
}

void TitleDemoController::RequestTitleDemoCapture(const std::string& name)
{
    if (!world_.demo.titleDemo_ || !world_.run.tankRunCapturePath_.empty())
        return;
    std::filesystem::create_directories("generated/title_demo");
    world_.run.tankRunCapturePath_ = "generated/title_demo/" + name + ".png";
}
void TitleDemoController::CopyTitleDemoCapture()
{
    if (world_.demo.titleDemo_)
        world_.arenaRunController->CopyTankRunCapture();
}
void TitleDemoController::FlushTitleDemoCapture()
{
    if (world_.demo.titleDemo_)
        world_.arenaRunController->FinishTankRunCapture();
}

nlohmann::json TitleDemoController::GetTitleDemoBuild() const
{
    float oldestBurstAge = 0;
    for (const auto& burst : world_.run.tankRunBursts_)
        oldestBurstAge = (std::max)(oldestBurstAge, burst.age);
    const auto combat = world_.resources.player_->GetRunCombatSnapshot();
    const auto growth = world_.resources.bulletManager_->GetGrowthStats();
    return {{"oldestBurstAge", oldestBurstAge},
            {"stage", world_.demo.titleDemoStatus_.stage},
            {"class", world_.resources.player_->GetCurrentClassName()},
            {"cards", world_.run.tankRun_.GetDraftCount()},
            {"barrels", combat.barrels},
            {"projectilesPerBarrel", combat.projectilesPerBarrel},
            {"activeDrones", combat.activeDrones},
            {"reflects", combat.reflects},
            {"homing", combat.homing},
            {"dashBurst", combat.dashBurst},
            {"maxWallBounces", combat.maxWallBounces},
            {"actorPierceCount", combat.actorPierceCount},
            {"impactSplitCount", combat.impactSplitCount},
            {"dashExplosion", combat.dashExplosion},
            {"homingTurnRate", combat.homingTurnRate},
            {"dashExplosionsEmitted", combat.dashExplosionsEmitted},
            {"wallBouncesObserved", growth.wallBounces},
            {"actorPiercesObserved", growth.actorPierces},
            {"splitChildrenObserved", growth.splitChildrenSpawned},
            {"shotDamage", combat.shotDamage},
            {"reloadSeconds", combat.reloadSeconds}};
}

void TitleDemoController::VerifyTitleDemoTransition(float dt)
{
    if (world_.demo.titleDemo_ || !world_.run.expeditionRun_ || world_.demo.titleDemoTransitionVerified_)
        return;
    wchar_t test[8]{};
    if (!(GetEnvironmentVariableW(L"CG2_TITLE_AUTOTEST", test, 8) > 0 && test[0] == L'1'))
        return;
    world_.demo.titleDemoTransitionTimer_ += dt;
    if (world_.demo.titleDemoTransitionTimer_ < 2.0f)
        return;
    const std::filesystem::path path("generated/title_demo/validation.json");
    std::ifstream input(path);
    if (!input)
        return;
    auto report = nlohmann::json::parse(input, nullptr, false);
    if (report.is_discarded() || !report.value("fadeComplete", false) ||
        report.value("transitionRequested", std::string{}) != "TANK_EXPEDITION")
        return;
    if (world_.demo.titleDemoTransitionTimer_ < 2.0f + dt * 1.5f && world_.run.tankRunCapturePath_.empty())
        world_.run.tankRunCapturePath_ = "generated/title_demo/new_game.png";
    if (world_.demo.titleDemoTransitionTimer_ < 3.0f)
        return; // Finish the image copy on a later frame/fence.
    const bool freshStart =
        world_.run.expeditionMapEnabled_
            ? world_.run.tankExpedition_.GetPhase() == tankexp::Phase::Map && world_.run.expeditionMapRun_.IsChoosing() &&
                  world_.run.expeditionMapRun_.GetChosenNodeIds().empty() && world_.run.expeditionMapRun_.GetVisitedNodeIds().empty() &&
                  world_.run.expeditionMapRun_.GetCurrency() == world_.run.expeditionMapRun_.GetDefinition().startingCurrency
            : world_.run.tankExpedition_.GetPhase() == tankexp::Phase::Dormant;
    const bool fresh = !world_.resources.player_->IsDemoInputEnabled() && !world_.resources.player_->IsDebugNoDamage() &&
                       world_.run.tankRun_.GetDraftCount() == 0 && freshStart && world_.resources.bulletManager_->GetBulletCount() == 0 &&
                       world_.resources.player_->GetHp() == world_.resources.player_->GetMaxHp();
    report["freshGame"] = {{"valid", fresh},
                           {"demoInput", world_.resources.player_->IsDemoInputEnabled()},
                           {"invulnerable", world_.resources.player_->IsDebugNoDamage()},
                           {"drafts", world_.run.tankRun_.GetDraftCount()},
                           {"hp", world_.resources.player_->GetHp()},
                           {"maxHp", world_.resources.player_->GetMaxHp()},
                           {"level", world_.resources.player_->GetLevel()},
                           {"class", world_.resources.player_->GetCurrentClassName()},
                           {"bullets", world_.resources.bulletManager_->GetBulletCount()}};
    bool buildsValid = false, allStagesShoot = false, activeSynergiesObserved = false, projectileGrowthObserved = false;
    bool singleProjectilePerBarrel = false, noSplitInDemoBuilds = false;
    if (report.contains("stageSamples") && report["stageSamples"].is_array() && report["stageSamples"].size() == 4) {
        const auto& samples = report["stageSamples"];
        const auto& early = samples[0]["build"];
        const auto& late = samples[2]["build"];
        const auto& boss = samples[3]["build"];
        // The current game has one shot per barrel; this showcase gains barrels
        // through evolution and changes projectile behavior through its cards.
        // None of its fixed loadouts contains the retired split/multishot card.
        singleProjectilePerBarrel = std::all_of(samples.begin(), samples.end(), [](const auto& sample) {
            return sample["build"].value("projectilesPerBarrel", 0) == 1;
        });
        noSplitInDemoBuilds = std::all_of(samples.begin(), samples.end(), [](const auto& sample) {
            return sample["build"].value("impactSplitCount", -1) == 0 && sample.value("splitChildrenObserved", uint64_t{1}) == 0;
        });
        buildsValid = singleProjectilePerBarrel && noSplitInDemoBuilds && early.value("cards", -1) == 0 && early.value("barrels", 0) == 1 &&
                      !early.value("reflects", true) && !early.value("homing", true) && !early.value("dashBurst", true) &&
                      late.value("cards", 0) == 5 && late.value("barrels", 0) > early.value("barrels", 0) &&
                      late.value("reflects", false) && late.value("homing", false) && late.value("dashBurst", false) &&
                      late.value("maxWallBounces", 0) >= 4 && boss.value("cards", 0) == 5 &&
                      boss.value("barrels", 0) > early.value("barrels", 0) && boss.value("actorPierceCount", 0) >= 1 &&
                      boss.value("dashExplosion", false);
        activeSynergiesObserved = samples[3].value("maxHomingTurnRate", 0.0f) >= 3.0f &&
                                  samples[3].value("maxDashExplosionsEmitted", 0u) > 0 && samples[3].value("dashes", 0) > 0;
        projectileGrowthObserved = samples[2].value("wallBouncesObserved", uint64_t{0}) > 0 &&
                                   late.value("barrels", 0) > early.value("barrels", 0) &&
                                   samples[3].value("actorPiercesObserved", uint64_t{0}) > 0;
        allStagesShoot = std::all_of(samples.begin(), samples.end(), [](const auto& sample) {
            return sample.value("projectileEmissionSamples", 0) > 0;
        });
    }
    report["fixedBuildsValid"] = buildsValid;
    report["allStagesShoot"] = allStagesShoot;
    report["singleProjectilePerBarrel"] = singleProjectilePerBarrel;
    report["noSplitInDemoBuilds"] = noSplitInDemoBuilds;
    report["activeSynergiesObserved"] = activeSynergiesObserved;
    report["projectileGrowthObserved"] = projectileGrowthObserved;
    const bool completed = fresh && report.value("stagesVisited", 0) == 15 && report.value("capturedStages", 0) == 15 && buildsValid &&
                           allStagesShoot && activeSynergiesObserved && projectileGrowthObserved &&
                           report.value("demoFrozenDuringFade", false) && report.value("projectileEmissionSamples", 0) > 0 &&
                           report.value("kills", 0) > 0 && report.value("dashes", 0) > 0 && report.value("rewards", 0) > 0 &&
                           report.value("routes", 0) > 0;
    report["completed"] = completed;
    std::ofstream output(path);
    output << report.dump(2) << '\n';
    output.close();
    world_.demo.titleDemoTransitionVerified_ = true;
    PostQuitMessage(completed ? 0 : 1);
}

float TitleDemoController::GetTitleDemoFade() const
{
    const float t = world_.demo.titleDemoStatus_.stageSeconds;
    const float edge = (std::max)((std::clamp)((0.55f - t) / 0.55f, 0.0f, 1.0f), (std::clamp)((t - 19.45f) / 0.55f, 0.0f, 1.0f));
    const float a = (std::max)(edge, world_.demo.titleDemoRoomFade_);
    return a * a * (3 - 2 * a);
}

void TitleDemoController::EnableTitleDemo()
{
    world_.demo.titleDemo_ = true;
}

bool TitleDemoController::IsTitleDemo() const
{
    return world_.demo.titleDemo_;
}

const TitleDemoStatus& TitleDemoController::GetTitleDemoStatus() const
{
    return world_.demo.titleDemoStatus_;
}

} // namespace gameplay
