#include "game/debug/session/GameplayScenarioRunner.h"
#include "game/session/GameplaySystems.h"
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "Enemy.h"
#endif
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "PlayerDrone.h"
#endif
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "RuntimeProfiler.h"
#endif
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include <cmath>
#endif
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include <cstdio>
#endif
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include <filesystem>
#endif
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include <stdexcept>
#endif

namespace gameplay {
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)

namespace {
using gameplaytest::Scenario;
void RequireScenario(bool condition, const std::string& error)
{
    if (!condition)
        throw std::runtime_error(error);
}
bool BossScenario(Scenario scenario)
{
    return scenario == Scenario::RivalBoss || scenario == Scenario::PrototypeBoss || scenario == Scenario::NeonBoss ||
           scenario == Scenario::BossDeath || gameplaytest::IsNeonDepthScenario(scenario);
}

// Preserve authored walls. Only the in-memory wave is replaced; points are
// selected from the room's real free cells rather than putting actors in walls.
std::vector<tankexp::RoomPoint> ScenarioSpawnPoints(const tankexp::RoomDefinition& room, bool melee, bool boss)
{
    std::vector<tankexp::RoomPoint> points;
    auto add = [&](tankexp::RoomPoint point) {
        if (!tankexp::IsRoomPointClear(room, point))
            return;
        const float px = point.x - room.playerStart.x, py = point.y - room.playerStart.y;
        if (px * px + py * py < 4.0f)
            return;
        if (boss && !room.objectiveTargets.empty()) {
            const auto target = room.objectiveTargets.front();
            const float bx = point.x - target.x, by = point.y - target.y;
            if (bx * bx + by * by < 12.25f)
                return;
        }
        for (const auto previous : points) {
            const float dx = point.x - previous.x, dy = point.y - previous.y;
            if (dx * dx + dy * dy < 2.56f)
                return;
        }
        points.push_back(point);
    };
    if (melee)
        add({room.playerStart.x + 3.0f, room.playerStart.y});
    for (const auto& spawn : room.spawns)
        add({spawn.x, spawn.y});
    for (int row = tankexp::kRoomTop + 1; row < tankexp::kRoomBottom; ++row)
        for (int column = tankexp::kRoomLeft + 1; column < tankexp::kRoomRight; ++column)
            add(tankexp::RoomCellToWorld(column, row));
    return points;
}
} // namespace

