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
    void Draw();

    // 衝突判定のためにリストを公開
    std::vector<ExpEnemy*> GetEnemyPtrs() const;

private:
    void Spawn(Stage& stage);

    std::vector<std::unique_ptr<ExpEnemy>> enemies_;
    Player* player_ = nullptr;
    BulletManager* bulletManager_ = nullptr;

    float spawnTimer_ = 0.0f;
    const float kSpawnInterval = 1.5f; // 1.5秒に1回生成試行
    const int kMaxEnemies = 30;        // 最大数
};