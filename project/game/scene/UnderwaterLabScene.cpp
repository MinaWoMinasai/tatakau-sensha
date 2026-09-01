#include "UnderwaterLabScene.h"

#include "ModelManager.h"
#include "Object3dCommon.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {

constexpr char kFloorModelName[] = "__underwater_lab_floor";
constexpr char kBoxModelName[] = "__underwater_lab_box";
constexpr char kCausticsTexturePath[] = "resources/UnderwaterCaustics.png";

} // namespace

void UnderwaterLabScene::Initialize()
{
	input_ = Input::GetInstance();

	camera_ = std::make_unique<Camera>();
	camera_->SetNearClip(0.1f);
	camera_->SetFarClip(500.0f);
	// Keep the fixed camera high enough to show the floor across most of the
	// frame while retaining near-to-far box comparisons.
	camera_->SetTranslate({ 0.0f, 20.0f, -42.0f });
	camera_->SetRotate({ 0.17f, 0.0f, 0.0f });
	camera_->Update();

	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize();
	debugCamera_->GetDistance() = 80.0f;
	debugCamera_->SetNearClip(0.1f);
	debugCamera_->SetFarClip(500.0f);

	Object3dCommon::GetInstance()->SetDefaultCamera(camera_.get());
	Object3dCommon::GetInstance()->SetDebugDefaultCamera(debugCamera_.get());
	Object3dCommon::GetInstance()->SetIsDebugCamera(false);

	ModelManager::GetInstance()->CreatePlaneModel(kFloorModelName, 240.0f, 300.0f);
	ModelManager::GetInstance()->CreateBoxModel(kBoxModelName, { 1.0f, 1.0f, 1.0f });

	// A generated plane guarantees a completely flat floor without adding assets.
	floor_ = MakeObject(
		kFloorModelName,
		{ 0.0f, 0.0f, 70.0f },
		{ 1.0f, 1.0f, 1.0f },
		{ 0.72f, 0.67f, 0.56f, 1.0f },
		false);
	floor_->SetCausticsTexture(kCausticsTexturePath);
	ApplyCausticsSettings();

	// Use existing geometry as a scene-local blue-green background. This avoids
	// changing the engine clear color or leaking render state into other scenes.
	background_ = MakeObject(
		kBoxModelName,
		{ 0.0f, 70.0f, 190.0f },
		{ 360.0f, 180.0f, 2.0f },
		{ 0.018f, 0.17f, 0.23f, 1.0f },
		false);

	struct BoxPlacement {
		Vector3 translate;
		Vector3 scale;
		Vector4 color;
	};
	const BoxPlacement placements[] = {
		{ { -12.0f, 2.0f, 8.0f },   { 4.0f, 4.0f, 4.0f }, { 0.88f, 0.88f, 0.86f, 1.0f } },
		{ {  10.0f, 3.0f, 30.0f },  { 6.0f, 6.0f, 6.0f }, { 0.84f, 0.54f, 0.38f, 1.0f } },
		{ {  -8.0f, 4.0f, 58.0f },  { 8.0f, 8.0f, 8.0f }, { 0.76f, 0.78f, 0.78f, 1.0f } },
		{ {  14.0f, 5.0f, 92.0f },  { 10.0f, 10.0f, 10.0f }, { 0.86f, 0.77f, 0.42f, 1.0f } },
		{ {   0.0f, 6.0f, 132.0f }, { 12.0f, 12.0f, 12.0f }, { 0.68f, 0.71f, 0.72f, 1.0f } },
	};

	depthBoxes_.reserve(std::size(placements));
	for (const BoxPlacement& placement : placements) {
		depthBoxes_.push_back(MakeObject(
			kBoxModelName,
			placement.translate,
			placement.scale,
			placement.color,
			false));
	}

	floor_->Update();
	background_->Update();
	for (auto& box : depthBoxes_) {
		box->Update();
	}
}

void UnderwaterLabScene::Update()
{
	if (input_->IsTrigger(input_->GetKey()[DIK_ESCAPE], input_->GetPreKey()[DIK_ESCAPE])) {
		nextSceneName_ = "TITLE";
		finished_ = true;
		return;
	}

	camera_->Update();
	debugCamera_->Update(
		input_->GetMouseState(),
		input_->GetKey(),
		input_->GetLeftStick());

	floor_->Update();
	background_->Update();
	for (auto& box : depthBoxes_) {
		box->Update();
	}

	DrawDebugWindow();
	ApplyCausticsSettings();
}

void UnderwaterLabScene::DrawPostEffect3D()
{
	Object3dCommon::GetInstance()->PreDraw(kNone);
	background_->Draw();
	floor_->Draw();
	for (auto& box : depthBoxes_) {
		box->Draw();
	}
}

std::unique_ptr<Object3d> UnderwaterLabScene::MakeObject(
	const std::string& modelName,
	const Vector3& translate,
	const Vector3& scale,
	const Vector4& color,
	bool lighting)
{
	auto object = std::make_unique<Object3d>();
	object->Initialize();
	object->SetModel(modelName);
	object->SetTranslate(translate);
	object->SetScale(scale);
	object->SetColor(color);
	object->SetLighting(lighting);
	return object;
}

void UnderwaterLabScene::ApplyCausticsSettings()
{
	if (!floor_) {
		return;
	}

	floor_->SetCausticsSettings(
		enableCaustics_,
		causticsScale_,
		causticsIntensity_,
		causticsColor_);
}

void UnderwaterLabScene::DrawDebugWindow()
{
#ifdef USE_IMGUI
	ImGui::Begin("Underwater Lab");
	ImGui::TextUnformatted("Esc: Title");
	ImGui::TextUnformatted("Shift+D: Debug camera");
	ImGui::Separator();
	ImGui::TextUnformatted("Use the existing BloomAndVignette window to adjust:");
	ImGui::BulletText("Bloom / Distortion Amount");
	ImGui::BulletText("Depth Fog");
	ImGui::SeparatorText("Caustics");
	ImGui::Checkbox("Enable", &enableCaustics_);
	ImGui::DragFloat("Scale", &causticsScale_, 0.001f, 0.001f, 0.25f, "%.3f");
	ImGui::DragFloat("Intensity", &causticsIntensity_, 0.01f, 0.0f, 1.0f, "%.2f");
	ImGui::ColorEdit3("Color", &causticsColor_.x);
	ImGui::TextUnformatted("Projection: worldPosition.xz * Scale");
	ImGui::End();
#endif
}