void GameplayScenarioRunner::InitializeGameplayScenario()
{
    auto& session = GameplayScenarioSession::Get();
    if (!session.IsActive() || session.IsFinished() || world_.demo.titleDemo_)
        return;
    session.BeginScene();
    try {
        if (session.GetSettings().recording.enabled) {
            Microsoft::WRL::ComPtr<ID3D12Resource> backbuffer;
            // DirectX getters are nonconst; no device work/copy is submitted here.
            auto* writableDx = cg2::Object3dCommon::GetInstance()->GetDxCommon();
            const HRESULT hr =
                writableDx->GetSwapChain()->GetBuffer(writableDx->GetSwapChain()->GetCurrentBackBufferIndex(), IID_PPV_ARGS(&backbuffer));
            RequireScenario(SUCCEEDED(hr), "Could not inspect the native recording backbuffer.");
            const auto desc = backbuffer->GetDesc();
            RequireScenario(desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM && desc.SampleDesc.Count == 1 && desc.Width <= 8192 &&
                                session.BeginRecording(static_cast<unsigned>(desc.Width), desc.Height),
                            "Native recording preflight failed.");
        }
        RequireScenario(world_.run.expeditionRun_ && world_.run.expeditionMapEnabled_ && world_.resources.player_ &&
                            world_.resources.enemy_ && world_.resources.enemyManager_ && world_.resources.stage_ &&
                            world_.resources.bulletManager_,
                        "Gameplay scenarios require the initialized TANK_EXPEDITION scene.");
        const auto& settings = session.GetSettings();
        const bool boss = BossScenario(settings.scenario);
        // 'arena' is a Developer alias for these existing authored rooms.
        const std::string roomId = settings.room == "arena" ? (boss ? "final_duel" : "outskirts") : settings.room;
        const auto* source = tankexp::FindRoom(world_.run.expeditionRooms_, roomId);
        RequireScenario(source && source->objective == (boss ? "boss" : "eliminate"),
                        "Scenario room is missing or has an incompatible objective: " + roomId);
        const auto* bossRoom = tankexp::FindRoom(world_.run.expeditionRooms_, "final_duel");
        RequireScenario(bossRoom && bossRoom->objective == "boss", "The authored final_duel boss room is missing.");
        const auto* secondRoom = tankexp::FindRoom(world_.run.expeditionRooms_, roomId == "crossfire" ? "outskirts" : "crossfire");
        RequireScenario(secondRoom && secondRoom->objective == "eliminate", "A second authored eliminate room is missing.");

        tankexp::MapDefinition fixture;
        fixture.startingCurrency = 1000;
        fixture.startNodes = {boss ? "scenario_boss" : "scenario_a"};
        if (boss) {
            fixture.nodes = {{"scenario_boss", "Scenario boss", tankexp::NodeKind::Boss, 0, 2, 0, roomId, 0, 0, {}}};
        } else {
            const bool expedition = settings.scenario == Scenario::ExpeditionTransition;
            const bool transition = expedition || settings.scenario == Scenario::StageTransition;
            fixture.nodes.push_back({"scenario_a",
                                     "Scenario combat A",
                                     tankexp::NodeKind::Combat,
                                     0,
                                     2,
                                     0,
                                     roomId,
                                     0,
                                     0,
                                     {expedition   ? "scenario_upgrade"
                                      : transition ? "scenario_b"
                                                   : "scenario_boss"}});
            int column = 1;
            if (expedition)
                fixture.nodes.push_back(
                    {"scenario_upgrade", "Scenario workshop", tankexp::NodeKind::Upgrade, column++, 2, -1, "", 0, 0, {"scenario_b"}});
            if (transition)
                fixture.nodes.push_back({"scenario_b",
                                         "Scenario combat B",
                                         tankexp::NodeKind::Combat,
                                         column++,
                                         2,
                                         1,
                                         secondRoom->id,
                                         0,
                                         0,
                                         {"scenario_boss"}});
            fixture.nodes.push_back(
                {"scenario_boss", "Scenario final boss", tankexp::NodeKind::Boss, column, 2, 2, bossRoom->id, 0, 0, {}});
        }
        std::string error;
        if (!world_.run.expeditionMapRun_.Reset(fixture, error))
            throw std::runtime_error("Invalid scenario map: " + error);
        // Existing map visuals borrow definition indices. Keep valid slots and
        // remove old edges at the frame boundary before the normal UI refresh.
        RequireScenario(world_.run.expeditionMapVisuals_.size() >= fixture.nodes.size(),
                        "The authored map has too few initialized visual slots.");
        world_.run.expeditionMapVisuals_.resize(fixture.nodes.size());
        world_.run.expeditionMapEdges_.clear();
        world_.run.expeditionMapSelection_ = fixture.startNodes.front();
        world_.run.expeditionMapScroll_ = 0.0f;
        world_.run.expeditionBuildChoice_ = false;
        world_.run.expeditionBuildChosen_ = true;
        world_.run.expeditionGuideActive_ = false;
        world_.run.tankExpeditionTutorial_.Skip();
        world_.combat.tutorialConfig_.enabled = false;
        world_.run.expeditionAuthoringHubOpen_ = false;
        world_.run.tankRunPaused_ = false;
        world_.run.expeditionRoomEditorOpen_ = world_.run.expeditionMapEditorOpen_ = world_.run.expeditionContentEditorOpen_ = false;
        world_.run.tankExpeditionBalanceEditorOpen_ = world_.run.tankExpeditionDetailsOpen_ = world_.run.expeditionMapPreview_ = false;
        world_.run.expeditionTransition_ = {};
        world_.run.expeditionTransitionAction_ = 0;
        world_.run.expeditionCollectAll_ = false;
        world_.combat.combatFlow_.Reset();
        world_.combat.bossDefeatHandled_ = world_.combat.playerDeathHandled_ = false;
        world_.run.tankRunMenuAge_ = 1.0f;
        world_.run.tankExpeditionMusicEnabled_ = world_.run.tankExpeditionEffectsEnabled_ = false;
        world_.run.tankExpeditionAudio_.SetMusicVolume(0);
        world_.run.tankExpeditionAudio_.SetEffectsVolume(0);

        tankrun::Config runConfig;
        runConfig.combatSeconds = 1000000.0f;
        world_.run.tankRun_ = tankrun::RunDirector(settings.seed, runConfig);
        RequireScenario(world_.run.tankRun_.ChooseLoadout(0) && world_.run.tankRun_.ChooseCore(0),
                        "Could not initialize scenario loadout.");
        world_.arenaRunController->ApplyTankRunCards();
        world_.resources.player_->ConfigurePrototypeLoadout(0);
        int style = settings.playerStyle;
        if (style == -1)
            style = settings.scenario == Scenario::Drone ? 1 : settings.scenario == Scenario::Melee ? 2 : 0;
        world_.run.expeditionBuildStyle_ = static_cast<tankbuild::Style>(style);
        RequireScenario(world_.resources.player_->SetExpeditionCombatStyle(world_.run.expeditionBuildStyle_),
                        "Could not apply the scenario player style.");
        world_.run.expeditionPurchases_.clear();
        world_.run.expeditionPurchasedModules_.clear();
        for (const auto& id : settings.upgrades) {
            const auto* upgrade = tankcontent::FindUpgrade(world_.run.expeditionContent_, id);
            RequireScenario(
                upgrade && tankcontent::EligibleUpgrade(*upgrade, world_.run.expeditionBuildStyle_, world_.run.tankRun_.GetCardCounts()),
                "Scenario upgrade is unknown, already equipped or incompatible: " + id);
            RequireScenario(world_.run.tankRun_.GrantExpeditionModules(upgrade->effects), "Could not grant scenario upgrade: " + id);
            world_.run.expeditionPurchases_[id] = 1;
            world_.run.expeditionPurchasedModules_[id] = *upgrade;
        }
        world_.arenaRunController->ApplyTankRunCards();
        world_.run.tankExpedition_.Reset();
        world_.run.tankExpedition_.OpenMap();
        world_.expeditionMapController->EnterExpeditionMapNode(fixture.startNodes.front());
        RequireScenario(world_.run.expeditionMapRun_.GetActiveNodeId() == fixture.startNodes.front() &&
                            world_.run.tankExpedition_.IsCombat(),
                        "The actual authored scenario room could not be entered: " + world_.run.expeditionMapStatus_);

        world_.resources.enemyManager_->ClearRunActors();
        if (!settings.enemyWave.empty()) {
            for (const auto& wave : settings.enemyWave) {
                const cg2::Vector3 position{wave.position[0], wave.position[1], wave.position[2]};
                RequireScenario(tankcontent::FindEnemy(world_.run.expeditionContent_, wave.type) &&
                                    !world_.resources.stage_->IsCollisionWithAnyBlock(position, 0.8f) &&
                                    world_.resources.enemyManager_->SpawnLevelEnemy(position, wave.type, wave.hp),
                                "Could not spawn configured enemy: " + wave.type);
            }
        } else {
            const auto points = ScenarioSpawnPoints(*source, settings.scenario == Scenario::Melee, boss);
            RequireScenario(points.size() >= settings.enemyCount, "Not enough valid free cells for the configured wave.");
            const std::array<const char*, 3> fallbackTypes{"Charger", "Skirmisher", "Sniper"};
            for (unsigned index = 0; index < settings.enemyCount; ++index) {
                const std::string type = source->spawns.empty() ? fallbackTypes[index % fallbackTypes.size()]
                                                                : source->spawns[index % source->spawns.size()].type;
                const auto point = points[index];
                RequireScenario(tankcontent::FindEnemy(world_.run.expeditionContent_, type) &&
                                    world_.resources.enemyManager_->SpawnLevelEnemy({point.x, point.y, 0}, type, 50000),
                                "Could not spawn scenario enemy: " + type);
            }
        }
        world_.run.tankExpeditionSpawned_ = static_cast<int>(world_.resources.enemyManager_->GetEnemyCount());
        if (boss) {
            const auto target = source->objectiveTargets.front();
            world_.resources.enemy_->ResetRunEncounter({target.x, target.y, 0}, settings.bossHp, 1, true);
            world_.resources.enemy_->EnableExpeditionRival(settings.scenario != Scenario::PrototypeBoss);
            if (gameplaytest::IsNeonDepthScenario(settings.scenario))
                world_.depthEncounter->ConfigureNeonDepthEncounter(true);
            else
                world_.resources.enemy_->EnableNeonDepthEncounter(false);
            auto progress = world_.resources.enemy_->GetEnemyProgressConfig();
            progress.levelingModeEnabled = false;
            world_.resources.enemy_->SetEnemyProgressConfig(progress);
        }
        world_.resources.neonBossVisualEnabled_ = settings.scenario == Scenario::NeonBoss || settings.scenario == Scenario::BossDeath ||
                                                  gameplaytest::IsNeonDepthScenario(settings.scenario);
        if (world_.resources.neonBossVisual_)
            world_.resources.neonBossVisual_->SetEnabled(world_.resources.neonBossVisualEnabled_);
        RequireScenario(settings.playerHp != 0,
                        "Initial playerHp=0 is unsupported by the living room-reset contract; use player_restart for real lethal damage.");
        if (settings.playerHp > 0) {
            RequireScenario(settings.playerHp <= world_.resources.player_->GetMaxHp(),
                            "Configured playerHp exceeds this player's actual maximum HP.");
            if (settings.playerHp < world_.resources.player_->GetHp())
                RequireScenario(world_.resources.player_->SpendRunHealth(world_.resources.player_->GetHp() - settings.playerHp),
                                "Could not apply configured player HP.");
            else
                world_.resources.player_->HealRunPlayer(settings.playerHp - world_.resources.player_->GetHp());
        }
        world_.presentation.debugPlayerNoDamage_ = true;
        world_.resources.player_->SetDebugNoDamage(true);
        world_.combat.previousPlayerHp_ = world_.resources.player_->GetHp();
        world_.combat.previousBossHp_ = world_.resources.enemy_->GetHp();
        world_.resources.player_->SetDemoInput(true, {}, boss ? world_.resources.enemy_->GetWorldPosition() : cg2::Vector3{50, 29, 0},
                                               false, false);
        for (unsigned index = 0; index < settings.initialProjectiles; ++index) {
            const float angle = static_cast<float>(index) * 0.618034f;
            const float radius = 2.0f + static_cast<float>(index % 12) * 0.30f;
            const auto point = source->playerStart;
            cg2::Vector3 position{point.x + 8.0f + std::cos(angle) * radius, point.y + std::sin(angle) * radius, 0};
            if (world_.resources.stage_->IsCollisionWithAnyBlock(position, 0.5f))
                position = world_.resources.player_->GetWorldPosition();
            auto projectile = std::make_unique<Bullet>();
            const auto owner = index < BulletManager::kMaxRunProjectilesPerOwner ? kPlayer : kEnemy;
            projectile->Initialize(position, {std::cos(angle) * 0.08f, std::sin(angle) * 0.08f, 0}, 1, owner, true);
            projectile->ConfigureGrowth(4, 8, 0);
            world_.resources.bulletManager_->Add(std::move(projectile));
        }
        RequireScenario(world_.resources.bulletManager_->GetBulletCount() == settings.initialProjectiles,
                        "The configured initial projectile count exceeds the real manager's owner cap.");
        if (gameplaytest::IsNeonDepthScenario(settings.scenario))
            world_.depthValidation->InitializeNeonDepthValidation();
        world_.resources.gameplayScenarioInitialized_ = true;
        session.ReportDetail("fixture", {{"authoredRoom", roomId},
                                         {"geometryPreserved", true},
                                         {"playerInvulnerable", world_.presentation.debugPlayerNoDamage_},
                                         {"upgrades", settings.upgrades},
                                         {"enemyCount", world_.resources.enemyManager_->GetEnemyCount()},
                                         {"initialProjectiles", world_.resources.bulletManager_->GetBulletCount()},
                                         {"sceneEpoch", session.GetSceneEpoch()}});
        world_.expeditionController->RefreshTankExpeditionUi();
        if (session.GetFrame() == 0 &&
            std::find(settings.captureFrames.begin(), settings.captureFrames.end(), 0u) != settings.captureFrames.end())
            QueueGameplayScenarioCapture("frame_0");
    }
    catch (const std::exception& error) {
        session.Fail(std::string("Scenario initialization: ") + error.what());
        session.Finish();
        PostQuitMessage(9);
    }
}

