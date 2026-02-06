#include "Enemy.h"
#include "Calculation.h"
#include "Player.h"
#include <DirectXMath.h>
#include "Stage.h"
using namespace DirectX;

static constexpr float kDeltaTime = 1.0f / 60.0f;

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
	param.cooldown = 0.7f;
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
	worldTransform_.scale = Vector3(2.0f, 2.0f, 2.0f);

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

	// X移動
	Vector3 pos = GetWorldPosition();
	pos.x += GetMove().x * (deltaTime * 60.0f);
	SetWorldPosition(pos);
	stage_->ResolveEnemyCollision(*this, X);

	// Y移動
	pos = GetWorldPosition();
	pos.y += GetMove().y * (deltaTime * 60.0f);
	SetWorldPosition(pos);
	stage_->ResolveEnemyCollision(*this, Y);

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

void Enemy::Draw() {
	if (!isDead_) {
		object_->Draw();
	}

	for (auto& p : particles_) {
		p.object->Draw();
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

		const float knockPower = 2.0f;

		velocity_ += dir * knockPower * other->GetHitPower();
	}
	// ダメージ処理
	if (other->GetCollisionAttribute() == kCollisionAttributePlayerBullet) {

		hp_ -= other->GetDamage();
	}
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
	wanderChangeTimer -= deltaTime;
	if (wanderChangeTimer <= 0.0f) {
		wanderVec = RandomDirection() * wanderPower;
		wanderChangeTimer = 3.0f;
	}

	evadeVec = EvadeBullets() * evadePower;
	dir_ = attackVec + evadeVec + wanderVec + WallAvoidByRay();

	if (Length(dir_) > 0.0001f) {
		dir_ = Normalize(dir_);
	} else {
		dir_ = { 0, 0, 0 };
	}

	dir_.z = 0.0f;

	// --- 5. プレイヤーの位置と方向ベクトル ---
	Vector3 dir = Normalize(toPlayer);

	// --- 6. 回転角度を算出 ---
	if (Length(velocity_) > 0.001f) {
		float angle = atan2(dir.y, dir.x);
		worldTransform_.rotate.z = angle;
		object_->SetRotate(worldTransform_.rotate);
	}

	if (isWallFollowing_) {

		wallFollowTimer_ -= 1.0f / 60.0f;
		dir_ = wallFollowDir_;

		if (wallFollowTimer_ <= 0.0f) {
			isWallFollowing_ = false;
		}
	}

	// 1. まず摩擦を適用（速度を少し減衰させる）
    // 何も入力がないとき、これでゆっくり止まる
    velocity_ *= friction_ * (deltaTime * 60.0f);

    // 2. 入力方向（dir_）に向かって加速を「加算」する
    // 目標速度との差分ではなく、純粋に「力」として足す
    if (Length(dir_) > 0.0001f) {
        // 加速度 accel_ をそのまま足す
        velocity_ += dir_ * accel_ * (deltaTime * 60.0f);
    }

    // 3. 最後に最大速度制限をかける
    if (Length(velocity_) > maxSpeed_) {
        velocity_ = Normalize(velocity_) * maxSpeed_;
    }
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

bool Enemy::HasLineOfSightToPlayer() {

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
	const int particleCount = 30;
	Vector3 center = object_->GetTranslate();

	for (int i = 0; i < particleCount; ++i) {

		EnemyParticle p{};

		p.object = std::make_unique<Object3d>();
		p.object->Initialize();
		p.object->SetModel("enemyParticle.obj");

		p.object->SetTranslate(center);

		// ----------------------
		// ランダム方向 → 正規化
		// ----------------------
		Vector3 dir = Rand(
			Vector3{ -1.0f, -1.0f, -1.0f },
			Vector3{ 1.0f,  1.0f,  1.0f }
		);
		dir = Normalize(dir);

		p.velocity = dir * Rand(3.0f, 6.0f);

		// 回転速度
		p.rotateSpeed = Rand(
			Vector3{ -6.0f, -6.0f, -6.0f },
			Vector3{ 6.0f,  6.0f,  6.0f }
		);

		// ----------------------
		// 寿命 & 透明度
		// ----------------------
		p.timer = 0.0f;
		p.lifeTime = Rand(0.9f, 1.4f);     // ★ 個体差
		p.startAlpha = Rand(0.6f, 1.0f);   // ★ 初期アルファ

		// Addブレンド映え用（少し明るめ）
		Vector4 color = {
			Rand(0.8f, 1.2f),
			Rand(0.8f, 1.2f),
			Rand(0.8f, 1.2f),
			p.startAlpha
		};
		p.object->SetColor(color);

		particles_.push_back(std::move(p));
	}
}

void Enemy::UpdateParticles(float deltaTime)
{
	for (auto& p : particles_) {

		p.timer += deltaTime;
		if (p.timer >= p.lifeTime) continue;

		float t = p.timer / p.lifeTime; // 0.0 → 1.0

		// ----------------------
		// 移動
		// ----------------------
		Vector3 pos = p.object->GetTranslate();
		pos += p.velocity * deltaTime;

		// 減速
		p.velocity *= 0.97f;

		// 重力
		p.velocity.y -= 6.0f * deltaTime;

		// ----------------------
		// 回転
		// ----------------------
		Vector3 rot = p.object->GetRotate();
		rot += p.rotateSpeed * deltaTime;

		// ----------------------
		// フェードアウト
		// ----------------------
		float alpha = p.startAlpha * (1.0f - t);
		alpha = std::max(alpha, 0.0f);

		Vector4 color = p.object->GetColor();
		color.w = alpha;
		p.object->SetColor(color);

		// ----------------------
		p.object->SetTranslate(pos);
		p.object->SetRotate(rot);
		p.object->Update();
	}

	// 寿命切れ削除
	particles_.erase(
		std::remove_if(
			particles_.begin(),
			particles_.end(),
			[](const EnemyParticle& p) {
				return p.timer >= p.lifeTime;
			}),
		particles_.end()
	);

	if (particles_.empty()) {
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
