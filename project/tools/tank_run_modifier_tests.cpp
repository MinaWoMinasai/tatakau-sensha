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
		const float coreDamage = core == TankRunCore::Ricochet ? 1.20f : core == TankRunCore::Assault ? 1.80f : 1.0f;
		const float coreSpeed = core == TankRunCore::Ricochet ? 1.25f * (modifiers.ricochet ? 1.2f : 1.0f) : core == TankRunCore::Assault ? 0.85f : 1.0f;
		const float coreInterval = core == TankRunCore::Assault ? 1.6f : 1.0f;
		if (!Near(tuning.damage, coreDamage * (modifiers.heavy ? 1.7f : 1.0f) * (modifiers.rapid ? 0.8f : 1.0f)) ||
			!Near(tuning.reloadInterval, coreInterval * (modifiers.heavy ? 1.2f : 1.0f) * (modifiers.rapid ? 0.65f : 1.0f)) ||
			!Near(tuning.bulletSpeed, coreSpeed * (modifiers.heavy ? 0.72f : 1.0f)) ||
			tuning.maxHp != (modifiers.repair ? 1.25f : 1.0f) ||
			!Near(tuning.moveSpeed, modifiers.thrusters ? 1.18f : 1.0f) ||
			!Near(tuning.dashSpeed, 1.85f * (modifiers.thrusters ? 1.25f : 1.0f)) ||
			tuning.extraProjectiles != 0 || tuning.impactSplitCount != 0 ||
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
    if(p.maxWallBounces!=4||p.actorPierceCount!=1||p.impactSplitCount!=0) return false;
    m.expedition=false;p=MakeTankRunTuning(m);
    return p.maxWallBounces==-1 && p.actorPierceCount==0 && p.impactSplitCount==0;
}
static_assert(CheckProjectileGrowthUnlocks(), "Ricochet and piercing remain bounded; even legacy scatter data never creates extra shots.");
static_assert(CheckSynergyUnlocks(), "Strong homing is transient, explosions require both cards, and arena stays unchanged.");

constexpr bool CheckActionContracts() {
    // A perpendicular dash retains exactly 20% of the previous movement.
    if(!Near(TankDashMomentum(.20f,0,.60f),.04f) || !Near(TankDashMomentum(-.10f,1,.60f),.58f)) return false;
    if(TankCanPerfectDodge(false,true,.1f) || TankCanPerfectDodge(true,false,.1f)) return false;
    if(!TankCanPerfectDodge(true,true,.1f) || TankCanPerfectDodge(true,true,.21f)) return false;
    const auto light=MakeTankMeleeCombo(0,false,false),finisher=MakeTankMeleeCombo(2,false,false);
    if(finisher.damage<=light.damage || finisher.arc<=light.arc || finisher.knockback<=light.knockback) return false;
    for(int step=0;step<3;++step) {
        const auto base=MakeTankMeleeCombo(step,false,false),reach=MakeTankMeleeCombo(step,true,false),impact=MakeTankMeleeCombo(step,false,true);
        if(!Near(reach.range/base.range,1.30f) || !Near(reach.damage/base.damage,1.20f)) return false;
        if(!Near(impact.knockback/base.knockback,1.50f)) return false;
        if(base.windup<=0 || base.recovery<=0 || base.duration<=0) return false;
    }
    TankRunModifiers m{};m.enabled=m.expedition=m.heavy=m.rapid=m.thrusters=m.repair=true;
    const auto t=MakeTankRunTuning(m);
    return Near(t.damage,1.25f) && Near(t.reloadInterval,.75f*1.10f) && Near(t.moveSpeed,1.12f) && Near(t.maxHp,1.15f);
}
static_assert(CheckActionContracts(), "Melee combos, small starter bonuses, momentum and optional bullet-only perfect dodge must compose.");

