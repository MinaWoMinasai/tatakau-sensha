#include "GameScene.h"
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "Enemy.h"
#include "PlayerDrone.h"
#include "RuntimeProfiler.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

namespace {
using gameplaytest::Scenario;
void RequireScenario(bool condition, const std::string& error) {
    if (!condition) throw std::runtime_error(error);
}
bool BossScenario(Scenario scenario) {
    return scenario == Scenario::RivalBoss || scenario == Scenario::PrototypeBoss ||
        scenario == Scenario::NeonBoss || scenario == Scenario::BossDeath || gameplaytest::IsNeonDepthScenario(scenario);
}

// Preserve authored walls. Only the in-memory wave is replaced; points are
// selected from the room's real free cells rather than putting actors in walls.
std::vector<tankexp::RoomPoint> ScenarioSpawnPoints(const tankexp::RoomDefinition& room, bool melee, bool boss) {
    std::vector<tankexp::RoomPoint> points;
    auto add = [&](tankexp::RoomPoint point) {
        if (!tankexp::IsRoomPointClear(room, point)) return;
        const float px = point.x - room.playerStart.x, py = point.y - room.playerStart.y;
        if (px * px + py * py < 4.0f) return;
        if (boss && !room.objectiveTargets.empty()) {
            const auto target = room.objectiveTargets.front();
            const float bx = point.x - target.x, by = point.y - target.y;
            if (bx * bx + by * by < 12.25f) return;
        }
        for (const auto previous : points) {
            const float dx = point.x - previous.x, dy = point.y - previous.y;
            if (dx * dx + dy * dy < 2.56f) return;
        }
        points.push_back(point);
    };
    if (melee) add({room.playerStart.x + 3.0f, room.playerStart.y});
    for (const auto& spawn : room.spawns) add({spawn.x, spawn.y});
    for (int row = tankexp::kRoomTop + 1; row < tankexp::kRoomBottom; ++row)
        for (int column = tankexp::kRoomLeft + 1; column < tankexp::kRoomRight; ++column)
            add(tankexp::RoomCellToWorld(column, row));
    return points;
}
}

