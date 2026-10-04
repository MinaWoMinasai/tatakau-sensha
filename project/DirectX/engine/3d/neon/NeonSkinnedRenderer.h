#pragma once

#include "Struct.h"
#include "NeonDissolve.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <d3d12.h>
#include <wrl.h>

namespace cg2 {

class DirectXCommon;
class SrvManager;
class SkinnedModel;

// HLSLのNeonSkinnedConstantsと同じ16byte単位の配置。time等の未使用パラメータは持たない。
/// @brief スキニングされたモデル全体のネオン描画設定を表す。
struct NeonSkinnedParams {
    Vector4 bodyColor = {0.012f, 0.004f, 0.025f, 1.0f};
    Vector3 emissiveColor = {1.0f, 0.035f, 0.6f};
    float emissiveIntensity = 12.0f; // 細いピンク線を既存Bloomの半解像度・輝度しきい値へ渡す。
    float rimStrength = 0.0f;        // 本体の面発光は任意。既定では外周だけを発光する。
    float rimPower = 3.0f;
    float outlineWidthPixels = 1.75f;
    uint32_t outlineEnabled = 1;
    uint32_t internalLineEnabled = 0; // 既存呼び出しは外周のみ。Preview等で明示的に有効化する。
    float internalLineWidthPixels = 1.0f;
    float internalLineIntensity = 8.0f;
    float internalLineThreshold = 0.12f;
    Vector3 geometryLineColor = {0.08f, 0.65f, 1.0f};
    float geometryLineIntensity = 4.0f;
    uint32_t geometryLineEnabled = 0; // 既存の外観を維持。対応Deviceで明示的に有効化する。
    float geometryLineWidthPixels = 1.0f; // 共有辺を挟む全幅。各三角形側はこの半幅。
    float bodyEmissionIntensity = 0.0f; // Body色による弱い面の補助発光。0で従来の出力を維持する。
    float bodyPadding = 0.0f;
    Vector3 featureMaskColor = {1.0f, 0.025f, 0.35f};
    float featureMaskIntensity = 5.0f;
    float featureMaskBlend = 0.0f; // 0で従来のTexture特徴線を維持する。
    uint32_t featureMaskDebugMode = 0; // 0: 発光合成、1: R coverage、2: G置換領域。
    float featureMaskPadding[2]{};
    Vector3 lineCoreColor = {1.0f, 0.25f, 0.45f};
    float lineCoreIntensity = 4.0f;
    Vector3 lineHaloColor = {1.0f, 0.025f, 0.25f};
    float lineHaloIntensity = 1.5f;
    Vector3 outlineCoreColor = {1.0f, 0.12f, 0.38f};
    float outlineCoreIntensity = 8.0f;
    uint32_t featureMaskRenderMode = 0; // 0: original RG, 1: coverage RGB, 2: SDF with coverage minification fallback.
    uint32_t splitLineEmission = 0; // Opt-in: independently colored core and narrow surface halo.
    uint32_t lineDiagnosticMode = 0; // 0: combined, 1: core, 2: surface halo, 3: dark body.
    float sdfRangeTexels = 16.0f; // Encoded distance = (R - .5) * 2 * range; positive inside a stroke.
    float sdfHaloWidthTexels = 4.0f; // UV surface halo, not screen-space bloom. Candidate G includes this support.
    float sdfLodBlendStart = 1.0f;
    float sdfLodBlendEnd = 2.0f;
    float qualityPadding = 0.0f;
    NeonDissolveParams dissolve; // Disabled by default; evaluated in skinned pre-World model space.
};

static_assert(sizeof(NeonSkinnedParams) == 288);
static_assert(offsetof(NeonSkinnedParams, dissolve) == 208);
static_assert(offsetof(NeonSkinnedParams, emissiveColor) == 16);
static_assert(offsetof(NeonSkinnedParams, rimStrength) == 32);
static_assert(offsetof(NeonSkinnedParams, outlineWidthPixels) == 40);
static_assert(offsetof(NeonSkinnedParams, outlineEnabled) == 44);
static_assert(offsetof(NeonSkinnedParams, internalLineEnabled) == 48);
static_assert(offsetof(NeonSkinnedParams, geometryLineColor) == 64);
static_assert(offsetof(NeonSkinnedParams, geometryLineIntensity) == 76);
static_assert(offsetof(NeonSkinnedParams, geometryLineEnabled) == 80);
static_assert(offsetof(NeonSkinnedParams, geometryLineWidthPixels) == 84);
static_assert(offsetof(NeonSkinnedParams, bodyEmissionIntensity) == 88);
static_assert(offsetof(NeonSkinnedParams, bodyPadding) == 92);
static_assert(offsetof(NeonSkinnedParams, featureMaskColor) == 96);
static_assert(offsetof(NeonSkinnedParams, featureMaskIntensity) == 108);
static_assert(offsetof(NeonSkinnedParams, featureMaskBlend) == 112);
static_assert(offsetof(NeonSkinnedParams, featureMaskDebugMode) == 116);
static_assert(offsetof(NeonSkinnedParams, featureMaskPadding) == 120);
static_assert(offsetof(NeonSkinnedParams, lineCoreColor) == 128);
static_assert(offsetof(NeonSkinnedParams, lineHaloColor) == 144);
static_assert(offsetof(NeonSkinnedParams, outlineCoreColor) == 160);
static_assert(offsetof(NeonSkinnedParams, featureMaskRenderMode) == 176);
static_assert(offsetof(NeonSkinnedParams, sdfHaloWidthTexels) == 192);

// Texture由来の特徴線と任意のAlpha Cutout。空の設定では通常の不透明Neonを維持する。
// alphaCutoff=0はCutoutなし。BLENDのソート/半透明合成は実装しない。
/// @brief スキニングされたサブメッシュごとのネオン外観を指定する。
struct NeonSkinnedSubmeshParams {
    float lineStrength = 1.0f;
    float alphaCutoff = 0.0f;
    float geometryLineStrength = 0.0f; // Texture特徴線とは独立。既定では構造線を出さない。
    float internalLineThresholdScale = 1.0f; // Textureの薄線と幅広い陰影の選別。1で従来のしきい値。
};
static_assert(sizeof(NeonSkinnedSubmeshParams) == 16);
static_assert(offsetof(NeonSkinnedSubmeshParams, geometryLineStrength) == 8);
static_assert(offsetof(NeonSkinnedSubmeshParams, internalLineThresholdScale) == 12);

// 既存SkinnedModelのGeometryと更新済みPaletteのみを利用する、独立した不透明Sceneパス。
// 通常の3枚のScene MRT (HDR / Normal / Material) + D24S8を呼び出し側でBindすること。
// モデルは同じDirectXCommon / SrvManagerでInitializeされている必要がある。
// 現在のDSVのStencil bit 0x80をこのパス用に予約する。他の7bitは保持する。
// このbitはDrawの前後で消去し、Hullはモデル全体の外側に描く。
// 内部の特徴線はBody Shaderで既存BaseColor Texture / UVから生成する。
// SkinnedModel::Initialize済みのMaterialのSRV indexも同じSrvManagerのHeapに属すること。
/// @brief 骨格変形後のモデルをネオンの面と輪郭線として描画する。
class NeonSkinnedRenderer {
public:
    /// @brief インスタンスの初期値と利用先を設定する。
    NeonSkinnedRenderer() = default;
    NeonSkinnedRenderer(const NeonSkinnedRenderer&) = delete;
    NeonSkinnedRenderer& operator=(const NeonSkinnedRenderer&) = delete;

