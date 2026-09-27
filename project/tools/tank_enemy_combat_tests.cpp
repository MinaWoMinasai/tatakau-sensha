#include "../game/exp/ExpEnemyCombatCycle.h"
#include "../game/exp/ExpEnemyMagazineCycle.h"
#include "../game/exp/ExpEnemyNavigation.h"
#include "../game/exp/ExpGuardCombat.h"
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
    expguard::PulseCycle pulse;
    pulse.Reset(5,.75f,0);
    assert(!pulse.Advance(10,true) && pulse.IsWarning());
    assert(!pulse.Advance(0,true));
    assert(!pulse.Advance(std::numeric_limits<float>::quiet_NaN(),true));
    for (int i = 0; i < 74; ++i) assert(!pulse.Advance(.01f,true));
    assert(pulse.Advance(.02f,true) && !pulse.IsWarning());
    for (int i = 0; i < 420; ++i) assert(!pulse.Advance(.01f,true));
    assert(!pulse.IsWarning());
    assert(!pulse.Advance(.06f,true) && pulse.IsWarning());
    assert(expguard::SummonSlots(0,0)==3 && expguard::SummonSlots(3,3)==0);
    assert(expguard::SummonSlots(1,5)==1 && expguard::SummonSlots(0,6)==0);
    // This is the same directional armor calculation used by both actor
    // damage and incoming (including reflected) bullets in ExpEnemy.
    assert(expguard::ShieldDamage(100, 1, 0, 3, 0) == 15);
    assert(expguard::ShieldDamage(100, 1, 0, -3, 0) == 100);
    assert(expguard::ShieldDamage(100, 1, 0, 0, 3) == 100);
    assert(expguard::ShieldDamage(100, 1, 0, 3, 0, true) == 40);
    assert(expguard::ShieldDamage(100, 1, 0, -3, 0, true) == 100);
    assert(expguard::ShieldDamage(6, 1, 0, 3, 0) == 1);
    assert(expguard::ShieldDamage(0, 1, 0, 3, 0) == 0);
    assert(expguard::ShieldDamage(100, 1, 0, 0, 0) == 100);
    assert(expguard::ShieldDamage(100, 0, 0, 3, 0) == 100);
    assert(expguard::ShieldDamage(100, 1, 0, std::cos(54.0f * 3.14159265f / 180),
        std::sin(54.0f * 3.14159265f / 180)) == 15);
    assert(expguard::ShieldDamage(100, 1, 0, std::cos(56.0f * 3.14159265f / 180),
        std::sin(56.0f * 3.14159265f / 180)) == 100);

    expguard::BladeCycle blade;
    blade.Reset(0);
    blade.SetIntervalScale(0.3f); // Authoring cannot erase the recovery opening.
    assert(!blade.Advance(0.01f, false));
    assert(blade.GetPhase() == Phase::Cooldown);
    assert(!blade.Advance(0.01f, true));
    assert(blade.GetPhase() == Phase::Locked && blade.IsCommitted());
    for (int i = 0; i < 44; ++i) {
        assert(!blade.Advance(0.01f, false));
        assert(blade.GetPhase() == Phase::Locked);
        assert(!blade.TryHit(1, 0, 2, 0, 0.8f, true));
    }
    assert(blade.Advance(0.02f, false)); // Committed swing can miss a dodging player.
    assert(blade.GetPhase() == Phase::Active && blade.SwingCount() == 1);
    assert(!blade.TryHit(1, 0, 2, 0, 0.8f, false)); // Cover cannot be cut through.
    assert(!blade.TryHit(1, 0, -2, 0, 0.8f, true)); // Dash behind it.
    assert(!blade.TryHit(1, 0, 8, 0, 0.8f, true));
    assert(blade.TryHit(1, 0, 2, 0, 0.8f, true));
    for (int i = 0; i < 12; ++i) {
        assert(!blade.Advance(0.01f, true));
        assert(!blade.TryHit(1, 0, 2, 0, 0.8f, true)); // One hit per whole sweep.
    }
    assert(blade.GetPhase() == Phase::Active);
    assert(!blade.Advance(0.03f, true));
    assert(blade.GetPhase() == Phase::Recovery);
    for (int i = 0; i < 89; ++i) {
        assert(!blade.Advance(0.01f, true));
        assert(blade.GetPhase() == Phase::Recovery);
        assert(!blade.TryHit(1, 0, 2, 0, 0.8f, true));
    }
    assert(!blade.Advance(0.02f, true));
    assert(blade.GetPhase() == Phase::Cooldown);

    blade.Reset(0);
    float elapsed = 0, lastHit = -100;
    unsigned hits = 0;
    for (int frame = 0; frame < 3000; ++frame) {
        elapsed += 0.01f;
        blade.Advance(0.01f, true);
        if (blade.TryHit(1, 0, 2, 0, 0.8f, true)) {
            assert(elapsed - lastHit >= 1.63f);
            lastHit = elapsed;
            ++hits;
        }
    }
    assert(hits >= 16 && hits <= 19);
    blade.Reset(0);
    assert(!blade.Advance(10, true));
    assert(blade.GetPhase() == Phase::Locked);
    assert(blade.Advance(10, true)); // Never skips to another instant swing.
    assert(!blade.Advance(10, true));
    assert(blade.GetPhase() == Phase::Recovery);
    assert(!blade.Advance(std::numeric_limits<float>::quiet_NaN(), true));
    assert(!blade.Advance(0, true));
    assert(blade.GetPhase() == Phase::Recovery);

    // Finite clips create a real punish window. This checks actual temporal
    // behavior (including lost sight and frame stalls), not getters alone.
    ExpEnemyMagazineCycle magazine;
    const ExpEnemyMagazineCycle::Timing magTiming{3,0.26f,0.16f,0.16f,1.50f};
    magazine.Reset(magTiming,0);
    int roundsFired=0;
    float reloadTime=0;
    bool sawReload=false;
    for(int frame=0;frame<1200;++frame) {
        const bool wasReloading=magazine.IsReloading();
        const int beforeAmmo=magazine.GetAmmo();
        const bool fired=magazine.Advance(0.01f,true);
        if(fired) {++roundsFired;assert(magazine.GetAmmo()==beforeAmmo-1);}
        if(magazine.IsReloading()) {
            assert(!fired&&magazine.GetAmmo()==0);
            sawReload=true;reloadTime+=0.01f;
        }
        if(wasReloading&&!magazine.IsReloading()) {
            assert(reloadTime>=1.49f);
            assert(roundsFired%3==0&&magazine.GetAmmo()==3);
            reloadTime=0;
        }
    }
    assert(sawReload&&roundsFired>=9&&roundsFired<=12);
    assert(magazine.GetReloadCount()>=3);
    magazine.Reset(magTiming,0);
    assert(!magazine.Advance(0.01f,true));
    assert(magazine.GetPhase()==Phase::Tracking);
    assert(!magazine.Advance(0.10f,false));
    assert(magazine.GetPhase()==Phase::Cooldown&&magazine.GetAmmo()==3);
    for(int frame=0;frame<100;++frame)assert(!magazine.Advance(0.1f,false));
    assert(magazine.GetAmmo()==3); // No blind shooting, no gratuitous refill.
    assert(!magazine.Advance(10.0f,true));
    assert(magazine.GetPhase()==Phase::Tracking);
    assert(!magazine.Advance(10.0f,true));
    assert(magazine.GetPhase()==Phase::Locked);
    assert(magazine.Advance(10.0f,false)); // Commitment survives lost sight.
    assert(magazine.GetAmmo()==2&&magazine.GetPhase()==Phase::Active);
    assert(!magazine.Advance(10.0f,true)); // One call never catches up multiple shots.
    // Even the fastest authored variant retains its warning and reload opening.
    magazine.Reset({1,0.26f,0.16f,0.16f,1.5f},0);
    magazine.SetIntervalScale(0.3f);
    assert(!magazine.Advance(100,true));assert(!magazine.Advance(100,true));
    assert(magazine.Advance(100,true));assert(!magazine.Advance(100,true));
    assert(magazine.IsReloading());
    for(int i=0;i<79;++i) {assert(!magazine.Advance(0.01f,false));assert(magazine.IsReloading());}
    assert(!magazine.Advance(0,false));
    assert(!magazine.Advance(-1,false));
    assert(!magazine.Advance(std::numeric_limits<float>::quiet_NaN(),false));
    assert(magazine.IsReloading());
    assert(!magazine.Advance(0.02f,false));
    assert(!magazine.IsReloading()&&magazine.GetAmmo()==1);

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
    std::cout << "Expedition enemy timing: finite magazines, mandatory reload, cover cancellation, aim lock, frame stalls, dash recovery and navigation passed.\n";
}
