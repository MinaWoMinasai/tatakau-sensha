#include "Action3DScene.h"

#include <algorithm>
#include <cmath>
#include <exception>

#include "Object3dCommon.h"
#include "ParticleManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void Action3DScene::Initialize()
{
	input_ = Input::GetInstance();
	camera_ = std::make_unique<Camera>();
	debugCamera_ = std::make_unique<DebugCamera>();

	Object3dCommon::GetInstance()->SetDefaultCamera(camera_.get());
	Object3dCommon::GetInstance()->SetDebugDefaultCamera(debugCamera_.get());
	Object3dCommon::GetInstance()->SetIsDebugCamera(false);
	Object3dCommon::GetInstance()->SetShadowFocus(playerPosition_);
	Object3dCommon::GetInstance()->SetShadowRange(85.0f);

	ground_ = std::make_unique<Object3d>();
	ground_->Initialize();
	ground_->SetModel("ground.obj");
	ground_->SetTranslate({ 0.0f, -30.0f, 0.0f });
	ground_->SetScale({ 2.5f, 1.0f, 2.5f });
	ground_->SetColor({ 0.20f, 0.24f, 0.30f, 1.0f });
	ground_->SetLighting(true);
	ground_->Update();

	skeletonRenderer_ = std::make_unique<NeonGridRenderer>();
	skeletonRenderer_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(), "resources/white512x512.png");
	skeletonRenderer_->SetLineStyle(0.30f, 1.15f);

	try {
		const std::string humanPath = "resources/models/human/walk.gltf";
		playerModel_ = std::make_unique<SkinnedModel>();
		playerModel_->Initialize(
			Object3dCommon::GetInstance()->GetDxCommon(),
			Object3dCommon::GetInstance()->GetSrvManager(),
			humanPath);

		playerObject_ = std::make_unique<Object3d>();
		playerObject_->Initialize();
		playerObject_->SetTranslate(playerPosition_);
		playerObject_->SetScale({ 8.0f, 8.0f, 8.0f });
		playerObject_->SetColor({ 0.55f, 0.90f, 1.0f, 1.0f });
		playerObject_->SetLighting(false);
		playerObject_->Update();

		// A: 呼吸するIdle（コード生成）、B: 配布モデルのWalk。
		// 両方を常時再生し、移動状態に応じて姿勢をクロスフェードする。
		idleAnimation_.name = "Idle_Breathing";
		idleAnimation_.duration = 2.0f;
		const auto spineIt = playerModel_->GetSkeleton().jointMap.find("mixamorig:Spine2");
		if (spineIt != playerModel_->GetSkeleton().jointMap.end()) {
			const QuaternionTransform& bind = playerModel_->GetSkeleton().joints[spineIt->second].bindTransform;
			NodeAnimation& breathing = idleAnimation_.nodeAnimations["mixamorig:Spine2"];
			breathing.translate.keyframes = {
				{ 0.0f, bind.translate },
				{ 1.0f, { bind.translate.x, bind.translate.y + 0.018f, bind.translate.z } },
				{ 2.0f, bind.translate },
			};
		}
		idleAnimationPlayer_.SetAnimation(&idleAnimation_);
		walkAnimationPlayer_.SetAnimation(&playerModel_->GetAnimation());

		const auto handIt = playerModel_->GetSkeleton().jointMap.find("mixamorig:RightHand");
		if (handIt != playerModel_->GetSkeleton().jointMap.end()) {
			rightHandJointIndex_ = handIt->second;
			selectedJointIndex_ = rightHandJointIndex_;
		}

		weaponObject_ = std::make_unique<Object3d>();
		weaponObject_->Initialize();
		weaponObject_->SetModel("weapon.obj");
		weaponObject_->SetColor({ 0.25f, 0.95f, 1.0f, 1.0f });
		weaponObject_->SetLighting(false);

		playerLoaded_ = true;
		playerStatus_ = "human/walk GPU Skinning: loaded";

		// ゲームプレイ構築中の仮ボス。同じRigを使い、ロックオン等を先に完成させる。
		bossModel_ = std::make_unique<SkinnedModel>();
		bossModel_->Initialize(
			Object3dCommon::GetInstance()->GetDxCommon(),
			Object3dCommon::GetInstance()->GetSrvManager(), humanPath);
		bossModel_->GetAnimationPlayer().SetPlaying(false);
		bossObject_ = std::make_unique<Object3d>();
		bossObject_->Initialize();
		bossObject_->SetTranslate(bossPosition_);
		bossObject_->SetRotate({ 0.0f, bossYaw_, 0.0f });
		bossObject_->SetScale({ 11.0f, 11.0f, 11.0f });
		bossObject_->SetColor({ 1.0f, 0.18f, 0.20f, 1.0f });
		bossObject_->SetLighting(false);
		bossObject_->Update();
		bossLoaded_ = true;
	} catch (const std::exception& error) {
		playerStatus_ = std::string("human/walk load failed: ") + error.what();
	}

	ParticleEmitterConfig handMagic{};
	handMagic.speedMin = 0.2f;
	handMagic.speedMax = 1.8f;
	handMagic.lifeTimeMin = 0.24f;
	handMagic.lifeTimeMax = 0.48f;
	handMagic.gravity = { 0.0f, 0.8f, 0.0f };
	handMagic.startScaleMin = { 0.12f, 0.12f, 0.12f };
	handMagic.startScaleMax = { 0.28f, 0.28f, 0.28f };
	handMagic.endScaleMin = { 0.01f, 0.01f, 0.01f };
	handMagic.endScaleMax = { 0.05f, 0.05f, 0.05f };
	handMagic.startColor = { 0.20f, 1.20f, 1.60f, 1.0f };
	handMagic.endColor = { 0.85f, 0.10f, 1.20f, 0.0f };
	handMagic.modelPath = "neonTriangleParticle.obj";
	handMagic.emitterShape = EmitterShape::Sphere;
	handMagic.shapeSize = { 0.20f, 0.20f, 0.20f };
	handMagic.easingType = EasingType::EaseOut;
	handMagic.isBillboard = true;
	ParticleManager::GetInstance()->RegisterEffect("ActionHandMagic", handMagic);

	UpdateThirdPersonCamera();
}

