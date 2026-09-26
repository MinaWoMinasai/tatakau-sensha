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
    combatShotsFired_ = combatDashCount_ = 0;
    shieldFlashTimer_ = 0.0f;
    shieldBlockCount_ = 0;
    combatRepathTimer_ = dashWarningTimer_ = dashTimer_ = 0;
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
        const ExpEnemyCombatCycle::Timing timing{ 0.36f, 0.23f, 0.30f, 0.70f, 0.23f };
        // Authored spawn positions deterministically stagger attack openings.
        combatStagger_ = std::fmod(std::abs(position.x * 0.173f + position.y * 0.319f), 0.85f);
        combatCycle_.Reset(timing, 0.55f + combatStagger_);
        bladeCycle_.Reset(0.35f + combatStagger_);
        orbitSign_ = static_cast<int>(std::abs(position.x + position.y)) % 4 < 2 ? -1.0f : 1.0f;
        dashCooldown_ = 2.3f + combatStagger_;
        ResetMagazine();
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
    if (IsExpeditionCombatRole()) ResetMagazine(definition.magazineSize, definition.reloadSeconds);
    expValue_=static_cast<uint32_t>((std::clamp)(definition.creditDrop,0,999)*5);
    baseColor_={definition.color[0],definition.color[1],definition.color[2],definition.color[3]};
    visualColor_=baseColor_;object_->SetColor(baseColor_);SetDamage(type_ == ExpEnemyType::BladeGuard ? 0u : authoredContactDamage_);
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
	combatMoveVelocity_ = {};
	combatWasDashing_ = false;
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
    case ExpEnemyType::Skirmisher:
        object_->SetModel("expEnemy.obj");
        baseColor_ = { 0.20f, 1.45f, 1.75f, 1.0f };
        hp_ = 28; expValue_ = 30;
        break;
    case ExpEnemyType::Flanker:
        object_->SetModel("expTriangle.obj");
        baseColor_ = { 1.75f, 0.25f, 0.80f, 1.0f };
        hp_ = 32; expValue_ = 35;
        break;
    case ExpEnemyType::Suppressor:
        object_->SetModel("expPentagon.obj");
        baseColor_ = { 1.60f, 0.95f, 0.16f, 1.0f };
        hp_ = 42; expValue_ = 45;
        break;
    case ExpEnemyType::ShieldGuard:
        object_->SetModel("expPentagon.obj");
        baseColor_ = { 0.18f, 1.20f, 1.60f, 1.0f };
        baseScale_ = { 1.1f, 1.1f, 1.1f };
        hp_ = 80; expValue_ = 45;
        break;
    case ExpEnemyType::BladeGuard:
        object_->SetModel("expEnemy.obj");
        baseColor_ = { 1.60f, 0.38f, 0.12f, 1.0f };
        hp_ = 65; expValue_ = 45;
        break;
    }
    object_->SetColor(baseColor_);
    visualColor_ = baseColor_;
    maxHp_ = hp_;
    SetDamage(type_ == ExpEnemyType::Shooter ? balanceConfig_.shooterContactDamage : balanceConfig_.contactDamage);
    if (type_ == ExpEnemyType::BladeGuard) SetDamage(0);
}

void ExpEnemy::ResetMagazine(int rounds, float reloadSeconds)
{
    ExpEnemyMagazineCycle::Timing timing{};
    if (type_ == ExpEnemyType::Sniper) timing = { 2, 0.52f, 0.26f, 0.20f, 1.55f };
    else if (type_ == ExpEnemyType::Flanker) timing = { 2, 0.24f, 0.17f, 0.22f, 1.55f };
    else if (type_ == ExpEnemyType::Suppressor) timing = { 5, 0.18f, 0.14f, 0.16f, 2.00f };
    else if (type_ == ExpEnemyType::ShieldGuard) timing = { 2, 0.38f, 0.18f, 0.28f, 1.60f };
    if (rounds > 0) timing.rounds = rounds;
    if (reloadSeconds > 0) timing.reload = reloadSeconds;
    magazineCycle_.Reset(timing, 0.55f + combatStagger_);
}

