#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include "WinApp.h"

class DirectXCommon
{
public:

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(std::unique_ptr<WinApp> winApp);

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


private:
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_ = nullptr;
	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Device> device_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator_ = nullptr;
	D3D12_COMMAND_QUEUE_DESC queueDesc_{};
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list_ = nullptr;

	WinApp *winApp_ = nullptr;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
};

