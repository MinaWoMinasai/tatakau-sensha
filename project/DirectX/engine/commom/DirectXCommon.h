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
#include "State.h"
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

	enum ShaderType {
		Object,
		Particle,
	};

	struct PSO {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsDesc_{};
		Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsState_ = nullptr;
		State state_;
		InputDesc inputDesc_;
		Root root_;
		// Shaderをコンパイルする
		IDxcBlob* vertexShaderBlob_ = nullptr;
		IDxcBlob* pixelShaderBlob_ = nullptr;
		ShaderType shaderType_;
		std::wstring vsFilePath_;
		std::wstring psFilePath_;
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
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetSrvHeap() { return srvDescriptorHeap_; };
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetDsvHeap() { return dsvDescriptorHeap_; };

	//Microsoft::WRL::ComPtr<ID3D12Resource> GetDepthStencilResource() { return depthStencilResource; };
	//std::span<const Microsoft::WRL::ComPtr<ID3D12Resource>> GetSwapChainResource() const { return swapChainResources; }
	//std::span<const D3D12_CPU_DESCRIPTOR_HANDLE> GetRtvHandles() const { return rtvHandles; }
	//D3D12_DEPTH_STENCIL_VIEW_DESC GetDsvDesc() { return dsvDesc; }
	//D3D12_RENDER_TARGET_VIEW_DESC GetRtvDesc() { return rtvDesc; }
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

	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);

	PSO& GetPSOObject() { return psoObject_; }
	PSO& GetPSOParticle() { return psoParticle_; }

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
	void CreateRenderTargetView();

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

	/// <summary>
	/// デスクリプタヒープの生成
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	static D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorCPUHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);
	static D3D12_GPU_DESCRIPTOR_HANDLE GetDescriptorGPUHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);

	void InitializeFixFPS();
	void UpdateFixFPS();

	void CreateShaderCommon(PSO& pso);
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
	HANDLE fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
	uint64_t fenceValue_ = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;

	// DescriptorSizeを取得しておく
	uint32_t descriptorSizeSRV;
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

	PSO psoObject_;
	PSO psoParticle_;
	ShaderType shaderType_;

};

