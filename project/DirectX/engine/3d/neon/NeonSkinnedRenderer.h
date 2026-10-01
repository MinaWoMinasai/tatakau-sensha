#pragma once

#include "Struct.h"
#include <cstddef>
#include <vector>
#include <d3d12.h>
#include <wrl.h>

class DirectXCommon;
class SrvManager;
class SkinnedModel;

// HLSLのNeonSkinnedConstantsと同じ16byte単位の配置。time等の未使用パラメータは持たない。
struct NeonSkinnedParams {
	Vector4 bodyColor = { 0.012f, 0.004f, 0.025f, 1.0f };
	Vector3 emissiveColor = { 1.0f, 0.035f, 0.6f };
	float emissiveIntensity = 4.0f;
	float rimStrength = 1.0f;
	float rimPower = 3.0f;
	float padding[2]{};
};

static_assert(sizeof(NeonSkinnedParams) == 48);
static_assert(offsetof(NeonSkinnedParams, emissiveColor) == 16);
static_assert(offsetof(NeonSkinnedParams, rimStrength) == 32);

// 既存SkinnedModelのGeometryと更新済みPaletteのみを利用する、独立した不透明Sceneパス。
// 通常の3枚のScene MRT (HDR / Normal / Material) + D24S8を呼び出し側でBindすること。
// モデルは同じDirectXCommon / SrvManagerでInitializeされている必要がある。
class NeonSkinnedRenderer {
public:
	NeonSkinnedRenderer() = default;
	NeonSkinnedRenderer(const NeonSkinnedRenderer&) = delete;
	NeonSkinnedRenderer& operator=(const NeonSkinnedRenderer&) = delete;

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	// 前フレームのGPU実行完了後、フレーム開始時に一度だけ呼ぶ。Draw用CB領域を再利用する。
	void BeginFrame();
	void SetParams(const NeonSkinnedParams& params);
	const NeonSkinnedParams& GetParams() const { return params_; }

	// transformationCbvはObject3d::Update()等で更新済みのTransformationMatrix(WithShadow)を指す。
	// 例: object.GetTransformationResource()->GetGPUVirtualAddress()。World / WVPは再計算しない。
	// cameraWorldPositionはそのTransformを作成したCamera / DebugCameraのワールド位置。
	// Render Target / Viewportは変更しない。後続パスは自身のRoot Signature / PSO等を再Bindする。
	// Renderer / Model / Transform CBはGPU実行完了まで保持し、途中で内容を上書きしないこと。
	void Draw(const SkinnedModel& model, D3D12_GPU_VIRTUAL_ADDRESS transformationCbv,
		const Vector3& cameraWorldPosition);

private:
	struct GpuConstants {
		NeonSkinnedParams params;
		Vector3 cameraWorldPosition{};
		float padding = 0.0f;
	};
	static_assert(sizeof(GpuConstants) == 64);
	static_assert(offsetof(GpuConstants, cameraWorldPosition) == 48);

	struct DrawConstantBuffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		GpuConstants* mapped = nullptr;
	};

	void CreatePipeline();
	DrawConstantBuffer& AcquireDrawConstantBuffer();

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> doubleSidedPipelineState_;
	NeonSkinnedParams params_;
	std::vector<DrawConstantBuffer> drawConstantBuffers_;
	size_t nextDrawIndex_ = 0;
};
