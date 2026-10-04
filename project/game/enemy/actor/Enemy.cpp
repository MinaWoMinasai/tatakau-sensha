#include "game/weapon/CombatTypes.h"
#include "Enemy.h"
#include "Calculation.h"
#include "EnemyManager.h"
#include "ExpEnemy.h"
#include "Player.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include <limits>
#include <queue>
#include "ParticleManager.h"
#include "Stage.h"
using namespace DirectX;

static constexpr float kDeltaTime = 1.0f / 60.0f;

namespace {
/// @brief 係数を0〜1に制限し、2つのRGBA色を線形補間して返す。
cg2::Vector4 LerpColor(const cg2::Vector4& a, const cg2::Vector4& b, float t)
{
	t = (std::clamp)(t, 0.0f, 1.0f);
	return {
		a.x + (b.x - a.x) * t,
		a.y + (b.y - a.y) * t,
		a.z + (b.z - a.z) * t,
		a.w + (b.w - a.w) * t
	};
}

/// @brief XY方向をangleDeg（度）だけ回転して返す。Z成分は保持する。
cg2::Vector3 RotateDirection2D(const cg2::Vector3& dir, float angleDeg)
{
	const float rad = angleDeg * 3.1415926535f / 180.0f;
	return {
		dir.x * std::cos(rad) - dir.y * std::sin(rad),
		dir.x * std::sin(rad) + dir.y * std::cos(rad),
		dir.z
	};
}

/// @brief ラジアン角を-π～πの範囲へ折り返す。両端はそのまま含む。
float NormalizeAngleRad(float angle)
{
	constexpr float twoPi = 6.283185307f;
	while (angle > 3.1415926535f) {
		angle -= twoPi;
	}
	while (angle < -3.1415926535f) {
		angle += twoPi;
	}
	return angle;
}
}

Enemy::~Enemy() {}

void Enemy::Fire() {
	if (!runEncounterEnabled_ || isDead_ || hp_ <= 0) return;

	assert(player_);

	// 速度は60FPS基準の1フレーム当たり。方向の長さではなくAttackParamの弾速を発射制御が使う。
	const float kBulletSpeed = 0.35f;

	// 現在のPlayer位置へ向ける。位置の更新や照準予測はここでは行わない。
	cg2::Vector3 playerPos = player_->GetWorldPosition();
	cg2::Vector3 enemyPos = GetWorldPosition();
	cg2::Vector3 direction = playerPos - enemyPos;
	direction = cg2::Normalize(direction);
	// ベクトルの長さを速さに合わせる
	direction = kBulletSpeed * direction;

	// 自機と同じ位置なら発射しない
	if (direction.x == 0.0f && direction.y == 0.0f && direction.z == 0.0f) {
		return;
	}

	AttackParam attackParam;
	attackParam.bulletSpeed = kBulletSpeed;
	attackParam.bulletCount = 1;
	attackParam.spreadAngleDeg = 0.0f;


	++shotsFired_;
	attackController_.Fire(
		worldTransform_.translate,
		direction,
		attackParam,
		BulletOwner::kEnemy
	);
}

void Enemy::ShotgunFire()
{
	if (!runEncounterEnabled_ || isDead_ || hp_ <= 0) return;
	assert(player_);

	// 発射位置
	cg2::Vector3 origin = GetWorldPosition();

	// 基準方向
	cg2::Vector3 baseDir;
	cg2::Vector3 aimTarget = currentMoveTargetPosition_;
	if (cg2::Length(aimTarget - origin) < 0.001f) {
		aimTarget = player_->GetWorldPosition();
	}
	if (cg2::Length(aimTarget - origin) < 0.001f) {
		return;
	}
	baseDir = cg2::Normalize(aimTarget - origin);

	// 攻撃パラメータを設定
	AttackParam param{};
	param.bulletSpeed = bossAttackConfig_.bulletSpeed;
	param.bulletCount = bossAttackConfig_.bulletCount;
	param.spreadAngleDeg = bossAttackConfig_.spreadAngleDeg;
	param.randomSpread = bossAttackConfig_.randomSpread;

	param.reflect = false;
	param.penetrate = false;
	param.cooldown = bossAttackConfig_.cooldown;
	param.damage = bossAttackConfig_.damage;
	param.bulletHp = bossAttackConfig_.bulletHp;
	param.bulletPenetration = bossAttackConfig_.bulletPenetration;

	switch (bossAttackConfig_.pattern) {
	case BossAttackConfig::Pattern::Ring:
		param.bulletCount = (std::max)(8, bossAttackConfig_.bulletCount);
		param.spreadAngleDeg = 360.0f;
		param.randomSpread = false;
		break;
	case BossAttackConfig::Pattern::Sniper:
		param.bulletCount = 1;
		param.bulletSpeed = bossAttackConfig_.bulletSpeed * 1.7f;
		param.spreadAngleDeg = 0.0f;
		param.randomSpread = false;
		break;
	case BossAttackConfig::Pattern::Alternating:
		param.bulletCount = (std::max)(1, bossAttackConfig_.bulletCount);
		param.spreadAngleDeg = (std::max)(8.0f, bossAttackConfig_.spreadAngleDeg * 0.35f);
		param.randomSpread = false;
		baseDir = RotateDirection2D(baseDir, (alternatingShotIndex_ % 2 == 0) ? -18.0f : 18.0f);
		alternatingShotIndex_++;
		break;
	case BossAttackConfig::Pattern::Spread:
	default:
		break;
	}

	// 発射
	++shotsFired_;
	attackController_.Fire(
		origin,
		baseDir,
		param,
		BulletOwner::kEnemy
	);
}

void Enemy::Initialize(cg2::Object3d* object, const cg2::Vector3& position, Stage* stage) {
	++encounterGeneration_;
	shotsFired_ = 0;
	runEncounterEnabled_ = true;
	runEncounterBaselineCaptured_ = false;
	prototypeCombatEnabled_ = false;
	prototypeResourceFocus_ = false;
	prototypeResourceTargetActive_ = false;
	prototypeCombat_.Reset();
	prototypePressure_ = 0;

	hpBarFill_ = std::make_unique<cg2::Object3d>();
	hpBarFill_->Initialize();
	cg2::Vector3 hpBarFillScale = hpBarFill_->GetScale();
	hpBarFillScale.y = 1.2f;
	hpBarFill_->SetScale(hpBarFillScale);
	hpBarFill_->SetModel("playerHPBarGreenLong.obj");

	hpBarFillTransform_ = cg2::InitWorldTransform();
	hpBarFillTransform_.translate = position;
	hpBarFill_->SetTransform(worldTransform_);
	hpBarFill_->Update();


	hpBarBG_ = std::make_unique<cg2::Object3d>();
	hpBarBG_->Initialize();
	cg2::Vector3 hpBarBackgroundScale = hpBarBG_->GetScale();
	hpBarBackgroundScale.y = 1.2f;
	hpBarBG_->SetScale(hpBarBackgroundScale);

	hpBarBG_->SetModel("playerHPBarLong.obj");

	hpBarBGTransform_ = cg2::InitWorldTransform();
	hpBarBGTransform_.translate = position;
	hpBarBG_->SetTransform(worldTransform_);
	hpBarBG_->Update();

	object_ = object;
	worldTransform_ = cg2::InitWorldTransform();
	worldTransform_.translate = position;
	baseScale_ = worldTransform_.scale;
	baseColor_ = cg2::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
	object_->SetColor(baseColor_);

	// 衝突属性を設定
	SetCollisionAttribute(kCollisionAttributeEnemy);
	// 自機・自機弾・ドローン・ボスに敵対する通常敵の弾を対象にする。通常敵本体は設定側で追加する。
	SetCollisionMask(kCollisionAttributePlayer | kCollisionAttributePlayerBullet | kCollisionAttributePlayerDrone | kCollisionAttributeHostileExpEnemyBullet);
	SetDamage(35);

	stage_ = stage;

	sprite = std::make_unique<cg2::Sprite>();
	sprite->Initialize(cg2::SpriteCommon::GetInstance(), "resources/bossHPGreen.png");
	sprite->SetPosition({ 20.0f, 210.0f });

	bossHpRed = std::make_unique<cg2::Sprite>();
	bossHpRed->Initialize(cg2::SpriteCommon::GetInstance(), "resources/bossHPRed.png");
	bossHpRed->SetPosition({ 20.0f, 210.0f });

	bossHpFont = std::make_unique<cg2::Sprite>();
	bossHpFont->Initialize(cg2::SpriteCommon::GetInstance(), "resources/BossHp.png");
	bossHpFont->SetPosition({ 20.0f, 160.0f });
	bossHpFont->SetSize({120.0f, 40.0f});

}

