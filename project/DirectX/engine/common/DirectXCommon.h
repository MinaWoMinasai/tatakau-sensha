#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include "WinApp.h"
#include <dxcapi.h>
#include <dxgidebug.h>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <vector>
#include <array>
#include "Root.h"
#include "InputDesc.h"
#include "Calculation.h"
#include "Struct.h"
#include "Texture.h"
#include "FramePacer.h"
#include "InitializationTask.h"

namespace myEngine {

/// @brief 終了時にDirectXの生存リソースを報告し、解放漏れの調査に使う。
struct D3DResourceLeakChecker {
    /// @brief この型の終了処理を行う。所有している資源の寿命を終了させる。
    ~D3DResourceLeakChecker()
    {
        // リソースリークチェック
        Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
            debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
            debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
            debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
        }
    }
};

/// @brief DirectXリソースへのComPtrを保持し、参照カウントで寿命を共有する。
class ResourceObject {
public:
    /// @brief インスタンスの初期値と利用先を設定する。
    ResourceObject(Microsoft::WRL::ComPtr<ID3D12Resource>& resource) : resource_(resource) {}

    /// @brief この型の終了処理を行う。所有している資源の寿命を終了させる。
    ~ResourceObject() = default;

    /// @brief 保持している値またはリソースを返す。
    Microsoft::WRL::ComPtr<ID3D12Resource> Get()
    {
        return resource_;
    };

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
};

/// @brief DirectX 12のデバイス・コマンド・描画先・フレーム同期を管理する。
class DirectXCommon {
public:
    /// @brief フレーム送信の各区間の所要時間をミリ秒で記録する。
    struct FrameSubmitProfile {
        float closeMs = 0.0f;
        float executeMs = 0.0f;
        float presentMs = 0.0f;
        float fenceWaitMs = 0.0f;
        float fpsLimitMs = 0.0f;
        float resetMs = 0.0f;
        float totalMs = 0.0f;
    };

    static constexpr DXGI_FORMAT kBackBufferRenderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    static constexpr DXGI_FORMAT kSceneRenderTargetFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    static constexpr DXGI_FORMAT kNormalBufferFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    static constexpr DXGI_FORMAT kMaterialBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    static constexpr DXGI_FORMAT kAmbientOcclusionBufferFormat = DXGI_FORMAT_R8_UNORM;
    static constexpr DXGI_FORMAT kMotionVectorBufferFormat = DXGI_FORMAT_R16G16_FLOAT;

    /// @brief グラフィックスパイプラインを作るシェーダーの種類。
    enum ShaderType {
        Object,
        Particle,
        ModelParticle,
        ComputeParticle,
        PostEffect,
        Shadow,
        Trail,
        Skybox,
        Skinning,
        SkinningShadow,
    };

    /// @brief ポストエフェクト用パイプラインの種類。
    enum PostEffectType {
        Bloom_Extract,
        Bloom_Downsample,
        Bloom_BlurH,
        Bloom_BlurV,
        Gaussian_Filter,
        Bloom_Composite,
        ObjectPost_Composite,
        ObjectPost_OutlineAdd,
        ObjectPost_BloomAdd,
        Random,
        SSAO_Resolve,
        SSAO_Denoise,
        SSR_Resolve,
        SSR_Denoise,
        MotionVector_Resolve,
        Temporal_Resolve,
        Phosphor_Persistence,
        Crt_Present,
    };

    /// @brief 1パイプラインの状態・シェーダー・入力配置・ルートシグネチャをまとめる。
    struct PSO {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsDesc_{};
        D3D12_COMPUTE_PIPELINE_STATE_DESC computeDesc_{};
        Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsState_ = nullptr;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> computeState_ = nullptr;
        InputDesc inputDesc_;
        Root root_;
        // Shaderをコンパイルする
        Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob_;
        Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob_;
        Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob_;
        ShaderType shaderType_;
        std::wstring vsFilePath_;
        std::wstring psFilePath_;
        PostEffectType postEffectType_;
    };

    /// @brief 初期化
    void Initialize(WinApp* winApp, bool titleFirst = false);
    /// @brief パイプラインの作成を1ステップ進める。すべて完了したらtrueを返す。
    bool InitializePipelinesStep();

    /// @brief すべてのパイプラインの作成が完了したかを返す。
    bool ArePipelinesReady() const
    {
        return pipelinesReady_;
    }

