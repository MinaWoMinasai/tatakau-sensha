#include "NavalBattleScene.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

#include "Object3dCommon.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
constexpr float kNormalAimLimit = 175.0f;
constexpr float kPrecisionAimLimit = 205.0f;
constexpr float kGunHalfArc = std::numbers::pi_v<float> * 0.66f;
constexpr float kBattleLimit = 760.0f;
constexpr float kEnemyScoutRange = 360.0f;
constexpr float kEnemyOpenFireRange = 245.0f;
constexpr float kEnemyPreferredRange = 150.0f;
constexpr float kEnemyMinRange = 96.0f;
constexpr float kEnemyFleetSpeed = 20.0f;
}

void NavalBattleScene::Initialize()
{
	input_ = Input::GetInstance();
	camera_ = std::make_unique<Camera>();
	debugCamera_ = std::make_unique<DebugCamera>();

	ModelManager::GetInstance()->LoadModel("ground.obj");
	ModelManager::GetInstance()->LoadModel("sea.obj");
	ModelManager::GetInstance()->LoadModel("bullet.obj");
	ModelManager::GetInstance()->LoadModel("navalHullBox.obj");

	Object3dCommon::GetInstance()->SetDefaultCamera(camera_.get());
	Object3dCommon::GetInstance()->SetDebugDefaultCamera(debugCamera_.get());
	Object3dCommon::GetInstance()->SetIsDebugCamera(false);
	Object3dCommon::GetInstance()->SetShadowRange(420.0f);

	sea_ = std::make_unique<Object3d>();
	sea_->Initialize();
	InitializeObject(*sea_, "sea.obj", { 0.72f, 0.92f, 1.0f, 1.0f }, false);
	sea_->SetEnvironmentCoefficient(2.0f);
	sea_->SetScale({ 0.50f, 1.0f, 0.50f });
	sea_->SetTranslate({ 0.0f, -1.2f, 0.0f });

	playerHull_ = std::make_unique<Object3d>();
	playerHull_->Initialize();
	InitializeObject(*playerHull_, "navalHullBox.obj", { 1.0f, 0.08f, 0.04f, 1.0f }, false);

	playerTurret_ = std::make_unique<Object3d>();
	playerTurret_->Initialize();
	InitializeObject(*playerTurret_, "bullet.obj", { 1.0f, 0.85f, 0.10f, 1.0f }, false);

	playerMarker_ = std::make_unique<Object3d>();
	playerMarker_->Initialize();
	InitializeObject(*playerMarker_, "bullet.obj", { 1.0f, 0.95f, 0.05f, 1.0f }, false);

	ResetBattle();
}

void NavalBattleScene::Update()
{
	battleTimer_ += finalDeltaTime_;

	if (input_->IsTrigger(input_->GetKey()[DIK_ESCAPE], input_->GetPreKey()[DIK_ESCAPE])) {
		finished_ = true;
		nextSceneName_ = "TITLE";
		return;
	}
	if (input_->IsTrigger(input_->GetKey()[DIK_R], input_->GetPreKey()[DIK_R])) {
		ResetBattle();
	}

	playerDamageFlash_ = (std::max)(0.0f, playerDamageFlash_ - finalDeltaTime_);
	if (!missionComplete_ && !gameOver_) {
		UpdatePlayer();
		UpdateProjectiles();
	}
	UpdateEnemies();
	UpdateWakeTrails();
	UpdateCamera();

	sea_->SetShininess(battleTimer_);
	sea_->Update();
	playerHull_->Update();
	playerTurret_->Update();
	playerMarker_->Update();
	for (auto& enemy : enemies_) {
		if ((enemy.alive || enemy.sinking) && enemy.hull) {
			enemy.hull->Update();
		}
	}
	for (auto& projectile : projectiles_) {
		if (projectile.object) {
			projectile.object->Update();
		}
	}
	for (auto& impact : impactEffects_) {
		if (impact.object) {
			impact.object->Update();
		}
	}
	for (auto& wake : wakeTrails_) {
		if (wake.object) {
			wake.object->Update();
		}
	}

	DrawDebugWindow();
}

void NavalBattleScene::DrawShadow()
{
	Object3dCommon::GetInstance()->PreDraw(kShadow);
	playerHull_->DrawShadow();
	playerTurret_->DrawShadow();
	playerMarker_->DrawShadow();
	for (auto& enemy : enemies_) {
		if (enemy.alive && enemy.hull) {
			enemy.hull->DrawShadow();
		}
	}
}

void NavalBattleScene::DrawPostEffect3D()
{
	Object3dCommon::GetInstance()->PreDraw(kNone);
	sea_->Draw();

	Object3dCommon::GetInstance()->PreDraw(kNormal);
	for (auto& wake : wakeTrails_) {
		if (wake.object) {
			wake.object->Draw();
		}
	}

	Object3dCommon::GetInstance()->PreDraw(kNone);
	playerHull_->Draw();
	playerTurret_->Draw();
	playerMarker_->Draw();
	for (auto& enemy : enemies_) {
		if ((enemy.alive || enemy.sinking) && enemy.hull) {
			enemy.hull->Draw();
		}
	}
	for (auto& projectile : projectiles_) {
		if (projectile.object) {
			projectile.object->Draw();
		}
	}
	for (auto& impact : impactEffects_) {
		if (impact.object) {
			impact.object->Draw();
		}
	}
}

void NavalBattleScene::ResetBattle()
{
	player_ = {};
	player_.position = { 0.0f, 1.1f, -36.0f };
	player_.yaw = 0.0f;
	player_.maxHp = 120.0f;
	player_.hp = player_.maxHp;
	player_.reload = 0.0f;
	battleTimer_ = 0.0f;
	cameraYawOffset_ = 0.0f;
	cameraPitch_ = 0.42f;
	cameraDistance_ = 70.0f;
	aimOffset_ = {};
	throttleStep_ = 0;
	precisionAimMode_ = false;
	lockTargetIndex_ = -1;
	precisionSwitchCooldown_ = 0.0f;
	projectiles_.clear();
	impactEffects_.clear();
	wakeTrails_.clear();
	enemies_.clear();
	wakeSpawnTimer_ = 0.0f;
	hullFoamSpawnTimer_ = 0.0f;
	playerVisualWaterHeight_ = player_.position.y;
	playerVisualPitch_ = 0.0f;
	playerVisualRoll_ = 0.0f;
	cameraShakeTime_ = 0.0f;
	cameraShakeDuration_ = 0.0f;
	cameraShakeIntensity_ = 0.0f;
	playerDamageFlash_ = 0.0f;
	missionComplete_ = false;
	gameOver_ = false;
	enemyFleetDetected_ = false;
	enemyFleetBroadsideDirection_ = 1;

	const Vector3 fleetCenter = { 0.0f, 1.0f, 360.0f };
	enemyFleetAnchor_ = fleetCenter;
	enemyFleetYaw_ = std::numbers::pi_v<float>;
	enemyFleetSpeed_ = 0.0f;
	const Vector3 enemyOffsets[] = {
		{ 0.0f, 0.0f, 0.0f },
		{ -30.0f, 0.0f, -28.0f },
		{ 30.0f, 0.0f, -28.0f },
		{ -58.0f, 0.0f, -58.0f },
		{ 58.0f, 0.0f, -58.0f },
		{ -22.0f, 0.0f, 36.0f },
		{ 22.0f, 0.0f, 36.0f },
	};

	constexpr int enemyCount = static_cast<int>(sizeof(enemyOffsets) / sizeof(enemyOffsets[0]));
	for (int index = 0; index < enemyCount; ++index) {
		const Vector3& offset = enemyOffsets[index];
		EnemyShip enemy{};
		enemy.ship.position = {
			fleetCenter.x + offset.x,
			fleetCenter.y,
			fleetCenter.z + offset.z,
		};
		enemy.formationOffset = offset;
		enemy.ship.maxHp = index == 0 ? 88.0f : 62.0f;
		enemy.ship.hp = enemy.ship.maxHp;
		enemy.ship.speed = 0.0f;
		enemy.ship.reload = 0.8f + static_cast<float>(index % 4) * 0.34f;
		enemy.desiredRange = kEnemyPreferredRange + static_cast<float>((index % 3) - 1) * 18.0f;
		enemy.broadsideDirection = (index % 2 == 0) ? 1 : -1;
		enemy.hull = std::make_unique<Object3d>();
		enemy.hull->Initialize();
		InitializeObject(*enemy.hull, "navalHullBox.obj", index == 0 ? Vector4{ 0.55f, 1.0f, 0.42f, 1.0f } : Vector4{ 0.40f, 0.95f, 0.35f, 1.0f }, false);
		enemy.hull->SetScale(index == 0 ? Vector3{ 2.3f, 0.50f, 6.4f } : Vector3{ 2.0f, 0.45f, 5.6f });
		enemies_.push_back(std::move(enemy));
	}

	UpdatePlayer();
	UpdateCamera();
	sea_->Update();
	for (auto& enemy : enemies_) {
		if (enemy.hull) {
			enemy.hull->SetTranslate(enemy.ship.position);
			enemy.hull->Update();
		}
	}
}

void NavalBattleScene::InitializeObject(Object3d& object, const std::string& modelPath, const Vector4& color, bool lighting)
{
	object.SetModel(modelPath);
	object.SetColor(color);
	object.SetLighting(lighting);
	object.SetInsensity(0.85f);
}