void Enemy::SetBossAttackConfig(const BossAttackConfig& config)
{
	bossAttackConfig_ = config;
	bossAttackConfig_.bulletSpeed = (std::max)(0.01f, bossAttackConfig_.bulletSpeed);
	bossAttackConfig_.bulletCount = (std::max)(1, bossAttackConfig_.bulletCount);
	bossAttackConfig_.spreadAngleDeg = (std::clamp)(bossAttackConfig_.spreadAngleDeg, 0.0f, 180.0f);
	if (bossAttackConfig_.pattern == BossAttackConfig::Pattern::Ring) {
		bossAttackConfig_.spreadAngleDeg = 360.0f;
	}
	bossAttackConfig_.cooldown = (std::max)(0.05f, bossAttackConfig_.cooldown);
	bossAttackConfig_.bulletHp = (std::max)(0.0f, bossAttackConfig_.bulletHp);
	bossAttackConfig_.bulletPenetration = (std::max)(0.0f, bossAttackConfig_.bulletPenetration);
	kFireTimerMax_ = bossAttackConfig_.cooldown;
}

void Enemy::SetEnemyProgressConfig(const EnemyProgressConfig& config)
{
	enemyProgressConfig_ = config;
	enemyProgressConfig_.expEnemyContactDamage = (std::max)(1u, enemyProgressConfig_.expEnemyContactDamage);
	enemyProgressConfig_.healOnExpEnemyKill = (std::max)(0, enemyProgressConfig_.healOnExpEnemyKill);
	enemyProgressConfig_.killsPerLevel = (std::max)(1, enemyProgressConfig_.killsPerLevel);
	enemyProgressConfig_.maxHpGainPerLevel = (std::max)(0, enemyProgressConfig_.maxHpGainPerLevel);
	enemyProgressConfig_.levelingEnterPlayerDistance = (std::max)(1.0f, enemyProgressConfig_.levelingEnterPlayerDistance);
	enemyProgressConfig_.levelingExitPlayerDistance = (std::clamp)(enemyProgressConfig_.levelingExitPlayerDistance, 0.5f, enemyProgressConfig_.levelingEnterPlayerDistance);
	enemyProgressConfig_.levelingSearchRadius = (std::max)(1.0f, enemyProgressConfig_.levelingSearchRadius);
	enemyProgressConfig_.aimTurnHalfSeconds = (std::max)(0.05f, enemyProgressConfig_.aimTurnHalfSeconds);
	if (!enemyProgressConfig_.expEnemyHostile || !enemyProgressConfig_.levelingModeEnabled) {
		levelingModeActive_ = false;
	}
	uint32_t mask = kCollisionAttributePlayer | kCollisionAttributePlayerBullet | kCollisionAttributePlayerDrone | kCollisionAttributeHostileExpEnemyBullet;
	if (enemyProgressConfig_.expEnemyHostile) {
		mask |= kCollisionAttributeExpEnemy;
	}
	SetCollisionMask(mask);
}

void Enemy::RegisterExpEnemyKill(uint32_t expValue)
{
	if (!runEncounterEnabled_ || isDead_ || hp_ <= 0) return;
	expEnemyKillCount_++;
	if (prototypeCombatEnabled_ && prototypeResourceFocus_) {
		// 資源争奪中の通常敵撃破は、撃破数と最大1の回復だけに使う。
		// 資源取得による成長はRegisterRunResourceClaimが別に処理する。
		HealFromFeeding((std::min)(1, enemyProgressConfig_.healOnExpEnemyKill));
		return;
	}
	enemyExp_ += expValue;
	HealFromFeeding(enemyProgressConfig_.healOnExpEnemyKill);
	if (expEnemyKillCount_ % enemyProgressConfig_.killsPerLevel == 0) {
		AdvanceFeedingLevel();
	}
}

void Enemy::HealFromFeeding(int amount)
{
	amount = (std::max)(0, (std::min)(amount, maxHP_ - hp_));
	if (prototypeCombatEnabled_) {
		amount = (std::min)(amount, prototypeFeedingHealBudget_);
		prototypeFeedingHealBudget_ -= amount;
	}
	hp_ += amount;
}

void Enemy::AdvanceFeedingLevel()
{
	if (prototypeCombatEnabled_ && enemyLevel_ >= 6) return;
	enemyLevel_++;
	int hpGain = enemyProgressConfig_.maxHpGainPerLevel;
	if (prototypeCombatEnabled_) {
		const int hpLimit = prototypeBaseMaxHp_ + prototypeBaseMaxHp_ / 2;
		hpGain = (std::max)(0, (std::min)(hpGain, hpLimit - maxHP_));
	}
	maxHP_ += hpGain;
	HealFromFeeding(hpGain);
	SetDamage(GetDamage() + enemyProgressConfig_.damageGainPerLevel);
	bossAttackConfig_.damage += enemyProgressConfig_.damageGainPerLevel;
}

void Enemy::RegisterRunResourceClaim()
{
	if (!runEncounterEnabled_ || !prototypeCombatEnabled_ || !prototypeResourceFocus_ || isDead_ || hp_ <= 0) return;
	HealFromFeeding(enemyProgressConfig_.healOnExpEnemyKill);
	AdvanceFeedingLevel();
}

void Enemy::EnablePrototypeCombat(bool enabled)
{
	if (prototypeCombatEnabled_ == enabled) return;
	prototypeCombatEnabled_ = enabled;
	prototypeCombat_.Reset();
	fireTimer_ = kFireTimerMax_;
	if (enabled) {
		prototypeBaseMaxHp_ = (std::max)(1, maxHP_);
		prototypeFeedingHealBudget_ = prototypeBaseMaxHp_ / 2;
	}
}

void Enemy::SetPrototypeMaxHp(int maxHp, bool healToFull)
{
	prototypeBaseMaxHp_ = (std::clamp)(maxHp, 1, 1000000);
	maxHP_ = prototypeBaseMaxHp_;
	prototypeFeedingHealBudget_ = prototypeBaseMaxHp_ / 2;
	// 最大HPの変更だけでは復活させない。死亡フラグはResetRunEncounterで解除する。
	hp_ = isDead_ ? 0 : (healToFull ? maxHP_ : (std::clamp)(hp_, 0, maxHP_));
}

Enemy::PrototypeTelegraph Enemy::GetPrototypeTelegraph() const
{
	PrototypeTelegraph result{};
	if (expeditionRivalEnabled_) {
		const auto status = GetRivalCombatStatus();
		result.active = status.enabled && (status.phase == RivalBossCombat::Phase::Tracking || status.phase == RivalBossCombat::Phase::Locked);
		result.attackType = status.pattern == RivalBossCombat::Pattern::Sweep ? PrototypeAttackType::Sweep : PrototypeAttackType::AimedSpread;
		result.direction = status.direction;
		result.progress = status.phase == RivalBossCombat::Phase::Locked ? 1.0f : status.progress;
		result.spreadAngleDeg = RivalBossCombat::WarningHalfAngle(status.pattern) * 2.0f;
		return result;
	}
	result.active = runEncounterEnabled_ && prototypeCombatEnabled_ && !isDead_ && hp_ > 0 &&
		prototypeCombat_.GetPhase() == PrototypeBossCombat::Phase::Telegraph;
	result.attackType = prototypeCombat_.GetAttackType();
	const float angle = prototypeCombat_.GetAimAngle();
	result.direction = { std::cos(angle), std::sin(angle), 0.0f };
	result.progress = prototypeCombat_.GetProgress();
	result.spreadAngleDeg = prototypeCombat_.GetSpreadAngleDeg();
	return result;
}

void Enemy::SetRunEncounterEnabled(bool enabled)
{
	runEncounterEnabled_ = enabled;
	if (!enabled) {
		velocity_ = {};
		impactVelocity_ = {};
		levelingModeActive_ = false;
		prototypeResourceTargetActive_ = false;
		prototypeCombat_.Reset();
	}
}

