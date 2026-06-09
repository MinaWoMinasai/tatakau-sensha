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

struct LevelData {
	std::string levelName;
	std::vector<LevelObject> objects;
};

class LevelLoader {
public:
	bool Load(const std::string& filePath, LevelData& outLevel) const;
};
