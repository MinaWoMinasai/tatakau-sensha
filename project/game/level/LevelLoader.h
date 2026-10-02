#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "Struct.h"

/// @brief レベルに配置するオブジェクトの種類・姿勢・追加設定を表す。
struct LevelObject {
    std::string name;
    std::string type;
    std::string prefab;
    cg2::Transform transform;
    nlohmann::json customProperties = nlohmann::json::object();
};

/// @brief レベルの敵出現領域と出現条件を表す。
struct LevelSpawnArea {
    std::string name;
    std::string prefab;
    cg2::Vector3 center;
    cg2::Vector3 size;
    float spawnInterval = 2.0f;
    int maxAlive = 8;
    int hp = -1;
    bool enabled = true;
    nlohmann::json customProperties = nlohmann::json::object();
};

/// @brief ボスの段階ごとの発動条件と性能設定を表す。
struct LevelBossPhase {
    std::string name;
    float startHpRate = 1.0f;
    std::string message;
    std::vector<LevelObject> objects;
    nlohmann::json customProperties = nlohmann::json::object();
};

/// @brief レベル全体の配置・出現・ボス設定を保持する。
struct LevelData {
    std::string toolName;
    std::string editorMode;
    std::string levelName;
    std::vector<LevelObject> objects;
    std::vector<LevelSpawnArea> spawnAreas;
    std::vector<LevelBossPhase> bossPhases;
    nlohmann::json balance = nlohmann::json::object();
};

/// @brief レベルのJSONを読み込み、配置や戦闘設定へ変換する。
class LevelLoader {
public:
    /// @brief レベルJSONの配置・出現領域・ボス段階を読み取る。
    /// @param filePath 読み込むJSONのパス。
    /// @param outLevel 読み込み結果。途中で失敗した場合は部分的に更新されうる。
    /// @return 対応形式を読み込めた場合true。失敗時の出力はゲームへ適用しない。
    bool Load(const std::string& filePath, LevelData& outLevel) const;
};
