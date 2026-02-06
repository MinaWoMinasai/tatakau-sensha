#include "AttackController.h"
#include <DirectXMath.h>
using namespace DirectX;

void AttackController::Fire(const Vector3& origin, const Vector3& baseDir, const AttackParam& param, BulletOwner owner)
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
        if (owner == BulletOwner::kPlayer) {
            bulletOrigin = origin + (baseDir * 1.5f);
        } else if (owner == BulletOwner::kEnemy) {
            bulletOrigin = origin + (baseDir * 3.0f);
        }
        
        bullet->Initialize(
            bulletOrigin,
            velocity,
            param.damage,
            owner,
            param.reflect
        );

        bulletManager_->Add(std::move(bullet));
    }
}