bool GameplayScenarioRunner::PrepareGameplayScenarioFrame()
{
    auto& session = GameplayScenarioSession::Get();
    if (!session.IsActive() || world_.demo.titleDemo_)
        return false;
    if (session.IsFinished())
        return true;
    if (session.GetSettings().recording.enabled &&
        ((world_.presentation.developerBloomFreeze_ &&
          (!world_.resources.gameplayScenarioCapturePending_ || world_.resources.gameplayScenarioCaptureRecording_)) ||
         world_.resources.neonBossDeveloperFreeze_ || world_.gameplayQueries->IsNeonShowcaseActive())) {
        session.Fail("Continuous recording cannot use comparison freeze or Showcase.");
        world_.bossPresentation->UpdateNeonBossVisual(0.0f);
        session.Finish();
        PostQuitMessage(9);
        return true;
    }
    world_.resources.gameplayScenarioCapture_.Resolve(*cg2::Object3dCommon::GetInstance()->GetDxCommon());
    if (world_.resources.gameplayScenarioCapturePending_) {
        if (world_.resources.gameplayScenarioCapture_.IsBusy()) {
            if (world_.resources.gameplayScenarioCaptureRecording_) {
                // PostDraw already waits for this frame fence. Never introduce
                // an extra TAA/history draw into a successful continuous run.
                session.Fail("Recording fence did not resolve on the next Update; additional held Draw rejected.");
                world_.bossPresentation->UpdateNeonBossVisual(0.0f);
                session.Finish();
                PostQuitMessage(9);
                return true;
            }
            // The existing freeze branch still resets renderer frame arenas.
            world_.presentation.developerBloomFreeze_ = true;
            return false;
        }
        if (world_.resources.gameplayScenarioCapture_.WasLastCaptureSuccessful()) {
            if (world_.resources.gameplayScenarioCaptureRecording_) {
                try {
                    const auto directory = tankexp::ExpeditionMapPath(session.GetOutputDirectory()) / "recording/frames";
                    session.ReportRecordedFrame(
                        world_.resources.gameplayScenarioRecordingSequence_, world_.resources.gameplayScenarioCaptureName_,
                        world_.resources.gameplayScenarioCaptureSnapshot_,
                        std::filesystem::file_size(directory / (world_.resources.gameplayScenarioCaptureName_ + ".png")),
                        std::filesystem::file_size(directory / (world_.resources.gameplayScenarioCaptureName_ + ".json")));
                }
                catch (const std::exception& error) {
                    session.Fail(std::string("Recording save validation: ") + error.what());
                }
            } else
                session.ReportCapture(world_.resources.gameplayScenarioCaptureName_, world_.resources.gameplayScenarioCaptureSnapshot_);
        } else
            session.Fail("Scenario capture failed: " + world_.resources.gameplayScenarioCapture_.GetStatus());
        world_.resources.gameplayScenarioCapturePending_ = false;
        if (!world_.resources.gameplayScenarioCaptureRecording_)
            world_.presentation.developerBloomFreeze_ = false;
        world_.resources.gameplayScenarioCaptureRecording_ = false;
    }
    if (session.ShouldFinish() || !world_.resources.gameplayScenarioInitialized_) {
        if (session.GetErrors().empty() && session.GetFrame() == session.GetSettings().frames) {
            const auto final = MakeGameplayScenarioSnapshot();
            const auto scenario = session.GetSettings().scenario;
            if (scenario == Scenario::BossDeath &&
                !(final.bossDead && final.bossHp == 0 && final.visualCreates == 1 && final.visualReleases == 1 &&
                  final.dissolveProgress == 1 && !final.visualHasResources && final.visualConstantBuffers == 0))
                session.Fail("Boss death and dissolve did not complete; increase the scenario duration.");
            if (scenario == Scenario::PlayerRestart && !(session.GetSceneEpoch() >= 2 && !final.playerDead && final.playerHp > 0))
                session.Fail("Player death and scene restart did not complete; increase the scenario duration.");
            if ((scenario == Scenario::StageTransition || scenario == Scenario::ExpeditionTransition) &&
                world_.resources.gameplayScenarioTransitionStep_ != 5)
                session.Fail("The actual second combat room was not entered and presented; increase the scenario duration.");
            if (scenario == Scenario::ExpeditionTransition && world_.run.expeditionMapTestPurchases_ == 0)
                session.Fail("The actual expedition service purchase did not complete.");
        }
        // MainLoop still records its final draw before consuming WM_QUIT.
        // Reset the visual's frame arena at this render-only boundary too.
        world_.bossPresentation->UpdateNeonBossVisual(0.0f);
        if (gameplaytest::IsNeonDepthScenario(session.GetSettings().scenario))
            world_.depthValidation->FinishNeonDepthValidation();
        const bool completed = session.Finish();
        PostQuitMessage(completed ? 0 : 9);
        return true;
    }
    if (world_.resources.gameplayScenarioFinishDeferred_) {
        // SceneManager checks finished_ before calling the next Update. Let
        // the captured old scene survive through Resolve, then freeze this
        // render-only boundary before permitting its ordinary replacement.
        world_.resources.gameplayScenarioFinishDeferred_ = false;
        world_.combat.finished_ = true;
        world_.presentation.developerBloomFreeze_ = true;
        return false;
    }
    try {
        const auto& settings = session.GetSettings();
        const unsigned frame = session.GetFrame();
        if (settings.scenario == Scenario::PreviewLifecycle && !world_.resources.gameplayScenarioEventApplied_) {
            const auto result =
                NeonSkinnedPreview::ValidateResourceLifecycle(world_.resources.camera.get(), world_.resources.debugCamera.get(), 3);
            session.ReportDetail("previewLifecycle",
                                 {{"completed", result.passed},
                                  {"repeats", result.completedRepeats},
                                  {"baselineStable", result.passed && result.descriptorsAfterTeardown == result.sharedCacheBaseline},
                                  {"initialDescriptors", result.initialDescriptors},
                                  {"sharedCacheBaseline", result.sharedCacheBaseline},
                                  {"maxDescriptorsWhileLoaded", result.maxDescriptorsWhileLoaded},
                                  {"descriptorsAfterTeardown", result.descriptorsAfterTeardown},
                                  {"error", result.error}});
            world_.resources.gameplayScenarioEventApplied_ = true;
            RequireScenario(result.passed, "Actual Preview lifecycle: " + result.error);
        }
        if (frame >= 120 && !world_.resources.gameplayScenarioEventApplied_ && settings.scenario == Scenario::BossDeath) {
            world_.resources.enemy_->TakeDamage(static_cast<uint32_t>(world_.resources.enemy_->GetHp()));
            RequireScenario(world_.resources.enemy_->IsDead() && world_.resources.enemy_->GetHp() == 0,
                            "Scheduled boss lethal damage did not kill the real boss.");
            world_.resources.gameplayScenarioEventApplied_ = true;
        }
        if (frame >= 120 && session.GetSceneEpoch() == 1 && !world_.resources.gameplayScenarioEventApplied_ &&
            settings.scenario == Scenario::PlayerRestart) {
            world_.presentation.debugPlayerNoDamage_ = false;
            world_.resources.player_->SetDebugNoDamage(false);
            world_.resources.player_->TakeDamage(1000000u, 0.0f);
            RequireScenario(world_.resources.player_->IsDead() && world_.resources.player_->GetHp() == 0,
                            "Scheduled player lethal damage was rejected by the real actor.");
            world_.resources.gameplayScenarioEventApplied_ = true;
        }
        if (settings.scenario == Scenario::PlayerRestart && world_.resources.player_->IsDead() &&
            world_.combat.combatFlow_.GetState() == GameFlowState::GameOver && world_.combat.combatFlow_.GetTimer() <= 0 &&
            world_.combat.phase_ == Phase::kMain && !world_.resources.gameplayScenarioRestartRequested_) {
            world_.combat.resultSelection_ = 0;
            world_.combatFlow->ConfirmResultSelection();
            world_.resources.gameplayScenarioRestartRequested_ = true;
            session.ReportDetail("restart",
                                 {{"resultConfirmed", true}, {"globalFrame", frame}, {"nextScene", world_.combat.nextSceneName_}});
        }
        const bool expedition = settings.scenario == Scenario::ExpeditionTransition;
        if (expedition || settings.scenario == Scenario::StageTransition) {
            if (frame >= 120 && world_.resources.gameplayScenarioTransitionStep_ == 0) {
                // Invoke actual damage/callbacks; ordinary room completion,
                // credit flight and presentation transitions remain in Update.
                for (auto* actor : world_.resources.enemyManager_->GetEnemyPtrs())
                    if (actor && !actor->IsDead())
                        actor->TakeDamageFromPlayer(static_cast<uint32_t>(actor->GetHp()));
                world_.resources.gameplayScenarioTransitionStep_ = 1;
            }
            if (!world_.run.expeditionTransition_.IsActive()) {
                if (world_.resources.gameplayScenarioTransitionStep_ == 1 && world_.run.expeditionMapRun_.IsChoosing()) {
                    world_.expeditionMapController->RequestExpeditionMapNode(expedition ? "scenario_upgrade" : "scenario_b");
                    RequireScenario(world_.run.expeditionTransition_.IsActive(), "The actual next-node transition was rejected.");
                    world_.resources.gameplayScenarioTransitionStep_ = 2;
                } else if (expedition && world_.resources.gameplayScenarioTransitionStep_ == 2 &&
                           world_.run.expeditionMapRun_.GetActiveNodeId() == "scenario_upgrade") {
                    RequireScenario(!world_.run.expeditionServiceOffers_.empty(), "The actual authored shop has no eligible offer.");
                    world_.expeditionMapController->SelectExpeditionService(0);
                    RequireScenario(world_.run.expeditionTransition_.IsActive(), "The actual authored shop purchase was rejected.");
                    world_.resources.gameplayScenarioTransitionStep_ = 3;
                } else if (expedition && world_.resources.gameplayScenarioTransitionStep_ == 3 &&
                           world_.run.expeditionMapRun_.IsChoosing()) {
                    world_.expeditionMapController->RequestExpeditionMapNode("scenario_b");
                    RequireScenario(world_.run.expeditionTransition_.IsActive() && world_.run.expeditionMapTestPurchases_ >= 1,
                                    "The actual shop-to-combat transition failed.");
                    world_.resources.gameplayScenarioTransitionStep_ = 4;
                }
            }
        }
        auto scripted = gameplaytest::InputAtFrame(settings, frame);
        if (settings.input.empty()) {
            if (settings.scenario == Scenario::Melee && frame < 120)
                scripted.movement = {};
            if (!BossScenario(settings.scenario)) {
                if (const auto* nearest =
                        world_.resources.enemyManager_->FindNearestEnemy(world_.resources.player_->GetWorldPosition(), 10000.0f)) {
                    const auto aim = nearest->GetWorldPosition();
                    scripted.aim = {aim.x, aim.y, aim.z};
                }
            } else {
                const auto aim = world_.resources.enemy_->GetWorldPosition();
                scripted.aim = {aim.x, aim.y, aim.z};
            }
        }
        world_.resources.player_->SetDemoInput(true, {scripted.movement[0], scripted.movement[1]},
                                               {scripted.aim[0], scripted.aim[1], scripted.aim[2]}, scripted.shoot, scripted.dash);
        if (gameplaytest::IsNeonDepthScenario(settings.scenario))
            world_.depthValidation->PrepareNeonDepthValidation();
    }
    catch (const std::exception& error) {
        session.Fail(std::string("Scenario driver: ") + error.what());
        session.Finish();
        PostQuitMessage(9);
        return true;
    }
    return false;
}

