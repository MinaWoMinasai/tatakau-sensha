#include "game/render/session/BossPresentation.h"
#include "game/session/GameplaySystems.h"
#include "GameStartMode.h"
#include "Enemy.h"
#include "RuntimeProfiler.h"
#include <cmath>
#include <fstream>
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG) && defined(USE_IMGUI)
#include "externals/imgui/imgui.h"
#endif

namespace gameplay {
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG) && defined(USE_IMGUI)
#endif

void BossPresentation::UpdateNeonBossVisual(float deltaTime)
{
    if (!world_.resources.neonBossVisual_ || !world_.resources.enemy_)
        return;
    cg2::RuntimeProfiler::CpuScope scope("Neon Boss Update");
    NeonBossVisualInput input;
    input.position = world_.resources.enemy_->GetWorldPosition();
    const auto aim = world_.resources.enemy_->GetAimDirection();
    input.aimAngleRadians = std::atan2(aim.y, aim.x);
    input.radius = world_.resources.enemy_->GetRadius();
    input.hp = world_.resources.enemy_->GetHp();
    input.maxHp = world_.resources.enemy_->GetMaxHp();
    input.alive = !world_.resources.enemy_->IsDead() && input.hp > 0;
    input.encounterActive = world_.gameplayQueries->IsRunRivalActive() && world_.resources.enemy_->IsRunEncounterEnabled() &&
                            !world_.gameplayQueries->IsTutorialCombatSuppressed();
    input.damageFeedback = world_.resources.enemy_->GetDamageFeedbackRatio();
    input.depthEncounter = world_.resources.enemy_->IsNeonDepthEncounterEnabled();
    input.depth = world_.resources.enemy_->GetNeonDepthSnapshot();
    const auto rival = world_.resources.enemy_->GetRivalCombatStatus();
    BossVisualBridgeInput bridgeInput;
    bridgeInput.encounterGeneration = world_.resources.enemy_->GetEncounterGeneration();
    bridgeInput.shotsFired = world_.resources.enemy_->GetShotsFired();
    bridgeInput.position = {input.position.x, input.position.y, input.position.z};
    bridgeInput.hp = input.hp;
    bridgeInput.maxHp = input.maxHp;
    bridgeInput.rivalEnabled = rival.enabled;
    bridgeInput.rivalPhaseTwo = rival.phase2;
    bridgeInput.rivalPhase = rival.phase;
    bridgeInput.prototypeEnabled = world_.resources.enemy_->IsPrototypeCombatEnabled();
    bridgeInput.prototypePhase = world_.resources.enemy_->GetPrototypeCombatPhase();
    const auto decision = world_.resources.bossVisualBridge_.Update(bridgeInput, deltaTime);
    // Reset remains before this encounter's first visual Update. The bridge
    // only maps observable values and owns presentation history, never combat.
    if (decision.resetEncounter)
        world_.resources.neonBossVisual_->ResetEncounter();
    input.phaseTwo = decision.phaseTwo;
    input.action = decision.action;
    world_.resources.neonBossVisual_->SetEnabled(world_.resources.neonBossVisualEnabled_);
    {
        cg2::RuntimeProfiler::CpuScope modelScope("Neon Depth Model Update");
        world_.resources.neonBossVisual_->Update(input, deltaTime);
    }
    world_.depthEncounter->UpdateNeonDepthEffects();
}

bool BossPresentation::UseNeonBossVisual() const
{
    if (world_.resources.enemy_ && world_.resources.enemy_->IsNeonDepthEncounterEnabled())
        return true; // Floor core owns gameplay; no legacy decorative tank.
    return world_.resources.neonBossVisualEnabled_ && world_.resources.neonBossVisual_ && world_.gameplayQueries->IsRunRivalActive() &&
           (world_.resources.neonBossVisual_->HasResources() || world_.resources.neonBossVisual_->IsFinished());
}

void BossPresentation::DrawNeonBossVisual()
{
    if (!UseNeonBossVisual() || world_.gameplayQueries->IsTutorialCombatSuppressed())
        return;
    if (world_.resources.neonBossVisual_)
        world_.resources.neonBossVisual_->Draw();
}

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
namespace {
constexpr const char* kBossCaptureDirectory = "generated/neon_boss_gameplay";
}

