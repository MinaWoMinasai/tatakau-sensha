#include "CollisionManager.h"
#include "Enemy.h"
#include "Player.h"
#include "Bullet.h"
#include "BulletManager.h"
#include "EnemyManager.h"
#include "game/player/TankSpecialCombat.h"

void CollisionManager::CheckAllCollisions(Player* player, Enemy* enemy, BulletManager* bulletManager, EnemyManager* enemyManager)
{
    activeBulletManager_ = bulletManager;
    bulletManager->SetCombatContext(player, enemy, enemyManager);

    // この走査で借用する一覧を作り直す。通知が終わるまで弾・敵・ドローンを削除しない。
    colliders_.clear();

    SetColliders(player, enemy, bulletManager, enemyManager);

    // 同じ組を重複して調べないよう、後ろの要素だけと組み合わせる。
    std::list<Collider*>::iterator itrA = colliders_.begin();
    for (; itrA != colliders_.end(); ++itrA) {

        Collider* colliderA = *itrA;

        std::list<Collider*>::iterator itrB = itrA;
        itrB++;

        for (; itrB != colliders_.end(); ++itrB) {

            Collider* colliderB = *itrB;

            CheckCollisionPair(colliderA, colliderB);
        }
    }
    // 通知中は生成を予約し、全ペアの走査後に反映する。追加による弾配列の再確保を走査へ持ち込まない。
    bulletManager->FlushPendingSplits();
    activeBulletManager_ = nullptr;
}

