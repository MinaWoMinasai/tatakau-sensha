#include "LevelLoader.h"
#include "Calculation.h"
#include <fstream>
#include <iostream>
#include <numbers>

namespace {

void LogLevelWarning(const std::string& message)
{
	std::cerr << "[LevelLoader] " << message << std::endl;
}

Vector3 ReadVector3Object(const nlohmann::json& json, const Vector3& fallback)
{
	if (!json.is_object()) {
		return fallback;
	}

	Vector3 value = fallback;
	if (json.contains("x") && json["x"].is_number()) {
		value.x = json["x"].get<float>();
	}
	if (json.contains("y") && json["y"].is_number()) {
		value.y = json["y"].get<float>();
	}
	if (json.contains("z") && json["z"].is_number()) {
		value.z = json["z"].get<float>();
	}
	return value;
}

Vector3 ReadVector3Array(const nlohmann::json& json, const Vector3& fallback)
{
	if (!json.is_array() || json.size() < 3 ||
		!json[0].is_number() || !json[1].is_number() || !json[2].is_number()) {
		return fallback;
	}

	return {
		json[0].get<float>(),
		json[1].get<float>(),
		json[2].get<float>()
	};
}

std::string ReadString(const nlohmann::json& json, const char* key, const std::string& fallback)
{
	if (!json.contains(key) || !json[key].is_string()) {
		return fallback;
	}
	return json[key].get<std::string>();
}

float ReadFloat(const nlohmann::json& json, const char* key, float fallback)
{
	if (!json.contains(key) || !json[key].is_number()) {
		return fallback;
	}
	return json[key].get<float>();
}

int ReadInt(const nlohmann::json& json, const char* key, int fallback)
{
	if (!json.contains(key) || !json[key].is_number()) {
		return fallback;
	}
	return json[key].get<int>();
}

bool ReadBool(const nlohmann::json& json, const char* key, bool fallback)
{
	if (!json.contains(key) || !json[key].is_boolean()) {
		return fallback;
	}
	return json[key].get<bool>();
}

LevelObject ReadLevelObject(const nlohmann::json& objectJson)
{
	LevelObject object{};
	object.name = ReadString(objectJson, "name", "");
	object.type = ReadString(objectJson, "type", "");
	object.prefab = ReadString(objectJson, "prefab", "Default");
	object.transform = InitWorldTransform();
	object.transform.translate = ReadVector3Object(objectJson.value("position", nlohmann::json::object()), object.transform.translate);
	object.transform.rotate = ReadVector3Object(objectJson.value("rotation", nlohmann::json::object()), object.transform.rotate);
	object.transform.scale = ReadVector3Object(objectJson.value("scale", nlohmann::json::object()), object.transform.scale);

	if (objectJson.contains("customProperties") && objectJson["customProperties"].is_object()) {
		object.customProperties = objectJson["customProperties"];
	}
	return object;
}

LevelObject ReadBlenderLevelObject(const nlohmann::json& objectJson)
{
	LevelObject object{};
	object.name = ReadString(objectJson, "name", "");
	object.type = ReadString(objectJson, "type", "");
	object.prefab = ReadString(objectJson, "file_name", "");
	object.transform = InitWorldTransform();

	const nlohmann::json transformJson =
		objectJson.value("transform", nlohmann::json::object());
	const Vector3 blenderTranslation = ReadVector3Array(
		transformJson.value("translation", nlohmann::json::array()),
		{ 0.0f, 0.0f, 0.0f });
	const Vector3 blenderRotationDegrees = ReadVector3Array(
		transformJson.value("rotation", nlohmann::json::array()),
		{ 0.0f, 0.0f, 0.0f });
	const Vector3 blenderScaling = ReadVector3Array(
		transformJson.value("scaling", nlohmann::json::array()),
		{ 1.0f, 1.0f, 1.0f });

	object.transform.translate = {
		blenderTranslation.x,
		blenderTranslation.z,
		blenderTranslation.y
	};
	object.transform.scale = {
		blenderScaling.x,
		blenderScaling.z,
		blenderScaling.y
	};
	constexpr float kDegreesToRadians = std::numbers::pi_v<float> / 180.0f;
	object.transform.rotate = {
		-blenderRotationDegrees.x * kDegreesToRadians,
		-blenderRotationDegrees.z * kDegreesToRadians,
		-blenderRotationDegrees.y * kDegreesToRadians
	};
	return object;
}

LevelSpawnArea ReadSpawnArea(const nlohmann::json& areaJson)
{
	LevelSpawnArea area{};
	area.name = ReadString(areaJson, "name", "");
	area.prefab = ReadString(areaJson, "prefab", "Basic");
	area.center = ReadVector3Object(areaJson.value("center", areaJson.value("position", nlohmann::json::object())), { 0.0f, 0.0f, 0.0f });
	area.size = ReadVector3Object(areaJson.value("size", nlohmann::json::object()), { 10.0f, 10.0f, 0.0f });
	area.spawnInterval = ReadFloat(areaJson, "spawnInterval", 2.0f);
	area.maxAlive = ReadInt(areaJson, "maxAlive", 8);
	area.hp = ReadInt(areaJson, "hp", -1);
	area.enabled = ReadBool(areaJson, "enabled", true);
	if (areaJson.contains("customProperties") && areaJson["customProperties"].is_object()) {
		area.customProperties = areaJson["customProperties"];
	}
	return area;
}

} // namespace

