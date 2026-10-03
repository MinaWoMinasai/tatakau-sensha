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
		ApplyRecommendedLineArtPreset();
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

void NeonSkinnedPreview::ApplyRecommendedLineArtPreset() {
	params_.bodyColor = { 0.018f, 0.0025f, 0.012f, 1.0f };
	params_.bodyEmissionIntensity = 1.5f;
	params_.outlineEnabled = 1;
	params_.outlineWidthPixels = 1.15f;
	params_.emissiveIntensity = 6.0f;
	params_.emissiveColor = { 1.0f, 0.025f, 0.35f };
	params_.internalLineEnabled = 1;
	params_.internalLineWidthPixels = 0.7f;
	params_.internalLineIntensity = 5.0f;
	params_.internalLineThreshold = 0.16f;
	params_.rimStrength = 0.0f;
	params_.geometryLineEnabled = 0;
	geometryPreset_ = GeometryPreset::Off;
	if (!model_) return;
	// 同じAtlasでも顔の薄い描き込みと服・髪の陰影は異なるため、実Material名で調整する。
	struct MaterialSetting { const char* name; float strength; float thresholdScale; };
	constexpr MaterialSetting settings[] = {
		{ "N00_000_00_FaceMouth_00_FACE (Instance)", 1.4f, 0.8f },
		{ "N00_000_00_EyeIris_00_EYE (Instance)", 1.15f, 1.0f },
		{ "N00_000_00_EyeHighlight_00_EYE (Instance)", 0.45f, 1.0f },
		{ "N00_000_00_Face_00_SKIN (Instance)", 1.25f, 0.35f },
		{ "N00_000_Hair_00_HAIR_01 (Instance)", 0.75f, 1.15f },
		{ "N00_000_00_Body_00_SKIN (Instance)", 0.12f, 1.8f },
		{ "N00_005_01_Shoes_01_CLOTH (Instance)", 0.45f, 1.4f }
	};
	for (size_t index = 0; index < submeshParams_.size(); ++index) {
		auto& surface = submeshParams_[index];
		surface.lineStrength = 0.15f;
		surface.internalLineThresholdScale = 1.5f;
		surface.geometryLineStrength = 0.0f;
		for (const auto& setting : settings) {
			if (model_->GetSubmesh(index).materialName == setting.name) {
				surface.lineStrength = setting.strength;
				surface.internalLineThresholdScale = setting.thresholdScale;
				break;
			}
		}
	}
}

void NeonSkinnedPreview::ApplyLegacyNeonPreset() {
	params_ = cg2::NeonSkinnedParams{};
	params_.internalLineEnabled = 1;
	geometryPreset_ = GeometryPreset::Off;
	for (auto& surface : submeshParams_) {
		surface.lineStrength = 1.0f;
		surface.internalLineThresholdScale = 1.0f;
		surface.geometryLineStrength = 0.0f;
	}
}

void NeonSkinnedPreview::ApplyGeometryPreset(GeometryPreset preset) {
	if (!ready_) return;
	geometryPreset_ = preset;
	params_.geometryLineEnabled = preset == GeometryPreset::Off ? 0u : 1u;
	params_.geometryLineWidthPixels = 1.0f; // 共有辺の両側を合わせた全幅。
	params_.geometryLineIntensity = 4.0f;
	params_.geometryLineColor = { 0.08f, 0.65f, 1.0f };
	for (size_t index = 0; index < submeshParams_.size(); ++index) {
		auto& strength = submeshParams_[index].geometryLineStrength;
		strength = preset == GeometryPreset::FullMeshDiagnostic ? 1.0f : 0.0f;
		if (preset != GeometryPreset::Subtle) continue;
		// AvatarSample_BのGLB metadataと実ロード結果で確認したMaterial名。
		// 顔・目・髪は0のまま。モデル固有の選別はPreview内に留める。
		const auto& materialName = model_->GetSubmesh(index).materialName;
		if (materialName == "N00_000_00_Body_00_SKIN (Instance)") strength = 0.2f;
		else if (materialName == "N00_005_01_Shoes_01_CLOTH (Instance)") strength = 0.15f;
	}
}