void GameScene::InitializeGameplayScenario() {
    auto& session = GameplayScenarioSession::Get();
    if (!session.IsActive() || session.IsFinished() || titleDemo_) return;
    session.BeginScene();
    try {
        if (session.GetSettings().recording.enabled) {
            Microsoft::WRL::ComPtr<ID3D12Resource> backbuffer;
            // DirectX getters are nonconst; no device work/copy is submitted here.
            auto* writableDx = cg2::Object3dCommon::GetInstance()->GetDxCommon();
            const HRESULT hr = writableDx->GetSwapChain()->GetBuffer(writableDx->GetSwapChain()->GetCurrentBackBufferIndex(), IID_PPV_ARGS(&backbuffer));
            RequireScenario(SUCCEEDED(hr), "Could not inspect the native recording backbuffer.");
            const auto desc = backbuffer->GetDesc();
            RequireScenario(desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM && desc.SampleDesc.Count == 1 && desc.Width <= 8192 &&
                session.BeginRecording(static_cast<unsigned>(desc.Width), desc.Height), "Native recording preflight failed.");
        }
        RequireScenario(expeditionRun_ && expeditionMapEnabled_ && player_ && enemy_ && enemyManager_ && stage_ && bulletManager_,
            "Gameplay scenarios require the initialized TANK_EXPEDITION scene.");
        const auto& settings = session.GetSettings();
        const bool boss = BossScenario(settings.scenario);
        // 'arena' is a Developer alias for these existing authored rooms.
        const std::string roomId = settings.room == "arena" ? (boss ? "final_duel" : "outskirts") : settings.room;
        const auto* source = tankexp::FindRoom(expeditionRooms_, roomId);
        RequireScenario(source && source->objective == (boss ? "boss" : "eliminate"),
            "Scenario room is missing or has an incompatible objective: " + roomId);
        const auto* bossRoom = tankexp::FindRoom(expeditionRooms_, "final_duel");
        RequireScenario(bossRoom && bossRoom->objective == "boss", "The authored final_duel boss room is missing.");
        const auto* secondRoom = tankexp::FindRoom(expeditionRooms_, roomId == "crossfire" ? "outskirts" : "crossfire");
        RequireScenario(secondRoom && secondRoom->objective == "eliminate", "A second authored eliminate room is missing.");

        tankexp::MapDefinition fixture;
        fixture.startingCurrency = 1000;
        fixture.startNodes = {boss ? "scenario_boss" : "scenario_a"};
        if (boss) {
            fixture.nodes = {{"scenario_boss", "Scenario boss", tankexp::NodeKind::Boss, 0, 2, 0, roomId, 0, 0, {}}};
        } else {
            const bool expedition = settings.scenario == Scenario::ExpeditionTransition;
            const bool transition = expedition || settings.scenario == Scenario::StageTransition;
            fixture.nodes.push_back({"scenario_a", "Scenario combat A", tankexp::NodeKind::Combat, 0, 2, 0, roomId, 0, 0,
                {expedition ? "scenario_upgrade" : transition ? "scenario_b" : "scenario_boss"}});
            int column = 1;
            if (expedition) fixture.nodes.push_back({"scenario_upgrade", "Scenario workshop", tankexp::NodeKind::Upgrade,
                column++, 2, -1, "", 0, 0, {"scenario_b"}});
            if (transition) fixture.nodes.push_back({"scenario_b", "Scenario combat B", tankexp::NodeKind::Combat,
                column++, 2, 1, secondRoom->id, 0, 0, {"scenario_boss"}});
            fixture.nodes.push_back({"scenario_boss", "Scenario final boss", tankexp::NodeKind::Boss,
                column, 2, 2, bossRoom->id, 0, 0, {}});
        }
        std::string error;
        if (!expeditionMapRun_.Reset(fixture, error)) throw std::runtime_error("Invalid scenario map: " + error);
        // Existing map visuals borrow definition indices. Keep valid slots and
        // remove old edges at the frame boundary before the normal UI refresh.
        RequireScenario(expeditionMapVisuals_.size() >= fixture.nodes.size(), "The authored map has too few initialized visual slots.");
        expeditionMapVisuals_.resize(fixture.nodes.size());
        expeditionMapEdges_.clear();
        expeditionMapSelection_ = fixture.startNodes.front();
        expeditionMapScroll_ = 0.0f;
        expeditionBuildChoice_ = false; expeditionBuildChosen_ = true;
        expeditionGuideActive_ = false; tankExpeditionTutorial_.Skip(); tutorialConfig_.enabled = false;
        expeditionAuthoringHubOpen_ = false; tankRunPaused_ = false;
        expeditionRoomEditorOpen_ = expeditionMapEditorOpen_ = expeditionContentEditorOpen_ = false;
        tankExpeditionBalanceEditorOpen_ = tankExpeditionDetailsOpen_ = expeditionMapPreview_ = false;
        expeditionTransition_ = {}; expeditionTransitionAction_ = 0;
        expeditionCollectAll_ = false; combatFlow_.Reset();
        bossDefeatHandled_ = playerDeathHandled_ = false;
        tankRunMenuAge_ = 1.0f;
        tankExpeditionMusicEnabled_ = tankExpeditionEffectsEnabled_ = false;
        tankExpeditionAudio_.SetMusicVolume(0); tankExpeditionAudio_.SetEffectsVolume(0);

        tankrun::Config runConfig; runConfig.combatSeconds = 1000000.0f;
        tankRun_ = tankrun::RunDirector(settings.seed, runConfig);
        RequireScenario(tankRun_.ChooseLoadout(0) && tankRun_.ChooseCore(0), "Could not initialize scenario loadout.");
        ApplyTankRunCards(); player_->ConfigurePrototypeLoadout(0);
        int style = settings.playerStyle;
        if (style == -1) style = settings.scenario == Scenario::Drone ? 1 : settings.scenario == Scenario::Melee ? 2 : 0;
        expeditionBuildStyle_ = static_cast<tankbuild::Style>(style);
        RequireScenario(player_->SetExpeditionCombatStyle(expeditionBuildStyle_), "Could not apply the scenario player style.");
        expeditionPurchases_.clear(); expeditionPurchasedModules_.clear();
        for (const auto& id : settings.upgrades) {
            const auto* upgrade = tankcontent::FindUpgrade(expeditionContent_, id);
            RequireScenario(upgrade && tankcontent::EligibleUpgrade(*upgrade, expeditionBuildStyle_, tankRun_.GetCardCounts()),
                "Scenario upgrade is unknown, already equipped or incompatible: " + id);
            RequireScenario(tankRun_.GrantExpeditionModules(upgrade->effects), "Could not grant scenario upgrade: " + id);
            expeditionPurchases_[id] = 1; expeditionPurchasedModules_[id] = *upgrade;
        }
        ApplyTankRunCards();
        tankExpedition_.Reset(); tankExpedition_.OpenMap();
        EnterExpeditionMapNode(fixture.startNodes.front());
        RequireScenario(expeditionMapRun_.GetActiveNodeId() == fixture.startNodes.front() && tankExpedition_.IsCombat(),
            "The actual authored scenario room could not be entered: " + expeditionMapStatus_);

        enemyManager_->ClearRunActors();
        if (!settings.enemyWave.empty()) {
            for (const auto& wave : settings.enemyWave) {
                const cg2::Vector3 position{wave.position[0], wave.position[1], wave.position[2]};
                RequireScenario(tankcontent::FindEnemy(expeditionContent_, wave.type) &&
                    !stage_->IsCollisionWithAnyBlock(position, 0.8f) &&
                    enemyManager_->SpawnLevelEnemy(position, wave.type, wave.hp), "Could not spawn configured enemy: " + wave.type);
            }
        } else {
            const auto points = ScenarioSpawnPoints(*source, settings.scenario == Scenario::Melee, boss);
            RequireScenario(points.size() >= settings.enemyCount, "Not enough valid free cells for the configured wave.");
            const std::array<const char*,3> fallbackTypes{"Charger", "Skirmisher", "Sniper"};
            for (unsigned index = 0; index < settings.enemyCount; ++index) {
                const std::string type = source->spawns.empty() ? fallbackTypes[index % fallbackTypes.size()] : source->spawns[index % source->spawns.size()].type;
                const auto point = points[index];
                RequireScenario(tankcontent::FindEnemy(expeditionContent_, type) &&
                    enemyManager_->SpawnLevelEnemy({point.x,point.y,0}, type, 50000), "Could not spawn scenario enemy: " + type);
            }
        }
        tankExpeditionSpawned_ = static_cast<int>(enemyManager_->GetEnemyCount());
        if (boss) {
            const auto target = source->objectiveTargets.front();
            enemy_->ResetRunEncounter({target.x,target.y,0}, settings.bossHp, 1, true);
            enemy_->EnableExpeditionRival(settings.scenario != Scenario::PrototypeBoss);
            if (gameplaytest::IsNeonDepthScenario(settings.scenario)) ConfigureNeonDepthEncounter(true);
            else enemy_->EnableNeonDepthEncounter(false);
            auto progress = enemy_->GetEnemyProgressConfig(); progress.levelingModeEnabled = false;
            enemy_->SetEnemyProgressConfig(progress);
        }
        neonBossVisualEnabled_ = settings.scenario == Scenario::NeonBoss || settings.scenario == Scenario::BossDeath || gameplaytest::IsNeonDepthScenario(settings.scenario);
        if (neonBossVisual_) neonBossVisual_->SetEnabled(neonBossVisualEnabled_);
        RequireScenario(settings.playerHp != 0, "Initial playerHp=0 is unsupported by the living room-reset contract; use player_restart for real lethal damage.");
        if (settings.playerHp > 0) {
            RequireScenario(settings.playerHp <= player_->GetMaxHp(), "Configured playerHp exceeds this player's actual maximum HP.");
            if (settings.playerHp < player_->GetHp())
                RequireScenario(player_->SpendRunHealth(player_->GetHp() - settings.playerHp), "Could not apply configured player HP.");
            else player_->HealRunPlayer(settings.playerHp - player_->GetHp());
        }
        debugPlayerNoDamage_ = true; player_->SetDebugNoDamage(true);
        previousPlayerHp_ = player_->GetHp(); previousBossHp_ = enemy_->GetHp();
        player_->SetDemoInput(true, {}, boss ? enemy_->GetWorldPosition() : cg2::Vector3{50,29,0}, false, false);
        for (unsigned index = 0; index < settings.initialProjectiles; ++index) {
            const float angle = static_cast<float>(index) * 0.618034f;
            const float radius = 2.0f + static_cast<float>(index % 12) * 0.30f;
            const auto point = source->playerStart;
            cg2::Vector3 position{point.x + 8.0f + std::cos(angle) * radius, point.y + std::sin(angle) * radius, 0};
            if (stage_->IsCollisionWithAnyBlock(position, 0.5f)) position = player_->GetWorldPosition();
            auto projectile = std::make_unique<Bullet>();
            const auto owner = index < BulletManager::kMaxRunProjectilesPerOwner ? kPlayer : kEnemy;
            projectile->Initialize(position, {std::cos(angle)*0.08f,std::sin(angle)*0.08f,0}, 1, owner, true);
            projectile->ConfigureGrowth(4, 8, 0);
            bulletManager_->Add(std::move(projectile));
        }
        RequireScenario(bulletManager_->GetBulletCount() == settings.initialProjectiles,
            "The configured initial projectile count exceeds the real manager's owner cap.");
        if (gameplaytest::IsNeonDepthScenario(settings.scenario)) InitializeNeonDepthValidation();
        gameplayScenarioInitialized_ = true;
        session.ReportDetail("fixture", {{"authoredRoom",roomId},{"geometryPreserved",true},{"playerInvulnerable",debugPlayerNoDamage_},
            {"upgrades",settings.upgrades},{"enemyCount",enemyManager_->GetEnemyCount()},
            {"initialProjectiles",bulletManager_->GetBulletCount()},{"sceneEpoch",session.GetSceneEpoch()}});
        RefreshTankExpeditionUi();
        if (session.GetFrame() == 0 &&
            std::find(settings.captureFrames.begin(), settings.captureFrames.end(), 0u) != settings.captureFrames.end())
            QueueGameplayScenarioCapture("frame_0");
    } catch (const std::exception& error) {
        session.Fail(std::string("Scenario initialization: ") + error.what());
        session.Finish(); PostQuitMessage(9);
    }
}

