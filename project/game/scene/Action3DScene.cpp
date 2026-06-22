#include "Action3DScene.h"

#include <algorithm>
#include <cmath>
#include <exception>

#include "Object3dCommon.h"

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
		playerLoaded_ = true;
		playerStatus_ = "human/walk GPU Skinning: loaded";
	} catch (const std::exception& error) {
		playerStatus_ = std::string("human/walk load failed: ") + error.what();
	}

	UpdateThirdPersonCamera();
}

void Action3DScene::Update()
{
	UpdatePlayer();
	UpdateThirdPersonCamera();
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

void Action3DScene::DrawControlWindow()
{
#ifdef USE_IMGUI
	ImGui::Begin("3D Action Scene");
	ImGui::Text("WASD : Move");
	ImGui::Text("Mouse : Orbit camera");
	ImGui::Separator();
	ImGui::TextColored(
		playerLoaded_ ? ImVec4{ 0.40f, 1.0f, 0.60f, 1.0f } : ImVec4{ 1.0f, 0.40f, 0.30f, 1.0f },
		"%s", playerStatus_.c_str());
	ImGui::DragFloat("Move Speed", &moveSpeed_, 0.5f, 1.0f, 100.0f);
	ImGui::DragFloat("Camera Distance", &cameraDistance_, 0.5f, 20.0f, 180.0f);
	ImGui::DragFloat("Camera Height", &cameraHeight_, 0.2f, 0.0f, 60.0f);
	ImGui::DragFloat("Mouse Sensitivity", &mouseSensitivity_, 0.0001f, 0.0005f, 0.02f, "%.4f");
	ImGui::Text("Position: %.1f, %.1f, %.1f", playerPosition_.x, playerPosition_.y, playerPosition_.z);
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
	}
}

void Action3DScene::DrawShadow()
{
	Object3dCommon::GetInstance()->PreDraw(kShadow);
	if (playerLoaded_) {
		playerObject_->DrawSkinnedShadow(*playerModel_);
	}
}

void Action3DScene::DrawSprite()
{
}
