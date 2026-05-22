#include "Enemy.h"
#include "Calculation.h"
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

Enemy::~Enemy() {}

void Enemy::Fire() {

	assert(player_);

	// 弾の速度
	const float kBulletSpeed = 0.35f;

	// 自キャラの位置を取得
	Vector3 playerPos = player_->GetWorldPosition();
	// 敵キャラのワールド座標を取得
	Vector3 enemyPos = GetWorldPosition();
	// 敵キャラから自キャラへのベクトルを求める
	Vector3 direction = playerPos - enemyPos;
	// ベクトルの正規化
	direction = Normalize(direction);
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


	attackController_.Fire(
		worldTransform_.translate,
		direction,
		attackParam,
		BulletOwner::kEnemy
	);
}

void Enemy::ShotgunFire()
{
	assert(player_);

	// 発射位置
	Vector3 origin = GetWorldPosition();

	// 基準方向
	Vector3 baseDir;
	baseDir = Normalize(player_->GetWorldPosition() - origin);

	// 攻撃パラメータを設定
	AttackParam param{};
	param.bulletSpeed = 0.4f;
	param.bulletCount = 2;
	param.spreadAngleDeg = 30.0f;
	param.randomSpread = true;

	param.reflect = false;
	param.penetrate = false;
	param.cooldown = 2.0f;
	param.damage = 10;

	// 発射
	attackController_.Fire(
		origin,
		baseDir,
		param,
		BulletOwner::kEnemy
	);
}

void Enemy::Initialize(Object3d* object, const Vector3& position, Stage* stage) {

	hpBarFill_ = std::make_unique<Object3d>();
	hpBarFill_->Initialize();
	hpBarFill_->GetScale().y = 1.2f;
	hpBarFill_->SetModel("playerHPBarGreenLong.obj");

	hpBarFillTransform_ = InitWorldTransform();
	hpBarFillTransform_.translate = position;
	hpBarFill_->SetTransform(worldTransform_);
	hpBarFill_->Update();


	hpBarBG_ = std::make_unique<Object3d>();
	hpBarBG_->Initialize();
	hpBarBG_->GetScale().y = 1.2f;

	hpBarBG_->SetModel("playerHPBarLong.obj");

	hpBarBGTransform_ = InitWorldTransform();
	hpBarBGTransform_.translate = position;
	hpBarBG_->SetTransform(worldTransform_);
	hpBarBG_->Update();

	object_ = object;
	worldTransform_ = InitWorldTransform();
	worldTransform_.translate = position;
	baseScale_ = worldTransform_.scale;
	baseColor_ = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
	object_->SetColor(baseColor_);

	// 衝突属性を設定
	SetCollisionAttribute(kCollisionAttributeEnemy);
	// 衝突対象をプレイヤーとプレイヤーの弾に設定
	SetCollisionMask(kCollisionAttributePlayer | kCollisionAttributePlayerBullet | kCollisionAttributePlayerDrone);

	stage_ = stage;
	
	sprite = std::make_unique<Sprite>();
	sprite->Initialize(SpriteCommon::GetInstance(), "resources/bossHPGreen.png");
	sprite->SetPosition({ 20.0f, 210.0f });
	
	bossHpRed = std::make_unique<Sprite>();
	bossHpRed->Initialize(SpriteCommon::GetInstance(), "resources/bossHPRed.png");
	bossHpRed->SetPosition({ 20.0f, 210.0f });

	bossHpFont = std::make_unique<Sprite>();
	bossHpFont->Initialize(SpriteCommon::GetInstance(), "resources/BossHp.png");
	bossHpFont->SetPosition({ 20.0f, 160.0f });
	bossHpFont->SetSize({120.0f, 40.0f});

}

