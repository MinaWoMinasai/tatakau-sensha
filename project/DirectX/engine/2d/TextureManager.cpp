#include "TextureManager.h"
#include <algorithm>
#include <cmath>
#include <filesystem> // 拡張子判定用
#include <cstdint>

TextureManager* TextureManager::instance = nullptr;

// Imguiで0番を使用するため、1番から使用する
uint32_t TextureManager::kSRVIndexTop = 1;

namespace
{
struct Float2
{
    float x;
    float y;
};

struct Float3
{
    float x;
    float y;
    float z;
};

constexpr float kPi = 3.14159265358979323846f;

float Saturate(float value)
{
    return (std::min)((std::max)(value, 0.0f), 1.0f);
}

Float3 operator+(Float3 lhs, Float3 rhs)
{
    return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

Float3 operator-(Float3 lhs, Float3 rhs)
{
    return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}

Float3 operator*(Float3 value, float scale)
{
    return { value.x * scale, value.y * scale, value.z * scale };
}

Float3 operator/(Float3 value, float scale)
{
    if (std::abs(scale) <= 0.00001f) {
        return {};
    }
    const float invScale = 1.0f / scale;
    return value * invScale;
}

float Dot(Float3 lhs, Float3 rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Float3 Cross(Float3 lhs, Float3 rhs)
{
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x
    };
}

Float3 Normalize(Float3 value)
{
    const float length = std::sqrt((std::max)(Dot(value, value), 0.0f));
    if (length <= 0.00001f) {
        return { 0.0f, 0.0f, 1.0f };
    }
    const float invLength = 1.0f / length;
    return value * invLength;
}

Float3 TangentToWorld(Float3 localDirection, Float3 normal)
{
    const Float3 up = std::fabs(normal.z) < 0.999f
        ? Float3{ 0.0f, 0.0f, 1.0f }
        : Float3{ 1.0f, 0.0f, 0.0f };
    const Float3 tangent = Normalize(Cross(up, normal));
    const Float3 bitangent = Cross(normal, tangent);
    return Normalize(tangent * localDirection.x + bitangent * localDirection.y + normal * localDirection.z);
}

float RadicalInverseVdc(uint32_t bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return static_cast<float>(bits) * 2.3283064365386963e-10f;
}

Float2 Hammersley(uint32_t index, uint32_t sampleCount)
{
    return {
        static_cast<float>(index) / static_cast<float>(sampleCount),
        RadicalInverseVdc(index)
    };
}

Float3 ImportanceSampleGGX(Float2 xi, Float3 normal, float roughness)
{
    const float a = roughness * roughness;
    const float a2 = a * a;
    const float phi = 2.0f * kPi * xi.x;
    const float cosTheta = std::sqrt((1.0f - xi.y) / (1.0f + (a2 - 1.0f) * xi.y));
    const float sinTheta = std::sqrt((std::max)(1.0f - cosTheta * cosTheta, 0.0f));

    const Float3 halfVector = {
        std::cos(phi) * sinTheta,
        std::sin(phi) * sinTheta,
        cosTheta
    };

    return TangentToWorld(halfVector, normal);
}

Float3 CosineSampleHemisphere(Float2 xi, Float3 normal)
{
    const float radius = std::sqrt(xi.x);
    const float phi = 2.0f * kPi * xi.y;
    const Float3 localDirection = {
        std::cos(phi) * radius,
        std::sin(phi) * radius,
        std::sqrt((std::max)(1.0f - xi.x, 0.0f))
    };
    return TangentToWorld(localDirection, normal);
}

Float3 TexelDirectionForCubeFace(size_t face, size_t x, size_t y, size_t width, size_t height)
{
    const float u = (2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(width)) - 1.0f;
    const float v = (2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(height)) - 1.0f;

    switch (face) {
    case 0:
        return Normalize({ 1.0f, -v, -u });
    case 1:
        return Normalize({ -1.0f, -v, u });
    case 2:
        return Normalize({ u, 1.0f, v });
    case 3:
        return Normalize({ u, -1.0f, -v });
    case 4:
        return Normalize({ u, -v, 1.0f });
    case 5:
    default:
        return Normalize({ -u, -v, -1.0f });
    }
}

Float3 SampleProceduralPbrEnvironment(Float3 direction, float lod)
{
    const Float3 dir = Normalize(direction);
    const float horizon = Saturate(dir.y * 0.5f + 0.5f);
    const Float3 sunDir = Normalize({ 0.36f, 0.30f, 0.88f });
    const float sunDot = Saturate(Dot(dir, sunDir));
    const float sun = std::pow(sunDot, 380.0f);
    const float sunGlow = std::pow(sunDot, 14.0f);
    const float sharpLightFade = 1.0f - Saturate((lod - 0.8f) / (4.2f - 0.8f));
    const float broadLightFade = 1.0f - Saturate((lod - 2.0f) / (7.0f - 2.0f));
    const float lowMist = std::pow(1.0f - horizon, 2.8f);
    const float zenithFade = std::pow(horizon, 1.25f);

    auto lerpFloat3 = [](Float3 a, Float3 b, float t) {
        return a * (1.0f - t) + b * t;
    };
    auto smoothstep = [](float edge0, float edge1, float value) {
        const float t = Saturate((value - edge0) / (edge1 - edge0));
        return t * t * (3.0f - 2.0f * t);
    };

    const Float3 zenith = { 0.065f, 0.155f, 0.300f };
    const Float3 midSky = { 0.170f, 0.430f, 0.620f };
    const Float3 horizonSky = { 0.700f, 0.760f, 0.720f };
    const Float3 waterBelow = { 0.045f, 0.260f, 0.340f };
    Float3 color = lerpFloat3(waterBelow, horizonSky, smoothstep(0.0f, 0.42f, horizon));
    color = lerpFloat3(color, midSky, smoothstep(0.16f, 0.70f, horizon));
    color = lerpFloat3(color, zenith, zenithFade * 0.72f);
    color = color + Float3{ 1.0f, 0.76f, 0.48f } * sunGlow * 0.42f * broadLightFade;
    color = color + Float3{ 1.0f, 0.92f, 0.66f } * sun * 5.2f * sharpLightFade;
    color = lerpFloat3(color, { 0.43f, 0.56f, 0.58f }, lowMist * 0.13f);
    return {
        (std::max)(color.x, 0.0f),
        (std::max)(color.y, 0.0f),
        (std::max)(color.z, 0.0f)
    };
}

void StoreFloatCubePixel(const DirectX::Image* imageData, size_t x, size_t y, Float3 color)
{
    assert(imageData != nullptr);
    uint8_t* row = imageData->pixels + imageData->rowPitch * y;
    float* pixel = reinterpret_cast<float*>(row + x * sizeof(float) * 4);
    pixel[0] = color.x;
    pixel[1] = color.y;
    pixel[2] = color.z;
    pixel[3] = 1.0f;
}

float GeometrySchlickGGXForIBL(float nDotV, float roughness)
{
    const float a = roughness;
    const float k = (a * a) * 0.5f;
    return nDotV / (nDotV * (1.0f - k) + k);
}

float GeometrySmithForIBL(Float3 normal, Float3 view, Float3 light, float roughness)
{
    const float nDotV = Saturate(Dot(normal, view));
    const float nDotL = Saturate(Dot(normal, light));
    return GeometrySchlickGGXForIBL(nDotV, roughness) * GeometrySchlickGGXForIBL(nDotL, roughness);
}

Float2 IntegrateBrdf(float nDotV, float roughness)
{
    nDotV = (std::max)(nDotV, 0.001f);
    roughness = (std::max)(roughness, 0.04f);

    const Float3 normal = { 0.0f, 0.0f, 1.0f };
    const Float3 view = {
        std::sqrt((std::max)(1.0f - nDotV * nDotV, 0.0f)),
        0.0f,
        nDotV
    };

    constexpr uint32_t kSampleCount = 256;
    float scale = 0.0f;
    float bias = 0.0f;
    for (uint32_t i = 0; i < kSampleCount; ++i) {
        const Float2 xi = Hammersley(i, kSampleCount);
        const Float3 halfVector = ImportanceSampleGGX(xi, normal, roughness);
        const Float3 light = Normalize(halfVector * (2.0f * Dot(view, halfVector)) - view);

        const float nDotL = Saturate(light.z);
        const float nDotH = Saturate(halfVector.z);
        const float vDotH = Saturate(Dot(view, halfVector));

        if (nDotL > 0.0f) {
            const float geometry = GeometrySmithForIBL(normal, view, light, roughness);
            const float geometryVisibility = geometry * vDotH / (nDotH * nDotV + 0.0001f);
            const float fresnel = std::pow(1.0f - vDotH, 5.0f);
            scale += (1.0f - fresnel) * geometryVisibility;
            bias += fresnel * geometryVisibility;
        }
    }

    return {
        scale / static_cast<float>(kSampleCount),
        bias / static_cast<float>(kSampleCount)
    };
}
}

std::string TextureManager::MakeTextureKey(const std::string& filePath, TextureColorSpace colorSpace)
{
    if (colorSpace == TextureColorSpace::LinearData) {
        return filePath + "#linear";
    }
    return filePath;
}

const std::string& TextureManager::GetFlatNormalTexturePath()
{
    static const std::string kFlatNormalTexturePath = "__engine/flat_normal";
    return kFlatNormalTexturePath;
}

const std::string& TextureManager::GetBrdfLutTexturePath()
{
    static const std::string kBrdfLutTexturePath = "__engine/brdf_lut";
    return kBrdfLutTexturePath;
}

const std::string& TextureManager::GetPbrIrradianceTexturePath()
{
    static const std::string kPbrIrradianceTexturePath = "__engine/pbr_irradiance";
    return kPbrIrradianceTexturePath;
}

const std::string& TextureManager::GetPbrPrefilteredEnvironmentTexturePath()
{
    static const std::string kPbrPrefilteredEnvironmentTexturePath = "__engine/pbr_prefiltered_environment";
    return kPbrPrefilteredEnvironmentTexturePath;
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
	CreateFlatNormalTexture();
	CreateBrdfLutTexture();
	CreatePbrIrradianceTexture();
	CreatePbrPrefilteredEnvironmentTexture();
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

void TextureManager::CreateFlatNormalTexture()
{
    const std::string& filePath = GetFlatNormalTexturePath();
    const std::string textureKey = MakeTextureKey(filePath, TextureColorSpace::LinearData);
    if (textureDatas.contains(textureKey)) return;

    assert(textureDatas.size() + kSRVIndexTop < SrvManager::kMaxSrvCount);

    constexpr size_t kTextureSize = 4;
    DirectX::ScratchImage image{};
    HRESULT hr = image.Initialize2D(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        kTextureSize,
        kTextureSize,
        1,
        1);
    assert(SUCCEEDED(hr));

    const DirectX::Image* imageData = image.GetImage(0, 0, 0);
    assert(imageData != nullptr);
    for (size_t y = 0; y < kTextureSize; ++y) {
        uint8_t* row = imageData->pixels + imageData->rowPitch * y;
        for (size_t x = 0; x < kTextureSize; ++x) {
            uint8_t* pixel = row + x * 4;
            pixel[0] = 128;
            pixel[1] = 128;
            pixel[2] = 255;
            pixel[3] = 255;
        }
    }

    TextureData& textureData = textureDatas[textureKey];
    textureData.metaData = image.GetMetadata();
    textureData.resource = texture.CreateResource(dxCommon_->GetDevice(), textureData.metaData);
    textureData.srvIndex = srvManager_->Allocate();
    textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
    textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);
    srvManager_->CreateSRVforTexture2D(
        textureData.srvIndex,
        textureData.resource.Get(),
        textureData.metaData.format,
        static_cast<UINT>(textureData.metaData.mipLevels));

    auto intermediateResource = texture.UploadData(
        textureData.resource,
        image,
        dxCommon_->GetDevice(),
        dxCommon_->GetList());
    dxCommon_->ExecuteCommandListAndWait();
}

void TextureManager::CreateBrdfLutTexture()
{
    const std::string& filePath = GetBrdfLutTexturePath();
    const std::string textureKey = MakeTextureKey(filePath, TextureColorSpace::LinearData);
    if (textureDatas.contains(textureKey)) return;

    assert(textureDatas.size() + kSRVIndexTop < SrvManager::kMaxSrvCount);

    constexpr size_t kTextureSize = 128;
    DirectX::ScratchImage image{};
    HRESULT hr = image.Initialize2D(
        DXGI_FORMAT_R32G32B32A32_FLOAT,
        kTextureSize,
        kTextureSize,
        1,
        1);
    assert(SUCCEEDED(hr));

    const DirectX::Image* imageData = image.GetImage(0, 0, 0);
    assert(imageData != nullptr);
    for (size_t y = 0; y < kTextureSize; ++y) {
        const float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(kTextureSize);
        uint8_t* row = imageData->pixels + imageData->rowPitch * y;
        for (size_t x = 0; x < kTextureSize; ++x) {
            const float nDotV = (static_cast<float>(x) + 0.5f) / static_cast<float>(kTextureSize);
            const Float2 integratedBrdf = IntegrateBrdf(nDotV, roughness);
            float* pixel = reinterpret_cast<float*>(row + x * sizeof(float) * 4);
            pixel[0] = integratedBrdf.x;
            pixel[1] = integratedBrdf.y;
            pixel[2] = 0.0f;
            pixel[3] = 1.0f;
        }
    }

    TextureData& textureData = textureDatas[textureKey];
    textureData.metaData = image.GetMetadata();
    textureData.resource = texture.CreateResource(dxCommon_->GetDevice(), textureData.metaData);
    textureData.srvIndex = srvManager_->Allocate();
    textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
    textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);
    srvManager_->CreateSRVforTexture2D(
        textureData.srvIndex,
        textureData.resource.Get(),
        textureData.metaData.format,
        static_cast<UINT>(textureData.metaData.mipLevels));

