// Boss encounter policy, scoped camera and scene-owned presentation integration.
#include "GameScene.h"
#include "Enemy.h"
#include "Player.h"
#include "RuntimeProfiler.h"
#include <fstream>

bool GameScene::LoadNeonDepthConfig(bool hotReload) {
    neondepth::Config candidate;
    std::string error;
    try {
        const std::filesystem::path path("resources/configs/neonBossDepth.json");
        if (std::filesystem::exists(path)) {
            if (!std::filesystem::is_regular_file(path) || std::filesystem::file_size(path) > 64 * 1024)
                throw std::runtime_error("Depth settings must be a regular file of at most 64 KiB.");
            std::ifstream stream(path, std::ios::binary);
            if (!stream) throw std::runtime_error("Could not open Depth settings.");
            const auto document = nlohmann::json::parse(stream);
            if (!neondepth::ReadConfig(document,candidate,error)) throw std::runtime_error(error);
        }
        // The camera/enabled choice is latched only on a fresh encounter. A
        // valid active edit queues attack geometry/timing and visual placement
        // for the next instance, preserving the already announced plan.
        if (hotReload && IsNeonDepthEncounterActive()) {
            if (!enemy_->QueueNeonDepthTuning(candidate.combat) ||
                (neonBossVisual_ && !neonBossVisual_->SetDepthProfile(candidate.visual)))
                throw std::runtime_error("Validated Depth edit could not be queued.");
        }
        neonDepthPendingConfig_ = candidate;
        if (!hotReload || !IsNeonDepthEncounterActive()) neonDepthConfig_ = candidate;
        neonDepthConfigStatus_ = hotReload ? "Depth settings queued for the next attack / encounter." : "Depth settings ready.";
        return true;
    } catch (const std::exception& exception) {
        neonDepthConfigStatus_ = std::string(exception.what()).substr(0,256);
        OutputDebugStringA((std::string("[NeonDepth] ")+neonDepthConfigStatus_+"\n").c_str());
        return false; // Preserve the previous valid profile (or initial defaults).
    }
}

bool GameScene::IsNeonDepthEncounterActive() const {
    return !titleDemo_ && enemy_ && enemy_->IsNeonDepthEncounterEnabled() &&
        enemy_->IsRunEncounterEnabled() && IsRunRivalActive() &&
        enemy_->GetNeonDepthSnapshot().phase != neondepth::Phase::Aborted;
}

void GameScene::ConfigureNeonDepthEncounter([[maybe_unused]] bool completedFixtureRoomReset) {
    if (!enemy_ || !expeditionRun_ || tankExpedition_.GetRoomKind()!=tankexp::RoomKind::Boss) return;
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    const auto& session = GameplayScenarioSession::Get();
    if (neonBossAutoTest_ || (session.IsActive() &&
        (!gameplaytest::IsNeonDepthScenario(session.GetSettings().scenario) || !completedFixtureRoomReset))) return;
#endif
    neonDepthConfig_ = neonDepthPendingConfig_;
    if (!neonDepthConfig_.enabled) return;
    // Old explicitly selected Developer fixture profiles are preserved by the
    // fixture adapter. Ordinary authored / sequential boss entry selects Depth.
    if (!neonDepthEffects_) neonDepthEffects_ = std::make_unique<NeonDepthEffects>();
    auto* dx = cg2::Object3dCommon::GetInstance()->GetDxCommon();
    if (!dx->InitializeTrailSceneNoDepthPipeline() || !neonDepthEffects_->Initialize(dx)) {
        neonDepthConfigStatus_ = "Depth floor cues could not initialize; encounter retains the existing safe policy.";
        OutputDebugStringA((neonDepthConfigStatus_+"\n").c_str());
        return;
    }
    enemy_->EnableNeonDepthEncounter(true,neonDepthConfig_.combat);
    if (!enemy_->IsNeonDepthEncounterEnabled()) return;
    if (neonBossVisual_) neonBossVisual_->SetDepthProfile(neonDepthConfig_.visual);
    if (neonDepthEffects_) neonDepthEffects_->ResetEncounter();
    SetEventCallout("床の投影コアを攻撃 / Enterで登場をスキップ",2.4f);
}