void Enemy::Update(float deltaTime) {

	UpdateHPBar();
	ApplyDamageFeedback(deltaTime);

	Move(deltaTime);

	if (!isDead_){
		if (HasLineOfSightToPlayer()) {
			fireTimer_ -= deltaTime;
			if (fireTimer_ <= 0.0f) {
				ShotgunFire();
				fireTimer_ = kFireTimerMax_;
			}
		} else {

			// 視線が通っていない場合、少しづつタイマーを回復
			if (fireTimer_ < kFireTimerMax_) {
				fireTimer_ += deltaTime;
			}
		}
	}

	Vector3 move = GetMove() * (deltaTime * 60.0f);
	const float maxStep = 0.35f;
	const int subStepCount = (std::max)(1, static_cast<int>((std::max)(std::abs(move.x), std::abs(move.y)) / maxStep) + 1);
	Vector3 stepMove = move / static_cast<float>(subStepCount);
	for (int i = 0; i < subStepCount; ++i) {
		Vector3 pos = GetWorldPosition();
		pos.x += stepMove.x;
		SetWorldPosition(pos);
		stage_->ResolveEnemyCollision(*this, X);

		pos = GetWorldPosition();
		pos.y += stepMove.y;
		SetWorldPosition(pos);
		stage_->ResolveEnemyCollision(*this, Y);
	}

	object_->SetTransform(worldTransform_);
	object_->Update();

	if (hp_ <= 0) {
		Die();
		hp_ = 0;
	}
	if (isExploding_) {
		UpdateParticles(deltaTime);
	}

	sprite->SetSize(Vector2(float(hp_), sprite->GetSize().y));
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
	if (!isDead_) {
		object_->Draw();
	}
}

void Enemy::DrawSprite()
{
	//bossHpRed->Draw();
	//sprite->Draw();
	//bossHpFont->Draw();
}

void Enemy::ApproachToPlayer(Vector3& startPos, Vector3& targetPos) {

	// 自キャラの位置を取得
	Vector3 playerPos = player_->GetWorldPosition();
	// 敵キャラのワールド座標を取得
	Vector3 enemyPos = GetWorldPosition();

	startPos = enemyPos;
	targetPos = playerPos;
}

Vector3 Enemy::GetWorldPosition() const {
	// ワールド座標を入れる
	Vector3 worldPos;
	// ワールド行列の平行移動成分を取得(ワールド座標)
	worldPos.x = worldTransform_.translate.x;
	worldPos.y = worldTransform_.translate.y;
	worldPos.z = worldTransform_.translate.z;

	return worldPos;
}

void Enemy::OnCollision(Collider* other) {
	
	// つみとバグ防止
	if (other->GetCollisionAttribute() == kCollisionAttributePlayer || other->GetCollisionAttribute() == kCollisionAttributePlayerDrone) {
		Vector3 dir = worldTransform_.translate - other->GetWorldPosition();

		if (Length(dir) < 0.001f) return;

		dir = Normalize(dir);

		const float knockPower = 0.18f;

		velocity_ += dir * knockPower * other->GetHitPower();
		const float maxKnockSpeed = 0.22f;
		if (Length(velocity_) > maxKnockSpeed) {
			velocity_ = Normalize(velocity_) * maxKnockSpeed;
		}
	}
	// ダメージ処理
	if (other->GetCollisionAttribute() == kCollisionAttributePlayerBullet) {

		hp_ -= other->GetDamage();
		TriggerDamageFeedback();
	}
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

	const float t = damageFeedbackDuration_ > 0.0f ? damageFeedbackTimer_ / damageFeedbackDuration_ : 0.0f;
	const float flash = t * t;
	const float pulse = std::sin(t * 3.14159265f) * 0.08f;
	worldTransform_.scale = baseScale_ * (1.0f + pulse);
	object_->SetColor(LerpColor(baseColor_, { 1.0f, 1.0f, 1.0f, baseColor_.w }, flash * 0.65f));
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
	
	UpdateAIState();

	AIStateMovePower();

	Vector3 toPlayer = player_->GetWorldPosition() - GetWorldPosition();
	Vector3 attackVec = Normalize(toPlayer) * attackPower;
	std::optional<Vector3> pathDir = FindPathDirectionToPlayer();

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

	if (Length(dir_) > 0.0001f) {
		dir_ = Normalize(dir_);
	} else {
		dir_ = { 0, 0, 0 };
	}

	dir_.z = 0.0f;
	dir_ = ApplyHumanLikeSteering(dir_, pathDir.has_value(), deltaTime);

	// --- 5. プレイヤーの位置と方向ベクトル ---
	Vector3 dir = Normalize(toPlayer);

	// --- 6. 回転角度を算出 ---
	if (Length(velocity_) > 0.001f) {
		float angle = atan2(dir.y, dir.x);
		worldTransform_.rotate.z = angle;
		object_->SetRotate(worldTransform_.rotate);
	}

	const float speed = pathDir ? 0.10f : maxSpeed_;
	Vector3 targetVelocity = dir_ * speed;
	velocity_ += (targetVelocity - velocity_) * 0.22f * (deltaTime * 60.0f);
}