void NavalBattleScene::UpdatePlayer()
{
	const float dt = finalDeltaTime_;
	const float acceleration = 12.0f;
	const float drag = 6.0f;
	const float maxForwardSpeed = 33.0f;
	const float maxReverseSpeed = -8.0f;
	const float turnRate = 0.58f;

	if (input_->IsTrigger(input_->GetKey()[DIK_W], input_->GetPreKey()[DIK_W])) {
		throttleStep_ = std::clamp(throttleStep_ + 1, -1, 3);
	}
	if (input_->IsTrigger(input_->GetKey()[DIK_S], input_->GetPreKey()[DIK_S])) {
		throttleStep_ = std::clamp(throttleStep_ - 1, -1, 3);
	}
	if (input_->IsTrigger(input_->GetKey()[DIK_X], input_->GetPreKey()[DIK_X])) {
		throttleStep_ = 0;
	}
	const Vector2 leftStick = input_->GetLeftStick();
	if (std::abs(leftStick.y) > 0.25f) {
		throttleStep_ = leftStick.y >= 0.0f
			? static_cast<int>(std::round(std::clamp(leftStick.y, 0.0f, 1.0f) * 3.0f))
			: -1;
	}

	const float targetSpeed = throttleStep_ >= 0
		? maxForwardSpeed * (static_cast<float>(throttleStep_) / 3.0f)
		: maxReverseSpeed;
	const float speedDiff = targetSpeed - player_.speed;
	if (std::abs(speedDiff) > 0.001f) {
		const float change = (std::min)(std::abs(speedDiff), acceleration * dt);
		player_.speed += std::copysign(change, speedDiff);
	} else if (std::abs(player_.speed) > 0.001f) {
		const float brake = (std::min)(std::abs(player_.speed), drag * dt);
		player_.speed -= std::copysign(brake, player_.speed);
	}
	player_.speed = std::clamp(player_.speed, maxReverseSpeed, maxForwardSpeed);

	float rudder = 0.0f;
	if (input_->IsPress(input_->GetKey()[DIK_A])) rudder -= 1.0f;
	if (input_->IsPress(input_->GetKey()[DIK_D])) rudder += 1.0f;
	if (!precisionAimMode_) {
		rudder += leftStick.x;
	}
	rudder = std::clamp(rudder, -1.0f, 1.0f);
	const float speedTurnScale = 0.25f + 0.75f * (std::min)(std::abs(player_.speed) / maxForwardSpeed, 1.0f);
	player_.yaw += rudder * turnRate * speedTurnScale * dt;

	const Vector3 forward = ForwardFromYaw(player_.yaw);
	player_.position.x += forward.x * player_.speed * dt;
	player_.position.z += forward.z * player_.speed * dt;
	player_.position.x = std::clamp(player_.position.x, -kBattleLimit, kBattleLimit);
	player_.position.z = std::clamp(player_.position.z, -kBattleLimit, kBattleLimit);

	player_.reload = (std::max)(0.0f, player_.reload - dt);
	const bool firePressed =
		input_->IsTrigger(input_->GetKey()[DIK_SPACE], input_->GetPreKey()[DIK_SPACE]) ||
		input_->IsTrigger(input_->GetMouseState().rgbButtons[0], input_->GetPreMouseState().rgbButtons[0]) ||
		input_->IsGamepadButtonTrigger(XINPUT_GAMEPAD_RIGHT_SHOULDER);
	if (firePressed) {
		FireMainGun();
	}
	const bool precisionTogglePressed =
		input_->IsTrigger(input_->GetKey()[DIK_C], input_->GetPreKey()[DIK_C]) ||
		input_->IsGamepadButtonTrigger(XINPUT_GAMEPAD_X);
	if (precisionTogglePressed) {
		if (precisionAimMode_) {
			precisionAimMode_ = false;
			lockTargetIndex_ = -1;
		} else {
			const int precisionTarget = FindPrecisionTargetFromAim();
			if (precisionTarget >= 0) {
				precisionAimMode_ = true;
				lockTargetIndex_ = precisionTarget;
				aimOffset_ = {};
				precisionSwitchCooldown_ = 0.22f;
			}
		}
	}

	float cameraInput = 0.0f;
	const bool switchLeftPressed = input_->IsTrigger(input_->GetKey()[DIK_Q], input_->GetPreKey()[DIK_Q]);
	const bool switchRightPressed = input_->IsTrigger(input_->GetKey()[DIK_E], input_->GetPreKey()[DIK_E]);
	if (!precisionAimMode_) {
		if (input_->IsPress(input_->GetKey()[DIK_Q])) cameraInput -= 1.0f;
		if (input_->IsPress(input_->GetKey()[DIK_E])) cameraInput += 1.0f;
	}
	const Vector2 rightStick = input_->GetRightStick();
	cameraYawOffset_ += cameraInput * 1.8f * dt;

	const DIMOUSESTATE mouse = input_->GetMouseState();
	const float aimLimit = precisionAimMode_ ? kPrecisionAimLimit : kNormalAimLimit;
	const float edgeStart = aimLimit * 0.70f;
	precisionSwitchCooldown_ = (std::max)(0.0f, precisionSwitchCooldown_ - dt);
	if (precisionAimMode_) {
		aimOffset_ = {};
		const float switchAxis = std::abs(leftStick.x) > std::abs(rightStick.x) ? leftStick.x : rightStick.x;
		int switchDirection = 0;
		if (switchLeftPressed || switchAxis < -0.68f) {
			switchDirection = -1;
		} else if (switchRightPressed || switchAxis > 0.68f) {
			switchDirection = 1;
		}
		if (switchDirection != 0 && precisionSwitchCooldown_ <= 0.0f) {
			const int nextTarget = FindNextPrecisionTarget(switchDirection);
			if (nextTarget >= 0) {
				lockTargetIndex_ = nextTarget;
				precisionSwitchCooldown_ = 0.28f;
			}
		}
	} else {
		if (input_->IsPress(mouse.rgbButtons[1])) {
			aimOffset_.x += static_cast<float>(mouse.lX) * 1.35f;
			aimOffset_.y += static_cast<float>(mouse.lY) * 1.10f;
		}
		aimOffset_.x += rightStick.x * 420.0f * dt;
		aimOffset_.y += -rightStick.y * 340.0f * dt;

		const float aimLength = std::sqrt(aimOffset_.x * aimOffset_.x + aimOffset_.y * aimOffset_.y);
		if (aimLength > aimLimit) {
			const float invLength = 1.0f / aimLength;
			aimOffset_.x *= aimLimit * invLength;
			aimOffset_.y *= aimLimit * invLength;
		}
		const float edgeX = std::abs(aimOffset_.x) > edgeStart
			? (std::abs(aimOffset_.x) - edgeStart) / (aimLimit - edgeStart) * (aimOffset_.x < 0.0f ? -1.0f : 1.0f)
			: 0.0f;
		const float edgeY = std::abs(aimOffset_.y) > edgeStart
			? (std::abs(aimOffset_.y) - edgeStart) / (aimLimit - edgeStart) * (aimOffset_.y < 0.0f ? -1.0f : 1.0f)
			: 0.0f;
		cameraYawOffset_ += edgeX * 0.95f * dt;
		cameraPitch_ += edgeY * 0.54f * dt;
	}
	cameraPitch_ = std::clamp(cameraPitch_, -1.38f, 1.38f);

	if (mouse.lZ != 0) {
		cameraDistance_ -= static_cast<float>(mouse.lZ) * 0.0045f;
		cameraDistance_ = std::clamp(cameraDistance_, 42.0f, 95.0f);
	}

	const Vector3 shipForward = ForwardFromYaw(player_.yaw);
	const Vector3 shipRight = { std::cos(player_.yaw), 0.0f, -std::sin(player_.yaw) };
	const float waveCenter = SampleOceanHeight(player_.position, battleTimer_);
	const Vector3 bowSample = {
		player_.position.x + shipForward.x * 4.6f,
		player_.position.y,
		player_.position.z + shipForward.z * 4.6f,
	};
	const Vector3 sternSample = {
		player_.position.x - shipForward.x * 4.6f,
		player_.position.y,
		player_.position.z - shipForward.z * 4.6f,
	};
	const Vector3 rightSample = {
		player_.position.x + shipRight.x * 1.9f,
		player_.position.y,
		player_.position.z + shipRight.z * 1.9f,
	};
	const Vector3 leftSample = {
		player_.position.x - shipRight.x * 1.9f,
		player_.position.y,
		player_.position.z - shipRight.z * 1.9f,
	};
	const float speedMotion = 0.70f + std::clamp(std::abs(player_.speed) / 33.0f, 0.0f, 1.0f) * 0.55f;
	const float targetWaterHeight = 1.1f + waveCenter * 0.70f * speedMotion;
	const float targetPitch = std::clamp((SampleOceanHeight(sternSample, battleTimer_) - SampleOceanHeight(bowSample, battleTimer_)) * 0.145f * speedMotion, -0.145f, 0.145f);
	const float targetRoll = std::clamp((SampleOceanHeight(rightSample, battleTimer_) - SampleOceanHeight(leftSample, battleTimer_)) * 0.205f * speedMotion, -0.170f, 0.170f);
	playerVisualWaterHeight_ += (targetWaterHeight - playerVisualWaterHeight_) * (std::min)(1.0f, dt * 4.8f);
	playerVisualPitch_ += (targetPitch - playerVisualPitch_) * (std::min)(1.0f, dt * 3.7f);
	playerVisualRoll_ += (targetRoll - playerVisualRoll_) * (std::min)(1.0f, dt * 4.1f);

	const Vector3 playerVisualPosition = {
		player_.position.x,
		playerVisualWaterHeight_,
		player_.position.z,
	};
	playerHull_->SetTranslate(playerVisualPosition);
	playerHull_->SetRotate({ playerVisualPitch_, player_.yaw, playerVisualRoll_ });
	playerHull_->SetScale({ 1.85f, 0.42f, 5.4f });

	const Vector3 turretOffset = shipForward;
	playerTurret_->SetTranslate({
		playerVisualPosition.x + turretOffset.x * 0.65f,
		playerVisualPosition.y + 0.62f,
		playerVisualPosition.z + turretOffset.z * 0.65f,
		});
	playerTurret_->SetRotate({ 0.0f, player_.yaw + std::numbers::pi_v<float> * 0.5f, 0.0f });
	playerTurret_->SetScale({ 0.32f, 0.32f, 0.32f });

	playerMarker_->SetTranslate({
		playerVisualPosition.x,
		playerVisualPosition.y + 1.55f,
		playerVisualPosition.z,
		});
	playerMarker_->SetRotate({ std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f });
	playerMarker_->SetScale({ 0.36f, 0.36f, 0.36f });
}

