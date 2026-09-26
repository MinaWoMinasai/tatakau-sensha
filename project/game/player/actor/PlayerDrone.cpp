#include "PlayerDrone.h"
#include "Stage.h"
#include "game/exp/ExpEnemy.h"

PlayerDrone::~PlayerDrone() {

}

void PlayerDrone::ConfigureRunAttack(const AttackParam& param, float reloadSeconds)
{
	runAttackEnabled_ = true;
	runAttackParam_ = param;
	runAttackParam_.bulletCount = 1;
	runReloadSeconds_ = (std::max)(0.05f, reloadSeconds);
}

void PlayerDrone::Attack(float deltaTime) {
	if (runAttackEnabled_) {
		runShotCooldown_ = (std::max)(0.0f, runShotCooldown_ - deltaTime);
		const bool wantsAttack = runInputOverride_ ? runWantsAttack_
			: (runRallyShotPending_ || input_->IsPress(input_->GetMouseState().rgbButtons[0]));
		if (runShotCooldown_ <= 0.0f && wantsAttack && runBulletManager_ &&
			runBulletManager_->GetBulletCounts().player + static_cast<size_t>(runAttackParam_.bulletCount) <= 240) {
			attackController_.Fire(GetWorldPosition(), dir, runAttackParam_, BulletOwner::kPlayer);
			runShotCooldown_ = runReloadSeconds_;
			runRallyShotPending_ = false;
		}
		return;
	}

	// 弾のクールタイムを計算する
	bulletCoolTime--;

	if (input_->IsPress(input_->GetMouseState().rgbButtons[0])) {

		if (bulletCoolTime <= 0) {

			// 発射位置
			Vector3 origin = GetWorldPosition();

			// 攻撃パラメータを設定
			AttackParam param{};
			param.bulletSpeed = 0.4f;
			param.bulletCount = 1;
			param.spreadAngleDeg = 20.0f;
			param.randomSpread = true;

			param.reflect = false;
			param.penetrate = false;
			param.cooldown = 1.0f;
			param.damage = 1;

			// 発射
			attackController_.Fire(
				origin,
				dir,
				param,
				BulletOwner::kPlayer
			);
			bulletCoolTime = kBulletTime;
		}
	}
}

void PlayerDrone::RotateToMouse(Camera* viewProjection) {
	if (runAttackEnabled_ && runInputOverride_) {
		const Vector3 aim = runAimTarget_ - worldTransform_.translate;
		if (Length(aim) > 0.001f) dir = Normalize(aim);
		angle_ = std::atan2(dir.y, dir.x);
		worldTransform_.rotate.z = angle_;
		object_->SetRotate(worldTransform_.rotate);
		return;
	}
	// --- 1. マウス座標取得 ---
	POINT mousePosition;
	GetCursorPos(&mousePosition);
	HWND hwnd = WinApp::GetInstance()->GetHwnd();
	ScreenToClient(hwnd, &mousePosition);

	// --- 2. 逆変換用の行列を準備 ---
	Matrix4x4 matViewport = MakeViewportMatrix(0, 0, WinApp::kClientWidth, WinApp::kClientHeight, 0, 1);
	Matrix4x4 matVPV = viewProjection->GetViewMatrix() * viewProjection->GetProjectionMatrix() * matViewport;
	Matrix4x4 matInverseVPV = Inverse(matVPV);

	// --- 3. マウス座標をワールドに変換 ---
	Vector3 posNear = Vector3((float)mousePosition.x, (float)mousePosition.y, 0);
	Vector3 posFar = Vector3((float)mousePosition.x, (float)mousePosition.y, 1);

	posNear = TransformMatrix(posNear, matInverseVPV);
	posFar = TransformMatrix(posFar, matInverseVPV);

	// --- 4. レイと Z=0 平面の交差 ---
	Vector3 mouseDirection = posFar - posNear;
	Vector3 rayDir = Normalize(mouseDirection);
	float t = -posNear.z / rayDir.z;
	Vector3 target = posNear + rayDir * t;

	// --- 5. プレイヤーの位置と方向ベクトル ---
	Vector3 playerPos = worldTransform_.translate;
	Vector3 targetPos = target - playerPos;
	dir = Normalize(targetPos);

	// --- 6. 回転角度を算出 ---
	angle_ = atan2(dir.y, dir.x);
	worldTransform_.rotate.z = angle_;
	object_->SetRotate(worldTransform_.rotate);
}

void PlayerDrone::Initialize(const Vector3& position, const Vector3& velocity) {
	
	neonVisual_ = false;
	object_ = std::make_unique<Object3d>();
	object_->Initialize();

	object_->SetModel("enemy.obj");
	
	worldTransform_ = InitWorldTransform();
	worldTransform_.translate = position;
	object_->SetTransform(worldTransform_);
	object_->Update();

	velocity_ = velocity;

	// シングルトンインスタンス
	input_ = Input::GetInstance();

	SetDamage(1);

	// 衝突属性を設定
	SetCollisionAttribute(kCollisionAttributePlayerDrone);
	// 衝突対象を自分に設定
	SetCollisionMask(kCollisionAttributePlayerDrone | kCollisionAttributeEnemyBullet | kCollisionAttributeEnemy | kCollisionAttributeExpEnemy);
}

