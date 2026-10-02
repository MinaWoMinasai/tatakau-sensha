#include "PlayerClassCatalog.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>
#include <nlohmann/json.hpp>

namespace {
cg2::Vector3 ReadVector3(const nlohmann::json& json, const cg2::Vector3& fallback)
{
	if (!json.is_array() || json.size() < 3) {
		return fallback;
	}
	return {
		json[0].get<float>(),
		json[1].get<float>(),
		json[2].get<float>()
	};
}

cg2::Vector2 ReadVector2(const nlohmann::json& json, const cg2::Vector2& fallback)
{
	if (!json.is_array() || json.size() < 2) {
		return fallback;
	}
	if (!json[0].is_number() || !json[1].is_number()) {
		return fallback;
	}
	return {
		json[0].get<float>(),
		json[1].get<float>()
	};
}

cg2::Vector4 ReadVector4(const nlohmann::json& json, const cg2::Vector4& fallback)
{
	if (!json.is_array() || json.size() < 4) {
		return fallback;
	}
	return {
		json[0].get<float>(),
		json[1].get<float>(),
		json[2].get<float>(),
		json[3].get<float>()
	};
}

ClassType ClassTypeFromString(const std::string& id)
{
	if (id == "Twin") return ClassType::Twin;
	if (id == "MachineGun") return ClassType::MachineGun;
	if (id == "Overseer") return ClassType::Overseer;
	if (id == "Triple") return ClassType::Triple;
	if (id == "Assassin") return ClassType::Assassin;
	if (id == "Bounder") return ClassType::Bounder;
	if (id == "Ninja") return ClassType::Ninja;
	if (id == "Smasher") return ClassType::Smasher;
	if (id == "Summoner") return ClassType::Summoner;
	return ClassType::Basic;
}

const std::array<ClassType, 10>& EditableClassTypes()
{
	static const std::array<ClassType, 10> types = {
		ClassType::Basic,
		ClassType::Twin,
		ClassType::MachineGun,
		ClassType::Overseer,
		ClassType::Triple,
		ClassType::Assassin,
		ClassType::Bounder,
		ClassType::Ninja,
		ClassType::Smasher,
		ClassType::Summoner
	};
	return types;
}

WeaponType WeaponTypeFromString(const std::string& id)
{
	if (id == "Laser") return WeaponType::Laser;
	if (id == "Mine") return WeaponType::Mine;
	if (id == "Drone") return WeaponType::Drone;
	if (id == "Melee") return WeaponType::Melee;
	return WeaponType::Projectile;
}

const char* WeaponTypeToString(WeaponType type)
{
	switch (type) {
	case WeaponType::Projectile: return "Projectile";
	case WeaponType::Laser: return "Laser";
	case WeaponType::Mine: return "Mine";
	case WeaponType::Drone: return "Drone";
	case WeaponType::Melee: return "Melee";
	}
	return "Projectile";
}

BarrelShape BarrelShapeFromString(const std::string& id)
{
	if (id == "Heavy") return BarrelShape::Heavy;
	if (id == "Short") return BarrelShape::Short;
	if (id == "Wide") return BarrelShape::Wide;
	if (id == "Trapezoid") return BarrelShape::Trapezoid;
	return BarrelShape::Box;
}

const char* BarrelShapeToString(BarrelShape shape)
{
	switch (shape) {
	case BarrelShape::Box: return "Box";
	case BarrelShape::Heavy: return "Heavy";
	case BarrelShape::Short: return "Short";
	case BarrelShape::Wide: return "Wide";
	case BarrelShape::Trapezoid: return "Trapezoid";
	}
	return "Box";
}

PlayerBodyShape BodyShapeFromString(const std::string& id)
{
	if (id == "Box") return PlayerBodyShape::Box;
	if (id == "Triangle") return PlayerBodyShape::Triangle;
	if (id == "Pentagon") return PlayerBodyShape::Pentagon;
	return PlayerBodyShape::Circle;
}

const char* BodyShapeToString(PlayerBodyShape shape)
{
	switch (shape) {
	case PlayerBodyShape::Circle: return "Circle";
	case PlayerBodyShape::Box: return "Box";
	case PlayerBodyShape::Triangle: return "Triangle";
	case PlayerBodyShape::Pentagon: return "Pentagon";
	}
	return "Circle";
}

nlohmann::json Vector3ToJson(const cg2::Vector3& value)
{
	return nlohmann::json::array({ value.x, value.y, value.z });
}

nlohmann::json Vector4ToJson(const cg2::Vector4& value)
{
	return nlohmann::json::array({ value.x, value.y, value.z, value.w });
}
}