void ExpEnemy::Update(Stage& stage, float deltaTime) {

    // 座標を移動させる
    //worldTransform_.translate += velocity_ * (deltaTime * 60.0f);

    dt_ = deltaTime;

    invincibleTimer_ -= deltaTime;
    shieldFlashTimer_ = (std::max)(0.0f, shieldFlashTimer_ - deltaTime);
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

    // Shapes and stationary shooters can also be slammed. Their original
    // inertia remains, with the same anti-tunnelling wall steps as mobile AI.
    MoveCombatActor(stage, velocity_ * timeWeight);

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
        if (blocked && IsDashing()) break;
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
    if (isDead_ || !std::isfinite(deltaTime)) return;
    const float dt = (std::clamp)(deltaTime, 0.0f, 0.20f);
    if (dt <= 0) return;
    combatRepathTimer_ -= dt;
    dashCooldown_ -= dt;
    shooterMuzzleFlashTimer_ = (std::max)(0.0f, shooterMuzzleFlashTimer_ - dt);
    const Vector3 origin = GetWorldPosition();
    const bool hasTarget = player_ && !player_->IsDead();
    Vector3 toTarget = hasTarget ? player_->GetWorldPosition() - origin : Vector3{};
    toTarget.z = 0;
    const float distance = Length(toTarget);
    const Vector3 desiredDirection = distance > 0.001f ? toTarget / distance : aimDirection_;
    const Vector3 tangent{ -desiredDirection.y * orbitSign_, desiredDirection.x * orbitSign_, 0 };
    const bool visible = hasTarget && Length(ClipCombatRay(stage, origin, desiredDirection, distance) - origin) >= distance - 0.05f;
    const bool charger = type_ == ExpEnemyType::Charger;
    const bool sniper = type_ == ExpEnemyType::Sniper;
    const bool flanker = type_ == ExpEnemyType::Flanker;
    const bool suppressor = type_ == ExpEnemyType::Suppressor;
    const bool shield = type_ == ExpEnemyType::ShieldGuard;
    const bool blade = type_ == ExpEnemyType::BladeGuard;
    if (hasTarget && !IsAttackAimLocked() && dashTimer_ <= 0 && dashWarningTimer_ <= 0) {
        if (shield) {
            // A readable turn rate lets the player genuinely flank the shield.
            const float oldAngle = std::atan2(aimDirection_.y, aimDirection_.x);
            const float goalAngle = std::atan2(desiredDirection.y, desiredDirection.x);
            const float turn = std::remainder(goalAngle - oldAngle, 6.28318531f);
            const float angle = oldAngle + (std::clamp)(turn, -1.65f * dt, 1.65f * dt);
            aimDirection_ = { std::cos(angle), std::sin(angle), 0 };
        } else aimDirection_ = desiredDirection;
    }

    // Navigation is shared by every mobile role. Clear sight is insufficient
    // for a body-sized actor; route around cover with a cached inflated grid.
    Vector3 desiredMoveVelocity{};
    bool movingToTarget = false;
    auto navigate = [&](Vector3 desired, float speed, bool pathToTarget) {
        if (Length(desired) < 0.01f || !hasTarget) return;
        desired = Normalize(desired);
        if (pathToTarget && (!visible || stage.IsCollisionWithAnyBlock(origin + desired * 1.7f, 0.85f))) {
            if (combatRepathTimer_ <= 0 || Length(combatWaypoint_ - origin) < 0.35f) {
                combatWaypoint_ = FindCombatWaypoint(stage, player_->GetWorldPosition());
                combatRepathTimer_ = 0.32f;
            }
            const Vector3 waypoint = combatWaypoint_ - origin;
            desired = Length(waypoint) > 0.08f ? Normalize(waypoint) : Vector3{};
        }
        desiredMoveVelocity = desired * (speed * authoredMoveSpeedScale_);
        movingToTarget = pathToTarget;
    };

    if (blade) {
        bladeCycle_.SetIntervalScale(authoredFireIntervalScale_);
        bladeCycle_.Advance(dt, visible && distance <= expguard::kBladeReach + 0.45f);
        const auto phase = bladeCycle_.GetPhase();
        if (hasTarget && phase == ExpEnemyCombatPhase::Cooldown && distance > 2.8f) {
            navigate(desiredDirection + tangent * 0.08f, 6.2f, true);
        }
        // No passive body damage: all danger comes from the announced sweep.
        SetDamage(0);
        if (hasTarget && bladeCycle_.TryHit(aimDirection_.x, aimDirection_.y,
            toTarget.x, toTarget.y, player_->GetRadius(), visible)) {
            const int previousHp = player_->GetHp();
            const uint32_t damage = hasAuthoredDefinition_ ?
                static_cast<uint32_t>(std::ceil(authoredContactDamage_ * 1.5f)) : 27u;
            player_->TakeDamage(damage, 0.55f);
            if (player_->GetHp() < previousHp) {
                player_->SetVelocity(player_->GetMove() * 0.25f + desiredDirection * 0.50f);
                ParticleManager::GetInstance()->EmitNeonDeathEffect(player_->GetWorldPosition(),
                    { 1.8f, 0.55f, 0.15f, 1 }, { 0.8f, 0.15f, 0.05f, 0 }, 0.12f);
            }
        }
    } else if (charger) {
        combatCycle_.SetRecoveryScale((std::max)(0.8f, authoredFireIntervalScale_));
        const bool started = combatCycle_.Advance(dt, visible && distance < 13.0f);
        if (started) ++combatDashCount_;
        const auto phase = combatCycle_.GetPhase();
        if (phase == ExpEnemyCombatPhase::Active) {
            if (MoveCombatActor(stage, aimDirection_ * (30.0f * authoredMoveSpeedScale_ * dt))) {
                combatCycle_.EnterRecovery();
                ParticleManager::GetInstance()->EmitNeonDeathEffect(GetWorldPosition(),
                    { 1.8f, 0.7f, 0.15f, 1 }, { 0.8f, 0.2f, 0.1f, 0 }, 0.12f);
            }
        } else if (hasTarget && distance > 2.2f &&
            (phase == ExpEnemyCombatPhase::Cooldown || phase == ExpEnemyCombatPhase::Tracking)) {
            navigate(desiredDirection + tangent * 0.18f, phase == ExpEnemyCombatPhase::Tracking ? 3.0f : 6.8f, true);
        }
        SetDamage(phase == ExpEnemyCombatPhase::Recovery ? 0u :
            (hasAuthoredDefinition_ ? authoredContactDamage_ : balanceConfig_.contactDamage) *
            (phase == ExpEnemyCombatPhase::Active ? 2u : 1u));
    } else {
        const float preferred = sniper ? 23.0f : flanker ? 6.5f : suppressor ? 17.5f : shield ? 8.5f : 12.5f;
        const float speed = sniper ? 5.5f : flanker ? 8.0f : suppressor ? 4.2f : shield ? 3.6f : 6.8f;
        const float range = sniper ? 64.0f : flanker ? 11.5f : suppressor ? 29.0f : 24.0f;
        const auto previousPhase = magazineCycle_.GetPhase();
        // Dashes are timed decisions, not perfect projectile dodges. The short
        // arrow windup and distance cap make their destination readable.
        if (hasTarget && visible && !suppressor && !shield && previousPhase == ExpEnemyCombatPhase::Cooldown &&
            dashCooldown_ <= 0 && dashTimer_ <= 0 && dashWarningTimer_ <= 0 && (!sniper || distance < 15)) {
            dashDirection_ = sniper ? desiredDirection * -1.0f : flanker ?
                Normalize(desiredDirection * (distance > 8 ? 1.0f : -0.65f) + tangent * 0.60f) : tangent;
            const Vector3 destination = origin + dashDirection_ * 4.2f;
            if (!stage.IsCollisionWithAnyBlock(destination, 0.9f) &&
                Length(ClipCombatRay(stage, origin, dashDirection_, 4.2f) - origin) >= 4.1f) {
                dashWarningTimer_ = 0.18f;
            }
            dashCooldown_ = 2.8f + combatStagger_;
        }
        const bool maneuvering = dashWarningTimer_ > 0 || dashTimer_ > 0;
        if (dashWarningTimer_ > 0) {
            dashWarningTimer_ = (std::max)(0.0f, dashWarningTimer_ - dt);
            if (dashWarningTimer_ <= 0) { dashTimer_ = 0.18f; ++combatDashCount_; }
        } else if (dashTimer_ > 0) {
            const float stepTime = (std::min)(dt, dashTimer_);
            if (MoveCombatActor(stage, dashDirection_ * (21.0f * stepTime))) dashTimer_ = 0;
            else dashTimer_ = (std::max)(0.0f, dashTimer_ - dt);
        }
        magazineCycle_.SetIntervalScale(authoredFireIntervalScale_);
        const bool shot = !maneuvering && magazineCycle_.Advance(dt, visible && distance <= range);
        const auto phase = magazineCycle_.GetPhase();
        if (!maneuvering && hasTarget && phase != ExpEnemyCombatPhase::Locked && phase != ExpEnemyCombatPhase::Active) {
            const float mobility = phase == ExpEnemyCombatPhase::Recovery ? 0.25f :
                phase == ExpEnemyCombatPhase::Tracking ? (sniper ? 0.0f : 0.45f) : 1.0f;
            if (!visible || distance > preferred + 2.5f) navigate(desiredDirection + tangent * 0.22f, speed * mobility, true);
            else if (distance < preferred - 2.5f) navigate(desiredDirection * -0.85f + tangent * 0.65f, speed * mobility, false);
            else navigate(tangent, speed * mobility * (sniper ? 0.6f : shield ? 0.3f : 0.85f), false);
        }
        SetDamage(magazineCycle_.IsReloading() ? 0u : (hasAuthoredDefinition_ ? authoredContactDamage_ : balanceConfig_.shooterContactDamage));
        if (shot) {
            AttackParam param{};
            param.bulletSpeed = balanceConfig_.shooterBulletSpeed * (sniper ? 1.30f : flanker ? 0.85f : 0.95f);
            param.bulletCount = flanker ? 5 : suppressor ? 3 : 1;
            param.spreadAngleDeg = flanker ? 50.0f : suppressor ? 32.0f : 0.0f;
            param.randomSpread = false;
            param.damage = hasAuthoredDefinition_ ? authoredBulletDamage_ : balanceConfig_.shooterBulletDamage;
            param.bulletHp = sniper ? tankspecial::kArmoredEnemyBulletHp : tankspecial::kOrdinaryEnemyBulletHp;
            param.bulletPenetration = 1.0f;
            param.canClaimRunResource = false;
            const Vector3 shotOrigin = GetWorldPosition();
            const Vector3 muzzle = ClipCombatRay(stage, shotOrigin, aimDirection_, 1.35f);
            if (Length(muzzle - shotOrigin) >= 1.25f) {
                attackController_.FireFromMuzzle(muzzle, aimDirection_, param, BulletOwner::kEnemy);
                shooterMuzzleFlashTimer_ = 0.12f;
                ++combatShotsFired_;
            }
        }
    }
    const bool dashing = IsDashing();
    if (dashing && !combatWasDashing_) combatMoveVelocity_ = combatMoveVelocity_ * 0.20f;
    const float response = blade && bladeCycle_.GetPhase() != ExpEnemyCombatPhase::Cooldown ? 12.0f :
        Length(desiredMoveVelocity) > 0.01f ? 2.5f : 3.5f;
    combatMoveVelocity_ += (desiredMoveVelocity - combatMoveVelocity_) * (1.0f - std::exp(-response * dt));
    if (MoveCombatActor(stage, combatMoveVelocity_ * dt)) {
        if (!movingToTarget) orbitSign_ = -orbitSign_;
        combatRepathTimer_ = 0;
        combatMoveVelocity_ = combatMoveVelocity_ * 0.35f;
    }
    combatWasDashing_ = dashing;
    // External impact momentum persists through aim/reload states and resolves
    // against the same substepped walls as locomotion and dashes.
    if (Length(velocity_) > 0.001f) {
        if (Length(velocity_) > 0.95f) velocity_ = Normalize(velocity_) * 0.95f;
        MoveCombatActor(stage, velocity_ * (dt * 60.0f));
        velocity_ = velocity_ * std::exp(-5.0f * dt);
    }
    worldTransform_.rotate = { 0, 0, std::atan2(aimDirection_.x, -aimDirection_.y) };
    telegraphEnd_ = ClipCombatRay(stage, GetWorldPosition(), aimDirection_, blade ? expguard::kBladeReach : charger ? 9.0f * authoredMoveSpeedScale_ : sniper ? 64.0f : flanker ? 11.5f : 24.0f);
    if (GetCombatPhase() == ExpEnemyCombatPhase::Recovery) {
        visualColor_ = LerpColor(visualColor_, { 0.25f, 0.70f, 0.85f, 1 }, 0.65f);
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
    auto arcPoint = [&](float angle, float radius) {
        return center + (forward * std::cos(angle) + side * std::sin(angle)) * radius;
    };
    if (type_ == ExpEnemyType::BladeGuard) {
        const auto phase = bladeCycle_.GetPhase();
        const bool recovery = phase == ExpEnemyCombatPhase::Recovery;
        const bool warning = phase == ExpEnemyCombatPhase::Locked;
        const bool active = phase == ExpEnemyCombatPhase::Active;
        const Vector4 body = recovery ? Vector4{ 0.25f, 0.8f, 0.95f, 0.8f } : visualColor_;
        const Vector3 tip = center + forward * 0.9f;
        const Vector3 rear = center - forward * 0.9f;
        line(tip, center + side * 0.72f, width, body);
        line(center + side * 0.72f, rear, width, body);
        line(rear, center - side * 0.72f, width, body);
        line(center - side * 0.72f, tip, width, body);

        // The blade visibly draws back, sweeps the announced sector, then
        // hangs off to the side during the full recovery opening.
        const float pose = warning ? -expguard::kBladeHalfAngle - 0.2f * bladeCycle_.WarningRatio() :
            active ? -expguard::kBladeHalfAngle + 2.0f * expguard::kBladeHalfAngle * bladeCycle_.SweepRatio() :
            recovery ? 1.65f : -0.45f;
        const Vector4 bladeColor = recovery ? Vector4{ 0.2f, 0.8f, 1.0f, 0.55f } : Vector4{ 2.0f, 0.65f, 0.24f, 0.95f };
        line(arcPoint(pose, 0.65f), arcPoint(pose, active ? expguard::kBladeReach : 2.0f), width * 1.6f, bladeColor);
        line(arcPoint(pose - 0.14f, 0.9f), arcPoint(pose + 0.14f, 0.9f), width, bladeColor);
        if (warning || active) {
            const float intensity = warning ? 0.45f + 0.55f * bladeCycle_.WarningRatio() : 1.0f;
            const Vector4 danger{ 1.65f, 0.35f + 0.30f * intensity, 0.10f, warning ? 0.42f : 0.70f };
            constexpr int segments = 16;
            for (int i = 0; i < segments; ++i) {
                const float a = -expguard::kBladeHalfAngle + 2.0f * expguard::kBladeHalfAngle * static_cast<float>(i) / segments;
                const float b = -expguard::kBladeHalfAngle + 2.0f * expguard::kBladeHalfAngle * static_cast<float>(i + 1) / segments;
                line(arcPoint(a, expguard::kBladeReach), arcPoint(b, expguard::kBladeReach), width * (active ? 1.7f : 0.7f), danger);
                if (warning && i % 4 == 0) {
                    line(arcPoint(a, 1.1f), arcPoint(a, expguard::kBladeReach), width * 0.55f,
                        { danger.x, danger.y, danger.z, 0.12f });
                }
                if (active && a < pose) {
                    line(arcPoint(a, 2.6f), arcPoint(b, 2.6f), width * 2.7f,
                        { 1.8f, 0.5f, 0.13f, 0.34f });
                }
            }
            line(center, arcPoint(-expguard::kBladeHalfAngle, expguard::kBladeReach), width * 0.6f, danger);
            line(center, arcPoint(expguard::kBladeHalfAngle, expguard::kBladeReach), width * 0.6f, danger);
        } else if (recovery) {
            const float radius = 1.15f + bladeCycle_.RecoveryRatio() * 0.20f;
            line(center - side * radius - forward * 0.3f, center - side * radius + forward * 0.3f, width, body);
            line(center + side * radius - forward * 0.3f, center + side * radius + forward * 0.3f, width, body);
        }
        return;
    }
    if (type_ == ExpEnemyType::ShieldGuard) {
        const float flash = shieldFlashTimer_ / 0.13f;
        const Vector4 shieldColor{ 0.25f + 1.1f * flash, 1.15f + 0.8f * flash, 1.65f + 0.5f * flash, 0.58f + 0.35f * flash };
        constexpr int segments = 8;
        for (int i = 0; i < segments; ++i) {
            const float a = -expguard::kShieldHalfAngle + 2.0f * expguard::kShieldHalfAngle * static_cast<float>(i) / segments;
            const float b = -expguard::kShieldHalfAngle + 2.0f * expguard::kShieldHalfAngle * static_cast<float>(i + 1) / segments;
            line(arcPoint(a, 1.45f), arcPoint(b, 1.45f), width * (1.3f + flash), shieldColor);
            // A transparent second band communicates its surface without
            // filling the body, player bullets or the aiming reticle.
            line(arcPoint(a, 1.33f), arcPoint(b, 1.33f), 0.22f,
                { shieldColor.x, shieldColor.y, shieldColor.z, 0.12f + 0.16f * flash });
        }
        line(arcPoint(-expguard::kShieldHalfAngle, 1.2f), arcPoint(-expguard::kShieldHalfAngle, 1.55f), width, shieldColor);
        line(arcPoint(expguard::kShieldHalfAngle, 1.2f), arcPoint(expguard::kShieldHalfAngle, 1.55f), width, shieldColor);
    }
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
        const int sides = type_ == ExpEnemyType::Flanker ? 3 : type_ == ExpEnemyType::Skirmisher ? 4 : type_ == ExpEnemyType::Suppressor ? 5 : 6;
        for (int i = 0; i < sides; ++i) {
            const float a = static_cast<float>(i) * 2 * pi / static_cast<float>(sides);
            const float b = static_cast<float>(i + 1) * 2 * pi / static_cast<float>(sides);
            line(center + (side * std::cos(a) + forward * std::sin(a)) * (0.82f * pulse),
                center + (side * std::cos(b) + forward * std::sin(b)) * (0.82f * pulse), width, visualColor_);
        }
        line(center + side * 0.20f, center + forward * 1.35f + side * 0.12f, width, visualColor_);
        line(center - side * 0.20f, center + forward * 1.35f - side * 0.12f, width, visualColor_);
        line(center - side * 0.36f, center + side * 0.36f, width, visualColor_);
        // Short rear rails distinguish magazine weapons from the rammer.
        const int ammo = magazineCycle_.GetAmmo(), capacity = magazineCycle_.GetCapacity();
        for (int i = 0; i < capacity; ++i) {
            const float offset = (static_cast<float>(i) - static_cast<float>(capacity - 1) * 0.5f) * 0.25f;
            const Vector3 tick = center - forward * 1.15f + side * offset;
            line(tick, tick - forward * 0.24f, width * 0.8f,
                i < ammo ? Vector4{ 1.6f, 1.5f, 0.6f, 0.95f } : Vector4{ 0.14f, 0.18f, 0.22f, 0.5f });
        }
    }
    const ExpEnemyCombatPhase phase = GetCombatPhase();
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
        } else if (type_ == ExpEnemyType::Charger) {
            for (float t = start; t < distance; t += 2.0f) {
                const Vector3 marker = center + forward * t;
                line(marker - forward * 0.50f + side * 0.40f, marker, width * 0.80f, warning);
                line(marker - forward * 0.50f - side * 0.40f, marker, width * 0.80f, warning);
            }
        } else {
            // A local muzzle warning keeps several mobile roles legible without
            // covering the whole arena with laser sight lines.
            const float reach = (std::min)(distance, type_ == ExpEnemyType::Skirmisher ? 3.5f : 6.0f);
            const float fan = type_ == ExpEnemyType::Flanker ? 0.46f : type_ == ExpEnemyType::Suppressor ? 0.27f : 0.0f;
            line(center + forward * start, center + forward * reach + side * (reach * fan), width * 0.75f, warning);
            if (fan > 0) line(center + forward * start, center + forward * reach - side * (reach * fan), width * 0.75f, warning);
        }
        if (type_ == ExpEnemyType::Charger || type_ == ExpEnemyType::Sniper) {
            const Vector3 end = center + forward * distance;
            line(end - side * 0.55f, end + side * 0.55f, width, warning);
        }
    }
    if (phase == ExpEnemyCombatPhase::Recovery) {
        const Vector4 recovery{ 0.25f, 1.0f, 1.2f, 0.75f };
        const float ratio = type_ == ExpEnemyType::Charger ? combatCycle_.GetRecoveryRatio() : 1.0f - magazineCycle_.GetReloadProgress();
        const float radius = 1.15f + ratio * 0.15f;
        line(center + cameraRight * radius - cameraUp * 0.25f, center + cameraRight * radius + cameraUp * 0.25f, width, recovery);
        line(center - cameraRight * radius - cameraUp * 0.25f, center - cameraRight * radius + cameraUp * 0.25f, width, recovery);
        if (type_ != ExpEnemyType::Charger) {
            const Vector3 start = center - cameraUp * 1.5f - cameraRight * 0.9f;
            line(start, start + cameraRight * 1.8f, width * 0.7f, { 0.12f, 0.3f, 0.4f, 0.6f });
            line(start, start + cameraRight * (1.8f * magazineCycle_.GetReloadProgress()), width * 1.5f, recovery);
        }
    }
    if (dashWarningTimer_ > 0 || dashTimer_ > 0) {
        const Vector3 dashSide{ -dashDirection_.y, dashDirection_.x, 0 };
        const Vector4 dashColor{ 0.4f, 1.6f, 2.0f, 0.75f };
        for (int i = 1; i <= 2; ++i) {
            const Vector3 marker = center + dashDirection_ * (static_cast<float>(i) * 1.5f);
            line(marker - dashDirection_ * 0.45f + dashSide * 0.35f, marker, width, dashColor);
            line(marker - dashDirection_ * 0.45f - dashSide * 0.35f, marker, width, dashColor);
        }
        if (dashTimer_ > 0) {
            line(center + dashSide * 0.5f, center + dashSide * 0.5f - dashDirection_ * 2.2f, width * 1.5f, dashColor);
            line(center - dashSide * 0.5f, center - dashSide * 0.5f - dashDirection_ * 2.2f, width * 1.5f, dashColor);
        }
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
    uint32_t damage = other->GetDamage();
    if (!killedByEnemy) {
        Vector3 source = other->GetWorldPosition();
        bool melee = otherAttribute == kCollisionAttributePlayer;
        if (auto* bullet = dynamic_cast<Bullet*>(other)) {
            // Current incoming direction correctly handles ricochets and
            // projectiles which already crossed the actor centre this frame.
            const Vector3 movement = bullet->GetMove();
            if (Length(movement) > 0.0001f) source = GetWorldPosition() - Normalize(movement);
            melee = bullet->GetSpecialKind() == Bullet::SpecialKind::SlashWave;
        }
        damage = ResolveShieldDamage(damage, source, melee);
    }
    ApplyDamage(damage, !killedByEnemy, true);

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
    return TakeDirectionalDamage(amount, player_ ? player_->GetWorldPosition() : GetWorldPosition());
}