    // Explicit frame-boundary teardown. Last GPU use must have completed and
    // SrvManager must remain alive. Safe to call again after resources are freed.
    void ReleaseGpuResources();

    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    // falseは任意機能のパイプラインを作らず従来描画を使う（比較・フォールバック検証用）。
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, bool enableGeometryLinePipeline = true);
    bool IsGeometryLinesSupported() const { return geometryLinesSupported_; }
    const std::string& GetGeometryLinesStatus() const { return geometryLinesStatus_; }
    // 前フレームのGPU実行完了後、フレーム開始時に一度だけ呼ぶ。Draw用CB領域を再利用する。
    void BeginFrame();
    size_t GetDrawConstantBufferCount() const { return drawConstantBuffers_.size(); }
    /// @brief パラメーターを設定する。
    void SetParams(const NeonSkinnedParams& params);
    /// @brief パラメーターを返す。
    const NeonSkinnedParams& GetParams() const
    {
        return params_;
    }
    // 空または描くモデルのSubmesh数と同じ長さ。各DrawのRoot Constantsへ記録する。
    void SetSubmeshParams(const std::vector<NeonSkinnedSubmeshParams>& params);
    // 空またはモデルのSubmesh数と同じ長さ。nullopt / 予約index 0 / 範囲外はG=0のnull SRVへ戻す。
    // SRVは同じSrvManagerのLinearData Texture2D。所有者はGPU完了まで資源とSRVを保持する。
    // TextureManagerのキャッシュを使い、GPU使用中の差し替え・破棄は行わない。
    void SetSubmeshFeatureMasks(const std::vector<std::optional<uint32_t>>& srvIndices);
    // Optional signed-distance maps paired with the coverage masks above. Missing / invalid maps
    // retain the coverage path, even if SDF mode was requested. Resources obey the same lifetime contract.
    void SetSubmeshFeatureDistanceMasks(const std::vector<std::optional<uint32_t>>& srvIndices);