void NavalBattleScene::UpdateEnemies()
{
	for (auto& enemy : enemies_) {
		if (!enemy.sinking || !enemy.hull) {
			continue;
		}
		enemy.sinkTimer = (std::max)(0.0f, enemy.sinkTimer - finalDeltaTime_);
		enemy.deathFlash = (std::max)(0.0f, enemy.deathFlash - finalDeltaTime_);
		if (enemy.sinkTimer <= 0.0f) {
			enemy.sinking = false;
			continue;
		}
		const float sinkT = 1.0f - std::clamp(enemy.sinkTimer / 5.0f, 0.0f, 1.0f);
		const float roll = enemy.broadsideDirection * (0.18f + sinkT * 0.72f);
		const Vector3 sinkPosition = {
			enemy.ship.position.x,
			enemy.ship.position.y - sinkT * 4.2f,
			enemy.ship.position.z,
		};
		enemy.hull->SetTranslate(sinkPosition);
		enemy.hull->SetRotate({ sinkT * 0.18f, enemy.ship.yaw, roll });
		enemy.hull->SetScale({ 2.0f, 0.45f, 5.6f });
		if (enemy.deathFlash > 0.0f) {
			const float flash = std::clamp(enemy.deathFlash / 0.7f, 0.0f, 1.0f);
			enemy.hull->SetColor({ 1.0f, 0.34f + flash * 0.44f, 0.12f, 1.0f });
		} else {
			enemy.hull->SetColor({ 0.16f, 0.30f, 0.24f, 0.75f });
		}
	}

	int aliveCount = 0;
	Vector3 aliveCenter{};
	for (auto& enemy : enemies_) {
		if (!enemy.alive) {
			continue;
		}
		++aliveCount;
		aliveCenter.x += enemy.ship.position.x;
		aliveCenter.y += enemy.ship.position.y;
		aliveCenter.z += enemy.ship.position.z;
		const float distance = DistanceXZ(enemy.ship.position, player_.position);
		enemy.detectedPlayer = enemy.detectedPlayer || distance <= kEnemyScoutRange;
		enemyFleetDetected_ = enemyFleetDetected_ || enemy.detectedPlayer;
	}
	if (aliveCount <= 0) {
		if (!missionComplete_ && !gameOver_) {
			missionComplete_ = true;
			AddCameraShake(0.28f, 0.32f);
		}
		return;
	}
	aliveCenter.x /= static_cast<float>(aliveCount);
	aliveCenter.y /= static_cast<float>(aliveCount);
	aliveCenter.z /= static_cast<float>(aliveCount);

	if (!enemyFleetDetected_) {
		enemyFleetAnchor_ = aliveCenter;
	}

	const float fleetDistance = DistanceXZ(enemyFleetAnchor_, player_.position);
	const float toPlayerYaw = std::atan2(player_.position.x - enemyFleetAnchor_.x, player_.position.z - enemyFleetAnchor_.z);
	float desiredFleetYaw = enemyFleetYaw_;
	float desiredFleetSpeed = 0.0f;
	if (enemyFleetDetected_) {
		if (fleetDistance > kEnemyPreferredRange + 28.0f) {
			desiredFleetYaw = toPlayerYaw;
			desiredFleetSpeed = kEnemyFleetSpeed;
		} else if (fleetDistance < kEnemyMinRange) {
			desiredFleetYaw = toPlayerYaw + std::numbers::pi_v<float>;
			desiredFleetSpeed = kEnemyFleetSpeed * 0.72f;
		} else {
			desiredFleetYaw = toPlayerYaw + enemyFleetBroadsideDirection_ * std::numbers::pi_v<float> * 0.5f;
			desiredFleetSpeed = kEnemyFleetSpeed * 0.76f;
		}
	} else {
		desiredFleetYaw = std::numbers::pi_v<float>;
		desiredFleetSpeed = kEnemyFleetSpeed * 0.45f;
	}

	const float fleetYawDelta = NormalizeAngle(desiredFleetYaw - enemyFleetYaw_);
	const float fleetMaxYawStep = 0.22f * finalDeltaTime_;
	enemyFleetYaw_ += std::copysign((std::min)(std::abs(fleetYawDelta), fleetMaxYawStep), fleetYawDelta);
	const float fleetSpeedDiff = desiredFleetSpeed - enemyFleetSpeed_;
	const float fleetSpeedChange = (std::min)(std::abs(fleetSpeedDiff), 3.8f * finalDeltaTime_);
	if (fleetSpeedChange > 0.0001f) {
		enemyFleetSpeed_ += std::copysign(fleetSpeedChange, fleetSpeedDiff);
	}
	const Vector3 fleetForward = ForwardFromYaw(enemyFleetYaw_);
	enemyFleetAnchor_.x += fleetForward.x * enemyFleetSpeed_ * finalDeltaTime_;
	enemyFleetAnchor_.z += fleetForward.z * enemyFleetSpeed_ * finalDeltaTime_;
	enemyFleetAnchor_.x = std::clamp(enemyFleetAnchor_.x, -kBattleLimit, kBattleLimit);
	enemyFleetAnchor_.z = std::clamp(enemyFleetAnchor_.z, -kBattleLimit, kBattleLimit);
	const float anchorDrift = DistanceXZ(enemyFleetAnchor_, aliveCenter);
	if (anchorDrift > 24.0f) {
		const float pull = (std::min)(1.0f, finalDeltaTime_ * 0.85f);
		enemyFleetAnchor_.x += (aliveCenter.x - enemyFleetAnchor_.x) * pull;
		enemyFleetAnchor_.z += (aliveCenter.z - enemyFleetAnchor_.z) * pull;
	}

	const Vector3 fleetRight = { std::cos(enemyFleetYaw_), 0.0f, -std::sin(enemyFleetYaw_) };
	for (auto& enemy : enemies_) {
		if (!enemy.alive) {
			continue;
		}

		const Vector3 formationTarget = {
			enemyFleetAnchor_.x + fleetRight.x * enemy.formationOffset.x + fleetForward.x * enemy.formationOffset.z,
			enemy.ship.position.y,
			enemyFleetAnchor_.z + fleetRight.z * enemy.formationOffset.x + fleetForward.z * enemy.formationOffset.z,
		};
		const float formationDistance = DistanceXZ(enemy.ship.position, formationTarget);
		const float formationYaw = std::atan2(formationTarget.x - enemy.ship.position.x, formationTarget.z - enemy.ship.position.z);
		const float distance = DistanceXZ(enemy.ship.position, player_.position);
		const float enemyToPlayerYaw = std::atan2(player_.position.x - enemy.ship.position.x, player_.position.z - enemy.ship.position.z);
		enemy.ship.reload = (std::max)(0.0f, enemy.ship.reload - finalDeltaTime_);
		enemy.aimYaw += NormalizeAngle(enemyToPlayerYaw - enemy.aimYaw) * (std::min)(1.0f, finalDeltaTime_ * 2.8f);

		const float desiredYaw = formationDistance > 14.0f ? formationYaw : enemyFleetYaw_;
		const float yawDelta = NormalizeAngle(desiredYaw - enemy.ship.yaw);
		const float formationCorrection = std::clamp(formationDistance / 70.0f, 0.0f, 1.0f);
		const float maxTurnRate = 0.18f + formationCorrection * 0.17f;
		enemy.ship.yaw += std::copysign((std::min)(std::abs(yawDelta), maxTurnRate * finalDeltaTime_), yawDelta);

		const float turnPenalty = std::clamp(std::abs(yawDelta) / (std::numbers::pi_v<float> * 0.75f), 0.0f, 1.0f);
		const float formationBoost = std::clamp(formationDistance * 0.085f, 0.0f, 4.2f);
		const float targetSpeed = std::clamp((enemyFleetSpeed_ + formationBoost) * (1.0f - turnPenalty * 0.46f), 2.0f, kEnemyFleetSpeed * 1.02f);
		const float speedDiff = targetSpeed - enemy.ship.speed;
		const float speedChange = (std::min)(std::abs(speedDiff), 4.8f * finalDeltaTime_);
		if (speedChange > 0.0001f) {
			enemy.ship.speed += std::copysign(speedChange, speedDiff);
		}
		const Vector3 forward = ForwardFromYaw(enemy.ship.yaw);
		enemy.ship.position.x += forward.x * enemy.ship.speed * finalDeltaTime_;
		enemy.ship.position.z += forward.z * enemy.ship.speed * finalDeltaTime_;
		enemy.ship.position.x = std::clamp(enemy.ship.position.x, -kBattleLimit, kBattleLimit);
		enemy.ship.position.z = std::clamp(enemy.ship.position.z, -kBattleLimit, kBattleLimit);

		if (enemyFleetDetected_ && distance <= kEnemyOpenFireRange && enemy.ship.reload <= 0.0f) {
			SpawnEnemyShell(enemy);
			enemy.ship.reload = 1.85f + static_cast<float>(std::abs(static_cast<int>(enemy.formationOffset.x)) % 3) * 0.22f;
		}

		enemy.hull->SetTranslate(enemy.ship.position);
		enemy.hull->SetRotate({ 0.0f, enemy.ship.yaw, 0.0f });
		enemy.hull->SetScale({ 2.0f, 0.45f, 5.6f });
		enemy.hull->SetColor(enemy.ship.hp / enemy.ship.maxHp > 0.35f ? Vector4{ 0.40f, 0.95f, 0.35f, 1.0f } : Vector4{ 0.82f, 0.62f, 0.22f, 1.0f });
	}
}

void NavalBattleScene::SpawnEnemyShell(EnemyShip& enemy)
{
	const Vector3 enemyForward = ForwardFromYaw(enemy.ship.yaw);
	const Vector3 enemyRight = { std::cos(enemy.ship.yaw), 0.0f, -std::sin(enemy.ship.yaw) };
	const Vector3 baseTargetPosition = {
		player_.position.x,
		player_.position.y + 1.1f,
		player_.position.z,
	};
	const Vector3 playerVelocity = VelocityFromShip(player_);
	const float gunOffsets[] = { -0.95f, 0.95f };
	for (float side : gunOffsets) {
		const Vector3 muzzlePosition = {
			enemy.ship.position.x + enemyRight.x * side + enemyForward.x * 1.65f,
			enemy.ship.position.y + 1.25f,
			enemy.ship.position.z + enemyRight.z * side + enemyForward.z * 1.65f,
		};
		const float horizontalSpeed = 78.0f;
		const Vector3 targetPosition = PredictBallisticTargetPosition(muzzlePosition, baseTargetPosition, playerVelocity, horizontalSpeed);
		const float shotYaw = std::atan2(targetPosition.x - muzzlePosition.x, targetPosition.z - muzzlePosition.z);
		const Vector3 shotForward = ForwardFromYaw(shotYaw);
		const float verticalVelocity = CalculateBallisticVerticalVelocity(muzzlePosition, targetPosition, horizontalSpeed);

		Projectile projectile{};
		projectile.position = muzzlePosition;
		projectile.velocity = { shotForward.x * horizontalSpeed, verticalVelocity, shotForward.z * horizontalSpeed };
		projectile.life = 4.2f;
		projectile.damage = 9.0f;
		projectile.radius = 4.8f;
		projectile.fromPlayer = false;
		projectile.object = std::make_unique<Object3d>();
		projectile.object->Initialize();
		InitializeObject(*projectile.object, "bullet.obj", { 1.0f, 0.38f, 0.16f, 1.0f }, false);
		projectile.object->SetRotate({ 0.0f, shotYaw + std::numbers::pi_v<float> * 0.5f, 0.0f });
		projectile.object->SetScale({ 0.11f, 0.11f, 0.11f });
		projectiles_.push_back(std::move(projectile));
	}
}

void NavalBattleScene::UpdateProjectiles()
{
	const float waterY = -0.92f;
	const float gravity = -18.0f;
	for (auto& projectile : projectiles_) {
		projectile.life -= finalDeltaTime_;
		projectile.velocity.y += gravity * finalDeltaTime_;
		projectile.position.x += projectile.velocity.x * finalDeltaTime_;
		projectile.position.y += projectile.velocity.y * finalDeltaTime_;
		projectile.position.z += projectile.velocity.z * finalDeltaTime_;

		if (projectile.object) {
			projectile.object->SetTranslate(projectile.position);
			const float yaw = std::atan2(projectile.velocity.x, projectile.velocity.z) + std::numbers::pi_v<float> * 0.5f;
			projectile.object->SetRotate({ 0.0f, yaw, 0.0f });
		}

		if (projectile.position.y <= waterY) {
			SpawnImpactEffect({ projectile.position.x, waterY + 0.55f, projectile.position.z }, true);
			projectile.life = 0.0f;
			continue;
		}

		if (projectile.fromPlayer) {
			for (auto& enemy : enemies_) {
				if (!enemy.alive) {
					continue;
				}
				const float verticalDistance = std::abs(projectile.position.y - enemy.ship.position.y);
				if (DistanceXZ(projectile.position, enemy.ship.position) < projectile.radius && verticalDistance < 5.0f) {
					enemy.ship.hp -= projectile.damage;
					SpawnImpactEffect({ enemy.ship.position.x, enemy.ship.position.y + 1.3f, enemy.ship.position.z }, false);
					projectile.life = 0.0f;
					if (enemy.ship.hp <= 0.0f) {
						enemy.alive = false;
						enemy.sinking = true;
						enemy.sinkTimer = 5.0f;
						enemy.deathFlash = 0.7f;
						enemy.ship.speed = 0.0f;
						AddCameraShake(0.34f, 0.32f);
						SpawnImpactEffect({ enemy.ship.position.x + 2.4f, enemy.ship.position.y + 1.7f, enemy.ship.position.z - 1.8f }, false);
					}
					break;
				}
			}
		} else {
			const float verticalDistance = std::abs(projectile.position.y - player_.position.y);
			if (DistanceXZ(projectile.position, player_.position) < projectile.radius + 1.4f && verticalDistance < 5.2f) {
				player_.hp = (std::max)(0.0f, player_.hp - projectile.damage);
				playerDamageFlash_ = 0.65f;
				SpawnImpactEffect({ player_.position.x, player_.position.y + 1.3f, player_.position.z }, false);
				AddCameraShake(player_.hp <= 0.0f ? 0.95f : 0.56f, player_.hp <= 0.0f ? 0.55f : 0.36f);
				if (player_.hp <= 0.0f && !gameOver_) {
					gameOver_ = true;
					throttleStep_ = 0;
					player_.speed = 0.0f;
				}
				projectile.life = 0.0f;
			}
		}
	}

	projectiles_.erase(
		std::remove_if(projectiles_.begin(), projectiles_.end(), [](const Projectile& projectile) {
			return projectile.life <= 0.0f;
		}),
		projectiles_.end());

	for (auto& impact : impactEffects_) {
		impact.life -= finalDeltaTime_;
		const float elapsed = impact.duration - impact.life;
		const float t = std::clamp(elapsed / impact.duration, 0.0f, 1.0f);
		const float fadeScale = 1.0f + t * 1.4f;
		if (impact.object) {
			if (impact.waterSplash) {
				impact.velocity.y -= 4.2f * finalDeltaTime_;
				impact.position.x += impact.velocity.x * finalDeltaTime_;
				impact.position.y += impact.velocity.y * finalDeltaTime_;
				impact.position.z += impact.velocity.z * finalDeltaTime_;
				const float rise = std::sin(t * std::numbers::pi_v<float>) * impact.verticalScale;
				impact.object->SetTranslate({
					impact.position.x,
					impact.position.y + rise * 0.55f,
					impact.position.z,
					});
				impact.object->SetRotate({ 0.0f, battleTimer_ * 0.7f + impact.baseScale, std::numbers::pi_v<float> * 0.5f });
				impact.object->SetScale({
					impact.baseScale * (0.62f + t * 0.92f),
					impact.baseScale * (0.62f + t * 0.92f),
					impact.verticalScale * (0.58f + std::sin(t * std::numbers::pi_v<float>) * 1.35f),
					});
			} else {
				impact.object->SetTranslate(impact.position);
				impact.object->SetScale({
					impact.baseScale * fadeScale,
					impact.baseScale * fadeScale,
					impact.baseScale * fadeScale,
					});
			}
		}
	}

	impactEffects_.erase(
		std::remove_if(impactEffects_.begin(), impactEffects_.end(), [](const ImpactEffect& impact) {
			return impact.life <= 0.0f;
		}),
		impactEffects_.end());
}

