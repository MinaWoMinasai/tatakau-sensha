#include "Player.h"
#include "Stage.h"

Player::~Player() {
	
}

void Player::Attack() {

	// 弾のクールタイムを計算する
	bulletCoolTime--;

	if (input_->IsPress(input_->GetMouseState().rgbButtons[0])) {

		if (bulletCoolTime <= 0) {

			// 発射位置
			Vector3 origin = GetWorldPosition();

			// 攻撃パラメータを設定
			AttackParam param{};
			param.bulletSpeed = 0.4f;
			param.bulletCount = 2;
			param.spreadAngleDeg = 5.0f;
			param.randomSpread = false;

			param.reflect = true;
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

void Player::DroneShoot(BulletManager* BulletManager)
{

	// 弾のクールタイムを計算する
	bulletCoolTime--;

	//if (input_->IsPress(input_->GetMouseState().rgbButtons[0])) {

		if (drones_.size() >= 7) {
			bulletCoolTime = 0;
			return;
		}

		if (bulletCoolTime <= 0) {

			// 弾の速度
			const float kBulletSpeed = 0.2f;
			Vector3 velocity = dir * kBulletSpeed;

			auto drone = std::make_unique<PlayerDrone>();
			drone->Initialize(dir * 0.3f + worldTransform_.translate, velocity);
			drone->SetAttackControllerBulletManager(BulletManager);
			drones_.push_back(std::move(drone));

			bulletCoolTime = 60;
		}
	//}
}

Sphere Player::GetSphere() const
{
	Sphere s{};
	s.center = GetWorldPosition();

	// 半径は「横幅基準」が安定
	s.radius = kRadius;

	return s;
}

void Player::RotateToMouse(Camera* viewProjection) {
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

void Player::Initialize(Object3d* object, const Vector3& position) {

	object_ = object;

	worldTransform_ = InitWorldTransform();
	worldTransform_.translate = position;
	object_->SetTransform(worldTransform_);
	object_->Update();

	for (int i = 0; i < hp_; ++i) {
		auto sprite = std::make_unique<Sprite>();

		sprite->Initialize(SpriteCommon::GetInstance(),
			"resources/playerSprite.png");
		sprite->SetPosition({ 40.0f + 50.0f * i, 80.0f  });

		hpSprites_.push_back(std::move(sprite));
	}

	hpFont = std::make_unique<Sprite>();
	hpFont->Initialize(SpriteCommon::GetInstance(), "resources/PlayerHp.png");
	hpFont->SetPosition({ 20.0f, 20.0f });
	hpFont->SetSize({ 120.0f, 40.0f });

	// シングルトンインスタンス
	input_ = Input::GetInstance();

	// 衝突属性を設定
	SetCollisionAttribute(kCollisionAttributePlayer);
	// 衝突対象を自分の属性以外に設定
	SetCollisionMask(kCollisionAttributeEnemy | kCollisionAttributeEnemyBullet);
}

void Player::Update(Camera* viewProjection, Stage& stage, BulletManager* BulletManager)
{
	float dt = 1.0f / 60.0f;

	invincibleTimer_ -= 1.0f / 60.0f;

	// ----------------------
	// 横入力
	// ----------------------
	RotateToMouse(viewProjection);
	inputDir_ = { 0,0,0 };

	//if (input_->IsPress(input_->GetKey()[DIK_A])) inputDir_.x -= 1.0f;
	//if (input_->IsPress(input_->GetKey()[DIK_D])) inputDir_.x += 1.0f;
	//
	//// ----------------------
	//// 横移動（加速・減速）
	//// ----------------------
	//float targetSpeedX = inputDir_.x * maxSpeed_;
	//float accel = (fabs(inputDir_.x) > 0.0f) ? accel_ : decel_;
	//
	//velocity_.x += (targetSpeedX - velocity_.x) * accel * dt;
	//
	//// ----------------------
	//// ジャンプ
	//// ----------------------
	//if (isOnGround_ && input_->IsTrigger(input_->GetKey()[DIK_SPACE], input_->GetPreKey()[DIK_SPACE])) {
	//	velocity_.y = jumpPower_;
	//	isOnGround_ = false;
	//}
	//
	//// ----------------------
	//// 重力
	//// ----------------------
	//velocity_.y += gravity_ * dt;

	if (input_->IsPress(input_->GetKey()[DIK_A])) inputDir_.x -= 1.0f;
	if (input_->IsPress(input_->GetKey()[DIK_D])) inputDir_.x += 1.0f;
	if (input_->IsPress(input_->GetKey()[DIK_W])) inputDir_.y += 1.0f;
	if (input_->IsPress(input_->GetKey()[DIK_S])) inputDir_.y -= 1.0f;
	
	if (Length(inputDir_) > 1.0f) {
		inputDir_ = Normalize(inputDir_);
	}
	
	// --- 目標速度 ---
	Vector3 targetVelocity = inputDir_ * maxSpeed_;
	
	// --- 慣性処理 ---
	float accel = (Length(inputDir_) > 0.0f) ? accel_ : decel_;
	
	velocity_ += (targetVelocity - velocity_) * accel * dt;

	//// X軸移動
	//Vector3 playerPos = GetWorldPosition();
	//playerPos.x += GetMove().x;
	//SetWorldPosition(playerPos);
	//stage.ResolvePlayerCollision(*this, X);
	//
	//// Y軸移動
	//playerPos = GetWorldPosition();
	//playerPos.y += GetMove().y;
	//SetWorldPosition(playerPos);
	//stage.ResolvePlayerCollision(*this, Y);

	// Y移動
	Vector3 pos = GetWorldPosition();
	pos.y += velocity_.y;
	SetWorldPosition(pos);
	stage.ResolvePlayerCollisionSphereY(*this);

	// X移動
	pos = GetWorldPosition();
	pos.x += velocity_.x;
	SetWorldPosition(pos);
	stage.ResolvePlayerCollisionSphereX(*this);

	// object_ の更新だけ（移動はしない）
	object_->SetTransform(worldTransform_);
	object_->Update();
	
	if (!isDead_) {
		// 攻撃処理
		//Attack();
		DroneShoot(BulletManager);

		for (auto& drone : drones_) {
			drone->Update(viewProjection, stage, worldTransform_.translate);
		}

		drones_.erase(
			std::remove_if(
				drones_.begin(),
				drones_.end(),
				[](const std::unique_ptr<PlayerDrone>& drone) {
					return drone->IsDead();
				}),
			drones_.end());

	}

	for (auto& sprite : hpSprites_) {
		sprite->Update();
	}
	hpFont->Update();

	if (hp_ <= 0) {
		Die();
	}
	if (isExploding_) {
		UpdateParticles(deltaTime);
	}

}

void Player::Draw() {

	// ドローンの描画
	for (auto& drone : drones_) {
		drone->Draw();
	}

	if (!isDead_) {
		// 無敵時間中は点滅

		if (invincibleTimer_ > 0.0f) {
			if (static_cast<int>(invincibleTimer_ * 10) % 2 == 0) {
				return;
			}
		}
		object_->Draw();
	}

	for (auto& p : particles_) {
		p.object->Draw();
	}
}

void Player::DrawSprite()
{
	for (auto& sprite : hpSprites_) {
		sprite->Draw();
	}
	hpFont->Draw();
}

std::vector<PlayerDrone*> Player::GetDronePtrs() const {
	std::vector<PlayerDrone*> result;
	result.reserve(drones_.size());
	for (const auto& d : drones_) {
		result.push_back(d.get());
	}
	return result;
}

Vector3 Player::GetWorldPosition() const {

	// ワールド座標を入れる
	Vector3 worldPos;
	// ワールド行列の平行移動成分を取得(ワールド座標)
	worldPos.x = worldTransform_.translate.x;
	worldPos.y = worldTransform_.translate.y;
	worldPos.z = worldTransform_.translate.z;

	return worldPos;
}

Vector2 Player::GetMoveInput() {
	Vector2 move = {0, 0};
	if (input_->IsPress(input_->GetKey()[DIK_A]))
		move.x -= kCharacterSpeed;
	if (input_->IsPress(input_->GetKey()[DIK_D]))
		move.x += kCharacterSpeed;
	if (input_->IsPress(input_->GetKey()[DIK_W]))
		move.y += kCharacterSpeed;
	if (input_->IsPress(input_->GetKey()[DIK_S]))
		move.y -= kCharacterSpeed;
	return move;
}

void Player::OnCollision(Collider* other) {
	
	Vector3 hitDir =
		worldTransform_.translate - other->GetWorldPosition();

	if (Length(hitDir) < 0.0001f) {
		return;
	}

	hitDir = Normalize(hitDir);

	const float kKnockBackPower = 0.3f;

	velocity_ += hitDir * kKnockBackPower * other->GetHitPower();
}

AABB Player::GetAABB() {
	Vector3 worldPos = GetWorldPosition();

	AABB aabb;

	aabb.min = { worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f };
	aabb.max = { worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f };

	return aabb;
}

void Player::Damage()
{
	if (invincibleTimer_ <= 0.0f) {
		if (hp_ > 0) {
			//hpSprites_.pop_back();
		}
		//hp_--;
		//invincibleTimer_ = 2.0f;
	}
}

void Player::Die()
{
	if (isDead_) return;

	isDead_ = true;
	isExploding_ = true;

	SpawnParticles();

	// すべての弾を消す
	//bullets_.clear();
}

bool Player::isFinished()
{
	if (isDead_ && !isExploding_) {
		return true;
	}
	return false;
}

void Player::SpawnParticles()
{
	const int particleCount = 30;
	Vector3 center = object_->GetTranslate();

	for (int i = 0; i < particleCount; ++i) {

		PlayerParticle p{};

		p.object = std::make_unique<Object3d>();
		p.object->Initialize();
		p.object->SetModel("playerParticle.obj");

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

void Player::UpdateParticles(float deltaTime)
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
			[](const PlayerParticle& p) {
				return p.timer >= p.lifeTime;
			}),
		particles_.end()
	);

	if (particles_.empty()) {
		isExploding_ = false;
	}
}
