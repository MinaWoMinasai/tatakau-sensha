#include "PlayerDrone.h"
#include "Stage.h"

PlayerDrone::~PlayerDrone() {

}

void PlayerDrone::Attack() {

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
	SetCollisionMask(kCollisionAttributePlayerDrone | kCollisionAttributeEnemyBullet | kCollisionAttributeEnemy);
}

void PlayerDrone::Update(Camera* viewProjection, Stage& stage, const Vector3& playerPosition)
{
	invincibleTimer_ -= 1.0f / 60.0f;

	RotateToMouse(viewProjection);

	Vector3 toPlayer = playerPosition - worldTransform_.translate;

	float distance = Length(toPlayer);
	if (distance < 0.01f) {
		return;
	}

	Vector3 dir = Normalize(toPlayer);
	
	// --- 目標速度 ---
	Vector3 targetVelocity = dir * maxSpeed_;

	// --- 慣性処理 ---
	float dt = 1.0f / 60.0f;
	float accel = (Length(dir) > 0.0f) ? accel_ : decel_;

	velocity_ += (targetVelocity - velocity_) * accel * dt;

	// X軸移動
	Vector3 playerPos = GetWorldPosition();
	playerPos.x += GetMove().x;
	SetWorldPosition(playerPos);
	stage.ResolvePlayerDroneCollision(*this, X);

	// Y軸移動
	playerPos = GetWorldPosition();
	playerPos.y += GetMove().y;
	SetWorldPosition(playerPos);
	stage.ResolvePlayerDroneCollision(*this, Y);
	
	Vector3 pos = GetWorldPosition();

	// ワールド座標からマップインデックスに変換
	int xIndex = static_cast<int>(pos.x / 2.0f); // kBlockWidth = 2.0f
	int yIndex = static_cast<int>(19 - (pos.y / 2.0f)); // kNumBlockVirtical - 1 = 19, kBlockHeight = 2.0f

	// Stageクラスに判定用関数がある場合の例
	// if (stage.GetMapChipType(pos) == MapChipType::kDamageBlock) { Die(); }

	// 直接MapChipデータを参照する場合の簡易判定（MapChipのインスタンスが必要）
	if (xIndex < 0 || xIndex >= 30 || yIndex < 0 || yIndex >= 20) {
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
		Attack();
	}

	if (hp_ <= 0) {
		Die();
	}
}

void PlayerDrone::Draw() {

	if (!isDead_) {
		// 無敵時間中は点滅

		if (invincibleTimer_ > 0.0f) {
			if (static_cast<int>(invincibleTimer_ * 10) % 2 == 0) {
				return;
			}
		}
		object_->Draw();
	}
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

	if (other->GetCollisionAttribute() == kCollisionAttributeEnemyBullet || other->GetCollisionAttribute() == kCollisionAttributeEnemy) {
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