void NavalBattleScene::SpawnImpactEffect(const Vector3& position, bool waterSplash)
{
	ImpactEffect impact{};
	impact.position = position;
	impact.waterSplash = waterSplash;
	impact.duration = waterSplash ? 1.05f : 0.55f;
	impact.life = impact.duration;
	impact.baseScale = waterSplash ? 0.82f : 0.62f;
	impact.verticalScale = waterSplash ? 3.8f : 1.0f;
	if (waterSplash) {
		const float seed = position.x * 0.073f + position.z * 0.041f;
		impact.velocity = {
			std::sin(seed) * 0.85f,
			4.2f,
			std::cos(seed * 1.37f) * 0.85f,
		};
	}
	impact.object = std::make_unique<Object3d>();
	impact.object->Initialize();
	InitializeObject(
		*impact.object,
		"bullet.obj",
		waterSplash ? Vector4{ 0.82f, 0.95f, 1.0f, 1.0f } : Vector4{ 1.0f, 0.38f, 0.08f, 1.0f },
		false);
	impact.object->SetTranslate(position);
	impact.object->SetScale({
		impact.baseScale,
		impact.baseScale,
		impact.baseScale,
		});
	impactEffects_.push_back(std::move(impact));

	if (waterSplash) {
		AddWaterFoamTrail(
			{ position.x, -0.76f, position.z },
			{},
			battleTimer_ * 0.41f,
			0.52f,
			0.0026f,
			0.0026f,
			0.22f);
		AddWaterFoamTrail(
			{ position.x, -0.74f, position.z },
			{},
			battleTimer_ * -0.33f + std::numbers::pi_v<float> * 0.5f,
			0.44f,
			0.0013f,
			0.0042f,
			0.16f);
		const float distance = DistanceXZ(position, player_.position);
		if (distance < 120.0f) {
			const float nearRate = 1.0f - std::clamp(distance / 120.0f, 0.0f, 1.0f);
			AddCameraShake(0.18f + nearRate * 0.58f, 0.22f + nearRate * 0.18f);
		}
	}
}

void NavalBattleScene::UpdateWakeTrails()
{
	const float dt = finalDeltaTime_;
	wakeSpawnTimer_ = (std::max)(0.0f, wakeSpawnTimer_ - dt);
	hullFoamSpawnTimer_ = (std::max)(0.0f, hullFoamSpawnTimer_ - dt);
	const float speedAbs = std::abs(player_.speed);
	if (speedAbs > 1.6f && wakeSpawnTimer_ <= 0.0f) {
		SpawnWakeTrail();
		const float speedRate = std::clamp(speedAbs / 33.0f, 0.0f, 1.0f);
		wakeSpawnTimer_ = 0.20f - speedRate * 0.07f;
	}
	if (speedAbs > 2.4f && hullFoamSpawnTimer_ <= 0.0f) {
		SpawnHullFoamTrail();
		const float speedRate = std::clamp(speedAbs / 33.0f, 0.0f, 1.0f);
		hullFoamSpawnTimer_ = 0.14f - speedRate * 0.045f;
	}

	for (auto& wake : wakeTrails_) {
		wake.life -= dt;
		wake.position.x += wake.velocity.x * dt;
		wake.position.y += wake.velocity.y * dt;
		wake.position.z += wake.velocity.z * dt;
		const float t = std::clamp(1.0f - wake.life / wake.duration, 0.0f, 1.0f);
		const float fade = 1.0f - t;
		if (wake.object) {
			Vector3 visualPosition = wake.position;
			visualPosition.y += t * 0.018f;
			wake.object->SetTranslate(visualPosition);
			wake.object->SetRotate({ 0.0f, wake.yaw, 0.0f });
			wake.object->SetScale({
				wake.baseWidth * (1.0f + t * 1.15f),
				1.0f,
				wake.baseLength * (1.0f + t * 1.05f),
				});
			wake.object->SetAlpha(wake.baseAlpha * fade);
		}
	}

	wakeTrails_.erase(
		std::remove_if(wakeTrails_.begin(), wakeTrails_.end(), [](const WakeTrail& wake) {
			return wake.life <= 0.0f;
		}),
		wakeTrails_.end());
}

void NavalBattleScene::SpawnWakeTrail()
{
	const float speedAbs = std::abs(player_.speed);
	const float speedRate = std::clamp(speedAbs / 33.0f, 0.0f, 1.0f);
	const Vector3 forward = ForwardFromYaw(player_.yaw);
	const Vector3 right = { std::cos(player_.yaw), 0.0f, -std::sin(player_.yaw) };
	const float directionSign = player_.speed >= 0.0f ? 1.0f : -1.0f;
	const Vector3 flowDirection = {
		-forward.x * directionSign,
		0.0f,
		-forward.z * directionSign,
	};
	const Vector3 sternCenter = {
		player_.position.x + flowDirection.x * 4.8f,
		-0.62f,
		player_.position.z + flowDirection.z * 4.8f,
	};

	for (float side : { -1.0f, 1.0f }) {
		WakeTrail wake{};
		wake.yaw = player_.yaw;
		wake.duration = 1.15f + speedRate * 0.62f;
		wake.life = wake.duration;
		wake.baseWidth = 0.00036f + speedRate * 0.00016f;
		wake.baseLength = 0.00125f + speedRate * 0.00085f;
		wake.baseAlpha = 0.28f;
		wake.position = {
			sternCenter.x + right.x * side * 1.25f,
			sternCenter.y + side * 0.006f,
			sternCenter.z + right.z * side * 1.25f,
		};
		wake.velocity = {
			flowDirection.x * speedAbs * 0.18f + right.x * side * 0.28f,
			0.0f,
			flowDirection.z * speedAbs * 0.18f + right.z * side * 0.28f,
		};
		wake.object = std::make_unique<Object3d>();
		wake.object->Initialize();
		InitializeObject(*wake.object, "ground.obj", { 0.80f, 0.95f, 1.0f, 0.28f }, false);
		wake.object->SetTranslate(wake.position);
		wake.object->SetRotate({ 0.0f, wake.yaw, 0.0f });
		wake.object->SetScale({ wake.baseWidth, 1.0f, wake.baseLength });
		wakeTrails_.push_back(std::move(wake));
	}

	if (wakeTrails_.size() > 96) {
		wakeTrails_.erase(wakeTrails_.begin(), wakeTrails_.begin() + (wakeTrails_.size() - 96));
	}
}

void NavalBattleScene::SpawnHullFoamTrail()
{
	const float speedAbs = std::abs(player_.speed);
	const float speedRate = std::clamp(speedAbs / 33.0f, 0.0f, 1.0f);
	const Vector3 forward = ForwardFromYaw(player_.yaw);
	const Vector3 right = { std::cos(player_.yaw), 0.0f, -std::sin(player_.yaw) };
	const float directionSign = player_.speed >= 0.0f ? 1.0f : -1.0f;
	const Vector3 flowDirection = {
		-forward.x * directionSign,
		0.0f,
		-forward.z * directionSign,
	};
	const Vector3 bowCenter = {
		player_.position.x + forward.x * directionSign * 4.9f,
		-0.56f,
		player_.position.z + forward.z * directionSign * 4.9f,
	};
	const Vector3 midCenter = {
		player_.position.x - forward.x * directionSign * 0.8f,
		-0.58f,
		player_.position.z - forward.z * directionSign * 0.8f,
	};

	for (float side : { -1.0f, 1.0f }) {
		const Vector3 bowPosition = {
			bowCenter.x + right.x * side * 1.0f,
			bowCenter.y + side * 0.007f,
			bowCenter.z + right.z * side * 1.0f,
		};
		const Vector3 bowVelocity = {
			flowDirection.x * speedAbs * 0.10f + right.x * side * (0.65f + speedRate * 0.35f),
			0.0f,
			flowDirection.z * speedAbs * 0.10f + right.z * side * (0.65f + speedRate * 0.35f),
		};
		AddWaterFoamTrail(
			bowPosition,
			bowVelocity,
			player_.yaw + side * 0.46f,
			0.48f + speedRate * 0.22f,
			0.00022f + speedRate * 0.00008f,
			0.00092f + speedRate * 0.00035f,
			0.24f);

		const Vector3 sidePosition = {
			midCenter.x + right.x * side * 2.05f,
			midCenter.y + side * 0.005f,
			midCenter.z + right.z * side * 2.05f,
		};
		const Vector3 sideVelocity = {
			flowDirection.x * speedAbs * 0.15f + right.x * side * 0.18f,
			0.0f,
			flowDirection.z * speedAbs * 0.15f + right.z * side * 0.18f,
		};
		AddWaterFoamTrail(
			sidePosition,
			sideVelocity,
			player_.yaw,
			0.62f + speedRate * 0.28f,
			0.00016f + speedRate * 0.00006f,
			0.00115f + speedRate * 0.00035f,
			0.18f);
	}

	if (wakeTrails_.size() > 96) {
		wakeTrails_.erase(wakeTrails_.begin(), wakeTrails_.begin() + (wakeTrails_.size() - 96));
	}
}

void NavalBattleScene::AddWaterFoamTrail(const Vector3& position, const Vector3& velocity, float yaw, float duration, float width, float length, float alpha)
{
	WakeTrail foam{};
	foam.position = position;
	foam.velocity = velocity;
	foam.yaw = yaw;
	foam.duration = duration;
	foam.life = duration;
	foam.baseWidth = width;
	foam.baseLength = length;
	foam.baseAlpha = alpha;
	foam.object = std::make_unique<Object3d>();
	foam.object->Initialize();
	InitializeObject(*foam.object, "ground.obj", { 0.82f, 0.96f, 1.0f, alpha }, false);
	foam.object->SetTranslate(foam.position);
	foam.object->SetRotate({ 0.0f, foam.yaw, 0.0f });
	foam.object->SetScale({ foam.baseWidth, 1.0f, foam.baseLength });
	wakeTrails_.push_back(std::move(foam));
}

