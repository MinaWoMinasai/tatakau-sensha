#pragma once

#include "Struct.h"
#include <cstddef>
#include <cstdint>
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
	float emissiveIntensity = 12.0f; // 細いピンク線を既存Bloomの半解像度・輝度しきい値へ渡す。
	float rimStrength = 0.0f; // 本体の面発光は任意。既定では外周だけを発光する。
	float rimPower = 3.0f;
	float outlineWidthPixels = 1.75f;
	uint32_t outlineEnabled = 1;
};

static_assert(sizeof(NeonSkinnedParams) == 48);
static_assert(offsetof(NeonSkinnedParams, emissiveColor) == 16);
static_assert(offsetof(NeonSkinnedParams, rimStrength) == 32);
static_assert(offsetof(NeonSkinnedParams, outlineWidthPixels) == 40);
static_assert(offsetof(NeonSkinnedParams, outlineEnabled) == 44);

// 既存SkinnedModelのGeometryと更新済みPaletteのみを利用する、独立した不透明Sceneパス。
// 通常の3枚のScene MRT (HDR / Normal / Material) + D24S8を呼び出し側でBindすること。
// モデルは同じDirectXCommon / SrvManagerでInitializeされている必要がある。
// 現在のDSVのStencil bit 0x80をこのパス用に予約する。他の7bitは保持する。
// このbitはDrawの前後で消去し、内部線・Submesh境界には輪郭を描かない。
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
	// 既定は現在のSceneサイズ(WinApp::kClientWidth / Height)。別サイズのパスは明示指定する。
	void Draw(const SkinnedModel& model, D3D12_GPU_VIRTUAL_ADDRESS transformationCbv,
		const Vector3& cameraWorldPosition, const Vector2& viewportSize);

private:
	struct GpuConstants {
		NeonSkinnedParams params;
		Vector3 cameraWorldPosition{};
		float padding = 0.0f;
		Vector2 viewportSize{};
		float viewportPadding[2]{};
	};
	static_assert(sizeof(GpuConstants) == 80);
	static_assert(offsetof(GpuConstants, cameraWorldPosition) == 48);
	static_assert(offsetof(GpuConstants, viewportSize) == 64);

	struct DrawConstantBuffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		GpuConstants* mapped = nullptr;
	};

	void CreatePipeline();
	DrawConstantBuffer& AcquireDrawConstantBuffer();
	void ClearOutlineStencil();

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> doubleSidedPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> outlinePipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> doubleSidedOutlinePipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> stencilClearPipelineState_;
	NeonSkinnedParams params_;
	std::vector<DrawConstantBuffer> drawConstantBuffers_;
	size_t nextDrawIndex_ = 0;
};
