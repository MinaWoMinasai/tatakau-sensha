#include "DirectXCommon.h"
#include "ResourcePaths.h"

#include <cassert>
#include "Bloom.h"
#include "ShaderDiskCache.h"
#include "StartupTrace.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

namespace myEngine {

using namespace Microsoft::WRL;

void DirectXCommon::Initialize(WinApp* winApp, bool titleFirst)
{
    StartupTrace::Scope startupScope("DirectXCommon.Initialize");
    assert(winApp);
    winApp_ = winApp;

    // FPS固定初期化
    InitializeFixFPS();

    {
        StartupTrace::Scope scope("DirectXCommon.Device");
        InitializeDevice();
    }
    InititalizeCommand();
    CreateSwapChain();
    CreateDepthBuffer();
    CreateDescriptorHeap();
    CreateSwapChainRtv();
    InitializeDepthStencilView();
    InitializeFence();

    CreateDXCCompiler();
    InitializeImGui();

    pipelineTask_ = BuildPipelines();
    if (titleFirst) {
        InitializePipelinesStep(); // スプライト用の3つのパイプラインだけ。
    } else {
        CreateShader();
    }
}

void DirectXCommon::PreDraw()
{
    // これから書き込むバックバッファのインデックスを取得
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
    // TransitionBarrierの設定
    D3D12_RESOURCE_BARRIER barrier{};
    // 今回のバリアはTransition
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    // Noneにしておく
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    // バリアを張る対象のリソース。現在のバックバッファに対して行う
    barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();
    // 遷移前(現在)のResourceState
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    // 遷移後のResourceState
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    // TransitionBarrierを張る
    list_->ResourceBarrier(1, &barrier);
    // 描画先のRTVとDSVを設定する
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    SetRenderTarget(rtvHandles_[backBufferIndex], dsvHandle);
    // 指定した色で画面全体をクリアする
    float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};

    list_->ClearRenderTargetView(rtvHandles_[backBufferIndex], clearColor, 0, nullptr);
    // 指定して深度で画面全体をクリアする
    list_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    SetViewport(WinApp::kRenderWidth, WinApp::kRenderHeight);
}

void DirectXCommon::PostDraw()
{
    // これから書き込むバックバッファのインデックスを取得
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
    // TransitionBarrierの設定
    D3D12_RESOURCE_BARRIER barrier{};
    // 今回のバリアはTransition
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    // Noneにしておく
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    // バリアを張る対象のリソース。現在のバックバッファに対して行う
    barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();
    // 遷移前(現在)のResourceState
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    // 遷移後のResourceState
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    // TranssitionBarrierを張る
    list_->ResourceBarrier(1, &barrier);

    CommandListExecuteAndReset();
}