void Action3DScene::Update()
{
	const bool retryPressed = input_->IsTrigger(input_->GetKey()[DIK_R], input_->GetPreKey()[DIK_R]) ||
		input_->IsTrigger(input_->GetKey()[DIK_RETURN], input_->GetPreKey()[DIK_RETURN]) ||
		input_->IsGamepadButtonTrigger(XINPUT_GAMEPAD_A);
	if (retryPressed) {
		ResetCombat();
	}
	if (combatFinished_) {
		Object3dCommon::GetInstance()->SetShadowFocus(playerPosition_);
		UpdateThirdPersonCamera();
		UpdateJointFeatures();
		BuildSkeletonDebugGeometry();
		ParticleManager::GetInstance()->Update(finalDeltaTime_, camera_.get(), debugCamera_.get());
		ground_->Update();
		DrawControlWindow();
		return;
	}
	UpdateLockOn();
	UpdatePlayer();
	UpdateBoss();
	Object3dCommon::GetInstance()->SetShadowFocus(playerPosition_);
	UpdateThirdPersonCamera();
	UpdateJointFeatures();
	BuildSkeletonDebugGeometry();
	ParticleManager::GetInstance()->Update(finalDeltaTime_, camera_.get(), debugCamera_.get());
	ground_->Update();
	DrawControlWindow();
}

