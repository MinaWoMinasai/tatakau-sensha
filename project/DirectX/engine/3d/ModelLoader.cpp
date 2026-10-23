#include "Model.h"
#include "ModelInternal.h"
#include "ResourcePaths.h"
#include "StringUtils.h"
#include "Utf8Path.h"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <utility>

namespace myEngine {

using namespace modelDetail;

namespace {
/// @brief 入力を0〜1の範囲に制限して返す。
float Clamp01(float value);
/// @brief ベクトルの最大成分を返す。
float MaxComponent(const Vector3& value);

/// @brief 材質ファイルの行から、テクスチャのファイル名を読み取る。
std::string ReadTextureFilename(std::istringstream& stream)
{
    std::string token;
    std::string textureFilename;
    while (stream >> token) {
        textureFilename = token;
    }
    return textureFilename;
}

/// @brief テクスチャパスを解決する。
std::string ResolveTexturePath(const std::string& directoryPath, const std::string& textureFilename)
{
    if (textureFilename.empty()) {
        return {};
    }

    const std::filesystem::path texturePath = Utf8ToPath(textureFilename);
    if (texturePath.is_absolute()) {
        return PathToUtf8Generic(texturePath);
    }
    return PathToUtf8Generic(Utf8ToPath(directoryPath) / texturePath);
}

/// @brief マテリアルの名前などから描画用途を推定する。
MaterialSemantic InferMaterialSemantic(const MaterialData& material)
{
    const std::string text = ToLowerAscii(material.materialName + " " + material.textureFilePath + " " + material.normalTextureFilePath +
                                          " " + material.metallicRoughnessTextureFilePath + " " + material.metallicTextureFilePath + " " +
                                          material.roughnessTextureFilePath + " " + material.occlusionTextureFilePath);

    if (material.hasEmissive || ContainsToken(text, "emissive") || ContainsToken(text, "emit")) {
        return MaterialSemantic::Emissive;
    }
    if (ContainsToken(text, "eye") || ContainsToken(text, "iris") || ContainsToken(text, "pupil")) {
        return MaterialSemantic::CharacterEye;
    }
    if (ContainsToken(text, "hair") || ContainsToken(text, "brow") || ContainsToken(text, "lash")) {
        return MaterialSemantic::CharacterHair;
    }
    if (ContainsToken(text, "skin") || ContainsToken(text, "face") || ContainsToken(text, "body") || ContainsToken(text, "hand") ||
        ContainsToken(text, "arm") || ContainsToken(text, "leg") || ContainsToken(text, "head") || ContainsToken(text, "neck") ||
        ContainsToken(text, "mouth") || ContainsToken(text, "lip")) {
        return MaterialSemantic::CharacterSkin;
    }
    if (ContainsToken(text, "cloth") || ContainsToken(text, "clothes") || ContainsToken(text, "dress") || ContainsToken(text, "shirt") ||
        ContainsToken(text, "skirt") || ContainsToken(text, "sleeve") || ContainsToken(text, "uniform") || ContainsToken(text, "wear") ||
        ContainsToken(text, "shoe") || ContainsToken(text, "sock") || ContainsToken(text, "ribbon")) {
        return MaterialSemantic::CharacterCloth;
    }
    if (ContainsToken(text, "glass") || ContainsToken(text, "transparent") || ContainsToken(text, "window") ||
        (material.hasBaseColorFactor && material.baseColorFactor.w < 0.75f)) {
        return MaterialSemantic::Glass;
    }
    if (ContainsToken(text, "metal") || ContainsToken(text, "steel") || ContainsToken(text, "iron") || ContainsToken(text, "gold") ||
        ContainsToken(text, "silver") || ContainsToken(text, "chrome") || material.hasMetallicTexture || material.metallicFactor > 0.65f) {
        return MaterialSemantic::Metal;
    }
    return MaterialSemantic::GenericPbr;
}

/// @brief LowerExtensionを返す。
std::string GetLowerExtension(const std::string& filename)
{
    return ToLowerAscii(PathToUtf8Generic(Utf8ToPath(filename).extension()));
}
} // namespace

namespace modelDetail {

/// @brief OBJ形式モデルであるか判定する。
bool IsObjModel(const std::string& filename)
{
    return GetLowerExtension(filename) == ".obj";
}

} // namespace modelDetail

namespace {

/// @brief Assimp位置を変換する。
Vector4 ConvertAssimpPosition(const aiVector3D& position)
{
    return {-position.x, position.y, position.z, 1.0f};
}

/// @brief Assimp方向を変換する。
Vector3 ConvertAssimpDirection(const aiVector3D& direction)
{
    return {-direction.x, direction.y, direction.z};
}

/// @brief Assimpテクスチャキーを作成して返す。
std::string MakeAssimpTextureKey(const std::filesystem::path& sourcePath, const aiString& texturePath,
                                 TextureManager::TextureColorSpace colorSpace)
{
    std::string key = PathToUtf8Generic(sourcePath) + "#embedded/" + texturePath.C_Str();
    if (colorSpace == TextureManager::TextureColorSpace::LinearData) {
        key += "#linear";
    }
    return key;
}

/// @brief Assimpテクスチャパスを解決する。
std::string ResolveAssimpTexturePath(const aiScene& scene, const std::filesystem::path& sourcePath, const aiString& texturePath,
                                     TextureManager::TextureColorSpace colorSpace)
{
    if (texturePath.length == 0) {
        return {};
    }

    const aiTexture* embeddedTexture = scene.GetEmbeddedTexture(texturePath.C_Str());
    if (embeddedTexture != nullptr) {
        if (embeddedTexture->mHeight == 0 && embeddedTexture->mWidth > 0) {
            const std::string textureKey = MakeAssimpTextureKey(sourcePath, texturePath, colorSpace);
            const auto* encodedBegin = reinterpret_cast<const uint8_t*>(embeddedTexture->pcData);
            if (TextureManager::GetInstance()->LoadTextureFromMemory(textureKey, encodedBegin, embeddedTexture->mWidth, colorSpace)) {
                return textureKey;
            }
        }
        return {};
    }

    std::filesystem::path resolvedPath = Utf8ToPath(texturePath.C_Str());
    if (!resolvedPath.is_absolute()) {
        resolvedPath = sourcePath.parent_path() / resolvedPath;
    }
    return PathToUtf8Generic(resolvedPath.lexically_normal());
}

/// @brief Assimp材質テクスチャを返す。
std::string GetAssimpMaterialTexture(const aiScene& scene, const aiMaterial& sourceMaterial, const std::filesystem::path& sourcePath,
                                     aiTextureType textureType, TextureManager::TextureColorSpace colorSpace)
{
    aiString texturePath;
    if (sourceMaterial.GetTexture(textureType, 0, &texturePath) != AI_SUCCESS) {
        return {};
    }
    return ResolveAssimpTexturePath(scene, sourcePath, texturePath, colorSpace);
}

/// @brief Assimp基準色を読み取る。
void ReadAssimpBaseColor(const aiMaterial& sourceMaterial, MaterialData& materialData)
{
    aiColor4D color{};
    if (aiGetMaterialColor(&sourceMaterial, AI_MATKEY_BASE_COLOR, &color) == AI_SUCCESS ||
        aiGetMaterialColor(&sourceMaterial, AI_MATKEY_COLOR_DIFFUSE, &color) == AI_SUCCESS) {
        materialData.baseColorFactor = {color.r, color.g, color.b, color.a};
        materialData.hasBaseColorFactor = true;
    }
}

/// @brief AssimpPBRFactorsを読み取る。
void ReadAssimpPbrFactors(const aiMaterial& sourceMaterial, MaterialData& materialData)
{
    float metallic = 0.0f;
    if (sourceMaterial.Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS) {
        materialData.metallicFactor = Clamp01(metallic);
        materialData.hasPbrFactors = true;
    }

    float roughness = 0.5f;
    if (sourceMaterial.Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
        materialData.roughnessFactor = Clamp01(roughness);
        materialData.hasPbrFactors = true;
    }

    float glossiness = 0.0f;
    if (sourceMaterial.Get(AI_MATKEY_GLOSSINESS_FACTOR, glossiness) == AI_SUCCESS) {
        materialData.roughnessFactor = Clamp01(1.0f - glossiness);
        materialData.hasPbrFactors = true;
    }
}

/// @brief Assimp自己発光を読み取る。
void ReadAssimpEmissive(const aiMaterial& sourceMaterial, MaterialData& materialData)
{
    aiColor4D emissive{};
    if (aiGetMaterialColor(&sourceMaterial, AI_MATKEY_COLOR_EMISSIVE, &emissive) != AI_SUCCESS) {
        return;
    }

    const Vector3 emissiveColor = {emissive.r, emissive.g, emissive.b};
    const float emissiveMax = MaxComponent(emissiveColor);
    if (emissiveMax <= 0.0001f) {
        return;
    }

    float emissiveIntensity = emissiveMax;
    sourceMaterial.Get(AI_MATKEY_EMISSIVE_INTENSITY, emissiveIntensity);

    materialData.emissiveColor = {
        emissiveColor.x / emissiveMax,
        emissiveColor.y / emissiveMax,
        emissiveColor.z / emissiveMax,
    };
    materialData.emissiveIntensity = (std::max)(emissiveIntensity, emissiveMax);
    materialData.hasEmissive = true;
}

/// @brief AssimpMaterialsを読み込む。
std::vector<MaterialData> LoadAssimpMaterials(const aiScene& scene, const std::filesystem::path& sourcePath)
{
    std::vector<MaterialData> materials;
    materials.reserve((std::max)(scene.mNumMaterials, 1u));

    for (uint32_t materialIndex = 0; materialIndex < scene.mNumMaterials; ++materialIndex) {
        const aiMaterial& sourceMaterial = *scene.mMaterials[materialIndex];
        MaterialData materialData = MakeDefaultPrimitiveMaterial();
        aiString materialName;
        if (sourceMaterial.Get(AI_MATKEY_NAME, materialName) == AI_SUCCESS && materialName.length > 0) {
            materialData.materialName = materialName.C_Str();
        } else {
            materialData.materialName = "material" + std::to_string(materialIndex);
        }

        ReadAssimpBaseColor(sourceMaterial, materialData);
        ReadAssimpPbrFactors(sourceMaterial, materialData);
        ReadAssimpEmissive(sourceMaterial, materialData);

        std::string baseColorTexture =
            GetAssimpMaterialTexture(scene, sourceMaterial, sourcePath, aiTextureType_BASE_COLOR, TextureManager::TextureColorSpace::SRGB);
        if (baseColorTexture.empty()) {
            baseColorTexture =
                GetAssimpMaterialTexture(scene, sourceMaterial, sourcePath, aiTextureType_DIFFUSE, TextureManager::TextureColorSpace::SRGB);
        }
        if (!baseColorTexture.empty()) {
            materialData.textureFilePath = baseColorTexture;
        }

        std::string normalTexture = GetAssimpMaterialTexture(scene, sourceMaterial, sourcePath, aiTextureType_NORMALS,
                                                             TextureManager::TextureColorSpace::LinearData);
        if (normalTexture.empty()) {
            normalTexture = GetAssimpMaterialTexture(scene, sourceMaterial, sourcePath, aiTextureType_NORMAL_CAMERA,
                                                     TextureManager::TextureColorSpace::LinearData);
        }
        if (normalTexture.empty()) {
            normalTexture = GetAssimpMaterialTexture(scene, sourceMaterial, sourcePath, aiTextureType_HEIGHT,
                                                     TextureManager::TextureColorSpace::LinearData);
        }
        if (!normalTexture.empty()) {
            materialData.normalTextureFilePath = normalTexture;
            materialData.hasNormalTexture = true;
        }

        const std::string metallicTexture = GetAssimpMaterialTexture(scene, sourceMaterial, sourcePath, aiTextureType_METALNESS,
                                                                     TextureManager::TextureColorSpace::LinearData);
        const std::string roughnessTexture = GetAssimpMaterialTexture(scene, sourceMaterial, sourcePath, aiTextureType_DIFFUSE_ROUGHNESS,
                                                                      TextureManager::TextureColorSpace::LinearData);
        if (!metallicTexture.empty() && metallicTexture == roughnessTexture) {
            materialData.metallicRoughnessTextureFilePath = metallicTexture;
            materialData.hasMetallicRoughnessTexture = true;
            materialData.hasMetallicTexture = true;
            materialData.hasRoughnessTexture = true;
            materialData.metallicMapChannel = 2.0f;
            materialData.roughnessMapChannel = 1.0f;
        } else {
            if (!metallicTexture.empty()) {
                materialData.metallicTextureFilePath = metallicTexture;
                materialData.hasMetallicTexture = true;
                materialData.metallicMapChannel = 0.0f;
            }
            if (!roughnessTexture.empty()) {
                materialData.roughnessTextureFilePath = roughnessTexture;
                materialData.hasRoughnessTexture = true;
                materialData.roughnessMapChannel = 0.0f;
            }
        }

        const std::string occlusionTexture = GetAssimpMaterialTexture(scene, sourceMaterial, sourcePath, aiTextureType_AMBIENT_OCCLUSION,
                                                                      TextureManager::TextureColorSpace::LinearData);
        if (!occlusionTexture.empty()) {
            materialData.occlusionTextureFilePath = occlusionTexture;
            materialData.hasOcclusionTexture = true;
            materialData.occlusionMapChannel = 0.0f;
        }

        materialData.semantic = InferMaterialSemantic(materialData);
        materialData.semanticInferred = true;
        materials.push_back(std::move(materialData));
    }

    if (materials.empty()) {
        materials.push_back(MakeDefaultPrimitiveMaterial());
    }
    return materials;
}

/// @brief Assimpノードメッシュを末尾へ追加する。
void AppendAssimpNodeMeshes(const aiScene& scene, const aiNode& node, const aiMatrix4x4& parentTransform, ModelData& modelData)
{
    const aiMatrix4x4 nodeTransform = parentTransform * node.mTransformation;
    aiMatrix3x3 normalTransform(nodeTransform);
    normalTransform.Inverse();
    normalTransform.Transpose();

    for (uint32_t nodeMeshIndex = 0; nodeMeshIndex < node.mNumMeshes; ++nodeMeshIndex) {
        const aiMesh& mesh = *scene.mMeshes[node.mMeshes[nodeMeshIndex]];
        const uint32_t vertexOffset = static_cast<uint32_t>(modelData.vertices.size());
        const uint32_t indexStart = static_cast<uint32_t>(modelData.indices.size());
        modelData.vertices.reserve(modelData.vertices.size() + mesh.mNumVertices);

        for (uint32_t vertexIndex = 0; vertexIndex < mesh.mNumVertices; ++vertexIndex) {
            const aiVector3D transformedPosition = nodeTransform * mesh.mVertices[vertexIndex];
            const aiVector3D sourceNormal = mesh.HasNormals() ? mesh.mNormals[vertexIndex] : aiVector3D{0.0f, 1.0f, 0.0f};
            aiVector3D transformedNormal = normalTransform * sourceNormal;
            transformedNormal.NormalizeSafe();
            const aiVector3D sourceUv = mesh.HasTextureCoords(0) ? mesh.mTextureCoords[0][vertexIndex] : aiVector3D{};

            VertexData vertex{};
            vertex.position = ConvertAssimpPosition(transformedPosition);
            vertex.texcoord = {sourceUv.x, 1.0f - sourceUv.y};
            vertex.normal = ConvertAssimpDirection(transformedNormal);
            if (mesh.HasTangentsAndBitangents()) {
                aiVector3D transformedTangent = normalTransform * mesh.mTangents[vertexIndex];
                transformedTangent.NormalizeSafe();
                const Vector3 tangent = ConvertAssimpDirection(transformedTangent);
                vertex.tangent = {tangent.x, tangent.y, tangent.z, 1.0f};
            }
            modelData.vertices.push_back(vertex);
        }

        for (uint32_t faceIndex = 0; faceIndex < mesh.mNumFaces; ++faceIndex) {
            const aiFace& face = mesh.mFaces[faceIndex];
            if (face.mNumIndices != 3) {
                continue;
            }
            modelData.indices.push_back(vertexOffset + face.mIndices[2]);
            modelData.indices.push_back(vertexOffset + face.mIndices[1]);
            modelData.indices.push_back(vertexOffset + face.mIndices[0]);
        }

        const uint32_t indexCount = static_cast<uint32_t>(modelData.indices.size()) - indexStart;
        if (indexCount > 0) {
            uint32_t materialIndex = mesh.mMaterialIndex < modelData.materials.size() ? mesh.mMaterialIndex : 0u;
            aiString materialName;
            if (mesh.mMaterialIndex < scene.mNumMaterials) {
                scene.mMaterials[mesh.mMaterialIndex]->Get(AI_MATKEY_NAME, materialName);
            }
            modelData.submeshes.push_back({
                indexStart,
                indexCount,
                materialIndex,
                materialName.length > 0 ? materialName.C_Str() : "material" + std::to_string(materialIndex),
            });
        }
    }

    for (uint32_t childIndex = 0; childIndex < node.mNumChildren; ++childIndex) {
        AppendAssimpNodeMeshes(scene, *node.mChildren[childIndex], nodeTransform, modelData);
    }
}

/// @brief 入力を0〜1の範囲に制限して返す。
float Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

/// @brief ベクトルの最大成分を返す。
float MaxComponent(const Vector3& value)
{
    return (std::max)((std::max)(value.x, value.y), value.z);
}

/// @brief MTLの鏡面反射指数を粗さへ変換する。
float RoughnessFromMtlSpecularPower(float specularPower)
{
    // WavefrontのNsは0..1000で作られることが多い。
    // 旧来のBlinn/Phong指数をマイクロファセットの粗さ係数へ変換する。
    specularPower = (std::max)(specularPower, 0.0f);
    return std::clamp(std::sqrt(2.0f / (specularPower + 2.0f)), 0.04f, 1.0f);
}

/// @brief ベクトル3を読み取る。
bool ReadVector3(std::istringstream& stream, Vector3& value)
{
    return static_cast<bool>(stream >> value.x >> value.y >> value.z);
}

/// @brief モデルファイルから読み取ったマテリアルとテクスチャ参照を保持する。
struct ParsedMaterialData {
    MaterialData material;
    bool hasExplicitRoughness = false;
    bool hasSpecularPower = false;
    bool hasLegacySpecular = false;
    float legacySpecularStrength = 0.0f;
};

/// @brief 読み取ったマテリアルの不足項目を補い、描画用の値へ整える。
void FinalizeParsedMaterial(ParsedMaterialData& parsedMaterial)
{
    MaterialData& materialData = parsedMaterial.material;
    if (parsedMaterial.hasLegacySpecular && !parsedMaterial.hasSpecularPower && !parsedMaterial.hasExplicitRoughness) {
        materialData.roughnessFactor = std::clamp(0.72f - parsedMaterial.legacySpecularStrength * 0.38f, 0.18f, 0.90f);
        materialData.hasPbrFactors = true;
    }

    if (materialData.textureFilePath.empty()) {
        materialData.textureFilePath = ResourcePaths::DefaultWhiteTexture();
    }
    if (!materialData.hasNormalTexture) {
        materialData.normalTextureFilePath = TextureManager::GetFlatNormalTexturePath();
    }
    if (!materialData.hasMetallicRoughnessTexture) {
        materialData.metallicRoughnessTextureFilePath = TextureManager::GetFlatNormalTexturePath();
    }
    if (!materialData.hasOcclusionTexture) {
        materialData.occlusionTextureFilePath = TextureManager::GetFlatNormalTexturePath();
    }
    materialData.semantic = InferMaterialSemantic(materialData);
    materialData.semanticInferred = true;
}

/// @brief MTLの1行を読み取り、対応する材質項目へ反映する。
void ParseMtlMaterialLine(ParsedMaterialData& parsedMaterial, const std::string& directoryPath, const std::string& identifierLower,
                          std::istringstream& stream)
{
    MaterialData& materialData = parsedMaterial.material;
    auto assignPackedMaterialTexture = [&](const std::string& texturePath, float occlusionChannel, float roughnessChannel,
                                           float metallicChannel, bool includesOcclusion) {
        materialData.metallicRoughnessTextureFilePath = texturePath;
        materialData.hasMetallicRoughnessTexture = !texturePath.empty();
        materialData.hasMetallicTexture = materialData.hasMetallicRoughnessTexture;
        materialData.hasRoughnessTexture = materialData.hasMetallicRoughnessTexture;
        materialData.metallicMapChannel = metallicChannel;
        materialData.roughnessMapChannel = roughnessChannel;
        if (includesOcclusion) {
            materialData.occlusionTextureFilePath = texturePath;
            materialData.hasOcclusionTexture = !texturePath.empty();
            materialData.occlusionMapChannel = occlusionChannel;
        }
    };
    auto assignSeparateScalarTexture = [&](const std::string& texturePath, bool metallicChannel, float channel) {
        if (texturePath.empty()) {
            return;
        }
        if (metallicChannel) {
            materialData.metallicTextureFilePath = texturePath;
            materialData.hasMetallicTexture = true;
            materialData.metallicMapChannel = channel;
        } else {
            materialData.roughnessTextureFilePath = texturePath;
            materialData.hasRoughnessTexture = true;
            materialData.roughnessMapChannel = channel;
        }
    };

    if (identifierLower == "kd") {
        Vector3 baseColor{};
        if (ReadVector3(stream, baseColor)) {
            materialData.baseColorFactor.x = baseColor.x;
            materialData.baseColorFactor.y = baseColor.y;
            materialData.baseColorFactor.z = baseColor.z;
            materialData.hasBaseColorFactor = true;
        }
    } else if (identifierLower == "d") {
        float alpha = 1.0f;
        if (stream >> alpha) {
            materialData.baseColorFactor.w = Clamp01(alpha);
            materialData.hasBaseColorFactor = true;
        }
    } else if (identifierLower == "tr") {
        float transparency = 0.0f;
        if (stream >> transparency) {
            materialData.baseColorFactor.w = Clamp01(1.0f - transparency);
            materialData.hasBaseColorFactor = true;
        }
    } else if (identifierLower == "ks") {
        Vector3 specularColor{};
        if (ReadVector3(stream, specularColor)) {
            parsedMaterial.legacySpecularStrength = Clamp01(MaxComponent(specularColor));
            parsedMaterial.hasLegacySpecular = true;
        }
    } else if (identifierLower == "ns") {
        float specularPower = 0.0f;
        if (stream >> specularPower) {
            materialData.roughnessFactor = RoughnessFromMtlSpecularPower(specularPower);
            materialData.hasPbrFactors = true;
            parsedMaterial.hasSpecularPower = true;
        }
    } else if (identifierLower == "pm" || identifierLower == "metallic") {
        float metallic = 0.0f;
        if (stream >> metallic) {
            materialData.metallicFactor = Clamp01(metallic);
            materialData.hasPbrFactors = true;
        }
    } else if (identifierLower == "pr" || identifierLower == "roughness") {
        float roughness = 0.5f;
        if (stream >> roughness) {
            materialData.roughnessFactor = Clamp01(roughness);
            materialData.hasPbrFactors = true;
            parsedMaterial.hasExplicitRoughness = true;
        }
    } else if (identifierLower == "pa" || identifierLower == "ao" || identifierLower == "ambientocclusion") {
        float occlusion = 1.0f;
        if (stream >> occlusion) {
            materialData.ambientOcclusionFactor = Clamp01(occlusion);
            materialData.hasPbrFactors = true;
        }
    } else if (identifierLower == "ke") {
        Vector3 emissive{};
        if (ReadVector3(stream, emissive)) {
            const float emissiveMax = MaxComponent(emissive);
            if (emissiveMax > 0.0001f) {
                materialData.emissiveColor = {
                    emissive.x / emissiveMax,
                    emissive.y / emissiveMax,
                    emissive.z / emissiveMax,
                };
                materialData.emissiveIntensity = emissiveMax;
                materialData.hasEmissive = true;
            }
        }
    } else if (identifierLower == "map_kd") {
        std::string textureFilename = ReadTextureFilename(stream);
        materialData.textureFilePath = ResolveTexturePath(directoryPath, textureFilename);
    } else if (identifierLower == "map_bump" || identifierLower == "map_normal" || identifierLower == "map_norm" ||
               identifierLower == "bump" || identifierLower == "norm") {
        std::string textureFilename = ReadTextureFilename(stream);
        materialData.normalTextureFilePath = ResolveTexturePath(directoryPath, textureFilename);
        materialData.hasNormalTexture = !materialData.normalTextureFilePath.empty();
    } else if (identifierLower == "map_orm" || identifierLower == "map_arm" || identifierLower == "map_occlusionroughnessmetallic") {
        std::string textureFilename = ReadTextureFilename(stream);
        const std::string texturePath = ResolveTexturePath(directoryPath, textureFilename);
        assignPackedMaterialTexture(texturePath, 0.0f, 1.0f, 2.0f, true);
    } else if (identifierLower == "map_rma") {
        std::string textureFilename = ReadTextureFilename(stream);
        const std::string texturePath = ResolveTexturePath(directoryPath, textureFilename);
        assignPackedMaterialTexture(texturePath, 2.0f, 0.0f, 1.0f, true);
    } else if (identifierLower == "map_mra") {
        std::string textureFilename = ReadTextureFilename(stream);
        const std::string texturePath = ResolveTexturePath(directoryPath, textureFilename);
        assignPackedMaterialTexture(texturePath, 2.0f, 1.0f, 0.0f, true);
    } else if (identifierLower == "map_mr" || identifierLower == "map_metallicroughness") {
        std::string textureFilename = ReadTextureFilename(stream);
        const std::string texturePath = ResolveTexturePath(directoryPath, textureFilename);
        assignPackedMaterialTexture(texturePath, 0.0f, 1.0f, 2.0f, false);
    } else if (identifierLower == "map_pm" || identifierLower == "map_metallic" || identifierLower == "map_metalness") {
        std::string textureFilename = ReadTextureFilename(stream);
        assignSeparateScalarTexture(ResolveTexturePath(directoryPath, textureFilename), true, 0.0f);
    } else if (identifierLower == "map_pr" || identifierLower == "map_roughness") {
        std::string textureFilename = ReadTextureFilename(stream);
        assignSeparateScalarTexture(ResolveTexturePath(directoryPath, textureFilename), false, 0.0f);
    } else if (identifierLower == "map_ao" || identifierLower == "map_occlusion") {
        std::string textureFilename = ReadTextureFilename(stream);
        materialData.occlusionTextureFilePath = ResolveTexturePath(directoryPath, textureFilename);
        materialData.hasOcclusionTexture = !materialData.occlusionTextureFilePath.empty();
        materialData.occlusionMapChannel = 0.0f;
    }
}

/// @brief ディレクトリにある材質のテンプレートをすべて読み込む。
std::vector<std::pair<std::string, MaterialData>> LoadMaterialTemplateLibrary(const std::string& directoryPath, const std::string& filename)
{
    std::vector<std::pair<std::string, MaterialData>> materials;
    std::ifstream file(Utf8ToPath(directoryPath + "/" + filename));
    assert(file.is_open());

    std::string line;
    std::string currentMaterialName;
    ParsedMaterialData currentMaterial;
    bool hasCurrentMaterial = false;

    auto flushCurrentMaterial = [&]() {
        if (!hasCurrentMaterial) {
            return;
        }
        currentMaterial.material.materialName = currentMaterialName.empty() ? "default" : currentMaterialName;
        FinalizeParsedMaterial(currentMaterial);
        materials.push_back({currentMaterialName, currentMaterial.material});
        currentMaterial = {};
        hasCurrentMaterial = false;
        currentMaterialName.clear();
    };

    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream stream(line);
        stream >> identifier;
        if (identifier.empty() || identifier[0] == '#') {
            continue;
        }

        const std::string identifierLower = ToLowerAscii(identifier);
        if (identifierLower == "newmtl") {
            flushCurrentMaterial();
            stream >> currentMaterialName;
            hasCurrentMaterial = true;
            continue;
        }

        if (!hasCurrentMaterial) {
            currentMaterialName = "default";
            hasCurrentMaterial = true;
        }
        ParseMtlMaterialLine(currentMaterial, directoryPath, identifierLower, stream);
    }