void DirectXCommon::CreateShaderCommon(PSO& pso, BlendMode blendMode, bool doubleSided, DXGI_FORMAT renderTargetFormat)
{
    const bool usesSceneNormalTarget = renderTargetFormat == kSceneRenderTargetFormat &&
                                       (pso.shaderType_ == Object || pso.shaderType_ == Skinning || pso.shaderType_ == Skybox ||
                                        pso.shaderType_ == Trail || pso.shaderType_ == ModelParticle);

    // 1. 各タイプごとのシェーダーパスとルートシグネチャ初期化
    switch (pso.shaderType_) {
    case Object:
        pso.root_.InitalizeForObject();
        pso.vsFilePath_ = ResourcePaths::EngineShader(L"Object3d.VS.hlsl");
        pso.psFilePath_ = usesSceneNormalTarget ? ResourcePaths::EngineShader(L"Object3d.Scene.PS.hlsl")
                                                : ResourcePaths::EngineShader(L"Object3d.PS.hlsl");
        break;
    case Particle:
        pso.root_.InitalizeForParticle();
        pso.vsFilePath_ = ResourcePaths::EngineShader(L"Particle.VS.hlsl");
        pso.psFilePath_ = ResourcePaths::EngineShader(L"Particle.PS.hlsl");
        break;
    case ModelParticle:
        pso.root_.InitalizeForModelParticle();
        pso.vsFilePath_ = ResourcePaths::EngineShader(L"ModelParticle.VS.hlsl");
        pso.psFilePath_ = usesSceneNormalTarget ? ResourcePaths::EngineShader(L"ModelParticle.Scene.PS.hlsl")
                                                : ResourcePaths::EngineShader(L"ModelParticle.PS.hlsl");
        break;
    case ComputeParticle:
        assert(false);
        break;
    case Shadow:
        pso.root_.InitalizeForShadow();
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
        pso.root_.InitalizeForTrail(); // 上で作った関数
        pso.vsFilePath_ = ResourcePaths::EngineShader(L"Trail.VS.hlsl");
        pso.psFilePath_ =
            usesSceneNormalTarget ? ResourcePaths::EngineShader(L"Trail.Scene.PS.hlsl") : ResourcePaths::EngineShader(L"Trail.PS.hlsl");
        break;
    case Skybox:                         // ★追加
        pso.root_.InitializeForSkybox(); // 前回作成した関数
        pso.vsFilePath_ = ResourcePaths::EngineShader(L"Skybox.VS.hlsl");
        pso.psFilePath_ =
            usesSceneNormalTarget ? ResourcePaths::EngineShader(L"Skybox.Scene.PS.hlsl") : ResourcePaths::EngineShader(L"Skybox.PS.hlsl");
        break;
    case Skinning:
        pso.root_.InitalizeForObject();
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
        std::filesystem::path(pso.psFilePath_.empty() ? pso.vsFilePath_ : pso.psFilePath_).filename().string() +
        "/format=" + std::to_string(renderTargetFormat) + "/blend=" + std::to_string(blendMode) + (doubleSided ? "/doubleSided" : "");
    StartupTrace::Scope pipelineScope("Pipeline.Prepare/" + pipelineName);
    StartupTrace::Count("pipeline.graphics.created");

    // 2. ルートシグネチャ生成
    {
        StartupTrace::Scope scope("Pipeline.RootSignature");
        pso.root_.Create(device_);
    }

    // 3. シェーダーコンパイル
    pso.vertexShaderBlob_ = CompileShader(pso.vsFilePath_, L"vs_6_0");
    assert(pso.vertexShaderBlob_ != nullptr);

    pso.pixelShaderBlob_ = nullptr;
    if (pso.shaderType_ != Shadow && pso.shaderType_ != SkinningShadow && !pso.psFilePath_.empty()) {
        pso.pixelShaderBlob_ = CompileShader(pso.psFilePath_, L"ps_6_0");
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

        // ★ Skybox用の特殊設定
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

        // ★ 軌跡用の特殊設定
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
        // ★ここを確実に設定！
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

void DirectXCommon::CreateComputeShaderCommon(PSO& pso, const std::wstring& shaderPath)
{
    const std::string pipelineName = std::filesystem::path(shaderPath).filename().string();
    StartupTrace::Scope pipelineScope("Pipeline.Prepare/" + pipelineName);
    StartupTrace::Count("pipeline.compute.created");
    pso.root_.InitializeForComputeParticle();
    pso.root_.Create(device_);
    pso.computeShaderBlob_ = CompileShader(shaderPath, L"cs_6_0");
    assert(pso.computeShaderBlob_ != nullptr);

    ZeroMemory(&pso.computeDesc_, sizeof(pso.computeDesc_));
    pso.computeDesc_.pRootSignature = pso.root_.GetSignature().Get();
    pso.computeDesc_.CS = {pso.computeShaderBlob_->GetBufferPointer(), pso.computeShaderBlob_->GetBufferSize()};

    StartupTrace::Scope createScope("Pipeline.DriverCompute/" + pipelineName);
    HRESULT hr = device_->CreateComputePipelineState(&pso.computeDesc_, IID_PPV_ARGS(&pso.computeState_));
    assert(SUCCEEDED(hr));
}

void DirectXCommon::CreateShader()
{
    StartupTrace::Scope scope("DirectXCommon.AllPipelines");
    while (!InitializePipelinesStep()) {
    }
}

bool DirectXCommon::InitializePipelinesStep()
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

InitializationTask DirectXCommon::BuildPipelines()
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
    conpositePSO.shaderType_ = PostEffect;
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
    conpositePSO.postEffectType_ = Bloom_Composite;
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
    CreateShaderCommon(conpositePSO, kAdd);
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

bool DirectXCommon::InitializeTrailSceneNoDepthPipeline()
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

ID3D12PipelineState* DirectXCommon::GetObjectScenePipeline(BlendMode blendMode)
{
    auto& source = GetPSOObjectForScene(blendMode);
    if (currentHasDsv_ || (blendMode != kNone && blendMode != kNormal && blendMode != kAdd)) {
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

void DirectXCommon::CreateGraphics() {}

void DirectXCommon::InitializeDevice()
{
    HRESULT hr;

    char debugLayerValue[8]{};
    const DWORD debugLayerValueLength =
        GetEnvironmentVariableA("MYENGINE_D3D12_DEBUG_LAYER", debugLayerValue, static_cast<DWORD>(std::size(debugLayerValue)));
    // 未設定と "1" は従来どおりON。明示的な "0" だけOFFにする。
    const bool debugLayerRequested = !(debugLayerValueLength == 1 && debugLayerValue[0] == '0');

    Microsoft::WRL::ComPtr<ID3D12Debug1> debugComtroller = nullptr;
    if (debugLayerRequested && SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugComtroller)))) {
        // デバッグレイヤーを有効化する
        debugComtroller->EnableDebugLayer();
        d3d12DebugLayerEnabled_ = true;
        // GPU-Based Validation は描画命令数に比例して極端に重くなるため、必要時のみ有効化する。
        char gpuValidationValue[8]{};
        const DWORD valueLength =
            GetEnvironmentVariableA("MYENGINE_GPU_BASED_VALIDATION", gpuValidationValue, static_cast<DWORD>(std::size(gpuValidationValue)));
        gpuBasedValidationEnabled_ = valueLength > 0 && gpuValidationValue[0] == '1';
        debugComtroller->SetEnableGPUBasedValidation(gpuBasedValidationEnabled_ ? TRUE : FALSE);
    }

    hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
    assert(SUCCEEDED(hr));

    // 良い順にアダプタを組む
    for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter_)) !=
                     DXGI_ERROR_NOT_FOUND;
         ++i) {
        // アダプターの情報を取得する
        DXGI_ADAPTER_DESC3 adapterDesc{};
        HRESULT hr = useAdapter_->GetDesc3(&adapterDesc);
        assert(SUCCEEDED(hr)); // 取得できないのは一大事
        // ソフトウェアアダプタでなければ採用!
        if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
            // 採用したアダプタの情報をログに出力。wstringのほうなので注意

            break;
        }
        useAdapter_ = nullptr; // ソフトウェアアダプタの場合は見なかったことにする
    }
    // 適切なアダプタが見当たらなかったので起動できない
    assert(useAdapter_ != nullptr);

    // 機能レベルとログ出力用の文字列
    D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0};
    const char* featrueLevelStrings[] = {"12.2", "12.1", "12.0"};
    // 高い順に生成できるか試していく
    for (size_t i = 0; i < _countof(featureLevels); ++i) {
        // 採用したアダプターでデバイスを生成
        hr = D3D12CreateDevice(useAdapter_.Get(), featureLevels[i], IID_PPV_ARGS(&device_));
        // 指定した操縦レベルでデバイスが生成できたかを確認
        if (SUCCEEDED(hr)) {
            // 生成できたのでログ出力を行ってループを抜ける

            break;
        }
    }
    // デバイスの生成が上手くいかなかったので起動できない
    assert(device_ != nullptr);
    //Log("Complete create D3D12Device!!!\n");// 初期化完了のログを出す

