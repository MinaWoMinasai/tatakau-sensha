#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include "game/player/PlayerDerivedStats.h"


namespace {
std::array<float, 9> Values(const TankPlayerStats& s)
{
    return {s.reloadSpeed, s.bulletDamage, s.bulletSpeed, s.moveSpeed, s.maxHp, s.staminaRecovery, s.stamina, s.maxStamina, s.bodyDamage};
}
void Exact(float value, float expected)
{
    assert(std::bit_cast<uint32_t>(value) == std::bit_cast<uint32_t>(expected));
}
void StatsGoldens()
{
    // Values captured from actual pre-extraction Player::RecalculateStatsFromBase.
    constexpr std::array<std::array<float, 9>, 7> expected{{
        {10,1,.300000012f,.200000003f,10000,1,3,3,3},
        {6.5f,1.5f,.395999998f,.224000007f,13000,1.15999997f,3,3,3.30000019f},
        {10,1,.300000012f,.200000003f,12000,1,3,3,3},
        {13.2000008f,2.25f,.306000054f,.224000007f,11500,1.25f,1.25f,3,3},
        {23.7600002f,13.5f,.275400043f,.257600009f,138,1.125f,1.25f,3,3},
        {39.6000023f,6.75f,.275400043f,.257600009f,138,1.125f,1.25f,3,3},
        {26.1360035f,9,.275400043f,.257600009f,138,1.125f,1.25f,3,3}
    }};
    for (size_t fixture = 0; fixture < expected.size(); ++fixture) {
        PlayerDerivedStatsInput input{};
        input.previousStamina = 1.25f;
        if (fixture == 1) input.upgradeLevels = {2,3,1,4,5,5,2};
        if (fixture == 2) input.upgradeLevels[1] = 2;
        TankRunModifiers modifiers{};
        if (fixture >= 3) {
            input.runEnabled = modifiers.enabled = modifiers.expedition = true;
            modifiers.heavy = modifiers.rapid = modifiers.repair = modifiers.thrusters = true;
            modifiers.core = TankRunCore::Assault;
        }
        input.runTuning = MakeTankRunTuning(modifiers);
        if (fixture >= 4) {
            input.combatStyleSelected = true;
            input.combatStyle = static_cast<tankbuild::Style>(fixture - 4);
            input.combatStyleProfile = DefaultTankCombatStyleBalances()[fixture - 4];
        }
        const auto values = Values(CalculatePlayerDerivedStats(input));
        for (size_t i = 0; i < values.size(); ++i) Exact(values[i], expected[fixture][i]);
        const int oldHp = fixture == 2 ? 10000 : 73;
        const int hp = ResolvePlayerRecalculatedHp(oldHp,10000,static_cast<int>(values[4]),false);
        assert(hp == (fixture == 2 ? 12000 : 73));
    }
}
void StatsBoundaries()
{
    PlayerDerivedStatsInput input{};
    input.runEnabled = true;
    input.combatStyleSelected = true;
    input.combatStyleProfile = SanitizeTankCombatStyleProfile({});
    input.previousStamina = 17;
    input.maintenanceMoveScale = 1.12f;
    input.maintenanceReloadScale = .82f;
    input.upgradeLevels = {5,5,5,5,5,5,5};
    auto first = CalculatePlayerDerivedStats(input);
    auto second = CalculatePlayerDerivedStats(input);
    assert(Values(first) == Values(second)); // Reapplying inputs does not compound prior stats.
    assert(first.stamina == first.maxStamina);
    input.previousStamina = .25f;
    assert(CalculatePlayerDerivedStats(input).stamina == .25f); // Run refits preserve spent stamina.
    input.combatStyleProfile.maxStamina = 0;
    assert(CalculatePlayerDerivedStats(input).stamina == 0);
    input.runEnabled = false; input.combatStyleSelected = false;
    input.base.maxHp = -5; input.base.maxStamina = -2; input.base.stamina = 4;
    input.base.reloadSpeed = 0; input.base.bulletDamage = 0;
    input.base.bulletSpeed = 0; input.base.moveSpeed = 0;
    const auto bounded = CalculatePlayerDerivedStats(input);
    assert(bounded.maxHp == 1 && bounded.maxStamina == 0 && bounded.stamina == 0);
    assert(bounded.reloadSpeed >= .05f && bounded.bulletDamage == .1f && bounded.bulletSpeed == .01f && bounded.moveSpeed == .01f);
    assert(ResolvePlayerRecalculatedHp(100,100,130,false) == 130);
    assert(ResolvePlayerRecalculatedHp(73,100,130,false) == 73);
    assert(ResolvePlayerRecalculatedHp(73,100,50,false) == 50);
    assert(ResolvePlayerRecalculatedHp(-2,100,50,false) == 0);
    assert(ResolvePlayerRecalculatedHp(0,0,50,false) == 0);
    assert(ResolvePlayerRecalculatedHp(0,100,50,true) == 50);
}
}
int main()
{
    StatsGoldens(); StatsBoundaries();
    std::cout << "PASS: player derived stats composition, HP and stamina preservation\n";
}