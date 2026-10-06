// Developer fixtures observe actual actors and declare every scripted shortcut.
#include "GameScene.h"
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "Enemy.h"
#include "PlayerDrone.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
using nlohmann::json;
using gameplaytest::Scenario;
const char* AttackName(neondepth::Attack value) {
    switch(value) { case neondepth::Attack::Volley:return "volley";
    case neondepth::Attack::Dive:return "dive";case neondepth::Attack::Beam:return "beam"; }
    return "invalid";
}
const char* PhaseName(neondepth::Phase value) {
    switch(value) {
    case neondepth::Phase::Intro:return "intro";case neondepth::Phase::Reposition:return "reposition";
    case neondepth::Phase::Telegraph:return "telegraph";case neondepth::Phase::Locked:return "locked";
    case neondepth::Phase::Airborne:return "airborne";case neondepth::Phase::Active:return "active";
    case neondepth::Phase::Recovery:return "recovery";case neondepth::Phase::Defeated:return "defeated";
    case neondepth::Phase::Aborted:return "aborted"; }
    return "invalid";
}
json Vector(const cg2::Vector3& value) { return json::array({value.x,value.y,value.z}); }
json Matrix(const cg2::Matrix4x4& value) {
    json rows=json::array(); for(const auto& row:value.m) rows.push_back({row[0],row[1],row[2],row[3]}); return rows;
}
json Circle(const neondepth::Circle& value) { return {{"center",value.center},{"radius",value.radius}}; }
json Segment(const neondepth::Segment& value) {
    return {{"start",value.start},{"end",value.end},{"halfWidth",value.halfWidth}};
}
json DepthSnapshot(const neondepth::Snapshot& value,const neondepth::Events& events) {
    const auto& plan=value.plan;
    json circles=json::array(),warning=json::array(),cachedEvents=json::array();
    for(unsigned index=0;index<plan.circleCount && index<neondepth::kMaximumCircles;++index) circles.push_back(Circle(plan.circles[index]));
    for(unsigned index=0;index<plan.beam.warningCount && index<neondepth::kMaximumWarningSegments;++index) warning.push_back(Segment(plan.beam.warning[index]));
    for(unsigned index=0;index<events.count && index<events.values.size();++index) {
        const auto& event=events.values[index]; cachedEvents.push_back({{"kind",static_cast<int>(event.kind)},
            {"generation",event.generation},{"instance",event.instance},{"index",event.index}});
    }
    return {{"generation",value.generation},{"phase",PhaseName(value.phase)},{"elapsed",value.elapsed},
        {"duration",value.duration},{"progress",value.progress},{"planning",value.planning},
        {"vulnerable",value.vulnerable},{"inputLocked",value.inputLocked},{"phaseTwoPending",value.phaseTwoPending},
        {"desiredCore",value.desiredCore},{"activationEvents",value.activationEvents},
        {"contactClaims",value.contactClaims},{"acceptedHits",value.acceptedHits},
        {"activeCircleMask",value.activeCircleMask},{"activeBeamClipped",value.activeBeamClipped},
        {"activeBeam",Segment(value.activeBeam)},{"cachedEvents",cachedEvents},{"eventsAreAdapterCache",true},
        {"plan",{{"generation",plan.generation},{"instance",plan.instance},{"attack",AttackName(plan.attack)},
            {"phaseTwo",plan.phaseTwo},{"coreStart",plan.coreStart},{"coreTarget",plan.coreTarget},{"lockedTarget",plan.lockedTarget},
            {"damage",plan.damage},{"hitInterval",plan.hitInterval},{"circles",circles},{"dive",Circle(plan.dive)},
            {"duration",{{"telegraph",plan.duration.telegraph},{"locked",plan.duration.locked},{"airborne",plan.duration.airborne},
                {"active",plan.duration.active},{"recovery",plan.duration.recovery}}},
            {"beam",{{"origin",plan.beam.origin},{"startAngle",plan.beam.startAngle},{"endAngle",plan.beam.endAngle},
                {"range",plan.beam.range},{"halfWidth",plan.beam.halfWidth},{"warning",warning}}}}}};
}
void RequireDepth(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
}

