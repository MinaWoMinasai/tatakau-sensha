#include "GraphicsLabScene.h"

#include <algorithm>
#include <cmath>
#include <exception>
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
const char* kPbrPlaneModelName = "__primitive_pbr_plane";
const char* kPbrBoxModelName = "__primitive_pbr_box";
const char* kPbrCylinderModelName = "__primitive_pbr_cylinder";
const char* kArcBlancOceanGridModelName = "__arc_blanc_ocean_grid";

#ifdef USE_IMGUI
const char* BoolStatus(bool value)
{
	return value ? "authored" : "fallback";
}

const char* TextureChannelName(float channel)
{
	const int channelIndex = static_cast<int>(channel + 0.5f);
	switch (channelIndex) {
	case 0:
		return "R";
	case 1:
		return "G";
	case 2:
		return "B";
	case 3:
		return "A";
	default:
		return "?";
	}
}

bool IsAuthoredBaseColorTexture(const std::string& path)
{
	return !path.empty() && path != "resources/white512x512.png";
}

void DrawTextureSlot(
	const char* label,
	const std::string& path,
	uint32_t srvIndex,
	bool authored,
	const char* colorSpace)
{
	ImGui::BulletText("%s: %s, srv=%u, %s", label, BoolStatus(authored), srvIndex, colorSpace);
	ImGui::TextWrapped("  %s", path.empty() ? "(empty)" : path.c_str());
}

void DrawScalarTextureSlot(
	const char* label,
	const std::string& path,
	uint32_t srvIndex,
	bool authored,
	float channel)
{
	ImGui::BulletText("%s: %s, srv=%u, channel=%s", label, BoolStatus(authored), srvIndex, TextureChannelName(channel));
	ImGui::TextWrapped("  %s", path.empty() ? "(empty)" : path.c_str());
}