void NavalBattleScene::UpdateCamera()
{
	cameraShakeTime_ = (std::max)(0.0f, cameraShakeTime_ - finalDeltaTime_);
	if (precisionAimMode_) {
		const bool currentTargetValid =
			lockTargetIndex_ >= 0 &&
			lockTargetIndex_ < static_cast<int>(enemies_.size()) &&
			enemies_[lockTargetIndex_].alive &&
			DistanceXZ(player_.position, enemies_[lockTargetIndex_].ship.position) <= 220.0f;
		if (!currentTargetValid) {
			lockTargetIndex_ = FindPrecisionTargetFromAim();
			if (lockTargetIndex_ < 0) {
				precisionAimMode_ = false;
			}
		}
	} else {
		lockTargetIndex_ = -1;
	}
	if (precisionAimMode_ && lockTargetIndex_ >= 0) {
		const Vector3& targetPosition = enemies_[lockTargetIndex_].ship.position;
		const float targetYaw = std::atan2(targetPosition.x - player_.position.x, targetPosition.z - player_.position.z);
		const float precisionCameraFollow = std::clamp(18.0f * finalDeltaTime_, 0.0f, 1.0f);
		cameraYawOffset_ += NormalizeAngle(targetYaw - cameraYawOffset_) * precisionCameraFollow;

		const float cameraHeight = 7.5f;
		const float targetAimHeight = 1.65f;
		const float horizontalDistance = (std::max)(DistanceXZ(player_.position, targetPosition), 1.0f);
		const float desiredPitch = std::clamp(
			std::atan2((player_.position.y + cameraHeight) - (targetPosition.y + targetAimHeight), horizontalDistance),
			-0.55f,
			0.90f);
		cameraPitch_ += (desiredPitch - cameraPitch_) * precisionCameraFollow;
	}

	const float cameraYaw = cameraYawOffset_;
	const float cameraPitch = precisionAimMode_ ? std::clamp(cameraPitch_, -1.26f, 1.26f) : cameraPitch_;
	const float cameraDistance = precisionAimMode_ ? (std::min)(cameraDistance_, 48.0f) : cameraDistance_;
	const float cameraHeight = precisionAimMode_ ? 7.5f : 14.0f;
	const float cosPitch = std::cos(cameraPitch);
	const Vector3 forward = {
		std::sin(cameraYaw) * cosPitch,
		-std::sin(cameraPitch),
		std::cos(cameraYaw) * cosPitch,
	};
	const Vector3 target = {
		player_.position.x,
		player_.position.y + cameraHeight,
		player_.position.z,
	};
	Vector3 eye = {
		target.x - forward.x * cameraDistance,
		target.y - forward.y * cameraDistance,
		target.z - forward.z * cameraDistance,
	};
	float cameraRoll = 0.0f;
	if (cameraShakeTime_ > 0.0f && cameraShakeDuration_ > 0.001f) {
		const float t = 1.0f - std::clamp(cameraShakeTime_ / cameraShakeDuration_, 0.0f, 1.0f);
		const float envelope = (1.0f - t) * (1.0f - t);
		const float shake = cameraShakeIntensity_ * envelope;
		const float phase = battleTimer_ * 74.0f;
		const Vector3 right = {
			std::cos(cameraYaw),
			0.0f,
			-std::sin(cameraYaw),
		};
		const Vector3 up = {
			std::sin(cameraYaw) * std::sin(cameraPitch),
			std::cos(cameraPitch),
			std::cos(cameraYaw) * std::sin(cameraPitch),
		};
		const float xShake = std::sin(phase * 1.37f) * shake;
		const float yShake = std::sin(phase * 2.11f + 0.7f) * shake * 0.65f;
		eye.x += right.x * xShake + up.x * yShake;
		eye.y += right.y * xShake + up.y * yShake;
		eye.z += right.z * xShake + up.z * yShake;
		cameraRoll = std::sin(phase * 1.73f) * shake * 0.015f;
	}

	camera_->SetTranslate(eye);
	camera_->SetRotate({ cameraPitch, cameraYaw, cameraRoll });
	camera_->Update();
	Object3dCommon::GetInstance()->SetShadowFocus(player_.position);
}

void NavalBattleScene::AddCameraShake(float intensity, float duration)
{
	cameraShakeIntensity_ = (std::max)(cameraShakeIntensity_, intensity);
	cameraShakeDuration_ = (std::max)(cameraShakeDuration_, duration);
	cameraShakeTime_ = (std::max)(cameraShakeTime_, duration);
}

int NavalBattleScene::CountAliveEnemies() const
{
	int aliveEnemies = 0;
	for (const auto& enemy : enemies_) {
		if (enemy.alive) {
			++aliveEnemies;
		}
	}
	return aliveEnemies;
}

void NavalBattleScene::FireMainGun()
{
	if (player_.reload > 0.0f || !HasFiringSolution()) {
		return;
	}

	int assistedTargetIndex = GetAssistedTargetIndex();
	if (assistedTargetIndex < 0) {
		const int aimCandidate = FindPrecisionTargetFromAim();
		if (aimCandidate >= 0) {
			const float targetYaw = std::atan2(enemies_[aimCandidate].ship.position.x - player_.position.x, enemies_[aimCandidate].ship.position.z - player_.position.z);
			if (std::abs(NormalizeAngle(targetYaw - GetAimYaw())) <= 0.16f) {
				assistedTargetIndex = aimCandidate;
			}
		}
	}
	const bool useAssistedAim = assistedTargetIndex >= 0;
	const Vector3 assistedBaseTargetPosition = useAssistedAim
		? Vector3{
			enemies_[assistedTargetIndex].ship.position.x,
			enemies_[assistedTargetIndex].ship.position.y + 1.15f,
			enemies_[assistedTargetIndex].ship.position.z,
		}
		: Vector3{};
	Vector3 reticleWaterTarget{};
	const bool useReticleWaterTarget = !useAssistedAim && TryGetReticleWaterTarget(reticleWaterTarget);
	const float aimYaw = GetEffectiveAimYaw();
	const Vector3 shipForward = ForwardFromYaw(player_.yaw);
	const Vector3 shipRight = { std::cos(player_.yaw), 0.0f, -std::sin(player_.yaw) };
	const std::vector<GunMount> bearableGuns = GetBearableGunMounts(aimYaw);
	const Vector3 assistedTargetVelocity = useAssistedAim ? VelocityFromShip(enemies_[assistedTargetIndex].ship) : Vector3{};

	for (const GunMount& gun : bearableGuns) {
		const Vector3 muzzlePosition = {
			player_.position.x + shipRight.x * gun.localOffset.x + shipForward.x * gun.localOffset.z,
			player_.position.y + gun.localOffset.y,
			player_.position.z + shipRight.z * gun.localOffset.x + shipForward.z * gun.localOffset.z,
		};

		float horizontalSpeed = 92.0f;
		if (useReticleWaterTarget) {
			const float targetDistance = DistanceXZ(muzzlePosition, reticleWaterTarget);
			horizontalSpeed = std::clamp(targetDistance / 0.42f, 18.0f, 92.0f);
		}
		const Vector3 assistedTargetPosition = useAssistedAim
			? PredictBallisticTargetPosition(muzzlePosition, assistedBaseTargetPosition, assistedTargetVelocity, horizontalSpeed)
			: Vector3{};
		const float shotYaw = useAssistedAim
			? std::atan2(assistedTargetPosition.x - muzzlePosition.x, assistedTargetPosition.z - muzzlePosition.z)
			: useReticleWaterTarget
				? std::atan2(reticleWaterTarget.x - muzzlePosition.x, reticleWaterTarget.z - muzzlePosition.z)
			: aimYaw;
		const Vector3 shotForward = ForwardFromYaw(shotYaw);
		const float verticalVelocity = useAssistedAim
			? CalculateBallisticVerticalVelocity(muzzlePosition, assistedTargetPosition, horizontalSpeed)
			: useReticleWaterTarget
				? CalculateBallisticVerticalVelocity(muzzlePosition, reticleWaterTarget, horizontalSpeed)
			: GetAimVerticalVelocity();

		Projectile projectile{};
		projectile.position = muzzlePosition;
		projectile.velocity = { shotForward.x * horizontalSpeed, verticalVelocity, shotForward.z * horizontalSpeed };
		projectile.life = 3.6f;
		projectile.damage = 18.0f;
		projectile.radius = 4.2f;
		projectile.object = std::make_unique<Object3d>();
		projectile.object->Initialize();
		InitializeObject(*projectile.object, "bullet.obj", { 1.0f, 0.82f, 0.18f, 1.0f }, false);
		projectile.object->SetRotate({ 0.0f, shotYaw + std::numbers::pi_v<float> * 0.5f, 0.0f });
		projectile.object->SetScale({ 0.13f, 0.13f, 0.13f });

		projectiles_.push_back(std::move(projectile));
	}

	AddCameraShake(0.22f + static_cast<float>(bearableGuns.size()) * 0.08f, 0.18f);
	player_.reload = 0.72f;
}

void NavalBattleScene::DrawDebugWindow()
{
#ifdef USE_IMGUI
	DrawBattleHud();

	int aliveEnemies = 0;
	for (const auto& enemy : enemies_) {
		if (enemy.alive) {
			++aliveEnemies;
		}
	}

	ImGui::Begin("Naval Battle Prototype");
	ImGui::Text("F4 from title: Naval prototype");
	ImGui::Text("W/S: throttle 3..0..-1  A/D: rudder");
	ImGui::Text("RDrag/RightStick: move reticle, edge scrolls camera");
	ImGui::Text("C/Gamepad X: precision on/off near target");
	ImGui::Text("Precision: Q/E or stick L/R switches target");
	ImGui::Text("Space/LClick/RB: fire  Wheel: zoom");
	ImGui::Text("X: all stop  R: reset  Esc: title");
	ImGui::Separator();
	ImGui::Text("Player HP: %.0f / %.0f", player_.hp, player_.maxHp);
	ImGui::Text("Player Pos: %.1f, %.1f, %.1f", player_.position.x, player_.position.y, player_.position.z);
	if (camera_) {
		const Vector3& cameraPos = camera_->GetTranslate();
		ImGui::Text("Camera Pos: %.1f, %.1f, %.1f", cameraPos.x, cameraPos.y, cameraPos.z);
	}
	ImGui::Text("Throttle: %d", throttleStep_);
	ImGui::Text("Speed: %.1f kt", player_.speed);
	ImGui::Text("Camera Pitch: %.2f  Distance: %.1f", cameraPitch_, cameraDistance_);
	ImGui::Text("Reload: %.2f", player_.reload);
	ImGui::Text("Bearable Guns: %zu", GetBearableGunMounts(GetEffectiveAimYaw()).size());
	ImGui::Text("Precision: %s  Lock: %d", precisionAimMode_ ? "ON" : "OFF", lockTargetIndex_);
	ImGui::Text("Enemies: %d", aliveEnemies);
	ImGui::Text("Shells: %zu", projectiles_.size());
	ImGui::Text("Impacts: %zu", impactEffects_.size());
	ImGui::Text("Prototype: long-range gunnery spacing");
	ImGui::End();
#endif
}

