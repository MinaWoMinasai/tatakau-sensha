// Common State getters follow Depth lifecycle while legacy timing remains exact.
#include "game/enemy/visual/NeonBossVisualState.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void StateTruth() {
    NeonBossVisualState state;
    neondepth::LifecycleInput input;
    state.UpdateDepth(input, {}, .1f);
    Require(state.GetLife() == NeonBossVisualLife::Dormant, "Inactive Depth became alive");
    input.encounterActive = true; input.intro = true; input.introProgress = .2f;
    input.hp = 25; input.maxHp = 100;
    state.UpdateDepth(input, {}, .1f);
    Require(state.GetLife() == NeonBossVisualLife::Alive && state.GetDepthLifecycle().GetLife() == neondepth::Life::Intro &&
        state.GetHpRatio() == .25f, "Depth state public life/HP diverged");
    input.intro = false; state.UpdateDepth(input, {}, 0);
    Require(state.GetDepthLifecycle().GetLife() == neondepth::Life::Active, "Intro skip left a distinct state");
    input.hp = 0; input.alive = false; state.UpdateDepth(input, {.35f, .8f}, .1f);
    Require(state.DidDeathStart() && state.GetLife() == NeonBossVisualLife::Dying && state.GetDissolveProgress() == 0,
        "HP0 did not latch common death truth before elapsed");
    input.hp = 100; input.alive = true;
    unsigned freezes = 0, releases = 0;
    for (int step = 0; step < 20; ++step) {
        state.UpdateDepth(input, {.8f, 2}, .1f);
        freezes += state.GetDepthLifecycle().ShouldFreezePose();
        releases += state.ShouldReleaseResources();
    }
    Require(state.GetLife() == NeonBossVisualLife::Finished && state.GetDissolveProgress() == 1 && freezes == 1 && releases == 1,
        "Depth terminal/public getters or release/freeze once failed");
    state.Reset();
    state.Update(true, true, 50, 100, NeonBossAction::Idle, 0);
    state.Update(true, false, 0, 100, NeonBossAction::Idle, 0);
    state.Update(false, false, 0, 100, NeonBossAction::Idle, NeonBossVisualState::kDeathHoldSeconds);
    Require(state.GetDissolveProgress() == 0, "Depth additions changed exact legacy hold");
    state.Update(false, false, 0, 100, NeonBossAction::Idle, NeonBossVisualState::kDissolveSeconds * .5f);
    Require(std::abs(state.GetDissolveProgress() - .5f) < 1.0e-6f, "Depth additions changed legacy dissolve");
}
}
int main() {
    try { StateTruth(); std::cout << "PASS: Depth common state truth, terminal once and legacy timing\n"; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