Vector3 Enemy::ApplyHumanLikeSteering(const Vector3& desiredDir, bool usingPath, float deltaTime)
{
	if (Length(desiredDir) < 0.001f) {
		return desiredDir;
	}

	steeringNoiseTimer_ -= deltaTime;
	if (steeringNoiseTimer_ <= 0.0f) {
		Vector3 side = { -desiredDir.y, desiredDir.x, 0.0f };
		float noisePower = usingPath ? Rand(-0.22f, 0.22f) : Rand(-0.12f, 0.12f);
		steeringNoise_ = side * noisePower;
		steeringNoiseTimer_ = Rand(0.25f, 0.75f);
	}

	hesitationCooldown_ -= deltaTime;
	if (hesitationCooldown_ <= 0.0f && usingPath && Rand(0.0f, 1.0f) < 0.18f) {
		hesitationTimer_ = Rand(0.08f, 0.18f);
		hesitationCooldown_ = Rand(1.2f, 2.6f);
	}

	float response = usingPath ? 0.10f : 0.16f;
	Vector3 targetDir = desiredDir + steeringNoise_;
	if (Length(targetDir) > 0.001f) {
		targetDir = Normalize(targetDir);
	}

	steeringDir_ += (targetDir - steeringDir_) * response * (deltaTime * 60.0f);
	if (Length(steeringDir_) > 0.001f) {
		steeringDir_ = Normalize(steeringDir_);
	} else {
		steeringDir_ = targetDir;
	}

	if (hesitationTimer_ > 0.0f) {
		hesitationTimer_ -= deltaTime;
		return steeringDir_ * 0.35f;
	}

	return steeringDir_;
}

std::optional<Vector3> Enemy::FindPathDirectionToPlayer()
{
	if (!stage_ || HasClearMoveRouteToPlayer()) {
		return std::nullopt;
	}

	std::optional<MapIndex> startOpt = WorldToMapIndex(GetWorldPosition());
	std::optional<MapIndex> goalOpt = WorldToMapIndex(player_->GetWorldPosition());
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

	struct QueueNode {
		int id = 0;
		int f = 0;
		bool operator>(const QueueNode& other) const { return f > other.f; }
	};

	std::vector<int> cameFrom(nodeCount, -1);
	std::vector<int> cost(nodeCount, std::numeric_limits<int>::max());
	std::priority_queue<QueueNode, std::vector<QueueNode>, std::greater<QueueNode>> open;

	const int startId = ToId(start);
	const int goalId = ToId(goal);
	cost[startId] = 0;
	open.push({ startId, Heuristic(start, goal) });

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
	Vector3 waypoint = MapIndexToWorld(next);
	Vector3 toWaypoint = waypoint - GetWorldPosition();
	toWaypoint.z = 0.0f;
	if (Length(toWaypoint) < 0.1f) {
		return std::nullopt;
	}
	return Normalize(toWaypoint);
}

std::optional<Enemy::MapIndex> Enemy::WorldToMapIndex(const Vector3& pos) const
{
	const int x = static_cast<int>(std::round(pos.x / MapChip::kBlockWidth));
	const int y = static_cast<int>(MapChip::kNumBlockVirtical - 1 - std::round(pos.y / MapChip::kBlockHeight));
	if (x < 0 || y < 0 || x >= static_cast<int>(MapChip::kNumBlockHorizontal) || y >= static_cast<int>(MapChip::kNumBlockVirtical)) {
		return std::nullopt;
	}
	return MapIndex{ x, y };
}

