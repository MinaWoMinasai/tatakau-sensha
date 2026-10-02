#include "Model.h"
#include "StringUtils.h"
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

namespace cg2 {

namespace {
float Clamp01(float value);
float MaxComponent(const Vector3& value);
MaterialData MakeDefaultPrimitiveMaterial();

struct SmoothNormalKey {
	int64_t x;
	int64_t y;
	int64_t z;

	bool operator==(const SmoothNormalKey& other) const
	{
		return x == other.x && y == other.y && z == other.z;
	}
};

struct SmoothNormalKeyHash {
	size_t operator()(const SmoothNormalKey& key) const
	{
		size_t h = std::hash<int64_t>{}(key.x);
		h ^= std::hash<int64_t>{}(key.y) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
		h ^= std::hash<int64_t>{}(key.z) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
		return h;
	}
};

SmoothNormalKey MakeSmoothNormalKey(const Vector4& position)
{
	constexpr float kScale = 10000.0f;
	return {
		static_cast<int64_t>(std::llround(position.x * kScale)),
		static_cast<int64_t>(std::llround(position.y * kScale)),
		static_cast<int64_t>(std::llround(position.z * kScale)),
	};
}

Vector3 ToVector3(const Vector4& value)
{
	return { value.x, value.y, value.z };
}

Vector3 SubtractVector3(const Vector3& a, const Vector3& b)
{
	return { a.x - b.x, a.y - b.y, a.z - b.z };
}

Vector3 CrossVector3(const Vector3& a, const Vector3& b)
{
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x,
	};
}

float DotVector3(const Vector3& a, const Vector3& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

float LengthVector3(const Vector3& value)
{
	return std::sqrt(DotVector3(value, value));
}

Vector3 NormalizeVector3(const Vector3& value)
{
	const float length = LengthVector3(value);
	if (length <= 0.00001f) {
		return { 0.0f, 1.0f, 0.0f };
	}
	return { value.x / length, value.y / length, value.z / length };
}

Vector3 MultiplyVector3(const Vector3& value, float scalar)
{
	return { value.x * scalar, value.y * scalar, value.z * scalar };
}

Vector3 AddVector3(const Vector3& a, const Vector3& b)
{
	return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Vector3 BuildFallbackTangent(const Vector3& normal)
{
	const Vector3 up = std::abs(normal.y) < 0.95f ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
	return NormalizeVector3(CrossVector3(up, normal));
}

std::string ReadTextureFilename(std::istringstream& stream)
{
	std::string token;
	std::string textureFilename;
	while (stream >> token) {
		textureFilename = token;
	}
	return textureFilename;
}

std::string ResolveTexturePath(const std::string& directoryPath, const std::string& textureFilename)
{
	if (textureFilename.empty()) {
		return {};
	}

	const std::filesystem::path texturePath(textureFilename);
	if (texturePath.is_absolute()) {
		return texturePath.generic_string();
	}
	return (std::filesystem::path(directoryPath) / texturePath).generic_string();
}

MaterialSemantic InferMaterialSemantic(const MaterialData& material)
{
	const std::string text = ToLowerAscii(
		material.materialName + " " +
		material.textureFilePath + " " +
		material.normalTextureFilePath + " " +
		material.metallicRoughnessTextureFilePath + " " +
		material.metallicTextureFilePath + " " +
		material.roughnessTextureFilePath + " " +
		material.occlusionTextureFilePath);

	if (material.hasEmissive || ContainsToken(text, "emissive") || ContainsToken(text, "emit")) {
		return MaterialSemantic::Emissive;
	}
	if (ContainsToken(text, "eye") || ContainsToken(text, "iris") || ContainsToken(text, "pupil")) {
		return MaterialSemantic::CharacterEye;
	}
	if (ContainsToken(text, "hair") || ContainsToken(text, "brow") || ContainsToken(text, "lash")) {
		return MaterialSemantic::CharacterHair;
	}
	if (ContainsToken(text, "skin") || ContainsToken(text, "face") || ContainsToken(text, "body") ||
		ContainsToken(text, "hand") || ContainsToken(text, "arm") || ContainsToken(text, "leg") ||
		ContainsToken(text, "head") || ContainsToken(text, "neck") || ContainsToken(text, "mouth") ||
		ContainsToken(text, "lip")) {
		return MaterialSemantic::CharacterSkin;
	}
	if (ContainsToken(text, "cloth") || ContainsToken(text, "clothes") || ContainsToken(text, "dress") ||
		ContainsToken(text, "shirt") || ContainsToken(text, "skirt") || ContainsToken(text, "sleeve") ||
		ContainsToken(text, "uniform") || ContainsToken(text, "wear") || ContainsToken(text, "shoe") ||
		ContainsToken(text, "sock") || ContainsToken(text, "ribbon")) {
		return MaterialSemantic::CharacterCloth;
	}
	if (ContainsToken(text, "glass") || ContainsToken(text, "transparent") || ContainsToken(text, "window") ||
		(material.hasBaseColorFactor && material.baseColorFactor.w < 0.75f)) {
		return MaterialSemantic::Glass;
	}
	if (ContainsToken(text, "metal") || ContainsToken(text, "steel") || ContainsToken(text, "iron") ||
		ContainsToken(text, "gold") || ContainsToken(text, "silver") || ContainsToken(text, "chrome") ||
		material.hasMetallicTexture || material.metallicFactor > 0.65f) {
		return MaterialSemantic::Metal;
	}
	return MaterialSemantic::GenericPbr;
}

std::string GetLowerExtension(const std::string& filename)
{
	return ToLowerAscii(std::filesystem::path(filename).extension().generic_string());
}

bool IsObjModel(const std::string& filename)
{
	return GetLowerExtension(filename) == ".obj";
}

Vector4 ConvertAssimpPosition(const aiVector3D& position)
{
	return { -position.x, position.y, position.z, 1.0f };
}

Vector3 ConvertAssimpDirection(const aiVector3D& direction)
{
	return { -direction.x, direction.y, direction.z };
}

std::string MakeAssimpTextureKey(
	const std::filesystem::path& sourcePath,
	const aiString& texturePath,
	TextureManager::TextureColorSpace colorSpace)
{
	std::string key = sourcePath.generic_string() + "#embedded/" + texturePath.C_Str();
	if (colorSpace == TextureManager::TextureColorSpace::LinearData) {
		key += "#linear";
	}
	return key;
}

std::string ResolveAssimpTexturePath(
	const aiScene& scene,
	const std::filesystem::path& sourcePath,
	const aiString& texturePath,
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
			if (TextureManager::GetInstance()->LoadTextureFromMemory(
				textureKey,
				encodedBegin,
				embeddedTexture->mWidth,
				colorSpace)) {
				return textureKey;
			}
		}
		return {};
	}

	std::filesystem::path resolvedPath(texturePath.C_Str());
	if (!resolvedPath.is_absolute()) {
		resolvedPath = sourcePath.parent_path() / resolvedPath;
	}
	return resolvedPath.lexically_normal().generic_string();
}

std::string GetAssimpMaterialTexture(
	const aiScene& scene,
	const aiMaterial& sourceMaterial,
	const std::filesystem::path& sourcePath,
	aiTextureType textureType,
	TextureManager::TextureColorSpace colorSpace)
{
	aiString texturePath;
	if (sourceMaterial.GetTexture(textureType, 0, &texturePath) != AI_SUCCESS) {
		return {};
	}
	return ResolveAssimpTexturePath(scene, sourcePath, texturePath, colorSpace);
}

void ReadAssimpBaseColor(const aiMaterial& sourceMaterial, MaterialData& materialData)
{
	aiColor4D color{};
	if (aiGetMaterialColor(&sourceMaterial, AI_MATKEY_BASE_COLOR, &color) == AI_SUCCESS ||
		aiGetMaterialColor(&sourceMaterial, AI_MATKEY_COLOR_DIFFUSE, &color) == AI_SUCCESS) {
		materialData.baseColorFactor = { color.r, color.g, color.b, color.a };
		materialData.hasBaseColorFactor = true;
	}
}

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

void ReadAssimpEmissive(const aiMaterial& sourceMaterial, MaterialData& materialData)
{
	aiColor4D emissive{};
	if (aiGetMaterialColor(&sourceMaterial, AI_MATKEY_COLOR_EMISSIVE, &emissive) != AI_SUCCESS) {
		return;
	}

	const Vector3 emissiveColor = { emissive.r, emissive.g, emissive.b };
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

std::vector<MaterialData> LoadAssimpMaterials(
	const aiScene& scene,
	const std::filesystem::path& sourcePath)
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

		std::string baseColorTexture = GetAssimpMaterialTexture(
			scene,
			sourceMaterial,
			sourcePath,
			aiTextureType_BASE_COLOR,
			TextureManager::TextureColorSpace::SRGB);
		if (baseColorTexture.empty()) {
			baseColorTexture = GetAssimpMaterialTexture(
				scene,
				sourceMaterial,
				sourcePath,
				aiTextureType_DIFFUSE,
				TextureManager::TextureColorSpace::SRGB);
		}
		if (!baseColorTexture.empty()) {
			materialData.textureFilePath = baseColorTexture;
		}

		std::string normalTexture = GetAssimpMaterialTexture(
			scene,
			sourceMaterial,
			sourcePath,
			aiTextureType_NORMALS,
			TextureManager::TextureColorSpace::LinearData);
		if (normalTexture.empty()) {
			normalTexture = GetAssimpMaterialTexture(
				scene,
				sourceMaterial,
				sourcePath,
				aiTextureType_NORMAL_CAMERA,
				TextureManager::TextureColorSpace::LinearData);
		}
		if (normalTexture.empty()) {
			normalTexture = GetAssimpMaterialTexture(
				scene,
				sourceMaterial,
				sourcePath,
				aiTextureType_HEIGHT,
				TextureManager::TextureColorSpace::LinearData);
		}
		if (!normalTexture.empty()) {
			materialData.normalTextureFilePath = normalTexture;
			materialData.hasNormalTexture = true;
		}

		const std::string metallicTexture = GetAssimpMaterialTexture(
			scene,
			sourceMaterial,
			sourcePath,
			aiTextureType_METALNESS,
			TextureManager::TextureColorSpace::LinearData);
		const std::string roughnessTexture = GetAssimpMaterialTexture(
			scene,
			sourceMaterial,
			sourcePath,
			aiTextureType_DIFFUSE_ROUGHNESS,
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

		const std::string occlusionTexture = GetAssimpMaterialTexture(
			scene,
			sourceMaterial,
			sourcePath,
			aiTextureType_AMBIENT_OCCLUSION,
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

void AppendAssimpNodeMeshes(
	const aiScene& scene,
	const aiNode& node,
	const aiMatrix4x4& parentTransform,
	ModelData& modelData)
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
			const aiVector3D sourceNormal = mesh.HasNormals() ? mesh.mNormals[vertexIndex] : aiVector3D{ 0.0f, 1.0f, 0.0f };
			aiVector3D transformedNormal = normalTransform * sourceNormal;
			transformedNormal.NormalizeSafe();
			const aiVector3D sourceUv = mesh.HasTextureCoords(0) ? mesh.mTextureCoords[0][vertexIndex] : aiVector3D{};

			VertexData vertex{};
			vertex.position = ConvertAssimpPosition(transformedPosition);
			vertex.texcoord = { sourceUv.x, 1.0f - sourceUv.y };
			vertex.normal = ConvertAssimpDirection(transformedNormal);
			if (mesh.HasTangentsAndBitangents()) {
				aiVector3D transformedTangent = normalTransform * mesh.mTangents[vertexIndex];
				transformedTangent.NormalizeSafe();
				const Vector3 tangent = ConvertAssimpDirection(transformedTangent);
				vertex.tangent = { tangent.x, tangent.y, tangent.z, 1.0f };
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
			uint32_t materialIndex = mesh.mMaterialIndex < modelData.materials.size()
				? mesh.mMaterialIndex
				: 0u;
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

float Clamp01(float value)
{
	return std::clamp(value, 0.0f, 1.0f);
}

float MaxComponent(const Vector3& value)
{
	return (std::max)((std::max)(value.x, value.y), value.z);
}

float RoughnessFromMtlSpecularPower(float specularPower)
{
	// Wavefront Ns is commonly authored in 0..1000. This maps the legacy
	// Blinn/Phong exponent into a microfacet roughness factor.
	specularPower = (std::max)(specularPower, 0.0f);
	return std::clamp(std::sqrt(2.0f / (specularPower + 2.0f)), 0.04f, 1.0f);
}

bool ReadVector3(std::istringstream& stream, Vector3& value)
{
	return static_cast<bool>(stream >> value.x >> value.y >> value.z);
}

MaterialData MakeDefaultPrimitiveMaterial()
{
	MaterialData material;
	material.materialName = "default";
	material.semantic = MaterialSemantic::GenericPbr;
	material.textureFilePath = "resources/white512x512.png";
	material.normalTextureFilePath = TextureManager::GetFlatNormalTexturePath();
	material.metallicRoughnessTextureFilePath = TextureManager::GetFlatNormalTexturePath();
	material.occlusionTextureFilePath = TextureManager::GetFlatNormalTexturePath();
	return material;
}

struct ParsedMaterialData {
	MaterialData material;
	bool hasExplicitRoughness = false;
	bool hasSpecularPower = false;
	bool hasLegacySpecular = false;
	float legacySpecularStrength = 0.0f;
};

void FinalizeParsedMaterial(ParsedMaterialData& parsedMaterial)
{
	MaterialData& materialData = parsedMaterial.material;
	if (parsedMaterial.hasLegacySpecular &&
		!parsedMaterial.hasSpecularPower &&
		!parsedMaterial.hasExplicitRoughness) {
		materialData.roughnessFactor = std::clamp(0.72f - parsedMaterial.legacySpecularStrength * 0.38f, 0.18f, 0.90f);
		materialData.hasPbrFactors = true;
	}

	if (materialData.textureFilePath.empty()) {
		materialData.textureFilePath = "resources/white512x512.png";
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

void ParseMtlMaterialLine(
	ParsedMaterialData& parsedMaterial,
	const std::string& directoryPath,
	const std::string& identifierLower,
	std::istringstream& stream)
{
	MaterialData& materialData = parsedMaterial.material;
	auto assignPackedMaterialTexture = [&](const std::string& texturePath, float occlusionChannel, float roughnessChannel, float metallicChannel, bool includesOcclusion) {
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
	} else if (identifierLower == "map_bump" || identifierLower == "map_normal" ||
		identifierLower == "map_norm" || identifierLower == "bump" || identifierLower == "norm") {
		std::string textureFilename = ReadTextureFilename(stream);
		materialData.normalTextureFilePath = ResolveTexturePath(directoryPath, textureFilename);
		materialData.hasNormalTexture = !materialData.normalTextureFilePath.empty();
	} else if (identifierLower == "map_orm" || identifierLower == "map_arm" ||
		identifierLower == "map_occlusionroughnessmetallic") {
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
	} else if (identifierLower == "map_pm" || identifierLower == "map_metallic" ||
		identifierLower == "map_metalness") {
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

std::vector<std::pair<std::string, MaterialData>> LoadMaterialTemplateLibrary(
	const std::string& directoryPath,
	const std::string& filename)
{
	std::vector<std::pair<std::string, MaterialData>> materials;
	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	std::string line;
	std::string currentMaterialName;
	ParsedMaterialData currentMaterial;
	bool hasCurrentMaterial = false;

	auto flushCurrentMaterial = [&]() {
		if (!hasCurrentMaterial) {
			return;
		}
		currentMaterial.material.materialName = currentMaterialName.empty()
			? "default"
			: currentMaterialName;
		FinalizeParsedMaterial(currentMaterial);
		materials.push_back({ currentMaterialName, currentMaterial.material });
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

Vector4 MakePosition(float x, float y, float z)
{
	return { x, y, z, 1.0f };
}

void AddDoubleSidedTriangle(std::vector<uint32_t>& indices, uint32_t index0, uint32_t index1, uint32_t index2)
{
	indices.push_back(index0);
	indices.push_back(index1);
	indices.push_back(index2);
	indices.push_back(index2);
	indices.push_back(index1);
	indices.push_back(index0);
}

void GenerateModelTangents(ModelData& modelData)
{
	std::vector<Vector3> tangentSums(modelData.vertices.size(), {});
	std::vector<Vector3> bitangentSums(modelData.vertices.size(), {});

	for (size_t i = 0; i + 2 < modelData.indices.size(); i += 3) {
		const uint32_t index0 = modelData.indices[i + 0];
		const uint32_t index1 = modelData.indices[i + 1];
		const uint32_t index2 = modelData.indices[i + 2];
		if (index0 >= modelData.vertices.size() ||
			index1 >= modelData.vertices.size() ||
			index2 >= modelData.vertices.size()) {
			continue;
		}

		const VertexData& v0 = modelData.vertices[index0];
		const VertexData& v1 = modelData.vertices[index1];
		const VertexData& v2 = modelData.vertices[index2];
		const Vector3 p0 = ToVector3(v0.position);
		const Vector3 p1 = ToVector3(v1.position);
		const Vector3 p2 = ToVector3(v2.position);
		const Vector3 edge1 = SubtractVector3(p1, p0);
		const Vector3 edge2 = SubtractVector3(p2, p0);
		const Vector2 deltaUv1 = { v1.texcoord.x - v0.texcoord.x, v1.texcoord.y - v0.texcoord.y };
		const Vector2 deltaUv2 = { v2.texcoord.x - v0.texcoord.x, v2.texcoord.y - v0.texcoord.y };
		const float det = deltaUv1.x * deltaUv2.y - deltaUv1.y * deltaUv2.x;

		Vector3 tangent = {};
		Vector3 bitangent = {};
		if (std::abs(det) > 0.000001f) {
			const float invDet = 1.0f / det;
			tangent = {
				(edge1.x * deltaUv2.y - edge2.x * deltaUv1.y) * invDet,
				(edge1.y * deltaUv2.y - edge2.y * deltaUv1.y) * invDet,
				(edge1.z * deltaUv2.y - edge2.z * deltaUv1.y) * invDet,
			};
			bitangent = {
				(edge2.x * deltaUv1.x - edge1.x * deltaUv2.x) * invDet,
				(edge2.y * deltaUv1.x - edge1.y * deltaUv2.x) * invDet,
				(edge2.z * deltaUv1.x - edge1.z * deltaUv2.x) * invDet,
			};
		} else {
			const Vector3 faceNormal = NormalizeVector3(CrossVector3(edge1, edge2));
			tangent = BuildFallbackTangent(faceNormal);
			bitangent = NormalizeVector3(CrossVector3(faceNormal, tangent));
		}

		const uint32_t indices[3] = { index0, index1, index2 };
		for (uint32_t index : indices) {
			tangentSums[index] = AddVector3(tangentSums[index], tangent);
			bitangentSums[index] = AddVector3(bitangentSums[index], bitangent);
		}
	}

	for (size_t i = 0; i < modelData.vertices.size(); ++i) {
		VertexData& vertex = modelData.vertices[i];
		const Vector3 normal = NormalizeVector3(vertex.normal);
		Vector3 tangent = tangentSums[i];
		tangent = SubtractVector3(tangent, MultiplyVector3(normal, DotVector3(normal, tangent)));
		if (LengthVector3(tangent) <= 0.00001f) {
			tangent = BuildFallbackTangent(normal);
		} else {
			tangent = NormalizeVector3(tangent);
		}

		const Vector3 bitangent = bitangentSums[i];
		const float handedness = DotVector3(CrossVector3(normal, tangent), bitangent) < 0.0f ? -1.0f : 1.0f;
		vertex.tangent = { tangent.x, tangent.y, tangent.z, handedness };
	}
}
}

void Model::Initialize(ModelCommon* modelCommon, const std::string& directorypath, const std::string& filename)
{
	if (IsObjModel(filename)) {
		InitializeFromModelData(modelCommon, LoadObjFile(directorypath, filename));
		return;
	}
	InitializeFromModelData(modelCommon, LoadAssimpFile(directorypath, filename));
}

void Model::InitializeFromModelData(ModelCommon* modelCommon, const ModelData& modelData)
{
	modelCommon_ = modelCommon;
	modelData_ = modelData;
	TextureManager::GetInstance()->CreateFlatNormalTexture();
	TextureManager::GetInstance()->CreateBrdfLutTexture();
	TextureManager::GetInstance()->CreatePbrIrradianceTexture();
	TextureManager::GetInstance()->CreatePbrPrefilteredEnvironmentTexture();

	if (modelData_.materials.empty()) {
		modelData_.materials.push_back(modelData_.material);
	}

	if (modelData_.submeshes.empty() && !modelData_.indices.empty()) {
		modelData_.submeshes.push_back({
			0,
			static_cast<uint32_t>(modelData_.indices.size()),
			0,
			"default",
		});
	}

	auto prepareMaterial = [](MaterialData& material) {
		if (material.textureFilePath.empty()) {
			material.textureFilePath = "resources/white512x512.png";
		}

		if (!material.metallicTextureFilePath.empty() ||
			!material.roughnessTextureFilePath.empty()) {
			const std::string packedMaterialPath = TextureManager::GetInstance()->CreatePackedPbrMaterialTexture(
				material.metallicTextureFilePath,
				material.metallicMapChannel,
				material.roughnessTextureFilePath,
				material.roughnessMapChannel,
				material.occlusionTextureFilePath,
				material.occlusionMapChannel,
				material.metallicFactor,
				material.roughnessFactor,
				material.ambientOcclusionFactor);
			if (!packedMaterialPath.empty()) {
				material.metallicRoughnessTextureFilePath = packedMaterialPath;
				material.hasMetallicRoughnessTexture =
					material.hasMetallicTexture || material.hasRoughnessTexture;
				material.metallicMapChannel = 2.0f;
				material.roughnessMapChannel = 1.0f;
				if (material.hasOcclusionTexture) {
					material.occlusionTextureFilePath = packedMaterialPath;
					material.occlusionMapChannel = 0.0f;
				}
			}
		}

		if (!material.hasNormalTexture || material.normalTextureFilePath.empty()) {
			material.normalTextureFilePath = TextureManager::GetFlatNormalTexturePath();
			material.hasNormalTexture = false;
		}
		if (!material.hasMetallicRoughnessTexture || material.metallicRoughnessTextureFilePath.empty()) {
			material.metallicRoughnessTextureFilePath = TextureManager::GetFlatNormalTexturePath();
			material.hasMetallicRoughnessTexture = false;
		}
		if (!material.hasOcclusionTexture || material.occlusionTextureFilePath.empty()) {
			material.occlusionTextureFilePath = TextureManager::GetFlatNormalTexturePath();
			material.hasOcclusionTexture = false;
		}
	};

	for (MaterialData& material : modelData_.materials) {
		prepareMaterial(material);
	}
	modelData_.material = modelData_.materials.front();

	GenerateModelTangents(modelData_);

	CreateGpuResources();

	for (MaterialData& material : modelData_.materials) {
		TextureManager::GetInstance()->LoadTexture(material.textureFilePath);
		if (material.hasNormalTexture) {
			TextureManager::GetInstance()->LoadTexture(
				material.normalTextureFilePath,
				TextureManager::TextureColorSpace::LinearData);
		}
		if (material.hasMetallicRoughnessTexture) {
			TextureManager::GetInstance()->LoadTexture(
				material.metallicRoughnessTextureFilePath,
				TextureManager::TextureColorSpace::LinearData);
		}
		if (material.hasOcclusionTexture) {
			TextureManager::GetInstance()->LoadTexture(
				material.occlusionTextureFilePath,
				TextureManager::TextureColorSpace::LinearData);
		}

		material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(material.textureFilePath);
		material.normalTextureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(
			material.normalTextureFilePath,
			TextureManager::TextureColorSpace::LinearData);
		material.metallicRoughnessTextureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(
			material.metallicRoughnessTextureFilePath,
			TextureManager::TextureColorSpace::LinearData);
		material.occlusionTextureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(
			material.occlusionTextureFilePath,
			TextureManager::TextureColorSpace::LinearData);
	}
	modelData_.material = modelData_.materials.front();
}

void Model::CreateGpuResources()
{
	// 用の頂点リソースを作る
	vertexResource = texture.CreateBufferResource(modelCommon_->GetDxCommon()->GetDevice(), sizeof(VertexData) * modelData_.vertices.size());

	// リソースの先端のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点3つ分のサイズ
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
	// 1頂点あたりのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);
	// 書き込むためのアドレスを取得
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	// 頂点データをリソースにコピー
	std::memcpy(vertexData, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());

	// IndexBufferを作成する。Skinningでは頂点とWeightを一対一で対応させるため、
	// 面ごとに頂点を複製せずIndexで共有する形を標準とする。
	indexResource = texture.CreateBufferResource(
		modelCommon_->GetDxCommon()->GetDevice(), sizeof(uint32_t) * modelData_.indices.size());
	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = UINT(sizeof(uint32_t) * modelData_.indices.size());
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
	std::memcpy(indexData, modelData_.indices.data(), sizeof(uint32_t) * modelData_.indices.size());
}

void Model::Draw()
{
	Draw({});
}

void Model::Draw(const std::vector<D3D12_GPU_VIRTUAL_ADDRESS>& materialCbvAddresses) {

	modelCommon_->GetDxCommon()->GetList()->IASetVertexBuffers(0, 1, &vertexBufferView); //VBVを設定
	modelCommon_->GetDxCommon()->GetList()->IASetIndexBuffer(&indexBufferView);

	auto drawSubmesh = [&](const ModelSubmesh& submesh) {
		const uint32_t materialIndex = submesh.materialIndex < modelData_.materials.size()
			? submesh.materialIndex
			: 0u;
		const MaterialData& material = modelData_.materials.empty()
			? modelData_.material
			: modelData_.materials[materialIndex];

		if (materialIndex < materialCbvAddresses.size() && materialCbvAddresses[materialIndex] != 0) {
			modelCommon_->GetDxCommon()->GetList()->SetGraphicsRootConstantBufferView(0, materialCbvAddresses[materialIndex]);
			modelCommon_->GetDxCommon()->GetList()->SetGraphicsRootConstantBufferView(10, materialCbvAddresses[materialIndex]);
		}

		modelCommon_->GetDxCommon()->GetList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(material.textureFilePath));
		modelCommon_->GetDxCommon()->GetList()->SetGraphicsRootDescriptorTable(
			11,
			TextureManager::GetInstance()->GetSrvHandleGPU(
				material.normalTextureFilePath,
				TextureManager::TextureColorSpace::LinearData));
		modelCommon_->GetDxCommon()->GetList()->SetGraphicsRootDescriptorTable(
			12,
			TextureManager::GetInstance()->GetSrvHandleGPU(
				material.metallicRoughnessTextureFilePath,
				TextureManager::TextureColorSpace::LinearData));
		modelCommon_->GetDxCommon()->GetList()->SetGraphicsRootDescriptorTable(
			13,
			TextureManager::GetInstance()->GetSrvHandleGPU(
				material.occlusionTextureFilePath,
				TextureManager::TextureColorSpace::LinearData));
		modelCommon_->GetDxCommon()->GetList()->SetGraphicsRootDescriptorTable(
			14,
			TextureManager::GetInstance()->GetSrvHandleGPU(
				TextureManager::GetBrdfLutTexturePath(),
				TextureManager::TextureColorSpace::LinearData));
		modelCommon_->GetDxCommon()->GetList()->SetGraphicsRootDescriptorTable(
			15,
			TextureManager::GetInstance()->GetSrvHandleGPU(
				TextureManager::GetPbrIrradianceTexturePath(),
				TextureManager::TextureColorSpace::LinearData));
		modelCommon_->GetDxCommon()->GetList()->SetGraphicsRootDescriptorTable(
			16,
			TextureManager::GetInstance()->GetSrvHandleGPU(
				TextureManager::GetPbrPrefilteredEnvironmentTexturePath(),
				TextureManager::TextureColorSpace::LinearData));
		modelCommon_->GetDxCommon()->GetList()->DrawIndexedInstanced(submesh.indexCount, 1, submesh.startIndex, 0, 0);
	};

	if (modelData_.submeshes.empty()) {
		drawSubmesh({ 0, static_cast<uint32_t>(modelData_.indices.size()), 0, "default" });
	} else {
		for (const ModelSubmesh& submesh : modelData_.submeshes) {
			drawSubmesh(submesh);
		}
	}

}

void Model::DrawOnlyMesh() {
	// 頂点バッファのセットと描画コマンドのみ
	modelCommon_->GetDxCommon()->GetList()->IASetVertexBuffers(0, 1, &vertexBufferView);
	modelCommon_->GetDxCommon()->GetList()->IASetIndexBuffer(&indexBufferView);
	if (modelData_.submeshes.empty()) {
		modelCommon_->GetDxCommon()->GetList()->DrawIndexedInstanced(UINT(modelData_.indices.size()), 1, 0, 0, 0);
	} else {
		for (const ModelSubmesh& submesh : modelData_.submeshes) {
			modelCommon_->GetDxCommon()->GetList()->DrawIndexedInstanced(submesh.indexCount, 1, submesh.startIndex, 0, 0);
		}
	}
}

void Model::RecalculateSmoothNormals()
{
	if (modelData_.vertices.empty() || modelData_.indices.size() < 3) {
		return;
	}

	std::unordered_map<SmoothNormalKey, Vector3, SmoothNormalKeyHash> normalSums;

	for (size_t i = 0; i + 2 < modelData_.indices.size(); i += 3) {
		const uint32_t index0 = modelData_.indices[i + 0];
		const uint32_t index1 = modelData_.indices[i + 1];
		const uint32_t index2 = modelData_.indices[i + 2];
		if (index0 >= modelData_.vertices.size() ||
			index1 >= modelData_.vertices.size() ||
			index2 >= modelData_.vertices.size()) {
			continue;
		}

		const Vector3 p0 = ToVector3(modelData_.vertices[index0].position);
		const Vector3 p1 = ToVector3(modelData_.vertices[index1].position);
		const Vector3 p2 = ToVector3(modelData_.vertices[index2].position);
		Vector3 faceNormal = CrossVector3(SubtractVector3(p1, p0), SubtractVector3(p2, p0));
		const float faceLength = LengthVector3(faceNormal);
		if (faceLength <= 0.00001f) {
			continue;
		}
		faceNormal = { faceNormal.x / faceLength, faceNormal.y / faceLength, faceNormal.z / faceLength };

		const uint32_t indices[3] = { index0, index1, index2 };
		for (uint32_t index : indices) {
			const SmoothNormalKey key = MakeSmoothNormalKey(modelData_.vertices[index].position);
			Vector3& sum = normalSums[key];
			sum.x += faceNormal.x;
			sum.y += faceNormal.y;
			sum.z += faceNormal.z;
		}
	}

	for (VertexData& vertex : modelData_.vertices) {
		const SmoothNormalKey key = MakeSmoothNormalKey(vertex.position);
		auto found = normalSums.find(key);
		if (found == normalSums.end()) {
			continue;
		}

		Vector3 normal = NormalizeVector3(found->second);
		if (DotVector3(normal, vertex.normal) < 0.0f) {
			normal = { -normal.x, -normal.y, -normal.z };
		}
		vertex.normal = normal;
	}

	GenerateModelTangents(modelData_);

	if (vertexData) {
		std::memcpy(vertexData, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
	}
}

MaterialData Model::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{

	MaterialData materialData; // 構築するMaterialData
	std::string line; // ファイルから読んだ一行を格納するもの
	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // 開けなかったらエラー
	bool hasExplicitRoughness = false;
	bool hasSpecularPower = false;
	bool hasLegacySpecular = false;
	float legacySpecularStrength = 0.0f;
	auto assignPackedMaterialTexture = [&](const std::string& texturePath, float occlusionChannel, float roughnessChannel, float metallicChannel, bool includesOcclusion) {
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
		} else if (identifierLower == "map_bump" || identifierLower == "map_normal" ||
			identifierLower == "map_norm" || identifierLower == "bump" || identifierLower == "norm") {
			std::string textureFilename = ReadTextureFilename(s);
			materialData.normalTextureFilePath = ResolveTexturePath(directoryPath, textureFilename);
			materialData.hasNormalTexture = !materialData.normalTextureFilePath.empty();
		} else if (identifierLower == "map_orm" || identifierLower == "map_arm" ||
			identifierLower == "map_occlusionroughnessmetallic") {
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
		} else if (identifierLower == "map_pm" || identifierLower == "map_metallic" ||
			identifierLower == "map_metalness") {
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
		materialData.textureFilePath = "resources/white512x512.png";
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
	std::vector<Vector3> normals; // 法線
	std::vector<Vector2> texcoords; // テクスチャ座標
	std::string line; // ファイルから読んだ一行を格納するもの
	std::string currentMaterialName = "default";
	uint32_t currentMaterialIndex = 0;
	std::unordered_map<std::string, uint32_t> materialNameToIndex;
	std::unordered_map<std::string, uint32_t> vertexDefinitionToIndex;

	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // 開けなかったらエラー

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
			if (lastSubmesh.materialIndex == materialIndex &&
				lastSubmesh.startIndex + lastSubmesh.indexCount == startIndex) {
				lastSubmesh.indexCount += indexCount;
				return;
			}
		}
		modelData.submeshes.push_back({ startIndex, indexCount, materialIndex, materialName });
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
			//texcoord.x = 1.0f - texcoord.x;
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
				uint32_t elementIndices[3] = { 0, 0, 0 };
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
				modelData.vertices.push_back({ position, texcoord, normal });
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
					const std::string resolvedName = materialName.empty()
						? "material" + std::to_string(materialIndex)
						: materialName;
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
	const std::filesystem::path sourcePath = std::filesystem::path(directoryPath) / filename;

	Assimp::Importer importer;
	constexpr unsigned int kImportFlags =
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_GenSmoothNormals |
		aiProcess_CalcTangentSpace |
		aiProcess_ImproveCacheLocality |
		aiProcess_SortByPType;

	const aiScene* scene = importer.ReadFile(sourcePath.generic_string(), kImportFlags);
	if (scene == nullptr || scene->mRootNode == nullptr || scene->mNumMeshes == 0) {
		throw std::runtime_error("Failed to load static model: " + sourcePath.generic_string() + " / " + importer.GetErrorString());
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

ModelData Model::CreatePlane(float width, float depth)
{
	width = (std::max)(width, 0.001f);
	depth = (std::max)(depth, 0.001f);

	const float halfWidth = width * 0.5f;
	const float halfDepth = depth * 0.5f;
	const Vector3 normal = { 0.0f, 1.0f, 0.0f };

	ModelData modelData;
	modelData.material = MakeDefaultPrimitiveMaterial();
	modelData.vertices = {
		{ MakePosition(-halfWidth, 0.0f, -halfDepth), { 0.0f, 1.0f }, normal },
		{ MakePosition( halfWidth, 0.0f, -halfDepth), { 1.0f, 1.0f }, normal },
		{ MakePosition(-halfWidth, 0.0f,  halfDepth), { 0.0f, 0.0f }, normal },
		{ MakePosition( halfWidth, 0.0f,  halfDepth), { 1.0f, 0.0f }, normal },
	};

	AddDoubleSidedTriangle(modelData.indices, 0, 1, 2);
	AddDoubleSidedTriangle(modelData.indices, 2, 1, 3);
	return modelData;
}

ModelData Model::CreateGrid(float width, float depth, uint32_t xSegments, uint32_t zSegments)
{
	width = (std::max)(width, 0.001f);
	depth = (std::max)(depth, 0.001f);
	xSegments = (std::max)(xSegments, 1u);
	zSegments = (std::max)(zSegments, 1u);

	const float halfWidth = width * 0.5f;
	const float halfDepth = depth * 0.5f;
	const Vector3 normal = { 0.0f, 1.0f, 0.0f };

	ModelData modelData;
	modelData.material = MakeDefaultPrimitiveMaterial();
	modelData.vertices.reserve(static_cast<size_t>(xSegments + 1u) * static_cast<size_t>(zSegments + 1u));
	modelData.indices.reserve(static_cast<size_t>(xSegments) * static_cast<size_t>(zSegments) * 6u);

	for (uint32_t z = 0; z <= zSegments; ++z) {
		const float v = static_cast<float>(z) / static_cast<float>(zSegments);
		const float posZ = -halfDepth + depth * v;
		for (uint32_t x = 0; x <= xSegments; ++x) {
			const float u = static_cast<float>(x) / static_cast<float>(xSegments);
			const float posX = -halfWidth + width * u;
			modelData.vertices.push_back({
				MakePosition(posX, 0.0f, posZ),
				{ u, 1.0f - v },
				normal,
				{ 1.0f, 0.0f, 0.0f, 1.0f },
			});
		}
	}

	const uint32_t stride = xSegments + 1u;
	for (uint32_t z = 0; z < zSegments; ++z) {
		for (uint32_t x = 0; x < xSegments; ++x) {
			const uint32_t i0 = z * stride + x;
			const uint32_t i1 = i0 + 1u;
			const uint32_t i2 = i0 + stride;
			const uint32_t i3 = i2 + 1u;
			modelData.indices.push_back(i0);
			modelData.indices.push_back(i2);
			modelData.indices.push_back(i1);
			modelData.indices.push_back(i2);
			modelData.indices.push_back(i3);
			modelData.indices.push_back(i1);
		}
	}

	return modelData;
}

ModelData Model::CreateBox(const Vector3& size)
{
	const float halfX = (std::max)(std::abs(size.x), 0.001f) * 0.5f;
	const float halfY = (std::max)(std::abs(size.y), 0.001f) * 0.5f;
	const float halfZ = (std::max)(std::abs(size.z), 0.001f) * 0.5f;

	ModelData modelData;
	modelData.material = MakeDefaultPrimitiveMaterial();
	modelData.vertices.reserve(24);
	modelData.indices.reserve(72);

	auto addQuad = [&](const Vector4& p0, const Vector4& p1, const Vector4& p2, const Vector4& p3, const Vector3& normal) {
		const uint32_t base = static_cast<uint32_t>(modelData.vertices.size());
		modelData.vertices.push_back({ p0, { 0.0f, 1.0f }, normal });
		modelData.vertices.push_back({ p1, { 1.0f, 1.0f }, normal });
		modelData.vertices.push_back({ p2, { 0.0f, 0.0f }, normal });
		modelData.vertices.push_back({ p3, { 1.0f, 0.0f }, normal });
		AddDoubleSidedTriangle(modelData.indices, base + 0, base + 1, base + 2);
		AddDoubleSidedTriangle(modelData.indices, base + 2, base + 1, base + 3);
	};

	addQuad(
		MakePosition(-halfX, -halfY,  halfZ), MakePosition( halfX, -halfY,  halfZ),
		MakePosition(-halfX,  halfY,  halfZ), MakePosition( halfX,  halfY,  halfZ),
		{ 0.0f, 0.0f, 1.0f });
	addQuad(
		MakePosition( halfX, -halfY, -halfZ), MakePosition(-halfX, -halfY, -halfZ),
		MakePosition( halfX,  halfY, -halfZ), MakePosition(-halfX,  halfY, -halfZ),
		{ 0.0f, 0.0f, -1.0f });
	addQuad(
		MakePosition( halfX, -halfY,  halfZ), MakePosition( halfX, -halfY, -halfZ),
		MakePosition( halfX,  halfY,  halfZ), MakePosition( halfX,  halfY, -halfZ),
		{ 1.0f, 0.0f, 0.0f });
	addQuad(
		MakePosition(-halfX, -halfY, -halfZ), MakePosition(-halfX, -halfY,  halfZ),
		MakePosition(-halfX,  halfY, -halfZ), MakePosition(-halfX,  halfY,  halfZ),
		{ -1.0f, 0.0f, 0.0f });
	addQuad(
		MakePosition(-halfX,  halfY,  halfZ), MakePosition( halfX,  halfY,  halfZ),
		MakePosition(-halfX,  halfY, -halfZ), MakePosition( halfX,  halfY, -halfZ),
		{ 0.0f, 1.0f, 0.0f });
	addQuad(
		MakePosition(-halfX, -halfY, -halfZ), MakePosition( halfX, -halfY, -halfZ),
		MakePosition(-halfX, -halfY,  halfZ), MakePosition( halfX, -halfY,  halfZ),
		{ 0.0f, -1.0f, 0.0f });

	return modelData;
}

ModelData Model::CreateFacetedCrystal(float radius, float height, uint32_t sides)
{
	constexpr float kPi = 3.14159265358979323846f;
	radius = (std::max)(std::abs(radius), 0.001f);
	height = (std::max)(std::abs(height), 0.001f);
	sides = (std::clamp)(sides, 5u, 16u);

	const float halfHeight = height * 0.5f;
	const float upperY = height * 0.18f;
	const float lowerY = -height * 0.18f;
	const float upperRadius = radius * 0.76f;
	const float lowerRadius = radius;
	const float lowerPhaseOffset = kPi / static_cast<float>(sides);

	std::vector<Vector3> upperRing(sides);
	std::vector<Vector3> lowerRing(sides);
	for (uint32_t side = 0; side < sides; ++side) {
		const float phase =
			static_cast<float>(side) * 2.0f * kPi / static_cast<float>(sides);
		upperRing[side] = {
			std::cos(phase) * upperRadius,
			upperY,
			std::sin(phase) * upperRadius,
		};
		lowerRing[side] = {
			std::cos(phase + lowerPhaseOffset) * lowerRadius,
			lowerY,
			std::sin(phase + lowerPhaseOffset) * lowerRadius,
		};
	}

	ModelData modelData;
	modelData.material = MakeDefaultPrimitiveMaterial();
	modelData.vertices.reserve(static_cast<size_t>(sides) * 12u);
	modelData.indices.reserve(static_cast<size_t>(sides) * 24u);

	auto addFacet = [&](const Vector3& p0, const Vector3& point1, const Vector3& point2) {
		Vector3 p1 = point1;
		Vector3 p2 = point2;
		Vector3 normal = NormalizeVector3(CrossVector3(
			SubtractVector3(p1, p0),
			SubtractVector3(p2, p0)));
		const Vector3 center = {
			(p0.x + p1.x + p2.x) / 3.0f,
			(p0.y + p1.y + p2.y) / 3.0f,
			(p0.z + p1.z + p2.z) / 3.0f,
		};
		if (DotVector3(normal, center) < 0.0f) {
			std::swap(p1, p2);
			normal = MultiplyVector3(normal, -1.0f);
		}

		const uint32_t base = static_cast<uint32_t>(modelData.vertices.size());
		modelData.vertices.push_back({ MakePosition(p0.x, p0.y, p0.z), { 0.5f, 0.0f }, normal });
		modelData.vertices.push_back({ MakePosition(p1.x, p1.y, p1.z), { 0.0f, 1.0f }, normal });
		modelData.vertices.push_back({ MakePosition(p2.x, p2.y, p2.z), { 1.0f, 1.0f }, normal });
		AddDoubleSidedTriangle(modelData.indices, base, base + 1u, base + 2u);
	};

	const Vector3 top = { 0.0f, halfHeight, 0.0f };
	const Vector3 bottom = { 0.0f, -halfHeight, 0.0f };
	for (uint32_t side = 0; side < sides; ++side) {
		const uint32_t next = (side + 1u) % sides;
		addFacet(top, upperRing[side], upperRing[next]);
		addFacet(upperRing[side], lowerRing[side], lowerRing[next]);
		addFacet(upperRing[side], lowerRing[next], upperRing[next]);
		addFacet(bottom, lowerRing[next], lowerRing[side]);
	}

	return modelData;
}

ModelData Model::CreateCylinder(float radius, float height, uint32_t segments)
{
	constexpr float kPi = 3.14159265358979323846f;
	radius = (std::max)(radius, 0.001f);
	height = (std::max)(height, 0.001f);
	segments = (std::max)(segments, 3u);

	const float halfHeight = height * 0.5f;
	ModelData modelData;
	modelData.material = MakeDefaultPrimitiveMaterial();
	modelData.vertices.reserve(static_cast<size_t>(segments + 1u) * 4u + 2u);
	modelData.indices.reserve(static_cast<size_t>(segments) * 24u);

	const uint32_t sideBase = static_cast<uint32_t>(modelData.vertices.size());
	for (uint32_t i = 0; i <= segments; ++i) {
		const float u = static_cast<float>(i) / static_cast<float>(segments);
		const float phi = u * kPi * 2.0f;
		const float x = std::cos(phi) * radius;
		const float z = std::sin(phi) * radius;
		const Vector3 normal = NormalizeVector3({ x, 0.0f, z });
		modelData.vertices.push_back({ MakePosition(x, -halfHeight, z), { u, 1.0f }, normal });
		modelData.vertices.push_back({ MakePosition(x,  halfHeight, z), { u, 0.0f }, normal });
	}

	for (uint32_t i = 0; i < segments; ++i) {
		const uint32_t index0 = sideBase + i * 2u;
		const uint32_t index1 = index0 + 1u;
		const uint32_t index2 = index0 + 2u;
		const uint32_t index3 = index0 + 3u;
		AddDoubleSidedTriangle(modelData.indices, index0, index2, index1);
		AddDoubleSidedTriangle(modelData.indices, index1, index2, index3);
	}

	const uint32_t topCenter = static_cast<uint32_t>(modelData.vertices.size());
	modelData.vertices.push_back({ MakePosition(0.0f, halfHeight, 0.0f), { 0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f } });
	const uint32_t topRingBase = static_cast<uint32_t>(modelData.vertices.size());
	for (uint32_t i = 0; i <= segments; ++i) {
		const float u = static_cast<float>(i) / static_cast<float>(segments);
		const float phi = u * kPi * 2.0f;
		const float x = std::cos(phi) * radius;
		const float z = std::sin(phi) * radius;
		modelData.vertices.push_back({
			MakePosition(x, halfHeight, z),
			{ x / (radius * 2.0f) + 0.5f, 0.5f - z / (radius * 2.0f) },
			{ 0.0f, 1.0f, 0.0f },
		});
	}

	const uint32_t bottomCenter = static_cast<uint32_t>(modelData.vertices.size());
	modelData.vertices.push_back({ MakePosition(0.0f, -halfHeight, 0.0f), { 0.5f, 0.5f }, { 0.0f, -1.0f, 0.0f } });
	const uint32_t bottomRingBase = static_cast<uint32_t>(modelData.vertices.size());
	for (uint32_t i = 0; i <= segments; ++i) {
		const float u = static_cast<float>(i) / static_cast<float>(segments);
		const float phi = u * kPi * 2.0f;
		const float x = std::cos(phi) * radius;
		const float z = std::sin(phi) * radius;
		modelData.vertices.push_back({
			MakePosition(x, -halfHeight, z),
			{ x / (radius * 2.0f) + 0.5f, z / (radius * 2.0f) + 0.5f },
			{ 0.0f, -1.0f, 0.0f },
		});
	}

	for (uint32_t i = 0; i < segments; ++i) {
		AddDoubleSidedTriangle(modelData.indices, topCenter, topRingBase + i, topRingBase + i + 1u);
		AddDoubleSidedTriangle(modelData.indices, bottomCenter, bottomRingBase + i + 1u, bottomRingBase + i);
	}

	return modelData;
}

ModelData Model::CreateUvSphere(float radius, uint32_t latitudeSegments, uint32_t longitudeSegments)
{
	constexpr float kPi = 3.14159265358979323846f;
	latitudeSegments = (std::max)(latitudeSegments, 3u);
	longitudeSegments = (std::max)(longitudeSegments, 3u);
	radius = (std::max)(radius, 0.001f);

	ModelData modelData;
	modelData.material = MakeDefaultPrimitiveMaterial();
	modelData.vertices.reserve(static_cast<size_t>(latitudeSegments + 1u) * static_cast<size_t>(longitudeSegments + 1u));
	modelData.indices.reserve(static_cast<size_t>(latitudeSegments) * static_cast<size_t>(longitudeSegments) * 12u);

	for (uint32_t latitude = 0; latitude <= latitudeSegments; ++latitude) {
		const float v = static_cast<float>(latitude) / static_cast<float>(latitudeSegments);
		const float theta = v * kPi;
		const float sinTheta = std::sin(theta);
		const float cosTheta = std::cos(theta);

		for (uint32_t longitude = 0; longitude <= longitudeSegments; ++longitude) {
			const float u = static_cast<float>(longitude) / static_cast<float>(longitudeSegments);
			const float phi = u * kPi * 2.0f;
			Vector3 normal = {
				sinTheta * std::cos(phi),
				cosTheta,
				sinTheta * std::sin(phi),
			};
			normal = NormalizeVector3(normal);

			modelData.vertices.push_back({
				{ normal.x * radius, normal.y * radius, normal.z * radius, 1.0f },
				{ u, v },
				normal,
				{ std::sin(phi), 0.0f, -std::cos(phi), 1.0f },
			});
		}
	}

	auto addDoubleSidedTriangle = [&](uint32_t index0, uint32_t index1, uint32_t index2) {
		modelData.indices.push_back(index0);
		modelData.indices.push_back(index1);
		modelData.indices.push_back(index2);
		modelData.indices.push_back(index2);
		modelData.indices.push_back(index1);
		modelData.indices.push_back(index0);
	};

	const uint32_t rowStride = longitudeSegments + 1u;
	for (uint32_t latitude = 0; latitude < latitudeSegments; ++latitude) {
		for (uint32_t longitude = 0; longitude < longitudeSegments; ++longitude) {
			const uint32_t index0 = latitude * rowStride + longitude;
			const uint32_t index1 = (latitude + 1u) * rowStride + longitude;
			const uint32_t index2 = index0 + 1u;
			const uint32_t index3 = index1 + 1u;

			if (latitude > 0u) {
				addDoubleSidedTriangle(index0, index1, index2);
			}
			if (latitude + 1u < latitudeSegments) {
				addDoubleSidedTriangle(index2, index1, index3);
			}
		}
	}

	return modelData;
}

} // namespace cg2
