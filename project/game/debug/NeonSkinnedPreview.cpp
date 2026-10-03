#include "NeonSkinnedPreview.h"

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "StartupTrace.h"
#include "NeonPreviewAnimations.h"
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

void NeonSkinnedPreview::Initialize(cg2::Camera* camera, cg2::DebugCamera* debugCamera) {
	camera_ = camera;
	debugCamera_ = debugCamera;
	params_.internalLineEnabled = 1;
}

void NeonSkinnedPreview::Load() {
	if (ready_) return;
	try {
		cg2::StartupTrace::Scope scope("NeonSkinnedPreview.Load");
		const auto metadata = ReadGlbMetadata();
		sourceAnimationCount_ = metadata.contains("animations") ? metadata["animations"].size() : 0;
		sourceMaterials_.clear();
		for (const auto& mesh : metadata.at("meshes")) {
			for (const auto& primitive : mesh.at("primitives")) {
				const auto& material = metadata.at("materials").at(primitive.at("material").get<size_t>());
				const std::string alphaMode = material.value("alphaMode", "OPAQUE");
				sourceMaterials_.push_back({ mesh.value("name", "unnamed"),
					material.value("name", "unnamed"), alphaMode,
					alphaMode == "MASK" ? material.value("alphaCutoff", 0.5f) : alphaMode == "BLEND" ? 0.03f : 0.0f });
			}
		}
		auto* common = cg2::Object3dCommon::GetInstance();
		model_ = std::make_unique<cg2::SkinnedModel>();
		model_->Initialize(common->GetDxCommon(), common->GetSrvManager(), kPreviewModelPath);
		// ロード時に一度だけ生成し、モデルが所有する。失敗してもBindPoseでPreviewを継続する。
		generatedAnimationCount_ = 0;
		animationError_.clear();
		try {
			auto clips = neonpreview::CreateAnimations(model_->GetSkeleton());
			const auto count = clips.size();
			if (model_->RegisterAnimations(std::move(clips), &animationError_)) {
				generatedAnimationCount_ = count;
				neonpreview::SelectAnimation(*model_, neonpreview::Clip::Idle);
			}
		} catch (const std::exception& error) { animationError_ = error.what(); }
		submeshParams_.assign(model_->GetSubmeshCount(), cg2::NeonSkinnedSubmeshParams{});
		for (size_t index = 0; index < model_->GetSubmeshCount(); ++index) {
			for (const auto& source : sourceMaterials_) {
				if (source.materialName == model_->GetSubmesh(index).materialName) {
					submeshParams_[index].alphaCutoff = source.alphaCutoff;
					break;
				}
			}
		}
		object_ = std::make_unique<cg2::Object3d>();
		object_->Initialize();
		object_->SetCamera(camera_);
		object_->SetDebugCamera(debugCamera_);
		renderer_.Initialize(common->GetDxCommon(), common->GetSrvManager());
		renderer_.SetParams(params_);
		PlaceInFrontOfCamera();
		ready_ = true;
		loadError_.clear();
		cg2::StartupTrace::Count("neon_preview.model_loads");
		cg2::StartupTrace::Count("neon_preview.submeshes", static_cast<double>(model_->GetSubmeshCount()));
		cg2::StartupTrace::Count("neon_preview.joints", static_cast<double>(model_->GetSkeleton().joints.size()));
		cg2::StartupTrace::Count("neon_preview.weight_assignments", model_->GetSkinCluster().GetAssignedInfluenceCount());
	} catch (const std::exception& error) {
		enabled_ = false;
		loadError_ = error.what();
	}
}

cg2::Vector3 NeonSkinnedPreview::GetCameraPosition() const {
	return cg2::Object3dCommon::GetInstance()->GetIsDebugCamera()
		? debugCamera_->GetEyePosition() : camera_->GetTranslate();
}

void NeonSkinnedPreview::PlaceInFrontOfCamera() {
	const cg2::Matrix4x4 world = cg2::Object3dCommon::GetInstance()->GetIsDebugCamera()
		? cg2::Inverse(debugCamera_->GetViewMatrix()) : camera_->GetWorldMatrix();
	const cg2::Vector3 forward{ world.m[2][0], world.m[2][1], world.m[2][2] };
	const cg2::Vector3 up{ world.m[1][0], world.m[1][1], world.m[1][2] };
	// 足元原点の約1.6mのモデルを画面中央へ置く。ゲーム側のCameraには変更を加えない。
	transform_.translate = GetCameraPosition() + forward * 35.0f - up * (0.8f * transform_.scale.y);
}