const char* ClassTypeToString(ClassType type)
{
	switch (type) {
	case ClassType::Basic: return "Basic";
	case ClassType::Twin: return "Twin";
	case ClassType::MachineGun: return "MachineGun";
	case ClassType::Overseer: return "Overseer";
	case ClassType::Triple: return "Triple";
	case ClassType::Assassin: return "Assassin";
	case ClassType::Bounder: return "Bounder";
	case ClassType::Ninja: return "Ninja";
	case ClassType::Smasher: return "Smasher";
	case ClassType::Summoner: return "Summoner";
	}
	return "Unknown";
}

bool PlayerClassCatalog::Load(const std::string& path)
{
	std::unordered_map<std::string, PlayerClassConfig> loadedConfigs;
	std::vector<std::string> loadedOrder;
	auto reportFailure = [](const std::string& reason) {
		std::cerr << "[PlayerClass] Reload failed: " << reason << std::endl;
		return false;
	};

	std::ifstream file(path);
	if (!file.is_open()) {
		return reportFailure("could not open " + path);
	}

	try {
		nlohmann::json root;
		file >> root;
		if (!root.is_object() || !root.contains("classes") || !root["classes"].is_array()) {
			return reportFailure("classes must be an array");
		}
		const nlohmann::json& classes = root["classes"];
		if (classes.empty()) {
			return reportFailure("classes must not be empty");
		}

	for (const nlohmann::json& item : classes) {
		if (!item.is_object() || !item.contains("id") || !item["id"].is_string()) {
			return reportFailure("each class requires a string id");
		}
		const std::string id = item["id"].get<std::string>();
		if (id.empty()) {
			return reportFailure("class id must not be empty");
		}
		PlayerClassConfig config = CreateDefaultConfig(ClassTypeFromString(id));
		config.id = id;
		config.type = ClassTypeFromString(id);
		config.displayName = item.value("displayName", config.displayName);
		config.requiredRank = item.value("requiredRank", config.requiredRank);
		config.usesDrone = item.value("usesDrone", config.usesDrone);
		config.maxDrones = item.value("maxDrones", config.maxDrones);
		config.reloadScale = item.value("reloadScale", config.reloadScale);
		config.bulletSpeedScale = item.value("bulletSpeedScale", config.bulletSpeedScale);
		config.bulletDamageScale = item.value("bulletDamageScale", config.bulletDamageScale);
		config.bulletCount = 1; // Legacy authoring data cannot restore multishot.
		config.spreadAngleDeg = item.value("spreadAngleDeg", config.spreadAngleDeg);
		config.randomSpread = item.value("randomSpread", config.randomSpread);
		config.reflect = item.value("reflect", config.reflect);
		config.penetrate = item.value("penetrate", config.penetrate);
		config.fireAllBarrels = item.value("fireAllBarrels", config.fireAllBarrels);
		config.alternateBarrels = item.value("alternateBarrels", config.alternateBarrels);
		config.recoilPower = item.value("recoilPower", config.recoilPower);
		config.specialActionId = item.value("specialActionId", config.specialActionId);
		config.specialActionCooldownScale = (std::max)(0.05f, item.value("specialActionCooldownScale", config.specialActionCooldownScale));
		config.specialActionStaminaCost = (std::max)(0.0f, item.value("specialActionStaminaCost", item.value("specialActionStaminaRequirement", config.specialActionStaminaCost)));
		config.saberCounterWindow = (std::max)(0.01f, item.value("saberCounterWindow", config.saberCounterWindow));
		config.saberCounterDamageScale = (std::max)(0.0f, item.value("saberCounterDamageScale", config.saberCounterDamageScale));
		config.saberCounterRangeScale = (std::max)(0.1f, item.value("saberCounterRangeScale", config.saberCounterRangeScale));
		config.bodyShape = BodyShapeFromString(item.value("bodyShape", std::string(BodyShapeToString(config.bodyShape))));
		if (item.contains("bodyScale")) {
			config.bodyScale = ReadVector2(item["bodyScale"], config.bodyScale);
		}
		if (item.contains("bodyFillColor")) {
			config.bodyFillColor = ReadVector4(item["bodyFillColor"], config.bodyFillColor);
		}
		if (item.contains("bodyOutlineColor")) {
			config.bodyOutlineColor = ReadVector4(item["bodyOutlineColor"], config.bodyOutlineColor);
		}

		config.barrels.clear();
		const nlohmann::json* mountsJson = nullptr;
		if (item.contains("weaponMounts") && item["weaponMounts"].is_array()) {
			mountsJson = &item["weaponMounts"];
		} else if (item.contains("barrels") && item["barrels"].is_array()) {
			mountsJson = &item["barrels"];
		}
		if (mountsJson) {
			for (const nlohmann::json& barrelJson : *mountsJson) {
				if (!barrelJson.is_object()) {
					return reportFailure("weaponMounts entries must be objects: " + id);
				}
				WeaponMountConfig barrel{};
				barrel.model = barrelJson.value("model", barrel.model);
				if (barrel.model.empty() ||
					!std::filesystem::exists(std::filesystem::path("resources") / barrel.model)) {
					return reportFailure("weapon model not found: " + barrel.model);
				}
				barrel.barrelShape = BarrelShapeFromString(barrelJson.value("barrelShape", std::string(BarrelShapeToString(barrel.barrelShape))));
				barrel.offset = ReadVector3(barrelJson.value("offset", nlohmann::json::array()), barrel.offset);
				barrel.scale = ReadVector3(barrelJson.value("scale", nlohmann::json::array()), barrel.scale);
				barrel.angleDeg = barrelJson.value("angleDeg", barrel.angleDeg);
				barrel.muzzleForward = barrelJson.value("muzzleForward", barrel.muzzleForward);
				barrel.fires = barrelJson.value("fires", barrel.fires);
				barrel.weaponType = WeaponTypeFromString(barrelJson.value("weaponType", std::string("Projectile")));
				barrel.damageScale = (std::max)(0.0f, barrelJson.value("damageScale", barrel.damageScale));
				barrel.projectileSpeedScale = (std::max)(0.01f, barrelJson.value("projectileSpeedScale", barrel.projectileSpeedScale));
				barrel.fireGroup = (std::max)(0, barrelJson.value("fireGroup", barrel.fireGroup));
				barrel.reloadScale = (std::max)(0.05f, barrelJson.value("reloadScale", barrel.reloadScale));
				barrel.recoilScale = (std::max)(0.0f, barrelJson.value("recoilScale", barrel.recoilScale));
				if (barrelJson.contains("barrelColor")) {
					barrel.barrelColor = ReadVector4(barrelJson["barrelColor"], barrel.barrelColor);
				}
				if (barrelJson.contains("outlineColor")) {
					barrel.outlineColor = ReadVector4(barrelJson["outlineColor"], barrel.outlineColor);
				}
				if (barrelJson.contains("effectColor")) {
					barrel.effectColor = ReadVector4(barrelJson["effectColor"], barrel.effectColor);
				}
				barrel.laserRange = (std::max)(0.1f, barrelJson.value("laserRange", barrel.laserRange));
				barrel.laserWidth = (std::max)(0.01f, barrelJson.value("laserWidth", barrel.laserWidth));
				barrel.laserDuration = (std::max)(0.01f, barrelJson.value("laserDuration", barrel.laserDuration));
				barrel.laserDamageInterval = (std::max)(0.01f, barrelJson.value("laserDamageInterval", barrel.laserDamageInterval));
				barrel.mineRadius = (std::max)(0.1f, barrelJson.value("mineRadius", barrel.mineRadius));
				barrel.mineFuseTime = (std::max)(0.0f, barrelJson.value("mineFuseTime", barrel.mineFuseTime));
				barrel.mineLifeTime = (std::max)(0.1f, barrelJson.value("mineLifeTime", barrel.mineLifeTime));
				barrel.meleeRange = (std::max)(0.1f, barrelJson.value("meleeRange", barrel.meleeRange));
				barrel.meleeArcDeg = (std::clamp)(barrelJson.value("meleeArcDeg", barrel.meleeArcDeg), 5.0f, 360.0f);
				barrel.meleeWidth = (std::max)(0.01f, barrelJson.value("meleeWidth", barrel.meleeWidth));
				barrel.meleeDuration = (std::max)(0.01f, barrelJson.value("meleeDuration", barrel.meleeDuration));
				barrel.meleeComboResetTime = (std::max)(0.05f, barrelJson.value("meleeComboResetTime", barrel.meleeComboResetTime));
				barrel.meleeCombo1DamageScale = (std::max)(0.0f, barrelJson.value("meleeCombo1DamageScale", barrel.meleeCombo1DamageScale));
				barrel.meleeCombo2DamageScale = (std::max)(0.0f, barrelJson.value("meleeCombo2DamageScale", barrel.meleeCombo2DamageScale));
				barrel.meleeCombo3DamageScale = (std::max)(0.0f, barrelJson.value("meleeCombo3DamageScale", barrel.meleeCombo3DamageScale));
				barrel.meleeCombo1RangeScale = (std::max)(0.05f, barrelJson.value("meleeCombo1RangeScale", barrel.meleeCombo1RangeScale));
				barrel.meleeCombo2RangeScale = (std::max)(0.05f, barrelJson.value("meleeCombo2RangeScale", barrel.meleeCombo2RangeScale));
				barrel.meleeCombo3RangeScale = (std::max)(0.05f, barrelJson.value("meleeCombo3RangeScale", barrel.meleeCombo3RangeScale));
				barrel.meleeCombo1Windup = (std::max)(0.0f, barrelJson.value("meleeCombo1Windup", barrel.meleeCombo1Windup));
				barrel.meleeCombo2Windup = (std::max)(0.0f, barrelJson.value("meleeCombo2Windup", barrel.meleeCombo2Windup));
				barrel.meleeCombo3Windup = (std::max)(0.0f, barrelJson.value("meleeCombo3Windup", barrel.meleeCombo3Windup));
				barrel.meleeCombo1Recovery = (std::max)(0.0f, barrelJson.value("meleeCombo1Recovery", barrel.meleeCombo1Recovery));
				barrel.meleeCombo2Recovery = (std::max)(0.0f, barrelJson.value("meleeCombo2Recovery", barrel.meleeCombo2Recovery));
				barrel.meleeCombo3Recovery = (std::max)(0.0f, barrelJson.value("meleeCombo3Recovery", barrel.meleeCombo3Recovery));
				config.barrels.push_back(barrel);
			}
		}
		if (config.barrels.empty()) {
			config.barrels = CreateDefaultConfig(config.type).barrels;
		}

		if (loadedConfigs.find(config.id) == loadedConfigs.end()) {
			loadedOrder.push_back(config.id);
		}
		loadedConfigs[config.id] = config;
	}

	// A valid configuration is authoritative: classes omitted from the JSON
	// stay unavailable. Basic is the only mandatory fallback needed to keep the
	// player in a valid state when an accidentally empty file is supplied.
	if (loadedConfigs.find("Basic") == loadedConfigs.end()) {
		PlayerClassConfig basic = CreateDefaultConfig(ClassType::Basic);
		loadedOrder.insert(loadedOrder.begin(), basic.id);
		loadedConfigs[basic.id] = basic;
	}

	} catch (const std::exception& e) {
		return reportFailure(e.what());
	}
	classConfigs_.swap(loadedConfigs);
	classOrder_.swap(loadedOrder);
	return true;
}

