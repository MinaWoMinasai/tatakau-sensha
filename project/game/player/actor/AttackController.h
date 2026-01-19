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

private:
    BulletManager* bulletManager_ = nullptr;
};