Vector3 Enemy::MapIndexToWorld(const MapIndex& index) const
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
				Vector3 candidateWorld = MapIndexToWorld({ x, y });
				float distance = Length(candidateWorld - MapIndexToWorld(base));
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
	if (!stage_ || !HasLineOfSightToPlayer()) {
		return false;
	}

	const Vector3 enemyPos = GetWorldPosition();
	const Vector3 playerPos = player_->GetWorldPosition();
	Vector3 toPlayer = playerPos - enemyPos;
	toPlayer.z = 0.0f;
	const float distance = Length(toPlayer);
	if (distance < 0.001f) {
		return true;
	}

	Vector3 normal = Normalize(toPlayer);
	Vector3 side = { -normal.y, normal.x, 0.0f };
	const float halfWidth = kWidth * 0.5f + 0.25f;
	const std::array<Vector3, 3> origins = {
		enemyPos,
		enemyPos + side * halfWidth,
		enemyPos - side * halfWidth
	};

	for (const Vector3& origin : origins) {
		Segment ray;
		ray.origin = origin;
		ray.diff = toPlayer;

		for (const auto& row : stage_->GetBlocks()) {
			for (const Block& block : row) {
				if (!block.isActive) {
					continue;
				}
				if (IsCollision(block.aabb, ray)) {
					return false;
				}
			}
		}
	}

	return true;
}

Vector3 Enemy::RandomDirection() { return Rand(Vector3(-0.2f, -0.2f, 0.0f), Vector3(0.2f, 0.2f, 0.5f)); }

Vector3 Enemy::EvadeBullets() {
	Vector3 evade(0, 0, 0);

	for (Bullet* bullet : bulletManager_->GetBulletPtrs()) {

		// 敵の弾は無視
		if (bullet->GetCollisionAttribute() == kCollisionAttributeEnemyBullet) {
			continue;
		}

		Vector3 toBullet = bullet->GetWorldPosition() - GetWorldPosition();
		float dist = Length(toBullet);

		const float kEvadeRadius = 10.0f;
		if (dist < kEvadeRadius) {

			toBullet *= -1.0f;
			Vector3 dir = Normalize(toBullet);
			float power = (kEvadeRadius - dist) / kEvadeRadius;

			evade += dir * power * 0.2f;
		}
	}

	return evade;
}

