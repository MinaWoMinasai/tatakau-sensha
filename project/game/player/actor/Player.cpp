#include "Player.h"
#include "Stage.h"
#include "Audio.h"
#include <algorithm>
#include <cmath>
#include <cstring>
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

void SetLabel(std::unique_ptr<TextLabel>& label, SpriteCommon* spriteCommon, const std::string& text, const Vector2& position, const TextStyle& style)
{
	if (!label) {
		label = std::make_unique<TextLabel>();
		label->Initialize(spriteCommon, text, style);
	} else {
		label->SetText(text);
	}
	label->SetPosition(position);
}

nlohmann::json Vector3ToJson(const Vector3& value)
{
	return nlohmann::json::array({ value.x, value.y, value.z });
}
}

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
	if (damageFeedbackTimer_ > 0.0f) {
		damageFeedbackTimer_ = (std::max)(0.0f, damageFeedbackTimer_ - deltaTime);
	}

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
	if (isDead_ || amount == 0 || invincibleTimer_ > 0.0f) {
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
	const int oldMaxHp = GetMaxHp();
	const int newMaxHp = (std::max)(1, config.maxHp);
	stats_.maxHp = static_cast<float>(newMaxHp);
	maxHpUpgradeAmount_ = (std::max)(1, config.maxHpUpgradeAmount);
	stats_.bodyDamage = static_cast<float>((std::max)(1u, config.bodyDamage));
	SetDamage(static_cast<uint32_t>(stats_.bodyDamage));

	if (config.healToFull) {
		hp_ = newMaxHp;
	} else if (oldMaxHp > 0 && hp_ >= oldMaxHp) {
		hp_ = newMaxHp;
	} else {
		hp_ = (std::clamp)(hp_, 0, newMaxHp);
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
		if (item.contains("barrels") && item["barrels"].is_array()) {
			for (const nlohmann::json& barrelJson : item["barrels"]) {
				PlayerBarrelConfig barrel{};
				barrel.model = barrelJson.value("model", barrel.model);
				barrel.offset = ReadVector3(barrelJson.value("offset", nlohmann::json::array()), barrel.offset);
				barrel.scale = ReadVector3(barrelJson.value("scale", nlohmann::json::array()), barrel.scale);
				barrel.angleDeg = barrelJson.value("angleDeg", barrel.angleDeg);
				barrel.muzzleForward = barrelJson.value("muzzleForward", barrel.muzzleForward);
				barrel.fires = barrelJson.value("fires", barrel.fires);
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
		item["barrels"] = nlohmann::json::array();
		for (const PlayerBarrelConfig& barrel : config->barrels) {
			nlohmann::json barrelJson;
			barrelJson["model"] = barrel.model;
			barrelJson["offset"] = Vector3ToJson(barrel.offset);
			barrelJson["scale"] = Vector3ToJson(barrel.scale);
			barrelJson["angleDeg"] = barrel.angleDeg;
			barrelJson["muzzleForward"] = barrel.muzzleForward;
			barrelJson["fires"] = barrel.fires;
			item["barrels"].push_back(barrelJson);
		}
		classes.push_back(item);
	}

	nlohmann::json root;
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
		if (config.barrels[i].fires) {
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
		const PlayerBarrelConfig& barrelConfig = config.barrels[index];
		const Vector3 fireDir = RotateDirection(forward, barrelConfig.angleDeg);
		const Vector3 muzzle =
			worldTransform_.translate +
			forward * barrelConfig.offset.x +
			right * barrelConfig.offset.y +
			Vector3{ 0.0f, 0.0f, barrelConfig.offset.z } +
			fireDir * barrelConfig.muzzleForward;
		attackController_.FireFromMuzzle(muzzle, fireDir, param, BulletOwner::kPlayer);
		if (index < barrels_.size()) {
			barrels_[index].recoilOffset = 0.22f;
		}
		SpawnCasing();
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
		const PlayerBarrelConfig barrelConfig = (config && i < config->barrels.size()) ? config->barrels[i] : PlayerBarrelConfig{};
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

	evolutionBackdropSprite_ = makePanel({ 0.0f, 0.0f }, { 1600.0f, 900.0f }, { 0.02f, 0.03f, 0.07f, 0.78f });
	evolutionPreviewPanelSprite_ = makePanel({ 36.0f, 86.0f }, { 500.0f, 690.0f }, { 0.05f, 0.12f, 0.17f, 0.86f });
	evolutionStatsPanelSprite_ = makePanel({ 1058.0f, 86.0f }, { 500.0f, 690.0f }, { 0.07f, 0.08f, 0.12f, 0.88f });
	evolutionPreviewTankSprite_ = std::make_unique<Sprite>();
	evolutionPreviewTankSprite_->Initialize(spriteCommon, "resources/normalTank.png");
	evolutionPreviewTankSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	evolutionPreviewTankSprite_->SetSize({ 280.0f, 170.0f });
	evolutionPreviewTankSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	evolutionShotSprite_ = makePanel({ 0.0f, 0.0f }, { 170.0f, 9.0f }, { 1.0f, 0.88f, 0.28f, 0.0f });
	evolutionShotSprite_->SetAnchorPoint({ 0.0f, 0.5f });
	evolutionChangeButtonSprite_ = makePanel({ 1115.0f, 700.0f }, { 385.0f, 54.0f }, { 0.24f, 0.86f, 0.44f, 0.92f });

	TextStyle titleStyle{};
	titleStyle.fontFamily = "Meiryo";
	titleStyle.fontSize = 34.0f;
	titleStyle.color = { 0.75f, 1.0f, 0.92f, 1.0f };
	titleStyle.outlineColor = { 0.0f, 0.08f, 0.10f, 0.95f };
	titleStyle.outlineThickness = 3.0f;
	titleStyle.padding = 8.0f;
	SetLabel(evolutionTitleLabel_, spriteCommon, "戦車図鑑 / 進化ツリー", { 50.0f, 30.0f }, titleStyle);

	TextStyle smallStyle = titleStyle;
	smallStyle.fontSize = 20.0f;
	smallStyle.color = { 0.86f, 0.92f, 1.0f, 1.0f };
	smallStyle.outlineThickness = 2.0f;
	SetLabel(evolutionHintLabel_, spriteCommon, "C:閉じる  /  カード選択:詳細  /  ボタン:機体変更", { 610.0f, 42.0f }, smallStyle);

	encyclopedia_.clear();
	const float cardWidth = 206.0f;
	const float cardHeight = 105.0f;
	const float cardGapX = 22.0f;
	const float cardGapY = 22.0f;
	const Vector2 cardBase = { 585.0f, 116.0f };

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

		const int col = i % 2;
		const int row = i / 2;
		const Vector2 cardPos = { cardBase.x + static_cast<float>(col) * (cardWidth + cardGapX), cardBase.y + static_cast<float>(row) * (cardHeight + cardGapY) };

		data.cardSprite = makePanel(cardPos, { cardWidth, cardHeight }, { 0.10f, 0.15f, 0.22f, 0.82f });
		data.sprite = std::make_unique<Sprite>();
		data.sprite->Initialize(SpriteCommon::GetInstance(), data.texturePath);
		data.sprite->SetPosition({ cardPos.x + 18.0f, cardPos.y + 10.0f });
		data.sprite->SetSize({ 170.0f, 56.0f });

		TextStyle cardNameStyle = smallStyle;
		cardNameStyle.fontSize = 17.0f;
		cardNameStyle.color = { 0.92f, 1.0f, 0.95f, 1.0f };
		SetLabel(data.nameLabel, spriteCommon, data.name, { cardPos.x + 12.0f, cardPos.y + 70.0f }, cardNameStyle);

		TextStyle rankStyle = cardNameStyle;
		rankStyle.fontSize = 14.0f;
		rankStyle.color = { 0.72f, 0.86f, 1.0f, 1.0f };
		SetLabel(data.rankLabel, spriteCommon, "Rank " + std::to_string(data.requiredRank), { cardPos.x + 132.0f, cardPos.y + 74.0f }, rankStyle);

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
		evolutionPreviewTankSprite_->SetPosition({ 286.0f + bob, 330.0f });
		evolutionPreviewTankSprite_->SetRotation(std::sin(evolutionUiTimer_ * 1.35f) * 0.08f);
		evolutionPreviewTankSprite_->SetColor(locked ? Vector4{ 0.35f, 0.40f, 0.45f, 0.65f } : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
		evolutionPreviewTankSprite_->Update();
	}

	if (evolutionShotSprite_) {
		const float shotPhase = std::fmod(evolutionUiTimer_ * 1.8f, 1.0f);
		evolutionShotSprite_->SetPosition({ 388.0f + shotPhase * 70.0f, 326.0f });
		evolutionShotSprite_->SetRotation(std::sin(evolutionUiTimer_ * 1.2f) * 0.12f);
		evolutionShotSprite_->SetSize({ 120.0f + shotPhase * 120.0f, 8.0f });
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

	if (isChangeMode) {
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
		SetLabel(evolutionPreviewNameLabel_, spriteCommon, selectedConfig->displayName, { 70.0f, 112.0f }, headingStyle);
		SetLabel(evolutionRoleLabel_, spriteCommon, roleText(*selectedConfig), { 72.0f, 580.0f }, bodyStyle);

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
			SetLabel(evolutionStatLabels_[i], spriteCommon, statLines[i], { 1100.0f, 150.0f + static_cast<float>(i) * 48.0f }, smallStyle);
		}

		TextStyle buttonStyle = headingStyle;
		buttonStyle.fontSize = 24.0f;
		buttonStyle.color = locked ? Vector4{ 0.68f, 0.68f, 0.72f, 1.0f } : Vector4{ 0.02f, 0.09f, 0.04f, 1.0f };
		buttonStyle.outlineColor = locked ? Vector4{ 0.0f, 0.0f, 0.0f, 0.70f } : Vector4{ 0.78f, 1.0f, 0.82f, 0.65f };
		SetLabel(evolutionChangeButtonLabel_, spriteCommon, locked ? "ランク不足" : "この戦車に変更", { 1212.0f, 711.0f }, buttonStyle);

		SpriteCommon::GetInstance()->PreDraw(kNormal);
		if (evolutionBackdropSprite_) evolutionBackdropSprite_->Draw();
		if (evolutionPreviewPanelSprite_) evolutionPreviewPanelSprite_->Draw();
		if (evolutionStatsPanelSprite_) evolutionStatsPanelSprite_->Draw();
		if (evolutionTitleLabel_) evolutionTitleLabel_->Draw();
		if (evolutionHintLabel_) evolutionHintLabel_->Draw();
		if (evolutionPreviewNameLabel_) evolutionPreviewNameLabel_->Draw();
		if (evolutionShotSprite_) evolutionShotSprite_->Draw();
		if (evolutionPreviewTankSprite_) evolutionPreviewTankSprite_->Draw();
		if (evolutionRoleLabel_) evolutionRoleLabel_->Draw();

		for (auto& tank : encyclopedia_) {
			if (tank.cardSprite) tank.cardSprite->Draw();
			tank.sprite->Draw(); // 各スプライトが持つ位置で描画
			if (tank.nameLabel) tank.nameLabel->Draw();
			if (tank.rankLabel) tank.rankLabel->Draw();
		}

		for (auto& label : evolutionStatLabels_) {
			if (label) {
				label->Draw();
			}
		}
		if (evolutionChangeButtonSprite_) {
			evolutionChangeButtonSprite_->Draw();
		}
		if (evolutionChangeButtonLabel_) {
			evolutionChangeButtonLabel_->Draw();
		}
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
	for (const PlayerBarrelConfig& barrel : selectedConfig->barrels) {
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
	if (!ImGui::Begin("Player Class Editor")) {
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
		ImGui::Text("Class config is missing.");
		ImGui::End();
		return;
	}

	if (ImGui::BeginCombo("Class", config->displayName.c_str())) {
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

	if (ImGui::Button("New Class")) {
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
	if (ImGui::Button("Duplicate Class")) {
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
	if (!isLegacyId && ImGui::Button("Delete Class")) {
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

	ImGui::Text("Class ID: %s", config->id.c_str());

	char nameBuffer[64]{};
	strncpy_s(nameBuffer, config->displayName.c_str(), _TRUNCATE);
	if (ImGui::InputText("Display Name", nameBuffer, sizeof(nameBuffer))) {
		config->displayName = nameBuffer;
	}

	ImGui::DragInt("Required Rank", &config->requiredRank, 1.0f, 1, 4);
	ImGui::Checkbox("Uses Drone", &config->usesDrone);
	ImGui::DragInt("Max Drones", &config->maxDrones, 1.0f, 0, 32);
	ImGui::DragFloat("Reload Scale", &config->reloadScale, 0.01f, 0.05f, 5.0f);
	ImGui::DragFloat("Bullet Speed Scale", &config->bulletSpeedScale, 0.01f, 0.05f, 5.0f);
	ImGui::DragFloat("Bullet Damage Scale", &config->bulletDamageScale, 0.01f, 0.05f, 20.0f);
	ImGui::DragInt("Bullet Count", &config->bulletCount, 1.0f, 1, 16);
	ImGui::DragFloat("Spread Angle Deg", &config->spreadAngleDeg, 0.1f, 0.0f, 180.0f);
	ImGui::Checkbox("Random Spread", &config->randomSpread);
	ImGui::Checkbox("Reflect", &config->reflect);
	ImGui::Checkbox("Penetrate", &config->penetrate);
	ImGui::Checkbox("Fire All Barrels", &config->fireAllBarrels);
	ImGui::Checkbox("Alternate Barrels", &config->alternateBarrels);
	ImGui::DragFloat("Recoil Power", &config->recoilPower, 0.001f, 0.0f, 0.5f);

	ImGui::Separator();
	ImGui::Text("Current Class: %s", GetCurrentClassName());
	if (ImGui::Button("Switch To This Class")) {
		EvolveById(config->id);
		rebuildBarrels = false;
		relayoutBarrels = false;
	}
	ImGui::SameLine();
	if (ImGui::Button("Apply To Current")) {
		rebuildBarrels = true;
	}
	ImGui::Separator();
	ImGui::Text("Barrels: %zu", config->barrels.size());
	if (ImGui::CollapsingHeader("Auto Barrel Layout", ImGuiTreeNodeFlags_DefaultOpen)) {
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
		ImGui::DragInt("Layout Count", &layoutCount, 1.0f, 1, 12);
		ImGui::DragFloat("Layout Forward", &layoutForward, 0.01f, -2.0f, 5.0f);
		ImGui::DragFloat("Layout Side Spacing", &layoutSideSpacing, 0.01f, 0.0f, 3.0f);
		ImGui::DragFloat("Layout Angle Spread", &layoutAngleSpread, 0.1f, -180.0f, 180.0f);
		ImGui::DragFloat("Layout Muzzle Forward", &layoutMuzzleForward, 0.01f, -2.0f, 5.0f);
		ImGui::DragFloat("Arc Radius", &layoutArcRadius, 0.01f, 0.0f, 3.0f);
		ImGui::DragFloat("Arc Center Angle", &layoutArcCenterAngle, 0.1f, -180.0f, 180.0f);
		ImGui::DragFloat("Arc Sweep Angle", &layoutArcSweepAngle, 0.1f, 0.0f, 360.0f);
		ImGui::DragFloat("Arc Rotation Scale", &layoutArcRotationScale, 0.01f, -2.0f, 2.0f);
		ImGui::DragFloat3("Layout Scale", &layoutScale.x, 0.01f, 0.01f, 10.0f);
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
				PlayerBarrelConfig barrel{};
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
		if (ImGui::Button("Generate Symmetric Layout")) {
			layoutCount = (std::clamp)(layoutCount, 1, 12);
			const std::string model = config->barrels.empty() ? "gunBarrel.obj" : config->barrels.front().model;
			const bool fires = config->barrels.empty() ? true : config->barrels.front().fires;
			config->barrels.clear();
			config->barrels.reserve(static_cast<size_t>(layoutCount));
			for (int i = 0; i < layoutCount; ++i) {
				const float centerIndex = (static_cast<float>(layoutCount) - 1.0f) * 0.5f;
				const float normalized = layoutCount <= 1 ? 0.0f : (static_cast<float>(i) - centerIndex) / centerIndex;
				PlayerBarrelConfig barrel{};
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
		if (ImGui::Button("Generate V Shape")) {
			layoutCount = (std::clamp)(layoutCount, 2, 12);
			const std::string model = config->barrels.empty() ? "gunBarrel.obj" : config->barrels.front().model;
			config->barrels.clear();
			config->barrels.reserve(static_cast<size_t>(layoutCount));
			for (int i = 0; i < layoutCount; ++i) {
				const float centerIndex = (static_cast<float>(layoutCount) - 1.0f) * 0.5f;
				const float signedIndex = static_cast<float>(i) - centerIndex;
				PlayerBarrelConfig barrel{};
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
		if (ImGui::Button("Generate Arc Fan")) {
			generateArcLayout();
		}
		if (ImGui::Button("Preset 3 Fan")) {
			layoutCount = 3;
			layoutArcRadius = 0.62f;
			layoutArcCenterAngle = 0.0f;
			layoutArcSweepAngle = 70.0f;
			layoutArcRotationScale = 1.0f;
			layoutScale = { 1.25f, 0.24f, 0.24f };
			generateArcLayout();
		}
		ImGui::SameLine();
		if (ImGui::Button("Preset 5 Fan")) {
			layoutCount = 5;
			layoutArcRadius = 0.64f;
			layoutArcCenterAngle = 0.0f;
			layoutArcSweepAngle = 120.0f;
			layoutArcRotationScale = 0.85f;
			layoutScale = { 1.18f, 0.22f, 0.22f };
			generateArcLayout();
		}
		ImGui::SameLine();
		if (ImGui::Button("Preset Side Stack")) {
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
	if (ImGui::Button("Add Barrel")) {
		PlayerBarrelConfig barrel{};
		if (!config->barrels.empty()) {
			barrel = config->barrels.back();
			barrel.offset.y += 0.34f;
		}
		config->barrels.push_back(barrel);
		rebuildBarrels = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Save JSON")) {
		SavePlayerClassConfigs();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reload JSON")) {
		LoadPlayerClassConfigs();
		rebuildBarrels = true;
	}

	for (size_t i = 0; i < config->barrels.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		PlayerBarrelConfig& barrel = config->barrels[i];
		const std::string label = "Barrel " + std::to_string(i);
		if (ImGui::TreeNode(label.c_str())) {
			char modelBuffer[128]{};
			strncpy_s(modelBuffer, barrel.model.c_str(), _TRUNCATE);
			if (ImGui::InputText("Model", modelBuffer, sizeof(modelBuffer))) {
				barrel.model = modelBuffer;
				rebuildBarrels = true;
			}
			relayoutBarrels |= ImGui::DragFloat3("Offset X/Y/Z", &barrel.offset.x, 0.01f, -10.0f, 10.0f);
			relayoutBarrels |= ImGui::DragFloat3("Scale", &barrel.scale.x, 0.01f, 0.0f, 10.0f);
			relayoutBarrels |= ImGui::DragFloat("Angle Deg", &barrel.angleDeg, 0.1f, -180.0f, 180.0f);
			ImGui::DragFloat("Muzzle Forward", &barrel.muzzleForward, 0.01f, -2.0f, 5.0f);
			ImGui::Checkbox("Fires", &barrel.fires);
			if (ImGui::Button("Duplicate")) {
				config->barrels.insert(config->barrels.begin() + static_cast<std::ptrdiff_t>(i + 1), barrel);
				rebuildBarrels = true;
				ImGui::TreePop();
				ImGui::PopID();
				break;
			}
			ImGui::SameLine();
			if (ImGui::Button("Remove") && config->barrels.size() > 1) {
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

	ImGui::Text("Tip: offset.x = forward, offset.y = side, angleDeg = aim offset.");
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
		stats_.staminaRecovery += 0.25f;
		break;
	case 1:
		stats_.maxHp += static_cast<float>(maxHpUpgradeAmount_);
		hp_ = (std::min)(GetMaxHp(), hp_ + maxHpUpgradeAmount_);
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
