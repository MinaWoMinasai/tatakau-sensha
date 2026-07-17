#include "TextureManager.h"
#include <filesystem> // 拡張子判定用

TextureManager* TextureManager::instance = nullptr;

// Imguiで0番を使用するため、1番から使用する
uint32_t TextureManager::kSRVIndexTop = 1;

std::string TextureManager::MakeTextureKey(const std::string& filePath, TextureColorSpace colorSpace)
{
    if (colorSpace == TextureColorSpace::LinearData) {
        return filePath + "#linear";
    }
    return filePath;
}

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

void TextureManager::LoadTexture(const std::string& filePath, TextureColorSpace colorSpace) {
    const std::string textureKey = MakeTextureKey(filePath, colorSpace);
    if (textureDatas.contains(textureKey)) return;

    assert(textureDatas.size() + kSRVIndexTop < SrvManager::kMaxSrvCount);

    DirectX::ScratchImage image{};
    std::wstring filePathW = LogWrite().ConvertString(filePath);
    HRESULT hr;
    const bool linearData = colorSpace == TextureColorSpace::LinearData;
    const DirectX::WIC_FLAGS wicFlags = linearData ? DirectX::WIC_FLAGS_NONE : DirectX::WIC_FLAGS_FORCE_SRGB;
    const DirectX::TEX_FILTER_FLAGS mipFilter = linearData ? DirectX::TEX_FILTER_DEFAULT : DirectX::TEX_FILTER_SRGB;

    // 拡張子で読み込み方法を分岐
    if (std::filesystem::path(filePath).extension() == ".dds") {
        hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
    } else {
        hr = DirectX::LoadFromWICFile(filePathW.c_str(), wicFlags, nullptr, image);
    }
    assert(SUCCEEDED(hr));

    // ミップマップ生成（DDSにミップマップが含まれていない場合のみ実行するのが一般的ですが、ここでは簡略化）
    DirectX::ScratchImage mipImages{};
    if (DirectX::IsCompressed(image.GetMetadata().format)) {
        // 圧縮フォーマットの場合はそのまま使用
        mipImages = std::move(image);
    } else {
        hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), mipFilter, 0, mipImages);
        assert(SUCCEEDED(hr));
    }

    TextureData& textureData = textureDatas[textureKey];
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

bool TextureManager::LoadTextureFromMemory(const std::string& textureKey, const void* data, size_t size, TextureColorSpace colorSpace) {
    const std::string resolvedTextureKey = MakeTextureKey(textureKey, colorSpace);
    if (textureDatas.contains(resolvedTextureKey)) return true;
    if (data == nullptr || size == 0) return false;

    assert(textureDatas.size() + kSRVIndexTop < SrvManager::kMaxSrvCount);

    DirectX::ScratchImage image{};
    const bool linearData = colorSpace == TextureColorSpace::LinearData;
    const DirectX::WIC_FLAGS wicFlags = linearData ? DirectX::WIC_FLAGS_NONE : DirectX::WIC_FLAGS_FORCE_SRGB;
    const DirectX::TEX_FILTER_FLAGS mipFilter = linearData ? DirectX::TEX_FILTER_DEFAULT : DirectX::TEX_FILTER_SRGB;
    HRESULT hr = DirectX::LoadFromWICMemory(
        static_cast<const uint8_t*>(data), size,
        wicFlags, nullptr, image);
    if (FAILED(hr)) {
        return false;
    }

    DirectX::ScratchImage mipImages{};
    hr = DirectX::GenerateMipMaps(
        image.GetImages(), image.GetImageCount(), image.GetMetadata(),
        mipFilter, 0, mipImages);
    if (FAILED(hr)) {
        return false;
    }

    TextureData& textureData = textureDatas[resolvedTextureKey];
    textureData.metaData = mipImages.GetMetadata();
    textureData.resource = texture.CreateResource(dxCommon_->GetDevice(), textureData.metaData);
    textureData.srvIndex = srvManager_->Allocate();
    textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
    textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);
    srvManager_->CreateSRVforTexture2D(
        textureData.srvIndex, textureData.resource.Get(),
        textureData.metaData.format, static_cast<UINT>(textureData.metaData.mipLevels));

    auto intermediateResource = texture.UploadData(
        textureData.resource, mipImages, dxCommon_->GetDevice(), dxCommon_->GetList());
    dxCommon_->ExecuteCommandListAndWait();
    return true;
}

void TextureManager::PreDraw()
{
    if (srvManager_) {
        srvManager_->PreDraw();
    }
}

uint32_t TextureManager::GetTextureIndexbyFilePath(const std::string& filePath, TextureColorSpace colorSpace)
{
    const std::string textureKey = MakeTextureKey(filePath, colorSpace);
	// 読み込み済みテクスチャを検索
	if (textureDatas.contains(textureKey)) {
		return textureDatas[textureKey].srvIndex;
	}
	
	assert(0);
	return false;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string& filePath, TextureColorSpace colorSpace)
{
    const std::string textureKey = MakeTextureKey(filePath, colorSpace);
	// 範囲外指定違反チェック
	//assert(textureIndex > textureDatas.size());
	TextureData& textureData = textureDatas[textureKey];
	return textureData.srvHandleGPU;
}

const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath, TextureColorSpace colorSpace)
{
    const std::string textureKey = MakeTextureKey(filePath, colorSpace);
	
	TextureData& textureData = textureDatas[textureKey];
	return textureData.metaData;
}

uint32_t TextureManager::GetSrvIndex(const std::string& filePath, TextureColorSpace colorSpace)
{
    const std::string textureKey = MakeTextureKey(filePath, colorSpace);
	return textureDatas[textureKey].srvIndex;
}
