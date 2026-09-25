#include "ExpEnemy.h"
#include "Enemy.h"
#include "Player.h"
#include "ParticleManager.h"
#include "Stage.h"
#include "NeonGridRenderer.h"
#include "ExpEnemyNavigation.h"
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
std::function<void(const Vector3&)> ExpEnemy::playerDefeatCallback_{};
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

void ExpEnemy::SetPlayerDefeatCallback(std::function<void(const Vector3&)> callback)
{
	playerDefeatCallback_ = std::move(callback);
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
        type_ == ExpEnemyType::Shooter || IsExpeditionCombatRole();
}

void ExpEnemy::Initialize(const Vector3& position, Player* player, ExpEnemyType type)
{
    hasAuthoredDefinition_=false;authoredMoveSpeedScale_=authoredFireIntervalScale_=1;
    isDead_ = false;
    isRunResource_ = false;
    runResourceClaimCallback_ = {};
    object_ = std::make_unique<Object3d>();
    object_->Initialize();

    worldTransform_ = InitWorldTransform();
    worldTransform_.translate = position;
    worldTransform_.scale = Vector3(1.0f, 1.0f, 1.0f);
    baseScale_ = worldTransform_.scale;

    type_ = type;
    ApplyTypeParams();
    if (IsExpeditionCombatRole()) {
        ExpEnemyCombatCycle::Timing timing{};
        if (type_ == ExpEnemyType::Sniper) {
            timing = { 0.85f, 0.38f, 0.10f, 1.50f, 0.65f };
        }
        // Authored spawn positions deterministically stagger attack openings.
        const float stagger = std::fmod(std::abs(position.x * 0.173f + position.y * 0.319f), 0.85f);
        combatCycle_.Reset(timing, 0.55f + stagger);
        telegraphEnd_ = position;
        object_->SetLighting(false);
    }

    // 衝突属性を設定
    SetCollisionAttribute(kCollisionAttributeExpEnemy);
    RefreshCollisionMask();

    player_ = player;

    object_->SetTransform(worldTransform_);
    object_->Update();

}

void ExpEnemy::ApplyAuthoredDefinition(const tankcontent::Enemy& definition)
{
    hasAuthoredDefinition_=true;
    hp_=maxHp_=(std::clamp)(definition.hp,1,9999);
    authoredContactDamage_=static_cast<uint32_t>((std::clamp)(definition.contactDamage,0,999));
    authoredBulletDamage_=static_cast<uint32_t>((std::clamp)(definition.bulletDamage,1,999));
    authoredMoveSpeedScale_=(std::clamp)(definition.moveSpeedScale,0.1f,3.0f);
    authoredFireIntervalScale_=(std::clamp)(definition.fireIntervalScale,0.3f,4.0f);
    expValue_=static_cast<uint32_t>((std::clamp)(definition.creditDrop,0,999)*5);
    baseColor_={definition.color[0],definition.color[1],definition.color[2],definition.color[3]};
    visualColor_=baseColor_;object_->SetColor(baseColor_);SetDamage(authoredContactDamage_);
}

void ExpEnemy::RefreshCollisionMask()
{
    uint32_t mask = kCollisionAttributePlayer | kCollisionAttributePlayerBullet | kCollisionAttributePlayerDrone;
    if (enemyInteractionConfig_.hostileToBoss || isRunResource_) {
        mask |= kCollisionAttributeEnemy;
    }
    if (isRunResource_) mask |= kCollisionAttributeEnemyBullet;
    SetCollisionMask(mask);
}

