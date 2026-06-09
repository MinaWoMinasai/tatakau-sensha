#include "EnemyManager.h"
#include "Stage.h"
#include "Player.h"
#include "Calculation.h"
#include <iostream>

namespace {

bool TryGetExpEnemyType(const std::string& prefab, ExpEnemyType& type)
{
    if (prefab == "Default" || prefab == "Basic" || prefab == "Square") {
        type = ExpEnemyType::Square;
        return true;
    }
    if (prefab == "Triangle") {
        type = ExpEnemyType::Triangle;
        return true;
    }
    if (prefab == "Pentagon") {
        type = ExpEnemyType::Pentagon;
        return true;
    }
    if (prefab == "Shooter") {
        type = ExpEnemyType::Shooter;
        return true;
    }
    return false;
}

} // namespace

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
    const float kMapMin = MapChip::kBlockWidth * 2.0f;
    const float kMapMaxX = MapChip::kBlockWidth * (MapChip::kNumBlockHorizontal - 3.0f);
    const float kMapMaxY = MapChip::kBlockHeight * (MapChip::kNumBlockVirtical - 3.0f);

    for (int i = 0; i < 10; ++i) { // 最大10回リトライ
        Vector3 spawnPos = { Rand(kMapMin, kMapMaxX), Rand(kMapMin, kMapMaxY), 0.0f };

        // 壁と重なっていないか確認
        if (!stage.IsCollisionWithAnyBlock(spawnPos, 1.0f)) {
            // プレイヤーのすぐ近くには出さない（安全のため）
            float dist = Length(spawnPos - player_->GetWorldPosition());
            if (dist < 10.0f) continue;

            ExpEnemyType type = ExpEnemyType::Square;
            const int roll = static_cast<int>(Rand(0.0f, 100.0f));
            if (roll >= 88) {
                type = ExpEnemyType::Pentagon;
            } else if (roll >= 68) {
                type = ExpEnemyType::Shooter;
            } else if (roll >= 38) {
                type = ExpEnemyType::Triangle;
            }

            // 生成
            auto newEnemy = std::make_unique<ExpEnemy>();
            newEnemy->Initialize(spawnPos, player_, type);
            newEnemy->SetAttackControllerBulletManager(bulletManager_);
            // AttackControllerが必要な場合はここでセットアップ
            enemies_.push_back(std::move(newEnemy));
            break;
        }
    }
}

void EnemyManager::Draw(bool drawBody) {
    if (!drawBody) {
        return;
    }
    DrawBodyOnly();
}

void EnemyManager::DrawBodyOnly() {
    for (auto& enemy : enemies_) {
        enemy->DrawBodyOnly();
    }
}

bool EnemyManager::SpawnLevelEnemy(const Vector3& position, const std::string& prefab, int hp)
{
    ExpEnemyType type = ExpEnemyType::Square;
    if (!TryGetExpEnemyType(prefab, type)) {
        std::cerr << "[LevelLoader] Unsupported Enemy prefab: " << prefab << std::endl;
        return false;
    }

    auto newEnemy = std::make_unique<ExpEnemy>();
    newEnemy->Initialize(position, player_, type);
    newEnemy->SetAttackControllerBulletManager(bulletManager_);
    if (hp > 0) {
        newEnemy->SetHp(hp);
    }
    enemies_.push_back(std::move(newEnemy));
    return true;
}

void EnemyManager::DrawBodyOnlyVisible(const Vector3& cameraPos, float halfWidth, float halfHeight) {
    for (auto& enemy : enemies_) {
        const Vector3 pos = enemy->GetWorldPosition();
        const float radius = enemy->GetRadius();
        if (pos.x + radius < cameraPos.x - halfWidth || pos.x - radius > cameraPos.x + halfWidth ||
            pos.y + radius < cameraPos.y - halfHeight || pos.y - radius > cameraPos.y + halfHeight) {
            continue;
        }
        enemy->DrawBodyOnly();
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
