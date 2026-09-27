#pragma once
#include <array>
#include <algorithm>

enum class TankRunCore { None = 0, Ricochet, Assault, Drone };

// Run-local choices. An empty value leaves the original game rules unchanged.
struct TankRunModifiers {
	bool enabled = false;
	bool expedition = false;
	bool ricochet = false;
	bool heavy = false;
	bool rapid = false;
	bool thrusters = false;
	bool capacitor = false;
	bool repair = false;
	bool drones = false;
	bool pierce = false;
	TankRunCore core = TankRunCore::None;
	bool scatterShot = false;
	bool homing = false;
	bool dashBurst = false;
	bool overdrive = false;
	bool meleeBlade = false;
	bool bladeReach = false;
	bool impactDrive = false;
	bool perfectDodge = false;
	bool droneFocus = false;
	bool droneGuard = false;
	bool meleeTempo = false;
	bool finisherCharge = false;
	bool railCannon = false;
	bool droneLaserLink = false;
	bool slashWave = false;
	bool parryBlade = false;
	bool extraBarrel1 = false;
	bool extraBarrel2 = false;
	bool fanMount = false;
	bool alternatingFire = false;
	bool heavyDroneCore = false;
	bool lightBladeActuator = false;
	bool heavyBladeEdge = false;
	bool chainLightning = false;
	bool markDetonation = false;
	bool boomerangShell = false;
	bool killBurst = false;
	bool droneCharge = false;
	bool droneRebuildBomb = false;
	bool targetPainter = false;
	bool autonomousSpread = false;
	bool dashSlash = false;
	bool spinBlade = false;
	bool wallSmash = false;
	std::array<float,42> effectPower=[] {std::array<float,42> p{};p.fill(1);return p;}();
};

inline constexpr float TankEffectPower(const TankRunModifiers& modifiers,size_t index) {
    const float p=index<modifiers.effectPower.size()?modifiers.effectPower[index]:1.0f;
    return p==p?(std::clamp)(p,0.1f,5.0f):1.0f;
}
inline constexpr int TankEffectCount(const TankRunModifiers& modifiers,size_t index,int base=1) {
    return (std::max)(1,static_cast<int>(static_cast<float>(base)*TankEffectPower(modifiers,index)+0.5f));
}

struct TankRunGrowth {
    float hp = 0.15f;
    float damage = 0.25f;
    float bulletSpeed = 0.20f;
    float reload = 0.25f;
    float move = 0.12f;
};

// Compose from neutral values on every change; repeated application never stacks.
struct TankRunTuning {
	float damage = 1.0f;
	float bulletSpeed = 1.0f;
	float reloadInterval = 1.0f;
	float moveSpeed = 1.0f;
	float staminaRecovery = 1.0f;
	float maxHp = 1.0f;
	float dashSpeed = 1.0f;
	float dashCooldown = 1.0f;
	float bulletHp = 0.0f;
	float bulletInterception = 0.0f;
	int extraProjectiles = 0;
	float minimumSpread = 0.0f;
	bool reflects = false;
    int maxWallBounces = -1;
    int actorPierceCount = 0;
    int impactSplitCount = 0;
};

