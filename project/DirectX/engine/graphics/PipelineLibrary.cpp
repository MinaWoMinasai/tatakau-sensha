#include "PipelineLibrary.h"
#include "DirectXCommon.h"
#include "ResourcePaths.h"

#include <cassert>
#include <iterator>
#include "Bloom.h"
#include "StartupTrace.h"
#include "Utf8Path.h"

namespace myEngine {

using namespace Microsoft::WRL;

namespace {

/// @brief ポストエフェクトのピクセルシェーダー。PostEffectType と同じ順に並べる。
constexpr const wchar_t* kPostEffectPixelShaders[] = {
    L"BloomExtract.PS.hlsl",        L"BloomDownsample.PS.hlsl", L"BloomBlurH.PS.hlsl",          L"BloomBlurV.PS.hlsl",
    L"GaussianFilter.PS.hlsl",      L"Composite.PS.hlsl",       L"ObjectPostComposite.PS.hlsl", L"ObjectPostOutlineAdd.PS.hlsl",
    L"ObjectPostBloomAdd.PS.hlsl",  L"Random.PS.hlsl",          L"SSAOResolve.PS.hlsl",         L"SSAODenoise.PS.hlsl",
    L"SSRResolve.PS.hlsl",          L"SSRDenoise.PS.hlsl",      L"MotionVectorResolve.PS.hlsl", L"TemporalResolve.PS.hlsl",
    L"PhosphorPersistence.PS.hlsl", L"CrtPresent.PS.hlsl",
};
static_assert(std::size(kPostEffectPixelShaders) == PipelineLibrary::Crt_Present + 1);

/// @brief 1パイプラインの作り方。
struct PipelineSpec {
    PipelineId id;
    PipelineLibrary::ShaderType shaderType;
    BlendMode blendMode = kNone;
    DXGI_FORMAT format = RenderTargetFormats::kBackBufferRenderTargetFormat;
    bool doubleSided = false;
    PipelineLibrary::PostEffectType postEffectType = PipelineLibrary::Bloom_Extract;
    const wchar_t* computeShader = nullptr;
};

constexpr DXGI_FORMAT kBack = RenderTargetFormats::kBackBufferRenderTargetFormat;
constexpr DXGI_FORMAT kHdr = RenderTargetFormats::kSceneRenderTargetFormat;

/// @brief ポストエフェクトのパイプラインの作り方を返す。
constexpr PipelineSpec PostEffectSpec(PipelineId id, PipelineLibrary::PostEffectType type, BlendMode blendMode = kNone,
                                      DXGI_FORMAT format = kBack)
{
    return {id, PipelineLibrary::PostEffect, blendMode, format, false, type};
}

/// @brief コンピュートパイプラインの作り方を返す。
constexpr PipelineSpec ComputeSpec(PipelineId id, const wchar_t* shader)
{
    return {id, PipelineLibrary::ComputeParticle, kNone, kBack, false, PipelineLibrary::Bloom_Extract, shader};
}

/// @brief すべてのパイプラインの作り方。上から順に作る。
constexpr PipelineSpec kPipelineSpecs[] = {
    {PipelineId::ObjectNone, PipelineLibrary::Object, kNone},
    {PipelineId::ObjectAlpha, PipelineLibrary::Object, kNormal},
    {PipelineId::ObjectAdd, PipelineLibrary::Object, kAdd},
    {PipelineId::ObjectNoneHdr, PipelineLibrary::Object, kNone, kHdr},
    {PipelineId::ObjectAlphaHdr, PipelineLibrary::Object, kNormal, kHdr},
    {PipelineId::ObjectAddHdr, PipelineLibrary::Object, kAdd, kHdr},
    {PipelineId::Particle, PipelineLibrary::Particle, kAdd},
    {PipelineId::ModelParticle, PipelineLibrary::ModelParticle, kAdd},
    {PipelineId::ModelParticleHdr, PipelineLibrary::ModelParticle, kAdd, kHdr},
    ComputeSpec(PipelineId::ParticleInitialize, L"ParticleInitialize.CS.hlsl"),
    ComputeSpec(PipelineId::ParticleEmit, L"ParticleEmit.CS.hlsl"),
    ComputeSpec(PipelineId::ParticleEmitBatch, L"ParticleEmitBatch.CS.hlsl"),
    ComputeSpec(PipelineId::ParticleUpdate, L"ParticleUpdate.CS.hlsl"),
    ComputeSpec(PipelineId::ParticleScan, L"ParticleScan.CS.hlsl"),
    ComputeSpec(PipelineId::ParticleCompact, L"ParticleCompact.CS.hlsl"),
    PostEffectSpec(PipelineId::BloomExtract, PipelineLibrary::Bloom_Extract),
    PostEffectSpec(PipelineId::BloomExtractHdr, PipelineLibrary::Bloom_Extract, kNone, kHdr),
    PostEffectSpec(PipelineId::BloomBlurH, PipelineLibrary::Bloom_BlurH),
    PostEffectSpec(PipelineId::BloomBlurHHdr, PipelineLibrary::Bloom_BlurH, kNone, kHdr),
    PostEffectSpec(PipelineId::BloomBlurV, PipelineLibrary::Bloom_BlurV),
    PostEffectSpec(PipelineId::BloomBlurVHdr, PipelineLibrary::Bloom_BlurV, kNone, kHdr),
    PostEffectSpec(PipelineId::GaussianFilter, PipelineLibrary::Gaussian_Filter),
    PostEffectSpec(PipelineId::BloomComposite, PipelineLibrary::Bloom_Composite, kAdd),
    PostEffectSpec(PipelineId::ObjectPostComposite, PipelineLibrary::ObjectPost_Composite, kNormal),
    PostEffectSpec(PipelineId::ObjectPostOutlineAdd, PipelineLibrary::ObjectPost_OutlineAdd, kAdd),
    PostEffectSpec(PipelineId::ObjectPostBloomAdd, PipelineLibrary::ObjectPost_BloomAdd, kAdd),
    PostEffectSpec(PipelineId::ObjectPostCompositeHdr, PipelineLibrary::ObjectPost_Composite, kNormal, kHdr),
    PostEffectSpec(PipelineId::ObjectPostOutlineAddHdr, PipelineLibrary::ObjectPost_OutlineAdd, kAdd, kHdr),
    PostEffectSpec(PipelineId::ObjectPostBloomAddHdr, PipelineLibrary::ObjectPost_BloomAdd, kAdd, kHdr),
    PostEffectSpec(PipelineId::Random, PipelineLibrary::Random),
    PostEffectSpec(PipelineId::RandomHdr, PipelineLibrary::Random, kNone, kHdr),
    PostEffectSpec(PipelineId::SsaoResolve, PipelineLibrary::SSAO_Resolve, kNone, RenderTargetFormats::kAmbientOcclusionBufferFormat),
    PostEffectSpec(PipelineId::SsaoDenoise, PipelineLibrary::SSAO_Denoise, kNone, RenderTargetFormats::kAmbientOcclusionBufferFormat),
    PostEffectSpec(PipelineId::SsrResolve, PipelineLibrary::SSR_Resolve),
    PostEffectSpec(PipelineId::SsrResolveHdr, PipelineLibrary::SSR_Resolve, kNone, kHdr),
    PostEffectSpec(PipelineId::SsrDenoise, PipelineLibrary::SSR_Denoise),
    PostEffectSpec(PipelineId::SsrDenoiseHdr, PipelineLibrary::SSR_Denoise, kNone, kHdr),
    PostEffectSpec(PipelineId::MotionVectorResolve, PipelineLibrary::MotionVector_Resolve, kNone,
                   RenderTargetFormats::kMotionVectorBufferFormat),
    PostEffectSpec(PipelineId::TemporalResolve, PipelineLibrary::Temporal_Resolve, kNone, kHdr),
    PostEffectSpec(PipelineId::PhosphorPersistence, PipelineLibrary::Phosphor_Persistence, kNone, kHdr),
    PostEffectSpec(PipelineId::CrtPresent, PipelineLibrary::Crt_Present),
    PostEffectSpec(PipelineId::BloomDownsample, PipelineLibrary::Bloom_Downsample),
    PostEffectSpec(PipelineId::BloomDownsampleHdr, PipelineLibrary::Bloom_Downsample, kNone, kHdr),
    {PipelineId::Shadow, PipelineLibrary::Shadow, kShadow},
    {PipelineId::Trail, PipelineLibrary::Trail, kAdd},
    {PipelineId::TrailHdr, PipelineLibrary::Trail, kAdd, kHdr},
    {PipelineId::HudRect, PipelineLibrary::Trail, kNormal},
    {PipelineId::Skybox, PipelineLibrary::Skybox, kNone},
    {PipelineId::SkyboxHdr, PipelineLibrary::Skybox, kNone, kHdr},
    {PipelineId::Skinning, PipelineLibrary::Skinning, kNone},
    {PipelineId::SkinningDoubleSided, PipelineLibrary::Skinning, kNone, kBack, true},
    {PipelineId::SkinningHdr, PipelineLibrary::Skinning, kNone, kHdr},
    {PipelineId::SkinningDoubleSidedHdr, PipelineLibrary::Skinning, kNone, kHdr, true},
    {PipelineId::SkinningShadow, PipelineLibrary::SkinningShadow, kShadow},
};
static_assert(std::size(kPipelineSpecs) == static_cast<size_t>(PipelineId::Count));

/// @brief 起動の1ステップ目にまとめて作るパイプラインの数。残りは1つずつ作る。
constexpr size_t kPipelinesInFirstStep = 3;

} // namespace

PipelineLibrary::PSO& PipelineLibrary::GetPSOObject(BlendMode blendMode)
{
    switch (blendMode) {
    case kNormal:
        return GetPipeline(PipelineId::ObjectAlpha);
    case kAdd:
        return GetPipeline(PipelineId::ObjectAdd);
    case kShadow:
        return GetPipeline(PipelineId::Shadow);
    case kAdd_Bloom_Extract:
        return GetPipeline(PipelineId::BloomExtract);
    case kAdd_Bloom_Downsample:
        return GetPipeline(PipelineId::BloomDownsample);
    case kAdd_Bloom_BlurH:
        return GetPipeline(PipelineId::BloomBlurH);
    case kAdd_Bloom_BlurV:
        return GetPipeline(PipelineId::BloomBlurV);
    case kAdd_Bloom_Composite:
        return GetPipeline(PipelineId::BloomComposite);
    case kAdd_ObjectPost_Composite:
        return GetPipeline(PipelineId::ObjectPostComposite);
    case kAdd_ObjectPost_OutlineAdd:
        return GetPipeline(PipelineId::ObjectPostOutlineAdd);
    case kAdd_ObjectPost_BloomAdd:
        return GetPipeline(PipelineId::ObjectPostBloomAdd);
    case kRandom:
        return GetPipeline(PipelineId::Random);
    case kAdd_SSAO_Resolve:
        return GetPipeline(PipelineId::SsaoResolve);
    case kAdd_SSAO_Denoise:
        return GetPipeline(PipelineId::SsaoDenoise);
    case kAdd_SSR_Resolve:
        return GetPipeline(PipelineId::SsrResolve);
    case kAdd_SSR_Denoise:
        return GetPipeline(PipelineId::SsrDenoise);
    case kAdd_MotionVector_Resolve:
        return GetPipeline(PipelineId::MotionVectorResolve);
    case kAdd_Temporal_Resolve:
        return GetPipeline(PipelineId::TemporalResolve);
    case kAdd_Phosphor_Persistence:
        return GetPipeline(PipelineId::PhosphorPersistence);
    case kAdd_Crt_Present:
        return GetPipeline(PipelineId::CrtPresent);
    default:
        return GetPipeline(PipelineId::ObjectNone);
    }
}

PipelineLibrary::PSO& PipelineLibrary::GetPSOObjectForScene(BlendMode blendMode)
{
    switch (blendMode) {
    case kNormal:
        return GetPipeline(PipelineId::ObjectAlphaHdr);
    case kAdd:
        return GetPipeline(PipelineId::ObjectAddHdr);
    case kShadow:
        return GetPipeline(PipelineId::Shadow);
    case kAdd_Bloom_Extract:
        return GetPipeline(PipelineId::BloomExtractHdr);
    case kAdd_Bloom_Downsample:
        return GetPipeline(PipelineId::BloomDownsampleHdr);
    case kAdd_Bloom_BlurH:
        return GetPipeline(PipelineId::BloomBlurHHdr);
    case kAdd_Bloom_BlurV:
        return GetPipeline(PipelineId::BloomBlurVHdr);
    case kAdd_ObjectPost_Composite:
        return GetPipeline(PipelineId::ObjectPostCompositeHdr);
    case kAdd_ObjectPost_OutlineAdd:
        return GetPipeline(PipelineId::ObjectPostOutlineAddHdr);
    case kAdd_ObjectPost_BloomAdd:
        return GetPipeline(PipelineId::ObjectPostBloomAddHdr);
    case kRandom:
        return GetPipeline(PipelineId::RandomHdr);
    case kAdd_SSAO_Resolve:
        return GetPipeline(PipelineId::SsaoResolve);
    case kAdd_SSAO_Denoise:
        return GetPipeline(PipelineId::SsaoDenoise);
    case kAdd_SSR_Resolve:
        return GetPipeline(PipelineId::SsrResolveHdr);
    case kAdd_SSR_Denoise:
        return GetPipeline(PipelineId::SsrDenoiseHdr);
    case kAdd_MotionVector_Resolve:
        return GetPipeline(PipelineId::MotionVectorResolve);
    case kAdd_Temporal_Resolve:
        return GetPipeline(PipelineId::TemporalResolve);
    case kAdd_Phosphor_Persistence:
        return GetPipeline(PipelineId::PhosphorPersistence);
    default:
        return GetPipeline(PipelineId::ObjectNoneHdr);
    }
}

void PipelineLibrary::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;
    device_ = dxCommon->GetDevice();
    pipelineTask_ = BuildPipelines();
}