bool GameScene::PrepareGameplayScenarioFrame() {
    auto& session = GameplayScenarioSession::Get();
    if (!session.IsActive() || titleDemo_) return false;
    if (session.IsFinished()) return true;
    if (session.GetSettings().recording.enabled &&
        ((developerBloomFreeze_ && (!gameplayScenarioCapturePending_ || gameplayScenarioCaptureRecording_)) ||
            neonBossDeveloperFreeze_ || IsNeonShowcaseActive())) {
        session.Fail("Continuous recording cannot use comparison freeze or Showcase.");
        UpdateNeonBossVisual(0.0f); session.Finish(); PostQuitMessage(9); return true;
    }
    gameplayScenarioCapture_.Resolve(*cg2::Object3dCommon::GetInstance()->GetDxCommon());
    if (gameplayScenarioCapturePending_) {
        if (gameplayScenarioCapture_.IsBusy()) {
            if (gameplayScenarioCaptureRecording_) {
                // PostDraw already waits for this frame fence. Never introduce
                // an extra TAA/history draw into a successful continuous run.
                session.Fail("Recording fence did not resolve on the next Update; additional held Draw rejected.");
                UpdateNeonBossVisual(0.0f); session.Finish(); PostQuitMessage(9); return true;
            }
            // The existing freeze branch still resets renderer frame arenas.
            developerBloomFreeze_ = true;
            return false;
        }
        if (gameplayScenarioCapture_.WasLastCaptureSuccessful()) {
            if (gameplayScenarioCaptureRecording_) {
                try {
                    const auto directory = tankexp::ExpeditionMapPath(session.GetOutputDirectory()) / "recording/frames";
                    session.ReportRecordedFrame(gameplayScenarioRecordingSequence_, gameplayScenarioCaptureName_, gameplayScenarioCaptureSnapshot_,
                        std::filesystem::file_size(directory / (gameplayScenarioCaptureName_ + ".png")),
                        std::filesystem::file_size(directory / (gameplayScenarioCaptureName_ + ".json")));
                } catch (const std::exception& error) { session.Fail(std::string("Recording save validation: ") + error.what()); }
            } else session.ReportCapture(gameplayScenarioCaptureName_, gameplayScenarioCaptureSnapshot_);
        } else session.Fail("Scenario capture failed: " + gameplayScenarioCapture_.GetStatus());
        gameplayScenarioCapturePending_ = false;
        if (!gameplayScenarioCaptureRecording_) developerBloomFreeze_ = false;
        gameplayScenarioCaptureRecording_ = false;
    }
    if (session.ShouldFinish() || !gameplayScenarioInitialized_) {
        if (session.GetErrors().empty() && session.GetFrame() == session.GetSettings().frames) {
            const auto final = MakeGameplayScenarioSnapshot();
            const auto scenario = session.GetSettings().scenario;
            if (scenario == Scenario::BossDeath && !(final.bossDead && final.bossHp == 0 &&
                final.visualCreates == 1 && final.visualReleases == 1 && final.dissolveProgress == 1 &&
                !final.visualHasResources && final.visualConstantBuffers == 0))
                session.Fail("Boss death and dissolve did not complete; increase the scenario duration.");
            if (scenario == Scenario::PlayerRestart && !(session.GetSceneEpoch() >= 2 && !final.playerDead && final.playerHp > 0))
                session.Fail("Player death and scene restart did not complete; increase the scenario duration.");
            if ((scenario == Scenario::StageTransition || scenario == Scenario::ExpeditionTransition) &&
                gameplayScenarioTransitionStep_ != 5)
                session.Fail("The actual second combat room was not entered and presented; increase the scenario duration.");
            if (scenario == Scenario::ExpeditionTransition && expeditionMapTestPurchases_ == 0)
                session.Fail("The actual expedition service purchase did not complete.");
        }
        // MainLoop still records its final draw before consuming WM_QUIT.
        // Reset the visual's frame arena at this render-only boundary too.
        UpdateNeonBossVisual(0.0f);
        if (gameplaytest::IsNeonDepthScenario(session.GetSettings().scenario)) FinishNeonDepthValidation();
        const bool completed = session.Finish(); PostQuitMessage(completed ? 0 : 9); return true;
    }
    if (gameplayScenarioFinishDeferred_) {
        // SceneManager checks finished_ before calling the next Update. Let
        // the captured old scene survive through Resolve, then freeze this
        // render-only boundary before permitting its ordinary replacement.
        gameplayScenarioFinishDeferred_ = false; finished_ = true;
        developerBloomFreeze_ = true;
        return false;
    }
    try {
        const auto& settings = session.GetSettings();
        const unsigned frame = session.GetFrame();
        if (settings.scenario == Scenario::PreviewLifecycle && !gameplayScenarioEventApplied_) {
            const auto result = NeonSkinnedPreview::ValidateResourceLifecycle(camera.get(), debugCamera.get(), 3);
            session.ReportDetail("previewLifecycle", {{"completed",result.passed},{"repeats",result.completedRepeats},
                {"baselineStable",result.passed && result.descriptorsAfterTeardown == result.sharedCacheBaseline},
                {"initialDescriptors",result.initialDescriptors},{"sharedCacheBaseline",result.sharedCacheBaseline},
                {"maxDescriptorsWhileLoaded",result.maxDescriptorsWhileLoaded},
                {"descriptorsAfterTeardown",result.descriptorsAfterTeardown},{"error",result.error}});
            gameplayScenarioEventApplied_ = true;
            RequireScenario(result.passed, "Actual Preview lifecycle: " + result.error);
        }
        if (frame >= 120 && !gameplayScenarioEventApplied_ && settings.scenario == Scenario::BossDeath) {
            enemy_->TakeDamage(static_cast<uint32_t>(enemy_->GetHp()));
            RequireScenario(enemy_->IsDead() && enemy_->GetHp() == 0, "Scheduled boss lethal damage did not kill the real boss.");
            gameplayScenarioEventApplied_ = true;
        }
        if (frame >= 120 && session.GetSceneEpoch() == 1 && !gameplayScenarioEventApplied_ && settings.scenario == Scenario::PlayerRestart) {
            debugPlayerNoDamage_ = false; player_->SetDebugNoDamage(false);
            player_->TakeDamage(1000000u, 0.0f);
            RequireScenario(player_->IsDead() && player_->GetHp() == 0, "Scheduled player lethal damage was rejected by the real actor.");
            gameplayScenarioEventApplied_ = true;
        }
        if (settings.scenario == Scenario::PlayerRestart && player_->IsDead() &&
            combatFlow_.GetState() == GameFlowState::GameOver && combatFlow_.GetTimer() <= 0 &&
            phase_ == Phase::kMain && !gameplayScenarioRestartRequested_) {
            resultSelection_ = 0; ConfirmResultSelection();
            gameplayScenarioRestartRequested_ = true;
            session.ReportDetail("restart", {{"resultConfirmed",true},{"globalFrame",frame},{"nextScene",nextSceneName_}});
        }
        const bool expedition = settings.scenario == Scenario::ExpeditionTransition;
        if (expedition || settings.scenario == Scenario::StageTransition) {
            if (frame >= 120 && gameplayScenarioTransitionStep_ == 0) {
                // Invoke actual damage/callbacks; ordinary room completion,
                // credit flight and presentation transitions remain in Update.
                for (auto* actor : enemyManager_->GetEnemyPtrs()) if (actor && !actor->IsDead())
                    actor->TakeDamageFromPlayer(static_cast<uint32_t>(actor->GetHp()));
                gameplayScenarioTransitionStep_ = 1;
            }
            if (!expeditionTransition_.IsActive()) {
                if (gameplayScenarioTransitionStep_ == 1 && expeditionMapRun_.IsChoosing()) {
                    RequestExpeditionMapNode(expedition ? "scenario_upgrade" : "scenario_b");
                    RequireScenario(expeditionTransition_.IsActive(), "The actual next-node transition was rejected.");
                    gameplayScenarioTransitionStep_ = 2;
                } else if (expedition && gameplayScenarioTransitionStep_ == 2 && expeditionMapRun_.GetActiveNodeId() == "scenario_upgrade") {
                    RequireScenario(!expeditionServiceOffers_.empty(), "The actual authored shop has no eligible offer.");
                    SelectExpeditionService(0);
                    RequireScenario(expeditionTransition_.IsActive(), "The actual authored shop purchase was rejected.");
                    gameplayScenarioTransitionStep_ = 3;
                } else if (expedition && gameplayScenarioTransitionStep_ == 3 && expeditionMapRun_.IsChoosing()) {
                    RequestExpeditionMapNode("scenario_b");
                    RequireScenario(expeditionTransition_.IsActive() && expeditionMapTestPurchases_ >= 1, "The actual shop-to-combat transition failed.");
                    gameplayScenarioTransitionStep_ = 4;
                }
            }
        }
        auto scripted = gameplaytest::InputAtFrame(settings, frame);
        if (settings.input.empty()) {
            if (settings.scenario == Scenario::Melee && frame < 120) scripted.movement = {};
            if (!BossScenario(settings.scenario)) {
                if (const auto* nearest = enemyManager_->FindNearestEnemy(player_->GetWorldPosition(), 10000.0f)) {
                    const auto aim = nearest->GetWorldPosition(); scripted.aim = {aim.x,aim.y,aim.z};
                }
            } else {
                const auto aim = enemy_->GetWorldPosition(); scripted.aim = {aim.x,aim.y,aim.z};
            }
        }
        player_->SetDemoInput(true, {scripted.movement[0],scripted.movement[1]},
            {scripted.aim[0],scripted.aim[1],scripted.aim[2]}, scripted.shoot, scripted.dash);
        if (gameplaytest::IsNeonDepthScenario(settings.scenario)) PrepareNeonDepthValidation();
    } catch (const std::exception& error) {
        session.Fail(std::string("Scenario driver: ") + error.what());
        session.Finish(); PostQuitMessage(9); return true;
    }
    return false;
}