bool GameScene::UpdateNeonDepthCamera(float baseDeltaTime) {
    const bool wantScope = IsNeonDepthEncounterActive() &&
        (combatFlow_.GetState()==GameFlowState::Playing || combatFlow_.GetState()==GameFlowState::BossDefeatSequence);
    if (!wantScope) { RestoreNeonDepthCamera(); return false; }
    if (!neonDepthCameraScoped_) {
        neonDepthSavedCamera_ = {camera->GetTranslate(),camera->GetRotate(),camera->GetProjectionJitter(),
            camera->GetFovY(),camera->GetAspectRatio(),camera->GetNearClip(),camera->GetFarClip()};
        neonDepthCameraScoped_ = true;
        neonDepthSavedDebugCamera_ = cg2::Object3dCommon::GetInstance()->GetIsDebugCamera();
    }
    cg2::Object3dCommon::GetInstance()->SetIsDebugCamera(false);
    neondepth::CameraFrame frame;
    auto profile = neonDepthConfig_.camera;
    profile.aspect = float(cg2::WinApp::kClientWidth)/float(cg2::WinApp::kClientHeight);
    // Authored expedition rooms share this world-space frame. Cover the entire
    // room interior and permitted evasive movement, rather than following the
    // decorative head or enlarging only a screen-space projection.
    const bool reducedMotion = neonBossVisual_ ? neonBossVisual_->GetDepthProfile().reducedMotion : neonDepthConfig_.visual.reducedMotion;
    cg2::Vector3 focus{44,31,0};
    if (cameraShakeTimer_ > 0 && !reducedMotion) {
        const float dt = std::isfinite(baseDeltaTime)&&baseDeltaTime>0 ? (std::min)(baseDeltaTime,.1f) : 0;
        cameraShakeTimer_ = (std::max)(0.0f,cameraShakeTimer_-dt);
        const float duration = (std::max)(cameraShakeDuration_,.001f);
        const float ratio = (std::clamp)(cameraShakeTimer_/duration,0.0f,1.0f);
        const float amplitude = (std::min)(cameraShakePower_,.45f)*ratio*ratio;
        const auto& snapshot = enemy_->GetNeonDepthSnapshot();
        // Analytic bounded shake consumes no shared gameplay RNG. The final
        // shaken matrix is installed before floor mouse input and all actors.
        const double eventPhase = double(snapshot.generation%997)+double(snapshot.plan.instance%991)*.71;
        const double age = double(duration-cameraShakeTimer_);
        focus.x += amplitude*float(std::sin(age*53+eventPhase));
        focus.y += amplitude*float(std::sin(age*67+eventPhase*.37));
    } else if (reducedMotion) cameraShakeTimer_=0;
    if (!neondepth::TryCamera(profile,focus,frame)) { RestoreNeonDepthCamera(); return false; }
    neonDepthFloorViewCenter_ = focus;
    camera->SetRotate(frame.rotation); camera->SetTranslate(frame.position);
    camera->SetFovY(profile.fovY); camera->SetAspectRatio(profile.aspect);
    camera->SetNearClip(profile.nearClip); camera->SetFarClip(profile.farClip);
    camera->SetProjectionJitter({});
    if (player_) player_->SetNeonDepthAimEnabled(true);
    return true;
}

void GameScene::RestoreNeonDepthCamera() {
    if (!neonDepthCameraScoped_ || !camera) return;
    camera->SetRotate(neonDepthSavedCamera_.rotation); camera->SetTranslate(neonDepthSavedCamera_.position);
    camera->SetFovY(neonDepthSavedCamera_.fovY); camera->SetAspectRatio(neonDepthSavedCamera_.aspect);
    camera->SetNearClip(neonDepthSavedCamera_.nearClip); camera->SetFarClip(neonDepthSavedCamera_.farClip);
    camera->SetProjectionJitter(neonDepthSavedCamera_.jitter);
    camera->Update();
    if (player_) player_->SetNeonDepthAimEnabled(false);
    cg2::Object3dCommon::GetInstance()->SetIsDebugCamera(neonDepthSavedDebugCamera_);
    neonDepthCameraScoped_=false;
}

bool GameScene::UpdateNeonDepthIntro(float baseDeltaTime) {
    neonDepthIntroDelta_ = 0;
    if (!IsNeonDepthEncounterActive() || enemy_->GetNeonDepthSnapshot().phase!=neondepth::Phase::Intro) return false;
    const bool permitted = combatFlow_.GetState()==GameFlowState::Playing && phase_==Phase::kMain &&
        !IsTankRunMenuOpen() && !player_->IsChangeMode() && !player_->IsDead();
    if (permitted && (neonDepthSkipRequested_ || input_->IsKeyTriggered(DIK_RETURN) || input_->IsKeyTriggered(DIK_SPACE))) {
        neonDepthSkipRequested_ = false;
        enemy_->SkipNeonDepthIntro();
        enemy_->Update(0); // Refresh transforms; no first combat tick on the skip input.
    } else {
        neonDepthIntroDelta_ = permitted ? baseDeltaTime : 0;
        enemy_->Update(neonDepthIntroDelta_);
    }
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    gameplayScenarioCombatDt_ = neonDepthIntroDelta_;
#endif
    // A zero-dt Player::Update still handles attacks; refresh borrowed drawing
    // transforms without passing input into any actor during the intro lock.
    if (playerObject_) playerObject_->Update();
    collisionDebugRingManager_->Clear();
    return true;
}