void Enemy::ResetRunEncounter(const cg2::Vector3& position, int hp, int pressure, bool resourceFocus)
{
	if (!object_) return;
	++encounterGeneration_;
	shotsFired_ = 0;
	expeditionRivalEnabled_ = false;
	// 初回の遭遇戦で基準ダメージを保存し、次の部屋で捕食による上昇を持ち越さない。
	if (!runEncounterBaselineCaptured_) {
		runEncounterBaseContactDamage_ = GetDamage();
		runEncounterBaseBulletDamage_ = bossAttackConfig_.damage;
		runEncounterBaselineCaptured_ = true;
	}
	// 借用先・射撃調整値を保ったまま、死亡・成長・行動の状態を新しい遭遇戦へ戻す。
	runEncounterEnabled_ = true;
	isDead_ = false;
	isExploding_ = false;
	radius_ = 2.0f;
	deathChargeTimer_ = deathEffectTimer_ = damageFeedbackTimer_ = 0.0f;
	enemyLevel_ = 1;
	expEnemyKillCount_ = 0;
	enemyExp_ = 0;
	SetDamage(runEncounterBaseContactDamage_);
	bossAttackConfig_.damage = runEncounterBaseBulletDamage_;
	prototypeCombatEnabled_ = true;
	prototypeResourceFocus_ = resourceFocus;
	prototypeResourceTargetActive_ = false;
	prototypeCombat_.Reset();
	SetPrototypePressure(pressure);
	SetPrototypeMaxHp(hp, true);
	EnemyProgressConfig progress = enemyProgressConfig_;
	progress.levelingModeEnabled = resourceFocus;
	SetEnemyProgressConfig(progress);
	levelingModeActive_ = false;
	currentMoveTargetPosition_ = position;
	velocity_ = dir_ = evadeVec = wanderVec = wallFollowDir_ = {};
	impactVelocity_ = {};
	steeringDir_ = { 1.0f, 0.0f, 0.0f };
	steeringNoise_ = {};
	aiState_ = AIState::Wander;
	attackPower = evadePower = wanderPower = 0.0f;
	isWallFollowing_ = false;
	wallFollowTimer_ = steeringNoiseTimer_ = hesitationTimer_ = 0.0f;
	hesitationCooldown_ = wanderChangeTimer = 1.0f;
	fireIntervalTimer = 0;
	time_ = 60;
	bulletCooldown_ = 0.5f;
	fireTimer_ = kFireTimerMax_;
	alternatingShotIndex_ = 0;
	worldTransform_.translate = position;
	worldTransform_.rotate = {};
	worldTransform_.scale = baseScale_;
	object_->SetColor(baseColor_);
	object_->SetTransform(worldTransform_);
	object_->Update();
	UpdateHPBar();
}

void Enemy::EnableExpeditionRival(bool enabled)
{
	expeditionRivalEnabled_ = enabled;
	rivalCombat_.Reset();
	rivalDashDirection_ = { 0.0f, 1.0f, 0.0f };
	rivalPathDirection_ = {};
	rivalPathTimer_ = rivalDashDistance_ = 0.0f;
	rivalStrafeSign_ = 1.0f;
	velocity_ = {};
	impactVelocity_ = {};
	if (enabled) {
		// ライバル戦ではPlayerを狙い続けるため、通常敵への敵対と捕食対象の探索を止める。
		prototypeResourceFocus_ = prototypeResourceTargetActive_ = levelingModeActive_ = false;
		EnemyProgressConfig progress = enemyProgressConfig_;
		progress.expEnemyHostile = progress.levelingModeEnabled = false;
		SetEnemyProgressConfig(progress);
	}
}

Enemy::RivalCombatStatus Enemy::GetRivalCombatStatus() const
{
	RivalCombatStatus status{};
	status.enabled = expeditionRivalEnabled_ && runEncounterEnabled_ && !isDead_ && hp_ > 0;
	status.phase2 = rivalCombat_.IsPhaseTwo();
	status.phase = rivalCombat_.GetPhase();
	status.pattern = rivalCombat_.GetPattern();
	status.progress = rivalCombat_.GetProgress();
	status.ammo = rivalCombat_.GetAmmo();
	status.capacity = rivalCombat_.GetCapacity();
	status.shotsFired = rivalCombat_.GetShotsFired();
	status.dashCount = rivalCombat_.GetDashCount();
	status.reloadCount = rivalCombat_.GetReloadCount();
	const float angle = rivalCombat_.GetAimAngle();
	status.direction = { std::cos(angle), std::sin(angle), 0.0f };
	status.dashDirection = rivalDashDirection_;
	status.dashDistance = rivalDashDistance_;
	return status;
}

void Enemy::SelectRivalDashDirection(const cg2::Vector3& towardPlayer)
{
	const cg2::Vector3 side{ -towardPlayer.y * rivalStrafeSign_, towardPlayer.x * rivalStrafeSign_, 0.0f };
	const bool close = cg2::Length(player_->GetWorldPosition() - GetWorldPosition()) < 14.0f;
	const cg2::Vector3 radial = towardPlayer * (close ? -0.55f : 0.35f);
	const std::array<cg2::Vector3, 6> candidates{
		cg2::Normalize(side + radial), cg2::Normalize(-side + radial), side, -side, -towardPlayer, towardPlayer
	};
	rivalDashDirection_ = candidates.front();
	rivalDashDistance_ = 0.0f;
	// 中央と機体幅の左右の線分で通行を確認し、距離を縮めながら突進先を探す。
	// 通る候補がなければ距離0のままとし、UpdateRivalCombatで突進を終了する。
	for (float scale : { 1.0f, 0.70f, 0.40f }) {
		for (const auto& candidate : candidates) {
			const float distance = rivalCombat_.GetDashDistance() * scale;
			if (HasClearMoveRouteToTarget(GetWorldPosition() + candidate * distance)) {
				rivalDashDirection_ = candidate;
				rivalDashDistance_ = distance;
				return;
			}
		}
	}
}

cg2::Vector3 Enemy::ResolveRivalMove(const cg2::Vector3& desired, const cg2::Vector3& towardPlayer, float deltaTime)
{
	rivalPathTimer_ -= deltaTime;
	const cg2::Vector3 position = GetWorldPosition();
	if (!HasClearMoveRouteToTarget(player_->GetWorldPosition())) {
		if (rivalPathTimer_ <= 0.0f) {
			const auto path = FindPathDirectionToPlayer();
			rivalPathDirection_ = path.value_or(cg2::Vector3{});
			rivalPathTimer_ = 0.25f;
		}
		if (cg2::Length(rivalPathDirection_) > 0.001f) return rivalPathDirection_;
	}
	if (HasClearMoveRouteToTarget(position + desired * 3.0f)) return desired;
	const cg2::Vector3 mirrored = towardPlayer * cg2::Dot(desired, towardPlayer) -
		(desired - towardPlayer * cg2::Dot(desired, towardPlayer));
	if (HasClearMoveRouteToTarget(position + mirrored * 3.0f)) {
		rivalStrafeSign_ *= -1.0f;
		return mirrored;
	}
	if (HasClearMoveRouteToTarget(position - towardPlayer * 3.0f)) return -towardPlayer;
	if (HasClearMoveRouteToTarget(position + towardPlayer * 3.0f)) return towardPlayer;
	return {};
}