bool LevelLoader::Load(const std::string& filePath, LevelData& outLevel) const
{
	std::ifstream file(filePath);
	if (!file.is_open()) {
		LogLevelWarning("Failed to open level file: " + filePath);
		return false;
	}

	nlohmann::json json;
	try {
		file >> json;
	} catch (const std::exception& e) {
		LogLevelWarning("Failed to parse level file: " + filePath + " (" + e.what() + ")");
		return false;
	}

	if (!json.is_object()) {
		LogLevelWarning("Level root must be an object: " + filePath);
		return false;
	}

	const bool isBlenderLevel = ReadString(json, "name", "") == "scene";
	if (isBlenderLevel) {
		outLevel = {};
		outLevel.toolName = "Blender Level Editor";
		outLevel.editorMode = "Blender";
		outLevel.levelName = ReadString(json, "name", "scene");

		if (json.contains("objects") && !json["objects"].is_array()) {
			LogLevelWarning("Blender objects must be an array: " + filePath);
			return false;
		}
		for (const auto& objectJson : json.value("objects", nlohmann::json::array())) {
			if (!objectJson.is_object()) {
				LogLevelWarning("Skipped non-object entry in Blender objects.");
				continue;
			}
			if (ReadString(objectJson, "type", "") != "MESH") {
				continue;
			}

			LevelObject object = ReadBlenderLevelObject(objectJson);
			if (object.prefab.empty()) {
				LogLevelWarning("Skipped Blender MESH without file_name: " + object.name);
				continue;
			}
			outLevel.objects.push_back(std::move(object));
		}
		return true;
	}

	outLevel = {};
	outLevel.toolName = ReadString(json, "toolName", "Level AI-ditor");
	outLevel.editorMode = ReadString(json, "editorMode", "ArenaBattle");
	outLevel.levelName = ReadString(json, "levelName", "");
	if (json.contains("balance") && json["balance"].is_object()) {
		outLevel.balance = json["balance"];
	}

	if (json.contains("objects") && !json["objects"].is_array()) {
		LogLevelWarning("objects must be an array: " + filePath);
		return false;
	}
	for (const auto& objectJson : json.value("objects", nlohmann::json::array())) {
		if (!objectJson.is_object()) {
			LogLevelWarning("Skipped non-object entry in objects.");
			continue;
		}

		LevelObject object = ReadLevelObject(objectJson);
		if (object.type.empty()) {
			LogLevelWarning("Skipped object with empty type: " + object.name);
			continue;
		}
		outLevel.objects.push_back(std::move(object));
	}

	if (json.contains("spawnAreas") && !json["spawnAreas"].is_array()) {
		LogLevelWarning("spawnAreas must be an array: " + filePath);
		return false;
	}
	for (const auto& areaJson : json.value("spawnAreas", nlohmann::json::array())) {
		if (!areaJson.is_object()) {
			LogLevelWarning("Skipped non-object entry in spawnAreas.");
			continue;
		}
		outLevel.spawnAreas.push_back(ReadSpawnArea(areaJson));
	}

	if (json.contains("bossPhases") && !json["bossPhases"].is_array()) {
		LogLevelWarning("bossPhases must be an array: " + filePath);
		return false;
	}
	for (const auto& phaseJson : json.value("bossPhases", nlohmann::json::array())) {
		if (!phaseJson.is_object()) {
			LogLevelWarning("Skipped non-object entry in bossPhases.");
			continue;
		}

		LevelBossPhase phase{};
		phase.name = ReadString(phaseJson, "name", "");
		phase.startHpRate = ReadFloat(phaseJson, "startHpRate", 1.0f);
		phase.message = ReadString(phaseJson, "message", "");
		if (phaseJson.contains("customProperties") && phaseJson["customProperties"].is_object()) {
			phase.customProperties = phaseJson["customProperties"];
		}
		if (phaseJson.contains("objects") && !phaseJson["objects"].is_array()) {
			LogLevelWarning("bossPhase objects must be an array: " + phase.name);
			continue;
		}
		for (const auto& objectJson : phaseJson.value("objects", nlohmann::json::array())) {
			if (!objectJson.is_object()) {
				LogLevelWarning("Skipped non-object entry in bossPhase objects: " + phase.name);
				continue;
			}
			LevelObject object = ReadLevelObject(objectJson);
			if (!object.type.empty()) {
				phase.objects.push_back(std::move(object));
			}
		}
		outLevel.bossPhases.push_back(std::move(phase));
	}

	return true;
}
