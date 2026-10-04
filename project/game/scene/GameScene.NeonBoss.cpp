#include "GameScene.h"
#include "Enemy.h"
#include "RuntimeProfiler.h"
#include <cmath>
#include <fstream>
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG) && defined(USE_IMGUI)
#include "externals/imgui/imgui.h"
#endif

void GameScene::UpdateNeonBossVisual(float deltaTime) {
    if (!neonBossVisual_ || !enemy_) return;
    cg2::RuntimeProfiler::CpuScope scope("Neon Boss Update");
    if (neonBossEncounterGeneration_ != enemy_->GetEncounterGeneration()) {
        neonBossVisual_->ResetEncounter();
        neonBossEncounterGeneration_ = enemy_->GetEncounterGeneration();
        neonBossPreviousShots_ = enemy_->GetShotsFired();
        neonBossAttackPresentationTimer_ = 0;
        neonBossPreviousPosition_ = enemy_->GetWorldPosition();
    }
    NeonBossVisualInput input;
    input.position = enemy_->GetWorldPosition();
    const auto aim = enemy_->GetAimDirection();
    input.aimAngleRadians = std::atan2(aim.y, aim.x);
    input.radius = enemy_->GetRadius();
    input.hp = enemy_->GetHp(); input.maxHp = enemy_->GetMaxHp();
    input.alive = !enemy_->IsDead() && input.hp > 0;
    input.encounterActive = IsRunRivalActive() && enemy_->IsRunEncounterEnabled() && !IsTutorialCombatSuppressed();
    input.damageFeedback = enemy_->GetDamageFeedbackRatio();
    const auto rival = enemy_->GetRivalCombatStatus();
    input.phaseTwo = rival.enabled ? rival.phase2 : input.hp <= input.maxHp / 2;
    if (rival.enabled) {
        using P = RivalBossCombat::Phase;
        switch (rival.phase) {
        case P::Reposition: input.action = NeonBossAction::Reposition; break;
        case P::Tracking: case P::Locked: case P::DashWarning: input.action = NeonBossAction::Telegraph; break;
        case P::Volley: input.action = NeonBossAction::Attack; break;
        case P::Dash: input.action = NeonBossAction::Dash; break;
        case P::Reload: input.action = NeonBossAction::Reload; break;
        }
    } else if (enemy_->IsPrototypeCombatEnabled()) {
        using P = PrototypeBossCombat::Phase;
        switch (enemy_->GetPrototypeCombatPhase()) {
        case P::Recovery: input.action = NeonBossAction::Idle; break;
        case P::Telegraph: input.action = NeonBossAction::Telegraph; break;
        case P::Attack: input.action = NeonBossAction::Attack; break;
        }
    } else {
        const auto displacement = input.position - neonBossPreviousPosition_;
        if (cg2::Length(displacement) > 0.001f) input.action = NeonBossAction::Reposition;
    }
    // AimedSpread can fire and return to Recovery in one gameplay tick.
    if (enemy_->GetShotsFired() != neonBossPreviousShots_)
        neonBossAttackPresentationTimer_ = 0.70f;
    else if (deltaTime > 0)
        neonBossAttackPresentationTimer_ = (std::max)(0.0f, neonBossAttackPresentationTimer_ - deltaTime);
    if (!rival.enabled && neonBossAttackPresentationTimer_ > 0 &&
        (input.action == NeonBossAction::Idle || input.action == NeonBossAction::Reposition))
        input.action = NeonBossAction::Attack;
    neonBossPreviousShots_ = enemy_->GetShotsFired();
    neonBossPreviousPosition_ = input.position;
    neonBossVisual_->SetEnabled(neonBossVisualEnabled_);
    neonBossVisual_->Update(input, deltaTime);
}

bool GameScene::UseNeonBossVisual() const {
    return neonBossVisualEnabled_ && neonBossVisual_ && IsRunRivalActive() &&
        (neonBossVisual_->HasResources() || neonBossVisual_->IsFinished());
}

