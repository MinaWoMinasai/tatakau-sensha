#pragma once
#include <Windows.h>
#include <algorithm>
#include "Calculation.h"
#include "Collider.h"
#include "CollisionConfig.h"
#include <Object3d.h>
#include "TrailManager.h"

struct BulletTrailSettings {
	float playerHalfWidth = 0.26f;
	float enemyHalfWidth = 0.22f;
	float lifetime = 0.24f;
	int maxPoints = 22;
	int interpolationSteps = 5;
	float headWidthScale = 1.0f;
	float tailWidthScale = 0.15f;
	bool useObjectColorForTrail = true;
	float trailHeadIntensity = 1.15f;
	float trailTailIntensity = 0.45f;
	float trailHeadAlpha = 1.0f;
	float trailTailAlpha = 0.0f;
	Vector4 playerObjectColor = { 1.0f, 0.78f, 0.28f, 1.0f };
	Vector4 enemyObjectColor = { 1.0f, 0.22f, 0.38f, 1.0f };
	Vector4 reflectableObjectColor = { 1.0f, 1.0f, 0.0f, 1.0f };
	Vector4 startColor = { 1.0f, 0.98f, 0.78f, 1.0f };
	Vector4 playerEndColor = { 1.0f, 0.55f, 0.20f, 0.0f };
	Vector4 enemyEndColor = { 1.0f, 0.20f, 0.36f, 0.0f };
	Vector4 reflectableEndColor = { 1.0f, 1.0f, 0.22f, 0.0f };
};

class Bullet : public Collider {

public:

	void Initialize(const Vector3& position, const Vector3& velocity, const uint32_t& damage, BulletOwner owner,
		bool reflectable);

	void Update(float deltaTime);

	void Draw();
	void AttachTrail(TrailManager* trailManager, BulletTrailSettings* trailSettings);
	void ReleaseTrail();

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
	void ApplyVisualSettings();
	void UpdateTrail(float deltaTime);
	TrailConfig MakeTrailConfig() const;
	Vector4 GetBulletColor() const;

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
	TrailInstance* trail_ = nullptr;
	BulletTrailSettings* trailSettings_ = nullptr;
};
