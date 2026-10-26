#include "CombatCollisionRules.h"
#include "Enemy.h"
#include "Player.h"
#include "Bullet.h"
#include "BulletManager.h"
#include "EnemyManager.h"
#include "game/player/TankSpecialCombat.h"

bool CombatCollisionRules::CanTest(Collider* colliderA, Collider* colliderB) const
{
    // 前のペアで消費された弾は対象外。死亡判定は入口だけで行い、成立した接触では双方へ通知する。
    // 片方のOnCollisionで弾が死亡しても、もう片方がダメージや命中処理を受け取れるようにする。
    const Bullet* bulletA = ColliderCast<const Bullet>(colliderA);
    const Bullet* bulletB = ColliderCast<const Bullet>(colliderB);
    if ((bulletA && bulletA->IsDead()) || (bulletB && bulletB->IsDead())) {
        return false;
    }
    // 連鎖ダメージで、この走査中に後続の対象が撃破されることもある。
    for (auto* actor : {colliderA, colliderB}) {
        if (auto* enemy = ColliderCast<ExpEnemy>(actor); enemy && enemy->IsDead()) {
            return false;
        }
        if (auto* boss = ColliderCast<Enemy>(actor); boss && (boss->IsDead() || boss->GetHp() <= 0)) {
            return false;
        }
    }
    // 貫通弾が複数フレーム重なっても、衝突IDの命中履歴で双方の通知を抑え、ダメージ・演出の重複を防ぐ。
    if ((bulletA && !bulletB && !bulletA->CanHitActor(colliderB)) || (bulletB && !bulletA && !bulletB->CanHitActor(colliderA))) {
        return false;
    }
    if ((bulletA && bulletA->GetSpecialKind() == Bullet::SpecialKind::SlashWave && !bulletA->CanHitActor(colliderB)) ||
        (bulletB && bulletB->GetSpecialKind() == Bullet::SpecialKind::SlashWave && !bulletB->CanHitActor(colliderA))) {
        return false;
    }
    return true;
}

bool CombatCollisionRules::HitsBySweep(Collider* colliderA, Collider* colliderB) const
{
    const Bullet* bulletA = ColliderCast<const Bullet>(colliderA);
    const Bullet* bulletB = ColliderCast<const Bullet>(colliderB);
    // 高速移動で現在位置の球が敵を通り越した場合も、XY平面の移動区間で接触を拾う。
    auto swept = [&](const Bullet* shot, Collider* target) {
        if (!shot || (shot->GetSpecialKind() == Bullet::SpecialKind::None && !shot->IsBoomerang())) {
            return false;
        }
        const auto a = shot->GetPreviousWorldPosition(), b = shot->GetWorldPosition(), p = target->GetWorldPosition();
        return tankspecial::SegmentTouches(a.x, a.y, b.x, b.y, p.x, p.y, shot->GetRadius() + target->GetRadius());
    };
    return swept(bulletA, colliderB) || swept(bulletB, colliderA);
}

void CombatCollisionRules::Resolve(Collider* colliderA, Collider* colliderB)
{
    // 自機のダッシュ衝撃で処理する接触は通常通知を省く。同じダッシュの対象履歴はPlayerが保持する。
    if (auto* player = ColliderCast<Player>(colliderA); player && player->TryDashImpact(colliderB)) {
        return;
    }
    if (auto* player = ColliderCast<Player>(colliderB); player && player->TryDashImpact(colliderA)) {
        return;
    }

    // 床コアの通常接触だけを省く。直前の明示ダッシュ攻撃は既存経路で成立させる。
    auto isDepthCoreContact = [](Collider* candidateCore, Collider* body) {
        const auto* core = ColliderCast<const Enemy>(candidateCore);
        const uint32_t attribute = body->GetCollisionAttribute();
        return core && core->IsNeonDepthEncounterEnabled() &&
               (attribute == kCollisionAttributePlayer || attribute == kCollisionAttributePlayerDrone);
    };
    if (isDepthCoreContact(colliderA, colliderB) || isDepthCoreContact(colliderB, colliderA)) {
        return;
    }

    Bullet* shot = ColliderCast<Bullet>(colliderA);
    Collider* victim = colliderB;
    if (!shot) {
        shot = ColliderCast<Bullet>(colliderB);
        victim = colliderA;
    }
    const bool playerHit = shot && shot->GetOwner() == kPlayer && !ColliderCast<Bullet>(victim) &&
                           (ColliderCast<ExpEnemy>(victim) || ColliderCast<Enemy>(victim));
    // 装甲で反射する弾は通常命中を適用せず、反射弾の生成を予約して元弾を死亡状態にする。
    if (playerHit && bulletManager_ && !shot->WasArmorReflected() &&
        (shot->GetSpecialKind() == Bullet::SpecialKind::None || shot->GetSpecialKind() == Bullet::SpecialKind::Rail)) {
        if (auto* armor = ColliderCast<ExpEnemy>(victim)) {
            myEngine::Vector3 movement = shot->GetWorldPosition() - shot->GetPreviousWorldPosition();
            if (myEngine::Length(movement) < .0001f) {
                movement = shot->GetMove();
            }
            const myEngine::Vector3 source = victim->GetWorldPosition() - myEngine::NormalizeOr(movement, myEngine::Vector3{1, 0, 0});
            if (armor->TryReflectProjectile(source)) {
                bulletManager_->QueueArmorReflection(*shot, victim->GetWorldPosition());
                shot->Die();
                return;
            }
        }
    }
    // ロック補正は今回の接触だけに使う。貫通して別の敵に当たるときの基礎ダメージは保持する。
    const uint32_t originalDamage = shot ? shot->GetDamage() : 0;
    if (playerHit && shot->GetSourcePlayer() && shot->GetSourceDroneIndex() >= 0) {
        shot->SetDamage(shot->GetSourcePlayer()->NotifyDroneHit(shot->GetSourceDroneIndex(), victim, originalDamage));
    }

    // 片方の通知で死亡しても実体はまだ管理配列に残る。双方の通知後に撃破結果から射撃強化を処理する。
    colliderA->OnCollision(colliderB);
    colliderB->OnCollision(colliderA);
    if (playerHit && bulletManager_) {
        bool killed = false;
        if (auto* regular = ColliderCast<ExpEnemy>(victim)) {
            killed = regular->IsDead();
        } else if (auto* boss = ColliderCast<Enemy>(victim)) {
            killed = boss->GetHp() <= 0;
        }
        bulletManager_->NotifyPlayerHit(*shot, *victim, killed);
    }
    // 射撃強化の通知まで補正値を使い終えてから元に戻す。この場で弾を削除しない。
    if (shot) {
        shot->SetDamage(originalDamage);
    }
}
