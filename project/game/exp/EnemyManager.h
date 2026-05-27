#pragma once
#include <list>
#include <memory>
#include "ExpEnemy.h"

class Player;
class Stage;
class BulletManager;

class EnemyManager {
public:
    void Initialize(Player* player, BulletManager* bulletManager);
    void Update(Stage& stage, float deltaTime);
    void Draw(bool drawBody = true);
    void DrawBodyOnly();
    void DrawBodyOnlyVisible(const Vector3& cameraPos, float halfWidth, float halfHeight);

    // 衝突判定のためにリストを公開
    std::vector<ExpEnemy*> GetEnemyPtrs() const;
    size_t GetEnemyCount() const { return enemies_.size(); }

private:
    void Spawn(Stage& stage);

    std::vector<std::unique_ptr<ExpEnemy>> enemies_;
    Player* player_ = nullptr;
    BulletManager* bulletManager_ = nullptr;

    float spawnTimer_ = 0.0f;
    const float kSpawnInterval = 1.2f; // 短い間隔で図形を散らす
    const int kMaxEnemies = 45;        // 最大数
};