gameplaytest::Snapshot GameplayScenarioRunner::MakeGameplayScenarioSnapshot() const
{
    gameplaytest::Snapshot snapshot;
    const auto& session = GameplayScenarioSession::Get();
    snapshot.frame = session.GetFrame();
    snapshot.sceneEpoch = session.GetSceneEpoch();
    snapshot.simulationTime = static_cast<float>(snapshot.frame) * session.GetSettings().fixedDeltaTime;
    const auto playerPosition = world_.resources.player_->GetWorldPosition(), bossPosition = world_.resources.enemy_->GetWorldPosition();
    snapshot.playerPosition = {playerPosition.x, playerPosition.y, playerPosition.z};
    snapshot.bossPosition = {bossPosition.x, bossPosition.y, bossPosition.z};
    snapshot.playerHp = world_.resources.player_->GetHp();
    snapshot.playerMaxHp = world_.resources.player_->GetMaxHp();
    snapshot.playerDead = world_.resources.player_->IsDead();
    snapshot.playerStyle = static_cast<int>(world_.resources.player_->GetExpeditionCombatStyle());
    snapshot.classId = world_.resources.player_->GetRunCombatSnapshot().classId;
    snapshot.primaryAttacks = world_.resources.player_->GetPrimaryAttackCount();
    const auto counts = world_.resources.bulletManager_->GetBulletCounts();
    snapshot.projectiles = static_cast<unsigned>(world_.resources.bulletManager_->GetBulletCount());
    snapshot.playerProjectiles = static_cast<unsigned>(counts.player);
    snapshot.enemyProjectiles = static_cast<unsigned>(counts.enemy + counts.hostileExpEnemy);
    snapshot.enemies = static_cast<unsigned>(world_.resources.enemyManager_->GetEnemyCount());
    for (const auto* drone : world_.resources.player_->GetDronePtrs())
        if (drone && !drone->IsDead())
            ++snapshot.activeDrones;
    snapshot.activeAttacks =
        static_cast<unsigned>(world_.presentation.playerLaserBeams_.size() + world_.presentation.playerMines_.size() +
                              world_.presentation.playerMineExplosions_.size() + world_.presentation.playerMeleeSlashes_.size());
    for (const auto* actor : world_.resources.enemyManager_->GetEnemyPtrs())
        if (actor && !actor->IsDead())
            snapshot.minimumEnemyHp = snapshot.minimumEnemyHp == -1 ? actor->GetHp() : (std::min)(snapshot.minimumEnemyHp, actor->GetHp());
    snapshot.bossActive = world_.gameplayQueries->IsRunRivalActive() && world_.resources.enemy_->IsRunEncounterEnabled();
    snapshot.bossDead = world_.resources.enemy_->IsDead();
    snapshot.bossHp = world_.resources.enemy_->GetHp();
    snapshot.bossEncounterGeneration = world_.resources.enemy_->GetEncounterGeneration();
    const auto rival = world_.resources.enemy_->GetRivalCombatStatus();
    snapshot.bossShots = world_.resources.enemy_->GetShotsFired();
    snapshot.bossDashes = rival.dashCount;
    snapshot.bossPhase =
        rival.enabled ? static_cast<unsigned>(rival.phase) : static_cast<unsigned>(world_.resources.enemy_->GetPrototypeCombatPhase());
    if (gameplaytest::IsNeonDepthScenario(session.GetSettings().scenario)) {
        const auto& depth = world_.resources.enemy_->GetNeonDepthSnapshot();
        snapshot.bossPhase = static_cast<unsigned>(depth.phase);
        snapshot.bossShots = static_cast<unsigned>(depth.activationEvents);
        snapshot.bossDashes = 0;
    }
    snapshot.descriptors = cg2::Object3dCommon::GetInstance()->GetSrvManager()->GetAllocatedCount();
    if (world_.resources.neonBossVisual_) {
        const auto& stats = world_.resources.neonBossVisual_->GetStats();
        snapshot.visualCreates = stats.resourceCreates;
        snapshot.visualReleases = stats.resourceReleases;
        snapshot.visualConstantBuffers = static_cast<unsigned>(world_.resources.neonBossVisual_->GetDrawConstantBufferCount());
        snapshot.visualHasResources = world_.resources.neonBossVisual_->HasResources();
        snapshot.dissolveProgress = world_.resources.neonBossVisual_->GetDissolveProgress();
    }
    snapshot.flow = static_cast<int>(world_.combat.combatFlow_.GetState());
    snapshot.expeditionPhase = static_cast<int>(world_.run.tankExpedition_.GetPhase());
    snapshot.visitedNodes = static_cast<unsigned>(world_.run.expeditionMapRun_.GetVisitedNodeIds().size());
    snapshot.nodeId = world_.run.expeditionMapRun_.GetActiveNodeId().empty() ? world_.run.expeditionMapRun_.GetCurrentNodeId()
                                                                             : world_.run.expeditionMapRun_.GetActiveNodeId();
    if (const auto* node = tankexp::FindMapNode(world_.run.expeditionMapRun_.GetDefinition(), snapshot.nodeId))
        snapshot.roomId = node->roomTemplate;
    return snapshot;
}

