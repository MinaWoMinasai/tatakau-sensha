#include "NeonSkinnedPreview.h"

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "StartupTrace.h"
#include "NeonPreviewAnimations.h"
#include "TextureManager.h"
#include "RuntimeProfiler.h"
#include "externals/nlohmann/json.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iterator>

namespace {
constexpr const char* kPreviewModelPath = "resources/models/neon_hologram/AvatarSample_B.glb";
constexpr const char* kFeatureMaskDirectory = "resources/models/neon_hologram/line_masks/";
constexpr const char* kFeatureMaskConfigPath = "resources/models/neon_hologram/line_masks/bindings.json";
struct QualityComparisonCase { const char* label; int candidate; uint32_t split; };
constexpr QualityComparisonCase kQualityComparisonCases[] = {
	{"original_auto",0,0}, {"original_authored",1,0}, {"v1_coverage",2,0},
	{"v1_coverage_core_halo",2,1}, {"v1_sdf_core_halo",3,1},
	{"v2_coverage_core_halo",4,1}, {"v2_sdf_core_halo",5,1}
};
struct DissolveComparisonCase { const char* label; float progress; bool enabled, noise, edge; };
constexpr DissolveComparisonCase kDissolveComparisonCases[] = {
	{"disabled",0,false,true,true}, {"progress_0",0,true,true,true},
	{"progress_025",0.25f,true,true,true}, {"progress_050",0.5f,true,true,true},
	{"progress_075",0.75f,true,true,true}, {"progress_1",1,true,true,true},
	{"no_noise_050",0.5f,true,false,true}, {"no_edge_050",0.5f,true,true,false}
};

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

NeonSkinnedPreview::~NeonSkinnedPreview() {
	// Scene teardown owns the model until this destructor completes. No GPU state is mutated here.
	if (model_) dissolve_.Reset(*model_);
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
		featureMaskBindings_.assign(model_->GetSubmeshCount(), FeatureMaskBinding{});
		featureMaskIndices_.assign(model_->GetSubmeshCount(), std::nullopt);
		featureDistanceIndices_.assign(model_->GetSubmeshCount(), std::nullopt);
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
	if (showcaseActive_) return showcaseCamera_.GetTranslate();
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
	showcaseCapture_.Resolve(*cg2::Object3dCommon::GetInstance()->GetDxCommon());
	AdvanceShowcaseComparison();
	AdvanceDissolveComparison();
	// DirectXCommon::PostDrawはFence完了後に次フレームへ進む。
	// Draw回数・表示モードに関係なく、このUpdateだけでCB領域をリセットする。
	renderer_.BeginFrame();
	if (!enabled_) { ResetDissolve(); return; }
	UpdateDissolveSequence(false);
	// Recording continues with F12 hidden. The clip trigger belongs to Update, not the UI.
	if (showcaseSequence_ && showcaseSequenceFrame_ >= 75 && !showcaseSequenceAttackStarted_) {
		neonpreview::SelectAnimation(*model_,neonpreview::Clip::Attack);
		showcaseSequenceAttackStarted_ = true;
	}
	const float animationDelta = restorePoseThisFrame_ ? 0.0f : deltaTime;
	restorePoseThisFrame_ = false;
	neonpreview::UpdateAnimation(*model_, animationDelta); // Normal/Neonでこの一回のPalette更新を共有する。
	UpdateDissolveSequence(true);
	dissolve_.Update(deltaTime);
	params_.dissolve = dissolveComparisonActive_ ? dissolveComparisonParams_ : dissolve_.GetParams();
	if (showcaseActive_) UpdateShowcaseCamera(deltaTime);
	object_->SetTransform(showcaseActive_ ? showcaseTransform_ : transform_);
	// Object3dの既存World/WVP経路を再利用する。グローバルdebug-camera設定は保存復帰する。
	auto* common = cg2::Object3dCommon::GetInstance();
	const bool debug = common->GetIsDebugCamera();
	if (showcaseActive_) { common->SetIsDebugCamera(false); object_->SetCamera(&showcaseCamera_); }
	object_->Update();
	if (showcaseActive_) { object_->SetCamera(camera_); common->SetIsDebugCamera(debug); }
	renderer_.SetParams(params_);
	auto surfaces = submeshParams_;
	if (!alphaCutoutEnabled_) for (auto& surface : surfaces) surface.alphaCutoff = 0.0f;
	renderer_.SetSubmeshParams(surfaces);
	renderer_.SetSubmeshFeatureMasks(featureMaskIndices_);
	renderer_.SetSubmeshFeatureDistanceMasks(featureDistanceIndices_);
}

void NeonSkinnedPreview::Draw() {
	if (!enabled_ || !ready_) return;
	cg2::RuntimeProfiler::GpuScope gpuScope("Neon Character");
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
	const auto maskColor = params_.featureMaskColor;
	const float maskIntensity = params_.featureMaskIntensity;
	const float maskBlend = params_.featureMaskBlend;
	const uint32_t maskDebugMode = params_.featureMaskDebugMode;
	params_ = cg2::NeonSkinnedParams{};
	params_.featureMaskColor = maskColor;
	params_.featureMaskIntensity = maskIntensity;
	params_.featureMaskBlend = maskBlend;
	params_.featureMaskDebugMode = maskDebugMode;
	params_.internalLineEnabled = 1;
	geometryPreset_ = GeometryPreset::Off;
	for (auto& surface : submeshParams_) {
		surface.lineStrength = 1.0f;
		surface.internalLineThresholdScale = 1.0f;
		surface.geometryLineStrength = 0.0f;
	}
}

void NeonSkinnedPreview::LoadFeatureMaskCandidates() {
	if (!ready_ || featureMasksLoadAttempted_) return;
	featureMaskBindings_.assign(model_->GetSubmeshCount(), FeatureMaskBinding{});
	featureMaskIndices_.assign(model_->GetSubmeshCount(), std::nullopt);
	featureDistanceIndices_.assign(model_->GetSubmeshCount(), std::nullopt);
	params_.featureMaskRenderMode = 0;
	qualityCandidate_ = 1;
	featureMasksLoadAttempted_ = true;
	featureMaskError_.clear();
	try {
		std::ifstream configStream(kFeatureMaskConfigPath);
		if (!configStream) throw std::runtime_error("Cannot open bindings.json; automatic lines retained.");
		const auto config = nlohmann::json::parse(configStream);
		if (config.at("schemaVersion").get<int>() != 1 || !config.at("bindings").is_array())
			throw std::runtime_error("Unsupported feature mask binding configuration.");
		for (const auto& binding : config.at("bindings")) {
			const auto materialName = binding.at("material").get<std::string>();
			const auto fileName = binding.at("file").get<std::string>();
			// この試作用設定ではUV0と同一ディレクトリのPNGだけを受け付ける。
			if (binding.at("uvSet").get<int>() != 0 || fileName.empty() ||
				std::filesystem::path(fileName).filename().string() != fileName ||
				std::filesystem::path(fileName).extension() != ".png")
				throw std::runtime_error("Feature masks require UV0 and a local PNG filename.");
			bool found = false;
			for (size_t index = 0; index < model_->GetSubmeshCount(); ++index) {
				if (model_->GetSubmesh(index).materialName != materialName) continue;
				found = true;
				auto& target = featureMaskBindings_[index];
				if (!target.path.empty()) throw std::runtime_error("Duplicate feature mask material binding: " + materialName);
				target.path = std::string(kFeatureMaskDirectory) + fileName;
				try {
					std::ifstream stream(target.path, std::ios::binary | std::ios::ate);
					if (!stream) throw std::runtime_error("PNG is missing; automatic lines retained.");
					const auto size = stream.tellg();
					if (size <= 0 || size > 16 * 1024 * 1024) throw std::runtime_error("PNG size is invalid.");
					std::vector<uint8_t> bytes(static_cast<size_t>(size));
					stream.seekg(0);
					if (!stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
						throw std::runtime_error("PNG read failed; automatic lines retained.");
					auto* textures = cg2::TextureManager::GetInstance();
					constexpr auto colorSpace = cg2::TextureManager::TextureColorSpace::LinearData;
					if (!textures->LoadTextureFromMemory(target.path, bytes.data(), bytes.size(), colorSpace))
						throw std::runtime_error("PNG decode/upload failed; automatic lines retained.");
					const auto& metadata = textures->GetMetaData(target.path, colorSpace);
					// WICはRGBA PNGをBGRA/BGRXの格納形式で返す場合がある。UNORM SRVの
					// 論理R/Gは同じで、sRGB形式は受け付けない。
					const bool linearRgb8 = metadata.format == DXGI_FORMAT_R8G8B8A8_UNORM ||
						metadata.format == DXGI_FORMAT_B8G8R8A8_UNORM || metadata.format == DXGI_FORMAT_B8G8R8X8_UNORM;
					if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || metadata.arraySize != 1 ||
						!linearRgb8)
						throw std::runtime_error("Mask must be a linear RGB(A)8 2D PNG (DXGI format " +
							std::to_string(metadata.format) + "); automatic lines retained.");
					target.srvIndex = textures->GetTextureIndexByFilePath(target.path, colorSpace);
					featureMaskIndices_[index] = target.srvIndex;
					target.status = "Loaded: LinearData UNORM (DXGI " + std::to_string(metadata.format) + ") / UV0 / " + std::to_string(metadata.width) + "x" +
						std::to_string(metadata.height) + " / " + std::to_string(metadata.mipLevels) + " mips";
				} catch (const std::exception& error) {
					target.srvIndex.reset();
					featureMaskIndices_[index].reset();
					target.status = error.what();
				}
			}
			if (!found) throw std::runtime_error("Mask target material is missing: " + materialName);
		}
	} catch (const std::exception& error) { featureMaskError_ = error.what(); }
	// 成功したMaterialだけに適用する。TextureManagerがGPU資源を保持し、再読込・破棄しない。
	bool hasMask = false;
	for (const auto& index : featureMaskIndices_) hasMask |= index.has_value();
	params_.featureMaskBlend = hasMask ? 1.0f : 0.0f;
	params_.featureMaskDebugMode = 0;
	originalMaskBindings_ = featureMaskBindings_;
	originalMaskIndices_ = featureMaskIndices_;
}

void NeonSkinnedPreview::DrawFeatureMaskImGui() {
#ifdef USE_IMGUI
	if (!ImGui::TreeNodeEx("Authored Feature Mask (candidate)", ImGuiTreeNodeFlags_DefaultOpen)) return;
	ImGui::BeginDisabled(!ready_ || featureMasksLoadAttempted_);
	if (ImGui::Button("Load / apply mask candidate")) LoadFeatureMaskCandidates();
	ImGui::EndDisabled();
	if (!featureMaskError_.empty()) ImGui::TextWrapped("Mask configuration: %s", featureMaskError_.c_str());
	if (!featureMasksLoadAttempted_) ImGui::TextWrapped("Candidate masks are not loaded; initial automatic lines are unchanged.");
	bool hasMask = false;
	for (const auto& index : featureMaskIndices_) hasMask |= index.has_value();
	ImGui::BeginDisabled(!hasMask);
	if (ImGui::Button("Auto lines only")) { params_.featureMaskBlend = 0.0f; params_.featureMaskDebugMode = 0; }
	ImGui::SameLine();
	if (ImGui::Button("Apply candidate")) { params_.featureMaskBlend = 1.0f; params_.featureMaskDebugMode = 0; }
	ImGui::SliderFloat("Mask application", &params_.featureMaskBlend, 0.0f, 1.0f, "%.2f");
	ImGui::ColorEdit3("Authored line color", &params_.featureMaskColor.x);
	ImGui::DragFloat("Authored line intensity (HDR)", &params_.featureMaskIntensity,
		0.05f, 0.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	int diagnostic = static_cast<int>(params_.featureMaskDebugMode);
	const char* modes[] = { "Shaded", "R: line coverage", "G: replacement region" };
	ImGui::BeginDisabled(params_.featureMaskBlend <= 0.0f);
	if (ImGui::Combo("Mask display", &diagnostic, modes, 3)) params_.featureMaskDebugMode = static_cast<uint32_t>(diagnostic);
	ImGui::EndDisabled();
	ImGui::EndDisabled();
	ImGui::TextWrapped("R specifies lines; G replaces automatic lines. G=1/R=0 removes only internal lines. BaseColor alpha cutout remains independent.");
	ImGui::TextWrapped("Diagnostics show unassigned surfaces as black. Application 0 restores automatic lines. Texture changes require an application restart.");
	if (featureMasksLoadAttempted_) {
		size_t loaded = 0, failed = 0;
		for (const auto& binding : featureMaskBindings_) {
			if (binding.path.empty()) continue;
			if (binding.srvIndex) ++loaded;
			else ++failed;
		}
		ImGui::Text("Material bindings: %zu loaded / %zu failed", loaded, failed);
		if (failed) ImGui::TextWrapped("Failed targets keep automatic lines; see load status below.");
		if (ImGui::TreeNode("Mask target Materials / load status")) {
			for (size_t index = 0; index < featureMaskBindings_.size(); ++index) {
				const auto& binding = featureMaskBindings_[index];
				if (binding.path.empty()) continue;
				ImGui::TextWrapped("%s", model_->GetSubmesh(index).materialName.c_str());
				ImGui::TextWrapped("%s / %s", binding.path.c_str(), binding.status.c_str());
			}
			ImGui::TreePop();
		}
		ImGui::TextWrapped("Some hair UVs are shared. The selected front-hair region has no front/back overlap in this model; broader edits may affect other strands.");
	}
	ImGui::TreePop();
#endif
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

void NeonSkinnedPreview::SelectQualityCandidate(int candidate) {
	if (!ready_ || candidate < 0 || candidate > 7) return;
	qualityError_.clear();
	if (candidate == 0) {
		params_.featureMaskBlend = 0; params_.featureMaskRenderMode = 0;
		params_.featureMaskDebugMode = 0; qualityCandidate_ = candidate; return;
	}
	if (candidate == 1) {
		if (!featureMasksLoadAttempted_) LoadFeatureMaskCandidates();
		featureMaskBindings_ = originalMaskBindings_; featureMaskIndices_ = originalMaskIndices_;
		featureDistanceIndices_.assign(model_->GetSubmeshCount(), std::nullopt);
		params_.featureMaskRenderMode = 0; params_.featureMaskBlend = 1; params_.featureMaskDebugMode = 0;
		qualityCandidate_ = candidate; return;
	}
	try {
		constexpr const char* directory = "resources/models/neon_hologram/line_masks/quality/";
		std::ifstream stream(std::string(directory) + "bindings.json");
		if (!stream) throw std::runtime_error("Quality candidate bindings.json missing.");
		const auto configuration = nlohmann::json::parse(stream);
		if (configuration.at("schemaVersion").get<int>() != 1 || configuration.at("uvSet").get<int>() != 0)
			throw std::runtime_error("Quality candidate requires schema 1 / UV0.");
		const std::string version = candidate < 4 ? "v1" : candidate < 6 ? "v2" : "v3";
		const nlohmann::json* selected = nullptr;
		for (const auto& value : configuration.at("versions")) if (value.at("id") == version) selected = &value;
		if (!selected) throw std::runtime_error("Candidate version is missing.");
		auto bindings = std::vector<FeatureMaskBinding>(model_->GetSubmeshCount());
		auto masks = std::vector<std::optional<uint32_t>>(model_->GetSubmeshCount());
		auto distances = masks;
		auto loadTexture = [this, selected](const std::string& path, const std::string& imageSha256) {
			if (const auto cached = qualityTextureProvenance_.find(path); cached != qualityTextureProvenance_.end())
				return cached->second;
			std::ifstream input(path,std::ios::binary|std::ios::ate);
			if (!input || input.tellg() <= 0 || input.tellg() > 16*1024*1024) throw std::runtime_error("Candidate PNG missing/invalid: " + path);
			std::vector<uint8_t> bytes(static_cast<size_t>(input.tellg())); input.seekg(0);
			if (!input.read(reinterpret_cast<char*>(bytes.data()),bytes.size())) throw std::runtime_error("Candidate PNG read failed.");
			auto* textures = cg2::TextureManager::GetInstance();
			constexpr auto linear = cg2::TextureManager::TextureColorSpace::LinearData;
			if (!textures->LoadTextureFromMemory(path,bytes.data(),bytes.size(),linear)) throw std::runtime_error("LinearData PNG load failed: " + path);
			const auto& metadata = textures->GetMetaData(path,linear);
			if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || metadata.arraySize != 1 ||
				(metadata.format != DXGI_FORMAT_R8G8B8A8_UNORM && metadata.format != DXGI_FORMAT_B8G8R8A8_UNORM && metadata.format != DXGI_FORMAT_B8G8R8X8_UNORM))
				throw std::runtime_error("Candidate must be linear RGB(A)8 2D PNG.");
			QualityTextureProvenance provenance{textures->GetTextureIndexByFilePath(path,linear),imageSha256,
				selected->value("authoringSha256",std::string{}),selected->value("authoringVersionSha256",std::string{}),
				selected->value("authoringRevision",std::string{})};
			qualityTextureProvenance_.emplace(path,provenance);
			return provenance;
		};
		for (const auto& binding : selected->at("bindings")) {
			if (binding.at("uvSet").get<int>() != 0) throw std::runtime_error("Only UV0 candidate masks are supported.");
			const auto material = binding.at("material").get<std::string>();
			const auto coverage = binding.at("coverageFile").get<std::string>();
			const auto sdf = binding.at("sdfFile").get<std::string>();
			for (const auto& file : {coverage,sdf}) if (std::filesystem::path(file).filename().string() != file ||
				std::filesystem::path(file).extension() != ".png") throw std::runtime_error("Candidate requires local PNG filenames.");
			bool found = false;
			for (size_t index=0; index<model_->GetSubmeshCount(); ++index) {
				if (model_->GetSubmesh(index).materialName != material) continue;
				found = true;
				if (masks[index]) throw std::runtime_error("Duplicate candidate Material.");
				bindings[index].path = std::string(directory)+coverage;
				bindings[index].distancePath = std::string(directory)+sdf;
				try {
					const auto coverageProvenance = loadTexture(bindings[index].path,binding.value("coverageSha256",std::string{}));
					masks[index] = coverageProvenance.srvIndex;
					bindings[index].coverageSha256 = coverageProvenance.imageSha256;
					bindings[index].authoringSha256 = coverageProvenance.authoringSha256;
					bindings[index].authoringVersionSha256 = coverageProvenance.authoringVersionSha256;
					bindings[index].authoringRevision = coverageProvenance.authoringRevision;
					bindings[index].srvIndex = masks[index];
					bindings[index].status = "Loaded LinearData coverage / UV0";
					try {
						const auto distanceProvenance = loadTexture(bindings[index].distancePath,binding.value("sdfSha256",std::string{}));
						distances[index] = distanceProvenance.srvIndex;
						bindings[index].distanceSha256 = distanceProvenance.imageSha256;
						bindings[index].status += " + SDF";
					}
					catch (const std::exception& error) { bindings[index].status += std::string(" / coverage fallback: ")+error.what(); }
				} catch (const std::exception& error) { bindings[index].status = std::string(error.what()) + "; automatic lines retained."; }
			}
			if (!found) throw std::runtime_error("Candidate Material missing: " + material);
		}
		featureMaskBindings_ = std::move(bindings); featureMaskIndices_ = std::move(masks); featureDistanceIndices_ = std::move(distances);
		params_.featureMaskRenderMode = candidate % 2 == 0 ? 1u : 2u;
		params_.featureMaskBlend = 1; params_.featureMaskDebugMode = 0;
		params_.sdfRangeTexels = configuration.at("sdfRangeTexels").get<float>();
		params_.sdfHaloWidthTexels = configuration.at("haloWidthTexels").get<float>();
		qualityCandidate_ = candidate;
	} catch (const std::exception& error) { qualityError_ = error.what(); }
}

void NeonSkinnedPreview::DrawQualityCandidateImGui() {
#ifdef USE_IMGUI
	if (!ImGui::TreeNodeEx("Line Art Quality Comparison",ImGuiTreeNodeFlags_DefaultOpen)) return;
	ImGui::BeginDisabled(!ready_);
	const char* candidates[] = {"Original automatic lines", "Original authored coverage", "V1: fair-width coverage", "V1: SDF + minification fallback", "V2: edited coverage", "V2: edited SDF + fallback", "V3: local hair revision coverage", "V3: local hair revision SDF"};
	int candidate = qualityCandidate_;
	if (ImGui::Combo("Line data / reconstruction",&candidate,candidates,8)) SelectQualityCandidate(candidate);
	if (ImGui::Button("Use V2 coverage + Core/Halo (explicit)")) {
		SelectQualityCandidate(4); if (qualityCandidate_ == 4 && qualityError_.empty()) params_.splitLineEmission = 1;
	}
	if (ImGui::Button("Try V3 local hair revision + Core/Halo")) {
		SelectQualityCandidate(6); if (qualityCandidate_ == 6 && qualityError_.empty()) params_.splitLineEmission = 1;
	}
	int maskDiagnostic = static_cast<int>(params_.featureMaskDebugMode);
	const char* maskChannels[] = {"Shaded", "R: line coverage", "G: replacement region"};
	if (ImGui::Combo("Coverage mask channels (debug)",&maskDiagnostic,maskChannels,3))
		params_.featureMaskDebugMode = static_cast<uint32_t>(maskDiagnostic);
	bool split = params_.splitLineEmission != 0;
	if (ImGui::Checkbox("Separate Core / Halo emission",&split)) params_.splitLineEmission = split ? 1u : 0u;
	const char* contributions[] = {"Combined", "Core only", "Surface Halo only", "Dark Body only"};
	int contribution = static_cast<int>(params_.lineDiagnosticMode);
	if (ImGui::Combo("Line contribution",&contribution,contributions,4)) params_.lineDiagnosticMode = contribution;
	ImGui::ColorEdit3("Core color",&params_.lineCoreColor.x);
	ImGui::SliderFloat("Core HDR intensity",&params_.lineCoreIntensity,0,12);
	ImGui::ColorEdit3("Surface Halo color",&params_.lineHaloColor.x);
	ImGui::SliderFloat("Surface Halo intensity",&params_.lineHaloIntensity,0,8);
	ImGui::ColorEdit3("Outline Core color",&params_.outlineCoreColor.x);
	ImGui::SliderFloat("Outline Core intensity",&params_.outlineCoreIntensity,0,16);
	ImGui::SliderFloat("SDF Halo width (texture texels)",&params_.sdfHaloWidthTexels,0,4);
	ImGui::TextWrapped("Data selection preserves pose, camera, transform, Body, outline and Bloom. Core/Halo is an independent switch. Original auto + Core/Halo OFF restores the original path.");
	ImGui::TextWrapped("Surface Halo does not spread beyond the silhouette; existing screen-space Bloom does. SDF switches toward coverage for minification.");
	if (!qualityError_.empty()) ImGui::TextWrapped("Candidate failed: %s",qualityError_.c_str());
	ImGui::EndDisabled(); ImGui::TreePop();
#endif
}

void NeonSkinnedPreview::EnterShowcase() {
	if (showcaseActive_) return;
	Load();
	if (!ready_) return;
	ResetDissolve(); // A new Showcase World cannot inherit a plane computed for the normal Preview World.
	if (dissolve_.IsActive()) return; // Protect the snapshot if an unexpected playback-restore failure occurred.
	checkpoint_ = { enabled_, neonMode_, alphaCutoutEnabled_, geometryPreset_, transform_, params_,
		submeshParams_, featureMaskIndices_, featureDistanceIndices_, featureMaskBindings_, qualityCandidate_, featureMasksLoadAttempted_,
		featureMaskError_, qualityError_, cg2::Object3dCommon::GetInstance()->GetIsDebugCamera(),
		camera_->GetTranslate(), camera_->GetRotate(), model_->CaptureAnimationPlaybackState() };
	showcaseActive_ = true; enabled_ = true;
	showcaseTransform_ = {{1,1,1},{},{}};
	showcaseFraming_ = showcaseView_ = showcaseDiagnostic_ = 0;
	showcaseYaw_ = 0; showcaseOrbit_ = false;
	showcaseCaptureDirectory_ = "generated/neon_directional_dissolve/showcase_" +
		std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count());
	showcaseCaptureNumber_ = 0;
	UpdateShowcaseCamera(0);
	cg2::StartupTrace::Count("neon_showcase.entries");
}

void NeonSkinnedPreview::LeaveShowcase() {
	if (!showcaseActive_) return;
	if (dissolveComparisonActive_) FinishDissolveComparison("Dissolve comparison cancelled on exit.");
	if (dissolveSequence_) FinishDissolveSequence();
	ResetDissolve();
	if (showcaseComparisonActive_) FinishShowcaseComparison("Comparison cancelled on Showcase exit.");
	showcaseSequence_ = false;
	showcaseCapture_.CancelRequest();
	enabled_ = checkpoint_.enabled; neonMode_ = checkpoint_.neonMode;
	alphaCutoutEnabled_ = checkpoint_.alphaCutout; geometryPreset_ = checkpoint_.geometryPreset;
	transform_ = checkpoint_.transform; params_ = checkpoint_.params;
	submeshParams_ = checkpoint_.surfaces; featureMaskIndices_ = checkpoint_.masks;
	featureDistanceIndices_ = checkpoint_.distances; featureMaskBindings_ = checkpoint_.bindings; qualityCandidate_ = checkpoint_.qualityCandidate;
	featureMasksLoadAttempted_ = checkpoint_.masksLoadAttempted; featureMaskError_ = checkpoint_.maskError; qualityError_ = checkpoint_.qualityError;
	if (!model_->RestoreAnimationPlaybackState(checkpoint_.playback))
		animationError_ = "Showcase checkpoint restore failed; playback was retained.";
	cg2::Object3dCommon::GetInstance()->SetIsDebugCamera(checkpoint_.usingDebugCamera);
	camera_->SetTranslate(checkpoint_.gameCameraPosition); camera_->SetRotate(checkpoint_.gameCameraRotation); camera_->Update();
	showcaseActive_ = false;
	cg2::StartupTrace::Count("neon_showcase.exits");
}

void NeonSkinnedPreview::StartShowcaseComparison() {
	if (!ready_ || !showcaseActive_ || showcaseComparisonActive_ || !model_->IsAnimationPaused() ||
		showcaseOrbit_ || showcaseSequence_ || dissolveSequence_ || dissolveComparisonActive_ || dissolve_.IsActive() ||
		showcaseCapture_.IsBusy() || cg2::RuntimeProfiler::Get().IsCaptureActive()) return;
	comparisonCheckpoint_ = {enabled_,neonMode_,alphaCutoutEnabled_,geometryPreset_,transform_,params_,
		submeshParams_,featureMaskIndices_,featureDistanceIndices_,featureMaskBindings_,qualityCandidate_,featureMasksLoadAttempted_,
		featureMaskError_,qualityError_,false,{},{},model_->CaptureAnimationPlaybackState()};
	showcaseComparisonDirectory_ = showcaseCaptureDirectory_ + "/comparison_" +
		std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count());
	showcaseComparisonIndex_ = 0; showcaseComparisonFrameRecorded_ = false; showcaseComparisonActive_ = true;
	showcaseComparisonStatus_ = "Capturing seven candidates at the same paused pose.";
	neonMode_ = true;
	ApplyShowcaseComparisonCase();
}

