#pragma once
#include <d3d12.h>
#include <cstdint>

class WindowScreenSize
{
public:
	void Initialize(const uint32_t& kClientWidth, const uint32_t& kClientHeight);

	D3D12_VIEWPORT GetViewport() { return viewport; }
	D3D12_RECT GetSissorRect() { return scissorRect; };

private:

	D3D12_VIEWPORT viewport{};
	D3D12_RECT scissorRect{};
};