PlayerClassConfig PlayerClassCatalog::CreateDefaultConfig(ClassType type)
{
	PlayerClassConfig config{};
	config.type = type;
	config.id = ClassTypeToString(type);
	config.displayName = ClassTypeToString(type);
	config.requiredRank = 1;
	config.spreadAngleDeg = 10.0f;
	config.randomSpread = true;
	config.reloadScale = 1.0f;
	config.alternateBarrels = false;
	auto makeBarrel = [](const cg2::Vector3& offset, const cg2::Vector3& scale, float angleDeg) {
		WeaponMountConfig barrel{};
		barrel.model = "gunBarrel.obj";
		barrel.offset = offset;
		barrel.scale = scale;
		barrel.angleDeg = angleDeg;
		barrel.muzzleForward = 0.95f;
		barrel.fires = true;
		return barrel;
	};
	config.barrels = {
		makeBarrel({ 0.72f, 0.0f, 0.0f }, { 1.25f, 0.24f, 0.24f }, 0.0f)
	};

	if (type == ClassType::Twin) {
		config.id = "Twin";
		config.displayName = "Twin";
		config.requiredRank = 2;
		config.spreadAngleDeg = 2.0f;
		config.randomSpread = true;
		config.reloadScale = 1.0f / 2.5f;
		config.alternateBarrels = true;
		config.barrels = {
			makeBarrel({ 0.72f, -0.34f, 0.0f }, { 1.25f, 0.24f, 0.24f }, 0.0f),
			makeBarrel({ 0.72f,  0.34f, 0.0f }, { 1.25f, 0.24f, 0.24f }, 0.0f)
		};
		return config;
	}

	if (type == ClassType::MachineGun) {
		config.requiredRank = 2;
		config.spreadAngleDeg = 30.0f;
		config.reloadScale = 0.6f;
	}
	if (type == ClassType::Overseer) {
		config.requiredRank = 2;
		config.usesDrone = true;
		config.recoilPower = 0.0f;
	}
	if (type == ClassType::Triple) {
		config.requiredRank = 3;
		config.bulletCount = 1;
		config.spreadAngleDeg = 45.0f;
		config.randomSpread = false;
	}
	if (type == ClassType::Bounder) {
		config.requiredRank = 3;
		config.reflect = true;
	}
	if (type == ClassType::Assassin) {
		config.requiredRank = 3;
		config.bulletSpeedScale = 1.5f;
	}
	if (type == ClassType::Ninja) {
		config.requiredRank = 4;
		config.bulletCount = 1;
		config.spreadAngleDeg = 15.0f;
		config.randomSpread = false;
	}
	if (type == ClassType::Smasher || type == ClassType::Summoner) {
		config.requiredRank = 4;
	}

	return config;
}