inline constexpr TankRunTuning MakeTankRunTuning(const TankRunModifiers& modifiers, const TankRunGrowth& growth = {})
{
	TankRunTuning result{};
	if (!modifiers.enabled) {
		return result;
	}
	// Enemy-bullet cancellation is independent of damage dealt to enemies.
	result.bulletHp = 1.0f;
	result.bulletInterception = modifiers.pierce ? 1.0f+2.0f*TankEffectPower(modifiers,7) : 1.0f;
	result.dashSpeed = 1.85f;
	result.reflects = modifiers.ricochet || modifiers.core == TankRunCore::Ricochet;
    if(modifiers.expedition) {
        result.maxWallBounces = modifiers.core==TankRunCore::Ricochet ? 2+(modifiers.ricochet?TankEffectCount(modifiers,0,2):0) : (modifiers.ricochet?TankEffectCount(modifiers,0):0);
        result.actorPierceCount = modifiers.pierce ? TankEffectCount(modifiers,7) : 0;
        result.impactSplitCount = 0; // Legacy scatter data no longer multiplies shots.
    }
	if (modifiers.core == TankRunCore::Ricochet) {
		result.damage *= 1.20f;
		result.bulletSpeed *= 1.25f;
		if (modifiers.ricochet) result.bulletSpeed *= 1.0f+0.2f*TankEffectPower(modifiers,0);
	} else if (modifiers.core == TankRunCore::Assault) {
		result.damage *= 1.80f;
		result.bulletSpeed *= 0.85f;
		result.reloadInterval *= 1.6f;
	}
	if (modifiers.heavy) {
		const float p=TankEffectPower(modifiers,1);
		result.damage *= 1.0f+(modifiers.expedition?growth.damage:0.7f)*p;
		result.bulletSpeed *= modifiers.expedition ? 1.0f+growth.bulletSpeed*p : (std::max)(0.05f,1.0f-0.28f*p);
		result.reloadInterval *= 1.0f+(modifiers.expedition?0.1f:0.2f)*p;
	}
	if (modifiers.rapid) {
		const float p=TankEffectPower(modifiers,2);
		result.damage *= modifiers.expedition ? 1.0f : (std::max)(0.05f,1.0f-0.2f*p);
		result.reloadInterval *= (std::max)(0.05f,1.0f-(modifiers.expedition?growth.reload:0.35f)*p);
	}
	if (modifiers.thrusters) {
		const float p=TankEffectPower(modifiers,3);
		result.moveSpeed = 1.0f+(modifiers.expedition?growth.move:0.18f)*p;
		result.staminaRecovery = 1.0f+0.25f*p;
		result.dashSpeed *= 1.0f+0.25f*p;
		result.dashCooldown = (std::max)(0.05f,1.0f-0.2f*p);
	}
	if (modifiers.repair) {
		result.maxHp = 1.0f+(modifiers.expedition?growth.hp:0.25f)*TankEffectPower(modifiers,5);
	}
	return result;
}

// Shared by the actor and graphics-free regression tests. Momentum is preserved
// in both axes, so a sideways dash bends a moving tank's path instead of snapping.
inline constexpr float TankDashMomentum(float previous, float direction, float speed) {
    return previous * 0.20f + direction * speed;
}
inline constexpr bool TankCanPerfectDodge(bool enabled, bool enemyBullet, float elapsed,float power=1.0f) {
    return enabled && enemyBullet && elapsed >= 0.0f && elapsed <= (std::min)(0.30f,0.20f*power);
}
struct TankMeleeComboTuning {
    float damage = 1.0f, range = 1.0f, arc = 120.0f;
    float windup = 0.05f, duration = 0.14f, recovery = 0.14f;
    float knockback = 0.16f;
};
inline constexpr TankMeleeComboTuning MakeTankMeleeCombo(int step, bool reach, bool impact,
    bool tempo = false, bool finisherCharge = false) {
    TankMeleeComboTuning result{};
    if(step == 1) { result.damage=1.10f; result.arc=145.0f; result.knockback=0.18f; }
    if(step == 2) { result.damage=1.70f; result.range=1.10f; result.arc=200.0f;
        result.windup=0.10f; result.duration=0.20f; result.recovery=0.21f; result.knockback=0.36f; }
    if(reach) { result.range*=1.30f; result.damage*=1.20f; }
    if(impact) result.knockback*=1.50f;
    if(tempo) { result.windup*=0.82f; result.duration*=0.82f; result.recovery*=0.82f; }
    if(finisherCharge && step==2) { result.damage*=1.50f; result.knockback*=1.20f; }
    return result;
}