void GameScene::DrawNeonBossVisual() {
    if (!UseNeonBossVisual() || IsTutorialCombatSuppressed()) return;
    neonBossVisual_->Draw();
}

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
namespace {
constexpr const char* kBossCaptureDirectory = "generated/neon_boss_gameplay";
}

void GameScene::StartNeonBossDeveloperEncounter() {
    if (!expeditionMapEnabled_) return;
    if (player_->IsDead()) {
        expeditionMapStatus_ = "自機が死亡しています。結果画面から再出撃してボス確認を開いてください。";
        neonBossValidationErrors_.push_back("Developer encounter requires a living player; restart from the result screen.");
        return;
    }
    // Copy an authored boss room into a one-node in-memory run. Use the ordinary
    // room entry/spawn path; never save over the player's map or room definitions.
    const auto& definition = expeditionMapRun_.GetDefinition();
    const auto found = std::find_if(definition.nodes.begin(), definition.nodes.end(),
        [](const auto& node) { return node.kind == tankexp::NodeKind::Boss; });
    if (found == definition.nodes.end()) { neonBossValidationErrors_.push_back("No authored boss node."); return; }
    auto node = *found; node.next.clear(); node.column = 0; node.row = 0;
    tankexp::MapDefinition fixture; fixture.startNodes = {node.id}; fixture.nodes = {node};
    std::string error;
    if (!expeditionMapRun_.Reset(fixture, error)) { neonBossValidationErrors_.push_back(error); return; }
    // Map UI owns one visual per definition node/edge. Retain one existing node
    // slot and drop stale edges before ordinary UI refresh can dereference them.
    // This entry runs after the previous frame fence completed.
    expeditionMapVisuals_.resize(1);
    expeditionMapEdges_.clear();
    expeditionMapSelection_ = node.id; expeditionMapScroll_ = 0;
    tankExpeditionTutorial_.Skip(); expeditionGuideActive_ = false;
    expeditionBuildChoice_ = false; expeditionBuildChosen_ = true;
    expeditionAuthoringHubOpen_ = false; tankRunPaused_ = false;
    expeditionTransition_ = {}; expeditionCollectAll_ = false;
    gameFlowState_ = GameFlowState::Playing; gameFlowTimer_ = 0;
    bossDefeatHandled_ = bossDefeatImpactTriggered_ = false;
    tankRunMenuAge_ = 1;
    EnterExpeditionMapNode(node.id);
    debugPlayerNoDamage_ = true; player_->SetDebugNoDamage(true);
    if (neonBossAutoTest_) {
        tankExpeditionMusicEnabled_ = tankExpeditionEffectsEnabled_ = false;
        tankExpeditionAudio_.SetMusicVolume(0); tankExpeditionAudio_.SetEffectsVolume(0);
        // Keep the existing camera while ensuring both actors are in its viewport.
        const auto position = enemy_->GetWorldPosition() + cg2::Vector3{-14,0,0};
        player_->ResetRunRoomState(position);
        player_->SetDemoInput(true, {}, enemy_->GetWorldPosition(), false, false);
        const auto center = (position + enemy_->GetWorldPosition()) * 0.5f;
        camera->SetTranslate({center.x,center.y,-55}); camera->Update();
    }
    neonBossVisualEnabled_ = true;
    UpdateNeonBossVisual(0);
    RefreshTankExpeditionUi();
}

