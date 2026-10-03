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

void AttackController::FireFromMuzzle(const cg2::Vector3& muzzlePosition, const cg2::Vector3& baseDir, const AttackParam& param,
                                      BulletOwner owner)
{
    FireInternal(muzzlePosition, baseDir, param, owner, true);
}

void AttackController::FireInternal(const cg2::Vector3& origin, const cg2::Vector3& baseDir, const AttackParam& param, BulletOwner owner,
                                    bool originIsMuzzle)
{

    assert(bulletManager_);
    // 正規化できない方向や非有限の弾速では弾を作らない。管理先の設定は呼び出し側の前提。
    const float directionLength = cg2::Length(baseDir);
    if (!std::isfinite(directionLength) || directionLength <= 0.0001f || !std::isfinite(param.bulletSpeed))
        return;
    cg2::Vector3 dirNorm = baseDir / directionLength;

    float halfSpread = param.spreadAngleDeg * 0.5f;

    for (int i = 0; i < param.bulletCount; ++i) {

        // 拡散角は度。均等配置では両端を含め、1発だけなら中心方向、ランダムなら範囲内から選ぶ。
        float angleOffset;
        if (param.randomSpread) {
            angleOffset = cg2::Rand(-halfSpread, halfSpread);
        } else {
            angleOffset = (param.bulletCount <= 1) ? 0.0f : -halfSpread + (param.spreadAngleDeg / (param.bulletCount - 1)) * i;
        }

        // 三角関数へ渡すときだけラジアンへ変換し、XY方向を回転する。Z成分は保つ。
        float rad = XMConvertToRadians(angleOffset);

        cg2::Vector3 dirRotated;
        dirRotated.x = dirNorm.x * cosf(rad) - dirNorm.y * sinf(rad);
        dirRotated.y = dirNorm.x * sinf(rad) + dirNorm.y * cosf(rad);
        dirRotated.z = dirNorm.z;

        cg2::Vector3 velocity = param.bulletSpeed * dirRotated;

        auto bullet = std::make_unique<Bullet>();

        // 銃口指定はその座標を使う。旧中心指定では所有者別にbaseDirで前へずらす（位置補正は正規化前の方向）。
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

        // 耐久度・貫通力の指定が正でなければ、ダメージから最低1の値を補う。
        bullet->Initialize(bulletOrigin, velocity, param.damage, owner, param.reflect,
                           param.bulletHp > 0.0f ? param.bulletHp : static_cast<float>((std::max)(1u, param.damage)),
                           param.bulletPenetration > 0.0f ? param.bulletPenetration : static_cast<float>((std::max)(1u, param.damage)));

        bullet->SetCanClaimRunResource(param.canClaimRunResource);
        bullet->ConfigureGrowth(param.maxWallBounces, param.actorPierceCount, param.impactSplitCount, param.impactSplitDamageScale);
        // 射撃強化とドローンの命中元は自機弾だけに設定する。敵側の基本生成条件は維持する。
        if (owner == kPlayer) {
            bullet->ConfigureShooterAbilities(param.shooterChain, param.shooterMark, param.shooterBoomerang, param.shooterKillBurst,
                                              param.shooterChainPower, param.shooterMarkPower, param.shooterBoomerangPower,
                                              param.shooterKillBurstPower);
            bullet->ConfigureDroneSource(param.sourceDroneIndex, param.sourcePlayer);
            bullet->ConfigureVisualScale(param.bulletVisualScale, param.bulletTrailScale);
        }
        // 所有権を渡す。遠征弾の上限でAddが拒否すると弾は破棄され、登録の成否はここへ返らない。
        bulletManager_->Add(std::move(bullet));
    }
}
