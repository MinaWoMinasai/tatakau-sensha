#include "Player.h"
#include "Stage.h"
#include "Audio.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

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

Vector3 ReadVector3(const nlohmann::json& json, const Vector3& fallback)
{
	if (!json.is_array() || json.size() < 3) {
		return fallback;
	}
	return {
		json[0].get<float>(),
		json[1].get<float>(),
		json[2].get<float>()
	};
}

Vector4 ReadVector4(const nlohmann::json& json, const Vector4& fallback)
{
	if (!json.is_array() || json.size() < 4) {
		return fallback;
	}
	return {
		json[0].get<float>(),
		json[1].get<float>(),
		json[2].get<float>(),
		json[3].get<float>()
	};
}

Vector2 ReadVector2Object(const nlohmann::json& json, const Vector2& fallback)
{
	if (!json.is_object()) {
		return fallback;
	}
	Vector2 value = fallback;
	if (json.contains("x") && json["x"].is_number()) value.x = json["x"].get<float>();
	if (json.contains("y") && json["y"].is_number()) value.y = json["y"].get<float>();
	return value;
}

nlohmann::json WriteVector2Object(const Vector2& value)
{
	return {
		{ "x", value.x },
		{ "y", value.y }
	};
}

ClassType ClassTypeFromString(const std::string& id)
{
	if (id == "Twin") return ClassType::Twin;
	if (id == "MachineGun") return ClassType::MachineGun;
	if (id == "Overseer") return ClassType::Overseer;
	if (id == "Triple") return ClassType::Triple;
	if (id == "Assassin") return ClassType::Assassin;
	if (id == "Bounder") return ClassType::Bounder;
	if (id == "Ninja") return ClassType::Ninja;
	if (id == "Smasher") return ClassType::Smasher;
	if (id == "Summoner") return ClassType::Summoner;
	return ClassType::Basic;
}

const std::array<ClassType, 10>& EditableClassTypes()
{
	static const std::array<ClassType, 10> types = {
		ClassType::Basic,
		ClassType::Twin,
		ClassType::MachineGun,
		ClassType::Overseer,
		ClassType::Triple,
		ClassType::Assassin,
		ClassType::Bounder,
		ClassType::Ninja,
		ClassType::Smasher,
		ClassType::Summoner
	};
	return types;
}

