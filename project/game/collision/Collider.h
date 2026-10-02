#pragma once
#include "Struct.h"
#include <atomic>
#include <cmath>

// 判定図形
enum class ColliderShape { Sphere, Capsule };

class Collider {

public:
	virtual ~Collider() = default;
	uint64_t GetCollisionId() const { return identity_.value; }
	// 半径を取得
	virtual float GetRadius() const { return radius_; }

	// 半径を設定
	void SetRadius(float radius) { if (std::isfinite(radius) && radius >= 0.0f) radius_ = radius; }

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
		if (!std::isfinite(r) || r < 0.0f) return;
		segment_ = seg;
		capsuleRadius_ = r;
	}
	const Segment& GetSegment() const { return segment_; }
	float GetCapsuleRadius() const { return capsuleRadius_; }

	float GetHitPower() const { return hitPower_; }
	void SetHitPower(float power) { hitPower_ = power; }

	uint32_t GetDamage() const { return damage_; };
	void SetDamage(const uint32_t& damage) { damage_ = damage; }

private:
	// Actor allocations may reuse an address while a piercing projectile lives.
	// Copies get a new identity; assigning state does not change an actor's ID.
	struct Identity {
		static inline std::atomic<uint64_t> next{1};
		uint64_t value = next.fetch_add(1, std::memory_order_relaxed);
		Identity() = default;
		Identity(const Identity&) : Identity() {}
		Identity& operator=(const Identity&) { return *this; }
	};
	Identity identity_{};
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
	uint32_t damage_ = 0;

};