void NeonSkinnedPreview::ApplyShowcaseComparisonCase() {
	params_ = comparisonCheckpoint_.params;
	const auto& comparison = kQualityComparisonCases[showcaseComparisonIndex_];
	SelectQualityCandidate(comparison.candidate);
	if (!qualityError_.empty() || qualityCandidate_ != comparison.candidate) {
		FinishShowcaseComparison("Comparison stopped: " + (qualityError_.empty() ? "Candidate selection failed." : qualityError_)); return;
	}
	if (comparison.candidate > 0) {
		bool hasMask = false;
		for (size_t index=0; index<featureMaskBindings_.size(); ++index) {
			if (featureMaskBindings_[index].path.empty()) continue;
			hasMask = true;
			if (!featureMaskIndices_[index] || (params_.featureMaskRenderMode == 2 && !featureDistanceIndices_[index])) {
				FinishShowcaseComparison("Comparison stopped: required coverage/SDF is unavailable: " + featureMaskBindings_[index].status); return;
			}
		}
		if (!hasMask || (comparison.candidate == 1 && !featureMaskError_.empty())) {
			FinishShowcaseComparison("Comparison stopped: authored masks are unavailable. " + featureMaskError_); return;
		}
	}
	params_.splitLineEmission = comparison.split;
	showcaseComparisonFrameRecorded_ = false;
}

