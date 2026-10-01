#include "NeonSkinnedPreview.h"

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "StartupTrace.h"
#include "externals/nlohmann/json.hpp"
#include <cstdint>
#include <fstream>
#include <stdexcept>

namespace {
constexpr const char* kPreviewModelPath = "resources/models/neon_hologram/AvatarSample_B.glb";

// 診断表示用のglTF metadataのみ読む。VRM表現・Materialを変換しない。
nlohmann::json ReadGlbMetadata() {
	std::ifstream stream(kPreviewModelPath, std::ios::binary);
	uint32_t header[5]{};
	if (!stream.read(reinterpret_cast<char*>(header), sizeof(header)) ||
		header[0] != 0x46546c67 || header[1] != 2 || header[4] != 0x4e4f534a ||
		header[3] > 8 * 1024 * 1024) {
		throw std::runtime_error("Preview model has no valid GLB JSON header.");
	}
	std::string json(header[3], '\0');
	if (!stream.read(json.data(), static_cast<std::streamsize>(json.size()))) {
		throw std::runtime_error("Preview model GLB JSON chunk is truncated.");
	}
	return nlohmann::json::parse(json);
}
}

void NeonSkinnedPreview::Initialize(Camera* camera, DebugCamera* debugCamera) {
	camera_ = camera;
	debugCamera_ = debugCamera;
}

void NeonSkinnedPreview::Load() {
	if (ready_) return;
	try {
		StartupTrace::Scope scope("NeonSkinnedPreview.Load");
		const auto metadata = ReadGlbMetadata();
		sourceAnimationCount_ = metadata.contains("animations") ? metadata["animations"].size() : 0;
		sourceMaterials_.clear();
		for (const auto& mesh : metadata.at("meshes")) {
			for (const auto& primitive : mesh.at("primitives")) {
				const auto& material = metadata.at("materials").at(primitive.at("material").get<size_t>());
				sourceMaterials_.push_back({ mesh.value("name", "unnamed"),
					material.value("name", "unnamed"), material.value("alphaMode", "OPAQUE") });
			}
		}
		auto* common = Object3dCommon::GetInstance();
		model_ = std::make_unique<SkinnedModel>();
		model_->Initialize(common->GetDxCommon(), common->GetSrvManager(), kPreviewModelPath);
		object_ = std::make_unique<Object3d>();
		object_->Initialize();
		object_->SetCamera(camera_);
		object_->SetDebugCamera(debugCamera_);
		renderer_.Initialize(common->GetDxCommon(), common->GetSrvManager());
		renderer_.SetParams(params_);
		PlaceInFrontOfCamera();
		ready_ = true;
		loadError_.clear();
		StartupTrace::Count("neon_preview.model_loads");
		StartupTrace::Count("neon_preview.submeshes", static_cast<double>(model_->GetSubmeshCount()));
		StartupTrace::Count("neon_preview.joints", static_cast<double>(model_->GetSkeleton().joints.size()));
		StartupTrace::Count("neon_preview.weight_assignments", model_->GetSkinCluster().GetAssignedInfluenceCount());
	} catch (const std::exception& error) {
		enabled_ = false;
		loadError_ = error.what();
	}
}

Vector3 NeonSkinnedPreview::GetCameraPosition() const {
	return Object3dCommon::GetInstance()->GetIsDebugCamera()
		? debugCamera_->GetEyePosition() : camera_->GetTranslate();
}

void NeonSkinnedPreview::PlaceInFrontOfCamera() {
	const Matrix4x4 world = Object3dCommon::GetInstance()->GetIsDebugCamera()
		? Inverse(debugCamera_->GetViewMatrix()) : camera_->GetWorldMatrix();
	const Vector3 forward{ world.m[2][0], world.m[2][1], world.m[2][2] };
	const Vector3 up{ world.m[1][0], world.m[1][1], world.m[1][2] };
	// 足元原点の約1.6mのモデルを画面中央へ置く。ゲーム側のCameraには変更を加えない。
	transform_.translate = GetCameraPosition() + forward * 25.0f - up * (0.8f * transform_.scale.y);
}

void NeonSkinnedPreview::Update(float deltaTime) {
	if (!ready_) return;
	// DirectXCommon::PostDrawはFence完了後に次フレームへ進む。
	// Draw回数・表示モードに関係なく、このUpdateだけでCB領域をリセットする。
	renderer_.BeginFrame();
	if (!enabled_) return;
	model_->Update(deltaTime); // Animationがなければ既存のBindPose fallbackを更新する。
	object_->SetTransform(transform_);
	object_->Update(); // World/WVP・Camera CBVは既存Object3dで一度だけ更新する。
	renderer_.SetParams(params_);
}