void DrawMaterialTextureDebug(const std::string& modelName, const ModelData& modelData)
{
	ImGui::PushID(modelName.c_str());
	if (ImGui::TreeNode(modelName.c_str())) {
		ImGui::Text(
			"mesh: vertices=%zu, indices=%zu, materials=%zu, submeshes=%zu",
			modelData.vertices.size(),
			modelData.indices.size(),
			modelData.materials.empty() ? size_t{ 1 } : modelData.materials.size(),
			modelData.submeshes.size());
		if (!modelData.submeshes.empty() && ImGui::TreeNode("Submeshes")) {
			for (size_t submeshIndex = 0; submeshIndex < modelData.submeshes.size(); ++submeshIndex) {
				const ModelSubmesh& submesh = modelData.submeshes[submeshIndex];
				ImGui::BulletText(
					"[%zu] start=%u count=%u material=%u %s",
					submeshIndex,
					submesh.startIndex,
					submesh.indexCount,
					submesh.materialIndex,
					submesh.materialName.c_str());
			}
			ImGui::TreePop();
		}

		auto drawMaterial = [](const char* label, const MaterialData& material) {
			if (!ImGui::TreeNode(label)) {
				return;
			}
			ImGui::TextWrapped("name: %s", material.materialName.empty() ? "(unnamed)" : material.materialName.c_str());
			ImGui::Text(
				"semantic: %s  %s",
				MaterialSemanticName(material.semantic),
				material.semanticInferred ? "inferred" : "default/manual");
			ImGui::Text(
				"baseColor: %.3f %.3f %.3f %.3f  %s",
				material.baseColorFactor.x,
				material.baseColorFactor.y,
				material.baseColorFactor.z,
				material.baseColorFactor.w,
				material.hasBaseColorFactor ? "authored" : "default");
			ImGui::Text(
				"pbr: metallic=%.3f roughness=%.3f ao=%.3f  %s",
				material.metallicFactor,
				material.roughnessFactor,
				material.ambientOcclusionFactor,
				material.hasPbrFactors ? "authored" : "default");
			ImGui::Text(
				"emissive: %.3f %.3f %.3f x %.3f  %s",
				material.emissiveColor.x,
				material.emissiveColor.y,
				material.emissiveColor.z,
				material.emissiveIntensity,
				material.hasEmissive ? "authored" : "default");

			DrawTextureSlot(
				"Base color",
				material.textureFilePath,
				material.textureIndex,
				IsAuthoredBaseColorTexture(material.textureFilePath),
				"sRGB");
			DrawTextureSlot(
				"Normal",
				material.normalTextureFilePath,
				material.normalTextureIndex,
				material.hasNormalTexture,
				"linear");
			DrawScalarTextureSlot(
				"Packed material",
				material.metallicRoughnessTextureFilePath,
				material.metallicRoughnessTextureIndex,
				material.hasMetallicRoughnessTexture,
				material.roughnessMapChannel);
			ImGui::Text("  metallic channel=%s, roughness channel=%s",
				TextureChannelName(material.metallicMapChannel),
				TextureChannelName(material.roughnessMapChannel));
			if (!material.metallicTextureFilePath.empty()) {
				ImGui::TextWrapped("  metallic source: %s", material.metallicTextureFilePath.c_str());
			}
			if (!material.roughnessTextureFilePath.empty()) {
				ImGui::TextWrapped("  roughness source: %s", material.roughnessTextureFilePath.c_str());
			}
			DrawScalarTextureSlot(
				"Occlusion",
				material.occlusionTextureFilePath,
				material.occlusionTextureIndex,
				material.hasOcclusionTexture,
				material.occlusionMapChannel);
			ImGui::TreePop();
		};

		if (modelData.materials.empty()) {
			drawMaterial("Material 0", modelData.material);
		} else {
			for (size_t materialIndex = 0; materialIndex < modelData.materials.size(); ++materialIndex) {
				ImGui::PushID(static_cast<int>(materialIndex));
				const std::string label = "Material " + std::to_string(materialIndex);
				drawMaterial(label.c_str(), modelData.materials[materialIndex]);
				ImGui::PopID();
			}
		}

		ImGui::TreePop();
	}
	ImGui::PopID();
}
#endif
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
	Object3dCommon::GetInstance()->GetLightDir() = { -0.12f, -0.26f, -0.96f };

	ModelManager::GetInstance()->CreateGridModel(kArcBlancOceanGridModelName, 1800.0f, 1800.0f, 256, 256);
	if (showSandBed_) {
		ModelManager::GetInstance()->LoadModel("graphicsSand.obj");
	}
	if (loadLookDevSamples_) {
		ModelManager::GetInstance()->LoadModel("graphicsBeach.obj");
		ModelManager::GetInstance()->LoadModel("cube.obj");
		ModelManager::GetInstance()->LoadModel("ball.obj");
		ModelManager::GetInstance()->LoadModel("jewelry.obj");
		ModelManager::GetInstance()->LoadModel("TestBlock.obj");
		ModelManager::GetInstance()->CreateUvSphereModel(kPbrSphereModelName, 1.0f, 64, 128);
		ModelManager::GetInstance()->CreatePlaneModel(kPbrPlaneModelName, 14.0f, 14.0f);
		ModelManager::GetInstance()->CreateBoxModel(kPbrBoxModelName, { 2.0f, 2.0f, 2.0f });
		ModelManager::GetInstance()->CreateCylinderModel(kPbrCylinderModelName, 1.0f, 2.4f, 96);
		if (Model* ballModel = ModelManager::GetInstance()->FindModel("ball.obj")) {
			ballModel->RecalculateSmoothNormals();
		}
	}
	TextureManager::GetInstance()->LoadTexture("resources/skybox.dds");
	const uint32_t skyboxEnvironmentSrv =
		TextureManager::GetInstance()->GetSrvIndex("resources/skybox.dds");
	uint32_t pbrEnvironmentSrv = skyboxEnvironmentSrv;
	if (loadLookDevSamples_ || loadSkinnedPbrSamples_) {
		ApplyPbrEnvironmentDebugMode();
		pbrEnvironmentSrv = pbrEnvironment_.GetEnvironmentSrvIndex();
	} else {
		appliedPbrEnvironmentDebugMode_ = pbrEnvironmentDebugMode_;
	}

	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize("resources/skybox.dds");
	skybox_->SetColor({ 1.0f, 1.0f, 1.0f, 2.0f });

	river_ = std::make_unique<Object3d>();
	river_->Initialize();
	river_->SetModel(kArcBlancOceanGridModelName);
	river_->SetColor(riverTint_);
	river_->SetLighting(false);
	river_->SetEnvironmentMap(pbrEnvironmentSrv);
	river_->SetEnvironmentCoefficient(3.15f);
	river_->SetInsensity(waterLightIntensity_);
	river_->SetScale({ 1.0f, 1.0f, 1.0f });
	river_->SetTranslate({ 0.0f, -1.15f, 0.0f });
	river_->SetWaterDiagnostics(
		true,
		waterSunPathEnabled_,
		waterAtmosphereEnabled_,
		waterFarFlattenEnabled_,
		waterProceduralCloudReflectionEnabled_,
		waterDebugMode_,
		waterAtmosphereStrength_,
		waterFarFlattenStrength_);

	oceanRenderer_ = std::make_unique<OceanRenderer>();
	oceanRenderer_->Initialize(
		ModelManager::GetInstance()->FindModel(kArcBlancOceanGridModelName),
		skyboxEnvironmentSrv);

	if (showSandBed_) {
		sandBed_ = std::make_unique<Object3d>();
		sandBed_->Initialize();
		sandBed_->SetModel("graphicsSand.obj");
		sandBed_->SetColor({ 0.78f, 0.75f, 0.62f, 1.0f });
		sandBed_->SetLighting(false);
		sandBed_->SetEnvironmentCoefficient(-1.0f);
		sandBed_->SetScale({ 1.0f, 1.0f, 1.0f });
		sandBed_->SetTranslate({ 0.0f, -4.2f, 0.0f });
	}

	if (loadLookDevSamples_) {
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
		const Vector4& color, float metallic, float roughness, bool animateRotation = false) {
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
		sample.object->SetEnvironmentMap(pbrEnvironmentSrv);
		sample.object->SetIBLIntensity(pbrIblDiffuseIntensity_, pbrIblSpecularIntensity_);
		sample.object->SetIBLMaxMipLevel(pbrIblMaxMipLevel_);
		sample.object->SetPBREnvironmentMode(pbrFilteredIblBlend_);
		sample.object->SetShadowReceiveStrength(enablePbrSampleShadows_ ? 1.0f : 0.0f);
		sample.object->SetShadowFilter(pbrShadowDepthBias_, pbrShadowSlopeBias_, pbrShadowPcfRadius_);
		sample.object->SetNormalDetail(pbrNormalDetailStrength_, pbrNormalDetailScale_);
		sample.object->SetMaterialDebugMode(pbrMaterialDebugMode_);
		sample.object->SetInsensity(pbrDirectLightIntensity_);
		sample.shadowReceiveStrength = 0.0f;
		sample.castsShadow = false;
		sample.animateRotation = animateRotation;
		metalObjects_.push_back(std::move(sample));
	};

	const Vector3 sphereScale = { 6.2f, 6.2f, 6.2f };
	const float xs[] = { -54.0f, -18.0f, 18.0f, 54.0f };
	const float dielectricRoughness[] = { 0.08f, 0.28f, 0.58f, 0.88f };
	const Vector4 dielectricColors[] = {
		{ 0.90f, 0.18f, 0.12f, 1.0f },
		{ 0.12f, 0.38f, 0.78f, 1.0f },
		{ 0.86f, 0.90f, 0.94f, 1.0f },
		{ 0.05f, 0.06f, 0.07f, 1.0f },
	};
	for (int i = 0; i < 4; ++i) {
		addPbrSample(
			kPbrSphereModelName,
			{ xs[i], 8.8f, -28.0f },
			sphereScale,
			dielectricColors[i],
			0.0f,
			dielectricRoughness[i]);
	}

	const float metalRoughness[] = { 0.10f, 0.28f, 0.52f, 0.80f };
	const Vector4 metalColors[] = {
		{ 1.00f, 0.77f, 0.34f, 1.0f },
		{ 0.92f, 0.95f, 1.00f, 1.0f },
		{ 0.95f, 0.64f, 0.54f, 1.0f },
		{ 0.66f, 0.90f, 0.54f, 1.0f },
	};
	for (int i = 0; i < 4; ++i) {
		addPbrSample(
			kPbrSphereModelName,
			{ xs[i], 8.8f, 8.0f },
			sphereScale,
			metalColors[i],
			1.0f,
			metalRoughness[i]);
	}

	addPbrSample(
		"jewelry.obj",
		{ 84.0f, 7.4f, -10.0f },
		{ 4.0f, 4.0f, 4.0f },
		{ 0.82f, 0.88f, 0.90f, 1.0f },
		1.0f,
		0.34f,
		true);

	auto addValidationPrimitive = [&](const std::string& modelPath, const Vector3& translate, const Vector3& rotate,
		const Vector3& scale, const Vector4& color, float metallic, float roughness) {
		LabObject sample = MakeObject(
			modelPath,
			translate,
			rotate,
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
		sample.object->SetEnvironmentMap(pbrEnvironmentSrv);
		sample.object->SetIBLIntensity(pbrIblDiffuseIntensity_, pbrIblSpecularIntensity_);
		sample.object->SetIBLMaxMipLevel(pbrIblMaxMipLevel_);
		sample.object->SetPBREnvironmentMode(pbrFilteredIblBlend_);
		sample.object->SetShadowReceiveStrength(enablePbrSampleShadows_ ? 1.0f : 0.0f);
		sample.object->SetShadowFilter(pbrShadowDepthBias_, pbrShadowSlopeBias_, pbrShadowPcfRadius_);
		sample.object->SetNormalDetail(pbrNormalDetailStrength_, pbrNormalDetailScale_);
		sample.object->SetMaterialDebugMode(pbrMaterialDebugMode_);
		sample.object->SetInsensity(pbrDirectLightIntensity_);
		sample.shadowReceiveStrength = 0.0f;
		sample.castsShadow = true;
		validationObjects_.push_back(std::move(sample));
	};

	addValidationPrimitive(
		kPbrPlaneModelName,
		{ -54.0f, 7.0f, 44.0f },
		{ -0.62f, 0.0f, 0.0f },
		{ 1.0f, 1.0f, 1.0f },
		{ 0.82f, 0.84f, 0.86f, 1.0f },
		0.0f,
		0.20f);
	addValidationPrimitive(
		kPbrBoxModelName,
		{ -18.0f, 8.2f, 44.0f },
		{ 0.18f, 0.48f, 0.0f },
		{ 4.8f, 4.8f, 4.8f },
		{ 0.78f, 0.33f, 0.22f, 1.0f },
		0.0f,
		0.52f);
	addValidationPrimitive(
		kPbrCylinderModelName,
		{ 18.0f, 8.8f, 44.0f },
		{ 0.0f, 0.0f, 0.0f },
		{ 4.2f, 4.2f, 4.2f },
		{ 0.94f, 0.96f, 1.0f, 1.0f },
		1.0f,
		0.28f);
	addValidationPrimitive(
		kPbrBoxModelName,
		{ 54.0f, 8.2f, 44.0f },
		{ -0.20f, 0.76f, 0.12f },
		{ 3.8f, 3.8f, 3.8f },
		{ 0.05f, 0.06f, 0.07f, 1.0f },
		0.0f,
		0.86f);

	LabObject authoredTestBlock;
	authoredTestBlock.object = std::make_unique<Object3d>();
	authoredTestBlock.object->Initialize();
	authoredTestBlock.object->SetModel("TestBlock.obj");
	authoredTestBlock.object->SetTranslate({ 90.0f, 8.5f, 44.0f });
	authoredTestBlock.object->SetRotate({ 0.18f, 0.58f, -0.06f });
	authoredTestBlock.object->SetScale({ 0.72f, 0.72f, 0.72f });
	authoredTestBlock.object->SetLighting(true);
	authoredTestBlock.object->SetLightingMode(2);
	authoredTestBlock.object->SetEnvironmentCoefficient(0.78f);
	authoredTestBlock.object->SetEnvironmentMap(pbrEnvironmentSrv);
	authoredTestBlock.object->SetIBLIntensity(pbrIblDiffuseIntensity_, pbrIblSpecularIntensity_);
	authoredTestBlock.object->SetIBLMaxMipLevel(pbrIblMaxMipLevel_);
	authoredTestBlock.object->SetPBREnvironmentMode(pbrFilteredIblBlend_);
	authoredTestBlock.object->SetShadowReceiveStrength(enablePbrSampleShadows_ ? 1.0f : 0.0f);
	authoredTestBlock.object->SetShadowFilter(pbrShadowDepthBias_, pbrShadowSlopeBias_, pbrShadowPcfRadius_);
	authoredTestBlock.object->SetNormalDetail(pbrNormalDetailStrength_, pbrNormalDetailScale_);
	authoredTestBlock.object->SetMaterialDebugMode(pbrMaterialDebugMode_);
	authoredTestBlock.object->SetInsensity(pbrDirectLightIntensity_);
	authoredTestBlock.environment = 0.78f;
	authoredTestBlock.shininess = 32.0f;
	authoredTestBlock.shadowReceiveStrength = 0.0f;
	authoredTestBlock.castsShadow = true;
	authoredTestBlock.kind = LabObjectKind::Scene;
	validationObjects_.push_back(std::move(authoredTestBlock));
	}

	if (loadSkinnedPbrSamples_) {
		AddSkinnedLabObject(
			"Human walk.gltf",
			"resources/models/human/walk.gltf",
			{ -96.0f, -1.0f, 78.0f },
			{ 0.0f, std::numbers::pi_v<float>, 0.0f },
			{ 8.0f, 8.0f, 8.0f });
		AddSkinnedLabObject(
			"VRoid testModel_animated.glb",
			"resources/models/player/testModel_animated.glb",
			{ 0.0f, -1.0f, 82.0f },
			{ 0.0f, std::numbers::pi_v<float>, 0.0f },
			{ 18.0f, 18.0f, 18.0f });
	}

	UpdateCamera();
	UpdateDedicatedOcean();
	if (sandBed_) {
		sandBed_->Update();
	}
	river_->Update();
	for (auto& object : sceneObjects_) {
		object.object->Update();
	}
	for (auto& object : metalObjects_) {
		object.object->Update();
	}
	for (auto& object : validationObjects_) {
		object.object->Update();
	}
	for (auto& object : skinnedLabObjects_) {
		if (object.loaded && object.object) {
			object.object->Update();
		}
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
		if (!fftPaused_) {
			fftTime_ += finalDeltaTime_ * waterTimeScale_;
		}
	}

	if (loadLookDevSamples_ || loadSkinnedPbrSamples_) {
		ApplyPbrEnvironmentDebugMode();
	}

	if (!Object3dCommon::GetInstance()->GetIsDebugCamera()) {
		UpdateCamera();
	}
	debugCamera_->Update(input_->GetMouseState(), input_->GetKey(), input_->GetLeftStick());
	skybox_->Update(camera_.get(), debugCamera_.get());
	river_->SetColor(riverTint_);
	river_->SetShininess(sceneTime_);
	river_->SetInsensity(waterLightIntensity_);
	river_->SetWaterDiagnostics(
		true,
		waterSunPathEnabled_,
		waterAtmosphereEnabled_,
		waterFarFlattenEnabled_,
		waterProceduralCloudReflectionEnabled_,
		waterDebugMode_,
		waterAtmosphereStrength_,
		waterFarFlattenStrength_);
	river_->Update();
	UpdateDedicatedOcean();
	if (sandBed_) {
		sandBed_->Update();
	}

	for (auto& object : sceneObjects_) {
		object.object->SetEnvironmentCoefficient(object.environment);
		object.object->SetShininess(object.shininess);
		object.object->Update();
	}
	for (size_t i = 0; i < metalObjects_.size(); ++i) {
		auto& object = metalObjects_[i];
		if (object.animateRotation) {
			Vector3 rotate = object.object->GetRotate();
			rotate.y += finalDeltaTime_ * (0.35f + static_cast<float>(i) * 0.12f);
			object.object->SetRotate(rotate);
		}
		object.object->SetEnvironmentCoefficient(object.environment);
		object.object->SetShininess(object.shininess);
		object.object->SetIBLIntensity(pbrIblDiffuseIntensity_, pbrIblSpecularIntensity_);
		object.object->SetIBLMaxMipLevel(pbrIblMaxMipLevel_);
		object.object->SetPBREnvironmentMode(pbrFilteredIblBlend_);
		object.object->SetShadowReceiveStrength(enablePbrSampleShadows_ ? 1.0f : object.shadowReceiveStrength);
		object.object->SetShadowFilter(pbrShadowDepthBias_, pbrShadowSlopeBias_, pbrShadowPcfRadius_);
		object.object->SetNormalDetail(pbrNormalDetailStrength_, pbrNormalDetailScale_);
		object.object->SetMaterialDebugMode(pbrMaterialDebugMode_);
		object.object->SetInsensity(pbrDirectLightIntensity_);
		object.object->Update();
	}
	for (auto& object : validationObjects_) {
		object.object->SetEnvironmentCoefficient(object.environment);
		object.object->SetShininess(object.shininess);
		object.object->SetIBLIntensity(pbrIblDiffuseIntensity_, pbrIblSpecularIntensity_);
		object.object->SetIBLMaxMipLevel(pbrIblMaxMipLevel_);
		object.object->SetPBREnvironmentMode(pbrFilteredIblBlend_);
		object.object->SetShadowReceiveStrength(enablePbrSampleShadows_ ? 1.0f : object.shadowReceiveStrength);
		object.object->SetShadowFilter(pbrShadowDepthBias_, pbrShadowSlopeBias_, pbrShadowPcfRadius_);
		object.object->SetNormalDetail(pbrNormalDetailStrength_, pbrNormalDetailScale_);
		object.object->SetMaterialDebugMode(pbrMaterialDebugMode_);
		object.object->SetInsensity(pbrDirectLightIntensity_);
		object.object->Update();
	}
	for (auto& object : skinnedLabObjects_) {
		if (!object.loaded || !object.model || !object.object) {
			continue;
		}
		object.model->Update(finalDeltaTime_);
		ApplyPbrSettingsToSkinnedObject(*object.object, object.shadowReceiveStrength);
		object.object->Update();
	}

	DrawDebugWindow();
}