void NeonSkinnedPreview::AdvanceShowcaseComparison() {
	if (!showcaseComparisonActive_ || !showcaseComparisonFrameRecorded_ || showcaseCapture_.IsBusy()) return;
	if (!showcaseCapture_.WasLastCaptureSuccessful()) {
		FinishShowcaseComparison("Comparison stopped: " + showcaseCapture_.GetStatus()); return;
	}
	if (++showcaseComparisonIndex_ == std::size(kQualityComparisonCases)) {
		FinishShowcaseComparison("Saved seven same-pose PNGs and settings: " + showcaseComparisonDirectory_); return;
	}
	ApplyShowcaseComparisonCase();
}

void NeonSkinnedPreview::FinishShowcaseComparison(const std::string& status) {
	params_ = comparisonCheckpoint_.params; neonMode_ = comparisonCheckpoint_.neonMode;
	submeshParams_ = comparisonCheckpoint_.surfaces; featureMaskIndices_ = comparisonCheckpoint_.masks;
	featureDistanceIndices_ = comparisonCheckpoint_.distances; featureMaskBindings_ = comparisonCheckpoint_.bindings;
	qualityCandidate_ = comparisonCheckpoint_.qualityCandidate; featureMasksLoadAttempted_ = comparisonCheckpoint_.masksLoadAttempted;
	featureMaskError_ = comparisonCheckpoint_.maskError; qualityError_ = comparisonCheckpoint_.qualityError;
	showcaseComparisonActive_ = false; showcaseComparisonStatus_ = status;
}