    // transformationCbvはObject3d::Update()等で更新済みのTransformationMatrix(WithShadow)を指す。
    // 例: object.GetTransformationResource()->GetGPUVirtualAddress()。World / WVPは再計算しない。
    // cameraWorldPositionはそのTransformを作成したCamera / DebugCameraのワールド位置。
    // Render Target / Viewportは変更しない。後続パスは自身のRoot Signature / PSO等を再Bindする。
    // Renderer / Model / Transform CBはGPU実行完了まで保持し、途中で内容を上書きしないこと。
    void Draw(const SkinnedModel& model, D3D12_GPU_VIRTUAL_ADDRESS transformationCbv, const Vector3& cameraWorldPosition);
    // 既定は現在のSceneサイズ(WinApp::kClientWidth / Height)。別サイズのパスは明示指定する。
    void Draw(const SkinnedModel& model, D3D12_GPU_VIRTUAL_ADDRESS transformationCbv, const Vector3& cameraWorldPosition,
              const Vector2& viewportSize);

private:
    /// @brief 対応するシェーダーへ渡す定数の配置を表す。CPUとHLSLのレイアウトを合わせる。
    struct GpuConstants {
        NeonSkinnedParams params;
        Vector3 cameraWorldPosition{};
        float padding = 0.0f;
        Vector2 viewportSize{};
        float viewportPadding[2]{};
    };
    static_assert(sizeof(GpuConstants) == 320);
    static_assert(offsetof(GpuConstants, cameraWorldPosition) == 288);
    static_assert(offsetof(GpuConstants, viewportSize) == 304);
    static constexpr size_t kDrawConstantBufferBytes =
        ((sizeof(GpuConstants) + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1)
            / D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) * D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    static_assert(kDrawConstantBufferBytes >= sizeof(GpuConstants));

    struct SubmeshConstants {
        NeonSkinnedSubmeshParams surface;
        uint32_t hasDistanceMask = 0;
        uint32_t padding[3]{};
    };
    static_assert(sizeof(SubmeshConstants) == 32);
    static_assert(offsetof(SubmeshConstants, hasDistanceMask) == 16);

    /// @brief 1回の描画に使う定数バッファとマップ先を保持する。
    struct DrawConstantBuffer {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        GpuConstants* mapped = nullptr;
    };

    /// @brief パイプラインを生成する。
    void CreatePipeline();
    /// @brief 描画定数バッファを利用枠を取得する。
    DrawConstantBuffer& AcquireDrawConstantBuffer();
    /// @brief 輪郭ステンシルを消去する。
    void ClearOutlineStencil();
    /// @brief サブメッシュ表面を描画・計算に結び付ける。
    void BindSubmeshSurface(const SkinnedModel& model, size_t index);

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> doubleSidedPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> geometryPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> doubleSidedGeometryPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> outlinePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> doubleSidedOutlinePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> stencilClearPipelineState_;
    NeonSkinnedParams params_;
    std::vector<NeonSkinnedSubmeshParams> submeshParams_;
    std::vector<std::optional<uint32_t>> featureMaskSrvIndices_;
    std::vector<std::optional<uint32_t>> featureDistanceSrvIndices_;
    uint32_t nullFeatureMaskSrvIndex_ = 0;
    std::vector<DrawConstantBuffer> drawConstantBuffers_;
    size_t nextDrawIndex_ = 0;
    bool geometryLinesSupported_ = false;
    std::string geometryLinesStatus_ = "Not initialized.";
};

} // namespace cg2