IScene::WaterPostProcessSettings GraphicsLabScene::GetWaterPostProcessSettings() const
{
	WaterPostProcessSettings settings{};
	settings.diagnosticsEnabled = true;
	settings.taaEnabled = waterTaaEnabled_;
	settings.bloomEnabled = waterBloomEnabled_;
	settings.historyWeight = waterHistoryWeight_;
	settings.debugMode = waterDebugMode_;
	return settings;
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
	if (showValidationPrimitives_ && enablePbrSampleShadows_) {
		for (auto& object : validationObjects_) {
			if (object.castsShadow) {
				object.object->DrawShadow();
			}
		}
	}
	if (showSkinnedPbrSamples_ && enablePbrSampleShadows_) {
		for (auto& object : skinnedLabObjects_) {
			if (object.loaded && object.castsShadow && object.object && object.model) {
				object.object->DrawSkinnedShadow(*object.model);
			}
		}
	}
}

void GraphicsLabScene::DrawPostEffect3D()
{
	skybox_->Draw();

	Object3dCommon::GetInstance()->PreDraw(kNone);
	if (showSandBed_ && sandBed_) {
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
	if (showValidationPrimitives_) {
		for (auto& object : validationObjects_) {
			object.object->Draw();
		}
	}
	if (showSkinnedPbrSamples_) {
		for (auto& object : skinnedLabObjects_) {
			if (object.loaded && object.object && object.model) {
				object.object->DrawSkinned(*object.model);
			}
		}
	}

	if (useDedicatedOceanRenderer_ && oceanRenderer_) {
		oceanRenderer_->Draw();
	} else {
		Object3dCommon::GetInstance()->PreDraw(kNormal);
		river_->Draw();
	}
}

void GraphicsLabScene::UpdateDedicatedOcean()
{
	if (!oceanRenderer_ || !camera_ || !debugCamera_) {
		return;
	}

	const int clampedMode = (std::clamp)(
		oceanMode_,
		static_cast<int>(OceanRenderer::Mode::Calm),
		static_cast<int>(OceanRenderer::Mode::ArcBlanc));
	oceanRenderer_->SetMode(static_cast<OceanRenderer::Mode>(clampedMode));
	const int clampedWaveSource = (std::clamp)(
		oceanWaveSource_,
		static_cast<int>(OceanRenderer::WaveSource::Procedural),
		static_cast<int>(OceanRenderer::WaveSource::FFTThreeCascades));
	oceanRenderer_->SetWaveSource(
		static_cast<OceanRenderer::WaveSource>(clampedWaveSource));
	oceanRenderer_->SetTint(riverTint_);
	oceanRenderer_->SetBaseHeight(-1.15f);
	oceanRenderer_->SetWind(
		oceanWindDirection_,
		oceanWindSpeed_,
		oceanChoppiness_);
	oceanRenderer_->SetSun(
		waterLightIntensity_,
		oceanSunSpecularStrength_,
		oceanArtisticSunLaneStrength_);
	oceanRenderer_->SetDiagnostics(
		waterSunPathEnabled_,
		waterAtmosphereEnabled_,
		waterFarFlattenEnabled_,
		waterProceduralCloudReflectionEnabled_,
		waterDebugMode_,
		waterAtmosphereStrength_,
		waterFarFlattenStrength_);
	oceanRenderer_->SetFFTSettings(
		fftTime_,
		fftAmplitude_,
		fftPatchLength_,
		static_cast<uint32_t>((std::max)(fftSeed_, 0)),
		fftPaused_,
		fftDebugMode_,
		fftDebugDisplayScale_);
	const int clampedSpectrumModel = (std::clamp)(
		fftSpectrumModel_,
		static_cast<int>(OceanRenderer::SpectrumModel::Phillips),
		static_cast<int>(
			OceanRenderer::SpectrumModel::JonswapDonelanBanner));
	oceanRenderer_->SetFFTSpectrumSettings(
		static_cast<OceanRenderer::SpectrumModel>(clampedSpectrumModel),
		fftFetch_,
		fftGamma_,
		fftLowFrequencyDamping_,
		fftHighFrequencyDamping_,
		fftSwellDirection_,
		fftSwellAmount_,
		fftOppositeWaveSuppression_);
	const int clampedBandMode = (std::clamp)(
		fftCascadeBandMode_,
		static_cast<int>(OceanRenderer::CascadeBandMode::HardCutoff),
		static_cast<int>(
			OceanRenderer::CascadeBandMode::SmoothTransition));
	oceanRenderer_->SetFFTCascadeSettings(
		static_cast<OceanRenderer::CascadeBandMode>(clampedBandMode),
		fftCascadeTransitionWidth_,
		fftCascadeSettings_,
		fftCascadeDisplayMode_,
		fftDebugCascadeIndex_);
	oceanRenderer_->Update(
		sceneTime_,
		*camera_,
		*debugCamera_,
		Object3dCommon::GetInstance()->GetIsDebugCamera());
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

void GraphicsLabScene::AddSkinnedLabObject(
	const std::string& label,
	const std::string& path,
	const Vector3& translate,
	const Vector3& rotate,
	const Vector3& scale)
{
	SkinnedLabObject sample;
	sample.label = label;
	sample.path = path;
	sample.status = label + ": not loaded";
	sample.shadowReceiveStrength = 0.35f;

	try {
		sample.model = std::make_unique<SkinnedModel>();
		sample.model->Initialize(
			Object3dCommon::GetInstance()->GetDxCommon(),
			Object3dCommon::GetInstance()->GetSrvManager(),
			path);

		sample.object = std::make_unique<Object3d>();
		sample.object->Initialize();
		sample.object->SetTranslate(translate);
		sample.object->SetRotate(rotate);
		sample.object->SetScale(scale);
		ApplyPbrSettingsToSkinnedObject(*sample.object, sample.shadowReceiveStrength);
		sample.object->Update();

		const auto& asset = sample.model->GetAsset();
		sample.loaded = true;
		sample.status =
			label + ": loaded / materials=" + std::to_string(asset.modelData.materials.size()) +
			" submeshes=" + std::to_string(asset.modelData.submeshes.size()) +
			" animations=" + std::to_string(sample.model->GetAnimations().size());
	} catch (const std::exception& error) {
		sample.loaded = false;
		sample.status = label + ": failed / " + error.what();
	}

	skinnedLabObjects_.push_back(std::move(sample));
}

void GraphicsLabScene::ApplyPbrSettingsToSkinnedObject(Object3d& object, float shadowReceiveStrength)
{
	const bool usePbrLighting = skinnedShadingMode_ == 0 && enableSkinnedPbrLighting_;
	const bool useCharacterLighting = skinnedShadingMode_ == 1;
	const bool useLighting = usePbrLighting || useCharacterLighting;
	object.SetLighting(useLighting);
	object.SetLightingMode(useCharacterLighting ? 3 : 2);
	object.SetEnvironmentCoefficient(usePbrLighting ? 0.78f : 0.0f);
	object.SetEnvironmentMap(pbrEnvironment_.GetEnvironmentSrvIndex());
	object.SetIBLIntensity(
		usePbrLighting ? pbrIblDiffuseIntensity_ : 0.0f,
		usePbrLighting ? pbrIblSpecularIntensity_ : 0.0f);
	object.SetIBLMaxMipLevel(pbrIblMaxMipLevel_);
	object.SetPBREnvironmentMode(usePbrLighting ? pbrFilteredIblBlend_ : 0.0f);
	object.SetShadowReceiveStrength(useLighting && enablePbrSampleShadows_ ? shadowReceiveStrength : 0.0f);
	object.SetShadowFilter(pbrShadowDepthBias_, pbrShadowSlopeBias_, pbrShadowPcfRadius_);
	object.SetNormalDetail(usePbrLighting ? pbrNormalDetailStrength_ : 0.0f, pbrNormalDetailScale_);
	object.SetMaterialDebugMode(pbrMaterialDebugMode_);
	object.SetInsensity(usePbrLighting ? pbrDirectLightIntensity_ : (useCharacterLighting ? 0.92f : 1.0f));
	object.SetCharacterShading(
		characterLightWrap_,
		characterShadowSoftness_,
		characterShadowStrength_,
		characterRimStrength_,
		characterRimPower_,
		characterSpecularStrength_,
		characterSpecularPower_);
}

void GraphicsLabScene::ApplyPbrEnvironmentDebugMode()
{
	if (appliedPbrEnvironmentDebugMode_ == pbrEnvironmentDebugMode_) {
		return;
	}

	PbrEnvironment::Settings environmentSettings{};
	switch (pbrEnvironmentDebugMode_) {
	case 1:
		environmentSettings.sourceType = PbrEnvironment::SourceType::TextureFile;
		environmentSettings.texturePath = "resources/skybox.dds";
		break;
	case 2:
		environmentSettings.sourceType = PbrEnvironment::SourceType::SolidColor;
		environmentSettings.solidColor = { 0.74f, 0.78f, 0.82f };
		break;
	case 3:
		environmentSettings.sourceType = PbrEnvironment::SourceType::SolidColor;
		environmentSettings.solidColor = { 1.0f, 1.0f, 1.0f };
		break;
	case 0:
	default:
		environmentSettings.sourceType = PbrEnvironment::SourceType::ProceduralSky;
		pbrEnvironmentDebugMode_ = 0;
		break;
	}

	pbrEnvironment_.Apply(environmentSettings);
	appliedPbrEnvironmentDebugMode_ = pbrEnvironmentDebugMode_;
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
	ImGui::Text("Arc Blanc style ocean look-dev scene");
	ImGui::Text("A,D: Orbit  W,S: Pitch  Mouse wheel: Zoom  Esc: Title");
	ImGui::Text("Shift+D: Debug camera  MMB: Orbit  Shift+MMB: Pan  Wheel: Zoom");
	ImGui::Checkbox("Pause water", &pauseWater_);
	if (ImGui::CollapsingHeader("Water isolation", ImGuiTreeNodeFlags_DefaultOpen)) {
		const char* oceanRenderers[] = {
			"Legacy Object3d ocean",
			"Dedicated OceanRenderer"
		};
		int rendererMode = useDedicatedOceanRenderer_ ? 1 : 0;
		if (ImGui::Combo(
			"Ocean renderer",
			&rendererMode,
			oceanRenderers,
			IM_ARRAYSIZE(oceanRenderers))) {
			useDedicatedOceanRenderer_ = rendererMode == 1;
		}
		const char* waveSources[] = {
			"Procedural",
			"FFT Single Cascade",
			"FFT Three Cascades"
		};
		ImGui::Combo(
			"Wave source",
			&oceanWaveSource_,
			waveSources,
			IM_ARRAYSIZE(waveSources));
		ImGui::Checkbox("FFT pause", &fftPaused_);
		const char* oceanModes[] = { "Calm", "Naval", "Arc Blanc" };
		ImGui::Combo(
			"Ocean mode",
			&oceanMode_,
			oceanModes,
			IM_ARRAYSIZE(oceanModes));
		ImGui::Checkbox("Water TAA", &waterTaaEnabled_);
		ImGui::SameLine();
		ImGui::Checkbox("Water bloom", &waterBloomEnabled_);
		ImGui::Checkbox("Water sun path", &waterSunPathEnabled_);
		ImGui::Checkbox("Water atmosphere blend", &waterAtmosphereEnabled_);
		ImGui::Checkbox("Water far flatten", &waterFarFlattenEnabled_);
		ImGui::Checkbox(
			"Water procedural cloud reflection",
			&waterProceduralCloudReflectionEnabled_);
		ImGui::DragFloat(
			"Water TAA history weight",
			&waterHistoryWeight_,
			0.005f,
			0.0f,
			0.10f,
			"%.3f");
		ImGui::DragFloat(
			"Water horizon atmosphere strength",
			&waterAtmosphereStrength_,
			0.02f,
			0.0f,
			2.0f);
		ImGui::DragFloat(
			"Water far flatten strength",
			&waterFarFlattenStrength_,
			0.02f,
			0.0f,
			1.0f);
		ImGui::DragFloat2(
			"Main wind direction (unit)",
			&oceanWindDirection_.x,
			0.01f,
			-1.0f,
			1.0f);
		ImGui::DragFloat(
			"Ocean wind speed",
			&oceanWindSpeed_,
			0.10f,
			0.0f,
			40.0f,
			"%.2f m/s");
		ImGui::DragFloat(
			"Ocean choppiness",
			&oceanChoppiness_,
			0.02f,
			0.0f,
			8.0f);
		const char* fftSpectrumModels[] = {
			"Phillips",
			"JONSWAP + Donelan-Banner"
		};
		ImGui::Combo(
			"FFT spectrum",
			&fftSpectrumModel_,
			fftSpectrumModels,
			IM_ARRAYSIZE(fftSpectrumModels));
		ImGui::DragFloat(
			"FFT energy scale (unitless)",
			&fftAmplitude_,
			0.01f,
			0.0f,
			8.0f,
			"%.3f");
		ImGui::DragFloat(
			"FFT patch length",
			&fftPatchLength_,
			1.0f,
			32.0f,
			2048.0f,
			"%.0f m");
		ImGui::InputInt("FFT seed", &fftSeed_);
		const char* cascadeDisplays[] = {
			"Combined",
			"Large",
			"Medium",
			"Small"
		};
		ImGui::Combo(
			"Cascade display",
			&fftCascadeDisplayMode_,
			cascadeDisplays,
			IM_ARRAYSIZE(cascadeDisplays));
		const char* bandModes[] = {
			"Arc Blanc exact hard cutoff",
			"Energy-conserving smooth transition"
		};
		ImGui::Combo(
			"Cascade band mode",
			&fftCascadeBandMode_,
			bandModes,
			IM_ARRAYSIZE(bandModes));
		ImGui::DragFloat(
			"Band transition width",
			&fftCascadeTransitionWidth_,
			0.01f,
			0.001f,
			8.0f,
			"%.3f rad/m");
		const char* cascadeNames[] = { "Large", "Medium", "Small" };
		for (uint32_t cascadeIndex = 0;
			cascadeIndex < OceanRenderer::kFFTCascadeCount;
			++cascadeIndex) {
			ImGui::PushID(static_cast<int>(cascadeIndex));
			ImGui::Text("%s cascade", cascadeNames[cascadeIndex]);
			auto& cascade = fftCascadeSettings_[cascadeIndex];
			ImGui::Checkbox("Enabled", &cascade.enabled);
			ImGui::DragFloat(
				"Patch length",
				&cascade.patchLength,
				1.0f,
				1.0f,
				2048.0f,
				"%.1f m");
			ImGui::DragFloat(
				"k min",
				&cascade.minimumWaveNumber,
				0.01f,
				0.0f,
				128.0f,
				"%.4f rad/m");
			ImGui::DragFloat(
				"k max",
				&cascade.maximumWaveNumber,
				0.01f,
				0.0f,
				160.0f,
				"%.4f rad/m");
			ImGui::DragFloat(
				"Displacement contribution",
				&cascade.displacementContribution,
				0.01f,
				0.0f,
				4.0f);
			ImGui::DragFloat(
				"Slope contribution",
				&cascade.slopeContribution,
				0.01f,
				0.0f,
				4.0f);
			ImGui::PopID();
		}
		ImGui::DragFloat(
			"JONSWAP fetch",
			&fftFetch_,
			1000.0f,
			1.0f,
			10000000.0f,
			"%.0f m");
		ImGui::DragFloat(
			"JONSWAP gamma (unitless)",
			&fftGamma_,
			0.01f,
			1.0f,
			10.0f,
			"%.2f");
		ImGui::DragFloat(
			"Low frequency damping (unitless)",
			&fftLowFrequencyDamping_,
			0.002f,
			0.0f,
			10.0f,
			"%.3f");
		ImGui::DragFloat(
			"High frequency damping (unitless)",
			&fftHighFrequencyDamping_,
			0.002f,
			0.0f,
			10.0f,
			"%.3f");
		ImGui::DragFloat2(
			"Swell direction (unit)",
			&fftSwellDirection_.x,
			0.01f,
			-1.0f,
			1.0f);
		ImGui::DragFloat(
			"Swell amount",
			&fftSwellAmount_,
			0.01f,
			0.0f,
			1.0f,
			"%.2f");
		ImGui::DragFloat(
			"Opposite-wave suppression",
			&fftOppositeWaveSuppression_,
			0.01f,
			0.0f,
			1.0f,
			"%.2f");
		const char* fftDebugModes[] = {
			"None",
			"Radial spectrum",
			"Directional spectrum",
			"Band weight",
			"Initial complex spectrum",
			"Evolved spectrum",
			"Final height",
			"Displacement X",
			"Displacement Z",
			"Final slope",
			"Final normal"
		};
		const char* fftDebugCascades[] = {
			"Large",
			"Medium",
			"Small"
		};
		ImGui::Combo(
			"Debug cascade",
			&fftDebugCascadeIndex_,
			fftDebugCascades,
			IM_ARRAYSIZE(fftDebugCascades));
		ImGui::Combo(
			"Debug texture",
			&fftDebugMode_,
			fftDebugModes,
			IM_ARRAYSIZE(fftDebugModes));
		ImGui::DragFloat(
			"Debug display scale",
			&fftDebugDisplayScale_,
			0.05f,
			0.01f,
			64.0f);
		if (oceanRenderer_) {
			if (ImGui::Button("Run FFT diagnostics") &&
				!oceanRenderer_->IsFFTDiagnosticsPending()) {
				oceanRenderer_->RequestFFTDiagnostics();
			}
			if (oceanRenderer_->IsFFTDiagnosticsPending()) {
				ImGui::SameLine();
				ImGui::TextUnformatted("Pending...");
			}
			const auto& diagnostics = oceanRenderer_->GetFFTDiagnostics();
			if (diagnostics.valid) {
				if (diagnostics.threeCascades) {
					const char* cascadeNames[] = {
						"Large",
						"Medium",
						"Small"
					};
					bool numericalPass = true;
					for (uint32_t cascadeIndex = 0;
						cascadeIndex < OceanRenderer::kFFTCascadeCount;
						++cascadeIndex) {
						const auto& cascade =
							diagnostics.cascades[cascadeIndex];
						if (!cascade.valid) {
							continue;
						}
						ImGui::Text(
							"%s target/GPU variance: %.6e / %.6e m^2",
							cascadeNames[cascadeIndex],
							cascade.targetSpectrumVariance,
							cascade.rgba16fVariance);
						ImGui::Text(
							"  RMS/Hs: %.6f / %.6f m   mean variance: %.6e",
							cascade.heightRms,
							cascade.significantWaveHeight,
							cascade.seedMeanVariance);
						ImGui::Text(
							"  Hermitian/imaginary: %.3e / %.3e   NaN/Inf: %u",
							cascade.hermitianSymmetryError,
							cascade.ifftImaginaryResidual,
							cascade.invalidValueCount);
						ImGui::Text(
							"  RGBA16F error: %.4f%%   GPU: %.3f ms",
							cascade.rgba16fQuantizationError * 100.0f,
							cascade.gpuTimeMilliseconds);
						numericalPass =
							numericalPass &&
							cascade.invalidValueCount == 0 &&
							cascade.hermitianSymmetryError <= 1.0e-4f &&
							cascade.ifftImaginaryResidual <= 1.0e-4f;
					}
					const float meanVarianceError =
						diagnostics.combinedTargetVariance > 1.0e-12f
						? std::abs(
							diagnostics.combinedSeedMeanVariance -
							diagnostics.combinedTargetVariance) /
							diagnostics.combinedTargetVariance
						: 0.0f;
					ImGui::Text(
						"Combined target/GPU variance: %.6e / %.6e m^2",
						diagnostics.combinedTargetVariance,
						diagnostics.combinedGpuVariance);
					ImGui::Text(
						"Combined RMS/Hs: %.6f / %.6f m",
						diagnostics.combinedHeightRms,
						diagnostics.combinedSignificantWaveHeight);
					ImGui::Text(
						"32-seed mean variance: %.6e   error: %.3f%%",
						diagnostics.combinedSeedMeanVariance,
						meanVarianceError * 100.0f);
					ImGui::Text(
						"Single target / partition error: %.6e / %.3f%%",
						diagnostics.singleReferenceTargetVariance,
						diagnostics.energyPartitionError * 100.0f);
					ImGui::Text(
						"Band overlap/missing energy: %.3e / %.3e m^2",
						diagnostics.bandOverlapEnergy,
						diagnostics.bandMissingEnergy);
					ImGui::Text(
						"Current GPU vs Single: %.3f%%   total GPU: %.3f ms",
						diagnostics.combinedVarianceRelativeError * 100.0f,
						diagnostics.totalGpuTimeMilliseconds);
					const bool variancePass =
						diagnostics.energyPartitionError <= 0.05f &&
						meanVarianceError <= 0.05f;
					ImGui::Text(
						"Acceptance: partition %s   numerical %s",
						variancePass ? "PASS" : "FAIL",
						numericalPass ? "PASS" : "FAIL");
				} else {
				ImGui::Text(
					"Height RMS: %.6f m   Hs: %.6f m",
					diagnostics.heightRms,
					diagnostics.significantWaveHeight);
				ImGui::Text(
					"Height min/max: %.6f / %.6f m",
					diagnostics.heightMinimum,
					diagnostics.heightMaximum);
				ImGui::Text(
					"Variance target / legacy 0.5: %.6e / %.6e m^2",
					diagnostics.targetSpectrumVariance,
					diagnostics.legacyCoefficientVariance);
				ImGui::Text(
					"Variance h0 predicted / evolved: %.6e / %.6e m^2",
					diagnostics.h0PredictedVariance,
					diagnostics.evolvedParsevalVariance);
				ImGui::Text(
					"Variance IFFT float / RGBA16F: %.6e / %.6e m^2",
					diagnostics.ifftFloatVariance,
					diagnostics.rgba16fVariance);
				ImGui::Text(
					"Gaussian E[r^2]/E[i^2]/E[|xi|^2]: %.4f / %.4f / %.4f",
					diagnostics.gaussianRealSquared,
					diagnostics.gaussianImaginarySquared,
					diagnostics.gaussianMagnitudeSquared);
				ImGui::Text(
					"Mean |h0|^2 / |h(t)|^2: %.6e / %.6e",
					diagnostics.h0MagnitudeSquared,
					diagnostics.evolvedMagnitudeSquared);
				ImGui::Text(
					"Gaussian samples: %u   self-conjugate bins: %u",
					diagnostics.gaussianSampleCount,
					diagnostics.selfConjugateBinCount);
				ImGui::Text(
					"Hermitian error: %.3e",
					diagnostics.hermitianSymmetryError);
				ImGui::Text(
					"IFFT imaginary RMS: %.3e m",
					diagnostics.ifftImaginaryResidual);
				ImGui::Text(
					"Directional norm error: %.3e",
					diagnostics.directionalNormalizationError);
				ImGui::Text(
					"Expected RMS N=64/128/256: %.5f / %.5f / %.5f m",
					diagnostics.expectedRms64,
					diagnostics.expectedRms128,
					diagnostics.expectedRms256);
				ImGui::Text(
					"Resolution spread: %.3f%%",
					diagnostics.resolutionRelativeSpread * 100.0f);
				ImGui::Text(
					"32-seed RMS mean/stddev: %.6f / %.6f m",
					diagnostics.seedRmsMean,
					diagnostics.seedRmsStandardDeviation);
				ImGui::Text(
					"32-seed RMS min/max: %.6f / %.6f m",
					diagnostics.seedRmsMinimum,
					diagnostics.seedRmsMaximum);
				ImGui::Text(
					"Mean-vs-physical / mean absolute error: %.3f%% / %.3f%%",
					diagnostics.seedMeanRelativeError * 100.0f,
					diagnostics.seedMeanAbsoluteRelativeError * 100.0f);
				ImGui::Text(
					"JONSWAP alpha: %.6f   peak omega: %.4f rad/s",
					diagnostics.jonswapAlpha,
					diagnostics.peakAngularFrequency);
				ImGui::Text(
					"NaN/Inf values: %u   sample time: %.3f s",
					diagnostics.invalidValueCount,
					diagnostics.sampleTime);
				const bool variancePass =
					diagnostics.seedMeanRelativeError <= 0.05f;
				const bool numericalPass =
					diagnostics.invalidValueCount == 0 &&
					diagnostics.hermitianSymmetryError <= 1.0e-4f &&
					diagnostics.ifftImaginaryResidual <= 1.0e-4f &&
					diagnostics.directionalNormalizationError <= 1.0e-3f &&
					diagnostics.resolutionRelativeSpread <= 0.01f;
				ImGui::Text(
					"Acceptance: variance %s   numerical %s",
					variancePass ? "PASS" : "FAIL",
					numericalPass ? "PASS" : "FAIL");
				}
			}
		}
		ImGui::DragFloat(
			"Physical GGX sun specular",
			&oceanSunSpecularStrength_,
			0.01f,
			0.0f,
			4.0f);
		ImGui::DragFloat(
			"Artistic sun lane",
			&oceanArtisticSunLaneStrength_,
			0.01f,
			0.0f,
			4.0f);
		const char* waterDebugModes[] = {
			"Final",
			"Normal",
			"Fresnel",
			"Sun Specular",
			"Foam",
			"TAA Reactive Mask"
		};
		ImGui::Combo(
			"Water debug view",
			&waterDebugMode_,
			waterDebugModes,
			IM_ARRAYSIZE(waterDebugModes));
	}
	ImGui::Checkbox("Show underwater sand", &showSandBed_);
	ImGui::Checkbox("Show beach", &showBeach_);
	ImGui::Checkbox("Show obstacles", &showObstacles_);
	ImGui::Checkbox("Show PBR samples", &showPbrSamples_);
	ImGui::Checkbox("Show validation primitives", &showValidationPrimitives_);
	if (skinnedLabObjects_.empty()) {
		ImGui::Text("Skinned PBR samples: skipped for faster ocean iteration.");
	} else {
		ImGui::Checkbox("Show skinned PBR samples", &showSkinnedPbrSamples_);
	}
	const char* skinnedShadingModes[] = { "PBR look-dev", "Character bridge", "Unlit texture reference" };
	ImGui::Combo("Skinned shading", &skinnedShadingMode_, skinnedShadingModes, IM_ARRAYSIZE(skinnedShadingModes));
	ImGui::Checkbox("Skinned PBR lighting", &enableSkinnedPbrLighting_);
	ImGui::DragFloat("Character light wrap", &characterLightWrap_, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("Character shadow softness", &characterShadowSoftness_, 0.01f, 0.001f, 0.6f);
	ImGui::DragFloat("Character shadow strength", &characterShadowStrength_, 0.01f, 0.0f, 0.95f);
	ImGui::DragFloat("Character rim strength", &characterRimStrength_, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("Character rim power", &characterRimPower_, 0.05f, 0.25f, 10.0f);
	ImGui::DragFloat("Character specular strength", &characterSpecularStrength_, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("Character specular power", &characterSpecularPower_, 0.5f, 1.0f, 128.0f);
	ImGui::Text("PBR grid: front row dielectrics, back row metals; roughness increases left to right.");
	ImGui::Text("Validation primitives: tilted plane, hard-edge boxes, and a cylinder.");
	if (ImGui::CollapsingHeader("Skinned PBR samples")) {
		for (const auto& object : skinnedLabObjects_) {
			ImGui::TextWrapped("%s", object.status.c_str());
		}
	}
	const char* pbrMaterialDebugModes[] = {
		"Final",
		"World normal",
		"Tangent normal",
		"Albedo",
		"Roughness",
		"Metallic",
		"Ambient occlusion",
		"F0",
		"SSR mask",
		"Packed raw",
		"UV"
	};
	ImGui::Combo("PBR material debug", &pbrMaterialDebugMode_, pbrMaterialDebugModes, IM_ARRAYSIZE(pbrMaterialDebugModes));
	const char* pbrEnvironmentModes[] = { "Clean sky", "Skybox DDS", "Neutral gray", "Neutral white" };
	ImGui::Combo("PBR env debug", &pbrEnvironmentDebugMode_, pbrEnvironmentModes, IM_ARRAYSIZE(pbrEnvironmentModes));
	ImGui::Text("PBR IBL source: %s", pbrEnvironment_.GetSourceLabel().c_str());
	ImGui::DragFloat("PBR filtered IBL blend", &pbrFilteredIblBlend_, 0.01f, 0.0f, 1.0f);
	ImGui::Checkbox("PBR sample shadows", &enablePbrSampleShadows_);
	ImGui::DragFloat("PBR shadow depth bias", &pbrShadowDepthBias_, 0.00001f, 0.0f, 0.01f, "%.5f");
	ImGui::DragFloat("PBR shadow slope bias", &pbrShadowSlopeBias_, 0.00005f, 0.0f, 0.02f, "%.5f");
	ImGui::DragFloat("PBR shadow PCF radius", &pbrShadowPcfRadius_, 0.05f, 0.0f, 4.0f);
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
	if (ImGui::CollapsingHeader("Loaded model material debug")) {
		const auto& models = ModelManager::GetInstance()->GetModels();
		ImGui::Text("loaded models: %zu", models.size());
		for (const auto& [modelName, model] : models) {
			if (model) {
				DrawMaterialTextureDebug(modelName, model->GetModelData());
			}
		}
		if (!skinnedLabObjects_.empty()) {
			ImGui::Separator();
			ImGui::TextUnformatted("Skinned");
			for (const auto& object : skinnedLabObjects_) {
				if (object.loaded && object.model) {
					DrawMaterialTextureDebug("Skinned: " + object.label, object.model->GetAsset().modelData);
				} else {
					ImGui::TextWrapped("%s", object.status.c_str());
				}
			}
		}
	}
	ImGui::End();
#endif
}