void PipelineLibrary::Release()
{
    pipelineTask_ = {}; // すべてのPSOが揃う前にタイトルが閉じられることがある。
}

void PipelineLibrary::CreateShaderCommon(PSO& pso, BlendMode blendMode, bool doubleSided, DXGI_FORMAT renderTargetFormat)
{
    const bool usesSceneNormalTarget = renderTargetFormat == kSceneRenderTargetFormat &&
                                       (pso.shaderType_ == Object || pso.shaderType_ == Skinning || pso.shaderType_ == Skybox ||
                                        pso.shaderType_ == Trail || pso.shaderType_ == ModelParticle);

    // 1. 各タイプごとのシェーダーパスとルートシグネチャ初期化
    switch (pso.shaderType_) {
    case Object:
        pso.root_.InitializeForObject();
        pso.vsFilePath_ = ResourcePaths::EngineShader(L"Object3d.VS.hlsl");
        pso.psFilePath_ = usesSceneNormalTarget ? ResourcePaths::EngineShader(L"Object3d.Scene.PS.hlsl")
                                                : ResourcePaths::EngineShader(L"Object3d.PS.hlsl");
        break;
    case Particle:
        pso.root_.InitializeForParticle();
        pso.vsFilePath_ = ResourcePaths::EngineShader(L"Particle.VS.hlsl");
        pso.psFilePath_ = ResourcePaths::EngineShader(L"Particle.PS.hlsl");
        break;
    case ModelParticle:
        pso.root_.InitializeForModelParticle();
        pso.vsFilePath_ = ResourcePaths::EngineShader(L"ModelParticle.VS.hlsl");
        pso.psFilePath_ = usesSceneNormalTarget ? ResourcePaths::EngineShader(L"ModelParticle.Scene.PS.hlsl")
                                                : ResourcePaths::EngineShader(L"ModelParticle.PS.hlsl");
        break;
    case ComputeParticle:
        assert(false);
        break;
    case Shadow:
        pso.root_.InitializeForShadow();
        pso.vsFilePath_ = ResourcePaths::EngineShader(L"Shadow.VS.hlsl");
        pso.psFilePath_ = L"";
        break;
    case PostEffect:
        pso.root_.InitializeForPostEffect();
        pso.vsFilePath_ = ResourcePaths::EngineShader(L"FullScreen.VS.hlsl");
        pso.psFilePath_ = ResourcePaths::EngineShader(kPostEffectPixelShaders[pso.postEffectType_]);
        break;
    case Trail:
        pso.root_.InitializeForTrail(); // 上で作った関数
        pso.vsFilePath_ = ResourcePaths::EngineShader(L"Trail.VS.hlsl");
        pso.psFilePath_ =
            usesSceneNormalTarget ? ResourcePaths::EngineShader(L"Trail.Scene.PS.hlsl") : ResourcePaths::EngineShader(L"Trail.PS.hlsl");
        break;
    case Skybox:
        pso.root_.InitializeForSkybox();
        pso.vsFilePath_ = ResourcePaths::EngineShader(L"Skybox.VS.hlsl");
        pso.psFilePath_ =
            usesSceneNormalTarget ? ResourcePaths::EngineShader(L"Skybox.Scene.PS.hlsl") : ResourcePaths::EngineShader(L"Skybox.PS.hlsl");
        break;
    case Skinning:
        pso.root_.InitializeForObject();
        pso.vsFilePath_ = ResourcePaths::EngineShader(L"SkinningObject3d.VS.hlsl");
        pso.psFilePath_ = usesSceneNormalTarget ? ResourcePaths::EngineShader(L"Object3d.Scene.PS.hlsl")
                                                : ResourcePaths::EngineShader(L"Object3d.PS.hlsl");
        break;
    case SkinningShadow:
        pso.root_.InitializeForSkinningShadow();
        pso.vsFilePath_ = ResourcePaths::EngineShader(L"SkinningShadow.VS.hlsl");
        pso.psFilePath_ = L"";
        break;
    default:
        assert(false);
        break;
    }
    const std::string pipelineName =
        PathToUtf8(std::filesystem::path(pso.psFilePath_.empty() ? pso.vsFilePath_ : pso.psFilePath_).filename()) +
        "/format=" + std::to_string(renderTargetFormat) + "/blend=" + std::to_string(blendMode) + (doubleSided ? "/doubleSided" : "");
    StartupTrace::Scope pipelineScope("Pipeline.Prepare/" + pipelineName);
    StartupTrace::Count("pipeline.graphics.created");

    // 2. ルートシグネチャ生成
    {
        StartupTrace::Scope scope("Pipeline.RootSignature");
        pso.root_.Create(device_);
    }

    // 3. シェーダーコンパイル
    pso.vertexShaderBlob_ = dxCommon_->CompileShader(pso.vsFilePath_, L"vs_6_0");
    assert(pso.vertexShaderBlob_ != nullptr);

    pso.pixelShaderBlob_.Reset();
    if (pso.shaderType_ != Shadow && pso.shaderType_ != SkinningShadow && !pso.psFilePath_.empty()) {
        pso.pixelShaderBlob_ = dxCommon_->CompileShader(pso.psFilePath_, L"ps_6_0");
        assert(pso.pixelShaderBlob_ != nullptr);
    }

    // 4. グラフィックスパイプライン記述子の初期化
    ZeroMemory(&pso.graphicsDesc_, sizeof(pso.graphicsDesc_));

    // --- 旧 State クラスの処理をここに統合 ---

    // [RasterizerState] の設定
    pso.graphicsDesc_.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    pso.graphicsDesc_.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

    // [BlendState] の設定
    pso.graphicsDesc_.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pso.graphicsDesc_.BlendState.RenderTarget[0].BlendEnable = FALSE;
    pso.graphicsDesc_.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    pso.graphicsDesc_.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
    pso.graphicsDesc_.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    pso.graphicsDesc_.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    pso.graphicsDesc_.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    pso.graphicsDesc_.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

    // 2. ブレンドが必要な特定のモードの時だけ、設定を上書きして有効化する
    if (blendMode == kNormal) {
        pso.graphicsDesc_.BlendState.RenderTarget[0].BlendEnable = TRUE;
        pso.graphicsDesc_.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        pso.graphicsDesc_.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    } else if (blendMode == kAdd) {
        pso.graphicsDesc_.BlendState.RenderTarget[0].BlendEnable = TRUE;
        pso.graphicsDesc_.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        pso.graphicsDesc_.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    }

    // [DepthStencilState] のデフォルト設定
    pso.graphicsDesc_.DepthStencilState.DepthEnable = TRUE;
    pso.graphicsDesc_.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    if (blendMode == kNormal || blendMode == kAdd) {
        // 半透明描画時は深度バッファを書き換えない
        pso.graphicsDesc_.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    } else {
        pso.graphicsDesc_.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    }

    // --- 統合ここまで ---

    // 5. 個別設定の上書き (Shadow / PostEffect / Normal)
    pso.graphicsDesc_.pRootSignature = pso.root_.GetSignature().Get();
    pso.graphicsDesc_.VS = {pso.vertexShaderBlob_->GetBufferPointer(), pso.vertexShaderBlob_->GetBufferSize()};
    if (pso.pixelShaderBlob_) {
        pso.graphicsDesc_.PS = {pso.pixelShaderBlob_->GetBufferPointer(), pso.pixelShaderBlob_->GetBufferSize()};
    }

    if (pso.shaderType_ == Skybox) {
        pso.graphicsDesc_.NumRenderTargets = 1;
        pso.graphicsDesc_.RTVFormats[0] = renderTargetFormat;
        pso.graphicsDesc_.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        // Skybox用の設定
        pso.graphicsDesc_.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;                // 中から見るのでカリングしない
        pso.graphicsDesc_.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 奥行きを更新しない（常に背景）
        pso.graphicsDesc_.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

        pso.inputDesc_.Initialize(); // 頂点レイアウトはObjectと同じでOK
        pso.graphicsDesc_.InputLayout = pso.inputDesc_.GetLayout();
    } else if (pso.shaderType_ == Shadow || pso.shaderType_ == SkinningShadow) {
        pso.graphicsDesc_.NumRenderTargets = 0;
        pso.graphicsDesc_.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        // Shadow用に比較関数を調整（必要に応じて）
        pso.graphicsDesc_.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        if (pso.shaderType_ == SkinningShadow) {
            pso.inputDesc_.InitializeForSkinning();
        } else {
            pso.inputDesc_.Initialize();
        }
        pso.graphicsDesc_.InputLayout = pso.inputDesc_.GetLayout();
    } else if (pso.shaderType_ == PostEffect) {
        pso.graphicsDesc_.NumRenderTargets = 1;
        pso.graphicsDesc_.RTVFormats[0] = renderTargetFormat;
        pso.graphicsDesc_.DSVFormat = DXGI_FORMAT_UNKNOWN;
        pso.graphicsDesc_.DepthStencilState.DepthEnable = FALSE;
        pso.graphicsDesc_.InputLayout = {nullptr, 0};
    } else if (pso.shaderType_ == Skinning) {
        pso.graphicsDesc_.NumRenderTargets = 1;
        pso.graphicsDesc_.RTVFormats[0] = renderTargetFormat;
        pso.graphicsDesc_.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        pso.graphicsDesc_.DepthStencilState.DepthEnable = TRUE;
        pso.graphicsDesc_.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        pso.graphicsDesc_.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        pso.inputDesc_.InitializeForSkinning();
        pso.graphicsDesc_.InputLayout = pso.inputDesc_.GetLayout();
    } else if (pso.shaderType_ == Trail) {
        pso.graphicsDesc_.NumRenderTargets = 1;
        pso.graphicsDesc_.RTVFormats[0] = renderTargetFormat;
        pso.graphicsDesc_.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        // 軌跡用の設定
        pso.graphicsDesc_.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;                // 両面描画
        pso.graphicsDesc_.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 深度は塗らない

        pso.inputDesc_.InitializeForTrail(); // 頂点レイアウト(Pos, Color, UV)
        pso.graphicsDesc_.InputLayout = pso.inputDesc_.GetLayout();
    } else if (pso.shaderType_ == ModelParticle) {
        pso.graphicsDesc_.NumRenderTargets = 1;
        pso.graphicsDesc_.RTVFormats[0] = renderTargetFormat;
        pso.graphicsDesc_.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        pso.graphicsDesc_.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso.graphicsDesc_.DepthStencilState.DepthEnable = TRUE;
        pso.graphicsDesc_.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        pso.graphicsDesc_.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

        pso.inputDesc_.Initialize();
        pso.graphicsDesc_.InputLayout = pso.inputDesc_.GetLayout();
    } else {
        pso.graphicsDesc_.NumRenderTargets = 1;
        pso.graphicsDesc_.RTVFormats[0] = renderTargetFormat;
        pso.graphicsDesc_.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        pso.inputDesc_.Initialize();
        pso.graphicsDesc_.InputLayout = pso.inputDesc_.GetLayout();
        // 深度テストと深度の書き込みを有効にする。
        pso.graphicsDesc_.DepthStencilState.DepthEnable = TRUE;
        pso.graphicsDesc_.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        pso.graphicsDesc_.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    }

    // 6. 残りの共通設定
    if (doubleSided) {
        pso.graphicsDesc_.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    }
    if (usesSceneNormalTarget) {
        pso.graphicsDesc_.BlendState.IndependentBlendEnable = TRUE;
        pso.graphicsDesc_.NumRenderTargets = 3;
        pso.graphicsDesc_.RTVFormats[1] = kNormalBufferFormat;
        pso.graphicsDesc_.RTVFormats[2] = kMaterialBufferFormat;
        for (const int target : {1, 2}) {
            D3D12_RENDER_TARGET_BLEND_DESC& blend = pso.graphicsDesc_.BlendState.RenderTarget[target];
            blend.BlendEnable = FALSE;
            blend.SrcBlend = D3D12_BLEND_ONE;
            blend.DestBlend = D3D12_BLEND_ZERO;
            blend.BlendOp = D3D12_BLEND_OP_ADD;
            blend.SrcBlendAlpha = D3D12_BLEND_ONE;
            blend.DestBlendAlpha = D3D12_BLEND_ZERO;
            blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
            blend.RenderTargetWriteMask = (blendMode == kNone && pso.shaderType_ != Skybox) ? D3D12_COLOR_WRITE_ENABLE_ALL : 0;
        }
    }
    pso.graphicsDesc_.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.graphicsDesc_.SampleDesc.Count = 1;
    pso.graphicsDesc_.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // 7. PSO生成
    StartupTrace::Scope createScope("Pipeline.DriverGraphics/" + pipelineName);
    HRESULT hr = device_->CreateGraphicsPipelineState(&pso.graphicsDesc_, IID_PPV_ARGS(&pso.graphicsState_));
    assert(SUCCEEDED(hr));
}

