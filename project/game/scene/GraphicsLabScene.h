#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Camera.h"
#include "DebugCamera.h"
#include "Input.h"
#include "IScene.h"
#include "Object3d.h"
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

	Input* input_ = nullptr;
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<DebugCamera> debugCamera_;
	std::unique_ptr<Skybox> skybox_;
	std::unique_ptr<Object3d> river_;
	std::unique_ptr<Object3d> sandBed_;
	std::vector<LabObject> metalObjects_;
	std::vector<LabObject> sceneObjects_;

	bool finished_ = false;
	std::string nextSceneName_ = "TITLE";
	float finalDeltaTime_ = 1.0f / 60.0f;
	float sceneTime_ = 0.0f;
	bool pauseWater_ = false;
	bool showSandBed_ = true;
	bool showBeach_ = true;
	bool showObstacles_ = true;
	bool showPbrSamples_ = true;
	bool usePbrProceduralEnvironment_ = true;
	bool enablePbrSampleShadows_ = false;
	float waterTimeScale_ = 1.0f;
	float waterLightIntensity_ = 2.2f;
	float pbrDirectLightIntensity_ = 2.4f;
	float pbrIblDiffuseIntensity_ = 0.62f;
	float pbrIblSpecularIntensity_ = 1.35f;
	float pbrIblMaxMipLevel_ = 7.0f;
	float pbrNormalDetailStrength_ = 0.0f;
	float pbrNormalDetailScale_ = 24.0f;
	Vector4 riverTint_ = { 0.88f, 0.98f, 1.0f, 0.84f };
	float cameraYaw_ = 0.18f;
	float cameraPitch_ = 0.20f;
	float cameraDistance_ = 125.0f;
	Vector3 cameraTarget_ = { 0.0f, 2.0f, 26.0f };
};
