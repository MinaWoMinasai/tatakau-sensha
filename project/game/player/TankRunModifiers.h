#pragma once

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
};

struct TankRunGrowth {
    float hp = 0.35f;
    float damage = 0.80f;
    float bulletSpeed = 0.20f;
    float reload = 0.40f;
    float move = 0.25f;
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
	result.bulletInterception = modifiers.pierce ? 3.0f : 1.0f;
	result.dashSpeed = 1.85f;
	result.reflects = modifiers.ricochet || modifiers.core == TankRunCore::Ricochet;
    if(modifiers.expedition) {
        result.maxWallBounces = modifiers.core==TankRunCore::Ricochet ? (modifiers.ricochet?4:2) : (modifiers.ricochet?1:0);
        result.actorPierceCount = modifiers.pierce ? 1 : 0;
        result.impactSplitCount = modifiers.scatterShot ? 2 : 0;
    }
	if (modifiers.core == TankRunCore::Ricochet) {
		result.damage *= 0.55f;
		result.bulletSpeed *= 1.25f;
		if (modifiers.ricochet) result.bulletSpeed *= 1.2f;
		result.extraProjectiles = 2;
		result.minimumSpread = 16.0f;
	} else if (modifiers.core == TankRunCore::Assault) {
		result.damage *= 0.45f;
		result.bulletSpeed *= 0.85f;
		result.reloadInterval *= 1.6f;
		result.extraProjectiles = 4;
		result.minimumSpread = 44.0f;
	}
	if (modifiers.scatterShot) {
		result.damage *= 0.75f;
		result.extraProjectiles += 2;
		if (result.minimumSpread < 28.0f) result.minimumSpread = 28.0f;
	}
	if (modifiers.heavy) {
		result.damage *= modifiers.expedition ? 1.0f + growth.damage : 1.7f;
		result.bulletSpeed *= modifiers.expedition ? 1.0f + growth.bulletSpeed : 0.72f;
		result.reloadInterval *= modifiers.expedition ? 1.10f : 1.2f;
	}
	if (modifiers.rapid) {
		result.damage *= modifiers.expedition ? 1.0f : 0.8f;
		result.reloadInterval *= modifiers.expedition ? 1.0f - growth.reload : 0.65f;
	}
	if (modifiers.thrusters) {
		result.moveSpeed = modifiers.expedition ? 1.0f + growth.move : 1.18f;
		result.staminaRecovery = 1.25f;
		result.dashSpeed *= 1.25f;
		result.dashCooldown = 0.8f;
	}
	if (modifiers.repair) {
		result.maxHp = modifiers.expedition ? 1.0f + growth.hp : 1.25f;
	}
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
        modifiers.expedition && modifiers.overdrive && overdriveActive ? 3.2f : 1.6f;
    result.dashExplosion = modifiers.expedition && modifiers.heavy && modifiers.dashBurst;
    return result;
}
