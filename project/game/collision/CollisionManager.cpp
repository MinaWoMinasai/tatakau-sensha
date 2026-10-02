#include "CollisionManager.h"
#include "Enemy.h"
#include "Player.h"
#include "Bullet.h"
#include "BulletManager.h"
#include "EnemyManager.h"
#include "game/player/TankSpecialCombat.h"

void CollisionManager::CheckAllCollisions(Player* player, Enemy* enemy, BulletManager* bulletManager, EnemyManager* enemyManager) {
	activeBulletManager_=bulletManager;
	bulletManager->SetCombatContext(player,enemy,enemyManager);

	// 衝突マネージャのリストをクリア
	colliders_.clear();

	// コライダーをリストに登録
	SetColliders(player, enemy, bulletManager, enemyManager);
	
	// リスト内のペアの総当たり
	std::list<Collider*>::iterator itrA = colliders_.begin();
	for (; itrA != colliders_.end(); ++itrA) {

		// イテレータAからコライダーAを取得
		Collider* colliderA = *itrA;

		std::list<Collider*>::iterator itrB = itrA;
		itrB++;

		for (; itrB != colliders_.end(); ++itrB) {

			// イテレータBからコライダーBを取得
			Collider* colliderB = *itrB;

			// ペアのあたり判定
			CheckCollisionPair(colliderA, colliderB);
		}
	}
	bulletManager->FlushPendingSplits();
	activeBulletManager_=nullptr;
}