void Enemy::UpdateRivalCombat(float deltaTime)
{
	if (!player_ || !bulletManager_ || !stage_ || isDead_ || hp_ <= 0 || !std::isfinite(deltaTime) || deltaTime <= 0.0f) {
		velocity_ = {};
		return;
	}
	using Phase = RivalBossCombat::Phase;
	currentMoveTargetPosition_ = player_->GetWorldPosition();
	cg2::Vector3 toward = currentMoveTargetPosition_ - GetWorldPosition();
	toward.z = 0.0f;
	const float distance = cg2::Length(toward);
	toward = distance > 0.001f ? toward / distance : cg2::Vector3{ 1.0f, 0.0f, 0.0f };
	// Player速度は60FPS基準なので12フレーム先を照準目標にする。
	// 追尾中は照準を更新し、固定後の角度はRivalBossCombatが保持する。
	const cg2::Vector3 aimTarget = currentMoveTargetPosition_ + player_->GetMove() * 12.0f;
	const cg2::Vector3 aim = aimTarget - GetWorldPosition();
	bool threat = false;
	if (rivalCombat_.GetPhase() == Phase::Reposition) {
		for (Bullet* bullet : bulletManager_->GetBulletPtrs()) {
			if (!bullet || bullet->IsDead() || bullet->GetCollisionAttribute() != kCollisionAttributePlayerBullet) continue;
			const cg2::Vector3 relative = bullet->GetWorldPosition() - GetWorldPosition();
			const cg2::Vector3 speed = bullet->GetMove();
			if (RivalBossCombat::IsIncomingThreat(relative.x, relative.y, speed.x, speed.y, radius_ + bullet->GetRadius() + 0.5f)) {
				threat = true;
				break;
			}
		}
	}
	const Phase previousPhase = rivalCombat_.GetPhase();
	const auto shot = rivalCombat_.Step(deltaTime, HasLineOfSightToPlayer(), std::atan2(aim.y, aim.x),
		static_cast<float>(hp_) / static_cast<float>((std::max)(1, maxHP_)), threat);
	const Phase phase = rivalCombat_.GetPhase();
	if (phase == Phase::DashWarning && previousPhase != phase) SelectRivalDashDirection(toward);
	if (phase == Phase::Reposition && previousPhase == Phase::Reload) rivalStrafeSign_ = rivalCombat_.GetStrafeSign();
	if (phase == Phase::Dash) {
		if (rivalDashDistance_ <= 0.05f) {
			rivalCombat_.FinishDash();
			velocity_ = {};
		} else {
			const float frames = (std::max)(0.001f, deltaTime * 60.0f);
			const float speed = (std::min)(rivalCombat_.GetDashSpeed(), rivalDashDistance_ / frames);
			velocity_ = rivalDashDirection_ * speed;
			rivalDashDistance_ = (std::max)(0.0f, rivalDashDistance_ - speed * frames);
		}
	} else if (phase == Phase::Reposition || phase == Phase::Tracking || phase == Phase::Reload) {
		const cg2::Vector3 side{ -toward.y * rivalStrafeSign_, toward.x * rivalStrafeSign_, 0.0f };
		const float desiredDistance = phase == Phase::Reload ? 23.0f : 18.0f;
		const float radial = (std::clamp)((distance - desiredDistance) / 9.0f, -0.90f, 1.0f);
		cg2::Vector3 desired = cg2::Normalize(side * (phase == Phase::Reload ? 0.20f : 0.90f) + toward * radial);
		desired = ResolveRivalMove(desired, toward, deltaTime);
		const float speed = phase == Phase::Reload ? 0.035f : (rivalCombat_.IsPhaseTwo() ? 0.18f : 0.15f);
		velocity_ += (desired * speed - velocity_) * (1.0f - std::exp(-2.5f * deltaTime));
	} else {
		// 照準固定・連射・突進予告中は速度を減衰させる。
		// 予告の起点は現在位置を使う一方、固定済みの照準角は行動状態が保持する。
		velocity_ = velocity_ * std::exp(-3.5f * deltaTime);
	}
	dir_ = cg2::Length(velocity_) > 0.001f ? cg2::Normalize(velocity_) : cg2::Vector3{};
	worldTransform_.rotate.z = (phase == Phase::Locked || phase == Phase::Volley || phase == Phase::Tracking)
		? rivalCombat_.GetAimAngle() : std::atan2(toward.y, toward.x);
	if (!shot.fire) return;
	++shotsFired_;
	AttackParam param{};
	param.bulletCount = 1;
	param.randomSpread = false;
	param.bulletSpeed = prototypeTuningEnabled_ ? (std::clamp)(bossAttackConfig_.bulletSpeed, 0.24f, 0.46f) : 0.32f;
	if (shot.pattern == RivalBossCombat::Pattern::FanBurst) param.bulletSpeed *= 0.90f;
	param.damage = prototypeTuningEnabled_ ? bossAttackConfig_.damage : 9u;
	// 遠征ライバルの弾には専用の最低耐久度を適用する。通常AI・試作戦闘の弾設定とは別経路。
	param.bulletHp = (std::max)(tankspecial::kBossEnemyBulletHp, bossAttackConfig_.bulletHp);
	param.bulletPenetration = 3.0f;
	param.reflect = param.penetrate = param.canClaimRunResource = false;
	const cg2::Vector3 base{ std::cos(shot.angle), std::sin(shot.angle), 0.0f };
	for (int i = 0; i < RivalBossCombat::ProjectileCount(shot.pattern); ++i) {
		const cg2::Vector3 direction = RotateDirection2D(base, RivalBossCombat::ProjectileOffset(shot.pattern, i, shot.round, rivalCombat_.GetCapacity()));
		attackController_.FireFromMuzzle(GetWorldPosition() + direction * 1.9f, direction, param, BulletOwner::kEnemy);
	}
}

void Enemy::UpdatePrototypeCombat(float deltaTime)
{
	if (!player_ || !bulletManager_ || isDead_ || hp_ <= 0) return;
	cg2::Vector3 direction = currentMoveTargetPosition_ - GetWorldPosition();
	if (cg2::Length(direction) <= 0.001f) direction = GetAimDirection();
	const float targetAngle = std::atan2(direction.y, direction.x);
	const float hpRatio = static_cast<float>(hp_) / static_cast<float>((std::max)(1, maxHP_));
	const int effectivePressure = (std::clamp)(prototypePressure_ + (enemyLevel_ - 1) / 2, 0, 4);
	const auto shot = prototypeCombat_.Step(deltaTime,
		HasLineOfSightToTarget(currentMoveTargetPosition_), targetAngle, hpRatio,
		effectivePressure, levelingModeActive_, prototypeTuningEnabled_ ? bossAttackConfig_.cooldown / 1.10f : 1.0f);
	if (prototypeCombat_.HoldsPosition()) {
		// 予告・攻撃中はAI速度を0にし、発射角を固定する。追加のノックバック移動はUpdateで別に反映する。
		velocity_ = {};
		worldTransform_.rotate.z = prototypeCombat_.GetAimAngle();
	}
	if (!shot.fire) return;
	++shotsFired_;

	AttackParam param{};
	param.bulletCount = 1;
	param.randomSpread = false;
	param.spreadAngleDeg = 0.0f;
	param.bulletSpeed = (shot.type == PrototypeAttackType::GapRing ? 0.20f : 0.24f) + 0.01f * shot.pressure;
	param.damage = static_cast<uint32_t>(6 + shot.pressure);
    if(prototypeTuningEnabled_) {
        param.bulletSpeed=bossAttackConfig_.bulletSpeed * (shot.type==PrototypeAttackType::GapRing ? 0.84f : 1.0f);
        param.damage=bossAttackConfig_.damage;
    }
	// 攻撃圧力や調整値で威力を変えても、この経路の耐久度・貫通力は固定値を使う。
	param.bulletHp = 5.0f;
	param.bulletPenetration = 3.0f;
	param.reflect = false;
	param.penetrate = false;
	const cg2::Vector3 baseDirection{ std::cos(shot.angleRadians), std::sin(shot.angleRadians), 0.0f };
	int count = PrototypeBossCombat::GetProjectileCount(shot.type);
    if(prototypeTuningEnabled_ && shot.type!=PrototypeAttackType::Sweep)
        count=(std::clamp)(static_cast<int>(std::round(count*bossAttackConfig_.bulletCount/5.0f)),1,64);
	for (int i = 0; i < count; ++i) {
		const cg2::Vector3 shotDirection = RotateDirection2D(baseDirection,
			prototypeTuningEnabled_ ? (count==1 ? 0.0f : shot.type==PrototypeAttackType::GapRing ?
                35.0f+290.0f*static_cast<float>(i)/static_cast<float>(count-1) :
                -22.0f+44.0f*static_cast<float>(i)/static_cast<float>(count-1)) :
                PrototypeBossCombat::GetProjectileOffsetDeg(shot.type, i));
		attackController_.FireFromMuzzle(GetWorldPosition() + shotDirection * 1.75f,
			shotDirection, param, BulletOwner::kEnemy);
	}
}

