#include "ExpEnemy.h"
#include "Enemy.h"
#include "Player.h"
#include "ParticleManager.h"
#include "Stage.h"
#include <algorithm>
#include <cmath>

namespace {
Vector4 LerpColor(const Vector4& a, const Vector4& b, float t)
{
    t = (std::clamp)(t, 0.0f, 1.0f);
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    };
}
}

ExpEnemy::BalanceConfig ExpEnemy::balanceConfig_{};
ExpEnemy::EnemyInteractionConfig ExpEnemy::enemyInteractionConfig_{};
std::function<void(uint32_t)> ExpEnemy::enemyKillCallback_{};
bool ExpEnemy::shapeNeonBillboardEnabled_ = false;
int ExpEnemy::shapeNeonRenderMode_ = 0;

void ExpEnemy::SetBalanceConfig(const BalanceConfig& config)
{
    balanceConfig_.contactDamage = (std::max)(1u, config.contactDamage);
    balanceConfig_.shooterContactDamage = (std::max)(1u, config.shooterContactDamage);
    balanceConfig_.shooterBulletDamage = (std::max)(1u, config.shooterBulletDamage);
	balanceConfig_.shooterDetectionRadius = (std::max)(1.0f, config.shooterDetectionRadius);
	balanceConfig_.shooterTurnSpeed = (std::max)(0.1f, config.shooterTurnSpeed);
	balanceConfig_.shooterFireInterval = (std::max)(0.1f, config.shooterFireInterval);
	balanceConfig_.shooterBulletSpeed = (std::max)(0.01f, config.shooterBulletSpeed);
}

void ExpEnemy::SetEnemyInteractionConfig(const EnemyInteractionConfig& config)
{
    enemyInteractionConfig_ = config;
}

void ExpEnemy::SetEnemyKillCallback(std::function<void(uint32_t)> callback)
{
    enemyKillCallback_ = std::move(callback);
}

void ExpEnemy::SetShapeNeonBillboardEnabled(bool enabled)
{
    shapeNeonBillboardEnabled_ = enabled;
    shapeNeonRenderMode_ = enabled ? 1 : 0;
}

void ExpEnemy::SetShapeNeonRenderMode(int mode)
{
    shapeNeonRenderMode_ = (std::clamp)(mode, 0, 3);
    shapeNeonBillboardEnabled_ = shapeNeonRenderMode_ != 0;
}

bool ExpEnemy::IsShapeNeonBillboardTarget() const
{
    return type_ == ExpEnemyType::Square ||
        type_ == ExpEnemyType::Triangle ||
        type_ == ExpEnemyType::Pentagon ||
        type_ == ExpEnemyType::Shooter;
}

void ExpEnemy::Initialize(const Vector3& position, Player* player, ExpEnemyType type)
{
    object_ = std::make_unique<Object3d>();
    object_->Initialize();

    worldTransform_ = InitWorldTransform();
    worldTransform_.translate = position;
    worldTransform_.scale = Vector3(1.0f, 1.0f, 1.0f);
    baseScale_ = worldTransform_.scale;

    type_ = type;
    ApplyTypeParams();

    // 衝突属性を設定
    SetCollisionAttribute(kCollisionAttributeExpEnemy);
    RefreshCollisionMask();

    player_ = player;

    object_->SetTransform(worldTransform_);
    object_->Update();

}

void ExpEnemy::RefreshCollisionMask()
{
    uint32_t mask = kCollisionAttributePlayer | kCollisionAttributePlayerBullet | kCollisionAttributePlayerDrone;
    if (enemyInteractionConfig_.hostileToBoss) {
        mask |= kCollisionAttributeEnemy;
    }
    SetCollisionMask(mask);
}

void ExpEnemy::ApplyTypeParams()
{
    switch (type_) {
    case ExpEnemyType::Square:
        object_->SetModel("expBlock.obj");
        baseColor_ = { 1.0f, 0.86f, 0.20f, 1.0f };
        hp_ = 6;
        expValue_ = 8;
        shootInterval_ = 0.0f;
        break;
    case ExpEnemyType::Triangle:
        object_->SetModel("expTriangle.obj");
        baseColor_ = { 1.0f, 0.30f, 0.35f, 1.0f };
        hp_ = 10;
        expValue_ = 14;
        shootInterval_ = 0.0f;
        break;
    case ExpEnemyType::Pentagon:
        object_->SetModel("expPentagon.obj");
        baseColor_ = { 0.35f, 0.48f, 1.0f, 1.0f };
        hp_ = 24;
        expValue_ = 35;
        shootInterval_ = 0.0f;
        break;
    case ExpEnemyType::Shooter:
        object_->SetModel("expEnemy.obj");
        baseColor_ = { 0.95f, 0.25f, 0.18f, 1.0f };
        hp_ = 14;
        expValue_ = 22;
        shootInterval_ = balanceConfig_.shooterFireInterval;
        break;
    }
    object_->SetColor(baseColor_);
    visualColor_ = baseColor_;
    maxHp_ = hp_;
    SetDamage(type_ == ExpEnemyType::Shooter ? balanceConfig_.shooterContactDamage : balanceConfig_.contactDamage);
}