void BossPresentation::RequestNeonBossDeveloperEncounter()
{
    if (world_.demo.titleDemo_ || !world_.run.expeditionMapEnabled_ || world_.combat.phase_ == Phase::kFadeOut ||
        world_.gameplayQueries->IsNeonShowcaseActive())
        return;
    if (world_.resources.player_->IsDead()) {
        // 死亡済みの自機は復活させず、通常の再出撃で新しいシーンを作る。
        GameStartSession::RequestDeveloperBossStart();
        world_.combat.resultSelection_ = 0;
        world_.combatFlow->ConfirmResultSelection();
        return;
    }
    world_.resources.neonBossDeveloperStartPending_ = true;
}

void BossPresentation::StartNeonBossDeveloperEncounter()
{
    if (!world_.run.expeditionMapEnabled_)
        return;
    if (world_.resources.player_->IsDead()) {
        world_.run.expeditionMapStatus_ = "自機が死亡しています。結果画面から再出撃してボス確認を開いてください。";
        world_.resources.neonBossValidationErrors_.push_back(
            "Developer encounter requires a living player; restart from the result screen.");
        return;
    }
    // Copy an authored boss room into a one-node in-memory run. Use the ordinary
    // room entry/spawn path; never save over the player's map or room definitions.
    const auto& definition = world_.run.expeditionMapRun_.GetDefinition();
    const auto found = std::find_if(definition.nodes.begin(), definition.nodes.end(), [](const auto& node) {
        return node.kind == tankexp::NodeKind::Boss;
    });
    if (found == definition.nodes.end()) {
        world_.resources.neonBossValidationErrors_.push_back("No authored boss node.");
        return;
    }
    auto node = *found;
    node.next.clear();
    node.column = 0;
    node.row = 0;
    tankexp::MapDefinition fixture;
    fixture.startNodes = {node.id};
    fixture.nodes = {node};
    std::string error;
    if (!world_.run.expeditionMapRun_.Reset(fixture, error)) {
        world_.resources.neonBossValidationErrors_.push_back(error);
        return;
    }
    // Map UI owns one visual per definition node/edge. Retain one existing node
    // slot and drop stale edges before ordinary UI refresh can dereference them.
    // This entry runs after the previous frame fence completed.
    world_.run.expeditionMapVisuals_.resize(1);
    world_.run.expeditionMapEdges_.clear();
    world_.run.expeditionMapSelection_ = node.id;
    world_.run.expeditionMapScroll_ = 0;
    world_.run.tankExpeditionTutorial_.Skip();
    world_.run.expeditionGuideActive_ = false;
    world_.run.expeditionBuildChoice_ = false;
    world_.run.expeditionBuildChosen_ = true;
    world_.run.expeditionAuthoringHubOpen_ = false;
    world_.run.tankRunPaused_ = false;
    world_.run.expeditionMapPreview_ = false;
    world_.run.tankExpeditionBalanceEditorOpen_ = world_.run.expeditionRoomEditorOpen_ = world_.run.expeditionMapEditorOpen_ =
        world_.run.expeditionContentEditorOpen_ = false;
    world_.presentation.developerBloomFreeze_ = world_.resources.neonBossDeveloperFreeze_ = false;
    world_.run.expeditionTransition_ = {};
    world_.run.expeditionCollectAll_ = false;
    world_.combat.combatFlow_.Reset();
    world_.combat.bossDefeatHandled_ = false;
    world_.run.tankRunMenuAge_ = 1;
    world_.expeditionMapController->EnterExpeditionMapNode(node.id);
    world_.presentation.debugPlayerNoDamage_ = true;
    world_.resources.player_->SetDebugNoDamage(true);
    if (world_.resources.neonBossAutoTest_) {
        world_.run.tankExpeditionMusicEnabled_ = world_.run.tankExpeditionEffectsEnabled_ = false;
        world_.run.tankExpeditionAudio_.SetMusicVolume(0);
        world_.run.tankExpeditionAudio_.SetEffectsVolume(0);
        // Keep the existing camera while ensuring both actors are in its viewport.
        const auto position = world_.resources.enemy_->GetWorldPosition() + cg2::Vector3{-14, 0, 0};
        world_.resources.player_->ResetRunRoomState(position);
        world_.resources.player_->SetDemoInput(true, {}, world_.resources.enemy_->GetWorldPosition(), false, false);
        const auto center = (position + world_.resources.enemy_->GetWorldPosition()) * 0.5f;
        world_.resources.camera->SetTranslate({center.x, center.y, -55});
        world_.resources.camera->Update();
    }
    world_.resources.neonBossVisualEnabled_ = true;
    UpdateNeonBossVisual(0);
    world_.expeditionController->RefreshTankExpeditionUi();
}