#ifdef _DEBUG

    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
    if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        // やばいエラー時にとまる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        // エラー時にとまる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        // 警告時にとまる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
        // 抑制するメッセージのID
        D3D12_MESSAGE_ID denyIds[] = {// Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
                                      // https://stackoverflow.com/qiestions/69805245/direct-12-application-is-crashing-in-windows-11
                                      D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE

        };

        // 抑制するレベル
        D3D12_MESSAGE_SEVERITY severities[] = {D3D12_MESSAGE_SEVERITY_INFO};
        D3D12_INFO_QUEUE_FILTER filter{};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        filter.DenyList.NumSeverities = _countof(severities);
        filter.DenyList.pSeverityList = severities;
        // 指定したメッセージの表示を抑制する
        infoQueue->PushStorageFilter(&filter);
    }
#endif
}

void DirectXCommon::InititalizeCommand()
{
    HRESULT hr;

    // コマンドキューを生成する
    hr = device_->CreateCommandQueue(&queueDesc_, IID_PPV_ARGS(&queue_));
    // コマンドキューの生成が上手くいかなかったので起動できない
    assert(SUCCEEDED(hr));

    // コマンドアロケータを生成する
    hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator_));
    // コマンドアロケータの生成が上手くいかなかったので起動できない
    assert(SUCCEEDED(hr));

    // コマンドリストを生成する
    hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(), nullptr, IID_PPV_ARGS(&list_));
    // コマンドリストの生成が上手くいかなかったので起動できない
    assert(SUCCEEDED(hr));
}

void DirectXCommon::CreateSwapChain()
{
    swapChainDesc_.Width = WinApp::kRenderWidth;                  // 画面の幅。ウィンドウのクライアント領域を同じものにしておく
    swapChainDesc_.Height = WinApp::kRenderHeight;                // 画面の高さ。ウィンドウのクライアント領域を同じものにしておく
    swapChainDesc_.Format = DXGI_FORMAT_R8G8B8A8_UNORM;           // 色の形式
    swapChainDesc_.SampleDesc.Count = 1;                          // マルチサンプルしない
    swapChainDesc_.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとして利用する
    swapChainDesc_.BufferCount = 2;                               // ダブルバッファ
    swapChainDesc_.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;    // モニタをうつしたら、中身を破棄

    // コマンドキュー、ウィンドウハンドル、設定を渡して生成する
    HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(queue_.Get(), winApp_->GetHwnd(), &swapChainDesc_, nullptr, nullptr,
                                                      reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf()));
    assert(SUCCEEDED(hr));

    dxgiFactory_->MakeWindowAssociation(winApp_->GetHwnd(), DXGI_MWA_NO_ALT_ENTER);
}