    /// @brief 描画前処理
    void PreDraw();

    /// @brief 描画後処理
    void PostDraw();

    /// @brief フレームのコマンドを送信して表示し、GPU完了後に記録領域を再利用する。
    void CommandListExecuteAndReset();

    /// @brief 記録したGPUコマンドを送信し、完了フェンスを待ってから再利用する。
    /// @note 待機終了までCPUがGPU用資源を書き換えないために使う。
    void ExecuteCommandListAndWait();
    /// @brief 今のコマンドリストに積んだコマンドが GPU で終わるまで、資源を解放せずに持っておく。
    /// 持ち主がフレームの途中で消える資源（コピーなどを積んだ直後に破棄するもの）に使う。
    void ReleaseAfterExecution(Microsoft::WRL::ComPtr<ID3D12Resource> resource)
    {
        releaseAfterExecution_.push_back(std::move(resource));
    }
    /// @brief 固定FPSを初期状態へ戻す。
    void ResetFixFPS();

    /// @brief 60FPSの上限で待つかどうかを設定する。
    void SetFrameLimitEnabled(bool enabled)
    {
        framePacer_.SetEnabled(enabled);
    }

    /// @brief 60FPSの上限で待つかを返す。
    bool IsFrameLimitEnabled() const
    {
        return framePacer_.IsEnabled();
    }

    /// @brief フレーム待機の計測値を返す。
    const FramePacer::Stats& GetFramePacingStats() const
    {
        return framePacer_.GetStats();
    }

    /// @brief フレーム送信の計測値を返す。
    const FrameSubmitProfile& GetFrameSubmitProfile() const
    {
        return frameSubmitProfile_;
    }

    /// @brief D3D12のデバッグレイヤーが有効かを返す。
    bool IsD3D12DebugLayerEnabled() const
    {
        return d3d12DebugLayerEnabled_;
    }

    /// @brief GPUベースの検証が有効かを返す。
    bool IsGpuBasedValidationEnabled() const
    {
        return gpuBasedValidationEnabled_;
    }

