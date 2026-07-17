#pragma once
#include <string>
#include "DirectXCommon.h"
#include "SrvManager.h"
#include <cstddef>
#include <cstdint>
#include <unordered_map>

class TextureManager
{

public:
	enum class TextureColorSpace {
		SRGB,
		LinearData,
	};

	static TextureManager* GetInstance();
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Finalize();

	void LoadTexture(const std::string& filePath, TextureColorSpace colorSpace = TextureColorSpace::SRGB);
	// GLBなどに埋め込まれたPNG/JPEGを仮想キー付きで直接読み込む。
	bool LoadTextureFromMemory(const std::string& textureKey, const void* data, size_t size, TextureColorSpace colorSpace = TextureColorSpace::SRGB);
	void PreDraw();

	uint32_t GetTextureIndexbyFilePath(const std::string& filePath, TextureColorSpace colorSpace = TextureColorSpace::SRGB);

	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath, TextureColorSpace colorSpace = TextureColorSpace::SRGB);

	// SRVインデックスの開始番号
	static uint32_t kSRVIndexTop;

	// メタデータを取得
	const DirectX::TexMetadata& GetMetaData(const std::string& filePath, TextureColorSpace colorSpace = TextureColorSpace::SRGB);

	// SRVインデックスを取得
	uint32_t GetSrvIndex(const std::string& filePath, TextureColorSpace colorSpace = TextureColorSpace::SRGB);

private:

	struct TextureData {
		DirectX::TexMetadata metaData;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		uint32_t srvIndex;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
	};

	static std::string MakeTextureKey(const std::string& filePath, TextureColorSpace colorSpace);

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

