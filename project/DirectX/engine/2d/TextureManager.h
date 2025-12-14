#pragma once
#include <string>
#include "DirectXCommon.h"
#include "SrvManager.h"
#include <unordered_map>

class TextureManager
{

public:
	static TextureManager* GetInstance();
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Finalize();

	void LoadTexture(const std::string& filePath);

	uint32_t GetTextureIndexbyFilePath(const std::string& filePath);

	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath);

	// SRVインデックスの開始番号
	static uint32_t kSRVIndexTop;

	// メタデータを取得
	const DirectX::TexMetadata& GetMetaData(const std::string& filePath);

	// SRVインデックスを取得
	uint32_t GetSrvIndex(const std::string& filePath);

private:

	struct TextureData {
		DirectX::TexMetadata metaData;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		uint32_t srvIndex;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
	};

	static TextureManager* instance;

	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(TextureManager&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;

	// テクスチャデータ
	std::unordered_map<std::string, TextureData> textureDatas;
		
	Texture texture;
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
};