void GameScene::UpdateNeonDepthEffects() {
    if (!neonDepthEffects_ || !enemy_ || !neonBossVisual_) return;
    cg2::RuntimeProfiler::CpuScope effectScope("Neon Depth FX Build");
    neondepth::EffectsInput input;
    input.frameId = ++neonDepthFrameId_;
    input.snapshot = enemy_->GetNeonDepthSnapshot();
    input.core = enemy_->GetWorldPosition(); input.coreRadius = enemy_->GetRadius();
    input.hitFeedback = enemy_->GetDamageFeedbackRatio();
    input.profile = neonBossVisual_->GetDepthProfile();
    input.formation = neonBossVisual_->GetDepthFormation();
    input.life = neonBossVisual_->GetDepthLife();
    input.dissolveProgress = neonBossVisual_->GetDissolveProgress();
    input.bodyVisualEnabled = neonBossVisualEnabled_;
    input.encounterActive = enemy_->IsNeonDepthEncounterEnabled() && enemy_->IsRunEncounterEnabled() && IsRunRivalActive();
    input.sockets.chestValid = neonBossVisual_->GetDepthSocketWorld(neondepth::Socket::Chest,input.sockets.chest);
    input.sockets.leftHandValid = neonBossVisual_->GetDepthSocketWorld(neondepth::Socket::LeftHand,input.sockets.leftHand);
    input.sockets.rightHandValid = neonBossVisual_->GetDepthSocketWorld(neondepth::Socket::RightHand,input.sockets.rightHand);
    input.viewProjection = camera->GetViewProjectionMatrix();
    const auto world = cg2::MakeAffineMatrix({1,1,1},camera->GetRotate(),{});
    input.cameraRight = {world.m[0][0],world.m[0][1],world.m[0][2]};
    input.cameraUp = {world.m[1][0],world.m[1][1],world.m[1][2]};
    input.cameraForward = {world.m[2][0],world.m[2][1],world.m[2][2]};
    const bool terminal = input.snapshot.phase==neondepth::Phase::Defeated || input.snapshot.phase==neondepth::Phase::Aborted;
    if (terminal && neonBossVisual_->IsFinished()) {
        // The previous submitted frame's fence completed before this Update.
        // Keep the wrapper's evidence counters, return its instance buffers once.
        if (neonDepthEffects_->HasResources()) neonDepthEffects_->Release();
    } else neonDepthEffects_->Update(input);
    // Observable counters share the profiler's actual frame identity. They do
    // not allocate resources or influence encounter/presentation clocks.
    auto& profiler = cg2::RuntimeProfiler::Get();
    const auto& model = neonBossVisual_->GetStats();
    const auto& effects = neonDepthEffects_->GetStats();
    profiler.SetCounter("Depth enabled",input.encounterActive);
    profiler.SetCounter("Depth phase",static_cast<unsigned>(input.snapshot.phase));
    profiler.SetCounter("Depth attack",static_cast<unsigned>(input.snapshot.plan.attack));
    profiler.SetCounter("Depth phase two",input.snapshot.plan.phaseTwo);
    profiler.SetCounter("Depth visual enabled",input.bodyVisualEnabled);
    profiler.SetCounter("Depth model creates",model.resourceCreates);
    profiler.SetCounter("Depth model releases",model.resourceReleases);
    profiler.SetCounter("Depth model updates",model.modelUpdates);
    profiler.SetCounter("Depth animation selections",model.animationSelections);
    profiler.SetCounter("Depth visual draws",model.draws);
    profiler.SetCounter("Depth model resources",neonBossVisual_->HasResources());
    profiler.SetCounter("Depth draw buffers",static_cast<double>(neonBossVisual_->GetDrawConstantBufferCount()));
    profiler.SetCounter("Depth effect creates",effects.resourceCreates);
    profiler.SetCounter("Depth effect releases",effects.resourceReleases);
    profiler.SetCounter("Depth effect resources",neonDepthEffects_->HasResources());
    profiler.SetCounter("Depth effect updates",static_cast<double>(effects.updates));
    profiler.SetCounter("Depth effect vertices",effects.vertices);
    profiler.SetCounter("Depth effect air vertices",effects.airVertices);
    profiler.SetCounter("Depth effect floor vertices",effects.floorVertices);
    profiler.SetCounter("Depth effect launch latches",effects.launchLatches);
    profiler.SetCounter("Depth effect invalid frames",static_cast<double>(effects.invalidFrames));
    profiler.SetCounter("Depth effect capacity rejects",static_cast<double>(effects.capacityRejected));
    profiler.SetCounter("Depth effect binding rejects",static_cast<double>(effects.depthBindingRejected));
    profiler.SetCounter("Depth effect duplicate updates",static_cast<double>(effects.duplicateUpdates));
    profiler.SetCounter("SRV descriptors",cg2::Object3dCommon::GetInstance()->GetSrvManager()->GetAllocatedCount());
}