nlohmann::json GameScene::MakeNeonBossMetadata() const {
    auto vector = [](const cg2::Vector3& p) { return nlohmann::json::array({p.x,p.y,p.z}); };
    const auto rival = enemy_->GetRivalCombatStatus();
    const auto stats = neonBossVisual_->GetStats();
    nlohmann::json data = {
        {"scene","GameScene / authored expedition boss"},{"build","Development"},
        {"visualEnabled",neonBossVisualEnabled_},{"samePoseFreeze",neonBossDeveloperFreeze_},
        {"encounterGeneration",enemy_->GetEncounterGeneration()},
        {"gameplay",{{"hp",enemy_->GetHp()},{"maxHp",enemy_->GetMaxHp()},{"dead",enemy_->IsDead()},
            {"position",vector(enemy_->GetWorldPosition())},{"aim",vector(enemy_->GetAimDirection())},
            {"phase",static_cast<int>(rival.phase)},{"phase2",rival.phase2},{"shots",enemy_->GetShotsFired()},
            {"dashCount",rival.dashCount}}},
        {"presentation",{{"animation",neonBossVisual_->GetAnimationName()},
            {"animationTime",neonBossVisual_->GetAnimationTime()},
            {"action",static_cast<int>(neonBossVisual_->GetState().GetAction())},
            {"life",static_cast<int>(neonBossVisual_->GetState().GetLife())},
            {"dissolveProgress",neonBossVisual_->GetDissolveProgress()},
            {"hasResources",neonBossVisual_->HasResources()},{"finished",neonBossVisual_->IsFinished()},
            {"resourceCreates",stats.resourceCreates},{"resourceReleases",stats.resourceReleases},
            {"drawConstantBuffers",neonBossVisual_->GetDrawConstantBufferCount()},
            {"animationSelections",stats.animationSelections},{"status",neonBossVisual_->GetStatus()}}},
        {"camera",{{"position",vector(camera->GetTranslate())},{"rotation",vector(camera->GetRotate())}}},
        {"descriptorCount",cg2::Object3dCommon::GetInstance()->GetSrvManager()->GetAllocatedCount()},
        {"globalBloomModified",false}
    };
    for (const auto& row : camera->GetViewProjectionMatrix().m)
        data["camera"]["viewProjection"].push_back({row[0],row[1],row[2],row[3]});
    for (const auto& row : neonBossVisual_->GetWorldMatrix().m)
        data["presentation"]["world"].push_back({row[0],row[1],row[2],row[3]});
    return data;
}

void GameScene::RecordNeonBossDeveloperFrame(cg2::DirectXCommon& dx) {
    if (neonBossCapture_.HasRequest()) {
        auto metadata = MakeNeonBossMetadata();
        const auto frame = MakeDeveloperGameCaptureMetadata(dx);
        for (const char* key : {"gpu", "queueTimestampFrequencyHz", "validation"})
            if (frame.contains(key)) metadata[key] = frame.at(key);
        metadata["comparisonTemporalAA"] = "Existing comparisonFreeze temporarily disables TAA/jitter; normal gameplay settings are preserved.";
        neonBossCapture_.SetFrameMetadata(std::move(metadata));
    }
    neonBossCapture_.Record(dx);
}

void GameScene::WriteNeonBossValidation(bool completed) {
    std::filesystem::create_directories(kBossCaptureDirectory);
    nlohmann::json report = MakeNeonBossMetadata();
    report["completed"] = completed; report["testMode"] = true;
    report["errors"] = neonBossValidationErrors_; report["captures"] = neonBossValidationCaptures_;
    report["elapsed"] = neonBossValidationAge_;
    report["fixture"] = "Actual authored boss room and RivalBossCombat; invulnerable stationary player. HP injected for phase/death coverage.";
    std::ofstream(std::string(kBossCaptureDirectory) + "/validation.json") << report.dump(2) << '\n';
}

