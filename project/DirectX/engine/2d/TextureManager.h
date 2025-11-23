#pragma once
#include <string>
#include "DirectXCommon.h"

class TextureManager
{

public:
	static TextureManager* GetInstance();
	void Initialize(DirectXCommon* dxCommon);
	void Finalize();

	void LoadTexture(const std::string& filePath);

	uint32_t GetTextureIndexbyFilePath(const std::string& filePath);

	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(uint32_t textureIndex);

	// SRVインデックスの開始番号
	static uint32_t kSRVIndexTop;

private:

	struct TextureData {
		std::string filePath;
		DirectX::TexMetadata metaData;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
	};

	static TextureManager* instance;

	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(TextureManager&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;

	// テクスチャデータ
	std::vector<TextureData> textureDatas;
		
	Texture texture;
	DirectXCommon* dxCommon_ = nullptr;
};