void PlayerClassCatalog::Save(const std::string& path) const
{
	nlohmann::json classes = nlohmann::json::array();
	for (const std::string& id : classOrder_) {
		const PlayerClassConfig* config = Find(id);
		if (!config) {
			continue;
		}

		nlohmann::json item;
		item["id"] = config->id;
		item["displayName"] = config->displayName;
		item["requiredRank"] = config->requiredRank;
		item["usesDrone"] = config->usesDrone;
		item["maxDrones"] = config->maxDrones;
		item["reloadScale"] = config->reloadScale;
		item["bulletSpeedScale"] = config->bulletSpeedScale;
		item["bulletDamageScale"] = config->bulletDamageScale;
		item["bulletCount"] = config->bulletCount;
		item["spreadAngleDeg"] = config->spreadAngleDeg;
		item["randomSpread"] = config->randomSpread;
		item["reflect"] = config->reflect;
		item["penetrate"] = config->penetrate;
		item["fireAllBarrels"] = config->fireAllBarrels;
		item["alternateBarrels"] = config->alternateBarrels;
		item["recoilPower"] = config->recoilPower;
		item["specialActionId"] = config->specialActionId;
		item["specialActionCooldownScale"] = config->specialActionCooldownScale;
		item["specialActionStaminaCost"] = config->specialActionStaminaCost;
		item["saberCounterWindow"] = config->saberCounterWindow;
		item["saberCounterDamageScale"] = config->saberCounterDamageScale;
		item["saberCounterRangeScale"] = config->saberCounterRangeScale;
		item["bodyShape"] = BodyShapeToString(config->bodyShape);
		item["bodyScale"] = nlohmann::json::array({ config->bodyScale.x, config->bodyScale.y });
		item["bodyFillColor"] = Vector4ToJson(config->bodyFillColor);
		item["bodyOutlineColor"] = Vector4ToJson(config->bodyOutlineColor);
		item["weaponMounts"] = nlohmann::json::array();
		for (const WeaponMountConfig& barrel : config->barrels) {
			nlohmann::json barrelJson;
			barrelJson["model"] = barrel.model;
			barrelJson["barrelShape"] = BarrelShapeToString(barrel.barrelShape);
			barrelJson["offset"] = Vector3ToJson(barrel.offset);
			barrelJson["scale"] = Vector3ToJson(barrel.scale);
			barrelJson["angleDeg"] = barrel.angleDeg;
			barrelJson["muzzleForward"] = barrel.muzzleForward;
			barrelJson["fires"] = barrel.fires;
			barrelJson["weaponType"] = WeaponTypeToString(barrel.weaponType);
			barrelJson["damageScale"] = barrel.damageScale;
			barrelJson["projectileSpeedScale"] = barrel.projectileSpeedScale;
			barrelJson["fireGroup"] = barrel.fireGroup;
			barrelJson["reloadScale"] = barrel.reloadScale;
			barrelJson["recoilScale"] = barrel.recoilScale;
			barrelJson["barrelColor"] = Vector4ToJson(barrel.barrelColor);
			barrelJson["outlineColor"] = Vector4ToJson(barrel.outlineColor);
			barrelJson["effectColor"] = Vector4ToJson(barrel.effectColor);
			barrelJson["laserRange"] = barrel.laserRange;
			barrelJson["laserWidth"] = barrel.laserWidth;
			barrelJson["laserDuration"] = barrel.laserDuration;
			barrelJson["laserDamageInterval"] = barrel.laserDamageInterval;
			barrelJson["mineRadius"] = barrel.mineRadius;
			barrelJson["mineFuseTime"] = barrel.mineFuseTime;
			barrelJson["mineLifeTime"] = barrel.mineLifeTime;
			barrelJson["meleeRange"] = barrel.meleeRange;
			barrelJson["meleeArcDeg"] = barrel.meleeArcDeg;
			barrelJson["meleeWidth"] = barrel.meleeWidth;
			barrelJson["meleeDuration"] = barrel.meleeDuration;
			barrelJson["meleeComboResetTime"] = barrel.meleeComboResetTime;
			barrelJson["meleeCombo1DamageScale"] = barrel.meleeCombo1DamageScale;
			barrelJson["meleeCombo2DamageScale"] = barrel.meleeCombo2DamageScale;
			barrelJson["meleeCombo3DamageScale"] = barrel.meleeCombo3DamageScale;
			barrelJson["meleeCombo1RangeScale"] = barrel.meleeCombo1RangeScale;
			barrelJson["meleeCombo2RangeScale"] = barrel.meleeCombo2RangeScale;
			barrelJson["meleeCombo3RangeScale"] = barrel.meleeCombo3RangeScale;
			barrelJson["meleeCombo1Windup"] = barrel.meleeCombo1Windup;
			barrelJson["meleeCombo2Windup"] = barrel.meleeCombo2Windup;
			barrelJson["meleeCombo3Windup"] = barrel.meleeCombo3Windup;
			barrelJson["meleeCombo1Recovery"] = barrel.meleeCombo1Recovery;
			barrelJson["meleeCombo2Recovery"] = barrel.meleeCombo2Recovery;
			barrelJson["meleeCombo3Recovery"] = barrel.meleeCombo3Recovery;
			item["weaponMounts"].push_back(barrelJson);
		}
		classes.push_back(item);
	}

	nlohmann::json root;
	root["version"] = 2;
	root["classes"] = classes;
	std::ofstream file(path);
	if (file.is_open()) {
		file << root.dump(2);
	}
}

