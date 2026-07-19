#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include "WinApp.h"
#include <dxcapi.h>
#include <dxgidebug.h>
#include <chrono>
#include <thread>
#include "Root.h"
#include "InputDesc.h"
#include "Calculation.h"
#include "Struct.h"
#include "Texture.h"

struct D3DResouceLeakCheaker {
	~D3DResouceLeakCheaker()
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

class ResourceObject {
public:
	ResourceObject(Microsoft::WRL::ComPtr<ID3D12Resource>& resource)
		:resource_(resource)
	{
	}
	~ResourceObject() {
		if (resource_) {
			resource_->Release();
		}
	}
	Microsoft::WRL::ComPtr<ID3D12Resource> Get() { return resource_; };

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
};

class DirectXCommon
{
public:
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

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(WinApp* winApp);
	
	/// <summary>
	/// 描画前処理
	/// </summary>
	void PreDraw();

	/// <summary>
	/// 描画後処理
	/// </summary>
	void PostDraw();

	/// <summary>
	/// 
	/// </summary>
	void CommandListExecuteAndReset();

	void ExecuteCommandListAndWait();
	void ResetFixFPS();
	const FrameSubmitProfile& GetFrameSubmitProfile() const { return frameSubmitProfile_; }
	bool IsGpuBasedValidationEnabled() const { return gpuBasedValidationEnabled_; }

	/// <summary>
	/// シェーダーをコンパイル
	/// </summary>
	/// <param name="filePath"></param>
	/// <param name="profile"></param>
	/// <param name="dxcUtils"></param>
	/// <param name="dxcCompiler"></param>
	/// <param name="includeHandler"></param>
	/// <returns></returns>
	IDxcBlob* CompileShader(const std::wstring& filePath, const wchar_t* profile);

	void Release();

    Microsoft::WRL::ComPtr<ID3D12Device>& GetDevice() { return device_; }
	Microsoft::WRL::ComPtr<IDXGIFactory7>& GetDxgiFactory() { return dxgiFactory_; }
	
	D3D12_VIEWPORT GetViewportRect() { return viewportRect_; }
	D3D12_RECT GetSissorRect() { return scissorRect_; };

	Microsoft::WRL::ComPtr<IDXGISwapChain4>& GetSwapChain() { return swapChain_; }
	DXGI_SWAP_CHAIN_DESC1 GetSwapChainDesc() { return swapChainDesc_; }

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetRtvHeap() { return rtvDescriptorHeap_; };
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetDsvHeap() { return dsvDescriptorHeap_; };

	Microsoft::WRL::ComPtr<ID3D12Fence> GetFence() { return fence_; }
	HANDLE GetFenceEvent() { return fenceEvent_; }
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetQueue() { return queue_; }
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> GetAllocator() { return allocator_; }
	D3D12_COMMAND_QUEUE_DESC GetQueueDesc() { return queueDesc_; }
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> GetList() { return list_; }
	uint64_t GetFenceValue() { return fenceValue_; }
	IDxcUtils* GetDxcUtils() { return dxcUtils_; }
	IDxcCompiler3* GetDxcCompiler() { return dxcCompiler_; }
	IDxcIncludeHandler* GetIncludeHandler() { return includeHandler_; }

