#pragma once
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
        const Vector3& origin,
        const Vector3& baseDir,
        const AttackParam& param,
        BulletOwner owner
    );

    void FireFromMuzzle(
        const Vector3& muzzlePosition,
        const Vector3& baseDir,
        const AttackParam& param,
        BulletOwner owner
    );

private:
    void FireInternal(
        const Vector3& origin,
        const Vector3& baseDir,
        const AttackParam& param,
        BulletOwner owner,
        bool originIsMuzzle
    );

    BulletManager* bulletManager_ = nullptr;
};