gameplaytest::Snapshot GameScene::MakeGameplayScenarioSnapshot() const {
    gameplaytest::Snapshot snapshot;
    const auto& session = GameplayScenarioSession::Get();
    snapshot.frame = session.GetFrame(); snapshot.sceneEpoch = session.GetSceneEpoch();
    snapshot.simulationTime = static_cast<float>(snapshot.frame) * session.GetSettings().fixedDeltaTime;
    const auto playerPosition = player_->GetWorldPosition(), bossPosition = enemy_->GetWorldPosition();
    snapshot.playerPosition = {playerPosition.x,playerPosition.y,playerPosition.z};
    snapshot.bossPosition = {bossPosition.x,bossPosition.y,bossPosition.z};
    snapshot.playerHp = player_->GetHp(); snapshot.playerMaxHp = player_->GetMaxHp(); snapshot.playerDead = player_->IsDead();
    snapshot.playerStyle = static_cast<int>(player_->GetExpeditionCombatStyle());
    snapshot.classId = player_->GetRunCombatSnapshot().classId;
    snapshot.primaryAttacks = player_->GetPrimaryAttackCount();
    const auto counts = bulletManager_->GetBulletCounts();
    snapshot.projectiles = static_cast<unsigned>(bulletManager_->GetBulletCount());
    snapshot.playerProjectiles = static_cast<unsigned>(counts.player);
    snapshot.enemyProjectiles = static_cast<unsigned>(counts.enemy + counts.hostileExpEnemy);
    snapshot.enemies = static_cast<unsigned>(enemyManager_->GetEnemyCount());
    for (const auto* drone : player_->GetDronePtrs()) if (drone && !drone->IsDead()) ++snapshot.activeDrones;
    snapshot.activeAttacks = static_cast<unsigned>(playerLaserBeams_.size() + playerMines_.size() + playerMineExplosions_.size() + playerMeleeSlashes_.size());
    for (const auto* actor : enemyManager_->GetEnemyPtrs()) if (actor && !actor->IsDead())
        snapshot.minimumEnemyHp = snapshot.minimumEnemyHp == -1 ? actor->GetHp() : (std::min)(snapshot.minimumEnemyHp, actor->GetHp());
    snapshot.bossActive = IsRunRivalActive() && enemy_->IsRunEncounterEnabled();
    snapshot.bossDead = enemy_->IsDead(); snapshot.bossHp = enemy_->GetHp();
    snapshot.bossEncounterGeneration = enemy_->GetEncounterGeneration();
    const auto rival = enemy_->GetRivalCombatStatus();
    snapshot.bossShots = enemy_->GetShotsFired(); snapshot.bossDashes = rival.dashCount;
    snapshot.bossPhase = rival.enabled ? static_cast<unsigned>(rival.phase) : static_cast<unsigned>(enemy_->GetPrototypeCombatPhase());
    if (gameplaytest::IsNeonDepthScenario(session.GetSettings().scenario)) {
        const auto& depth = enemy_->GetNeonDepthSnapshot();
        snapshot.bossPhase = static_cast<unsigned>(depth.phase);
        snapshot.bossShots = static_cast<unsigned>(depth.activationEvents);
        snapshot.bossDashes = 0;
    }
    snapshot.descriptors = cg2::Object3dCommon::GetInstance()->GetSrvManager()->GetAllocatedCount();
    if (neonBossVisual_) {
        const auto& stats = neonBossVisual_->GetStats();
        snapshot.visualCreates = stats.resourceCreates; snapshot.visualReleases = stats.resourceReleases;
        snapshot.visualConstantBuffers = static_cast<unsigned>(neonBossVisual_->GetDrawConstantBufferCount());
        snapshot.visualHasResources = neonBossVisual_->HasResources(); snapshot.dissolveProgress = neonBossVisual_->GetDissolveProgress();
    }
    snapshot.flow = static_cast<int>(combatFlow_.GetState()); snapshot.expeditionPhase = static_cast<int>(tankExpedition_.GetPhase());
    snapshot.visitedNodes = static_cast<unsigned>(expeditionMapRun_.GetVisitedNodeIds().size());
    snapshot.nodeId = expeditionMapRun_.GetActiveNodeId().empty() ? expeditionMapRun_.GetCurrentNodeId() : expeditionMapRun_.GetActiveNodeId();
    if (const auto* node = tankexp::FindMapNode(expeditionMapRun_.GetDefinition(), snapshot.nodeId)) snapshot.roomId = node->roomTemplate;
    return snapshot;
}

