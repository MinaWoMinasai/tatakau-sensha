#include "LevelLoader.h"
#include "Calculation.h"
#include <fstream>
#include <iostream>
#include <numbers>
#include <type_traits>

namespace {

/// @brief レベル予告を診断ログへ出力する。
void LogLevelWarning(const std::string& message)
{
    std::cerr << "[LevelLoader] " << message << std::endl;
}

/// @brief ベクトル3オブジェクトを読み取る。
myEngine::Vector3 ReadVector3Object(const nlohmann::json& json, const myEngine::Vector3& fallback)
{
    if (!json.is_object()) {
        return fallback;
    }

    myEngine::Vector3 value = fallback;
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

/// @brief ベクトル3配列を読み取る。
myEngine::Vector3 ReadVector3Array(const nlohmann::json& json, const myEngine::Vector3& fallback)
{
    if (!json.is_array() || json.size() < 3 || !json[0].is_number() || !json[1].is_number() || !json[2].is_number()) {
        return fallback;
    }

    return {json[0].get<float>(), json[1].get<float>(), json[2].get<float>()};
}

/// @brief キーの値を型 T として読み取る。キーが無いか、型が合わないときは fallback を返す。
template <class T> T ReadValue(const nlohmann::json& json, const char* key, const T& fallback)
{
    if (!json.contains(key)) {
        return fallback;
    }
    const nlohmann::json& value = json[key];
    bool typeMatches = false;
    if constexpr (std::is_same_v<T, bool>) {
        typeMatches = value.is_boolean();
    } else if constexpr (std::is_arithmetic_v<T>) {
        typeMatches = value.is_number();
    } else {
        typeMatches = value.is_string();
    }
    return typeMatches ? value.get<T>() : fallback;
}

/// @brief 文字列を読み取る。
std::string ReadString(const nlohmann::json& json, const char* key, const std::string& fallback)
{
    return ReadValue<std::string>(json, key, fallback);
}

/// @brief キーの値がオブジェクトなら outValue へ写す。
void ReadObjectInto(const nlohmann::json& json, const char* key, nlohmann::json& outValue)
{
    if (json.contains(key) && json[key].is_object()) {
        outValue = json[key];
    }
}

/// @brief json[key] の配列の要素を順に処理する。オブジェクトでない要素は警告して飛ばす。
/// @param label 警告に出す配列の名前。
/// @return key の値が配列でなければ false（キーが無いときは空の配列として true）。
template <class Function> bool ForEachObjectIn(const nlohmann::json& json, const char* key, const std::string& label, Function&& function)
{
    if (json.contains(key) && !json[key].is_array()) {
        return false;
    }
    for (const auto& entry : json.value(key, nlohmann::json::array())) {
        if (!entry.is_object()) {
            LogLevelWarning("Skipped non-object entry in " + label + ".");
            continue;
        }
        function(entry);
    }
    return true;
}

/// @brief レベルオブジェクトを読み取る。
LevelObject ReadLevelObject(const nlohmann::json& objectJson)
{
    LevelObject object{};
    object.name = ReadString(objectJson, "name", "");
    object.type = ReadString(objectJson, "type", "");
    object.prefab = ReadString(objectJson, "prefab", "Default");
    object.transform = myEngine::InitWorldTransform();
    object.transform.translate = ReadVector3Object(objectJson.value("position", nlohmann::json::object()), object.transform.translate);
    object.transform.rotate = ReadVector3Object(objectJson.value("rotation", nlohmann::json::object()), object.transform.rotate);
    object.transform.scale = ReadVector3Object(objectJson.value("scale", nlohmann::json::object()), object.transform.scale);
    ReadObjectInto(objectJson, "customProperties", object.customProperties);
    return object;
}

/// @brief Blenderレベルオブジェクトを読み取る。
LevelObject ReadBlenderLevelObject(const nlohmann::json& objectJson)
{
    LevelObject object{};
    object.name = ReadString(objectJson, "name", "");
    object.type = ReadString(objectJson, "type", "");
    object.prefab = ReadString(objectJson, "file_name", "");
    object.transform = myEngine::InitWorldTransform();

    const nlohmann::json transformJson = objectJson.value("transform", nlohmann::json::object());
    const myEngine::Vector3 blenderTranslation =
        ReadVector3Array(transformJson.value("translation", nlohmann::json::array()), {0.0f, 0.0f, 0.0f});
    const myEngine::Vector3 blenderRotationDegrees =
        ReadVector3Array(transformJson.value("rotation", nlohmann::json::array()), {0.0f, 0.0f, 0.0f});
    const myEngine::Vector3 blenderScaling = ReadVector3Array(transformJson.value("scaling", nlohmann::json::array()), {1.0f, 1.0f, 1.0f});

    object.transform.translate = {blenderTranslation.x, blenderTranslation.z, blenderTranslation.y};
    object.transform.scale = {blenderScaling.x, blenderScaling.z, blenderScaling.y};
    constexpr float kDegreesToRadians = std::numbers::pi_v<float> / 180.0f;
    object.transform.rotate = {-blenderRotationDegrees.x * kDegreesToRadians, -blenderRotationDegrees.z * kDegreesToRadians,
                               -blenderRotationDegrees.y * kDegreesToRadians};
    return object;
}

/// @brief 出現範囲を読み取る。
LevelSpawnArea ReadSpawnArea(const nlohmann::json& areaJson)
{
    LevelSpawnArea area{};
    area.name = ReadString(areaJson, "name", "");
    area.prefab = ReadString(areaJson, "prefab", "Basic");
    area.center = ReadVector3Object(areaJson.value("center", areaJson.value("position", nlohmann::json::object())), {0.0f, 0.0f, 0.0f});
    area.size = ReadVector3Object(areaJson.value("size", nlohmann::json::object()), {10.0f, 10.0f, 0.0f});
    area.spawnInterval = ReadValue(areaJson, "spawnInterval", 2.0f);
    area.maxAlive = ReadValue(areaJson, "maxAlive", 8);
    area.hp = ReadValue(areaJson, "hp", -1);
    area.enabled = ReadValue(areaJson, "enabled", true);
    ReadObjectInto(areaJson, "customProperties", area.customProperties);
    return area;
}

/// @brief ボスの段階を読み取る。
/// @param outPhase objects の値が配列でないときは false を返す（読み飛ばす段階として扱う）。
bool ReadBossPhase(const nlohmann::json& phaseJson, LevelBossPhase& outPhase)
{
    outPhase.name = ReadString(phaseJson, "name", "");
    outPhase.startHpRate = ReadValue(phaseJson, "startHpRate", 1.0f);
    outPhase.message = ReadString(phaseJson, "message", "");
    ReadObjectInto(phaseJson, "customProperties", outPhase.customProperties);
    const bool objectsValid =
        ForEachObjectIn(phaseJson, "objects", "bossPhase objects: " + outPhase.name, [&](const nlohmann::json& objectJson) {
            LevelObject object = ReadLevelObject(objectJson);
            if (!object.type.empty()) {
                outPhase.objects.push_back(std::move(object));
            }
        });
    if (!objectsValid) {
        LogLevelWarning("bossPhase objects must be an array: " + outPhase.name);
    }
    return objectsValid;
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
    }
    catch (const std::exception& e) {
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

        const bool objectsValid = ForEachObjectIn(json, "objects", "Blender objects", [&](const nlohmann::json& objectJson) {
            if (ReadString(objectJson, "type", "") != "MESH") {
                return;
            }
            LevelObject object = ReadBlenderLevelObject(objectJson);
            if (object.prefab.empty()) {
                LogLevelWarning("Skipped Blender MESH without file_name: " + object.name);
                return;
            }
            outLevel.objects.push_back(std::move(object));
        });
        if (!objectsValid) {
            LogLevelWarning("Blender objects must be an array: " + filePath);
        }
        return objectsValid;
    }

    outLevel = {};
    outLevel.toolName = ReadString(json, "toolName", "Level AI-ditor");
    outLevel.editorMode = ReadString(json, "editorMode", "ArenaBattle");
    outLevel.levelName = ReadString(json, "levelName", "");
    ReadObjectInto(json, "balance", outLevel.balance);

    const bool objectsValid = ForEachObjectIn(json, "objects", "objects", [&](const nlohmann::json& objectJson) {
        LevelObject object = ReadLevelObject(objectJson);
        if (object.type.empty()) {
            LogLevelWarning("Skipped object with empty type: " + object.name);
            return;
        }
        outLevel.objects.push_back(std::move(object));
    });
    if (!objectsValid) {
        LogLevelWarning("objects must be an array: " + filePath);
        return false;
    }

    const bool spawnAreasValid = ForEachObjectIn(json, "spawnAreas", "spawnAreas", [&](const nlohmann::json& areaJson) {
        outLevel.spawnAreas.push_back(ReadSpawnArea(areaJson));
    });
    if (!spawnAreasValid) {
        LogLevelWarning("spawnAreas must be an array: " + filePath);
        return false;
    }

    const bool bossPhasesValid = ForEachObjectIn(json, "bossPhases", "bossPhases", [&](const nlohmann::json& phaseJson) {
        LevelBossPhase phase{};
        if (ReadBossPhase(phaseJson, phase)) {
            outLevel.bossPhases.push_back(std::move(phase));
        }
    });
    if (!bossPhasesValid) {
        LogLevelWarning("bossPhases must be an array: " + filePath);
        return false;
    }

    return true;
}
