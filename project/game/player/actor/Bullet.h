#pragma once
#include <Windows.h>
#include <algorithm>
#include "Calculation.h"
#include "Collider.h"
#include "CollisionConfig.h"
#include <Object3d.h>

class Bullet : public Collider {

public:

	void Initialize(const Vector3& position, const Vector3& velocity, const uint32_t& damage, BulletOwner owner,
		bool reflectable);

	void Update(float deltaTime);

	void Draw();

	bool IsDead() const { return isDead_; }

	/// <summary>
	/// 衝突判定
	/// </summary>
	void OnCollision(Collider* other) override;

	// ワールド座標を取得
	Vector3 GetWorldPosition() const override;

	Vector3 GetMove() { return velocity_; }

	// セッター
	void SetWorldPosition(const Vector3& pos) {
		worldTransform_.translate = pos;
		object_->SetTransform(worldTransform_);
		object_->Update();
	}

	void SetVelocity(const Vector3& v) { velocity_ = v; }

	float GetRadius() const override { return radius_; }

	bool IsReflectable() const { return isReflectable_; }

	void Die();

private:

	std::unique_ptr<Object3d> object_;

	// ワールドトランスフォーム
	Transform worldTransform_;

	// 速度
	Vector3 velocity_;

	// 寿命
	const float kLifeTime = 3.0f;
	// デスタイマー
	float deathTimer_ = kLifeTime;
	// デスフラグ
	bool isDead_ = false;

	// キャラクターの当たり判定サイズ
	static inline const float kWidth = 1.6f;
	static inline const float kHeight = 1.6f;

	float radius_ = 0.5f;

	// 反射するか
	bool isReflectable_ = false;
	BulletOwner owner_;
};
