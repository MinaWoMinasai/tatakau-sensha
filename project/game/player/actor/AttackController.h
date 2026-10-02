#pragma once
#include "game/weapon/CombatTypes.h"
#include "Calculation.h"
#include "Bullet.h"
#include "BulletManager.h"

class AttackController
{
public:

    void SetBulletManager(BulletManager* manager) {
        bulletManager_ = manager;
    }

    void Fire(
        const cg2::Vector3& origin,
        const cg2::Vector3& baseDir,
        const AttackParam& param,
        BulletOwner owner
    );

    void FireFromMuzzle(
        const cg2::Vector3& muzzlePosition,
        const cg2::Vector3& baseDir,
        const AttackParam& param,
        BulletOwner owner
    );

private:
    void FireInternal(
        const cg2::Vector3& origin,
        const cg2::Vector3& baseDir,
        const AttackParam& param,
        BulletOwner owner,
        bool originIsMuzzle
    );

    BulletManager* bulletManager_ = nullptr;
};

