#include "PipelineLibrary.h"
#include "DirectXCommon.h"
#include "ResourcePaths.h"

#include <cassert>
#include "Bloom.h"
#include "StartupTrace.h"
#include "Utf8Path.h"

namespace myEngine {

using namespace Microsoft::WRL;

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
        switch (pso.postEffectType_) {
        case Bloom_Extract:
            pso.psFilePath_ = ResourcePaths::EngineShader(L"BloomExtract.PS.hlsl");
            break;
        case Bloom_Downsample:
            pso.psFilePath_ = ResourcePaths::EngineShader(L"BloomDownsample.PS.hlsl");
            break;
        case Bloom_BlurH:
            pso.psFilePath_ = ResourcePaths::EngineShader(L"BloomBlurH.PS.hlsl");
            break;
        case Bloom_BlurV:
            pso.psFilePath_ = ResourcePaths::EngineShader(L"BloomBlurV.PS.hlsl");
            break;
        case Gaussian_Filter:
            pso.psFilePath_ = ResourcePaths::EngineShader(L"GaussianFilter.PS.hlsl");
            break;
        case Bloom_Composite:
            pso.psFilePath_ = ResourcePaths::EngineShader(L"Composite.PS.hlsl");
            break;
        case ObjectPost_Composite:
            pso.psFilePath_ = ResourcePaths::EngineShader(L"ObjectPostComposite.PS.hlsl");
            break;
        case ObjectPost_OutlineAdd:
            pso.psFilePath_ = ResourcePaths::EngineShader(L"ObjectPostOutlineAdd.PS.hlsl");
            break;
        case ObjectPost_BloomAdd:
            pso.psFilePath_ = ResourcePaths::EngineShader(L"ObjectPostBloomAdd.PS.hlsl");
            break;
        case Random:
            pso.psFilePath_ = ResourcePaths::EngineShader(L"Random.PS.hlsl");
            break;
        case SSAO_Resolve:
            pso.psFilePath_ = ResourcePaths::EngineShader(L"SSAOResolve.PS.hlsl");
            break;
        case SSAO_Denoise:
            pso.psFilePath_ = ResourcePaths::EngineShader(L"SSAODenoise.PS.hlsl");
            break;
        case SSR_Resolve:
            pso.psFilePath_ = ResourcePaths::EngineShader(L"SSRResolve.PS.hlsl");
            break;
        case SSR_Denoise:
            pso.psFilePath_ = ResourcePaths::EngineShader(L"SSRDenoise.PS.hlsl");
            break;
        case MotionVector_Resolve:
            pso.psFilePath_ = ResourcePaths::EngineShader(L"MotionVectorResolve.PS.hlsl");
            break;
        case Temporal_Resolve:
            pso.psFilePath_ = ResourcePaths::EngineShader(L"TemporalResolve.PS.hlsl");
            break;
        case Phosphor_Persistence:
            pso.psFilePath_ = ResourcePaths::EngineShader(L"PhosphorPersistence.PS.hlsl");
            break;
        case Crt_Present:
            pso.psFilePath_ = ResourcePaths::EngineShader(L"CrtPresent.PS.hlsl");
            break;
        }
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
        pso.graphicsDesc_.BlendState.RenderTarget[1].BlendEnable = FALSE;
        pso.graphicsDesc_.BlendState.RenderTarget[1].SrcBlend = D3D12_BLEND_ONE;
        pso.graphicsDesc_.BlendState.RenderTarget[1].DestBlend = D3D12_BLEND_ZERO;
        pso.graphicsDesc_.BlendState.RenderTarget[1].BlendOp = D3D12_BLEND_OP_ADD;
        pso.graphicsDesc_.BlendState.RenderTarget[1].SrcBlendAlpha = D3D12_BLEND_ONE;
        pso.graphicsDesc_.BlendState.RenderTarget[1].DestBlendAlpha = D3D12_BLEND_ZERO;
        pso.graphicsDesc_.BlendState.RenderTarget[1].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        pso.graphicsDesc_.BlendState.RenderTarget[1].RenderTargetWriteMask =
            (blendMode == kNone && pso.shaderType_ != Skybox) ? D3D12_COLOR_WRITE_ENABLE_ALL : 0;
        pso.graphicsDesc_.BlendState.RenderTarget[2].BlendEnable = FALSE;
        pso.graphicsDesc_.BlendState.RenderTarget[2].SrcBlend = D3D12_BLEND_ONE;
        pso.graphicsDesc_.BlendState.RenderTarget[2].DestBlend = D3D12_BLEND_ZERO;
        pso.graphicsDesc_.BlendState.RenderTarget[2].BlendOp = D3D12_BLEND_OP_ADD;
        pso.graphicsDesc_.BlendState.RenderTarget[2].SrcBlendAlpha = D3D12_BLEND_ONE;
        pso.graphicsDesc_.BlendState.RenderTarget[2].DestBlendAlpha = D3D12_BLEND_ZERO;
        pso.graphicsDesc_.BlendState.RenderTarget[2].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        pso.graphicsDesc_.BlendState.RenderTarget[2].RenderTargetWriteMask =
            (blendMode == kNone && pso.shaderType_ != Skybox) ? D3D12_COLOR_WRITE_ENABLE_ALL : 0;
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
    objectPSO_None.shaderType_ = Object;
    objectPSO_Alpha.shaderType_ = Object;
    objectPSO_Add.shaderType_ = Object;
    objectPSO_None_HDR.shaderType_ = Object;
    objectPSO_Alpha_HDR.shaderType_ = Object;
    objectPSO_Add_HDR.shaderType_ = Object;
    psoParticle_.shaderType_ = Particle;
    psoModelParticle_.shaderType_ = ModelParticle;
    psoModelParticle_HDR.shaderType_ = ModelParticle;
    psoComputeParticle_.shaderType_ = ComputeParticle;
    psoScanParticle_.shaderType_ = ComputeParticle;
    psoCompactParticle_.shaderType_ = ComputeParticle;
    psoInitializeParticle_.shaderType_ = ComputeParticle;
    psoEmitParticle_.shaderType_ = ComputeParticle;
    psoEmitBatchParticle_.shaderType_ = ComputeParticle;
    bloomPSO.shaderType_ = PostEffect;
    blurHPSO.shaderType_ = PostEffect;
    blurVPSO.shaderType_ = PostEffect;
    bloomPSO_HDR.shaderType_ = PostEffect;
    blurHPSO_HDR.shaderType_ = PostEffect;
    blurVPSO_HDR.shaderType_ = PostEffect;
    gaussianFilterPSO.shaderType_ = PostEffect;
    compositePSO.shaderType_ = PostEffect;
    objectPostCompositePSO.shaderType_ = PostEffect;
    objectPostOutlineAddPSO.shaderType_ = PostEffect;
    objectPostBloomAddPSO.shaderType_ = PostEffect;
    randomPSO.shaderType_ = PostEffect;
    ssaoResolvePSO.shaderType_ = PostEffect;
    ssaoDenoisePSO.shaderType_ = PostEffect;
    ssrResolvePSO.shaderType_ = PostEffect;
    ssrDenoisePSO.shaderType_ = PostEffect;
    motionVectorResolvePSO.shaderType_ = PostEffect;
    temporalResolvePSO.shaderType_ = PostEffect;
    phosphorPersistencePSO.shaderType_ = PostEffect;
    crtPresentPSO.shaderType_ = PostEffect;
    objectPostCompositePSO_HDR.shaderType_ = PostEffect;
    objectPostOutlineAddPSO_HDR.shaderType_ = PostEffect;
    objectPostBloomAddPSO_HDR.shaderType_ = PostEffect;
    randomPSO_HDR.shaderType_ = PostEffect;
    ssrResolvePSO_HDR.shaderType_ = PostEffect;
    ssrDenoisePSO_HDR.shaderType_ = PostEffect;
    downsamplePSO.shaderType_ = PostEffect;
    downsamplePSO_HDR.shaderType_ = PostEffect;
    shadowPSO.shaderType_ = Shadow;
    trailPSO.shaderType_ = Trail;
    trailPSO_HDR.shaderType_ = Trail;
    hudRectPSO.shaderType_ = Trail;
    skyboxPSO.shaderType_ = Skybox;
    skyboxPSO_HDR.shaderType_ = Skybox;
    skinningPSO.shaderType_ = Skinning;
    skinningDoubleSidedPSO.shaderType_ = Skinning;
    skinningPSO_HDR.shaderType_ = Skinning;
    skinningDoubleSidedPSO_HDR.shaderType_ = Skinning;
    skinningShadowPSO.shaderType_ = SkinningShadow;