bool NeonSkinnedPreview::TriggerDissolve() {
	if (!ready_ || !enabled_ || !neonMode_) return false;
	const auto& transform = showcaseActive_ ? showcaseTransform_ : transform_;
	const auto world = cg2::MakeAffineMatrix(transform.scale,transform.rotate,transform.translate);
	const auto cameraWorld = showcaseActive_ ? showcaseCamera_.GetWorldMatrix() :
		cg2::Object3dCommon::GetInstance()->GetIsDebugCamera() ? cg2::Inverse(debugCamera_->GetViewMatrix()) : camera_->GetWorldMatrix();
	const bool success = dissolve_.Trigger(*model_,world,cameraWorld,
		static_cast<cg2::NeonDissolveDirection>(dissolveDirection_),dissolveSettings_,dissolveWait_,dissolveDuration_);
	if (success) params_.dissolve = dissolve_.GetParams();
	return success;
}

void NeonSkinnedPreview::ResetDissolve() {
	if (!model_ || !dissolve_.IsActive()) return;
	if (dissolve_.Reset(*model_)) {
		params_.dissolve = dissolve_.GetParams();
		restorePoseThisFrame_ = true; // First restored frame samples the saved time/blend without advancing it.
	}
}

void NeonSkinnedPreview::StartDissolveComparison() {
	if (!showcaseActive_ || !ready_ || !neonMode_ || showcaseComparisonActive_ || dissolveComparisonActive_ ||
		showcaseSequence_ || dissolveSequence_ || showcaseCapture_.IsBusy() || showcaseOrbit_ ||
		cg2::RuntimeProfiler::Get().IsCaptureActive() || !model_->IsAnimationPaused() || dissolve_.IsPlaying()) return;
	const auto& settings = dissolve_.IsActive() ? dissolve_.GetParams() : dissolveSettings_;
	if (params_.geometryLineEnabled || !(settings.noiseStrength > 0) || !(settings.noiseScale > 0) ||
		!settings.edgeEnabled || !(settings.edgeWidth > 0) || !(settings.edgeIntensity > 0)) {
		dissolveComparisonStatus_ = "Eight-condition diagnostics require Geometry Lines OFF, nonzero noise strength/scale and edge emission ON with positive width/intensity.";
		return;
	}
	dissolveComparisonController_ = dissolve_;
	dissolveComparisonPlayback_ = model_->CaptureAnimationPlaybackState();
	dissolveComparisonOriginalParams_ = params_.dissolve;
	if (!dissolve_.IsActive() && !TriggerDissolve()) {
		dissolveComparisonStatus_ = "Cannot start dissolve comparison: " + dissolve_.GetError(); return;
	}
	dissolve_.SetPlaying(false);
	dissolveComparisonDirectory_ = showcaseCaptureDirectory_ + "/dissolve_comparison_" +
		std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count());
	dissolveComparisonIndex_ = 0; dissolveComparisonActive_ = true;
	dissolveComparisonStatus_ = "Capturing eight fixed-pose dissolve conditions.";
	ApplyDissolveComparisonCase();
}

void NeonSkinnedPreview::ApplyDissolveComparisonCase() {
	const auto& comparison = kDissolveComparisonCases[dissolveComparisonIndex_];
	dissolve_.Seek(comparison.progress);
	dissolveComparisonParams_ = dissolve_.GetParams();
	dissolveComparisonParams_.enabled = comparison.enabled ? 1u : 0u;
	if (!comparison.noise) dissolveComparisonParams_.noiseStrength = 0;
	if (!comparison.edge) dissolveComparisonParams_.edgeEnabled = 0;
	params_.dissolve = dissolveComparisonParams_;
	dissolveComparisonFrameRecorded_ = false;
}

void NeonSkinnedPreview::AdvanceDissolveComparison() {
	if (!dissolveComparisonActive_ || !dissolveComparisonFrameRecorded_ || showcaseCapture_.IsBusy()) return;
	if (!showcaseCapture_.WasLastCaptureSuccessful()) {
		FinishDissolveComparison("Dissolve comparison failed: " + showcaseCapture_.GetStatus()); return;
	}
	if (++dissolveComparisonIndex_ == std::size(kDissolveComparisonCases)) {
		FinishDissolveComparison("Saved eight same-pose conditions: " + dissolveComparisonDirectory_); return;
	}
	ApplyDissolveComparisonCase();
}

void NeonSkinnedPreview::FinishDissolveComparison(const std::string& status) {
	if (!dissolveComparisonActive_) return;
	if (!model_->RestoreAnimationPlaybackState(dissolveComparisonPlayback_)) {
		dissolveComparisonStatus_ = "Comparison checkpoint restore failed."; return;
	}
	dissolve_ = dissolveComparisonController_;
	params_.dissolve = dissolveComparisonOriginalParams_;
	dissolveComparisonActive_ = false; dissolveComparisonStatus_ = status;
	restorePoseThisFrame_ = true;
}