void CollisionManager::CheckCollisionPair(Collider* colliderA, Collider* colliderB) {
	// Earlier pairs may have consumed a projectile. Check only at entry: the
	// first valid impact must still deliver both callbacks if one kills a bullet.
	const Bullet* bulletA = dynamic_cast<const Bullet*>(colliderA);
	const Bullet* bulletB = dynamic_cast<const Bullet*>(colliderB);
	if ((bulletA && bulletA->IsDead()) || (bulletB && bulletB->IsDead())) {
		return;
	}
	// Chain damage can defeat later actors in this same collision pass.
	for(auto* actor:{colliderA,colliderB}) {
		if(auto* enemy=dynamic_cast<ExpEnemy*>(actor);enemy&&enemy->IsDead())return;
		if(auto* boss=dynamic_cast<Enemy*>(actor);boss&&(boss->IsDead()||boss->GetHp()<=0))return;
	}
	// A penetrating bullet may overlap an actor for several frames. Suppress
	// both callbacks so that neither damage nor impact effects repeat.
	if ((bulletA && !bulletB && !bulletA->CanHitActor(colliderB)) ||
		(bulletB && !bulletA && !bulletB->CanHitActor(colliderA))) return;
	if((bulletA&&bulletA->GetSpecialKind()==Bullet::SpecialKind::SlashWave&&!bulletA->CanHitActor(colliderB))||
		(bulletB&&bulletB->GetSpecialKind()==Bullet::SpecialKind::SlashWave&&!bulletB->CanHitActor(colliderA)))return;

	bool hit = false;

	// --- Sphere × Sphere ---
	if (colliderA->GetShape() == ColliderShape::Sphere && colliderB->GetShape() == ColliderShape::Sphere) {
		float dist = cg2::Length(colliderA->GetWorldPosition() - colliderB->GetWorldPosition());
		hit = dist < (colliderA->GetRadius() + colliderB->GetRadius());
		// Large, fast special shots must not tunnel through a target between frames.
		auto swept=[&](const Bullet* shot,Collider* target) {
			if(!shot||(shot->GetSpecialKind()==Bullet::SpecialKind::None&&!shot->IsBoomerang()))return false;
			const auto a=shot->GetPreviousWorldPosition(),b=shot->GetWorldPosition(),p=target->GetWorldPosition();
			return tankspecial::SegmentTouches(a.x,a.y,b.x,b.y,p.x,p.y,shot->GetRadius()+target->GetRadius());
		};
		hit=hit||swept(bulletA,colliderB)||swept(bulletB,colliderA);
	}

	// --- Capsule（Laser） × Sphere ---
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

	// フィルタリング
	bool canA = (colliderA->GetCollisionMask() & colliderB->GetCollisionAttribute());
	bool canB = (colliderB->GetCollisionMask() & colliderA->GetCollisionAttribute());
	if (!(canA || canB))
		return;

	// A successful early-dash body hit replaces both contact callbacks. The
	// player owns the per-dash target ledger, preventing repeated overlap damage.
	if (auto* player = dynamic_cast<Player*>(colliderA); player && player->TryDashImpact(colliderB)) return;
	if (auto* player = dynamic_cast<Player*>(colliderB); player && player->TryDashImpact(colliderA)) return;

	Bullet* shot=dynamic_cast<Bullet*>(colliderA);
	Collider* victim=colliderB;
	if(!shot){shot=dynamic_cast<Bullet*>(colliderB);victim=colliderA;}
	const bool playerHit=shot&&shot->GetOwner()==kPlayer&&!dynamic_cast<Bullet*>(victim)&&
		(dynamic_cast<ExpEnemy*>(victim)||dynamic_cast<Enemy*>(victim));
	if(playerHit&&activeBulletManager_&&!shot->WasArmorReflected()&&
		(shot->GetSpecialKind()==Bullet::SpecialKind::None||shot->GetSpecialKind()==Bullet::SpecialKind::Rail)) {
		if(auto* armor=dynamic_cast<ExpEnemy*>(victim)) {
			cg2::Vector3 movement=shot->GetWorldPosition()-shot->GetPreviousWorldPosition();
			if(cg2::Length(movement)<.0001f)movement=shot->GetMove();
			const cg2::Vector3 source=victim->GetWorldPosition()-(cg2::Length(movement)>.0001f?cg2::Normalize(movement):cg2::Vector3{1,0,0});
			if(armor->TryReflectProjectile(source)) {
				activeBulletManager_->QueueArmorReflection(*shot,victim->GetWorldPosition());shot->Die();return;
			}
		}
	}
	const uint32_t originalDamage=shot?shot->GetDamage():0;
	if(playerHit&&shot->GetSourcePlayer()&&shot->GetSourceDroneIndex()>=0)
		shot->SetDamage(shot->GetSourcePlayer()->NotifyDroneHit(shot->GetSourceDroneIndex(),victim,originalDamage));

	colliderA->OnCollision(colliderB);
	colliderB->OnCollision(colliderA);
	if(playerHit&&activeBulletManager_) {
		bool killed=false;
		if(auto* regular=dynamic_cast<ExpEnemy*>(victim))killed=regular->IsDead();
		else if(auto* boss=dynamic_cast<Enemy*>(victim))killed=boss->GetHp()<=0;
		activeBulletManager_->NotifyPlayerHit(*shot,*victim,killed);
	}
	if(shot)shot->SetDamage(originalDamage);
}

void CollisionManager::SetColliders(Player* player, Enemy* enemy, BulletManager* bulletManager, EnemyManager* enemyManager) {

	// プレイヤーを登録
	colliders_.push_back(player);

	// プレイヤードローンを登録
	for (PlayerDrone* drone : player->GetDronePtrs()) {
		if(drone&&drone->IsRunAvailable())colliders_.push_back(drone);
	}

	// 敵を登録
	if (enemy && !enemy->IsDead()) colliders_.push_back(enemy);

	// 弾を登録
	for (Bullet* bullet : bulletManager->GetBulletPtrs()) {
		colliders_.push_back(bullet);
	}

	if (enemyManager) {
		for (ExpEnemy* expEnemy : enemyManager->GetEnemyPtrs()) {
			if(expEnemy&&!expEnemy->IsDead())colliders_.push_back(expEnemy);
		}
	}
	
}
