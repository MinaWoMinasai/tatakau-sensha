#pragma once
#include <Windows.h>
#include <algorithm>
#include <vector>
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
	float widthCurvePower = 1.0f;
	float colorCurvePower = 1.0f;
	bool useObjectColorForTrail = true;
	float trailHeadIntensity = 1.15f;
	float trailTailIntensity = 0.45f;
	float trailHeadAlpha = 1.0f;
	float trailTailAlpha = 0.0f;
	float playerTrailLifetimeScale = 1.0f;
	float playerTrailAlphaScale = 1.0f;
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
	enum class SpecialKind { None, Rail, SlashWave, ParryReflection };
	struct SpecialImpact { SpecialKind kind=SpecialKind::None;Vector3 position{},direction{};bool bulletCut=false; };
	std::vector<SpecialImpact> ConsumeSpecialImpacts() {auto events=std::move(specialImpacts_);specialImpacts_.clear();return events;}
	void ConfigureSpecial(SpecialKind kind, float radius, float lifetime);
	SpecialKind GetSpecialKind() const { return specialKind_; }
	const Vector3& GetPreviousWorldPosition() const { return previousPosition_; }
	struct GrowthEvents {
		uint32_t wallBounces = 0;
		uint32_t actorPierces = 0;
	};

	void Initialize(const Vector3& position, const Vector3& velocity, const uint32_t& damage, BulletOwner owner,
		bool reflectable, float bulletHp = 1.0f, float bulletPenetration = 1.0f);

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

	Vector3 GetMove() const { return velocity_; }

	// セッター
	void SetWorldPosition(const Vector3& pos) {
		worldTransform_.translate = pos;
		object_->SetTransform(worldTransform_);
		object_->Update();
	}

	void SetVelocity(const Vector3& v) { velocity_ = v; }

	float GetRadius() const override { return radius_; }

	bool IsReflectable() const { return isReflectable_; }
	BulletOwner GetOwner() const { return owner_; }
	float GetBulletHp() const { return bulletHp_; }
	float GetBulletPenetration() const { return bulletPenetration_; }
	bool CanClaimRunResource() const { return canClaimRunResource_; }
	void SetCanClaimRunResource(bool enabled) { canClaimRunResource_ = enabled; }
	void ApplyBulletDurabilityDamage(float amount);
	void ConfigureGrowth(int maxWallBounces, int actorPierceCount, int impactSplitCount,
		float impactSplitDamageScale = 0.55f);
	bool UsesRunProjectileRules() const { return usesRunProjectileRules_; }
	bool CanHitActor(const Collider* actor) const;
	void OnWallImpact(const Vector3& safePosition, const Vector3& normal);
	// Drain only outside collision iteration. Children cannot split again and
	// share the parent's expiry time and actor hit history.
	void AppendImpactChildren(std::vector<std::unique_ptr<Bullet>>& children, size_t availableSlots);
	int GetRemainingWallBounces() const { return remainingWallBounces_; }
	int GetRemainingActorPierces() const { return remainingActorPierces_; }
	float GetRemainingLifetime() const { return deathTimer_; }
	GrowthEvents ConsumeGrowthEvents() { const auto events = growthEvents_; growthEvents_ = {}; return events; }

	void Die();

private:
	void ApplyVisualSettings();
	void UpdateTrail(float deltaTime);
	TrailConfig MakeTrailConfig() const;
	Vector4 GetBulletColor() const;
	void QueueImpactSplit(const Vector3& direction);

	std::unique_ptr<Object3d> object_;

	// ワールドトランスフォーム
	Transform worldTransform_;

	// 速度
	Vector3 velocity_;
	Vector3 previousPosition_{};
	SpecialKind specialKind_ = SpecialKind::None;
	std::vector<SpecialImpact> specialImpacts_;

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
	float bulletHp_ = 1.0f;
	float bulletPenetration_ = 1.0f;
	bool canClaimRunResource_ = true;
	bool usesRunProjectileRules_ = false;
	int remainingWallBounces_ = -1;
	int remainingActorPierces_ = 0;
	int impactSplitCount_ = 0;
	int pendingImpactSplitCount_ = 0;
	float impactSplitDamageScale_ = 0.55f;
	Vector3 pendingImpactDirection_{};
	Vector3 pendingImpactPosition_{};
	Vector3 pendingImpactWallNormal_{};
	std::vector<uint64_t> hitActorIds_;
	GrowthEvents growthEvents_{};
	TrailInstance* trail_ = nullptr;
	BulletTrailSettings* trailSettings_ = nullptr;
};