void Action3DScene::UpdatePlayer()
{
	if (!playerLoaded_) {
		return;
	}
	invincibleTimer_ = (std::max)(0.0f, invincibleTimer_ - finalDeltaTime_);
	playerAttackTimer_ = (std::max)(0.0f, playerAttackTimer_ - finalDeltaTime_);
	if (dodgeTimer_ <= 0.0f) {
		stamina_ = (std::min)(maxStamina_, stamina_ + staminaRegen_ * finalDeltaTime_);
	}

	Vector3 inputMove{};
	if (input_->IsPress(input_->GetKey()[DIK_W])) inputMove.z += 1.0f;
	if (input_->IsPress(input_->GetKey()[DIK_S])) inputMove.z -= 1.0f;
	if (input_->IsPress(input_->GetKey()[DIK_A])) inputMove.x -= 1.0f;
	if (input_->IsPress(input_->GetKey()[DIK_D])) inputMove.x += 1.0f;
	const Vector2 leftStick = input_->GetLeftStick();
	inputMove.x += leftStick.x;
	inputMove.z += leftStick.y;

	const float inputLength = std::sqrt(inputMove.x * inputMove.x + inputMove.z * inputMove.z);
	bool isMoving = inputLength > 0.0001f;
	Vector3 worldMove = lastMoveDirection_;
	if (isMoving) {
		inputMove.x /= inputLength;
		inputMove.z /= inputLength;

		// カメラの水平向きを基準に、WASDをワールド移動へ変換する。
		const Vector3 cameraForward = { std::sin(cameraYaw_), 0.0f, std::cos(cameraYaw_) };
		const Vector3 cameraRight = { std::cos(cameraYaw_), 0.0f, -std::sin(cameraYaw_) };
		worldMove = {
			cameraRight.x * inputMove.x + cameraForward.x * inputMove.z,
			0.0f,
			cameraRight.z * inputMove.x + cameraForward.z * inputMove.z,
		};

		lastMoveDirection_ = worldMove;
	}

	const bool dodgePressed = input_->IsTrigger(
		input_->GetKey()[DIK_SPACE], input_->GetPreKey()[DIK_SPACE]) ||
		input_->IsGamepadButtonTrigger(XINPUT_GAMEPAD_B);
	if (dodgePressed && dodgeTimer_ <= 0.0f && stamina_ >= dodgeCost_ && playerHealth_ > 0.0f) {
		dodgeDirection_ = isMoving
			? worldMove
			: Vector3{ std::sin(playerYaw_), 0.0f, std::cos(playerYaw_) };
		dodgeTimer_ = dodgeDuration_;
		invincibleTimer_ = dodgeDuration_ + 0.08f;
		stamina_ -= dodgeCost_;
		dodgeParticleTimer_ = 0.0f;
	}

	if (dodgeTimer_ > 0.0f) {
		dodgeTimer_ = (std::max)(0.0f, dodgeTimer_ - finalDeltaTime_);
		playerPosition_.x += dodgeDirection_.x * dodgeSpeed_ * finalDeltaTime_;
		playerPosition_.z += dodgeDirection_.z * dodgeSpeed_ * finalDeltaTime_;
		playerYaw_ = std::atan2(dodgeDirection_.x, dodgeDirection_.z);
		isMoving = true;
		dodgeParticleTimer_ -= finalDeltaTime_;
		if (dodgeParticleTimer_ <= 0.0f) {
			ParticleManager::GetInstance()->Emit("DashDust", playerPosition_, 4);
			dodgeParticleTimer_ = 0.04f;
		}
	} else if (isMoving && playerAttackTimer_ <= 0.0f) {
		playerPosition_.x += worldMove.x * moveSpeed_ * finalDeltaTime_;
		playerPosition_.z += worldMove.z * moveSpeed_ * finalDeltaTime_;
		playerYaw_ = std::atan2(worldMove.x, worldMove.z);
	}
	if (lockOnEnabled_ && bossLoaded_) {
		const float dx = bossPosition_.x - playerPosition_.x;
		const float dz = bossPosition_.z - playerPosition_.z;
		playerYaw_ = std::atan2(dx, dz);
	}

	playerObject_->SetTranslate(playerPosition_);
	playerObject_->SetRotate({ 0.0f, playerYaw_, 0.0f });
	playerObject_->SetColor(invincibleTimer_ > 0.0f
		? Vector4{ 0.65f, 1.35f, 1.60f, 0.72f }
		: Vector4{ 0.55f, 0.90f, 1.0f, 1.0f });
	const float targetBlend = isMoving ? 1.0f : 0.0f;
	if (useAnimationBlend_) {
		const float blendStep = finalDeltaTime_ / (std::max)(animationBlendDuration_, 0.01f);
		if (locomotionBlend_ < targetBlend) {
			locomotionBlend_ = (std::min)(targetBlend, locomotionBlend_ + blendStep);
		} else {
			locomotionBlend_ = (std::max)(targetBlend, locomotionBlend_ - blendStep);
		}
	} else {
		locomotionBlend_ = targetBlend;
	}
	playerModel_->UpdateBlended(
		finalDeltaTime_, idleAnimationPlayer_, walkAnimationPlayer_, locomotionBlend_);
	playerObject_->Update();
	UpdatePlayerCombat();
}

