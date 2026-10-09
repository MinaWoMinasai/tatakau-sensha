#include "game/debug/session/DepthValidation.h"
#include "game/session/GameplaySystems.h"
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "Enemy.h"
#endif
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "PlayerDrone.h"
#endif
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include <cmath>
#endif
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include <limits>
#endif
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include <stdexcept>
#endif

namespace gameplay {
// Developer fixtures observe actual actors and declare every scripted shortcut.
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)

namespace {
using gameplaytest::Scenario;
using nlohmann::json;
const char* AttackName(neondepth::Attack value)
{
    switch (value) {
    case neondepth::Attack::Volley:
        return "volley";
    case neondepth::Attack::Dive:
        return "dive";
    case neondepth::Attack::Beam:
        return "beam";
    }
    return "invalid";
}
const char* PhaseName(neondepth::Phase value)
{
    switch (value) {
    case neondepth::Phase::Intro:
        return "intro";
    case neondepth::Phase::Reposition:
        return "reposition";
    case neondepth::Phase::Telegraph:
        return "telegraph";
    case neondepth::Phase::Locked:
        return "locked";
    case neondepth::Phase::Airborne:
        return "airborne";
    case neondepth::Phase::Active:
        return "active";
    case neondepth::Phase::Recovery:
        return "recovery";
    case neondepth::Phase::Defeated:
        return "defeated";
    case neondepth::Phase::Aborted:
        return "aborted";
    }
    return "invalid";
}
json Vector(const cg2::Vector3& value)
{
    return json::array({value.x, value.y, value.z});
}
json Matrix(const cg2::Matrix4x4& value)
{
    json rows = json::array();
    for (const auto& row : value.m)
        rows.push_back({row[0], row[1], row[2], row[3]});
    return rows;
}
json Circle(const neondepth::Circle& value)
{
    return {{"center", value.center}, {"radius", value.radius}};
}
json Segment(const neondepth::Segment& value)
{
    return {{"start", value.start}, {"end", value.end}, {"halfWidth", value.halfWidth}};
}
json DepthSnapshot(const neondepth::Snapshot& value, const neondepth::Events& events)
{
    const auto& plan = value.plan;
    json circles = json::array(), warning = json::array(), cachedEvents = json::array();
    for (unsigned index = 0; index < plan.circleCount && index < neondepth::kMaximumCircles; ++index)
        circles.push_back(Circle(plan.circles[index]));
    for (unsigned index = 0; index < plan.beam.warningCount && index < neondepth::kMaximumWarningSegments; ++index)
        warning.push_back(Segment(plan.beam.warning[index]));
    for (unsigned index = 0; index < events.count && index < events.values.size(); ++index) {
        const auto& event = events.values[index];
        cachedEvents.push_back({{"kind", static_cast<int>(event.kind)},
                                {"generation", event.generation},
                                {"instance", event.instance},
                                {"index", event.index}});
    }
    return {{"generation", value.generation},
            {"phase", PhaseName(value.phase)},
            {"elapsed", value.elapsed},
            {"duration", value.duration},
            {"progress", value.progress},
            {"planning", value.planning},
            {"vulnerable", value.vulnerable},
            {"inputLocked", value.inputLocked},
            {"phaseTwoPending", value.phaseTwoPending},
            {"desiredCore", value.desiredCore},
            {"activationEvents", value.activationEvents},
            {"contactClaims", value.contactClaims},
            {"acceptedHits", value.acceptedHits},
            {"activeCircleMask", value.activeCircleMask},
            {"activeBeamClipped", value.activeBeamClipped},
            {"activeBeam", Segment(value.activeBeam)},
            {"cachedEvents", cachedEvents},
            {"eventsAreAdapterCache", true},
            {"plan",
             {{"generation", plan.generation},
              {"instance", plan.instance},
              {"attack", AttackName(plan.attack)},
              {"phaseTwo", plan.phaseTwo},
              {"coreStart", plan.coreStart},
              {"coreTarget", plan.coreTarget},
              {"lockedTarget", plan.lockedTarget},
              {"damage", plan.damage},
              {"hitInterval", plan.hitInterval},
              {"circles", circles},
              {"dive", Circle(plan.dive)},
              {"duration",
               {{"telegraph", plan.duration.telegraph},
                {"locked", plan.duration.locked},
                {"airborne", plan.duration.airborne},
                {"active", plan.duration.active},
                {"recovery", plan.duration.recovery}}},
              {"beam",
               {{"origin", plan.beam.origin},
                {"startAngle", plan.beam.startAngle},
                {"endAngle", plan.beam.endAngle},
                {"range", plan.beam.range},
                {"halfWidth", plan.beam.halfWidth},
                {"warning", warning}}}}}};
}
void RequireDepth(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace

void DepthValidation::InitializeNeonDepthValidation()
{
    auto& session = GameplayScenarioSession::Get();
    const auto& settings = session.GetSettings();
    RequireDepth(gameplaytest::IsDepthScenario(settings.scenario) && world_.resources.enemy_->IsNeonDepthEncounterEnabled(),
                 "Depth fixture must use the ordinary configured real Enemy policy.");
    const bool realDamage = settings.scenario == Scenario::NeonDepthDamage || settings.scenario == Scenario::NeonDepthDodge ||
                            settings.depthFixture.probe == "stationary_damage";
    const bool playerDeath = settings.depthFixture.probe == "player_death" || settings.depthFixture.probe == "simultaneous_death" ||
                             settings.depthFixture.probe == "retry" || settings.depthFixture.probe == "title_return";
    world_.presentation.debugPlayerNoDamage_ = !realDamage && !playerDeath;
    world_.resources.player_->SetDebugNoDamage(world_.presentation.debugPlayerNoDamage_);
    world_.resources.neonBossVisualEnabled_ = settings.depthFixture.visualEnabled;
    RequireDepth(world_.resources.neonBossVisual_ != nullptr, "Depth fixture requires the initialized production Visual owner.");
    world_.resources.neonBossVisual_->SetEnabled(world_.resources.neonBossVisualEnabled_);
    auto profile =
        world_.resources.neonBossVisual_->GetDepthProfile(); // Read-only current config; never replace with a fabricated default.
    profile.reducedMotion = settings.depthFixture.reducedMotion;
    RequireDepth(world_.resources.neonBossVisual_->SetDepthProfile(profile), "Valid fixture reduced-motion profile was rejected.");
    world_.resources.neonDepthPreviousBossHp_ = world_.resources.enemy_->GetHp();
    session.ReportDetail("depthFixture", MakeNeonDepthValidationConditions());
}

nlohmann::json DepthValidation::MakeNeonDepthValidationConditions() const
{
    const auto& session = GameplayScenarioSession::Get();
    const auto& settings = session.GetSettings();
    auto* dx = cg2::Object3dCommon::GetInstance()->GetDxCommon();
    const auto actual = world_.gameplayQueries->MakeDeveloperGameCaptureMetadata(*dx);
    const bool bossDefeatRequested = settings.depthFixture.probe == "hp0" || settings.depthFixture.probe == "simultaneous_death";
    const bool playerLethalRequested = settings.depthFixture.probe == "player_death" ||
                                       settings.depthFixture.probe == "simultaneous_death" || settings.depthFixture.probe == "retry" ||
                                       settings.depthFixture.probe == "title_return";
    auto applied = [&](const char* name) {
        const auto count = (std::min)(world_.resources.neonDepthOperations_.size(), size_t{64});
        for (size_t index = 0; index < count; ++index) {
            const auto& entry = world_.resources.neonDepthOperations_.at(index);
            if (entry.is_object() && entry.contains("operation") && entry.at("operation").is_string() && entry.at("operation") == name)
                return true;
        }
        return false;
    };
    // IScene exposes this observing query as nonconst; it does not mutate Scene.
    const auto showcase = world_.gameplayQueries->GetDeveloperShowcaseState();
    const auto* window = cg2::WinApp::GetInstance();
    const int clientWidth = window->GetClientWidth(), clientHeight = window->GetClientHeight();
    json clientToRenderScale = nullptr;
    if (clientWidth > 0 && clientHeight > 0)
        clientToRenderScale =
            json::array({double(cg2::WinApp::kClientWidth) / clientWidth, double(cg2::WinApp::kClientHeight) / clientHeight});
    json result = {{"shortcutFixture", true},
                   {"normalMainRoute", false},
                   {"automatedInput", true},
                   {"dynamicProbeInput", settings.input.empty()},
                   {"initialPositionAssistance", false},
                   {"playerInvulnerable", world_.resources.player_->IsDebugNoDamage()},
                   {"initialRoomProtectionSeconds", .45},
                   {"bossHpConfigured", settings.bossHp},
                   {"playerHpConfigured", settings.playerHp},
                   {"phaseTwoHpInjectionRequested", settings.depthFixture.injectPhaseTwo},
                   {"phaseTwoHpInjected", world_.resources.neonDepthPhaseTwoInjected_},
                   {"forcedBossDefeatRequested", bossDefeatRequested},
                   {"forcedBossDefeat", applied("actual_enemy_die")},
                   {"forcedPlayerLethalDamageRequested", playerLethalRequested},
                   {"forcedPlayerLethalDamage", applied("actual_player_lethal_damage")},
                   {"probe", settings.depthFixture.probe},
                   {"visualEnabled", world_.resources.neonBossVisualEnabled_},
                   {"reducedMotion", settings.depthFixture.reducedMotion},
                   {"reducedMotionApplied", world_.resources.neonBossVisual_
                                                ? json(world_.resources.neonBossVisual_->GetDepthProfile().reducedMotion)
                                                : json(nullptr)},
                   {"userPause", world_.run.tankRunPaused_},
                   {"introSkipPending", world_.resources.neonDepthSkipRequested_},
                   {"comparisonFreeze", showcase.comparisonFreeze},
                   {"HUDIncluded", true},
                   {"debugUIIncluded", false},
                   {"audio", "none"},
                   {"actualBossPolicy", world_.resources.enemy_->IsNeonDepthEncounterEnabled() ? "depth" : "legacy"},
                   {"resolution", {cg2::WinApp::kClientWidth, cg2::WinApp::kClientHeight}},
                   {"renderResolutionSource", "WinApp fixed native swap-chain and viewport dimensions"},
                   {"actualClientResolution", {clientWidth, clientHeight}},
                   {"clientToRenderScale", clientToRenderScale},
                   {"inputMapping", "client pixel multiplied by native render/client scale"},
                   {"cameraScope",
                    {{"scoped", world_.resources.neonDepthCameraScoped_},
                     {"saved",
                      {{"position", Vector(world_.resources.neonDepthSavedCamera_.position)},
                       {"rotation", Vector(world_.resources.neonDepthSavedCamera_.rotation)},
                       {"fovY", world_.resources.neonDepthSavedCamera_.fovY},
                       {"aspect", world_.resources.neonDepthSavedCamera_.aspect},
                       {"nearClip", world_.resources.neonDepthSavedCamera_.nearClip},
                       {"farClip", world_.resources.neonDepthSavedCamera_.farClip},
                       {"jitter", {world_.resources.neonDepthSavedCamera_.jitter.x, world_.resources.neonDepthSavedCamera_.jitter.y}},
                       {"debugCamera", world_.resources.neonDepthSavedDebugCamera_}}},
                     {"current",
                      {{"position", Vector(world_.resources.camera->GetTranslate())},
                       {"rotation", Vector(world_.resources.camera->GetRotate())},
                       {"fovY", world_.resources.camera->GetFovY()},
                       {"aspect", world_.resources.camera->GetAspectRatio()},
                       {"nearClip", world_.resources.camera->GetNearClip()},
                       {"farClip", world_.resources.camera->GetFarClip()},
                       {"jitter", {world_.resources.camera->GetProjectionJitter().x, world_.resources.camera->GetProjectionJitter().y}},
                       {"debugCamera", cg2::Object3dCommon::GetInstance()->GetIsDebugCamera()}}}}},
                   {"operations", world_.resources.neonDepthOperations_}};
    for (const char* key : {"globalPost", "gpu", "validation"})
        if (actual.contains(key))
            result[key] = actual.at(key);
    return result;
}

void DepthValidation::PrepareNeonDepthValidation()
{
    auto& session = GameplayScenarioSession::Get();
    const auto& settings = session.GetSettings();
    const auto& fixture = settings.depthFixture;
    const unsigned frame = session.GetFrame();
    const auto before = world_.resources.enemy_->GetNeonDepthSnapshot();
    world_.resources.neonDepthForcedHpThisFrame_ = false;
    auto operation = [&](const char* name) {
        RequireDepth(world_.resources.neonDepthOperations_.size() < 64, "Depth operation evidence exceeded its bound.");
        world_.resources.neonDepthOperations_.push_back({{"operation", name},
                                                         {"beforeCompletedFrame", frame + 1},
                                                         {"sceneEpoch", session.GetSceneEpoch()},
                                                         {"generation", before.generation},
                                                         {"instance", before.plan.instance},
                                                         {"actualPhase", PhaseName(before.phase)},
                                                         {"actualAttack", AttackName(before.plan.attack)}});
    };
    if (fixture.injectPhaseTwo && !world_.resources.neonDepthPhaseTwoInjected_ && world_.resources.neonDepthCycleOneMask_ == 7 &&
        before.phase == neondepth::Phase::Reposition && before.vulnerable) {
        const int target = (std::max)(1, world_.resources.enemy_->GetMaxHp() / 3);
        if (world_.resources.enemy_->GetHp() > target)
            world_.resources.enemy_->TakeDamage(static_cast<uint32_t>(world_.resources.enemy_->GetHp() - target));
        RequireDepth(world_.resources.enemy_->GetHp() <= target && !world_.resources.enemy_->IsDead(),
                     "Phase2 injection failed on the actual living core.");
        world_.resources.neonDepthPhaseTwoInjected_ = world_.resources.neonDepthForcedHpThisFrame_ = true;
        operation("phase2_hp_injection");
    }
    if (world_.resources.neonDepthPauseEnd_ && frame >= world_.resources.neonDepthPauseEnd_)
        world_.run.tankRunPaused_ = false;
    const bool match = fixture.phase == PhaseName(before.phase) && before.progress >= fixture.minimumProgress &&
                       (before.phase == neondepth::Phase::Intro || before.phase == neondepth::Phase::Reposition ||
                        fixture.attack == AttackName(before.plan.attack));
    if (!world_.resources.neonDepthFixtureTriggered_ && settings.scenario == Scenario::NeonDepthLifecycle && match &&
        !(fixture.probe == "retry" && session.GetSceneEpoch() > 1)) {
        const bool playerLethal = fixture.probe == "player_death" || fixture.probe == "simultaneous_death" || fixture.probe == "retry" ||
                                  fixture.probe == "title_return";
        if (playerLethal && world_.resources.player_->GetInvincibilityRemainingSeconds() > 0)
            return;
        if (fixture.probe == "pause") {
            world_.resources.neonDepthPauseSnapshot_ = before;
            world_.resources.neonDepthPauseBegin_ = frame;
            world_.resources.neonDepthPauseEnd_ = frame + 60;
            world_.run.tankRunPaused_ = true;
            operation("user_pause_60_updates");
        } else if (fixture.probe == "abort") {
            world_.resources.enemy_->AbortNeonDepthEncounter();
            operation("actual_enemy_abort");
        } else if (fixture.probe == "hp0" || fixture.probe == "simultaneous_death") {
            world_.resources.enemy_->Die();
            world_.resources.neonDepthForcedHpThisFrame_ = true;
            operation("actual_enemy_die");
            RequireDepth(world_.resources.enemy_->GetHp() == 0 && world_.resources.enemy_->IsDead(),
                         "Actual Enemy::Die did not reach HP0.");
        } else if (fixture.probe == "intro_skip") {
            // Use the same requested input consumed by the normal keyboard/GUI
            // path, including its zero-tick player/combat lock on the skip frame.
            world_.resources.neonDepthSkipRequested_ = true;
            operation("queued_intro_skip_input");
        } else if (fixture.probe == "multidraw" || fixture.probe == "stationary_damage")
            return;
        if (playerLethal) {
            world_.presentation.debugPlayerNoDamage_ = false;
            world_.resources.player_->SetDebugNoDamage(false);
            world_.resources.player_->TakeDamage((std::numeric_limits<uint32_t>::max)(), 0);
            RequireDepth(world_.resources.player_->GetHp() == 0 && world_.resources.player_->IsDead(),
                         "Actual Player lethal damage did not reach HP0.");
            operation("actual_player_lethal_damage");
        }
        world_.resources.neonDepthFixtureTriggered_ = true;
    }
    if ((fixture.probe == "retry" || fixture.probe == "title_return") && session.GetSceneEpoch() == 1 &&
        world_.resources.player_->IsDead() && world_.combat.combatFlow_.GetState() == GameFlowState::GameOver &&
        world_.combat.combatFlow_.GetTimer() <= 0 && world_.combat.phase_ == Phase::kMain) {
        world_.combat.resultSelection_ = fixture.probe == "retry" ? 0 : 1;
        world_.combatFlow->ConfirmResultSelection();
        operation(fixture.probe == "retry" ? "actual_confirm_retry" : "actual_confirm_title_return");
    }
    if (settings.input.empty() && !world_.resources.player_->IsDead() && !world_.run.tankRunPaused_) {
        cg2::Vector2 movement{};
        bool shoot = false, dash = false;
        const auto core = world_.resources.enemy_->GetWorldPosition(), position = world_.resources.player_->GetWorldPosition();
        if (settings.scenario == Scenario::NeonDepthDamage) {
            const auto direction = core - position;
            const float length = cg2::Length(direction);
            const float range = world_.resources.player_->GetExpeditionCombatStyle() == tankbuild::Style::Melee ? 2.8f : 7;
            if (length > range && length > .001f)
                movement = {direction.x / length, direction.y / length};
            shoot = before.vulnerable;
        } else if (settings.scenario == Scenario::NeonDepthDodge) {
            const bool retreat = before.phase == neondepth::Phase::Locked || before.phase == neondepth::Phase::Airborne ||
                                 before.phase == neondepth::Phase::Active;
            if (retreat) {
                // Read the committed plan. This helper changes only regular input,
                // never the actual player position, shape or attack clock.
                cg2::Vector2 away{position.x - before.plan.lockedTarget[0], position.y - before.plan.lockedTarget[1]};
                float length = std::sqrt(away.x * away.x + away.y * away.y);
                if (length < .001f) {
                    away = {0, -1};
                    length = 1;
                }
                movement = {away.x / length, away.y / length};
                if (before.phase == neondepth::Phase::Locked && world_.resources.neonDepthDashInstance_ != before.plan.instance) {
                    dash = true;
                    world_.resources.neonDepthDashInstance_ = before.plan.instance;
                }
            }
        }
        world_.resources.player_->SetDemoInput(true, movement, core, shoot, dash);
    }
}

void DepthValidation::RecordNeonDepthValidation()
{
    auto& session = GameplayScenarioSession::Get();
    const auto& snapshot = world_.resources.enemy_->GetNeonDepthSnapshot();
    if (world_.resources.enemy_->GetHp() < world_.resources.neonDepthPreviousBossHp_ && !world_.resources.neonDepthForcedHpThisFrame_)
        world_.resources.neonDepthRealCoreDamage_ +=
            static_cast<uint64_t>(world_.resources.neonDepthPreviousBossHp_ - world_.resources.enemy_->GetHp());
    world_.resources.neonDepthPreviousBossHp_ = world_.resources.enemy_->GetHp();
    if (snapshot.phase == neondepth::Phase::Recovery && snapshot.plan.instance) {
        const unsigned bit = 1u << static_cast<unsigned>(snapshot.plan.attack);
        (snapshot.plan.phaseTwo ? world_.resources.neonDepthCycleTwoMask_ : world_.resources.neonDepthCycleOneMask_) |= bit;
    }
    if (world_.resources.neonDepthPauseEnd_ && session.GetFrame() <= world_.resources.neonDepthPauseEnd_) {
        if (!(snapshot == world_.resources.neonDepthPauseSnapshot_))
            session.Fail("User pause advanced the actual Depth gameplay snapshot.");
    } else if (world_.resources.neonDepthPauseEnd_ && (snapshot.elapsed != world_.resources.neonDepthPauseSnapshot_.elapsed ||
                                                       snapshot.phase != world_.resources.neonDepthPauseSnapshot_.phase))
        world_.resources.neonDepthResumeSeen_ = true;
    const auto combat = world_.resources.player_->GetRunCombatSnapshot();
    json gameplay = {{"boss", DepthSnapshot(snapshot, world_.resources.enemy_->GetNeonDepthEvents())},
                     {"bossHp", world_.resources.enemy_->GetHp()},
                     {"bossMaxHp", world_.resources.enemy_->GetMaxHp()},
                     {"bossDead", world_.resources.enemy_->IsDead()},
                     {"bossPosition", Vector(world_.resources.enemy_->GetWorldPosition())},
                     {"playerHp", world_.resources.player_->GetHp()},
                     {"playerMaxHp", world_.resources.player_->GetMaxHp()},
                     {"playerDead", world_.resources.player_->IsDead()},
                     {"playerPosition", Vector(world_.resources.player_->GetWorldPosition())},
                     {"playerStyle", static_cast<int>(combat.style)},
                     {"playerRadius", world_.resources.player_->GetRadius()},
                     {"coreRadius", world_.resources.enemy_->GetRadius()},
                     {"primaryAttacks", world_.resources.player_->GetPrimaryAttackCount()},
                     {"dashStarts", world_.resources.player_->GetDashStartedCount()},
                     {"damageTakenCount", world_.resources.player_->GetDamageTakenCount()},
                     {"justDodgeCount", world_.resources.player_->GetPerfectDodgeCount()},
                     {"invincibilitySeconds", world_.resources.player_->GetInvincibilityRemainingSeconds()},
                     {"justEvading", world_.resources.player_->IsJustEvading()},
                     {"dashing", world_.resources.player_->IsDashing()},
                     {"activeDrones", combat.activeDrones},
                     {"homing", combat.homing},
                     {"contactClaims", world_.resources.enemy_->GetNeonDepthContactClaims()},
                     {"acceptedHits", world_.resources.enemy_->GetNeonDepthAcceptedHits()},
                     {"flow", static_cast<int>(world_.combat.combatFlow_.GetState())},
                     {"bossDefeatHandled", world_.combat.bossDefeatHandled_},
                     {"playerDeathHandled", world_.combat.playerDeathHandled_},
                     {"actualCoreDamageExcludingInjection", world_.resources.neonDepthRealCoreDamage_}};
    // Record after the actual Update. Draw totals include submitted prior-frame
    // draws; this observer never updates/allocates/releases presentation resources.
    json effects = nullptr;
    if (world_.resources.neonDepthEffects_) {
        const auto& stats = world_.resources.neonDepthEffects_->GetStats();
        effects = {{"hasResources", world_.resources.neonDepthEffects_->HasResources()},
                   {"resourceCreates", stats.resourceCreates},
                   {"resourceReleases", stats.resourceReleases},
                   {"resourceFailures", stats.resourceFailures},
                   {"updates", stats.updates},
                   {"duplicateUpdates", stats.duplicateUpdates},
                   {"invalidFrames", stats.invalidFrames},
                   {"capacityRejected", stats.capacityRejected},
                   {"depthBindingRejected", stats.depthBindingRejected},
                   {"airDraws", stats.airDraws},
                   {"floorDraws", stats.floorDraws},
                   {"vertices", stats.vertices},
                   {"airVertices", stats.airVertices},
                   {"floorVertices", stats.floorVertices},
                   {"lineCommands", stats.lineCommands},
                   {"fillCommands", stats.fillCommands},
                   {"launchLatches", stats.launchLatches}};
    }
    json presentation = {{"conditions", MakeNeonDepthValidationConditions()},
                         {"cameraWorld", Matrix(world_.resources.camera->GetWorldMatrix())},
                         {"viewProjection", Matrix(world_.resources.camera->GetViewProjectionMatrix())},
                         {"cameraPosition", Vector(world_.resources.camera->GetTranslate())},
                         {"cameraRotation", Vector(world_.resources.camera->GetRotate())},
                         {"visualHasResources", world_.resources.neonBossVisual_->HasResources()},
                         {"visualFinished", world_.resources.neonBossVisual_->IsFinished()},
                         {"effects", effects},
                         {"visualLife", static_cast<int>(world_.resources.neonBossVisual_->GetDepthLife())},
                         {"animation", world_.resources.neonBossVisual_->GetAnimationName()},
                         {"animationTime", world_.resources.neonBossVisual_->GetAnimationTime()},
                         {"dissolveProgress", world_.resources.neonBossVisual_->GetDissolveProgress()},
                         {"world", Matrix(world_.resources.neonBossVisual_->GetWorldMatrix())},
                         {"modelUpdates", world_.resources.neonBossVisual_->GetStats().modelUpdates},
                         {"visualDraws", world_.resources.neonBossVisual_->GetStats().draws},
                         {"visualCreates", world_.resources.neonBossVisual_->GetStats().resourceCreates},
                         {"visualReleases", world_.resources.neonBossVisual_->GetStats().resourceReleases},
                         {"poseFreezes", world_.resources.neonBossVisual_->GetStats().depthPoseFreezes},
                         {"drawConstantBuffers", world_.resources.neonBossVisual_->GetDrawConstantBufferCount()}};
    session.ReportDepthFrame({{"frame", session.GetFrame()},
                              {"sceneEpoch", session.GetSceneEpoch()},
                              {"gameplay", gameplay},
                              {"presentation", presentation},
                              {"actualGameplayDelta", world_.resources.gameplayScenarioCombatDt_},
                              {"actualPresentationDelta", world_.resources.gameplayScenarioPresentationDt_}});
}

void DepthValidation::DrawNeonDepthValidation()
{
    auto& session = GameplayScenarioSession::Get();
    if (!session.IsActive() || session.IsFinished() || !gameplaytest::IsDepthScenario(session.GetSettings().scenario) ||
        session.GetSettings().depthFixture.probe != "multidraw" || world_.presentation.developerBloomFreeze_ ||
        world_.resources.neonBossDeveloperFreeze_ || !world_.resources.neonBossVisual_ || !world_.resources.neonBossVisual_->IsVisible())
        return;
    const auto before = world_.resources.enemy_->GetNeonDepthSnapshot();
    const auto modelUpdates = world_.resources.neonBossVisual_->GetStats().modelUpdates;
    const auto draws = world_.resources.neonBossVisual_->GetStats().draws;
    const auto playerHp = world_.resources.player_->GetHp();
    const auto primary = world_.resources.player_->GetPrimaryAttackCount(), damage = world_.resources.player_->GetDamageTakenCount();
    const auto position = world_.resources.player_->GetWorldPosition();
    // The ordinary Update reset the arena after the previous frame fence.
    // Each Draw uses a distinct slot; do not BeginFrame/Update between commands.
    world_.resources.neonBossVisual_->Draw();
    world_.resources.neonBossVisual_->Draw();
    const auto afterPosition = world_.resources.player_->GetWorldPosition();
    const bool unchanged = before == world_.resources.enemy_->GetNeonDepthSnapshot() && playerHp == world_.resources.player_->GetHp() &&
                           primary == world_.resources.player_->GetPrimaryAttackCount() &&
                           damage == world_.resources.player_->GetDamageTakenCount() && position.x == afterPosition.x &&
                           position.y == afterPosition.y && position.z == afterPosition.z;
    const bool modelUnchanged = modelUpdates == world_.resources.neonBossVisual_->GetStats().modelUpdates;
    session.ReportDepthDraw({{"extraDrawCalls", 2},
                             {"actualVisibleDraws", world_.resources.neonBossVisual_->GetStats().draws - draws},
                             {"gameplayUnchanged", unchanged},
                             {"modelUpdatesUnchanged", modelUnchanged},
                             {"constantBuffersAfter", world_.resources.neonBossVisual_->GetDrawConstantBufferCount()}});
    if (!unchanged || !modelUnchanged)
        session.Fail("Actual extra Depth Draws changed gameplay or model sampling.");
    world_.resources.neonDepthFixtureTriggered_ = true;
}

void DepthValidation::FinishNeonDepthValidation()
{
    auto& session = GameplayScenarioSession::Get();
    const auto& settings = session.GetSettings();
    const auto& fixture = settings.depthFixture;
    if (settings.scenario == Scenario::NeonDepthCycles || settings.scenario == Scenario::NeonDepthParity) {
        if (world_.resources.neonDepthCycleOneMask_ != 7 || (fixture.injectPhaseTwo && world_.resources.neonDepthCycleTwoMask_ != 7))
            session.Fail("Depth runtime did not complete all three actual attacks/recoveries in the requested phases.");
    } else if (settings.scenario == Scenario::NeonDepthDamage) {
        if (world_.resources.neonDepthRealCoreDamage_ == 0 || world_.resources.player_->GetPrimaryAttackCount() == 0 ||
            world_.resources.player_->IsDebugNoDamage())
            session.Fail("This style did not damage the actual Depth core through ordinary attacks.");
    } else if (settings.scenario == Scenario::NeonDepthLifecycle) {
        if (fixture.probe == "retry") {
            if (session.GetSceneEpoch() < 2 || world_.resources.player_->IsDead())
                session.Fail("The actual retry scene did not initialize a living player.");
        } else if (fixture.probe == "title_return")
            session.Fail("Deadline reached without actual initialized TITLE notification.");
        else if (fixture.probe == "pause" && !world_.resources.neonDepthResumeSeen_)
            session.Fail("Actual user pause/resume was not demonstrated.");
        else if (fixture.probe != "stationary_damage" && !world_.resources.neonDepthFixtureTriggered_)
            session.Fail("Requested real lifecycle phase was never reached.");
        if ((fixture.probe == "hp0" || fixture.probe == "simultaneous_death" || fixture.probe == "abort") &&
            (world_.resources.neonBossVisual_->HasResources() || !world_.resources.neonBossVisual_->IsFinished()))
            session.Fail("Depth terminal retained presentation resources.");
        if (fixture.probe == "simultaneous_death" &&
            (world_.combat.combatFlow_.GetState() != GameFlowState::GameOver || world_.combat.bossDefeatHandled_))
            session.Fail("Expedition simultaneous death lost its existing player-death priority.");
    }
    session.ReportDetail("depthFinal", {{"phaseOneAttackMask", world_.resources.neonDepthCycleOneMask_},
                                        {"phaseTwoAttackMask", world_.resources.neonDepthCycleTwoMask_},
                                        {"actualCoreDamageExcludingInjection", world_.resources.neonDepthRealCoreDamage_},
                                        {"probeTriggered", world_.resources.neonDepthFixtureTriggered_},
                                        {"actualUserPauseResumed", world_.resources.neonDepthResumeSeen_},
                                        {"conditions", MakeNeonDepthValidationConditions()}});
}
#endif

} // namespace gameplay
