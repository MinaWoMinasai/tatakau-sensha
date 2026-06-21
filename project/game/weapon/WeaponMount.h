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
};
