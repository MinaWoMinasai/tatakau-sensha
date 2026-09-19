#include "../game/exp/ExpEnemyCombatCycle.h"
#include "../game/exp/ExpEnemyNavigation.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

namespace {
using Phase = ExpEnemyCombatPhase;
void Tick(ExpEnemyCombatCycle& cycle, float seconds, bool visible = true) {
    constexpr float step = 0.01f;
    for (int i = 0; i < static_cast<int>(std::round(seconds / step)); ++i) {
        cycle.Advance(step, visible);
    }
}
}

int main() {
    ExpEnemyCombatCycle cycle;
    const ExpEnemyCombatCycle::Timing timing{ 0.60f, 0.30f, 0.42f, 1.10f, 0.55f };
    cycle.Reset(timing, 0.10f);
    assert(!cycle.Advance(0.11f, false));
    assert(cycle.GetPhase() == Phase::Cooldown);
    assert(!cycle.Advance(0.01f, true));
    assert(cycle.GetPhase() == Phase::Tracking);
    Tick(cycle, 0.25f);
    assert(!cycle.IsAimLocked());
    assert(cycle.GetWarningRatio() > 0.40f && cycle.GetWarningRatio() < 0.45f);

    // Cover during target tracking cancels the attack and requires a fresh windup.
    assert(!cycle.Advance(0.01f, false));
    assert(cycle.GetPhase() == Phase::Cooldown);
    assert(cycle.GetWarningRatio() == 0.0f);
    Tick(cycle, 0.42f);
    assert(cycle.GetPhase() == Phase::Tracking);
    Tick(cycle, 0.62f);
    assert(cycle.GetPhase() == Phase::Locked);
    assert(cycle.IsAimLocked());
    assert(cycle.GetWarningRatio() == 1.0f);

    // After commitment the target may dodge out of sight; the shot keeps its
    // locked direction rather than reacquiring the new player position.
    assert(!cycle.Advance(0.10f, false));
    assert(cycle.GetPhase() == Phase::Locked);
    assert(cycle.IsAimLocked());
    assert(cycle.Advance(0.30f, false));
    assert(cycle.GetPhase() == Phase::Active);
    assert(!cycle.Advance(0.10f, true));
    assert(cycle.IsAimLocked());

    // A wall/contact impact ends a dash immediately and creates a full opening.
    cycle.EnterRecovery();
    assert(cycle.GetPhase() == Phase::Recovery);
    assert(!cycle.IsAimLocked());
    assert(cycle.GetRecoveryRatio() == 1.0f);
    Tick(cycle, 0.90f);
    assert(cycle.GetPhase() == Phase::Recovery);
    Tick(cycle, 0.22f);
    assert(cycle.GetPhase() == Phase::Cooldown);

    // Frame stalls must not skip the visible lock window or cause burst shots.
    cycle.Reset(timing, 0.0f);
    assert(!cycle.Advance(5.0f, true));
    assert(cycle.GetPhase() == Phase::Tracking);
    assert(!cycle.Advance(5.0f, true));
    assert(cycle.GetPhase() == Phase::Locked);
    assert(cycle.Advance(5.0f, true));
    assert(cycle.GetPhase() == Phase::Active);
    assert(!cycle.Advance(5.0f, true));
    assert(cycle.GetPhase() == Phase::Recovery);

    // Pause and invalid time cannot silently advance a pending attack.
    cycle.Reset(timing, 0.0f);
    assert(!cycle.Advance(0.0f, true));
    assert(!cycle.Advance(-1.0f, true));
    assert(!cycle.Advance(std::numeric_limits<float>::quiet_NaN(), true));
    assert(cycle.GetPhase() == Phase::Cooldown);

    const ExpEnemyCombatCycle::Timing sniper{ 0.85f, 0.38f, 0.10f, 1.50f, 0.65f };
    cycle.Reset(sniper, 0.55f);
    int shots = 0;
    float sinceLastShot = 100.0f;
    for (int frame = 0; frame < 1200; ++frame) {
        sinceLastShot += 1.0f / 60.0f;
        if (cycle.Advance(1.0f / 60.0f, true)) {
            assert(sinceLastShot >= 3.45f);
            sinceLastShot = 0.0f;
            ++shots;
        }
    }
    assert(shots == 6);
    // A wall with a single gap requires travelling away from the target first;
    // local steering can oscillate here, but the route must cross the open gap.
    std::array<bool, 7 * 7> grid{};
    for (int y = 0; y < 6; ++y) grid[y * 7 + 3] = true;
    int current = 1 * 7 + 1;
    const int goal = 1 * 7 + 5;
    bool usedGap = false;
    for (int step = 0; step < 40 && current != goal; ++step) {
        const int next = FindExpEnemyNextCell<7, 7>(grid, current, goal);
        assert(next >= 0 && !grid[next]);
        assert(std::abs(next % 7 - current % 7) + std::abs(next / 7 - current / 7) == 1);
        if (next == 6 * 7 + 3) usedGap = true;
        current = next;
    }
    assert(current == goal && usedGap);
    grid[6 * 7 + 3] = true;
    assert((FindExpEnemyNextCell<7, 7>(grid, 1 * 7 + 1, goal) == -1));
    grid.fill(true);
    assert((FindExpEnemyNextCell<7, 7>(grid, 0, 48) == -1));
    std::cout << "Expedition enemy timing: tracking, cover cancellation, aim lock, recovery, frame stalls and sniper cadence passed.\n";
}
