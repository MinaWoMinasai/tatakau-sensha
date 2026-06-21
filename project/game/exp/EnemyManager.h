#pragma once
#include <list>
#include <memory>
#include <string>
#include <vector>
#include "ExpEnemy.h"

class Player;
class Stage;
class BulletManager;
class Enemy;

class EnemyManager {
public:
    struct SpawnArea {
        std::string name;
        std::string prefab;
        Vector3 center;
        Vector3 size;
        float spawnInterval = 2.0f;
        float timer = 0.0f;
        int maxAlive = 8;
        int hp = -1;
        bool enabled = true;
    };

    void Initialize(Player* player, BulletManager* bulletManager, Enemy* boss);
    void Update(Stage& stage, float deltaTime);
    void Draw(bool drawBody = true);
    void DrawBodyOnly();
    void DrawBodyOnlyVisible(const Vector3& cameraPos, float halfWidth, float halfHeight);
    bool SpawnLevelEnemy(const Vector3& position, const std::string& prefab, int hp = -1);
    void AddLevelSpawnArea(const SpawnArea& spawnArea);
    void ClearLevelData();
    void SetDefaultRandomSpawnEnabled(bool enabled) { defaultRandomSpawnEnabled_ = enabled; }
    void SetExpEnemyHostileToBoss(bool hostile);
    ExpEnemy* FindNearestEnemy(const Vector3& position, float maxDistance) const;

    // 衝突判定のためにリストを公開
    std::vector<ExpEnemy*> GetEnemyPtrs() const;
    size_t GetEnemyCount() const { return enemies_.size(); }

private:
    void Spawn(Stage& stage);
    void UpdateLevelSpawnAreas(Stage& stage, float deltaTime);
    int CountEnemiesInArea(const SpawnArea& spawnArea) const;

    std::vector<std::unique_ptr<ExpEnemy>> enemies_;
    std::vector<SpawnArea> spawnAreas_;
    Player* player_ = nullptr;
    BulletManager* bulletManager_ = nullptr;
	Enemy* boss_ = nullptr;

    float spawnTimer_ = 0.0f;
    bool defaultRandomSpawnEnabled_ = true;
    const float kSpawnInterval = 1.2f; // 短い間隔で図形を散らす
    const int kMaxEnemies = 45;        // 最大数
};