void PlayerDrone::Update(Camera* viewProjection, Stage& stage, const Vector3& playerPosition, float deltaTime)
{
	const float dt = runAttackEnabled_ ? (std::max)(0.0f, deltaTime) : 1.0f / 60.0f;
	invincibleTimer_ -= dt;

	RotateToMouse(viewProjection);

	Vector3 toPlayer = playerPosition + (runAttackEnabled_ ? runFollowOffset_ : Vector3{}) - worldTransform_.translate;

	float distance = Length(toPlayer);
	if (distance < 0.01f && !runAttackEnabled_) {
		return;
	}

	// Run companions must still shoot when resting directly over their owner.
	Vector3 dir = distance < 0.01f ? Vector3{} : Normalize(toPlayer);
	
	// --- 目標速度 ---
	// Keep companions close enough to contribute even while the run player boosts.
	const float followSpeed = runAttackEnabled_ ? (std::min)(runCatchupSpeed_, runFollowSpeed_ + distance * 0.035f) : maxSpeed_;
	Vector3 targetVelocity = dir * (runAttackEnabled_ ? followSpeed * (std::min)(1.0f, distance / 1.2f) : followSpeed);

	// --- 慣性処理 ---
	float accel = runAttackEnabled_ ? runFollowResponse_ : ((Length(dir) > 0.0f) ? accel_ : decel_);

	velocity_ += (targetVelocity - velocity_) * (runAttackEnabled_ ? 1.0f-std::exp(-accel*dt) : accel*dt);

	Vector3 frameMove = GetMove() * (dt * 60.0f);
	const float maxStep = 0.30f;
	const int subStepCount = (std::max)(1, static_cast<int>((std::max)(std::abs(frameMove.x), std::abs(frameMove.y)) / maxStep) + 1);
	Vector3 stepMove = frameMove / static_cast<float>(subStepCount);
	for (int i = 0; i < subStepCount; ++i) {
		Vector3 playerPos = GetWorldPosition();
		playerPos.x += stepMove.x;
		SetWorldPosition(playerPos);
		stage.ResolvePlayerDroneCollision(*this, X);

		playerPos = GetWorldPosition();
		playerPos.y += stepMove.y;
		SetWorldPosition(playerPos);
		stage.ResolvePlayerDroneCollision(*this, Y);
	}
	
	Vector3 pos = GetWorldPosition();

	// ワールド座標からマップインデックスに変換
	int xIndex = static_cast<int>(pos.x / MapChip::kBlockWidth);
	int yIndex = static_cast<int>(MapChip::kNumBlockVirtical - 1 - (pos.y / MapChip::kBlockHeight));

	// Stageクラスに判定用関数がある場合の例
	// if (stage.GetMapChipType(pos) == MapChipType::kDamageBlock) { Die(); }

	// 直接MapChipデータを参照する場合の簡易判定（MapChipのインスタンスが必要）
	if (xIndex < 0 || xIndex >= static_cast<int>(MapChip::kNumBlockHorizontal) || yIndex < 0 || yIndex >= static_cast<int>(MapChip::kNumBlockVirtical)) {
		Die(); // そもそもマップ配列の範囲外なら死亡
	} else {
		// マップの値を直接チェック（Stage経由でMapChipを取得する想定）
		// MapChipType type = stage.GetMapChip().GetMapChipTypeByIndex(xIndex, yIndex);
		// if (type == MapChipType::kDamageBlock) { Die(); }
	}

	// object_ の更新だけ（移動はしない）
	object_->SetTransform(worldTransform_);
	object_->Update();

	if (!isDead_) {
		// 攻撃処理
		Attack(dt);
	}

	if (hp_ <= 0) {
		Die();
	}
}

bool PlayerDrone::IsVisualVisible() const {
	return !isDead_ && !(invincibleTimer_ > 0.0f && static_cast<int>(invincibleTimer_ * 10) % 2 == 0);
}

void PlayerDrone::Draw() {
	if (!neonVisual_ && IsVisualVisible()) object_->Draw();
}

void PlayerDrone::DrawSprite()
{
}

Vector3 PlayerDrone::GetWorldPosition() const {

	// ワールド座標を入れる
	Vector3 worldPos;
	// ワールド行列の平行移動成分を取得(ワールド座標)
	worldPos.x = worldTransform_.translate.x;
	worldPos.y = worldTransform_.translate.y;
	worldPos.z = worldTransform_.translate.z;

	return worldPos;
}

void PlayerDrone::OnCollision(Collider* other) {
	if (const auto* resource = dynamic_cast<const ExpEnemy*>(other); resource && resource->IsRunResource()) {
		return;
	}

	if (other->GetCollisionAttribute() == kCollisionAttributeEnemyBullet ||
		other->GetCollisionAttribute() == kCollisionAttributeEnemy ||
		other->GetCollisionAttribute() == kCollisionAttributeExpEnemy) {
		hp_--;
	}

	Vector3 hitDir =
		worldTransform_.translate - other->GetWorldPosition();

	if (Length(hitDir) < 0.0001f) {
		return;
	}

	hitDir = Normalize(hitDir);

	const float kKnockBackPower = 0.1f;

	velocity_ += hitDir * kKnockBackPower * other->GetHitPower();
	const float maxKnockSpeed = 0.22f;
	if (Length(velocity_) > maxKnockSpeed) {
		velocity_ = Normalize(velocity_) * maxKnockSpeed;
	}
}

AABB PlayerDrone::GetAABB() {
	Vector3 worldPos = GetWorldPosition();

	AABB aabb;

	aabb.min = { worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f };
	aabb.max = { worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f };

	return aabb;
}

void PlayerDrone::Damage()
{
	if (invincibleTimer_ <= 0.0f) {
		if (hp_ > 0) {
		}
		hp_--;
		invincibleTimer_ = 2.0f;
	}
}

void PlayerDrone::Die()
{
	if (isDead_) return;

	isDead_ = true;
}
