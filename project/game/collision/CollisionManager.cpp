#include "CollisionManager.h"
#include "Enemy.h"
#include "Player.h"
#include "Bullet.h"
#include "BulletManager.h"
#include "EnemyManager.h"
#include "RuntimeProfiler.h"

void CollisionManager::CheckAllCollisions(Player* player, Enemy* enemy, BulletManager* bulletManager, EnemyManager* enemyManager)
{
    rules_.SetBulletManager(bulletManager);
    bulletManager->SetCombatContext(player, enemy, enemyManager);

    // この走査で借用する一覧を作り直す。通知が終わるまで弾・敵・ドローンを削除しない。
    // clearは容量を残すので、毎フレームの確保を避けられる。
    colliders_.clear();

    SetColliders(player, enemy, bulletManager, enemyManager);

    // 同じ組を重複して調べないよう、後ろの要素だけと組み合わせる。通知中に一覧へ追加しない。
    for (size_t indexA = 0; indexA < colliders_.size(); ++indexA) {
        for (size_t indexB = indexA + 1; indexB < colliders_.size(); ++indexB) {
            CheckCollisionPair(colliders_[indexA], colliders_[indexB]);
        }
    }
    // 通知中は生成を予約し、全ペアの走査後に反映する。追加による弾配列の再確保を走査へ持ち込まない。
    myEngine::RuntimeProfiler::CpuScope flushScope("Collision Flush");
    bulletManager->FlushPendingSplits();
    rules_.SetBulletManager(nullptr);
}

void CollisionManager::CheckCollisionPair(Collider* colliderA, Collider* colliderB)
{
    // どちらかのマスクが相手を許可すれば、後続で双方へ通知する。マスクごとに通知を分ける処理ではない。
    // 属性だけで決まるので、型の確認や距離の計算より先に、当たらない組を除外する。
    const bool canA = (colliderA->GetCollisionMask() & colliderB->GetCollisionAttribute());
    const bool canB = (colliderB->GetCollisionMask() & colliderA->GetCollisionAttribute());
    if (!(canA || canB)) {
        return;
    }

    // 死亡済みの対象や命中済みの組など、戦闘のルールで調べない組を除外する。
    if (!rules_.CanTest(colliderA, colliderB)) {
        return;
    }
    if (!IsTouching(colliderA, colliderB)) {
        return;
    }
    // 接触した組への処理（ダッシュ衝撃・床コア・装甲反射・ドローン補正・通知）は戦闘のルールに任せる。
    myEngine::RuntimeProfiler::CpuScope scope("Collision Resolve");
    rules_.Resolve(colliderA, colliderB);
}

bool CollisionManager::IsTouching(Collider* colliderA, Collider* colliderB) const
{
    bool hit = false;

    // 球同士は現在位置で判定する。特殊弾・帰還弾は前回位置からの線分も調べる。
    if (colliderA->GetShape() == ColliderShape::Sphere && colliderB->GetShape() == ColliderShape::Sphere) {
        float dist = myEngine::Length(colliderA->GetWorldPosition() - colliderB->GetWorldPosition());
        hit = dist < (colliderA->GetRadius() + colliderB->GetRadius());
        hit = hit || rules_.HitsBySweep(colliderA, colliderB);
    }

    // レーザーのカプセルと球は、線分と半径による判定を使う。
    else if (colliderA->GetShape() == ColliderShape::Capsule && colliderB->GetShape() == ColliderShape::Sphere) {
        myEngine::Sphere s{colliderB->GetWorldPosition(), colliderB->GetRadius()};
        hit = myEngine::IsCollision(colliderA->GetSegment(), s, colliderA->GetCapsuleRadius());
    } else if (colliderB->GetShape() == ColliderShape::Capsule && colliderA->GetShape() == ColliderShape::Sphere) {
        myEngine::Sphere s{colliderA->GetWorldPosition(), colliderA->GetRadius()};
        hit = myEngine::IsCollision(colliderB->GetSegment(), s, colliderB->GetCapsuleRadius());
    }
    return hit;
}

void CollisionManager::SetColliders(Player* player, Enemy* enemy, BulletManager* bulletManager, EnemyManager* enemyManager)
{
    // 自機はここでは死亡状態で除外しない。各通知の受付条件とシーン側の呼び出し条件を併せて確認する。
    colliders_.push_back(player);

    // 再構築中など利用できないドローンを除外する。
    for (PlayerDrone* drone : player->GetDronePtrs()) {
        if (drone && drone->IsRunAvailable()) {
            colliders_.push_back(drone);
        }
    }

    // 敵を登録
    if (enemy && !enemy->IsDead()) {
        colliders_.push_back(enemy);
    }

    // 死亡済みの削除待ち弾も一覧には含む。ペアの入口で除外し、削除はBulletManagerの更新に任せる。
    for (Bullet* bullet : bulletManager->GetBulletPtrs()) {
        colliders_.push_back(bullet);
    }

    if (enemyManager) {
        for (ExpEnemy* expEnemy : enemyManager->GetEnemyPtrs()) {
            if (expEnemy && !expEnemy->IsDead()) {
                colliders_.push_back(expEnemy);
            }
        }
    }
}