    auto intermediateResource = texture.UploadData(
        textureData.resource,
        image,
        dxCommon_->GetDevice(),
        dxCommon_->GetList());
    dxCommon_->ExecuteCommandListAndWait();
}

void TextureManager::CreatePbrIrradianceTexture()
{
    const std::string& filePath = GetPbrIrradianceTexturePath();
    const std::string textureKey = MakeTextureKey(filePath, TextureColorSpace::LinearData);
    if (textureDatas.contains(textureKey)) return;

    assert(textureDatas.size() + kSRVIndexTop < SrvManager::kMaxSrvCount);

    constexpr size_t kTextureSize = 32;
    constexpr uint32_t kSampleCount = 96;
    DirectX::ScratchImage image{};
    HRESULT hr = image.InitializeCube(
        DXGI_FORMAT_R32G32B32A32_FLOAT,
        kTextureSize,
        kTextureSize,
        1,
        1);
    assert(SUCCEEDED(hr));

    for (size_t face = 0; face < 6; ++face) {
        const DirectX::Image* imageData = image.GetImage(0, face, 0);
        assert(imageData != nullptr);
        for (size_t y = 0; y < kTextureSize; ++y) {
            for (size_t x = 0; x < kTextureSize; ++x) {
                const Float3 normal = TexelDirectionForCubeFace(face, x, y, kTextureSize, kTextureSize);
                Float3 color{};
                for (uint32_t sampleIndex = 0; sampleIndex < kSampleCount; ++sampleIndex) {
                    const Float2 xi = Hammersley(sampleIndex, kSampleCount);
                    const Float3 sampleDirection = CosineSampleHemisphere(xi, normal);
                    color = color + SampleProceduralPbrEnvironment(sampleDirection, 7.0f);
                }
                StoreFloatCubePixel(imageData, x, y, color / static_cast<float>(kSampleCount));
            }
        }
    }

    TextureData& textureData = textureDatas[textureKey];
    textureData.metaData = image.GetMetadata();
    textureData.resource = texture.CreateResource(dxCommon_->GetDevice(), textureData.metaData);
    textureData.srvIndex = srvManager_->Allocate();
    textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
    textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);
    srvManager_->CreateSRVforTextureCube(
        textureData.srvIndex,
        textureData.resource.Get(),
        textureData.metaData.format,
        static_cast<UINT>(textureData.metaData.mipLevels));

    auto intermediateResource = texture.UploadData(
        textureData.resource,
        image,
        dxCommon_->GetDevice(),
        dxCommon_->GetList());
    dxCommon_->ExecuteCommandListAndWait();
}

