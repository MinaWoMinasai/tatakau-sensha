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

		playerModel_->GetAnimationPlayer().SetPlaying(false);

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
	UpdatePlayer();
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

	Vector3 inputMove{};
	if (input_->IsPress(input_->GetKey()[DIK_W])) inputMove.z += 1.0f;
	if (input_->IsPress(input_->GetKey()[DIK_S])) inputMove.z -= 1.0f;
	if (input_->IsPress(input_->GetKey()[DIK_A])) inputMove.x -= 1.0f;
	if (input_->IsPress(input_->GetKey()[DIK_D])) inputMove.x += 1.0f;
	const Vector2 leftStick = input_->GetLeftStick();
	inputMove.x += leftStick.x;
	inputMove.z += leftStick.y;

	const float inputLength = std::sqrt(inputMove.x * inputMove.x + inputMove.z * inputMove.z);
	const bool isMoving = inputLength > 0.0001f;
	if (isMoving) {
		inputMove.x /= inputLength;
		inputMove.z /= inputLength;

		// カメラの水平向きを基準に、WASDをワールド移動へ変換する。
		const Vector3 cameraForward = { std::sin(cameraYaw_), 0.0f, std::cos(cameraYaw_) };
		const Vector3 cameraRight = { std::cos(cameraYaw_), 0.0f, -std::sin(cameraYaw_) };
		Vector3 worldMove = {
			cameraRight.x * inputMove.x + cameraForward.x * inputMove.z,
			0.0f,
			cameraRight.z * inputMove.x + cameraForward.z * inputMove.z,
		};

		playerPosition_.x += worldMove.x * moveSpeed_ * finalDeltaTime_;
		playerPosition_.z += worldMove.z * moveSpeed_ * finalDeltaTime_;
		playerYaw_ = std::atan2(worldMove.x, worldMove.z);
	}

	playerObject_->SetTranslate(playerPosition_);
	playerObject_->SetRotate({ 0.0f, playerYaw_, 0.0f });
	playerModel_->GetAnimationPlayer().SetPlaying(isMoving);
	playerModel_->Update(finalDeltaTime_);
	playerObject_->Update();
}

void Action3DScene::UpdateThirdPersonCamera()
{
#ifdef USE_IMGUI
	const bool captureMouse = ImGui::GetIO().WantCaptureMouse;
#else
	const bool captureMouse = false;
#endif
	if (!captureMouse) {
		const DIMOUSESTATE& mouse = input_->GetMouseState();
		cameraYaw_ += static_cast<float>(mouse.lX) * mouseSensitivity_;
		cameraPitch_ += static_cast<float>(mouse.lY) * mouseSensitivity_;
	}
	const Vector2 rightStick = input_->GetRightStick();
	cameraYaw_ += rightStick.x * gamepadCameraSpeed_ * finalDeltaTime_;
	cameraPitch_ -= rightStick.y * gamepadCameraSpeed_ * finalDeltaTime_;
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
		const Matrix4x4 localAttachment = MakeAffineMatrix(
			weaponLocalScale_, weaponLocalRotate_, weaponLocalOffset_);
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
	ImGui::Separator();
	ImGui::TextColored(
		playerLoaded_ ? ImVec4{ 0.40f, 1.0f, 0.60f, 1.0f } : ImVec4{ 1.0f, 0.40f, 0.30f, 1.0f },
		"%s", playerStatus_.c_str());
	ImGui::DragFloat("Move Speed", &moveSpeed_, 0.5f, 1.0f, 100.0f);
	ImGui::DragFloat("Camera Distance", &cameraDistance_, 0.5f, 20.0f, 180.0f);
	ImGui::DragFloat("Camera Height", &cameraHeight_, 0.2f, 0.0f, 60.0f);
	ImGui::DragFloat("Mouse Sensitivity", &mouseSensitivity_, 0.0001f, 0.0005f, 0.02f, "%.4f");
	ImGui::Text("Position: %.1f, %.1f, %.1f", playerPosition_.x, playerPosition_.y, playerPosition_.z);
	ImGui::SeparatorText("CG4 Evaluation Features");
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
}

void Action3DScene::DrawSprite()
{
}
