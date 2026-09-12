#pragma once

#include <array>
#include <cstdint>

#include <d3d12.h>
#include <wrl.h>

#include "DirectXCommon.h"
#include "Struct.h"

// A texture-free billboard renderer driven by a small set of persistent,
// CPU-animated metaballs. Coloring is deliberately separate from the scalar
// field so later layers can share the same contour source.
class ProceduralFlameRenderer {
public:
	static constexpr uint32_t kMaxMetaballs = 12;
	static constexpr uint32_t kMaxStarSparks = 8;

	enum class DisplayMode : uint32_t {
		OuterContour = 0,
		Field = 1,
		FilledMask = 2,
		ContourMask = 3,
		CoreMask = 4,
		CoreHotMask = 5,
	};

	struct Parameters {
		Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		float time = 0.0f;
		float noiseScale = 7.2f;
		float noiseSpeed = 0.95f;
		float distortionStrength = 0.29f;
		float contourThreshold = 1.05f;
		float contourWidth = 0.032f;
		float contourSoftness = 0.010f;
		float contourAaScale = 1.05f;
		float contourWidthModulation = 0.22f;
		float fieldGain = 1.0f;
		float contourEmissiveIntensity = 3.8f;
		float innerLineWidth = 0.005f;
		float innerLineIntensity = 0.75f;
		float outerGlowWidth = 0.095f;
		float outerGlowIntensity = 0.38f;
		float coreThreshold = 3.7f;
		float coreSoftness = 0.16f;
		float coreIntensity = 4.8f;
		float coreVerticalBias = 0.95f;
		float coreBreakup = 1.0f;
		float coreNoiseScale = 5.4f;
		float coreHotThreshold = 4.65f;
		Vector4 coreCyanTint = { 0.24f, 0.82f, 1.0f, 1.0f };
		bool enableStarSparks = true;
		uint32_t starSparkCount = 8;
		float starSparkSize = 0.040f;
		float starSparkIntensity = 7.2f;
		float starSparkTwinkleSpeed = 1.15f;
		float starSparkGlowStrength = 0.55f;
		float flowSpeed = 1.0f;
		float radiusScale = 1.0f;
		float swayStrength = 1.0f;
		float spawnSpread = 0.20f;
		float compactSupportScale = 2.2f;
		float satelliteSeparation = 0.14f;
		uint32_t activeMetaballCount = 12;
		DisplayMode displayMode = DisplayMode::OuterContour;
	};

	void Initialize(DirectXCommon* dxCommon);
	void SetParameters(const Parameters& parameters);
	const Parameters& GetParameters() const { return parameters_; }
	void Update(float deltaTime);
	void ResetMetaballs();

	void Draw(
		const Vector3& center,
		const Vector2& size,
		const Matrix4x4& cameraWorld,
		const Matrix4x4& viewProjection);

private:
	struct MetaballState {
		Vector2 position{};
		float radius = 0.1f;
		Vector2 velocity{};
		float lateralSway = 0.05f;
		float lifetime = 4.0f;
		float seed = 0.0f;
		float age = 0.0f;
		float phase = 0.0f;
		float baseX = 0.5f;
		float baseRadius = 0.1f;
		float separationDirection = 1.0f;
		bool isSatellite = false;
		uint32_t generation = 0;
	};

	struct GpuParameters {
		Vector4 color{};
		Vector4 coreCyanTint = { 0.24f, 0.82f, 1.0f, 1.0f };
		float time = 0.0f;
		float noiseScale = 1.0f;
		float noiseSpeed = 1.0f;
		float distortionStrength = 0.0f;
		float contourThreshold = 1.0f;
		float contourWidth = 0.045f;
		float contourSoftness = 0.015f;
		float contourEmissiveIntensity = 1.0f;
		float innerLineWidth = 0.012f;
		float innerLineIntensity = 10.0f;
		float outerGlowWidth = 0.14f;
		float outerGlowIntensity = 1.15f;
		float contourAaScale = 1.05f;
		float contourWidthModulation = 0.10f;
		float contourPadding[2]{};
		float coreThreshold = 3.2f;
		float coreSoftness = 0.22f;
		float coreIntensity = 7.5f;
		float coreVerticalBias = 0.95f;
		float coreBreakup = 0.32f;
		float coreNoiseScale = 1.65f;
		float coreHotThreshold = 4.5f;
		float corePadding = 0.0f;
		float fieldGain = 1.0f;
		float billboardAspect = 1.0f;
		float compactSupportScale = 2.35f;
		uint32_t displayMode = 0;
		uint32_t activeMetaballCount = 0;
		float padding[3]{};
		uint32_t enableStarSparks = 1;
		uint32_t starSparkCount = 6;
		float starSparkSize = 0.034f;
		float starSparkIntensity = 9.0f;
		float starSparkTwinkleSpeed = 0.72f;
		float starSparkGlowStrength = 0.65f;
		float starSparkPadding[2]{};
		std::array<Vector4, kMaxStarSparks> starSparkData{};
		std::array<Vector4, kMaxStarSparks> starSparkColors{};
		std::array<Vector4, kMaxMetaballs> metaballs{};
	};

	void CreatePipeline();
	void RespawnMetaball(uint32_t index, float initialLifeFraction = 0.0f);
	void EvaluateMetaball(uint32_t index);
	void UploadParameters(float billboardAspect);

	DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> viewProjectionResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> parameterResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	TrailVertex* vertexData_ = nullptr;
	Matrix4x4* viewProjectionData_ = nullptr;
	GpuParameters* parameterData_ = nullptr;
	Parameters parameters_{};
	std::array<MetaballState, kMaxMetaballs> metaballs_{};
};