void TextureManager::CreatePbrPrefilteredEnvironmentTexture()
{
    const std::string& filePath = GetPbrPrefilteredEnvironmentTexturePath();
    const std::string textureKey = MakeTextureKey(filePath, TextureColorSpace::LinearData);
    if (textureDatas.contains(textureKey)) return;

    assert(textureDatas.size() + kSRVIndexTop < SrvManager::kMaxSrvCount);

    constexpr size_t kTextureSize = 128;
    constexpr size_t kMipLevels = 8;
    constexpr uint32_t kSampleCount = 96;
    DirectX::ScratchImage image{};
    HRESULT hr = image.InitializeCube(
        DXGI_FORMAT_R32G32B32A32_FLOAT,
        kTextureSize,
        kTextureSize,
        1,
        kMipLevels);
    assert(SUCCEEDED(hr));

    for (size_t mip = 0; mip < kMipLevels; ++mip) {
        const float roughness = static_cast<float>(mip) / static_cast<float>(kMipLevels - 1);
        for (size_t face = 0; face < 6; ++face) {
            const DirectX::Image* imageData = image.GetImage(mip, face, 0);
            assert(imageData != nullptr);
            for (size_t y = 0; y < imageData->height; ++y) {
                for (size_t x = 0; x < imageData->width; ++x) {
                    const Float3 reflection = TexelDirectionForCubeFace(
                        face, x, y, imageData->width, imageData->height);
                    const Float3 normal = reflection;
                    const Float3 view = reflection;
                    Float3 color{};
                    float totalWeight = 0.0f;
                    for (uint32_t sampleIndex = 0; sampleIndex < kSampleCount; ++sampleIndex) {
                        const Float2 xi = Hammersley(sampleIndex, kSampleCount);
                        const Float3 halfVector = ImportanceSampleGGX(xi, normal, (std::max)(roughness, 0.04f));
                        const Float3 light = Normalize(halfVector * (2.0f * Dot(view, halfVector)) - view);
                        const float nDotL = Saturate(Dot(normal, light));
                        if (nDotL > 0.0f) {
                            color = color + SampleProceduralPbrEnvironment(light, roughness * 7.0f) * nDotL;
                            totalWeight += nDotL;
                        }
                    }

                    if (totalWeight > 0.00001f) {
                        color = color / totalWeight;
                    }
                    StoreFloatCubePixel(imageData, x, y, color);
                }
            }
        }
    }

    TextureData& textureData = textureDatas[textureKey];
    textureData.metaData = image.GetMetadata();
    textureData.resource = texture.CreateResource(dxCommon_->GetDevice(), textureData.metaData);
    textureData.srvIndex = srvManager_->Allocate();
    textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
    textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);
    srvManager_->CreateSRVforTextureCube(
        textureData.srvIndex,
        textureData.resource.Get(),
        textureData.metaData.format,
        static_cast<UINT>(textureData.metaData.mipLevels));

    auto intermediateResource = texture.UploadData(
        textureData.resource,
        image,
        dxCommon_->GetDevice(),
        dxCommon_->GetList());
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