void Action3DScene::UpdatePlayerCombat()
{
#ifdef USE_IMGUI
	const bool mouseCaptured = ImGui::GetIO().WantCaptureMouse;
#else
	const bool mouseCaptured = false;
#endif
	const bool attackPressed = (!mouseCaptured && input_->IsTrigger(
		input_->GetMouseState().rgbButtons[0], input_->GetPreMouseState().rgbButtons[0])) ||
		input_->IsGamepadButtonTrigger(XINPUT_GAMEPAD_X);
	if (attackPressed && playerAttackTimer_ <= 0.0f && dodgeTimer_ <= 0.0f && playerHealth_ > 0.0f) {
		playerAttackTimer_ = playerAttackDuration_;
		playerAttackHit_ = false;
	}

	const float attackElapsed = playerAttackDuration_ - playerAttackTimer_;
	const bool activeFrame = playerAttackTimer_ > 0.0f && attackElapsed >= 0.10f && attackElapsed <= 0.27f;
	if (!activeFrame || playerAttackHit_ || !bossLoaded_ || bossHealth_ <= 0.0f) {
		return;
	}
	const float dx = bossPosition_.x - playerPosition_.x;
	const float dz = bossPosition_.z - playerPosition_.z;
	const float distance = std::sqrt(dx * dx + dz * dz);
	if (distance > playerAttackRange_ || distance <= 0.0001f) {
		return;
	}
	const Vector3 forward = { std::sin(playerYaw_), 0.0f, std::cos(playerYaw_) };
	const float facing = (dx * forward.x + dz * forward.z) / distance;
	if (facing < 0.25f) {
		return;
	}
	playerAttackHit_ = true;
	bossHealth_ = (std::max)(0.0f, bossHealth_ - playerAttackDamage_);
	ParticleManager::GetInstance()->EmitHitEffect(bossPosition_ + Vector3{ 0.0f, 8.0f, 0.0f });
	if (bossHealth_ <= 0.0f) {
		bossAttackState_ = BossAttackState::Defeated;
		lockOnEnabled_ = false;
		combatFinished_ = true;
		playerWon_ = true;
	}
}

void Action3DScene::UpdateBoss()
{
	if (!bossLoaded_) {
		return;
	}
	float dx = playerPosition_.x - bossPosition_.x;
	float dz = playerPosition_.z - bossPosition_.z;
	float distance = std::sqrt(dx * dx + dz * dz);
	bossYaw_ = std::atan2(dx, dz);
	bossStateTimer_ -= finalDeltaTime_;
	Vector4 bossColor = { 1.0f, 0.18f, 0.20f, 1.0f };

	switch (bossAttackState_) {
	case BossAttackState::Idle:
		if (distance > 22.0f && distance > 0.0001f) {
			bossPosition_.x += (dx / distance) * bossMoveSpeed_ * finalDeltaTime_;
			bossPosition_.z += (dz / distance) * bossMoveSpeed_ * finalDeltaTime_;
		}
		if (bossStateTimer_ <= 0.0f && distance < 34.0f) {
			bossAttackState_ = BossAttackState::Telegraph;
			bossStateTimer_ = 0.90f;
		}
		break;
	case BossAttackState::Telegraph: {
		const float pulse = 0.5f + 0.5f * std::sin(bossStateTimer_ * 32.0f);
		bossColor = { 1.4f, 0.15f + pulse * 0.55f, 0.05f, 1.0f };
		if (bossStateTimer_ <= 0.0f) {
			bossAttackDirection_ = distance > 0.0001f
				? Vector3{ dx / distance, 0.0f, dz / distance }
				: Vector3{ 0.0f, 0.0f, -1.0f };
			bossAttackState_ = BossAttackState::Attack;
			bossStateTimer_ = 0.34f;
			bossAttackHit_ = false;
		}
		break;
	}
	case BossAttackState::Attack:
		bossColor = { 2.0f, 0.06f, 0.03f, 1.0f };
		bossPosition_.x += bossAttackDirection_.x * bossLungeSpeed_ * finalDeltaTime_;
		bossPosition_.z += bossAttackDirection_.z * bossLungeSpeed_ * finalDeltaTime_;
		dx = playerPosition_.x - bossPosition_.x;
		dz = playerPosition_.z - bossPosition_.z;
		distance = std::sqrt(dx * dx + dz * dz);
		if (!bossAttackHit_ && distance <= bossAttackRange_) {
			bossAttackHit_ = true;
			DamagePlayer(bossAttackDamage_);
		}
		if (bossStateTimer_ <= 0.0f) {
			bossAttackState_ = BossAttackState::Recovery;
			bossStateTimer_ = 0.85f;
		}
		break;
	case BossAttackState::Recovery:
		bossColor = { 0.42f, 0.06f, 0.08f, 1.0f };
		if (bossStateTimer_ <= 0.0f) {
			bossAttackState_ = BossAttackState::Idle;
			bossStateTimer_ = 1.15f;
		}
		break;
	case BossAttackState::Defeated:
		bossColor = { 0.08f, 0.08f, 0.08f, 0.35f };
		break;
	}

	bossObject_->SetTranslate(bossPosition_);
	bossObject_->SetRotate({ 0.0f, bossYaw_, 0.0f });
	bossObject_->SetColor(bossColor);
	bossModel_->Update(finalDeltaTime_);
	bossObject_->Update();
}