void GameScene::UpdateNeonBossDeveloperValidation() {
    neonBossCapture_.Resolve(*cg2::Object3dCommon::GetInstance()->GetDxCommon());
    if (neonBossDeveloperStartPending_ && phase_ == Phase::kMain) {
        neonBossDeveloperStartPending_ = false;
        StartNeonBossDeveloperEncounter();
        if (neonBossAutoTest_) WriteNeonBossValidation(false);
    }
    if (!neonBossAutoTest_ || !neonBossVisual_ || neonBossDeveloperStartPending_) return;
    neonBossValidationAge_ += 1.0f / 60.0f;
    if (neonBossValidationAge_ > 60) {
        neonBossValidationErrors_.push_back("Timed out waiting for actual boss phases/captures.");
        WriteNeonBossValidation(false); PostQuitMessage(9); neonBossAutoTest_ = false; return;
    }
    if (neonBossCaptureStep_) {
        if (neonBossCapture_.IsBusy()) return;
        if (!neonBossCapture_.WasLastCaptureSuccessful()) neonBossValidationErrors_.push_back(neonBossCapture_.GetStatus());
        if (neonBossCaptureStep_ == 1) {
            neonBossVisualEnabled_ = true; neonBossVisual_->SetEnabled(true);
            neonBossCapture_.Request(kBossCaptureDirectory, neonBossCaptureName_, {});
            neonBossValidationCaptures_.push_back(neonBossCaptureName_);
            neonBossCaptureStep_ = 2; return;
        }
        neonBossCaptureStep_ = 0; neonBossDeveloperFreeze_ = false; ++neonBossProbe_;
        WriteNeonBossValidation(false);
        if (neonBossProbe_ == 1 && cg2::RuntimeProfiler::Get().IsAllowed()) {
            // Same camera, pose, effects and collider for this A/B cost sample.
            neonBossDeveloperFreeze_ = true;
            neonBossVisualEnabled_ = false; neonBossVisual_->SetEnabled(false);
            if (cg2::RuntimeProfiler::Get().StartCapture(std::string(kBossCaptureDirectory)+"/profile_off.csv",120,30))
                neonBossProfileStep_ = 1;
            else neonBossDeveloperFreeze_ = false;
        }
    }
    if (neonBossProfileStep_) {
        if (!cg2::RuntimeProfiler::Get().IsCaptureComplete()) return;
        if (neonBossProfileStep_ == 1) {
            neonBossVisualEnabled_ = true; neonBossVisual_->SetEnabled(true);
            if (cg2::RuntimeProfiler::Get().StartCapture(std::string(kBossCaptureDirectory)+"/profile_on.csv",120,30)) {
                neonBossProfileStep_ = 2; return;
            }
        }
        neonBossProfileStep_ = 0; neonBossDeveloperFreeze_ = false;
    }
    if (!IsRunRivalActive()) return;
    const auto rival = enemy_->GetRivalCombatStatus();
    if (enemy_->IsDead() && neonBossProbe_ >= 6 &&
        (enemy_->GetShotsFired() != neonBossDeadShots_ || rival.dashCount != neonBossDeadDashCount_ ||
            cg2::Length(enemy_->GetWorldPosition() - neonBossDeadPosition_) > 0.0001f))
        neonBossValidationErrors_.push_back("Gameplay changed during death dissolve.");
    std::string name;
    switch (neonBossProbe_) {
    case 0:
        // Let the existing bind-to-idle blend finish and entry flash settle.
        if (neonBossVisual_->HasResources() && neonBossVisual_->GetAnimationTime() >= 0.45f) name = "idle";
        break;
    case 1: if (rival.phase == RivalBossCombat::Phase::Locked) name = "telegraph"; break;
    case 2:
        if (rival.phase == RivalBossCombat::Phase::Volley && rival.shotsFired > 0 &&
            neonBossVisual_->GetAnimationTime() >= 0.80f) name = "attack";
        break;
    case 3: if (rival.phase == RivalBossCombat::Phase::Dash) name = "dash"; break;
    case 4:
        if (!rival.phase2 && enemy_->GetHp() > enemy_->GetMaxHp() / 2)
            enemy_->TakeDamage(static_cast<uint32_t>(enemy_->GetHp() - enemy_->GetMaxHp() / 3));
        if (rival.phase2) name = "low_hp";
        break;
    case 5:
        if (!enemy_->IsDead()) {
            enemy_->TakeDamage(static_cast<uint32_t>(enemy_->GetHp()));
            neonBossDeadShots_ = enemy_->GetShotsFired(); neonBossDeadDashCount_ = rival.dashCount;
            neonBossDeadPosition_ = enemy_->GetWorldPosition();
            UpdateNeonBossVisual(0); // Record the exact zero endpoint before advancing time.
        }
        name = "dissolve_000"; break;
    case 6: if (neonBossVisual_->GetDissolveProgress() >= 0.50f) name = "dissolve_050"; break;
    case 7: if (neonBossVisual_->IsFinished()) name = "dissolve_100"; break;
    default:
        if (neonBossVisual_->HasResources()) neonBossValidationErrors_.push_back("Terminal visual retained resources.");
        if (neonBossVisual_->GetStats().resourceCreates != 1 || neonBossVisual_->GetStats().resourceReleases != 1)
            neonBossValidationErrors_.push_back("Unexpected resource lifecycle count.");
        WriteNeonBossValidation(neonBossValidationErrors_.empty());
        PostQuitMessage(neonBossValidationErrors_.empty() ? 0 : 9); neonBossAutoTest_ = false; return;
    }
    if (name.empty()) return;
    neonBossDeveloperFreeze_ = true; neonBossCaptureName_ = name;
    const bool compare = neonBossProbe_ <= 4;
    neonBossVisualEnabled_ = !compare; neonBossVisual_->SetEnabled(!compare);
    const auto capture = compare ? name + "_legacy" : name;
    neonBossCapture_.Request(kBossCaptureDirectory, capture, {});
    neonBossValidationCaptures_.push_back(capture); neonBossCaptureStep_ = compare ? 1 : 2;
}