    bloomPSO.postEffectType_ = Bloom_Extract;
    blurHPSO.postEffectType_ = Bloom_BlurH;
    blurVPSO.postEffectType_ = Bloom_BlurV;
    bloomPSO_HDR.postEffectType_ = Bloom_Extract;
    blurHPSO_HDR.postEffectType_ = Bloom_BlurH;
    blurVPSO_HDR.postEffectType_ = Bloom_BlurV;
    gaussianFilterPSO.postEffectType_ = Gaussian_Filter;
    compositePSO.postEffectType_ = Bloom_Composite;
    objectPostCompositePSO.postEffectType_ = ObjectPost_Composite;
    objectPostOutlineAddPSO.postEffectType_ = ObjectPost_OutlineAdd;
    objectPostBloomAddPSO.postEffectType_ = ObjectPost_BloomAdd;
    randomPSO.postEffectType_ = Random;
    ssaoResolvePSO.postEffectType_ = SSAO_Resolve;
    ssaoDenoisePSO.postEffectType_ = SSAO_Denoise;
    ssrResolvePSO.postEffectType_ = SSR_Resolve;
    ssrDenoisePSO.postEffectType_ = SSR_Denoise;
    motionVectorResolvePSO.postEffectType_ = MotionVector_Resolve;
    temporalResolvePSO.postEffectType_ = Temporal_Resolve;
    phosphorPersistencePSO.postEffectType_ = Phosphor_Persistence;
    crtPresentPSO.postEffectType_ = Crt_Present;
    objectPostCompositePSO_HDR.postEffectType_ = ObjectPost_Composite;
    objectPostOutlineAddPSO_HDR.postEffectType_ = ObjectPost_OutlineAdd;
    objectPostBloomAddPSO_HDR.postEffectType_ = ObjectPost_BloomAdd;
    randomPSO_HDR.postEffectType_ = Random;
    ssrResolvePSO_HDR.postEffectType_ = SSR_Resolve;
    ssrDenoisePSO_HDR.postEffectType_ = SSR_Denoise;
    downsamplePSO.postEffectType_ = Bloom_Downsample;
    downsamplePSO_HDR.postEffectType_ = Bloom_Downsample;