void Action3DScene::DamagePlayer(float damage)
{
	if (invincibleTimer_ > 0.0f || playerHealth_ <= 0.0f) {
		return;
	}
	playerHealth_ = (std::max)(0.0f, playerHealth_ - damage);
	invincibleTimer_ = 0.22f;
	ParticleManager::GetInstance()->EmitHitEffect(playerPosition_ + Vector3{ 0.0f, 7.0f, 0.0f });
	if (playerHealth_ <= 0.0f) {
		combatFinished_ = true;
		playerWon_ = false;
		lockOnEnabled_ = false;
	}
}

void Action3DScene::ResetCombat()
{
	playerPosition_ = { 0.0f, -25.0f, 0.0f };
	bossPosition_ = { 0.0f, -25.0f, 58.0f };
	playerHealth_ = playerMaxHealth_;
	stamina_ = maxStamina_;
	bossHealth_ = bossMaxHealth_;
	dodgeTimer_ = 0.0f;
	invincibleTimer_ = 0.0f;
	playerAttackTimer_ = 0.0f;
	bossAttackState_ = BossAttackState::Idle;
	bossStateTimer_ = 1.2f;
	combatFinished_ = false;
	playerWon_ = false;
	lockOnEnabled_ = true;
}

void Action3DScene::UpdateLockOn()
{
	const bool toggleKeyboard = input_->IsTrigger(
		input_->GetKey()[DIK_Q], input_->GetPreKey()[DIK_Q]);
	const bool toggleGamepad = input_->IsGamepadButtonTrigger(XINPUT_GAMEPAD_RIGHT_THUMB);
	if (toggleKeyboard || toggleGamepad) {
		lockOnEnabled_ = !lockOnEnabled_;
	}
	if (lockOnEnabled_ && bossLoaded_) {
		const float dx = bossPosition_.x - playerPosition_.x;
		const float dz = bossPosition_.z - playerPosition_.z;
		const float distance = std::sqrt(dx * dx + dz * dz);
		if (distance > lockOnMaxDistance_) {
			lockOnEnabled_ = false;
		}
	}
}

void Action3DScene::UpdateThirdPersonCamera()
{
#ifdef USE_IMGUI
	const bool captureMouse = ImGui::GetIO().WantCaptureMouse;
#else
	const bool captureMouse = false;
#endif
	if (!captureMouse && !lockOnEnabled_) {
		const DIMOUSESTATE& mouse = input_->GetMouseState();
		cameraYaw_ += static_cast<float>(mouse.lX) * mouseSensitivity_;
		cameraPitch_ += static_cast<float>(mouse.lY) * mouseSensitivity_;
	}
	const Vector2 rightStick = input_->GetRightStick();
	if (!lockOnEnabled_) {
		cameraYaw_ += rightStick.x * gamepadCameraSpeed_ * finalDeltaTime_;
		cameraPitch_ -= rightStick.y * gamepadCameraSpeed_ * finalDeltaTime_;
	} else if (bossLoaded_) {
		const float targetYaw = std::atan2(
			bossPosition_.x - playerPosition_.x,
			bossPosition_.z - playerPosition_.z);
		const float angleDifference = std::remainder(targetYaw - cameraYaw_, 6.28318530718f);
		const float turnAmount = (std::clamp)(
			angleDifference, -lockOnCameraTurnSpeed_ * finalDeltaTime_, lockOnCameraTurnSpeed_ * finalDeltaTime_);
		cameraYaw_ += turnAmount;
	}
	cameraPitch_ = (std::clamp)(cameraPitch_, -0.15f, 1.10f);

	// Cameraのローカル+Zを注視方向にし、プレイヤー背後へ配置する。
	const float cosPitch = std::cos(cameraPitch_);
	const Vector3 forward = {
		std::sin(cameraYaw_) * cosPitch,
		-std::sin(cameraPitch_),
		std::cos(cameraYaw_) * cosPitch,
	};
	const Vector3 target = {
		playerPosition_.x,
		playerPosition_.y + cameraHeight_,
		playerPosition_.z,
	};
	const Vector3 eye = {
		target.x - forward.x * cameraDistance_,
		target.y - forward.y * cameraDistance_,
		target.z - forward.z * cameraDistance_,
	};

	camera_->SetRotate({ cameraPitch_, cameraYaw_, 0.0f });
	camera_->SetTranslate(eye);
	camera_->Update();
}