void NeonSkinnedPreview::StartDissolveSequence() {
	if (!showcaseActive_ || !ready_ || !neonMode_ || showcaseComparisonActive_ || dissolveComparisonActive_ ||
		showcaseSequence_ || dissolveSequence_ || dissolve_.IsActive() || showcaseCapture_.IsBusy() ||
		cg2::RuntimeProfiler::Get().IsCaptureActive()) return;
	dissolveSequencePlayback_ = model_->CaptureAnimationPlaybackState();
	dissolveSequenceOriginalOrbit_ = showcaseOrbit_; dissolveSequenceOriginalYaw_ = showcaseYaw_;
	showcaseSequenceFrame_ = 0; dissolveSequenceLastEventFrame_ = UINT32_MAX;
	showcaseSequenceDirectory_ = showcaseCaptureDirectory_ + "/dissolve_sequence_" +
		std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count());
	neonpreview::SelectAnimation(*model_,neonpreview::Clip::Idle);
	model_->SeekCurrentAnimation(0); model_->SetAnimationPlaying(true);
	showcaseOrbit_ = false; showcaseYaw_ = 0; dissolveSequence_ = true;
}

void NeonSkinnedPreview::UpdateDissolveSequence(bool afterPoseUpdate) {
	if (!dissolveSequence_ || dissolveSequenceLastEventFrame_ == showcaseSequenceFrame_) return;
	if (!afterPoseUpdate && showcaseSequenceFrame_ >= 480 && !showcaseCapture_.IsBusy()) {
		FinishDissolveSequence(); return;
	}
	const auto frame = showcaseSequenceFrame_;
	const bool trigger = frame == 60 || frame == 265;
	if (trigger != afterPoseUpdate) return;
	if (trigger) {
		if (!TriggerDissolve()) { FinishDissolveSequence(); return; }
	} else if (frame == 120 || frame == 330) {
		showcaseOrbit_ = true;
	} else if (frame == 200 || frame == 415) {
		ResetDissolve(); showcaseOrbit_ = false;
	} else if (frame == 235) {
		neonpreview::SelectAnimation(*model_,neonpreview::Clip::Attack);
		model_->SeekCurrentAnimation(0); model_->SetAnimationPlaying(true);
		showcaseYaw_ = 0;
	} else return;
	dissolveSequenceLastEventFrame_ = frame;
}

void NeonSkinnedPreview::FinishDissolveSequence() {
	if (!dissolveSequence_) return;
	ResetDissolve();
	if (!model_->RestoreAnimationPlaybackState(dissolveSequencePlayback_))
		animationError_ = "Dissolve sequence playback restore failed.";
	showcaseOrbit_ = dissolveSequenceOriginalOrbit_; showcaseYaw_ = dissolveSequenceOriginalYaw_;
	dissolveSequence_ = false; restorePoseThisFrame_ = true;
}

void NeonSkinnedPreview::DrawDissolveImGui() {
#ifdef USE_IMGUI
	if (!ready_) return;
	const bool recording = showcaseSequence_ || dissolveSequence_;
	const bool locked = showcaseComparisonActive_ || dissolveComparisonActive_ || recording ||
		cg2::RuntimeProfiler::Get().IsCaptureActive() || showcaseCapture_.IsBusy();
	ImGui::TextUnformatted("Directional surface dissolve / Neon only");
	ImGui::TextWrapped("Trigger freezes the displayed animation including its blend. The start camera's scan plane and seed remain fixed when the camera moves.");
	ImGui::BeginDisabled(locked || !neonMode_);
	if (ImGui::Button("Trigger Dissolve")) TriggerDissolve();
	ImGui::EndDisabled();
	ImGui::BeginDisabled(locked || !dissolve_.IsActive());
	ImGui::SameLine();
	if (ImGui::Button(dissolve_.IsPlaying() ? "Dissolve Pause" : "Dissolve Play")) dissolve_.SetPlaying(!dissolve_.IsPlaying());
	float progress = dissolve_.GetParams().progress;
	if (ImGui::SliderFloat("Dissolve progress",&progress,0,1,"%.3f")) dissolve_.Seek(progress);
	if (ImGui::Button("Restart from same pose")) dissolve_.Restart(*model_);
	ImGui::SameLine(); if (ImGui::Button("Reset / Restore playback")) ResetDissolve();
	float speed = dissolve_.GetPlaybackSpeed();
	if (ImGui::SliderFloat("Dissolve playback speed",&speed,0.05f,4,"%.2fx")) dissolve_.SetPlaybackSpeed(speed);
	ImGui::EndDisabled();
	ImGui::Text("Dissolve: %s / %.3f s / progress %.3f",dissolve_.IsActive() ?
		(dissolve_.IsPlaying() ? "playing" : "paused/finished") : "disabled",dissolve_.GetElapsed(),dissolve_.GetParams().progress);
	ImGui::BeginDisabled(locked || dissolve_.IsActive());
	const char* directions[] = {"Start-camera upper left to lower right", "Upper right to lower left", "Top to bottom", "Left to right"};
	ImGui::Combo("Scan direction",&dissolveDirection_,directions,4);
	ImGui::SliderFloat("Wait before dissolve (s)",&dissolveWait_,0,2);
	ImGui::SliderFloat("Dissolve duration (s)",&dissolveDuration_,0.1f,6);
	ImGui::SliderFloat("Noise strength (model units)",&dissolveSettings_.noiseStrength,0,0.3f);
	ImGui::SliderFloat("Noise scale (cells/model unit)",&dissolveSettings_.noiseScale,0,64);
	ImGui::InputScalar("Fixed seed",ImGuiDataType_U32,&dissolveSettings_.seed);
	bool edge = dissolveSettings_.edgeEnabled != 0;
	if (ImGui::Checkbox("Dissolve edge emission",&edge)) dissolveSettings_.edgeEnabled = edge ? 1u : 0u;
	ImGui::SliderFloat("Edge width (model units)",&dissolveSettings_.edgeWidth,0,0.08f);
	ImGui::SliderFloat("Edge HDR intensity",&dissolveSettings_.edgeIntensity,0,20);
	ImGui::ColorEdit3("Edge color",&dissolveSettings_.edgeColor.x);
	ImGui::EndDisabled();
	ImGui::TextWrapped("Reset before editing the plane/noise/edge settings. Animation and Transform controls are locked during dissolve. Restart retains the first pose, plane and seed. No cut faces or particles are generated.");
	if (!dissolve_.GetError().empty()) ImGui::TextWrapped("Dissolve error: %s",dissolve_.GetError().c_str());
	if (showcaseActive_) {
		ImGui::BeginDisabled(locked || !neonMode_ || !model_->IsAnimationPaused() || showcaseOrbit_ || dissolve_.IsPlaying());
		if (ImGui::Button("Capture 8 dissolve conditions (same pose)")) StartDissolveComparison();
		ImGui::EndDisabled();
		ImGui::TextWrapped("Eight-condition capture requires Geometry Lines OFF, nonzero noise strength/scale, and edge emission ON with positive width/intensity. Other experiments remain available through single-frame capture.");
		if (!dissolveComparisonStatus_.empty()) ImGui::TextWrapped("%s",dissolveComparisonStatus_.c_str());
		ImGui::BeginDisabled(showcaseComparisonActive_ || dissolveComparisonActive_ || showcaseSequence_ ||
			cg2::RuntimeProfiler::Get().IsCaptureActive() || (!dissolveSequence_ && (dissolve_.IsActive() || showcaseCapture_.IsBusy())) || !neonMode_);
		if (ImGui::Button(dissolveSequence_ ? "Stop dissolve recording" : "Record 8s: Idle / Attack / dissolve / orbit / Reset")) {
			if (dissolveSequence_) FinishDissolveSequence(); else StartDissolveSequence();
		}
		ImGui::EndDisabled();
	}
#endif
}

void NeonSkinnedPreview::UpdateShowcaseCamera(float deltaTime) {
	if (showcaseOrbit_ && std::isfinite(deltaTime)) showcaseYaw_ += showcaseOrbitSpeed_ * deltaTime;
	constexpr float targets[] = {1.32f, 1.10f, 0.82f, 0.82f, 1.32f};
	constexpr float distances[] = {1.15f, 2.65f, 4.65f, 8.5f, 0.65f};
	const float distance = distances[showcaseFraming_];
	showcaseCamera_.SetTranslate({std::sin(showcaseYaw_) * distance, targets[showcaseFraming_], -std::cos(showcaseYaw_) * distance});
	showcaseCamera_.SetRotate({0, -showcaseYaw_, 0});
	showcaseCamera_.SetFovY(0.45f);
	showcaseCamera_.SetNearClip(0.03f); showcaseCamera_.SetFarClip(100.0f);
	const auto viewport = cg2::Object3dCommon::GetInstance()->GetDxCommon()->GetViewportRect();
	showcaseCamera_.SetAspectRatio(viewport.Width / (std::max)(1.0f, viewport.Height));
	showcaseCamera_.SetProjectionJitter({});
	showcaseCamera_.Update();
}