void NeonSkinnedPreview::DrawGeometryLinesImGui() {
#ifdef USE_IMGUI
	if (!ImGui::TreeNode("Mesh Geometry Lines (diagnostic)")) return;
	if (ready_) ImGui::TextWrapped("Barycentrics: %s", renderer_.GetGeometryLinesStatus().c_str());
	else ImGui::TextWrapped("Barycentrics: enable Preview to check this device.");
	ImGui::BeginDisabled(!ready_ || !renderer_.IsGeometryLinesSupported());
	if (ImGui::RadioButton("OFF", geometryPreset_ == GeometryPreset::Off)) ApplyGeometryPreset(GeometryPreset::Off);
	ImGui::SameLine();
	if (ImGui::RadioButton("Subtle Geometry", geometryPreset_ == GeometryPreset::Subtle)) ApplyGeometryPreset(GeometryPreset::Subtle);
	if (ImGui::RadioButton("Full Mesh Diagnostic", geometryPreset_ == GeometryPreset::FullMeshDiagnostic))
		ApplyGeometryPreset(GeometryPreset::FullMeshDiagnostic);
	bool geometryEnabled = params_.geometryLineEnabled != 0;
	if (ImGui::Checkbox("Geometry Lines Enable", &geometryEnabled)) {
		params_.geometryLineEnabled = geometryEnabled ? 1u : 0u;
		geometryPreset_ = GeometryPreset::Custom;
	}
	if (ImGui::DragFloat("Geometry width (full pixels)", &params_.geometryLineWidthPixels,
		0.05f, 0.0f, 8.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) geometryPreset_ = GeometryPreset::Custom;
	if (ImGui::DragFloat("Geometry intensity (HDR)", &params_.geometryLineIntensity,
		0.05f, 0.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) geometryPreset_ = GeometryPreset::Custom;
	if (ImGui::ColorEdit3("Geometry line color", &params_.geometryLineColor.x)) geometryPreset_ = GeometryPreset::Custom;
	ImGui::EndDisabled();
	ImGui::TextWrapped("Start with Subtle Geometry or Full Mesh Diagnostic; initial Submesh Geometry Strength is zero.");
	ImGui::TextWrapped("Triangle mesh edges are independent of texture feature lines. Full Mesh Diagnostic includes face and hair triangles.");
	ImGui::TreePop();
#endif
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
	ImGui::BeginDisabled(!ready_);
	if (ImGui::Button("Recommended Line Art")) ApplyRecommendedLineArtPreset();
	if (ImGui::Button("Legacy Neon comparison")) ApplyLegacyNeonPreset();
	ImGui::EndDisabled();
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
	DrawGeometryLinesImGui();
	ImGui::DragFloat3("Position", &transform_.translate.x, 0.1f);
	ImGui::DragFloat3("Rotation (radians)", &transform_.rotate.x, 0.01f);
	ImGui::DragFloat3("Scale", &transform_.scale.x, 0.05f, 0.01f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	if (ImGui::Button("Place in front of camera")) PlaceInFrontOfCamera();
	if (ImGui::Button("Reset: dark body + neon lines")) ApplyLegacyNeonPreset();
	ImGui::ColorEdit3("Body tint", &params_.bodyColor.x);
	ImGui::DragFloat("Body emission", &params_.bodyEmissionIntensity, 0.02f, 0.0f, 4.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	bool outlineEnabled = params_.outlineEnabled != 0;
	if (ImGui::Checkbox("Outline Enable", &outlineEnabled)) params_.outlineEnabled = outlineEnabled ? 1u : 0u;
	ImGui::DragFloat("Outline width (pixels)", &params_.outlineWidthPixels, 0.05f, 0.0f, 8.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::ColorEdit3("Outline / feature line color", &params_.emissiveColor.x);
	ImGui::DragFloat("Outline intensity (HDR)", &params_.emissiveIntensity, 0.05f, 0.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
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
			ImGui::DragFloat("Internal threshold scale", &submeshParams_[i].internalLineThresholdScale,
				0.02f, 0.1f, 4.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::DragFloat("Alpha cutoff", &submeshParams_[i].alphaCutoff, 0.01f, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::BeginDisabled(!renderer_.IsGeometryLinesSupported());
			if (ImGui::DragFloat("Geometry strength", &submeshParams_[i].geometryLineStrength,
				0.02f, 0.0f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) geometryPreset_ = GeometryPreset::Custom;
			ImGui::EndDisabled();
			ImGui::PopID();
		}
		ImGui::TreePop();
	}
#endif
}
#endif