    flushCurrentMaterial();
    return materials;
}
} // namespace

MaterialData Model::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
    MaterialData materialData;                                      // 構築するMaterialData
    std::string line;                                               // ファイルから読んだ一行を格納するもの
    std::ifstream file(Utf8ToPath(directoryPath + "/" + filename)); // ファイルを開く
    assert(file.is_open());                                         // 開けなかったらエラー
    bool hasExplicitRoughness = false;
    bool hasSpecularPower = false;
    bool hasLegacySpecular = false;
    float legacySpecularStrength = 0.0f;
    auto assignPackedMaterialTexture = [&](const std::string& texturePath, float occlusionChannel, float roughnessChannel,
                                           float metallicChannel, bool includesOcclusion) {
        materialData.metallicRoughnessTextureFilePath = texturePath;
        materialData.hasMetallicRoughnessTexture = !texturePath.empty();
        materialData.hasMetallicTexture = materialData.hasMetallicRoughnessTexture;
        materialData.hasRoughnessTexture = materialData.hasMetallicRoughnessTexture;
        materialData.metallicMapChannel = metallicChannel;
        materialData.roughnessMapChannel = roughnessChannel;
        if (includesOcclusion) {
            materialData.occlusionTextureFilePath = texturePath;
            materialData.hasOcclusionTexture = !texturePath.empty();
            materialData.occlusionMapChannel = occlusionChannel;
        }
    };
    auto assignSeparateScalarTexture = [&](const std::string& texturePath, bool metallicChannel, float channel) {
        if (texturePath.empty()) {
            return;
        }
        if (metallicChannel) {
            materialData.metallicTextureFilePath = texturePath;
            materialData.hasMetallicTexture = true;
            materialData.metallicMapChannel = channel;
        } else {
            materialData.roughnessTextureFilePath = texturePath;
            materialData.hasRoughnessTexture = true;
            materialData.roughnessMapChannel = channel;
        }
    };

    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier;
        if (identifier.empty() || identifier[0] == '#') {
            continue;
        }

        // identifierに応じた処理
        const std::string identifierLower = ToLowerAscii(identifier);
        if (identifierLower == "kd") {
            Vector3 baseColor{};
            if (ReadVector3(s, baseColor)) {
                materialData.baseColorFactor.x = baseColor.x;
                materialData.baseColorFactor.y = baseColor.y;
                materialData.baseColorFactor.z = baseColor.z;
                materialData.hasBaseColorFactor = true;
            }
        } else if (identifierLower == "d") {
            float alpha = 1.0f;
            if (s >> alpha) {
                materialData.baseColorFactor.w = Clamp01(alpha);
                materialData.hasBaseColorFactor = true;
            }
        } else if (identifierLower == "tr") {
            float transparency = 0.0f;
            if (s >> transparency) {
                materialData.baseColorFactor.w = Clamp01(1.0f - transparency);
                materialData.hasBaseColorFactor = true;
            }
        } else if (identifierLower == "ks") {
            Vector3 specularColor{};
            if (ReadVector3(s, specularColor)) {
                legacySpecularStrength = Clamp01(MaxComponent(specularColor));
                hasLegacySpecular = true;
            }
        } else if (identifierLower == "ns") {
            float specularPower = 0.0f;
            if (s >> specularPower) {
                materialData.roughnessFactor = RoughnessFromMtlSpecularPower(specularPower);
                materialData.hasPbrFactors = true;
                hasSpecularPower = true;
            }
        } else if (identifierLower == "pm" || identifierLower == "metallic") {
            float metallic = 0.0f;
            if (s >> metallic) {
                materialData.metallicFactor = Clamp01(metallic);
                materialData.hasPbrFactors = true;
            }
        } else if (identifierLower == "pr" || identifierLower == "roughness") {
            float roughness = 0.5f;
            if (s >> roughness) {
                materialData.roughnessFactor = Clamp01(roughness);
                materialData.hasPbrFactors = true;
                hasExplicitRoughness = true;
            }
        } else if (identifierLower == "pa" || identifierLower == "ao" || identifierLower == "ambientocclusion") {
            float occlusion = 1.0f;
            if (s >> occlusion) {
                materialData.ambientOcclusionFactor = Clamp01(occlusion);
                materialData.hasPbrFactors = true;
            }
        } else if (identifierLower == "ke") {
            Vector3 emissive{};
            if (ReadVector3(s, emissive)) {
                const float emissiveMax = MaxComponent(emissive);
                if (emissiveMax > 0.0001f) {
                    materialData.emissiveColor = {
                        emissive.x / emissiveMax,
                        emissive.y / emissiveMax,
                        emissive.z / emissiveMax,
                    };
                    materialData.emissiveIntensity = emissiveMax;
                    materialData.hasEmissive = true;
                }
            }
        } else if (identifierLower == "map_kd") {
            std::string textureFilename = ReadTextureFilename(s);
            // 連結してファイルパスにする
            materialData.textureFilePath = ResolveTexturePath(directoryPath, textureFilename);
        } else if (identifierLower == "map_bump" || identifierLower == "map_normal" || identifierLower == "map_norm" ||
                   identifierLower == "bump" || identifierLower == "norm") {
            std::string textureFilename = ReadTextureFilename(s);
            materialData.normalTextureFilePath = ResolveTexturePath(directoryPath, textureFilename);
            materialData.hasNormalTexture = !materialData.normalTextureFilePath.empty();
        } else if (identifierLower == "map_orm" || identifierLower == "map_arm" || identifierLower == "map_occlusionroughnessmetallic") {
            std::string textureFilename = ReadTextureFilename(s);
            const std::string texturePath = ResolveTexturePath(directoryPath, textureFilename);
            assignPackedMaterialTexture(texturePath, 0.0f, 1.0f, 2.0f, true);
        } else if (identifierLower == "map_rma") {
            std::string textureFilename = ReadTextureFilename(s);
            const std::string texturePath = ResolveTexturePath(directoryPath, textureFilename);
            assignPackedMaterialTexture(texturePath, 2.0f, 0.0f, 1.0f, true);
        } else if (identifierLower == "map_mra") {
            std::string textureFilename = ReadTextureFilename(s);
            const std::string texturePath = ResolveTexturePath(directoryPath, textureFilename);
            assignPackedMaterialTexture(texturePath, 2.0f, 1.0f, 0.0f, true);
        } else if (identifierLower == "map_mr" || identifierLower == "map_metallicroughness") {
            std::string textureFilename = ReadTextureFilename(s);
            const std::string texturePath = ResolveTexturePath(directoryPath, textureFilename);
            assignPackedMaterialTexture(texturePath, 0.0f, 1.0f, 2.0f, false);
        } else if (identifierLower == "map_pm" || identifierLower == "map_metallic" || identifierLower == "map_metalness") {
            std::string textureFilename = ReadTextureFilename(s);
            assignSeparateScalarTexture(ResolveTexturePath(directoryPath, textureFilename), true, 0.0f);
        } else if (identifierLower == "map_pr" || identifierLower == "map_roughness") {
            std::string textureFilename = ReadTextureFilename(s);
            assignSeparateScalarTexture(ResolveTexturePath(directoryPath, textureFilename), false, 0.0f);
        } else if (identifierLower == "map_ao" || identifierLower == "map_occlusion") {
            std::string textureFilename = ReadTextureFilename(s);
            materialData.occlusionTextureFilePath = ResolveTexturePath(directoryPath, textureFilename);
            materialData.hasOcclusionTexture = !materialData.occlusionTextureFilePath.empty();
            materialData.occlusionMapChannel = 0.0f;
        }
    }

    if (hasLegacySpecular && !hasSpecularPower && !hasExplicitRoughness) {
        materialData.roughnessFactor = std::clamp(0.72f - legacySpecularStrength * 0.38f, 0.18f, 0.90f);
        materialData.hasPbrFactors = true;
    }

    if (materialData.textureFilePath.empty()) {
        materialData.textureFilePath = ResourcePaths::DefaultWhiteTexture();
    }
    if (!materialData.hasNormalTexture) {
        materialData.normalTextureFilePath = TextureManager::GetFlatNormalTexturePath();
    }
    if (!materialData.hasMetallicRoughnessTexture) {
        materialData.metallicRoughnessTextureFilePath = TextureManager::GetFlatNormalTexturePath();
    }
    if (!materialData.hasOcclusionTexture) {
        materialData.occlusionTextureFilePath = TextureManager::GetFlatNormalTexturePath();
    }

    return materialData;
}