void NeonSkinnedPreview::Draw() {
	if (!enabled_ || !ready_) return;
	if (neonMode_) {
		renderer_.Draw(*model_, object_->GetTransformationResource()->GetGPUVirtualAddress(), GetCameraPosition());
		StartupTrace::Count("neon_preview.neon_draws");
	} else {
		object_->DrawSkinned(*model_);
		StartupTrace::Count("neon_preview.normal_draws");
	}
	// Custom Root Signatureの状態を後続の通常Rendererへ渡さない。
	Object3dCommon::GetInstance()->PreDraw(kNormal);
}

void NeonSkinnedPreview::DrawImGui() {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("Neon Skinned Preview", ImGuiTreeNodeFlags_DefaultOpen)) return;
	ImGui::TextUnformatted("AvatarSample_B / Developer only / BindPose");
	if (ImGui::Checkbox("Preview Enable", &enabled_) && enabled_) Load();
	if (!loadError_.empty()) ImGui::TextWrapped("Load failed: %s", loadError_.c_str());
	int mode = neonMode_ ? 1 : 0;
	if (ImGui::RadioButton("Normal", mode == 0)) neonMode_ = false;
	ImGui::SameLine();
	if (ImGui::RadioButton("Neon", mode == 1)) neonMode_ = true;
	ImGui::DragFloat3("Position", &transform_.translate.x, 0.1f);
	ImGui::DragFloat3("Rotation (radians)", &transform_.rotate.x, 0.01f);
	ImGui::DragFloat3("Scale", &transform_.scale.x, 0.05f, 0.01f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	if (ImGui::Button("Place in front of camera")) PlaceInFrontOfCamera();
	if (ImGui::Button("Reset: dark body + pink outline")) params_ = NeonSkinnedParams{};
	ImGui::ColorEdit3("bodyColor", &params_.bodyColor.x);
	bool outlineEnabled = params_.outlineEnabled != 0;
	if (ImGui::Checkbox("Outline Enable", &outlineEnabled)) params_.outlineEnabled = outlineEnabled ? 1u : 0u;
	ImGui::DragFloat("Outline width (pixels)", &params_.outlineWidthPixels, 0.05f, 0.0f, 8.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::ColorEdit3("Neon line color", &params_.emissiveColor.x);
	ImGui::DragFloat("Neon line intensity (HDR)", &params_.emissiveIntensity, 0.05f, 0.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	if (ImGui::TreeNode("Optional body rim")) {
		ImGui::DragFloat("rimStrength", &params_.rimStrength, 0.02f, 0.0f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat("rimPower", &params_.rimPower, 0.05f, 0.05f, 20.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::TreePop();
	}
	ImGui::TextWrapped("Neon is an opaque preview. VRM/MToon alpha, morph targets and spring bones are not rendered.");
	if (!ready_) return;
	ImGui::TextWrapped("Outline draws the exterior silhouette. Hair strands, eyes and mouth lines need a later feature-line pass.");
	ImGui::Text("Submeshes: %zu / Joints: %zu / Weight assignments: %u", model_->GetSubmeshCount(),
		model_->GetSkeleton().joints.size(), model_->GetSkinCluster().GetAssignedInfluenceCount());
	ImGui::Text("Source animations: %zu / Current: %s", sourceAnimationCount_, model_->GetAnimation().name.c_str());
	if (ImGui::TreeNode("Submesh / Material diagnostics")) {
		for (size_t i = 0; i < model_->GetSubmeshCount(); ++i) {
			const auto& submesh = model_->GetSubmesh(i);
			const SourceMaterial* source = nullptr;
			for (const auto& candidate : sourceMaterials_) {
				if (candidate.materialName == submesh.materialName) { source = &candidate; break; }
			}
			ImGui::Text("[%zu] %s", i, source ? source->meshName.c_str() : "Unknown mesh");
			ImGui::TextWrapped("%s / alpha=%s / doubleSided=%s", submesh.materialName.c_str(),
				source ? source->alphaMode.c_str() : "Unknown", submesh.doubleSided ? "true" : "false");
		}
		ImGui::TreePop();
	}
#endif
}
#endif
