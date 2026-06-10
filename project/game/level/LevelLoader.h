#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "Struct.h"

struct LevelObject {
	std::string name;
	std::string type;
	std::string prefab;
	Transform transform;
	nlohmann::json customProperties = nlohmann::json::object();
};

struct LevelSpawnArea {
	std::string name;
	std::string prefab;
	Vector3 center;
	Vector3 size;
	float spawnInterval = 2.0f;
	int maxAlive = 8;
	int hp = -1;
	bool enabled = true;
	nlohmann::json customProperties = nlohmann::json::object();
};

struct LevelBossPhase {
	std::string name;
	float startHpRate = 1.0f;
	std::string message;
	std::vector<LevelObject> objects;
	nlohmann::json customProperties = nlohmann::json::object();
};

struct LevelData {
	std::string toolName;
	std::string editorMode;
	std::string levelName;
	std::vector<LevelObject> objects;
	std::vector<LevelSpawnArea> spawnAreas;
	std::vector<LevelBossPhase> bossPhases;
	nlohmann::json balance = nlohmann::json::object();
};

class LevelLoader {
public:
	bool Load(const std::string& filePath, LevelData& outLevel) const;
};