const PlayerClassConfig* PlayerClassCatalog::Find(ClassType type) const
{
	return Find(ClassTypeToString(type));
}

const PlayerClassConfig* PlayerClassCatalog::Find(const std::string& classId) const
{
	auto it = classConfigs_.find(classId);
	if (it == classConfigs_.end()) {
		return nullptr;
	}
	return &it->second;
}

PlayerClassConfig* PlayerClassCatalog::FindMutable(const std::string& classId)
{
	auto it = classConfigs_.find(classId);
	if (it == classConfigs_.end()) {
		return nullptr;
	}
	return &it->second;
}

void PlayerClassCatalog::ResetToDefaults()
{
	classConfigs_.clear();
	classOrder_.clear();
	for (ClassType type : EditableClassTypes()) {
		PlayerClassConfig config = CreateDefaultConfig(type);
		classOrder_.push_back(config.id);
		classConfigs_[config.id] = std::move(config);
	}
}

void PlayerClassCatalog::InsertOrAssign(const PlayerClassConfig& config)
{
	if (classConfigs_.find(config.id) == classConfigs_.end()) {
		classOrder_.push_back(config.id);
	}
	classConfigs_[config.id] = config;
}

bool PlayerClassCatalog::Erase(const std::string& id)
{
	const std::string erasedId = id;
	if (classConfigs_.erase(erasedId) == 0) return false;
	classOrder_.erase(std::remove(classOrder_.begin(), classOrder_.end(), erasedId), classOrder_.end());
	return true;
}

void PlayerClassCatalog::Swap(PlayerClassCatalog& other) noexcept
{
	classConfigs_.swap(other.classConfigs_);
	classOrder_.swap(other.classOrder_);
}
