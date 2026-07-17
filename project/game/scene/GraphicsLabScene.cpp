#include "GraphicsLabScene.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

#include "ModelManager.h"
#include "Object3dCommon.h"
#include "TextureManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
const char* kPbrSphereModelName = "__primitive_pbr_sphere";
}

void GraphicsLabScene::Initialize()
{
	input_ = Input::GetInstance();

	camera_ = std::make_unique<Camera>();
	camera_->SetNearClip(0.3f);
	camera_->SetFarClip(2400.0f);
	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize();
	debugCamera_->GetDistance() = cameraDistance_;
	debugCamera_->SetNearClip(0.3f);
	debugCamera_->SetFarClip(2400.0f);

	Object3dCommon::GetInstance()->SetDefaultCamera(camera_.get());
	Object3dCommon::GetInstance()->SetDebugDefaultCamera(debugCamera_.get());
	Object3dCommon::GetInstance()->SetIsDebugCamera(false);
	Object3dCommon::GetInstance()->SetShadowFocus({ 0.0f, 0.0f, 32.0f });
	Object3dCommon::GetInstance()->SetShadowRange(210.0f);
	Object3dCommon::GetInstance()->GetLightDir() = { -0.36f, -0.30f, -0.88f };

	ModelManager::GetInstance()->LoadModel("graphicsOcean.obj");
	ModelManager::GetInstance()->LoadModel("graphicsSand.obj");
	ModelManager::GetInstance()->LoadModel("graphicsBeach.obj");
	ModelManager::GetInstance()->LoadModel("cube.obj");
	ModelManager::GetInstance()->LoadModel("ball.obj");
	ModelManager::GetInstance()->LoadModel("jewelry.obj");
	ModelManager::GetInstance()->CreateUvSphereModel(kPbrSphereModelName, 1.0f, 64, 128);
	if (Model* ballModel = ModelManager::GetInstance()->FindModel("ball.obj")) {
		ballModel->RecalculateSmoothNormals();
	}
	TextureManager::GetInstance()->LoadTexture("resources/skybox.dds");
	const uint32_t skyboxSrv = TextureManager::GetInstance()->GetSrvIndex("resources/skybox.dds");

	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize("resources/skybox.dds");
	skybox_->SetColor({ 1.0f, 1.0f, 1.0f, 2.0f });

	river_ = std::make_unique<Object3d>();
	river_->Initialize();
	river_->SetModel("graphicsOcean.obj");
	river_->SetColor(riverTint_);
	river_->SetLighting(false);
	river_->SetEnvironmentMap(skyboxSrv);
	river_->SetEnvironmentCoefficient(2.75f);
	river_->SetInsensity(waterLightIntensity_);
	river_->SetScale({ 1.0f, 1.0f, 1.0f });
	river_->SetTranslate({ 0.0f, -1.15f, 0.0f });

	sandBed_ = std::make_unique<Object3d>();
	sandBed_->Initialize();
	sandBed_->SetModel("graphicsSand.obj");
	sandBed_->SetColor({ 0.78f, 0.75f, 0.62f, 1.0f });
	sandBed_->SetLighting(false);
	sandBed_->SetEnvironmentCoefficient(-1.0f);
	sandBed_->SetScale({ 1.0f, 1.0f, 1.0f });
	sandBed_->SetTranslate({ 0.0f, -4.2f, 0.0f });

	sceneObjects_.push_back(MakeObject(
		"graphicsBeach.obj",
		{ 0.0f, -1.06f, 0.0f },
		{ 0.0f, 0.0f, 0.0f },
		{ 1.0f, 1.0f, 1.0f },
		{ 0.96f, 0.90f, 0.70f, 1.0f },
		false,
		-1.0f,
		32.0f,
		LabObjectKind::Beach));

	sceneObjects_.push_back(MakeObject(
		"cube.obj",
		{ -330.0f, -0.45f, -74.0f },
		{ 0.08f, 0.54f, -0.18f },
		{ 34.0f, 10.0f, 20.0f },
		{ 0.32f, 0.36f, 0.34f, 1.0f },
		true,
		0.08f,
		42.0f,
		LabObjectKind::Obstacle));
	sceneObjects_.push_back(MakeObject(
		"cube.obj",
		{ -272.0f, -0.72f, -44.0f },
		{ -0.12f, -0.26f, 0.10f },
		{ 15.0f, 5.8f, 12.0f },
		{ 0.38f, 0.40f, 0.36f, 1.0f },
		true,
		0.04f,
		28.0f,
		LabObjectKind::Obstacle));
	sceneObjects_.push_back(MakeObject(
		"cube.obj",
		{ 338.0f, -0.50f, -82.0f },
		{ -0.10f, -0.46f, 0.15f },
		{ 30.0f, 9.0f, 18.0f },
		{ 0.30f, 0.34f, 0.33f, 1.0f },
		true,
		0.08f,
		42.0f,
		LabObjectKind::Obstacle));
	sceneObjects_.push_back(MakeObject(
		"cube.obj",
		{ 220.0f, -1.05f, 96.0f },
		{ 0.16f, 0.74f, -0.08f },
		{ 12.0f, 4.0f, 9.0f },
		{ 0.34f, 0.37f, 0.35f, 1.0f },
		true,
		0.04f,
		24.0f,
		LabObjectKind::Obstacle));

	auto addPbrSample = [&](const std::string& modelPath, const Vector3& translate, const Vector3& scale,
		const Vector4& color, float metallic, float roughness) {
		LabObject sample = MakeObject(
			modelPath,
			translate,
			{ 0.0f, 0.0f, 0.0f },
			scale,
			color,
			true,
			0.78f,
			32.0f,
			LabObjectKind::Scene);
		sample.object->SetLightingMode(2);
		sample.object->SetMetallic(metallic);
		sample.object->SetRoughness(roughness);
		sample.object->SetAmbientOcclusion(1.0f);
		sample.object->SetEnvironmentMap(skyboxSrv);
		sample.object->SetIBLIntensity(pbrIblDiffuseIntensity_, pbrIblSpecularIntensity_);
		sample.object->SetIBLMaxMipLevel(pbrIblMaxMipLevel_);
		sample.object->SetPBREnvironmentMode(usePbrProceduralEnvironment_ ? 1.0f : 0.0f);
		sample.object->SetShadowReceiveStrength(enablePbrSampleShadows_ ? 1.0f : 0.0f);
		sample.object->SetNormalDetail(pbrNormalDetailStrength_, pbrNormalDetailScale_);
		sample.object->SetInsensity(pbrDirectLightIntensity_);
		sample.shadowReceiveStrength = 0.0f;
		sample.castsShadow = false;
		metalObjects_.push_back(std::move(sample));
	};

	addPbrSample(kPbrSphereModelName, { -48.0f, 8.8f, -12.0f }, { 7.0f, 7.0f, 7.0f }, { 1.00f, 0.77f, 0.34f, 1.0f }, 1.0f, 0.18f);
	addPbrSample(kPbrSphereModelName, { -16.0f, 8.8f, -12.0f }, { 7.0f, 7.0f, 7.0f }, { 0.92f, 0.95f, 1.00f, 1.0f }, 1.0f, 0.56f);
	addPbrSample(kPbrSphereModelName, { 16.0f, 8.8f, -12.0f }, { 7.0f, 7.0f, 7.0f }, { 0.12f, 0.38f, 0.78f, 1.0f }, 0.0f, 0.28f);
	addPbrSample("jewelry.obj", { 52.0f, 7.6f, -12.0f }, { 4.2f, 4.2f, 4.2f }, { 0.82f, 0.88f, 0.90f, 1.0f }, 1.0f, 0.34f);

	UpdateCamera();
	sandBed_->Update();
	river_->Update();
	for (auto& object : sceneObjects_) {
		object.object->Update();
	}
	for (auto& object : metalObjects_) {
		object.object->Update();
	}
}

