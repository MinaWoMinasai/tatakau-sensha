#pragma once
#include <wrl.h>
#include <dxgi1_6.h>
#include <cassert>
#include <cstdint>
#include <d3d12.h>

class SwapChain
{

public:

	void Initialize(const int32_t& kClientWidth, const int32_t& kClientHeight);

	void Create(const Microsoft::WRL::ComPtr<IDXGIFactory7>& dxgiFactory, const Microsoft::WRL::ComPtr<ID3D12CommandQueue>& commandQueue, HWND& hwnd);

	Microsoft::WRL::ComPtr<IDXGISwapChain4> GetList() { return list; }
	DXGI_SWAP_CHAIN_DESC1 GetDesc() { return desc; }

private:

	// スワップチェーンを生成する
	Microsoft::WRL::ComPtr<IDXGISwapChain4> list = nullptr;
	DXGI_SWAP_CHAIN_DESC1 desc{};

};

