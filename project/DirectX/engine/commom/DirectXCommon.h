#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include "WinApp.h"
#include <dxcapi.h>

class DirectXCommon
{
public:

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(WinApp* winApp);
	
    //Microsoft::WRL::ComPtr<ID3D12Device>& GetDevice() { return device_; }
	//Microsoft::WRL::ComPtr<IDXGIFactory7>& GetDxgiFactory() { return dxgiFactory_; }
	
	D3D12_VIEWPORT GetViewportRect() { return viewportRect_; }
	D3D12_RECT GetSissorRect() { return scissorRect_; };

	//Microsoft::WRL::ComPtr<IDXGISwapChain4>& GetSwapChain() { return swapChain_; }
	//DXGI_SWAP_CHAIN_DESC1 GetSwapChainDesc() { return swapChainDesc_; }

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetRtvHeap() { return rtvDescriptorHeap_; };
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetSrvHeap() { return srvDescriptorHeap_; };
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetDsvHeap() { return dsvDescriptorHeap_; };

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
	
};

