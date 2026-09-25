#include "Bullet.h"
#include "ParticleManager.h"
#include <cmath>

void Bullet::Initialize(const Vector3& position, const Vector3& velocity, const uint32_t& damage, BulletOwner owner,
	bool reflectable, float bulletHp, float bulletPenetration) {
	object_ = std::make_unique<Object3d>();
	object_->Initialize();

	owner_ = owner;
	isDead_ = false;
	deathTimer_ = kLifeTime;
	usesRunProjectileRules_ = false;
	remainingWallBounces_ = -1;
	remainingActorPierces_ = 0;
	impactSplitCount_ = 0;
	pendingImpactSplitCount_ = 0;
	hitActorIds_.clear();
	growthEvents_ = {};
	canClaimRunResource_ = true;
	isReflectable_ = reflectable;
	bulletHp_ = (std::max)(0.1f, bulletHp);
	bulletPenetration_ = (std::max)(0.1f, bulletPenetration);
	

	// モデル切り替え（見た目差分）
	if (owner_ == kPlayer) {
		object_->SetModel("bullet.obj");
		SetCollisionAttribute(kCollisionAttributePlayerBullet);
		SetCollisionMask(kCollisionAttributeEnemy | kCollisionAttributeExpEnemy | kCollisionAttributeEnemyBullet);
		if (isReflectable_) {
			object_->SetColor(Vector4(1.0f, 1.0f, 0.0f, 1.0f));
		} else {
			object_->SetColor(Vector4(1.0f, 0.78f, 0.28f, 1.0f));
		}
	} else if (owner_ == kEnemy) {
		object_->SetModel("bullet.obj");
		object_->SetColor(Vector4(1.0f, 0.22f, 0.38f, 1.0f));
		SetCollisionAttribute(kCollisionAttributeEnemyBullet);
		SetCollisionMask(kCollisionAttributePlayer | kCollisionAttributePlayerDrone | kCollisionAttributeExpEnemy | kCollisionAttributePlayerBullet);
	} else if (owner_ == kExpEnemyHostile) {
		object_->SetModel("bullet.obj");
		object_->SetColor(Vector4(1.0f, 0.16f, 0.08f, 1.0f));
		SetCollisionAttribute(kCollisionAttributeHostileExpEnemyBullet);
		SetCollisionMask(kCollisionAttributeEnemy);
	}

	worldTransform_ = InitWorldTransform();
	worldTransform_.translate = position;
	worldTransform_.scale = Vector3(0.5f, 0.5f, 0.5f);
	velocity_ = velocity;
	if (Length(velocity_) > 0.001f) {
		worldTransform_.rotate.z = std::atan2(velocity_.y, velocity_.x);
	}

	SetDamage(damage);

	object_->SetTransform(worldTransform_);
	ApplyVisualSettings();
	object_->Update();
}

void Bullet::Update(float deltaTime) {
	if (isDead_) return;

	// 座標を移動させる
	worldTransform_.translate += velocity_ * (deltaTime * 60.0f);
	if (Length(velocity_) > 0.001f) {
		worldTransform_.rotate.z = std::atan2(velocity_.y, velocity_.x);
	}

	// 時間経過でデス
	deathTimer_ -= deltaTime;
	if (deathTimer_ <= 0) {
		Die();
	}

	object_->SetTransform(worldTransform_);
	ApplyVisualSettings();
	object_->Update();
	UpdateTrail(deltaTime);

}

void Bullet::Draw() {

	//object_->Draw();

}

void Bullet::OnCollision(Collider* other) {
	if (isDead_) {
		return;
	}
	Bullet* otherBullet = dynamic_cast<Bullet*>(other);
	if (otherBullet) {
		// CollisionManager checked both bullets before this pair. The first
		// callback may consume one, but both sides still exchange durability.
		if (otherBullet->GetOwner() == owner_) {
			return;
		}
		Vector3 impactNormal = velocity_ * -1.0f;
		ParticleManager::GetInstance()->EmitNeonImpactEffect(
			GetWorldPosition(), impactNormal, GetBulletColor(), 7);
		ApplyBulletDurabilityDamage(otherBullet->GetBulletPenetration());
		return;
	}

	if (!CanHitActor(other)) return;
	hitActorIds_.push_back(other->GetCollisionId());
	QueueImpactSplit(velocity_);
	Vector3 impactNormal = velocity_ * -1.0f;
	ParticleManager::GetInstance()->EmitNeonImpactEffect(
		GetWorldPosition(), impactNormal, GetBulletColor(), 11);
	if (remainingActorPierces_ > 0) {
		--remainingActorPierces_;
		++growthEvents_.actorPierces;
	} else {
		Die();
	}
}