void GameplayScenarioRunner::RecordGameplayScenarioFrame()
{
    auto& session = GameplayScenarioSession::Get();
    if (!session.IsActive() || session.IsFinished() || world_.demo.titleDemo_ || !world_.resources.gameplayScenarioInitialized_ ||
        world_.presentation.developerBloomFreeze_)
        return;
    session.Record(MakeGameplayScenarioSnapshot(), world_.resources.gameplayScenarioCombatDt_,
                   world_.resources.gameplayScenarioPresentationDt_,
                   world_.resources.gameplayScenarioCombatDt_ / session.GetSettings().fixedDeltaTime);
    if (gameplaytest::IsNeonDepthScenario(session.GetSettings().scenario)) {
        try {
            world_.depthValidation->RecordNeonDepthValidation();
        }
        catch (const std::exception& error) {
            session.Fail(std::string("Depth frame evidence: ") + error.what());
        }
    }
    cg2::RuntimeProfiler::Get().SetCounter("Scenario simulation frame", session.GetFrame());
    const auto scenario = session.GetSettings().scenario;
    if ((scenario == Scenario::StageTransition || scenario == Scenario::ExpeditionTransition) &&
        world_.run.expeditionMapRun_.GetActiveNodeId() == "scenario_b" && world_.run.tankExpedition_.IsCombat() &&
        !world_.run.expeditionTransition_.IsActive())
        world_.resources.gameplayScenarioTransitionStep_ = 5; // Latch actual entry even if room B later clears.
    if (session.ShouldRecordCompletedFrame()) {
        char name[32]{};
        std::snprintf(name, sizeof(name), "frame_%05u", session.GetRecordingSequenceFrame());
        QueueGameplayScenarioCapture(name, true);
    }
    const auto& frames = session.GetSettings().captureFrames;
    if (std::find(frames.begin(), frames.end(), session.GetFrame()) != frames.end())
        QueueGameplayScenarioCapture("frame_" + std::to_string(session.GetFrame()));
    if (world_.combat.finished_ && world_.resources.gameplayScenarioCapturePending_) {
        if (world_.resources.gameplayScenarioCaptureRecording_)
            session.Fail("Scene exited during continuous recording.");
        world_.combat.finished_ = false;
        world_.resources.gameplayScenarioFinishDeferred_ = true;
    }
    if (session.ShouldFinish())
        session.ReportDetail("transitions", {{"step", world_.resources.gameplayScenarioTransitionStep_},
                                             {"servicePurchases", world_.run.expeditionMapTestPurchases_},
                                             {"visited", world_.run.expeditionMapRun_.GetVisitedNodeIds()},
                                             {"chosen", world_.run.expeditionMapRun_.GetChosenNodeIds()}});
}

