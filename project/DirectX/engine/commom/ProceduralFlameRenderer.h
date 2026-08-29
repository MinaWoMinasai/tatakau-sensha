#pragma once

#include <d3d12.h>
#include <wrl.h>

#include "DirectXCommon.h"
#include "Struct.h"

// A small, texture-free billboard renderer whose mask is intentionally kept
// separate from flame coloring. Later phases can derive core/outline/rainbow
// bands from the same mask without changing the draw path.
class ProceduralFlameRenderer {
public:
	struct Parameters {
		Vector4 color = { 1.0f, 0.82f, 0.50f, 1.0f };
		float time = 0.0f;
		float noiseScale = 3.6f;
		float noiseSpeed = 0.72f;
		float distortionStrength = 0.16f;
		float flameWidth = 0.76f;
		float flameHeight = 0.94f;
		float edgeSoftness = 0.018f;
		float threshold = 0.0f;
		float emissiveIntensity = 3.0f;
		float debugMask = 0.0f;
		float bodyRoundness = 1.0f;
		float neckWidth = 0.50f;
		float tongueStrength = 1.0f;
		float padding[3] = {};
	};

	void Initialize(DirectXCommon* dxCommon);
	void SetParameters(const Parameters& parameters);
	const Parameters& GetParameters() const { return parameters_; }

	void Draw(
		const Vector3& center,
		const Vector2& size,
		const Matrix4x4& cameraWorld,
		const Matrix4x4& viewProjection);

private:
	void CreatePipeline();

	DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> viewProjectionResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> parameterResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	TrailVertex* vertexData_ = nullptr;
	Matrix4x4* viewProjectionData_ = nullptr;
	Parameters* parameterData_ = nullptr;
	Parameters parameters_{};
};
