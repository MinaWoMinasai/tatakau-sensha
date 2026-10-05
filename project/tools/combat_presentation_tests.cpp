#include "game/flow/CombatFlowController.h"
#include "game/enemy/visual/BossVisualBridge.h"
#include <iostream>
#include <limits>
#include <stdexcept>

void Require(bool passed, const char* message) {
    if (!passed) throw std::runtime_error(message);
}

int main() {
    try {
        CombatDeathInput death;
        death.bossActive = death.bossDead = death.playerDead = true;
        Require(SelectCombatDeathOutcome(death) == CombatDeathOutcome::BossDefeat, "legacy simultaneous death priority");
        death.expedition = true; death.bossRoom = true;
        Require(SelectCombatDeathOutcome(death) == CombatDeathOutcome::PlayerDeath, "expedition simultaneous death priority");
        death.playerDeathHandled = true;
        Require(SelectCombatDeathOutcome(death) == CombatDeathOutcome::BossDefeat, "already handled player death cannot mask boss");
        death.bossDefeatHandled = true;
        Require(SelectCombatDeathOutcome(death) == CombatDeathOutcome::None, "death events must not repeat");
        death.playerDead = false; death.bossDefeatHandled = false; death.bossRoom = false;
        Require(SelectCombatDeathOutcome(death) == CombatDeathOutcome::None, "resource-room boss death cannot finish expedition");
        death.expedition = false; death.bossActive = false;
        Require(SelectCombatDeathOutcome(death) == CombatDeathOutcome::None, "inactive boss cannot cause result");

        CombatFlowController flow;
        flow.BeginBossDefeat(1.0f, 0.25f);
        Require(flow.GetTimer() == 2.20f, "default boss duration clamp changed");
        int impacts = 0;
        for (int frame = 0; frame < 240; ++frame) {
            const auto events = flow.Update(1.0f / 60.0f, true);
            impacts += events.bossImpact ? 1 : 0;
            Require(events.waitForPresentation && !events.bossResultReady, "pending credits must delay result");
        }
        Require(impacts == 1 && flow.GetTimer() == 0.0f, "boss impact once and exact terminal");
        Require(flow.Update(1.0f / 60.0f, false).bossResultReady, "credit completion must release result wait");
        flow.EnterResult(true);
        Require(flow.GetState() == CombatFlowState::StageClear && !flow.Update(1.0f / 60.0f, false).waitForPresentation,
            "result input readiness");
        flow.Reset(); flow.BeginBossDefeat(10.0f, 0.3f);
        Require(flow.GetTimer() == 1.10f, "fast dissolve lower clamp");
        const float before = flow.GetTimer();
        for (float dt : {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
            Require(!flow.Update(dt, false).bossImpact && flow.GetTimer() == before, "invalid/paused dt mutated flow");
        flow.BeginGameOver(0.5f);
        auto end = flow.Update(0.5f, false);
        Require(end.gameOverTextReady && end.waitForPresentation, "timer terminal frame must still return before result input");
        Require(!flow.Update(1.0f/60.0f, false).waitForPresentation, "next frame allows result input");
        flow.EnterResult(false);
        Require(flow.GetState() == CombatFlowState::GameOver && flow.GetTimer() == 0.0f, "game-over result reset");

        BossVisualBridge bridge;
        BossVisualBridgeInput input;
        input.encounterGeneration = 1;
        Require(bridge.Update(input, 0).resetEncounter, "first encounter reset");
        Require(!bridge.Update(input, 0).resetEncounter, "same encounter must not reset");
        input.rivalEnabled = true;
        const RivalBossCombat::Phase phases[] = {RivalBossCombat::Phase::Reposition, RivalBossCombat::Phase::Tracking,
            RivalBossCombat::Phase::Locked, RivalBossCombat::Phase::Volley, RivalBossCombat::Phase::DashWarning,
            RivalBossCombat::Phase::Dash, RivalBossCombat::Phase::Reload};
        const NeonBossAction actions[] = {NeonBossAction::Reposition, NeonBossAction::Telegraph, NeonBossAction::Telegraph,
            NeonBossAction::Attack, NeonBossAction::Telegraph, NeonBossAction::Dash, NeonBossAction::Reload};
        for (size_t i = 0; i < std::size(phases); ++i) {
            input.rivalPhase = phases[i];
            Require(bridge.Update(input, 1.0f/60.0f).action == actions[i], "Rival phase mapping");
        }
        input.hp = 1; input.maxHp = 100; input.rivalPhaseTwo = false;
        Require(!bridge.Update(input, 0).phaseTwo, "Rival phase flag remains authoritative");
        input.rivalEnabled = false; input.prototypeEnabled = true;
        input.prototypePhase = PrototypeBossCombat::Phase::Recovery;
        Require(bridge.Update(input, 0).phaseTwo, "non-rival HP phase mapping");
        ++input.shotsFired;
        Require(bridge.Update(input, 1.0f/60.0f).action == NeonBossAction::Attack, "single-tick shot must be visible");
        for (int i = 0; i < 600; ++i)
            Require(bridge.Update(input, 0).action == NeonBossAction::Attack, "pause consumed presentation hold");
        input.prototypePhase = PrototypeBossCombat::Phase::Telegraph;
        Require(bridge.Update(input, 0).action == NeonBossAction::Telegraph, "attack hold cannot mask new telegraph");
        input.prototypePhase = PrototypeBossCombat::Phase::Attack;
        Require(bridge.Update(input, 0).action == NeonBossAction::Attack, "Prototype attack mapping");
        input.prototypePhase = PrototypeBossCombat::Phase::Recovery;
        Require(bridge.Update(input, 0.8f).action == NeonBossAction::Idle, "hold expiry");
        ++input.shotsFired; bridge.Update(input, 0);
        ++input.encounterGeneration;
        auto nextEncounter = bridge.Update(input, 0);
        Require(nextEncounter.resetEncounter && nextEncounter.action == NeonBossAction::Idle, "new encounter inherited attack hold");
        input.prototypeEnabled = false; input.position[0] = 1.0f;
        Require(bridge.Update(input, 0).action == NeonBossAction::Reposition, "legacy displacement mapping");
        Require(bridge.Update(input, 0).action == NeonBossAction::Idle, "stationary legacy mapping");
        std::cout << "PASS: combat outcome priority/timing/currency and value-only boss bridge history/mapping\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
