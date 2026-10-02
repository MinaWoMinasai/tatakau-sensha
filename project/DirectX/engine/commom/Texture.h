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

namespace cg2 {

/// @brief GPUテクスチャとアップロード時に必要な情報をまとめる。
class Texture {
public:
    /// @brief バッファリソースを生成する。
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(Microsoft::WRL::ComPtr<ID3D12Device>& device, size_t sizeInBytes);

    // テクスチャデータを読む
    DirectX::ScratchImage Load(const std::string& filePath);

    /// @brief リソースを生成する。
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateResource(Microsoft::WRL::ComPtr<ID3D12Device>& device,
                                                          const DirectX::TexMetadata& metadata);

    /// @brief 深度ステンシルリソースを生成する。
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilResource(Microsoft::WRL::ComPtr<ID3D12Device>& device, int32_t width,
                                                                      int32_t height);

    /// @brief データをGPUへ転送する。
    [[nodiscard]]
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadData(Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
                                                      const DirectX::ScratchImage& mipImages, Microsoft::WRL::ComPtr<ID3D12Device>& device,
                                                      const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList);
};

} // namespace cg2