uint32_t ExpEnemy::ResolveShieldDamage(uint32_t amount, const Vector3& attackSource, bool melee)
{
    if (type_ != ExpEnemyType::ShieldGuard || isDead_ || amount == 0) return amount;
    const Vector3 offset = attackSource - GetWorldPosition();
    const uint32_t result = expguard::ShieldDamage(amount, aimDirection_.x, aimDirection_.y, offset.x, offset.y, melee);
    if (result < amount) {
        ++shieldBlockCount_;
        if (shieldFlashTimer_ <= 0.0f) {
            const Vector3 normal = Length(offset) > 0.0001f ? Normalize(offset) : aimDirection_;
            ParticleManager::GetInstance()->EmitNeonImpactEffect(GetWorldPosition() + normal * 1.25f,
                normal, { 0.30f, 1.40f, 1.90f, 0.95f }, 4);
        }
        shieldFlashTimer_ = 0.13f;
    }
    return result;
}

bool ExpEnemy::TakeDirectionalDamage(uint32_t amount, const Vector3& attackSource, bool melee)
{
    return ApplyDamage(ResolveShieldDamage(amount, attackSource, melee), true, false);
}

void ExpEnemy::ApplyKnockback(const Vector3& direction, float power)
{
    if (isDead_ || isRunResource_ || !std::isfinite(power) || power <= 0.0f || Length(direction) < 0.001f) return;
    velocity_ += Normalize(direction) * (std::min)(0.95f, power);
    if (Length(velocity_) > 0.95f) velocity_ = Normalize(velocity_) * 0.95f;
    // A committed impact can interrupt a light enemy, but does not reset its
    // magazine or grant an instant shot when it recovers.
    if (power >= 0.30f) {
        if (type_ == ExpEnemyType::Charger) combatCycle_.EnterRecovery();
        dashTimer_ = dashWarningTimer_ = 0.0f;
        dashCooldown_ = (std::max)(dashCooldown_, 0.65f);
    }
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
