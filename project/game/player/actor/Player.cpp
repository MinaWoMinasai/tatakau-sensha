#include "Player.h"
#include "Stage.h"
#include "Audio.h"

Player::~Player() {

}

void Player::Attack(BulletManager* bulletManager, float deltaTime) {

	// 弾のクールタイムを計算する
	bulletCoolTime -= deltaTime;
	if (bulletCoolTime > 0.0f) return; // クールタイム中は抜ける

	if (input_->IsPress(input_->GetMouseState().rgbButtons[0])) {

		if (bulletCoolTime <= 0.0f) {

			// 発射位置
			Vector3 origin = GetWorldPosition();

			// 攻撃パラメータを設定
			AttackParam param{};
			param.bulletSpeed = stats_.bulletSpeed;
			param.bulletCount = 1;
			param.spreadAngleDeg = 5.0f;
			param.randomSpread = false;

			param.reflect = false;
			param.penetrate = false;
			param.cooldown = 1.0f;
			param.damage = static_cast<uint32_t>(stats_.bulletDamage);
			bool firedByClass = false;

			Vector3 recoilDir = Normalize(dir_) * -1.0f;
			float recoilPower = 0.01f; // 弾の重さ（慣性の強さ）

			// 個別にクールタイムを設定するために先に設定
			float baseReload = isBuffActive_ ? (stats_.reloadSpeed * 0.7f) / 60.0f : stats_.reloadSpeed / 60.0f;
			bulletCoolTime = baseReload;

			switch (currentClass_) {
			case ClassType::Basic:
				// 既存の単発攻撃
				param.spreadAngleDeg = 10.0f;
				param.randomSpread = true;
				bulletCoolTime = baseReload;
				if (isBuffActive_) {
					param.reflect = true;
					bulletCoolTime = baseReload * 0.6f;
					param.spreadAngleDeg = 20.0f;
				}
				attackController_.Fire(origin, dir_, param, BulletOwner::kPlayer);
				firedByClass = true;
				
				SpawnCasing(); // ここで呼び出す
				break;

			case ClassType::Twin:
			{
				param.spreadAngleDeg = 2.0f;
				param.randomSpread = true;
				float offsetValue = 0.6f; // 砲身の横幅
				Vector3 rightDir = { -dir_.y, dir_.x, 0.0f }; // dir_に垂直なベクトル（右方向）

				if (shootBarrelIndex_ == 0) {
					// 左から発射
					attackController_.Fire(origin - rightDir * offsetValue, dir_, param, BulletOwner::kPlayer);
					firedByClass = true;
					shootBarrelIndex_ = 1; // 次は右
				} else {
					// 右から発射
					attackController_.Fire(origin + rightDir * offsetValue, dir_, param, BulletOwner::kPlayer);
					firedByClass = true;
					shootBarrelIndex_ = 0; // 次は左
				}

				// クールタイムを半分にする（2門合わせてBasicと同じ秒間発射数にする場合）
				bulletCoolTime = baseReload / 2.5f;
			}
			break;

			case ClassType::MachineGun:
				// 角度をランダムにずらす
				param.spreadAngleDeg = 30.0f;
				param.randomSpread = true;
				attackController_.Fire(origin, dir_, param, BulletOwner::kPlayer);
				firedByClass = true;
				// リロード補正0.6倍
				bulletCoolTime = baseReload * 0.6f;
				break;

			case ClassType::Overseer:
				// 弾は撃たず、ドローン管理関数を呼ぶ
				DroneShoot(bulletManager);
				firedByClass = true;
				// 反動なし
				recoilPower = 0.0f;
				break;

			case ClassType::Smasher:
				
				break;
			case ClassType::Triple:
				param.bulletCount = 3;
				param.spreadAngleDeg = 45.0f; // 扇状に広がる
				break;

			case ClassType::Bounder:
				param.reflect = true; // 既存のシステムに反射フラグがあるため有効化
				param.bulletCount = 1;
				break;

			case ClassType::Assassin:
				// Assassinは弾速が速いなどのボーナスがあっても良い
				param.bulletSpeed *= 1.5f;
				isStealth_ = false; // 撃ったら解除
				stealthTimer_ = 0.0f;
				break;

			case ClassType::Ninja:
				// 手裏剣風に3つ拡散
				param.bulletCount = 3;
				param.spreadAngleDeg = 15.0f;
				// ステルス解除はしない(仕様通りなら攻撃中も維持)
				break;
			}

			if (!firedByClass && currentClass_ != ClassType::Smasher) {
				attackController_.Fire(origin, dir_, param, BulletOwner::kPlayer);
				SpawnCasing();
			}

			velocity_ += recoilDir * recoilPower;
			Audio::GetInstance()->PlayAudioSE(L"bulletShoot", 0.6f);
		}
	}
}

