#include "EnemyManager.h"
#include "Stage.h"
#include "Player.h"
#include "Calculation.h"

void EnemyManager::Initialize(Player* player, BulletManager* bulletManager) {
    player_ = player;
    bulletManager_ = bulletManager;
}

void EnemyManager::Update(Stage& stage, float deltaTime) {
    // 1. 一定間隔でスポーン
    spawnTimer_ -= deltaTime;
    if (spawnTimer_ <= 0.0f && enemies_.size() < kMaxEnemies) {
        Spawn(stage);
        spawnTimer_ = kSpawnInterval;
    }

    // 2. 全ての敵を更新 & デスフラグが立ったら削除
    for (auto it = enemies_.begin(); it != enemies_.end(); ) {
        (*it)->Update(stage, deltaTime);

        if ((*it)->IsDead()) {
            it = enemies_.erase(it); // 削除
        } else {
            ++it;
        }
    }
}

void EnemyManager::Spawn(Stage& stage) {
    // ステージの範囲（仮に0~30とする。お手元のmapサイズに合わせて調整してください）
    const float kMapMin = 2.0f;
    const float kMapMax = 28.0f;

    for (int i = 0; i < 10; ++i) { // 最大10回リトライ
        Vector3 spawnPos = { Rand(kMapMin, kMapMax), Rand(kMapMin, kMapMax), 0.0f };

        // 壁と重なっていないか確認
        if (!stage.IsCollisionWithAnyBlock(spawnPos, 1.0f)) {
            // プレイヤーのすぐ近くには出さない（安全のため）
            float dist = Length(spawnPos - player_->GetWorldPosition());
            if (dist < 10.0f) continue;

            // 生成
            auto newEnemy = std::make_unique<ExpEnemy>();
            newEnemy->Initialize(spawnPos, player_);
            newEnemy->SetAttackControllerBulletManager(bulletManager_);
            // AttackControllerが必要な場合はここでセットアップ
            enemies_.push_back(std::move(newEnemy));
            break;
        }
    }
}

void EnemyManager::Draw() {
    for (auto& enemy : enemies_) {
        enemy->Draw();
    }
}

std::vector<ExpEnemy*> EnemyManager::GetEnemyPtrs() const {
    std::vector<ExpEnemy*> result;
    result.reserve(enemies_.size());
    for (const auto& e : enemies_) {
        result.push_back(e.get());
    }
    return result;
}