nlohmann::json NeonSkinnedPreview::MakeShowcaseMetadata() const {
    const auto& renderParams = renderer_.GetParams();
	auto vector = [](const cg2::Vector3& v) { return nlohmann::json::array({v.x,v.y,v.z}); };
	const char* framings[] = {"face", "upper_body", "full_body", "game_size", "face_detail"};
	nlohmann::json metadata = {
		{"schemaVersion",1}, {"model",kPreviewModelPath}, {"build","Development"}, {"renderScale",1.0},
		{"expectedModelSha256","7FCA4A77FDC60AB2C78A9907430744562626180125FA386EB74FB2EA15C2E518"},
		{"mode",neonMode_ ? "Neon" : "Normal"}, {"framing",framings[showcaseFraming_]},
		{"qualityCandidate",qualityCandidate_},
		{"textureProvenance","Manifest expected SHA retained at first Preview load; GPU contents are not hashed. Runtime texture regeneration requires an application restart."},
		{"camera",{{"position",vector(showcaseCamera_.GetTranslate())},{"rotation",vector(showcaseCamera_.GetRotate())},
			{"fovY",0.45f},{"orbit",showcaseOrbit_}}},
		{"transform",{{"position",vector(showcaseTransform_.translate)},{"rotation",vector(showcaseTransform_.rotate)},
			{"scale",vector(showcaseTransform_.scale)}}},
		{"animation",{{"clip",model_->GetAnimation().name},{"time",model_->GetCurrentAnimationTime()},
			{"paused",model_->IsAnimationPaused()},{"speed",model_->GetAnimationPlayer().GetPlaybackSpeed()}}},
		{"bloom",{{"threshold",showcaseBloomThreshold_},{"intensity",showcaseBloomIntensity_},{"exposure",showcaseExposure_},
			{"diagnostic",showcaseDiagnostic_},{"taa",false},{"grayscale",false}}},
		{"neon",{{"bodyColor",{renderParams.bodyColor.x,renderParams.bodyColor.y,renderParams.bodyColor.z}},
			{"bodyEmission",renderParams.bodyEmissionIntensity},{"outlineWidth",renderParams.outlineWidthPixels},
			{"outlineEnabled",renderParams.outlineEnabled},{"internalEnabled",renderParams.internalLineEnabled},
			{"rimStrength",renderParams.rimStrength},{"rimPower",renderParams.rimPower},
			{"outlineIntensity",renderParams.emissiveIntensity},{"lineColor",vector(renderParams.emissiveColor)},
			{"internalWidth",renderParams.internalLineWidthPixels},{"internalIntensity",renderParams.internalLineIntensity},
			{"internalThreshold",renderParams.internalLineThreshold},{"maskBlend",renderParams.featureMaskBlend},
			{"maskColor",vector(renderParams.featureMaskColor)},{"maskIntensity",renderParams.featureMaskIntensity},
			{"maskDiagnostic",renderParams.featureMaskDebugMode},{"geometryLines",renderParams.geometryLineEnabled},
			{"geometryWidth",renderParams.geometryLineWidthPixels},{"geometryIntensity",renderParams.geometryLineIntensity},
			{"geometryColor",vector(renderParams.geometryLineColor)},
			{"alphaCutout",alphaCutoutEnabled_},{"maskRenderMode",renderParams.featureMaskRenderMode},
			{"splitCoreHalo",renderParams.splitLineEmission},{"lineDiagnostic",renderParams.lineDiagnosticMode},
			{"coreColor",vector(renderParams.lineCoreColor)},{"coreIntensity",renderParams.lineCoreIntensity},
			{"haloColor",vector(renderParams.lineHaloColor)},{"haloIntensity",renderParams.lineHaloIntensity},
			{"outlineCoreColor",vector(renderParams.outlineCoreColor)},{"outlineCoreIntensity",renderParams.outlineCoreIntensity},
			{"sdfRangeTexels",renderParams.sdfRangeTexels},{"sdfHaloWidthTexels",renderParams.sdfHaloWidthTexels},
			{"sdfLodBlendStart",renderParams.sdfLodBlendStart},{"sdfLodBlendEnd",renderParams.sdfLodBlendEnd}}}
	};
	const auto& effect = renderParams.dissolve;
	metadata["dissolve"] = {{"active",dissolve_.IsActive()},{"playing",dissolve_.IsPlaying()},
		{"enabled",effect.enabled},{"progress",effect.progress},{"direction",vector(effect.direction)},
		{"scanMin",effect.scanMin},{"scanMax",effect.scanMax},{"noiseStrength",effect.noiseStrength},
		{"noiseScale",effect.noiseScale},{"seed",effect.seed},{"edgeEnabled",effect.edgeEnabled},
		{"edgeColor",vector(effect.edgeColor)},{"edgeIntensity",effect.edgeIntensity},{"edgeWidth",effect.edgeWidth},
		{"elapsed",dissolve_.GetElapsed()},{"waitDuration",dissolve_.GetWaitDuration()},
		{"duration",dissolve_.GetDuration()},{"playbackSpeed",dissolve_.GetPlaybackSpeed()},
		{"directionPreset",static_cast<int>(dissolve_.GetDirectionPreset())},
		{"coordinateSpace","Frozen skinned model space before World; start camera plane remains fixed."},
		{"distanceUnits","scan bounds, noise strength and edge width in model units; noise scale in lattice cells/model unit"}};
	if (dissolve_.IsActive()) {
		auto matrix = [](const cg2::Matrix4x4& value) {
			nlohmann::json rows = nlohmann::json::array();
			for (const auto& row : value.m) rows.push_back({row[0],row[1],row[2],row[3]});
			return rows;
		};
		const auto& saved = dissolve_.GetSavedPlayback();
		auto& freeze = metadata["dissolve"]["frozenPlayback"];
		freeze = {{"animationIndex",saved.animationIndex},{"clip",model_->GetAnimations().at(saved.animationIndex).name},
			{"time",saved.time},{"speed",saved.speed},{"playing",saved.playing},{"loop",saved.loop},{"paused",saved.paused},
			{"transitionActive",saved.transitionActive},{"transitionDuration",saved.transitionDuration},{"transitionElapsed",saved.transitionElapsed}};
		metadata["dissolve"]["startWorld"] = matrix(dissolve_.GetStartWorld());
		metadata["dissolve"]["startCameraWorld"] = matrix(dissolve_.GetStartCameraWorld());
		for (const auto& pose : saved.transitionStartPose) freeze["transitionStartPose"].push_back({
			{"translation",vector(pose.translate)},{"scale",vector(pose.scale)},
			{"rotation",{pose.rotate.x,pose.rotate.y,pose.rotate.z,pose.rotate.w}}});
		const auto& poses = dissolve_.GetFrozenPose();
		for (size_t index=0; index<poses.size(); ++index) {
			const auto& pose = poses[index];
			metadata["dissolve"]["frozenJointPose"].push_back({{"joint",model_->GetSkeleton().joints[index].name},
				{"translation",vector(pose.translate)},{"scale",vector(pose.scale)},
				{"rotation",{pose.rotate.x,pose.rotate.y,pose.rotate.z,pose.rotate.w}}});
		}
	}
	Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter;
	auto* dx = cg2::Object3dCommon::GetInstance()->GetDxCommon();
	const auto viewport = dx->GetViewportRect();
	metadata["resolution"] = {static_cast<uint32_t>(viewport.Width),static_cast<uint32_t>(viewport.Height)};
	metadata["sceneViewportResolution"] = metadata["resolution"];
	metadata["resolutionSource"] = "DirectXCommon viewport; Showcase uses full-size Scene and backbuffer (renderScale 1).";
	metadata["validation"] = {{"d3d12DebugLayer",dx->IsD3D12DebugLayerEnabled()},
		{"gpuBasedValidation",dx->IsGpuBasedValidationEnabled()}};
	UINT64 timestampFrequency = 0;
	if (SUCCEEDED(dx->GetQueue()->GetTimestampFrequency(&timestampFrequency)))
		metadata["queueTimestampFrequencyHz"] = timestampFrequency;
	if (SUCCEEDED(dx->GetDxgiFactory()->EnumAdapterByLuid(dx->GetDevice()->GetAdapterLuid(),IID_PPV_ARGS(&adapter)))) {
		DXGI_ADAPTER_DESC3 description{};
		if (SUCCEEDED(adapter->GetDesc3(&description))) {
			char name[256]{};
			WideCharToMultiByte(CP_UTF8,0,description.Description,-1,name,sizeof(name),nullptr,nullptr);
			metadata["gpu"] = {{"adapter",name},{"vendor",description.VendorId},{"device",description.DeviceId},
				{"dedicatedVideoMemoryBytes",description.DedicatedVideoMemory}};
		}
	}
	for (size_t index = 0; index < submeshParams_.size(); ++index) metadata["submeshes"].push_back({
		{"material",model_->GetSubmesh(index).materialName},{"mask",featureMaskBindings_[index].path},
		{"maskBound",featureMaskIndices_[index].has_value()},{"distanceMask",featureMaskBindings_[index].distancePath},
		{"distanceBound",featureDistanceIndices_[index].has_value()},{"loadStatus",featureMaskBindings_[index].status},
		{"coverageManifestExpectedSha256",featureMaskBindings_[index].coverageSha256},{"sdfManifestExpectedSha256",featureMaskBindings_[index].distanceSha256},
		{"authoringManifestExpectedSha256",featureMaskBindings_[index].authoringSha256},
		{"authoringVersionManifestExpectedSha256",featureMaskBindings_[index].authoringVersionSha256},
		{"authoringRevision",featureMaskBindings_[index].authoringRevision},
		{"lineStrength",submeshParams_[index].lineStrength},{"geometryStrength",submeshParams_[index].geometryLineStrength},
		{"thresholdScale",submeshParams_[index].internalLineThresholdScale},{"alphaCutoff",submeshParams_[index].alphaCutoff}});
	return metadata;
}