void Player::DroneShoot(BulletManager* BulletManager)
{

	// 弾のクールタイムを計算する
	bulletCoolTime--;

	//if (input_->IsPress(input_->GetMouseState().rgbButtons[0])) {

	if (drones_.size() >= 7) {
		bulletCoolTime = 0.0f;
		return;
	}

	if (bulletCoolTime <= 0.0f) {

		// 弾の速度
		const float kBulletSpeed = 0.2f;
		Vector3 velocity = dir_ * kBulletSpeed;

		auto drone = std::make_unique<PlayerDrone>();
		drone->Initialize(dir_ * 0.3f + worldTransform_.translate, velocity);
		drone->SetAttackControllerBulletManager(BulletManager);
		drones_.push_back(std::move(drone));

		bulletCoolTime = 1.0f;
	}
	//}
}

void Player::Smash(float deltaTime)
{

	if (isSmash_) {
		return;
	}

	// キーを離したら突撃する
	if (input_->IsMomentRelease(input_->GetMouseState().rgbButtons[0], input_->GetPreMouseState().rgbButtons[0])) {
		smashDir_ = dir_;
		isSmash_ = true;
		Vector3 recoilDir = Normalize(smashDir_);
		float recoilPower = easeInQuad(smashCharge_) / 2.0f; // 弾の重さ（慣性の強さ）
		recoilPower = std::min(recoilPower, 0.7f);
		velocity_ = recoilDir * recoilPower;
		return;
	}

	if (input_->IsPress(input_->GetMouseState().rgbButtons[0])) {

		if (isBuffActive_) {
			smashCharge_ += deltaTime * 5.0f;
		} else {
			smashCharge_ += deltaTime;
		}
		if (smashCharge_ >= maxCharge_) {
			smashCharge_ = maxCharge_;
		}
	}
}

Sphere Player::GetSphere() const
{
	Sphere s{};
	s.center = GetWorldPosition();

	// 半径は「横幅基準」が安定
	s.radius = kRadius;

	return s;
}

void Player::AddExp(int amount)
{
	if (level_ >= kMaxLevel) return;

	exp_ += amount;
	// レベルアップ判定（余剰分も考慮してループ）
	while (exp_ >= nextLevelExp_) {
		const int previousRank = GetRankFromLevel(level_);
		exp_ -= nextLevelExp_;
		level_++;
		skillPoints_++;

		// 次の必要経験値を再計算（例: レベル * 100 + 補正）
		nextLevelExp_ = GetNextLevelExp();

		if (GetRankFromLevel(level_) > previousRank) {
			isChangeMode = true;
		}

		if (level_ >= kMaxLevel) {
			exp_ = nextLevelExp_; // カンスト表示用
			break;
		}
	}
}

bool Player::RequestSlow()
{
	if (requestSlow_) {
		requestSlow_ = false;
		return true;
	}
	return false;
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
	dir_ = Normalize(targetPos);

	// --- 6. 回転角度を算出 ---
	angle_ = atan2(dir_.y, dir_.x);
	worldTransform_.rotate.z = angle_;
	object_->SetRotate(worldTransform_.rotate);
}