nlohmann::json BossPresentation::MakeNeonBossMetadata() const
{
    auto vector = [](const cg2::Vector3& p) {
        return nlohmann::json::array({p.x, p.y, p.z});
    };
    const auto rival = world_.resources.enemy_->GetRivalCombatStatus();
    const auto stats = world_.resources.neonBossVisual_->GetStats();
    nlohmann::json data = {
        {"scene", "GameScene / authored expedition boss"},
        {"build", "Development"},
        {"visualEnabled", world_.resources.neonBossVisualEnabled_},
        {"samePoseFreeze", world_.resources.neonBossDeveloperFreeze_},
        {"encounterGeneration", world_.resources.enemy_->GetEncounterGeneration()},
        {"gameplay",
         {{"hp", world_.resources.enemy_->GetHp()},
          {"maxHp", world_.resources.enemy_->GetMaxHp()},
          {"dead", world_.resources.enemy_->IsDead()},
          {"position", vector(world_.resources.enemy_->GetWorldPosition())},
          {"aim", vector(world_.resources.enemy_->GetAimDirection())},
          {"phase", static_cast<int>(rival.phase)},
          {"phase2", rival.phase2},
          {"shots", world_.resources.enemy_->GetShotsFired()},
          {"dashCount", rival.dashCount}}},
        {"presentation",
         {{"animation", world_.resources.neonBossVisual_->GetAnimationName()},
          {"animationTime", world_.resources.neonBossVisual_->GetAnimationTime()},
          {"action", static_cast<int>(world_.resources.neonBossVisual_->GetState().GetAction())},
          {"life", static_cast<int>(world_.resources.neonBossVisual_->GetState().GetLife())},
          {"dissolveProgress", world_.resources.neonBossVisual_->GetDissolveProgress()},
          {"hasResources", world_.resources.neonBossVisual_->HasResources()},
          {"finished", world_.resources.neonBossVisual_->IsFinished()},
          {"resourceCreates", stats.resourceCreates},
          {"resourceReleases", stats.resourceReleases},
          {"drawConstantBuffers", world_.resources.neonBossVisual_->GetDrawConstantBufferCount()},
          {"animationSelections", stats.animationSelections},
          {"status", world_.resources.neonBossVisual_->GetStatus()}}},
        {"camera",
         {{"position", vector(world_.resources.camera->GetTranslate())}, {"rotation", vector(world_.resources.camera->GetRotate())}}},
        {"descriptorCount", cg2::Object3dCommon::GetInstance()->GetSrvManager()->GetAllocatedCount()},
        {"globalBloomModified", false}};
    for (const auto& row : world_.resources.camera->GetViewProjectionMatrix().m)
        data["camera"]["viewProjection"].push_back({row[0], row[1], row[2], row[3]});
    for (const auto& row : world_.resources.neonBossVisual_->GetWorldMatrix().m)
        data["presentation"]["world"].push_back({row[0], row[1], row[2], row[3]});
    return data;
}

void BossPresentation::RecordNeonBossDeveloperFrame(cg2::DirectXCommon& dx)
{
    if (world_.resources.neonBossCapture_.HasRequest()) {
        auto metadata = MakeNeonBossMetadata();
        const auto frame = world_.gameplayQueries->MakeDeveloperGameCaptureMetadata(dx);
        for (const char* key : {"gpu", "queueTimestampFrequencyHz", "validation"})
            if (frame.contains(key))
                metadata[key] = frame.at(key);
        metadata["comparisonTemporalAA"] =
            "Existing comparisonFreeze temporarily disables TAA/jitter; normal gameplay settings are preserved.";
        world_.resources.neonBossCapture_.SetFrameMetadata(std::move(metadata));
    }
    world_.resources.neonBossCapture_.Record(dx);
}