void ExpEnemy::Update(Stage& stage, float deltaTime) {

    // 座標を移動させる
    //worldTransform_.translate += velocity_ * (deltaTime * 60.0f);

    dt_ = deltaTime;

    invincibleTimer_ -= deltaTime;
    ApplyDamageFeedback(deltaTime);

    // --- 慣性処理 ---
    velocity_ += (velocity_ * -1.0f) * 0.98f * deltaTime;

    float timeWeight = deltaTime * 60.0f;

    // X移動
    Vector3 pos = GetWorldPosition();
    pos.x += velocity_.x * timeWeight;
    SetWorldPosition(pos);
    stage.ResolveExpEnemyCollision(*this, X);

    // Y移動
    pos = GetWorldPosition();
    pos.y += velocity_.y * timeWeight;
    SetWorldPosition(pos);
    stage.ResolveExpEnemyCollision(*this, Y);

	if (type_ != ExpEnemyType::Shooter) {
		worldTransform_.rotate.y += 0.2f * deltaTime;
		worldTransform_.rotate.x += 0.2f * deltaTime;
		worldTransform_.rotate.z += 0.2f * deltaTime;
		object_->SetTransform(worldTransform_);
		object_->Update();
		return;
	}
	shootInterval_ = balanceConfig_.shooterFireInterval;

	const Vector3 origin = GetWorldPosition();
	Vector3 targetPosition{};
	BulletOwner bulletOwner = BulletOwner::kEnemy;
	float nearestDistance = balanceConfig_.shooterDetectionRadius;
	bool hasTarget = false;
	if (player_ && !player_->IsDead()) {
		const float distance = Length(player_->GetWorldPosition() - origin);
		if (distance <= nearestDistance) {
			targetPosition = player_->GetWorldPosition();
			nearestDistance = distance;
			hasTarget = true;
		}
	}
	if (IsHostileToBoss() && boss_ && !boss_->IsDead()) {
		const float distance = Length(boss_->GetWorldPosition() - origin);
		if (distance < nearestDistance) {
			targetPosition = boss_->GetWorldPosition();
			nearestDistance = distance;
			bulletOwner = BulletOwner::kExpEnemyHostile;
			hasTarget = true;
		}
	}

	auto hasLineOfSight = [&](const Vector3& target) {
		Segment ray{};
		ray.origin = origin;
		ray.diff = target - origin;
		const float targetDistance = Length(ray.diff);
		for (const auto& row : stage.GetBlocks()) {
			for (const Block& block : row) {
				if (!block.isActive || !IsCollision(block.aabb, ray)) {
					continue;
				}
				const Vector3 blockCenter = (block.aabb.max + block.aabb.min) / 2.0f;
				if (Length(blockCenter - origin) < targetDistance) {
					return false;
				}
			}
		}
		return true;
	};

	if (hasTarget) {
		const Vector3 desiredDirection = Normalize(targetPosition - origin);
		const float turnT = (std::clamp)(balanceConfig_.shooterTurnSpeed * deltaTime, 0.0f, 1.0f);
		aimDirection_ += (desiredDirection - aimDirection_) * turnT;
		if (Length(aimDirection_) > 0.001f) {
			aimDirection_ = Normalize(aimDirection_);
		}
		worldTransform_.rotate = { 0.0f, 0.0f, std::atan2(aimDirection_.x, -aimDirection_.y) };
		bulletCoolTime -= deltaTime;
		if (bulletCoolTime <= 0.0f && hasLineOfSight(targetPosition)) {
			AttackParam param{};
			param.bulletSpeed = balanceConfig_.shooterBulletSpeed;
			param.bulletCount = 1;
			param.spreadAngleDeg = 3.0f;
			param.randomSpread = true;
			param.damage = balanceConfig_.shooterBulletDamage;
			attackController_.FireFromMuzzle(origin + aimDirection_ * 1.4f, aimDirection_, param, bulletOwner);
			bulletCoolTime = shootInterval_;
		}
	} else {
		bulletCoolTime = (std::min)(bulletCoolTime, shootInterval_);
	}

	object_->SetTransform(worldTransform_);
	object_->Update();
}

void ExpEnemy::Draw(bool drawBody) {
    if (!drawBody) {
        return;
    }
    DrawBodyOnly();
}

void ExpEnemy::DrawBodyOnly() {
    if (shapeNeonRenderMode_ != 0 && IsShapeNeonRenderTarget()) {
        return;
    }
    object_->Draw();
}

