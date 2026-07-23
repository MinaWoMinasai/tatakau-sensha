#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Camera.h"
#include "DebugCamera.h"
#include "Input.h"
#include "IScene.h"
#include "Object3d.h"
#include "PbrEnvironment.h"
#include "SkinCluster.h"
#include "Skybox.h"

class GraphicsLabScene : public IScene {
public:
	void Initialize() override;
	void Update() override;
	void Draw() override {}
	void DrawShadow() override;
	void DrawPostEffect3D() override;
	void DrawSprite() override {}

	bool IsFinished() const override { return finished_; }
	float GetFinalDeltaTime() const override { return finalDeltaTime_; }
	std::string GetNextSceneName() const override { return nextSceneName_; }

private:
	enum class LabObjectKind {
		Scene,
		Beach,
		Obstacle,
	};

	struct LabObject {
		std::unique_ptr<Object3d> object;
		float environment = 0.0f;
		float shininess = 32.0f;
		float shadowReceiveStrength = 1.0f;
		bool castsShadow = true;
		bool animateRotation = false;
		LabObjectKind kind = LabObjectKind::Scene;
	};

	struct SkinnedLabObject {
		std::unique_ptr<SkinnedModel> model;
		std::unique_ptr<Object3d> object;
		std::string label;
		std::string path;
		std::string status;
		float shadowReceiveStrength = 0.0f;
		bool loaded = false;
		bool castsShadow = true;
	};

	LabObject MakeObject(
		const std::string& modelPath,
		const Vector3& translate,
		const Vector3& rotate,
		const Vector3& scale,
		const Vector4& color,
		bool lighting,
		float environment,
		float shininess,
		LabObjectKind kind);
	void UpdateCamera();
	void DrawDebugWindow();
	bool ShouldDrawLabObject(const LabObject& object) const;
	void ApplyPbrEnvironmentDebugMode();
	void AddSkinnedLabObject(
		const std::string& label,
		const std::string& path,
		const Vector3& translate,
		const Vector3& rotate,
		const Vector3& scale);
	void ApplyPbrSettingsToSkinnedObject(Object3d& object, float shadowReceiveStrength);

	Input* input_ = nullptr;
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<DebugCamera> debugCamera_;
	std::unique_ptr<Skybox> skybox_;
	std::unique_ptr<Object3d> river_;
	std::unique_ptr<Object3d> sandBed_;
	PbrEnvironment pbrEnvironment_;
	std::vector<LabObject> metalObjects_;
	std::vector<LabObject> validationObjects_;
	std::vector<LabObject> sceneObjects_;
	std::vector<SkinnedLabObject> skinnedLabObjects_;

	bool finished_ = false;
	std::string nextSceneName_ = "TITLE";
	float finalDeltaTime_ = 1.0f / 60.0f;
	float sceneTime_ = 0.0f;
	bool pauseWater_ = false;
	bool showSandBed_ = false;
	bool showBeach_ = false;
	bool showObstacles_ = false;
	bool showPbrSamples_ = false;
	bool showValidationPrimitives_ = false;
	bool showSkinnedPbrSamples_ = false;
	bool loadSkinnedPbrSamples_ = false;
	bool loadLookDevSamples_ = false;
	bool enableSkinnedPbrLighting_ = true;
	int skinnedShadingMode_ = 1;
	float characterLightWrap_ = 0.28f;
	float characterShadowSoftness_ = 0.16f;
	float characterShadowStrength_ = 0.54f;
	float characterRimStrength_ = 0.12f;
	float characterRimPower_ = 3.2f;
	float characterSpecularStrength_ = 0.075f;
	float characterSpecularPower_ = 42.0f;
	bool enablePbrSampleShadows_ = false;
	int pbrEnvironmentDebugMode_ = 0;
	int appliedPbrEnvironmentDebugMode_ = -1;
	int pbrMaterialDebugMode_ = 0;
	float waterTimeScale_ = 1.0f;
	float waterLightIntensity_ = 2.75f;
	float pbrDirectLightIntensity_ = 2.4f;
	float pbrIblDiffuseIntensity_ = 0.62f;
	float pbrIblSpecularIntensity_ = 1.35f;
	float pbrIblMaxMipLevel_ = 7.0f;
	float pbrFilteredIblBlend_ = 1.0f;
	float pbrNormalDetailStrength_ = 0.0f;
	float pbrNormalDetailScale_ = 24.0f;
	float pbrShadowDepthBias_ = 0.00035f;
	float pbrShadowSlopeBias_ = 0.0018f;
	float pbrShadowPcfRadius_ = 1.0f;
	Vector4 riverTint_ = { 0.72f, 0.82f, 0.92f, 0.98f };
	float cameraYaw_ = 0.02f;
	float cameraPitch_ = 0.13f;
	float cameraDistance_ = 112.0f;
	Vector3 cameraTarget_ = { 0.0f, 1.0f, 180.0f };
};