void NeonSkinnedPreview::Update(float deltaTime) {
	if (!ready_) return;
	// DirectXCommon::PostDrawはFence完了後に次フレームへ進む。
	// Draw回数・表示モードに関係なく、このUpdateだけでCB領域をリセットする。
	renderer_.BeginFrame();
	if (!enabled_) return;
	neonpreview::UpdateAnimation(*model_, deltaTime); // Normal/Neonでこの一回のPalette更新を共有する。
	object_->SetTransform(transform_);
	object_->Update(); // World/WVP・Camera CBVは既存Object3dで一度だけ更新する。
	renderer_.SetParams(params_);
	auto surfaces = submeshParams_;
	if (!alphaCutoutEnabled_) for (auto& surface : surfaces) surface.alphaCutoff = 0.0f;
	renderer_.SetSubmeshParams(surfaces);
}

void NeonSkinnedPreview::Draw() {
	if (!enabled_ || !ready_) return;
	if (neonMode_) {
		renderer_.Draw(*model_, object_->GetTransformationResource()->GetGPUVirtualAddress(), GetCameraPosition());
		cg2::StartupTrace::Count("neon_preview.neon_draws");
	} else {
		object_->DrawSkinned(*model_);
		cg2::StartupTrace::Count("neon_preview.normal_draws");
	}
	// Custom Root Signatureの状態を後続の通常Rendererへ渡さない。
	cg2::Object3dCommon::GetInstance()->PreDraw(cg2::kNormal);
}