void Enemy::Update(float deltaTime) {
	if (!runEncounterEnabled_) return;
	// Gameplay death is immediate; only the retained presentation advances afterward.
	if (hp_ <= 0) Die();
	if (isDead_) {
		UpdateDefeatPresentation(deltaTime);
		return;
	}

	UpdateHPBar();
	ApplyDamageFeedback(deltaTime);

	if (expeditionRivalEnabled_) {
		UpdateRivalCombat(deltaTime);
	} else {
		Move(deltaTime);
	}

	if (expeditionRivalEnabled_) {
		// ライバルの発射はUpdateRivalCombatで残弾を消費して行う。通常AIの射撃を重ねない。
	} else if (prototypeCombatEnabled_) {
		UpdatePrototypeCombat(deltaTime);
	} else if (!isDead_){
		if (HasLineOfSightToTarget(currentMoveTargetPosition_)) {
			fireTimer_ -= deltaTime;
			if (fireTimer_ <= 0.0f) {
				ShotgunFire();
				fireTimer_ = kFireTimerMax_;
			}
		} else {

			// 遮蔽中は発射せず、減っていた待ち時間を最大値へ向けて戻す。
			if (fireTimer_ < kFireTimerMax_) {
				fireTimer_ += deltaTime;
			}
		}
	}

	// AI速度と追加ノックバックは60FPS基準。秒数を基準フレーム数へ換算して移動量を求める。
	cg2::Vector3 move = (GetMove() + impactVelocity_) * (deltaTime * 60.0f);
	impactVelocity_ = impactVelocity_ * std::exp(-5.0f * deltaTime);
	const float maxStep = 0.35f;
	const int subStepCount = (std::max)(1, static_cast<int>((std::max)(std::abs(move.x), std::abs(move.y)) / maxStep) + 1);
	cg2::Vector3 stepMove = move / static_cast<float>(subStepCount);
	// 各小刻み移動でXの補正後にYを進める。地形側は位置を補正し、速度停止はここで判断する。
	for (int i = 0; i < subStepCount; ++i) {
		const cg2::Vector3 before = GetWorldPosition();
		cg2::Vector3 pos = GetWorldPosition();
		pos.x += stepMove.x;
		SetWorldPosition(pos);
		stage_->ResolveEnemyCollision(*this, cg2::X);

		pos = GetWorldPosition();
		pos.y += stepMove.y;
		SetWorldPosition(pos);
		stage_->ResolveEnemyCollision(*this, cg2::Y);
		if (expeditionRivalEnabled_ && rivalCombat_.GetPhase() == RivalBossCombat::Phase::Dash &&
			cg2::Length(GetWorldPosition() - (before + stepMove)) > 0.015f) {
			// 意図した位置と地形補正後の位置が離れたら突進を終了する。
			// 通常突進は再装填、弾への反応による回避突進は位置取りへ戻る。
			rivalCombat_.FinishDash();
			velocity_ = {};
			rivalDashDistance_ = 0.0f;
			break;
		}
	}

	object_->SetTransform(worldTransform_);
	object_->Update();

	if (isExploding_) {
		UpdateParticles(deltaTime);
	}

	sprite->SetSize(cg2::Vector2(float(hp_), sprite->GetSize().y));
	sprite->Update();
	bossHpRed->Update();
	bossHpFont->Update();
}

void Enemy::Draw(bool drawBody) {
	if (drawBody) {
		DrawBodyOnly();
	}
}

void Enemy::DrawBodyOnly() {
	if (!runEncounterEnabled_) return;
	if (isDead_) {
		if (!isExploding_) {
			return;
		}

		const float progress = deathChargeDuration_ > 0.0f
			? (std::clamp)(1.0f - deathChargeTimer_ / deathChargeDuration_, 0.0f, 1.0f)
			: 1.0f;
		const float charge = deathChargeTimer_ > 0.0f
			? std::sin(progress * 3.1415926535f)
			: 0.0f;
		cg2::Transform chargeTransform = worldTransform_;
		chargeTransform.scale = worldTransform_.scale * (1.0f + charge * 0.18f);
		const cg2::Vector4 savedColor = object_->GetColor();
		const bool savedLighting = object_->IsLightingEnabled();
		object_->SetTransform(chargeTransform);
		object_->SetLighting(false);
		const cg2::Vector4 dissolveBodyColor{ 0.72f, 0.08f, 0.035f, savedColor.w };
		object_->SetColor({
			dissolveBodyColor.x + (1.6f - dissolveBodyColor.x) * charge,
			dissolveBodyColor.y + (1.6f - dissolveBodyColor.y) * charge,
			dissolveBodyColor.z + (1.6f - dissolveBodyColor.z) * charge,
			savedColor.w });
		object_->Update();
		object_->Draw();
		object_->SetTransform(worldTransform_);
		object_->SetColor(savedColor);
		object_->SetLighting(savedLighting);
		object_->Update();
		return;
	}

	if (!isDead_) {
		object_->Draw();
	}
}

void Enemy::DrawSprite()
{
	if (!runEncounterEnabled_) return;

}

void Enemy::ApproachToPlayer(cg2::Vector3& startPos, cg2::Vector3& targetPos) {

	// 自キャラの位置を取得
	cg2::Vector3 playerPos = player_->GetWorldPosition();
	// 敵キャラのワールド座標を取得
	cg2::Vector3 enemyPos = GetWorldPosition();

	startPos = enemyPos;
	targetPos = playerPos;
}

cg2::Vector3 Enemy::GetWorldPosition() const {
	// ワールド座標を入れる
	cg2::Vector3 worldPos;
	// ワールド行列の平行移動成分を取得(ワールド座標)
	worldPos.x = worldTransform_.translate.x;
	worldPos.y = worldTransform_.translate.y;
	worldPos.z = worldTransform_.translate.z;

	return worldPos;
}

void Enemy::OnCollision(Collider* other) {
	if (!runEncounterEnabled_ || isDead_ || hp_ <= 0 || !other) return;

	if (other->GetCollisionAttribute() == kCollisionAttributeExpEnemy) {
		if (!enemyProgressConfig_.expEnemyHostile) {
			return;
		}
		ExpEnemy* expEnemy = dynamic_cast<ExpEnemy*>(other);
		if (!expEnemy || expEnemy->IsDead() || expEnemy->IsRunResource()) {
			return;
		}
		cg2::Vector3 dir = worldTransform_.translate - other->GetWorldPosition();
		if (cg2::Length(dir) > 0.001f) {
			velocity_ += cg2::Normalize(dir) * 0.05f;
		}
		const bool killed = expEnemy->TakeDamageFromEnemy(enemyProgressConfig_.expEnemyContactDamage);
		if (killed) {
			RegisterExpEnemyKill(expEnemy->GetExpValue());
		}
		return;
	}

	// 自機・ドローンとの接触は離れる方向の速度を加える。位置は次のUpdateで進める。
	if (other->GetCollisionAttribute() == kCollisionAttributePlayer || other->GetCollisionAttribute() == kCollisionAttributePlayerDrone) {
		cg2::Vector3 dir = worldTransform_.translate - other->GetWorldPosition();

		if (cg2::Length(dir) < 0.001f) return;

		dir = cg2::Normalize(dir);

		const float knockPower = 0.18f;

		velocity_ += dir * knockPower * other->GetHitPower();
		const float maxKnockSpeed = 0.22f;
		if (cg2::Length(velocity_) > maxKnockSpeed) {
			velocity_ = cg2::Normalize(velocity_) * maxKnockSpeed;
		}
	}
	// 全てのダメージ経路で、致死通知中にGameplayを停止する。
	if (other->GetCollisionAttribute() == kCollisionAttributePlayerBullet ||
		other->GetCollisionAttribute() == kCollisionAttributeHostileExpEnemyBullet) {

		TakeDamage(other->GetDamage());
	}
}

void Enemy::TakeDamage(uint32_t amount)
{
	if (!runEncounterEnabled_ || isDead_ || hp_ <= 0 || amount == 0) {
		return;
	}
	// Compare unsigned damage before narrowing, including amounts above INT_MAX.
	hp_ = amount >= static_cast<uint32_t>(hp_) ? 0 : hp_ - static_cast<int>(amount);
	TriggerDamageFeedback();
	if (hp_ == 0) Die();
}

void Enemy::ApplyKnockback(const cg2::Vector3& direction, float power)
{
	if (!runEncounterEnabled_ || isDead_ || !std::isfinite(power) || power <= 0.0f || cg2::Length(direction) < 0.001f) return;
	// AI速度とは別の押し返し速度を加える。行動時計や固定済みの攻撃は中断しない。
	impactVelocity_ += cg2::Normalize(direction) * ((std::min)(0.95f,power) * 0.35f);
	if (cg2::Length(impactVelocity_) > 0.34f) impactVelocity_ = cg2::Normalize(impactVelocity_) * 0.34f;
}

void Enemy::TriggerDamageFeedback()
{
	damageFeedbackTimer_ = damageFeedbackDuration_;
}

void Enemy::ApplyDamageFeedback(float deltaTime)
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
		baseScale_ * (1.0f + impact * 0.07f);

	object_->SetColor(
		LerpColor(
			baseColor_,
			{ 1.0f, 1.0f, 1.0f, baseColor_.w },
			(std::min)(1.0f, impact * 0.95f)));
}