void NeonSkinnedPreview::RecordShowcaseCapture(cg2::DirectXCommon& dx) {
	if (!showcaseActive_) return;
	bool requestedSequenceFrame = false;
	bool requestedComparisonFrame = false;
	bool requestedDissolveComparisonFrame = false;
	if (showcaseComparisonActive_ && !showcaseComparisonFrameRecorded_ && !showcaseCapture_.IsBusy()) {
		char name[96]{};
		std::snprintf(name,sizeof(name),"%02u_%s",showcaseComparisonIndex_,kQualityComparisonCases[showcaseComparisonIndex_].label);
		showcaseCapture_.Request(showcaseComparisonDirectory_,name,{});
		showcaseComparisonFrameRecorded_ = requestedComparisonFrame = true;
	}
	if (dissolveComparisonActive_ && !dissolveComparisonFrameRecorded_ && !showcaseCapture_.IsBusy()) {
		char name[96]{};
		std::snprintf(name,sizeof(name),"%02u_%s",dissolveComparisonIndex_,kDissolveComparisonCases[dissolveComparisonIndex_].label);
		showcaseCapture_.Request(dissolveComparisonDirectory_,name,{});
		dissolveComparisonFrameRecorded_ = requestedDissolveComparisonFrame = true;
	}
	if ((showcaseSequence_ || dissolveSequence_) && !showcaseCapture_.IsBusy() && (!dissolveSequence_ || showcaseSequenceFrame_ < 480)) {
		char name[80]{};
		std::snprintf(name,sizeof(name),"frame_%05u",showcaseSequenceFrame_++);
		auto metadata = MakeShowcaseMetadata(); metadata["sequenceFrame"] = showcaseSequenceFrame_ - 1;
		metadata["sequenceRate"] = 60;
		showcaseCapture_.Request(showcaseSequenceDirectory_,name,std::move(metadata));
		requestedSequenceFrame = true;
		if (!dissolveSequence_ && showcaseSequenceFrame_ >= 240) showcaseSequence_ = false;
	}
	if (showcaseCapture_.HasRequest()) {
		auto frameMetadata = MakeShowcaseMetadata();
		if (requestedComparisonFrame) frameMetadata["comparison"] = {{"case",showcaseComparisonIndex_},
			{"label",kQualityComparisonCases[showcaseComparisonIndex_].label},{"count",std::size(kQualityComparisonCases)},
			{"samePausedPose",true},{"restoreAppearanceAfterCapture",true}};
		if (requestedDissolveComparisonFrame) frameMetadata["dissolveComparison"] = {{"case",dissolveComparisonIndex_},
			{"label",kDissolveComparisonCases[dissolveComparisonIndex_].label},{"count",std::size(kDissolveComparisonCases)},
			{"sameFrozenPose",true},{"restorePlaybackAfterCapture",true}};
		if (requestedSequenceFrame) {
			frameMetadata["sequenceFrame"] = showcaseSequenceFrame_ - 1; frameMetadata["sequenceRate"] = 60;
			frameMetadata["sequenceKind"] = dissolveSequence_ ? "Idle/Attack dissolve with orbit and Reset" : "Idle/Attack orbit";
		}
		showcaseCapture_.SetFrameMetadata(std::move(frameMetadata));
	}
	showcaseCapture_.Record(dx);
}

void NeonSkinnedPreview::DrawShowcaseImGui() {
#ifdef USE_IMGUI
	if (!showcaseActive_) {
		if (ImGui::Button("Enter Neon Character Showcase")) EnterShowcase();
		ImGui::TextWrapped("Developer-only black-background comparison. Game state and Preview settings are restored on exit.");
		return;
	}
	const bool gpuCapture = cg2::RuntimeProfiler::Get().IsCaptureActive();
	const bool automated = showcaseComparisonActive_ || dissolveComparisonActive_ || dissolveSequence_;
	ImGui::BeginDisabled(automated || gpuCapture);
	const char* framings[] = {"Face close-up", "Upper body", "Full body", "Game-size", "Face detail (magnified)"};
	ImGui::Combo("Showcase framing", &showcaseFraming_, framings,5);
	const char* views[] = {"Front", "Three-quarter (45 deg)", "Side (90 deg)"};
	if (ImGui::Combo("Showcase view",&showcaseView_,views,3)) {
		showcaseYaw_ = static_cast<float>(showcaseView_) * cg2::pi * 0.25f; showcaseOrbit_ = false;
	}
	ImGui::Checkbox("Camera orbit (independent of animation)",&showcaseOrbit_);
	ImGui::SliderFloat("Orbit speed (rad/s)",&showcaseOrbitSpeed_,0.05f,0.8f);
	ImGui::TextWrapped("Camera remains fixed when orbit is off. Animation Pause/Seek below does not change the camera.");
	const char* diagnostics[] = {"Final color", "Scene HDR (tone mapped)", "Bloom blur only", "Bloom extract (tone mapped)"};
	ImGui::Combo("Showcase output",&showcaseDiagnostic_,diagnostics,4);
	ImGui::SliderFloat("Showcase Bloom threshold",&showcaseBloomThreshold_,0.0f,4.0f);
	ImGui::SliderFloat("Showcase Bloom intensity",&showcaseBloomIntensity_,0.0f,2.0f);
	ImGui::TextWrapped("Existing Bloom applies intensity in both blur passes and final composition: effective gain is intensity cubed.");
	ImGui::SliderFloat("Showcase exposure",&showcaseExposure_,0.05f,2.0f);
	ImGui::InputText("Capture label",showcaseCaptureLabel_,sizeof(showcaseCaptureLabel_));
	ImGui::EndDisabled();
	ImGui::BeginDisabled(showcaseCapture_.IsBusy() || showcaseSequence_ || automated || gpuCapture);
	if (ImGui::Button("Save clean PNG + settings JSON")) {
		char name[96]{}; std::snprintf(name,sizeof(name),"%03u_%s",showcaseCaptureNumber_++,showcaseCaptureLabel_);
		showcaseCapture_.Request(showcaseCaptureDirectory_,name,MakeShowcaseMetadata());
	}
	ImGui::EndDisabled();
	ImGui::BeginDisabled(automated || dissolve_.IsActive() || showcaseSequence_ || showcaseCapture_.IsBusy() || gpuCapture ||
		!model_->IsAnimationPaused() || showcaseOrbit_);
	if (ImGui::Button("Capture 7 candidates at current paused pose")) StartShowcaseComparison();
	ImGui::EndDisabled();
	ImGui::TextWrapped("Seven Neon cases, fixed camera and paused pose. Stop orbit, pause animation, and finish recording/timing first. Appearance is restored after capture.");
	if (!showcaseComparisonStatus_.empty()) ImGui::TextWrapped("%s",showcaseComparisonStatus_.c_str());
	ImGui::BeginDisabled(automated || dissolve_.IsActive() || gpuCapture || (showcaseCapture_.IsBusy() && !showcaseSequence_));
	if (ImGui::Button(showcaseSequence_ ? "Stop image sequence" : "Record 4s: Idle / Attack / orbit")) {
		showcaseSequence_ = !showcaseSequence_; showcaseSequenceFrame_ = 0; showcaseSequenceAttackStarted_ = false;
		if (showcaseSequence_) {
			showcaseSequenceDirectory_ = showcaseCaptureDirectory_ + "/sequence_" +
				std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch()).count());
			neonpreview::SelectAnimation(*model_,neonpreview::Clip::Idle);
			model_->SeekCurrentAnimation(0); model_->SetAnimationPlaying(true);
			showcaseOrbit_ = true; showcaseYaw_ = 0;
		}
	}
	ImGui::EndDisabled();
	if (!showcaseSequenceDirectory_.empty()) ImGui::TextWrapped("Sequence folder: %s",showcaseSequenceDirectory_.c_str());
	ImGui::BeginDisabled(automated || showcaseSequence_ || showcaseCapture_.IsBusy() || gpuCapture || dissolve_.IsPlaying());
	if (ImGui::Button("Measure GPU: 60 warmup + 300 frames")) {
		const auto path = std::filesystem::path(showcaseCaptureDirectory_) / (std::string(showcaseCaptureLabel_) + "_gpu.csv");
		if (std::filesystem::path(showcaseCaptureLabel_).filename().string() == showcaseCaptureLabel_ &&
			cg2::RuntimeProfiler::Get().StartCapture(path.generic_string(),300,60)) {
			auto metadata = MakeShowcaseMetadata();
			metadata["measurement"] = {{"warmupFrames",60},{"sampleFrames",300},{"method","D3D12 timestamp query / queue frequency / existing fence"}};
			std::ofstream stream(path.parent_path() / (std::string(showcaseCaptureLabel_) + "_gpu.json")); stream << metadata.dump(2) << '\n';
			showcaseTimingStatus_ = "GPU capture: " + path.generic_string();
		} else {
			showcaseTimingStatus_ = "GPU capture unavailable: profiler/timestamp queries disabled, or invalid label/output path.";
		}
	}
	ImGui::EndDisabled();
	if (!showcaseTimingStatus_.empty()) {
		ImGui::TextWrapped("%s",showcaseTimingStatus_.c_str());
		if (cg2::RuntimeProfiler::Get().IsCaptureComplete()) ImGui::TextUnformatted("GPU capture complete (300 timestamp frames).");
	}
	ImGui::TextWrapped("%s",showcaseCapture_.GetStatus().c_str());
	ImGui::TextWrapped("Raw images exclude UI. Sequence PNGs contain actual engine frames at 60 updates/s; offline encoding does not alter their appearance.");
	ImGui::TextWrapped("For timing comparisons, stop orbit and animation, hold all settings fixed, and wait for the 300-frame capture to finish.");
	ImGui::Separator();