void BossPresentation::WriteNeonBossValidation(bool completed)
{
    std::filesystem::create_directories(kBossCaptureDirectory);
    nlohmann::json report = MakeNeonBossMetadata();
    report["completed"] = completed;
    report["testMode"] = true;
    report["errors"] = world_.resources.neonBossValidationErrors_;
    report["captures"] = world_.resources.neonBossValidationCaptures_;
    report["elapsed"] = world_.resources.neonBossValidationAge_;
    report["fixture"] =
        "Actual authored boss room and RivalBossCombat; invulnerable stationary player. HP injected for phase/death coverage.";
    std::ofstream(std::string(kBossCaptureDirectory) + "/validation.json") << report.dump(2) << '\n';
}

void BossPresentation::UpdateNeonBossDeveloperValidation()
{
    world_.resources.neonBossCapture_.Resolve(*cg2::Object3dCommon::GetInstance()->GetDxCommon());
    if (world_.resources.neonBossDeveloperStartPending_ && world_.combat.phase_ == Phase::kMain) {
        world_.resources.neonBossDeveloperStartPending_ = false;
        StartNeonBossDeveloperEncounter();
        if (world_.resources.neonBossAutoTest_)
            WriteNeonBossValidation(false);
    }
    if (!world_.resources.neonBossAutoTest_ || !world_.resources.neonBossVisual_ || world_.resources.neonBossDeveloperStartPending_)
        return;
    world_.resources.neonBossValidationAge_ += 1.0f / 60.0f;
    if (world_.resources.neonBossValidationAge_ > 60) {
        world_.resources.neonBossValidationErrors_.push_back("Timed out waiting for actual boss phases/captures.");
        WriteNeonBossValidation(false);
        PostQuitMessage(9);
        world_.resources.neonBossAutoTest_ = false;
        return;
    }
    if (world_.resources.neonBossCaptureStep_) {
        if (world_.resources.neonBossCapture_.IsBusy())
            return;
        if (!world_.resources.neonBossCapture_.WasLastCaptureSuccessful())
            world_.resources.neonBossValidationErrors_.push_back(world_.resources.neonBossCapture_.GetStatus());
        if (world_.resources.neonBossCaptureStep_ == 1) {
            world_.resources.neonBossVisualEnabled_ = true;
            world_.resources.neonBossVisual_->SetEnabled(true);
            world_.resources.neonBossCapture_.Request(kBossCaptureDirectory, world_.resources.neonBossCaptureName_, {});
            world_.resources.neonBossValidationCaptures_.push_back(world_.resources.neonBossCaptureName_);
            world_.resources.neonBossCaptureStep_ = 2;
            return;
        }
        world_.resources.neonBossCaptureStep_ = 0;
        world_.resources.neonBossDeveloperFreeze_ = false;
        ++world_.resources.neonBossProbe_;
        WriteNeonBossValidation(false);
        if (world_.resources.neonBossProbe_ == 1 && cg2::RuntimeProfiler::Get().IsAllowed()) {
            // Same camera, pose, effects and collider for this A/B cost sample.
            world_.resources.neonBossDeveloperFreeze_ = true;
            world_.resources.neonBossVisualEnabled_ = false;
            world_.resources.neonBossVisual_->SetEnabled(false);
            if (cg2::RuntimeProfiler::Get().StartCapture(std::string(kBossCaptureDirectory) + "/profile_off.csv", 120, 30))
                world_.resources.neonBossProfileStep_ = 1;
            else
                world_.resources.neonBossDeveloperFreeze_ = false;
        }
    }
    if (world_.resources.neonBossProfileStep_) {
        if (!cg2::RuntimeProfiler::Get().IsCaptureComplete())
            return;
        if (world_.resources.neonBossProfileStep_ == 1) {
            world_.resources.neonBossVisualEnabled_ = true;
            world_.resources.neonBossVisual_->SetEnabled(true);
            if (cg2::RuntimeProfiler::Get().StartCapture(std::string(kBossCaptureDirectory) + "/profile_on.csv", 120, 30)) {
                world_.resources.neonBossProfileStep_ = 2;
                return;
            }
        }
        world_.resources.neonBossProfileStep_ = 0;
        world_.resources.neonBossDeveloperFreeze_ = false;
    }
    if (!world_.gameplayQueries->IsRunRivalActive())
        return;
    const auto rival = world_.resources.enemy_->GetRivalCombatStatus();
    if (world_.resources.enemy_->IsDead() && world_.resources.neonBossProbe_ >= 6 &&
        (world_.resources.enemy_->GetShotsFired() != world_.resources.neonBossDeadShots_ ||
         rival.dashCount != world_.resources.neonBossDeadDashCount_ ||
         cg2::Length(world_.resources.enemy_->GetWorldPosition() - world_.resources.neonBossDeadPosition_) > 0.0001f))
        world_.resources.neonBossValidationErrors_.push_back("Gameplay changed during death dissolve.");
    std::string name;
    switch (world_.resources.neonBossProbe_) {
    case 0:
        // Let the existing bind-to-idle blend finish and entry flash settle.
        if (world_.resources.neonBossVisual_->HasResources() && world_.resources.neonBossVisual_->GetAnimationTime() >= 0.45f)
            name = "idle";
        break;
    case 1:
        if (rival.phase == RivalBossCombat::Phase::Locked)
            name = "telegraph";
        break;
    case 2:
        if (rival.phase == RivalBossCombat::Phase::Volley && rival.shotsFired > 0 &&
            world_.resources.neonBossVisual_->GetAnimationTime() >= 0.80f)
            name = "attack";
        break;
    case 3:
        if (rival.phase == RivalBossCombat::Phase::Dash)
            name = "dash";
        break;
    case 4:
        if (!rival.phase2 && world_.resources.enemy_->GetHp() > world_.resources.enemy_->GetMaxHp() / 2)
            world_.resources.enemy_->TakeDamage(
                static_cast<uint32_t>(world_.resources.enemy_->GetHp() - world_.resources.enemy_->GetMaxHp() / 3));
        if (rival.phase2)
            name = "low_hp";
        break;
    case 5:
        if (!world_.resources.enemy_->IsDead()) {
            world_.resources.enemy_->TakeDamage(static_cast<uint32_t>(world_.resources.enemy_->GetHp()));
            world_.resources.neonBossDeadShots_ = world_.resources.enemy_->GetShotsFired();
            world_.resources.neonBossDeadDashCount_ = rival.dashCount;
            world_.resources.neonBossDeadPosition_ = world_.resources.enemy_->GetWorldPosition();
            UpdateNeonBossVisual(0); // Record the exact zero endpoint before advancing time.
        }
        name = "dissolve_000";
        break;
    case 6:
        if (world_.resources.neonBossVisual_->GetDissolveProgress() >= 0.50f)
            name = "dissolve_050";
        break;
    case 7:
        if (world_.resources.neonBossVisual_->IsFinished())
            name = "dissolve_100";
        break;
    default:
        if (world_.resources.neonBossVisual_->HasResources())
            world_.resources.neonBossValidationErrors_.push_back("Terminal visual retained resources.");
        if (world_.resources.neonBossVisual_->GetStats().resourceCreates != 1 ||
            world_.resources.neonBossVisual_->GetStats().resourceReleases != 1)
            world_.resources.neonBossValidationErrors_.push_back("Unexpected resource lifecycle count.");
        WriteNeonBossValidation(world_.resources.neonBossValidationErrors_.empty());
        PostQuitMessage(world_.resources.neonBossValidationErrors_.empty() ? 0 : 9);
        world_.resources.neonBossAutoTest_ = false;
        return;
    }
    if (name.empty())
        return;
    world_.resources.neonBossDeveloperFreeze_ = true;
    world_.resources.neonBossCaptureName_ = name;
    const bool compare = world_.resources.neonBossProbe_ <= 4;
    world_.resources.neonBossVisualEnabled_ = !compare;
    world_.resources.neonBossVisual_->SetEnabled(!compare);
    const auto capture = compare ? name + "_legacy" : name;
    world_.resources.neonBossCapture_.Request(kBossCaptureDirectory, capture, {});
    world_.resources.neonBossValidationCaptures_.push_back(capture);
    world_.resources.neonBossCaptureStep_ = compare ? 1 : 2;
}