void Bullet::ConfigureGrowth(int maxWallBounces, int actorPierceCount, int impactSplitCount,
	float impactSplitDamageScale)
{
	remainingWallBounces_ = (std::clamp)(maxWallBounces, -1, 32);
	remainingActorPierces_ = (std::clamp)(actorPierceCount, 0, 8);
	impactSplitCount_ = (std::clamp)(impactSplitCount, 0, 2);
	impactSplitDamageScale_ = std::isfinite(impactSplitDamageScale)
		? (std::clamp)(impactSplitDamageScale, 0.1f, 0.95f) : 0.55f;
	usesRunProjectileRules_ = maxWallBounces >= 0 || remainingActorPierces_ > 0 || impactSplitCount_ > 0;
}

bool Bullet::CanHitActor(const Collider* actor) const
{
	return actor && std::find(hitActorIds_.begin(), hitActorIds_.end(), actor->GetCollisionId()) == hitActorIds_.end();
}

void Bullet::QueueImpactSplit(const Vector3& direction)
{
	if (impactSplitCount_ <= 0 || deathTimer_ <= 0.0f) return;
	const float speed = Length(direction);
	if (!std::isfinite(speed) || speed <= 0.0001f) return;
	pendingImpactSplitCount_ = impactSplitCount_;
	impactSplitCount_ = 0;
	pendingImpactDirection_ = direction;
	pendingImpactPosition_ = GetWorldPosition();
	pendingImpactWallNormal_ = {};
}

void Bullet::OnWallImpact(const Vector3& safePosition, const Vector3& normal)
{
	if (isDead_) return;
	SetWorldPosition(safePosition);
	const float normalLength = Length(normal);
	if (!std::isfinite(normalLength) || normalLength <= 0.0001f) {
		Die();
		return;
	}
	const Vector3 unitNormal = normal / normalLength;
	const Vector3 reflected = velocity_ - 2.0f * Dot(velocity_, unitNormal) * unitNormal;
	if (!std::isfinite(Length(reflected))) {
		Die();
		return;
	}
	const bool canReflect = isReflectable_ && remainingWallBounces_ != 0;
	if (canReflect && remainingWallBounces_ > 0) --remainingWallBounces_;
	if (canReflect && usesRunProjectileRules_) ++growthEvents_.wallBounces;
	// Split away from the surface even when the parent has no ricochet card.
	const bool splitAtWall = impactSplitCount_ > 0;
	QueueImpactSplit(reflected);
	if (splitAtWall) pendingImpactWallNormal_ = unitNormal;
	if (canReflect) {
		SetVelocity(reflected);
	} else {
		Die();
	}
}

void Bullet::AppendImpactChildren(std::vector<std::unique_ptr<Bullet>>& children, size_t availableSlots)
{
	const int count = (std::min)(pendingImpactSplitCount_, static_cast<int>((std::min)(availableSlots, size_t{2})));
	pendingImpactSplitCount_ = 0; // A saturated budget drops this impact, never retries later.
	if (count <= 0 || deathTimer_ <= 0.0f) return;
	const float speed = Length(pendingImpactDirection_);
	if (!std::isfinite(speed) || speed <= 0.0001f) return;
	const Vector3 base = pendingImpactDirection_ / speed;
	for (int index = 0; index < count; ++index) {
		const float angle = count == 1 ? 0.0f : (index == 0 ? -0.42f : 0.42f);
		const float cosine = std::cos(angle), sine = std::sin(angle);
		Vector3 direction = {base.x * cosine - base.y * sine, base.x * sine + base.y * cosine, base.z};
		// An oblique fork must not start by flying back into the wall.
		const float outward = Dot(direction, pendingImpactWallNormal_);
		if (outward < 0.1f && Length(pendingImpactWallNormal_) > 0.5f) {
			direction = Normalize(direction + pendingImpactWallNormal_ * (0.1f - outward));
		}
		auto child = std::make_unique<Bullet>();
		const uint32_t childDamage = static_cast<uint32_t>((std::max)(1.0, std::round(static_cast<double>(GetDamage()) * impactSplitDamageScale_)));
		child->Initialize(pendingImpactPosition_ + direction * (radius_ + 0.1f), direction * (speed * 0.9f),
			childDamage, owner_, isReflectable_, bulletHp_, bulletPenetration_);
		child->ConfigureGrowth(remainingWallBounces_, remainingActorPierces_, 0, impactSplitDamageScale_);
		child->usesRunProjectileRules_ = true;
		child->deathTimer_ = deathTimer_;
		child->canClaimRunResource_ = canClaimRunResource_;
		child->hitActorIds_ = hitActorIds_;
		children.push_back(std::move(child));
	}
}

void Bullet::ApplyBulletDurabilityDamage(float amount)
{
	if (isDead_) {
		return;
	}
	bulletHp_ -= (std::max)(0.0f, amount);
	if (bulletHp_ <= 0.0f) {
		Die();
	}
}

Vector3 Bullet::GetWorldPosition() const {

	// ワールド座標を入れる変数
	Vector3 worldPos;
	// ワールド行列の平行移動成分を取得(ワールド座標)
	worldPos.x = worldTransform_.translate.x;
	worldPos.y = worldTransform_.translate.y;
	worldPos.z = worldTransform_.translate.z;

	return worldPos;
}

void Bullet::Die() {
	isDead_ = true;
	ReleaseTrail();
}

