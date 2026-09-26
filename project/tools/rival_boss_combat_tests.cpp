#include "../game/enemy/actor/RivalBossCombat.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>

using Boss = RivalBossCombat;
using Phase = Boss::Phase;

static void Reach(Boss& boss, Phase target, float hp = 1.0f, float aim = 0.0f) {
    for (int i = 0; i < 2000 && boss.GetPhase() != target; ++i) boss.Step(0.01f, true, aim, hp);
    assert(boss.GetPhase() == target);
}

int main() {
    Boss boss;
    // A room without line of sight cannot begin an attack through cover.
    for (int i = 0; i < 500; ++i) assert(!boss.Step(0.01f, false, 0.0f, 1.0f).fire);
    assert(boss.GetPhase() == Phase::Reposition);
    Reach(boss, Phase::Tracking);
    boss.Step(0.01f, true, 0.9f, 1.0f);
    assert(std::abs(boss.GetAimAngle() - 0.9f) < 0.001f);
    boss.Step(0.01f, false, 1.2f, 1.0f);
    assert(boss.GetPhase() == Phase::Reposition);

    // All shots in a magazine share the committed aim even after target moves.
    Reach(boss, Phase::Locked, 1.0f, 0.75f);
    int rounds = 0;
    for (int i = 0; i < 1000 && boss.GetPhase() != Phase::DashWarning; ++i) {
        const auto shot = boss.Step(0.01f, false, -2.5f, 1.0f);
        if (!shot.fire) continue;
        assert(std::abs(shot.angle - 0.75f) < 0.001f);
        assert(shot.round == rounds);
        ++rounds;
        assert(boss.GetAmmo() == 4 - rounds);
    }
    assert(rounds == 4 && boss.GetAmmo() == 0);
    assert(boss.GetPhase() == Phase::DashWarning);
    for (int i = 0; i < 30; ++i) assert(!boss.Step(0.01f, true, 0, 1).fire);
    assert(boss.GetPhase() == Phase::DashWarning);
    Reach(boss, Phase::Dash);
    boss.FinishDash(); // solid wall contact must stop the dash immediately
    assert(boss.GetPhase() == Phase::Reload);
    for (int i = 0; i < 110; ++i) assert(!boss.Step(0.01f, true, 0, 1).fire);
    assert(boss.GetPhase() == Phase::Reload && boss.GetAmmo() == 0);
    Reach(boss, Phase::Reposition);
    assert(boss.GetAmmo() == 4);

    // Low HP is latched, but it never adds ammo or cancels the current opening.
    Reach(boss, Phase::Locked);
    const int before = boss.GetAmmo();
    boss.Step(0.01f, true, 0, 0.30f);
    assert(!boss.IsPhaseTwo() && boss.GetAmmo() == before);
    Reach(boss, Phase::Reload, 0.30f);
    for (int i = 0; i < 100; ++i) assert(!boss.Step(0.01f, true, 0, 0.30f).fire);
    assert(boss.GetPhase() == Phase::Reload);
    Reach(boss, Phase::Tracking, 0.30f);
    assert(boss.IsPhaseTwo() && boss.GetCapacity() == 5 && boss.GetAmmo() == 5);

    // A long frame cannot skip lock/dash warnings or dump a whole magazine.
    boss.Reset();
    for (int i = 0; i < 7; ++i) assert(!boss.Step(5.0f, true, 0, 1).fire);
    assert(boss.GetPhase() == Phase::Tracking);
    Reach(boss, Phase::Locked);
    assert(!boss.Step(5.0f, true, 0, 1).fire);
    assert(boss.GetPhase() == Phase::Locked);
    const float progress = boss.GetProgress();
    assert(!boss.Step(0, true, 0, 1).fire);
    assert(!boss.Step(-1, true, 0, 1).fire);
    assert(!boss.Step(std::numeric_limits<float>::quiet_NaN(), true, 0, 1).fire);
    assert(boss.GetProgress() == progress);

    // Over multiple cycles the boss really alternates all three attack shapes;
    // every discharged magazine reaches reload and the bullet budget is bounded.
    boss.Reset();
    std::set<Boss::Pattern> patterns;
    int reloads = 0, bullets = 0, roundsThisCycle = 0;
    Phase previous = boss.GetPhase();
    for (int i = 0; i < 6000; ++i) {
        const auto shot = boss.Step(0.01f, true, 0, i < 3000 ? 1.0f : 0.3f);
        if (shot.fire) {
            patterns.insert(shot.pattern);
            ++roundsThisCycle;
            bullets += Boss::ProjectileCount(shot.pattern);
            assert(roundsThisCycle <= boss.GetCapacity());
            for (int p = 0; p < Boss::ProjectileCount(shot.pattern); ++p)
                assert(std::abs(Boss::ProjectileOffset(shot.pattern, p, shot.round, boss.GetCapacity())) <= 39.0f);
        }
        if (boss.GetPhase() == Phase::Reload && previous != Phase::Reload) {
            assert(roundsThisCycle == boss.GetCapacity());
            roundsThisCycle = 0;
            ++reloads;
        }
        previous = boss.GetPhase();
    }
    assert(patterns.size() == 3 && reloads >= 10 && bullets < 240);

    // Threat detection must ignore outgoing and off-axis bullets.
    assert(Boss::IsIncomingThreat(8, 0, -0.4f, 0, 2));
    assert(!Boss::IsIncomingThreat(8, 0, 0.4f, 0, 2));
    assert(!Boss::IsIncomingThreat(8, 5, -0.4f, 0, 2));
    assert(!Boss::IsIncomingThreat(40, 0, -0.4f, 0, 2));
    assert(!Boss::IsIncomingThreat(1, 0, 0, 0, 2));
    boss.Reset();
    for (int i = 0; i < 250; ++i) boss.Step(0.01f, false, 0, 1);
    boss.Step(0.01f, true, 0, 1, true);
    assert(boss.GetPhase() == Phase::DashWarning && boss.IsReactiveDash());
    Reach(boss, Phase::Dash);
    boss.FinishDash();
    assert(boss.GetPhase() == Phase::Reposition);
    assert(boss.GetAmmo() == boss.GetCapacity());
    boss.Step(0.01f, true, 0, 1, true);
    assert(boss.GetPhase() == Phase::Reposition); // finite dodge cooldown

    std::cout << "Rival boss: locked aim, magazines, reload, phase transition, dash/cover, threat geometry and hitch tests passed.\n";
}