void DirectXCommon::CreateDepthBuffer()
{
    // 生成するResourceの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = WinApp::kRenderWidth;                    // テクスチャの幅
    resourceDesc.Height = WinApp::kRenderHeight;                  // テクスチャの高さ
    resourceDesc.MipLevels = 1;                                   // mipmapの数
    resourceDesc.DepthOrArraySize = 1;                            // 奥行き or 配列Textureの配列数
    resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;          // TextureのFormat
    resourceDesc.SampleDesc.Count = 1;                            // サンプリングカウント。1固定。
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;  // 2次元
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DepthStencilとして使う通知

    // 利用するHeapの設定
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作る
    // 深度値のクリア設定
    D3D12_CLEAR_VALUE depthClearValue{};
    depthClearValue.DepthStencil.Depth = 1.0f;              // 1.0f(最大値)でクリア
    depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // フォーマット。Resourceとあわせる

    // Resourceの生成
    HRESULT hr = device_->CreateCommittedResource(&heapProperties,                       // Heapの設定
                                                  D3D12_HEAP_FLAG_NONE,                  // Heapの特殊な設定。特になし。
                                                  &resourceDesc,                         // Resourceの設定
                                                  D3D12_RESOURCE_STATE_DEPTH_WRITE,      // 初回のResourceState。Textureは基本読むだけ
                                                  &depthClearValue,                      // Clear最適地。
                                                  IID_PPV_ARGS(&depthStencilResource_)); // 作成するResourceポインタへのポインタ
    assert(SUCCEEDED(hr));
}

void DirectXCommon::CreateDescriptorHeap()
{
    descriptorSizeRTV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    descriptorSizeDSV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    // RTV用のヒープでディスクリプタの数は2。RTVはShader内で触るものではないので、ShaderVisibleは false
    rtvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
    // DSV用のヒープでディスクリプタの数は1。DSVはShader内で触るものではないので、ShaderVisibleは false
    dsvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, kMaxDsvCount, false);
}

void DirectXCommon::CreateSwapChainRtv()
{
    // SwapChainからResourceを引っ張ってくる
    HRESULT hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&swapChainResources_[0]));
    // 上手く取得できなければ起動できない
    assert(SUCCEEDED(hr));
    hr = swapChain_->GetBuffer(1, IID_PPV_ARGS(&swapChainResources_[1]));
    assert(SUCCEEDED(hr));

    // RTVの設定
    rtvDesc_.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;      //
    rtvDesc_.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; //
    // ディスクリプタの先頭を取得する
    D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    //RTVを2つつくるのでディスクリプタを2つ用意
    // まず1つ目をつくる。1つ目は最初の所に作る。作る場所を指定してあげる必要がある
    rtvHandles_[0] = rtvStartHandle;
    device_->CreateRenderTargetView(swapChainResources_[0].Get(), &rtvDesc_, rtvHandles_[0]);
    // 2つ目のディスクリプタハンドルを得る
    rtvHandles_[1].ptr = rtvHandles_[0].ptr + device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    // 2つ目を作る
    device_->CreateRenderTargetView(swapChainResources_[1].Get(), &rtvDesc_, rtvHandles_[1]);

    rtvHandles_[0] = rtvStartHandle;
    rtvHandles_[1].ptr = rtvHandles_[0].ptr + device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void DirectXCommon::InitializeDepthStencilView()
{
    // DSVの設定
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;        // Format。基本的にはResourceに合わせる
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2dTexture
    // DSVHeapの先頭にDSVをつくる
    device_->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());

    // DSVヒープの先頭ハンドルを取得して保持
    dsvHandle_ = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    // 保持したハンドルに対して作成
    device_->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvHandle_);
    dsvHeapIndex_ = 1;
}

void DirectXCommon::InitializeFence()
{
    // 初期値0でFenceを作る
    HRESULT hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    assert(SUCCEEDED(hr));

    // FenceのSignalを持つためのイベントを作成する
    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(fenceEvent_ != nullptr);
}

void DirectXCommon::InitializeViewport()
{
    //クライアント領域のサイズと一緒にして画面全体に表示
    viewportRect_.Width = FLOAT(WinApp::kRenderWidth);
    viewportRect_.Height = FLOAT(WinApp::kRenderHeight);
    viewportRect_.TopLeftX = 0;
    viewportRect_.TopLeftY = 0;
    viewportRect_.MinDepth = 0.0f;
    viewportRect_.MaxDepth = 1.0f;
}

void DirectXCommon::InitializeSissorRect()
{
    //基本的にビューポートと同じ矩形が構成されるようにする
    scissorRect_.left = 0;
    scissorRect_.right = LONG(WinApp::kRenderWidth);
    scissorRect_.top = 0;
    scissorRect_.bottom = LONG(WinApp::kRenderHeight);
}