    CreateShaderCommon(objectPSO_None, kNone);
    CreateShaderCommon(objectPSO_Alpha, kNormal);
    CreateShaderCommon(objectPSO_Add, kAdd);
    co_await std::suspend_always{};
    CreateShaderCommon(objectPSO_None_HDR, kNone, false, kSceneRenderTargetFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(objectPSO_Alpha_HDR, kNormal, false, kSceneRenderTargetFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(objectPSO_Add_HDR, kAdd, false, kSceneRenderTargetFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(psoParticle_, kAdd);
    co_await std::suspend_always{};
    CreateShaderCommon(psoModelParticle_, kAdd);
    co_await std::suspend_always{};
    CreateShaderCommon(psoModelParticle_HDR, kAdd, false, kSceneRenderTargetFormat);
    co_await std::suspend_always{};
    CreateComputeShaderCommon(psoInitializeParticle_, ResourcePaths::EngineShader(L"ParticleInitialize.CS.hlsl"));
    co_await std::suspend_always{};
    CreateComputeShaderCommon(psoEmitParticle_, ResourcePaths::EngineShader(L"ParticleEmit.CS.hlsl"));
    co_await std::suspend_always{};
    CreateComputeShaderCommon(psoEmitBatchParticle_, ResourcePaths::EngineShader(L"ParticleEmitBatch.CS.hlsl"));
    co_await std::suspend_always{};
    CreateComputeShaderCommon(psoComputeParticle_, ResourcePaths::EngineShader(L"ParticleUpdate.CS.hlsl"));
    co_await std::suspend_always{};
    CreateComputeShaderCommon(psoScanParticle_, ResourcePaths::EngineShader(L"ParticleScan.CS.hlsl"));
    co_await std::suspend_always{};
    CreateComputeShaderCommon(psoCompactParticle_, ResourcePaths::EngineShader(L"ParticleCompact.CS.hlsl"));
    co_await std::suspend_always{};
    CreateShaderCommon(bloomPSO, kNone);
    co_await std::suspend_always{};
    CreateShaderCommon(bloomPSO_HDR, kNone, false, kSceneRenderTargetFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(blurHPSO, kNone);
    co_await std::suspend_always{};
    CreateShaderCommon(blurHPSO_HDR, kNone, false, kSceneRenderTargetFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(blurVPSO, kNone);
    co_await std::suspend_always{};
    CreateShaderCommon(blurVPSO_HDR, kNone, false, kSceneRenderTargetFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(gaussianFilterPSO, kNone);
    co_await std::suspend_always{};
    CreateShaderCommon(compositePSO, kAdd);
    co_await std::suspend_always{};
    CreateShaderCommon(objectPostCompositePSO, kNormal);
    co_await std::suspend_always{};
    CreateShaderCommon(objectPostOutlineAddPSO, kAdd);
    co_await std::suspend_always{};
    CreateShaderCommon(objectPostBloomAddPSO, kAdd);
    co_await std::suspend_always{};
    CreateShaderCommon(objectPostCompositePSO_HDR, kNormal, false, kSceneRenderTargetFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(objectPostOutlineAddPSO_HDR, kAdd, false, kSceneRenderTargetFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(objectPostBloomAddPSO_HDR, kAdd, false, kSceneRenderTargetFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(randomPSO, kNone);
    co_await std::suspend_always{};
    CreateShaderCommon(randomPSO_HDR, kNone, false, kSceneRenderTargetFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(ssaoResolvePSO, kNone, false, kAmbientOcclusionBufferFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(ssaoDenoisePSO, kNone, false, kAmbientOcclusionBufferFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(ssrResolvePSO, kNone);
    co_await std::suspend_always{};
    CreateShaderCommon(ssrResolvePSO_HDR, kNone, false, kSceneRenderTargetFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(ssrDenoisePSO, kNone);
    co_await std::suspend_always{};
    CreateShaderCommon(ssrDenoisePSO_HDR, kNone, false, kSceneRenderTargetFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(motionVectorResolvePSO, kNone, false, kMotionVectorBufferFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(temporalResolvePSO, kNone, false, kSceneRenderTargetFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(phosphorPersistencePSO, kNone, false, kSceneRenderTargetFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(crtPresentPSO, kNone);
    co_await std::suspend_always{};
    CreateShaderCommon(downsamplePSO, kNone);
    co_await std::suspend_always{};
    CreateShaderCommon(downsamplePSO_HDR, kNone, false, kSceneRenderTargetFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(shadowPSO, kShadow);
    co_await std::suspend_always{};
    CreateShaderCommon(trailPSO, kAdd);
    co_await std::suspend_always{};
    CreateShaderCommon(trailPSO_HDR, kAdd, false, kSceneRenderTargetFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(hudRectPSO, kNormal);
    co_await std::suspend_always{};
    CreateShaderCommon(skyboxPSO, kNone);
    co_await std::suspend_always{};
    CreateShaderCommon(skyboxPSO_HDR, kNone, false, kSceneRenderTargetFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(skinningPSO, kNone);
    co_await std::suspend_always{};
    CreateShaderCommon(skinningDoubleSidedPSO, kNone, true);
    co_await std::suspend_always{};
    CreateShaderCommon(skinningPSO_HDR, kNone, false, kSceneRenderTargetFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(skinningDoubleSidedPSO_HDR, kNone, true, kSceneRenderTargetFormat);
    co_await std::suspend_always{};
    CreateShaderCommon(skinningShadowPSO, kShadow);
    co_await std::suspend_always{};
}

bool PipelineLibrary::InitializeTrailSceneNoDepthPipeline()
{
    if (trailSceneNoDepthPipeline_) {
        return true;
    }
    if (!device_ || !trailPSO_HDR.graphicsState_) {
        return false;
    }
    auto description = trailPSO_HDR.graphicsDesc_;
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
