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
#include "Root.h"
#include "InputDesc.h"
#include "Calculation.h"
#include "Struct.h"
#include "Texture.h"
#include "FramePacer.h"

namespace cg2 {

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

    enum ShaderType {
        Object,
        Particle,
        ModelParticle,
        ComputeParticle,
        PostEffect,
        Shadow,
        Trail,
        Skybox,
        Ocean,
        Skinning,
        SkinningShadow,
    };

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
        IDxcBlob* vertexShaderBlob_ = nullptr;
        IDxcBlob* pixelShaderBlob_ = nullptr;
        IDxcBlob* computeShaderBlob_ = nullptr;
        ShaderType shaderType_;
        std::wstring vsFilePath_;
        std::wstring psFilePath_;
        PostEffectType postEffectType_;
    };

    /// @brief 初期化
    void Initialize(WinApp* winApp);

    /// @brief 描画前処理
    void PreDraw();

    /// @brief 描画後処理
    void PostDraw();

    /// @brief
    /// @brief フレームのコマンドを送信して表示し、GPU完了後に記録領域を再利用する。
    void CommandListExecuteAndReset();

    /// @brief 記録したGPUコマンドを送信し、完了フェンスを待ってから再利用する。
    /// @note 待機終了までCPUがGPU用資源を書き換えないために使う。
    void ExecuteCommandListAndWait();
    /// @brief 固定FPSを初期状態へ戻す。
    void ResetFixFPS();
    /// @brief フレーム上限有効を設定する。
    void SetFrameLimitEnabled(bool enabled)
    {
        framePacer_.SetEnabled(enabled);
    }
    /// @brief フレーム上限有効であるか判定する。
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
    /// @brief D3D12開発表示Layer有効であるか判定する。
    bool IsD3D12DebugLayerEnabled() const
    {
        return d3d12DebugLayerEnabled_;
    }
    /// @brief GPUBased検証有効であるか判定する。
    bool IsGpuBasedValidationEnabled() const
    {
        return gpuBasedValidationEnabled_;
    }

    /// @brief シェーダーをコンパイル
    IDxcBlob* CompileShader(const std::wstring& filePath, const wchar_t* profile);

    /// @brief 所有する資源を解放する。
    void Release();

    /// @brief Deviceを返す。
    Microsoft::WRL::ComPtr<ID3D12Device>& GetDevice()
    {
        return device_;
    }
    /// @brief Dxgiファクトリーを返す。
    Microsoft::WRL::ComPtr<IDXGIFactory7>& GetDxgiFactory()
    {
        return dxgiFactory_;
    }

    /// @brief ビューポート矩形を返す。
    D3D12_VIEWPORT GetViewportRect()
    {
        return viewportRect_;
    }
    /// @brief 描画を制限する矩形を返す。
    D3D12_RECT GetSissorRect()
    {
        return scissorRect_;
    };

    /// @brief 交換Chainを返す。
    Microsoft::WRL::ComPtr<IDXGISwapChain4>& GetSwapChain()
    {
        return swapChain_;
    }
    /// @brief 交換Chain記述を返す。
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
    /// @brief フェンスイベントを返す。
    HANDLE GetFenceEvent()
    {
        return fenceEvent_;
    }
    /// @brief 予約を返す。
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetQueue()
    {
        return queue_;
    }
    /// @brief Allocatorを返す。
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> GetAllocator()
    {
        return allocator_;
    }
    /// @brief 予約記述を返す。
    D3D12_COMMAND_QUEUE_DESC GetQueueDesc()
    {
        return queueDesc_;
    }
    /// @brief 一覧を返す。
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> GetList()
    {
        return list_;
    }
    /// @brief フェンス値を返す。
    uint64_t GetFenceValue()
    {
        return fenceValue_;
    }
    /// @brief DxcUtilsを返す。
    IDxcUtils* GetDxcUtils()
    {
        return dxcUtils_;
    }
    /// @brief DxcCompilerを返す。
    IDxcCompiler3* GetDxcCompiler()
    {
        return dxcCompiler_;
    }
    /// @brief include依存Handlerを返す。
    IDxcIncludeHandler* GetIncludeHandler()
    {
        return includeHandler_;
    }

    /// @brief PSOオブジェクトを返す。
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
            return conpositePSO;
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
        default:
            return objectPSO_None;
            break;
        }
    }

    /// @brief PSOオブジェクト用のシーンを返す。
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
        default:
            return objectPSO_None_HDR;
        }
    }
    /// @brief PSO海面用のシーンを返す。
    PSO& GetPSOOceanForScene()
    {
        return oceanPSO_HDR;
    }

    /// @brief PSO粒子を返す。
    PSO& GetPSOParticle()
    {
        return psoParticle_;
    }
    /// @brief PSOモデル粒子を返す。
    PSO& GetPSOModelParticle()
    {
        return psoModelParticle_;
    }
    /// @brief PSOモデル粒子用のシーンを返す。
    PSO& GetPSOModelParticleForScene()
    {
        return psoModelParticle_HDR;
    }
    /// @brief ガウシアンFilterPSOを返す。
    const PSO& GetGaussianFilterPSO() const
    {
        return gaussianFilterPSO;
    }
    /// @brief PSO計算粒子を返す。
    PSO& GetPSOComputeParticle()
    {
        return psoComputeParticle_;
    }
    /// @brief PSO初期化粒子を返す。
    PSO& GetPSOInitializeParticle()
    {
        return psoInitializeParticle_;
    }
    /// @brief PSOEmit粒子を返す。
    PSO& GetPSOEmitParticle()
    {
        return psoEmitParticle_;
    }
    /// @brief PSOEmit一括処理粒子を返す。
    PSO& GetPSOEmitBatchParticle()
    {
        return psoEmitBatchParticle_;
    }
    /// @brief PSO軌跡を返す。
    PSO& GetPSOTrail()
    {
        return trailPSO;
    }
    /// @brief PSO軌跡用のシーンを返す。
    PSO& GetPSOTrailForScene()
    {
        return trailPSO_HDR;
    }
    /// @brief PSOHUD矩形を返す。
    bool InitializeTrailSceneNoDepthPipeline();
    ID3D12PipelineState* GetTrailSceneNoDepthPipeline() const { return trailSceneNoDepthPipeline_.Get(); }
    PSO& GetPSOHudRect()
    {
        return hudRectPSO;
    }
    /// @brief PSO背景を返す。
    PSO& GetPSOSkybox()
    {
        return skyboxPSO_HDR;
    }
    /// @brief PSOスキニングを返す。
    PSO& GetPSOSkinning()
    {
        return skinningPSO;
    }
    /// @brief PSOスキニング用のシーンを返す。
    PSO& GetPSOSkinningForScene()
    {
        return skinningPSO_HDR;
    }
    /// @brief PSOスキニング両面面を返す。
    PSO& GetPSOSkinningDoubleSided()
    {
        return skinningDoubleSidedPSO;
    }
    /// @brief PSOスキニング両面面用のシーンを返す。
    PSO& GetPSOSkinningDoubleSidedForScene()
    {
        return skinningDoubleSidedPSO_HDR;
    }
    /// @brief PSOスキニング影を返す。
    PSO& GetPSOSkinningShadow()
    {
        return skinningShadowPSO;
    }

    /// @brief ディスクリプターCPUハンドルを返す。
    static D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorCPUHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap,
                                                              uint32_t descriptorSize, uint32_t index);
    /// @brief ディスクリプターGPUハンドルを返す。
    static D3D12_GPU_DESCRIPTOR_HANDLE GetDescriptorGPUHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap,
                                                              uint32_t descriptorSize, uint32_t index);

    /// @brief デスクリプタヒープの生成
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors,
                                                                      bool shaderVisible);

    /// @brief RTV記述を返す。
    D3D12_RENDER_TARGET_VIEW_DESC GetRtvDesc()
    {
        return rtvDesc_;
    }
    /// @brief テクスチャリソースを生成する。
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format,
                                                                 D3D12_RESOURCE_FLAGS flags, const D3D12_CLEAR_VALUE* clearValue,
                                                                 D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_RENDER_TARGET);

    /// @brief 描画対象を設定する。
    void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);
    /// @brief 描画対象を設定する。
    void SetRenderTargets(D3D12_CPU_DESCRIPTOR_HANDLE colorRtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE normalRtvHandle,
                          D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);
    /// @brief 描画対象を設定する。
    void SetRenderTargets(D3D12_CPU_DESCRIPTOR_HANDLE colorRtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE normalRtvHandle,
                          D3D12_CPU_DESCRIPTOR_HANDLE materialRtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);
    /// @brief 描画対象No深度を設定する。
    void SetRenderTargetNoDepth(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle);
    /// @brief 現在RTVハンドルを返す。
    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTVHandle() const
    {
        return currentRtvHandle_;
    }
    /// @brief 現在DSVハンドルを返す。
    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentDSVHandle() const
    {
        return currentDsvHandle_;
    }
    /// @brief 現在DSVが存在するか判定する。
    bool HasCurrentDSV() const
    {
        return currentHasDsv_;
    }

    /// @brief 描画対象を消去する。
    void ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle);
    /// @brief 描画対象を消去する。
    void ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, const float clearColor[4]);

    /// @brief 深度バッファを消去する。
    void ClearDepthBuffer();

    /// @brief 背面バッファを設定する。
    void SetBackBuffer();

    /// @brief ビューポートを設定する。
    void SetViewport(uint32_t width, uint32_t height);

    /// @brief NewDSVハンドルを返す。
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
    void InititalizeCommand();

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
    void InitializeSissorRect();

    /// @brief DXCコンパイラの生成
    void CreateDXCCompiler();

    /// @brief ImGuiの初期化
    /// @note 互換用の空の窓口。実際の初期化はGame::InitializeImGuiが担当する。
    void InitializeImGui();

    /// @brief 固定FPSを初期化する。
    void InitializeFixFPS();
    /// @brief 固定FPSを更新する。
    void UpdateFixFPS();

    /// @brief シェーダー共通基盤を生成する。
    void CreateShaderCommon(PSO& pso, BlendMode blendMode = kAdd, bool doubleSided = false,
                            DXGI_FORMAT renderTargetFormat = kBackBufferRenderTargetFormat);
    /// @brief 計算シェーダー共通基盤を生成する。
    void CreateComputeShaderCommon(PSO& pso, const std::wstring& shaderPath);
    /// @brief シェーダーを生成する。
    void CreateShader();
    /// @brief Graphicsを生成する。
    void CreateGraphics();

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
    HANDLE fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
    uint64_t fenceValue_ = 0;

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

    IDxcUtils* dxcUtils_ = nullptr;
    IDxcCompiler3* dxcCompiler_ = nullptr;
    IDxcIncludeHandler* includeHandler_ = nullptr;
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
    PSO oceanPSO_HDR;
    PSO psoParticle_;
    PSO psoModelParticle_;
    PSO psoModelParticle_HDR;
    PSO psoComputeParticle_;
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
    PSO conpositePSO;
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
};

} // namespace cg2