constexpr bool CheckStyleUpgrades() {
    for(int step=0;step<3;++step) {
        const auto base=MakeTankMeleeCombo(step,false,false);
        const auto combo=MakeTankMeleeCombo(step,true,true,true,true);
        if(!Near(combo.windup/base.windup,.82f)||!Near(combo.duration/base.duration,.82f)||!Near(combo.recovery/base.recovery,.82f))return false;
        if(!Near(combo.range/base.range,1.3f)||!Near(combo.damage/base.damage,1.2f*(step==2?1.5f:1.0f)))return false;
        if(!Near(combo.knockback/base.knockback,1.5f*(step==2?1.2f:1.0f)))return false;
    }
    TankRunModifiers mods{};mods.enabled=true;
    const auto primary=MakeTankDroneTuning(mods,true),support=MakeTankDroneTuning(mods,false);
    if(primary.damageScale<=support.damageScale||primary.reloadSeconds>=support.reloadSeconds)return false;
    mods.droneFocus=mods.droneGuard=true;
    const auto upgraded=MakeTankDroneTuning(mods,true);
    if(!Near(upgraded.damageScale,primary.damageScale*1.4f)||!Near(upgraded.reloadSeconds,primary.reloadSeconds*1.15f))return false;
    if(upgraded.bulletHp!=3||upgraded.interception!=3||upgraded.spreadDegrees!=0)return false;
    // Drone defense must not accidentally buff the player's main cannon.
    if(MakeTankRunTuning(mods).bulletHp!=1||MakeTankRunTuning(mods).bulletInterception!=1)return false;
    mods.enabled=false;
    const auto disabled=MakeTankDroneTuning(mods,true);
    return disabled.damageScale==primary.damageScale&&disabled.bulletHp==primary.bulletHp;
}
static_assert(CheckStyleUpgrades(), "New style upgrades must compose, preserve telegraph time and only improve the intended weapon.");

constexpr bool CheckAuthoredEffectPower() {
    for(float p:{0.1f,1.0f,2.0f,5.0f}) {
        TankRunModifiers m{};m.enabled=m.expedition=true;m.effectPower.fill(p);
        m.heavy=true;auto t=MakeTankRunTuning(m);
        if(!Near(t.damage,1+.25f*p)||!Near(t.bulletSpeed,1+.2f*p)||!Near(t.reloadInterval,1+.1f*p))return false;
        m.heavy=false;m.rapid=true;t=MakeTankRunTuning(m);
        if(!Near(t.reloadInterval,(std::max)(.05f,1-.25f*p)))return false;
        m.rapid=false;m.repair=m.thrusters=m.ricochet=m.pierce=true;t=MakeTankRunTuning(m);
        if(!Near(t.maxHp,1+.15f*p)||!Near(t.moveSpeed,1+.12f*p)||!Near(t.bulletInterception,1+2*p))return false;
        if(t.maxWallBounces!=TankEffectCount(m,0)||t.actorPierceCount!=TankEffectCount(m,7)||t.extraProjectiles!=0)return false;
        m.bladeReach=m.impactDrive=m.meleeTempo=m.finisherCharge=true;
        const auto combo=MakeTankMeleeCombo(2,m),base=MakeTankMeleeCombo(2,false,false);
        if(!Near(combo.range/base.range,1+.3f*p)||!Near(combo.damage/base.damage,(1+.2f*p)*(1+.5f*p)))return false;
        if(!Near(combo.knockback/base.knockback,(1+.5f*p)*(1+.2f*p)))return false;
        if(!Near(combo.recovery/base.recovery,(std::max)(.05f,1-.18f*p)))return false;
        m.droneFocus=m.droneGuard=true;const auto drone=MakeTankDroneTuning(m,true);
        if(!Near(drone.damageScale,1+.4f*p)||!Near(drone.reloadSeconds,.5f*(1+.15f*p))||!Near(drone.bulletHp,1+2*p))return false;
        m.homing=m.dashBurst=true;const auto synergy=MakeTankRunSynergy(m,false);
        if(!Near(synergy.homingTurnRate,1.6f*p)||!Near(synergy.explosionDamageScale,1.4f*p))return false;
        if(!TankCanPerfectDodge(true,true,(std::min)(.3f,.2f*p),p)||TankCanPerfectDodge(true,true,.301f,p))return false;
    }
    TankRunModifiers m{};m.enabled=m.expedition=m.heavy=m.repair=m.thrusters=true;
    TankRunGrowth g{};g.damage=.8f;g.hp=.7f;g.move=.6f;
    const auto t=MakeTankRunTuning(m,g);
    return Near(t.damage,1.8f)&&Near(t.maxHp,1.7f)&&Near(t.moveSpeed,1.6f);
}
static_assert(CheckAuthoredEffectPower(), "F6 effect multipliers must alter every numeric combat effect, preserve bounded integers and remove hidden foundation caps.");

int main()
{
	for (int core = 0; core < 4; ++core) {
		if (!ValidateRunModifierCombinations(static_cast<TankRunCore>(core), 4096)) return core + 1;
	}
	return 0;
}