#endif
}

void NeonSkinnedPreview::DrawAnimationImGui() {
#ifdef USE_IMGUI
	if (ready_) {
		ImGui::Text("GLB animations: %zu / Generated clips: %zu", sourceAnimationCount_, generatedAnimationCount_);
		if (!animationError_.empty()) ImGui::TextWrapped("Motion unavailable (BindPose retained): %s", animationError_.c_str());
		ImGui::BeginDisabled(dissolve_.IsActive() || dissolveComparisonActive_ || dissolveSequence_ ||
			cg2::RuntimeProfiler::Get().IsCaptureActive());
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
		ImGui::EndDisabled();
		if (dissolve_.IsActive()) ImGui::TextUnformatted("Animation is frozen for dissolve; use Reset / Restore playback first.");
	}
#endif
}

void NeonSkinnedPreview::DrawImGui() {
#ifdef USE_IMGUI
	if (showcaseActive_) { ImGui::TextUnformatted("Neon Character Showcase is open in its dedicated window."); return; }
	if (!ImGui::CollapsingHeader("Neon Skinned Preview", ImGuiTreeNodeFlags_DefaultOpen)) return;
	DrawShowcaseImGui();
	if (showcaseActive_) return;
	ImGui::TextUnformatted("AvatarSample_B / Developer only");
	ImGui::BeginDisabled(showcaseActive_);
	if (ImGui::Checkbox("Preview Enable", &enabled_)) { if (enabled_) Load(); else ResetDissolve(); }
	ImGui::EndDisabled();
	if (!loadError_.empty()) ImGui::TextWrapped("Load failed: %s", loadError_.c_str());
	int mode = neonMode_ ? 1 : 0;
	ImGui::BeginDisabled(dissolve_.IsActive());
	if (ImGui::RadioButton("Normal", mode == 0)) neonMode_ = false;
	ImGui::SameLine();
	if (ImGui::RadioButton("Neon", mode == 1)) neonMode_ = true;
	ImGui::EndDisabled();
	ImGui::BeginDisabled(!ready_);
	if (ImGui::Button("Recommended Line Art")) ApplyRecommendedLineArtPreset();
	if (ImGui::Button("Legacy Neon comparison")) ApplyLegacyNeonPreset();
	ImGui::EndDisabled();
	DrawFeatureMaskImGui();
	DrawQualityCandidateImGui();
	DrawAnimationImGui();
	DrawGeometryLinesImGui();
	DrawDissolveImGui();
	ImGui::BeginDisabled(dissolve_.IsActive());
	ImGui::DragFloat3("Position", &transform_.translate.x, 0.1f);
	ImGui::DragFloat3("Rotation (radians)", &transform_.rotate.x, 0.01f);
	ImGui::DragFloat3("Scale", &transform_.scale.x, 0.05f, 0.01f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	if (ImGui::Button("Place in front of camera")) PlaceInFrontOfCamera();
	ImGui::EndDisabled();
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

void NeonSkinnedPreview::DrawShowcaseWindow() {
#ifdef USE_IMGUI
	if (!showcaseActive_) return;
	ImGui::SetNextWindowPos({12,45},ImGuiCond_FirstUseEver);
	const float availableHeight = (std::max)(200.0f,ImGui::GetIO().DisplaySize.y - 80.0f);
	ImGui::SetNextWindowSize({510,(std::min)(760.0f,availableHeight)},ImGuiCond_FirstUseEver);
	// Apply the height cap to an existing imgui.ini size too; leave its user-selected width free.
	ImGui::SetNextWindowSizeConstraints({0,200},{(std::numeric_limits<float>::max)(),availableHeight});
	const bool gpuCapture = cg2::RuntimeProfiler::Get().IsCaptureActive();
	bool open = true;
	if (!ImGui::Begin("Neon Character Showcase",gpuCapture ? nullptr : &open)) {
		ImGui::End(); if (!open) LeaveShowcase(); return;
	}
	ImGui::BeginDisabled(gpuCapture);
	if (ImGui::Button("Return to normal Preview")) { LeaveShowcase(); ImGui::EndDisabled(); ImGui::End(); return; }
	ImGui::EndDisabled();
	ImGui::SameLine(); ImGui::TextUnformatted("Black background / Developer only");
	if (ImGui::BeginTabBar("NeonShowcaseTabs")) {
		if (ImGui::BeginTabItem("View / Capture")) { DrawShowcaseImGui(); ImGui::EndTabItem(); }
		if (ImGui::BeginTabItem("Appearance")) {
			ImGui::BeginDisabled(showcaseComparisonActive_ || dissolveComparisonActive_ || dissolveSequence_ || gpuCapture);
			ImGui::BeginDisabled(dissolve_.IsActive());
			int mode = neonMode_ ? 1 : 0;
			if (ImGui::RadioButton("Normal",mode == 0)) neonMode_ = false;
			ImGui::SameLine(); if (ImGui::RadioButton("Neon",mode == 1)) neonMode_ = true;
			ImGui::EndDisabled();
			DrawQualityCandidateImGui();
			if (ImGui::TreeNode("Existing Body / Outline / Internal settings")) {
				ImGui::ColorEdit3("Body tint",&params_.bodyColor.x);
				ImGui::SliderFloat("Body emission",&params_.bodyEmissionIntensity,0,4);
				ImGui::SliderFloat("Outline width (pixels)",&params_.outlineWidthPixels,0,8);
				ImGui::SliderFloat("Outline HDR intensity",&params_.emissiveIntensity,0,20);
				ImGui::ColorEdit3("Original line color",&params_.emissiveColor.x);
				ImGui::SliderFloat("Internal intensity",&params_.internalLineIntensity,0,20);
				ImGui::SliderFloat("Authored legacy intensity",&params_.featureMaskIntensity,0,20);
				ImGui::Checkbox("Alpha cutout",&alphaCutoutEnabled_);
				ImGui::TreePop();
			}
			DrawGeometryLinesImGui();
			ImGui::EndDisabled();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Animation")) {
			ImGui::BeginDisabled(showcaseComparisonActive_); DrawAnimationImGui(); ImGui::EndDisabled(); ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Dissolve")) { DrawDissolveImGui(); ImGui::EndTabItem(); }
		ImGui::EndTabBar();
	}
	ImGui::End();
	if (!open) LeaveShowcase();
#endif
}
#endif