void GraphicsLabScene::Update()
{
	if (input_->IsTrigger(input_->GetKey()[DIK_ESCAPE], input_->GetPreKey()[DIK_ESCAPE])) {
		finished_ = true;
		nextSceneName_ = "TITLE";
		return;
	}

	if (!pauseWater_) {
		sceneTime_ += finalDeltaTime_ * waterTimeScale_;
	}

	if (!Object3dCommon::GetInstance()->GetIsDebugCamera()) {
		UpdateCamera();
	}
	debugCamera_->Update(input_->GetMouseState(), input_->GetKey(), input_->GetLeftStick());
	skybox_->Update(camera_.get(), debugCamera_.get());
	river_->SetColor(riverTint_);
	river_->SetShininess(sceneTime_);
	river_->SetInsensity(waterLightIntensity_);
	river_->Update();
	sandBed_->Update();

	for (auto& object : sceneObjects_) {
		object.object->SetEnvironmentCoefficient(object.environment);
		object.object->SetShininess(object.shininess);
		object.object->Update();
	}
	for (size_t i = 0; i < metalObjects_.size(); ++i) {
		auto& object = metalObjects_[i];
		if (i >= 3) {
			Vector3 rotate = object.object->GetRotate();
			rotate.y += finalDeltaTime_ * (0.35f + static_cast<float>(i) * 0.12f);
			object.object->SetRotate(rotate);
		}
		object.object->SetEnvironmentCoefficient(object.environment);
		object.object->SetShininess(object.shininess);
		object.object->SetIBLIntensity(pbrIblDiffuseIntensity_, pbrIblSpecularIntensity_);
		object.object->SetIBLMaxMipLevel(pbrIblMaxMipLevel_);
		object.object->SetPBREnvironmentMode(usePbrProceduralEnvironment_ ? 1.0f : 0.0f);
		object.object->SetShadowReceiveStrength(enablePbrSampleShadows_ ? 1.0f : object.shadowReceiveStrength);
		object.object->SetNormalDetail(pbrNormalDetailStrength_, pbrNormalDetailScale_);
		object.object->SetInsensity(pbrDirectLightIntensity_);
		object.object->Update();
	}

	DrawDebugWindow();
}