void GameScene::InitializeNeonDepthValidation() {
    auto& session=GameplayScenarioSession::Get(); const auto& settings=session.GetSettings();
    RequireDepth(gameplaytest::IsDepthScenario(settings.scenario) && enemy_->IsNeonDepthEncounterEnabled(),
        "Depth fixture must use the ordinary configured real Enemy policy.");
    const bool realDamage=settings.scenario==Scenario::NeonDepthDamage || settings.scenario==Scenario::NeonDepthDodge ||
        settings.depthFixture.probe=="stationary_damage";
    const bool playerDeath=settings.depthFixture.probe=="player_death" || settings.depthFixture.probe=="simultaneous_death" ||
        settings.depthFixture.probe=="retry" || settings.depthFixture.probe=="title_return";
    debugPlayerNoDamage_=!realDamage && !playerDeath; player_->SetDebugNoDamage(debugPlayerNoDamage_);
    neonBossVisualEnabled_=settings.depthFixture.visualEnabled;
    RequireDepth(neonBossVisual_!=nullptr,"Depth fixture requires the initialized production Visual owner.");
    neonBossVisual_->SetEnabled(neonBossVisualEnabled_);
    auto profile=neonBossVisual_->GetDepthProfile(); // Read-only current config; never replace with a fabricated default.
    profile.reducedMotion=settings.depthFixture.reducedMotion;
    RequireDepth(neonBossVisual_->SetDepthProfile(profile),"Valid fixture reduced-motion profile was rejected.");
    neonDepthPreviousBossHp_=enemy_->GetHp();
    session.ReportDetail("depthFixture",MakeNeonDepthValidationConditions());
}

nlohmann::json GameScene::MakeNeonDepthValidationConditions() const {
    const auto& session=GameplayScenarioSession::Get(); const auto& settings=session.GetSettings();
    auto* dx=cg2::Object3dCommon::GetInstance()->GetDxCommon();
    const auto actual=MakeDeveloperGameCaptureMetadata(*dx);
    const bool bossDefeatRequested=settings.depthFixture.probe=="hp0" || settings.depthFixture.probe=="simultaneous_death";
    const bool playerLethalRequested=settings.depthFixture.probe=="player_death" || settings.depthFixture.probe=="simultaneous_death" ||
        settings.depthFixture.probe=="retry" || settings.depthFixture.probe=="title_return";
    auto applied=[&](const char* name) {
        const auto count=(std::min)(neonDepthOperations_.size(),size_t{64});
        for(size_t index=0;index<count;++index) {
            const auto& entry=neonDepthOperations_.at(index);
            if(entry.is_object() && entry.contains("operation") && entry.at("operation").is_string() && entry.at("operation")==name) return true;
        }
        return false;
    };
    // IScene exposes this observing query as nonconst; it does not mutate Scene.
    const auto showcase=const_cast<GameScene*>(this)->GetDeveloperShowcaseState();
    const auto* window=cg2::WinApp::GetInstance();
    const int clientWidth=window->GetClientWidth(),clientHeight=window->GetClientHeight();
    json clientToRenderScale=nullptr;
    if(clientWidth>0 && clientHeight>0) clientToRenderScale=json::array({
        double(cg2::WinApp::kClientWidth)/clientWidth,double(cg2::WinApp::kClientHeight)/clientHeight});
    json result={{"shortcutFixture",true},{"normalMainRoute",false},{"automatedInput",true},
        {"dynamicProbeInput",settings.input.empty()},{"initialPositionAssistance",false},
        {"playerInvulnerable",player_->IsDebugNoDamage()},{"initialRoomProtectionSeconds",.45},
        {"bossHpConfigured",settings.bossHp},{"playerHpConfigured",settings.playerHp},
        {"phaseTwoHpInjectionRequested",settings.depthFixture.injectPhaseTwo},{"phaseTwoHpInjected",neonDepthPhaseTwoInjected_},
        {"forcedBossDefeatRequested",bossDefeatRequested},{"forcedBossDefeat",applied("actual_enemy_die")},
        {"forcedPlayerLethalDamageRequested",playerLethalRequested},{"forcedPlayerLethalDamage",applied("actual_player_lethal_damage")},
        {"probe",settings.depthFixture.probe},{"visualEnabled",neonBossVisualEnabled_},
        {"reducedMotion",settings.depthFixture.reducedMotion},
        {"reducedMotionApplied",neonBossVisual_?json(neonBossVisual_->GetDepthProfile().reducedMotion):json(nullptr)},
        {"userPause",tankRunPaused_},
        {"introSkipPending",neonDepthSkipRequested_},
        {"comparisonFreeze",showcase.comparisonFreeze},{"HUDIncluded",true},
        {"debugUIIncluded",false},{"audio","none"},{"actualBossPolicy",enemy_->IsNeonDepthEncounterEnabled()?"depth":"legacy"},
        {"resolution",{cg2::WinApp::kClientWidth,cg2::WinApp::kClientHeight}},
        {"renderResolutionSource","WinApp fixed native swap-chain and viewport dimensions"},
        {"actualClientResolution",{clientWidth,clientHeight}},{"clientToRenderScale",clientToRenderScale},
        {"inputMapping","client pixel multiplied by native render/client scale"},
        {"cameraScope",{{"scoped",neonDepthCameraScoped_},
            {"saved",{{"position",Vector(neonDepthSavedCamera_.position)},{"rotation",Vector(neonDepthSavedCamera_.rotation)},
                {"fovY",neonDepthSavedCamera_.fovY},{"aspect",neonDepthSavedCamera_.aspect},
                {"nearClip",neonDepthSavedCamera_.nearClip},{"farClip",neonDepthSavedCamera_.farClip},
                {"jitter",{neonDepthSavedCamera_.jitter.x,neonDepthSavedCamera_.jitter.y}},
                {"debugCamera",neonDepthSavedDebugCamera_}}},
            {"current",{{"position",Vector(camera->GetTranslate())},{"rotation",Vector(camera->GetRotate())},
                {"fovY",camera->GetFovY()},{"aspect",camera->GetAspectRatio()},
                {"nearClip",camera->GetNearClip()},{"farClip",camera->GetFarClip()},
                {"jitter",{camera->GetProjectionJitter().x,camera->GetProjectionJitter().y}},
                {"debugCamera",cg2::Object3dCommon::GetInstance()->GetIsDebugCamera()}}}}},
        {"operations",neonDepthOperations_}};
    for(const char* key:{"globalPost","gpu","validation"}) if(actual.contains(key)) result[key]=actual.at(key);
    return result;
}

