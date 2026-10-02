#include "game/weapon/CombatTypes.h"
#include "AttackController.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
using namespace DirectX;

void AttackController::Fire(const cg2::Vector3& origin, const cg2::Vector3& baseDir, const AttackParam& param, BulletOwner owner)
{
    FireInternal(origin, baseDir, param, owner, false);
}

void AttackController::FireFromMuzzle(const cg2::Vector3& muzzlePosition, const cg2::Vector3& baseDir, const AttackParam& param, BulletOwner owner)
{
    FireInternal(muzzlePosition, baseDir, param, owner, true);
}

void AttackController::FireInternal(const cg2::Vector3& origin, const cg2::Vector3& baseDir, const AttackParam& param, BulletOwner owner, bool originIsMuzzle)
{

    assert(bulletManager_);
    const float directionLength = cg2::Length(baseDir);
    if (!std::isfinite(directionLength) || directionLength <= 0.0001f || !std::isfinite(param.bulletSpeed)) return;
    cg2::Vector3 dirNorm = baseDir / directionLength;

    float halfSpread = param.spreadAngleDeg * 0.5f;

    for (int i = 0; i < param.bulletCount; ++i) {

        float angleOffset;
        if (param.randomSpread) {
            angleOffset = cg2::Rand(-halfSpread, halfSpread);
        } else {
            angleOffset = (param.bulletCount <= 1)
                ? 0.0f
                : -halfSpread +
                (param.spreadAngleDeg / (param.bulletCount - 1)) * i;
        }

        float rad = XMConvertToRadians(angleOffset);

        cg2::Vector3 dirRotated;
        dirRotated.x = dirNorm.x * cosf(rad) - dirNorm.y * sinf(rad);
        dirRotated.y = dirNorm.x * sinf(rad) + dirNorm.y * cosf(rad);
        dirRotated.z = dirNorm.z;

        cg2::Vector3 velocity = param.bulletSpeed * dirRotated;

        auto bullet = std::make_unique<Bullet>();

        // 敵とプレイヤーで発射位置を少し変える
        cg2::Vector3 bulletOrigin;
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

		bullet->SetCanClaimRunResource(param.canClaimRunResource);
		bullet->ConfigureGrowth(param.maxWallBounces, param.actorPierceCount, param.impactSplitCount, param.impactSplitDamageScale);
		if(owner==kPlayer) {
			bullet->ConfigureShooterAbilities(param.shooterChain,param.shooterMark,param.shooterBoomerang,param.shooterKillBurst,
				param.shooterChainPower,param.shooterMarkPower,param.shooterBoomerangPower,param.shooterKillBurstPower);
			bullet->ConfigureDroneSource(param.sourceDroneIndex,param.sourcePlayer);
			bullet->ConfigureVisualScale(param.bulletVisualScale,param.bulletTrailScale);
		}
		bulletManager_->Add(std::move(bullet));
    }
}