void GraphicsLabScene::DrawShadow()
{
	Object3dCommon::GetInstance()->PreDraw(kShadow);
	for (auto& object : sceneObjects_) {
		if (ShouldDrawLabObject(object) && object.castsShadow) {
			object.object->DrawShadow();
		}
	}
	if (showPbrSamples_ && enablePbrSampleShadows_) {
		for (auto& object : metalObjects_) {
			object.object->DrawShadow();
		}
	}
}

void GraphicsLabScene::DrawPostEffect3D()
{
	skybox_->Draw();

	Object3dCommon::GetInstance()->PreDraw(kNone);
	if (showSandBed_) {
		sandBed_->Draw();
	}
	for (auto& object : sceneObjects_) {
		if (ShouldDrawLabObject(object)) {
			object.object->Draw();
		}
	}
	if (showPbrSamples_) {
		for (auto& object : metalObjects_) {
			object.object->Draw();
		}
	}

	Object3dCommon::GetInstance()->PreDraw(kNormal);
	river_->Draw();
}

GraphicsLabScene::LabObject GraphicsLabScene::MakeObject(
	const std::string& modelPath,
	const Vector3& translate,
	const Vector3& rotate,
	const Vector3& scale,
	const Vector4& color,
	bool lighting,
	float environment,
	float shininess,
	GraphicsLabScene::LabObjectKind kind)
{
	LabObject result;
	result.object = std::make_unique<Object3d>();
	result.object->Initialize();
	result.object->SetModel(modelPath);
	result.object->SetTranslate(translate);
	result.object->SetRotate(rotate);
	result.object->SetScale(scale);
	result.object->SetColor(color);
	result.object->SetLighting(lighting);
	result.object->SetEnvironmentCoefficient(environment);
	result.object->SetShininess(shininess);
	result.object->SetInsensity(1.15f);
	result.environment = environment;
	result.shininess = shininess;
	result.shadowReceiveStrength = 1.0f;
	result.castsShadow = true;
	result.kind = kind;
	return result;
}

bool GraphicsLabScene::ShouldDrawLabObject(const LabObject& object) const
{
	switch (object.kind) {
	case LabObjectKind::Beach:
		return showBeach_;
	case LabObjectKind::Obstacle:
		return showObstacles_;
	case LabObjectKind::Scene:
	default:
		return true;
	}
}