void GameScene::PrepareNeonDepthValidation() {
    auto& session=GameplayScenarioSession::Get(); const auto& settings=session.GetSettings();
    const auto& fixture=settings.depthFixture; const unsigned frame=session.GetFrame();
    const auto before=enemy_->GetNeonDepthSnapshot(); neonDepthForcedHpThisFrame_=false;
    auto operation=[&](const char* name) {
        RequireDepth(neonDepthOperations_.size()<64,"Depth operation evidence exceeded its bound.");
        neonDepthOperations_.push_back({{"operation",name},{"beforeCompletedFrame",frame+1},
            {"sceneEpoch",session.GetSceneEpoch()},{"generation",before.generation},{"instance",before.plan.instance},
            {"actualPhase",PhaseName(before.phase)},{"actualAttack",AttackName(before.plan.attack)}});
    };
    if(fixture.injectPhaseTwo && !neonDepthPhaseTwoInjected_ && neonDepthCycleOneMask_==7 &&
        before.phase==neondepth::Phase::Reposition && before.vulnerable) {
        const int target=(std::max)(1,enemy_->GetMaxHp()/3);
        if(enemy_->GetHp()>target) enemy_->TakeDamage(static_cast<uint32_t>(enemy_->GetHp()-target));
        RequireDepth(enemy_->GetHp()<=target && !enemy_->IsDead(),"Phase2 injection failed on the actual living core.");
        neonDepthPhaseTwoInjected_=neonDepthForcedHpThisFrame_=true; operation("phase2_hp_injection");
    }
    if(neonDepthPauseEnd_ && frame>=neonDepthPauseEnd_) tankRunPaused_=false;
    const bool match=fixture.phase==PhaseName(before.phase) && before.progress>=fixture.minimumProgress &&
        (before.phase==neondepth::Phase::Intro || before.phase==neondepth::Phase::Reposition || fixture.attack==AttackName(before.plan.attack));
    if(!neonDepthFixtureTriggered_ && settings.scenario==Scenario::NeonDepthLifecycle && match &&
        !(fixture.probe=="retry" && session.GetSceneEpoch()>1)) {
        const bool playerLethal=fixture.probe=="player_death" || fixture.probe=="simultaneous_death" ||
            fixture.probe=="retry" || fixture.probe=="title_return";
        if(playerLethal && player_->GetInvincibilityRemainingSeconds()>0) return;
        if(fixture.probe=="pause") {
            neonDepthPauseSnapshot_=before; neonDepthPauseBegin_=frame; neonDepthPauseEnd_=frame+60; tankRunPaused_=true;
            operation("user_pause_60_updates");
        } else if(fixture.probe=="abort") { enemy_->AbortNeonDepthEncounter(); operation("actual_enemy_abort"); }
        else if(fixture.probe=="hp0" || fixture.probe=="simultaneous_death") {
            enemy_->Die(); neonDepthForcedHpThisFrame_=true; operation("actual_enemy_die");
            RequireDepth(enemy_->GetHp()==0 && enemy_->IsDead(),"Actual Enemy::Die did not reach HP0.");
        } else if(fixture.probe=="intro_skip") {
            // Use the same requested input consumed by the normal keyboard/GUI
            // path, including its zero-tick player/combat lock on the skip frame.
            neonDepthSkipRequested_=true; operation("queued_intro_skip_input");
        }
        else if(fixture.probe=="multidraw" || fixture.probe=="stationary_damage") return;
        if(playerLethal) {
            debugPlayerNoDamage_=false; player_->SetDebugNoDamage(false);
            player_->TakeDamage((std::numeric_limits<uint32_t>::max)(),0);
            RequireDepth(player_->GetHp()==0 && player_->IsDead(),"Actual Player lethal damage did not reach HP0.");
            operation("actual_player_lethal_damage");
        }
        neonDepthFixtureTriggered_=true;
    }
    if((fixture.probe=="retry" || fixture.probe=="title_return") && session.GetSceneEpoch()==1 && player_->IsDead() &&
        combatFlow_.GetState()==GameFlowState::GameOver && combatFlow_.GetTimer()<=0 && phase_==Phase::kMain) {
        resultSelection_=fixture.probe=="retry"?0:1; ConfirmResultSelection();
        operation(fixture.probe=="retry"?"actual_confirm_retry":"actual_confirm_title_return");
    }
    if(settings.input.empty() && !player_->IsDead() && !tankRunPaused_) {
        cg2::Vector2 movement{}; bool shoot=false,dash=false;
        const auto core=enemy_->GetWorldPosition(),position=player_->GetWorldPosition();
        if(settings.scenario==Scenario::NeonDepthDamage) {
            const auto direction=core-position; const float length=cg2::Length(direction);
            const float range=player_->GetExpeditionCombatStyle()==tankbuild::Style::Melee?2.8f:7;
            if(length>range && length>.001f) movement={direction.x/length,direction.y/length};
            shoot=before.vulnerable;
        } else if(settings.scenario==Scenario::NeonDepthDodge) {
            const bool retreat=before.phase==neondepth::Phase::Locked || before.phase==neondepth::Phase::Airborne || before.phase==neondepth::Phase::Active;
            if(retreat) {
                // Read the committed plan. This helper changes only regular input,
                // never the actual player position, shape or attack clock.
                cg2::Vector2 away{position.x-before.plan.lockedTarget[0],position.y-before.plan.lockedTarget[1]};
                float length=std::sqrt(away.x*away.x+away.y*away.y);
                if(length<.001f) { away={0,-1}; length=1; }
                movement={away.x/length,away.y/length};
                if(before.phase==neondepth::Phase::Locked && neonDepthDashInstance_!=before.plan.instance) {
                    dash=true; neonDepthDashInstance_=before.plan.instance;
                }
            }
        }
        player_->SetDemoInput(true,movement,core,shoot,dash);
    }
}