inline constexpr TankMeleeComboTuning MakeTankMeleeCombo(int step,const TankRunModifiers& modifiers) {
    auto result=MakeTankMeleeCombo(step,false,false);
    if(!modifiers.enabled)return result;
    if(modifiers.bladeReach) {const float p=TankEffectPower(modifiers,13);result.range*=1.0f+.3f*p;result.damage*=1.0f+.2f*p;}
    if(modifiers.impactDrive)result.knockback*=1.0f+.5f*TankEffectPower(modifiers,14);
    if(modifiers.meleeTempo) {const float rate=(std::max)(.05f,1.0f-.18f*TankEffectPower(modifiers,18));result.windup*=rate;result.duration*=rate;result.recovery*=rate;}
    if(modifiers.finisherCharge&&step==2) {const float p=TankEffectPower(modifiers,19);result.damage*=1.0f+.5f*p;result.knockback*=1.0f+.2f*p;}
    if(modifiers.lightBladeActuator) {const float p=TankEffectPower(modifiers,29);result.damage*=(std::max)(.1f,1-.05f*p);const float speed=(std::max)(.2f,1-.15f*p);result.windup*=speed;result.duration*=speed;result.recovery*=speed;}
    if(modifiers.heavyBladeEdge) {const float p=TankEffectPower(modifiers,30);result.damage*=1+.45f*p;const float speed=1+.18f*p;result.windup*=speed;result.duration*=speed;result.recovery*=speed;}
    return result;
}

// The actor and card demonstrations share the same attack tradeoffs. These
// scales compose with the owner's damage, speed and reload upgrades.
struct TankDroneTuning {
    float damageScale=0.35f, reloadSeconds=0.75f, spreadDegrees=6.0f;
    float bulletHp=1.0f, interception=1.0f;
    float sizeScale=1.0f,trailScale=1.0f;
};
inline constexpr TankDroneTuning MakeTankDroneTuning(const TankRunModifiers& modifiers, bool primaryStyle) {
    TankDroneTuning result{};
    if(primaryStyle) { result.damageScale=1.0f; result.reloadSeconds=0.5f; result.spreadDegrees=0.0f; }
    if(!modifiers.enabled) return result;
    if(modifiers.droneFocus) {const float p=TankEffectPower(modifiers,16);result.damageScale*=1.0f+(modifiers.expedition?.15f:.4f)*p;if(!modifiers.expedition)result.reloadSeconds*=1.0f+.15f*p;result.spreadDegrees=0;}
    if(modifiers.heavyDroneCore) {const float p=TankEffectPower(modifiers,28);result.damageScale*=1.0f+.45f*p;result.reloadSeconds*=1.0f+.2f*p;result.sizeScale=1+.2f*p;result.trailScale=1+.25f*p;}
    if(modifiers.droneGuard) {result.bulletHp=1.0f+2.0f*TankEffectPower(modifiers,17);result.interception=result.bulletHp;}
    return result;
}

// Cross-card effects compose at the moment of firing/dashing. Keeping the
// transient Overdrive state separate prevents a permanent stronger homing buff.
struct TankRunSynergy {
    float homingTurnRate = 0.0f;
    bool dashExplosion = false;
    float explosionRadius = 3.2f;
    float explosionDamageScale = 2.0f;
};
inline constexpr TankRunSynergy MakeTankRunSynergy(const TankRunModifiers& modifiers, bool overdriveActive) {
    TankRunSynergy result{};
    if(!modifiers.enabled) return result;
    if(modifiers.homing) result.homingTurnRate =
        (modifiers.expedition && modifiers.overdrive && overdriveActive ? 3.2f : 1.6f)*TankEffectPower(modifiers,9);
    result.dashExplosion = modifiers.expedition && modifiers.heavy && modifiers.dashBurst;
    result.explosionDamageScale=(result.dashExplosion?2.0f:1.4f)*TankEffectPower(modifiers,10);
    return result;
}