	PSO& GetPSOObject(BlendMode blendMode = kAdd) {
		switch (blendMode)
		{
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

	PSO& GetPSOObjectForScene(BlendMode blendMode = kAdd) {
		switch (blendMode)
		{
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

	PSO& GetPSOParticle() { return psoParticle_; }
	PSO& GetPSOModelParticle() { return psoModelParticle_; }
	PSO& GetPSOModelParticleForScene() { return psoModelParticle_HDR; }
	const PSO& GetGaussianFilterPSO() const { return gaussianFilterPSO; }
	PSO& GetPSOComputeParticle() { return psoComputeParticle_; }
	PSO& GetPSOInitializeParticle() { return psoInitializeParticle_; }
	PSO& GetPSOEmitParticle() { return psoEmitParticle_; }
	PSO& GetPSOEmitBatchParticle() { return psoEmitBatchParticle_; }
	PSO& GetPSOTrail() { return trailPSO; }
	PSO& GetPSOTrailForScene() { return trailPSO_HDR; }
	PSO& GetPSOHudRect() { return hudRectPSO; }
	PSO& GetPSOSkybox() { return skyboxPSO_HDR; }
	PSO& GetPSOSkinning() { return skinningPSO; }
	PSO& GetPSOSkinningForScene() { return skinningPSO_HDR; }
	PSO& GetPSOSkinningDoubleSided() { return skinningDoubleSidedPSO; }
	PSO& GetPSOSkinningDoubleSidedForScene() { return skinningDoubleSidedPSO_HDR; }
	PSO& GetPSOSkinningShadow() { return skinningShadowPSO; }

	static D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorCPUHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);
	static D3D12_GPU_DESCRIPTOR_HANDLE GetDescriptorGPUHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);

	/// <summary>
	/// デスクリプタヒープの生成
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	D3D12_RENDER_TARGET_VIEW_DESC GetRtvDesc() { return rtvDesc_; }
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(
		uint32_t width,
		uint32_t height,
		DXGI_FORMAT format,
		D3D12_RESOURCE_FLAGS flags,
		const D3D12_CLEAR_VALUE* clearValue
	);

	void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);
	void SetRenderTargets(
		D3D12_CPU_DESCRIPTOR_HANDLE colorRtvHandle,
		D3D12_CPU_DESCRIPTOR_HANDLE normalRtvHandle,
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);
	void SetRenderTargets(
		D3D12_CPU_DESCRIPTOR_HANDLE colorRtvHandle,
		D3D12_CPU_DESCRIPTOR_HANDLE normalRtvHandle,
		D3D12_CPU_DESCRIPTOR_HANDLE materialRtvHandle,
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);
	void SetRenderTargetNoDepth(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle);
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTVHandle() const { return currentRtvHandle_; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentDSVHandle() const { return currentDsvHandle_; }
	bool HasCurrentDSV() const { return currentHasDsv_; }

	void ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle);
	void ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, const float clearColor[4]);

	void ClearDepthBuffer();

	void SetBackBuffer();

	void SetViewport(uint32_t width, uint32_t height);

	D3D12_CPU_DESCRIPTOR_HANDLE GetNewDsvHandle();

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateUAVBufferResource(size_t sizeInBytes, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

private:

	/// <summary>
	/// デバイスの初期化
	/// </summary>
	void InitializeDevice();

	/// <summary>
	/// コマンドの初期化
	/// </summary>
	void InititalizeCommand();

	/// <summary>
	/// スワップチェーンの生成
	/// </summary>
	void CreateSwapChain();

	/// <summary>
	/// 深度バッファの生成
	/// </summary>
	void CreateDepthBuffer();

	/// <summary>
	/// 各種デスクリプタヒープの生成
	/// </summary>
	void CreateDescriptorHeap();

	/// <summary>
	/// レンダーターゲットビューの初期化
	/// </summary>
	void CreateSwapChainRtv();

	/// <summary>
	/// 深度ステンシルビューの初期化
	/// </summary>
	void InitializeDepthStencilView();

	/// <summary>
	/// フェンスの初期化
	/// </summary>
	void InitializeFence();

	/// <summary>
	/// ビューポート矩形の初期化
	/// </summary>
	void InitializeViewport();

	/// <summary>
	/// シザリング矩形の初期化
	/// </summary>
	void InitializeSissorRect();

	/// <summary>
	/// DXCコンパイラの生成
	/// </summary>
	void CreateDXCCompiler();

	/// <summary>
	/// ImGuiの初期化
	/// </summary>
	void InitializeImGui();

	void InitializeFixFPS();
	void UpdateFixFPS();

	void CreateShaderCommon(
		PSO& pso,
		BlendMode blendMode = kAdd,
		bool doubleSided = false,
		DXGI_FORMAT renderTargetFormat = kBackBufferRenderTargetFormat);
	void CreateComputeShaderCommon(PSO& pso, const std::wstring& shaderPath);
	void CreateShader();
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
	bool gpuBasedValidationEnabled_ = false;
	HANDLE fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
	uint64_t fenceValue_ = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;

	// DescriptorSizeを取得しておく
	uint32_t descriptorSizeRTV;
	uint32_t descriptorSizeDSV;

	Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources_[2] = { nullptr };
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc_{};
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[2];

	// ビューポート・シザリング矩形
	//---------------------------------
	D3D12_VIEWPORT viewportRect_{};
	D3D12_RECT scissorRect_{};

	IDxcUtils* dxcUtils_ = nullptr;
	IDxcCompiler3* dxcCompiler_ = nullptr;
	IDxcIncludeHandler* includeHandler_ = nullptr;
	
	// FPS固定変数
	//---------------------------------
	
	// 記録時間
	std::chrono::steady_clock::time_point reference_;
	
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

