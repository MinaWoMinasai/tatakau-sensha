#pragma once
#include "MathTypes.h"
#include <cstddef>
#include <cstdint>
#include <string>

namespace myEngine {

// GPU の定数バッファの配置。フィールドの順番は resources/shaders/Object3d.PS.hlsl の
// Material と完全に一致させること。
/// @brief シェーダーへ渡す材質・色・ライティング設定を表す。
struct alignas(16) Material {
    Vector4 color;
    int32_t enableLighting;
    int32_t lightingMode;
    float environmentCoefficient; // 追加：環境マッピング係数 (0.0~1.0)
    float padding;                // 16バイトアライメントのための調整
    Matrix4x4 uvTransform;
    float shininess;
    float waterDiagnosticsEnabled;
    float waterSunPathEnabled;
    float waterAtmosphereEnabled;
    float waterFarFlattenEnabled;
    float waterProceduralCloudReflectionEnabled;
    float waterDebugMode;
    float waterAtmosphereStrength;
    float waterFarFlattenStrength;
    float metallic;
    float roughness;
    float ambientOcclusion;
    Vector3 emissiveColor;
    float emissiveIntensity;
    float iblDiffuseIntensity;
    float iblSpecularIntensity;
    float iblMaxMipLevel;
    float pbrEnvironmentMode;
    float shadowReceiveStrength;
    float normalDetailStrength;
    float normalDetailScale;
    float normalMapStrength;
    float metallicMapStrength;
    float roughnessMapStrength;
    float occlusionMapStrength;
    float metallicMapChannel;
    float roughnessMapChannel;
    float occlusionMapChannel;
    float shadowDepthBias;
    float shadowSlopeBias;
    float shadowPcfRadius;
    float materialDebugMode;
    float materialDebugPadding[2];
    float characterLightWrap;
    float characterShadowSoftness;
    float characterShadowStrength;
    float characterRimStrength;
    float characterRimPower;
    float characterSpecularStrength;
    float characterSpecularPower;
    float characterPadding;
    float crystalEnabled;
    float crystalFresnelPower;
    float iridescenceFactor;
    float iridescenceIor;
    float iridescenceThicknessMinimumNm; // 薄膜の厚さ（ナノメートル）。
    float iridescenceThicknessMaximumNm; // 薄膜の厚さ（ナノメートル）。
    float crystalEdgeEmission;
    float crystalCoreEmission;
    Vector3 crystalCoreColor;
    float crystalCorePadding;
    Vector3 crystalEdgeColor;
    float crystalEdgePadding;
    float enableCaustics;
    float causticsScale;
    float causticsIntensity;
    float causticsPadding;
    Vector3 causticsColor;
    float causticsColorPadding;
    float causticsAnimationEnabled;
    float causticsPlaybackTime;
    float causticsLoopDuration;
    float causticsFrameCount;
    float causticsAtlasColumns;
    float causticsAtlasRows;
    float causticsAnimationPadding[2];
};

static_assert(sizeof(Material) % 16 == 0, "Material constant buffer must be 16-byte aligned.");
static_assert(sizeof(Material) == 400, "Update the HLSL Material layout when changing Material.");
static_assert(offsetof(Material, uvTransform) == 32);
static_assert(offsetof(Material, emissiveColor) == 144);
static_assert(offsetof(Material, characterLightWrap) == 240);
static_assert(offsetof(Material, crystalEnabled) == 272);
static_assert(offsetof(Material, iridescenceThicknessMinimumNm) == 288);
static_assert(offsetof(Material, crystalCoreColor) == 304);
static_assert(offsetof(Material, crystalEdgeColor) == 320);
static_assert(offsetof(Material, enableCaustics) == 336);
static_assert(offsetof(Material, causticsColor) == 352);
static_assert(offsetof(Material, causticsAnimationEnabled) == 368);
static_assert(offsetof(Material, causticsAtlasColumns) == 384);

/// @brief 結晶材質の透過・発光・屈折などの外観を指定する。
struct CrystalMaterialSettings {
    bool enabled = true;
    float fresnelPower = 4.6f;
    float iridescenceFactor = 0.72f;
    float iridescenceIor = 1.52f;
    float thicknessMinimumNm = 360.0f;
    float thicknessMaximumNm = 720.0f;
    float edgeEmission = 3.0f;
    float coreEmission = 0.08f;
    Vector3 coreColor = {0.08f, 0.48f, 0.82f};
    Vector3 edgeColor = {0.92f, 0.20f, 0.72f};
};

/// @brief 既定値材質を作成して返す。
inline Material MakeDefaultMaterial()
{
    Material material{};
    material.color = {1.0f, 1.0f, 1.0f, 1.0f};
    material.enableLighting = false;
    material.lightingMode = 0;
    material.environmentCoefficient = 0.0f;
    material.padding = 0.0f;
    material.uvTransform = {};
    material.uvTransform.m[0][0] = 1.0f;
    material.uvTransform.m[1][1] = 1.0f;
    material.uvTransform.m[2][2] = 1.0f;
    material.uvTransform.m[3][3] = 1.0f;
    material.shininess = 32.0f;
    material.waterDiagnosticsEnabled = 0.0f;
    material.waterSunPathEnabled = 1.0f;
    material.waterAtmosphereEnabled = 1.0f;
    material.waterFarFlattenEnabled = 1.0f;
    material.waterProceduralCloudReflectionEnabled = 1.0f;
    material.waterDebugMode = 0.0f;
    material.waterAtmosphereStrength = 1.0f;
    material.waterFarFlattenStrength = 1.0f;
    material.metallic = 0.0f;
    material.roughness = 0.5f;
    material.ambientOcclusion = 1.0f;
    material.emissiveColor = {0.0f, 0.0f, 0.0f};
    material.emissiveIntensity = 0.0f;
    material.iblDiffuseIntensity = 1.0f;
    material.iblSpecularIntensity = 1.0f;
    material.iblMaxMipLevel = 7.0f;
    material.pbrEnvironmentMode = 0.0f;
    material.shadowReceiveStrength = 1.0f;
    material.normalDetailStrength = 0.0f;
    material.normalDetailScale = 24.0f;
    material.normalMapStrength = 0.0f;
    material.metallicMapStrength = 0.0f;
    material.roughnessMapStrength = 0.0f;
    material.occlusionMapStrength = 0.0f;
    material.metallicMapChannel = 2.0f;
    material.roughnessMapChannel = 1.0f;
    material.occlusionMapChannel = 0.0f;
    material.shadowDepthBias = 0.00035f;
    material.shadowSlopeBias = 0.0018f;
    material.shadowPcfRadius = 1.0f;
    material.materialDebugMode = 0.0f;
    material.materialDebugPadding[0] = 0.0f;
    material.materialDebugPadding[1] = 0.0f;
    material.characterLightWrap = 0.28f;
    material.characterShadowSoftness = 0.16f;
    material.characterShadowStrength = 0.54f;
    material.characterRimStrength = 0.12f;
    material.characterRimPower = 3.2f;
    material.characterSpecularStrength = 0.075f;
    material.characterSpecularPower = 42.0f;
    material.characterPadding = 0.0f;
    material.crystalEnabled = 0.0f;
    material.crystalFresnelPower = 4.6f;
    material.iridescenceFactor = 0.72f;
    material.iridescenceIor = 1.52f;
    material.iridescenceThicknessMinimumNm = 360.0f;
    material.iridescenceThicknessMaximumNm = 720.0f;
    material.crystalEdgeEmission = 3.0f;
    material.crystalCoreEmission = 0.08f;
    material.crystalCoreColor = {0.08f, 0.48f, 0.82f};
    material.crystalCorePadding = 0.0f;
    material.crystalEdgeColor = {0.92f, 0.20f, 0.72f};
    material.crystalEdgePadding = 0.0f;
    material.enableCaustics = 0.0f;
    material.causticsScale = 0.035f;
    material.causticsIntensity = 0.25f;
    material.causticsPadding = 0.0f;
    material.causticsColor = {0.75f, 0.92f, 1.0f};
    material.causticsColorPadding = 0.0f;
    material.causticsAnimationEnabled = 0.0f;
    material.causticsPlaybackTime = 0.0f;
    material.causticsLoopDuration = 4.0f;
    material.causticsFrameCount = 1.0f;
    material.causticsAtlasColumns = 1.0f;
    material.causticsAtlasRows = 1.0f;
    material.causticsAnimationPadding[0] = 0.0f;
    material.causticsAnimationPadding[1] = 0.0f;
    return material;
}

/// @brief マテリアルの用途。描画時の扱いを切り替えるために使う。
enum class MaterialSemantic : uint32_t {
    Unknown = 0,
    GenericPbr,
    CharacterSkin,
    CharacterHair,
    CharacterCloth,
    CharacterEye,
    Metal,
    Glass,
    Emissive,
};

/// @brief 材質用途を表示用の名前へ変換する。
inline const char* MaterialSemanticName(MaterialSemantic semantic)
{
    switch (semantic) {
    case MaterialSemantic::GenericPbr:
        return "Generic PBR";
    case MaterialSemantic::CharacterSkin:
        return "Character skin";
    case MaterialSemantic::CharacterHair:
        return "Character hair";
    case MaterialSemantic::CharacterCloth:
        return "Character cloth";
    case MaterialSemantic::CharacterEye:
        return "Character eye";
    case MaterialSemantic::Metal:
        return "Metal";
    case MaterialSemantic::Glass:
        return "Glass";
    case MaterialSemantic::Emissive:
        return "Emissive";
    default:
        return "Unknown";
    }
}

/// @brief モデルの材質とテクスチャの参照情報を保持する。
struct MaterialData {
    std::string materialName;
    MaterialSemantic semantic = MaterialSemantic::GenericPbr;
    bool semanticInferred = false;
    std::string textureFilePath;
    uint32_t textureIndex = 0;
    Vector4 baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    bool hasBaseColorFactor = false;
    float metallicFactor = 0.0f;
    float roughnessFactor = 0.5f;
    float ambientOcclusionFactor = 1.0f;
    bool hasPbrFactors = false;
    Vector3 emissiveColor = {0.0f, 0.0f, 0.0f};
    float emissiveIntensity = 0.0f;
    bool hasEmissive = false;
    std::string normalTextureFilePath;
    uint32_t normalTextureIndex = 0;
    bool hasNormalTexture = false;
    std::string metallicRoughnessTextureFilePath;
    uint32_t metallicRoughnessTextureIndex = 0;
    bool hasMetallicRoughnessTexture = false;
    std::string metallicTextureFilePath;
    std::string roughnessTextureFilePath;
    bool hasMetallicTexture = false;
    bool hasRoughnessTexture = false;
    float metallicMapChannel = 2.0f;
    float roughnessMapChannel = 1.0f;
    std::string occlusionTextureFilePath;
    uint32_t occlusionTextureIndex = 0;
    bool hasOcclusionTexture = false;
    float occlusionMapChannel = 0.0f;
};

} // namespace myEngine