void GameScene::RecordGameplayScenarioFrame() {
    auto& session = GameplayScenarioSession::Get();
    if (!session.IsActive() || session.IsFinished() || titleDemo_ || !gameplayScenarioInitialized_ || developerBloomFreeze_) return;
    session.Record(MakeGameplayScenarioSnapshot(), gameplayScenarioCombatDt_, gameplayScenarioPresentationDt_,
        gameplayScenarioCombatDt_ / session.GetSettings().fixedDeltaTime);
    if (gameplaytest::IsNeonDepthScenario(session.GetSettings().scenario)) {
        try { RecordNeonDepthValidation(); }
        catch (const std::exception& error) { session.Fail(std::string("Depth frame evidence: ")+error.what()); }
    }
    cg2::RuntimeProfiler::Get().SetCounter("Scenario simulation frame", session.GetFrame());
    const auto scenario = session.GetSettings().scenario;
    if ((scenario == Scenario::StageTransition || scenario == Scenario::ExpeditionTransition) &&
        expeditionMapRun_.GetActiveNodeId() == "scenario_b" && tankExpedition_.IsCombat() && !expeditionTransition_.IsActive())
        gameplayScenarioTransitionStep_ = 5; // Latch actual entry even if room B later clears.
    if (session.ShouldRecordCompletedFrame()) {
        char name[32]{};
        std::snprintf(name, sizeof(name), "frame_%05u", session.GetRecordingSequenceFrame());
        QueueGameplayScenarioCapture(name, true);
    }
    const auto& frames = session.GetSettings().captureFrames;
    if (std::find(frames.begin(), frames.end(), session.GetFrame()) != frames.end())
        QueueGameplayScenarioCapture("frame_" + std::to_string(session.GetFrame()));
    if (finished_ && gameplayScenarioCapturePending_) {
        if (gameplayScenarioCaptureRecording_) session.Fail("Scene exited during continuous recording.");
        finished_ = false; gameplayScenarioFinishDeferred_ = true;
    }
    if (session.ShouldFinish()) session.ReportDetail("transitions", {{"step",gameplayScenarioTransitionStep_},
        {"servicePurchases",expeditionMapTestPurchases_},{"visited",expeditionMapRun_.GetVisitedNodeIds()},
        {"chosen",expeditionMapRun_.GetChosenNodeIds()}});
}