const char* ClassTypeToString(ClassType type)
{
	switch (type) {
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

WeaponType WeaponTypeFromString(const std::string& id)
{
	if (id == "Laser") return WeaponType::Laser;
	if (id == "Mine") return WeaponType::Mine;
	if (id == "Drone") return WeaponType::Drone;
	if (id == "Melee") return WeaponType::Melee;
	return WeaponType::Projectile;
}

const char* WeaponTypeToString(WeaponType type)
{
	switch (type) {
	case WeaponType::Projectile: return "Projectile";
	case WeaponType::Laser: return "Laser";
	case WeaponType::Mine: return "Mine";
	case WeaponType::Drone: return "Drone";
	case WeaponType::Melee: return "Melee";
	}
	return "Projectile";
}

const char* ClassTexturePath(ClassType type)
{
	switch (type) {
	case ClassType::Twin: return "resources/twin.png";
	case ClassType::MachineGun: return "resources/machineGun.png";
	case ClassType::Overseer:
	case ClassType::Summoner: return "resources/drone.png";
	default: return "resources/normalTank.png";
	}
}

const std::array<const char*, 7>& UpgradeHudNames()
{
	static const std::array<const char*, 7> names = {
		"自動回復",
		"最大HP",
		"体当たり",
		"弾速",
		"弾ダメージ",
		"リロード",
		"移動速度"
	};
	return names;
}

void SetLabel(std::unique_ptr<TextLabel>& label, SpriteCommon* spriteCommon, const std::string& text, const Vector2& position, const TextStyle& style)
{
	if (!label) {
		label = std::make_unique<TextLabel>();
		label->Initialize(spriteCommon, text, style);
	} else {
		label->SetStyle(style);
		label->SetText(text);
	}
	label->SetPosition(position);
}

nlohmann::json Vector3ToJson(const Vector3& value)
{
	return nlohmann::json::array({ value.x, value.y, value.z });
}

nlohmann::json Vector4ToJson(const Vector4& value)
{
	return nlohmann::json::array({ value.x, value.y, value.z, value.w });
}
}

Player::~Player() {

}

void Player::Attack(BulletManager* bulletManager, float deltaTime) {

	// 弾のクールタイムを計算する
	bulletCoolTime -= deltaTime;
	if (bulletCoolTime > 0.0f) return; // クールタイム中は抜ける

	if (input_->IsPress(input_->GetMouseState().rgbButtons[0]) && !upgradeHudMouseCaptured_) {

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

			if (const PlayerClassConfig* config = GetCurrentClassConfig()) {
				if (FireConfiguredClass(*config, bulletManager, baseReload, recoilDir, recoilPower)) {
					velocity_ += recoilDir * recoilPower;
					Audio::GetInstance()->PlayAudioSE(L"bulletShoot", 0.6f);
					return;
				}
			}

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
				if (!barrels_.empty()) {
					barrels_[0].recoilOffset = 0.22f;
				}
				
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
					if (!barrels_.empty()) {
						barrels_[0].recoilOffset = 0.22f;
					}
					shootBarrelIndex_ = 1; // 次は右
				} else {
					// 右から発射
					attackController_.Fire(origin + rightDir * offsetValue, dir_, param, BulletOwner::kPlayer);
					firedByClass = true;
					if (barrels_.size() > 1) {
						barrels_[1].recoilOffset = 0.22f;
					}
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

	const PlayerClassConfig* config = GetCurrentClassConfig();
	const size_t maxDrones = static_cast<size_t>((std::max)(0, config ? config->maxDrones : 7));
	if (drones_.size() >= maxDrones) {
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
	baseVehicleColor_ = object_->GetColor();

	worldTransform_ = InitWorldTransform();
	worldTransform_.translate = position;
	object_->SetTransform(worldTransform_);
	object_->Update();
	LoadPlayerClassConfigs();
	InitializeBarrels();
	UpdateBarrelLayout();

	// シングルトンインスタンス
	input_ = Input::GetInstance();

	// 衝突属性を設定
	SetCollisionAttribute(kCollisionAttributePlayer);
	// 衝突対象を自分の属性以外に設定
	SetCollisionMask(kCollisionAttributeEnemy | kCollisionAttributeExpEnemy | kCollisionAttributeEnemyBullet);
	SetDamage(static_cast<uint32_t>(stats_.bodyDamage));

	level_ = 1;
	exp_ = 0;
	nextLevelExp_ = GetNextLevelExp();
	hp_ = GetMaxHp();

	machineGunBtnSprite_ = std::make_unique<Sprite>();
	machineGunBtnSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white512x512.png");
	machineGunBtnSprite_->SetPosition({ btnPos_ });
	machineGunBtnSprite_->SetSize({ btnSize_ });

	InitializeEncyclopedia();
	InitializeUpgradeHud();
	LoadUpgradeHudConfig();
	ApplyUpgradeHudLayout();

}

void Player::Update(Camera* viewProjection, Stage& stage, BulletManager* BulletManager, float deltaTime)
{
	sprite->Update();

	POINT mousePos;
	GetCursorPos(&mousePos);
	ScreenToClient(WinApp::GetInstance()->GetHwnd(), &mousePos);
	mousePosition_ = { static_cast<float>(mousePos.x), static_cast<float>(mousePos.y) };
	UpdateEncyclopedia();
	UpdateUpgradeHud();

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
	if (damageFeedbackTimer_ > 0.0f) {
		damageFeedbackTimer_ = (std::max)(0.0f, damageFeedbackTimer_ - deltaTime);
	}

	if (dashTimer_ > 0.0f) {
		dashTimer_ -= deltaTime;
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

	if (!isDead_ && !isDashing_ && Length(inputDir_) > 0.05f) {
		movementParticleTimer_ -= deltaTime;
		if (movementParticleTimer_ <= 0.0f) {
			SpawnAfterimage();
			movementParticleTimer_ = movementParticleInterval_;
		}
	} else {
		movementParticleTimer_ = 0.0f;
	}

	// object_ の更新だけ（移動はしない）
	object_->SetTransform(worldTransform_);
	object_->Update();
	UpdateBarrelLayout();

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
	if (isDead_) {
		if (deathChargeTimer_ <= 0.0f) {
			return;
		}

		const float progress = 1.0f - deathChargeTimer_ / deathChargeDuration_;
		const float charge = progress * progress;
		Transform chargeTransform = worldTransform_;
		chargeTransform.scale = worldTransform_.scale * (1.0f + charge * 0.65f);
		const Vector4 savedColor = object_->GetColor();
		const bool savedLighting = object_->IsLightingEnabled();
		object_->SetTransform(chargeTransform);
		object_->SetLighting(false);
		object_->SetColor({ 2.4f, 2.4f, 2.4f, 1.0f });
		object_->Update();
		object_->Draw();
		object_->SetTransform(worldTransform_);
		object_->SetColor(savedColor);
		object_->SetLighting(savedLighting);
		object_->Update();
		return;
	}

	if (!isDead_) {
		// 無敵時間中は点滅

		if (invincibleTimer_ > 0.0f && !isJustEvaded_) {
			if (static_cast<int>(invincibleTimer_ * 10) % 5 == 0) {
				// 透明度を下げる
				if (isStealth_) {
					SetVehicleAlpha(0.2f);
				} else {
					SetVehicleAlpha(0.5f);
				}
				if (damageFeedbackTimer_ > 0.0f) {
					Transform drawTransform = worldTransform_;
					const float t = damageFeedbackTimer_ / damageFeedbackDuration_;
					const float pulse = std::sin(t * 3.14159265f) * 0.10f;
					drawTransform.scale = worldTransform_.scale * (1.0f + pulse);
					object_->SetTransform(drawTransform);
					object_->Update();
				}
				object_->Draw();
				DrawBarrels();
				return;
			}
		}
		if (!isStealth_) {
			SetVehicleAlpha(1.0f);
		}
		if (damageFeedbackTimer_ > 0.0f) {
			Transform drawTransform = worldTransform_;
			const float t = damageFeedbackTimer_ / damageFeedbackDuration_;
			const float pulse = std::sin(t * 3.14159265f) * 0.10f;
			drawTransform.scale = worldTransform_.scale * (1.0f + pulse);
			object_->SetTransform(drawTransform);
			object_->Update();
		}
		object_->Draw();
		DrawBarrels();
	}

}

void Player::DrawSprite()
{
	SpriteCommon::GetInstance()->PreDraw(kNormal);
	DrawUpgradeHud();
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

	if (other->GetCollisionAttribute() == kCollisionAttributeEnemy ||
		other->GetCollisionAttribute() == kCollisionAttributeExpEnemy ||
		other->GetCollisionAttribute() == kCollisionAttributeEnemyBullet) {
		TakeDamage(other->GetDamage(), 0.45f);
	}
}

AABB Player::GetAABB() {
	Vector3 worldPos = GetWorldPosition();

	AABB aabb;

	aabb.min = { worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f };
	aabb.max = { worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f };

	return aabb;
}

void Player::Damage(uint32_t amount)
{
	TakeDamage(amount, 0.45f);
}

void Player::TakeDamage(uint32_t amount, float invincibleTime)
{
	if (isDead_ || amount == 0 || invincibleTimer_ > 0.0f || debugNoDamage_) {
		return;
	}

	// --- ジャスト回避判定 ---
	//if (isDashing_ && (kDashDuration - dashTimer_) <= kJustEvadeWindow) {
	//	requestSlow_ = true;
	//	return;
	//}

	hp_ -= static_cast<int>(amount);
	if (hp_ < 0) {
		hp_ = 0;
	}

	TriggerDamageFeedback();
	invincibleTimer_ = invincibleTime;
	if (hp_ <= 0) {
		Die();
	}
}

void Player::ApplyBalanceConfig(const BalanceConfig& config)
{
	baseStats_.maxHp = static_cast<float>((std::max)(1, config.maxHp));
	baseStats_.reloadSpeed = (std::max)(0.05f, config.reloadSpeed);
	baseStats_.bulletDamage = (std::max)(0.1f, config.bulletDamage);
	baseStats_.bulletSpeed = (std::max)(0.01f, config.bulletSpeed);
	baseStats_.moveSpeed = (std::max)(0.01f, config.moveSpeed);
	baseStats_.staminaRecovery = (std::max)(0.0f, config.staminaRecovery);
	baseStats_.maxStamina = (std::max)(0.0f, config.maxStamina);
	baseStats_.bodyDamage = static_cast<float>((std::max)(1u, config.bodyDamage));

	healthRegenUpgradeRate_ = (std::max)(0.0f, config.healthRegenUpgrade);
	maxHpUpgradeRate_ = (std::max)(0.0f, config.maxHpUpgradeAmount);
	bodyDamageUpgradeRate_ = (std::max)(0.0f, config.bodyDamageUpgrade);
	bulletSpeedUpgradeRate_ = config.bulletSpeedUpgrade;
	bulletDamageUpgradeRate_ = (std::max)(0.0f, config.bulletDamageUpgrade);
	reloadUpgradeRate_ = (std::max)(0.0f, config.reloadUpgrade);
	moveSpeedUpgradeRate_ = config.moveSpeedUpgrade;
	minReloadSpeed_ = (std::max)(0.05f, config.minReloadSpeed);

	RecalculateStatsFromBase(config.healToFull);
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
	return level_ * 10 + 20;
}

void Player::Evolve(ClassType newClass)
{
	EvolveById(ClassTypeToString(newClass));
}

void Player::EvolveById(const std::string& classId)
{
	const PlayerClassConfig* config = GetClassConfig(classId);
	if (!config) {
		return;
	}

	currentClassId_ = config->id;
	currentClass_ = config->type;
	
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
	InitializeBarrels();
	UpdateBarrelLayout();

	isChangeMode = false;
}

void Player::LoadPlayerClassConfigs(const std::string& path)
{
	classConfigs_.clear();
	classOrder_.clear();
	for (ClassType type : EditableClassTypes()) {
		PlayerClassConfig config = CreateDefaultClassConfig(type);
		classOrder_.push_back(config.id);
		classConfigs_[config.id] = config;
	}

	std::ifstream file(path);
	if (!file.is_open()) {
		return;
	}

	nlohmann::json root;
	file >> root;
	const nlohmann::json& classes = root.contains("classes") ? root["classes"] : root;
	if (!classes.is_array()) {
		return;
	}

	for (const nlohmann::json& item : classes) {
		const std::string id = item.value("id", "Basic");
		PlayerClassConfig config = CreateDefaultClassConfig(ClassTypeFromString(id));
		config.id = id;
		config.type = ClassTypeFromString(id);
		config.displayName = item.value("displayName", config.displayName);
		config.requiredRank = item.value("requiredRank", config.requiredRank);
		config.usesDrone = item.value("usesDrone", config.usesDrone);
		config.maxDrones = item.value("maxDrones", config.maxDrones);
		config.reloadScale = item.value("reloadScale", config.reloadScale);
		config.bulletSpeedScale = item.value("bulletSpeedScale", config.bulletSpeedScale);
		config.bulletDamageScale = item.value("bulletDamageScale", config.bulletDamageScale);
		config.bulletCount = item.value("bulletCount", config.bulletCount);
		config.spreadAngleDeg = item.value("spreadAngleDeg", config.spreadAngleDeg);
		config.randomSpread = item.value("randomSpread", config.randomSpread);
		config.reflect = item.value("reflect", config.reflect);
		config.penetrate = item.value("penetrate", config.penetrate);
		config.fireAllBarrels = item.value("fireAllBarrels", config.fireAllBarrels);
		config.alternateBarrels = item.value("alternateBarrels", config.alternateBarrels);
		config.recoilPower = item.value("recoilPower", config.recoilPower);

		config.barrels.clear();
		const nlohmann::json* mountsJson = nullptr;
		if (item.contains("weaponMounts") && item["weaponMounts"].is_array()) {
			mountsJson = &item["weaponMounts"];
		} else if (item.contains("barrels") && item["barrels"].is_array()) {
			mountsJson = &item["barrels"];
		}
		if (mountsJson) {
			for (const nlohmann::json& barrelJson : *mountsJson) {
				WeaponMountConfig barrel{};
				barrel.model = barrelJson.value("model", barrel.model);
				barrel.offset = ReadVector3(barrelJson.value("offset", nlohmann::json::array()), barrel.offset);
				barrel.scale = ReadVector3(barrelJson.value("scale", nlohmann::json::array()), barrel.scale);
				barrel.angleDeg = barrelJson.value("angleDeg", barrel.angleDeg);
				barrel.muzzleForward = barrelJson.value("muzzleForward", barrel.muzzleForward);
				barrel.fires = barrelJson.value("fires", barrel.fires);
				barrel.weaponType = WeaponTypeFromString(barrelJson.value("weaponType", std::string("Projectile")));
				barrel.damageScale = (std::max)(0.0f, barrelJson.value("damageScale", barrel.damageScale));
				barrel.projectileSpeedScale = (std::max)(0.01f, barrelJson.value("projectileSpeedScale", barrel.projectileSpeedScale));
				if (barrelJson.contains("effectColor")) {
					barrel.effectColor = ReadVector4(barrelJson["effectColor"], barrel.effectColor);
				}
				barrel.laserRange = (std::max)(0.1f, barrelJson.value("laserRange", barrel.laserRange));
				barrel.laserWidth = (std::max)(0.01f, barrelJson.value("laserWidth", barrel.laserWidth));
				barrel.laserDuration = (std::max)(0.01f, barrelJson.value("laserDuration", barrel.laserDuration));
				barrel.laserDamageInterval = (std::max)(0.01f, barrelJson.value("laserDamageInterval", barrel.laserDamageInterval));
				barrel.mineRadius = (std::max)(0.1f, barrelJson.value("mineRadius", barrel.mineRadius));
				barrel.mineFuseTime = (std::max)(0.0f, barrelJson.value("mineFuseTime", barrel.mineFuseTime));
				barrel.mineLifeTime = (std::max)(0.1f, barrelJson.value("mineLifeTime", barrel.mineLifeTime));
				barrel.meleeRange = (std::max)(0.1f, barrelJson.value("meleeRange", barrel.meleeRange));
				barrel.meleeArcDeg = (std::clamp)(barrelJson.value("meleeArcDeg", barrel.meleeArcDeg), 5.0f, 360.0f);
				barrel.meleeWidth = (std::max)(0.01f, barrelJson.value("meleeWidth", barrel.meleeWidth));
				barrel.meleeDuration = (std::max)(0.01f, barrelJson.value("meleeDuration", barrel.meleeDuration));
				config.barrels.push_back(barrel);
			}
		}
		if (config.barrels.empty()) {
			config.barrels = CreateDefaultClassConfig(config.type).barrels;
		}

		if (classConfigs_.find(config.id) == classConfigs_.end()) {
			classOrder_.push_back(config.id);
		}
		classConfigs_[config.id] = config;
	}

	if (classConfigs_.find(currentClassId_) == classConfigs_.end()) {
		currentClassId_ = "Basic";
		currentClass_ = ClassType::Basic;
	}
}

Player::PlayerClassConfig Player::CreateDefaultClassConfig(ClassType type) const
{
	PlayerClassConfig config{};
	config.type = type;
	config.id = ClassTypeToString(type);
	config.displayName = ClassTypeToString(type);
	config.requiredRank = 1;
	config.spreadAngleDeg = 10.0f;
	config.randomSpread = true;
	config.reloadScale = 1.0f;
	config.alternateBarrels = false;
	config.barrels = {
		{ "gunBarrel.obj", { 0.72f, 0.0f, 0.0f }, { 1.25f, 0.24f, 0.24f }, 0.0f, 0.95f, true }
	};

	if (type == ClassType::Twin) {
		config.id = "Twin";
		config.displayName = "Twin";
		config.requiredRank = 2;
		config.spreadAngleDeg = 2.0f;
		config.randomSpread = true;
		config.reloadScale = 1.0f / 2.5f;
		config.alternateBarrels = true;
		config.barrels = {
			{ "gunBarrel.obj", { 0.72f, -0.34f, 0.0f }, { 1.25f, 0.24f, 0.24f }, 0.0f, 0.95f, true },
			{ "gunBarrel.obj", { 0.72f,  0.34f, 0.0f }, { 1.25f, 0.24f, 0.24f }, 0.0f, 0.95f, true }
		};
		return config;
	}

	if (type == ClassType::MachineGun) {
		config.requiredRank = 2;
		config.spreadAngleDeg = 30.0f;
		config.reloadScale = 0.6f;
	}
	if (type == ClassType::Overseer) {
		config.requiredRank = 2;
		config.usesDrone = true;
		config.recoilPower = 0.0f;
	}
	if (type == ClassType::Triple) {
		config.requiredRank = 3;
		config.bulletCount = 3;
		config.spreadAngleDeg = 45.0f;
		config.randomSpread = false;
	}
	if (type == ClassType::Bounder) {
		config.requiredRank = 3;
		config.reflect = true;
	}
	if (type == ClassType::Assassin) {
		config.requiredRank = 3;
		config.bulletSpeedScale = 1.5f;
	}
	if (type == ClassType::Ninja) {
		config.requiredRank = 4;
		config.bulletCount = 3;
		config.spreadAngleDeg = 15.0f;
		config.randomSpread = false;
	}
	if (type == ClassType::Smasher || type == ClassType::Summoner) {
		config.requiredRank = 4;
	}

	return config;
}

void Player::SavePlayerClassConfigs(const std::string& path) const
{
	nlohmann::json classes = nlohmann::json::array();
	for (const std::string& id : classOrder_) {
		const PlayerClassConfig* config = GetClassConfig(id);
		if (!config) {
			continue;
		}

		nlohmann::json item;
		item["id"] = config->id;
		item["displayName"] = config->displayName;
		item["requiredRank"] = config->requiredRank;
		item["usesDrone"] = config->usesDrone;
		item["maxDrones"] = config->maxDrones;
		item["reloadScale"] = config->reloadScale;
		item["bulletSpeedScale"] = config->bulletSpeedScale;
		item["bulletDamageScale"] = config->bulletDamageScale;
		item["bulletCount"] = config->bulletCount;
		item["spreadAngleDeg"] = config->spreadAngleDeg;
		item["randomSpread"] = config->randomSpread;
		item["reflect"] = config->reflect;
		item["penetrate"] = config->penetrate;
		item["fireAllBarrels"] = config->fireAllBarrels;
		item["alternateBarrels"] = config->alternateBarrels;
		item["recoilPower"] = config->recoilPower;
		item["weaponMounts"] = nlohmann::json::array();
		for (const WeaponMountConfig& barrel : config->barrels) {
			nlohmann::json barrelJson;
			barrelJson["model"] = barrel.model;
			barrelJson["offset"] = Vector3ToJson(barrel.offset);
			barrelJson["scale"] = Vector3ToJson(barrel.scale);
			barrelJson["angleDeg"] = barrel.angleDeg;
			barrelJson["muzzleForward"] = barrel.muzzleForward;
			barrelJson["fires"] = barrel.fires;
			barrelJson["weaponType"] = WeaponTypeToString(barrel.weaponType);
			barrelJson["damageScale"] = barrel.damageScale;
			barrelJson["projectileSpeedScale"] = barrel.projectileSpeedScale;
			barrelJson["effectColor"] = Vector4ToJson(barrel.effectColor);
			barrelJson["laserRange"] = barrel.laserRange;
			barrelJson["laserWidth"] = barrel.laserWidth;
			barrelJson["laserDuration"] = barrel.laserDuration;
			barrelJson["laserDamageInterval"] = barrel.laserDamageInterval;
			barrelJson["mineRadius"] = barrel.mineRadius;
			barrelJson["mineFuseTime"] = barrel.mineFuseTime;
			barrelJson["mineLifeTime"] = barrel.mineLifeTime;
			barrelJson["meleeRange"] = barrel.meleeRange;
			barrelJson["meleeArcDeg"] = barrel.meleeArcDeg;
			barrelJson["meleeWidth"] = barrel.meleeWidth;
			barrelJson["meleeDuration"] = barrel.meleeDuration;
			item["weaponMounts"].push_back(barrelJson);
		}
		classes.push_back(item);
	}

	nlohmann::json root;
	root["version"] = 2;
	root["classes"] = classes;
	std::ofstream file(path);
	if (file.is_open()) {
		file << root.dump(2);
	}
}

const Player::PlayerClassConfig* Player::GetClassConfig(ClassType type) const
{
	return GetClassConfig(ClassTypeToString(type));
}

const Player::PlayerClassConfig* Player::GetClassConfig(const std::string& classId) const
{
	auto it = classConfigs_.find(classId);
	if (it == classConfigs_.end()) {
		return nullptr;
	}
	return &it->second;
}

const Player::PlayerClassConfig* Player::GetCurrentClassConfig() const
{
	return GetClassConfig(currentClassId_);
}

Player::PlayerClassConfig* Player::GetMutableClassConfig(const std::string& classId)
{
	auto it = classConfigs_.find(classId);
	if (it == classConfigs_.end()) {
		return nullptr;
	}
	return &it->second;
}

std::vector<Player::LaserShotEvent> Player::ConsumeLaserShotEvents()
{
	std::vector<LaserShotEvent> events = std::move(pendingLaserShots_);
	pendingLaserShots_.clear();
	return events;
}

std::vector<Player::MineDropEvent> Player::ConsumeMineDropEvents()
{
	std::vector<MineDropEvent> events = std::move(pendingMineDrops_);
	pendingMineDrops_.clear();
	return events;
}

std::vector<Player::MeleeSlashEvent> Player::ConsumeMeleeSlashEvents()
{
	std::vector<MeleeSlashEvent> events = std::move(pendingMeleeSlashes_);
	pendingMeleeSlashes_.clear();
	return events;
}

bool Player::FireConfiguredClass(const PlayerClassConfig& config, BulletManager* bulletManager, float baseReload, Vector3& recoilDir, float& recoilPower)
{
	if (config.usesDrone) {
		DroneShoot(bulletManager);
		recoilPower = 0.0f;
		bulletCoolTime = baseReload * config.reloadScale;
		return true;
	}

	AttackParam param{};
	param.bulletSpeed = stats_.bulletSpeed * config.bulletSpeedScale;
	param.bulletCount = config.bulletCount;
	param.spreadAngleDeg = config.spreadAngleDeg;
	param.randomSpread = config.randomSpread;
	param.reflect = config.reflect;
	param.penetrate = config.penetrate;
	param.cooldown = 1.0f;
	param.damage = static_cast<uint32_t>((std::max)(1.0f, stats_.bulletDamage * config.bulletDamageScale));

	if (isBuffActive_) {
		param.reflect = true;
		param.spreadAngleDeg += 10.0f;
	}

	std::vector<size_t> fireIndices;
	for (size_t i = 0; i < config.barrels.size(); ++i) {
		if (config.barrels[i].fires &&
			(config.barrels[i].weaponType == WeaponType::Projectile ||
			 config.barrels[i].weaponType == WeaponType::Laser ||
			 config.barrels[i].weaponType == WeaponType::Mine ||
			 config.barrels[i].weaponType == WeaponType::Melee)) {
			fireIndices.push_back(i);
		}
	}
	if (fireIndices.empty()) {
		return false;
	}

	if (config.alternateBarrels && !config.fireAllBarrels) {
		const size_t selectableCount = fireIndices.size();
		const size_t index = fireIndices[shootBarrelIndex_ % selectableCount];
		fireIndices = { index };
		shootBarrelIndex_ = static_cast<int>((shootBarrelIndex_ + 1) % selectableCount);
	}

	const Vector3 forward = Length(dir_) > 0.0001f ? Normalize(dir_) : Vector3{ 1.0f, 0.0f, 0.0f };
	const Vector3 right = { -forward.y, forward.x, 0.0f };

	for (size_t index : fireIndices) {
		const WeaponMountConfig& barrelConfig = config.barrels[index];
		const Vector3 fireDir = RotateDirection(forward, barrelConfig.angleDeg);
		const Vector3 mountBase =
			worldTransform_.translate +
			forward * barrelConfig.offset.x +
			right * barrelConfig.offset.y +
			Vector3{ 0.0f, 0.0f, barrelConfig.offset.z };
		const Vector3 muzzle = mountBase + fireDir * barrelConfig.muzzleForward;
		AttackParam mountParam = param;
		mountParam.damage = static_cast<uint32_t>((std::max)(1.0f, static_cast<float>(param.damage) * barrelConfig.damageScale));
		if (barrelConfig.weaponType == WeaponType::Laser) {
			LaserShotEvent event{};
			event.origin = muzzle;
			event.direction = fireDir;
			event.range = barrelConfig.laserRange;
			event.width = barrelConfig.laserWidth;
			event.duration = barrelConfig.laserDuration;
			event.damageInterval = barrelConfig.laserDamageInterval;
			event.damage = mountParam.damage;
			event.color = barrelConfig.effectColor;
			pendingLaserShots_.push_back(event);
		} else if (barrelConfig.weaponType == WeaponType::Mine) {
			MineDropEvent event{};
			event.position = muzzle;
			event.radius = barrelConfig.mineRadius;
			event.fuseTime = barrelConfig.mineFuseTime;
			event.lifeTime = barrelConfig.mineLifeTime;
			event.damage = mountParam.damage;
			event.color = barrelConfig.effectColor;
			pendingMineDrops_.push_back(event);
		} else if (barrelConfig.weaponType == WeaponType::Melee) {
			MeleeSlashEvent event{};
			event.origin = mountBase;
			event.direction = fireDir;
			event.range = barrelConfig.meleeRange;
			event.arcDeg = barrelConfig.meleeArcDeg;
			event.width = barrelConfig.meleeWidth;
			event.duration = barrelConfig.meleeDuration;
			event.damage = mountParam.damage;
			event.color = barrelConfig.effectColor;
			pendingMeleeSlashes_.push_back(event);
		} else {
			mountParam.bulletSpeed *= barrelConfig.projectileSpeedScale;
			attackController_.FireFromMuzzle(muzzle, fireDir, mountParam, BulletOwner::kPlayer);
			SpawnCasing();
		}
		if (index < barrels_.size()) {
			barrels_[index].recoilOffset = 0.22f;
		}
	}

	bulletCoolTime = baseReload * config.reloadScale;
	recoilDir = Normalize(dir_) * -1.0f;
	recoilPower = config.recoilPower;
	return true;
}

Vector3 Player::RotateDirection(const Vector3& direction, float angleDeg) const
{
	const float rad = angleDeg * 3.1415926535f / 180.0f;
	return {
		direction.x * cosf(rad) - direction.y * sinf(rad),
		direction.x * sinf(rad) + direction.y * cosf(rad),
		direction.z
	};
}

void Player::InitializeBarrels()
{
	barrels_.clear();
	const PlayerClassConfig* config = GetCurrentClassConfig();
	const size_t barrelCount = config ? config->barrels.size() : 1u;
	barrels_.reserve(barrelCount);
	for (size_t i = 0; i < barrelCount; ++i) {
		BarrelModel barrel{};
		barrel.object = std::make_unique<Object3d>();
		barrel.object->Initialize();
		barrel.object->SetModel(config ? config->barrels[i].model : "gunBarrel.obj");
		barrel.object->SetColor(Vector4(0.48f, 0.86f, 0.22f, 1.0f));
		baseBarrelColor_ = Vector4(0.48f, 0.86f, 0.22f, 1.0f);
		barrel.transform = InitWorldTransform();
		barrel.transform.scale = config ? config->barrels[i].scale : Vector3{ 1.25f, 0.24f, 0.24f };
		barrels_.push_back(std::move(barrel));
	}
}

void Player::UpdateBarrelLayout()
{
	if (barrels_.empty()) {
		return;
	}

	const Vector3 forward = Length(dir_) > 0.0001f ? Normalize(dir_) : Vector3{ 1.0f, 0.0f, 0.0f };
	const Vector3 right = { -forward.y, forward.x, 0.0f };
	const PlayerClassConfig* config = GetCurrentClassConfig();
	const float recoilReturn = 0.055f;

	for (size_t i = 0; i < barrels_.size(); ++i) {
		BarrelModel& barrel = barrels_[i];
		const WeaponMountConfig barrelConfig = (config && i < config->barrels.size()) ? config->barrels[i] : WeaponMountConfig{};
		const bool active = config ? i < config->barrels.size() : i == 0;

		barrel.recoilOffset = (std::max)(0.0f, barrel.recoilOffset - recoilReturn * dt_ * 60.0f);
		barrel.localOffset = forward * (barrelConfig.offset.x - barrel.recoilOffset) + right * barrelConfig.offset.y + Vector3{ 0.0f, 0.0f, barrelConfig.offset.z };
		barrel.transform.translate = worldTransform_.translate + barrel.localOffset;
		barrel.transform.rotate = worldTransform_.rotate;
		barrel.transform.rotate.z += barrelConfig.angleDeg * 3.1415926535f / 180.0f;
		barrel.transform.scale = active ? barrelConfig.scale : Vector3{ 0.0f, 0.0f, 0.0f };
		barrel.object->SetTransform(barrel.transform);
		barrel.object->Update();
	}
}

void Player::DrawBarrels()
{
	const PlayerClassConfig* config = GetCurrentClassConfig();
	const size_t activeCount = config ? config->barrels.size() : 1u;
	for (size_t i = 0; i < barrels_.size() && i < activeCount; ++i) {
		barrels_[i].object->Draw();
	}
}

void Player::SetVehicleAlpha(float alpha)
{
	float flash = 0.0f;
	if (damageFeedbackTimer_ > 0.0f && damageFeedbackDuration_ > 0.0f) {
		const float t = damageFeedbackTimer_ / damageFeedbackDuration_;
		flash = t * t * 0.75f;
	}
	Vector4 vehicleColor = LerpColor(baseVehicleColor_, { 1.0f, 1.0f, 1.0f, baseVehicleColor_.w }, flash);
	vehicleColor.w = alpha;
	object_->SetColor(vehicleColor);
	for (BarrelModel& barrel : barrels_) {
		if (barrel.object) {
			Vector4 barrelColor = LerpColor(baseBarrelColor_, { 1.0f, 1.0f, 1.0f, baseBarrelColor_.w }, flash);
			barrelColor.w = alpha;
			barrel.object->SetColor(barrelColor);
		}
	}
}

void Player::TriggerDamageFeedback()
{
	damageFeedbackTimer_ = damageFeedbackDuration_;
}

void Player::InitializeEncyclopedia()
{
	SpriteCommon* spriteCommon = SpriteCommon::GetInstance();
	auto makePanel = [spriteCommon](const Vector2& pos, const Vector2& size, const Vector4& color) {
		auto panel = std::make_unique<Sprite>();
		panel->Initialize(spriteCommon, "resources/white512x512.png");
		panel->SetPosition(pos);
		panel->SetSize(size);
		panel->SetColor(color);
		return panel;
	};

	evolutionBackdropSprite_ = makePanel({ 0.0f, 0.0f }, { 1280.0f, 720.0f }, { 0.02f, 0.03f, 0.07f, 0.78f });
	evolutionPreviewPanelSprite_ = makePanel({ 28.0f, 84.0f }, { 360.0f, 560.0f }, { 0.05f, 0.12f, 0.17f, 0.86f });
	evolutionStatsPanelSprite_ = makePanel({ 910.0f, 84.0f }, { 342.0f, 560.0f }, { 0.07f, 0.08f, 0.12f, 0.88f });
	evolutionPreviewTankSprite_ = std::make_unique<Sprite>();
	evolutionPreviewTankSprite_->Initialize(spriteCommon, "resources/normalTank.png");
	evolutionPreviewTankSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	evolutionPreviewTankSprite_->SetSize({ 250.0f, 150.0f });
	evolutionPreviewTankSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	evolutionShotSprite_ = makePanel({ 0.0f, 0.0f }, { 170.0f, 9.0f }, { 1.0f, 0.88f, 0.28f, 0.0f });
	evolutionShotSprite_->SetAnchorPoint({ 0.0f, 0.5f });
	evolutionChangeButtonSprite_ = makePanel({ 940.0f, 650.0f }, { 290.0f, 48.0f }, { 0.24f, 0.86f, 0.44f, 0.92f });

	TextStyle titleStyle{};
	titleStyle.fontFamily = "Meiryo";
	titleStyle.fontSize = 34.0f;
	titleStyle.color = { 0.75f, 1.0f, 0.92f, 1.0f };
	titleStyle.outlineColor = { 0.0f, 0.08f, 0.10f, 0.95f };
	titleStyle.outlineThickness = 3.0f;
	titleStyle.padding = 8.0f;
	SetLabel(evolutionTitleLabel_, spriteCommon, "戦車図鑑 / 進化ツリー", { 40.0f, 28.0f }, titleStyle);

	TextStyle smallStyle = titleStyle;
	smallStyle.fontSize = 20.0f;
	smallStyle.color = { 0.86f, 0.92f, 1.0f, 1.0f };
	smallStyle.outlineThickness = 2.0f;
	SetLabel(evolutionHintLabel_, spriteCommon, "C:閉じる / カード選択:詳細 / ボタン:機体変更", { 520.0f, 42.0f }, smallStyle);

	encyclopedia_.clear();
	const float cardWidth = 154.0f;
	const float cardHeight = 80.0f;
	const float cardGapX = 12.0f;
	const float cardGapY = 12.0f;
	const Vector2 cardBase = { 416.0f, 112.0f };

	for (int i = 0; i < static_cast<int>(classOrder_.size()); ++i) {
		const PlayerClassConfig* config = GetClassConfig(classOrder_[i]);
		if (!config) {
			continue;
		}
		TankData data;
		data.type = config->type;
		data.classId = config->id;
		data.name = config->displayName;
		data.requiredRank = config->requiredRank;
		data.texturePath = ClassTexturePath(config->type);

		const int col = i % 3;
		const int row = i / 3;
		const Vector2 cardPos = { cardBase.x + static_cast<float>(col) * (cardWidth + cardGapX), cardBase.y + static_cast<float>(row) * (cardHeight + cardGapY) };

		data.cardSprite = makePanel(cardPos, { cardWidth, cardHeight }, { 0.10f, 0.15f, 0.22f, 0.82f });
		data.sprite = std::make_unique<Sprite>();
		data.sprite->Initialize(SpriteCommon::GetInstance(), data.texturePath);
		data.sprite->SetPosition({ cardPos.x + 14.0f, cardPos.y + 8.0f });
		data.sprite->SetSize({ 126.0f, 38.0f });

		TextStyle cardNameStyle = smallStyle;
		cardNameStyle.fontSize = 14.0f;
		cardNameStyle.color = { 0.92f, 1.0f, 0.95f, 1.0f };
		SetLabel(data.nameLabel, spriteCommon, data.name, { cardPos.x + 10.0f, cardPos.y + 50.0f }, cardNameStyle);

		TextStyle rankStyle = cardNameStyle;
		rankStyle.fontSize = 12.0f;
		rankStyle.color = { 0.72f, 0.86f, 1.0f, 1.0f };
		SetLabel(data.rankLabel, spriteCommon, "R" + std::to_string(data.requiredRank), { cardPos.x + 112.0f, cardPos.y + 54.0f }, rankStyle);

		encyclopedia_.push_back(std::move(data));
	}
}

void Player::InitializeUpgradeHud()
{
	SpriteCommon* spriteCommon = SpriteCommon::GetInstance();
	auto makePanel = [spriteCommon](const Vector2& pos, const Vector2& size, const Vector4& color) {
		auto panel = std::make_unique<Sprite>();
		panel->Initialize(spriteCommon, "resources/white512x512.png");
		panel->SetPosition(pos);
		panel->SetSize(size);
		panel->SetColor(color);
		return panel;
	};

	upgradeHudBackdropSprite_ = makePanel(upgradeHudPanelPos_, upgradeHudPanelSize_, { 0.03f, 0.04f, 0.06f, 0.58f });
	upgradeHudExpBackSprite_ = makePanel(upgradeHudExpBarPos_, upgradeHudExpBarSize_, { 0.04f, 0.04f, 0.05f, 0.82f });
	upgradeHudExpFillSprite_ = makePanel(upgradeHudExpBarPos_, { 0.0f, upgradeHudExpBarSize_.y }, { 0.96f, 0.83f, 0.24f, 0.95f });
	upgradeHudLevelBackSprite_ = makePanel(upgradeHudLevelBarPos_, upgradeHudLevelBarSize_, { 0.04f, 0.04f, 0.05f, 0.82f });
	upgradeHudLevelFillSprite_ = makePanel({ 785.0f, 786.0f }, { 0.0f, 18.0f }, { 0.36f, 1.0f, 0.56f, 0.92f });

	TextStyle titleStyle{};
	titleStyle.fontFamily = "Meiryo";
	titleStyle.fontSize = 18.0f;
	titleStyle.color = { 0.84f, 1.0f, 0.94f, 1.0f };
	titleStyle.outlineColor = { 0.0f, 0.03f, 0.05f, 0.95f };
	titleStyle.outlineThickness = 2.0f;
	titleStyle.padding = 5.0f;
	SetLabel(upgradeHudTitleLabel_, spriteCommon, "TANK UPGRADES", upgradeHudTitlePos_, titleStyle);

	TextStyle smallStyle = titleStyle;
	smallStyle.fontSize = 15.0f;
	smallStyle.color = { 0.90f, 0.94f, 1.0f, 1.0f };
	SetLabel(upgradeHudPointLabel_, spriteCommon, "", upgradeHudPointPos_, smallStyle);
	SetLabel(upgradeHudExpLabel_, spriteCommon, "", upgradeHudExpTextPos_, smallStyle);
	SetLabel(upgradeHudLevelLabel_, spriteCommon, "", upgradeHudLevelTextPos_, smallStyle);

	for (int i = 0; i < 7; ++i) {
		const float y = upgradeHudRowStart_.y + static_cast<float>(i) * upgradeHudRowGap_;
		upgradeHudButtonSprites_[i] = makePanel({ upgradeHudRowStart_.x, y }, upgradeHudButtonSize_, { 0.10f, 0.12f, 0.15f, 0.84f });
		upgradeHudMinusSprites_[i] = makePanel({ upgradeHudMinusX_, y }, upgradeHudPlusSize_, { 0.26f, 0.42f, 0.86f, 0.68f });
		upgradeHudPlusSprites_[i] = makePanel({ upgradeHudPlusX_, y }, upgradeHudPlusSize_, { 0.34f, 0.95f, 0.64f, 0.88f });
	}
	InitializeUpgradeHudBatch();
}

void Player::InitializeUpgradeHudBatch()
{
	DirectXCommon* dxCommon = SpriteCommon::GetInstance()->GetDxCommon();
	if (!dxCommon) {
		return;
	}
	TextureManager::GetInstance()->LoadTexture("resources/white512x512.png");

	upgradeHudBatchVertexResource_ = dxCommon->CreateBufferResource(sizeof(TrailVertex) * kUpgradeHudBatchMaxVertices);
	upgradeHudBatchVertexBufferView_.BufferLocation = upgradeHudBatchVertexResource_->GetGPUVirtualAddress();
	upgradeHudBatchVertexBufferView_.SizeInBytes = sizeof(TrailVertex) * kUpgradeHudBatchMaxVertices;
	upgradeHudBatchVertexBufferView_.StrideInBytes = sizeof(TrailVertex);
	upgradeHudBatchVertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&upgradeHudBatchVertexData_));

	upgradeHudBatchTransformResource_ = dxCommon->CreateBufferResource(sizeof(Matrix4x4));
	upgradeHudBatchTransformResource_->Map(0, nullptr, reinterpret_cast<void**>(&upgradeHudBatchTransformData_));
	*upgradeHudBatchTransformData_ = MakeOrthographicMatrix(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 100.0f);

	upgradeHudBatchMaterialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
	upgradeHudBatchMaterialResource_->Map(0, nullptr, reinterpret_cast<void**>(&upgradeHudBatchMaterialData_));
	upgradeHudBatchMaterialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	upgradeHudBatchMaterialData_->enableLighting = false;
	upgradeHudBatchMaterialData_->lightingMode = false;
	upgradeHudBatchMaterialData_->environmentCoefficient = 0.0f;
	upgradeHudBatchMaterialData_->padding = 0.0f;
	upgradeHudBatchMaterialData_->uvTransform = MakeIdentity4x4();
	upgradeHudBatchMaterialData_->shininess = 1.0f;
}

void Player::UpdateUpgradeHud()
{
	upgradeHudMouseCaptured_ = false;
	if (!upgradeHudVisible_ || isChangeMode || isDead_) {
		return;
	}

	for (float& timer : upgradeHudFlashTimers_) {
		timer = (std::max)(0.0f, timer - dt_);
	}
	for (float& timer : upgradeHudRefundFlashTimers_) {
		timer = (std::max)(0.0f, timer - dt_);
	}
	for (float& timer : upgradeHudMissFlashTimers_) {
		timer = (std::max)(0.0f, timer - dt_);
	}

	ApplyUpgradeHudLayout();
	const bool wantsUpgradeList = !upgradeHudHideListWithoutPoints_ || skillPoints_ > 0;
	const float targetListVisibility = wantsUpgradeList ? 1.0f : 0.0f;
	const float listStep = dt_ * upgradeHudListAnimSpeed_;
	if (upgradeHudListVisibility_ < targetListVisibility) {
		upgradeHudListVisibility_ = (std::min)(targetListVisibility, upgradeHudListVisibility_ + listStep);
	} else if (upgradeHudListVisibility_ > targetListVisibility) {
		upgradeHudListVisibility_ = (std::max)(targetListVisibility, upgradeHudListVisibility_ - listStep);
	}
	const bool showUpgradeList = upgradeHudListVisibility_ > 0.01f;
	const float listAlpha = (std::clamp)(upgradeHudListVisibility_, 0.0f, 1.0f);
	const float easedListAlpha = listAlpha * listAlpha * (3.0f - 2.0f * listAlpha);
	const float listOffsetX = -(1.0f - easedListAlpha) * upgradeHudListSlideDistance_;
	if (!showUpgradeList) {
		if (upgradeHudExpBackSprite_) upgradeHudExpBackSprite_->Update();
		if (upgradeHudExpFillSprite_) upgradeHudExpFillSprite_->Update();
		if (upgradeHudLevelBackSprite_) upgradeHudLevelBackSprite_->Update();
		if (upgradeHudLevelFillSprite_) upgradeHudLevelFillSprite_->Update();
		return;
	}

	const bool canUpgrade = skillPoints_ > 0;
	const bool click = input_ && input_->IsTrigger(input_->GetMouseState().rgbButtons[0], input_->GetPreMouseState().rgbButtons[0]);
	for (int i = 0; i < 7; ++i) {
		const float y = upgradeHudRowStart_.y + static_cast<float>(i) * upgradeHudRowGap_;
		Sprite* button = upgradeHudButtonSprites_[i].get();
		Sprite* plus = upgradeHudPlusSprites_[i].get();
		Sprite* minus = upgradeHudMinusSprites_[i].get();
		if (button) {
			button->SetPosition({ upgradeHudRowStart_.x + listOffsetX, y });
		}
		if (minus) {
			minus->SetPosition({ upgradeHudMinusX_ + listOffsetX, y });
		}
		if (plus) {
			plus->SetPosition({ upgradeHudPlusX_ + listOffsetX, y });
		}
		const bool plusHovered = plus && plus->IsHovered(mousePosition_);
		const bool minusHovered = minus && minus->IsHovered(mousePosition_);
		const bool rowHovered = button && button->IsHovered(mousePosition_);
		const bool hovered = rowHovered || plusHovered || minusHovered;
		upgradeHudMouseCaptured_ = upgradeHudMouseCaptured_ || hovered;
		const bool maxed = upgradeLevels_[i] >= maxEnhancePoint;
		if (wantsUpgradeList && click && plusHovered) {
			if (canUpgrade && !maxed && ApplyStatUpgrade(i)) {
				upgradeHudFlashTimers_[i] = 0.22f;
			} else {
				upgradeHudMissFlashTimers_[i] = 0.26f;
			}
		} else if (wantsUpgradeList && click && minusHovered) {
			if (RefundStatUpgrade(i)) {
				upgradeHudRefundFlashTimers_[i] = 0.22f;
			} else {
				upgradeHudMissFlashTimers_[i] = 0.26f;
			}
		}

		const float flash = (std::min)(1.0f, upgradeHudFlashTimers_[i] / 0.22f);
		const float refundFlash = (std::min)(1.0f, upgradeHudRefundFlashTimers_[i] / 0.22f);
		const float missFlash = (std::min)(1.0f, upgradeHudMissFlashTimers_[i] / 0.26f);
		if (button) {
			if (maxed) {
				button->SetColor({ 0.12f, 0.12f, 0.14f, 0.72f });
			} else if (rowHovered) {
				button->SetColor({ 0.18f + flash * 0.18f + missFlash * 0.22f, 0.28f + flash * 0.30f, 0.30f + refundFlash * 0.20f, 0.92f });
			} else {
				button->SetColor({ 0.10f + flash * 0.25f + missFlash * 0.20f, 0.12f + flash * 0.36f, 0.15f + refundFlash * 0.22f, 0.84f });
			}
			button->Update();
		}
		if (minus) {
			if (upgradeLevels_[i] <= 0) {
				minus->SetColor({ 0.12f + missFlash * 0.32f, 0.14f, 0.18f, 0.42f + missFlash * 0.32f });
			} else {
				minus->SetColor(minusHovered
					? Vector4{ 0.48f + refundFlash * 0.18f, 0.66f + refundFlash * 0.20f, 1.0f, 0.96f }
					: Vector4{ 0.26f + refundFlash * 0.25f, 0.42f + refundFlash * 0.22f, 0.86f, 0.70f });
			}
			minus->Update();
		}
		if (plus) {
			if (maxed) {
				plus->SetColor({ 0.18f + missFlash * 0.30f, 0.18f, 0.20f, 0.72f + missFlash * 0.20f });
			} else if (canUpgrade) {
				plus->SetColor(plusHovered ? Vector4{ 0.62f, 1.0f, 0.78f, 0.98f } : Vector4{ 0.34f + flash * 0.35f, 0.95f, 0.64f, 0.88f });
			} else {
				plus->SetColor({ 0.15f + missFlash * 0.34f, 0.22f, 0.24f, 0.64f + missFlash * 0.25f });
			}
			plus->Update();
		}
	}

	if (upgradeHudBackdropSprite_) upgradeHudBackdropSprite_->Update();
	if (upgradeHudExpBackSprite_) upgradeHudExpBackSprite_->Update();
	if (upgradeHudExpFillSprite_) upgradeHudExpFillSprite_->Update();
	if (upgradeHudLevelBackSprite_) upgradeHudLevelBackSprite_->Update();
	if (upgradeHudLevelFillSprite_) upgradeHudLevelFillSprite_->Update();
}

void Player::DrawUpgradeHud()
{
	upgradeHudProfile_ = {};
	if (!upgradeHudVisible_ || isChangeMode || isDead_) {
		return;
	}
	upgradeHudProfile_.visible = true;
	const auto totalStart = std::chrono::steady_clock::now();

	SpriteCommon* spriteCommon = SpriteCommon::GetInstance();
	TextStyle smallStyle{};
	smallStyle.fontFamily = "Meiryo";
	smallStyle.fontSize = 15.0f;
	smallStyle.color = { 0.90f, 0.94f, 1.0f, 1.0f };
	smallStyle.outlineColor = { 0.0f, 0.03f, 0.05f, 0.95f };
	smallStyle.outlineThickness = 2.0f;
	smallStyle.padding = 5.0f;

	const int safeNextExp = (std::max)(1, nextLevelExp_);
	const float expRatio = (std::clamp)(static_cast<float>(exp_) / static_cast<float>(safeNextExp), 0.0f, 1.0f);
	const float levelRatio = (std::clamp)(static_cast<float>(level_ - 1) / static_cast<float>((std::max)(1, kMaxLevel - 1)), 0.0f, 1.0f);
	if (upgradeHudExpFillSprite_) {
		upgradeHudExpFillSprite_->SetSize({ upgradeHudExpBarSize_.x * expRatio, upgradeHudExpBarSize_.y });
		upgradeHudExpFillSprite_->Update();
	}
	if (upgradeHudLevelFillSprite_) {
		upgradeHudLevelFillSprite_->SetSize({ upgradeHudLevelBarSize_.x * levelRatio, upgradeHudLevelBarSize_.y });
		upgradeHudLevelFillSprite_->Update();
	}

	const bool showUpgradeList = upgradeHudListVisibility_ > 0.01f;
	const float listAlpha = (std::clamp)(upgradeHudListVisibility_, 0.0f, 1.0f);
	const float easedListAlpha = listAlpha * listAlpha * (3.0f - 2.0f * listAlpha);
	const float listOffsetX = -(1.0f - easedListAlpha) * upgradeHudListSlideDistance_;
	const auto& names = UpgradeHudNames();
	const std::string className = GetCurrentClassName();
	const bool baseTextDirty =
		cachedUpgradeHudExp_ != exp_ ||
		cachedUpgradeHudNextExp_ != nextLevelExp_ ||
		cachedUpgradeHudLevel_ != level_ ||
		cachedUpgradeHudClassName_ != className;
	if (baseTextDirty) {
		SetLabel(upgradeHudExpLabel_, spriteCommon, "EXP " + std::to_string(exp_) + " / " + std::to_string(nextLevelExp_), upgradeHudExpTextPos_, smallStyle);
		SetLabel(upgradeHudLevelLabel_, spriteCommon, "Lv " + std::to_string(level_) + " " + className, upgradeHudLevelTextPos_, smallStyle);
		cachedUpgradeHudExp_ = exp_;
		cachedUpgradeHudNextExp_ = nextLevelExp_;
		cachedUpgradeHudLevel_ = level_;
		cachedUpgradeHudClassName_ = className;
	} else {
		if (upgradeHudExpLabel_) upgradeHudExpLabel_->SetPosition(upgradeHudExpTextPos_);
		if (upgradeHudLevelLabel_) upgradeHudLevelLabel_->SetPosition(upgradeHudLevelTextPos_);
	}

	if (showUpgradeList) {
		const bool listDirty =
			!cachedUpgradeHudListVisible_ ||
			cachedUpgradeHudSkillPoints_ != skillPoints_ ||
			cachedUpgradeHudLevels_ != upgradeLevels_;
		if (listDirty) {
			SetLabel(upgradeHudPointLabel_, spriteCommon, "x" + std::to_string(skillPoints_), { upgradeHudPointPos_.x + listOffsetX, upgradeHudPointPos_.y }, smallStyle);
			SetLabel(upgradeHudTitleLabel_, spriteCommon, "強化", { upgradeHudTitlePos_.x + listOffsetX, upgradeHudTitlePos_.y }, smallStyle);
			for (int i = 0; i < 7; ++i) {
				const float y = upgradeHudRowStart_.y + static_cast<float>(i) * upgradeHudRowGap_;
				SetLabel(upgradeHudNameLabels_[i], spriteCommon, std::to_string(i + 1) + " " + names[i],
					{ upgradeHudNameX_ + listOffsetX, y + upgradeHudNameTextOffsetY_ }, smallStyle);
				SetLabel(upgradeHudLevelLabels_[i], spriteCommon, "Lv." + std::to_string(upgradeLevels_[i]),
					{ upgradeHudLevelX_ + listOffsetX, y + upgradeHudLevelTextOffsetY_ }, smallStyle);
				SetLabel(upgradeHudMinusLabels_[i], spriteCommon, "-",
					{ upgradeHudMinusLabelX_ + listOffsetX, y + upgradeHudMinusTextOffsetY_ }, smallStyle);
				SetLabel(upgradeHudPlusLabels_[i], spriteCommon, upgradeLevels_[i] >= maxEnhancePoint ? "済" : "+",
					{ upgradeHudPlusLabelX_ + listOffsetX, y + upgradeHudPlusTextOffsetY_ }, smallStyle);
			}
			cachedUpgradeHudSkillPoints_ = skillPoints_;
			cachedUpgradeHudLevels_ = upgradeLevels_;
		} else {
			if (upgradeHudPointLabel_) upgradeHudPointLabel_->SetPosition({ upgradeHudPointPos_.x + listOffsetX, upgradeHudPointPos_.y });
			if (upgradeHudTitleLabel_) upgradeHudTitleLabel_->SetPosition({ upgradeHudTitlePos_.x + listOffsetX, upgradeHudTitlePos_.y });
			for (int i = 0; i < 7; ++i) {
				const float y = upgradeHudRowStart_.y + static_cast<float>(i) * upgradeHudRowGap_;
				if (upgradeHudNameLabels_[i]) upgradeHudNameLabels_[i]->SetPosition({ upgradeHudNameX_ + listOffsetX, y + upgradeHudNameTextOffsetY_ });
				if (upgradeHudLevelLabels_[i]) upgradeHudLevelLabels_[i]->SetPosition({ upgradeHudLevelX_ + listOffsetX, y + upgradeHudLevelTextOffsetY_ });
				if (upgradeHudMinusLabels_[i]) upgradeHudMinusLabels_[i]->SetPosition({ upgradeHudMinusLabelX_ + listOffsetX, y + upgradeHudMinusTextOffsetY_ });
				if (upgradeHudPlusLabels_[i]) upgradeHudPlusLabels_[i]->SetPosition({ upgradeHudPlusLabelX_ + listOffsetX, y + upgradeHudPlusTextOffsetY_ });
			}
		}
		if (upgradeHudPointLabel_) upgradeHudPointLabel_->SetAlpha(easedListAlpha);
		if (upgradeHudTitleLabel_) upgradeHudTitleLabel_->SetAlpha(easedListAlpha);
		for (int i = 0; i < 7; ++i) {
			if (upgradeHudNameLabels_[i]) upgradeHudNameLabels_[i]->SetAlpha(easedListAlpha);
			if (upgradeHudLevelLabels_[i]) upgradeHudLevelLabels_[i]->SetAlpha(easedListAlpha);
			if (upgradeHudMinusLabels_[i]) upgradeHudMinusLabels_[i]->SetAlpha(easedListAlpha);
			if (upgradeHudPlusLabels_[i]) upgradeHudPlusLabels_[i]->SetAlpha(easedListAlpha);
		}
	}
	cachedUpgradeHudListVisible_ = showUpgradeList;

	const auto spriteStart = std::chrono::steady_clock::now();
	if (upgradeHudUseRectBatch_) {
		DrawUpgradeHudRectBatch(showUpgradeList, expRatio, levelRatio, easedListAlpha, listOffsetX);
	} else {
		SpriteCommon::GetInstance()->PreDraw(kNormal);
		if (showUpgradeList && upgradeHudDrawListPanels_) {
			if (upgradeHudBackdropSprite_) { upgradeHudBackdropSprite_->Draw(); ++upgradeHudProfile_.spriteDraws; }
			for (int i = 0; i < 7; ++i) {
			if (upgradeHudButtonSprites_[i]) { upgradeHudButtonSprites_[i]->Draw(); ++upgradeHudProfile_.spriteDraws; }
				if (upgradeHudMinusSprites_[i]) { upgradeHudMinusSprites_[i]->Draw(); ++upgradeHudProfile_.spriteDraws; }
				if (upgradeHudPlusSprites_[i]) { upgradeHudPlusSprites_[i]->Draw(); ++upgradeHudProfile_.spriteDraws; }
			}
		}
		if (upgradeHudDrawBottomBars_) {
			if (upgradeHudLevelBackSprite_) { upgradeHudLevelBackSprite_->Draw(); ++upgradeHudProfile_.spriteDraws; }
			if (upgradeHudLevelFillSprite_) { upgradeHudLevelFillSprite_->Draw(); ++upgradeHudProfile_.spriteDraws; }
			if (upgradeHudExpBackSprite_) { upgradeHudExpBackSprite_->Draw(); ++upgradeHudProfile_.spriteDraws; }
			if (upgradeHudExpFillSprite_) { upgradeHudExpFillSprite_->Draw(); ++upgradeHudProfile_.spriteDraws; }
		}
	}
	const auto spriteEnd = std::chrono::steady_clock::now();

	const auto textStart = std::chrono::steady_clock::now();
	SpriteCommon::GetInstance()->PreDraw(kNormal);
	if (showUpgradeList && upgradeHudDrawListText_) {
		if (upgradeHudTitleLabel_) { upgradeHudTitleLabel_->Draw(); ++upgradeHudProfile_.textDraws; }
		if (upgradeHudPointLabel_) { upgradeHudPointLabel_->Draw(); ++upgradeHudProfile_.textDraws; }
		for (int i = 0; i < 7; ++i) {
			if (upgradeHudNameLabels_[i]) { upgradeHudNameLabels_[i]->Draw(); ++upgradeHudProfile_.textDraws; }
			if (upgradeHudLevelLabels_[i]) { upgradeHudLevelLabels_[i]->Draw(); ++upgradeHudProfile_.textDraws; }
			if (upgradeHudMinusLabels_[i]) { upgradeHudMinusLabels_[i]->Draw(); ++upgradeHudProfile_.textDraws; }
			if (upgradeHudPlusLabels_[i]) { upgradeHudPlusLabels_[i]->Draw(); ++upgradeHudProfile_.textDraws; }
		}
	}
	if (upgradeHudDrawBottomText_) {
		if (upgradeHudLevelLabel_) { upgradeHudLevelLabel_->Draw(); ++upgradeHudProfile_.textDraws; }
		if (upgradeHudExpLabel_) { upgradeHudExpLabel_->Draw(); ++upgradeHudProfile_.textDraws; }
	}
	const auto textEnd = std::chrono::steady_clock::now();
	const auto totalEnd = std::chrono::steady_clock::now();

	upgradeHudProfile_.spriteMs = std::chrono::duration<float, std::milli>(spriteEnd - spriteStart).count();
	upgradeHudProfile_.textMs = std::chrono::duration<float, std::milli>(textEnd - textStart).count();
	upgradeHudProfile_.updateMs = std::chrono::duration<float, std::milli>(spriteStart - totalStart).count();
	upgradeHudProfile_.totalMs = std::chrono::duration<float, std::milli>(totalEnd - totalStart).count();
}

void Player::QueueUpgradeHudRect(std::vector<TrailVertex>& vertices, const Vector2& pos, const Vector2& size, const Vector4& color) const
{
	const float left = pos.x;
	const float top = pos.y;
	const float right = pos.x + size.x;
	const float bottom = pos.y + size.y;
	const Vector3 p0{ left, bottom, 0.0f };
	const Vector3 p1{ left, top, 0.0f };
	const Vector3 p2{ right, bottom, 0.0f };
	const Vector3 p3{ right, top, 0.0f };

	vertices.push_back({ p0, color, { 0.0f, 1.0f } });
	vertices.push_back({ p1, color, { 0.0f, 0.0f } });
	vertices.push_back({ p2, color, { 1.0f, 1.0f } });
	vertices.push_back({ p1, color, { 0.0f, 0.0f } });
	vertices.push_back({ p3, color, { 1.0f, 0.0f } });
	vertices.push_back({ p2, color, { 1.0f, 1.0f } });
}

void Player::DrawUpgradeHudRectBatch(bool showUpgradeList, float expRatio, float levelRatio, float listAlpha, float listOffsetX)
{
	if (!upgradeHudBatchVertexData_ || !upgradeHudBatchTransformData_ || !upgradeHudBatchMaterialData_) {
		return;
	}

	std::vector<TrailVertex> vertices;
	vertices.reserve(kUpgradeHudBatchMaxVertices);
	auto withListAlpha = [listAlpha](Vector4 color) {
		color.w *= listAlpha;
		return color;
	};
	auto offsetListPos = [listOffsetX](Vector2 pos) {
		pos.x += listOffsetX;
		return pos;
	};

	if (showUpgradeList && upgradeHudDrawListPanels_) {
		QueueUpgradeHudRect(vertices, offsetListPos(upgradeHudPanelPos_), upgradeHudPanelSize_, withListAlpha({ 0.03f, 0.04f, 0.06f, 0.58f }));
		for (int i = 0; i < 7; ++i) {
			const float y = upgradeHudRowStart_.y + static_cast<float>(i) * upgradeHudRowGap_;
			float flash = (std::min)(1.0f, upgradeHudFlashTimers_[i] / 0.22f);
			const float refundFlash = (std::min)(1.0f, upgradeHudRefundFlashTimers_[i] / 0.22f);
			const float missFlash = (std::min)(1.0f, upgradeHudMissFlashTimers_[i] / 0.26f);
			const Vector4 buttonColor = LerpColor({ 0.10f + missFlash * 0.20f, 0.12f, 0.15f + refundFlash * 0.16f, 0.84f }, { 0.35f, 0.90f, 0.72f, 0.96f }, flash);
			const Vector4 minusColor = upgradeLevels_[i] > 0
				? LerpColor({ 0.26f, 0.42f, 0.86f, 0.70f }, { 0.70f, 0.86f, 1.0f, 0.98f }, refundFlash)
				: Vector4{ 0.12f + missFlash * 0.30f, 0.14f, 0.18f, 0.42f + missFlash * 0.28f };
			const Vector4 plusColor = skillPoints_ > 0
				? LerpColor({ 0.34f, 0.95f, 0.64f, 0.88f }, { 1.0f, 1.0f, 0.46f, 1.0f }, flash)
				: Vector4{ 0.18f + missFlash * 0.32f, 0.22f, 0.24f, 0.48f + missFlash * 0.28f };
			QueueUpgradeHudRect(vertices, offsetListPos({ upgradeHudRowStart_.x, y }), upgradeHudButtonSize_, withListAlpha(buttonColor));
			QueueUpgradeHudRect(vertices, offsetListPos({ upgradeHudMinusX_, y }), upgradeHudPlusSize_, withListAlpha(minusColor));
			QueueUpgradeHudRect(vertices, offsetListPos({ upgradeHudPlusX_, y }), upgradeHudPlusSize_, withListAlpha(plusColor));
		}
	}

	if (upgradeHudDrawBottomBars_) {
		QueueUpgradeHudRect(vertices, upgradeHudLevelBarPos_, upgradeHudLevelBarSize_, { 0.04f, 0.04f, 0.05f, 0.82f });
		QueueUpgradeHudRect(vertices, upgradeHudLevelBarPos_, { upgradeHudLevelBarSize_.x * levelRatio, upgradeHudLevelBarSize_.y }, { 0.36f, 1.0f, 0.56f, 0.92f });
		QueueUpgradeHudRect(vertices, upgradeHudExpBarPos_, upgradeHudExpBarSize_, { 0.04f, 0.04f, 0.05f, 0.82f });
		QueueUpgradeHudRect(vertices, upgradeHudExpBarPos_, { upgradeHudExpBarSize_.x * expRatio, upgradeHudExpBarSize_.y }, { 0.96f, 0.83f, 0.24f, 0.95f });
	}

	if (vertices.empty()) {
		return;
	}
	if (vertices.size() > kUpgradeHudBatchMaxVertices) {
		vertices.resize(kUpgradeHudBatchMaxVertices);
	}
	std::memcpy(upgradeHudBatchVertexData_, vertices.data(), sizeof(TrailVertex) * vertices.size());
	*upgradeHudBatchTransformData_ = MakeOrthographicMatrix(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 100.0f);

	DirectXCommon* dxCommon = SpriteCommon::GetInstance()->GetDxCommon();
	ID3D12GraphicsCommandList* commandList = dxCommon->GetList().Get();
	TextureManager::GetInstance()->PreDraw();
	commandList->SetGraphicsRootSignature(dxCommon->GetPSOHudRect().root_.GetSignature().Get());
	commandList->SetPipelineState(dxCommon->GetPSOHudRect().graphicsState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &upgradeHudBatchVertexBufferView_);
	commandList->SetGraphicsRootConstantBufferView(0, upgradeHudBatchMaterialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, upgradeHudBatchTransformResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU("resources/white512x512.png"));
	commandList->DrawInstanced(static_cast<UINT>(vertices.size()), 1, 0, 0);
	upgradeHudProfile_.spriteDraws += 1;
}

void Player::ApplyUpgradeHudLayout()
{
	if (upgradeHudBackdropSprite_) {
		upgradeHudBackdropSprite_->SetPosition(upgradeHudPanelPos_);
		upgradeHudBackdropSprite_->SetSize(upgradeHudPanelSize_);
	}
	if (upgradeHudExpBackSprite_) {
		upgradeHudExpBackSprite_->SetPosition(upgradeHudExpBarPos_);
		upgradeHudExpBackSprite_->SetSize(upgradeHudExpBarSize_);
	}
	if (upgradeHudExpFillSprite_) {
		upgradeHudExpFillSprite_->SetPosition(upgradeHudExpBarPos_);
	}
	if (upgradeHudLevelBackSprite_) {
		upgradeHudLevelBackSprite_->SetPosition(upgradeHudLevelBarPos_);
		upgradeHudLevelBackSprite_->SetSize(upgradeHudLevelBarSize_);
	}
	if (upgradeHudLevelFillSprite_) {
		upgradeHudLevelFillSprite_->SetPosition(upgradeHudLevelBarPos_);
	}
	for (int i = 0; i < 7; ++i) {
		const float y = upgradeHudRowStart_.y + static_cast<float>(i) * upgradeHudRowGap_;
		if (upgradeHudButtonSprites_[i]) {
			upgradeHudButtonSprites_[i]->SetPosition({ upgradeHudRowStart_.x, y });
			upgradeHudButtonSprites_[i]->SetSize(upgradeHudButtonSize_);
		}
		if (upgradeHudMinusSprites_[i]) {
			upgradeHudMinusSprites_[i]->SetPosition({ upgradeHudMinusX_, y });
			upgradeHudMinusSprites_[i]->SetSize(upgradeHudPlusSize_);
		}
		if (upgradeHudPlusSprites_[i]) {
			upgradeHudPlusSprites_[i]->SetPosition({ upgradeHudPlusX_, y });
			upgradeHudPlusSprites_[i]->SetSize(upgradeHudPlusSize_);
		}
	}
}

bool Player::LoadUpgradeHudConfig(const std::string& path)
{
	std::ifstream file(path);
	if (!file.is_open()) {
		upgradeHudConfigStatus_ = "HUD設定ファイルが見つからないため初期値を使用します。";
		return false;
	}
	nlohmann::json json{};
	try {
		file >> json;
	} catch (...) {
		upgradeHudConfigStatus_ = "HUD設定JSONの読み込みに失敗しました。";
		return false;
	}
	if (!json.is_object()) {
		return false;
	}
	upgradeHudVisible_ = json.value("visible", upgradeHudVisible_);
	upgradeHudHideListWithoutPoints_ = json.value("hideListWithoutPoints", upgradeHudHideListWithoutPoints_);
	upgradeHudDrawListPanels_ = json.value("drawListPanels", upgradeHudDrawListPanels_);
	upgradeHudDrawListText_ = json.value("drawListText", upgradeHudDrawListText_);
	upgradeHudDrawBottomBars_ = json.value("drawBottomBars", upgradeHudDrawBottomBars_);
	upgradeHudDrawBottomText_ = json.value("drawBottomText", upgradeHudDrawBottomText_);
	upgradeHudUseRectBatch_ = json.value("useRectBatch", upgradeHudUseRectBatch_);
	upgradeHudListAnimSpeed_ = json.value("listAnimSpeed", upgradeHudListAnimSpeed_);
	upgradeHudListSlideDistance_ = json.value("listSlideDistance", upgradeHudListSlideDistance_);
	upgradeHudPanelPos_ = ReadVector2Object(json.value("panelPos", nlohmann::json::object()), upgradeHudPanelPos_);
	upgradeHudPanelSize_ = ReadVector2Object(json.value("panelSize", nlohmann::json::object()), upgradeHudPanelSize_);
	upgradeHudRowStart_ = ReadVector2Object(json.value("rowStart", nlohmann::json::object()), upgradeHudRowStart_);
	upgradeHudButtonSize_ = ReadVector2Object(json.value("buttonSize", nlohmann::json::object()), upgradeHudButtonSize_);
	upgradeHudPlusSize_ = ReadVector2Object(json.value("plusSize", nlohmann::json::object()), upgradeHudPlusSize_);
	upgradeHudRowGap_ = json.value("rowGap", upgradeHudRowGap_);
	upgradeHudNameX_ = json.value("nameX", upgradeHudNameX_);
	upgradeHudLevelX_ = json.value("levelX", upgradeHudLevelX_);
	upgradeHudMinusX_ = json.value("minusX", upgradeHudMinusX_);
	upgradeHudPlusX_ = json.value("plusX", upgradeHudPlusX_);
	upgradeHudMinusLabelX_ = json.value("minusLabelX", upgradeHudMinusLabelX_);
	upgradeHudPlusLabelX_ = json.value("plusLabelX", upgradeHudPlusLabelX_);
	upgradeHudNameTextOffsetY_ = json.value("nameTextOffsetY", upgradeHudNameTextOffsetY_);
	upgradeHudLevelTextOffsetY_ = json.value("levelTextOffsetY", upgradeHudLevelTextOffsetY_);
	upgradeHudMinusTextOffsetY_ = json.value("minusTextOffsetY", upgradeHudMinusTextOffsetY_);
	upgradeHudPlusTextOffsetY_ = json.value("plusTextOffsetY", upgradeHudPlusTextOffsetY_);
	upgradeHudTitlePos_ = ReadVector2Object(json.value("titlePos", nlohmann::json::object()), upgradeHudTitlePos_);
	upgradeHudPointPos_ = ReadVector2Object(json.value("pointPos", nlohmann::json::object()), upgradeHudPointPos_);
	upgradeHudLevelBarPos_ = ReadVector2Object(json.value("levelBarPos", nlohmann::json::object()), upgradeHudLevelBarPos_);
	upgradeHudLevelBarSize_ = ReadVector2Object(json.value("levelBarSize", nlohmann::json::object()), upgradeHudLevelBarSize_);
	upgradeHudLevelTextPos_ = ReadVector2Object(json.value("levelTextPos", nlohmann::json::object()), upgradeHudLevelTextPos_);
	upgradeHudExpBarPos_ = ReadVector2Object(json.value("expBarPos", nlohmann::json::object()), upgradeHudExpBarPos_);
	upgradeHudExpBarSize_ = ReadVector2Object(json.value("expBarSize", nlohmann::json::object()), upgradeHudExpBarSize_);
	upgradeHudExpTextPos_ = ReadVector2Object(json.value("expTextPos", nlohmann::json::object()), upgradeHudExpTextPos_);
	ApplyUpgradeHudLayout();
	upgradeHudConfigStatus_ = "HUD設定を読み込みました: " + path;
	return true;
}

bool Player::SaveUpgradeHudConfig(const std::string& path) const
{
	std::filesystem::create_directories(std::filesystem::path(path).parent_path());
	std::ofstream file(path);
	if (!file.is_open()) {
		return false;
	}
	nlohmann::json json = {
		{ "version", 1 },
		{ "visible", upgradeHudVisible_ },
		{ "hideListWithoutPoints", upgradeHudHideListWithoutPoints_ },
		{ "drawListPanels", upgradeHudDrawListPanels_ },
		{ "drawListText", upgradeHudDrawListText_ },
		{ "drawBottomBars", upgradeHudDrawBottomBars_ },
		{ "drawBottomText", upgradeHudDrawBottomText_ },
		{ "useRectBatch", upgradeHudUseRectBatch_ },
		{ "listAnimSpeed", upgradeHudListAnimSpeed_ },
		{ "listSlideDistance", upgradeHudListSlideDistance_ },
		{ "panelPos", WriteVector2Object(upgradeHudPanelPos_) },
		{ "panelSize", WriteVector2Object(upgradeHudPanelSize_) },
		{ "rowStart", WriteVector2Object(upgradeHudRowStart_) },
		{ "buttonSize", WriteVector2Object(upgradeHudButtonSize_) },
		{ "plusSize", WriteVector2Object(upgradeHudPlusSize_) },
		{ "rowGap", upgradeHudRowGap_ },
		{ "nameX", upgradeHudNameX_ },
		{ "levelX", upgradeHudLevelX_ },
		{ "minusX", upgradeHudMinusX_ },
		{ "plusX", upgradeHudPlusX_ },
		{ "minusLabelX", upgradeHudMinusLabelX_ },
		{ "plusLabelX", upgradeHudPlusLabelX_ },
		{ "nameTextOffsetY", upgradeHudNameTextOffsetY_ },
		{ "levelTextOffsetY", upgradeHudLevelTextOffsetY_ },
		{ "minusTextOffsetY", upgradeHudMinusTextOffsetY_ },
		{ "plusTextOffsetY", upgradeHudPlusTextOffsetY_ },
		{ "titlePos", WriteVector2Object(upgradeHudTitlePos_) },
		{ "pointPos", WriteVector2Object(upgradeHudPointPos_) },
		{ "levelBarPos", WriteVector2Object(upgradeHudLevelBarPos_) },
		{ "levelBarSize", WriteVector2Object(upgradeHudLevelBarSize_) },
		{ "levelTextPos", WriteVector2Object(upgradeHudLevelTextPos_) },
		{ "expBarPos", WriteVector2Object(upgradeHudExpBarPos_) },
		{ "expBarSize", WriteVector2Object(upgradeHudExpBarSize_) },
		{ "expTextPos", WriteVector2Object(upgradeHudExpTextPos_) }
	};
	file << json.dump(2);
	return true;
}

void Player::DrawUpgradeHudDebugImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("プレイヤー強化HUD", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}
	ImGui::Checkbox("HUDを表示", &upgradeHudVisible_);
	ImGui::Checkbox("スキルポイントがない時は強化リストを隠す", &upgradeHudHideListWithoutPoints_);
	ImGui::Checkbox("強化リスト背景/ボタンを描画", &upgradeHudDrawListPanels_);
	ImGui::Checkbox("強化リスト文字を描画", &upgradeHudDrawListText_);
	ImGui::Checkbox("下部EXP/Levelバーを描画", &upgradeHudDrawBottomBars_);
	ImGui::Checkbox("下部EXP/Level文字を描画", &upgradeHudDrawBottomText_);
	ImGui::Checkbox("背景/バーを矩形バッチで描画", &upgradeHudUseRectBatch_);
	ImGui::Text("矩形Draw数: %d", upgradeHudProfile_.spriteDraws);
	ImGui::DragFloat("リスト表示アニメ速度", &upgradeHudListAnimSpeed_, 0.1f, 1.0f, 30.0f);
	ImGui::DragFloat("リストスライド距離", &upgradeHudListSlideDistance_, 1.0f, 0.0f, 500.0f);
	if (ImGui::Button("強化HUD設定を保存")) {
		upgradeHudConfigStatus_ = SaveUpgradeHudConfig() ? "強化HUD設定を保存しました。" : "強化HUD設定の保存に失敗しました。";
	}
	ImGui::SameLine();
	if (ImGui::Button("強化HUD設定を再読み込み")) {
		LoadUpgradeHudConfig();
	}
	if (!upgradeHudConfigStatus_.empty()) {
		ImGui::TextWrapped("%s", upgradeHudConfigStatus_.c_str());
	}
	ImGui::DragFloat2("左パネル位置", &upgradeHudPanelPos_.x, 1.0f);
	ImGui::DragFloat2("左パネルサイズ", &upgradeHudPanelSize_.x, 1.0f, 0.0f, 2000.0f);
	ImGui::DragFloat2("強化行 開始位置", &upgradeHudRowStart_.x, 1.0f);
	ImGui::DragFloat2("強化ボタンサイズ", &upgradeHudButtonSize_.x, 1.0f, 0.0f, 1000.0f);
	ImGui::DragFloat2("プラスボタンサイズ", &upgradeHudPlusSize_.x, 1.0f, 0.0f, 300.0f);
	ImGui::DragFloat("強化行 間隔", &upgradeHudRowGap_, 1.0f, 10.0f, 100.0f);
	ImGui::DragFloat("項目名X", &upgradeHudNameX_, 1.0f);
	ImGui::DragFloat("Lv表示X", &upgradeHudLevelX_, 1.0f);
	ImGui::DragFloat("マイナスボタンX", &upgradeHudMinusX_, 1.0f);
	ImGui::DragFloat("プラスボタンX", &upgradeHudPlusX_, 1.0f);
	ImGui::DragFloat("マイナス文字X", &upgradeHudMinusLabelX_, 1.0f);
	ImGui::DragFloat("プラス文字X", &upgradeHudPlusLabelX_, 1.0f);
	ImGui::DragFloat("項目名文字Yオフセット", &upgradeHudNameTextOffsetY_, 0.25f, -20.0f, 40.0f);
	ImGui::DragFloat("Lv文字Yオフセット", &upgradeHudLevelTextOffsetY_, 0.25f, -20.0f, 40.0f);
	ImGui::DragFloat("マイナス文字Yオフセット", &upgradeHudMinusTextOffsetY_, 0.25f, -20.0f, 40.0f);
	ImGui::DragFloat("プラス文字Yオフセット", &upgradeHudPlusTextOffsetY_, 0.25f, -20.0f, 40.0f);
	ImGui::DragFloat2("タイトル位置", &upgradeHudTitlePos_.x, 1.0f);
	ImGui::DragFloat2("ポイント表示位置", &upgradeHudPointPos_.x, 1.0f);
	ImGui::Separator();
	ImGui::DragFloat2("レベルバー位置", &upgradeHudLevelBarPos_.x, 1.0f);
	ImGui::DragFloat2("レベルバーサイズ", &upgradeHudLevelBarSize_.x, 1.0f, 0.0f, 2000.0f);
	ImGui::DragFloat2("レベル文字位置", &upgradeHudLevelTextPos_.x, 1.0f);
	ImGui::DragFloat2("経験値バー位置", &upgradeHudExpBarPos_.x, 1.0f);
	ImGui::DragFloat2("経験値バーサイズ", &upgradeHudExpBarSize_.x, 1.0f, 0.0f, 2000.0f);
	ImGui::DragFloat2("経験値文字位置", &upgradeHudExpTextPos_.x, 1.0f);
	ApplyUpgradeHudLayout();
#endif
}

void Player::SpawnParticles()
{
	Vector3 center = GetWorldPosition();
	ParticleManager::GetInstance()->EmitNeonDeathEffect(
		center,
		{ 1.45f, 1.30f, 0.72f, 1.0f },
		{ 0.08f, 1.25f, 1.55f, 0.0f },
		1.0f);
	deathChargeTimer_ = deathChargeDuration_;
	deathEffectTimer_ = 1.85f;
}

void Player::UpdateParticles(float deltaTime)
{
	deathChargeTimer_ = (std::max)(0.0f, deathChargeTimer_ - deltaTime);
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
	if (!isChangeMode) {
		return;
	}
	if (encyclopedia_.size() != classOrder_.size()) {
		InitializeEncyclopedia();
	}
	if (encyclopedia_.empty()) {
		return;
	}
	evolutionUiTimer_ += dt_;
	int currentRank = GetRankFromLevel(this->level_);

	for (int i = 0; i < static_cast<int>(encyclopedia_.size()); ++i) {
		auto& tank = encyclopedia_[i];
		bool isAvailable = (currentRank >= tank.requiredRank);
		const bool selected = (i == evolutionSelectedIndex_);
		const bool hovered = tank.cardSprite && tank.cardSprite->IsHovered(mousePosition_);

		if (hovered && input_->IsTrigger(input_->GetMouseState().rgbButtons[0], input_->GetPreMouseState().rgbButtons[0])) {
			evolutionSelectedIndex_ = i;
			codexSelectedClassIndex_ = i;
		}

		if (tank.cardSprite) {
			if (selected) {
				tank.cardSprite->SetColor({ 0.18f, 0.48f, 0.42f, 0.94f });
			} else if (hovered) {
				tank.cardSprite->SetColor({ 0.16f, 0.30f, 0.38f, 0.92f });
			} else if (isAvailable) {
				tank.cardSprite->SetColor({ 0.10f, 0.15f, 0.22f, 0.82f });
			} else {
				tank.cardSprite->SetColor({ 0.05f, 0.05f, 0.07f, 0.72f });
			}
			tank.cardSprite->Update();
		}

		if (isAvailable) {
			tank.sprite->SetColor({ 1.0f, 1.0f, 1.0f, selected ? 1.0f : 0.86f });
		} else {
			tank.sprite->SetColor({ 0.20f, 0.24f, 0.28f, 0.45f });
		}

		tank.sprite->Update();
	}

	evolutionSelectedIndex_ = (std::clamp)(evolutionSelectedIndex_, 0, static_cast<int>(encyclopedia_.size()) - 1);
	const TankData& selectedTank = encyclopedia_[evolutionSelectedIndex_];
	const PlayerClassConfig* selectedConfig = GetClassConfig(selectedTank.classId);
	if (!selectedConfig) {
		return;
	}

	const bool locked = currentRank < selectedTank.requiredRank;
	if (evolutionPreviewTankSprite_) {
		evolutionPreviewTankSprite_->SetTexture(selectedTank.texturePath);
		const float bob = std::sin(evolutionUiTimer_ * 2.0f) * 12.0f;
		evolutionPreviewTankSprite_->SetPosition({ 210.0f + bob, 300.0f });
		evolutionPreviewTankSprite_->SetRotation(std::sin(evolutionUiTimer_ * 1.35f) * 0.08f);
		evolutionPreviewTankSprite_->SetColor(locked ? Vector4{ 0.35f, 0.40f, 0.45f, 0.65f } : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
		evolutionPreviewTankSprite_->Update();
	}

	if (evolutionShotSprite_) {
		const float shotPhase = std::fmod(evolutionUiTimer_ * 1.8f, 1.0f);
		evolutionShotSprite_->SetPosition({ 300.0f + shotPhase * 52.0f, 296.0f });
		evolutionShotSprite_->SetRotation(std::sin(evolutionUiTimer_ * 1.2f) * 0.12f);
		evolutionShotSprite_->SetSize({ 85.0f + shotPhase * 95.0f, 7.0f });
		evolutionShotSprite_->SetColor(locked ? Vector4{ 0.55f, 0.55f, 0.60f, 0.18f } : Vector4{ 1.0f, 0.90f, 0.32f, 0.82f * (1.0f - shotPhase * 0.55f) });
		evolutionShotSprite_->Update();
	}

	if (evolutionChangeButtonSprite_) {
		const bool buttonHovered = evolutionChangeButtonSprite_->IsHovered(mousePosition_);
		if (locked) {
			evolutionChangeButtonSprite_->SetColor({ 0.16f, 0.16f, 0.18f, 0.82f });
		} else if (buttonHovered) {
			evolutionChangeButtonSprite_->SetColor({ 0.36f, 1.0f, 0.58f, 0.96f });
			if (input_->IsTrigger(input_->GetMouseState().rgbButtons[0], input_->GetPreMouseState().rgbButtons[0])) {
				EvolveById(selectedTank.classId);
			}
		} else {
			evolutionChangeButtonSprite_->SetColor({ 0.24f, 0.86f, 0.44f, 0.92f });
		}
		evolutionChangeButtonSprite_->Update();
	}
}

void Player::DrawEncyclopedia() {

	evolutionUiProfile_ = {};
	if (isChangeMode) {
		evolutionUiProfile_.visible = true;
		const auto totalStart = std::chrono::steady_clock::now();
		if (encyclopedia_.empty()) {
			return;
		}
		evolutionSelectedIndex_ = (std::clamp)(evolutionSelectedIndex_, 0, static_cast<int>(encyclopedia_.size()) - 1);
		const TankData& selectedTank = encyclopedia_[evolutionSelectedIndex_];
		const PlayerClassConfig* selectedConfig = GetClassConfig(selectedTank.classId);
		if (!selectedConfig) {
			return;
		}

		auto roleText = [](const PlayerClassConfig& config) {
			if (config.usesDrone) {
				return std::string("ドローンで周囲を制圧する支援型タンク。");
			}
			if (config.reflect) {
				return std::string("反射弾で壁越しにも圧をかける技巧型タンク。");
			}
			if (config.bulletSpeedScale > 1.2f) {
				return std::string("高速弾で遠距離から狙う狙撃型タンク。");
			}
			if (config.reloadScale < 0.75f) {
				return std::string("連射力で押し切る近中距離向けタンク。");
			}
			if (config.bulletCount > 1 || config.barrels.size() >= 3) {
				return std::string("複数の砲身で広い範囲を抑える制圧型タンク。");
			}
			return std::string("扱いやすい基本性能を持つバランス型タンク。");
		};

		TextStyle headingStyle{};
		headingStyle.fontFamily = "Meiryo";
		headingStyle.fontSize = 28.0f;
		headingStyle.color = { 0.82f, 1.0f, 0.92f, 1.0f };
		headingStyle.outlineColor = { 0.0f, 0.05f, 0.08f, 0.95f };
		headingStyle.outlineThickness = 3.0f;
		headingStyle.padding = 8.0f;

		TextStyle bodyStyle = headingStyle;
		bodyStyle.fontSize = 21.0f;
		bodyStyle.color = { 0.92f, 0.96f, 1.0f, 1.0f };
		bodyStyle.outlineThickness = 2.0f;

		TextStyle smallStyle = bodyStyle;
		smallStyle.fontSize = 18.0f;

		SpriteCommon* spriteCommon = SpriteCommon::GetInstance();
		SetLabel(evolutionPreviewNameLabel_, spriteCommon, selectedConfig->displayName, { 58.0f, 112.0f }, headingStyle);
		SetLabel(evolutionRoleLabel_, spriteCommon, roleText(*selectedConfig), { 58.0f, 560.0f }, bodyStyle);

		const int currentRank = GetRankFromLevel(level_);
		const bool locked = currentRank < selectedConfig->requiredRank;
		std::array<std::string, 9> statLines = {
			"必要ランク: " + std::to_string(selectedConfig->requiredRank) + (locked ? "  (未解放)" : "  (使用可能)"),
			"砲塔数: " + std::to_string(selectedConfig->barrels.size()),
			"発射方式: " + std::string(selectedConfig->fireAllBarrels ? "全砲門" : (selectedConfig->alternateBarrels ? "交互発射" : "単発")),
			"弾数: " + std::to_string(selectedConfig->bulletCount),
			"リロード倍率: " + std::to_string(selectedConfig->reloadScale).substr(0, 4),
			"弾速倍率: " + std::to_string(selectedConfig->bulletSpeedScale).substr(0, 4),
			"ダメージ倍率: " + std::to_string(selectedConfig->bulletDamageScale).substr(0, 4),
			"拡散角: " + std::to_string(selectedConfig->spreadAngleDeg).substr(0, 4),
			std::string("特殊: ") + (selectedConfig->usesDrone ? "ドローン" : selectedConfig->reflect ? "反射" : selectedConfig->penetrate ? "貫通" : "なし")
		};
		for (int i = 0; i < static_cast<int>(statLines.size()); ++i) {
			SetLabel(evolutionStatLabels_[i], spriteCommon, statLines[i], { 932.0f, 136.0f + static_cast<float>(i) * 40.0f }, smallStyle);
		}

		TextStyle buttonStyle = headingStyle;
		buttonStyle.fontSize = 24.0f;
		buttonStyle.color = locked ? Vector4{ 0.68f, 0.68f, 0.72f, 1.0f } : Vector4{ 0.02f, 0.09f, 0.04f, 1.0f };
		buttonStyle.outlineColor = locked ? Vector4{ 0.0f, 0.0f, 0.0f, 0.70f } : Vector4{ 0.78f, 1.0f, 0.82f, 0.65f };
		SetLabel(evolutionChangeButtonLabel_, spriteCommon, locked ? "ランク不足" : "この戦車に変更", { 990.0f, 660.0f }, buttonStyle);

		const auto spriteStart = std::chrono::steady_clock::now();
		SpriteCommon::GetInstance()->PreDraw(kNormal);
		if (evolutionBackdropSprite_) { evolutionBackdropSprite_->Draw(); ++evolutionUiProfile_.spriteDraws; }
		if (evolutionPreviewPanelSprite_) { evolutionPreviewPanelSprite_->Draw(); ++evolutionUiProfile_.spriteDraws; }
		if (evolutionStatsPanelSprite_) { evolutionStatsPanelSprite_->Draw(); ++evolutionUiProfile_.spriteDraws; }
		if (evolutionShotSprite_) { evolutionShotSprite_->Draw(); ++evolutionUiProfile_.spriteDraws; }
		if (evolutionPreviewTankSprite_) { evolutionPreviewTankSprite_->Draw(); ++evolutionUiProfile_.spriteDraws; }

		for (auto& tank : encyclopedia_) {
			if (tank.cardSprite) { tank.cardSprite->Draw(); ++evolutionUiProfile_.spriteDraws; }
			tank.sprite->Draw(); // 各スプライトが持つ位置で描画
			++evolutionUiProfile_.spriteDraws;
		}
		if (evolutionChangeButtonSprite_) {
			evolutionChangeButtonSprite_->Draw();
			++evolutionUiProfile_.spriteDraws;
		}
		const auto spriteEnd = std::chrono::steady_clock::now();

		const auto textStart = std::chrono::steady_clock::now();
		if (evolutionTitleLabel_) { evolutionTitleLabel_->Draw(); ++evolutionUiProfile_.textDraws; }
		if (evolutionHintLabel_) { evolutionHintLabel_->Draw(); ++evolutionUiProfile_.textDraws; }
		if (evolutionPreviewNameLabel_) { evolutionPreviewNameLabel_->Draw(); ++evolutionUiProfile_.textDraws; }
		if (evolutionRoleLabel_) { evolutionRoleLabel_->Draw(); ++evolutionUiProfile_.textDraws; }

		for (auto& tank : encyclopedia_) {
			if (tank.nameLabel) { tank.nameLabel->Draw(); ++evolutionUiProfile_.textDraws; }
			if (tank.rankLabel) { tank.rankLabel->Draw(); ++evolutionUiProfile_.textDraws; }
		}

		for (auto& label : evolutionStatLabels_) {
			if (label) {
				label->Draw();
				++evolutionUiProfile_.textDraws;
			}
		}
		if (evolutionChangeButtonLabel_) {
			evolutionChangeButtonLabel_->Draw();
			++evolutionUiProfile_.textDraws;
		}
		const auto textEnd = std::chrono::steady_clock::now();
		const auto totalEnd = std::chrono::steady_clock::now();

		evolutionUiProfile_.spriteMs = std::chrono::duration<float, std::milli>(spriteEnd - spriteStart).count();
		evolutionUiProfile_.textMs = std::chrono::duration<float, std::milli>(textEnd - textStart).count();
		evolutionUiProfile_.updateMs = std::chrono::duration<float, std::milli>(spriteStart - totalStart).count();
		evolutionUiProfile_.totalMs = std::chrono::duration<float, std::milli>(totalEnd - totalStart).count();
	}
}

void Player::DrawTankCodex()
{
#ifdef USE_IMGUI
	if (classOrder_.empty()) {
		LoadPlayerClassConfigs();
	}
	if (classOrder_.empty()) {
		return;
	}

	codexSelectedClassIndex_ = (std::clamp)(codexSelectedClassIndex_, 0, static_cast<int>(classOrder_.size()) - 1);
	const std::string selectedId = classOrder_[codexSelectedClassIndex_];
	const PlayerClassConfig* selectedConfig = GetClassConfig(selectedId);
	if (!selectedConfig) {
		return;
	}

	if (!ImGui::Begin("Tank Codex / Evolution Tree")) {
		ImGui::End();
		return;
	}

	const int currentRank = GetRankFromLevel(level_);
	codexPreviewTimer_ += dt_;
	ImGui::Text("Level %d  Rank %d  Current: %s", level_, currentRank, GetCurrentClassName());
	ImGui::Text("Click a tank to inspect it. Locked tanks can be previewed, but not equipped.");
	ImGui::Separator();

	ImGui::Columns(3, "TankCodexColumns", true);
	ImGui::SetColumnWidth(0, 230.0f);
	ImGui::SetColumnWidth(1, 420.0f);

	ImGui::Text("Evolution Tree");
	ImGui::BeginChild("TankCodexTree", ImVec2(0.0f, 430.0f), true);
	for (int rank = 1; rank <= 4; ++rank) {
		ImGui::Text("Rank %d", rank);
		ImGui::Indent(14.0f);
		for (int i = 0; i < static_cast<int>(classOrder_.size()); ++i) {
			const PlayerClassConfig* config = GetClassConfig(classOrder_[i]);
			if (!config || config->requiredRank != rank) {
				continue;
			}
			const bool selected = i == codexSelectedClassIndex_;
			const bool locked = currentRank < config->requiredRank;
			ImGui::PushID(i);
			if (locked) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.62f, 1.0f));
			}
			std::string label = config->displayName;
			if (config->id == currentClassId_) {
				label += "  [Current]";
			} else if (locked) {
				label += "  [Locked]";
			}
			if (ImGui::Selectable(label.c_str(), selected)) {
				codexSelectedClassIndex_ = i;
			}
			if (locked) {
				ImGui::PopStyleColor();
			}
			ImGui::PopID();
		}
		ImGui::Unindent(14.0f);
		ImGui::Spacing();
		if (rank < 4) {
			ImGui::Text("  v");
		}
	}
	ImGui::EndChild();

	ImGui::NextColumn();

	ImGui::Text("Preview");
	ImGui::BeginChild("TankCodexPreview", ImVec2(0.0f, 430.0f), true);
	ImGui::Checkbox("Auto Move", &codexPreviewAutoMove_);
	ImGui::SameLine();
	ImGui::Checkbox("Auto Fire", &codexPreviewAutoFire_);
	ImGui::SliderFloat("Aim Deg", &codexPreviewAimDeg_, -180.0f, 180.0f);
	if (ImGui::Button("Test Shot")) {
		codexPreviewTimer_ = 0.0f;
	}

	const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
	const ImVec2 canvasSize = ImVec2((std::max)(360.0f, ImGui::GetContentRegionAvail().x), 300.0f);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(8, 12, 20, 255));
	drawList->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(90, 160, 190, 150));

	const float grid = 28.0f;
	for (float x = std::fmod(codexPreviewTimer_ * 18.0f, grid); x < canvasSize.x; x += grid) {
		drawList->AddLine(ImVec2(canvasPos.x + x, canvasPos.y), ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y), IM_COL32(45, 105, 155, 80), 1.0f);
	}
	for (float y = std::fmod(codexPreviewTimer_ * 10.0f, grid); y < canvasSize.y; y += grid) {
		drawList->AddLine(ImVec2(canvasPos.x, canvasPos.y + y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y), IM_COL32(45, 105, 155, 80), 1.0f);
	}

	const float moveWave = codexPreviewAutoMove_ ? std::sin(codexPreviewTimer_ * 1.7f) : 0.0f;
	const ImVec2 center = ImVec2(canvasPos.x + canvasSize.x * 0.48f + moveWave * 42.0f, canvasPos.y + canvasSize.y * 0.55f);
	const float aimRad = codexPreviewAimDeg_ * 3.1415926535f / 180.0f;
	const ImVec2 forward = ImVec2(std::cos(aimRad), std::sin(aimRad));
	const ImVec2 right = ImVec2(-forward.y, forward.x);
	const float bodyRadius = 38.0f;
	const ImU32 bodyFill = IM_COL32(97, 250, 135, 235);
	const ImU32 bodyOutline = IM_COL32(190, 255, 210, 255);
	const ImU32 barrelFill = IM_COL32(120, 245, 145, 170);
	const ImU32 barrelOutline = IM_COL32(210, 255, 220, 230);

	auto addRotatedRect = [&](ImVec2 origin, ImVec2 axisX, ImVec2 axisY, float halfX, float halfY, ImU32 fill, ImU32 outline) {
		const ImVec2 p0 = ImVec2(origin.x - axisX.x * halfX - axisY.x * halfY, origin.y - axisX.y * halfX - axisY.y * halfY);
		const ImVec2 p1 = ImVec2(origin.x + axisX.x * halfX - axisY.x * halfY, origin.y + axisX.y * halfX - axisY.y * halfY);
		const ImVec2 p2 = ImVec2(origin.x + axisX.x * halfX + axisY.x * halfY, origin.y + axisX.y * halfX + axisY.y * halfY);
		const ImVec2 p3 = ImVec2(origin.x - axisX.x * halfX + axisY.x * halfY, origin.y - axisX.y * halfX + axisY.y * halfY);
		drawList->AddQuadFilled(p0, p1, p2, p3, fill);
		drawList->AddQuad(p0, p1, p2, p3, outline, 2.0f);
	};

	std::vector<ImVec2> muzzlePoints;
	for (const WeaponMountConfig& barrel : selectedConfig->barrels) {
		const float localAngleRad = (codexPreviewAimDeg_ + barrel.angleDeg) * 3.1415926535f / 180.0f;
		const ImVec2 barrelForward = ImVec2(std::cos(localAngleRad), std::sin(localAngleRad));
		const ImVec2 barrelRight = ImVec2(-barrelForward.y, barrelForward.x);
		const ImVec2 offset = ImVec2(
			forward.x * barrel.offset.x * 42.0f + right.x * barrel.offset.y * 42.0f,
			forward.y * barrel.offset.x * 42.0f + right.y * barrel.offset.y * 42.0f
		);
		const float length = (std::max)(26.0f, barrel.scale.x * 34.0f);
		const float width = (std::max)(8.0f, barrel.scale.y * 38.0f);
		const ImVec2 base = ImVec2(center.x + offset.x + barrelForward.x * length * 0.32f, center.y + offset.y + barrelForward.y * length * 0.32f);
		addRotatedRect(base, barrelForward, barrelRight, length * 0.5f, width * 0.5f, barrelFill, barrelOutline);
		muzzlePoints.push_back(ImVec2(base.x + barrelForward.x * length * 0.56f, base.y + barrelForward.y * length * 0.56f));
	}

	drawList->AddCircleFilled(center, bodyRadius + 7.0f, IM_COL32(95, 255, 135, 45), 48);
	drawList->AddCircleFilled(center, bodyRadius, bodyFill, 48);
	drawList->AddCircle(center, bodyRadius, bodyOutline, 48, 3.0f);

	const bool showShot = codexPreviewAutoFire_ || std::fmod(codexPreviewTimer_, 1.0f) < 0.18f;
	if (showShot) {
		const float shotPhase = std::fmod(codexPreviewTimer_ * 1.8f, 1.0f);
		for (const ImVec2& muzzle : muzzlePoints) {
			const ImVec2 head = ImVec2(muzzle.x + forward.x * (50.0f + shotPhase * 130.0f), muzzle.y + forward.y * (50.0f + shotPhase * 130.0f));
			drawList->AddLine(muzzle, head, IM_COL32(255, 245, 175, 190), 8.0f);
			drawList->AddLine(muzzle, head, IM_COL32(255, 105, 130, 210), 3.0f);
			drawList->AddCircleFilled(head, 8.0f, IM_COL32(255, 245, 185, 220), 20);
		}
	}

	if (selectedConfig->usesDrone) {
		for (int i = 0; i < (std::min)(selectedConfig->maxDrones, 7); ++i) {
			const float a = codexPreviewTimer_ * 1.8f + static_cast<float>(i) * 6.283185307f / (std::max)(1, (std::min)(selectedConfig->maxDrones, 7));
			const ImVec2 drone = ImVec2(center.x + std::cos(a) * 78.0f, center.y + std::sin(a) * 78.0f);
			drawList->AddCircleFilled(drone, 8.0f, IM_COL32(120, 230, 255, 210), 20);
			drawList->AddCircle(drone, 8.0f, IM_COL32(215, 250, 255, 230), 20, 2.0f);
		}
	}

	ImGui::Dummy(canvasSize);
	ImGui::EndChild();

	ImGui::NextColumn();

	ImGui::Text("Tank Data");
	ImGui::BeginChild("TankCodexStats", ImVec2(0.0f, 430.0f), true);
	const bool locked = currentRank < selectedConfig->requiredRank;
	ImGui::Text("Name: %s", selectedConfig->displayName.c_str());
	ImGui::Text("ID: %s", selectedConfig->id.c_str());
	ImGui::Text("Required Rank: %d  %s", selectedConfig->requiredRank, locked ? "(locked)" : "(available)");
	ImGui::Separator();
	ImGui::Text("Barrels: %zu", selectedConfig->barrels.size());
	ImGui::Text("Fire Mode: %s%s",
		selectedConfig->fireAllBarrels ? "All Barrels" : (selectedConfig->alternateBarrels ? "Alternate" : "Single"),
		selectedConfig->usesDrone ? " + Drone" : "");
	ImGui::Text("Bullet Count: %d", selectedConfig->bulletCount);
	ImGui::Text("Reload Scale: %.2f", selectedConfig->reloadScale);
	ImGui::Text("Bullet Speed Scale: %.2f", selectedConfig->bulletSpeedScale);
	ImGui::Text("Bullet Damage Scale: %.2f", selectedConfig->bulletDamageScale);
	ImGui::Text("Spread: %.1f deg  %s", selectedConfig->spreadAngleDeg, selectedConfig->randomSpread ? "random" : "fixed");
	ImGui::Text("Reflect: %s", selectedConfig->reflect ? "yes" : "no");
	ImGui::Text("Penetrate: %s", selectedConfig->penetrate ? "yes" : "no");
	ImGui::Text("Drones: %s  max %d", selectedConfig->usesDrone ? "yes" : "no", selectedConfig->maxDrones);
	ImGui::Separator();
	ImGui::TextWrapped("Role: %s",
		selectedConfig->usesDrone ? "Controls drones and keeps pressure while repositioning." :
		selectedConfig->reflect ? "Uses bounce shots to fight around cover." :
		selectedConfig->bulletSpeedScale > 1.2f ? "Long range, fast projectile style." :
		selectedConfig->reloadScale < 0.75f ? "Rapid fire tank for close and mid range pressure." :
		selectedConfig->bulletCount > 1 || selectedConfig->barrels.size() >= 3 ? "Wide multi-shot tank that controls space." :
		"Balanced starter tank.");
	ImGui::Spacing();
	if (locked) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
	}
	const bool changeClicked = ImGui::Button(locked ? "Locked" : "Change To This Tank", ImVec2(-1.0f, 32.0f));
	if (locked) {
		ImGui::PopStyleColor(3);
	}
	if (!locked && changeClicked) {
		EvolveById(selectedConfig->id);
	}
	if (locked) {
		ImGui::TextWrapped("Need Rank %d. Use debug exp or play to unlock it.", selectedConfig->requiredRank);
	}
	if (ImGui::Button("Edit This In Class Editor", ImVec2(-1.0f, 28.0f))) {
		editorSelectedClassIndex_ = codexSelectedClassIndex_;
	}
	ImGui::EndChild();

	ImGui::Columns(1);
	ImGui::End();