void GameplayScenarioRunner::QueueGameplayScenarioCapture(const std::string& name, bool continuousRecording)
{
    auto& session = GameplayScenarioSession::Get();
    if (world_.resources.gameplayScenarioCapturePending_ || world_.resources.gameplayScenarioCapture_.IsBusy()) {
        session.Fail("Overlapping scenario capture requests.");
        return;
    }
    world_.resources.gameplayScenarioCaptureName_ = name;
    world_.resources.gameplayScenarioCaptureSnapshot_ = MakeGameplayScenarioSnapshot();
    world_.resources.gameplayScenarioCaptureRecording_ = continuousRecording;
    world_.resources.gameplayScenarioRecordingSequence_ = continuousRecording ? session.GetRecordingSequenceFrame() : 0;
    auto directory = tankexp::ExpeditionMapPath(session.GetOutputDirectory());
    if (continuousRecording)
        directory /= "recording/frames";
    world_.resources.gameplayScenarioCapture_.Request(directory, name, session.MakeMetadata());
    world_.resources.gameplayScenarioCapturePending_ = true;
    // Sparse comparisons retain the original policy. A continuous request is
    // pending readback only and never changes the standard temporal profile.
    if (!continuousRecording)
        world_.presentation.developerBloomFreeze_ = true;
}