ModelData Model::LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
    ModelData modelData;
    std::vector<Vector4> positions; // 位置
    std::vector<Vector3> normals;   // 法線
    std::vector<Vector2> texcoords; // テクスチャ座標
    std::string line;               // ファイルから読んだ一行を格納するもの
    std::string currentMaterialName = "default";
    uint32_t currentMaterialIndex = 0;
    std::unordered_map<std::string, uint32_t> materialNameToIndex;
    std::unordered_map<std::string, uint32_t> vertexDefinitionToIndex;

    std::ifstream file(Utf8ToPath(directoryPath + "/" + filename)); // ファイルを開く
    assert(file.is_open());                                         // 開けなかったらエラー

    auto ensureDefaultMaterial = [&]() {
        if (!modelData.materials.empty()) {
            return;
        }
        modelData.materials.push_back(MakeDefaultPrimitiveMaterial());
        modelData.material = modelData.materials.front();
        materialNameToIndex["default"] = 0;
        currentMaterialName = "default";
        currentMaterialIndex = 0;
    };

    auto findOrCreateMaterialIndex = [&](const std::string& materialName) -> uint32_t {
        ensureDefaultMaterial();
        const std::string lookupName = materialName.empty() ? "default" : materialName;
        auto materialIt = materialNameToIndex.find(lookupName);
        if (materialIt != materialNameToIndex.end()) {
            return materialIt->second;
        }
        const uint32_t newMaterialIndex = static_cast<uint32_t>(modelData.materials.size());
        MaterialData materialData = MakeDefaultPrimitiveMaterial();
        materialData.materialName = lookupName;
        materialData.semantic = InferMaterialSemantic(materialData);
        materialData.semanticInferred = true;
        modelData.materials.push_back(std::move(materialData));
        materialNameToIndex[lookupName] = newMaterialIndex;
        return newMaterialIndex;
    };

    auto appendSubmesh = [&](uint32_t materialIndex, const std::string& materialName, uint32_t startIndex, uint32_t indexCount) {
        if (indexCount == 0) {
            return;
        }
        if (!modelData.submeshes.empty()) {
            ModelSubmesh& lastSubmesh = modelData.submeshes.back();
            if (lastSubmesh.materialIndex == materialIndex && lastSubmesh.startIndex + lastSubmesh.indexCount == startIndex) {
                lastSubmesh.indexCount += indexCount;
                return;
            }
        }
        modelData.submeshes.push_back({startIndex, indexCount, materialIndex, materialName});
    };

    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier; // 先端の識別子を読む

        const std::string identifierLower = ToLowerAscii(identifier);
        if (identifierLower == "v") {
            Vector4 position;
            s >> position.x >> position.y >> position.z;
            position.w = 1.0f;
            position.x *= 1.0f;
            position.y *= 1.0f;
            position.z *= -1.0f;
            positions.push_back(position);
        } else if (identifierLower == "vt") {
            Vector2 texcoord;
            s >> texcoord.x >> texcoord.y;
            texcoord.y = 1.0f - texcoord.y;

            texcoords.push_back(texcoord);
        } else if (identifierLower == "vn") {
            Vector3 normal;
            s >> normal.x >> normal.y >> normal.z;
            normal.x *= -1.0f;
            normals.push_back(normal);
        } else if (identifierLower == "f") {
            ensureDefaultMaterial();
            const uint32_t faceStartIndex = static_cast<uint32_t>(modelData.indices.size());
            std::vector<uint32_t> faceIndices;
            std::string vertexDef;
            while (s >> vertexDef) {
                auto existing = vertexDefinitionToIndex.find(vertexDef);
                if (existing != vertexDefinitionToIndex.end()) {
                    faceIndices.push_back(existing->second);
                    continue;
                }

                std::istringstream v(vertexDef);

                std::string indexStr;
                uint32_t elementIndices[3] = {0, 0, 0};
                int i = 0;
                while (std::getline(v, indexStr, '/') && i < 3) {
                    if (!indexStr.empty()) {
                        elementIndices[i] = std::stoi(indexStr);
                    } else {
                        elementIndices[i] = 0;
                    }
                    ++i;
                }

                Vector4 position = (elementIndices[0] > 0) ? positions[elementIndices[0] - 1] : Vector4{};
                Vector2 texcoord = (elementIndices[1] > 0) ? texcoords[elementIndices[1] - 1] : Vector2{};
                Vector3 normal = (elementIndices[2] > 0) ? normals[elementIndices[2] - 1] : Vector3{};

                const uint32_t vertexIndex = static_cast<uint32_t>(modelData.vertices.size());
                modelData.vertices.push_back({position, texcoord, normal});
                vertexDefinitionToIndex.emplace(vertexDef, vertexIndex);
                faceIndices.push_back(vertexIndex);
            }

            // 三角形だけでなく四角形以上もfanで三角形化する。
            // 座標系変換に合わせて頂点順を反転する。
            for (size_t index = 1; index + 1 < faceIndices.size(); ++index) {
                modelData.indices.push_back(faceIndices[index + 1]);
                modelData.indices.push_back(faceIndices[index]);
                modelData.indices.push_back(faceIndices[0]);
            }
            const uint32_t faceIndexCount = static_cast<uint32_t>(modelData.indices.size()) - faceStartIndex;
            appendSubmesh(currentMaterialIndex, currentMaterialName, faceStartIndex, faceIndexCount);
        } else if (identifierLower == "mtllib") {
            // materialTemplateLiblaryファイルの名前を取得する
            std::string materialFilename;
            s >> materialFilename;
            // 基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
            const auto loadedMaterials = LoadMaterialTemplateLibrary(directoryPath, materialFilename);
            if (!loadedMaterials.empty()) {
                modelData.materials.clear();
                materialNameToIndex.clear();
                for (const auto& [materialName, materialData] : loadedMaterials) {
                    const uint32_t materialIndex = static_cast<uint32_t>(modelData.materials.size());
                    const std::string resolvedName = materialName.empty() ? "material" + std::to_string(materialIndex) : materialName;
                    MaterialData resolvedMaterialData = materialData;
                    if (resolvedMaterialData.materialName.empty() || resolvedMaterialData.materialName == "default") {
                        resolvedMaterialData.materialName = resolvedName;
                        resolvedMaterialData.semantic = InferMaterialSemantic(resolvedMaterialData);
                        resolvedMaterialData.semanticInferred = true;
                    }
                    modelData.materials.push_back(std::move(resolvedMaterialData));
                    materialNameToIndex[resolvedName] = materialIndex;
                }
                modelData.material = modelData.materials.front();
                currentMaterialName = loadedMaterials.front().first.empty() ? "material0" : loadedMaterials.front().first;
                currentMaterialIndex = 0;
            }
        } else if (identifierLower == "usemtl" || identifierLower == "newmtl") {
            s >> currentMaterialName;
            currentMaterialIndex = findOrCreateMaterialIndex(currentMaterialName);
        }
    }
    if (modelData.materials.empty()) {
        ensureDefaultMaterial();
    }
    if (modelData.submeshes.empty() && !modelData.indices.empty()) {
        modelData.submeshes.push_back({
            0,
            static_cast<uint32_t>(modelData.indices.size()),
            0,
            "default",
        });
    }
    modelData.material = modelData.materials.front();
    return modelData;
}