void Enemy::AIStateMovePower() {
	switch (aiState_) {
	case AIState::Attack:
		attackPower = 1.0f;
		evadePower = 0.9f;
		wanderPower = 0.1f;
		break;

	case AIState::Wander:
		attackPower = 0.3f;
		evadePower = 1.2f;
		wanderPower = 0.5f;
		break;

	case AIState::Evade:
		attackPower = 0.1f;
		evadePower = 3.0f;
		wanderPower = 0.3f;
		break;
	}
}

void Enemy::Move(float deltaTime) {
	if (!runEncounterEnabled_ || isDead_ || hp_ <= 0) {
		velocity_ = {};
		return;
	}

	UpdateAIState();

	AIStateMovePower();

	const cg2::Vector3 moveTargetPos = ResolveMoveTargetPosition();
	currentMoveTargetPosition_ = moveTargetPos;
	cg2::Vector3 toTarget = moveTargetPos - GetWorldPosition();
	toTarget.z = 0.0f;
	if (prototypeResourceTargetActive_ && cg2::Length(toTarget) <= 8.0f && HasLineOfSightToTarget(moveTargetPos)) {
		// 見通しのある資源を8ワールド単位以内で狙う場合はAI移動を止め、資源を通り越して撃たない。
		velocity_ = {};
		RotateTowardTarget(moveTargetPos, deltaTime);
		return;
	}
	cg2::Vector3 attackVec = cg2::Length(toTarget) > 0.001f ? cg2::Normalize(toTarget) * attackPower : cg2::Vector3{ 0.0f, 0.0f, 0.0f };
	std::optional<cg2::Vector3> pathDir = FindPathDirectionToTarget(moveTargetPos);

	if (pathDir) {
		attackVec = *pathDir * 1.7f;
		wanderVec = { 0.0f, 0.0f, 0.0f };
	} else {
		wanderChangeTimer -= deltaTime;
		if (wanderChangeTimer <= 0.0f) {
			wanderVec = RandomDirection() * wanderPower * 0.25f;
			wanderChangeTimer = 3.0f;
		}
	}

	evadeVec = EvadeBullets() * evadePower * 0.35f;
	dir_ = attackVec + evadeVec + wanderVec;

	if (cg2::Length(dir_) > 0.0001f) {
		dir_ = cg2::Normalize(dir_);
	} else {
		dir_ = { 0, 0, 0 };
	}

	dir_.z = 0.0f;
	dir_ = ApplyHumanLikeSteering(dir_, pathDir.has_value(), deltaTime);

	RotateTowardTarget(moveTargetPos, deltaTime);

	const float speed = pathDir ? 0.10f : maxSpeed_;
	cg2::Vector3 targetVelocity = dir_ * speed;
	velocity_ += (targetVelocity - velocity_) * 0.22f * (deltaTime * 60.0f);
}

void Enemy::RotateTowardTarget(const cg2::Vector3& targetPos, float deltaTime)
{
	cg2::Vector3 toTarget = targetPos - GetWorldPosition();
	toTarget.z = 0.0f;
	if (cg2::Length(toTarget) < 0.001f) {
		return;
	}

	// 短い側の角度差（ラジアン）を使い、180度の旋回に必要な秒数から1回の上限を求める。
	const float targetAngle = std::atan2(toTarget.y, toTarget.x);
	const float currentAngle = worldTransform_.rotate.z;
	const float angleDiff = NormalizeAngleRad(targetAngle - currentAngle);
	const float halfTurnSeconds = (std::max)(0.05f, enemyProgressConfig_.aimTurnHalfSeconds);
	const float maxStep = 3.1415926535f * (deltaTime / halfTurnSeconds);
	const float step = (std::clamp)(angleDiff, -maxStep, maxStep);
	worldTransform_.rotate.z = NormalizeAngleRad(currentAngle + step);
	object_->SetRotate(worldTransform_.rotate);
}

cg2::Vector3 Enemy::ApplyHumanLikeSteering(const cg2::Vector3& desiredDir, bool usingPath, float deltaTime)
{
	if (cg2::Length(desiredDir) < 0.001f) {
		return desiredDir;
	}

	steeringNoiseTimer_ -= deltaTime;
	if (steeringNoiseTimer_ <= 0.0f) {
		cg2::Vector3 side = { -desiredDir.y, desiredDir.x, 0.0f };
		float noisePower = usingPath ? cg2::Rand(-0.22f, 0.22f) : cg2::Rand(-0.12f, 0.12f);
		steeringNoise_ = side * noisePower;
		steeringNoiseTimer_ = cg2::Rand(0.25f, 0.75f);
	}

	hesitationCooldown_ -= deltaTime;
	if (hesitationCooldown_ <= 0.0f && usingPath && cg2::Rand(0.0f, 1.0f) < 0.18f) {
		hesitationTimer_ = cg2::Rand(0.08f, 0.18f);
		hesitationCooldown_ = cg2::Rand(1.2f, 2.6f);
	}

	float response = usingPath ? 0.10f : 0.16f;
	cg2::Vector3 targetDir = desiredDir + steeringNoise_;
	if (cg2::Length(targetDir) > 0.001f) {
		targetDir = cg2::Normalize(targetDir);
	}

	steeringDir_ += (targetDir - steeringDir_) * response * (deltaTime * 60.0f);
	if (cg2::Length(steeringDir_) > 0.001f) {
		steeringDir_ = cg2::Normalize(steeringDir_);
	} else {
		steeringDir_ = targetDir;
	}

	if (hesitationTimer_ > 0.0f) {
		hesitationTimer_ -= deltaTime;
		return steeringDir_ * 0.35f;
	}

	return steeringDir_;
}

std::optional<cg2::Vector3> Enemy::FindPathDirectionToPlayer()
{
	return FindPathDirectionToTarget(player_->GetWorldPosition());
}

cg2::Vector3 Enemy::ResolveMoveTargetPosition()
{
	prototypeResourceTargetActive_ = false;
	if (!player_) {
		levelingModeActive_ = false;
		return GetWorldPosition();
	}

	const cg2::Vector3 playerPos = player_->GetWorldPosition();
	if (!enemyProgressConfig_.expEnemyHostile || !enemyProgressConfig_.levelingModeEnabled || !enemyManager_) {
		levelingModeActive_ = false;
		return playerPos;
	}

	const float playerDistance = cg2::Length(playerPos - GetWorldPosition());
	if (levelingModeActive_) {
		if (playerDistance <= enemyProgressConfig_.levelingExitPlayerDistance) {
			levelingModeActive_ = false;
			return playerPos;
		}
	} else if (playerDistance >= enemyProgressConfig_.levelingEnterPlayerDistance) {
		levelingModeActive_ = true;
	}

	if (!levelingModeActive_) {
		return playerPos;
	}

	const bool resourceFocus = prototypeCombatEnabled_ && prototypeResourceFocus_;
	ExpEnemy* target = resourceFocus
		? enemyManager_->FindNearestRunResource(GetWorldPosition(), enemyProgressConfig_.levelingSearchRadius)
		: nullptr;
	if (!target) target = enemyManager_->FindNearestEnemy(GetWorldPosition(), enemyProgressConfig_.levelingSearchRadius, !resourceFocus);
	if (!target) {
		levelingModeActive_ = false;
		return playerPos;
	}

	prototypeResourceTargetActive_ = resourceFocus && target->IsRunResource();
	return target->GetWorldPosition();
}