void DirectXCommon::CreateDXCCompiler()
{
    // dxcompilerを初期化
    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
    assert(SUCCEEDED(hr));
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
    assert(SUCCEEDED(hr));

    // 現時点で includeはしないが、includeに対応するための設定を行っておく
    hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
    assert(SUCCEEDED(hr));
}

void DirectXCommon::InitializeImGui()
{
    // 互換用の空の窓口。実際の初期化はGame::InitializeImGuiが担当する。
}

void DirectXCommon::CommandListExecuteAndReset()
{
    using Clock = std::chrono::steady_clock;
    auto elapsedMs = [](Clock::time_point start, Clock::time_point end) {
        return std::chrono::duration<float, std::milli>(end - start).count();
    };
    frameSubmitProfile_ = {};
    const auto totalStart = Clock::now();

    // コマンドリストの内容を確定させるすべてのコマンドを積んでからCloseすること
    const auto closeStart = Clock::now();
    HRESULT hr = list_->Close();
    assert(SUCCEEDED(hr));
    const auto closeEnd = Clock::now();
    frameSubmitProfile_.closeMs = elapsedMs(closeStart, closeEnd);

    // GPUにコマンドリストの実行を行わせる
    ID3D12CommandList* commandLists[] = {list_.Get()};
    const auto executeStart = Clock::now();
    queue_->ExecuteCommandLists(1, commandLists);
    const auto executeEnd = Clock::now();
    frameSubmitProfile_.executeMs = elapsedMs(executeStart, executeEnd);
    //GPUとOSに画面の交換を行うよう通知する
    const auto presentStart = Clock::now();
    swapChain_->Present(0, 0);
    const auto presentEnd = Clock::now();
    frameSubmitProfile_.presentMs = elapsedMs(presentStart, presentEnd);
    // fenceの値を更新
    fenceValue_++;
    // GPUがここまでたどり着いたときに、Fenceの値を指定して値に代入するようにSignalを送る
    queue_->Signal(fence_.Get(), fenceValue_);
    // Fenceの値が指定したSignal値にたどり着いているか確認する
    // GetCompletedValueの初期値はFence作成時に渡した初期値
    if (fence_->GetCompletedValue() < fenceValue_) {
        // 指定したSignalにたどり着いていないので、たどり着くまで待つようにイベントを設定する
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        // イベント待つ
        const auto fenceStart = Clock::now();
        WaitForSingleObject(fenceEvent_, INFINITE);
        frameSubmitProfile_.fenceWaitMs = elapsedMs(fenceStart, Clock::now());
    }

    // FPS固定
    const auto fpsLimitStart = Clock::now();
    UpdateFixFPS();
    frameSubmitProfile_.fpsLimitMs = elapsedMs(fpsLimitStart, Clock::now());

    // 次のフレーム用のコマンドリストを準備
    const auto resetStart = Clock::now();
    hr = allocator_->Reset();
    assert(SUCCEEDED(hr));
    hr = list_->Reset(allocator_.Get(), nullptr);
    assert(SUCCEEDED(hr));
    const auto totalEnd = Clock::now();
    frameSubmitProfile_.resetMs = elapsedMs(resetStart, totalEnd);
    frameSubmitProfile_.totalMs = elapsedMs(totalStart, totalEnd);
}

