#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "Camera.h"
#include "DebugCamera.h"
#include "Input.h"
#include "IScene.h"
#include "Object3d.h"
#include "OceanRenderer.h"
#include "PbrEnvironment.h"
#include "ProceduralFlameRenderer.h"
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
	WaterPostProcessSettings GetWaterPostProcessSettings() const override;

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
	void UpdateDedicatedOcean();

	Input* input_ = nullptr;
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<DebugCamera> debugCamera_;
	std::unique_ptr<Skybox> skybox_;
	std::unique_ptr<Object3d> river_;
	std::unique_ptr<OceanRenderer> oceanRenderer_;
	std::unique_ptr<Object3d> sandBed_;
	std::unique_ptr<Object3d> crystal_;
	std::unique_ptr<ProceduralFlameRenderer> proceduralFlame_;
	PbrEnvironment pbrEnvironment_;
	std::vector<LabObject> metalObjects_;
	std::vector<LabObject> validationObjects_;
	std::vector<LabObject> sceneObjects_;
	std::vector<SkinnedLabObject> skinnedLabObjects_;

	bool finished_ = false;
	std::string nextSceneName_ = "TITLE";
	float finalDeltaTime_ = 1.0f / 60.0f;
	float sceneTime_ = 0.0f;
	float flameTime_ = 0.0f;
	bool showProceduralFlame_ = true;
	bool pauseProceduralFlame_ = false;
	bool proceduralFlameDarkBackground_ = true;
	Vector3 proceduralFlamePosition_ = { -13.0f, 1.0f, 174.0f };
	Vector2 proceduralFlameSize_ = { 11.0f, 22.0f };
	ProceduralFlameRenderer::Parameters proceduralFlameParameters_{};
	bool pauseWater_ = false;
	bool useDedicatedOceanRenderer_ = true;
	int oceanMode_ = static_cast<int>(OceanRenderer::Mode::ArcBlanc);
	int oceanWaveSource_ =
		static_cast<int>(OceanRenderer::WaveSource::FFTThreeCascades);
	int oceanMeshMode_ =
		static_cast<int>(OceanRenderer::MeshMode::ProjectedGrid);
	int oceanProjectedGridResolution_ = 256;
	float oceanProjectedNearClamp_ = 3.0f;
	float oceanProjectedFarClamp_ = 1800.0f;
	float oceanProjectedOverscanX_ = 1.10f;
	float oceanProjectedOverscanTop_ = 1.05f;
	float oceanProjectedOverscanBottom_ = 1.15f;
	bool oceanNearDisplacementFadeEnabled_ = true;
	float oceanNearFadeWidth_ = 5.0f;
	float oceanMinimumSafeNearDistance_ = 3.0f;
	int oceanProjectedGridDebugMode_ =
		static_cast<int>(OceanRenderer::ProjectedGridDebugMode::None);
	bool oceanProjectedWireframe_ = false;
	bool oceanProjectedOuterWireframe_ = false;
	bool waterTaaEnabled_ = true;
	bool waterBloomEnabled_ = true;
	bool waterSunPathEnabled_ = true;
	bool waterAtmosphereEnabled_ = true;
	bool waterFarFlattenEnabled_ = true;
	bool waterProceduralCloudReflectionEnabled_ = false;
	int waterDebugMode_ = 0;
	float waterHistoryWeight_ = 0.08f;
	float waterAtmosphereStrength_ = 1.0f;
	float waterFarFlattenStrength_ = 1.0f;
	Vector2 oceanWindDirection_ = { 0.18f, 0.98f };
	float oceanWindSpeed_ = 12.0f;
	float oceanChoppiness_ = 3.10f;
	int oceanNormalMode_ = 1;
	bool oceanBreakingPreviewEnabled_ = false;
	float oceanJacobianThreshold_ = 0.72f;
	float oceanJacobianBias_ = 0.0f;
	float oceanBreakingSmoothWidth_ = 0.12f;
	float oceanBreakingMaskIntensity_ = 0.25f;
	float oceanSunSpecularStrength_ = 0.70f;
	float oceanArtisticSunLaneStrength_ = 1.0f;
	bool fftPaused_ = false;
	float fftTime_ = 0.0f;
	float fftAmplitude_ = 1.0f;
	float fftPatchLength_ = 256.0f;
	int fftSeed_ = 1337;
	int fftSpectrumModel_ =
		static_cast<int>(
			OceanRenderer::SpectrumModel::JonswapDonelanBanner);
	float fftFetch_ = 100000.0f;
	float fftGamma_ = 3.3f;
	float fftLowFrequencyDamping_ = 0.0f;
	float fftHighFrequencyDamping_ = 0.01f;
	Vector2 fftSwellDirection_ = { 0.60f, 0.80f };
	float fftSwellAmount_ = 0.20f;
	float fftOppositeWaveSuppression_ = 0.85f;
	int fftCascadeBandMode_ =
		static_cast<int>(OceanRenderer::CascadeBandMode::HardCutoff);
	float fftCascadeTransitionWidth_ = 0.25f;
	int fftCascadeDisplayMode_ = 0;
	int fftDebugCascadeIndex_ = 0;
	std::array<
		OceanRenderer::OceanCascadeSettings,
		OceanRenderer::kFFTCascadeCount> fftCascadeSettings_ = {
			OceanRenderer::OceanCascadeSettings{
				true,
				256.0f,
				0.0f,
				2.35619449f,
				1.0f,
				1.0f },
			OceanRenderer::OceanCascadeSettings{
				true,
				16.0f,
				2.35619449f,
				9.42477796f,
				1.0f,
				1.0f },
			OceanRenderer::OceanCascadeSettings{
				true,
				4.0f,
				9.42477796f,
				142.1722540f,
				0.0f,
				1.0f },
		};
	int fftDebugMode_ = 0;
	float fftDebugDisplayScale_ = 1.0f;
	bool showSandBed_ = false;
	bool showBeach_ = false;
	bool showObstacles_ = false;
	bool showPbrSamples_ = false;
	bool showValidationPrimitives_ = false;
	bool showCrystal_ = true;
	bool pauseCrystalRotation_ = false;
	float crystalRotation_ = 0.38f;
	CrystalMaterialSettings crystalSettings_{};
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