void NeonSkinnedPreview::DrawImGui() {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("Neon Skinned Preview", ImGuiTreeNodeFlags_DefaultOpen)) return;
	ImGui::TextUnformatted("AvatarSample_B / Developer only");
	if (ImGui::Checkbox("Preview Enable", &enabled_) && enabled_) Load();
	if (!loadError_.empty()) ImGui::TextWrapped("Load failed: %s", loadError_.c_str());
	int mode = neonMode_ ? 1 : 0;
	if (ImGui::RadioButton("Normal", mode == 0)) neonMode_ = false;
	ImGui::SameLine();
	if (ImGui::RadioButton("Neon", mode == 1)) neonMode_ = true;
	if (ready_) {
		ImGui::Text("GLB animations: %zu / Generated clips: %zu", sourceAnimationCount_, generatedAnimationCount_);
		if (!animationError_.empty()) ImGui::TextWrapped("Motion unavailable (BindPose retained): %s", animationError_.c_str());
		const auto& clip = model_->GetAnimation();
		int selection = clip.name == "Preview_Idle" ? 1 : clip.name == "Preview_Attack" ? 2 : 0;
		const char* clips[] = { "BindPose", "Preview_Idle", "Preview_Attack" };
		ImGui::BeginDisabled(generatedAnimationCount_ == 0);
		if (ImGui::Combo("Animation", &selection, clips, 3))
			neonpreview::SelectAnimation(*model_, static_cast<neonpreview::Clip>(selection));
		if (ImGui::Button("Play Attack")) neonpreview::SelectAnimation(*model_, neonpreview::Clip::Attack);
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button(model_->IsAnimationPaused() ? "Play" : "Pause"))
			model_->SetAnimationPlaying(model_->IsAnimationPaused());
		ImGui::SameLine();
		if (ImGui::Button("Restart")) model_->SetAnimation(model_->GetCurrentAnimationIndex(), true);
		float speed = model_->GetAnimationPlayer().GetPlaybackSpeed();
		if (ImGui::DragFloat("Playback Speed", &speed, 0.05f, 0.1f, 3.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp))
			model_->SetAnimationPlaybackSpeed(speed);
		const float duration = model_->GetCurrentAnimationDuration();
		float time = model_->GetCurrentAnimationTime();
		ImGui::TextWrapped("Current: %s / loop=%s / %.3f / %.3f s", model_->GetAnimation().name.c_str(),
			model_->IsCurrentAnimationLooping() ? "true" : "false", time, duration);
		ImGui::BeginDisabled(!model_->IsAnimationPaused() || duration <= 0.0f);
		if (ImGui::SliderFloat("Paused playback position", &time, 0.0f, duration > 0.0f ? duration : 1.0f, "%.3f s"))
			model_->SeekCurrentAnimation(time);
		ImGui::EndDisabled();
		ImGui::TextWrapped("Pause freezes playback and blend. Seek ends the blend and shows the exact sampled pose.");
	}
	ImGui::DragFloat3("Position", &transform_.translate.x, 0.1f);
	ImGui::DragFloat3("Rotation (radians)", &transform_.rotate.x, 0.01f);
	ImGui::DragFloat3("Scale", &transform_.scale.x, 0.05f, 0.01f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	if (ImGui::Button("Place in front of camera")) PlaceInFrontOfCamera();
	if (ImGui::Button("Reset: dark body + neon lines")) {
		params_ = cg2::NeonSkinnedParams{};
		params_.internalLineEnabled = 1;
	}
	ImGui::ColorEdit3("bodyColor", &params_.bodyColor.x);
	bool outlineEnabled = params_.outlineEnabled != 0;
	if (ImGui::Checkbox("Outline Enable", &outlineEnabled)) params_.outlineEnabled = outlineEnabled ? 1u : 0u;
	ImGui::DragFloat("Outline width (pixels)", &params_.outlineWidthPixels, 0.05f, 0.0f, 8.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::ColorEdit3("Neon line color", &params_.emissiveColor.x);
	ImGui::DragFloat("Neon line intensity (HDR)", &params_.emissiveIntensity, 0.05f, 0.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	bool internalEnabled = params_.internalLineEnabled != 0;
	if (ImGui::Checkbox("Internal Line Enable", &internalEnabled)) params_.internalLineEnabled = internalEnabled ? 1u : 0u;
	ImGui::DragFloat("Internal width (pixels)", &params_.internalLineWidthPixels, 0.05f, 0.0f, 4.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::DragFloat("Internal intensity (HDR)", &params_.internalLineIntensity, 0.05f, 0.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::DragFloat("Internal edge threshold", &params_.internalLineThreshold, 0.005f, 0.001f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::Checkbox("Texture alpha cutout", &alphaCutoutEnabled_);
	if (ImGui::TreeNode("Optional body rim")) {
		ImGui::DragFloat("rimStrength", &params_.rimStrength, 0.02f, 0.0f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat("rimPower", &params_.rimPower, 0.05f, 0.05f, 20.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::TreePop();
	}
	ImGui::TextWrapped("Neon supports alpha cutout. BLEND is approximated by cutout; MToon, morph targets and spring bones are not rendered.");
	if (!ready_) return;
	ImGui::TextWrapped("Internal lines follow the existing texture details of hair, eyes, mouth and clothes. Higher threshold removes faint shading edges.");
	ImGui::Text("Submeshes: %zu / Joints: %zu / Weight assignments: %u", model_->GetSubmeshCount(),
		model_->GetSkeleton().joints.size(), model_->GetSkinCluster().GetAssignedInfluenceCount());
	ImGui::Text("Source animations: %zu / Current: %s", sourceAnimationCount_, model_->GetAnimation().name.c_str());
	if (ImGui::TreeNode("Submesh / Material diagnostics")) {
		for (size_t i = 0; i < model_->GetSubmeshCount(); ++i) {
			ImGui::PushID(static_cast<int>(i));
			const auto& submesh = model_->GetSubmesh(i);
			const SourceMaterial* source = nullptr;
			for (const auto& candidate : sourceMaterials_) {
				if (candidate.materialName == submesh.materialName) { source = &candidate; break; }
			}
			ImGui::Text("[%zu] %s", i, source ? source->meshName.c_str() : "Unknown mesh");
			ImGui::TextWrapped("%s / alpha=%s / doubleSided=%s", submesh.materialName.c_str(),
				source ? source->alphaMode.c_str() : "Unknown", submesh.doubleSided ? "true" : "false");
			ImGui::DragFloat("Line strength", &submeshParams_[i].lineStrength, 0.02f, 0.0f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::DragFloat("Alpha cutoff", &submeshParams_[i].alphaCutoff, 0.01f, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::PopID();
		}
		ImGui::TreePop();
	}
#endif
}
#endif
