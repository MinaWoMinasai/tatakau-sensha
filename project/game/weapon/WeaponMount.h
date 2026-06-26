#pragma once

#include "Struct.h"
#include <string>

enum class WeaponType {
	Projectile = 0,
	Laser,
	Mine,
	Drone,
	Melee,
};

struct WeaponMountConfig {
	std::string model = "gunBarrel.obj";
	Vector3 offset = { 0.72f, 0.0f, 0.0f };
	Vector3 scale = { 1.25f, 0.24f, 0.24f };
	float angleDeg = 0.0f;
	float muzzleForward = 0.95f;
	bool fires = true;
	WeaponType weaponType = WeaponType::Projectile;
	float damageScale = 1.0f;
	float projectileSpeedScale = 1.0f;
	Vector4 effectColor = { 0.25f, 1.0f, 0.95f, 1.0f };
	float laserRange = 18.0f;
	float laserWidth = 0.18f;
	float laserDuration = 0.12f;
	float laserDamageInterval = 0.08f;
	float mineRadius = 3.2f;
	float mineFuseTime = 0.45f;
	float mineLifeTime = 5.0f;
	float meleeRange = 3.4f;
	float meleeArcDeg = 105.0f;
	float meleeWidth = 0.20f;
	float meleeDuration = 0.18f;
};