void GameScene::DrawNeonBossDeveloperTools() {
#ifdef USE_IMGUI
    ImGui::TextUnformatted("本編のボス状態を3D表示へ渡します。HP・攻撃・衝突はEnemyが管理します。");
    if (player_ && player_->IsDead()) ImGui::TextUnformatted("自機死亡中: 結果画面から再出撃してください。");
    if (expeditionMapEnabled_ && ImGui::Button("本編ボス戦へ移動 / 再生成")) neonBossDeveloperStartPending_ = true;
    if (ImGui::Checkbox("3D Neon Visual (OFF: existing tank)", &neonBossVisualEnabled_))
        if (neonBossVisual_) neonBossVisual_->SetEnabled(neonBossVisualEnabled_);
    ImGui::Checkbox("Freeze for visual comparison", &developerBloomFreeze_);
    if (!enemy_ || !IsRunRivalActive() || !neonBossVisual_) return;
    const auto rival = enemy_->GetRivalCombatStatus();
    ImGui::Text("HP %d / %d | Phase %d | Phase 2 %s | Shots %u", enemy_->GetHp(), enemy_->GetMaxHp(),
        static_cast<int>(rival.phase), rival.phase2 ? "YES" : "NO", enemy_->GetShotsFired());
    if (!enemy_->IsDead()) {
        if (ImGui::Button("HP -> 33% / Phase 2")) enemy_->TakeDamage(static_cast<uint32_t>((std::max)(0, enemy_->GetHp() - enemy_->GetMaxHp() / 3)));
        ImGui::SameLine();
        if (ImGui::Button("Kill / Directional Dissolve")) {
            enemy_->TakeDamage(static_cast<uint32_t>(enemy_->GetHp())); UpdateNeonBossVisual(0);
        }
    }
    ImGui::Text("Animation %s | Dissolve %.3f", neonBossVisual_->GetAnimationName().c_str(), neonBossVisual_->GetDissolveProgress());
    ImGui::TextWrapped("%s", neonBossVisual_->GetStatus().c_str());
    if (ImGui::Button("Capture gameplay")) neonBossCapture_.Request(kBossCaptureDirectory,"manual",{});
    ImGui::TextWrapped("%s",neonBossCapture_.GetStatus().c_str());
    const auto stats = neonBossVisual_->GetStats();
    ImGui::Text("Resources %u created / %u released | Animation selections %u",stats.resourceCreates,stats.resourceReleases,stats.animationSelections);
    if (ImGui::Button("Profile 3D ON (120 frames)")) {
        neonBossVisualEnabled_ = true; neonBossVisual_->SetEnabled(true);
        cg2::RuntimeProfiler::Get().StartCapture(std::string(kBossCaptureDirectory)+"/profile_on.csv",120,30);
    }
    ImGui::SameLine();
    if (ImGui::Button("Profile OFF (120 frames)")) {
        neonBossVisualEnabled_ = false; neonBossVisual_->SetEnabled(false);
        cg2::RuntimeProfiler::Get().StartCapture(std::string(kBossCaptureDirectory)+"/profile_off.csv",120,30);
    }
#endif
}
#endif
