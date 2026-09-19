#include "../game/player/TankExpeditionLoadout.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>

static bool Near(float a, float b) { return std::abs(a - b) < 0.0001f; }

static void MaintenanceEconomy() {
    TankExpeditionMaintenance maintenance;
    assert(!maintenance.Spend(0));
    assert(!maintenance.Refund(0));
    assert(!maintenance.AwardRoom(0));
    assert(!maintenance.AwardRoom(5)); // boss/event rewards cannot mint an extra point
    for (int room = 1; room <= 4; ++room) {
        assert(maintenance.AwardRoom(room));
        assert(!maintenance.AwardRoom(room));
    }
    assert(maintenance.Points() == 4);
    assert(!maintenance.Spend(-1) && !maintenance.Spend(3));
    for (int rank = 1; rank <= 3; ++rank) {
        assert(maintenance.Spend(0));
        assert(maintenance.Rank(0) == rank);
    }
    assert(!maintenance.Spend(0));
    assert(maintenance.Points() == 1);
    assert(Near(maintenance.MoveScale(), 1.18f));
    assert(maintenance.Spend(1));
    assert(!maintenance.Spend(2));
    assert(maintenance.Refund(0));
    assert(maintenance.Spend(2));
    assert(Near(maintenance.MoveScale(), 1.12f));
    assert(Near(maintenance.ReloadScale(), 0.93f));
    assert(Near(maintenance.DamageScale(), 0.92f));
    assert(maintenance.MitigateDamage(0) == 0);
    assert(maintenance.MitigateDamage(1) == 1);
    assert(maintenance.MitigateDamage(12) == 11);
    assert(maintenance.MitigateDamage(100) == 92);
    assert(maintenance.MitigateDamage((std::numeric_limits<uint32_t>::max)()) > 0);

    // Repeated respecs conserve the four earned points and derive each buff
    // from the current rank; reading/recomputing never compounds a multiplier.
    for (int cycle = 0; cycle < 100; ++cycle) {
        for (int stat = 0; stat < 3; ++stat) while (maintenance.Refund(stat)) {}
        assert(maintenance.Points() == 4);
        assert(Near(maintenance.MoveScale(), 1.0f));
        assert(Near(maintenance.ReloadScale(), 1.0f));
        assert(Near(maintenance.DamageScale(), 1.0f));
        assert(maintenance.MitigateDamage((std::numeric_limits<uint32_t>::max)()) == (std::numeric_limits<uint32_t>::max)());
        for (int rank = 0; rank < 3; ++rank) assert(maintenance.Spend(cycle % 3));
        assert(maintenance.Spend((cycle + 1) % 3));
    }
    maintenance = {};
    assert(maintenance.Points() == 0 && maintenance.Rank(0) == 0);
    assert(maintenance.AwardRoom(1));
    assert(maintenance.AwardRoom(2));
    assert(maintenance.AwardRoom(3));
    for (int rank = 0; rank < 3; ++rank) assert(maintenance.Spend(2));
    assert(Near(maintenance.DamageScale(), 0.76f));
    assert(maintenance.MitigateDamage(100) == 76);
    assert(maintenance.MitigateDamage(1) == 1);
}

static void EvolutionFamilies() {
    std::set<std::string_view> ids;
    for (const auto& specialization : kTankExpeditionSpecializations) {
        assert(ids.insert(specialization.id).second);
        assert(FindTankExpeditionSpecialization(specialization.id) == &specialization);
        assert(specialization.barrels >= 1 && specialization.barrels <= 3);
        assert(specialization.reloadScale > 0 && specialization.damageScale > 0 && specialization.speedScale > 0);
        assert((std::string_view(specialization.starter) == "Overseer") == (specialization.drones > 0));
    }
    for (const auto starter : { "Twin", "MachineGun", "Overseer" }) {
        int choices = 0;
        for (const auto& specialization : kTankExpeditionSpecializations) {
            if (std::string_view(starter) == specialization.starter) ++choices;
        }
        assert(choices == 2);
    }
    assert(!FindTankExpeditionSpecialization("Triple"));
    const auto* fan = FindTankExpeditionSpecialization("exp_twin_fan");
    const auto* focus = FindTankExpeditionSpecialization("exp_twin_lance");
    assert(fan->fanAngle > focus->fanAngle && fan->barrels > focus->barrels);
    assert(focus->speedScale > fan->speedScale && focus->damageScale > fan->damageScale);
    const auto* storm = FindTankExpeditionSpecialization("exp_machine_storm");
    const auto* bank = FindTankExpeditionSpecialization("exp_machine_bank");
    assert(storm->alternate && storm->reloadScale < bank->reloadScale);
    assert(bank->reflect && !storm->reflect && bank->randomSpread == 0.0f);

    const auto* swarm = FindTankExpeditionSpecialization("exp_overseer_swarm");
    const auto* artillery = FindTankExpeditionSpecialization("exp_overseer_artillery");
    assert(TankExpeditionDroneLimit(nullptr, 32, false, false) == 6);
    assert(TankExpeditionDroneLimit(swarm, 32, false, false) == 8);
    assert(TankExpeditionDroneLimit(artillery, 32, false, false) == 3);
    assert(TankExpeditionDroneLimit(swarm, 32, true, true) == 12);
    assert(TankExpeditionDroneLimit(artillery, 32, true, true) == 5);
    assert(artillery->damageScale > swarm->damageScale && artillery->randomSpread == 0.0f);
}

int main() {
    MaintenanceEconomy();
    EvolutionFamilies();
    std::cout << "Tank expedition loadout tests passed.\n";
}