void PipelineLibrary::CreateComputeShaderCommon(PSO& pso, const std::wstring& shaderPath)
{
    const std::string pipelineName = PathToUtf8(std::filesystem::path(shaderPath).filename());
    StartupTrace::Scope pipelineScope("Pipeline.Prepare/" + pipelineName);
    StartupTrace::Count("pipeline.compute.created");
    pso.root_.InitializeForComputeParticle();
    pso.root_.Create(device_);
    pso.computeShaderBlob_ = dxCommon_->CompileShader(shaderPath, L"cs_6_0");
    assert(pso.computeShaderBlob_ != nullptr);

    ZeroMemory(&pso.computeDesc_, sizeof(pso.computeDesc_));
    pso.computeDesc_.pRootSignature = pso.root_.GetSignature().Get();
    pso.computeDesc_.CS = {pso.computeShaderBlob_->GetBufferPointer(), pso.computeShaderBlob_->GetBufferSize()};

    StartupTrace::Scope createScope("Pipeline.DriverCompute/" + pipelineName);
    HRESULT hr = device_->CreateComputePipelineState(&pso.computeDesc_, IID_PPV_ARGS(&pso.computeState_));
    assert(SUCCEEDED(hr));
}

void PipelineLibrary::CreateAll()
{
    StartupTrace::Scope scope("DirectXCommon.AllPipelines");
    while (!InitializeStep()) {
    }
}