void GameScene::RecordNeonDepthValidation() {
    auto& session=GameplayScenarioSession::Get();
    const auto& snapshot=enemy_->GetNeonDepthSnapshot();
    if(enemy_->GetHp()<neonDepthPreviousBossHp_ && !neonDepthForcedHpThisFrame_)
        neonDepthRealCoreDamage_+=static_cast<uint64_t>(neonDepthPreviousBossHp_-enemy_->GetHp());
    neonDepthPreviousBossHp_=enemy_->GetHp();
    if(snapshot.phase==neondepth::Phase::Recovery && snapshot.plan.instance) {
        const unsigned bit=1u<<static_cast<unsigned>(snapshot.plan.attack);
        (snapshot.plan.phaseTwo?neonDepthCycleTwoMask_:neonDepthCycleOneMask_)|=bit;
    }
    if(neonDepthPauseEnd_ && session.GetFrame()<=neonDepthPauseEnd_) {
        if(!(snapshot==neonDepthPauseSnapshot_)) session.Fail("User pause advanced the actual Depth gameplay snapshot.");
    } else if(neonDepthPauseEnd_ && (snapshot.elapsed!=neonDepthPauseSnapshot_.elapsed || snapshot.phase!=neonDepthPauseSnapshot_.phase))
        neonDepthResumeSeen_=true;
    const auto combat=player_->GetRunCombatSnapshot();
    json gameplay={{"boss",DepthSnapshot(snapshot,enemy_->GetNeonDepthEvents())},{"bossHp",enemy_->GetHp()},
        {"bossMaxHp",enemy_->GetMaxHp()},{"bossDead",enemy_->IsDead()},{"bossPosition",Vector(enemy_->GetWorldPosition())},
        {"playerHp",player_->GetHp()},{"playerMaxHp",player_->GetMaxHp()},{"playerDead",player_->IsDead()},
        {"playerPosition",Vector(player_->GetWorldPosition())},{"playerStyle",static_cast<int>(combat.style)},
        {"playerRadius",player_->GetRadius()},{"coreRadius",enemy_->GetRadius()},
        {"primaryAttacks",player_->GetPrimaryAttackCount()},{"dashStarts",player_->GetDashStartedCount()},
        {"damageTakenCount",player_->GetDamageTakenCount()},{"justDodgeCount",player_->GetPerfectDodgeCount()},
        {"invincibilitySeconds",player_->GetInvincibilityRemainingSeconds()},{"justEvading",player_->IsJustEvading()},
        {"dashing",player_->IsDashing()},{"activeDrones",combat.activeDrones},{"homing",combat.homing},
        {"contactClaims",enemy_->GetNeonDepthContactClaims()},{"acceptedHits",enemy_->GetNeonDepthAcceptedHits()},
        {"flow",static_cast<int>(combatFlow_.GetState())},{"bossDefeatHandled",bossDefeatHandled_},
        {"playerDeathHandled",playerDeathHandled_},{"actualCoreDamageExcludingInjection",neonDepthRealCoreDamage_}};
    // Record after the actual Update. Draw totals include submitted prior-frame
    // draws; this observer never updates/allocates/releases presentation resources.
    json effects=nullptr;
    if(neonDepthEffects_) {
        const auto& stats=neonDepthEffects_->GetStats();
        effects={{"hasResources",neonDepthEffects_->HasResources()},
            {"resourceCreates",stats.resourceCreates},{"resourceReleases",stats.resourceReleases},
            {"resourceFailures",stats.resourceFailures},{"updates",stats.updates},
            {"duplicateUpdates",stats.duplicateUpdates},{"invalidFrames",stats.invalidFrames},
            {"capacityRejected",stats.capacityRejected},{"depthBindingRejected",stats.depthBindingRejected},
            {"airDraws",stats.airDraws},{"floorDraws",stats.floorDraws},
            {"vertices",stats.vertices},{"airVertices",stats.airVertices},{"floorVertices",stats.floorVertices},
            {"lineCommands",stats.lineCommands},{"fillCommands",stats.fillCommands},{"launchLatches",stats.launchLatches}};
    }
    json presentation={{"conditions",MakeNeonDepthValidationConditions()},{"cameraWorld",Matrix(camera->GetWorldMatrix())},
        {"viewProjection",Matrix(camera->GetViewProjectionMatrix())},{"cameraPosition",Vector(camera->GetTranslate())},
        {"cameraRotation",Vector(camera->GetRotate())},{"visualHasResources",neonBossVisual_->HasResources()},
        {"visualFinished",neonBossVisual_->IsFinished()},{"effects",effects},
        {"visualLife",static_cast<int>(neonBossVisual_->GetDepthLife())},{"animation",neonBossVisual_->GetAnimationName()},
        {"animationTime",neonBossVisual_->GetAnimationTime()},{"dissolveProgress",neonBossVisual_->GetDissolveProgress()},
        {"world",Matrix(neonBossVisual_->GetWorldMatrix())},{"modelUpdates",neonBossVisual_->GetStats().modelUpdates},
        {"visualDraws",neonBossVisual_->GetStats().draws},{"visualCreates",neonBossVisual_->GetStats().resourceCreates},
        {"visualReleases",neonBossVisual_->GetStats().resourceReleases},{"poseFreezes",neonBossVisual_->GetStats().depthPoseFreezes},
        {"drawConstantBuffers",neonBossVisual_->GetDrawConstantBufferCount()}};
    session.ReportDepthFrame({{"frame",session.GetFrame()},{"sceneEpoch",session.GetSceneEpoch()},
        {"gameplay",gameplay},{"presentation",presentation},{"actualGameplayDelta",gameplayScenarioCombatDt_},
        {"actualPresentationDelta",gameplayScenarioPresentationDt_}});
}

