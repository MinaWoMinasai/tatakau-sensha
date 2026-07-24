#pragma once

#include <cstdint>

#include <d3d12.h>
#include <wrl.h>

#include "Calculation.h"

class Camera;
class DebugCamera;
class Model;

class OceanRenderer {
public:
	enum class Mode : uint32_t {
		Calm = 0,
		Naval = 1,
		ArcBlanc = 2,
	};

	struct alignas(16) OceanParameters {
		Matrix4x4 viewProjection{};
		Vector4 tint = { 0.72f, 0.82f, 0.92f, 0.98f };
		Vector3 cameraPosition{};
		float time = 0.0f;
		Vector3 gridOrigin{};
		float baseHeight = -1.15f;
		Vector2 windDirection = { 0.18f, 0.98f };
		float windSpeed = 12.0f;
		float choppiness = 3.10f;
		Vector3 sunDirection = { -0.12f, -0.26f, -0.96f };
		float sunIntensity = 2.75f;
		Vector3 sunColor = { 1.0f, 0.97f, 0.90f };
		float sunSpecularStrength = 0.70f;
		float artisticSunLaneStrength = 1.0f;
		float atmosphereStrength = 1.0f;
		float farFlattenStrength = 1.0f;
		float debugMode = 0.0f;
		float mode = static_cast<float>(Mode::ArcBlanc);
		float diagnosticsEnabled = 1.0f;
		float sunPathEnabled = 1.0f;
		float atmosphereEnabled = 1.0f;
		float farFlattenEnabled = 1.0f;
		float proceduralCloudReflectionEnabled = 0.0f;
		Vector2 padding{};
	};

	void Initialize(Model* gridModel, uint32_t environmentSrvIndex);
	void Update(
		float time,
		Camera& camera,
		DebugCamera& debugCamera,
		bool useDebugCamera);
	void Draw() const;

	void SetMode(Mode mode);
	void SetTint(const Vector4& tint);
	void SetBaseHeight(float height);
	void SetWind(const Vector2& direction, float speed, float choppiness);
	void SetSun(
		float intensity,
		float sunSpecularStrength,
		float artisticSunLaneStrength);
	void SetDiagnostics(
		bool sunPathEnabled,
		bool atmosphereEnabled,
		bool farFlattenEnabled,
		bool proceduralCloudReflectionEnabled,
		int debugMode,
		float atmosphereStrength,
		float farFlattenStrength);
	void SetEnvironmentSrvIndex(uint32_t environmentSrvIndex);

	const OceanParameters& GetParameters() const { return parameters_; }

private:
	void UploadParameters();

	Model* gridModel_ = nullptr;
	uint32_t environmentSrvIndex_ = 0;
	OceanParameters parameters_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> parameterResource_;
	OceanParameters* parameterData_ = nullptr;
};
