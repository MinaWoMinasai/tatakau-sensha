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

	enum class DisplayMode : uint32_t {
		Emissive = 0,
		Field = 1,
		FilledMask = 2,
		IsoBand = 3,
	};

	struct Parameters {
		Vector4 color = { 1.0f, 0.82f, 0.50f, 1.0f };
		float time = 0.0f;
		float noiseScale = 3.6f;
		float noiseSpeed = 0.72f;
		float distortionStrength = 0.12f;
		float fieldThreshold = 1.05f;
		float edgeSoftness = 0.06f;
		float isoBandWidth = 0.10f;
		float fieldGain = 1.0f;
		float emissiveIntensity = 3.0f;
		float flowSpeed = 1.0f;
		float radiusScale = 1.0f;
		float swayStrength = 1.0f;
		float spawnSpread = 0.22f;
		uint32_t activeMetaballCount = 10;
		DisplayMode displayMode = DisplayMode::Emissive;
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
		uint32_t generation = 0;
	};

	struct GpuParameters {
		Vector4 color{};
		float time = 0.0f;
		float noiseScale = 1.0f;
		float noiseSpeed = 1.0f;
		float distortionStrength = 0.0f;
		float fieldThreshold = 1.0f;
		float edgeSoftness = 0.05f;
		float isoBandWidth = 0.1f;
		float emissiveIntensity = 1.0f;
		float fieldGain = 1.0f;
		float billboardAspect = 1.0f;
		uint32_t displayMode = 0;
		uint32_t activeMetaballCount = 0;
		std::array<Vector4, kMaxMetaballs> metaballs{};
	};

	void CreatePipeline();
	void RespawnMetaball(uint32_t index, float initialLifeFraction = 0.0f);
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