Matrix4x4 Action3DScene::GetPlayerWorldMatrix() const
{
	return MakeAffineMatrix(
		Vector3{ 8.0f, 8.0f, 8.0f },
		Vector3{ 0.0f, playerYaw_, 0.0f },
		playerPosition_);
}

Vector3 Action3DScene::GetJointWorldPosition(int32_t jointIndex) const
{
	if (!playerLoaded_ || jointIndex < 0 ||
		static_cast<size_t>(jointIndex) >= playerModel_->GetSkeleton().joints.size()) {
		return playerPosition_;
	}
	const Matrix4x4 jointWorld = Multiply(
		playerModel_->GetSkeleton().joints[jointIndex].skeletonSpaceMatrix,
		GetPlayerWorldMatrix());
	return TransformMatrix(Vector3{ 0.0f, 0.0f, 0.0f }, jointWorld);
}

void Action3DScene::UpdateJointFeatures()
{
	if (!playerLoaded_ || rightHandJointIndex_ < 0) {
		return;
	}

	const Joint& rightHand = playerModel_->GetSkeleton().joints[rightHandJointIndex_];
	const Matrix4x4 handWorld = Multiply(rightHand.skeletonSpaceMatrix, GetPlayerWorldMatrix());
	rightHandWorldPosition_ = TransformMatrix(Vector3{ 0.0f, 0.0f, 0.0f }, handWorld);

	if (attachWeaponToHand_ && weaponObject_) {
		Vector3 animatedWeaponRotate = weaponLocalRotate_;
		if (playerAttackTimer_ > 0.0f) {
			const float normalized = 1.0f - playerAttackTimer_ / playerAttackDuration_;
			animatedWeaponRotate.y += std::sin(normalized * 3.14159265f) * 2.35f;
			animatedWeaponRotate.z -= normalized * 1.15f;
		}
		const Matrix4x4 localAttachment = MakeAffineMatrix(
			weaponLocalScale_, animatedWeaponRotate, weaponLocalOffset_);
		weaponObject_->UpdateWithWorldMatrix(Multiply(localAttachment, handWorld));
	}

	if (emitParticlesFromHand_) {
		handParticleTimer_ -= finalDeltaTime_;
		if (handParticleTimer_ <= 0.0f) {
			ParticleManager::GetInstance()->Emit("ActionHandMagic", rightHandWorldPosition_, 3);
			handParticleTimer_ = handParticleInterval_;
		}
	} else {
		handParticleTimer_ = 0.0f;
	}
}

void Action3DScene::BuildSkeletonDebugGeometry()
{
	skeletonRenderer_->BeginFrame();
	if (!showSkeletonDebug_ || !playerLoaded_) {
		return;
	}

	const Skeleton& skeleton = playerModel_->GetSkeleton();
	const float cosPitch = std::cos(cameraPitch_);
	const Vector3 cameraForward = {
		std::sin(cameraYaw_) * cosPitch,
		-std::sin(cameraPitch_),
		std::cos(cameraYaw_) * cosPitch,
	};
	for (const Joint& joint : skeleton.joints) {
		if (!joint.parent.has_value()) {
			continue;
		}
		const Vector3 childPosition = GetJointWorldPosition(joint.index);
		const Vector3 parentPosition = GetJointWorldPosition(*joint.parent);
		const bool selected = joint.index == selectedJointIndex_ || *joint.parent == selectedJointIndex_;
		const Vector4 color = selected
			? Vector4{ 1.0f, 0.25f, 0.85f, 1.0f }
			: Vector4{ 0.10f, 0.95f, 1.0f, 0.72f };
		skeletonRenderer_->QueueCameraFacingLine(
			parentPosition, childPosition, skeletonLineWidth_, color, cameraForward);
	}

	if (showSelectedJointAxes_ && selectedJointIndex_ >= 0 &&
		static_cast<size_t>(selectedJointIndex_) < skeleton.joints.size()) {
		const Matrix4x4 selectedWorld = Multiply(
			skeleton.joints[selectedJointIndex_].skeletonSpaceMatrix,
			GetPlayerWorldMatrix());
		const Vector3 origin = TransformMatrix(Vector3{ 0.0f, 0.0f, 0.0f }, selectedWorld);
		// selectedWorldにはプレイヤーの8倍スケールも含まれる。
		const float axisLength = 0.18f;
		const Vector3 xEnd = TransformMatrix(Vector3{ axisLength, 0.0f, 0.0f }, selectedWorld);
		const Vector3 yEnd = TransformMatrix(Vector3{ 0.0f, axisLength, 0.0f }, selectedWorld);
		const Vector3 zEnd = TransformMatrix(Vector3{ 0.0f, 0.0f, axisLength }, selectedWorld);
		skeletonRenderer_->QueueCameraFacingLine(origin, xEnd, skeletonLineWidth_ * 1.4f, { 1, 0.1f, 0.1f, 1 }, cameraForward);
		skeletonRenderer_->QueueCameraFacingLine(origin, yEnd, skeletonLineWidth_ * 1.4f, { 0.1f, 1, 0.1f, 1 }, cameraForward);
		skeletonRenderer_->QueueCameraFacingLine(origin, zEnd, skeletonLineWidth_ * 1.4f, { 0.1f, 0.3f, 1, 1 }, cameraForward);
	}
}