void GameScene::DrawNeonDepthValidation() {
    auto& session=GameplayScenarioSession::Get();
    if(!session.IsActive() || session.IsFinished() || !gameplaytest::IsDepthScenario(session.GetSettings().scenario) ||
        session.GetSettings().depthFixture.probe!="multidraw" || developerBloomFreeze_ || neonBossDeveloperFreeze_ ||
        !neonBossVisual_ || !neonBossVisual_->IsVisible()) return;
    const auto before=enemy_->GetNeonDepthSnapshot(); const auto modelUpdates=neonBossVisual_->GetStats().modelUpdates;
    const auto draws=neonBossVisual_->GetStats().draws; const auto playerHp=player_->GetHp();
    const auto primary=player_->GetPrimaryAttackCount(),damage=player_->GetDamageTakenCount();
    const auto position=player_->GetWorldPosition();
    // The ordinary Update reset the arena after the previous frame fence.
    // Each Draw uses a distinct slot; do not BeginFrame/Update between commands.
    neonBossVisual_->Draw(); neonBossVisual_->Draw();
    const auto afterPosition=player_->GetWorldPosition();
    const bool unchanged=before==enemy_->GetNeonDepthSnapshot() && playerHp==player_->GetHp() &&
        primary==player_->GetPrimaryAttackCount() && damage==player_->GetDamageTakenCount() &&
        position.x==afterPosition.x && position.y==afterPosition.y && position.z==afterPosition.z;
    const bool modelUnchanged=modelUpdates==neonBossVisual_->GetStats().modelUpdates;
    session.ReportDepthDraw({{"extraDrawCalls",2},{"actualVisibleDraws",neonBossVisual_->GetStats().draws-draws},
        {"gameplayUnchanged",unchanged},{"modelUpdatesUnchanged",modelUnchanged},
        {"constantBuffersAfter",neonBossVisual_->GetDrawConstantBufferCount()}});
    if(!unchanged || !modelUnchanged) session.Fail("Actual extra Depth Draws changed gameplay or model sampling.");
    neonDepthFixtureTriggered_=true;
}

