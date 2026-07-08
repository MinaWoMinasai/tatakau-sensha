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
}

void NavalBattleScene::Initialize()
{
	input_ = Input::GetInstance();
	camera_ = std::make_unique<Camera>();
	debugCamera_ = std::make_unique<DebugCamera>();

	ModelManager::GetInstance()->LoadModel("ground.obj");
	ModelManager::GetInstance()->LoadModel("bullet.obj");
	ModelManager::GetInstance()->LoadModel("navalHullBox.obj");

	Object3dCommon::GetInstance()->SetDefaultCamera(camera_.get());
	Object3dCommon::GetInstance()->SetDebugDefaultCamera(debugCamera_.get());
	Object3dCommon::GetInstance()->SetIsDebugCamera(false);
	Object3dCommon::GetInstance()->SetShadowRange(420.0f);

	sea_ = std::make_unique<Object3d>();
	sea_->Initialize();
	InitializeObject(*sea_, "ground.obj", { 0.72f, 0.92f, 1.0f, 1.0f }, false);
	sea_->SetEnvironmentCoefficient(2.0f);
	sea_->SetScale({ 0.13f, 1.0f, 0.13f });
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

	UpdatePlayer();
	UpdateEnemies();
	UpdateProjectiles();
	UpdateCamera();

	sea_->SetShininess(battleTimer_);
	sea_->Update();
	playerHull_->Update();
	playerTurret_->Update();
	playerMarker_->Update();
	for (auto& enemy : enemies_) {
		if (enemy.alive && enemy.hull) {
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
	playerHull_->Draw();
	playerTurret_->Draw();
	playerMarker_->Draw();
	for (auto& enemy : enemies_) {
		if (enemy.alive && enemy.hull) {
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
	enemies_.clear();

	const Vector3 enemyPositions[] = {
		{ -76.0f, 1.0f, 110.0f },
		{ 0.0f, 1.0f, 145.0f },
		{ 82.0f, 1.0f, 115.0f },
	};

	for (const Vector3& position : enemyPositions) {
		EnemyShip enemy{};
		enemy.ship.position = position;
		enemy.ship.maxHp = 50.0f;
		enemy.ship.hp = enemy.ship.maxHp;
		enemy.ship.speed = 0.0f;
		enemy.hull = std::make_unique<Object3d>();
		enemy.hull->Initialize();
		InitializeObject(*enemy.hull, "navalHullBox.obj", { 0.40f, 0.95f, 0.35f, 1.0f }, false);
		enemy.hull->SetScale({ 2.0f, 0.45f, 5.6f });
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
	player_.position.x = std::clamp(player_.position.x, -230.0f, 230.0f);
	player_.position.z = std::clamp(player_.position.z, -230.0f, 230.0f);

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

	playerHull_->SetTranslate(player_.position);
	playerHull_->SetRotate({ 0.0f, player_.yaw, 0.0f });
	playerHull_->SetScale({ 1.85f, 0.42f, 5.4f });

	const Vector3 turretOffset = ForwardFromYaw(player_.yaw);
	playerTurret_->SetTranslate({
		player_.position.x + turretOffset.x * 0.65f,
		player_.position.y + 0.62f,
		player_.position.z + turretOffset.z * 0.65f,
		});
	playerTurret_->SetRotate({ 0.0f, player_.yaw + std::numbers::pi_v<float> * 0.5f, 0.0f });
	playerTurret_->SetScale({ 0.32f, 0.32f, 0.32f });

	playerMarker_->SetTranslate({
		player_.position.x,
		player_.position.y + 1.55f,
		player_.position.z,
		});
	playerMarker_->SetRotate({ std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f });
	playerMarker_->SetScale({ 0.36f, 0.36f, 0.36f });
}

void NavalBattleScene::UpdateEnemies()
{
	for (auto& enemy : enemies_) {
		if (!enemy.alive) {
			continue;
		}

		const Vector3 toPlayer = {
			player_.position.x - enemy.ship.position.x,
			0.0f,
			player_.position.z - enemy.ship.position.z,
		};
		enemy.ship.yaw = std::atan2(toPlayer.x, toPlayer.z);

		const float distance = DistanceXZ(enemy.ship.position, player_.position);
		if (distance > 72.0f) {
			const Vector3 forward = ForwardFromYaw(enemy.ship.yaw);
			enemy.ship.position.x += forward.x * 4.2f * finalDeltaTime_;
			enemy.ship.position.z += forward.z * 4.2f * finalDeltaTime_;
		} else if (distance < 14.0f) {
			player_.hp = (std::max)(0.0f, player_.hp - 1.5f * finalDeltaTime_);
		}

		enemy.hull->SetTranslate(enemy.ship.position);
		enemy.hull->SetRotate({ 0.0f, enemy.ship.yaw, 0.0f });
		enemy.hull->SetScale({ 2.0f, 0.45f, 5.6f });
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
				}
				break;
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
			impact.object->SetTranslate(impact.position);
			if (impact.waterSplash) {
				impact.object->SetRotate({ std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f });
				impact.object->SetScale({
					impact.baseScale * (0.55f + t * 0.35f),
					impact.baseScale * (1.15f + t * 0.95f),
					impact.baseScale * (0.55f + t * 0.35f),
					});
			} else {
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
	impact.duration = waterSplash ? 0.72f : 0.55f;
	impact.life = impact.duration;
	impact.baseScale = waterSplash ? 0.78f : 0.62f;
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
}

void NavalBattleScene::UpdateCamera()
{
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
	const Vector3 eye = {
		target.x - forward.x * cameraDistance,
		target.y - forward.y * cameraDistance,
		target.z - forward.z * cameraDistance,
	};

	camera_->SetTranslate(eye);
	camera_->SetRotate({ cameraPitch, cameraYaw, 0.0f });
	camera_->Update();
	Object3dCommon::GetInstance()->SetShadowFocus(player_.position);
}

void NavalBattleScene::FireMainGun()
{
	if (player_.reload > 0.0f || !HasFiringSolution()) {
		return;
	}

	const int assistedTargetIndex = GetAssistedTargetIndex();
	const bool useAssistedAim = assistedTargetIndex >= 0;
	const Vector3 assistedTargetPosition = useAssistedAim
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

	for (const GunMount& gun : bearableGuns) {
		const Vector3 muzzlePosition = {
			player_.position.x + shipRight.x * gun.localOffset.x + shipForward.x * gun.localOffset.z,
			player_.position.y + gun.localOffset.y,
			player_.position.z + shipRight.z * gun.localOffset.x + shipForward.z * gun.localOffset.z,
		};

		const float shotYaw = useAssistedAim
			? std::atan2(assistedTargetPosition.x - muzzlePosition.x, assistedTargetPosition.z - muzzlePosition.z)
			: useReticleWaterTarget
				? std::atan2(reticleWaterTarget.x - muzzlePosition.x, reticleWaterTarget.z - muzzlePosition.z)
			: aimYaw;
		const Vector3 shotForward = ForwardFromYaw(shotYaw);
		float horizontalSpeed = 92.0f;
		if (useReticleWaterTarget) {
			const float targetDistance = DistanceXZ(muzzlePosition, reticleWaterTarget);
			horizontalSpeed = std::clamp(targetDistance / 0.42f, 18.0f, 92.0f);
		}
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
	const ImU32 hudYellow = IM_COL32(255, 230, 85, 225);
	const ImU32 hudOrange = IM_COL32(255, 106, 62, 230);
	const ImU32 hudRed = IM_COL32(255, 70, 65, 235);
	const ImU32 reticleMain = firingSolution ? (precisionAimMode_ ? hudOrange : hudGreen) : hudRed;
	const ImU32 reticleSoft = firingSolution ? softWhite : IM_COL32(255, 80, 70, 105);
	const ImU32 panel = IM_COL32(20, 38, 58, 115);
	const ImU32 panelLine = IM_COL32(170, 210, 255, 105);

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
	drawList->AddCircle(radarCenter, radarSize * 0.42f, IM_COL32(170, 210, 255, 65), 48, 1.0f);
	drawList->AddLine(ImVec2(radarCenter.x, radarMin.y + 8.0f), ImVec2(radarCenter.x, radarMax.y - 8.0f), IM_COL32(170, 210, 255, 35), 1.0f);
	drawList->AddLine(ImVec2(radarMin.x + 8.0f, radarCenter.y), ImVec2(radarMax.x - 8.0f, radarCenter.y), IM_COL32(170, 210, 255, 35), 1.0f);

	const float radarRange = 180.0f;
	for (const auto& enemy : enemies_) {
		if (!enemy.alive) {
			continue;
		}
		const float dx = std::clamp((enemy.ship.position.x - player_.position.x) / radarRange, -1.0f, 1.0f);
		const float dz = std::clamp((enemy.ship.position.z - player_.position.z) / radarRange, -1.0f, 1.0f);
		const ImVec2 enemyDot(radarCenter.x + dx * radarSize * 0.42f, radarCenter.y - dz * radarSize * 0.42f);
		drawList->AddCircleFilled(enemyDot, 4.5f, hudYellow, 12);
	}
	const ImVec2 shipTip(
		radarCenter.x + std::sin(player_.yaw) * 10.0f,
		radarCenter.y - std::cos(player_.yaw) * 10.0f);
	drawList->AddTriangleFilled(
		shipTip,
		ImVec2(radarCenter.x - 7.0f, radarCenter.y + 8.0f),
		ImVec2(radarCenter.x + 7.0f, radarCenter.y + 8.0f),
		hudGreen);

	const ImVec2 weaponMin(viewport.x - 260.0f, viewport.y - 230.0f);
	const ImVec2 weaponMax(viewport.x - 24.0f, viewport.y - 32.0f);
	drawList->AddRectFilled(weaponMin, weaponMax, panel, 3.0f);
	drawList->AddRect(weaponMin, weaponMax, panelLine, 3.0f, 0, 1.5f);
	drawList->AddText(ImVec2(weaponMin.x + 10.0f, weaponMin.y + 8.0f), hudYellow, "WEAPONS");
	const char* weaponLines[] = {
		"1  35.6cm Main Gun",
		"2  12.7cm Secondary",
		"3  10cm AA Gun",
		"4  Torpedo",
		"5  Machine Gun",
	};
	for (int i = 0; i < 5; ++i) {
		const float y = weaponMin.y + 34.0f + static_cast<float>(i) * 28.0f;
		const ImU32 rowCol = i == 0 ? IM_COL32(110, 255, 110, 150) : IM_COL32(200, 225, 255, 105);
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
		IM_COL32(120, 245, 105, 205),
		2.0f);

	const ImVec2 missionMin(viewport.x - 310.0f, 38.0f);
	const ImVec2 missionMax(viewport.x - 24.0f, 96.0f);
	drawList->AddRectFilled(missionMin, missionMax, IM_COL32(16, 28, 42, 120), 3.0f);
	drawList->AddRect(missionMin, missionMax, panelLine, 3.0f, 0, 1.2f);
	drawList->AddText(ImVec2(missionMin.x + 12.0f, missionMin.y + 8.0f), hudGreen, "MISSION");
	drawList->AddText(ImVec2(missionMin.x + 12.0f, missionMin.y + 30.0f), hudYellow, "Destroy all enemy ships");

	const std::string speedText = std::to_string(static_cast<int>(std::round(player_.speed))) + ".0 kt";
	drawList->AddText(ImVec2(screenCenter.x - 170.0f, viewport.y - 86.0f), softWhite, speedText.c_str());
	const std::string throttleText = "THROTTLE " + std::to_string(throttleStep_);
	drawList->AddText(ImVec2(screenCenter.x - 60.0f, viewport.y - 58.0f), hudGreen, throttleText.c_str());

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
