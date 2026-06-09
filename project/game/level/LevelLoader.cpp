#include "LevelLoader.h"
#include "Calculation.h"
#include <fstream>
#include <iostream>

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

std::string ReadString(const nlohmann::json& json, const char* key, const std::string& fallback)
{
	if (!json.contains(key) || !json[key].is_string()) {
		return fallback;
	}
	return json[key].get<std::string>();
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

	outLevel = {};
	outLevel.levelName = ReadString(json, "levelName", "");

	if (!json.contains("objects")) {
		LogLevelWarning("Level has no objects array: " + filePath);
		return true;
	}
	if (!json["objects"].is_array()) {
		LogLevelWarning("objects must be an array: " + filePath);
		return false;
	}

	for (const auto& objectJson : json["objects"]) {
		if (!objectJson.is_object()) {
			LogLevelWarning("Skipped non-object entry in objects.");
			continue;
		}

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
		} else {
			object.customProperties = nlohmann::json::object();
		}

		if (object.type.empty()) {
			LogLevelWarning("Skipped object with empty type: " + object.name);
			continue;
		}

		outLevel.objects.push_back(std::move(object));
	}

	return true;
}