    /// @brief シェーダーをコンパイルする。同じファイルとプロファイルの2回目以降は、メモリかディスクのキャッシュを返す。
    /// @return 失敗したときは空のComPtr。
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const std::wstring& filePath, const wchar_t* profile);

    /// @brief 所有する資源を解放する。
    void Release();

    /// @brief デバイスを返す。
    Microsoft::WRL::ComPtr<ID3D12Device>& GetDevice()
    {
        return device_;
    }

    /// @brief DXGIファクトリーを返す。
    Microsoft::WRL::ComPtr<IDXGIFactory7>& GetDxgiFactory()
    {
        return dxgiFactory_;
    }

    /// @brief ビューポート矩形を返す。
    D3D12_VIEWPORT GetViewportRect()
    {
        return viewportRect_;
    }

    /// @brief シザー矩形を返す。
    D3D12_RECT GetScissorRect()
    {
        return scissorRect_;
    };

    /// @brief スワップチェーンを返す。
    Microsoft::WRL::ComPtr<IDXGISwapChain4>& GetSwapChain()
    {
        return swapChain_;
    }

    /// @brief スワップチェーンの記述を返す。
    DXGI_SWAP_CHAIN_DESC1 GetSwapChainDesc()
    {
        return swapChainDesc_;
    }

    /// @brief RTVヒープを返す。
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetRtvHeap()
    {
        return rtvDescriptorHeap_;
    };

    /// @brief DSVヒープを返す。
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetDsvHeap()
    {
        return dsvDescriptorHeap_;
    };

    /// @brief フェンスを返す。
    Microsoft::WRL::ComPtr<ID3D12Fence> GetFence()
    {
        return fence_;
    }

    /// @brief コマンドキューを返す。
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetQueue()
    {
        return queue_;
    }

    /// @brief コマンドアロケータを返す。
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> GetAllocator()
    {
        return allocator_;
    }

    /// @brief コマンドキューの記述を返す。
    D3D12_COMMAND_QUEUE_DESC GetQueueDesc()
    {
        return queueDesc_;
    }

    /// @brief コマンドリストを返す。
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> GetList()
    {
        return list_;
    }

    /// @brief フェンス値を返す。
    uint64_t GetFenceValue()
    {
        return fenceValue_;
    }

    /// @brief DXCのユーティリティを返す。
    IDxcUtils* GetDxcUtils()
    {
        return dxcUtils_.Get();
    }

    /// @brief DXCのコンパイラを返す。
    IDxcCompiler3* GetDxcCompiler()
    {
        return dxcCompiler_.Get();
    }

    /// @brief シェーダーの #include を解決するハンドラーを返す。
    IDxcIncludeHandler* GetIncludeHandler()
    {
        return includeHandler_.Get();
    }

    /// @brief ブレンドモードに対応する、バックバッファ（SDR）向けのPSOを返す。
    PSO& GetPSOObject(BlendMode blendMode = kAdd)
    {
        switch (blendMode) {
        case kNone:
            return objectPSO_None;
            break;
        case kNormal:
            return objectPSO_Alpha;
            break;
        case kAdd:
            return objectPSO_Add;
            break;
        case kShadow:
            return shadowPSO;
            break;
        case kAdd_Bloom_Extract:
            return bloomPSO;
            break;

        case kAdd_Bloom_Downsample:
            return downsamplePSO;
            break;

        case kAdd_Bloom_BlurH:
            return blurHPSO;
            break;

        case kAdd_Bloom_BlurV:
            return blurVPSO;
            break;

        case kAdd_Bloom_Composite:
            return compositePSO;
            break;
        case kAdd_ObjectPost_Composite:
            return objectPostCompositePSO;
            break;
        case kAdd_ObjectPost_OutlineAdd:
            return objectPostOutlineAddPSO;
            break;
        case kAdd_ObjectPost_BloomAdd:
            return objectPostBloomAddPSO;
            break;
        case kRandom:
            return randomPSO;
            break;
        case kAdd_SSAO_Resolve:
            return ssaoResolvePSO;
            break;
        case kAdd_SSAO_Denoise:
            return ssaoDenoisePSO;
            break;
        case kAdd_SSR_Resolve:
            return ssrResolvePSO;
            break;
        case kAdd_SSR_Denoise:
            return ssrDenoisePSO;
            break;
        case kAdd_MotionVector_Resolve:
            return motionVectorResolvePSO;
            break;
        case kAdd_Temporal_Resolve:
            return temporalResolvePSO;
            break;
        case kAdd_Phosphor_Persistence:
            return phosphorPersistencePSO;
            break;
        case kAdd_Crt_Present:
            return crtPresentPSO;
            break;
        default:
            return objectPSO_None;
            break;
        }
    }

    /// @brief ブレンドモードに対応する、シーン（HDR）向けのPSOを返す。
    PSO& GetPSOObjectForScene(BlendMode blendMode = kAdd)
    {
        switch (blendMode) {
        case kNone:
            return objectPSO_None_HDR;
        case kNormal:
            return objectPSO_Alpha_HDR;
        case kAdd:
            return objectPSO_Add_HDR;
        case kShadow:
            return shadowPSO;
        case kAdd_Bloom_Extract:
            return bloomPSO_HDR;
        case kAdd_Bloom_Downsample:
            return downsamplePSO_HDR;
        case kAdd_Bloom_BlurH:
            return blurHPSO_HDR;
        case kAdd_Bloom_BlurV:
            return blurVPSO_HDR;
        case kAdd_ObjectPost_Composite:
            return objectPostCompositePSO_HDR;
        case kAdd_ObjectPost_OutlineAdd:
            return objectPostOutlineAddPSO_HDR;
        case kAdd_ObjectPost_BloomAdd:
            return objectPostBloomAddPSO_HDR;
        case kRandom:
            return randomPSO_HDR;
        case kAdd_SSAO_Resolve:
            return ssaoResolvePSO;
        case kAdd_SSAO_Denoise:
            return ssaoDenoisePSO;
        case kAdd_SSR_Resolve:
            return ssrResolvePSO_HDR;
        case kAdd_SSR_Denoise:
            return ssrDenoisePSO_HDR;
        case kAdd_MotionVector_Resolve:
            return motionVectorResolvePSO;
        case kAdd_Temporal_Resolve:
            return temporalResolvePSO;
        case kAdd_Phosphor_Persistence:
            return phosphorPersistencePSO;
        default:
            return objectPSO_None_HDR;
        }
    }

    /// @brief 深度を持たないHDRキャプチャにはDSVなしの互換PSOを再利用する。
    ID3D12PipelineState* GetObjectScenePipeline(BlendMode blendMode);

    /// @brief パーティクル用のPSOを返す。
    PSO& GetPSOParticle()
    {
        return psoParticle_;
    }

    /// @brief モデルパーティクル用のPSOを返す。
    PSO& GetPSOModelParticle()
    {
        return psoModelParticle_;
    }

    /// @brief モデルパーティクルのシーン（HDR）用PSOを返す。
    PSO& GetPSOModelParticleForScene()
    {
        return psoModelParticle_HDR;
    }

    /// @brief ガウシアンフィルター用のPSOを返す。
    const PSO& GetGaussianFilterPSO() const
    {
        return gaussianFilterPSO;
    }

    /// @brief GPUパーティクルを更新するPSOを返す。
    PSO& GetPSOComputeParticle()
    {
        return psoComputeParticle_;
    }

    /// @brief GPUパーティクルの詰め先を数えるPSOを返す。
    PSO& GetPSOScanParticle()
    {
        return psoScanParticle_;
    }

    /// @brief GPUパーティクルを詰め直すPSOを返す。
    PSO& GetPSOCompactParticle()
    {
        return psoCompactParticle_;
    }

    /// @brief GPUパーティクルを初期化するPSOを返す。
    PSO& GetPSOInitializeParticle()
    {
        return psoInitializeParticle_;
    }

    /// @brief GPUパーティクルを発生させるPSOを返す。
    PSO& GetPSOEmitParticle()
    {
        return psoEmitParticle_;
    }

    /// @brief GPUパーティクルをまとめて発生させるPSOを返す。
    PSO& GetPSOEmitBatchParticle()
    {
        return psoEmitBatchParticle_;
    }

    /// @brief 軌跡用のPSOを返す。
    PSO& GetPSOTrail()
    {
        return trailPSO;
    }

    /// @brief 軌跡のシーン（HDR）用PSOを返す。
    PSO& GetPSOTrailForScene()
    {
        return trailPSO_HDR;
    }

    /// @brief 深度バッファなしでシーン（HDR）へ軌跡を描くパイプラインを作る。成功したらtrueを返す。
    bool InitializeTrailSceneNoDepthPipeline();

    /// @brief InitializeTrailSceneNoDepthPipelineで作ったパイプラインを返す。
    ID3D12PipelineState* GetTrailSceneNoDepthPipeline() const
    {
        return trailSceneNoDepthPipeline_.Get();
    }

    /// @brief HUDの矩形用のPSOを返す。
    PSO& GetPSOHudRect()
    {
        return hudRectPSO;
    }

    /// @brief スカイボックス用のPSOを返す。
    PSO& GetPSOSkybox()
    {
        return skyboxPSO_HDR;
    }

    /// @brief スキニング用のPSOを返す。
    PSO& GetPSOSkinning()
    {
        return skinningPSO;
    }

    /// @brief スキニングのシーン（HDR）用PSOを返す。
    PSO& GetPSOSkinningForScene()
    {
        return skinningPSO_HDR;
    }

    /// @brief 両面を描くスキニング用のPSOを返す。
    PSO& GetPSOSkinningDoubleSided()
    {
        return skinningDoubleSidedPSO;
    }

    /// @brief 両面を描くスキニングのシーン（HDR）用PSOを返す。
    PSO& GetPSOSkinningDoubleSidedForScene()
    {
        return skinningDoubleSidedPSO_HDR;
    }

    /// @brief スキニングの影用のPSOを返す。
    PSO& GetPSOSkinningShadow()
    {
        return skinningShadowPSO;
    }

    /// @brief ヒープのindex番目のCPUハンドルを返す。
    static D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorCPUHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap,
                                                              uint32_t descriptorSize, uint32_t index);
    /// @brief ヒープのindex番目のGPUハンドルを返す。
    static D3D12_GPU_DESCRIPTOR_HANDLE GetDescriptorGPUHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap,
                                                              uint32_t descriptorSize, uint32_t index);

    /// @brief デスクリプタヒープの生成
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors,
                                                                      bool shaderVisible);

    /// @brief RTVの記述を返す。
    D3D12_RENDER_TARGET_VIEW_DESC GetRtvDesc()
    {
        return rtvDesc_;
    }

    /// @brief テクスチャリソースを生成する。
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format,
                                                                 D3D12_RESOURCE_FLAGS flags, const D3D12_CLEAR_VALUE* clearValue,
                                                                 D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_RENDER_TARGET);

    /// @brief 描画先を1枚と深度に設定する。
    void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);
    /// @brief 描画先をカラー・法線の2枚と深度に設定する。
    void SetRenderTargets(D3D12_CPU_DESCRIPTOR_HANDLE colorRtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE normalRtvHandle,
                          D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);
    /// @brief 描画先をカラー・法線・マテリアルの3枚と深度に設定する。
    void SetRenderTargets(D3D12_CPU_DESCRIPTOR_HANDLE colorRtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE normalRtvHandle,
                          D3D12_CPU_DESCRIPTOR_HANDLE materialRtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);
    /// @brief 深度なしで描画先を1枚設定する。
    void SetRenderTargetNoDepth(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle);

    // シーンカラーだけでなく、すべてのMRTとラスタ範囲を記録する。
    struct RenderTargetState {
        std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 3> rtvs{};
        UINT count = 1;
        D3D12_CPU_DESCRIPTOR_HANDLE dsv{};
        bool hasDepth = false;
        D3D12_VIEWPORT viewport{};
        D3D12_RECT scissor{};
    };

    RenderTargetState GetRenderTargetState() const
    {
        return {currentRtvHandles_, currentRtvCount_, currentDsvHandle_, currentHasDsv_, viewportRect_, scissorRect_};
    }

    /// @brief GetRenderTargetStateで取った描画先の状態に戻す。
    void RestoreRenderTargetState(const RenderTargetState& state);

    /// @brief 現在のRTVハンドルを返す。
    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTVHandle() const
    {
        return currentRtvHandle_;
    }

    /// @brief 現在のDSVハンドルを返す。
    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentDSVHandle() const
    {
        return currentDsvHandle_;
    }

    /// @brief 現在DSVを設定しているかを返す。
    bool HasCurrentDSV() const
    {
        return currentHasDsv_;
    }

    /// @brief 描画先を黒で消去する。
    void ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle);
    /// @brief 描画先を指定した色で消去する。
    void ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, const float clearColor[4]);

    /// @brief 深度バッファを消去する。
    void ClearDepthBuffer();

    /// @brief バックバッファを描画先にする。
    void SetBackBuffer();
    /// @brief バックバッファと同じ形式・深度を使う中間の提示先を描画先にする。
    void SetPresentTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv);

    /// @brief ビューポートを設定する。
    void SetViewport(uint32_t width, uint32_t height);

    /// @brief DSVヒープから次のハンドルを1つ確保して返す。
    D3D12_CPU_DESCRIPTOR_HANDLE GetNewDsvHandle();

    /// @brief バッファリソースを生成する。
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);
    /// @brief UAVバッファリソースを生成する。
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateUAVBufferResource(size_t sizeInBytes,
                                                                   D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

private:
    /// @brief デバイスの初期化
    void InitializeDevice();

    /// @brief コマンドの初期化
    void InitializeCommand();

    /// @brief スワップチェーンの生成
    void CreateSwapChain();

    /// @brief 深度バッファの生成
    void CreateDepthBuffer();

    /// @brief 各種デスクリプタヒープの生成
    void CreateDescriptorHeap();

    /// @brief レンダーターゲットビューの初期化
    void CreateSwapChainRtv();

    /// @brief 深度ステンシルビューの初期化
    void InitializeDepthStencilView();

    /// @brief フェンスの初期化
    void InitializeFence();

    /// @brief ビューポート矩形の初期化
    void InitializeViewport();

    /// @brief シザリング矩形の初期化
    void InitializeScissorRect();

    /// @brief DXCコンパイラの生成
    void CreateDXCCompiler();

    /// @brief ImGuiの初期化
    /// @note 互換用の空の窓口。実際の初期化はGame::InitializeImGuiが担当する。
    void InitializeImGui();

    /// @brief 固定FPSを初期化する。
    void InitializeFixFPS();
    /// @brief 固定FPSを更新する。
    void UpdateFixFPS();

    /// @brief PSOの種類とブレンドモードから、グラフィックスパイプラインを作る。
    void CreateShaderCommon(PSO& pso, BlendMode blendMode = kAdd, bool doubleSided = false,
                            DXGI_FORMAT renderTargetFormat = kBackBufferRenderTargetFormat);
    /// @brief コンピュートパイプラインを作る。
    void CreateComputeShaderCommon(PSO& pso, const std::wstring& shaderPath);
    /// @brief 残っているパイプラインをすべて作る。
    void CreateShader();
    /// @brief パイプラインを順番に作成するコルーチンを作る。
    InitializationTask BuildPipelines();
    InitializationTask pipelineTask_;
    bool pipelinesReady_ = false;

    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_ = nullptr;
    Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Device> device_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator_ = nullptr;
    D3D12_COMMAND_QUEUE_DESC queueDesc_{};
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list_ = nullptr;

    WinApp* winApp_ = nullptr;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_ = nullptr;
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc_{};
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_ = nullptr;
    FrameSubmitProfile frameSubmitProfile_{};
    bool d3d12DebugLayerEnabled_ = false;
    bool gpuBasedValidationEnabled_ = false;
    // InitializeFence で作り、Release で閉じる。
    HANDLE fenceEvent_ = nullptr;
    uint64_t fenceValue_ = 0;
    // ReleaseAfterExecution で預かった資源。コマンドリストの実行が終わったら解放する。
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> releaseAfterExecution_;

    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;

    // DescriptorSizeを取得しておく
    uint32_t descriptorSizeRTV;
    uint32_t descriptorSizeDSV;

    Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources_[2] = {nullptr};
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc_{};
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[2];

    // ビューポート・シザリング矩形
    //---------------------------------
    D3D12_VIEWPORT viewportRect_{};
    D3D12_RECT scissorRect_{};

    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
    std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<IDxcBlob>> shaderCache_;

    // FPS固定変数
    //---------------------------------

    FramePacer framePacer_;

    PSO objectPSO_None;
    PSO objectPSO_Alpha;
    PSO objectPSO_Add;
    PSO objectPSO_None_HDR;
    PSO objectPSO_Alpha_HDR;
    PSO objectPSO_Add_HDR;
    PSO psoParticle_;
    PSO psoModelParticle_;
    PSO psoModelParticle_HDR;
    PSO psoComputeParticle_;
    PSO psoScanParticle_;
    PSO psoCompactParticle_;
    PSO psoInitializeParticle_;
    PSO psoEmitParticle_;
    PSO psoEmitBatchParticle_;
    PSO bloomPSO;
    PSO downsamplePSO;
    PSO blurHPSO;
    PSO blurVPSO;
    PSO bloomPSO_HDR;
    PSO downsamplePSO_HDR;
    PSO blurHPSO_HDR;
    PSO blurVPSO_HDR;
    PSO gaussianFilterPSO;
    PSO compositePSO;
    PSO objectPostCompositePSO;
    PSO objectPostOutlineAddPSO;
    PSO objectPostBloomAddPSO;
    PSO randomPSO;
    PSO ssaoResolvePSO;
    PSO ssaoDenoisePSO;
    PSO ssrResolvePSO;
    PSO ssrDenoisePSO;
    PSO motionVectorResolvePSO;
    PSO temporalResolvePSO;
    PSO phosphorPersistencePSO;
    PSO crtPresentPSO;
    PSO objectPostCompositePSO_HDR;
    PSO objectPostOutlineAddPSO_HDR;
    PSO objectPostBloomAddPSO_HDR;
    PSO randomPSO_HDR;
    PSO ssrResolvePSO_HDR;
    PSO ssrDenoisePSO_HDR;
    PSO shadowPSO;
    PSO trailPSO;
    PSO trailPSO_HDR;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> trailSceneNoDepthPipeline_;
    PSO hudRectPSO;
    std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 3> noDepthObjectPipelines_;
    PSO skyboxPSO;
    PSO skyboxPSO_HDR;
    PSO skinningPSO;
    PSO skinningDoubleSidedPSO;
    PSO skinningPSO_HDR;
    PSO skinningDoubleSidedPSO_HDR;
    PSO skinningShadowPSO;
    ShaderType shaderType_;

    uint32_t dsvHeapIndex_ = 0;
    const uint32_t kMaxDsvCount = 10;

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_;
    D3D12_CPU_DESCRIPTOR_HANDLE currentRtvHandle_{};
    D3D12_CPU_DESCRIPTOR_HANDLE currentDsvHandle_{};
    bool currentHasDsv_ = false;
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 3> currentRtvHandles_{};
    UINT currentRtvCount_ = 1;
};

} // namespace myEngine
