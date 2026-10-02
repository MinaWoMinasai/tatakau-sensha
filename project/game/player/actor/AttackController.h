#pragma once
#include "game/weapon/CombatTypes.h"
#include "Calculation.h"
#include "Bullet.h"
#include "BulletManager.h"

/// @brief 機体の発射設定から弾を作り、BulletManagerへ所有権を渡す。
class AttackController {
public:
    /// @brief 弾管理を設定する。
    void SetBulletManager(BulletManager* manager)
    {
        bulletManager_ = manager;
    }

    /// @brief 指定した攻撃または演出の発射を開始する。
    void Fire(const cg2::Vector3& origin, const cg2::Vector3& baseDir, const AttackParam& param, BulletOwner owner);

    /// @brief からの銃口を発射する。
    void FireFromMuzzle(const cg2::Vector3& muzzlePosition, const cg2::Vector3& baseDir, const AttackParam& param, BulletOwner owner);

private:
    /// @brief Internalを発射する。
    void FireInternal(const cg2::Vector3& origin, const cg2::Vector3& baseDir, const AttackParam& param, BulletOwner owner,
                      bool originIsMuzzle);

    BulletManager* bulletManager_ = nullptr;
};