void GameScene::FinishNeonDepthValidation() {
    auto& session=GameplayScenarioSession::Get(); const auto& settings=session.GetSettings(); const auto& fixture=settings.depthFixture;
    if(settings.scenario==Scenario::NeonDepthCycles || settings.scenario==Scenario::NeonDepthParity) {
        if(neonDepthCycleOneMask_!=7 || (fixture.injectPhaseTwo && neonDepthCycleTwoMask_!=7))
            session.Fail("Depth runtime did not complete all three actual attacks/recoveries in the requested phases.");
    } else if(settings.scenario==Scenario::NeonDepthDamage) {
        if(neonDepthRealCoreDamage_==0 || player_->GetPrimaryAttackCount()==0 || player_->IsDebugNoDamage())
            session.Fail("This style did not damage the actual Depth core through ordinary attacks.");
    } else if(settings.scenario==Scenario::NeonDepthLifecycle) {
        if(fixture.probe=="retry") {
            if(session.GetSceneEpoch()<2 || player_->IsDead()) session.Fail("The actual retry scene did not initialize a living player.");
        } else if(fixture.probe=="title_return") session.Fail("Deadline reached without actual initialized TITLE notification.");
        else if(fixture.probe=="pause" && !neonDepthResumeSeen_) session.Fail("Actual user pause/resume was not demonstrated.");
        else if(fixture.probe!="stationary_damage" && !neonDepthFixtureTriggered_) session.Fail("Requested real lifecycle phase was never reached.");
        if((fixture.probe=="hp0" || fixture.probe=="simultaneous_death" || fixture.probe=="abort") &&
            (neonBossVisual_->HasResources() || !neonBossVisual_->IsFinished()))
            session.Fail("Depth terminal retained presentation resources.");
        if(fixture.probe=="simultaneous_death" && (combatFlow_.GetState()!=GameFlowState::GameOver || bossDefeatHandled_))
            session.Fail("Expedition simultaneous death lost its existing player-death priority.");
    }
    session.ReportDetail("depthFinal",{{"phaseOneAttackMask",neonDepthCycleOneMask_},{"phaseTwoAttackMask",neonDepthCycleTwoMask_},
        {"actualCoreDamageExcludingInjection",neonDepthRealCoreDamage_},{"probeTriggered",neonDepthFixtureTriggered_},
        {"actualUserPauseResumed",neonDepthResumeSeen_},{"conditions",MakeNeonDepthValidationConditions()}});
}
#endif
