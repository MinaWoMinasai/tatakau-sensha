#pragma once
#include <d3d12.h>

class State
{
public:

	void Initialize();

	D3D12_DEPTH_STENCIL_DESC GetDepthStencilDesc() { return depthStencilDesc; }
	D3D12_BLEND_DESC GetBlendDesc() { return blendDesc; }
	D3D12_RASTERIZER_DESC GetRasterizerDesc() { return rasterizerDesc; }

private:

	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};

	// BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};

	// RasiterzerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
};