void GraphicsLabScene::UpdateCamera()
{
	cameraYaw_ += (input_->IsPress(input_->GetKey()[DIK_D]) ? 1.0f : 0.0f) * finalDeltaTime_ * 0.9f;
	cameraYaw_ -= (input_->IsPress(input_->GetKey()[DIK_A]) ? 1.0f : 0.0f) * finalDeltaTime_ * 0.9f;
	cameraPitch_ += (input_->IsPress(input_->GetKey()[DIK_W]) ? 1.0f : 0.0f) * finalDeltaTime_ * 0.55f;
	cameraPitch_ -= (input_->IsPress(input_->GetKey()[DIK_S]) ? 1.0f : 0.0f) * finalDeltaTime_ * 0.55f;
	cameraPitch_ = std::clamp(cameraPitch_, 0.10f, 1.02f);

	const float wheel = static_cast<float>(input_->GetMouseState().lZ);
	if (std::abs(wheel) > 0.0f) {
		cameraDistance_ = std::clamp(cameraDistance_ - wheel * 0.018f, 36.0f, 180.0f);
	}

	const float cosPitch = std::cos(cameraPitch_);
	const Vector3 eye = {
		cameraTarget_.x + std::sin(cameraYaw_) * cosPitch * cameraDistance_,
		cameraTarget_.y + std::sin(cameraPitch_) * cameraDistance_,
		cameraTarget_.z - std::cos(cameraYaw_) * cosPitch * cameraDistance_,
	};
	const Vector3 diff = {
		cameraTarget_.x - eye.x,
		cameraTarget_.y - eye.y,
		cameraTarget_.z - eye.z,
	};
	const float yaw = std::atan2(diff.x, diff.z);
	const float horizontal = std::sqrt(diff.x * diff.x + diff.z * diff.z);
	const float pitch = -std::atan2(diff.y, horizontal);

	camera_->SetTranslate(eye);
	camera_->SetRotate({ pitch, yaw, 0.0f });
	camera_->Update();
}

void GraphicsLabScene::DrawDebugWindow()
{
#ifdef USE_IMGUI
	ImGui::Begin("Graphics Lab");
	ImGui::Text("Realistic water look-dev scene");
	ImGui::Text("A,D: Orbit  W,S: Pitch  Mouse wheel: Zoom  Esc: Title");
	ImGui::Text("Shift+D: Debug camera  MMB: Orbit  Shift+MMB: Pan  Wheel: Zoom");
	ImGui::Checkbox("Pause water", &pauseWater_);
	ImGui::Checkbox("Show underwater sand", &showSandBed_);
	ImGui::Checkbox("Show beach", &showBeach_);
	ImGui::Checkbox("Show obstacles", &showObstacles_);
	ImGui::Checkbox("Show PBR samples", &showPbrSamples_);
	ImGui::Checkbox("PBR procedural environment", &usePbrProceduralEnvironment_);
	ImGui::Checkbox("PBR sample shadows", &enablePbrSampleShadows_);
	ImGui::DragFloat("Water speed", &waterTimeScale_, 0.02f, 0.0f, 4.0f);
	ImGui::DragFloat("Water light intensity", &waterLightIntensity_, 0.05f, 0.0f, 8.0f);
	ImGui::DragFloat("PBR direct light", &pbrDirectLightIntensity_, 0.05f, 0.0f, 8.0f);
	ImGui::DragFloat("PBR IBL diffuse", &pbrIblDiffuseIntensity_, 0.02f, 0.0f, 4.0f);
	ImGui::DragFloat("PBR IBL specular", &pbrIblSpecularIntensity_, 0.02f, 0.0f, 4.0f);
	ImGui::DragFloat("PBR IBL max mip", &pbrIblMaxMipLevel_, 0.1f, 0.0f, 12.0f);
	ImGui::DragFloat("PBR normal detail", &pbrNormalDetailStrength_, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("PBR normal detail scale", &pbrNormalDetailScale_, 0.5f, 1.0f, 96.0f);
	ImGui::ColorEdit4("River tint", &riverTint_.x);
	ImGui::DragFloat("Camera distance", &cameraDistance_, 0.5f, 36.0f, 180.0f);
	ImGui::Text("Beach and rocks provide shoreline context for the water.");
	ImGui::End();
#endif
}