std::optional<cg2::Vector3> Enemy::FindPathDirectionToTarget(const cg2::Vector3& targetPos)
{
	if (!stage_ || HasClearMoveRouteToTarget(targetPos)) {
		return std::nullopt;
	}

	std::optional<MapIndex> startOpt = WorldToMapIndex(GetWorldPosition());
	std::optional<MapIndex> goalOpt = WorldToMapIndex(targetPos);
	if (!startOpt || !goalOpt) {
		return std::nullopt;
	}

	const MapIndex start = FindNearestPathPassableCell(*startOpt);
	const MapIndex goal = FindNearestPathPassableCell(*goalOpt);
	const int width = static_cast<int>(MapChip::kNumBlockHorizontal);
	const int height = static_cast<int>(MapChip::kNumBlockVirtical);
	const int nodeCount = width * height;
	auto ToId = [width](const MapIndex& p) { return p.y * width + p.x; };
	auto Heuristic = [](const MapIndex& a, const MapIndex& b) {
		return std::abs(a.x - b.x) + std::abs(a.y - b.y);
	};

	/// @brief 探索セルの一次元IDと、到達コストに推定残距離を足した優先度を保持する。
	struct QueueNode {
		int id = 0;
		int f = 0;
		/// @brief 探索優先度fを比較する。キューでは小さいfの候補を先に取り出す。
		bool operator>(const QueueNode& other) const { return f > other.f; }
	};

	std::vector<int> cameFrom(nodeCount, -1);
	std::vector<int> cost(nodeCount, std::numeric_limits<int>::max());
	std::priority_queue<QueueNode, std::vector<QueueNode>, std::greater<QueueNode>> open;

	const int startId = ToId(start);
	const int goalId = ToId(goal);
	cost[startId] = 0;
	open.push({ startId, Heuristic(start, goal) });

	// 8近傍を探索する。斜め移動は隣の縦横セルも通れる場合に限り、角をすり抜けない。
	const std::array<MapIndex, 8> dirs = {
		MapIndex{ 1, 0 }, MapIndex{ -1, 0 }, MapIndex{ 0, 1 }, MapIndex{ 0, -1 },
		MapIndex{ 1, 1 }, MapIndex{ 1, -1 }, MapIndex{ -1, 1 }, MapIndex{ -1, -1 }
	};

	while (!open.empty()) {
		QueueNode current = open.top();
		open.pop();
		if (current.id == goalId) {
			break;
		}

		MapIndex currentPos{ current.id % width, current.id / width };
		for (const MapIndex& d : dirs) {
			MapIndex next{ currentPos.x + d.x, currentPos.y + d.y };
			if (next.x < 0 || next.y < 0 || next.x >= width || next.y >= height) {
				continue;
			}
			if (!IsPathPassableCell(next.x, next.y) && ToId(next) != goalId) {
				continue;
			}
			if (d.x != 0 && d.y != 0 && (!IsPathPassableCell(currentPos.x + d.x, currentPos.y) || !IsPathPassableCell(currentPos.x, currentPos.y + d.y))) {
				continue;
			}

			const int nextId = ToId(next);
			const int moveCost = (d.x != 0 && d.y != 0) ? 14 : 10;
			const int newCost = cost[current.id] + moveCost;
			if (newCost >= cost[nextId]) {
				continue;
			}

			cost[nextId] = newCost;
			cameFrom[nextId] = current.id;
			open.push({ nextId, newCost + Heuristic(next, goal) * 10 });
		}
	}

	if (cameFrom[goalId] < 0) {
		return std::nullopt;
	}

	int currentId = goalId;
	int previousId = goalId;
	while (cameFrom[currentId] >= 0 && cameFrom[currentId] != startId) {
		currentId = cameFrom[currentId];
		previousId = currentId;
	}

	if (cameFrom[currentId] == startId) {
		previousId = currentId;
	}

	MapIndex next{ previousId % width, previousId / width };
	cg2::Vector3 waypoint = MapIndexToWorld(next);
	cg2::Vector3 toWaypoint = waypoint - GetWorldPosition();
	toWaypoint.z = 0.0f;
	if (cg2::Length(toWaypoint) < 0.1f) {
		return std::nullopt;
	}
	return cg2::Normalize(toWaypoint);
}

std::optional<Enemy::MapIndex> Enemy::WorldToMapIndex(const cg2::Vector3& pos) const
{
	// blocks_[y][x]に対応させる。列は右向き、CSVの行は下向きなのでワールドYを反転する。
	const int x = static_cast<int>(std::round(pos.x / MapChip::kBlockWidth));
	const int y = static_cast<int>(MapChip::kNumBlockVirtical - 1 - std::round(pos.y / MapChip::kBlockHeight));
	if (x < 0 || y < 0 || x >= static_cast<int>(MapChip::kNumBlockHorizontal) || y >= static_cast<int>(MapChip::kNumBlockVirtical)) {
		return std::nullopt;
	}
	return MapIndex{ x, y };
}

cg2::Vector3 Enemy::MapIndexToWorld(const MapIndex& index) const
{
	return {
		MapChip::kBlockWidth * static_cast<float>(index.x),
		MapChip::kBlockHeight * static_cast<float>(MapChip::kNumBlockVirtical - 1 - index.y),
		0.0f
	};
}

bool Enemy::IsPassableCell(int x, int y) const
{
	if (!stage_) {
		return true;
	}
	if (x <= 0 || y <= 0 || x >= static_cast<int>(MapChip::kNumBlockHorizontal) - 1 || y >= static_cast<int>(MapChip::kNumBlockVirtical) - 1) {
		return false;
	}

	const auto& blocks = stage_->GetBlocks();
	if (y < 0 || y >= static_cast<int>(blocks.size()) || x < 0 || x >= static_cast<int>(blocks[y].size())) {
		return false;
	}

	return !blocks[y][x].isActive;
}

bool Enemy::IsPathPassableCell(int x, int y) const
{
	// 機体半幅を覆うセル数へ切り上げ、周辺の正方形領域も空いていることを求める。
	const int clearance = static_cast<int>(std::ceil((kWidth * 0.5f) / MapChip::kBlockWidth));
	for (int offsetY = -clearance; offsetY <= clearance; ++offsetY) {
		for (int offsetX = -clearance; offsetX <= clearance; ++offsetX) {
			if (!IsPassableCell(x + offsetX, y + offsetY)) {
				return false;
			}
		}
	}
	return true;
}

Enemy::MapIndex Enemy::FindNearestPathPassableCell(const MapIndex& base) const
{
	if (IsPathPassableCell(base.x, base.y)) {
		return base;
	}

	const int width = static_cast<int>(MapChip::kNumBlockHorizontal);
	const int height = static_cast<int>(MapChip::kNumBlockVirtical);
	for (int radius = 1; radius < 8; ++radius) {
		MapIndex best = base;
		float bestDistance = std::numeric_limits<float>::max();
		for (int y = base.y - radius; y <= base.y + radius; ++y) {
			for (int x = base.x - radius; x <= base.x + radius; ++x) {
				if (x < 0 || y < 0 || x >= width || y >= height || !IsPathPassableCell(x, y)) {
					continue;
				}
				cg2::Vector3 candidateWorld = MapIndexToWorld({ x, y });
				float distance = cg2::Length(candidateWorld - MapIndexToWorld(base));
				if (distance < bestDistance) {
					bestDistance = distance;
					best = { x, y };
				}
			}
		}
		if (bestDistance < std::numeric_limits<float>::max()) {
			return best;
		}
	}

	return base;
}

bool Enemy::HasClearMoveRouteToPlayer() const
{
	return HasClearMoveRouteToTarget(player_->GetWorldPosition());
}

bool Enemy::HasClearMoveRouteToTarget(const cg2::Vector3& targetPos) const
{
	if (!stage_) {
		return false;
	}

	const cg2::Vector3 enemyPos = GetWorldPosition();
	cg2::Vector3 toTarget = targetPos - enemyPos;
	toTarget.z = 0.0f;
	const float distance = cg2::Length(toTarget);
	if (distance < 0.001f) {
		return true;
	}

	cg2::Vector3 normal = cg2::Normalize(toTarget);
	cg2::Vector3 side = { -normal.y, normal.x, 0.0f };
	const float halfWidth = kWidth * 0.5f + 0.25f;
	// 中央と左右の線分で機体幅の通行を近似する。線分AABB判定は端点の接触も含む。
	const std::array<cg2::Vector3, 3> origins = {
		enemyPos,
		enemyPos + side * halfWidth,
		enemyPos - side * halfWidth
	};

	for (const cg2::Vector3& origin : origins) {
		cg2::Segment ray;
		ray.origin = origin;
		ray.diff = toTarget;

		for (const auto& row : stage_->GetBlocks()) {
			for (const Block& block : row) {
				if (!block.isActive) {
					continue;
				}
				if (cg2::IsCollision(block.aabb, ray)) {
					return false;
				}
			}
		}
	}

	return true;
}

cg2::Vector3 Enemy::RandomDirection() { return cg2::Rand(cg2::Vector3(-0.2f, -0.2f, 0.0f), cg2::Vector3(0.2f, 0.2f, 0.5f)); }

