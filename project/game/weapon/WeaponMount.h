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

enum class BarrelShape {
	Box = 0,
	Heavy,
	Short,
	Wide,
	Trapezoid,
};

struct WeaponMountConfig {
	std::string model = "gunBarrel.obj";
	BarrelShape barrelShape = BarrelShape::Box;
	cg2::Vector3 offset = { 0.72f, 0.0f, 0.0f };
	cg2::Vector3 scale = { 1.25f, 0.24f, 0.24f };
	float angleDeg = 0.0f;
	float muzzleForward = 0.95f;
	bool fires = true;
	WeaponType weaponType = WeaponType::Projectile;
	float damageScale = 1.0f;
	float projectileSpeedScale = 1.0f;
	int fireGroup = 0;
	float reloadScale = 1.0f;
	float recoilScale = 1.0f;
	cg2::Vector4 barrelColor = { 0.25f, 1.0f, 0.95f, 1.0f };
	cg2::Vector4 outlineColor = { 0.80f, 1.0f, 0.95f, 1.0f };
	cg2::Vector4 effectColor = { 0.25f, 1.0f, 0.95f, 1.0f };
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
	float meleeComboResetTime = 0.90f;
	float meleeCombo1DamageScale = 1.0f;
	float meleeCombo2DamageScale = 1.0f;
	float meleeCombo3DamageScale = 1.35f;
	float meleeCombo1RangeScale = 1.0f;
	float meleeCombo2RangeScale = 1.0f;
	float meleeCombo3RangeScale = 1.18f;
	float meleeCombo1Windup = 0.08f;
	float meleeCombo2Windup = 0.10f;
	float meleeCombo3Windup = 0.18f;
	float meleeCombo1Recovery = 0.10f;
	float meleeCombo2Recovery = 0.11f;
	float meleeCombo3Recovery = 0.24f;
};