void DirectXCommon::ExecuteCommandListAndWait()
{
    // Close
    list_->Close();

    // 実行
    ID3D12CommandList* lists[] = {list_.Get()};
    queue_->ExecuteCommandLists(1, lists);

    // Fence
    fenceValue_++;
    queue_->Signal(fence_.Get(), fenceValue_);
    if (fence_->GetCompletedValue() < fenceValue_) {
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    // Reset
    allocator_->Reset();
    list_->Reset(allocator_.Get(), nullptr);
}

IDxcBlob* DirectXCommon::CompileShader(const std::wstring& filePath, const wchar_t* profile)
{
    const std::wstring cacheKey = filePath + L"|" + profile;
    const auto cached = shaderCache_.find(cacheKey);
    if (cached != shaderCache_.end() && cached->second) {
        StartupTrace::Count("shader.memoryHit");
        cached->second->AddRef();
        return cached->second.Get();
    }

    const auto shaderNameUtf8 = std::filesystem::path(filePath).filename().u8string();
    const std::string shaderName(reinterpret_cast<const char*>(shaderNameUtf8.data()), shaderNameUtf8.size());
    Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource;
    HRESULT hr = dxcUtils_->LoadFile(filePath.c_str(), nullptr, &shaderSource);
    if (FAILED(hr) || !shaderSource) {
        OutputDebugStringW((L"Shader source could not be read: " + filePath + L"\n").c_str());
        assert(false);
        return nullptr;
    }
    DxcBuffer shaderSourceBuffer{};
    shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderSourceBuffer.Size = shaderSource->GetBufferSize();
    shaderSourceBuffer.Encoding = DXC_CP_UTF8;

    LPCWSTR arguments[] = {filePath.c_str(), L"-E",
                           L"main",          L"-T",
                           profile,
#ifdef _DEBUG
                           L"-Zi",           L"-Qembed_debug",
                           L"-Od",
#else
                           L"-O3",
#endif
                           L"-Zpr"};

    static ShaderDiskCache diskCache;
    std::string diskKey;
    if (ShaderDiskCache::Enabled()) {
        StartupTrace::Scope cacheScope("ShaderCache::" + shaderName);
        diskKey = diskCache.MakeKey(filePath, shaderSourceBuffer.Ptr, shaderSourceBuffer.Size, arguments, _countof(arguments));
        auto diskBlob = diskCache.Load(diskKey, includeHandler_, dxcUtils_);
        if (diskBlob) {
            StartupTrace::Count("shader.diskHit");
            shaderCache_[cacheKey] = diskBlob;
            return diskBlob.Detach();
        }
        StartupTrace::Count("shader.diskMiss");
    }

    Microsoft::WRL::ComPtr<ShaderDiskCache::IncludeRecorder> includes;
    if (!diskKey.empty()) {
        includes = Microsoft::WRL::Make<ShaderDiskCache::IncludeRecorder>(includeHandler_);
    }
    Microsoft::WRL::ComPtr<IDxcResult> shaderResult;
    {
        StartupTrace::Scope compileScope("CompileShader::" + shaderName);
        StartupTrace::Count("shader.compile");
        hr = dxcCompiler_->Compile(&shaderSourceBuffer, arguments, _countof(arguments), includes ? includes.Get() : includeHandler_,
                                   IID_PPV_ARGS(&shaderResult));
    }
    if (FAILED(hr) || !shaderResult) {
        assert(false);
        return nullptr;
    }
    Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError;
    shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
    if (shaderError && shaderError->GetStringLength() != 0) {
        OutputDebugStringA(shaderError->GetStringPointer());
        assert(false);
    }
    HRESULT compileStatus = E_FAIL;
    shaderResult->GetStatus(&compileStatus);
    if (FAILED(compileStatus)) {
        return nullptr;
    }
    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
    hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    if (FAILED(hr) || !shaderBlob) {
        assert(false);
        return nullptr;
    }
    if (includes) {
        diskCache.Store(diskKey, *includes.Get(), shaderBlob.Get());
    }
    shaderCache_[cacheKey] = shaderBlob;
    return shaderBlob.Detach();
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors,
                                                                                 bool shaderVisible)
{
    // ディスクリプタヒープの生成
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
    descriptorHeapDesc.Type = heapType;                 // レンダーターゲットビュー用
    descriptorHeapDesc.NumDescriptors = numDescriptors; // ダブルバッファ用に2つ。多くても別に構わない
    descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                             : D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // シェーダーからアクセスできるようにする
    HRESULT hr = device_->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap)); //
    // ディスクリプタヒープが作れなかったので起動できない
    assert(SUCCEEDED(hr));

    return descriptorHeap;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format,
                                                                            D3D12_RESOURCE_FLAGS flags, const D3D12_CLEAR_VALUE* clearValue,
                                                                            D3D12_RESOURCE_STATES initialState)
{
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = flags;

    ComPtr<ID3D12Resource> resource;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    HRESULT hr =
        device_->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, initialState, clearValue, IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));

    return resource;
}

void DirectXCommon::SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle)
{
    currentRtvHandle_ = rtvHandle;
    currentRtvHandles_ = {rtvHandle, {}, {}};
    currentRtvCount_ = 1;
    currentDsvHandle_ = dsvHandle;
    currentHasDsv_ = true;
    list_->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
}

void DirectXCommon::SetRenderTargets(D3D12_CPU_DESCRIPTOR_HANDLE colorRtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE normalRtvHandle,
                                     D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle)
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[] = {
        colorRtvHandle,
        normalRtvHandle,
    };
    currentRtvHandle_ = colorRtvHandle;
    currentRtvHandles_ = {colorRtvHandle, normalRtvHandle, {}};
    currentRtvCount_ = 2;
    currentDsvHandle_ = dsvHandle;
    currentHasDsv_ = true;
    list_->OMSetRenderTargets(_countof(rtvHandles), rtvHandles, false, &dsvHandle);
}

void DirectXCommon::SetRenderTargets(D3D12_CPU_DESCRIPTOR_HANDLE colorRtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE normalRtvHandle,
                                     D3D12_CPU_DESCRIPTOR_HANDLE materialRtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle)
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[] = {
        colorRtvHandle,
        normalRtvHandle,
        materialRtvHandle,
    };
    currentRtvHandle_ = colorRtvHandle;
    currentRtvHandles_ = {colorRtvHandle, normalRtvHandle, materialRtvHandle};
    currentRtvCount_ = 3;
    currentDsvHandle_ = dsvHandle;
    currentHasDsv_ = true;
    list_->OMSetRenderTargets(_countof(rtvHandles), rtvHandles, false, &dsvHandle);
}