void ExpEnemy::SetRunResource(std::function<void(bool playerOwned)> onClaim)
{
    isRunResource_ = true;
    runResourceClaimCallback_ = std::move(onClaim);
    velocity_ = {};
    baseScale_ = { 1.5f, 1.5f, 1.5f };
    worldTransform_.scale = baseScale_;
    baseColor_ = { 1.0f, 0.74f, 0.20f, 1.0f };
    visualColor_ = baseColor_;
    damageFeedbackDuration_ = 0.14f;
    SetDamage(0);
    RefreshCollisionMask();
    object_->SetColor(visualColor_);
    object_->SetTransform(worldTransform_);
    object_->Update();
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
    case ExpEnemyType::Charger:
        object_->SetModel("expTriangle.obj");
        baseColor_ = { 1.60f, 0.36f, 0.10f, 1.0f };
        baseScale_ = { 1.05f, 1.05f, 1.05f };
        hp_ = 30;
        expValue_ = 28;
        break;
    case ExpEnemyType::Sniper:
        object_->SetModel("expEnemy.obj");
        baseColor_ = { 1.40f, 0.16f, 0.72f, 1.0f };
        baseScale_ = { 0.90f, 1.10f, 0.90f };
        hp_ = 24;
        expValue_ = 30;
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

    if (isRunResource_) {
        // Cores stay at their marked contest point even when struck or rammed.
        velocity_ = {};
        worldTransform_.rotate.z += 0.35f * deltaTime;
        object_->SetTransform(worldTransform_);
        object_->Update();
        return;
    }
    if (IsExpeditionCombatRole()) {
        UpdateExpeditionCombat(stage, deltaTime);
        object_->SetTransform(worldTransform_);
        object_->Update();
        return;
    }

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
	shootInterval_ = balanceConfig_.shooterFireInterval * authoredFireIntervalScale_;
	shooterMuzzleFlashTimer_ = (std::max)(0.0f, shooterMuzzleFlashTimer_ - deltaTime);

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
	if (IsHostileToBoss() && boss_ && boss_->IsRunEncounterEnabled() && !boss_->IsDead()) {
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

	const bool canSeeTarget = hasTarget && hasLineOfSight(targetPosition);
	if (hasTarget) {
		const Vector3 desiredDirection = Normalize(targetPosition - origin);
		const float turnT = (std::clamp)(balanceConfig_.shooterTurnSpeed * deltaTime, 0.0f, 1.0f);
		aimDirection_ += (desiredDirection - aimDirection_) * turnT;
		if (Length(aimDirection_) > 0.001f) {
			aimDirection_ = Normalize(aimDirection_);
		}
		worldTransform_.rotate = { 0.0f, 0.0f, std::atan2(aimDirection_.x, -aimDirection_.y) };
	}

	if (canSeeTarget) {
		if (!shooterHadVisibleTarget_ && bulletCoolTime <= 0.0f) {
			bulletCoolTime = kShooterWarningDuration;
		}
		shooterHadVisibleTarget_ = true;
		bulletCoolTime -= deltaTime;
		if (bulletCoolTime > 0.0f) {
			shooterWarningRatio_ = bulletCoolTime <= kShooterWarningDuration
				? (std::clamp)(1.0f - bulletCoolTime / kShooterWarningDuration, 0.0f, 1.0f)
				: 0.0f;
		} else {
			AttackParam param{};
			param.bulletSpeed = balanceConfig_.shooterBulletSpeed;
			param.bulletCount = 1;
			param.spreadAngleDeg = 3.0f;
			param.randomSpread = true;
			param.damage = hasAuthoredDefinition_ ? authoredBulletDamage_ : balanceConfig_.shooterBulletDamage;
			param.canClaimRunResource = false;
			attackController_.FireFromMuzzle(origin + aimDirection_ * 1.4f, aimDirection_, param, bulletOwner);
			bulletCoolTime = shootInterval_;
			shooterWarningRatio_ = 0.0f;
			shooterMuzzleFlashTimer_ = kShooterMuzzleFlashDuration;
		}
	} else {
		shooterHadVisibleTarget_ = false;
		shooterWarningRatio_ = 0.0f;
		bulletCoolTime = (std::max)(bulletCoolTime, kShooterWarningDuration);
	}

	object_->SetTransform(worldTransform_);
	object_->Update();
}

Vector3 ExpEnemy::ClipCombatRay(Stage& stage, const Vector3& origin, const Vector3& direction, float distance) const
{
    float nearest = distance;
    for (const auto& row : stage.GetBlocks()) {
        for (const Block& block : row) {
            if (!block.isActive) continue;
            float enter = 0.0f;
            float leave = nearest;
            auto clipAxis = [&](float start, float speed, float minimum, float maximum) {
                if (std::abs(speed) < 0.0001f) return start >= minimum && start <= maximum;
                float nearT = (minimum - start) / speed;
                float farT = (maximum - start) / speed;
                if (nearT > farT) std::swap(nearT, farT);
                enter = (std::max)(enter, nearT);
                leave = (std::min)(leave, farT);
                return enter <= leave;
            };
            if (clipAxis(origin.x, direction.x, block.aabb.min.x, block.aabb.max.x) &&
                clipAxis(origin.y, direction.y, block.aabb.min.y, block.aabb.max.y)) {
                nearest = (std::max)(0.0f, enter);
            }
        }
    }
    return origin + direction * nearest;
}

bool ExpEnemy::MoveCombatActor(Stage& stage, const Vector3& displacement)
{
    // Collision is discrete; small substeps prevent a dash from skipping thin walls.
    const int steps = (std::max)(1, static_cast<int>(std::ceil(Length(displacement) / 0.35f)));
    const Vector3 step = displacement / static_cast<float>(steps);
    bool blocked = false;
    for (int i = 0; i < steps; ++i) {
        const Vector3 before = GetWorldPosition();
        Vector3 desired = before;
        desired.x += step.x;
        SetWorldPosition(desired);
        stage.ResolveExpEnemyCollision(*this, X);
        blocked = blocked || std::abs(GetWorldPosition().x - desired.x) > 0.005f;
        desired = GetWorldPosition();
        desired.y += step.y;
        SetWorldPosition(desired);
        stage.ResolveExpEnemyCollision(*this, Y);
        blocked = blocked || std::abs(GetWorldPosition().y - desired.y) > 0.005f;
        if (blocked && combatCycle_.GetPhase() == ExpEnemyCombatPhase::Active) break;
    }
    return blocked;
}

Vector3 ExpEnemy::FindCombatWaypoint(Stage& stage, const Vector3& target) const
{
    constexpr int width = MapChip::kNumBlockHorizontal;
    constexpr int height = MapChip::kNumBlockVirtical;
    constexpr float cellSize = 2.0f;
    std::array<bool, width * height> blocked{};
    for (const auto& row : stage.GetBlocks()) {
        for (const Block& block : row) {
            if (!block.isActive) continue;
            // Inflate by the actor half-width, not the point-sized targeting ray.
            const int minX = (std::max)(0, static_cast<int>(std::ceil((block.aabb.min.x - 0.85f) / cellSize)));
            const int maxX = (std::min)(width - 1, static_cast<int>(std::floor((block.aabb.max.x + 0.85f) / cellSize)));
            const int minY = (std::max)(0, static_cast<int>(std::ceil((block.aabb.min.y - 0.85f) / cellSize)));
            const int maxY = (std::min)(height - 1, static_cast<int>(std::floor((block.aabb.max.y + 0.85f) / cellSize)));
            for (int y = minY; y <= maxY; ++y) for (int x = minX; x <= maxX; ++x) blocked[y * width + x] = true;
        }
    }
    auto cell = [&](const Vector3& position) {
        const int x = (std::clamp)(static_cast<int>(std::round(position.x / cellSize)), 0, width - 1);
        const int y = (std::clamp)(static_cast<int>(std::round(position.y / cellSize)), 0, height - 1);
        return y * width + x;
    };
    const int next = FindExpEnemyNextCell<width, height>(blocked, cell(GetWorldPosition()), cell(target));
    return next >= 0 ? Vector3{ static_cast<float>(next % width) * cellSize,
        static_cast<float>(next / width) * cellSize, 0.0f } : GetWorldPosition();
}

void ExpEnemy::UpdateExpeditionCombat(Stage& stage, float deltaTime)
{
    if (isDead_) return;
    const float dt = (std::clamp)(deltaTime, 0.0f, 0.20f);
    combatRepathTimer_ -= dt;
    shooterMuzzleFlashTimer_ = (std::max)(0.0f, shooterMuzzleFlashTimer_ - dt);
    const Vector3 origin = GetWorldPosition();
    Vector3 toTarget{};
    float distance = 0.0f;
    const bool hasTarget = player_ && !player_->IsDead();
    if (hasTarget) {
        toTarget = player_->GetWorldPosition() - origin;
        toTarget.z = 0.0f;
        distance = Length(toTarget);
    }
    const Vector3 desiredDirection = distance > 0.001f ? toTarget / distance : aimDirection_;
    if (hasTarget && !combatCycle_.IsAimLocked() && combatCycle_.GetPhase() != ExpEnemyCombatPhase::Recovery) {
        aimDirection_ = desiredDirection;
    }
    const bool visible = hasTarget && distance <= (type_ == ExpEnemyType::Sniper ? 64.0f : 38.0f) &&
        Length(ClipCombatRay(stage, origin, desiredDirection, distance) - origin) >= distance - 0.05f;
    const bool canAttack = visible && (type_ == ExpEnemyType::Sniper || distance < 13.0f);
    combatCycle_.SetRecoveryScale(balanceConfig_.shooterFireInterval * authoredFireIntervalScale_ / 1.6f);
    const bool attackStarted = combatCycle_.Advance(dt, canAttack);
    const ExpEnemyCombatPhase phase = combatCycle_.GetPhase();

    if (type_ == ExpEnemyType::Charger) {
        if (phase == ExpEnemyCombatPhase::Active) {
            velocity_ = {};
            if (MoveCombatActor(stage, aimDirection_ * (32.0f * authoredMoveSpeedScale_ * dt))) {
                combatCycle_.EnterRecovery();
                ParticleManager::GetInstance()->EmitNeonDeathEffect(
                    GetWorldPosition(), { 1.8f, 0.7f, 0.15f, 1.0f }, { 0.8f, 0.2f, 0.1f, 0.0f }, 0.12f);
            }
        } else if (phase == ExpEnemyCombatPhase::Cooldown && hasTarget && distance > 2.2f) {
            // Modest pursuit keeps a charger relevant between attacks. A cached
            // grid route prevents oscillation at concave cover and narrow gaps.
            Vector3 moveDirection = desiredDirection;
            if (!visible || stage.IsCollisionWithAnyBlock(origin + moveDirection * 1.6f, 0.85f)) {
                if (combatRepathTimer_ <= 0.0f || Length(combatWaypoint_ - origin) < 0.30f) {
                    combatWaypoint_ = FindCombatWaypoint(stage, player_->GetWorldPosition());
                    combatRepathTimer_ = 0.35f;
                }
                const Vector3 toWaypoint = combatWaypoint_ - origin;
                moveDirection = Length(toWaypoint) > 0.10f ? Normalize(toWaypoint) : Vector3{};
            }
            MoveCombatActor(stage, moveDirection * (4.0f * authoredMoveSpeedScale_ * dt));
        }
        SetDamage(combatCycle_.GetPhase() == ExpEnemyCombatPhase::Recovery ? 0u :
            (hasAuthoredDefinition_ ? authoredContactDamage_ : balanceConfig_.contactDamage) *
            (combatCycle_.GetPhase() == ExpEnemyCombatPhase::Active ? 2u : 1u));
    } else {
        SetDamage(hasAuthoredDefinition_ ? authoredContactDamage_ : balanceConfig_.shooterContactDamage);
        if (attackStarted) {
            AttackParam param{};
            param.bulletSpeed = balanceConfig_.shooterBulletSpeed * 1.30f;
            param.bulletCount = 1;
            param.spreadAngleDeg = 0.0f;
            param.randomSpread = false;
            param.damage = hasAuthoredDefinition_ ? authoredBulletDamage_ : balanceConfig_.shooterBulletDamage;
            param.bulletHp = 32.0f;
            param.bulletPenetration = 1.0f;
            param.canClaimRunResource = false;
            // Stop the muzzle at nearby cover rather than spawning through it.
            const Vector3 muzzle = ClipCombatRay(stage, origin, aimDirection_, 1.35f);
            if (Length(muzzle - origin) >= 1.25f) {
                attackController_.FireFromMuzzle(muzzle, aimDirection_, param, BulletOwner::kEnemy);
                shooterMuzzleFlashTimer_ = 0.15f;
            }
        }
    }
    if (combatCycle_.GetPhase() == ExpEnemyCombatPhase::Locked) {
        // Commitment freezes both the direction and the ray origin. Repeated
        // hits must not sweep the locked warning sideways just before firing.
        velocity_ = {};
    } else if (Length(velocity_) > 0.001f && combatCycle_.GetPhase() != ExpEnemyCombatPhase::Active) {
        // Preserve hit reaction, but cap repeated pellets so the locked telegraph
        // cannot be swept across the arena by an accumulated knockback impulse.
        if (Length(velocity_) > 0.07f) velocity_ = Normalize(velocity_) * 0.07f;
        MoveCombatActor(stage, velocity_ * (dt * 60.0f));
        velocity_ = velocity_ * (std::max)(0.0f, 1.0f - dt * 9.0f);
    }
    worldTransform_.rotate = { 0.0f, 0.0f, std::atan2(aimDirection_.x, -aimDirection_.y) };
    telegraphEnd_ = ClipCombatRay(stage, GetWorldPosition(), aimDirection_,
        type_ == ExpEnemyType::Charger ? 13.44f : 64.0f);
    if (combatCycle_.GetPhase() == ExpEnemyCombatPhase::Recovery) {
        visualColor_ = LerpColor(visualColor_, { 0.25f, 0.70f, 0.85f, 1.0f }, 0.65f);
        object_->SetColor(visualColor_);
    }
}

void ExpEnemy::QueueCombatVisuals(NeonGridRenderer& renderer, const Vector3& cameraRight,
    const Vector3& cameraUp, const Vector3& cameraForward, float lineWidth) const
{
    if (!IsExpeditionCombatRole() || isDead_) return;
    const Vector3 center = GetWorldPosition() + Vector3{ 0.0f, 0.0f, 0.38f };
    const Vector3 forward = aimDirection_;
    const Vector3 side{ -forward.y, forward.x, 0.0f };
    const float width = (std::max)(0.045f, lineWidth);
    auto line = [&](const Vector3& a, const Vector3& b, float thickness, const Vector4& color) {
        renderer.QueueCameraFacingLine(a, b, thickness, color, cameraForward);
    };
    const float pulse = 1.0f + GetAttackTelegraphRatio() * 0.10f;
    if (type_ == ExpEnemyType::Charger) {
        const Vector3 nose = center + forward * (1.18f * pulse);
        const Vector3 left = center - forward * (0.72f * pulse) + side * (0.82f * pulse);
        const Vector3 right = center - forward * (0.72f * pulse) - side * (0.82f * pulse);
        line(nose, left, width, visualColor_);
        line(left, right, width, visualColor_);
        line(right, nose, width, visualColor_);
        line(center - side * 0.42f, center + forward * 0.48f, width * 0.80f, visualColor_);
        line(center + forward * 0.48f, center + side * 0.42f, width * 0.80f, visualColor_);
    } else {
        constexpr float pi = 3.14159265f;
        for (int i = 0; i < 6; ++i) {
            const float a = static_cast<float>(i) * pi / 3.0f;
            const float b = static_cast<float>(i + 1) * pi / 3.0f;
            line(center + (side * std::cos(a) + forward * std::sin(a)) * (0.82f * pulse),
                center + (side * std::cos(b) + forward * std::sin(b)) * (0.82f * pulse), width, visualColor_);
        }
        line(center + side * 0.20f, center + forward * 1.35f + side * 0.12f, width, visualColor_);
        line(center - side * 0.20f, center + forward * 1.35f - side * 0.12f, width, visualColor_);
        line(center - side * 0.36f, center + side * 0.36f, width, visualColor_);
    }
    const ExpEnemyCombatPhase phase = combatCycle_.GetPhase();
    if (phase == ExpEnemyCombatPhase::Tracking || phase == ExpEnemyCombatPhase::Locked) {
        const bool locked = phase == ExpEnemyCombatPhase::Locked;
        const Vector4 warning = locked ? Vector4{ 2.1f, 1.45f, 0.75f, 0.95f } :
            type_ == ExpEnemyType::Sniper ? Vector4{ 1.8f, 0.20f, 0.62f, 0.52f } : Vector4{ 1.8f, 0.50f, 0.12f, 0.52f };
        const float distance = Length(telegraphEnd_ - GetWorldPosition());
        const float start = (std::min)(1.5f, distance);
        if (type_ == ExpEnemyType::Sniper) {
            for (float t = start; t < distance; t += locked ? 1.6f : 2.0f) {
                line(center + forward * t, center + forward * (std::min)(distance, t + (locked ? 1.30f : 0.85f)),
                    locked ? width * 0.85f : width * 0.60f, warning);
            }
        } else {
            for (float t = start; t < distance; t += 2.0f) {
                const Vector3 marker = center + forward * t;
                line(marker - forward * 0.50f + side * 0.40f, marker, width * 0.80f, warning);
                line(marker - forward * 0.50f - side * 0.40f, marker, width * 0.80f, warning);
            }
        }
        const Vector3 end = center + forward * distance;
        line(end - side * 0.55f, end + side * 0.55f, width, warning);
    }
    if (phase == ExpEnemyCombatPhase::Recovery) {
        const Vector4 recovery{ 0.25f, 1.0f, 1.2f, 0.75f };
        const float radius = 1.15f + combatCycle_.GetRecoveryRatio() * 0.15f;
        line(center + cameraRight * radius - cameraUp * 0.25f, center + cameraRight * radius + cameraUp * 0.25f, width, recovery);
        line(center - cameraRight * radius - cameraUp * 0.25f, center - cameraRight * radius + cameraUp * 0.25f, width, recovery);
    }
    if (shooterMuzzleFlashTimer_ > 0.0f) {
        const Vector3 muzzle = center + forward * 1.45f;
        const Vector4 flash{ 2.4f, 1.6f, 0.9f, 1.0f };
        line(muzzle - side * 0.35f, muzzle + side * 0.35f, width * 1.5f, flash);
        line(muzzle - forward * 0.28f, muzzle + forward * 0.28f, width * 1.5f, flash);
    }
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
    // Collision pairs are collected before processing. A later projectile in
    // the same frame must not award a second kill for this defeated resource.
    if (isDead_) {
        return;
    }
    const uint32_t otherAttribute = other->GetCollisionAttribute();
    if (isRunResource_ && (otherAttribute == kCollisionAttributePlayer || otherAttribute == kCollisionAttributeEnemy ||
        otherAttribute == kCollisionAttributePlayerDrone)) {
        return;
    }
    if (isRunResource_) {
        const auto* bullet = dynamic_cast<const Bullet*>(other);
        if (bullet && !bullet->CanClaimRunResource()) return;
    }
    Vector3 hitDir =
        worldTransform_.translate - other->GetWorldPosition();

    if (Length(hitDir) < 0.0001f) {
        hitDir = { 1.0f, 0.0f, 0.0f };
    }
    hitDir = Normalize(hitDir);

    const float kKnockBackPower = 0.05f;

    if (!isRunResource_) velocity_ += hitDir * kKnockBackPower * other->GetHitPower() * (dt_ * 60.0f);
    if (type_ == ExpEnemyType::Charger && otherAttribute == kCollisionAttributePlayer &&
        combatCycle_.GetPhase() == ExpEnemyCombatPhase::Active) {
        combatCycle_.EnterRecovery();
        velocity_ = {};
        // Keep this frame's committed contact damage for the other collider's callback.
    }
    
    if (other->GetCollisionAttribute() == kCollisionAttributeEnemy) {
        return;
    }

    const bool canTakeDamage =
        other->GetCollisionAttribute() == kCollisionAttributePlayer ||
        other->GetCollisionAttribute() == kCollisionAttributePlayerBullet ||
        other->GetCollisionAttribute() == kCollisionAttributePlayerDrone ||
        (other->GetCollisionAttribute() == kCollisionAttributeEnemyBullet && (IsHostileToBoss() || isRunResource_));
    if (!canTakeDamage) {
        return;
    }

    if (other->GetCollisionAttribute() == kCollisionAttributePlayer && invincibleTimer_ > 0.0f) {
        return;
    }

    const bool killedByEnemy =
        other->GetCollisionAttribute() == kCollisionAttributeEnemyBullet && (IsHostileToBoss() || isRunResource_);
    ApplyDamage(other->GetDamage(), !killedByEnemy, true);

    if (other->GetCollisionAttribute() == kCollisionAttributePlayer) {
        invincibleTimer_ = 0.5f;
    }
}

bool ExpEnemy::TakeDamageFromEnemy(uint32_t amount)
{
    // The existing enemy contact caller awards ordinary feeding itself.
    return ApplyDamage(amount, false, false);
}

bool ExpEnemy::TakeDamageFromPlayer(uint32_t amount)
{
    return ApplyDamage(amount, true, false);
}

bool ExpEnemy::ApplyDamage(uint32_t amount, bool playerOwned, bool reportOrdinaryEnemyKill)
{
    if (isDead_ || amount == 0) {
        return false;
    }
    hp_ -= static_cast<int>((std::min)(amount, static_cast<uint32_t>((std::max)(0, hp_))));
    TriggerDamageFeedback();
    if (hp_ <= 0) {
        // Commit the result before invoking user callbacks: reentrant or later
        // same-frame damage cannot claim the same shared HP pool twice.
        isDead_ = true;
        ParticleManager::GetInstance()->EmitNeonDeathEffect(
            GetWorldPosition(),
            isRunResource_ ? Vector4{ 1.35f, 0.92f, 0.28f, 1.0f } : Vector4{ 1.20f, 0.32f, 1.35f, 1.0f },
            { 0.18f, 1.10f, 1.35f, 0.0f },
            isRunResource_ ? 0.46f : 0.32f);
        if (isRunResource_) {
            if (runResourceClaimCallback_) runResourceClaimCallback_(playerOwned);
        } else if (playerOwned) {
            if (player_) player_->AddExp(expValue_);
            if (playerDefeatCallback_) playerDefeatCallback_(GetWorldPosition());
        } else if (reportOrdinaryEnemyKill && enemyKillCallback_) {
            enemyKillCallback_(expValue_);
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

    const float t =
        damageFeedbackDuration_ > 0.0f
        ? damageFeedbackTimer_ / damageFeedbackDuration_
        : 0.0f;

    const float impact = t * t;

    worldTransform_.scale =
        baseScale_ * (1.0f + impact * (isRunResource_ ? 0.22f : 0.10f));

    visualColor_ = LerpColor(
        baseColor_,
        { 1.0f, 1.0f, 1.0f, baseColor_.w },
        (std::min)(1.0f, impact));

    object_->SetColor(visualColor_);
}

AABB ExpEnemy::GetAABB() {
    Vector3 worldPos = GetWorldPosition();

    AABB aabb;

    aabb.min = { worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f };
    aabb.max = { worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f };

    return aabb;
}