bool PipelineLibrary::InitializeStep()
{
    if (pipelinesReady_) {
        return true;
    }
    StartupTrace::Scope scope("DirectXCommon.PipelineStep");
    pipelinesReady_ = pipelineTask_.Advance();
    if (pipelinesReady_) {
        StartupTrace::Mark("engine.pipelines_ready");
    }
    return pipelinesReady_;
}

InitializationTask PipelineLibrary::BuildPipelines()
{
    for (size_t index = 0; index < std::size(kPipelineSpecs); ++index) {
        const PipelineSpec& spec = kPipelineSpecs[index];
        PSO& pso = GetPipeline(spec.id);
        pso.shaderType_ = spec.shaderType;
        pso.postEffectType_ = spec.postEffectType;
        if (spec.computeShader) {
            CreateComputeShaderCommon(pso, ResourcePaths::EngineShader(spec.computeShader));
        } else {
            CreateShaderCommon(pso, spec.blendMode, spec.doubleSided, spec.format);
        }
        if (index + 1 >= kPipelinesInFirstStep) {
            co_await std::suspend_always{};
        }
    }
}

bool PipelineLibrary::InitializeTrailSceneNoDepthPipeline()
{
    if (trailSceneNoDepthPipeline_) {
        return true;
    }
    const PSO& source = GetPipeline(PipelineId::TrailHdr);
    if (!device_ || !source.graphicsState_) {
        return false;
    }
    auto description = source.graphicsDesc_;
    description.DepthStencilState.DepthEnable = FALSE;
    description.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    description.DSVFormat = DXGI_FORMAT_UNKNOWN;
    return SUCCEEDED(device_->CreateGraphicsPipelineState(&description, IID_PPV_ARGS(&trailSceneNoDepthPipeline_)));
}

ID3D12PipelineState* PipelineLibrary::GetObjectScenePipeline(BlendMode blendMode, bool hasDepth)
{
    auto& source = GetPSOObjectForScene(blendMode);
    if (hasDepth || (blendMode != kNone && blendMode != kNormal && blendMode != kAdd)) {
        return source.graphicsState_.Get();
    }
    const size_t index = blendMode == kNone ? 0 : blendMode == kNormal ? 1 : 2;
    auto& pipeline = noDepthObjectPipelines_[index];
    if (!pipeline) {
        auto description = source.graphicsDesc_;
        description.DepthStencilState.DepthEnable = FALSE;
        description.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        description.DSVFormat = DXGI_FORMAT_UNKNOWN;
        const HRESULT result = device_->CreateGraphicsPipelineState(&description, IID_PPV_ARGS(&pipeline));
        assert(SUCCEEDED(result));
        if (FAILED(result)) {
            return nullptr;
        }
    }
    return pipeline.Get();
}

} // namespace myEngine