#endif
}

int Player::GetRankFromLevel(int level) {
	if (level >= 15) return 4;
	if (level >= 10) return 3;
	if (level >= 5) return 2;
	return 1;
}

void Player::DrawPlayerClassEditor()
{
#ifdef USE_IMGUI
	if (!ImGui::Begin("プレイヤー機体エディタ")) {
		ImGui::End();
		return;
	}

	if (classOrder_.empty()) {
		LoadPlayerClassConfigs();
	}
	editorSelectedClassIndex_ = (std::clamp)(editorSelectedClassIndex_, 0, static_cast<int>(classOrder_.size()) - 1);
	std::string selectedId = classOrder_[editorSelectedClassIndex_];
	PlayerClassConfig* config = GetMutableClassConfig(selectedId);
	if (!config) {
		ImGui::Text("機体設定が見つかりません。");
		ImGui::End();
		return;
	}

	if (ImGui::BeginCombo("機体クラス", config->displayName.c_str())) {
		for (int i = 0; i < static_cast<int>(classOrder_.size()); ++i) {
			const std::string& id = classOrder_[i];
			const PlayerClassConfig* itemConfig = GetClassConfig(id);
			const char* label = itemConfig ? itemConfig->displayName.c_str() : id.c_str();
			const bool selected = i == editorSelectedClassIndex_;
			if (ImGui::Selectable(label, selected)) {
				editorSelectedClassIndex_ = i;
			}
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	bool rebuildBarrels = false;
	bool relayoutBarrels = false;
	auto makeUniqueId = [this](const std::string& baseId) {
		const std::string prefix = baseId.empty() ? "CustomTank" : baseId;
		if (classConfigs_.find(prefix) == classConfigs_.end()) {
			return prefix;
		}
		for (int i = 1; i < 1000; ++i) {
			const std::string candidate = prefix + "_" + std::to_string(i);
			if (classConfigs_.find(candidate) == classConfigs_.end()) {
				return candidate;
			}
		}
		return prefix + "_Copy";
	};

	if (ImGui::Button("新規作成")) {
		PlayerClassConfig newConfig = CreateDefaultClassConfig(ClassType::Basic);
		newConfig.id = makeUniqueId("CustomTank");
		newConfig.displayName = newConfig.id;
		classOrder_.push_back(newConfig.id);
		classConfigs_[newConfig.id] = newConfig;
		editorSelectedClassIndex_ = static_cast<int>(classOrder_.size()) - 1;
		selectedId = newConfig.id;
		config = GetMutableClassConfig(selectedId);
	}
	ImGui::SameLine();
	if (ImGui::Button("複製")) {
		PlayerClassConfig newConfig = *config;
		newConfig.id = makeUniqueId(config->id + "_Copy");
		newConfig.displayName = newConfig.id;
		classOrder_.push_back(newConfig.id);
		classConfigs_[newConfig.id] = newConfig;
		editorSelectedClassIndex_ = static_cast<int>(classOrder_.size()) - 1;
		selectedId = newConfig.id;
		config = GetMutableClassConfig(selectedId);
	}
	ImGui::SameLine();
	const bool isLegacyId = config->id == ClassTypeToString(config->type);
	if (!isLegacyId && ImGui::Button("削除")) {
		const bool deletingCurrent = currentClassId_ == config->id;
		classConfigs_.erase(config->id);
		classOrder_.erase(classOrder_.begin() + editorSelectedClassIndex_);
		editorSelectedClassIndex_ = (std::clamp)(editorSelectedClassIndex_, 0, static_cast<int>(classOrder_.size()) - 1);
		if (deletingCurrent) {
			EvolveById("Basic");
		}
		ImGui::End();
		return;
	}

	ImGui::Text("機体ID: %s", config->id.c_str());

	char nameBuffer[64]{};
	strncpy_s(nameBuffer, config->displayName.c_str(), _TRUNCATE);
	if (ImGui::InputText("表示名", nameBuffer, sizeof(nameBuffer))) {
		config->displayName = nameBuffer;
	}

	ImGui::DragInt("必要ランク", &config->requiredRank, 1.0f, 1, 4);
	ImGui::Checkbox("ドローン機体", &config->usesDrone);
	ImGui::DragInt("最大ドローン数", &config->maxDrones, 1.0f, 0, 32);
	ImGui::DragFloat("リロード倍率", &config->reloadScale, 0.01f, 0.05f, 5.0f);
	ImGui::DragFloat("弾速倍率", &config->bulletSpeedScale, 0.01f, 0.05f, 5.0f);
	ImGui::DragFloat("弾ダメージ倍率", &config->bulletDamageScale, 0.01f, 0.05f, 20.0f);
	ImGui::DragInt("同時発射弾数", &config->bulletCount, 1.0f, 1, 16);
	ImGui::DragFloat("拡散角度", &config->spreadAngleDeg, 0.1f, 0.0f, 180.0f);
	ImGui::Checkbox("ランダム拡散", &config->randomSpread);
	ImGui::Checkbox("反射弾", &config->reflect);
	ImGui::Checkbox("貫通弾", &config->penetrate);
	ImGui::Checkbox("全砲塔から発射", &config->fireAllBarrels);
	ImGui::Checkbox("砲塔を交互発射", &config->alternateBarrels);
	ImGui::DragFloat("反動", &config->recoilPower, 0.001f, 0.0f, 0.5f);

	ImGui::Separator();
	ImGui::Text("現在の機体: %s", GetCurrentClassName());
	if (ImGui::Button("この機体に切り替え")) {
		EvolveById(config->id);
		rebuildBarrels = false;
		relayoutBarrels = false;
	}
	ImGui::SameLine();
	if (ImGui::Button("現在の機体へ反映")) {
		rebuildBarrels = true;
	}
	ImGui::Separator();
	ImGui::Text("砲塔数: %zu", config->barrels.size());
	if (ImGui::CollapsingHeader("砲塔の自動配置", ImGuiTreeNodeFlags_DefaultOpen)) {
		static int layoutCount = 2;
		static float layoutForward = 0.72f;
		static float layoutSideSpacing = 0.34f;
		static float layoutAngleSpread = 0.0f;
		static float layoutMuzzleForward = 0.95f;
		static float layoutArcRadius = 0.62f;
		static float layoutArcCenterAngle = 0.0f;
		static float layoutArcSweepAngle = 80.0f;
		static float layoutArcRotationScale = 1.0f;
		static Vector3 layoutScale = { 1.25f, 0.24f, 0.24f };
		ImGui::DragInt("配置数", &layoutCount, 1.0f, 1, 12);
		ImGui::DragFloat("前方向位置", &layoutForward, 0.01f, -2.0f, 5.0f);
		ImGui::DragFloat("横間隔", &layoutSideSpacing, 0.01f, 0.0f, 3.0f);
		ImGui::DragFloat("角度広がり", &layoutAngleSpread, 0.1f, -180.0f, 180.0f);
		ImGui::DragFloat("銃口の前方オフセット", &layoutMuzzleForward, 0.01f, -2.0f, 5.0f);
		ImGui::DragFloat("円弧半径", &layoutArcRadius, 0.01f, 0.0f, 3.0f);
		ImGui::DragFloat("円弧中心角", &layoutArcCenterAngle, 0.1f, -180.0f, 180.0f);
		ImGui::DragFloat("円弧の広がり角", &layoutArcSweepAngle, 0.1f, 0.0f, 360.0f);
		ImGui::DragFloat("円弧回転倍率", &layoutArcRotationScale, 0.01f, -2.0f, 2.0f);
		ImGui::DragFloat3("配置スケール", &layoutScale.x, 0.01f, 0.01f, 10.0f);
		auto generateArcLayout = [&]() {
			layoutCount = (std::clamp)(layoutCount, 1, 12);
			const std::string model = config->barrels.empty() ? "gunBarrel.obj" : config->barrels.front().model;
			const bool fires = config->barrels.empty() ? true : config->barrels.front().fires;
			constexpr float kDegToRad = 3.1415926535f / 180.0f;
			config->barrels.clear();
			config->barrels.reserve(static_cast<size_t>(layoutCount));
			for (int i = 0; i < layoutCount; ++i) {
				const float centerIndex = (static_cast<float>(layoutCount) - 1.0f) * 0.5f;
				const float normalized = layoutCount <= 1 ? 0.0f : (static_cast<float>(i) - centerIndex) / centerIndex;
				const float angleDeg = layoutArcCenterAngle + normalized * layoutArcSweepAngle * 0.5f;
				const float angleRad = angleDeg * kDegToRad;
				WeaponMountConfig barrel{};
				barrel.model = model;
				barrel.offset = {
					std::cos(angleRad) * layoutArcRadius,
					std::sin(angleRad) * layoutArcRadius,
					0.0f
				};
				barrel.scale = layoutScale;
				barrel.angleDeg = (angleDeg - layoutArcCenterAngle) * layoutArcRotationScale + layoutArcCenterAngle;
				barrel.muzzleForward = layoutMuzzleForward;
				barrel.fires = fires;
				config->barrels.push_back(barrel);
			}
			config->fireAllBarrels = layoutCount > 1;
			config->alternateBarrels = false;
			rebuildBarrels = true;
		};
		if (ImGui::Button("左右対称配置を生成")) {
			layoutCount = (std::clamp)(layoutCount, 1, 12);
			const std::string model = config->barrels.empty() ? "gunBarrel.obj" : config->barrels.front().model;
			const bool fires = config->barrels.empty() ? true : config->barrels.front().fires;
			config->barrels.clear();
			config->barrels.reserve(static_cast<size_t>(layoutCount));
			for (int i = 0; i < layoutCount; ++i) {
				const float centerIndex = (static_cast<float>(layoutCount) - 1.0f) * 0.5f;
				const float normalized = layoutCount <= 1 ? 0.0f : (static_cast<float>(i) - centerIndex) / centerIndex;
				WeaponMountConfig barrel{};
				barrel.model = model;
				barrel.offset = { layoutForward, (static_cast<float>(i) - centerIndex) * layoutSideSpacing, 0.0f };
				barrel.scale = layoutScale;
				barrel.angleDeg = normalized * layoutAngleSpread * 0.5f;
				barrel.muzzleForward = layoutMuzzleForward;
				barrel.fires = fires;
				config->barrels.push_back(barrel);
			}
			config->fireAllBarrels = layoutCount > 2;
			config->alternateBarrels = layoutCount == 2;
			rebuildBarrels = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("V字配置を生成")) {
			layoutCount = (std::clamp)(layoutCount, 2, 12);
			const std::string model = config->barrels.empty() ? "gunBarrel.obj" : config->barrels.front().model;
			config->barrels.clear();
			config->barrels.reserve(static_cast<size_t>(layoutCount));
			for (int i = 0; i < layoutCount; ++i) {
				const float centerIndex = (static_cast<float>(layoutCount) - 1.0f) * 0.5f;
				const float signedIndex = static_cast<float>(i) - centerIndex;
				WeaponMountConfig barrel{};
				barrel.model = model;
				barrel.offset = {
					layoutForward - std::abs(signedIndex) * 0.12f,
					signedIndex * layoutSideSpacing,
					0.0f
				};
				barrel.scale = layoutScale;
				barrel.angleDeg = (layoutCount <= 1 || centerIndex == 0.0f) ? 0.0f : (signedIndex / centerIndex) * layoutAngleSpread * 0.5f;
				barrel.muzzleForward = layoutMuzzleForward;
				barrel.fires = true;
				config->barrels.push_back(barrel);
			}
			config->fireAllBarrels = true;
			config->alternateBarrels = false;
			rebuildBarrels = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("円弧ファン配置を生成")) {
			generateArcLayout();
		}
		if (ImGui::Button("プリセット 3連ファン")) {
			layoutCount = 3;
			layoutArcRadius = 0.62f;
			layoutArcCenterAngle = 0.0f;
			layoutArcSweepAngle = 70.0f;
			layoutArcRotationScale = 1.0f;
			layoutScale = { 1.25f, 0.24f, 0.24f };
			generateArcLayout();
		}
		ImGui::SameLine();
		if (ImGui::Button("プリセット 5連ファン")) {
			layoutCount = 5;
			layoutArcRadius = 0.64f;
			layoutArcCenterAngle = 0.0f;
			layoutArcSweepAngle = 120.0f;
			layoutArcRotationScale = 0.85f;
			layoutScale = { 1.18f, 0.22f, 0.22f };
			generateArcLayout();
		}
		ImGui::SameLine();
		if (ImGui::Button("プリセット 側面積み")) {
			layoutCount = 4;
			layoutArcRadius = 0.58f;
			layoutArcCenterAngle = 25.0f;
			layoutArcSweepAngle = 90.0f;
			layoutArcRotationScale = 0.65f;
			layoutScale = { 1.18f, 0.22f, 0.22f };
			generateArcLayout();
		}
		ImGui::Separator();
	}
	if (ImGui::Button("武器マウントを追加")) {
		WeaponMountConfig barrel{};
		if (!config->barrels.empty()) {
			barrel = config->barrels.back();
			barrel.offset.y += 0.34f;
		}
		config->barrels.push_back(barrel);
		rebuildBarrels = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("JSON保存")) {
		SavePlayerClassConfigs();
	}
	ImGui::SameLine();
	if (ImGui::Button("JSON再読み込み")) {
		LoadPlayerClassConfigs();
		rebuildBarrels = true;
	}

	for (size_t i = 0; i < config->barrels.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		WeaponMountConfig& barrel = config->barrels[i];
		const std::string label = "武器マウント " + std::to_string(i);
		if (ImGui::TreeNode(label.c_str())) {
			char modelBuffer[128]{};
			strncpy_s(modelBuffer, barrel.model.c_str(), _TRUNCATE);
			if (ImGui::InputText("モデル", modelBuffer, sizeof(modelBuffer))) {
				barrel.model = modelBuffer;
				rebuildBarrels = true;
			}
			relayoutBarrels |= ImGui::DragFloat3("位置 X/Y/Z", &barrel.offset.x, 0.01f, -10.0f, 10.0f);
			relayoutBarrels |= ImGui::DragFloat3("スケール", &barrel.scale.x, 0.01f, 0.0f, 10.0f);
			relayoutBarrels |= ImGui::DragFloat("角度", &barrel.angleDeg, 0.1f, -180.0f, 180.0f);
			ImGui::DragFloat("銃口の前方オフセット", &barrel.muzzleForward, 0.01f, -2.0f, 5.0f);
			ImGui::Checkbox("マウントを有効化", &barrel.fires);
			const char* weaponTypeNames[] = { "Projectile", "Laser", "Mine", "Drone (準備中)", "Melee" };
			int weaponTypeIndex = static_cast<int>(barrel.weaponType);
			if (ImGui::Combo("武器種", &weaponTypeIndex, weaponTypeNames, IM_ARRAYSIZE(weaponTypeNames))) {
				barrel.weaponType = static_cast<WeaponType>((std::clamp)(weaponTypeIndex, 0, 4));
			}
			if (barrel.weaponType == WeaponType::Laser) {
				ImGui::DragFloat("レーザー射程", &barrel.laserRange, 0.1f, 0.5f, 80.0f);
				ImGui::DragFloat("レーザー太さ", &barrel.laserWidth, 0.005f, 0.02f, 2.0f);
				ImGui::DragFloat("レーザー表示時間", &barrel.laserDuration, 0.005f, 0.01f, 1.0f);
				ImGui::DragFloat("レーザーダメージ間隔", &barrel.laserDamageInterval, 0.005f, 0.01f, 1.0f);
			} else if (barrel.weaponType == WeaponType::Mine) {
				ImGui::DragFloat("地雷爆発半径", &barrel.mineRadius, 0.05f, 0.2f, 20.0f);
				ImGui::DragFloat("地雷起爆待ち", &barrel.mineFuseTime, 0.01f, 0.0f, 5.0f);
				ImGui::DragFloat("地雷寿命", &barrel.mineLifeTime, 0.05f, 0.2f, 30.0f);
			} else if (barrel.weaponType == WeaponType::Melee) {
				ImGui::DragFloat("近接射程", &barrel.meleeRange, 0.05f, 0.3f, 12.0f);
				ImGui::DragFloat("近接角度", &barrel.meleeArcDeg, 0.5f, 5.0f, 360.0f);
				ImGui::DragFloat("近接線幅", &barrel.meleeWidth, 0.005f, 0.02f, 1.0f);
				ImGui::DragFloat("近接表示時間", &barrel.meleeDuration, 0.005f, 0.03f, 1.0f);
			} else if (barrel.weaponType != WeaponType::Projectile) {
				ImGui::TextDisabled("この武器種は次の実装段階まで発射されません。");
			}
			ImGui::ColorEdit4("エフェクト色", &barrel.effectColor.x);
			ImGui::DragFloat("マウント威力倍率", &barrel.damageScale, 0.01f, 0.0f, 20.0f);
			ImGui::DragFloat("マウント弾速倍率", &barrel.projectileSpeedScale, 0.01f, 0.01f, 10.0f);
			if (ImGui::Button("複製")) {
				config->barrels.insert(config->barrels.begin() + static_cast<std::ptrdiff_t>(i + 1), barrel);
				rebuildBarrels = true;
				ImGui::TreePop();
				ImGui::PopID();
				break;
			}
			ImGui::SameLine();
			if (ImGui::Button("削除") && config->barrels.size() > 1) {
				config->barrels.erase(config->barrels.begin() + static_cast<std::ptrdiff_t>(i));
				rebuildBarrels = true;
				ImGui::TreePop();
				ImGui::PopID();
				break;
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	if (config && config->id == currentClassId_ && (rebuildBarrels || relayoutBarrels)) {
		if (rebuildBarrels) {
			InitializeBarrels();
		}
		UpdateBarrelLayout();
	}

	ImGui::Text("メモ: 位置X=前方向、位置Y=横方向、角度=照準からのずれです。");
	ImGui::TextDisabled("WeaponMount v2: 旧 barrels JSONも自動で読み込めます。");
	ImGui::End();
#endif
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
	if (const PlayerClassConfig* config = GetCurrentClassConfig()) {
		return config->displayName.c_str();
	}
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
		break;
	case 1:
		break;
	case 2:
		break;
	case 3:
		break;
	case 4:
		break;
	case 5:
		break;
	case 6:
		break;
	}

	RecalculateStatsFromBase(false);
	return true;
}

bool Player::RefundStatUpgrade(int index)
{
	if (index < 0 || index >= static_cast<int>(upgradeLevels_.size())) {
		return false;
	}
	if (upgradeLevels_[index] <= 0) {
		return false;
	}

	upgradeLevels_[index]--;
	skillPoints_++;
	RecalculateStatsFromBase(false);
	return true;
}

void Player::RecalculateStatsFromBase(bool healToFull)
{
	const int oldMaxHp = GetMaxHp();
	const bool wasFullHp = oldMaxHp > 0 && hp_ >= oldMaxHp;

	stats_ = baseStats_;
	stats_.staminaRecovery *= 1.0f + healthRegenUpgradeRate_ * static_cast<float>(upgradeLevels_[0]);
	stats_.maxHp *= 1.0f + maxHpUpgradeRate_ * static_cast<float>(upgradeLevels_[1]);
	stats_.bodyDamage *= 1.0f + bodyDamageUpgradeRate_ * static_cast<float>(upgradeLevels_[2]);
	stats_.bulletSpeed *= 1.0f + bulletSpeedUpgradeRate_ * static_cast<float>(upgradeLevels_[3]);
	stats_.bulletDamage *= 1.0f + bulletDamageUpgradeRate_ * static_cast<float>(upgradeLevels_[4]);
	stats_.reloadSpeed *= (std::max)(0.05f, 1.0f - reloadUpgradeRate_ * static_cast<float>(upgradeLevels_[5]));
	stats_.reloadSpeed = (std::max)(minReloadSpeed_, stats_.reloadSpeed);
	stats_.moveSpeed *= 1.0f + moveSpeedUpgradeRate_ * static_cast<float>(upgradeLevels_[6]);
	stats_.maxHp = (std::max)(1.0f, stats_.maxHp);
	stats_.reloadSpeed = (std::max)(0.05f, stats_.reloadSpeed);
	stats_.bulletDamage = (std::max)(0.1f, stats_.bulletDamage);
	stats_.bulletSpeed = (std::max)(0.01f, stats_.bulletSpeed);
	stats_.moveSpeed = (std::max)(0.01f, stats_.moveSpeed);
	stats_.maxStamina = (std::max)(0.0f, stats_.maxStamina);
	stats_.stamina = (std::min)(stats_.stamina, stats_.maxStamina);

	SetDamage(static_cast<uint32_t>((std::max)(1.0f, stats_.bodyDamage)));

	const int newMaxHp = GetMaxHp();
	if (healToFull || wasFullHp) {
		hp_ = newMaxHp;
	} else {
		hp_ = (std::clamp)(hp_, 0, newMaxHp);
	}
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
	for (BarrelModel& barrel : barrels_) {
		if (barrel.object) {
			barrel.object->SetAlpha(stealthAlpha_);
		}
	}
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
	Vector3 position = GetWorldPosition();
	Vector3 moveDirection = velocity_;
	moveDirection.z = 0.0f;
	if (Length(moveDirection) > 0.001f) {
		position -= Normalize(moveDirection) * 0.65f;
	}
	ParticleManager::GetInstance()->Emit("DashDust", position, 1);
}

std::vector<Player::NeonBarrelLayout> Player::GetNeonBarrelLayouts() const
{
	std::vector<NeonBarrelLayout> layouts;
	const PlayerClassConfig* config = GetCurrentClassConfig();
	if (!config) {
		layouts.push_back({});
		return layouts;
	}

	layouts.reserve(config->barrels.size());
	for (size_t i = 0; i < config->barrels.size(); ++i) {
		const WeaponMountConfig& barrel = config->barrels[i];
		NeonBarrelLayout layout{};
		layout.offset = barrel.offset;
		layout.scale = barrel.scale;
		layout.angleRad = barrel.angleDeg * 3.1415926535f / 180.0f;
		if (i < barrels_.size()) {
			layout.recoilOffset = barrels_[i].recoilOffset;
		}
		layouts.push_back(layout);
	}
	return layouts;
}

float Player::GetDamageFeedbackRatio() const
{
	if (damageFeedbackDuration_ <= 0.0f) {
		return 0.0f;
	}
	return (std::clamp)(damageFeedbackTimer_ / damageFeedbackDuration_, 0.0f, 1.0f);
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
