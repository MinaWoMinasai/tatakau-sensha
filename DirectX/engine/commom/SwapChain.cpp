#include "SwapChain.h"

void SwapChain::Initialize(const int32_t& kClientWidth, const int32_t& kClientHeight)
{

	desc.Width = kClientWidth; // 画面の幅。ウィンドウのクライアント領域を同じものにしておく
	desc.Height = kClientHeight; // 画面の高さ。ウィンドウのクライアント領域を同じものにしておく
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式
	desc.SampleDesc.Count = 1; // マルチサンプルしない
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとして利用する
	desc.BufferCount = 2; // ダブルバッファ
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // モニタをうつしたら、中身を破棄
	
}

void SwapChain::Create(const Microsoft::WRL::ComPtr<IDXGIFactory7>& dxgiFactory, const Microsoft::WRL::ComPtr<ID3D12CommandQueue>& commandQueue, HWND& hwnd)
{
	// コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	HRESULT hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &desc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(list.GetAddressOf()));
	assert(SUCCEEDED(hr));
}
