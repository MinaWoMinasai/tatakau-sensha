#include "Player.h"
#include "Stage.h"
#include "Audio.h"
#include "game/ui/TankButtonUI.h"
#include "game/ui/NeonTextEffect.h"
#include "ObjectPostEffect.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
struct SpecialActionDefinition {
	const char* id;
	const char* displayName;
	bool implemented;
};

const std::array<SpecialActionDefinition, 4>& SpecialActionDefinitions()
{
	static const std::array<SpecialActionDefinition, 4> definitions = {{
		{ "none", "なし", true },
		{ "perfect_dodge", "ジャスト回避", true },
		{ "saber_counter", "剣カウンター", true },
		{ "charge_beam", "チャージビーム（準備中）", false }
	}};
	return definitions;
}

#ifdef USE_IMGUI
void DrawEditorHelp(const char* text)
{
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
		ImGui::TextUnformatted(text);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}
#endif

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

Vector2 ReadVector2(const nlohmann::json& json, const Vector2& fallback)
{
	if (!json.is_array() || json.size() < 2) {
		return fallback;
	}
	if (!json[0].is_number() || !json[1].is_number()) {
		return fallback;
	}
	return {
		json[0].get<float>(),
		json[1].get<float>()
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

BarrelShape BarrelShapeFromString(const std::string& id)
{
	if (id == "Heavy") return BarrelShape::Heavy;
	if (id == "Short") return BarrelShape::Short;
	if (id == "Wide") return BarrelShape::Wide;
	if (id == "Trapezoid") return BarrelShape::Trapezoid;
	return BarrelShape::Box;
}

const char* BarrelShapeToString(BarrelShape shape)
{
	switch (shape) {
	case BarrelShape::Box: return "Box";
	case BarrelShape::Heavy: return "Heavy";
	case BarrelShape::Short: return "Short";
	case BarrelShape::Wide: return "Wide";
	case BarrelShape::Trapezoid: return "Trapezoid";
	}
	return "Box";
}

Player::BodyShape BodyShapeFromString(const std::string& id)
{
	if (id == "Box") return Player::BodyShape::Box;
	if (id == "Triangle") return Player::BodyShape::Triangle;
	if (id == "Pentagon") return Player::BodyShape::Pentagon;
	return Player::BodyShape::Circle;
}

const char* BodyShapeToString(Player::BodyShape shape)
{
	switch (shape) {
	case Player::BodyShape::Circle: return "Circle";
	case Player::BodyShape::Box: return "Box";
	case Player::BodyShape::Triangle: return "Triangle";
	case Player::BodyShape::Pentagon: return "Pentagon";
	}
	return "Circle";
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
	bulletCoolTime = (std::max)(0.0f, bulletCoolTime - deltaTime);
	for (float& cooldown : weaponGroupCooldowns_) {
		cooldown = (std::max)(0.0f, cooldown - deltaTime);
	}

	if (input_->IsPress(input_->GetMouseState().rgbButtons[0]) && !upgradeHudMouseCaptured_) {

		if (const PlayerClassConfig* config = GetCurrentClassConfig()) {
			const float baseReload = isBuffActive_ ? (stats_.reloadSpeed * 0.7f) / 60.0f : stats_.reloadSpeed / 60.0f;
			Vector3 recoilDir = Normalize(dir_) * -1.0f;
			float recoilPower = 0.01f;
			if (FireConfiguredClass(*config, bulletManager, baseReload, recoilDir, recoilPower)) {
				velocity_ += recoilDir * recoilPower;
				Audio::GetInstance()->PlayAudioSE(L"bulletShoot", 0.6f);
			}
			return;
		}

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
			// AddExpはPlayer::Update後の衝突処理から呼ばれる場合があるため、
			// 同じフレームの初回描画より先に遅延フォント更新を完了させる。
			PrepareStaticEvolutionTextTextures();
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
	LoadEvolutionUiStyle();
	InitializeStaticEvolutionPrototype();
	InitializeUpgradeHud();
	LoadUpgradeHudConfig();
	ApplyUpgradeHudLayout();

}

void Player::Update(
	Camera* viewProjection,
	Stage& stage,
	BulletManager* BulletManager,
	float deltaTime,
	float uiDeltaTime)
{
	sprite->Update();

	POINT mousePos;
	GetCursorPos(&mousePos);
	ScreenToClient(WinApp::GetInstance()->GetHwnd(), &mousePos);
	mousePosition_ = { static_cast<float>(mousePos.x), static_cast<float>(mousePos.y) };
	const bool evolutionUiWasOpen = isChangeMode;
	UpdateEncyclopedia(uiDeltaTime);
	UpdateUpgradeHud();

	if (input_->IsTrigger(input_->GetKey()[DIK_C], input_->GetPreKey()[DIK_C])) {
		if (isChangeMode) {
			isChangeMode = false;
			evolutionCancelledEvent_ = true;
		} else {
			isChangeMode = true;
			PrepareStaticEvolutionTextTextures();
		}
	}
	// 進化UIを操作したクリックやキー入力を、そのまま射撃・移動へ流さない。
	// 確定やキャンセルでこのフレーム中に閉じた場合も、次フレームまでゲーム入力を抑止する。
	if (evolutionUiWasOpen || isChangeMode) {
		machineGunBtnSprite_->Update();
		return;
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
	if (meleeComboTimer_ > 0.0f) {
		meleeComboTimer_ = (std::max)(0.0f, meleeComboTimer_ - deltaTime);
	} else {
		meleeComboStep_ = 0;
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

	// 右クリックは機体ごとの特殊行動スロットとして扱う。
	if (input_->IsTrigger(input_->GetMouseState().rgbButtons[1], input_->GetPreMouseState().rgbButtons[1])) {
		TryActivateSpecialAction();
	}
	if (saberCounterTimer_ > 0.0f) {
		saberCounterTimer_ = (std::max)(0.0f, saberCounterTimer_ - deltaTime);
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
	if (saberCounterTimer_ > 0.0f) {
		const PlayerClassConfig* config = GetCurrentClassConfig();
		if (config && config->specialActionId == "saber_counter") {
			TriggerSaberCounter(*config);
			return;
		}
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

void Player::UpdateDefeatPresentation(float deltaTime)
{
	if (isExploding_) {
		UpdateParticles((std::max)(0.0f, deltaTime));
	}
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

bool Player::TryActivateSpecialAction()
{
	const PlayerClassConfig* config = GetCurrentClassConfig();
	if (!config || config->specialActionId == "none" || dashCooldown_ > 0.0f) {
		return false;
	}
	if (config->specialActionId == "perfect_dodge") {
		return ActivatePerfectDodge(*config);
	}
	if (config->specialActionId == "saber_counter") {
		return ActivateSaberCounter(*config);
	}
	// charge_beam は同じ入口へ後から実装する。
	return false;
}

bool Player::ActivatePerfectDodge(const PlayerClassConfig& config)
{
	const float staminaCost = (std::max)(0.0f, config.specialActionStaminaCost);
	if (stats_.stamina < staminaCost) {
		return false;
	}

	Vector3 dashDir = inputDir_;
	if (Length(dashDir) < 0.01f) {
		dashDir = dir_;
	}
	if (Length(dashDir) < 0.01f) {
		return false;
	}

	velocity_ = Normalize(dashDir) * kDashSpeed;
	isDashing_ = true;
	dashTimer_ = kDashDuration;
	dashCooldown_ = kDashCooldown * (std::max)(0.05f, config.specialActionCooldownScale);
	stats_.stamina = (std::max)(0.0f, stats_.stamina - staminaCost);
	return true;
}

bool Player::ActivateSaberCounter(const PlayerClassConfig& config)
{
	const bool hasMeleeWeapon = std::any_of(config.barrels.begin(), config.barrels.end(), [](const WeaponMountConfig& mount) {
		return mount.fires && mount.weaponType == WeaponType::Melee;
	});
	const float staminaCost = (std::max)(0.0f, config.specialActionStaminaCost);
	if (!hasMeleeWeapon || stats_.stamina < staminaCost) {
		return false;
	}

	saberCounterTimer_ = (std::max)(0.01f, config.saberCounterWindow);
	dashCooldown_ = 1.0f * (std::max)(0.05f, config.specialActionCooldownScale);
	stats_.stamina = (std::max)(0.0f, stats_.stamina - staminaCost);
	return true;
}

void Player::TriggerSaberCounter(const PlayerClassConfig& config)
{
	const auto mountIt = std::find_if(config.barrels.begin(), config.barrels.end(), [](const WeaponMountConfig& mount) {
		return mount.fires && mount.weaponType == WeaponType::Melee;
	});
	if (mountIt == config.barrels.end()) {
		return;
	}

	const WeaponMountConfig& mount = *mountIt;
	const Vector3 forward = Length(dir_) > 0.0001f ? Normalize(dir_) : Vector3{ 1.0f, 0.0f, 0.0f };
	const Vector3 right = { -forward.y, forward.x, 0.0f };
	MeleeSlashEvent event{};
	event.origin = worldTransform_.translate + forward * mount.offset.x + right * mount.offset.y + Vector3{ 0.0f, 0.0f, mount.offset.z };
	event.direction = RotateDirection(forward, mount.angleDeg);
	event.range = mount.meleeRange * config.saberCounterRangeScale;
	event.arcDeg = (std::max)(180.0f, mount.meleeArcDeg);
	event.width = mount.meleeWidth * 1.35f;
	event.duration = (std::max)(0.08f, mount.meleeDuration * 0.85f);
	event.windupDuration = 0.0f;
	event.recoveryDuration = 0.22f;
	event.comboStep = 2;
	event.damage = static_cast<uint32_t>((std::max)(1.0f, stats_.bulletDamage * mount.damageScale * config.saberCounterDamageScale));
	event.color = { 0.65f, 1.45f, 1.25f, 1.0f };
	pendingMeleeSlashes_.push_back(event);
	saberCounterTimer_ = 0.0f;
	invincibleTimer_ = 0.28f;
	requestSlow_ = true;
}

void Player::EvolveById(const std::string& classId)
{
	const PlayerClassConfig* config = GetClassConfig(classId);
	if (!config) {
		return;
	}

	currentClassId_ = config->id;
	currentClass_ = config->type;
	bulletCoolTime = 0.0f;
	shootBarrelIndex_ = 0;
	shootGroupIndex_ = 0;
	weaponGroupCooldowns_.clear();
	
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

bool Player::CanEvolveTo(const std::string& classId) const
{
	const PlayerClassConfig* currentConfig = GetCurrentClassConfig();
	const PlayerClassConfig* targetConfig = GetClassConfig(classId);
	if (!currentConfig || !targetConfig || targetConfig->id == currentConfig->id) {
		return false;
	}

	const int currentRank = GetRankFromLevel(level_);
	if (currentRank < targetConfig->requiredRank) {
		return false;
	}

	if (currentConfig->id == "Basic") {
		return targetConfig->id == "Twin" ||
			targetConfig->id == "MachineGun" ||
			targetConfig->id == "Overseer";
	}

	// 上位ランクは個別の分岐定義がまだないため、少なくとも一段上の
	// ランクへ進む場合だけを進化として扱う。
	return targetConfig->requiredRank == currentConfig->requiredRank + 1;
}

bool Player::TryConfirmEvolutionById(const std::string& classId)
{
	if (!CanEvolveTo(classId)) {
		return false;
	}
	EvolveById(classId);
	evolutionConfirmedEvent_ = true;
	return true;
}

bool Player::ConsumeEvolutionConfirmed()
{
	const bool confirmed = evolutionConfirmedEvent_;
	evolutionConfirmedEvent_ = false;
	return confirmed;
}

bool Player::ConsumeEvolutionCancelled()
{
	const bool cancelled = evolutionCancelledEvent_;
	evolutionCancelledEvent_ = false;
	return cancelled;
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
		config.specialActionId = item.value("specialActionId", config.specialActionId);
		config.specialActionCooldownScale = (std::max)(0.05f, item.value("specialActionCooldownScale", config.specialActionCooldownScale));
		config.specialActionStaminaCost = (std::max)(0.0f, item.value("specialActionStaminaCost", item.value("specialActionStaminaRequirement", config.specialActionStaminaCost)));
		config.saberCounterWindow = (std::max)(0.01f, item.value("saberCounterWindow", config.saberCounterWindow));
		config.saberCounterDamageScale = (std::max)(0.0f, item.value("saberCounterDamageScale", config.saberCounterDamageScale));
		config.saberCounterRangeScale = (std::max)(0.1f, item.value("saberCounterRangeScale", config.saberCounterRangeScale));
		config.bodyShape = BodyShapeFromString(item.value("bodyShape", std::string(BodyShapeToString(config.bodyShape))));
		if (item.contains("bodyScale")) {
			config.bodyScale = ReadVector2(item["bodyScale"], config.bodyScale);
		}
		if (item.contains("bodyFillColor")) {
			config.bodyFillColor = ReadVector4(item["bodyFillColor"], config.bodyFillColor);
		}
		if (item.contains("bodyOutlineColor")) {
			config.bodyOutlineColor = ReadVector4(item["bodyOutlineColor"], config.bodyOutlineColor);
		}

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
				barrel.barrelShape = BarrelShapeFromString(barrelJson.value("barrelShape", std::string(BarrelShapeToString(barrel.barrelShape))));
				barrel.offset = ReadVector3(barrelJson.value("offset", nlohmann::json::array()), barrel.offset);
				barrel.scale = ReadVector3(barrelJson.value("scale", nlohmann::json::array()), barrel.scale);
				barrel.angleDeg = barrelJson.value("angleDeg", barrel.angleDeg);
				barrel.muzzleForward = barrelJson.value("muzzleForward", barrel.muzzleForward);
				barrel.fires = barrelJson.value("fires", barrel.fires);
				barrel.weaponType = WeaponTypeFromString(barrelJson.value("weaponType", std::string("Projectile")));
				barrel.damageScale = (std::max)(0.0f, barrelJson.value("damageScale", barrel.damageScale));
				barrel.projectileSpeedScale = (std::max)(0.01f, barrelJson.value("projectileSpeedScale", barrel.projectileSpeedScale));
				barrel.fireGroup = (std::max)(0, barrelJson.value("fireGroup", barrel.fireGroup));
				barrel.reloadScale = (std::max)(0.05f, barrelJson.value("reloadScale", barrel.reloadScale));
				barrel.recoilScale = (std::max)(0.0f, barrelJson.value("recoilScale", barrel.recoilScale));
				if (barrelJson.contains("barrelColor")) {
					barrel.barrelColor = ReadVector4(barrelJson["barrelColor"], barrel.barrelColor);
				}
				if (barrelJson.contains("outlineColor")) {
					barrel.outlineColor = ReadVector4(barrelJson["outlineColor"], barrel.outlineColor);
				}
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
				barrel.meleeComboResetTime = (std::max)(0.05f, barrelJson.value("meleeComboResetTime", barrel.meleeComboResetTime));
				barrel.meleeCombo1DamageScale = (std::max)(0.0f, barrelJson.value("meleeCombo1DamageScale", barrel.meleeCombo1DamageScale));
				barrel.meleeCombo2DamageScale = (std::max)(0.0f, barrelJson.value("meleeCombo2DamageScale", barrel.meleeCombo2DamageScale));
				barrel.meleeCombo3DamageScale = (std::max)(0.0f, barrelJson.value("meleeCombo3DamageScale", barrel.meleeCombo3DamageScale));
				barrel.meleeCombo1RangeScale = (std::max)(0.05f, barrelJson.value("meleeCombo1RangeScale", barrel.meleeCombo1RangeScale));
				barrel.meleeCombo2RangeScale = (std::max)(0.05f, barrelJson.value("meleeCombo2RangeScale", barrel.meleeCombo2RangeScale));
				barrel.meleeCombo3RangeScale = (std::max)(0.05f, barrelJson.value("meleeCombo3RangeScale", barrel.meleeCombo3RangeScale));
				barrel.meleeCombo1Windup = (std::max)(0.0f, barrelJson.value("meleeCombo1Windup", barrel.meleeCombo1Windup));
				barrel.meleeCombo2Windup = (std::max)(0.0f, barrelJson.value("meleeCombo2Windup", barrel.meleeCombo2Windup));
				barrel.meleeCombo3Windup = (std::max)(0.0f, barrelJson.value("meleeCombo3Windup", barrel.meleeCombo3Windup));
				barrel.meleeCombo1Recovery = (std::max)(0.0f, barrelJson.value("meleeCombo1Recovery", barrel.meleeCombo1Recovery));
				barrel.meleeCombo2Recovery = (std::max)(0.0f, barrelJson.value("meleeCombo2Recovery", barrel.meleeCombo2Recovery));
				barrel.meleeCombo3Recovery = (std::max)(0.0f, barrelJson.value("meleeCombo3Recovery", barrel.meleeCombo3Recovery));
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
	auto makeBarrel = [](Vector3 offset, Vector3 scale, float angleDeg) {
		WeaponMountConfig barrel{};
		barrel.model = "gunBarrel.obj";
		barrel.offset = offset;
		barrel.scale = scale;
		barrel.angleDeg = angleDeg;
		barrel.muzzleForward = 0.95f;
		barrel.fires = true;
		return barrel;
	};
	config.barrels = {
		makeBarrel({ 0.72f, 0.0f, 0.0f }, { 1.25f, 0.24f, 0.24f }, 0.0f)
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
			makeBarrel({ 0.72f, -0.34f, 0.0f }, { 1.25f, 0.24f, 0.24f }, 0.0f),
			makeBarrel({ 0.72f,  0.34f, 0.0f }, { 1.25f, 0.24f, 0.24f }, 0.0f)
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
		item["specialActionId"] = config->specialActionId;
		item["specialActionCooldownScale"] = config->specialActionCooldownScale;
		item["specialActionStaminaCost"] = config->specialActionStaminaCost;
		item["saberCounterWindow"] = config->saberCounterWindow;
		item["saberCounterDamageScale"] = config->saberCounterDamageScale;
		item["saberCounterRangeScale"] = config->saberCounterRangeScale;
		item["bodyShape"] = BodyShapeToString(config->bodyShape);
		item["bodyScale"] = nlohmann::json::array({ config->bodyScale.x, config->bodyScale.y });
		item["bodyFillColor"] = Vector4ToJson(config->bodyFillColor);
		item["bodyOutlineColor"] = Vector4ToJson(config->bodyOutlineColor);
		item["weaponMounts"] = nlohmann::json::array();
		for (const WeaponMountConfig& barrel : config->barrels) {
			nlohmann::json barrelJson;
			barrelJson["model"] = barrel.model;
			barrelJson["barrelShape"] = BarrelShapeToString(barrel.barrelShape);
			barrelJson["offset"] = Vector3ToJson(barrel.offset);
			barrelJson["scale"] = Vector3ToJson(barrel.scale);
			barrelJson["angleDeg"] = barrel.angleDeg;
			barrelJson["muzzleForward"] = barrel.muzzleForward;
			barrelJson["fires"] = barrel.fires;
			barrelJson["weaponType"] = WeaponTypeToString(barrel.weaponType);
			barrelJson["damageScale"] = barrel.damageScale;
			barrelJson["projectileSpeedScale"] = barrel.projectileSpeedScale;
			barrelJson["fireGroup"] = barrel.fireGroup;
			barrelJson["reloadScale"] = barrel.reloadScale;
			barrelJson["recoilScale"] = barrel.recoilScale;
			barrelJson["barrelColor"] = Vector4ToJson(barrel.barrelColor);
			barrelJson["outlineColor"] = Vector4ToJson(barrel.outlineColor);
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
			barrelJson["meleeComboResetTime"] = barrel.meleeComboResetTime;
			barrelJson["meleeCombo1DamageScale"] = barrel.meleeCombo1DamageScale;
			barrelJson["meleeCombo2DamageScale"] = barrel.meleeCombo2DamageScale;
			barrelJson["meleeCombo3DamageScale"] = barrel.meleeCombo3DamageScale;
			barrelJson["meleeCombo1RangeScale"] = barrel.meleeCombo1RangeScale;
			barrelJson["meleeCombo2RangeScale"] = barrel.meleeCombo2RangeScale;
			barrelJson["meleeCombo3RangeScale"] = barrel.meleeCombo3RangeScale;
			barrelJson["meleeCombo1Windup"] = barrel.meleeCombo1Windup;
			barrelJson["meleeCombo2Windup"] = barrel.meleeCombo2Windup;
			barrelJson["meleeCombo3Windup"] = barrel.meleeCombo3Windup;
			barrelJson["meleeCombo1Recovery"] = barrel.meleeCombo1Recovery;
			barrelJson["meleeCombo2Recovery"] = barrel.meleeCombo2Recovery;
			barrelJson["meleeCombo3Recovery"] = barrel.meleeCombo3Recovery;
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

bool Player::GetTankButtonVisualData(const std::string& classId, TankButtonVisualData& output) const
{
	const PlayerClassConfig* config = GetClassConfig(classId);
	if (!config) {
		return false;
	}
	output = {};
	output.classId = config->id;
	output.rank = (std::clamp)(config->requiredRank, 1, 4);
	output.bodyScale = config->bodyScale;
	output.bodyFillColor = config->bodyFillColor;
	output.bodyOutlineColor = config->bodyOutlineColor;
	output.weaponMounts = config->barrels;
	output.usesDrone = config->usesDrone;
	switch (config->bodyShape) {
	case BodyShape::Box:
		output.bodyShape = TankButtonBodyShape::Box;
		break;
	case BodyShape::Triangle:
		output.bodyShape = TankButtonBodyShape::Triangle;
		break;
	case BodyShape::Pentagon:
		output.bodyShape = TankButtonBodyShape::Pentagon;
		break;
	case BodyShape::Circle:
	default:
		output.bodyShape = TankButtonBodyShape::Circle;
		break;
	}
	if (classId == "Twin") {
		output.hiraganaName = "ついん";
	} else if (classId == "MachineGun") {
		output.hiraganaName = "ましんがん";
	} else if (classId == "Overseer") {
		output.hiraganaName = "おーばーしあ";
	} else if (classId == "Basic") {
		output.hiraganaName = "べーしっく";
	} else {
		output.hiraganaName = config->displayName;
	}
	return true;
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
		if (bulletCoolTime > 0.0f) {
			return false;
		}
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

	std::vector<int> fireGroups;
	for (size_t index : fireIndices) {
		const int group = (std::max)(0, config.barrels[index].fireGroup);
		if (std::find(fireGroups.begin(), fireGroups.end(), group) == fireGroups.end()) {
			fireGroups.push_back(group);
		}
	}
	std::sort(fireGroups.begin(), fireGroups.end());
	const bool usesGroupCooldowns = config.alternateBarrels && !config.fireAllBarrels && fireGroups.size() > 1;
	int selectedGroupSlot = -1;

	if (config.alternateBarrels && !config.fireAllBarrels) {
		if (usesGroupCooldowns) {
			if (weaponGroupCooldowns_.size() != fireGroups.size()) {
				weaponGroupCooldowns_.assign(fireGroups.size(), 0.0f);
				shootGroupIndex_ = 0;
			}
			for (size_t attempt = 0; attempt < fireGroups.size(); ++attempt) {
				const size_t slot = (static_cast<size_t>(shootGroupIndex_) + attempt) % fireGroups.size();
				if (weaponGroupCooldowns_[slot] <= 0.0f) {
					selectedGroupSlot = static_cast<int>(slot);
					break;
				}
			}
			if (selectedGroupSlot < 0) {
				bulletCoolTime = 0.0f;
				return false;
			}
			const int selectedGroup = fireGroups[static_cast<size_t>(selectedGroupSlot)];
			std::vector<size_t> groupIndices;
			for (size_t index : fireIndices) {
				if ((std::max)(0, config.barrels[index].fireGroup) == selectedGroup) {
					groupIndices.push_back(index);
				}
			}
			fireIndices = groupIndices;
			shootGroupIndex_ = (selectedGroupSlot + 1) % static_cast<int>(fireGroups.size());
		} else {
			if (bulletCoolTime > 0.0f) {
				return false;
			}
			const size_t selectableCount = fireIndices.size();
			const size_t index = fireIndices[shootBarrelIndex_ % selectableCount];
			fireIndices = { index };
			shootBarrelIndex_ = static_cast<int>((shootBarrelIndex_ + 1) % selectableCount);
		}
	} else if (bulletCoolTime > 0.0f) {
		return false;
	}

	const Vector3 forward = Length(dir_) > 0.0001f ? Normalize(dir_) : Vector3{ 1.0f, 0.0f, 0.0f };
	const Vector3 right = { -forward.y, forward.x, 0.0f };
	bool firesMelee = false;
	float meleeComboResetTime = 0.90f;
	for (size_t index : fireIndices) {
		if (index < config.barrels.size() && config.barrels[index].weaponType == WeaponType::Melee) {
			firesMelee = true;
			meleeComboResetTime = config.barrels[index].meleeComboResetTime;
			break;
		}
	}
	if (firesMelee && meleeComboTimer_ <= 0.0f) {
		meleeComboStep_ = 0;
	}
	const int meleeComboStepForShot = (std::clamp)(meleeComboStep_, 0, 2);
	float meleeActionDuration = 0.0f;
	if (firesMelee) {
		meleeComboStep_ = (meleeComboStepForShot + 1) % 3;
		meleeComboTimer_ = (std::max)(0.05f, meleeComboResetTime);
	}

	Vector3 combinedRecoil{};
	float firedReloadScale = 1.0f;
	for (size_t index : fireIndices) {
		const WeaponMountConfig& barrelConfig = config.barrels[index];
		const Vector3 fireDir = RotateDirection(forward, barrelConfig.angleDeg);
		combinedRecoil = combinedRecoil + fireDir * (-(std::max)(0.0f, barrelConfig.recoilScale));
		firedReloadScale = (std::max)(firedReloadScale, barrelConfig.reloadScale);
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
			const float meleeDamageScale =
				meleeComboStepForShot == 0 ? barrelConfig.meleeCombo1DamageScale :
				meleeComboStepForShot == 1 ? barrelConfig.meleeCombo2DamageScale :
				barrelConfig.meleeCombo3DamageScale;
			const float meleeRangeScale =
				meleeComboStepForShot == 0 ? barrelConfig.meleeCombo1RangeScale :
				meleeComboStepForShot == 1 ? barrelConfig.meleeCombo2RangeScale :
				barrelConfig.meleeCombo3RangeScale;
			const float meleeWindup =
				meleeComboStepForShot == 0 ? barrelConfig.meleeCombo1Windup :
				meleeComboStepForShot == 1 ? barrelConfig.meleeCombo2Windup :
				barrelConfig.meleeCombo3Windup;
			const float meleeRecovery =
				meleeComboStepForShot == 0 ? barrelConfig.meleeCombo1Recovery :
				meleeComboStepForShot == 1 ? barrelConfig.meleeCombo2Recovery :
				barrelConfig.meleeCombo3Recovery;
			MeleeSlashEvent event{};
			event.origin = mountBase;
			event.direction = fireDir;
			event.range = barrelConfig.meleeRange * meleeRangeScale;
			event.arcDeg = barrelConfig.meleeArcDeg;
			event.width = barrelConfig.meleeWidth;
			event.duration = barrelConfig.meleeDuration;
			event.windupDuration = meleeWindup;
			event.recoveryDuration = meleeRecovery;
			event.damage = static_cast<uint32_t>((std::max)(1.0f, static_cast<float>(mountParam.damage) * meleeDamageScale));
			event.color = barrelConfig.effectColor;
			event.comboStep = meleeComboStepForShot;
			pendingMeleeSlashes_.push_back(event);
			meleeActionDuration = (std::max)(meleeActionDuration, meleeWindup + barrelConfig.meleeDuration + meleeRecovery);
		} else {
			mountParam.bulletSpeed *= barrelConfig.projectileSpeedScale;
			attackController_.FireFromMuzzle(muzzle, fireDir, mountParam, BulletOwner::kPlayer);
			SpawnCasing();
		}
		if (index < barrels_.size()) {
			barrels_[index].recoilOffset = 0.22f;
		}
	}

	const float reloadTime = (std::max)(baseReload * config.reloadScale * firedReloadScale, meleeActionDuration);
	if (usesGroupCooldowns && selectedGroupSlot >= 0 && static_cast<size_t>(selectedGroupSlot) < weaponGroupCooldowns_.size()) {
		weaponGroupCooldowns_[static_cast<size_t>(selectedGroupSlot)] = reloadTime;
		bulletCoolTime = 0.0f;
	} else {
		bulletCoolTime = reloadTime;
	}
	const float combinedRecoilLength = Length(combinedRecoil);
	if (combinedRecoilLength > 0.0001f) {
		recoilDir = combinedRecoil * (1.0f / combinedRecoilLength);
		recoilPower = config.recoilPower * combinedRecoilLength;
	} else {
		recoilDir = Normalize(dir_) * -1.0f;
		recoilPower = 0.0f;
	}
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
	*upgradeHudBatchMaterialData_ = MakeDefaultMaterial();
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

void Player::AppendGameplayNeonTextLabels(std::vector<TextLabel*>& labels) const
{
	if (isChangeMode) {
		return;
	}
	if (upgradeHudDrawBottomText_ && upgradeHudLevelLabel_) {
		labels.push_back(upgradeHudLevelLabel_.get());
	}
	if (upgradeHudListVisibility_ > 0.01f && upgradeHudDrawListText_) {
		if (upgradeHudTitleLabel_) {
			labels.push_back(upgradeHudTitleLabel_.get());
		}
		if (upgradeHudPointLabel_) {
			labels.push_back(upgradeHudPointLabel_.get());
		}
	}
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

bool Player::ShouldUseStaticEvolutionPrototype() const
{
	if (!evolutionUiStyle_.enabled) {
		return false;
	}
	const PlayerClassConfig* current = GetCurrentClassConfig();
	if (!current || current->requiredRank >= 4) {
		return false;
	}
	const int targetRank = current->requiredRank + 1;
	for (const std::string& id : classOrder_) {
		const PlayerClassConfig* config = GetClassConfig(id);
		if (config && config->requiredRank == targetRank) {
			return true;
		}
	}
	return false;
}

void Player::RefreshStaticEvolutionCandidates()
{
	staticEvolutionCandidateCount_ = 0;
	for (std::string& id : staticEvolutionCandidateIds_) {
		id.clear();
	}
	const PlayerClassConfig* current = GetCurrentClassConfig();
	if (!current) {
		return;
	}
	const int targetRank = current->requiredRank + 1;
	for (const std::string& id : classOrder_) {
		const PlayerClassConfig* config = GetClassConfig(id);
		if (!config || config->requiredRank != targetRank || config->id == current->id) {
			continue;
		}
		if (staticEvolutionCandidateCount_ >= staticEvolutionCandidateIds_.size()) {
			break;
		}
		staticEvolutionCandidateIds_[staticEvolutionCandidateCount_++] = config->id;
	}
	if (staticEvolutionCandidateCount_ == 0) {
		evolutionUiStyle_.fixedSelectedCandidate = 0;
	} else {
		evolutionUiStyle_.fixedSelectedCandidate = (std::clamp)(
			evolutionUiStyle_.fixedSelectedCandidate,
			0,
			static_cast<int>(staticEvolutionCandidateCount_ - 1));
	}
}

std::string Player::GetEvolutionClassName(const std::string& classId) const
{
	static const std::unordered_map<std::string, std::string> names = {
		{ "Basic", "BASIC" },
		{ "Basic_Copy", "SWORD" },
		{ "Twin", "TWIN" },
		{ "MachineGun", "MACHINE GUN" },
		{ "Overseer", "OVERSEER" },
		{ "Triple", "TRIPLE" },
		{ "Triple_Copy", "TRIPLE GUN" },
		{ "Assassin", "ASSASSIN" },
		{ "Bounder", "BOUNDER" },
		{ "Ninja", "NINJA" },
		{ "Smasher", "SMASHER" },
		{ "Summoner", "SUMMONER" },
	};
	if (const auto it = names.find(classId); it != names.end()) {
		return it->second;
	}
	std::string result = classId;
	for (char& c : result) {
		if (c == '_') {
			c = ' ';
		} else {
			c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
		}
	}
	return result;
}

std::string Player::GetEvolutionShortRole(const PlayerClassConfig& config) const
{
	if (config.usesDrone || config.id == "Summoner") return "DRONE CONTROL";
	if (config.reflect) return "RICOCHET";
	if (config.id == "Ninja" || config.id == "Assassin") return "PRECISION";
	if (config.id == "Smasher") return "IMPACT";
	if (config.id == "Twin") return "DUAL FIRE";
	if (config.id == "MachineGun") return "SUPPRESSION";
	if (config.barrels.size() >= 3) return "MULTI BARREL";
	return "ADVANCED";
}

std::string Player::GetEvolutionRole(const PlayerClassConfig& config) const
{
	if (config.usesDrone || config.id == "Summoner") return "役割: ドローンを展開する支援制圧型";
	if (config.reflect) return "役割: 反射弾で空間を制圧する技巧型";
	if (config.id == "Ninja" || config.id == "Assassin") return "役割: 高速攻撃を狙う精密射撃型";
	if (config.id == "Smasher") return "役割: 高い衝撃力で押し切る近距離型";
	if (config.id == "MachineGun") return "役割: 弾幕で押す近中距離制圧型";
	if (config.barrels.size() >= 2) return "役割: 複数砲身を活かす連続射撃型";
	return "役割: 基礎性能を強化した万能型";
}

std::array<std::string, 3> Player::GetEvolutionDeltas(
	const PlayerClassConfig& current,
	const PlayerClassConfig& target) const
{
	char reload[64]{};
	const float currentReload = (std::max)(0.0001f, current.reloadScale);
	const int reloadPercent = static_cast<int>(std::round((target.reloadScale / currentReload - 1.0f) * 100.0f));
	std::snprintf(reload, sizeof(reload), "発射間隔  %+d%%", reloadPercent);
	char spread[64]{};
	std::snprintf(
		spread,
		sizeof(spread),
		"拡散角  %.0f° → %.0f°",
		current.spreadAngleDeg,
		target.spreadAngleDeg);
	return {
		"砲身  " + std::to_string(current.barrels.size()) + " → " + std::to_string(target.barrels.size()),
		std::string(reload),
		std::string(spread)
	};
}

std::string Player::GetEvolutionAbility(const PlayerClassConfig& config) const
{
	if (config.usesDrone || config.id == "Summoner") {
		return "固有能力: 最大" + std::to_string(config.maxDrones) + "機のドローンを展開";
	}
	if (config.reflect) return "固有能力: 発射した弾が障害物で反射";
	if (config.penetrate) return "固有能力: 敵を貫通する弾を発射";
	if (config.fireAllBarrels) return "固有能力: 全砲身から同時射撃";
	if (config.alternateBarrels) return "固有能力: 複数の砲身から交互に射撃";
	if (config.bulletCount > 1) return "固有能力: 1回の射撃で複数弾を発射";
	if (config.barrels.size() >= 2) return "固有能力: 複数砲身による多方向射撃";
	return "固有能力: 機体固有の武装構成";
}

float Player::GetEvolutionRenderScale() const
{
	const float virtualWidth = (std::max)(1.0f, evolutionUiStyle_.virtualResolution.x);
	const float virtualHeight = (std::max)(1.0f, evolutionUiStyle_.virtualResolution.y);
	const float clientWidth = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
	const float clientHeight = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
	return (std::min)(
		clientWidth / virtualWidth,
		clientHeight / virtualHeight);
}

Vector2 Player::GetEvolutionRenderOffset() const
{
	const float scale = GetEvolutionRenderScale();
	const float clientWidth = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
	const float clientHeight = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
	return {
		(clientWidth - evolutionUiStyle_.virtualResolution.x * scale) * 0.5f,
		(clientHeight - evolutionUiStyle_.virtualResolution.y * scale) * 0.5f
	};
}

Vector2 Player::EvolutionAnchorToVirtual(const Vector2& normalizedAnchor) const
{
	const float safe = (std::clamp)(
		evolutionUiStyle_.safeMargin,
		0.0f,
		(std::min)(evolutionUiStyle_.virtualResolution.x, evolutionUiStyle_.virtualResolution.y) * 0.45f);
	const Vector2 usable = {
		(std::max)(1.0f, evolutionUiStyle_.virtualResolution.x - safe * 2.0f),
		(std::max)(1.0f, evolutionUiStyle_.virtualResolution.y - safe * 2.0f)
	};
	return {
		safe + (std::clamp)(normalizedAnchor.x, 0.0f, 1.0f) * usable.x,
		safe + (std::clamp)(normalizedAnchor.y, 0.0f, 1.0f) * usable.y
	};
}

Vector2 Player::EvolutionVirtualToRender(const Vector2& virtualPosition) const
{
	const float scale = GetEvolutionRenderScale();
	const Vector2 offset = GetEvolutionRenderOffset();
	return { offset.x + virtualPosition.x * scale, offset.y + virtualPosition.y * scale };
}

Vector2 Player::EvolutionClientToVirtual(const Vector2& clientPosition) const
{
	const float clientWidth = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
	const float clientHeight = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
	const float virtualWidth = (std::max)(1.0f, evolutionUiStyle_.virtualResolution.x);
	const float virtualHeight = (std::max)(1.0f, evolutionUiStyle_.virtualResolution.y);
	const float scale = (std::max)(0.0001f, (std::min)(clientWidth / virtualWidth, clientHeight / virtualHeight));
	const Vector2 offset = {
		(clientWidth - virtualWidth * scale) * 0.5f,
		(clientHeight - virtualHeight * scale) * 0.5f
	};
	return {
		(clientPosition.x - offset.x) / scale,
		(clientPosition.y - offset.y) / scale
	};
}

void Player::InitializeStaticEvolutionPrototype()
{
	SpriteCommon* spriteCommon = SpriteCommon::GetInstance();
	auto makeSprite = [spriteCommon](const std::string& texture, const Vector2& anchor) {
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon, texture);
		sprite->SetAnchorPoint(anchor);
		return sprite;
	};

	staticEvolutionBackdropSprite_ = makeSprite("resources/white512x512.png", { 0.0f, 0.0f });
	staticEvolutionDetailPanelSprite_ = makeSprite("resources/white512x512.png", { 0.5f, 0.5f });
	staticEvolutionConfirmButtonSprite_ = makeSprite("resources/white512x512.png", { 0.5f, 0.5f });
	staticEvolutionBranchGlowSprite_ = makeSprite("resources/white512x512.png", { 0.5f, 0.5f });
	staticEvolutionBranchCoreSprite_ = makeSprite("resources/white512x512.png", { 0.5f, 0.5f });
	for (auto& line : staticEvolutionConfirmOutlineSprites_) {
		line = makeSprite("resources/white512x512.png", { 0.0f, 0.5f });
	}
	for (auto& nodePanels : staticEvolutionNodePanelSprites_) {
		for (auto& panel : nodePanels) {
			panel = makeSprite("resources/white512x512.png", { 0.5f, 0.5f });
		}
	}
	for (auto& nodeLines : staticEvolutionNodeFrameSprites_) {
		for (auto& line : nodeLines) {
			line = makeSprite("resources/white512x512.png", { 0.0f, 0.5f });
		}
	}
	for (auto& nodeLines : staticEvolutionSilhouetteSprites_) {
		for (auto& line : nodeLines) {
			line = makeSprite("resources/white512x512.png", { 0.0f, 0.5f });
		}
	}
	for (auto& line : staticEvolutionCircuitSprites_) {
		line = makeSprite("resources/white512x512.png", { 0.0f, 0.5f });
	}

	tankButtonUiStyle_ = std::make_unique<TankButtonUiStyle>();
	LoadTankButtonUiStyle(*tankButtonUiStyle_);
	for (size_t classIndex = 0; classIndex < staticEvolutionTankButtons_.size(); ++classIndex) {
		staticEvolutionTankButtons_[classIndex] = std::make_unique<TankButtonUI>();
		staticEvolutionTankButtons_[classIndex]->Initialize(spriteCommon);
	}
	staticEvolutionButtonBloomEffect_ = std::make_unique<ObjectPostEffect>();
	staticEvolutionButtonBloomEffect_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(),
		Object3dCommon::GetInstance()->GetSrvManager(),
		nullptr,
		1.0f);
	staticEvolutionTextEffect_ = std::make_unique<NeonTextEffect>();
	staticEvolutionTextEffect_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(),
		Object3dCommon::GetInstance()->GetSrvManager());
	staticEvolutionTextEffect_->SetStyle(evolutionUiStyle_.neonText);

	UpdateStaticEvolutionPrototype();
}

void Player::UpdateStaticEvolutionPrototype()
{
	if (!staticEvolutionBackdropSprite_) {
		return;
	}

	RefreshStaticEvolutionCandidates();
	if (staticEvolutionCandidateCount_ == 0) {
		return;
	}
	if (input_ && input_->IsTrigger(input_->GetKey()[DIK_ESCAPE], input_->GetPreKey()[DIK_ESCAPE])) {
		isChangeMode = false;
		evolutionCancelledEvent_ = true;
		return;
	}

	const size_t activeNodeCount = staticEvolutionCandidateCount_ + 1;
	const float renderScale = GetEvolutionRenderScale();
	const Vector2 mouseVirtual = EvolutionClientToVirtual(mousePosition_);
	const auto& activeAnchors = evolutionUiStyle_.radialLayout
		? evolutionUiStyle_.radialNodeAnchors
		: evolutionUiStyle_.nodeAnchors;

	staticEvolutionNodeCentersVirtual_[0] = EvolutionAnchorToVirtual(activeAnchors[0]);
	for (size_t candidateIndex = 0; candidateIndex < staticEvolutionCandidateCount_; ++candidateIndex) {
		Vector2 anchor{};
		if (staticEvolutionCandidateCount_ == 3) {
			anchor = activeAnchors[candidateIndex + 1];
		} else if (evolutionUiStyle_.radialLayout) {
			const float angle = -1.5707963268f +
				static_cast<float>(candidateIndex) * 6.2831853072f /
				static_cast<float>(staticEvolutionCandidateCount_);
			anchor = {
				0.50f + std::cos(angle) * 0.30f,
				0.40f + std::sin(angle) * 0.27f
			};
		} else {
			const float y = staticEvolutionCandidateCount_ == 1
				? 0.43f
				: 0.13f + static_cast<float>(candidateIndex) *
					(0.54f / static_cast<float>(staticEvolutionCandidateCount_ - 1));
			anchor = { activeAnchors[1].x, y };
		}
		staticEvolutionNodeCentersVirtual_[candidateIndex + 1] = EvolutionAnchorToVirtual(anchor);
	}

	for (size_t i = 0; i < activeNodeCount; ++i) {
		const Vector2 baseSize = i == 0
			? evolutionUiStyle_.currentNodeSize
			: evolutionUiStyle_.candidateNodeSize;
		staticEvolutionNodeHitSizesVirtual_[i] = { baseSize.x + 16.0f, baseSize.y + 16.0f };
	}

	staticEvolutionHoveredNode_ = -1;
	for (size_t i = 1; i < activeNodeCount; ++i) {
		const Vector2 center = staticEvolutionNodeCentersVirtual_[i];
		const Vector2 hitSize = staticEvolutionNodeHitSizesVirtual_[i];
		if (mouseVirtual.x >= center.x - hitSize.x * 0.5f &&
			mouseVirtual.x <= center.x + hitSize.x * 0.5f &&
			mouseVirtual.y >= center.y - hitSize.y * 0.5f &&
			mouseVirtual.y <= center.y + hitSize.y * 0.5f) {
			staticEvolutionHoveredNode_ = static_cast<int>(i);
			break;
		}
	}
	const bool primaryTriggered = input_ && input_->IsTrigger(
		input_->GetMouseState().rgbButtons[0],
		input_->GetPreMouseState().rgbButtons[0]);
	if (primaryTriggered && staticEvolutionHoveredNode_ > 0) {
		evolutionUiStyle_.fixedSelectedCandidate = staticEvolutionHoveredNode_ - 1;
	}

	staticEvolutionBackdropSprite_->SetPosition({ 0.0f, 0.0f });
	staticEvolutionBackdropSprite_->SetSize({
		static_cast<float>(WinApp::GetInstance()->GetClientWidth()),
		static_cast<float>(WinApp::GetInstance()->GetClientHeight())
	});
	staticEvolutionBackdropSprite_->SetColor({ 0.005f, 0.012f, 0.025f, evolutionUiStyle_.backgroundDimOpacity });
	staticEvolutionBackdropSprite_->Update();

	const int currentRank = GetRankFromLevel(level_);
	for (size_t i = 0; i < activeNodeCount; ++i) {
		const bool selected = i > 0 && i - 1 == evolutionUiStyle_.fixedSelectedCandidate;
		const bool hovered = i == staticEvolutionHoveredNode_;
		const std::string& nodeClassId = i == 0
			? currentClassId_
			: staticEvolutionCandidateIds_[i - 1];
		const PlayerClassConfig* candidateConfig = i > 0 ? GetClassConfig(nodeClassId) : nullptr;
		const PlayerClassConfig* nodeConfig = GetClassConfig(nodeClassId);
		const bool locked = candidateConfig && currentRank < candidateConfig->requiredRank;
		float stateScale = evolutionUiStyle_.normalScale;
		if (selected) {
			stateScale = evolutionUiStyle_.selectedScale;
		} else if (hovered) {
			stateScale = evolutionUiStyle_.hoverScale;
		}
		const Vector2 baseSize = i == 0
			? evolutionUiStyle_.currentNodeSize
			: evolutionUiStyle_.candidateNodeSize;
		const Vector2 drawSize = {
			(std::max)(1.0f, baseSize.x * stateScale),
			(std::max)(1.0f, baseSize.y * stateScale)
		};
		staticEvolutionNodeDrawSizesVirtual_[i] = drawSize;

		const float cut = (std::clamp)(evolutionUiStyle_.nodeCornerCut, 0.0f, drawSize.y * 0.30f);
		Vector4 panelColor = evolutionUiStyle_.panelColor;
		panelColor.w = locked ? 0.97f : 0.91f;
		const std::array<Vector2, 3> panelPositions = {{
			staticEvolutionNodeCentersVirtual_[i],
			{ staticEvolutionNodeCentersVirtual_[i].x, staticEvolutionNodeCentersVirtual_[i].y - drawSize.y * 0.5f + cut * 0.5f },
			{ staticEvolutionNodeCentersVirtual_[i].x, staticEvolutionNodeCentersVirtual_[i].y + drawSize.y * 0.5f - cut * 0.5f }
		}};
		const std::array<Vector2, 3> panelSizes = {{
			{ drawSize.x, (std::max)(1.0f, drawSize.y - cut * 2.0f) },
			{ (std::max)(1.0f, drawSize.x - cut * 2.0f), cut },
			{ (std::max)(1.0f, drawSize.x - cut * 2.0f), cut }
		}};
		for (int panelIndex = 0; panelIndex < 3; ++panelIndex) {
			Sprite* panel = staticEvolutionNodePanelSprites_[i][panelIndex].get();
			panel->SetPosition(EvolutionVirtualToRender(panelPositions[panelIndex]));
			panel->SetSize({ panelSizes[panelIndex].x * renderScale, panelSizes[panelIndex].y * renderScale });
			panel->SetColor(panelColor);
			panel->Update();
		}

		if (staticEvolutionTankButtons_[i] && tankButtonUiStyle_) {
			TankButtonVisualData visualData{};
			if (GetTankButtonVisualData(nodeClassId, visualData)) {
				staticEvolutionTankButtons_[i]->SetVisualData(visualData);
			}
			TankButtonUiStyle renderedButtonStyle = *tankButtonUiStyle_;
			renderedButtonStyle.buttonWidth = drawSize.x * renderScale;
			renderedButtonStyle.buttonHeight = drawSize.y * renderScale;
			renderedButtonStyle.cornerRadius = evolutionUiStyle_.nodeCornerCut * renderScale;
			renderedButtonStyle.borderWidth = evolutionUiStyle_.nodeOutlineWidth * renderScale;
			renderedButtonStyle.glowWidth = evolutionUiStyle_.nodeOutlineGlowWidth * renderScale;
			renderedButtonStyle.iconOffsetY *= renderScale;
			renderedButtonStyle.labelOffsetY *= renderScale;
			renderedButtonStyle.labelFontSize *= renderScale;
			renderedButtonStyle.labelOutlineWidth *= renderScale;
			TankButtonState buttonState = TankButtonState::Normal;
			if (locked) {
				buttonState = TankButtonState::Locked;
			} else if (selected) {
				buttonState = TankButtonState::Selected;
			} else if (hovered) {
				buttonState = TankButtonState::Hover;
			}
			staticEvolutionTankButtons_[i]->SetRank(nodeConfig ? nodeConfig->requiredRank : 1);
			staticEvolutionTankButtons_[i]->SetState(buttonState);
			staticEvolutionTankButtons_[i]->Update(
				EvolutionVirtualToRender(staticEvolutionNodeCentersVirtual_[i]),
				renderedButtonStyle);
		}
	}
	if (staticEvolutionButtonBloomEffect_ && tankButtonUiStyle_) {
		BloomParam bloomParam = staticEvolutionButtonBloomEffect_->GetParam();
		bloomParam.threshold = 0.0f;
		bloomParam.intensity = 1.10f + tankButtonUiStyle_->bloomBoost * 2.5f;
		bloomParam.outlineWidth = 0.0f;
		staticEvolutionButtonBloomEffect_->SetParam(bloomParam);
		staticEvolutionButtonBloomEffect_->Update(0.0f);
	}
	if (staticEvolutionTextEffect_) {
		staticEvolutionTextEffect_->SetStyle(evolutionUiStyle_.neonText);
	}

	const Vector2 panelCenterVirtual = EvolutionAnchorToVirtual(evolutionUiStyle_.detailPanelAnchor);
	staticEvolutionDetailPanelSprite_->SetPosition(EvolutionVirtualToRender(panelCenterVirtual));
	staticEvolutionDetailPanelSprite_->SetSize({
		evolutionUiStyle_.detailPanelSize.x * renderScale,
		evolutionUiStyle_.detailPanelSize.y * renderScale
	});
	staticEvolutionDetailPanelSprite_->SetColor(evolutionUiStyle_.panelColor);
	staticEvolutionDetailPanelSprite_->Update();

	const Vector2 buttonCenterVirtual = {
		panelCenterVirtual.x + evolutionUiStyle_.detailPanelSize.x * 0.5f - evolutionUiStyle_.confirmButtonSize.x * 0.5f - 18.0f,
		panelCenterVirtual.y - evolutionUiStyle_.detailPanelSize.y * 0.5f + 32.0f
	};
	staticEvolutionConfirmHovered_ =
		mouseVirtual.x >= buttonCenterVirtual.x - evolutionUiStyle_.confirmButtonSize.x * 0.5f &&
		mouseVirtual.x <= buttonCenterVirtual.x + evolutionUiStyle_.confirmButtonSize.x * 0.5f &&
		mouseVirtual.y >= buttonCenterVirtual.y - evolutionUiStyle_.confirmButtonSize.y * 0.5f &&
		mouseVirtual.y <= buttonCenterVirtual.y + evolutionUiStyle_.confirmButtonSize.y * 0.5f;
	const std::string& selectedClassId = staticEvolutionCandidateIds_[
		static_cast<size_t>(evolutionUiStyle_.fixedSelectedCandidate)];
	const bool canConfirm = CanEvolveTo(selectedClassId);
	staticEvolutionConfirmButtonSprite_->SetPosition(EvolutionVirtualToRender(buttonCenterVirtual));
	staticEvolutionConfirmButtonSprite_->SetSize({
		evolutionUiStyle_.confirmButtonSize.x * renderScale,
		evolutionUiStyle_.confirmButtonSize.y * renderScale
	});
	Vector4 buttonColor = evolutionUiStyle_.panelColor;
	buttonColor.x *= 0.72f;
	buttonColor.y *= 0.72f;
	buttonColor.z *= 0.72f;
	buttonColor.w = 0.98f;
	if (!canConfirm) {
		buttonColor = {
			buttonColor.x * evolutionUiStyle_.lockedColor.x,
			buttonColor.y * evolutionUiStyle_.lockedColor.y,
			buttonColor.z * evolutionUiStyle_.lockedColor.z,
			buttonColor.w * evolutionUiStyle_.lockedColor.w
		};
	}
	staticEvolutionConfirmButtonSprite_->SetColor(buttonColor);
	staticEvolutionConfirmButtonSprite_->Update();

	const float buttonCut = 7.0f;
	const float halfButtonW = evolutionUiStyle_.confirmButtonSize.x * 0.5f;
	const float halfButtonH = evolutionUiStyle_.confirmButtonSize.y * 0.5f;
	const std::array<Vector2, 8> buttonPoints = {{
		{ buttonCenterVirtual.x - halfButtonW + buttonCut, buttonCenterVirtual.y - halfButtonH },
		{ buttonCenterVirtual.x + halfButtonW - buttonCut, buttonCenterVirtual.y - halfButtonH },
		{ buttonCenterVirtual.x + halfButtonW, buttonCenterVirtual.y - halfButtonH + buttonCut },
		{ buttonCenterVirtual.x + halfButtonW, buttonCenterVirtual.y + halfButtonH - buttonCut },
		{ buttonCenterVirtual.x + halfButtonW - buttonCut, buttonCenterVirtual.y + halfButtonH },
		{ buttonCenterVirtual.x - halfButtonW + buttonCut, buttonCenterVirtual.y + halfButtonH },
		{ buttonCenterVirtual.x - halfButtonW, buttonCenterVirtual.y + halfButtonH - buttonCut },
		{ buttonCenterVirtual.x - halfButtonW, buttonCenterVirtual.y - halfButtonH + buttonCut }
	}};
	for (int i = 0; i < 8; ++i) {
		const Vector2 a = EvolutionVirtualToRender(buttonPoints[i]);
		const Vector2 b = EvolutionVirtualToRender(buttonPoints[(i + 1) % 8]);
		const float dx = b.x - a.x;
		const float dy = b.y - a.y;
		Sprite* line = staticEvolutionConfirmOutlineSprites_[i].get();
		line->SetPosition(a);
		line->SetRotation(std::atan2(dy, dx));
		line->SetSize({ std::sqrt(dx * dx + dy * dy), (std::max)(1.0f, 1.6f * renderScale) });
		Vector4 outlineColor = !canConfirm
			? evolutionUiStyle_.lockedColor
			: staticEvolutionConfirmHovered_ ? evolutionUiStyle_.hoverColor : evolutionUiStyle_.availableColor;
		outlineColor.w = 0.90f;
		line->SetColor(outlineColor);
		line->Update();
	}

	UpdateStaticEvolutionNodeFrames();
	UpdateStaticEvolutionSilhouettes();
	UpdateStaticEvolutionCircuit();
	UpdateStaticEvolutionText();
	// TextLabelのフォント差し替えはテクスチャ転送を伴う。描画中に遅延更新すると
	// 転送処理がコマンドリストをResetするため、必ずUpdate段階で同期しておく。
	PrepareStaticEvolutionTextTextures();

	const bool confirmTriggered = input_ && input_->IsTrigger(
		input_->GetKey()[DIK_RETURN],
		input_->GetPreKey()[DIK_RETURN]);
	if (canConfirm && (confirmTriggered || (primaryTriggered && staticEvolutionConfirmHovered_))) {
		TryConfirmEvolutionById(selectedClassId);
	}
}

void Player::UpdateStaticEvolutionCircuit()
{
	const Vector2 current = staticEvolutionNodeCentersVirtual_[0];
	for (int& count : staticEvolutionCircuitControlPointCounts_) {
		count = 0;
	}
	int pathCount = 0;
	if (evolutionUiStyle_.radialLayout) {
		for (size_t candidateIndex = 0; candidateIndex < staticEvolutionCandidateCount_; ++candidateIndex) {
			staticEvolutionCircuitControlPoints_[candidateIndex] = {{
				current,
				staticEvolutionNodeCentersVirtual_[candidateIndex + 1],
				{},
				{}
			}};
			staticEvolutionCircuitControlPointCounts_[candidateIndex] = 2;
		}
		pathCount = static_cast<int>(staticEvolutionCandidateCount_);
		staticEvolutionBranchGlowSprite_->SetSize({ 0.0f, 0.0f });
		staticEvolutionBranchCoreSprite_->SetSize({ 0.0f, 0.0f });
	} else {
		const Vector2 branch = EvolutionAnchorToVirtual(evolutionUiStyle_.branchPointAnchor);
		staticEvolutionCircuitControlPoints_[0] = {{ current, branch, {}, {} }};
		staticEvolutionCircuitControlPointCounts_[0] = 2;
		for (size_t candidateIndex = 0; candidateIndex < staticEvolutionCandidateCount_; ++candidateIndex) {
			staticEvolutionCircuitControlPoints_[candidateIndex + 1] = {{
				branch,
				staticEvolutionNodeCentersVirtual_[candidateIndex + 1],
				{},
				{}
			}};
			staticEvolutionCircuitControlPointCounts_[candidateIndex + 1] = 2;
		}
		pathCount = static_cast<int>(staticEvolutionCandidateCount_ + 1);
		const float renderScale = GetEvolutionRenderScale();
		const Vector2 renderBranch = EvolutionVirtualToRender(branch);
		staticEvolutionBranchGlowSprite_->SetPosition(renderBranch);
		staticEvolutionBranchGlowSprite_->SetRotation(0.785398163f);
		staticEvolutionBranchGlowSprite_->SetSize({ 20.0f * renderScale, 20.0f * renderScale });
		Vector4 branchGlow = evolutionUiStyle_.selectedColor;
		branchGlow.w = 0.16f;
		staticEvolutionBranchGlowSprite_->SetColor(branchGlow);
		staticEvolutionBranchGlowSprite_->Update();
		staticEvolutionBranchCoreSprite_->SetPosition(renderBranch);
		staticEvolutionBranchCoreSprite_->SetRotation(0.785398163f);
		staticEvolutionBranchCoreSprite_->SetSize({ 7.0f * renderScale, 7.0f * renderScale });
		Vector4 branchCore = evolutionUiStyle_.selectedColor;
		branchCore.w = 0.94f;
		staticEvolutionBranchCoreSprite_->SetColor(branchCore);
		staticEvolutionBranchCoreSprite_->Update();
	}

	const int currentRank = GetRankFromLevel(level_);
	const float renderScale = GetEvolutionRenderScale();
	size_t spriteIndex = 0;
	auto setLine = [&](Sprite* sprite, const Vector2& a, const Vector2& b, float width, const Vector4& color) {
		const Vector2 renderA = EvolutionVirtualToRender(a);
		const Vector2 renderB = EvolutionVirtualToRender(b);
		const float dx = renderB.x - renderA.x;
		const float dy = renderB.y - renderA.y;
		const float length = std::sqrt(dx * dx + dy * dy);
		sprite->SetPosition(renderA);
		sprite->SetRotation(std::atan2(dy, dx));
		sprite->SetSize({ length, (std::max)(0.5f, width * renderScale) });
		sprite->SetColor(color);
		sprite->Update();
	};

	for (int pathIndex = 0; pathIndex < pathCount; ++pathIndex) {
		const bool trunk = !evolutionUiStyle_.radialLayout && pathIndex == 0;
		const int candidateIndex = evolutionUiStyle_.radialLayout ? pathIndex : pathIndex - 1;
		const bool selected = trunk || candidateIndex == evolutionUiStyle_.fixedSelectedCandidate;
		const bool hovered = !trunk && candidateIndex + 1 == staticEvolutionHoveredNode_;
		const PlayerClassConfig* config = trunk || candidateIndex < 0 ||
			candidateIndex >= static_cast<int>(staticEvolutionCandidateCount_)
			? nullptr
			: GetClassConfig(staticEvolutionCandidateIds_[static_cast<size_t>(candidateIndex)]);
		const bool locked = config && currentRank < config->requiredRank;
		Vector4 routeColor = evolutionUiStyle_.availableColor;
		float brightness = 0.38f;
		if (locked) {
			routeColor = evolutionUiStyle_.lockedColor;
			brightness = 0.14f;
		} else if (selected) {
			routeColor = evolutionUiStyle_.selectedColor;
			brightness = 1.0f;
		} else if (hovered) {
			routeColor = evolutionUiStyle_.hoverColor;
			brightness = 0.84f;
		}

		const int count = staticEvolutionCircuitControlPointCounts_[pathIndex];
		for (int pointIndex = 0; pointIndex + 1 < count; ++pointIndex) {
			const Vector2 a = staticEvolutionCircuitControlPoints_[pathIndex][pointIndex];
			const Vector2 b = staticEvolutionCircuitControlPoints_[pathIndex][pointIndex + 1];
			Vector4 outer = routeColor;
			Vector4 middle = routeColor;
			Vector4 core = routeColor;
			const float opacity = evolutionUiStyle_.circuitOpacity * brightness;
			outer.w = opacity * evolutionUiStyle_.circuitOuterAlpha;
			middle.w = opacity * evolutionUiStyle_.circuitMiddleAlpha;
			core.w = opacity * evolutionUiStyle_.circuitCoreAlpha;
			if (spriteIndex + 2 < staticEvolutionCircuitSprites_.size()) {
				setLine(staticEvolutionCircuitSprites_[spriteIndex++].get(), a, b, evolutionUiStyle_.circuitOuterGlowWidth, outer);
				setLine(staticEvolutionCircuitSprites_[spriteIndex++].get(), a, b, evolutionUiStyle_.circuitMiddleGlowWidth, middle);
				setLine(staticEvolutionCircuitSprites_[spriteIndex++].get(), a, b, evolutionUiStyle_.circuitCoreWidth, core);
			}
		}
	}
	while (spriteIndex < staticEvolutionCircuitSprites_.size()) {
		Sprite* sprite = staticEvolutionCircuitSprites_[spriteIndex++].get();
		sprite->SetSize({ 0.0f, 0.0f });
		sprite->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
		sprite->Update();
	}
}

void Player::UpdateStaticEvolutionNodeFrames()
{
	const int currentRank = GetRankFromLevel(level_);
	const float renderScale = GetEvolutionRenderScale();
	auto setLine = [&](Sprite* sprite, const Vector2& a, const Vector2& b, float width, const Vector4& color) {
		const Vector2 renderA = EvolutionVirtualToRender(a);
		const Vector2 renderB = EvolutionVirtualToRender(b);
		const float dx = renderB.x - renderA.x;
		const float dy = renderB.y - renderA.y;
		sprite->SetPosition(renderA);
		sprite->SetRotation(std::atan2(dy, dx));
		sprite->SetSize({ std::sqrt(dx * dx + dy * dy), (std::max)(0.5f, width * renderScale) });
		sprite->SetColor(color);
		sprite->Update();
	};

	const int activeNodeCount = static_cast<int>(staticEvolutionCandidateCount_ + 1);
	for (int nodeIndex = 0; nodeIndex < activeNodeCount; ++nodeIndex) {
		const bool selected = nodeIndex > 0 && nodeIndex - 1 == evolutionUiStyle_.fixedSelectedCandidate;
		const bool hovered = nodeIndex == staticEvolutionHoveredNode_;
		const PlayerClassConfig* config = nodeIndex > 0
			? GetClassConfig(staticEvolutionCandidateIds_[static_cast<size_t>(nodeIndex - 1)])
			: nullptr;
		const bool locked = config && currentRank < config->requiredRank;
		Vector4 stateColor = nodeIndex == 0 ? evolutionUiStyle_.normalColor : evolutionUiStyle_.availableColor;
		float glowAlpha = nodeIndex == 0 ? 0.035f : 0.050f;
		float middleAlpha = nodeIndex == 0 ? 0.12f : 0.18f;
		float coreAlpha = nodeIndex == 0 ? 0.58f : 0.72f;
		if (locked) {
			stateColor = evolutionUiStyle_.lockedColor;
			glowAlpha = 0.015f;
			middleAlpha = 0.06f;
			coreAlpha = 0.34f;
		} else if (selected) {
			stateColor = evolutionUiStyle_.selectedColor;
			glowAlpha = 0.18f;
			middleAlpha = 0.42f;
			coreAlpha = 1.0f;
		} else if (hovered) {
			stateColor = evolutionUiStyle_.hoverColor;
			glowAlpha = 0.14f;
			middleAlpha = 0.34f;
			coreAlpha = 0.94f;
		}

		const Vector2 center = staticEvolutionNodeCentersVirtual_[nodeIndex];
		const Vector2 size = staticEvolutionNodeDrawSizesVirtual_[nodeIndex];
		const float halfW = size.x * 0.5f;
		const float halfH = size.y * 0.5f;
		const float cut = (std::clamp)(evolutionUiStyle_.nodeCornerCut, 0.0f, halfH * 0.60f);
		const std::array<Vector2, 8> points = {{
			{ center.x - halfW + cut, center.y - halfH },
			{ center.x + halfW - cut, center.y - halfH },
			{ center.x + halfW, center.y - halfH + cut },
			{ center.x + halfW, center.y + halfH - cut },
			{ center.x + halfW - cut, center.y + halfH },
			{ center.x - halfW + cut, center.y + halfH },
			{ center.x - halfW, center.y + halfH - cut },
			{ center.x - halfW, center.y - halfH + cut }
		}};
		for (int segmentIndex = 0; segmentIndex < 8; ++segmentIndex) {
			Vector4 outer = stateColor;
			Vector4 middle = stateColor;
			Vector4 core = stateColor;
			outer.w = glowAlpha;
			middle.w = middleAlpha;
			core.w = coreAlpha;
			const Vector2 a = points[segmentIndex];
			const Vector2 b = points[(segmentIndex + 1) % 8];
			const size_t spriteIndex = static_cast<size_t>(segmentIndex * 3);
			setLine(staticEvolutionNodeFrameSprites_[nodeIndex][spriteIndex].get(), a, b, evolutionUiStyle_.nodeOutlineGlowWidth, outer);
			setLine(staticEvolutionNodeFrameSprites_[nodeIndex][spriteIndex + 1].get(), a, b, evolutionUiStyle_.nodeOutlineWidth * 2.2f, middle);
			setLine(staticEvolutionNodeFrameSprites_[nodeIndex][spriteIndex + 2].get(), a, b, evolutionUiStyle_.nodeOutlineWidth, core);
		}
	}
}

void Player::UpdateStaticEvolutionSilhouettes()
{
	struct Segment {
		Vector2 a{};
		Vector2 b{};
	};
	const int currentRank = GetRankFromLevel(level_);
	const float renderScale = GetEvolutionRenderScale();
	constexpr float kTwoPi = 6.283185307f;

	const int activeNodeCount = static_cast<int>(staticEvolutionCandidateCount_ + 1);
	for (int nodeIndex = 0; nodeIndex < activeNodeCount; ++nodeIndex) {
		const std::string& nodeClassId = nodeIndex == 0
			? currentClassId_
			: staticEvolutionCandidateIds_[static_cast<size_t>(nodeIndex - 1)];
		const PlayerClassConfig* config = GetClassConfig(nodeClassId);
		std::array<Segment, kStaticEvolutionSilhouetteSpriteCount> segments{};
		size_t segmentCount = 0;
		auto addSegment = [&](const Vector2& a, const Vector2& b) {
			if (segmentCount < segments.size()) {
				segments[segmentCount++] = { a, b };
			}
		};
		if (config) {
			const bool selected = nodeIndex > 0 && nodeIndex - 1 == evolutionUiStyle_.fixedSelectedCandidate;
			const bool hovered = nodeIndex == staticEvolutionHoveredNode_;
			const PlayerClassConfig* candidateConfig = nodeIndex > 0 ? config : nullptr;
			const bool locked = candidateConfig && currentRank < candidateConfig->requiredRank;
			float stateScale = evolutionUiStyle_.normalScale;
			if (selected) {
				stateScale = evolutionUiStyle_.selectedScale;
			} else if (hovered) {
				stateScale = evolutionUiStyle_.hoverScale;
			}
			const Vector2 nodeSize = staticEvolutionNodeDrawSizesVirtual_[nodeIndex];
			const Vector2 center = {
				staticEvolutionNodeCentersVirtual_[nodeIndex].x - nodeSize.x * 0.30f,
				staticEvolutionNodeCentersVirtual_[nodeIndex].y - 2.0f
			};
			const float silhouetteScale = evolutionUiStyle_.silhouetteScale * stateScale;
			const float radius = 18.0f * silhouetteScale;
			int bodySegments = 14;
			float bodyRotation = 0.0f;
			switch (config->bodyShape) {
			case BodyShape::Box:
				bodySegments = 4;
				bodyRotation = kTwoPi * 0.125f;
				break;
			case BodyShape::Triangle:
				bodySegments = 3;
				bodyRotation = -kTwoPi * 0.25f;
				break;
			case BodyShape::Pentagon:
				bodySegments = 5;
				bodyRotation = -kTwoPi * 0.25f;
				break;
			case BodyShape::Circle:
			default:
				break;
			}
			std::array<Vector2, 14> bodyPoints{};
			for (int i = 0; i < bodySegments; ++i) {
				const float angle = bodyRotation + static_cast<float>(i) * kTwoPi / static_cast<float>(bodySegments);
				bodyPoints[i] = {
					center.x + std::cos(angle) * radius * (std::clamp)(config->bodyScale.x, 0.45f, 1.80f),
					center.y + std::sin(angle) * radius * (std::clamp)(config->bodyScale.y, 0.45f, 1.80f)
				};
			}
			for (int i = 0; i < bodySegments; ++i) {
				addSegment(bodyPoints[i], bodyPoints[(i + 1) % bodySegments]);
			}

			for (const WeaponMountConfig& mount : config->barrels) {
				const float angle = mount.angleDeg * 3.1415926535f / 180.0f;
				const Vector2 forward{ std::cos(angle), std::sin(angle) };
				const Vector2 right{ -forward.y, forward.x };
				float length = (std::max)(14.0f, mount.scale.x * 15.0f) * silhouetteScale;
				float halfWidth = (std::max)(2.2f, mount.scale.y * 7.0f) * silhouetteScale;
				if (mount.barrelShape == BarrelShape::Heavy) {
					length *= 1.12f;
					halfWidth *= 1.45f;
				} else if (mount.barrelShape == BarrelShape::Short) {
					length *= 0.58f;
				} else if (mount.barrelShape == BarrelShape::Wide) {
					length *= 0.86f;
					halfWidth *= 1.80f;
				}
				const Vector2 mountCenter = {
					center.x + mount.offset.x * radius * 0.55f + forward.x * length * 0.30f,
					center.y + mount.offset.y * radius * 0.55f + forward.y * length * 0.30f
				};
				const float baseWidth = mount.barrelShape == BarrelShape::Trapezoid ? halfWidth * 1.28f : halfWidth;
				const float tipWidth = mount.barrelShape == BarrelShape::Trapezoid ? halfWidth * 0.72f : halfWidth;
				const Vector2 base = { mountCenter.x - forward.x * length * 0.5f, mountCenter.y - forward.y * length * 0.5f };
				const Vector2 tip = { mountCenter.x + forward.x * length * 0.5f, mountCenter.y + forward.y * length * 0.5f };
				const Vector2 p0{ base.x - right.x * baseWidth, base.y - right.y * baseWidth };
				const Vector2 p1{ tip.x - right.x * tipWidth, tip.y - right.y * tipWidth };
				const Vector2 p2{ tip.x + right.x * tipWidth, tip.y + right.y * tipWidth };
				const Vector2 p3{ base.x + right.x * baseWidth, base.y + right.y * baseWidth };
				addSegment(p0, p1);
				addSegment(p1, p2);
				addSegment(p2, p3);
				addSegment(p3, p0);
			}

			if (config->usesDrone) {
				for (float side : { -1.0f, 1.0f }) {
					const Vector2 droneCenter{ center.x - 28.0f * silhouetteScale, center.y + side * 18.0f * silhouetteScale };
					const float droneRadius = 5.0f * silhouetteScale;
					const Vector2 top{ droneCenter.x, droneCenter.y - droneRadius };
					const Vector2 rightPoint{ droneCenter.x + droneRadius, droneCenter.y };
					const Vector2 bottom{ droneCenter.x, droneCenter.y + droneRadius };
					const Vector2 leftPoint{ droneCenter.x - droneRadius, droneCenter.y };
					addSegment(top, rightPoint);
					addSegment(rightPoint, bottom);
					addSegment(bottom, leftPoint);
					addSegment(leftPoint, top);
				}
			}

			Vector4 silhouetteColor = selected ? evolutionUiStyle_.selectedColor : evolutionUiStyle_.classTextColor;
			if (hovered && !selected) {
				silhouetteColor = evolutionUiStyle_.hoverColor;
			}
			silhouetteColor.w = locked ? 0.30f : selected ? 0.94f : 0.72f;
			for (size_t i = 0; i < segmentCount; ++i) {
				const Vector2 renderA = EvolutionVirtualToRender(segments[i].a);
				const Vector2 renderB = EvolutionVirtualToRender(segments[i].b);
				const float dx = renderB.x - renderA.x;
				const float dy = renderB.y - renderA.y;
				Sprite* line = staticEvolutionSilhouetteSprites_[nodeIndex][i].get();
				line->SetPosition(renderA);
				line->SetRotation(std::atan2(dy, dx));
				line->SetSize({ std::sqrt(dx * dx + dy * dy), (std::max)(0.75f, 1.55f * renderScale) });
				line->SetColor(silhouetteColor);
				line->Update();
			}
		}
		while (segmentCount < staticEvolutionSilhouetteSprites_[nodeIndex].size()) {
			Sprite* line = staticEvolutionSilhouetteSprites_[nodeIndex][segmentCount++].get();
			line->SetSize({ 0.0f, 0.0f });
			line->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
			line->Update();
		}
	}
}

void Player::UpdateStaticEvolutionText()
{
	const float renderScale = GetEvolutionRenderScale();
	const float safe = evolutionUiStyle_.safeMargin;
	const auto makeStyle = [&](float size, const Vector4& color, float outlineWidth) {
		TextStyle style{};
		style.fontFamily = evolutionUiStyle_.fontFamily;
		style.fontPath = evolutionUiStyle_.fontPath;
		style.fontWeight = evolutionUiStyle_.fontWeight;
		style.fontSize = (std::max)(8.0f, size * renderScale);
		style.color = color;
		style.outlineColor = evolutionUiStyle_.textOutlineColor;
		style.outlineThickness = (std::max)(0.0f, outlineWidth * renderScale);
		style.padding = 6.0f * renderScale;
		return style;
	};

	const TextStyle titleStyle = makeStyle(
		evolutionUiStyle_.titleFontSize,
		evolutionUiStyle_.titleTextColor,
		evolutionUiStyle_.titleOutlineWidth);
	const TextStyle classNameStyle = makeStyle(
		evolutionUiStyle_.classNameFontSize,
		evolutionUiStyle_.classTextColor,
		evolutionUiStyle_.classNameOutlineWidth);
	const TextStyle bodyStyle = makeStyle(
		evolutionUiStyle_.bodyFontSize,
		evolutionUiStyle_.bodyTextColor,
		evolutionUiStyle_.bodyOutlineWidth);
	const TextStyle buttonStyle = makeStyle(
		evolutionUiStyle_.buttonFontSize,
		evolutionUiStyle_.buttonTextColor,
		evolutionUiStyle_.buttonOutlineWidth);
	SpriteCommon* spriteCommon = SpriteCommon::GetInstance();

	SetLabel(
		staticEvolutionTitleLabel_,
		spriteCommon,
		"EVOLUTION CIRCUIT // RANK " +
			std::to_string(GetCurrentClassConfig() ? GetCurrentClassConfig()->requiredRank : 1) +
			" TO " +
			std::to_string(GetCurrentClassConfig() ? GetCurrentClassConfig()->requiredRank + 1 : 2),
		EvolutionVirtualToRender({ safe, safe * 0.62f }),
		titleStyle);
	SetLabel(
		staticEvolutionPrototypeLabel_,
		spriteCommon,
		"INTERACTION DEBUG",
		EvolutionVirtualToRender({ evolutionUiStyle_.virtualResolution.x - safe, safe * 0.72f }),
		bodyStyle);
	staticEvolutionPrototypeLabel_->SetAnchorPoint({ 1.0f, 0.0f });

	const int currentRank = GetRankFromLevel(level_);
	const int activeNodeCount = static_cast<int>(staticEvolutionCandidateCount_ + 1);
	for (int i = 0; i < activeNodeCount; ++i) {
		const bool selected = i > 0 && i - 1 == evolutionUiStyle_.fixedSelectedCandidate;
		const bool hovered = i == staticEvolutionHoveredNode_;
		const std::string& nodeClassId = i == 0
			? currentClassId_
			: staticEvolutionCandidateIds_[static_cast<size_t>(i - 1)];
		const PlayerClassConfig* config = GetClassConfig(nodeClassId);
		const bool locked = i > 0 && config && currentRank < config->requiredRank;
		Vector4 textColor = evolutionUiStyle_.classTextColor;
		if (locked) {
			textColor = evolutionUiStyle_.lockedColor;
			textColor.w = 1.0f;
		} else if (selected) {
			textColor = evolutionUiStyle_.selectedColor;
		} else if (hovered) {
			textColor = evolutionUiStyle_.hoverColor;
		}
		TextStyle nodeClassStyle = makeStyle(
			evolutionUiStyle_.classNameFontSize,
			textColor,
			evolutionUiStyle_.classNameOutlineWidth);
		Vector4 roleColor = evolutionUiStyle_.bodyTextColor;
		roleColor.w = locked ? 0.42f : 0.72f;
		TextStyle nodeRoleStyle = makeStyle(
			evolutionUiStyle_.bodyFontSize * 0.78f,
			roleColor,
			evolutionUiStyle_.bodyOutlineWidth);
		const Vector2 center = staticEvolutionNodeCentersVirtual_[i];
		const float textX = center.x + 34.0f;
		SetLabel(
			staticEvolutionNodeNameLabels_[i],
			spriteCommon,
			GetEvolutionClassName(nodeClassId),
			EvolutionVirtualToRender({ textX, center.y - 12.0f }),
			nodeClassStyle);
		staticEvolutionNodeNameLabels_[i]->SetAnchorPoint({ 0.5f, 0.5f });
		SetLabel(
			staticEvolutionNodeRankLabels_[i],
			spriteCommon,
			i == 0 ? "CURRENT CLASS" : config ? GetEvolutionShortRole(*config) : "UNKNOWN",
			EvolutionVirtualToRender({ textX, center.y + 18.0f }),
			nodeRoleStyle);
		staticEvolutionNodeRankLabels_[i]->SetAnchorPoint({ 0.5f, 0.5f });
	}

	const std::string& selectedClassId = staticEvolutionCandidateIds_[
		static_cast<size_t>(evolutionUiStyle_.fixedSelectedCandidate)];
	const PlayerClassConfig* selected = GetClassConfig(selectedClassId);
	const PlayerClassConfig* current = GetCurrentClassConfig();
	if (!selected || !current) {
		return;
	}
	const std::array<std::string, 3> deltas = GetEvolutionDeltas(*current, *selected);
	const Vector2 panelCenter = EvolutionAnchorToVirtual(evolutionUiStyle_.detailPanelAnchor);
	const Vector2 panelTopLeft = {
		panelCenter.x - evolutionUiStyle_.detailPanelSize.x * 0.5f,
		panelCenter.y - evolutionUiStyle_.detailPanelSize.y * 0.5f
	};
	SetLabel(
		staticEvolutionDetailClassLabel_,
		spriteCommon,
		GetEvolutionClassName(selectedClassId),
		EvolutionVirtualToRender({ panelTopLeft.x + 24.0f, panelTopLeft.y + 13.0f }),
		classNameStyle);
	SetLabel(
		staticEvolutionRoleLabel_,
		spriteCommon,
		GetEvolutionRole(*selected),
		EvolutionVirtualToRender({ panelTopLeft.x + 150.0f, panelTopLeft.y + 18.0f }),
		bodyStyle);
	for (int i = 0; i < 3; ++i) {
		SetLabel(
			staticEvolutionDeltaLabels_[i],
			spriteCommon,
			deltas[i],
			EvolutionVirtualToRender({ panelTopLeft.x + 24.0f + static_cast<float>(i) * 276.0f, panelTopLeft.y + 76.0f }),
			bodyStyle);
	}
	SetLabel(
		staticEvolutionAbilityLabel_,
		spriteCommon,
		GetEvolutionAbility(*selected),
		EvolutionVirtualToRender({ panelTopLeft.x + 400.0f, panelTopLeft.y + 18.0f }),
		bodyStyle);
	const Vector2 buttonCenter = {
		panelCenter.x + evolutionUiStyle_.detailPanelSize.x * 0.5f - evolutionUiStyle_.confirmButtonSize.x * 0.5f - 18.0f,
		panelCenter.y - evolutionUiStyle_.detailPanelSize.y * 0.5f + 32.0f
	};
	SetLabel(
		staticEvolutionConfirmLabel_,
		spriteCommon,
		CanEvolveTo(selectedClassId) ? "ENTER  進化決定" : "RANK不足",
		EvolutionVirtualToRender(buttonCenter),
		buttonStyle);
	staticEvolutionConfirmLabel_->SetAnchorPoint({ 0.5f, 0.5f });
	SetLabel(
		staticEvolutionPanelHintLabel_,
		spriteCommon,
		"ESC  戻る  /  ENTER  決定",
		EvolutionVirtualToRender({ panelTopLeft.x + evolutionUiStyle_.detailPanelSize.x - 278.0f, panelTopLeft.y + 94.0f }),
		bodyStyle);
}

void Player::PrepareStaticEvolutionTextTextures()
{
	auto prepare = [](TextLabel* label) {
		if (label) {
			label->PrepareForDraw();
		}
	};

	prepare(staticEvolutionTitleLabel_.get());
	prepare(staticEvolutionPrototypeLabel_.get());
	prepare(staticEvolutionDetailClassLabel_.get());
	prepare(staticEvolutionRoleLabel_.get());
	for (const auto& label : staticEvolutionDeltaLabels_) {
		prepare(label.get());
	}
	prepare(staticEvolutionAbilityLabel_.get());
	prepare(staticEvolutionConfirmLabel_.get());
	prepare(staticEvolutionPanelHintLabel_.get());

	for (const auto& label : staticEvolutionNodeNameLabels_) {
		prepare(label.get());
	}
	for (const auto& label : staticEvolutionNodeRankLabels_) {
		prepare(label.get());
	}
	for (size_t i = 0; i < staticEvolutionCandidateCount_ + 1; ++i) {
		if (staticEvolutionTankButtons_[i]) {
			prepare(staticEvolutionTankButtons_[i]->GetLabel());
		}
	}
}

bool Player::LoadEvolutionUiStyle(const std::string& path)
{
	std::ifstream file(path);
	if (!file.is_open()) {
		evolutionUiStyleStatus_ = "進化UI設定が見つからないため初期値を使用します。";
		return false;
	}

	nlohmann::json root{};
	try {
		file >> root;
	} catch (...) {
		evolutionUiStyleStatus_ = "進化UI設定JSONの読み込みに失敗しました。";
		return false;
	}
	if (!root.is_object()) {
		return false;
	}

	evolutionUiStyle_.enabled = root.value("enabled", evolutionUiStyle_.enabled);
	evolutionUiStyle_.virtualResolution = ReadVector2Object(root.value("virtualResolution", nlohmann::json::object()), evolutionUiStyle_.virtualResolution);
	evolutionUiStyle_.safeMargin = root.value("safeMargin", evolutionUiStyle_.safeMargin);
	if (root.contains("layout") && root["layout"].is_object()) {
		const nlohmann::json& layout = root["layout"];
		evolutionUiStyle_.radialLayout = layout.value("mode", std::string("leftToRight")) == "radial";
		evolutionUiStyle_.branchPointAnchor = ReadVector2Object(
			layout.value("branchPoint", nlohmann::json::object()),
			evolutionUiStyle_.branchPointAnchor);
		if (layout.contains("radialNodes") && layout["radialNodes"].is_object()) {
			const nlohmann::json& radialNodes = layout["radialNodes"];
			const std::array<const char*, 4> keys = { "Basic", "Twin", "MachineGun", "Overseer" };
			for (int i = 0; i < 4; ++i) {
				evolutionUiStyle_.radialNodeAnchors[i] = ReadVector2Object(
					radialNodes.value(keys[i], nlohmann::json::object()),
					evolutionUiStyle_.radialNodeAnchors[i]);
			}
		}
	}
	if (root.contains("nodes") && root["nodes"].is_object()) {
		const nlohmann::json& nodes = root["nodes"];
		const std::array<const char*, 4> keys = { "Basic", "Twin", "MachineGun", "Overseer" };
		for (int i = 0; i < 4; ++i) {
			evolutionUiStyle_.nodeAnchors[i] =
				ReadVector2Object(nodes.value(keys[i], nlohmann::json::object()), evolutionUiStyle_.nodeAnchors[i]);
		}
		evolutionUiStyle_.currentNodeSize = ReadVector2Object(nodes.value("currentSize", nlohmann::json::object()), evolutionUiStyle_.currentNodeSize);
		evolutionUiStyle_.candidateNodeSize = ReadVector2Object(nodes.value("candidateSize", nlohmann::json::object()), evolutionUiStyle_.candidateNodeSize);
		evolutionUiStyle_.nodeCornerCut = nodes.value("cornerCut", evolutionUiStyle_.nodeCornerCut);
		evolutionUiStyle_.nodeOutlineGlowWidth = nodes.value("outlineGlowWidth", evolutionUiStyle_.nodeOutlineGlowWidth);
		evolutionUiStyle_.nodeOutlineWidth = nodes.value("outlineWidth", evolutionUiStyle_.nodeOutlineWidth);
		evolutionUiStyle_.silhouetteScale = nodes.value("silhouetteScale", evolutionUiStyle_.silhouetteScale);
		if (nodes.contains("scale") && nodes["scale"].is_object()) {
			const nlohmann::json& scale = nodes["scale"];
			evolutionUiStyle_.normalScale = scale.value("normal", evolutionUiStyle_.normalScale);
			evolutionUiStyle_.hoverScale = scale.value("hover", evolutionUiStyle_.hoverScale);
			evolutionUiStyle_.selectedScale = scale.value("selected", evolutionUiStyle_.selectedScale);
		}
	}
	if (root.contains("circuit") && root["circuit"].is_object()) {
		const nlohmann::json& circuit = root["circuit"];
		evolutionUiStyle_.circuitOuterGlowWidth = circuit.value("outerGlowWidth", evolutionUiStyle_.circuitOuterGlowWidth);
		evolutionUiStyle_.circuitMiddleGlowWidth = circuit.value("middleGlowWidth", evolutionUiStyle_.circuitMiddleGlowWidth);
		evolutionUiStyle_.circuitCoreWidth = circuit.value("coreWidth", evolutionUiStyle_.circuitCoreWidth);
		evolutionUiStyle_.circuitOpacity = circuit.value("opacity", evolutionUiStyle_.circuitOpacity);
		evolutionUiStyle_.circuitOuterAlpha = circuit.value("outerAlpha", evolutionUiStyle_.circuitOuterAlpha);
		evolutionUiStyle_.circuitMiddleAlpha = circuit.value("middleAlpha", evolutionUiStyle_.circuitMiddleAlpha);
		evolutionUiStyle_.circuitCoreAlpha = circuit.value("coreAlpha", evolutionUiStyle_.circuitCoreAlpha);
	}
	evolutionUiStyle_.backgroundDimOpacity = root.value("backgroundDimOpacity", evolutionUiStyle_.backgroundDimOpacity);
	if (root.contains("detailPanel") && root["detailPanel"].is_object()) {
		const nlohmann::json& panel = root["detailPanel"];
		evolutionUiStyle_.detailPanelAnchor = ReadVector2Object(panel.value("anchor", nlohmann::json::object()), evolutionUiStyle_.detailPanelAnchor);
		evolutionUiStyle_.detailPanelSize = ReadVector2Object(panel.value("size", nlohmann::json::object()), evolutionUiStyle_.detailPanelSize);
	}
	if (root.contains("confirmButton") && root["confirmButton"].is_object()) {
		evolutionUiStyle_.confirmButtonSize = ReadVector2Object(root["confirmButton"].value("size", nlohmann::json::object()), evolutionUiStyle_.confirmButtonSize);
	}
	if (root.contains("text") && root["text"].is_object()) {
		const nlohmann::json& text = root["text"];
		evolutionUiStyle_.fontFamily = text.value("fontFamily", evolutionUiStyle_.fontFamily);
		evolutionUiStyle_.fontPath = text.value("fontPath", evolutionUiStyle_.fontPath);
		evolutionUiStyle_.fontWeight = text.value("fontWeight", evolutionUiStyle_.fontWeight);
		evolutionUiStyle_.titleFontSize = text.value("titleSize", evolutionUiStyle_.titleFontSize);
		evolutionUiStyle_.classNameFontSize = text.value("classNameSize", evolutionUiStyle_.classNameFontSize);
		evolutionUiStyle_.bodyFontSize = text.value("bodySize", evolutionUiStyle_.bodyFontSize);
		evolutionUiStyle_.buttonFontSize = text.value("buttonSize", evolutionUiStyle_.buttonFontSize);
		evolutionUiStyle_.titleTextColor = ReadVector4(text.value("titleColor", nlohmann::json::array()), evolutionUiStyle_.titleTextColor);
		evolutionUiStyle_.classTextColor = ReadVector4(text.value("classNameColor", nlohmann::json::array()), evolutionUiStyle_.classTextColor);
		evolutionUiStyle_.bodyTextColor = ReadVector4(text.value("bodyColor", nlohmann::json::array()), evolutionUiStyle_.bodyTextColor);
		evolutionUiStyle_.buttonTextColor = ReadVector4(text.value("buttonColor", nlohmann::json::array()), evolutionUiStyle_.buttonTextColor);
		evolutionUiStyle_.textOutlineColor = ReadVector4(text.value("outlineColor", nlohmann::json::array()), evolutionUiStyle_.textOutlineColor);
		const float legacyOutline = text.value("outlineWidth", evolutionUiStyle_.classNameOutlineWidth);
		evolutionUiStyle_.titleOutlineWidth = text.value("titleOutlineWidth", legacyOutline);
		evolutionUiStyle_.classNameOutlineWidth = text.value("classNameOutlineWidth", legacyOutline);
		evolutionUiStyle_.bodyOutlineWidth = text.value("bodyOutlineWidth", evolutionUiStyle_.bodyOutlineWidth);
		evolutionUiStyle_.buttonOutlineWidth = text.value("buttonOutlineWidth", evolutionUiStyle_.buttonOutlineWidth);
	}
	if (root.contains("neonText") && root["neonText"].is_object()) {
		const nlohmann::json& neon = root["neonText"];
		evolutionUiStyle_.neonText.enabled = neon.value("enabled", evolutionUiStyle_.neonText.enabled);
		evolutionUiStyle_.neonText.glowColor = ReadVector4(
			neon.value("glowColor", nlohmann::json::array()),
			evolutionUiStyle_.neonText.glowColor);
		evolutionUiStyle_.neonText.sourceBrightness = neon.value(
			"sourceBrightness", evolutionUiStyle_.neonText.sourceBrightness);
		evolutionUiStyle_.neonText.threshold = neon.value(
			"threshold", evolutionUiStyle_.neonText.threshold);
		evolutionUiStyle_.neonText.innerIntensity = neon.value(
			"innerIntensity", evolutionUiStyle_.neonText.innerIntensity);
		evolutionUiStyle_.neonText.outerIntensity = neon.value(
			"outerIntensity", evolutionUiStyle_.neonText.outerIntensity);
	}
	if (root.contains("colors") && root["colors"].is_object()) {
		const nlohmann::json& colors = root["colors"];
		evolutionUiStyle_.normalColor = ReadVector4(colors.value("normal", nlohmann::json::array()), evolutionUiStyle_.normalColor);
		evolutionUiStyle_.availableColor = ReadVector4(colors.value("available", nlohmann::json::array()), evolutionUiStyle_.availableColor);
		evolutionUiStyle_.hoverColor = ReadVector4(colors.value("hover", nlohmann::json::array()), evolutionUiStyle_.hoverColor);
		evolutionUiStyle_.selectedColor = ReadVector4(colors.value("selected", nlohmann::json::array()), evolutionUiStyle_.selectedColor);
		evolutionUiStyle_.lockedColor = ReadVector4(colors.value("locked", nlohmann::json::array()), evolutionUiStyle_.lockedColor);
		evolutionUiStyle_.panelColor = ReadVector4(colors.value("panel", nlohmann::json::array()), evolutionUiStyle_.panelColor);
	}
	evolutionUiStyle_.fixedSelectedCandidate =
		(std::clamp)(
			root.value("fixedSelectedCandidate", evolutionUiStyle_.fixedSelectedCandidate),
			0,
			static_cast<int>(kStaticEvolutionMaxCandidates - 1));
	evolutionUiStyleStatus_ = "進化UI設定を読み込みました: " + path;
	return true;
}

bool Player::SaveEvolutionUiStyle(const std::string& path) const
{
	std::filesystem::create_directories(std::filesystem::path(path).parent_path());
	nlohmann::json root{};
	root["version"] = 2;
	root["enabled"] = evolutionUiStyle_.enabled;
	root["virtualResolution"] = WriteVector2Object(evolutionUiStyle_.virtualResolution);
	root["safeMargin"] = evolutionUiStyle_.safeMargin;
	root["layout"] = {
		{ "mode", evolutionUiStyle_.radialLayout ? "radial" : "leftToRight" },
		{ "branchPoint", WriteVector2Object(evolutionUiStyle_.branchPointAnchor) },
		{ "radialNodes", {
			{ "Basic", WriteVector2Object(evolutionUiStyle_.radialNodeAnchors[0]) },
			{ "Twin", WriteVector2Object(evolutionUiStyle_.radialNodeAnchors[1]) },
			{ "MachineGun", WriteVector2Object(evolutionUiStyle_.radialNodeAnchors[2]) },
			{ "Overseer", WriteVector2Object(evolutionUiStyle_.radialNodeAnchors[3]) }
		} }
	};
	root["nodes"] = {
		{ "Basic", WriteVector2Object(evolutionUiStyle_.nodeAnchors[0]) },
		{ "Twin", WriteVector2Object(evolutionUiStyle_.nodeAnchors[1]) },
		{ "MachineGun", WriteVector2Object(evolutionUiStyle_.nodeAnchors[2]) },
		{ "Overseer", WriteVector2Object(evolutionUiStyle_.nodeAnchors[3]) },
		{ "currentSize", WriteVector2Object(evolutionUiStyle_.currentNodeSize) },
		{ "candidateSize", WriteVector2Object(evolutionUiStyle_.candidateNodeSize) },
		{ "cornerCut", evolutionUiStyle_.nodeCornerCut },
		{ "outlineGlowWidth", evolutionUiStyle_.nodeOutlineGlowWidth },
		{ "outlineWidth", evolutionUiStyle_.nodeOutlineWidth },
		{ "silhouetteScale", evolutionUiStyle_.silhouetteScale },
		{ "scale", {
			{ "normal", evolutionUiStyle_.normalScale },
			{ "hover", evolutionUiStyle_.hoverScale },
			{ "selected", evolutionUiStyle_.selectedScale }
		} }
	};
	root["circuit"] = {
		{ "outerGlowWidth", evolutionUiStyle_.circuitOuterGlowWidth },
		{ "middleGlowWidth", evolutionUiStyle_.circuitMiddleGlowWidth },
		{ "coreWidth", evolutionUiStyle_.circuitCoreWidth },
		{ "opacity", evolutionUiStyle_.circuitOpacity },
		{ "outerAlpha", evolutionUiStyle_.circuitOuterAlpha },
		{ "middleAlpha", evolutionUiStyle_.circuitMiddleAlpha },
		{ "coreAlpha", evolutionUiStyle_.circuitCoreAlpha }
	};
	root["backgroundDimOpacity"] = evolutionUiStyle_.backgroundDimOpacity;
	root["detailPanel"] = {
		{ "anchor", WriteVector2Object(evolutionUiStyle_.detailPanelAnchor) },
		{ "size", WriteVector2Object(evolutionUiStyle_.detailPanelSize) }
	};
	root["confirmButton"] = {
		{ "size", WriteVector2Object(evolutionUiStyle_.confirmButtonSize) }
	};
	root["text"] = {
		{ "fontFamily", evolutionUiStyle_.fontFamily },
		{ "fontPath", evolutionUiStyle_.fontPath },
		{ "fontWeight", evolutionUiStyle_.fontWeight },
		{ "titleSize", evolutionUiStyle_.titleFontSize },
		{ "classNameSize", evolutionUiStyle_.classNameFontSize },
		{ "bodySize", evolutionUiStyle_.bodyFontSize },
		{ "buttonSize", evolutionUiStyle_.buttonFontSize },
		{ "titleColor", Vector4ToJson(evolutionUiStyle_.titleTextColor) },
		{ "classNameColor", Vector4ToJson(evolutionUiStyle_.classTextColor) },
		{ "bodyColor", Vector4ToJson(evolutionUiStyle_.bodyTextColor) },
		{ "buttonColor", Vector4ToJson(evolutionUiStyle_.buttonTextColor) },
		{ "outlineColor", Vector4ToJson(evolutionUiStyle_.textOutlineColor) },
		{ "titleOutlineWidth", evolutionUiStyle_.titleOutlineWidth },
		{ "classNameOutlineWidth", evolutionUiStyle_.classNameOutlineWidth },
		{ "bodyOutlineWidth", evolutionUiStyle_.bodyOutlineWidth },
		{ "buttonOutlineWidth", evolutionUiStyle_.buttonOutlineWidth }
	};
	root["neonText"] = {
		{ "enabled", evolutionUiStyle_.neonText.enabled },
		{ "glowColor", Vector4ToJson(evolutionUiStyle_.neonText.glowColor) },
		{ "sourceBrightness", evolutionUiStyle_.neonText.sourceBrightness },
		{ "threshold", evolutionUiStyle_.neonText.threshold },
		{ "innerIntensity", evolutionUiStyle_.neonText.innerIntensity },
		{ "outerIntensity", evolutionUiStyle_.neonText.outerIntensity }
	};
	root["colors"] = {
		{ "normal", Vector4ToJson(evolutionUiStyle_.normalColor) },
		{ "available", Vector4ToJson(evolutionUiStyle_.availableColor) },
		{ "hover", Vector4ToJson(evolutionUiStyle_.hoverColor) },
		{ "selected", Vector4ToJson(evolutionUiStyle_.selectedColor) },
		{ "locked", Vector4ToJson(evolutionUiStyle_.lockedColor) },
		{ "panel", Vector4ToJson(evolutionUiStyle_.panelColor) }
	};
	root["fixedSelectedCandidate"] = evolutionUiStyle_.fixedSelectedCandidate;

	std::ofstream file(path);
	if (!file.is_open()) {
		return false;
	}
	file << root.dump(2);
	return true;
}

void Player::DrawEvolutionUiStyleEditor()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("進化UI", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}
	ImGui::Checkbox("新しい進化UIを有効化", &evolutionUiStyle_.enabled);
	RefreshStaticEvolutionCandidates();
	if (staticEvolutionCandidateCount_ > 0) {
		ImGui::SliderInt(
			"選択中の候補",
			&evolutionUiStyle_.fixedSelectedCandidate,
			0,
			static_cast<int>(staticEvolutionCandidateCount_ - 1));
		ImGui::SameLine();
		ImGui::TextDisabled(
			"%s",
			GetEvolutionClassName(staticEvolutionCandidateIds_[
				static_cast<size_t>(evolutionUiStyle_.fixedSelectedCandidate)]).c_str());
	}
	if (ImGui::Button("進化UI設定を保存")) {
		evolutionUiStyleStatus_ = SaveEvolutionUiStyle()
			? "進化UI設定を保存しました。"
			: "進化UI設定の保存に失敗しました。";
	}
	ImGui::SameLine();
	if (ImGui::Button("進化UI設定を再読み込み")) {
		LoadEvolutionUiStyle();
	}
	if (!evolutionUiStyleStatus_.empty()) {
		ImGui::TextWrapped("%s", evolutionUiStyleStatus_.c_str());
	}

	const float clientWidth = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
	const float clientHeight = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
	const float actualScale = (std::min)(
		clientWidth / (std::max)(1.0f, evolutionUiStyle_.virtualResolution.x),
		clientHeight / (std::max)(1.0f, evolutionUiStyle_.virtualResolution.y));
	ImGui::Text("実解像度: %.0f x %.0f / UI倍率: %.3f", clientWidth, clientHeight, actualScale);

	ImGui::SeparatorText("仮想画面と配置");
	ImGui::DragFloat2("仮想解像度", &evolutionUiStyle_.virtualResolution.x, 1.0f, 320.0f, 7680.0f);
	ImGui::DragFloat("セーフマージン", &evolutionUiStyle_.safeMargin, 1.0f, 0.0f, 360.0f);
	int layoutMode = evolutionUiStyle_.radialLayout ? 1 : 0;
	const char* layoutNames[] = { "左から右へ分岐", "放射型" };
	if (ImGui::Combo("配置モード", &layoutMode, layoutNames, IM_ARRAYSIZE(layoutNames))) {
		evolutionUiStyle_.radialLayout = layoutMode == 1;
	}
	const char* nodeNames[] = { "Basic", "Twin", "MachineGun", "Overseer" };
	for (int i = 0; i < 4; ++i) {
		ImGui::PushID(i);
		ImGui::DragFloat2(
			evolutionUiStyle_.radialLayout ? "放射型アンカー" : nodeNames[i],
			evolutionUiStyle_.radialLayout
				? &evolutionUiStyle_.radialNodeAnchors[i].x
				: &evolutionUiStyle_.nodeAnchors[i].x,
			0.005f,
			0.0f,
			1.0f);
		ImGui::PopID();
	}
	if (!evolutionUiStyle_.radialLayout) {
		ImGui::DragFloat2("分岐点", &evolutionUiStyle_.branchPointAnchor.x, 0.005f, 0.0f, 1.0f);
	}
	ImGui::DragFloat2("現在ノードサイズ", &evolutionUiStyle_.currentNodeSize.x, 1.0f, 32.0f, 600.0f);
	ImGui::DragFloat2("候補ノードサイズ", &evolutionUiStyle_.candidateNodeSize.x, 1.0f, 32.0f, 600.0f);
	ImGui::DragFloat("通常拡大率", &evolutionUiStyle_.normalScale, 0.005f, 0.5f, 2.0f);
	ImGui::DragFloat("ホバー拡大率", &evolutionUiStyle_.hoverScale, 0.005f, 0.5f, 2.0f);
	ImGui::DragFloat("選択拡大率", &evolutionUiStyle_.selectedScale, 0.005f, 0.5f, 2.0f);
	ImGui::DragFloat("角落とし", &evolutionUiStyle_.nodeCornerCut, 0.25f, 0.0f, 48.0f);
	ImGui::DragFloat("ノード外光幅", &evolutionUiStyle_.nodeOutlineGlowWidth, 0.25f, 0.5f, 40.0f);
	ImGui::DragFloat("ノード輪郭幅", &evolutionUiStyle_.nodeOutlineWidth, 0.1f, 0.5f, 12.0f);
	ImGui::DragFloat("戦車シルエット倍率", &evolutionUiStyle_.silhouetteScale, 0.01f, 0.5f, 2.0f);
	ImGui::DragFloat2("説明パネル位置（正規化）", &evolutionUiStyle_.detailPanelAnchor.x, 0.005f, 0.0f, 1.0f);
	ImGui::DragFloat2("説明パネルサイズ", &evolutionUiStyle_.detailPanelSize.x, 1.0f, 64.0f, 2000.0f);
	ImGui::DragFloat2("決定ボタンサイズ", &evolutionUiStyle_.confirmButtonSize.x, 1.0f, 32.0f, 600.0f);

	ImGui::SeparatorText("ネオン回路");
	ImGui::DragFloat("外光幅", &evolutionUiStyle_.circuitOuterGlowWidth, 0.25f, 0.5f, 80.0f);
	ImGui::DragFloat("中光幅", &evolutionUiStyle_.circuitMiddleGlowWidth, 0.25f, 0.5f, 60.0f);
	ImGui::DragFloat("中心線幅", &evolutionUiStyle_.circuitCoreWidth, 0.1f, 0.25f, 24.0f);
	ImGui::SliderFloat("回路全体透明度", &evolutionUiStyle_.circuitOpacity, 0.0f, 1.0f);
	ImGui::SliderFloat("外光透明度", &evolutionUiStyle_.circuitOuterAlpha, 0.0f, 1.0f);
	ImGui::SliderFloat("中光透明度", &evolutionUiStyle_.circuitMiddleAlpha, 0.0f, 1.0f);
	ImGui::SliderFloat("中心線透明度", &evolutionUiStyle_.circuitCoreAlpha, 0.0f, 1.0f);
	ImGui::SliderFloat("背景暗転率", &evolutionUiStyle_.backgroundDimOpacity, 0.0f, 1.0f);

	ImGui::SeparatorText("文字");
	ImGui::Text("Font: %s / weight %d", evolutionUiStyle_.fontFamily.c_str(), evolutionUiStyle_.fontWeight);
	ImGui::TextDisabled("%s", evolutionUiStyle_.fontPath.c_str());
	ImGui::DragFloat("タイトル文字サイズ", &evolutionUiStyle_.titleFontSize, 0.5f, 8.0f, 96.0f);
	ImGui::DragFloat("クラス名文字サイズ", &evolutionUiStyle_.classNameFontSize, 0.5f, 8.0f, 72.0f);
	ImGui::DragFloat("本文文字サイズ", &evolutionUiStyle_.bodyFontSize, 0.5f, 8.0f, 48.0f);
	ImGui::DragFloat("ボタン文字サイズ", &evolutionUiStyle_.buttonFontSize, 0.5f, 8.0f, 48.0f);
	ImGui::ColorEdit4("タイトル文字色", &evolutionUiStyle_.titleTextColor.x);
	ImGui::ColorEdit4("クラス名文字色", &evolutionUiStyle_.classTextColor.x);
	ImGui::ColorEdit4("本文文字色", &evolutionUiStyle_.bodyTextColor.x);
	ImGui::ColorEdit4("ボタン文字色", &evolutionUiStyle_.buttonTextColor.x);
	ImGui::ColorEdit4("文字アウトライン色", &evolutionUiStyle_.textOutlineColor.x);
	ImGui::DragFloat("タイトルアウトライン幅", &evolutionUiStyle_.titleOutlineWidth, 0.05f, 0.0f, 4.0f);
	ImGui::DragFloat("クラス名アウトライン幅", &evolutionUiStyle_.classNameOutlineWidth, 0.05f, 0.0f, 4.0f);
	ImGui::DragFloat("本文アウトライン幅", &evolutionUiStyle_.bodyOutlineWidth, 0.05f, 0.0f, 4.0f);
	ImGui::DragFloat("ボタンアウトライン幅", &evolutionUiStyle_.buttonOutlineWidth, 0.05f, 0.0f, 4.0f);
	ImGui::SeparatorText("文字ネオン");
	ImGui::Checkbox("文字ネオンを有効化", &evolutionUiStyle_.neonText.enabled);
	ImGui::ColorEdit4("文字発光色", &evolutionUiStyle_.neonText.glowColor.x);
	ImGui::DragFloat("発光源輝度", &evolutionUiStyle_.neonText.sourceBrightness, 0.02f, 0.0f, 8.0f);
	ImGui::DragFloat("抽出しきい値", &evolutionUiStyle_.neonText.threshold, 0.01f, 0.0f, 4.0f);
	ImGui::DragFloat("内光強度", &evolutionUiStyle_.neonText.innerIntensity, 0.01f, 0.0f, 4.0f);
	ImGui::DragFloat("外光強度", &evolutionUiStyle_.neonText.outerIntensity, 0.01f, 0.0f, 4.0f);

	ImGui::SeparatorText("状態色");
	ImGui::ColorEdit4("通常色", &evolutionUiStyle_.normalColor.x);
	ImGui::ColorEdit4("選択可能色", &evolutionUiStyle_.availableColor.x);
	ImGui::ColorEdit4("ホバー色", &evolutionUiStyle_.hoverColor.x);
	ImGui::ColorEdit4("選択色", &evolutionUiStyle_.selectedColor.x);
	ImGui::ColorEdit4("ロック色", &evolutionUiStyle_.lockedColor.x);
	ImGui::ColorEdit4("パネル色", &evolutionUiStyle_.panelColor.x);

	ImGui::SeparatorText("デバッグ表示");
	ImGui::Checkbox("仮想画面領域", &showEvolutionVirtualBounds_);
	ImGui::Checkbox("セーフエリア", &showEvolutionSafeArea_);
	ImGui::Checkbox("ノード描画領域", &showEvolutionNodeBounds_);
	ImGui::Checkbox("マウス判定領域", &showEvolutionMouseBounds_);
	ImGui::Checkbox("TextLabel領域", &showEvolutionTextBounds_);
	ImGui::Checkbox("画面中央線", &showEvolutionCenterLines_);
	ImGui::Checkbox("接続回路の制御点", &showEvolutionCircuitControlPoints_);
	ImGui::Checkbox("実解像度とUI倍率", &showEvolutionResolutionInfo_);
#endif
}

void Player::DrawStaticEvolutionPrototype()
{
	evolutionUiProfile_.visible = true;
	const auto totalStart = std::chrono::steady_clock::now();
	const auto spriteStart = totalStart;
	SpriteCommon::GetInstance()->PreDraw(kNormal);
	auto drawSprite = [&](const std::unique_ptr<Sprite>& sprite) {
		if (sprite && sprite->GetColor().w > 0.001f && sprite->GetSize().x > 0.0f && sprite->GetSize().y > 0.0f) {
			sprite->Draw();
			++evolutionUiProfile_.spriteDraws;
		}
	};

	for (const auto& line : staticEvolutionCircuitSprites_) {
		drawSprite(line);
	}
	drawSprite(staticEvolutionBranchGlowSprite_);
	drawSprite(staticEvolutionBranchCoreSprite_);
	drawSprite(staticEvolutionDetailPanelSprite_);
	for (size_t i = 0; i < staticEvolutionCandidateCount_ + 1; ++i) {
		if (staticEvolutionTankButtons_[i]) {
			staticEvolutionTankButtons_[i]->Draw();
		}
	}
	drawSprite(staticEvolutionConfirmButtonSprite_);
	for (const auto& line : staticEvolutionConfirmOutlineSprites_) {
		drawSprite(line);
	}
	const auto spriteEnd = std::chrono::steady_clock::now();

	const auto textStart = spriteEnd;
	SpriteCommon::GetInstance()->PreDraw(kNormal);
	auto drawLabel = [&](const std::unique_ptr<TextLabel>& label) {
		if (label) {
			label->Draw();
			++evolutionUiProfile_.textDraws;
		}
	};
	drawLabel(staticEvolutionTitleLabel_);
	const bool anyDebugOverlay =
		showEvolutionVirtualBounds_ ||
		showEvolutionSafeArea_ ||
		showEvolutionNodeBounds_ ||
		showEvolutionMouseBounds_ ||
		showEvolutionTextBounds_ ||
		showEvolutionCenterLines_ ||
		showEvolutionCircuitControlPoints_ ||
		showEvolutionResolutionInfo_;
	if (anyDebugOverlay) {
		drawLabel(staticEvolutionPrototypeLabel_);
	}
	drawLabel(staticEvolutionDetailClassLabel_);
	drawLabel(staticEvolutionRoleLabel_);
	for (const auto& label : staticEvolutionDeltaLabels_) {
		drawLabel(label);
	}
	drawLabel(staticEvolutionAbilityLabel_);
	drawLabel(staticEvolutionConfirmLabel_);
	drawLabel(staticEvolutionPanelHintLabel_);
	const auto textEnd = std::chrono::steady_clock::now();

	DrawStaticEvolutionDebugOverlay();
	const auto totalEnd = std::chrono::steady_clock::now();
	evolutionUiProfile_.spriteMs = std::chrono::duration<float, std::milli>(spriteEnd - spriteStart).count();
	evolutionUiProfile_.textMs = std::chrono::duration<float, std::milli>(textEnd - textStart).count();
	evolutionUiProfile_.updateMs = 0.0f;
	evolutionUiProfile_.totalMs = std::chrono::duration<float, std::milli>(totalEnd - totalStart).count();
}

void Player::DrawEvolutionAfterPostEffects()
{
	if (!isChangeMode || !ShouldUseStaticEvolutionPrototype()) {
		return;
	}
	SpriteCommon::GetInstance()->PreDraw(kNormal);
	if (staticEvolutionBackdropSprite_) {
		staticEvolutionBackdropSprite_->Draw();
	}
	if (staticEvolutionButtonBloomEffect_) {
		staticEvolutionButtonBloomEffect_->BeginCapture();
		SpriteCommon::GetInstance()->PreDrawForScene(kNormal);
		for (const auto& line : staticEvolutionCircuitSprites_) {
			if (line &&
				line->GetSize().x > 0.0f &&
				line->GetSize().y > 0.0f &&
				line->GetColor().w > 0.001f) {
				line->Draw();
			}
		}
		if (staticEvolutionBranchGlowSprite_ &&
			staticEvolutionBranchGlowSprite_->GetSize().x > 0.0f &&
			staticEvolutionBranchGlowSprite_->GetColor().w > 0.001f) {
			staticEvolutionBranchGlowSprite_->Draw();
		}
		if (staticEvolutionBranchCoreSprite_ &&
			staticEvolutionBranchCoreSprite_->GetSize().x > 0.0f &&
			staticEvolutionBranchCoreSprite_->GetColor().w > 0.001f) {
			staticEvolutionBranchCoreSprite_->Draw();
		}
		for (size_t i = 0; i < staticEvolutionCandidateCount_ + 1; ++i) {
			if (staticEvolutionTankButtons_[i]) {
				staticEvolutionTankButtons_[i]->DrawBloomSource();
			}
		}
		staticEvolutionButtonBloomEffect_->EndCaptureBloomOnlyToBackBuffer();
	}
	if (staticEvolutionTextEffect_) {
		std::vector<TextLabel*> neonLabels;
		neonLabels.reserve(4);
		if (staticEvolutionTitleLabel_) {
			neonLabels.push_back(staticEvolutionTitleLabel_.get());
		}
		if (staticEvolutionDetailClassLabel_) {
			neonLabels.push_back(staticEvolutionDetailClassLabel_.get());
		}
		if (staticEvolutionCandidateCount_ > 0) {
			const size_t selectedNode = static_cast<size_t>(
				(std::clamp)(evolutionUiStyle_.fixedSelectedCandidate, 0, static_cast<int>(staticEvolutionCandidateCount_ - 1))) + 1;
			if (selectedNode < staticEvolutionTankButtons_.size() && staticEvolutionTankButtons_[selectedNode]) {
				neonLabels.push_back(staticEvolutionTankButtons_[selectedNode]->GetLabel());
			}
		}
		staticEvolutionTextEffect_->DrawBloom(neonLabels);
	}
}

void Player::DrawStaticEvolutionDebugOverlay()
{
#ifdef USE_IMGUI
	if (!showEvolutionVirtualBounds_ &&
		!showEvolutionSafeArea_ &&
		!showEvolutionNodeBounds_ &&
		!showEvolutionMouseBounds_ &&
		!showEvolutionTextBounds_ &&
		!showEvolutionCenterLines_ &&
		!showEvolutionCircuitControlPoints_ &&
		!showEvolutionResolutionInfo_) {
		return;
	}

	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	const float clientWidth = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
	const float clientHeight = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
	const float virtualWidth = (std::max)(1.0f, evolutionUiStyle_.virtualResolution.x);
	const float virtualHeight = (std::max)(1.0f, evolutionUiStyle_.virtualResolution.y);
	const float scale = (std::max)(0.0001f, (std::min)(clientWidth / virtualWidth, clientHeight / virtualHeight));
	const Vector2 offset = {
		(clientWidth - virtualWidth * scale) * 0.5f,
		(clientHeight - virtualHeight * scale) * 0.5f
	};
	auto toClient = [&](const Vector2& point) {
		return ImVec2(offset.x + point.x * scale, offset.y + point.y * scale);
	};
	auto drawCenteredRect = [&](const Vector2& center, const Vector2& size, ImU32 color) {
		const Vector2 minPoint{ center.x - size.x * 0.5f, center.y - size.y * 0.5f };
		const Vector2 maxPoint{ center.x + size.x * 0.5f, center.y + size.y * 0.5f };
		drawList->AddRect(toClient(minPoint), toClient(maxPoint), color, 0.0f, 0, 1.5f);
	};

	if (showEvolutionVirtualBounds_) {
		drawList->AddRect(
			toClient({ 0.0f, 0.0f }),
			toClient(evolutionUiStyle_.virtualResolution),
			IM_COL32(255, 210, 70, 230),
			0.0f,
			0,
			2.0f);
	}
	if (showEvolutionSafeArea_) {
		const float safe = evolutionUiStyle_.safeMargin;
		drawList->AddRect(
			toClient({ safe, safe }),
			toClient({ virtualWidth - safe, virtualHeight - safe }),
			IM_COL32(80, 255, 160, 230),
			0.0f,
			0,
			2.0f);
	}
	if (showEvolutionCenterLines_) {
		drawList->AddLine(ImVec2(clientWidth * 0.5f, 0.0f), ImVec2(clientWidth * 0.5f, clientHeight), IM_COL32(255, 80, 180, 180), 1.0f);
		drawList->AddLine(ImVec2(0.0f, clientHeight * 0.5f), ImVec2(clientWidth, clientHeight * 0.5f), IM_COL32(255, 80, 180, 180), 1.0f);
	}
	for (size_t i = 0; i < staticEvolutionCandidateCount_ + 1; ++i) {
		if (showEvolutionNodeBounds_) {
			drawCenteredRect(staticEvolutionNodeCentersVirtual_[i], staticEvolutionNodeDrawSizesVirtual_[i], IM_COL32(75, 190, 255, 230));
		}
		if (showEvolutionMouseBounds_) {
			drawCenteredRect(staticEvolutionNodeCentersVirtual_[i], staticEvolutionNodeHitSizesVirtual_[i], IM_COL32(255, 120, 70, 210));
		}
	}
	if (showEvolutionCircuitControlPoints_) {
		for (int pathIndex = 0; pathIndex < static_cast<int>(staticEvolutionCircuitControlPoints_.size()); ++pathIndex) {
			const int count = staticEvolutionCircuitControlPointCounts_[pathIndex];
			for (int i = 0; i < count; ++i) {
				const ImVec2 p = toClient(staticEvolutionCircuitControlPoints_[pathIndex][i]);
				drawList->AddCircleFilled(p, 4.0f, IM_COL32(255, 235, 90, 240), 12);
				if (i + 1 < count) {
					drawList->AddLine(p, toClient(staticEvolutionCircuitControlPoints_[pathIndex][i + 1]), IM_COL32(255, 235, 90, 150), 1.0f);
				}
			}
		}
	}
	if (showEvolutionTextBounds_) {
		auto drawTextBounds = [&](const std::unique_ptr<TextLabel>& label) {
			if (!label || !label->GetSprite()) {
				return;
			}
			Sprite* sprite = label->GetSprite();
			const float renderScale = GetEvolutionRenderScale();
			const Vector2 renderOffset = GetEvolutionRenderOffset();
			const Vector2 renderPosition = sprite->GetPosition();
			const Vector2 renderSize = sprite->GetSize();
			const Vector2 anchor = sprite->GetAnchorPoint();
			const Vector2 virtualPosition = {
				(renderPosition.x - renderOffset.x) / renderScale,
				(renderPosition.y - renderOffset.y) / renderScale
			};
			const Vector2 virtualSize = { renderSize.x / renderScale, renderSize.y / renderScale };
			const Vector2 minPoint = {
				virtualPosition.x - virtualSize.x * anchor.x,
				virtualPosition.y - virtualSize.y * anchor.y
			};
			drawList->AddRect(
				toClient(minPoint),
				toClient({ minPoint.x + virtualSize.x, minPoint.y + virtualSize.y }),
				IM_COL32(190, 110, 255, 220),
				0.0f,
				0,
				1.0f);
		};
		drawTextBounds(staticEvolutionTitleLabel_);
		drawTextBounds(staticEvolutionPrototypeLabel_);
		for (size_t i = 0; i < staticEvolutionCandidateCount_ + 1; ++i) {
			drawTextBounds(staticEvolutionNodeNameLabels_[i]);
			drawTextBounds(staticEvolutionNodeRankLabels_[i]);
		}
		drawTextBounds(staticEvolutionDetailClassLabel_);
		drawTextBounds(staticEvolutionRoleLabel_);
		for (const auto& label : staticEvolutionDeltaLabels_) {
			drawTextBounds(label);
		}
		drawTextBounds(staticEvolutionAbilityLabel_);
		drawTextBounds(staticEvolutionConfirmLabel_);
		drawTextBounds(staticEvolutionPanelHintLabel_);
	}
	if (showEvolutionResolutionInfo_) {
		char buffer[160]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"Evolution UI  %.0f x %.0f  scale %.3f  virtual %.0f x %.0f",
			clientWidth,
			clientHeight,
			scale,
			virtualWidth,
			virtualHeight);
		drawList->AddText(ImVec2(18.0f, clientHeight - 28.0f), IM_COL32(220, 250, 255, 255), buffer);
	}
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

void Player::UpdateEncyclopedia(float uiDeltaTime)
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
	evolutionUiTimer_ += (std::max)(0.0f, uiDeltaTime);
	if (ShouldUseStaticEvolutionPrototype()) {
		UpdateStaticEvolutionPrototype();
		return;
	}
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
				TryConfirmEvolutionById(selectedTank.classId);
			}
		} else {
			evolutionChangeButtonSprite_->SetColor({ 0.24f, 0.86f, 0.44f, 0.92f });
		}
		evolutionChangeButtonSprite_->Update();
	}
}

void Player::DrawEncyclopedia() {

	evolutionUiProfile_ = {};
	if (isChangeMode && ShouldUseStaticEvolutionPrototype()) {
		DrawStaticEvolutionPrototype();
		return;
	}
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
	auto toImColor = [](const Vector4& color) {
		return IM_COL32(
			static_cast<int>(std::clamp(color.x, 0.0f, 1.0f) * 255.0f),
			static_cast<int>(std::clamp(color.y, 0.0f, 1.0f) * 255.0f),
			static_cast<int>(std::clamp(color.z, 0.0f, 1.0f) * 255.0f),
			static_cast<int>(std::clamp(color.w, 0.0f, 1.0f) * 255.0f));
	};
	const ImU32 bodyFill = toImColor(selectedConfig->bodyFillColor);
	const ImU32 bodyOutline = toImColor(selectedConfig->bodyOutlineColor);

	auto addRotatedRect = [&](ImVec2 origin, ImVec2 axisX, ImVec2 axisY, float halfX, float halfY, ImU32 fill, ImU32 outline) {
		const ImVec2 p0 = ImVec2(origin.x - axisX.x * halfX - axisY.x * halfY, origin.y - axisX.y * halfX - axisY.y * halfY);
		const ImVec2 p1 = ImVec2(origin.x + axisX.x * halfX - axisY.x * halfY, origin.y + axisX.y * halfX - axisY.y * halfY);
		const ImVec2 p2 = ImVec2(origin.x + axisX.x * halfX + axisY.x * halfY, origin.y + axisX.y * halfX + axisY.y * halfY);
		const ImVec2 p3 = ImVec2(origin.x - axisX.x * halfX + axisY.x * halfY, origin.y - axisX.y * halfX + axisY.y * halfY);
		drawList->AddQuadFilled(p0, p1, p2, p3, fill);
		drawList->AddQuad(p0, p1, p2, p3, outline, 2.0f);
	};
	auto addPolygonBody = [&](int segments, float rotationRad, ImU32 fill, ImU32 outline) {
		segments = (std::clamp)(segments, 3, 48);
		std::vector<ImVec2> points;
		points.reserve(static_cast<size_t>(segments));
		for (int i = 0; i < segments; ++i) {
			const float angle = rotationRad + static_cast<float>(i) * 6.283185307f / static_cast<float>(segments);
			points.push_back(ImVec2(
				center.x + std::cos(angle) * bodyRadius * selectedConfig->bodyScale.x,
				center.y + std::sin(angle) * bodyRadius * selectedConfig->bodyScale.y));
		}
		drawList->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), fill);
		drawList->AddPolyline(points.data(), static_cast<int>(points.size()), outline, ImDrawFlags_Closed, 3.0f);
	};
	auto groupColor = [](int group) {
		static const ImU32 colors[] = {
			IM_COL32(160, 255, 120, 230),
			IM_COL32(255, 220, 80, 230),
			IM_COL32(90, 230, 255, 230),
			IM_COL32(255, 110, 190, 230),
			IM_COL32(190, 140, 255, 230),
			IM_COL32(255, 150, 95, 230)
		};
		return colors[static_cast<size_t>((std::max)(0, group)) % (sizeof(colors) / sizeof(colors[0]))];
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
		float length = (std::max)(26.0f, barrel.scale.x * 34.0f);
		float width = (std::max)(8.0f, barrel.scale.y * 38.0f);
		if (barrel.barrelShape == BarrelShape::Heavy) {
			length *= 1.12f;
			width *= 1.45f;
		} else if (barrel.barrelShape == BarrelShape::Short) {
			length *= 0.58f;
			width *= 1.08f;
		} else if (barrel.barrelShape == BarrelShape::Wide) {
			length *= 0.86f;
			width *= 1.80f;
		}
		const ImVec2 base = ImVec2(center.x + offset.x + barrelForward.x * length * 0.32f, center.y + offset.y + barrelForward.y * length * 0.32f);
		const ImU32 barrelFill = toImColor(barrel.barrelColor);
		const ImU32 outline = groupColor(barrel.fireGroup);
		if (barrel.barrelShape == BarrelShape::Trapezoid) {
			const float halfBase = width * 0.64f;
			const float halfTip = width * 0.36f;
			const float halfLength = length * 0.5f;
			const ImVec2 p0 = ImVec2(base.x - barrelForward.x * halfLength - barrelRight.x * halfBase, base.y - barrelForward.y * halfLength - barrelRight.y * halfBase);
			const ImVec2 p1 = ImVec2(base.x + barrelForward.x * halfLength - barrelRight.x * halfTip, base.y + barrelForward.y * halfLength - barrelRight.y * halfTip);
			const ImVec2 p2 = ImVec2(base.x + barrelForward.x * halfLength + barrelRight.x * halfTip, base.y + barrelForward.y * halfLength + barrelRight.y * halfTip);
			const ImVec2 p3 = ImVec2(base.x - barrelForward.x * halfLength + barrelRight.x * halfBase, base.y - barrelForward.y * halfLength + barrelRight.y * halfBase);
			drawList->AddQuadFilled(p0, p1, p2, p3, barrelFill);
			drawList->AddQuad(p0, p1, p2, p3, outline, 2.0f);
		} else {
			addRotatedRect(base, barrelForward, barrelRight, length * 0.5f, width * 0.5f, barrelFill, outline);
		}
		muzzlePoints.push_back(ImVec2(base.x + barrelForward.x * length * 0.56f, base.y + barrelForward.y * length * 0.56f));
	}

	switch (selectedConfig->bodyShape) {
	case BodyShape::Box:
		addPolygonBody(4, aimRad + 6.283185307f * 0.125f, bodyFill, bodyOutline);
		break;
	case BodyShape::Triangle:
		addPolygonBody(3, aimRad - 6.283185307f * 0.25f, bodyFill, bodyOutline);
		break;
	case BodyShape::Pentagon:
		addPolygonBody(5, aimRad - 6.283185307f * 0.25f, bodyFill, bodyOutline);
		break;
	case BodyShape::Circle:
	default:
		drawList->AddCircleFilled(center, bodyRadius + 7.0f, IM_COL32(95, 255, 135, 45), 48);
		drawList->AddCircleFilled(center, bodyRadius, bodyFill, 48);
		drawList->AddCircle(center, bodyRadius, bodyOutline, 48, 3.0f);
		break;
	}

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

int Player::GetRankFromLevel(int level) const {
	if (level >= 15) return 4;
	if (level >= 10) return 3;
	if (level >= 5) return 2;
	return 1;
}

void Player::DrawPlayerClassEditor()
{
#ifdef USE_IMGUI
	static bool hasUnsavedEditorChanges = false;
	ImGui::SetNextWindowSize(ImVec2(760.0f, 780.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(500.0f, 24.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowBgAlpha(0.96f);
	if (!ImGui::Begin("プレイヤー機体データ編集ツール")) {
		ImGui::End();
		return;
	}

	ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "EDITOR MODE");
	ImGui::SameLine();
	ImGui::TextDisabled("コードを変更せずに、機体性能・砲性能・特殊行動を調整する開発補助ツール");
	ImGui::TextWrapped("目的: C++を直接編集してビルドし直す手間を減らし、JSON化したプレイヤー機体データを画面上で確認・編集・保存できるようにする。");
	ImGui::Separator();

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

	bool rebuildBarrels = false;
	bool relayoutBarrels = false;
	bool editedThisFrame = false;
	static std::unordered_map<std::string, PlayerClassConfig> editorBaselineConfigs;
	static std::unordered_map<std::string, std::string> editorBaselineLabels;
	static bool editorBaselineInitialized = false;
	auto refreshEditorBaselines = [&]() {
		editorBaselineConfigs.clear();
		editorBaselineLabels.clear();
		for (const std::string& id : classOrder_) {
			if (const PlayerClassConfig* baselineConfig = GetClassConfig(id)) {
				editorBaselineConfigs[id] = *baselineConfig;
				editorBaselineLabels[id] = "JSON保存時";
			}
		}
		editorBaselineInitialized = true;
	};
	if (!editorBaselineInitialized) {
		refreshEditorBaselines();
	}
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

	ImGui::Text("編集中: %s / %s", config->id.c_str(), config->displayName.c_str());
	ImGui::SameLine();
	ImGui::TextDisabled(hasUnsavedEditorChanges ? "保存状態: 未保存の変更あり" : "保存状態: 保存済み");
	ImGui::Text("現在ゲームに反映中: %s", GetCurrentClassName());
	ImGui::Separator();

	if (ImGui::CollapsingHeader("データ操作", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::TextWrapped("JSONの機体データを作成・複製・保存・再読み込みします。新しい機体案を試す入口です。");
		if (ImGui::Button("新規作成")) {
			PlayerClassConfig newConfig = CreateDefaultClassConfig(ClassType::Basic);
			newConfig.id = makeUniqueId("CustomTank");
			newConfig.displayName = newConfig.id;
			classOrder_.push_back(newConfig.id);
			classConfigs_[newConfig.id] = newConfig;
			editorSelectedClassIndex_ = static_cast<int>(classOrder_.size()) - 1;
			selectedId = newConfig.id;
			config = GetMutableClassConfig(selectedId);
			editorBaselineConfigs[newConfig.id] = newConfig;
			editorBaselineLabels[newConfig.id] = "新規作成時";
			hasUnsavedEditorChanges = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("複製")) {
			const PlayerClassConfig sourceConfig = *config;
			PlayerClassConfig newConfig = *config;
			newConfig.id = makeUniqueId(config->id + "_Copy");
			newConfig.displayName = newConfig.id;
			classOrder_.push_back(newConfig.id);
			classConfigs_[newConfig.id] = newConfig;
			editorSelectedClassIndex_ = static_cast<int>(classOrder_.size()) - 1;
			selectedId = newConfig.id;
			config = GetMutableClassConfig(selectedId);
			editorBaselineConfigs[newConfig.id] = sourceConfig;
			editorBaselineLabels[newConfig.id] = "複製元: " + sourceConfig.displayName;
			hasUnsavedEditorChanges = true;
		}
		ImGui::SameLine();
		const bool isLegacyId = config->id == ClassTypeToString(config->type);
		if (!isLegacyId && ImGui::Button("削除")) {
			const bool deletingCurrent = currentClassId_ == config->id;
			classConfigs_.erase(config->id);
			editorBaselineConfigs.erase(config->id);
			editorBaselineLabels.erase(config->id);
			classOrder_.erase(classOrder_.begin() + editorSelectedClassIndex_);
			editorSelectedClassIndex_ = (std::clamp)(editorSelectedClassIndex_, 0, static_cast<int>(classOrder_.size()) - 1);
			if (deletingCurrent) {
				EvolveById("Basic");
			}
			ImGui::End();
			return;
		}
		ImGui::SameLine();
		if (ImGui::Button("JSON保存")) {
			SavePlayerClassConfigs();
			refreshEditorBaselines();
			hasUnsavedEditorChanges = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("JSON再読み込み")) {
			LoadPlayerClassConfigs();
			refreshEditorBaselines();
			rebuildBarrels = true;
			hasUnsavedEditorChanges = false;
		}
	}

	if (ImGui::CollapsingHeader("機体づくり変更サマリー", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::TextWrapped("目的: プログラムを増やさず、既存機体を元にして新しい機体バリエーションを作るための差分確認です。");
		const auto baselineIt = editorBaselineConfigs.find(config->id);
		if (baselineIt == editorBaselineConfigs.end()) {
			ImGui::TextDisabled("比較元がありません。JSON保存または再読み込み後に比較できます。");
		} else {
			const PlayerClassConfig& baseline = baselineIt->second;
			const std::string sourceLabel = editorBaselineLabels.count(config->id) > 0 ? editorBaselineLabels[config->id] : "比較元";
			ImGui::Text("比較元: %s", sourceLabel.c_str());

			int changeCount = 0;
			auto floatChanged = [](float a, float b) {
				return std::abs(a - b) > 0.0005f;
			};
			auto boolText = [](bool value) {
				return value ? "ON" : "OFF";
			};
			auto weaponTypeName = [](WeaponType type) {
				switch (type) {
				case WeaponType::Projectile: return "Projectile";
				case WeaponType::Laser: return "Laser";
				case WeaponType::Mine: return "Mine";
				case WeaponType::Drone: return "Drone";
				case WeaponType::Melee: return "Melee";
				}
				return "Unknown";
			};
			auto addChange = [&](const char* label, const std::string& before, const std::string& after) {
				++changeCount;
				ImGui::BulletText("%s: %s -> %s", label, before.c_str(), after.c_str());
			};
			auto addIntChange = [&](const char* label, int before, int after) {
				if (before != after) {
					addChange(label, std::to_string(before), std::to_string(after));
				}
			};
			auto addFloatChange = [&](const char* label, float before, float after) {
				if (!floatChanged(before, after)) {
					return;
				}
				char beforeText[32]{};
				char afterText[32]{};
				std::snprintf(beforeText, sizeof(beforeText), "%.2f", before);
				std::snprintf(afterText, sizeof(afterText), "%.2f", after);
				addChange(label, beforeText, afterText);
			};
			auto addBoolChange = [&](const char* label, bool before, bool after) {
				if (before != after) {
					addChange(label, boolText(before), boolText(after));
				}
			};
			if (baseline.displayName != config->displayName) {
				addChange("表示名", baseline.displayName, config->displayName);
			}
			addIntChange("必要ランク", baseline.requiredRank, config->requiredRank);
			addBoolChange("ドローン機体", baseline.usesDrone, config->usesDrone);
			addIntChange("最大ドローン数", baseline.maxDrones, config->maxDrones);
			addFloatChange("リロード倍率", baseline.reloadScale, config->reloadScale);
			addFloatChange("弾速倍率", baseline.bulletSpeedScale, config->bulletSpeedScale);
			addFloatChange("弾ダメージ倍率", baseline.bulletDamageScale, config->bulletDamageScale);
			addIntChange("同時発射弾数", baseline.bulletCount, config->bulletCount);
			addFloatChange("拡散角度", baseline.spreadAngleDeg, config->spreadAngleDeg);
			addBoolChange("ランダム拡散", baseline.randomSpread, config->randomSpread);
			addBoolChange("反射弾", baseline.reflect, config->reflect);
			addBoolChange("貫通弾", baseline.penetrate, config->penetrate);
			addBoolChange("全砲塔から発射", baseline.fireAllBarrels, config->fireAllBarrels);
			addBoolChange("砲塔を交互発射", baseline.alternateBarrels, config->alternateBarrels);
			addFloatChange("反動", baseline.recoilPower, config->recoilPower);
			if (baseline.specialActionId != config->specialActionId) {
				addChange("特殊行動", baseline.specialActionId, config->specialActionId);
			}
			addFloatChange("特殊行動クールタイム倍率", baseline.specialActionCooldownScale, config->specialActionCooldownScale);
			addFloatChange("特殊行動スタミナ消費", baseline.specialActionStaminaCost, config->specialActionStaminaCost);
			if (baseline.barrels.size() != config->barrels.size()) {
				addChange("砲塔数", std::to_string(baseline.barrels.size()), std::to_string(config->barrels.size()));
			}
			const size_t comparableBarrels = (std::min)(baseline.barrels.size(), config->barrels.size());
			for (size_t i = 0; i < comparableBarrels && i < 4; ++i) {
				const WeaponMountConfig& before = baseline.barrels[i];
				const WeaponMountConfig& after = config->barrels[i];
				const std::string prefix = "武器マウント" + std::to_string(i) + " ";
				if (before.weaponType != after.weaponType) {
					addChange((prefix + "武器種").c_str(), weaponTypeName(before.weaponType), weaponTypeName(after.weaponType));
				}
				addFloatChange((prefix + "威力倍率").c_str(), before.damageScale, after.damageScale);
				addFloatChange((prefix + "弾速倍率").c_str(), before.projectileSpeedScale, after.projectileSpeedScale);
			}
			if (changeCount == 0) {
				ImGui::TextDisabled("まだ比較元から変わっていません。複製してから弾数や砲塔配置を変えると、ここに変更内容が出ます。");
			} else {
				ImGui::TextColored(ImVec4(0.55f, 1.0f, 0.70f, 1.0f), "変更項目: %d", changeCount);
			}
		}
	}

	if (ImGui::CollapsingHeader("基本情報", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (ImGui::BeginCombo("編集する機体", config->displayName.c_str())) {
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
		DrawEditorHelp("編集対象の機体データを選びます。変更内容はJSON保存するまでファイルには反映されません。");
		ImGui::Text("機体ID: %s", config->id.c_str());
		DrawEditorHelp("ゲーム内部とJSONで使う識別子です。既存機体はIDを固定し、表示名だけ変える運用が安全です。");

		char nameBuffer[64]{};
		strncpy_s(nameBuffer, config->displayName.c_str(), _TRUNCATE);
		if (ImGui::InputText("表示名", nameBuffer, sizeof(nameBuffer))) {
			config->displayName = nameBuffer;
			editedThisFrame = true;
		}
		DrawEditorHelp("ゲーム内や選択画面で見える機体名です。");
		editedThisFrame |= ImGui::DragInt("必要ランク", &config->requiredRank, 1.0f, 1, 4);
		DrawEditorHelp("プレイヤーがこの機体を解放できるランクです。");
	}

	if (ImGui::CollapsingHeader("ネオン外観", ImGuiTreeNodeFlags_DefaultOpen)) {
		const char* bodyShapeNames[] = { "Circle", "Box", "Triangle", "Pentagon" };
		int bodyShapeIndex = static_cast<int>(config->bodyShape);
		if (ImGui::Combo("ボディ形状", &bodyShapeIndex, bodyShapeNames, IM_ARRAYSIZE(bodyShapeNames))) {
			config->bodyShape = static_cast<BodyShape>((std::clamp)(bodyShapeIndex, 0, 3));
			editedThisFrame = true;
		}
		editedThisFrame |= ImGui::DragFloat2("ボディスケール", &config->bodyScale.x, 0.01f, 0.25f, 3.0f);
		editedThisFrame |= ImGui::ColorEdit4("ボディ塗り色", &config->bodyFillColor.x);
		editedThisFrame |= ImGui::ColorEdit4("ボディ枠線色", &config->bodyOutlineColor.x);
		DrawEditorHelp("モデルを増やさず、ネオン枠線描画側の形と色を変更します。");
	}

	if (ImGui::CollapsingHeader("機体性能", ImGuiTreeNodeFlags_DefaultOpen)) {
		editedThisFrame |= ImGui::Checkbox("ドローン機体", &config->usesDrone);
		DrawEditorHelp("有効にすると、この機体はドローンを使用するタイプとして扱われます。");
		editedThisFrame |= ImGui::DragInt("最大ドローン数", &config->maxDrones, 1.0f, 0, 32);
		DrawEditorHelp("同時に扱えるドローンの上限です。");
		editedThisFrame |= ImGui::DragFloat("リロード倍率", &config->reloadScale, 0.01f, 0.05f, 5.0f);
		DrawEditorHelp("射撃間隔にかかる倍率です。小さいほど連射が速くなります。");
		editedThisFrame |= ImGui::DragFloat("反動", &config->recoilPower, 0.001f, 0.0f, 0.5f);
		DrawEditorHelp("射撃時に機体へ加わる押し戻し量です。");
	}

	if (ImGui::CollapsingHeader("砲性能", ImGuiTreeNodeFlags_DefaultOpen)) {
		editedThisFrame |= ImGui::DragInt("同時発射弾数", &config->bulletCount, 1.0f, 1, 16);
		DrawEditorHelp("1回の射撃で出る弾の数です。");
		editedThisFrame |= ImGui::DragFloat("拡散角度", &config->spreadAngleDeg, 0.1f, 0.0f, 180.0f);
		DrawEditorHelp("複数弾を撃つときの広がり角度です。");
		editedThisFrame |= ImGui::Checkbox("全砲塔から発射", &config->fireAllBarrels);
		DrawEditorHelp("有効にすると、登録されている発射可能な砲塔すべてから撃ちます。");
		editedThisFrame |= ImGui::Checkbox("砲塔を交互発射", &config->alternateBarrels);
		DrawEditorHelp("有効にすると、複数砲塔を順番に切り替えて発射します。");
	}

	if (ImGui::CollapsingHeader("弾性能・特殊効果", ImGuiTreeNodeFlags_DefaultOpen)) {
		editedThisFrame |= ImGui::DragFloat("弾速倍率", &config->bulletSpeedScale, 0.01f, 0.05f, 5.0f);
		DrawEditorHelp("基礎弾速にかかる倍率です。大きいほど弾が速く飛びます。");
		editedThisFrame |= ImGui::DragFloat("弾ダメージ倍率", &config->bulletDamageScale, 0.01f, 0.05f, 20.0f);
		DrawEditorHelp("基礎ダメージにかかる倍率です。");
		editedThisFrame |= ImGui::Checkbox("ランダム拡散", &config->randomSpread);
		DrawEditorHelp("有効にすると弾の散り方にランダム性を持たせます。");
		editedThisFrame |= ImGui::Checkbox("反射弾", &config->reflect);
		DrawEditorHelp("有効にすると弾が壁などで反射するタイプになります。");
		editedThisFrame |= ImGui::Checkbox("貫通弾", &config->penetrate);
		DrawEditorHelp("有効にすると弾が敵を貫通するタイプになります。");
	}

	if (ImGui::CollapsingHeader("特殊行動", ImGuiTreeNodeFlags_DefaultOpen)) {
		const auto& specialActions = SpecialActionDefinitions();
		const SpecialActionDefinition* selectedSpecialAction = &specialActions.front();
		for (const SpecialActionDefinition& definition : specialActions) {
			if (config->specialActionId == definition.id) {
				selectedSpecialAction = &definition;
				break;
			}
		}
		if (ImGui::BeginCombo("特殊行動", selectedSpecialAction->displayName)) {
			for (const SpecialActionDefinition& definition : specialActions) {
				const bool selected = config->specialActionId == definition.id;
				if (ImGui::Selectable(definition.displayName, selected)) {
					config->specialActionId = definition.id;
					editedThisFrame = true;
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		DrawEditorHelp("右クリックに割り当てる特殊行動です。未実装のものはデータ上の割り当てだけ行えます。");
		editedThisFrame |= ImGui::DragFloat("特殊行動クールタイム倍率", &config->specialActionCooldownScale, 0.01f, 0.05f, 10.0f);
		DrawEditorHelp("特殊行動の再使用時間にかかる倍率です。小さいほど再使用が早くなります。");
		editedThisFrame |= ImGui::DragFloat("特殊行動スタミナ消費", &config->specialActionStaminaCost, 0.05f, 0.0f, 100.0f);
		DrawEditorHelp("特殊行動を使うときに消費するスタミナ量です。");
		if (config->specialActionId == "saber_counter") {
			editedThisFrame |= ImGui::DragFloat("カウンター受付時間", &config->saberCounterWindow, 0.005f, 0.01f, 2.0f);
			editedThisFrame |= ImGui::DragFloat("カウンター威力倍率", &config->saberCounterDamageScale, 0.05f, 0.0f, 20.0f);
			editedThisFrame |= ImGui::DragFloat("カウンター射程倍率", &config->saberCounterRangeScale, 0.01f, 0.1f, 5.0f);
		}
		if (!selectedSpecialAction->implemented) {
			ImGui::TextDisabled("この特殊行動は割り当てのみ対応しています。");
		}
	}

	ImGui::Separator();
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
	if (ImGui::CollapsingHeader("砲配置・武器マウント", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("砲塔数: %zu", config->barrels.size());
		ImGui::TextWrapped("機体に取り付ける砲塔の位置、武器種、レーザー・地雷・近接などの個別性能を編集します。");
		if (ImGui::TreeNodeEx("砲塔の自動配置", ImGuiTreeNodeFlags_DefaultOpen)) {
		static int layoutCount = 2;
		static float layoutForward = 0.72f;
		static float layoutSideSpacing = 0.34f;
		static float layoutAngleSpread = 0.0f;
		static float layoutMuzzleForward = 0.95f;
		static float layoutArcRadius = 0.62f;
		static float layoutArcCenterAngle = 0.0f;
		static float layoutArcSweepAngle = 80.0f;
		static float layoutArcRotationScale = 1.0f;
		static bool layoutSnapAngles = true;
		static float layoutSnapStepDeg = 15.0f;
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
		ImGui::Checkbox("角度をスナップ", &layoutSnapAngles);
		ImGui::SameLine();
		ImGui::DragFloat("スナップ角", &layoutSnapStepDeg, 1.0f, 1.0f, 90.0f);
		ImGui::DragFloat3("配置スケール", &layoutScale.x, 0.01f, 0.01f, 10.0f);
		auto snapAngle = [](float angleDeg) {
			if (!layoutSnapAngles || layoutSnapStepDeg <= 0.0f) {
				return angleDeg;
			}
			return std::round(angleDeg / layoutSnapStepDeg) * layoutSnapStepDeg;
		};
		auto applyCommonMountStyle = [&](WeaponMountConfig& barrel) {
			if (!config->barrels.empty()) {
				const WeaponMountConfig& source = config->barrels.front();
				barrel.model = source.model;
				barrel.fires = source.fires;
				barrel.weaponType = source.weaponType;
				barrel.effectColor = source.effectColor;
				barrel.damageScale = source.damageScale;
				barrel.projectileSpeedScale = source.projectileSpeedScale;
			}
			barrel.muzzleForward = layoutMuzzleForward;
		};
		auto generateArcLayout = [&]() {
			layoutCount = (std::clamp)(layoutCount, 1, 12);
			constexpr float kDegToRad = 3.1415926535f / 180.0f;
			config->barrels.clear();
			config->barrels.reserve(static_cast<size_t>(layoutCount));
			for (int i = 0; i < layoutCount; ++i) {
				const float centerIndex = (static_cast<float>(layoutCount) - 1.0f) * 0.5f;
				const float normalized = layoutCount <= 1 ? 0.0f : (static_cast<float>(i) - centerIndex) / centerIndex;
				const float angleDeg = snapAngle(layoutArcCenterAngle + normalized * layoutArcSweepAngle * 0.5f);
				const float angleRad = angleDeg * kDegToRad;
				WeaponMountConfig barrel{};
				applyCommonMountStyle(barrel);
				barrel.offset = {
					std::cos(angleRad) * layoutArcRadius,
					std::sin(angleRad) * layoutArcRadius,
					0.0f
				};
				barrel.scale = layoutScale;
				barrel.angleDeg = snapAngle((angleDeg - layoutArcCenterAngle) * layoutArcRotationScale + layoutArcCenterAngle);
				config->barrels.push_back(barrel);
			}
			config->fireAllBarrels = layoutCount > 1;
			config->alternateBarrels = false;
			rebuildBarrels = true;
			editedThisFrame = true;
		};
		auto generateRadialLayout = [&](int count, float radius, float startAngleDeg, float angleOffsetDeg, Vector3 scale, WeaponType forcedType = WeaponType::Projectile) {
			count = (std::clamp)(count, 1, 12);
			constexpr float kDegToRad = 3.1415926535f / 180.0f;
			config->barrels.clear();
			config->barrels.reserve(static_cast<size_t>(count));
			for (int i = 0; i < count; ++i) {
				const float angleDeg = snapAngle(startAngleDeg + 360.0f * static_cast<float>(i) / static_cast<float>(count));
				const float angleRad = angleDeg * kDegToRad;
				WeaponMountConfig barrel{};
				applyCommonMountStyle(barrel);
				barrel.offset = { std::cos(angleRad) * radius, std::sin(angleRad) * radius, 0.0f };
				barrel.scale = scale;
				barrel.angleDeg = snapAngle(angleDeg + angleOffsetDeg);
				barrel.weaponType = forcedType;
				barrel.fires = true;
				config->barrels.push_back(barrel);
			}
			config->fireAllBarrels = count > 1;
			config->alternateBarrels = false;
			rebuildBarrels = true;
			editedThisFrame = true;
		};
		auto generateFixedAngleLayout = [&](const std::vector<float>& anglesDeg, float radius, Vector3 scale, bool fireAll, bool alternate, WeaponType forcedType = WeaponType::Projectile) {
			constexpr float kDegToRad = 3.1415926535f / 180.0f;
			config->barrels.clear();
			config->barrels.reserve(anglesDeg.size());
			for (float rawAngleDeg : anglesDeg) {
				const float angleDeg = snapAngle(rawAngleDeg);
				const float angleRad = angleDeg * kDegToRad;
				WeaponMountConfig barrel{};
				applyCommonMountStyle(barrel);
				barrel.offset = { std::cos(angleRad) * radius, std::sin(angleRad) * radius, 0.0f };
				barrel.scale = scale;
				barrel.angleDeg = angleDeg;
				barrel.weaponType = forcedType;
				barrel.fires = true;
				config->barrels.push_back(barrel);
			}
			config->fireAllBarrels = fireAll;
			config->alternateBarrels = alternate;
			rebuildBarrels = true;
			editedThisFrame = true;
		};
		if (ImGui::Button("左右対称配置を生成")) {
			layoutCount = (std::clamp)(layoutCount, 1, 12);
			config->barrels.clear();
			config->barrels.reserve(static_cast<size_t>(layoutCount));
			for (int i = 0; i < layoutCount; ++i) {
				const float centerIndex = (static_cast<float>(layoutCount) - 1.0f) * 0.5f;
				const float normalized = layoutCount <= 1 ? 0.0f : (static_cast<float>(i) - centerIndex) / centerIndex;
				WeaponMountConfig barrel{};
				applyCommonMountStyle(barrel);
				barrel.offset = { layoutForward, (static_cast<float>(i) - centerIndex) * layoutSideSpacing, 0.0f };
				barrel.scale = layoutScale;
				barrel.angleDeg = snapAngle(normalized * layoutAngleSpread * 0.5f);
				config->barrels.push_back(barrel);
			}
			config->fireAllBarrels = layoutCount > 2;
			config->alternateBarrels = layoutCount == 2;
			rebuildBarrels = true;
			editedThisFrame = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("V字配置を生成")) {
			layoutCount = (std::clamp)(layoutCount, 2, 12);
			config->barrels.clear();
			config->barrels.reserve(static_cast<size_t>(layoutCount));
			for (int i = 0; i < layoutCount; ++i) {
				const float centerIndex = (static_cast<float>(layoutCount) - 1.0f) * 0.5f;
				const float signedIndex = static_cast<float>(i) - centerIndex;
				WeaponMountConfig barrel{};
				applyCommonMountStyle(barrel);
				barrel.offset = {
					layoutForward - std::abs(signedIndex) * 0.12f,
					signedIndex * layoutSideSpacing,
					0.0f
				};
				barrel.scale = layoutScale;
				barrel.angleDeg = snapAngle((layoutCount <= 1 || centerIndex == 0.0f) ? 0.0f : (signedIndex / centerIndex) * layoutAngleSpread * 0.5f);
				config->barrels.push_back(barrel);
			}
			config->fireAllBarrels = true;
			config->alternateBarrels = false;
			rebuildBarrels = true;
			editedThisFrame = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("円弧ファン配置を生成")) {
			generateArcLayout();
		}
		ImGui::SeparatorText("diep.io風プリセット");
		if (ImGui::Button("単砲")) {
			generateFixedAngleLayout({ 0.0f }, 0.72f, { 1.25f, 0.24f, 0.24f }, false, false);
		}
		ImGui::SameLine();
		if (ImGui::Button("ツイン")) {
			layoutCount = 2;
			layoutForward = 0.72f;
			layoutSideSpacing = 0.34f;
			layoutAngleSpread = 0.0f;
			layoutScale = { 1.25f, 0.24f, 0.24f };
			config->barrels.clear();
			for (float side : { -0.5f, 0.5f }) {
				WeaponMountConfig barrel{};
				applyCommonMountStyle(barrel);
				barrel.offset = { layoutForward, side * layoutSideSpacing * 2.0f, 0.0f };
				barrel.scale = layoutScale;
				barrel.angleDeg = 0.0f;
				config->barrels.push_back(barrel);
			}
			config->fireAllBarrels = false;
			config->alternateBarrels = true;
			rebuildBarrels = true;
			editedThisFrame = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("大型砲")) {
			generateFixedAngleLayout({ 0.0f }, 0.74f, { 1.58f, 0.42f, 0.42f }, false, false);
			config->reloadScale = (std::max)(config->reloadScale, 1.35f);
			config->bulletDamageScale = (std::max)(config->bulletDamageScale, 2.0f);
			config->recoilPower = (std::max)(config->recoilPower, 0.035f);
		}
		ImGui::SameLine();
		if (ImGui::Button("トラッパー")) {
			generateFixedAngleLayout({ 0.0f }, 0.64f, { 0.72f, 0.46f, 0.46f }, false, false, WeaponType::Mine);
		}
		if (ImGui::Button("前後砲")) {
			generateFixedAngleLayout({ 0.0f, 180.0f }, 0.70f, { 1.18f, 0.22f, 0.22f }, true, false);
		}
		ImGui::SameLine();
		if (ImGui::Button("左右サイド砲")) {
			generateFixedAngleLayout({ 90.0f, -90.0f }, 0.70f, { 1.18f, 0.22f, 0.22f }, true, false);
		}
		ImGui::SameLine();
		if (ImGui::Button("十字4砲")) {
			generateRadialLayout(4, 0.70f, 0.0f, 0.0f, { 1.14f, 0.22f, 0.22f });
		}
		ImGui::SameLine();
		if (ImGui::Button("斜め4砲")) {
			generateRadialLayout(4, 0.70f, 45.0f, 0.0f, { 1.14f, 0.22f, 0.22f });
		}
		if (ImGui::Button("オクト8砲")) {
			generateRadialLayout(8, 0.68f, 0.0f, 0.0f, { 1.04f, 0.18f, 0.18f });
		}
		ImGui::SameLine();
		if (ImGui::Button("前方3連")) {
			layoutCount = 3;
			layoutArcRadius = 0.66f;
			layoutArcCenterAngle = 0.0f;
			layoutArcSweepAngle = 70.0f;
			layoutArcRotationScale = 1.0f;
			layoutScale = { 1.25f, 0.24f, 0.24f };
			generateArcLayout();
		}
		ImGui::SameLine();
		if (ImGui::Button("前方5連")) {
			layoutCount = 5;
			layoutArcRadius = 0.66f;
			layoutArcCenterAngle = 0.0f;
			layoutArcSweepAngle = 120.0f;
			layoutArcRotationScale = 0.9f;
			layoutScale = { 1.14f, 0.20f, 0.20f };
			generateArcLayout();
		}
		if (ImGui::Button("ツイン前後")) {
			config->barrels.clear();
			for (float angleDeg : { 0.0f, 180.0f }) {
				const float angleRad = angleDeg * 3.1415926535f / 180.0f;
				const Vector3 forwardOffset = { std::cos(angleRad) * 0.72f, std::sin(angleRad) * 0.72f, 0.0f };
				const Vector3 sideAxis = { -std::sin(angleRad), std::cos(angleRad), 0.0f };
				for (float side : { -0.22f, 0.22f }) {
					WeaponMountConfig barrel{};
					applyCommonMountStyle(barrel);
					barrel.offset = forwardOffset + sideAxis * side;
					barrel.scale = { 1.12f, 0.20f, 0.20f };
					barrel.angleDeg = angleDeg;
					barrel.fireGroup = angleDeg == 0.0f ? 0 : 1;
					config->barrels.push_back(barrel);
				}
			}
			config->fireAllBarrels = false;
			config->alternateBarrels = true;
			rebuildBarrels = true;
			editedThisFrame = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("前方+左右")) {
			generateFixedAngleLayout({ 0.0f, 90.0f, -90.0f }, 0.70f, { 1.14f, 0.21f, 0.21f }, true, false);
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
			ImGui::TreePop();
		}
		if (ImGui::Button("武器マウントを追加")) {
			WeaponMountConfig barrel{};
			if (!config->barrels.empty()) {
				barrel = config->barrels.back();
				barrel.offset.y += 0.34f;
			}
			config->barrels.push_back(barrel);
			rebuildBarrels = true;
			editedThisFrame = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("左右ペアをグループ化")) {
			for (WeaponMountConfig& barrel : config->barrels) {
				const int angleBucket = static_cast<int>(std::round(barrel.angleDeg / 15.0f));
				barrel.fireGroup = (angleBucket + 12) * 2 + (barrel.offset.y >= 0.0f ? 1 : 0);
			}
			config->fireAllBarrels = false;
			config->alternateBarrels = true;
			editedThisFrame = true;
		}
		if (ImGui::Button("角度ごとにグループ化")) {
			for (WeaponMountConfig& barrel : config->barrels) {
				barrel.fireGroup = static_cast<int>(std::round((barrel.angleDeg + 180.0f) / 15.0f));
			}
			config->fireAllBarrels = false;
			config->alternateBarrels = true;
			editedThisFrame = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("前後を2グループ化")) {
			for (WeaponMountConfig& barrel : config->barrels) {
				const float angle = std::fmod(barrel.angleDeg + 360.0f, 360.0f);
				barrel.fireGroup = (angle > 90.0f && angle < 270.0f) ? 1 : 0;
			}
			config->fireAllBarrels = false;
			config->alternateBarrels = true;
			editedThisFrame = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("全砲を同じグループへ")) {
			for (WeaponMountConfig& barrel : config->barrels) {
				barrel.fireGroup = 0;
			}
			editedThisFrame = true;
		}
		if (ImGui::Button("グループ交互射撃を有効化")) {
			config->fireAllBarrels = false;
			config->alternateBarrels = true;
			editedThisFrame = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("全グループ同時射撃に戻す")) {
			config->fireAllBarrels = true;
			config->alternateBarrels = false;
			editedThisFrame = true;
		}
		if (ImGui::Button("左右ミラーを生成")) {
			std::vector<WeaponMountConfig> mirrored;
			mirrored.reserve(config->barrels.size() * 2);
			for (const WeaponMountConfig& source : config->barrels) {
				if (source.offset.y < -0.001f) {
					continue;
				}
				WeaponMountConfig left = source;
				left.offset.y = -std::abs(source.offset.y);
				left.angleDeg = -source.angleDeg;
				WeaponMountConfig right = source;
				right.offset.y = std::abs(source.offset.y);
				right.angleDeg = source.angleDeg;
				if (std::abs(source.offset.y) <= 0.001f) {
					mirrored.push_back(source);
				} else {
					mirrored.push_back(left);
					mirrored.push_back(right);
				}
			}
			if (!mirrored.empty()) {
				config->barrels = mirrored;
				rebuildBarrels = true;
				editedThisFrame = true;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("選択風: 右側を左へ同期")) {
			for (WeaponMountConfig& right : config->barrels) {
				if (right.offset.y <= 0.001f) {
					continue;
				}
				for (WeaponMountConfig& left : config->barrels) {
					if (left.offset.y >= -0.001f) {
						continue;
					}
					if (std::abs(std::abs(left.offset.y) - right.offset.y) < 0.05f &&
						std::abs(left.offset.x - right.offset.x) < 0.05f) {
						left = right;
						left.offset.y = -right.offset.y;
						left.angleDeg = -right.angleDeg;
						break;
					}
				}
			}
			rebuildBarrels = true;
			editedThisFrame = true;
		}
		{
			std::vector<int> groups;
			for (const WeaponMountConfig& barrel : config->barrels) {
				if (!barrel.fires) {
					continue;
				}
				const int group = (std::max)(0, barrel.fireGroup);
				if (std::find(groups.begin(), groups.end(), group) == groups.end()) {
					groups.push_back(group);
				}
			}
			std::sort(groups.begin(), groups.end());
			ImGui::Text("発射グループ数: %zu / モード: %s",
				groups.size(),
				(config->alternateBarrels && !config->fireAllBarrels && groups.size() > 1) ? "グループ交互" :
				(config->fireAllBarrels ? "全砲同時" : "砲塔交互"));
			if (config->id == currentClassId_ && !weaponGroupCooldowns_.empty()) {
				ImGui::Text("実行中グループCD: ");
				for (size_t groupIndex = 0; groupIndex < groups.size() && groupIndex < weaponGroupCooldowns_.size(); ++groupIndex) {
					ImGui::SameLine();
					ImGui::Text("[%d %.2f]", groups[groupIndex], weaponGroupCooldowns_[groupIndex]);
				}
			}
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
					editedThisFrame = true;
				}
				const char* barrelShapeNames[] = { "Box", "Heavy", "Short", "Wide", "Trapezoid" };
				int barrelShapeIndex = static_cast<int>(barrel.barrelShape);
				if (ImGui::Combo("ネオン砲身形状", &barrelShapeIndex, barrelShapeNames, IM_ARRAYSIZE(barrelShapeNames))) {
					barrel.barrelShape = static_cast<BarrelShape>((std::clamp)(barrelShapeIndex, 0, 4));
					editedThisFrame = true;
				}
				relayoutBarrels |= ImGui::DragFloat3("位置 X/Y/Z", &barrel.offset.x, 0.01f, -10.0f, 10.0f);
				relayoutBarrels |= ImGui::DragFloat3("スケール", &barrel.scale.x, 0.01f, 0.0f, 10.0f);
				relayoutBarrels |= ImGui::DragFloat("角度", &barrel.angleDeg, 0.1f, -180.0f, 180.0f);
				editedThisFrame |= ImGui::DragFloat("銃口の前方オフセット", &barrel.muzzleForward, 0.01f, -2.0f, 5.0f);
				editedThisFrame |= ImGui::Checkbox("マウントを有効化", &barrel.fires);
				const char* weaponTypeNames[] = { "Projectile", "Laser", "Mine", "Drone (準備中)", "Melee" };
				int weaponTypeIndex = static_cast<int>(barrel.weaponType);
				if (ImGui::Combo("武器種", &weaponTypeIndex, weaponTypeNames, IM_ARRAYSIZE(weaponTypeNames))) {
					barrel.weaponType = static_cast<WeaponType>((std::clamp)(weaponTypeIndex, 0, 4));
					editedThisFrame = true;
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
				ImGui::DragFloat("コンボリセット時間", &barrel.meleeComboResetTime, 0.01f, 0.05f, 3.0f);
				ImGui::DragFloat("1段目 ダメージ倍率", &barrel.meleeCombo1DamageScale, 0.01f, 0.0f, 10.0f);
				ImGui::DragFloat("2段目 ダメージ倍率", &barrel.meleeCombo2DamageScale, 0.01f, 0.0f, 10.0f);
				ImGui::DragFloat("3段目 ダメージ倍率", &barrel.meleeCombo3DamageScale, 0.01f, 0.0f, 10.0f);
				ImGui::DragFloat("1段目 射程倍率", &barrel.meleeCombo1RangeScale, 0.01f, 0.05f, 5.0f);
				ImGui::DragFloat("2段目 射程倍率", &barrel.meleeCombo2RangeScale, 0.01f, 0.05f, 5.0f);
				ImGui::DragFloat("3段目 射程倍率", &barrel.meleeCombo3RangeScale, 0.01f, 0.05f, 5.0f);
				ImGui::DragFloat("1段目 予備動作", &barrel.meleeCombo1Windup, 0.005f, 0.0f, 1.5f);
				ImGui::DragFloat("2段目 予備動作", &barrel.meleeCombo2Windup, 0.005f, 0.0f, 1.5f);
				ImGui::DragFloat("3段目 予備動作", &barrel.meleeCombo3Windup, 0.005f, 0.0f, 1.5f);
				ImGui::DragFloat("1段目 後隙", &barrel.meleeCombo1Recovery, 0.005f, 0.0f, 1.5f);
				ImGui::DragFloat("2段目 後隙", &barrel.meleeCombo2Recovery, 0.005f, 0.0f, 1.5f);
				ImGui::DragFloat("3段目 後隙", &barrel.meleeCombo3Recovery, 0.005f, 0.0f, 1.5f);
			} else if (barrel.weaponType != WeaponType::Projectile) {
				ImGui::TextDisabled("この武器種は次の実装段階まで発射されません。");
			}
			editedThisFrame |= ImGui::ColorEdit4("エフェクト色", &barrel.effectColor.x);
			editedThisFrame |= ImGui::ColorEdit4("砲身塗り色", &barrel.barrelColor.x);
			editedThisFrame |= ImGui::ColorEdit4("砲身枠線色", &barrel.outlineColor.x);
			ImGui::DragFloat("マウント威力倍率", &barrel.damageScale, 0.01f, 0.0f, 20.0f);
			ImGui::DragFloat("マウント弾速倍率", &barrel.projectileSpeedScale, 0.01f, 0.01f, 10.0f);
			editedThisFrame |= ImGui::DragInt("発射グループ", &barrel.fireGroup, 1.0f, 0, 64);
			editedThisFrame |= ImGui::DragFloat("個別リロード倍率", &barrel.reloadScale, 0.01f, 0.05f, 10.0f);
			editedThisFrame |= ImGui::DragFloat("個別反動倍率", &barrel.recoilScale, 0.01f, 0.0f, 10.0f);
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
	}

	if (config && config->id == currentClassId_ && (rebuildBarrels || relayoutBarrels)) {
		if (rebuildBarrels) {
			InitializeBarrels();
		}
		UpdateBarrelLayout();
	}

	ImGui::Text("メモ: 位置X=前方向、位置Y=横方向、角度=照準からのずれです。");
	ImGui::TextDisabled("WeaponMount v2: 旧 barrels JSONも自動で読み込めます。");
	if (editedThisFrame || relayoutBarrels) {
		hasUnsavedEditorChanges = true;
	}
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
	ParticleManager::GetInstance()->EmitNeonMovementEffect(position, moveDirection);
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
		layout.isMelee = barrel.weaponType == WeaponType::Melee;
		layout.shape = barrel.barrelShape;
		layout.fireGroup = barrel.fireGroup;
		layout.barrelColor = barrel.barrelColor;
		layout.outlineColor = barrel.outlineColor;
		if (i < barrels_.size()) {
			layout.recoilOffset = barrels_[i].recoilOffset;
		}
		layouts.push_back(layout);
	}
	return layouts;
}

Player::NeonBodyLayout Player::GetNeonBodyLayout() const
{
	NeonBodyLayout layout{};
	const PlayerClassConfig* config = GetCurrentClassConfig();
	if (!config) {
		return layout;
	}
	layout.shape = config->bodyShape;
	layout.scale = config->bodyScale;
	layout.fillColor = config->bodyFillColor;
	layout.outlineColor = config->bodyOutlineColor;
	return layout;
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
