#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"
#include "LogWrite.h"



class Texture
{
public:

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(Microsoft::WRL::ComPtr<ID3D12Device>& device, size_t sizeInBytes);

	// テクスチャデータを読む
	DirectX::ScratchImage Load(const std::string& filePath);

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateResource(Microsoft::WRL::ComPtr<ID3D12Device>& device, const DirectX::TexMetadata& metadata);

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilResouce(Microsoft::WRL::ComPtr<ID3D12Device>& device, int32_t width, int32_t height);

	[[nodiscard]]
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadData(Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages, Microsoft::WRL::ComPtr<ID3D12Device>& device, const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList);


};