void CollisionManager::CheckCollisionPair(Collider* colliderA, Collider* colliderB)
{
    // 前のペアで消費された弾は対象外。死亡判定は入口だけで行い、成立した接触では双方へ通知する。
    // 片方のOnCollisionで弾が死亡しても、もう片方がダメージや命中処理を受け取れるようにする。
    const Bullet* bulletA = dynamic_cast<const Bullet*>(colliderA);
    const Bullet* bulletB = dynamic_cast<const Bullet*>(colliderB);
    if ((bulletA && bulletA->IsDead()) || (bulletB && bulletB->IsDead())) {
        return;
    }
    // 連鎖ダメージで、この走査中に後続の対象が撃破されることもある。
    for (auto* actor : {colliderA, colliderB}) {
        if (auto* enemy = dynamic_cast<ExpEnemy*>(actor); enemy && enemy->IsDead())
            return;
        if (auto* boss = dynamic_cast<Enemy*>(actor); boss && (boss->IsDead() || boss->GetHp() <= 0))
            return;
    }
    // 貫通弾が複数フレーム重なっても、衝突IDの命中履歴で双方の通知を抑え、ダメージ・演出の重複を防ぐ。
    if ((bulletA && !bulletB && !bulletA->CanHitActor(colliderB)) || (bulletB && !bulletA && !bulletB->CanHitActor(colliderA)))
        return;
    if ((bulletA && bulletA->GetSpecialKind() == Bullet::SpecialKind::SlashWave && !bulletA->CanHitActor(colliderB)) ||
        (bulletB && bulletB->GetSpecialKind() == Bullet::SpecialKind::SlashWave && !bulletB->CanHitActor(colliderA)))
        return;

    bool hit = false;

    // 球同士は現在位置で判定する。特殊弾・帰還弾は前回位置からの線分も調べる。
    if (colliderA->GetShape() == ColliderShape::Sphere && colliderB->GetShape() == ColliderShape::Sphere) {
        float dist = cg2::Length(colliderA->GetWorldPosition() - colliderB->GetWorldPosition());
        hit = dist < (colliderA->GetRadius() + colliderB->GetRadius());
        // 高速移動で現在位置の球が敵を通り越した場合も、XY平面の移動区間で接触を拾う。
        auto swept = [&](const Bullet* shot, Collider* target) {
            if (!shot || (shot->GetSpecialKind() == Bullet::SpecialKind::None && !shot->IsBoomerang()))
                return false;
            const auto a = shot->GetPreviousWorldPosition(), b = shot->GetWorldPosition(), p = target->GetWorldPosition();
            return tankspecial::SegmentTouches(a.x, a.y, b.x, b.y, p.x, p.y, shot->GetRadius() + target->GetRadius());
        };
        hit = hit || swept(bulletA, colliderB) || swept(bulletB, colliderA);
    }

    // レーザーのカプセルと球は、線分と半径による判定を使う。
    else if (colliderA->GetShape() == ColliderShape::Capsule && colliderB->GetShape() == ColliderShape::Sphere) {
        cg2::Sphere s{colliderB->GetWorldPosition(), colliderB->GetRadius()};
        hit = cg2::IsCollision(colliderA->GetSegment(), s, colliderA->GetCapsuleRadius());
    } else if (colliderB->GetShape() == ColliderShape::Capsule && colliderA->GetShape() == ColliderShape::Sphere) {
        cg2::Sphere s{colliderA->GetWorldPosition(), colliderA->GetRadius()};
        hit = cg2::IsCollision(colliderB->GetSegment(), s, colliderB->GetCapsuleRadius());
    }

    if (!hit) {
        return;
    }

    // どちらかのマスクが相手を許可すれば、後続で双方へ通知する。マスクごとに通知を分ける処理ではない。
    bool canA = (colliderA->GetCollisionMask() & colliderB->GetCollisionAttribute());
    bool canB = (colliderB->GetCollisionMask() & colliderA->GetCollisionAttribute());
    if (!(canA || canB))
        return;

    // 自機のダッシュ衝撃で処理する接触は通常通知を省く。同じダッシュの対象履歴はPlayerが保持する。
    if (auto* player = dynamic_cast<Player*>(colliderA); player && player->TryDashImpact(colliderB))
        return;
    if (auto* player = dynamic_cast<Player*>(colliderB); player && player->TryDashImpact(colliderA))
        return;

    Bullet* shot = dynamic_cast<Bullet*>(colliderA);
    Collider* victim = colliderB;
    if (!shot) {
        shot = dynamic_cast<Bullet*>(colliderB);
        victim = colliderA;
    }
    const bool playerHit = shot && shot->GetOwner() == kPlayer && !dynamic_cast<Bullet*>(victim) &&
                           (dynamic_cast<ExpEnemy*>(victim) || dynamic_cast<Enemy*>(victim));
    // 装甲で反射する弾は通常命中を適用せず、反射弾の生成を予約して元弾を死亡状態にする。
    if (playerHit && activeBulletManager_ && !shot->WasArmorReflected() &&
        (shot->GetSpecialKind() == Bullet::SpecialKind::None || shot->GetSpecialKind() == Bullet::SpecialKind::Rail)) {
        if (auto* armor = dynamic_cast<ExpEnemy*>(victim)) {
            cg2::Vector3 movement = shot->GetWorldPosition() - shot->GetPreviousWorldPosition();
            if (cg2::Length(movement) < .0001f)
                movement = shot->GetMove();
            const cg2::Vector3 source =
                victim->GetWorldPosition() - (cg2::Length(movement) > .0001f ? cg2::Normalize(movement) : cg2::Vector3{1, 0, 0});
            if (armor->TryReflectProjectile(source)) {
                activeBulletManager_->QueueArmorReflection(*shot, victim->GetWorldPosition());
                shot->Die();
                return;
            }
        }
    }
    // ロック補正は今回の接触だけに使う。貫通して別の敵に当たるときの基礎ダメージは保持する。
    const uint32_t originalDamage = shot ? shot->GetDamage() : 0;
    if (playerHit && shot->GetSourcePlayer() && shot->GetSourceDroneIndex() >= 0)
        shot->SetDamage(shot->GetSourcePlayer()->NotifyDroneHit(shot->GetSourceDroneIndex(), victim, originalDamage));

    // 片方の通知で死亡しても実体はまだ管理配列に残る。双方の通知後に撃破結果から射撃強化を処理する。
    colliderA->OnCollision(colliderB);
    colliderB->OnCollision(colliderA);
    if (playerHit && activeBulletManager_) {
        bool killed = false;
        if (auto* regular = dynamic_cast<ExpEnemy*>(victim))
            killed = regular->IsDead();
        else if (auto* boss = dynamic_cast<Enemy*>(victim))
            killed = boss->GetHp() <= 0;
        activeBulletManager_->NotifyPlayerHit(*shot, *victim, killed);
    }
    // 射撃強化の通知まで補正値を使い終えてから元に戻す。この場で弾を削除しない。
    if (shot)
        shot->SetDamage(originalDamage);
}

void CollisionManager::SetColliders(Player* player, Enemy* enemy, BulletManager* bulletManager, EnemyManager* enemyManager)
{

    // 自機はここでは死亡状態で除外しない。各通知の受付条件とシーン側の呼び出し条件を併せて確認する。
    colliders_.push_back(player);

    // 再構築中など利用できないドローンを除外する。
    for (PlayerDrone* drone : player->GetDronePtrs()) {
        if (drone && drone->IsRunAvailable())
            colliders_.push_back(drone);
    }

    // 敵を登録
    if (enemy && !enemy->IsDead())
        colliders_.push_back(enemy);

    // 死亡済みの削除待ち弾も一覧には含む。ペアの入口で除外し、削除はBulletManagerの更新に任せる。
    for (Bullet* bullet : bulletManager->GetBulletPtrs()) {
        colliders_.push_back(bullet);
    }

    if (enemyManager) {
        for (ExpEnemy* expEnemy : enemyManager->GetEnemyPtrs()) {
            if (expEnemy && !expEnemy->IsDead())
                colliders_.push_back(expEnemy);
        }
    }
}