void GameScene::QueueGameplayScenarioCapture(const std::string& name, bool continuousRecording) {
    auto& session = GameplayScenarioSession::Get();
    if (gameplayScenarioCapturePending_ || gameplayScenarioCapture_.IsBusy()) { session.Fail("Overlapping scenario capture requests."); return; }
    gameplayScenarioCaptureName_ = name;
    gameplayScenarioCaptureSnapshot_ = MakeGameplayScenarioSnapshot();
    gameplayScenarioCaptureRecording_ = continuousRecording;
    gameplayScenarioRecordingSequence_ = continuousRecording ? session.GetRecordingSequenceFrame() : 0;
    auto directory = tankexp::ExpeditionMapPath(session.GetOutputDirectory());
    if (continuousRecording) directory /= "recording/frames";
    gameplayScenarioCapture_.Request(directory, name, session.MakeMetadata());
    gameplayScenarioCapturePending_ = true;
    // Sparse comparisons retain the original policy. A continuous request is
    // pending readback only and never changes the standard temporal profile.
    if (!continuousRecording) developerBloomFreeze_ = true;
}

void GameScene::RecordGameplayScenarioCapture(cg2::DirectXCommon& dx) {
    auto& session = GameplayScenarioSession::Get();
    if (!session.IsActive() || titleDemo_ || !gameplayScenarioCapturePending_) return;
    if (gameplayScenarioCapture_.HasRequest()) {
        auto metadata = session.MakeMetadata();
        metadata["snapshot"] = gameplaytest::ToJson(gameplayScenarioCaptureSnapshot_);
        metadata["camera"] = {{"position",{camera->GetTranslate().x,camera->GetTranslate().y,camera->GetTranslate().z}},
            {"rotation",{camera->GetRotate().x,camera->GetRotate().y,camera->GetRotate().z}}};
        for (const auto& row : camera->GetViewProjectionMatrix().m)
            metadata["camera"]["viewProjection"].push_back({row[0],row[1],row[2],row[3]});
        if (BossScenario(session.GetSettings().scenario) && neonBossVisual_)
            metadata["bossVisual"] = MakeNeonBossMetadata().at("presentation");
        metadata["comparisonFreeze"] = !gameplayScenarioCaptureRecording_;
        if (gameplaytest::IsNeonDepthScenario(session.GetSettings().scenario)) metadata["depthConditions"] = MakeNeonDepthValidationConditions();
        if (gameplayScenarioCaptureRecording_) {
            const auto recording = session.MakeRecordingMetadata();
            metadata["sequenceFrame"] = gameplayScenarioRecordingSequence_;
            metadata["sequenceRate"] = recording.at("sequenceRate");
            metadata["recordingClocks"] = recording.at("recordingClocks");
            metadata["heldDrawCount"] = 0;
            metadata["recordingConditions"] = {{"automatedInput",true},{"shortcutFixture",true},{"normalMainRoute",false},
                {"playerInvulnerable",debugPlayerNoDamage_},{"playerHpConfigured",session.GetSettings().playerHp > 0},
                {"bossHpConfigured",true},{"configuredBossHp",session.GetSettings().bossHp},
                {"forcedDefeat",session.GetSettings().scenario == Scenario::BossDeath},
                {"cameraFixtureCentering",!gameplaytest::IsNeonDepthScenario(session.GetSettings().scenario) && BossScenario(session.GetSettings().scenario) && IsRunRivalActive()},
                {"debugCamera",cg2::Object3dCommon::GetInstance()->GetIsDebugCamera()},
                {"HUDIncluded",true},{"debugUIIncluded",false},{"audio","none"},{"sceneTimeScale",timeScale_}};
            Microsoft::WRL::ComPtr<ID3D12Resource> backbuffer;
            const HRESULT hr = dx.GetSwapChain()->GetBuffer(dx.GetSwapChain()->GetCurrentBackBufferIndex(), IID_PPV_ARGS(&backbuffer));
            if (FAILED(hr)) { session.Fail("Recording backbuffer inspection failed."); gameplayScenarioCapture_.CancelRequest(); return; }
            const auto desc = backbuffer->GetDesc();
            if (desc.Width > 8192 || !session.BeginRecording(static_cast<unsigned>(desc.Width), desc.Height)) {
                gameplayScenarioCapture_.CancelRequest(); return;
            }
        }
        const auto frame = MakeDeveloperGameCaptureMetadata(dx);
        for (const char* key : {"gpu","queueTimestampFrequencyHz","validation","resolution","globalPost"})
            if (frame.contains(key)) metadata[key] = frame.at(key);
        gameplayScenarioCapture_.SetFrameMetadata(std::move(metadata));
    }
    gameplayScenarioCapture_.Record(dx);
}
#endif
