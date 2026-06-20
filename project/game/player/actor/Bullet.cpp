#include "Bullet.h"
#include "ParticleManager.h"
#include <cmath>

void Bullet::Initialize(const Vector3& position, const Vector3& velocity, const uint32_t& damage, BulletOwner owner,
	bool reflectable, float bulletHp, float bulletPenetration) {
	object_ = std::make_unique<Object3d>();
	object_->Initialize();

	owner_ = owner;
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
		if (otherBullet->IsDead()) {
			return;
		}
		if (otherBullet->GetOwner() == owner_) {
			return;
		}
		Vector3 impactNormal = velocity_ * -1.0f;
		ParticleManager::GetInstance()->EmitNeonImpactEffect(
			GetWorldPosition(), impactNormal, GetBulletColor(), 7);
		ApplyBulletDurabilityDamage(otherBullet->GetBulletPenetration());
		return;
	}

	Vector3 impactNormal = velocity_ * -1.0f;
	ParticleManager::GetInstance()->EmitNeonImpactEffect(
		GetWorldPosition(), impactNormal, GetBulletColor(), 11);
	Die();
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
		return trailSettings_->enemyObjectColor;
	}
	if (owner_ == kPlayer) {
		if (isReflectable_) {
			return { 1.0f, 1.0f, 0.22f, 1.0f };
		}
		return { 1.0f, 0.55f, 0.20f, 1.0f };
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