void Action3DScene::DrawControlWindow()
{
#ifdef USE_IMGUI
	ImGui::Begin("3D Action Scene");
	ImGui::Text("WASD / Left Stick : Move");
	ImGui::Text("Mouse / Right Stick : Orbit camera");
	ImGui::Text("Q / Right Stick Click : Toggle lock-on");
	ImGui::Text("Space / B : Dodge    Left Click / X : Attack    R : Reset");
	ImGui::Separator();
	ImGui::TextColored(
		playerLoaded_ ? ImVec4{ 0.40f, 1.0f, 0.60f, 1.0f } : ImVec4{ 1.0f, 0.40f, 0.30f, 1.0f },
		"%s", playerStatus_.c_str());
	ImGui::DragFloat("Move Speed", &moveSpeed_, 0.5f, 1.0f, 100.0f);
	ImGui::DragFloat("Camera Distance", &cameraDistance_, 0.5f, 20.0f, 180.0f);
	ImGui::DragFloat("Camera Height", &cameraHeight_, 0.2f, 0.0f, 60.0f);
	ImGui::DragFloat("Mouse Sensitivity", &mouseSensitivity_, 0.0001f, 0.0005f, 0.02f, "%.4f");
	ImGui::Text("Position: %.1f, %.1f, %.1f", playerPosition_.x, playerPosition_.y, playerPosition_.z);
	ImGui::SeparatorText("Boss Action Prototype");
	ImGui::Checkbox("Lock On", &lockOnEnabled_);
	ImGui::DragFloat("Lock On Max Distance", &lockOnMaxDistance_, 1.0f, 20.0f, 400.0f);
	if (bossLoaded_) {
		const float dx = bossPosition_.x - playerPosition_.x;
		const float dz = bossPosition_.z - playerPosition_.z;
		ImGui::Text("Boss distance: %.1f", std::sqrt(dx * dx + dz * dz));
		ImGui::TextColored({ 1.0f, 0.35f, 0.35f, 1.0f }, "Temporary boss: human Rig (replace later)");
	}
	ImGui::ProgressBar(playerHealth_ / playerMaxHealth_, ImVec2(-1.0f, 0.0f), "Player HP");
	ImGui::ProgressBar(stamina_ / maxStamina_, ImVec2(-1.0f, 0.0f), "Stamina");
	ImGui::ProgressBar(bossHealth_ / bossMaxHealth_, ImVec2(-1.0f, 0.0f), "Boss HP");
	const char* bossStateName = "Idle / Approach";
	switch (bossAttackState_) {
	case BossAttackState::Telegraph: bossStateName = "TELEGRAPH - DODGE!"; break;
	case BossAttackState::Attack: bossStateName = "ATTACK"; break;
	case BossAttackState::Recovery: bossStateName = "RECOVERY - ATTACK NOW"; break;
	case BossAttackState::Defeated: bossStateName = "DEFEATED"; break;
	default: break;
	}
	ImGui::Text("Boss State: %s", bossStateName);
	ImGui::Text("Invincible: %s (%.2f)", invincibleTimer_ > 0.0f ? "YES" : "NO", invincibleTimer_);
	if (combatFinished_) {
		ImGui::Separator();
		ImGui::TextColored(
			playerWon_ ? ImVec4{ 0.35f, 1.0f, 0.55f, 1.0f } : ImVec4{ 1.0f, 0.20f, 0.20f, 1.0f },
			playerWon_ ? "VICTORY" : "YOU DIED");
		ImGui::Text("Press Enter / A / R to retry");
	}
	if (ImGui::Button("Reset Combat")) {
		ResetCombat();
	}
	ImGui::SeparatorText("CG4 Evaluation Features");
	ImGui::Checkbox("Idle <-> Walk Animation Blend (5)", &useAnimationBlend_);
	ImGui::DragFloat("Blend Duration", &animationBlendDuration_, 0.01f, 0.01f, 2.0f, "%.2f sec");
	ImGui::ProgressBar(locomotionBlend_, ImVec2(-1.0f, 0.0f), locomotionBlend_ < 0.5f ? "Idle Breathing" : "Walk");
	ImGui::TextDisabled("Translate/Scale=Lerp, Rotation=Quaternion Slerp; both clips keep playing.");
	ImGui::Checkbox("Skeleton Debug (10)", &showSkeletonDebug_);
	ImGui::SameLine();
	ImGui::Checkbox("Selected Joint Axes", &showSelectedJointAxes_);
	if (playerLoaded_ && !playerModel_->GetSkeleton().joints.empty()) {
		const int32_t maxJoint = static_cast<int32_t>(playerModel_->GetSkeleton().joints.size()) - 1;
		selectedJointIndex_ = (std::clamp)(selectedJointIndex_, 0, maxJoint);
		ImGui::SliderInt("Selected Joint", &selectedJointIndex_, 0, maxJoint);
		ImGui::Text("Joint Name: %s", playerModel_->GetSkeleton().joints[selectedJointIndex_].name.c_str());
	}
	ImGui::DragFloat("Bone Line Width", &skeletonLineWidth_, 0.01f, 0.02f, 0.8f);
	ImGui::Checkbox("Weapon follows RightHand Joint (10)", &attachWeaponToHand_);
	if (ImGui::TreeNode("Weapon Attachment Transform")) {
		ImGui::DragFloat3("Local Scale", &weaponLocalScale_.x, 0.01f, 0.01f, 5.0f);
		ImGui::DragFloat3("Local Rotation", &weaponLocalRotate_.x, 0.01f, -6.3f, 6.3f);
		ImGui::DragFloat3("Local Offset", &weaponLocalOffset_.x, 0.01f, -10.0f, 10.0f);
		ImGui::TreePop();
	}
	ImGui::Checkbox("GPU Particle from RightHand Joint (10)", &emitParticlesFromHand_);
	ImGui::DragFloat("Emit Interval", &handParticleInterval_, 0.005f, 0.01f, 0.5f);
	ImGui::Text("RightHand: %.2f, %.2f, %.2f", rightHandWorldPosition_.x, rightHandWorldPosition_.y, rightHandWorldPosition_.z);
	ImGui::Text("Particle update: %s", ParticleManager::GetInstance()->IsUseGpuUpdate() ? "GPU" : "CPU");
	ImGui::End();
#endif
}

void Action3DScene::Draw()
{
}

void Action3DScene::DrawPostEffect3D()
{
	Object3dCommon::GetInstance()->PreDraw(kNone);
	ground_->Draw();
	if (bossLoaded_) {
		bossObject_->DrawSkinned(*bossModel_);
	}
	if (playerLoaded_) {
		playerObject_->DrawSkinned(*playerModel_);
		if (attachWeaponToHand_ && weaponObject_) {
			weaponObject_->Draw();
		}
	}
	if (showSkeletonDebug_) {
		skeletonRenderer_->DrawAll(camera_->GetViewProjectionMatrix());
	}
	ParticleManager::GetInstance()->Draw();
}

void Action3DScene::DrawShadow()
{
	Object3dCommon::GetInstance()->PreDraw(kShadow);
	if (playerLoaded_) {
		playerObject_->DrawSkinnedShadow(*playerModel_);
		if (attachWeaponToHand_ && weaponObject_) {
			weaponObject_->DrawShadow();
		}
	}
	if (bossLoaded_) {
		bossObject_->DrawSkinnedShadow(*bossModel_);
	}
}

void Action3DScene::DrawSprite()
{
}