void NavalBattleScene::DrawBattleHud()
{
#ifdef USE_IMGUI
	ImGuiIO& io = ImGui::GetIO();
	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	const ImVec2 viewport = io.DisplaySize;
	const ImVec2 screenCenter(viewport.x * 0.5f, viewport.y * 0.5f);
	const ImVec2 center(screenCenter.x + aimOffset_.x, screenCenter.y + aimOffset_.y);
	const float reticleRadius = precisionAimMode_ ? 155.0f : 118.0f;
	const float aimBoundary = precisionAimMode_ ? kPrecisionAimLimit : kNormalAimLimit;
	const std::vector<GunMount> bearableGuns = GetBearableGunMounts(GetEffectiveAimYaw());
	const bool firingSolution = HasFiringSolution();

	const ImU32 softWhite = IM_COL32(220, 235, 255, 92);
	const ImU32 hudGreen = IM_COL32(135, 255, 115, 205);
	const ImU32 hudBlue = IM_COL32(95, 220, 255, 205);
	const ImU32 hudPurple = IM_COL32(255, 90, 230, 185);
	const ImU32 hudYellow = IM_COL32(255, 230, 85, 225);
	const ImU32 hudOrange = IM_COL32(255, 106, 62, 230);
	const ImU32 hudRed = IM_COL32(255, 70, 65, 235);
	const ImU32 reticleMain = firingSolution ? (precisionAimMode_ ? hudOrange : hudGreen) : hudRed;
	const ImU32 reticleSoft = firingSolution ? softWhite : IM_COL32(255, 80, 70, 105);
	const ImU32 panel = IM_COL32(20, 38, 58, 115);
	const ImU32 panelLine = IM_COL32(170, 210, 255, 105);
	const int aliveEnemies = CountAliveEnemies();
	const float playerHpRate = std::clamp(player_.hp / player_.maxHp, 0.0f, 1.0f);

	const float skyBottomY = viewport.y * 0.34f;
	const float hazeBottomY = viewport.y * 0.43f;
	drawList->AddRectFilledMultiColor(
		ImVec2(0.0f, 0.0f),
		ImVec2(viewport.x, skyBottomY),
		IM_COL32(18, 48, 86, 235),
		IM_COL32(24, 62, 104, 235),
		IM_COL32(88, 166, 210, 155),
		IM_COL32(75, 150, 200, 155));
	drawList->AddRectFilledMultiColor(
		ImVec2(0.0f, skyBottomY),
		ImVec2(viewport.x, hazeBottomY),
		IM_COL32(98, 178, 220, 135),
		IM_COL32(98, 178, 220, 135),
		IM_COL32(84, 170, 210, 0),
		IM_COL32(84, 170, 210, 0));

	if (playerDamageFlash_ > 0.0f) {
		const int alpha = static_cast<int>(std::clamp(playerDamageFlash_ / 0.65f, 0.0f, 1.0f) * 105.0f);
		drawList->AddRectFilled(ImVec2(0.0f, 0.0f), viewport, IM_COL32(255, 38, 34, alpha));
	}

	const ImVec2 partyMin(24.0f, 34.0f);
	const ImVec2 partyMax(300.0f, 154.0f);
	drawList->AddRectFilled(partyMin, partyMax, IM_COL32(18, 28, 48, 82), 2.0f);
	drawList->AddRect(partyMin, partyMax, IM_COL32(180, 220, 255, 65), 2.0f, 0, 1.0f);
	drawList->AddText(ImVec2(partyMin.x + 10.0f, partyMin.y + 8.0f), hudGreen, "P1: 鋼鉄部隊");
	drawList->AddText(ImVec2(partyMin.x + 10.0f, partyMin.y + 32.0f), hudYellow, ("ENEMY SHIPS  " + std::to_string(aliveEnemies)).c_str());
	drawList->AddText(ImVec2(partyMin.x + 10.0f, partyMin.y + 56.0f), hudBlue, ("HP  " + std::to_string(static_cast<int>(player_.hp)) + " / " + std::to_string(static_cast<int>(player_.maxHp))).c_str());
	drawList->AddRectFilled(ImVec2(partyMin.x + 88.0f, partyMin.y + 58.0f), ImVec2(partyMax.x - 14.0f, partyMin.y + 69.0f), IM_COL32(20, 28, 42, 150), 2.0f);
	drawList->AddRectFilled(ImVec2(partyMin.x + 88.0f, partyMin.y + 58.0f), ImVec2(partyMin.x + 88.0f + (partyMax.x - partyMin.x - 102.0f) * playerHpRate, partyMin.y + 69.0f), hudBlue, 2.0f);
	drawList->AddText(ImVec2(partyMin.x + 10.0f, partyMin.y + 82.0f), hudPurple, "1  35.6cm Main Gun");

	drawList->AddCircle(screenCenter, aimBoundary, IM_COL32(220, 235, 255, 38), 96, 1.0f);
	drawList->AddCircle(center, reticleRadius, reticleSoft, 96, 2.0f);
	drawList->AddCircle(center, reticleRadius * 0.58f, firingSolution ? IM_COL32(220, 235, 255, 52) : IM_COL32(255, 80, 70, 55), 72, 1.0f);
	drawList->AddLine(ImVec2(center.x - reticleRadius, center.y), ImVec2(center.x - 28.0f, center.y), reticleSoft, 1.4f);
	drawList->AddLine(ImVec2(center.x + 28.0f, center.y), ImVec2(center.x + reticleRadius, center.y), reticleSoft, 1.4f);
	drawList->AddLine(ImVec2(center.x, center.y - reticleRadius), ImVec2(center.x, center.y - 28.0f), reticleSoft, 1.4f);
	drawList->AddLine(ImVec2(center.x, center.y + 28.0f), ImVec2(center.x, center.y + reticleRadius), reticleSoft, 1.4f);
	drawList->AddCircle(center, 8.0f, reticleMain, 24, 2.2f);
	if (firingSolution) {
		drawList->AddLine(ImVec2(center.x - 13.0f, center.y), ImVec2(center.x + 13.0f, center.y), reticleMain, 2.0f);
		drawList->AddLine(ImVec2(center.x, center.y - 13.0f), ImVec2(center.x, center.y + 13.0f), reticleMain, 2.0f);
	} else {
		drawList->AddLine(ImVec2(center.x - 13.0f, center.y - 13.0f), ImVec2(center.x + 13.0f, center.y + 13.0f), hudRed, 2.4f);
		drawList->AddLine(ImVec2(center.x + 13.0f, center.y - 13.0f), ImVec2(center.x - 13.0f, center.y + 13.0f), hudRed, 2.4f);
	}
	drawList->AddText(ImVec2(center.x - 44.0f, center.y + reticleRadius + 10.0f), reticleSoft,
		precisionAimMode_ ? "PRECISION FIRE" : "NORMAL SIGHT");
	const std::string gunArcText = firingSolution
		? std::to_string(bearableGuns.size()) + " GUNS READY"
		: "NO GUN ARC";
	drawList->AddText(ImVec2(center.x + 22.0f, center.y + 22.0f), firingSolution ? hudGreen : hudRed, gunArcText.c_str());

	if (camera_) {
		constexpr float fovY = 0.45f;
		const float aspectRatio = viewport.x > 1.0f && viewport.y > 1.0f ? viewport.x / viewport.y : 16.0f / 9.0f;
		const float halfTanY = std::tan(fovY * 0.5f);
		const float halfTanX = halfTanY * aspectRatio;
		const float cameraYaw = cameraYawOffset_;
		const float cameraPitch = precisionAimMode_ ? std::clamp(cameraPitch_, -1.26f, 1.26f) : cameraPitch_;
		const float cosPitch = std::cos(cameraPitch);
		const float sinPitch = std::sin(cameraPitch);
		const Vector3 cameraForward = {
			std::sin(cameraYaw) * cosPitch,
			-sinPitch,
			std::cos(cameraYaw) * cosPitch,
		};
		const Vector3 cameraRight = {
			std::cos(cameraYaw),
			0.0f,
			-std::sin(cameraYaw),
		};
		const Vector3 cameraUp = {
			std::sin(cameraYaw) * sinPitch,
			cosPitch,
			std::cos(cameraYaw) * sinPitch,
		};
		const Vector3& cameraPosition = camera_->GetTranslate();
		auto dot = [](const Vector3& a, const Vector3& b) {
			return a.x * b.x + a.y * b.y + a.z * b.z;
			};
		auto projectWorldToScreen = [&](const Vector3& worldPosition, ImVec2& screenPosition, float& viewDepth) {
			const Vector3 toPoint = {
				worldPosition.x - cameraPosition.x,
				worldPosition.y - cameraPosition.y,
				worldPosition.z - cameraPosition.z,
			};
			const float viewX = dot(toPoint, cameraRight);
			const float viewY = dot(toPoint, cameraUp);
			viewDepth = dot(toPoint, cameraForward);
			if (viewDepth <= 1.0f) {
				return false;
			}

			const float normalizedX = viewX / (viewDepth * halfTanX);
			const float normalizedY = viewY / (viewDepth * halfTanY);
			if (normalizedX < -1.35f || normalizedX > 1.35f || normalizedY < -1.35f || normalizedY > 1.35f) {
				return false;
			}

			screenPosition.x = screenCenter.x + normalizedX * viewport.x * 0.5f;
			screenPosition.y = screenCenter.y - normalizedY * viewport.y * 0.5f;
			return true;
			};

		for (size_t i = 0; i < enemies_.size(); ++i) {
			const EnemyShip& enemy = enemies_[i];
			if (!enemy.alive) {
				continue;
			}

			const bool isLockedTarget = precisionAimMode_ && static_cast<int>(i) == lockTargetIndex_;
			const Vector3 labelWorld = {
				enemy.ship.position.x,
				enemy.ship.position.y + 6.2f,
				enemy.ship.position.z,
			};
			ImVec2 labelPos{};
			float depth = 0.0f;
			if (!projectWorldToScreen(labelWorld, labelPos, depth)) {
				continue;
			}

			const float distance = DistanceXZ(player_.position, enemy.ship.position);
			const float hpRate = std::clamp(enemy.ship.hp / enemy.ship.maxHp, 0.0f, 1.0f);
			const float depthScale = std::clamp(180.0f / depth, 0.58f, 1.1f);
			const float barWidth = (isLockedTarget ? 76.0f : 62.0f) * depthScale;
			const float barHeight = (isLockedTarget ? 6.0f : 5.0f) * depthScale;
			const ImU32 markerColor = isLockedTarget ? hudOrange : IM_COL32(255, 95, 85, 230);
			const ImU32 textColor = isLockedTarget ? hudYellow : IM_COL32(235, 250, 255, 205);
			const ImU32 hpColor = hpRate > 0.45f ? IM_COL32(145, 255, 105, 210) : IM_COL32(255, 95, 75, 225);
			const std::string enemyName = "ENEMY SHIP " + std::to_string(i + 1);
			const std::string distanceText = std::to_string(static_cast<int>(distance)) + "m";

			const ImVec2 barMin(labelPos.x - barWidth * 0.5f, labelPos.y - 20.0f * depthScale);
			const ImVec2 barMax(labelPos.x + barWidth * 0.5f, barMin.y + barHeight);
			drawList->AddRectFilled(
				ImVec2(barMin.x - 1.0f, barMin.y - 1.0f),
				ImVec2(barMax.x + 1.0f, barMax.y + 1.0f),
				IM_COL32(10, 15, 20, 165),
				1.0f);
			drawList->AddRectFilled(barMin, barMax, IM_COL32(80, 15, 20, 165), 1.0f);
			drawList->AddRectFilled(barMin, ImVec2(barMin.x + barWidth * hpRate, barMax.y), hpColor, 1.0f);
			drawList->AddLine(ImVec2(labelPos.x - 14.0f * depthScale, labelPos.y), ImVec2(labelPos.x + 14.0f * depthScale, labelPos.y), markerColor, isLockedTarget ? 2.0f : 1.2f);
			drawList->AddLine(ImVec2(labelPos.x, labelPos.y - 9.0f * depthScale), ImVec2(labelPos.x, labelPos.y + 9.0f * depthScale), markerColor, isLockedTarget ? 2.0f : 1.2f);
			drawList->AddCircle(labelPos, isLockedTarget ? 10.0f * depthScale : 6.5f * depthScale, markerColor, 24, isLockedTarget ? 2.0f : 1.2f);
			if (isLockedTarget) {
				drawList->AddRect(
					ImVec2(labelPos.x - 23.0f * depthScale, labelPos.y - 23.0f * depthScale),
					ImVec2(labelPos.x + 23.0f * depthScale, labelPos.y + 23.0f * depthScale),
					IM_COL32(255, 170, 80, 120),
					0.0f,
					0,
					1.2f);
			}
			drawList->AddText(ImVec2(labelPos.x - barWidth * 0.5f, barMin.y - 18.0f), textColor, enemyName.c_str());
			drawList->AddText(ImVec2(labelPos.x + barWidth * 0.5f + 6.0f, barMin.y - 6.0f), textColor, distanceText.c_str());
		}
	}

	const float radarSize = 150.0f;
	const ImVec2 radarMin(32.0f, viewport.y - radarSize - 34.0f);
	const ImVec2 radarMax(radarMin.x + radarSize, radarMin.y + radarSize);
	const ImVec2 radarCenter(radarMin.x + radarSize * 0.5f, radarMin.y + radarSize * 0.5f);
	drawList->AddRectFilled(radarMin, radarMax, panel, 2.0f);
	drawList->AddRect(radarMin, radarMax, panelLine, 2.0f, 0, 1.5f);
	drawList->AddText(ImVec2(radarMin.x + 8.0f, radarMin.y + 6.0f), IM_COL32(190, 225, 255, 130), "AREA MAP");
	drawList->AddRect(
		ImVec2(radarMin.x + 10.0f, radarMin.y + 22.0f),
		ImVec2(radarMax.x - 10.0f, radarMax.y - 10.0f),
		IM_COL32(170, 210, 255, 45),
		0.0f,
		0,
		1.0f);
	drawList->AddLine(ImVec2(radarCenter.x, radarMin.y + 8.0f), ImVec2(radarCenter.x, radarMax.y - 8.0f), IM_COL32(170, 210, 255, 35), 1.0f);
	drawList->AddLine(ImVec2(radarMin.x + 8.0f, radarCenter.y), ImVec2(radarMax.x - 8.0f, radarCenter.y), IM_COL32(170, 210, 255, 35), 1.0f);

	constexpr float detectionRange = 330.0f;
	const ImVec2 mapMin(radarMin.x + 10.0f, radarMin.y + 22.0f);
	const ImVec2 mapMax(radarMax.x - 10.0f, radarMax.y - 10.0f);
	const float mapWidth = mapMax.x - mapMin.x;
	const float mapHeight = mapMax.y - mapMin.y;
	const auto WorldToMap = [&](const Vector3& world) {
		const float nx = std::clamp((world.x + kBattleLimit) / (kBattleLimit * 2.0f), 0.0f, 1.0f);
		const float nz = std::clamp((world.z + kBattleLimit) / (kBattleLimit * 2.0f), 0.0f, 1.0f);
		return ImVec2(mapMin.x + nx * mapWidth, mapMax.y - nz * mapHeight);
	};
	const ImVec2 playerDot = WorldToMap(player_.position);
	const float detectionRadiusX = detectionRange / (kBattleLimit * 2.0f) * mapWidth;
	const float detectionRadiusY = detectionRange / (kBattleLimit * 2.0f) * mapHeight;
	const float detectionRadius = (detectionRadiusX + detectionRadiusY) * 0.5f;
	drawList->AddCircle(playerDot, detectionRadius, IM_COL32(150, 230, 255, 42), 48, 1.0f);
	drawList->AddCircle(playerDot, detectionRadius * 0.52f, IM_COL32(150, 230, 255, 24), 36, 1.0f);

	for (const auto& enemy : enemies_) {
		if (!enemy.alive) {
			continue;
		}
		const float distance = DistanceXZ(player_.position, enemy.ship.position);
		if (distance > detectionRange) {
			continue;
		}
		const ImVec2 enemyDot = WorldToMap(enemy.ship.position);
		drawList->AddCircleFilled(enemyDot, 3.6f, hudYellow, 12);
		drawList->AddCircle(enemyDot, 6.0f, IM_COL32(255, 235, 120, 90), 16, 1.0f);
	}
	const ImVec2 shipTip(
		playerDot.x + std::sin(player_.yaw) * 9.0f,
		playerDot.y - std::cos(player_.yaw) * 9.0f);
	const ImVec2 shipLeft(
		playerDot.x + std::sin(player_.yaw + 2.45f) * 7.0f,
		playerDot.y - std::cos(player_.yaw + 2.45f) * 7.0f);
	const ImVec2 shipRight(
		playerDot.x + std::sin(player_.yaw - 2.45f) * 7.0f,
		playerDot.y - std::cos(player_.yaw - 2.45f) * 7.0f);
	drawList->AddTriangleFilled(
		shipTip,
		shipLeft,
		shipRight,
		hudGreen);

	const ImVec2 weaponMin(viewport.x - 260.0f, viewport.y - 230.0f);
	const ImVec2 weaponMax(viewport.x - 24.0f, viewport.y - 32.0f);
	drawList->AddRectFilled(weaponMin, weaponMax, IM_COL32(32, 20, 58, 118), 3.0f);
	drawList->AddRect(weaponMin, weaponMax, hudPurple, 3.0f, 0, 1.5f);
	drawList->AddText(ImVec2(weaponMin.x + 10.0f, weaponMin.y + 8.0f), hudPurple, "WEAPONS");
	const char* weaponLines[] = {
		"1  35.6cm Main Gun",
		"2  12.7cm Secondary",
		"3  10cm AA Gun",
		"4  Torpedo",
		"5  Machine Gun",
	};
	for (int i = 0; i < 5; ++i) {
		const float y = weaponMin.y + 34.0f + static_cast<float>(i) * 28.0f;
		const ImU32 rowCol = i == 0 ? IM_COL32(160, 255, 130, 190) : IM_COL32(230, 190, 255, 120);
		drawList->AddText(ImVec2(weaponMin.x + 12.0f, y), rowCol, weaponLines[i]);
	}
	const float reloadRate = 1.0f - std::clamp(player_.reload / 0.72f, 0.0f, 1.0f);
	drawList->AddRectFilled(
		ImVec2(weaponMin.x + 12.0f, weaponMax.y - 26.0f),
		ImVec2(weaponMax.x - 12.0f, weaponMax.y - 14.0f),
		IM_COL32(30, 45, 65, 180),
		2.0f);
	drawList->AddRectFilled(
		ImVec2(weaponMin.x + 12.0f, weaponMax.y - 26.0f),
		ImVec2(weaponMin.x + 12.0f + (weaponMax.x - weaponMin.x - 24.0f) * reloadRate, weaponMax.y - 14.0f),
		IM_COL32(150, 255, 115, 220),
		2.0f);

	const ImVec2 missionMin(viewport.x - 310.0f, 38.0f);
	const ImVec2 missionMax(viewport.x - 24.0f, 96.0f);
	drawList->AddRectFilled(missionMin, missionMax, IM_COL32(16, 28, 42, 120), 3.0f);
	drawList->AddRect(missionMin, missionMax, panelLine, 3.0f, 0, 1.2f);
	drawList->AddText(ImVec2(missionMin.x + 12.0f, missionMin.y + 8.0f), hudGreen, "MISSION");
	drawList->AddText(ImVec2(missionMin.x + 12.0f, missionMin.y + 30.0f), hudYellow, "Destroy all enemy ships");

	const ImVec2 bottomCenter(screenCenter.x, viewport.y - 56.0f);
	const float hpBarWidth = 330.0f;
	drawList->AddRectFilled(
		ImVec2(bottomCenter.x - hpBarWidth * 0.5f, bottomCenter.y - 2.0f),
		ImVec2(bottomCenter.x + hpBarWidth * 0.5f, bottomCenter.y + 12.0f),
		IM_COL32(18, 30, 54, 165),
		3.0f);
	drawList->AddRectFilled(
		ImVec2(bottomCenter.x - hpBarWidth * 0.5f, bottomCenter.y - 2.0f),
		ImVec2(bottomCenter.x - hpBarWidth * 0.5f + hpBarWidth * playerHpRate, bottomCenter.y + 12.0f),
		hudBlue,
		3.0f);
	drawList->AddRect(ImVec2(bottomCenter.x - hpBarWidth * 0.5f, bottomCenter.y - 2.0f), ImVec2(bottomCenter.x + hpBarWidth * 0.5f, bottomCenter.y + 12.0f), IM_COL32(170, 230, 255, 100), 3.0f, 0, 1.0f);
	drawList->AddText(ImVec2(bottomCenter.x - 52.0f, bottomCenter.y + 16.0f), hudBlue, (std::to_string(static_cast<int>(player_.hp)) + " / " + std::to_string(static_cast<int>(player_.maxHp))).c_str());

	const ImVec2 throttleBase(screenCenter.x - 260.0f, viewport.y - 168.0f);
	drawList->AddText(ImVec2(throttleBase.x - 18.0f, throttleBase.y + 132.0f), hudGreen, (std::to_string(static_cast<int>(std::round(player_.speed))) + ".0 kt").c_str());
	for (int step = 3; step >= -1; --step) {
		const float y = throttleBase.y + static_cast<float>(3 - step) * 24.0f;
		const bool active = step == throttleStep_;
		const ImU32 col = active ? hudGreen : IM_COL32(120, 255, 100, 78);
		drawList->AddText(ImVec2(throttleBase.x, y), col, std::to_string(step).c_str());
		drawList->AddRectFilled(ImVec2(throttleBase.x + 28.0f, y + 5.0f), ImVec2(throttleBase.x + 28.0f + (active ? 42.0f : 24.0f), y + 12.0f), col, 1.0f);
	}
	drawList->AddText(ImVec2(bottomCenter.x - 50.0f, viewport.y - 28.0f), hudGreen, ("THROTTLE " + std::to_string(throttleStep_)).c_str());

	if (missionComplete_ || gameOver_) {
		const char* resultText = missionComplete_ ? "MISSION COMPLETE" : "GAME OVER";
		const ImU32 resultColor = missionComplete_ ? hudGreen : hudRed;
		const ImVec2 resultCenter(screenCenter.x, screenCenter.y - 118.0f);
		drawList->AddRectFilled(ImVec2(resultCenter.x - 230.0f, resultCenter.y - 34.0f), ImVec2(resultCenter.x + 230.0f, resultCenter.y + 40.0f), IM_COL32(5, 12, 24, 170), 4.0f);
		drawList->AddRect(ImVec2(resultCenter.x - 230.0f, resultCenter.y - 34.0f), ImVec2(resultCenter.x + 230.0f, resultCenter.y + 40.0f), resultColor, 4.0f, 0, 2.0f);
		drawList->AddText(ImVec2(resultCenter.x - 78.0f, resultCenter.y - 12.0f), resultColor, resultText);
		drawList->AddText(ImVec2(resultCenter.x - 58.0f, resultCenter.y + 14.0f), softWhite, "R: RETRY");
	}

	if (lockTargetIndex_ >= 0) {
		const EnemyShip& target = enemies_[lockTargetIndex_];
		const float distance = DistanceXZ(player_.position, target.ship.position);
		const std::string lockText = "LOCK " + std::to_string(static_cast<int>(distance)) + "m";
		drawList->AddText(ImVec2(center.x + 22.0f, center.y - 34.0f), hudYellow, lockText.c_str());
		drawList->AddCircle(center, 18.0f, hudOrange, 32, 2.0f);
	}
#endif
}

