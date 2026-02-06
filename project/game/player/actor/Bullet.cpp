#include "Bullet.h"

void Bullet::Initialize(const Vector3& position, const Vector3& velocity, const uint32_t& damage, BulletOwner owner,
	bool reflectable) {
	object_ = std::make_unique<Object3d>();
	object_->Initialize();

	owner_ = owner;
	isReflectable_ = reflectable;
	

	// モデル切り替え（見た目差分）
	if (owner_ == kPlayer) {
		object_->SetModel("playerBullet.obj");
		SetCollisionAttribute(kCollisionAttributePlayerBullet);
		SetCollisionMask(kCollisionAttributeEnemy);
		if (isReflectable_) {
			object_->SetColor(Vector4(1.0f, 1.0f, 0.0f, 1.0f));
		}
	} else if (owner_ == kEnemy) {
		object_->SetModel("enemyBullet.obj");
		SetCollisionAttribute(kCollisionAttributeEnemyBullet);
		SetCollisionMask(kCollisionAttributePlayer | kCollisionAttributePlayerDrone);
	}

	worldTransform_ = InitWorldTransform();
	worldTransform_.translate = position;
	worldTransform_.scale = Vector3(0.5f, 0.5f, 0.5f);
	velocity_ = velocity;

	SetDamage(damage);

	object_->SetTransform(worldTransform_);
	object_->Update();
}

void Bullet::Update(float deltaTime) {

	// 座標を移動させる
	worldTransform_.translate += velocity_ * (deltaTime * 60.0f);

	// 時間経過でデス
	deathTimer_ -= deltaTime;
	if (deathTimer_ <= 0) {
		isDead_ = true;
	}

	object_->SetTransform(worldTransform_);
	object_->Update();

}

void Bullet::Draw() {

	object_->Draw();

}

void Bullet::OnCollision(Collider* other) {
	(void)other;
	// デスフラグを立てる
	isDead_ = true;
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
}