void DirectXCommon::SetRenderTargetNoDepth(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle)
{
    currentRtvHandle_ = rtvHandle;
    currentRtvHandles_ = {rtvHandle, {}, {}};
    currentRtvCount_ = 1;
    currentDsvHandle_ = {};
    currentHasDsv_ = false;
    list_->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
}

void DirectXCommon::RestoreRenderTargetState(const RenderTargetState& state)
{
    assert(state.count >= 1 && state.count <= state.rtvs.size());
    currentRtvHandles_ = state.rtvs;
    currentRtvCount_ = state.count;
    currentRtvHandle_ = state.rtvs[0];
    currentDsvHandle_ = state.dsv;
    currentHasDsv_ = state.hasDepth;
    viewportRect_ = state.viewport;
    scissorRect_ = state.scissor;
    list_->OMSetRenderTargets(state.count, state.rtvs.data(), false, state.hasDepth ? &state.dsv : nullptr);
    list_->RSSetViewports(1, &viewportRect_);
    list_->RSSetScissorRects(1, &scissorRect_);
}

void DirectXCommon::ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle)
{
    float clearColor[4] = {0, 0, 0, 1};
    ClearRenderTarget(rtvHandle, clearColor);
}

void DirectXCommon::ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, const float clearColor[4])
{
    list_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
}

void DirectXCommon::ClearDepthBuffer()
{
    list_->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void DirectXCommon::SetBackBuffer()
{
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHandles_[backBufferIndex];
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    currentRtvHandle_ = rtv;
    currentRtvHandles_ = {rtv, {}, {}};
    currentRtvCount_ = 1;
    currentDsvHandle_ = dsv;
    currentHasDsv_ = true;
    list_->OMSetRenderTargets(1, &rtv, false, &dsv);
}

void DirectXCommon::SetPresentTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv)
{
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    currentRtvHandle_ = rtv;
    currentRtvHandles_ = {rtv, {}, {}};
    currentRtvCount_ = 1;
    currentDsvHandle_ = dsv;
    currentHasDsv_ = true;
    list_->OMSetRenderTargets(1, &rtv, false, &dsv);
}

void DirectXCommon::SetViewport(uint32_t width, uint32_t height)
{
    viewportRect_.TopLeftX = 0.0f;
    viewportRect_.TopLeftY = 0.0f;
    viewportRect_.Width = FLOAT(width);
    viewportRect_.Height = FLOAT(height);
    viewportRect_.MinDepth = 0.0f;
    viewportRect_.MaxDepth = 1.0f;

    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = LONG(width);
    scissorRect_.bottom = LONG(height);

    list_->RSSetViewports(1, &viewportRect_);
    list_->RSSetScissorRects(1, &scissorRect_);
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetNewDsvHandle()
{
    assert(dsvHeapIndex_ < kMaxDsvCount);

    // 先頭のハンドルを取得
    D3D12_CPU_DESCRIPTOR_HANDLE handle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    // 現在のインデックス分だけずらす
    handle.ptr += (descriptorSizeDSV * dsvHeapIndex_);

    // 次回のためにインデックスを進める
    dsvHeapIndex_++;

    return handle;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes)
{
    // 頂点リソース用のヒープの設定
    D3D12_HEAP_PROPERTIES uploadHeapProperties{};
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // UploadHeap5を使う
    // 頂点リソースの設定
    D3D12_RESOURCE_DESC vertexResourceDesc{};
    //バッファリソース。テクスチャの場合はまた別の設定をする
    vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vertexResourceDesc.Width = sizeInBytes; //リソースのサイズ
    // バッファの場合はこれらは1にする決まり
    vertexResourceDesc.Height = 1;
    vertexResourceDesc.DepthOrArraySize = 1;
    vertexResourceDesc.MipLevels = 1;
    vertexResourceDesc.SampleDesc.Count = 1;
    // バッファの場合はこれにする決まり
    vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    // 実際に頂点リソースを作る
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = nullptr;
    HRESULT hr = device_->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexResourceDesc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexResource));
    assert(SUCCEEDED(hr));

    return vertexResource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateUAVBufferResource(size_t sizeInBytes, D3D12_RESOURCE_FLAGS flags)
{
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = sizeInBytes;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = flags;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr = device_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON,
                                                  nullptr, IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));

    return resource;
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetDescriptorCPUHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap,
                                                                  uint32_t descriptorSize, uint32_t index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handleCPU.ptr += (descriptorSize * index);
    return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetDescriptorGPUHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap,
                                                                  uint32_t descriptorSize, uint32_t index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handleGPU.ptr += (descriptorSize * index);
    return handleGPU;
}

