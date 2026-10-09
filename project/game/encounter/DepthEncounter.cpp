#include "game/encounter/DepthEncounter.h"
#include "game/session/GameplaySystems.h"
#include "Enemy.h"
#include "Player.h"
#include "RuntimeProfiler.h"
#include <fstream>

namespace gameplay {
// Boss encounter policy, scoped camera and scene-owned presentation integration.

bool DepthEncounter::LoadNeonDepthConfig(bool hotReload)
{
    neondepth::Config candidate;
    std::string error;
    try {
        const std::filesystem::path path("resources/configs/neonBossDepth.json");
        if (std::filesystem::exists(path)) {
            if (!std::filesystem::is_regular_file(path) || std::filesystem::file_size(path) > 64 * 1024)
                throw std::runtime_error("Depth settings must be a regular file of at most 64 KiB.");
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
                throw std::runtime_error("Could not open Depth settings.");
            const auto document = nlohmann::json::parse(stream);
            if (!neondepth::ReadConfig(document, candidate, error))
                throw std::runtime_error(error);
        }
        // The camera/enabled choice is latched only on a fresh encounter. A
        // valid active edit queues attack geometry/timing and visual placement
        // for the next instance, preserving the already announced plan.
        if (hotReload && IsNeonDepthEncounterActive()) {
            if (!world_.resources.enemy_->QueueNeonDepthTuning(candidate.combat) ||
                (world_.resources.neonBossVisual_ && !world_.resources.neonBossVisual_->SetDepthProfile(candidate.visual)))
                throw std::runtime_error("Validated Depth edit could not be queued.");
        }
        world_.resources.neonDepthPendingConfig_ = candidate;
        if (!hotReload || !IsNeonDepthEncounterActive())
            world_.resources.neonDepthConfig_ = candidate;
        world_.resources.neonDepthConfigStatus_ =
            hotReload ? "Depth settings queued for the next attack / encounter." : "Depth settings ready.";
        return true;
    }
    catch (const std::exception& exception) {
        world_.resources.neonDepthConfigStatus_ = std::string(exception.what()).substr(0, 256);
        OutputDebugStringA((std::string("[NeonDepth] ") + world_.resources.neonDepthConfigStatus_ + "\n").c_str());
        return false; // Preserve the previous valid profile (or initial defaults).
    }
}

bool DepthEncounter::IsNeonDepthEncounterActive() const
{
    return !world_.demo.titleDemo_ && world_.resources.enemy_ && world_.resources.enemy_->IsNeonDepthEncounterEnabled() &&
           world_.resources.enemy_->IsRunEncounterEnabled() && world_.gameplayQueries->IsRunRivalActive() &&
           world_.resources.enemy_->GetNeonDepthSnapshot().phase != neondepth::Phase::Aborted;
}

void DepthEncounter::ConfigureNeonDepthEncounter([[maybe_unused]] bool completedFixtureRoomReset)
{
    if (!world_.resources.enemy_ || !world_.run.expeditionRun_ || world_.run.tankExpedition_.GetRoomKind() != tankexp::RoomKind::Boss)
        return;
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    const auto& session = GameplayScenarioSession::Get();
    if (world_.resources.neonBossAutoTest_ ||
        (session.IsActive() && (!gameplaytest::IsNeonDepthScenario(session.GetSettings().scenario) || !completedFixtureRoomReset)))
        return;
#endif
    world_.resources.neonDepthConfig_ = world_.resources.neonDepthPendingConfig_;
    if (!world_.resources.neonDepthConfig_.enabled)
        return;
    // Old explicitly selected Developer fixture profiles are preserved by the
    // fixture adapter. Ordinary authored / sequential boss entry selects Depth.
    if (!world_.resources.neonDepthEffects_)
        world_.resources.neonDepthEffects_ = std::make_unique<NeonDepthEffects>();
    auto* dx = cg2::Object3dCommon::GetInstance()->GetDxCommon();
    if (!dx->InitializeTrailSceneNoDepthPipeline() || !world_.resources.neonDepthEffects_->Initialize(dx)) {
        world_.resources.neonDepthConfigStatus_ = "Depth floor cues could not initialize; encounter retains the existing safe policy.";
        OutputDebugStringA((world_.resources.neonDepthConfigStatus_ + "\n").c_str());
        return;
    }
    world_.resources.enemy_->EnableNeonDepthEncounter(true, world_.resources.neonDepthConfig_.combat);
    if (!world_.resources.enemy_->IsNeonDepthEncounterEnabled())
        return;
    if (world_.resources.neonBossVisual_)
        world_.resources.neonBossVisual_->SetDepthProfile(world_.resources.neonDepthConfig_.visual);
    if (world_.resources.neonDepthEffects_)
        world_.resources.neonDepthEffects_->ResetEncounter();
    world_.combatFlow->SetEventCallout("床の投影コアを攻撃 / Enterで登場をスキップ", 2.4f);
}

bool DepthEncounter::UpdateNeonDepthCamera(float baseDeltaTime)
{
    const bool wantScope = IsNeonDepthEncounterActive() && (world_.combat.combatFlow_.GetState() == GameFlowState::Playing ||
                                                            world_.combat.combatFlow_.GetState() == GameFlowState::BossDefeatSequence);
    if (!wantScope) {
        RestoreNeonDepthCamera();
        return false;
    }
    if (!world_.resources.neonDepthCameraScoped_) {
        world_.resources.neonDepthSavedCamera_ = {world_.resources.camera->GetTranslate(),        world_.resources.camera->GetRotate(),
                                                  world_.resources.camera->GetProjectionJitter(), world_.resources.camera->GetFovY(),
                                                  world_.resources.camera->GetAspectRatio(),      world_.resources.camera->GetNearClip(),
                                                  world_.resources.camera->GetFarClip()};
        world_.resources.neonDepthCameraScoped_ = true;
        world_.resources.neonDepthSavedDebugCamera_ = cg2::Object3dCommon::GetInstance()->GetIsDebugCamera();
    }
    cg2::Object3dCommon::GetInstance()->SetIsDebugCamera(false);
    neondepth::CameraFrame frame;
    auto profile = world_.resources.neonDepthConfig_.camera;
    profile.aspect = float(cg2::WinApp::kClientWidth) / float(cg2::WinApp::kClientHeight);
    // Authored expedition rooms share this world-space frame. Cover the entire
    // room interior and permitted evasive movement, rather than following the
    // decorative head or enlarging only a screen-space projection.
    const bool reducedMotion = world_.resources.neonBossVisual_ ? world_.resources.neonBossVisual_->GetDepthProfile().reducedMotion
                                                                : world_.resources.neonDepthConfig_.visual.reducedMotion;
    cg2::Vector3 focus{44, 31, 0};
    if (world_.presentation.cameraShakeTimer_ > 0 && !reducedMotion) {
        const float dt = std::isfinite(baseDeltaTime) && baseDeltaTime > 0 ? (std::min)(baseDeltaTime, .1f) : 0;
        world_.presentation.cameraShakeTimer_ = (std::max)(0.0f, world_.presentation.cameraShakeTimer_ - dt);
        const float duration = (std::max)(world_.presentation.cameraShakeDuration_, .001f);
        const float ratio = (std::clamp)(world_.presentation.cameraShakeTimer_ / duration, 0.0f, 1.0f);
        const float amplitude = (std::min)(world_.presentation.cameraShakePower_, .45f) * ratio * ratio;
        const auto& snapshot = world_.resources.enemy_->GetNeonDepthSnapshot();
        // Analytic bounded shake consumes no shared gameplay RNG. The final
        // shaken matrix is installed before floor mouse input and all actors.
        const double eventPhase = double(snapshot.generation % 997) + double(snapshot.plan.instance % 991) * .71;
        const double age = double(duration - world_.presentation.cameraShakeTimer_);
        focus.x += amplitude * float(std::sin(age * 53 + eventPhase));
        focus.y += amplitude * float(std::sin(age * 67 + eventPhase * .37));
    } else if (reducedMotion)
        world_.presentation.cameraShakeTimer_ = 0;
    if (!neondepth::TryCamera(profile, focus, frame)) {
        RestoreNeonDepthCamera();
        return false;
    }
    world_.resources.neonDepthFloorViewCenter_ = focus;
    world_.resources.camera->SetRotate(frame.rotation);
    world_.resources.camera->SetTranslate(frame.position);
    world_.resources.camera->SetFovY(profile.fovY);
    world_.resources.camera->SetAspectRatio(profile.aspect);
    world_.resources.camera->SetNearClip(profile.nearClip);
    world_.resources.camera->SetFarClip(profile.farClip);
    world_.resources.camera->SetProjectionJitter({});
    if (world_.resources.player_)
        world_.resources.player_->SetNeonDepthAimEnabled(true);
    return true;
}

void DepthEncounter::RestoreNeonDepthCamera()
{
    if (!world_.resources.neonDepthCameraScoped_ || !world_.resources.camera)
        return;
    world_.resources.camera->SetRotate(world_.resources.neonDepthSavedCamera_.rotation);
    world_.resources.camera->SetTranslate(world_.resources.neonDepthSavedCamera_.position);
    world_.resources.camera->SetFovY(world_.resources.neonDepthSavedCamera_.fovY);
    world_.resources.camera->SetAspectRatio(world_.resources.neonDepthSavedCamera_.aspect);
    world_.resources.camera->SetNearClip(world_.resources.neonDepthSavedCamera_.nearClip);
    world_.resources.camera->SetFarClip(world_.resources.neonDepthSavedCamera_.farClip);
    world_.resources.camera->SetProjectionJitter(world_.resources.neonDepthSavedCamera_.jitter);
    world_.resources.camera->Update();
    if (world_.resources.player_)
        world_.resources.player_->SetNeonDepthAimEnabled(false);
    cg2::Object3dCommon::GetInstance()->SetIsDebugCamera(world_.resources.neonDepthSavedDebugCamera_);
    world_.resources.neonDepthCameraScoped_ = false;
}

bool DepthEncounter::UpdateNeonDepthIntro(float baseDeltaTime)
{
    world_.resources.neonDepthIntroDelta_ = 0;
    if (!IsNeonDepthEncounterActive() || world_.resources.enemy_->GetNeonDepthSnapshot().phase != neondepth::Phase::Intro)
        return false;
    const bool permitted = world_.combat.combatFlow_.GetState() == GameFlowState::Playing && world_.combat.phase_ == Phase::kMain &&
                           !world_.arenaRunController->IsTankRunMenuOpen() && !world_.resources.player_->IsChangeMode() &&
                           !world_.resources.player_->IsDead();
    if (permitted && (world_.resources.neonDepthSkipRequested_ || world_.resources.input_->IsKeyTriggered(DIK_RETURN) ||
                      world_.resources.input_->IsKeyTriggered(DIK_SPACE))) {
        world_.resources.neonDepthSkipRequested_ = false;
        world_.resources.enemy_->SkipNeonDepthIntro();
        world_.resources.enemy_->Update(0); // Refresh transforms; no first combat tick on the skip input.
    } else {
        world_.resources.neonDepthIntroDelta_ = permitted ? baseDeltaTime : 0;
        world_.resources.enemy_->Update(world_.resources.neonDepthIntroDelta_);
    }
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    world_.resources.gameplayScenarioCombatDt_ = world_.resources.neonDepthIntroDelta_;
#endif
    // A zero-dt Player::Update still handles attacks; refresh borrowed drawing
    // transforms without passing input into any actor during the intro lock.
    if (world_.resources.playerObject_)
        world_.resources.playerObject_->Update();
    world_.resources.collisionDebugRingManager_->Clear();
    return true;
}

void DepthEncounter::UpdateNeonDepthEffects()
{
    if (!world_.resources.neonDepthEffects_ || !world_.resources.enemy_ || !world_.resources.neonBossVisual_)
        return;
    cg2::RuntimeProfiler::CpuScope effectScope("Neon Depth FX Build");
    neondepth::EffectsInput input;
    input.frameId = ++world_.resources.neonDepthFrameId_;
    input.snapshot = world_.resources.enemy_->GetNeonDepthSnapshot();
    input.core = world_.resources.enemy_->GetWorldPosition();
    input.coreRadius = world_.resources.enemy_->GetRadius();
    input.hitFeedback = world_.resources.enemy_->GetDamageFeedbackRatio();
    input.profile = world_.resources.neonBossVisual_->GetDepthProfile();
    input.formation = world_.resources.neonBossVisual_->GetDepthFormation();
    input.life = world_.resources.neonBossVisual_->GetDepthLife();
    input.dissolveProgress = world_.resources.neonBossVisual_->GetDissolveProgress();
    input.bodyVisualEnabled = world_.resources.neonBossVisualEnabled_;
    input.encounterActive = world_.resources.enemy_->IsNeonDepthEncounterEnabled() && world_.resources.enemy_->IsRunEncounterEnabled() &&
                            world_.gameplayQueries->IsRunRivalActive();
    input.sockets.chestValid = world_.resources.neonBossVisual_->GetDepthSocketWorld(neondepth::Socket::Chest, input.sockets.chest);
    input.sockets.leftHandValid =
        world_.resources.neonBossVisual_->GetDepthSocketWorld(neondepth::Socket::LeftHand, input.sockets.leftHand);
    input.sockets.rightHandValid =
        world_.resources.neonBossVisual_->GetDepthSocketWorld(neondepth::Socket::RightHand, input.sockets.rightHand);
    input.viewProjection = world_.resources.camera->GetViewProjectionMatrix();
    const auto world = cg2::MakeAffineMatrix({1, 1, 1}, world_.resources.camera->GetRotate(), {});
    input.cameraRight = {world.m[0][0], world.m[0][1], world.m[0][2]};
    input.cameraUp = {world.m[1][0], world.m[1][1], world.m[1][2]};
    input.cameraForward = {world.m[2][0], world.m[2][1], world.m[2][2]};
    const bool terminal = input.snapshot.phase == neondepth::Phase::Defeated || input.snapshot.phase == neondepth::Phase::Aborted;
    if (terminal && world_.resources.neonBossVisual_->IsFinished()) {
        // The previous submitted frame's fence completed before this Update.
        // Keep the wrapper's evidence counters, return its instance buffers once.
        if (world_.resources.neonDepthEffects_->HasResources())
            world_.resources.neonDepthEffects_->Release();
    } else
        world_.resources.neonDepthEffects_->Update(input);
    // Observable counters share the profiler's actual frame identity. They do
    // not allocate resources or influence encounter/presentation clocks.
    auto& profiler = cg2::RuntimeProfiler::Get();
    const auto& model = world_.resources.neonBossVisual_->GetStats();
    const auto& effects = world_.resources.neonDepthEffects_->GetStats();
    profiler.SetCounter("Depth enabled", input.encounterActive);
    profiler.SetCounter("Depth phase", static_cast<unsigned>(input.snapshot.phase));
    profiler.SetCounter("Depth attack", static_cast<unsigned>(input.snapshot.plan.attack));
    profiler.SetCounter("Depth phase two", input.snapshot.plan.phaseTwo);
    profiler.SetCounter("Depth visual enabled", input.bodyVisualEnabled);
    profiler.SetCounter("Depth model creates", model.resourceCreates);
    profiler.SetCounter("Depth model releases", model.resourceReleases);
    profiler.SetCounter("Depth model updates", model.modelUpdates);
    profiler.SetCounter("Depth animation selections", model.animationSelections);
    profiler.SetCounter("Depth visual draws", model.draws);
    profiler.SetCounter("Depth model resources", world_.resources.neonBossVisual_->HasResources());
    profiler.SetCounter("Depth draw buffers", static_cast<double>(world_.resources.neonBossVisual_->GetDrawConstantBufferCount()));
    profiler.SetCounter("Depth effect creates", effects.resourceCreates);
    profiler.SetCounter("Depth effect releases", effects.resourceReleases);
    profiler.SetCounter("Depth effect resources", world_.resources.neonDepthEffects_->HasResources());
    profiler.SetCounter("Depth effect updates", static_cast<double>(effects.updates));
    profiler.SetCounter("Depth effect vertices", effects.vertices);
    profiler.SetCounter("Depth effect air vertices", effects.airVertices);
    profiler.SetCounter("Depth effect floor vertices", effects.floorVertices);
    profiler.SetCounter("Depth effect launch latches", effects.launchLatches);
    profiler.SetCounter("Depth effect invalid frames", static_cast<double>(effects.invalidFrames));
    profiler.SetCounter("Depth effect capacity rejects", static_cast<double>(effects.capacityRejected));
    profiler.SetCounter("Depth effect binding rejects", static_cast<double>(effects.depthBindingRejected));
    profiler.SetCounter("Depth effect duplicate updates", static_cast<double>(effects.duplicateUpdates));
    profiler.SetCounter("SRV descriptors", cg2::Object3dCommon::GetInstance()->GetSrvManager()->GetAllocatedCount());
}

} // namespace gameplay
