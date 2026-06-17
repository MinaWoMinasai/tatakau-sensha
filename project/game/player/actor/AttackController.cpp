#include "AttackController.h"
#include <DirectXMath.h>
#include <algorithm>
using namespace DirectX;

void AttackController::Fire(const Vector3& origin, const Vector3& baseDir, const AttackParam& param, BulletOwner owner)
{
    FireInternal(origin, baseDir, param, owner, false);
}

void AttackController::FireFromMuzzle(const Vector3& muzzlePosition, const Vector3& baseDir, const AttackParam& param, BulletOwner owner)
{
    FireInternal(muzzlePosition, baseDir, param, owner, true);
}

void AttackController::FireInternal(const Vector3& origin, const Vector3& baseDir, const AttackParam& param, BulletOwner owner, bool originIsMuzzle)
{

    assert(bulletManager_);
    Vector3 dirNorm = Normalize(baseDir);

    float halfSpread = param.spreadAngleDeg * 0.5f;

    for (int i = 0; i < param.bulletCount; ++i) {

        float angleOffset;
        if (param.randomSpread) {
            angleOffset = Rand(-halfSpread, halfSpread);
        } else {
            angleOffset = (param.bulletCount <= 1)
                ? 0.0f
                : -halfSpread +
                (param.spreadAngleDeg / (param.bulletCount - 1)) * i;
        }

        float rad = XMConvertToRadians(angleOffset);

        Vector3 dirRotated;
        dirRotated.x = dirNorm.x * cosf(rad) - dirNorm.y * sinf(rad);
        dirRotated.y = dirNorm.x * sinf(rad) + dirNorm.y * cosf(rad);
        dirRotated.z = dirNorm.z;

        Vector3 velocity = param.bulletSpeed * dirRotated;

        auto bullet = std::make_unique<Bullet>();
        
        // 敵とプレイヤーで発射位置を少し変える
        Vector3 bulletOrigin;
        if (originIsMuzzle) {
            bulletOrigin = origin;
        } else if (owner == BulletOwner::kPlayer) {
            bulletOrigin = origin + (baseDir * 1.5f);
        } else if (owner == BulletOwner::kEnemy) {
            bulletOrigin = origin + (baseDir * 3.0f);
        } else {
            bulletOrigin = origin;
        }
        
        bullet->Initialize(
            bulletOrigin,
            velocity,
            param.damage,
            owner,
            param.reflect,
            param.bulletHp > 0.0f ? param.bulletHp : static_cast<float>((std::max)(1u, param.damage)),
            param.bulletPenetration > 0.0f ? param.bulletPenetration : static_cast<float>((std::max)(1u, param.damage))
        );

        bulletManager_->Add(std::move(bullet));
    }
}