ModelData Model::LoadAssimpFile(const std::string& directoryPath, const std::string& filename)
{
    const std::filesystem::path sourcePath = Utf8ToPath(directoryPath) / Utf8ToPath(filename);

    Assimp::Importer importer;
    constexpr unsigned int kImportFlags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals |
                                          aiProcess_CalcTangentSpace | aiProcess_ImproveCacheLocality | aiProcess_SortByPType;

    // Assimp は Windows でもファイル名をUTF-8として扱う。
    const std::string sourcePathUtf8 = PathToUtf8Generic(sourcePath);
    const aiScene* scene = importer.ReadFile(sourcePathUtf8, kImportFlags);
    if (scene == nullptr || scene->mRootNode == nullptr || scene->mNumMeshes == 0) {
        throw std::runtime_error("Failed to load static model: " + sourcePathUtf8 + " / " + importer.GetErrorString());
    }

    ModelData modelData;
    modelData.materials = LoadAssimpMaterials(*scene, sourcePath);
    AppendAssimpNodeMeshes(*scene, *scene->mRootNode, aiMatrix4x4{}, modelData);

    if (modelData.materials.empty()) {
        modelData.materials.push_back(MakeDefaultPrimitiveMaterial());
    }
    if (modelData.submeshes.empty() && !modelData.indices.empty()) {
        modelData.submeshes.push_back({
            0,
            static_cast<uint32_t>(modelData.indices.size()),
            0,
            "default",
        });
    }
    modelData.material = modelData.materials.front();
    return modelData;
}

} // namespace myEngine
