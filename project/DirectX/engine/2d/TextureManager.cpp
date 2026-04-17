#include "TextureManager.h"
#include <filesystem> // 拡張子判定用

TextureManager* TextureManager::instance = nullptr;

// Imguiで0番を使用するため、1番から使用する
uint32_t TextureManager::kSRVIndexTop = 1;

TextureManager* TextureManager::GetInstance() {
	if (instance == nullptr) {
		instance = new TextureManager;
	}
	return instance;
}

void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	textureDatas.reserve(SrvManager::kMaxSrvCount);
}

void TextureManager::Finalize() {
	delete instance;
	instance = nullptr;
}

void TextureManager::LoadTexture(const std::string& filePath) {
    if (textureDatas.contains(filePath)) return;

    assert(textureDatas.size() + kSRVIndexTop < SrvManager::kMaxSrvCount);

    DirectX::ScratchImage image{};
    std::wstring filePathW = LogWrite().ConvertString(filePath);
    HRESULT hr;

    // 拡張子で読み込み方法を分岐
    if (std::filesystem::path(filePath).extension() == ".dds") {
        hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
    } else {
        hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    }
    assert(SUCCEEDED(hr));

    // ミップマップ生成（DDSにミップマップが含まれていない場合のみ実行するのが一般的ですが、ここでは簡略化）
    DirectX::ScratchImage mipImages{};
    if (DirectX::IsCompressed(image.GetMetadata().format)) {
        // 圧縮フォーマットの場合はそのまま使用
        mipImages = std::move(image);
    } else {
        hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
        assert(SUCCEEDED(hr));
    }

    TextureData& textureData = textureDatas[filePath];
    textureData.metaData = mipImages.GetMetadata();
    textureData.resource = texture.CreateResource(dxCommon_->GetDevice(), textureData.metaData);
    textureData.srvIndex = srvManager_->Allocate();
    textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
    textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

    // CubeMapかどうかの判定
    if (textureData.metaData.miscFlags & DirectX::TEX_MISC_TEXTURECUBE) {
        srvManager_->CreateSRVforTextureCube(textureData.srvIndex, textureData.resource.Get(),
            textureData.metaData.format, static_cast<UINT>(textureData.metaData.mipLevels));
    } else {
        srvManager_->CreateSRVforTexture2D(textureData.srvIndex, textureData.resource.Get(),
            textureData.metaData.format, static_cast<UINT>(textureData.metaData.mipLevels));
    }

    auto intermediateResource = texture.UploadData(
        textureData.resource,
        mipImages,
        dxCommon_->GetDevice(),
        dxCommon_->GetList()
    );

    // GPUの完了を待つ。この関数が終わるまで intermediateResource は生存し続ける。
    dxCommon_->ExecuteCommandListAndWait();
}

uint32_t TextureManager::GetTextureIndexbyFilePath(const std::string& filePath)
{
	// 読み込み済みテクスチャを検索
	if (textureDatas.contains(filePath)) {
		uint32_t textureIndex = static_cast<uint32_t>(textureDatas.contains(filePath));
		return textureIndex + kSRVIndexTop;
	}
	
	assert(0);
	return false;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string& filePath)
{
	// 範囲外指定違反チェック
	//assert(textureIndex > textureDatas.size());
	TextureData& textureData = textureDatas[filePath];
	return textureData.srvHandleGPU;
}

const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath)
{
	
	TextureData& textureData = textureDatas[filePath];
	return textureData.metaData;
}

uint32_t TextureManager::GetSrvIndex(const std::string& filePath)
{
	return textureDatas[filePath].srvIndex;
}
