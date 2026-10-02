#pragma once
#include "Collider.h"
#include <list>

class Enemy;
class Player;
class PlayerDrone;
class BulletManager;
class EnemyManager;

/// @brief 登録済みのコライダーを組み合わせ、衝突判定と通知を実行する。
class CollisionManager {

public:
    /// @brief 衝突判定と応答
    void CheckAllCollisions(Player* player, Enemy* enemy, BulletManager* bulletManager, EnemyManager* enemyManager = nullptr);

    /// @brief コライダー二つの衝突判定と応答
    /// @param colliderA コライダーA
    /// @param colliderB コライダーB
    void CheckCollisionPair(Collider* colliderA, Collider* colliderB);

    /// @brief コライダーを設定する
    void SetColliders(Player* player, Enemy* enemy, BulletManager* bulletManager, EnemyManager* enemyManager = nullptr);

    /// @brief Collidersを返す。
    const std::list<Collider*>& GetColliders() const
    {
        return colliders_;
    }

private:
    BulletManager* activeBulletManager_ = nullptr;
    // コライダーリスト
    std::list<Collider*> colliders_;
};
