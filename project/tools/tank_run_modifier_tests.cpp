#include "../game/player/TankRunModifiers.h"

namespace {
constexpr bool Near(float actual, float expected)
{
	return actual > expected - 0.0001f && actual < expected + 0.0001f;
}

constexpr bool ValidateRunModifierCombinations(TankRunCore core, unsigned combinations)
{
	for (unsigned bits = 0; bits < combinations; ++bits) {
		TankRunModifiers modifiers{};
		modifiers.enabled = true;
		modifiers.core = core;
		modifiers.ricochet = (bits & 1) != 0;
		modifiers.heavy = (bits & 2) != 0;
		modifiers.rapid = (bits & 4) != 0;
		modifiers.thrusters = (bits & 8) != 0;
		modifiers.capacitor = (bits & 16) != 0;
		modifiers.repair = (bits & 32) != 0;
		modifiers.drones = (bits & 64) != 0;
		modifiers.pierce = (bits & 128) != 0;
		modifiers.scatterShot = (bits & 256) != 0;
		modifiers.homing = (bits & 512) != 0;
		modifiers.dashBurst = (bits & 1024) != 0;
		modifiers.overdrive = (bits & 2048) != 0;
		const TankRunTuning tuning = MakeTankRunTuning(modifiers);
		// More enemy damage must not implicitly grant stronger bullet defense.
		if (tuning.bulletHp != 1.0f ||
			tuning.bulletInterception != (modifiers.pierce ? 3.0f : 1.0f)) {
			return false;
		}
		const float coreDamage = core == TankRunCore::Ricochet ? 0.55f : core == TankRunCore::Assault ? 0.45f : 1.0f;
		const float coreSpeed = core == TankRunCore::Ricochet ? 1.25f * (modifiers.ricochet ? 1.2f : 1.0f) : core == TankRunCore::Assault ? 0.85f : 1.0f;
		const float coreInterval = core == TankRunCore::Assault ? 1.6f : 1.0f;
		const int coreExtra = core == TankRunCore::Ricochet ? 2 : core == TankRunCore::Assault ? 4 : 0;
		if (!Near(tuning.damage, coreDamage * (modifiers.heavy ? 1.7f : 1.0f) * (modifiers.rapid ? 0.8f : 1.0f) * (modifiers.scatterShot ? 0.75f : 1.0f)) ||
			!Near(tuning.reloadInterval, coreInterval * (modifiers.heavy ? 1.2f : 1.0f) * (modifiers.rapid ? 0.65f : 1.0f)) ||
			!Near(tuning.bulletSpeed, coreSpeed * (modifiers.heavy ? 0.72f : 1.0f)) ||
			tuning.maxHp != (modifiers.repair ? 1.25f : 1.0f) ||
			tuning.moveSpeed != (modifiers.thrusters ? 1.18f : 1.0f) ||
			!Near(tuning.dashSpeed, 1.85f * (modifiers.thrusters ? 1.25f : 1.0f)) ||
			tuning.extraProjectiles != coreExtra + (modifiers.scatterShot ? 2 : 0) ||
			tuning.extraProjectiles > 6 ||
			tuning.reflects != (modifiers.ricochet || core == TankRunCore::Ricochet)) {
			return false;
		}
		modifiers.enabled = false;
		const TankRunTuning legacy = MakeTankRunTuning(modifiers);
		if (legacy.damage != 1.0f || legacy.reloadInterval != 1.0f || legacy.bulletSpeed != 1.0f ||
			legacy.moveSpeed != 1.0f || legacy.maxHp != 1.0f || legacy.staminaRecovery != 1.0f ||
			legacy.dashSpeed != 1.0f || legacy.dashCooldown != 1.0f ||
			legacy.bulletHp != 0.0f || legacy.bulletInterception != 0.0f ||
			legacy.extraProjectiles != 0 || legacy.minimumSpread != 0.0f || legacy.reflects) {
			return false;
		}
	}
	return true;
}

static_assert(ValidateRunModifierCombinations(TankRunCore::None, 256), "Run modifiers must preserve legacy rules and independent bullet durability.");
static_assert(ValidateRunModifierCombinations(TankRunCore::Ricochet, 1), "Ricochet core contract.");
static_assert(ValidateRunModifierCombinations(TankRunCore::Assault, 1), "Assault core contract.");
static_assert(ValidateRunModifierCombinations(TankRunCore::Drone, 1), "Drone core contract.");
}

constexpr bool CheckSynergyUnlocks() {
    TankRunModifiers m{};
    m.homing=true;m.overdrive=true;m.heavy=true;m.dashBurst=true;
    if(MakeTankRunSynergy(m,true).homingTurnRate!=0 || MakeTankRunSynergy(m,true).dashExplosion) return false;
    m.enabled=true;
    if(MakeTankRunSynergy(m,true).homingTurnRate!=1.6f || MakeTankRunSynergy(m,true).dashExplosion) return false;
    m.expedition=true;
    if(MakeTankRunSynergy(m,false).homingTurnRate!=1.6f || MakeTankRunSynergy(m,true).homingTurnRate!=3.2f) return false;
    if(!MakeTankRunSynergy(m,true).dashExplosion) return false;
    m.overdrive=false;
    if(MakeTankRunSynergy(m,true).homingTurnRate!=1.6f) return false;
    m.homing=false;
    if(MakeTankRunSynergy(m,true).homingTurnRate!=0) return false;
    m.heavy=false;
    if(MakeTankRunSynergy(m,true).dashExplosion) return false;
    m.heavy=true;m.dashBurst=false;
    if(MakeTankRunSynergy(m,true).dashExplosion) return false;
    return true;
}
constexpr bool CheckProjectileGrowthUnlocks() {
    TankRunModifiers m{};m.enabled=true;m.expedition=true;
    auto p=MakeTankRunTuning(m);
    if(p.maxWallBounces!=0||p.actorPierceCount||p.impactSplitCount) return false;
    m.ricochet=true;
    if(MakeTankRunTuning(m).maxWallBounces!=1) return false;
    m.ricochet=false;m.core=TankRunCore::Ricochet;
    if(MakeTankRunTuning(m).maxWallBounces!=2) return false;
    m.ricochet=true;m.pierce=true;m.scatterShot=true;
    p=MakeTankRunTuning(m);
    if(p.maxWallBounces!=4||p.actorPierceCount!=1||p.impactSplitCount!=2) return false;
    m.expedition=false;p=MakeTankRunTuning(m);
    return p.maxWallBounces==-1 && p.actorPierceCount==0 && p.impactSplitCount==0;
}
static_assert(CheckProjectileGrowthUnlocks(), "Expedition earns bounded bounce, actor piercing, and nonrecursive split; arena is unchanged.");
static_assert(CheckSynergyUnlocks(), "Strong homing is transient, explosions require both cards, and arena stays unchanged.");

int main()
{
	for (int core = 0; core < 4; ++core) {
		if (!ValidateRunModifierCombinations(static_cast<TankRunCore>(core), 4096)) return core + 1;
	}
	return 0;
}