void ExpEnemy::DrawNeonFillBodyOnly() {
    if (shapeNeonRenderMode_ != 3 || !IsShapeNeonRenderTarget()) {
        return;
    }

    const Vector4 savedColor = object_->GetColor();
    const bool savedLighting = object_->IsLightingEnabled();
    const float savedEnvironmentCoefficient = object_->GetEnvironmentCoefficient();

    object_->SetLighting(false);
    object_->SetEnvironmentCoefficient(0.0f);
    object_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
    object_->Draw();

    object_->SetColor(savedColor);
    object_->SetLighting(savedLighting);
    object_->SetEnvironmentCoefficient(savedEnvironmentCoefficient);
}

void ExpEnemy::OnCollision(Collider* other)
{
    Vector3 hitDir =
        worldTransform_.translate - other->GetWorldPosition();

    if (Length(hitDir) < 0.0001f) {
        hitDir = { 1.0f, 0.0f, 0.0f };
    }
    hitDir = Normalize(hitDir);

    const float kKnockBackPower = 0.05f;

    velocity_ += hitDir * kKnockBackPower * other->GetHitPower() * (dt_ * 60.0f);
    
    if (other->GetCollisionAttribute() == kCollisionAttributeEnemy) {
        return;
    }

    const bool canTakeDamage =
        other->GetCollisionAttribute() == kCollisionAttributePlayer ||
        other->GetCollisionAttribute() == kCollisionAttributePlayerBullet ||
        other->GetCollisionAttribute() == kCollisionAttributePlayerDrone ||
        (other->GetCollisionAttribute() == kCollisionAttributeEnemyBullet && IsHostileToBoss());
    if (!canTakeDamage) {
        return;
    }

    if (other->GetCollisionAttribute() == kCollisionAttributePlayer && invincibleTimer_ > 0.0f) {
        return;
    }

    const bool killedByEnemy =
        other->GetCollisionAttribute() == kCollisionAttributeEnemyBullet && IsHostileToBoss();

    hp_ -= other->GetDamage();
    TriggerDamageFeedback();
    if (hp_ <= 0) {
        isDead_ = true;
        ParticleManager::GetInstance()->EmitNeonDeathEffect(
            GetWorldPosition(),
            { 1.20f, 0.32f, 1.35f, 1.0f },
            { 0.18f, 1.10f, 1.35f, 0.0f },
            0.32f);
        if (killedByEnemy) {
            if (enemyKillCallback_) {
                enemyKillCallback_(expValue_);
            }
        } else if (player_) {
            player_->AddExp(expValue_);
        }
    }

    if (other->GetCollisionAttribute() == kCollisionAttributePlayer) {
        invincibleTimer_ = 0.5f;
    }
}

bool ExpEnemy::TakeDamageFromEnemy(uint32_t amount)
{
    if (isDead_ || amount == 0) {
        return false;
    }

    hp_ -= static_cast<int>(amount);
    TriggerDamageFeedback();
    if (hp_ <= 0) {
        isDead_ = true;
        ParticleManager::GetInstance()->EmitNeonDeathEffect(
            GetWorldPosition(),
            { 1.20f, 0.32f, 1.35f, 1.0f },
            { 0.18f, 1.10f, 1.35f, 0.0f },
            0.32f);
        return true;
    }
    return false;
}

bool ExpEnemy::TakeDamageFromPlayer(uint32_t amount)
{
    if (isDead_ || amount == 0) {
        return false;
    }

    hp_ -= static_cast<int>(amount);
    TriggerDamageFeedback();
    if (hp_ <= 0) {
        isDead_ = true;
        ParticleManager::GetInstance()->EmitNeonDeathEffect(
            GetWorldPosition(),
            { 1.20f, 0.32f, 1.35f, 1.0f },
            { 0.18f, 1.10f, 1.35f, 0.0f },
            0.32f);
        if (player_) {
            player_->AddExp(expValue_);
        }
        return true;
    }
    return false;
}

void ExpEnemy::TriggerDamageFeedback()
{
    damageFeedbackTimer_ = damageFeedbackDuration_;
}

void ExpEnemy::ApplyDamageFeedback(float deltaTime)
{
    if (damageFeedbackTimer_ > 0.0f) {
        damageFeedbackTimer_ = (std::max)(0.0f, damageFeedbackTimer_ - deltaTime);
    }

    const float t = damageFeedbackDuration_ > 0.0f ? damageFeedbackTimer_ / damageFeedbackDuration_ : 0.0f;
    const float flash = t * t;
    const float pulse = std::sin(t * 3.14159265f) * 0.16f;
    worldTransform_.scale = baseScale_ * (1.0f + pulse);
    visualColor_ = LerpColor(baseColor_, { 1.0f, 1.0f, 1.0f, baseColor_.w }, flash * 0.9f);
    object_->SetColor(visualColor_);
}

AABB ExpEnemy::GetAABB() {
    Vector3 worldPos = GetWorldPosition();

    AABB aabb;

    aabb.min = { worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f };
    aabb.max = { worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f };

    return aabb;
}
