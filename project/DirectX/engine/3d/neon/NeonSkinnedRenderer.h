#pragma once

#include "Struct.h"
#include <cstddef>
#include <cstdint>
#include <vector>
#include <d3d12.h>
#include <wrl.h>

namespace cg2 {

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
	uint32_t internalLineEnabled = 0; // 既存呼び出しは外周のみ。Preview等で明示的に有効化する。
	float internalLineWidthPixels = 1.0f;
	float internalLineIntensity = 8.0f;
	float internalLineThreshold = 0.12f;
};

static_assert(sizeof(NeonSkinnedParams) == 64);
static_assert(offsetof(NeonSkinnedParams, emissiveColor) == 16);
static_assert(offsetof(NeonSkinnedParams, rimStrength) == 32);
static_assert(offsetof(NeonSkinnedParams, outlineWidthPixels) == 40);
static_assert(offsetof(NeonSkinnedParams, outlineEnabled) == 44);
static_assert(offsetof(NeonSkinnedParams, internalLineEnabled) == 48);

// Texture由来の特徴線と任意のAlpha Cutout。空の設定では通常の不透明Neonを維持する。
// alphaCutoff=0はCutoutなし。BLENDのソート/半透明合成は実装しない。
struct NeonSkinnedSubmeshParams {
	float lineStrength = 1.0f;
	float alphaCutoff = 0.0f;
};
static_assert(sizeof(NeonSkinnedSubmeshParams) == 8);

// 既存SkinnedModelのGeometryと更新済みPaletteのみを利用する、独立した不透明Sceneパス。
// 通常の3枚のScene MRT (HDR / Normal / Material) + D24S8を呼び出し側でBindすること。
// モデルは同じDirectXCommon / SrvManagerでInitializeされている必要がある。
// 現在のDSVのStencil bit 0x80をこのパス用に予約する。他の7bitは保持する。
// このbitはDrawの前後で消去し、Hullはモデル全体の外側に描く。
// 内部の特徴線はBody Shaderで既存BaseColor Texture / UVから生成する。
// SkinnedModel::Initialize済みのMaterialのSRV indexも同じSrvManagerのHeapに属すること。
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
	// 空または描くモデルのSubmesh数と同じ長さ。各DrawのRoot Constantsへ記録する。
	void SetSubmeshParams(const std::vector<NeonSkinnedSubmeshParams>& params);

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
	static_assert(sizeof(GpuConstants) == 96);
	static_assert(offsetof(GpuConstants, cameraWorldPosition) == 64);
	static_assert(offsetof(GpuConstants, viewportSize) == 80);

	struct DrawConstantBuffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		GpuConstants* mapped = nullptr;
	};

	void CreatePipeline();
	DrawConstantBuffer& AcquireDrawConstantBuffer();
	void ClearOutlineStencil();
	void BindSubmeshSurface(const SkinnedModel& model, size_t index);

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> doubleSidedPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> outlinePipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> doubleSidedOutlinePipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> stencilClearPipelineState_;
	NeonSkinnedParams params_;
	std::vector<NeonSkinnedSubmeshParams> submeshParams_;
	std::vector<DrawConstantBuffer> drawConstantBuffers_;
	size_t nextDrawIndex_ = 0;
};

} // namespace cg2
