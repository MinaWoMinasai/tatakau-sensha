#pragma once
#include "Struct.h"

// 判定図形
enum class ColliderShape { Sphere, Capsule };

class Collider {

public:
	// 半径を取得
	virtual float GetRadius() const { return radius_; }

	// 半径を設定
	void SetRadius(float radius) { radius_ = radius; }

	/// <summary>
	/// ワールド座標の取得
	/// </summary>
	virtual Vector3 GetWorldPosition() const = 0;

	/// <summary>
	/// 衝突判定
	/// </summary>
	virtual void OnCollision(Collider* other) = 0;

	// 衝突属性(自分)を取得
	uint32_t GetCollisionAttribute() const { return collisionAttribute_; }
	// 衝突属性(自分)を設定
	void SetCollisionAttribute(uint32_t attribute) { collisionAttribute_ = attribute; }
	// 衝突マスク(相手)を取得
	uint32_t GetCollisionMask() const { return collisionMask_; }
	// 衝突マスク(相手)を設定
	void SetCollisionMask(uint32_t mask) { collisionMask_ = mask; }

	void SetIsGard(bool flag) { isAttack_ = flag; }
	bool GetIsAttack() { return isAttack_; }

	// 図形セット
	ColliderShape GetShape() const { return shape_; }
	void SetShape(ColliderShape shape) { shape_ = shape; }

	void SetCapsule(const Segment& seg, float r) {
		segment_ = seg;
		capsuleRadius_ = r;
	}
	const Segment& GetSegment() const { return segment_; }
	float GetCapsuleRadius() const { return capsuleRadius_; }

	float GetHitPower() const { return hitPower_; }
	void SetHitPower(float power) { hitPower_ = power; }

	uint32_t GetDamage() { return damage_; };
	void SetDamage(const uint32_t& damage) { damage_ = damage; }

private:
	// 衝突半径
	float radius_ = 0.8f;

	// 衝突属性
	uint32_t collisionAttribute_ = 0xffffffff;
	// 衝突マスク
	uint32_t collisionMask_ = 0xffffffff;

	bool isAttack_ = false;

	// 衝突図形
	ColliderShape shape_ = ColliderShape::Sphere;

	Segment segment_;
	float capsuleRadius_ = 0.0f;
	float hitPower_ = 1.0f;

	// 与えるダメージ
	uint32_t damage_;

};