void Bullet::ReleaseTrail() {
	if (trail_) {
		trail_->SetActive(false);
		trail_ = nullptr;
	}
}

void Bullet::AttachTrail(TrailManager* trailManager, BulletTrailSettings* trailSettings) {
	if (!trailManager || trail_) {
		return;
	}

	trailSettings_ = trailSettings;
	ApplyVisualSettings();
	trail_ = trailManager->CreateInstance();
	trail_->SetIsPermanent(false);
	trail_->SetActive(true);
	trail_->SetConfig(MakeTrailConfig());
}

Vector4 Bullet::GetBulletColor() const {
	if (trailSettings_) {
		if (owner_ == kPlayer && isReflectable_) {
			return trailSettings_->reflectableObjectColor;
		}
		if (owner_ == kPlayer) {
			return trailSettings_->playerObjectColor;
		}
		if (owner_ == kExpEnemyHostile) {
			return { 1.0f, 0.16f, 0.08f, 1.0f };
		}
		return trailSettings_->enemyObjectColor;
	}
	if (owner_ == kPlayer) {
		if (isReflectable_) {
			return { 1.0f, 1.0f, 0.22f, 1.0f };
		}
		return { 1.0f, 0.55f, 0.20f, 1.0f };
	}
	if (owner_ == kExpEnemyHostile) {
		return { 1.0f, 0.16f, 0.08f, 1.0f };
	}
	return { 1.0f, 0.20f, 0.36f, 1.0f };
}

void Bullet::ApplyVisualSettings() {
	if (!object_) {
		return;
	}
	object_->SetColor(GetBulletColor());
}

TrailConfig Bullet::MakeTrailConfig() const {
	TrailConfig config{};
	const Vector4 color = GetBulletColor();
	if (trailSettings_) {
		if (trailSettings_->useObjectColorForTrail) {
			config.startColor = {
				color.x * trailSettings_->trailHeadIntensity,
				color.y * trailSettings_->trailHeadIntensity,
				color.z * trailSettings_->trailHeadIntensity,
				trailSettings_->trailHeadAlpha
			};
			config.endColor = {
				color.x * trailSettings_->trailTailIntensity,
				color.y * trailSettings_->trailTailIntensity,
				color.z * trailSettings_->trailTailIntensity,
				trailSettings_->trailTailAlpha
			};
		} else {
			config.startColor = trailSettings_->startColor;
			if (owner_ == kPlayer && isReflectable_) {
				config.endColor = trailSettings_->reflectableEndColor;
			} else if (owner_ == kPlayer) {
				config.endColor = trailSettings_->playerEndColor;
			} else {
				config.endColor = trailSettings_->enemyEndColor;
			}
		}
		config.interpolationSteps = static_cast<uint32_t>((std::max)(1, trailSettings_->interpolationSteps));
		config.maxPoints = static_cast<uint32_t>((std::max)(2, trailSettings_->maxPoints));
		config.lifetime = (std::max)(0.01f, trailSettings_->lifetime);
		config.startWidthScale = (std::max)(0.0f, trailSettings_->headWidthScale);
		config.endWidthScale = (std::max)(0.0f, trailSettings_->tailWidthScale);
		config.widthCurvePower = (std::max)(0.05f, trailSettings_->widthCurvePower);
		config.colorCurvePower = (std::max)(0.05f, trailSettings_->colorCurvePower);
		if(owner_==kPlayer) {
			config.lifetime*=(std::clamp)(trailSettings_->playerTrailLifetimeScale,0.1f,1.0f);
			config.startColor.w*=(std::clamp)(trailSettings_->playerTrailAlphaScale,0.1f,1.0f);
			config.endColor.w*=(std::clamp)(trailSettings_->playerTrailAlphaScale,0.1f,1.0f);
		}
	} else {
		config.startColor = { 1.0f, 0.98f, 0.78f, 1.0f };
		config.endColor = { color.x, color.y, color.z, 0.0f };
		config.interpolationSteps = 5;
		config.maxPoints = 22;
		config.lifetime = 0.24f;
	}
	return config;
}

void Bullet::UpdateTrail(float deltaTime) {
	if (!trail_ || trail_->IsActive() == false) {
		return;
	}

	Vector3 dir = velocity_;
	dir.z = 0.0f;
	const float speed = Length(dir);
	if (speed <= 0.001f) {
		return;
	}
	dir = dir / speed;

	Vector3 side = { -dir.y, dir.x, 0.0f };
	float halfWidth = (owner_ == kPlayer) ? 0.26f : 0.22f;
	if (trailSettings_) {
		halfWidth = (owner_ == kPlayer) ? trailSettings_->playerHalfWidth : trailSettings_->enemyHalfWidth;
	}
	const Vector3 center = { worldTransform_.translate.x, worldTransform_.translate.y, worldTransform_.translate.z - 0.015f };
	const Vector3 tip = center + side * halfWidth;
	const Vector3 base = center - side * halfWidth;
	trail_->Update(deltaTime, tip, base, MakeTrailConfig());
}