cg2::Vector3 Enemy::EvadeBullets() {
	cg2::Vector3 evade(0, 0, 0);

	for (Bullet* bullet : bulletManager_->GetBulletPtrs()) {

		// 敵の弾は無視
		if (bullet->GetCollisionAttribute() == kCollisionAttributeEnemyBullet) {
			continue;
		}

		cg2::Vector3 toBullet = bullet->GetWorldPosition() - GetWorldPosition();
		float dist = cg2::Length(toBullet);

		const float kEvadeRadius = 10.0f;
		if (dist < kEvadeRadius) {

			toBullet *= -1.0f;
			cg2::Vector3 dir = cg2::Normalize(toBullet);
			float power = (kEvadeRadius - dist) / kEvadeRadius;

			evade += dir * power * 0.2f;
		}
	}

	return evade;
}

void Enemy::UpdateAIState() {

	cg2::Vector3 toPlayer = player_->GetWorldPosition() - GetWorldPosition();
	float dist = cg2::Length(toPlayer);

	// 敵弾属性だけを除き、8ワールド単位未満の弾を回避のきっかけにする。接近速度や死亡は検査しない。
	bool bulletDanger = false;
	for (Bullet* bullet : bulletManager_->GetBulletPtrs()) {

		// 敵の弾は無視
		if (bullet->GetCollisionAttribute() == kCollisionAttributeEnemyBullet) {
			continue;
		}

		float distB = cg2::Length(bullet->GetWorldPosition() - GetWorldPosition());
		if (distB < 8.0f) {
			bulletDanger = true;
			break;
		}
	}

	// ① 弾が危険 → Evade 強制
	if (bulletDanger) {
		aiState_ = AIState::Evade;
		return;
	}

	// ② 距離による切り替え
	if (dist > 25.0f) {
		aiState_ = AIState::Wander; // 遠い → 探しながら徘徊
	} else if (dist > 10.0f) {
		aiState_ = AIState::Attack; // 中距離 → 攻撃に近づく
	} else {
		aiState_ = AIState::Evade; // 近すぎる → 逃げる
	}
}

cg2::AABB Enemy::GetAABB() {
	cg2::Vector3 worldPos = GetWorldPosition();

	cg2::AABB aabb;

	aabb.min = { worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f };
	aabb.max = { worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f };

	return aabb;
}

cg2::Segment Enemy::MakeForwardRay(float length) const {

	cg2::Segment seg;
	seg.origin = GetWorldPosition();

	cg2::Vector3 forward = dir_;
	if (cg2::Length(forward) < 0.001f) {
		forward = { 1, 0, 0 }; // 保険
	}

	forward = cg2::Normalize(forward);

	seg.diff = forward * length; // 線分の差分

	return seg;
}

bool Enemy::IsBlockNearByRay() {

	const float kRayLength = 3.0f;

	cg2::Segment ray = MakeForwardRay(kRayLength);

	for (const auto& row : stage_->GetBlocks()) {
		for (const Block& block : row) {

			if (!block.isActive) {
				continue;
			}

			if (cg2::IsCollision(block.aabb, ray)) {
				return true;
			}
		}
	}

	return false;
}

cg2::Vector3 Enemy::WallAvoidByRay() {

	cg2::Vector3 avoid(0, 0, 0);

	if (cg2::Length(dir_) < 0.001f) {
		return avoid;
	}

	cg2::Vector3 forward = cg2::Normalize(dir_);

	if (IsBlockNearByRay() && !isWallFollowing_) {

		cg2::Vector3 left(-forward.y, forward.x, 0);
		cg2::Vector3 right(forward.y, -forward.x, 0);

		float leftScore = ScoreDir(left);
		float rightScore = ScoreDir(right);


		isWallFollowing_ = true;
		wallFollowTimer_ = 1.2f; // この時間は壁沿い優先
		wallFollowDir_ = (leftScore < rightScore) ? left : right;

		// 正面ブレーキ
		avoid += -forward * 1.5f;

		// 横へ逃がす（壁沿い移動）
		cg2::Vector3 side(-forward.y, forward.x, 0.0f);
		avoid += side * 1.0f;
	}

	return avoid;
}

float Enemy::ScoreDir(const cg2::Vector3& dir)
{
	cg2::Vector3 futurePos = GetWorldPosition() + dir * 3.0f;
	return cg2::Length(player_->GetWorldPosition() - futurePos);
}

cg2::Segment Enemy::MakeRayToPlayer() const {

	cg2::Segment seg;
	seg.origin = GetWorldPosition();

	cg2::Vector3 toPlayer = player_->GetWorldPosition() - seg.origin;
	seg.diff = toPlayer; // プレイヤーまで

	return seg;
}

bool Enemy::HitPlayerByRay(const cg2::Segment& ray)
{

	cg2::Sphere playerSphere;
	playerSphere.center = player_->GetWorldPosition();
	playerSphere.radius = player_->GetRadius();

	return cg2::IsCollision(ray, playerSphere);
}

bool Enemy::HasLineOfSightToPlayer() const {
	return HasLineOfSightToTarget(player_->GetWorldPosition());
}

bool Enemy::HasLineOfSightToTarget(const cg2::Vector3& targetPos) const {

	if (!stage_) {
		return false;
	}

	cg2::Segment ray;
	ray.origin = GetWorldPosition();
	ray.diff = targetPos - ray.origin;

	float targetDist = cg2::Length(ray.diff);
	if (targetDist < 0.001f) {
		return false;
	}

	// 手前に壁がある？
	for (const auto& row : stage_->GetBlocks()) {
		for (const Block& block : row) {

			if (!block.isActive) {
				continue;
			}

			if (cg2::IsCollision(block.aabb, ray)) {

				// 交点までの距離ではなくブロック中心を比べる近似。目標より中心が遠いブロックは遮蔽にしない。
				float blockDist =
					cg2::Length((block.aabb.max + block.aabb.min) / 2.0f - ray.origin);

				if (blockDist < targetDist) {
					return false; // 壁に遮られている
				}
			}
		}
	}

	return true; // 視線が通っている
}

void Enemy::Die()
{
	if (!runEncounterEnabled_ || isDead_) return;

	isDead_ = true;
	hp_ = 0;
	velocity_ = dir_ = impactVelocity_ = {};
	rivalDashDistance_ = 0.0f;
	levelingModeActive_ = prototypeResourceTargetActive_ = false;
	isExploding_ = true;

	SpawnParticles();


	radius_ = 0.0f;
}

bool Enemy::isFinished()
{
	if (isDead_ && !isExploding_) {
		return true;
	}
	return false;
}

void Enemy::UpdateDefeatPresentation(float deltaTime)
{
	if (!runEncounterEnabled_) return;
	if (isExploding_) {
		UpdateParticles((std::max)(0.0f, deltaTime));
	}
}

void Enemy::SpawnParticles()
{
	cg2::Vector3 center = GetWorldPosition();
	cg2::ParticleManager::GetInstance()->EmitNeonDeathEffect(
		center,
		{ 1.55f, 0.22f, 0.48f, 1.0f },
		{ 1.30f, 0.92f, 0.18f, 0.0f },
		1.35f);
	deathChargeTimer_ = deathChargeDuration_;
	deathEffectTimer_ = 2.0f;
}

void Enemy::UpdateParticles(float deltaTime)
{
	deathChargeTimer_ = (std::max)(0.0f, deathChargeTimer_ - deltaTime);
	deathEffectTimer_ -= deltaTime;
	if (deathEffectTimer_ <= 0.0f) {
		isExploding_ = false;
	}
}

void Enemy::UpdateHPBar()
{
	cg2::Vector3 enemyPos = GetWorldPosition();
	cg2::Vector3 barOffset = { -2.0f, -2.5f, 0.0f }; // ボスの位置を基準に左下へ配置

	// 背景の更新
	hpBarBGTransform_.translate = enemyPos + barOffset;
	hpBarBG_->SetTransform(hpBarBGTransform_);
	hpBarBG_->Update();

	// ゲージ中身の更新
	float hpPercent = (float)hp_ / (float)maxHP_;
	hpBarFillTransform_.translate = enemyPos + barOffset;
	// XスケールだけHP割合にする
	hpBarFillTransform_.scale.x = 1.0f * hpPercent;

	// 位置は背景と同じ中心を保つ。左端を固定する位置補正は行わない。


	hpBarFill_->SetTransform(hpBarFillTransform_);
	hpBarFill_->Update();

}

void Enemy::HPBarDraw()
{
	if (runEncounterEnabled_ && !isDead_) {
		hpBarBG_->Draw();
		hpBarFill_->Draw();
	}
}
