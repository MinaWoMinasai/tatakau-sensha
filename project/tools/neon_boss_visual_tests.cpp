#include "game/enemy/visual/NeonBossVisualState.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Require(bool result, const char* message) {
    if (!result) throw std::runtime_error(message);
}
void TestMapping() {
    Require(MapNeonBossAnimation(NeonBossAction::Idle, true).motion == NeonBossMotion::Idle,
        "Idle must use the real existing idle clip");
    Require(MapNeonBossAnimation(NeonBossAction::Reposition, true).motion == NeonBossMotion::Idle &&
        MapNeonBossAnimation(NeonBossAction::Reposition, true).speed > 1.0f,
        "Absent locomotion clip must fall back to a faster existing idle");
    Require(MapNeonBossAnimation(NeonBossAction::Telegraph, true).motion == NeonBossMotion::Attack &&
        MapNeonBossAnimation(NeonBossAction::Attack, true).motion == NeonBossMotion::Attack,
        "Telegraph and Attack must share the existing pull/push clip without reset");
    Require(MapNeonBossAnimation(NeonBossAction::Dash, true).holdAttackPose,
        "Absent dash clip must hold the existing push pose");
    Require(MapNeonBossAnimation(NeonBossAction::Reload, true).motion == NeonBossMotion::Idle,
        "Absent reload clip must use the existing idle fallback");
    for (auto action : {NeonBossAction::Idle, NeonBossAction::Reposition, NeonBossAction::Telegraph,
        NeonBossAction::Attack, NeonBossAction::Dash, NeonBossAction::Reload}) {
        Require(MapNeonBossAnimation(action, false).motion == NeonBossMotion::HoldPose,
            "Death must hold the current pose; no death clip exists");
    }
    Require(NeonBossHpRatio(-5, 10) == 0 && NeonBossHpRatio(15, 10) == 1 && NeonBossHpRatio(1, 0) == 0,
        "HP presentation ratios must be bounded and tolerate absent max HP");
}
void TestAnimationEdges() {
    NeonBossVisualState state;
    state.Update(false, true, 100, 100, NeonBossAction::Idle, 1.0f);
    Require(state.GetLife() == NeonBossVisualLife::Dormant && state.GetAnimationSelectionCount() == 0,
        "Inactive boss must not request animation/resource startup");
    state.Update(true, true, 100, 100, NeonBossAction::Idle, 1.0f / 60);
    Require(state.DidAnimationChange() && state.GetAnimationSelectionCount() == 1, "Initial idle entry missing");
    for (int frame = 0; frame < 600; ++frame) {
        state.Update(true, true, 100, 100, NeonBossAction::Idle, 1.0f / 60);
        Require(!state.DidAnimationChange() && !state.DidActionChange(),
            "Repeated idle input must not restart animation");
    }
    state.Update(true, true, 100, 100, NeonBossAction::Reposition, 1.0f / 60);
    Require(state.DidActionChange() && !state.DidAnimationChange(), "Speed-only fallback must keep idle time");
    state.Update(true, true, 100, 100, NeonBossAction::Telegraph, 1.0f / 60);
    Require(state.DidAnimationChange() && state.GetAnimationSelectionCount() == 2, "Telegraph must select Attack once");
    for (int frame = 0; frame < 120; ++frame) {
        state.Update(true, true, 100, 100, NeonBossAction::Telegraph, 1.0f / 60);
        Require(!state.DidAnimationChange(), "Sustained Telegraph must not reset its pull animation");
    }
    state.Update(true, true, 100, 100, NeonBossAction::Attack, 1.0f / 60);
    Require(state.DidActionChange() && !state.DidAnimationChange(), "Telegraph -> Attack must continue the same clip");
    state.Update(true, true, 100, 100, NeonBossAction::Dash, 1.0f / 60);
    Require(!state.DidAnimationChange() && state.GetMapping().holdAttackPose, "Dash must hold, not restart Attack");
    state.Update(true, true, 40, 100, NeonBossAction::Reload, 1.0f / 60);
    Require(state.DidAnimationChange() && state.GetAnimationSelectionCount() == 3 && state.GetHpRatio() == 0.4f,
        "Reload must select Idle once and update HP presentation");
}
void TestDeathLifecycle() {
    NeonBossVisualState state;
    state.Update(true, true, 10, 100, NeonBossAction::Dash, 0);
    state.Update(true, true, 0, 100, NeonBossAction::Dash, 4.0f);
    Require(state.DidDeathStart() && state.GetLife() == NeonBossVisualLife::Dying && state.GetDissolveProgress() == 0,
        "Zero HP must latch death before advancing the new death effect");
    Require(state.GetHpRatio() == 0.1f && state.GetMapping().motion == NeonBossMotion::HoldPose,
        "Death must freeze the last alive presentation");
    const auto selections = state.GetAnimationSelectionCount();
    // Even incorrect/reused gameplay input cannot revive one presentation encounter.
    state.Update(true, true, 100, 100, NeonBossAction::Attack, NeonBossVisualState::kDeathHoldSeconds);
    Require(state.GetLife() == NeonBossVisualLife::Dying && state.GetDissolveProgress() == 0 && !state.DidDeathStart(),
        "Gameplay reactivation must not rewind the latched death");
    state.Update(false, true, 100, 100, NeonBossAction::Attack, NeonBossVisualState::kDissolveSeconds * 0.5f);
    Require(std::abs(state.GetDissolveProgress() - 0.5f) < 1e-6f,
        "Dissolve must continue after gameplay deactivates");
    const auto midpoint = state.GetDissolveProgress();
    for (float invalid : {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        state.Update(false, false, 0, 100, NeonBossAction::Idle, invalid);
        Require(state.GetDissolveProgress() == midpoint, "Invalid delta time must not corrupt dissolve progress");
    }
    state.Update(false, false, 0, 100, NeonBossAction::Idle, 100.0f);
    Require(state.GetLife() == NeonBossVisualLife::Finished && state.GetDissolveProgress() == 1 && state.ShouldReleaseResources(),
        "Dissolve terminal progress must be exact and request resource release");
    state.Update(true, true, 100, 100, NeonBossAction::Idle, 1.0f);
    Require(state.GetLife() == NeonBossVisualLife::Finished && state.GetDissolveProgress() == 1 && !state.ShouldReleaseResources() &&
        state.GetAnimationSelectionCount() == selections, "Finished state must neither revive nor release resources repeatedly");
    state.Reset();
    state.Update(true, true, 100, 100, NeonBossAction::Idle, 0);
    Require(state.GetLife() == NeonBossVisualLife::Alive && state.GetDissolveProgress() == 0 && state.DidAnimationChange(),
        "Explicit new encounter reset must permit a fresh visual");
}

void TestSameClipTelegraphEntry() {
    NeonBossVisualState state;
    state.Update(true, true, 100, 100, NeonBossAction::Attack, 0);
    state.Update(true, true, 100, 100, NeonBossAction::Telegraph, 0);
    Require(state.DidActionChange() && !state.DidAnimationChange(), "Volley -> DashWarning must keep the real Attack clip");
    Require(NeonBossEntryPoseTime(state.GetAction(), state.DidActionChange(), 1.5f) == 0.50f,
        "A completed push/recovery must return to the pull pose when Telegraph begins");
    Require(NeonBossEntryPoseTime(state.GetAction(), state.DidActionChange(), 0.2f) < 0,
        "A newly started pull must continue naturally toward its Telegraph pose");
    for (int frame = 0; frame < 120; ++frame) {
        state.Update(true, true, 100, 100, NeonBossAction::Telegraph, 1.0f / 60);
        Require(NeonBossEntryPoseTime(state.GetAction(), state.DidActionChange(), 1.5f) < 0,
            "Repeated Telegraph input must never seek/restart the clip again");
    }
    state.Update(true, true, 100, 100, NeonBossAction::Attack, 0);
    Require(!state.DidAnimationChange() && NeonBossEntryPoseTime(state.GetAction(), state.DidActionChange(), 0.5f) < 0,
        "Telegraph -> Attack must resume its current time without a seek");
    state.Update(true, true, 100, 100, NeonBossAction::Dash, 0);
    Require(NeonBossEntryPoseTime(state.GetAction(), state.DidActionChange(), 0.5f) == 0.82f,
        "Dash entry must retain the existing exact push-pose fallback");
}
}

int main() {
    try {
        TestMapping(); TestAnimationEdges(); TestDeathLifecycle(); TestSameClipTelegraphEntry();
        std::cout << "PASS: Neon boss mapping, animation edges, monotonic death and dissolve endpoint\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