Vector3 NavalBattleScene::ForwardFromYaw(float yaw) const
{
	return { std::sin(yaw), 0.0f, std::cos(yaw) };
}

Vector3 NavalBattleScene::VelocityFromShip(const ShipState& ship) const
{
	const Vector3 forward = ForwardFromYaw(ship.yaw);
	return {
		forward.x * ship.speed,
		0.0f,
		forward.z * ship.speed,
	};
}

Vector3 NavalBattleScene::PredictBallisticTargetPosition(const Vector3& muzzlePosition, const Vector3& targetPosition, const Vector3& targetVelocity, float horizontalSpeed) const
{
	Vector3 predicted = targetPosition;
	const float safeHorizontalSpeed = (std::max)(horizontalSpeed, 1.0f);
	for (int i = 0; i < 3; ++i) {
		const float flightTime = std::clamp(DistanceXZ(muzzlePosition, predicted) / safeHorizontalSpeed, 0.05f, 5.0f);
		predicted = {
			targetPosition.x + targetVelocity.x * flightTime,
			targetPosition.y + targetVelocity.y * flightTime,
			targetPosition.z + targetVelocity.z * flightTime,
		};
	}
	return predicted;
}

float NavalBattleScene::SampleOceanHeight(const Vector3& position, float time) const
{
	// Match the sea vertex shader as closely as possible.
	// The sea object is drawn with XZ scale 0.5, so world-space samples are converted
	// back to the sea mesh local coordinates before evaluating the same wave phases.
	constexpr float seaScaleXZ = 0.50f;
	const float x = position.x / seaScaleXZ;
	const float z = position.z / seaScaleXZ;

	constexpr float dirALen = 0.99984999f;
	constexpr float dirBLen = 1.00224745f;
	constexpr float dirCLen = 0.99699549f;
	constexpr float dirDLen = 0.99724621f;

	const float dirAx = 0.86f / dirALen;
	const float dirAz = 0.51f / dirALen;
	const float dirBx = -0.42f / dirBLen;
	const float dirBz = 0.91f / dirBLen;
	const float dirCx = 0.62f / dirCLen;
	const float dirCz = -0.78f / dirCLen;
	const float dirDx = 0.27f / dirDLen;
	const float dirDz = 0.96f / dirDLen;

	const float phaseA = (x * dirAx + z * dirAz) * 0.014f + time * 0.66f;
	const float phaseB = (x * dirBx + z * dirBz) * 0.021f - time * 0.54f;
	const float phaseC = (x * dirCx + z * dirCz) * 0.038f + time * 1.05f;
	const float phaseD = x * 0.068f + z * -0.041f + time * 1.55f;
	const float swellFocus = 0.88f + 0.18f * std::sin((x * dirDx + z * dirDz) * 0.006f + time * 0.18f);

	return (
		std::sin(phaseA) * 1.75f +
		std::sin(phaseB) * 1.05f +
		std::sin(phaseC) * 0.50f +
		std::sin(phaseD) * 0.28f
		) * swellFocus;
}