void Player::Initialize(Object3d* object, const Vector3& position) {

	sprite = std::make_unique<Sprite>();
	sprite->Initialize(SpriteCommon::GetInstance(), "resources/fade.png");
	sprite->SetColor(Vector4(1.0f, 1.0f, 1.0f, 0.8f));

	object_ = object;

	worldTransform_ = InitWorldTransform();
	worldTransform_.translate = position;
	object_->SetTransform(worldTransform_);
	object_->Update();

	for (int i = 0; i < hp_; ++i) {
		auto sprite = std::make_unique<Sprite>();

		sprite->Initialize(SpriteCommon::GetInstance(),
			"resources/playerSprite.png");
		sprite->SetPosition({ 40.0f + 50.0f * i, 80.0f });

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
	SetDamage(static_cast<uint32_t>(stats_.bodyDamage));

	level_ = 1;
	exp_ = 0;
	nextLevelExp_ = GetNextLevelExp();

	machineGunBtnSprite_ = std::make_unique<Sprite>();
	machineGunBtnSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white512x512.png");
	machineGunBtnSprite_->SetPosition({ btnPos_ });
	machineGunBtnSprite_->SetSize({ btnSize_ });

	InitializeEncyclopedia();

}

void Player::Update(Camera* viewProjection, Stage& stage, BulletManager* BulletManager, float deltaTime)
{
	sprite->Update();

	POINT mousePos;
	GetCursorPos(&mousePos);
	ScreenToClient(WinApp::GetInstance()->GetHwnd(), &mousePos);
	mousePosition_ = { static_cast<float>(mousePos.x), static_cast<float>(mousePos.y) };
	UpdateEncyclopedia();

	if (input_->IsTrigger(input_->GetKey()[DIK_C], input_->GetPreKey()[DIK_C])) {
		isChangeMode = !isChangeMode;
	}

	if (skillPoints_ > 0) {
		if (input_->IsTrigger(input_->GetKey()[DIK_1], input_->GetPreKey()[DIK_1])) ApplyStatUpgrade(0);
		if (input_->IsTrigger(input_->GetKey()[DIK_2], input_->GetPreKey()[DIK_2])) ApplyStatUpgrade(1);
		if (input_->IsTrigger(input_->GetKey()[DIK_3], input_->GetPreKey()[DIK_3])) ApplyStatUpgrade(2);
		if (input_->IsTrigger(input_->GetKey()[DIK_4], input_->GetPreKey()[DIK_4])) ApplyStatUpgrade(3);
		if (input_->IsTrigger(input_->GetKey()[DIK_5], input_->GetPreKey()[DIK_5])) ApplyStatUpgrade(4);
		if (input_->IsTrigger(input_->GetKey()[DIK_6], input_->GetPreKey()[DIK_6])) ApplyStatUpgrade(5);
		if (input_->IsTrigger(input_->GetKey()[DIK_7], input_->GetPreKey()[DIK_7])) ApplyStatUpgrade(6);
	}

	//SetDamage(int(stats_.bodyDamage));

	//maxCharge_ -= deltaTime;
	//if (maxCharge_ < 1.0f) {
	//	maxCharge_ = 1.0f;
	//}

	// バフタイマーの更新
	if (buffTimer_ > 0.0f) {
		buffTimer_ -= deltaTime;
		SpawnBuffParticle();
		if (buffTimer_ <= 0.0f) {
			isBuffActive_ = false;
			//object_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f }); // 元の色に戻す
		}
	}

	//if (isJustEvaded_ && invincibleTimer_ > 0.0f) {
	//	SpawnAfterimage();
	//}

	// バフタイマーの更新
	//if (isSmash_) {
	//	// チャージが0でも、速度が落ちたらスマッシュ終了とみなす
	//	smashCharge_ -= deltaTime * 2.0f;
	//	if (smashCharge_ <= 0.0f || Length(velocity_) < 0.1f) {
	//		isSmash_ = false;
	//		smashCharge_ = 0.0f;
	//	}
	//}

	dt_ = deltaTime;

	invincibleTimer_ -= deltaTime;

	if (dashTimer_ > 0.0f) {
		dashTimer_ -= deltaTime;
		SpawnAfterimage();
		if (dashTimer_ <= 0.0f) {
			isDashing_ = false;
			isJustEvaded_ = false;
		}
	}

	if (dashCooldown_ > 0.0f) {
		dashCooldown_ -= deltaTime;
	}

	// 右クリックでダッシュ
	if (input_->IsTrigger(input_->GetMouseState().rgbButtons[1], input_->GetPreMouseState().rgbButtons[1]) && dashCooldown_ <= 0.0f && stats_.stamina >= 1.0f) {

		// 移動中ならその方向へ、止まっていれば向いている方向(dir_)へダッシュ
		Vector3 dashDir = inputDir_;
		if (Length(dashDir) < 0.01f) {
			dashDir = dir_;
		}

		// 瞬間的に速度を上書き、または強く加算
		velocity_ = Normalize(dashDir) * kDashSpeed;

		isDashing_ = true;
		dashTimer_ = kDashDuration;
		dashCooldown_ = kDashCooldown;

		// ダッシュした瞬間に少し無敵にする（これが回避の基礎）
		//stats_.stamina -= 1.0f;

	}

	// スタミナ回復
	if (!isDashing_) {
		stats_.stamina += stats_.staminaRecovery * deltaTime;
		if (stats_.stamina > stats_.maxStamina) {
			stats_.stamina = stats_.maxStamina;
		}
	}

	UpdateStealth(deltaTime);
	UpdateSummoner(deltaTime);


	// ----------------------
	// 横入力
	// ----------------------
	RotateToMouse(viewProjection);
	inputDir_ = { 0,0,0 };

	if (input_->IsPress(input_->GetKey()[DIK_A])) inputDir_.x -= 1.0f;
	if (input_->IsPress(input_->GetKey()[DIK_D])) inputDir_.x += 1.0f;
	if (input_->IsPress(input_->GetKey()[DIK_W])) inputDir_.y += 1.0f;
	if (input_->IsPress(input_->GetKey()[DIK_S])) inputDir_.y -= 1.0f;

	if (Length(inputDir_) > 1.0f) {
		inputDir_ = Normalize(inputDir_);
	}

	// --- 目標速度 ---
	Vector3 targetVelocity = inputDir_ * stats_.moveSpeed;

	// --- 慣性処理 ---
	float accel = (Length(inputDir_) > 0.0f) ? accel_ : decel_;

	velocity_ += (targetVelocity - velocity_) * accel * deltaTime;

	float timeWeight = deltaTime * 60.0f;

	Vector3 frameMove = velocity_ * timeWeight;
	const float maxStep = 0.35f;
	const int subStepCount = (std::max)(1, static_cast<int>((std::max)(std::abs(frameMove.x), std::abs(frameMove.y)) / maxStep) + 1);
	Vector3 stepMove = frameMove / static_cast<float>(subStepCount);
	for (int i = 0; i < subStepCount; ++i) {
		Vector3 pos = GetWorldPosition();
		pos.x += stepMove.x;
		SetWorldPosition(pos);
		stage.ResolvePlayerCollision(*this, X);

		pos = GetWorldPosition();
		pos.y += stepMove.y;
		SetWorldPosition(pos);
		stage.ResolvePlayerCollision(*this, Y);
	}

	// object_ の更新だけ（移動はしない）
	object_->SetTransform(worldTransform_);
	object_->Update();

	if (!isDead_) {
		// 攻撃処理

		if (currentClass_ == ClassType::Smasher) {
			Smash(deltaTime);
		} else {
			Attack(BulletManager, deltaTime);
		}//DroneShoot(BulletManager);

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
	
	Vector3 footPos = GetWorldPosition();
	footPos.y -= 1.0f; // プレイヤーの少し下へオフセット
	footPos.x -= 1.0f;

	// 2. スクリーン座標に変換
	Vector2 screenBasePos = WorldToScreen(footPos, viewProjection);

	// 3. 3つのスプライトを中央揃えで配置
	float interval = 25.0f; // アイコン同士の間隔
	float totalWidth = interval * (hpSprites_.size() - 1);
	float startX = screenBasePos.x - (totalWidth / 2.0f);

	for (size_t i = 0; i < hpSprites_.size(); ++i) {
		hpSprites_[i]->SetPosition({ startX + (interval * i), screenBasePos.y });
		hpSprites_[i]->SetSize({ 20.0f, 20.0f }); // 少し小さくすると見やすい
		hpSprites_[i]->Update();
	}

	// HPの文字（Font）も足元に置くなら同様に、不要なら非表示に
	//hpFont->SetPosition({ screenBasePos.x - 20.0f, screenBasePos.y - 30.0f });
	//hpFont->Update();

	machineGunBtnSprite_->Update();

	if (hp_ <= 0) {
		Die();
	}
	if (isExploding_) {
		UpdateParticles(deltaTime);
	}

}

void Player::Draw(bool drawBody) {

	// ドローンの描画
	for (auto& drone : drones_) {
		drone->Draw();
	}

	if (drawBody) {
		DrawBodyOnly();
	}

}

void Player::DrawBodyOnly() {
	if (!isDead_) {
		// 無敵時間中は点滅

		if (invincibleTimer_ > 0.0f && !isJustEvaded_) {
			if (static_cast<int>(invincibleTimer_ * 10) % 5 == 0) {
				// 透明度を下げる
				if (isStealth_) {
					object_->GetColor().w = 0.2f;
				} else {
					object_->GetColor().w = 0.5f;
				}
				object_->Draw();
				return;
			}
		}
		if (!isStealth_) {
			object_->GetColor().w = 1.0f;
		}
		object_->Draw();
	}

}

void Player::DrawSprite()
{
	for (auto& sprite : hpSprites_) {
		sprite->Draw();
	}
	//hpFont->Draw();

	//DrawEncyclopedia();
	//machineGunBtnSprite_->Draw();

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

void Player::OnCollision(Collider* other) {

	if (currentClass_ == ClassType::Assassin) {
		isStealth_ = false;
		stealthTimer_ = 0.0f;
	}
	
	if ((invincibleTimer_ > 0.0f && !isDashing_) || isJustEvaded_ || isSmash_) {
		// 無敵中に敵の弾を受けてもノックバックしない
		if (other->GetCollisionAttribute() == kCollisionAttributeEnemyBullet) {
			return;
		}
	}

	if (!isJustEvaded_ && isDashing_ && (kDashDuration - dashTimer_) <= kJustEvadeWindow) {
		requestSlow_ = true;
		isJustEvaded_ = true;
		invincibleTimer_ = dashTimer_;
		buffTimer_ = kBuffDuration;
		isBuffActive_ = true;
		// 演出として色を変える（例：金色っぽく）
		//object_->SetColor({ 0.0f, 1.0f, 0.0f, 1.0f });
		maxCharge_ = 5.0f;
		return;
	}

	Vector3 hitDir =
		worldTransform_.translate - other->GetWorldPosition();

	if (Length(hitDir) < 0.0001f) {
		return;
	}

	hitDir = Normalize(hitDir);

	const float kKnockBackPower = 0.15f;

	velocity_ += hitDir * kKnockBackPower * other->GetHitPower() * (dt_ * 60.0f);
	const float maxKnockSpeed = isDashing_ ? 0.32f : 0.20f;
	if (Length(velocity_) > maxKnockSpeed) {
		velocity_ = Normalize(velocity_) * maxKnockSpeed;
	}
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

		// --- ジャスト回避判定 ---
		//if (isDashing_ && (kDashDuration - dashTimer_) <= kJustEvadeWindow) {
		//	requestSlow_ = true;
		//	return;
		//}

		if (hp_ > 0) {
			//hpSprites_.pop_back();
		}

		//hp_--;
		invincibleTimer_ = 2.0f;
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

int Player::GetNextLevelExp() const
{
	// 簡易的な計算式（必要に応じて調整）
	return level_ * 20 + 20;
}

void Player::Evolve(ClassType newClass)
{
	currentClass_ = newClass;
	
	// 進化時に特殊状態をリセットする
	isSmash_ = false;
	smashCharge_ = 0.0f;
	isStealth_ = false;

	// モデルの切り替え
	switch (currentClass_) {
	case ClassType::Twin:
		//object_->SetModel("player_twin.obj"); // 砲身2つのモデル

		break;
		//case ClassType::Sniper:
			//object_->SetModel("player_sniper.obj");
			//break;
			// ...
	}

	isChangeMode = false;
}

void Player::InitializeEncyclopedia()
{
	std::vector<TankSeed> seeds = {
		   { ClassType::Basic,      "Basic",      1, "resources/normalTank.png" },
		   { ClassType::Twin,       "Twin",       2, "resources/twin.png" },
		   { ClassType::Overseer,   "Overseer",   2, "resources/drone.png" },
		   { ClassType::MachineGun, "MachineGun", 2, "resources/machineGun.png" },
		   { ClassType::Triple,     "Triple",     3, "resources/normalTank.png" },
		   { ClassType::Assassin,   "Assassin",   3, "resources/normalTank.png" },
		   { ClassType::Bounder,    "Bounder",    3, "resources/normalTank.png" },
		   { ClassType::Ninja,      "Ninja",      4, "resources/normalTank.png" },
		   { ClassType::Smasher,    "Smasher",    4, "resources/normalTank.png" },
		   { ClassType::Summoner,   "Summoner",   4, "resources/normalTank.png" }
	};

	const float kBtnWidth = 165.0f;
	const float kBtnHeight = 54.0f;
	// 間隔を大幅に広げる
	const float kMarginX = 150.0f;
	const float kMarginY = 80.0f;
	// 画面全体の中央付近（左上基準）
	const Vector2 kBasePos = { 240.0f, 120.0f };

	// 既存のリストをクリアして再構築
	encyclopedia_.clear();

	for (int i = 0; i < seeds.size(); ++i) {
		// 新しい実体を作成
		TankData data;
		data.type = seeds[i].type;
		data.name = seeds[i].name;
		data.requiredRank = seeds[i].requiredRank;
		data.texturePath = seeds[i].texturePath;

		// スプライトの生成
		data.sprite = std::make_unique<Sprite>();
		data.sprite->Initialize(SpriteCommon::GetInstance(), data.texturePath);
		data.sprite->SetSize({ kBtnWidth, kBtnHeight });

		// ポジション計算
		Vector2 pos;
		if (i == 0) {
			pos = { kBasePos.x + (kBtnWidth + kMarginX), kBasePos.y };
		} else {
			int row = (i - 1) / 3 + 1;
			int col = (i - 1) % 3;
			pos = { kBasePos.x + (col * (kBtnWidth + kMarginX)), kBasePos.y + (row * (kBtnHeight + kMarginY)) };
		}
		data.sprite->SetPosition(pos);

		// moveを使ってvectorに追加（これでコピーを回避！）
		encyclopedia_.push_back(std::move(data));
	}
}

void Player::SpawnParticles()
{
	Vector3 center = GetWorldPosition();
	ParticleManager::GetInstance()->Emit("PlayerDeathBurst", center, 70);
	ParticleManager::GetInstance()->Emit("DeathSmoke", center, 22);
	ParticleManager::GetInstance()->EmitHitEffect(center);
	deathEffectTimer_ = 1.1f;
}

void Player::UpdateParticles(float deltaTime)
{
	deathEffectTimer_ -= deltaTime;
	if (deathEffectTimer_ <= 0.0f) {
		isExploding_ = false;
	}
}

void Player::UpdateP(float deltaTime)
{
	(void)deltaTime;
}

void Player::UpdateEncyclopedia()
{
	int currentRank = GetRankFromLevel(this->level_);

	for (auto& tank : encyclopedia_) {
		// ランクが足りているか
		bool isAvailable = (currentRank >= tank.requiredRank);

		if (isAvailable) {
			// ホバー判定（現在のボタンの位置とサイズで判定）
			if (tank.sprite->IsHovered(mousePosition_)) {
				tank.sprite->SetColor({ 0.0f, 1.0f, 0.0f, 0.8f }); // ホバー時は緑
				if (input_->IsTrigger(input_->GetMouseState().rgbButtons[0], input_->GetPreMouseState().rgbButtons[0])) { // 左クリック時
					Evolve(tank.type);
				}
			} else {
				tank.sprite->SetColor({ 1.0f, 1.0f, 1.0f, 0.8f }); // 通常
			}
		} else {
			tank.sprite->SetColor({ 0.2f, 0.2f, 0.2f, 0.3f }); // ロック中（暗く・半透明）
		}

		tank.sprite->Update(); // エンジンの仕様に合わせてUpdateを呼ぶ
	}
}

void Player::DrawEncyclopedia() {

	if (isChangeMode) {
		sprite->Draw();

		for (auto& tank : encyclopedia_) {
			tank.sprite->Draw(); // 各スプライトが持つ位置で描画
		}
	}
}

int Player::GetRankFromLevel(int level) {
	if (level >= 15) return 4;
	if (level >= 10) return 3;
	if (level >= 5) return 2;
	return 1;
}

int Player::GetUpgradeLevel(int index) const
{
	if (index < 0 || index >= static_cast<int>(upgradeLevels_.size())) {
		return 0;
	}
	return upgradeLevels_[index];
}

const char* Player::GetCurrentClassName() const
{
	switch (currentClass_) {
	case ClassType::Basic: return "Basic";
	case ClassType::Twin: return "Twin";
	case ClassType::MachineGun: return "MachineGun";
	case ClassType::Overseer: return "Overseer";
	case ClassType::Triple: return "Triple";
	case ClassType::Assassin: return "Assassin";
	case ClassType::Bounder: return "Bounder";
	case ClassType::Ninja: return "Ninja";
	case ClassType::Smasher: return "Smasher";
	case ClassType::Summoner: return "Summoner";
	}
	return "Unknown";
}

bool Player::ApplyStatUpgrade(int index)
{
	if (skillPoints_ <= 0 || index < 0 || index >= static_cast<int>(upgradeLevels_.size())) {
		return false;
	}
	if (upgradeLevels_[index] >= maxEnhancePoint) {
		return false;
	}

	upgradeLevels_[index]++;
	skillPoints_--;

	switch (index) {
	case 0:
		stats_.staminaRecovery += 0.25f;
		break;
	case 1:
		stats_.maxHp += 1.0f;
		hp_++;
		{
			auto hpSprite = std::make_unique<Sprite>();
			hpSprite->Initialize(SpriteCommon::GetInstance(), "resources/playerSprite.png");
			hpSprite->SetSize({ 20.0f, 20.0f });
			hpSprites_.push_back(std::move(hpSprite));
		}
		break;
	case 2:
		stats_.bodyDamage += 1.0f;
		SetDamage(static_cast<uint32_t>(stats_.bodyDamage));
		break;
	case 3:
		stats_.bulletSpeed += 0.04f;
		break;
	case 4:
		stats_.bulletDamage += 1.0f;
		break;
	case 5:
		stats_.reloadSpeed = (std::max)(3.0f, stats_.reloadSpeed - 1.0f);
		break;
	case 6:
		stats_.moveSpeed += 0.025f;
		break;
	}

	return true;
}

void Player::UpdateStealth(float deltaTime) {
	// Assassinの仕様: しばらく経つとステルス
	if (currentClass_ == ClassType::Assassin) {
		// 移動入力があるか、攻撃しているかをチェック
		if (Length(velocity_) > 0.1f || input_->IsPress(input_->GetMouseState().rgbButtons[0])) {
			stealthTimer_ = 0.0f;
			isStealth_ = false;
		} else {
			stealthTimer_ += deltaTime;
			if (stealthTimer_ >= 2.0f) { // 2秒静止でステルス
				isStealth_ = true;
			}
		}
	}

	// Ninjaの仕様: ダッシュまたはジャスト回避でステルス
	if (currentClass_ == ClassType::Ninja) {
		if (isDashing_ || isJustEvaded_) {
			isStealth_ = true;
			stealthTimer_ = 1.5f; // 1.5秒間持続
		}

		if (stealthTimer_ > 0) {
			stealthTimer_ -= deltaTime;
		} else {
			isStealth_ = false;
		}
	}

	// アルファ値の適用 (描画時に反映させる)
	float targetAlpha = isStealth_ ? 0.2f : 1.0f;
	stealthAlpha_ = Lerp(stealthAlpha_, targetAlpha, 0.1f);
	// 実際のモデルのカラーに適用
	object_->SetAlpha(stealthAlpha_);
}

void Player::UpdateSummoner(float deltaTime) {
	if (currentClass_ != ClassType::Summoner) return;

	// ドローンの数が足りなければ生成
	if (drones_.size() < kMaxSummonerDrones) {
		summonTimer_ += deltaTime;
		if (summonTimer_ > 2.0f) {
			// ドローンを生成してリストに追加する処理
			// drones_.push_back(std::make_unique<PlayerDrone>(...));
			summonTimer_ = 0.0f;
		}
	}
}

// 薬莢（スモールパーティクル）を生成する汎用関数
void Player::SpawnCasing() {
	ParticleManager::GetInstance()->Emit("CasingSpark", GetWorldPosition() + dir_ * 1.0f, 2);
}

// 残像を生成する関数
void Player::SpawnAfterimage() {
	ParticleManager::GetInstance()->Emit("DashDust", GetWorldPosition(), isJustEvaded_ ? 4 : 2);
}

// バフ中の粒子を生成
void Player::SpawnBuffParticle() {
	ParticleManager::GetInstance()->Emit("DashDust", GetWorldPosition(), 1);
}
Vector2 Player::WorldToScreen(const Vector3& worldPos, Camera* camera) {
	// 1. ビュープロジェクション行列で変換
	//Matrix4x4 matViewport = MakeViewportMatrix(0, 0, WinApp::kClientWidth, WinApp::kClientHeight, 0, 1);
	Matrix4x4 matVP = camera->GetViewMatrix() * camera->GetProjectionMatrix();
	Vector3 ndcPos = TransformMatrix(worldPos, matVP);

	// 2. NDC座標 (-1.0 ~ 1.0) をスクリーン座標 (0 ~ ウィンドウ幅/高) に変換
	// ※ WinAppなどのシングルトンから画面サイズを取得してください
	float screenX = (ndcPos.x + 1.0f) * 0.5f * WinApp::kClientWidth;
	float screenY = (1.0f - ndcPos.y) * 0.5f * WinApp::kClientHeight;

	return { screenX, screenY };
}