void Enemy::UpdateAIState() {

	Vector3 toPlayer = player_->GetWorldPosition() - GetWorldPosition();
	float dist = Length(toPlayer);

	// プレイヤー弾の危険判定
	bool bulletDanger = false;
	for (Bullet* bullet : bulletManager_->GetBulletPtrs()) {
		
		// 敵の弾は無視
		if (bullet->GetCollisionAttribute() == kCollisionAttributeEnemyBullet) {
			continue;
		}

		float distB = Length(bullet->GetWorldPosition() - GetWorldPosition());
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

AABB Enemy::GetAABB() {
	Vector3 worldPos = GetWorldPosition();

	AABB aabb;

	aabb.min = { worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f };
	aabb.max = { worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f };

	return aabb;
}

Segment Enemy::MakeForwardRay(float length) const {

	Segment seg;
	seg.origin = GetWorldPosition();

	Vector3 forward = dir_;
	if (Length(forward) < 0.001f) {
		forward = { 1, 0, 0 }; // 保険
	}

	forward = Normalize(forward);

	seg.diff = forward * length; // 線分の差分

	return seg;
}

bool Enemy::IsBlockNearByRay() {

	const float kRayLength = 3.0f;

	Segment ray = MakeForwardRay(kRayLength);
	
	for (const auto& row : stage_->GetBlocks()) {
		for (const Block& block : row) {

			if (!block.isActive) {
				continue;
			}

			if (IsCollision(block.aabb, ray)) {
				return true;
			}
		}
	}

	return false;
}

Vector3 Enemy::WallAvoidByRay() {

	Vector3 avoid(0, 0, 0);

	if (Length(dir_) < 0.001f) {
		return avoid;
	}

	Vector3 forward = Normalize(dir_);

	if (IsBlockNearByRay() && !isWallFollowing_) {
		
		Vector3 left(-forward.y, forward.x, 0);
		Vector3 right(forward.y, -forward.x, 0);

		float leftScore = ScoreDir(left);
		float rightScore = ScoreDir(right);

		
		isWallFollowing_ = true;
		wallFollowTimer_ = 1.2f; // この時間は壁沿い優先
		wallFollowDir_ = (leftScore < rightScore) ? left : right;

		// 正面ブレーキ
		avoid += -forward * 1.5f;

		// 横へ逃がす（壁沿い移動）
		Vector3 side(-forward.y, forward.x, 0.0f);
		avoid += side * 1.0f;
	}

	return avoid;
}

float Enemy::ScoreDir(const Vector3& dir)
{
	Vector3 futurePos = GetWorldPosition() + dir * 3.0f;
	return Length(player_->GetWorldPosition() - futurePos);
}

Segment Enemy::MakeRayToPlayer() const {

	Segment seg;
	seg.origin = GetWorldPosition();

	Vector3 toPlayer = player_->GetWorldPosition() - seg.origin;
	seg.diff = toPlayer; // プレイヤーまで

	return seg;
}

bool Enemy::HitPlayerByRay(const Segment& ray)
{

	Sphere playerSphere;
	playerSphere.center = player_->GetWorldPosition();
	playerSphere.radius = player_->GetRadius();

	return IsCollision(ray, playerSphere);
}

bool Enemy::HasLineOfSightToPlayer() const {

	Segment ray = MakeRayToPlayer();

	float playerDist = Length(ray.diff);
	
	Sphere playerSphere;
	playerSphere.center = player_->GetWorldPosition();
	playerSphere.radius = player_->GetRadius();
	
	// プレイヤーに当たる？
	if (!IsCollision(ray, playerSphere)) {
		return false;
	}

	// 手前に壁がある？
	for (const auto& row : stage_->GetBlocks()) {
		for (const Block& block : row) {

			if (!block.isActive) {
				continue;
			}

			if (IsCollision(block.aabb, ray)) {

				// ブロック中心までの距離で簡易判定
				float blockDist =
					Length((block.aabb.max + block.aabb.min) / 2.0f - ray.origin);

				if (blockDist < playerDist) {
					return false; // 壁に遮られている
				}
			}
		}
	}

	return true; // 視線が通っている
}

void Enemy::Die()
{
	if (isDead_) return;

	isDead_ = true;
	isExploding_ = true;

	SpawnParticles();
	//bullets_.clear();

	radius_ = 0.0f;
}

bool Enemy::isFinished()
{
	if (isDead_ && !isExploding_) {
		return true;
	}
	return false;
}

void Enemy::SpawnParticles()
{
	Vector3 center = GetWorldPosition();
	ParticleManager::GetInstance()->Emit("EnemyDeathBurst", center, 58);
	ParticleManager::GetInstance()->Emit("DeathSmoke", center, 14);
	ParticleManager::GetInstance()->EmitHitEffect(center);
	deathEffectTimer_ = 1.0f;
}

void Enemy::UpdateParticles(float deltaTime)
{
	deathEffectTimer_ -= deltaTime;
	if (deathEffectTimer_ <= 0.0f) {
		isExploding_ = false;
	}
}

void Enemy::UpdateHPBar()
{
	Vector3 enemyPos = GetWorldPosition();
	Vector3 barOffset = { -2.0f, -2.5f, 0.0f }; // プレイヤーの少し下に配置

	// 背景の更新
	hpBarBGTransform_.translate = enemyPos + barOffset;
	hpBarBG_->SetTransform(hpBarBGTransform_);
	hpBarBG_->Update();

	// ゲージ中身の更新
	float hpPercent = (float)hp_ / (float)maxHP_;
	hpBarFillTransform_.translate = enemyPos + barOffset;
	// XスケールだけHP割合にする
	hpBarFillTransform_.scale.x = 1.0f * hpPercent;

	// ゲージが左端から縮むように、少し位置をずらす調整をするとより綺麗です
	//hpBarFillTransform_.translate.x -= 3.0f * (1.0f - hpPercent) * 0.5f;

	hpBarFill_->SetTransform(hpBarFillTransform_);
	hpBarFill_->Update();

}

void Enemy::HPBarDraw()
{
	if (!isDead_) {
		hpBarBG_->Draw();
		hpBarFill_->Draw();
	}
}