void DirectXCommon::InitializeFixFPS()
{
    framePacer_.Initialize();
}

void DirectXCommon::ResetFixFPS()
{
    framePacer_.Reset();
}

void DirectXCommon::UpdateFixFPS()
{
    framePacer_.Wait();
}

void DirectXCommon::Release()
{
    pipelineTask_ = {}; // すべてのPSOが揃う前にタイトルが閉じられることがある。
    const auto releaseIfCreated = [](IUnknown* object) {
        if (object) {
            object->Release();
        }
    };

    releaseIfCreated(objectPSO_None.root_.GetSignatureBlob());
    if (objectPSO_None.root_.GetErrorBlob()) {
        objectPSO_None.root_.GetErrorBlob()->Release();
    }
    releaseIfCreated(objectPSO_None.pixelShaderBlob_);
    releaseIfCreated(objectPSO_None.vertexShaderBlob_);

    releaseIfCreated(objectPSO_Alpha.root_.GetSignatureBlob());
    if (objectPSO_Alpha.root_.GetErrorBlob()) {
        objectPSO_Alpha.root_.GetErrorBlob()->Release();
    }
    releaseIfCreated(objectPSO_Alpha.pixelShaderBlob_);
    releaseIfCreated(objectPSO_Alpha.vertexShaderBlob_);

    releaseIfCreated(shadowPSO.root_.GetSignatureBlob());
    if (shadowPSO.root_.GetErrorBlob()) {
        shadowPSO.root_.GetErrorBlob()->Release();
    }
    releaseIfCreated(shadowPSO.vertexShaderBlob_);

    releaseIfCreated(bloomPSO.root_.GetSignatureBlob());
    if (bloomPSO.root_.GetErrorBlob()) {
        bloomPSO.root_.GetErrorBlob()->Release();
    }
    releaseIfCreated(bloomPSO.pixelShaderBlob_);
    releaseIfCreated(bloomPSO.vertexShaderBlob_);

    releaseIfCreated(downsamplePSO.root_.GetSignatureBlob());
    if (downsamplePSO.root_.GetErrorBlob()) {
        downsamplePSO.root_.GetErrorBlob()->Release();
    }
    releaseIfCreated(downsamplePSO.pixelShaderBlob_);
    releaseIfCreated(downsamplePSO.vertexShaderBlob_);

    releaseIfCreated(blurHPSO.root_.GetSignatureBlob());
    if (blurHPSO.root_.GetErrorBlob()) {
        blurHPSO.root_.GetErrorBlob()->Release();
    }
    releaseIfCreated(blurHPSO.pixelShaderBlob_);
    releaseIfCreated(blurHPSO.vertexShaderBlob_);

    releaseIfCreated(blurVPSO.root_.GetSignatureBlob());
    if (blurVPSO.root_.GetErrorBlob()) {
        blurVPSO.root_.GetErrorBlob()->Release();
    }
    releaseIfCreated(blurVPSO.pixelShaderBlob_);
    releaseIfCreated(blurVPSO.vertexShaderBlob_);

    releaseIfCreated(gaussianFilterPSO.root_.GetSignatureBlob());
    if (gaussianFilterPSO.root_.GetErrorBlob()) {
        gaussianFilterPSO.root_.GetErrorBlob()->Release();
    }
    releaseIfCreated(gaussianFilterPSO.pixelShaderBlob_);
    releaseIfCreated(gaussianFilterPSO.vertexShaderBlob_);

    releaseIfCreated(conpositePSO.root_.GetSignatureBlob());
    if (conpositePSO.root_.GetErrorBlob()) {
        conpositePSO.root_.GetErrorBlob()->Release();
    }
    releaseIfCreated(conpositePSO.pixelShaderBlob_);
    releaseIfCreated(conpositePSO.vertexShaderBlob_);

    releaseIfCreated(randomPSO.root_.GetSignatureBlob());
    if (randomPSO.root_.GetErrorBlob()) {
        randomPSO.root_.GetErrorBlob()->Release();
    }
    releaseIfCreated(randomPSO.pixelShaderBlob_);
    releaseIfCreated(randomPSO.vertexShaderBlob_);

    releaseIfCreated(trailPSO.root_.GetSignatureBlob());
    if (trailPSO.root_.GetErrorBlob()) {
        trailPSO.root_.GetErrorBlob()->Release();
    }
    releaseIfCreated(trailPSO.pixelShaderBlob_);
    releaseIfCreated(trailPSO.vertexShaderBlob_);

    releaseIfCreated(hudRectPSO.root_.GetSignatureBlob());
    if (hudRectPSO.root_.GetErrorBlob()) {
        hudRectPSO.root_.GetErrorBlob()->Release();
    }
    releaseIfCreated(hudRectPSO.pixelShaderBlob_);
    releaseIfCreated(hudRectPSO.vertexShaderBlob_);
}

} // namespace myEngine
