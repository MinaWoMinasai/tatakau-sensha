#pragma once
#include <wrl.h>
#include <d3d12.h>
#include "Texture.h"
#include "SwapChain.h"
#include "Descriptor.h"
#include <span>

class View
{

public:

	void CreateDSV(Texture texture, const uint32_t kClientWidth, const uint32_t kClientHeight, SwapChain swapChain, Descriptor descriptor, Microsoft::WRL::ComPtr<ID3D12Device>& device);

	void CreateSRV(SwapChain swapChain, Descriptor descriptor, Microsoft::WRL::ComPtr<ID3D12Device>& device);

	Microsoft::WRL::ComPtr<ID3D12Resource> GetDepthStencilResource() { return depthStencilResource; };
	std::span<const Microsoft::WRL::ComPtr<ID3D12Resource>> GetSwapChainResource() const { return swapChainResources; }
	std::span<const D3D12_CPU_DESCRIPTOR_HANDLE> GetRtvHandles() const { return rtvHandles; }
	D3D12_DEPTH_STENCIL_VIEW_DESC GetDsvDesc() { return dsvDesc; }
	D3D12_RENDER_TARGET_VIEW_DESC GetRtvDesc() { return rtvDesc; }

private:

	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource;
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources[2] = { nullptr };
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];

};

