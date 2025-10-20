#include "View.h"

void View::CreateDSV(Texture texture, const uint32_t kClientWidth, const uint32_t kClientHeight, SwapChain swapChain, Descriptor descriptor, Microsoft::WRL::ComPtr<ID3D12Device>& device)
{

	// DepthStencilTextureをウィンドウのサイズで作成
	depthStencilResource = texture.CreateDepthStencilResouce(device, kClientWidth, kClientHeight);

	// DSVの設定
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // Format。基本的にはResourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2dTexture
	// DSVHeapの先頭にDSVをつくる
	device->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc, descriptor.GetDsvHeap()->GetCPUDescriptorHandleForHeapStart());


}

void View::CreateSRV(SwapChain swapChain, Descriptor descriptor, Microsoft::WRL::ComPtr<ID3D12Device>& device)
{

	// SwapChainからResourceを引っ張ってくる
	HRESULT hr = swapChain.GetList()->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	// 上手く取得できなければ起動できない
	assert(SUCCEEDED(hr));
	hr = swapChain.GetList()->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr));
	
	// RTVの設定
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 
	// ディスクリプタの先頭を取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = descriptor.GetRtvHeap()->GetCPUDescriptorHandleForHeapStart();
	//RTVを2つつくるのでディスクリプタを2つ用意
	// まず1つ目をつくる。1つ目は最初の所に作る。作る場所を指定してあげる必要がある
	rtvHandles[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResources[0].Get(), &rtvDesc, rtvHandles[0]);
	// 2つ目のディスクリプタハンドルを得る
	rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	// 2つ目を作る
	device->CreateRenderTargetView(swapChainResources[1].Get(), &rtvDesc, rtvHandles[1]);

	rtvHandles[0] = rtvStartHandle;
	rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

}