void GameplayScenarioRunner::RecordGameplayScenarioCapture(cg2::DirectXCommon& dx)
{
    auto& session = GameplayScenarioSession::Get();
    if (!session.IsActive() || world_.demo.titleDemo_ || !world_.resources.gameplayScenarioCapturePending_)
        return;
    if (world_.resources.gameplayScenarioCapture_.HasRequest()) {
        auto metadata = session.MakeMetadata();
        metadata["snapshot"] = gameplaytest::ToJson(world_.resources.gameplayScenarioCaptureSnapshot_);
        metadata["camera"] = {
            {"position",
             {world_.resources.camera->GetTranslate().x, world_.resources.camera->GetTranslate().y,
              world_.resources.camera->GetTranslate().z}},
            {"rotation",
             {world_.resources.camera->GetRotate().x, world_.resources.camera->GetRotate().y, world_.resources.camera->GetRotate().z}}};
        for (const auto& row : world_.resources.camera->GetViewProjectionMatrix().m)
            metadata["camera"]["viewProjection"].push_back({row[0], row[1], row[2], row[3]});
        if (BossScenario(session.GetSettings().scenario) && world_.resources.neonBossVisual_)
            metadata["bossVisual"] = world_.bossPresentation->MakeNeonBossMetadata().at("presentation");
        metadata["comparisonFreeze"] = !world_.resources.gameplayScenarioCaptureRecording_;
        if (gameplaytest::IsNeonDepthScenario(session.GetSettings().scenario))
            metadata["depthConditions"] = world_.depthValidation->MakeNeonDepthValidationConditions();
        if (world_.resources.gameplayScenarioCaptureRecording_) {
            const auto recording = session.MakeRecordingMetadata();
            metadata["sequenceFrame"] = world_.resources.gameplayScenarioRecordingSequence_;
            metadata["sequenceRate"] = recording.at("sequenceRate");
            metadata["recordingClocks"] = recording.at("recordingClocks");
            metadata["heldDrawCount"] = 0;
            metadata["recordingConditions"] = {
                {"automatedInput", true},
                {"shortcutFixture", true},
                {"normalMainRoute", false},
                {"playerInvulnerable", world_.presentation.debugPlayerNoDamage_},
                {"playerHpConfigured", session.GetSettings().playerHp > 0},
                {"bossHpConfigured", true},
                {"configuredBossHp", session.GetSettings().bossHp},
                {"forcedDefeat", session.GetSettings().scenario == Scenario::BossDeath},
                {"cameraFixtureCentering", !gameplaytest::IsNeonDepthScenario(session.GetSettings().scenario) &&
                                               BossScenario(session.GetSettings().scenario) && world_.gameplayQueries->IsRunRivalActive()},
                {"debugCamera", cg2::Object3dCommon::GetInstance()->GetIsDebugCamera()},
                {"HUDIncluded", true},
                {"debugUIIncluded", false},
                {"audio", "none"},
                {"sceneTimeScale", world_.combat.timeScale_}};
            Microsoft::WRL::ComPtr<ID3D12Resource> backbuffer;
            const HRESULT hr = dx.GetSwapChain()->GetBuffer(dx.GetSwapChain()->GetCurrentBackBufferIndex(), IID_PPV_ARGS(&backbuffer));
            if (FAILED(hr)) {
                session.Fail("Recording backbuffer inspection failed.");
                world_.resources.gameplayScenarioCapture_.CancelRequest();
                return;
            }
            const auto desc = backbuffer->GetDesc();
            if (desc.Width > 8192 || !session.BeginRecording(static_cast<unsigned>(desc.Width), desc.Height)) {
                world_.resources.gameplayScenarioCapture_.CancelRequest();
                return;
            }
        }
        const auto frame = world_.gameplayQueries->MakeDeveloperGameCaptureMetadata(dx);
        for (const char* key : {"gpu", "queueTimestampFrequencyHz", "validation", "resolution", "globalPost", "localCategories",
                                "visualAppearance", "sourceResolution", "sourceCounts"})
            if (frame.contains(key))
                metadata[key] = frame.at(key);
        world_.resources.gameplayScenarioCapture_.SetFrameMetadata(std::move(metadata));
    }
    world_.resources.gameplayScenarioCapture_.Record(dx);
}
#endif

} // namespace gameplay