void BossPresentation::DrawNeonBossDeveloperTools()
{
#ifdef USE_IMGUI
    ImGui::TextUnformatted("本編のボス状態を3D表示へ渡します。HP・攻撃・衝突はEnemyが管理します。");
    ImGui::TextUnformatted("F7: ボス戦へ移動 / 再生成。現在の遠征を確認用ルートへ置き換え、自機を無敵にします。");
    if (world_.resources.player_ && world_.resources.player_->IsDead())
        ImGui::TextUnformatted("自機死亡中: F7または下のボタンで再出撃し、ボス戦から開始します。");
    if (world_.run.expeditionMapEnabled_ && ImGui::Button("本編ボス戦へ移動 / 再生成 (F7)"))
        RequestNeonBossDeveloperEncounter();
    if (ImGui::Checkbox("3D Neon Visual (OFF: existing tank)", &world_.resources.neonBossVisualEnabled_))
        if (world_.resources.neonBossVisual_)
            world_.resources.neonBossVisual_->SetEnabled(world_.resources.neonBossVisualEnabled_);
    ImGui::Checkbox("Freeze for visual comparison", &world_.presentation.developerBloomFreeze_);
    if (ImGui::Button("Reload Depth settings"))
        world_.depthEncounter->LoadNeonDepthConfig(true);
    ImGui::TextWrapped("%s", world_.resources.neonDepthConfigStatus_.c_str());
    if (world_.resources.enemy_ && world_.resources.enemy_->IsNeonDepthEncounterEnabled()) {
        const auto& depth = world_.resources.enemy_->GetNeonDepthSnapshot();
        ImGui::Text("Depth attack %d | phase %d | instance %llu", int(depth.plan.attack), int(depth.phase),
                    static_cast<unsigned long long>(depth.plan.instance));
        if (ImGui::Button("Skip entrance"))
            world_.resources.neonDepthSkipRequested_ = true;
        auto profile = world_.resources.neonBossVisual_->GetDepthProfile();
        if (ImGui::Checkbox("Reduced motion", &profile.reducedMotion))
            world_.resources.neonBossVisual_->SetDepthProfile(profile);
    }
    if (!world_.resources.enemy_ || !world_.gameplayQueries->IsRunRivalActive() || !world_.resources.neonBossVisual_)
        return;
    const auto rival = world_.resources.enemy_->GetRivalCombatStatus();
    ImGui::Text("HP %d / %d | Phase %d | Phase 2 %s | Shots %u", world_.resources.enemy_->GetHp(), world_.resources.enemy_->GetMaxHp(),
                static_cast<int>(rival.phase), rival.phase2 ? "YES" : "NO", world_.resources.enemy_->GetShotsFired());
    if (!world_.resources.enemy_->IsDead()) {
        if (ImGui::Button("HP -> 33% / Phase 2"))
            world_.resources.enemy_->TakeDamage(
                static_cast<uint32_t>((std::max)(0, world_.resources.enemy_->GetHp() - world_.resources.enemy_->GetMaxHp() / 3)));
        ImGui::SameLine();
        if (ImGui::Button("Kill / Directional Dissolve")) {
            world_.resources.enemy_->TakeDamage(static_cast<uint32_t>(world_.resources.enemy_->GetHp()));
            UpdateNeonBossVisual(0);
        }
    }
    ImGui::Text("Animation %s | Dissolve %.3f", world_.resources.neonBossVisual_->GetAnimationName().c_str(),
                world_.resources.neonBossVisual_->GetDissolveProgress());
    ImGui::TextWrapped("%s", world_.resources.neonBossVisual_->GetStatus().c_str());
    if (ImGui::Button("Capture gameplay"))
        world_.resources.neonBossCapture_.Request(kBossCaptureDirectory, "manual", {});
    ImGui::TextWrapped("%s", world_.resources.neonBossCapture_.GetStatus().c_str());
    const auto stats = world_.resources.neonBossVisual_->GetStats();
    ImGui::Text("Resources %u created / %u released | Animation selections %u", stats.resourceCreates, stats.resourceReleases,
                stats.animationSelections);
    if (ImGui::Button("Profile 3D ON (120 frames)")) {
        world_.resources.neonBossVisualEnabled_ = true;
        world_.resources.neonBossVisual_->SetEnabled(true);
        cg2::RuntimeProfiler::Get().StartCapture(std::string(kBossCaptureDirectory) + "/profile_on.csv", 120, 30);
    }
    ImGui::SameLine();
    if (ImGui::Button("Profile OFF (120 frames)")) {
        world_.resources.neonBossVisualEnabled_ = false;
        world_.resources.neonBossVisual_->SetEnabled(false);
        cg2::RuntimeProfiler::Get().StartCapture(std::string(kBossCaptureDirectory) + "/profile_off.csv", 120, 30);
    }
#endif
}
#endif

} // namespace gameplay