Vector3 NavalBattleScene::GetReticleRayDirection() const
{
	constexpr float virtualWidth = 1280.0f;
	constexpr float virtualHeight = 720.0f;
	constexpr float fovY = 0.45f;
	constexpr float aspectRatio = virtualWidth / virtualHeight;

	const float cameraYaw = cameraYawOffset_;
	const float cameraPitch = precisionAimMode_ ? std::clamp(cameraPitch_, -1.26f, 1.26f) : cameraPitch_;
	const float cosPitch = std::cos(cameraPitch);
	const float sinPitch = std::sin(cameraPitch);
	const Vector3 forward = {
		std::sin(cameraYaw) * cosPitch,
		-sinPitch,
		std::cos(cameraYaw) * cosPitch,
	};
	const Vector3 right = {
		std::cos(cameraYaw),
		0.0f,
		-std::sin(cameraYaw),
	};
	const Vector3 up = {
		std::sin(cameraYaw) * sinPitch,
		cosPitch,
		std::cos(cameraYaw) * sinPitch,
	};

	const float halfTanY = std::tan(fovY * 0.5f);
	const float halfTanX = halfTanY * aspectRatio;
	const float screenX = std::clamp(aimOffset_.x / (virtualWidth * 0.5f), -1.0f, 1.0f);
	const float screenY = std::clamp(-aimOffset_.y / (virtualHeight * 0.5f), -1.0f, 1.0f);
	Vector3 ray = {
		forward.x + right.x * screenX * halfTanX + up.x * screenY * halfTanY,
		forward.y + right.y * screenX * halfTanX + up.y * screenY * halfTanY,
		forward.z + right.z * screenX * halfTanX + up.z * screenY * halfTanY,
	};
	const float length = std::sqrt(ray.x * ray.x + ray.y * ray.y + ray.z * ray.z);
	if (length > 0.0001f) {
		ray.x /= length;
		ray.y /= length;
		ray.z /= length;
	}
	return ray;
}

bool NavalBattleScene::TryGetReticleWaterTarget(Vector3& targetPosition) const
{
	if (!camera_) {
		return false;
	}

	constexpr float waterY = -0.92f;
	const Vector3& cameraPosition = camera_->GetTranslate();
	const Vector3 ray = GetReticleRayDirection();
	if (ray.y >= -0.015f) {
		return false;
	}

	const float t = (waterY - cameraPosition.y) / ray.y;
	if (t <= 0.0f || t > 520.0f) {
		return false;
	}

	targetPosition = {
		cameraPosition.x + ray.x * t,
		waterY,
		cameraPosition.z + ray.z * t,
	};
	return true;
}

float NavalBattleScene::GetAimYaw() const
{
	const Vector3 ray = GetReticleRayDirection();
	return NormalizeAngle(std::atan2(ray.x, ray.z));
}

float NavalBattleScene::GetEffectiveAimYaw() const
{
	const int assistedTargetIndex = GetAssistedTargetIndex();
	if (assistedTargetIndex >= 0) {
		const Vector3& targetPosition = enemies_[assistedTargetIndex].ship.position;
		return std::atan2(targetPosition.x - player_.position.x, targetPosition.z - player_.position.z);
	}
	return GetAimYaw();
}

float NavalBattleScene::GetAimVerticalVelocity() const
{
	const Vector3 ray = GetReticleRayDirection();
	const float horizontalLength = (std::max)(std::sqrt(ray.x * ray.x + ray.z * ray.z), 0.1f);
	const float reticlePitchVelocity = ray.y / horizontalLength * 92.0f;
	return std::clamp(reticlePitchVelocity + 10.0f, 8.0f, 42.0f);
}

float NavalBattleScene::CalculateBallisticVerticalVelocity(const Vector3& muzzlePosition, const Vector3& targetPosition, float horizontalSpeed) const
{
	constexpr float gravity = -18.0f;
	const float horizontalDistance = DistanceXZ(muzzlePosition, targetPosition);
	const float safeHorizontalSpeed = (std::max)(horizontalSpeed, 1.0f);
	const float flightTime = (std::max)(horizontalDistance / safeHorizontalSpeed, 0.08f);
	const float heightDifference = targetPosition.y - muzzlePosition.y;
	const float verticalVelocity = (heightDifference - 0.5f * gravity * flightTime * flightTime) / flightTime;
	return std::clamp(verticalVelocity, 5.0f, 31.0f);
}

int NavalBattleScene::GetAssistedTargetIndex() const
{
	if (!precisionAimMode_) {
		return -1;
	}

	const int candidateIndex = lockTargetIndex_;
	if (candidateIndex < 0 || candidateIndex >= static_cast<int>(enemies_.size())) {
		return -1;
	}

	const EnemyShip& target = enemies_[candidateIndex];
	if (!target.alive) {
		return -1;
	}

	const float distance = DistanceXZ(player_.position, target.ship.position);
	if (distance > 220.0f) {
		return -1;
	}

	return candidateIndex;
}

std::vector<NavalBattleScene::GunMount> NavalBattleScene::GetBearableGunMounts(float aimYaw) const
{
	const GunMount guns[] = {
		{ { -0.78f, 1.42f, 1.75f }, 0.0f, kGunHalfArc },
		{ { 0.78f, 1.42f, 1.75f }, 0.0f, kGunHalfArc },
		{ { -0.78f, 1.42f, -1.75f }, std::numbers::pi_v<float>, kGunHalfArc },
		{ { 0.78f, 1.42f, -1.75f }, std::numbers::pi_v<float>, kGunHalfArc },
	};

	std::vector<GunMount> bearableGuns;
	const float relativeAimYaw = NormalizeAngle(aimYaw - player_.yaw);
	for (const GunMount& gun : guns) {
		const float angleToGunCenter = std::abs(NormalizeAngle(relativeAimYaw - gun.centerYaw));
		if (angleToGunCenter <= gun.halfArc) {
			bearableGuns.push_back(gun);
		}
	}
	return bearableGuns;
}

bool NavalBattleScene::HasFiringSolution() const
{
	return !GetBearableGunMounts(GetEffectiveAimYaw()).empty();
}

float NavalBattleScene::DistanceXZ(const Vector3& a, const Vector3& b) const
{
	const float dx = a.x - b.x;
	const float dz = a.z - b.z;
	return std::sqrt(dx * dx + dz * dz);
}

int NavalBattleScene::FindPrecisionTargetFromAim() const
{
	int targetIndex = -1;
	float bestScore = 999999.0f;
	const float aimYaw = GetAimYaw();
	for (int i = 0; i < static_cast<int>(enemies_.size()); ++i) {
		const EnemyShip& enemy = enemies_[i];
		if (!enemy.alive) {
			continue;
		}

		const float distance = DistanceXZ(player_.position, enemy.ship.position);
		if (distance > 220.0f) {
			continue;
		}

		const float toEnemyYaw = std::atan2(enemy.ship.position.x - player_.position.x, enemy.ship.position.z - player_.position.z);
		const float angle = std::abs(NormalizeAngle(toEnemyYaw - aimYaw));
		if (angle > 0.42f) {
			continue;
		}

		const float score = angle * 500.0f + distance;
		if (score < bestScore) {
			bestScore = score;
			targetIndex = i;
		}
	}
	return targetIndex;
}

int NavalBattleScene::FindNextPrecisionTarget(int direction) const
{
	if (enemies_.empty()) {
		return -1;
	}

	const float twoPi = std::numbers::pi_v<float> * 2.0f;
	const float baseYaw =
		lockTargetIndex_ >= 0 && lockTargetIndex_ < static_cast<int>(enemies_.size()) && enemies_[lockTargetIndex_].alive
		? std::atan2(enemies_[lockTargetIndex_].ship.position.x - player_.position.x, enemies_[lockTargetIndex_].ship.position.z - player_.position.z)
		: cameraYawOffset_;

	int targetIndex = -1;
	float bestDelta = 999999.0f;
	for (int i = 0; i < static_cast<int>(enemies_.size()); ++i) {
		if (i == lockTargetIndex_) {
			continue;
		}
		const EnemyShip& enemy = enemies_[i];
		if (!enemy.alive) {
			continue;
		}

		const float distance = DistanceXZ(player_.position, enemy.ship.position);
		if (distance > 220.0f) {
			continue;
		}

		const float targetYaw = std::atan2(enemy.ship.position.x - player_.position.x, enemy.ship.position.z - player_.position.z);
		float signedDelta = NormalizeAngle(targetYaw - baseYaw);
		if (direction < 0) {
			signedDelta = -signedDelta;
		}
		if (signedDelta <= 0.04f) {
			signedDelta += twoPi;
		}
		if (signedDelta < bestDelta) {
			bestDelta = signedDelta;
			targetIndex = i;
		}
	}

	return targetIndex;
}

int NavalBattleScene::FindLockTarget() const
{
	int targetIndex = -1;
	float bestScore = 999999.0f;
	for (int i = 0; i < static_cast<int>(enemies_.size()); ++i) {
		const EnemyShip& enemy = enemies_[i];
		if (!enemy.alive) {
			continue;
		}
		const float toEnemyYaw = std::atan2(enemy.ship.position.x - player_.position.x, enemy.ship.position.z - player_.position.z);
		const float angle = std::abs(NormalizeAngle(toEnemyYaw - cameraYawOffset_));
		const float distance = DistanceXZ(enemy.ship.position, player_.position);
		const float score = angle * 260.0f + distance;
		if (score < bestScore) {
			bestScore = score;
			targetIndex = i;
		}
	}
	return targetIndex;
}

float NavalBattleScene::NormalizeAngle(float angle) const
{
	const float twoPi = std::numbers::pi_v<float> * 2.0f;
	while (angle > std::numbers::pi_v<float>) {
		angle -= twoPi;
	}
	while (angle < -std::numbers::pi_v<float>) {
		angle += twoPi;
	}
	return angle;
}
