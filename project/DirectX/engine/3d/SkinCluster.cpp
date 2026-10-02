#include "SkinCluster.h"
#include "StringUtils.h"

#include "Calculation.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace cg2 {

namespace {

/// @brief 姿勢を変換する。
QuaternionTransform ConvertTransform(const aiMatrix4x4& matrix) {
	aiVector3D scale;
	aiVector3D translate;
	aiQuaternion rotate;
	matrix.Decompose(scale, rotate, translate);
	return {
		{ scale.x, scale.y, scale.z },
		NormalizeQuaternion({ rotate.x, -rotate.y, -rotate.z, rotate.w }),
		{ -translate.x, translate.y, translate.z },
	};
}

/// @brief ノードを読み取る。
SkeletonNode ReadNode(const aiNode& source) {
	SkeletonNode node;
	node.name = source.mName.C_Str();
	node.transform = ConvertTransform(source.mTransformation);
	node.children.reserve(source.mNumChildren);
	for (uint32_t index = 0; index < source.mNumChildren; ++index) {
		node.children.push_back(ReadNode(*source.mChildren[index]));
	}
	return node;
}

/// @brief 逆行列結合姿勢を変換する。
Matrix4x4 ConvertInverseBindPose(const aiMatrix4x4& sourceOffsetMatrix) {
	aiMatrix4x4 bindPose = sourceOffsetMatrix;
	bindPose.Inverse();
	const QuaternionTransform transform = ConvertTransform(bindPose);
	return Inverse(MakeAffineMatrix(transform.scale, transform.rotate, transform.translate));
}

/// @brief 入力を0〜1の範囲に制限して返す。
float Clamp01(float value) {
	return std::clamp(value, 0.0f, 1.0f);
}

/// @brief ベクトルの最大成分を返す。
float MaxComponent(const Vector3& value) {
	return (std::max)((std::max)(value.x, value.y), value.z);
}

/// @brief Assimp方向を変換する。
Vector3 ConvertAssimpDirection(const aiVector3D& direction) {
	return { -direction.x, direction.y, direction.z };
}

/// @brief 入力の3成分をエンジンのVector3へ変換する。
Vector3 ToVector3(const Vector4& value) {
	return { value.x, value.y, value.z };
}

/// @brief 2つの3Dベクトルの差を返す。
Vector3 SubtractVector3(const Vector3& a, const Vector3& b) {
	return { a.x - b.x, a.y - b.y, a.z - b.z };
}

/// @brief 2つの3Dベクトルの外積を返す。
Vector3 CrossVector3(const Vector3& a, const Vector3& b) {
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x,
	};
}

/// @brief 2つの3Dベクトルの内積を返す。
float DotVector3(const Vector3& a, const Vector3& b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

/// @brief 3Dベクトルの長さを返す。
float LengthVector3(const Vector3& value) {
	return std::sqrt(DotVector3(value, value));
}

/// @brief ベクトル3局所を正規化する。
Vector3 NormalizeVector3Local(const Vector3& value) {
	const float length = LengthVector3(value);
	if (length <= 0.00001f) {
		return { 0.0f, 1.0f, 0.0f };
	}
	return { value.x / length, value.y / length, value.z / length };
}

/// @brief 3Dベクトルへ指定倍率を掛けて返す。
Vector3 MultiplyVector3(const Vector3& value, float scalar) {
	return { value.x * scalar, value.y * scalar, value.z * scalar };
}

/// @brief ベクトル3を追加する。
Vector3 AddVector3(const Vector3& a, const Vector3& b) {
	return { a.x + b.x, a.y + b.y, a.z + b.z };
}

/// @brief Fallback接線を組み立てる。
Vector3 BuildFallbackTangent(const Vector3& normal) {
	const Vector3 up = std::abs(normal.y) < 0.95f ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
	return NormalizeVector3Local(CrossVector3(up, normal));
}

/// @brief マテリアルの名前などから描画用途を推定する。
MaterialSemantic InferMaterialSemantic(const MaterialData& material) {
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

/// @brief 既定値骨格変形材質を作成して返す。
MaterialData MakeDefaultSkinnedMaterial() {
	MaterialData material;
	material.materialName = "default";
	material.semantic = MaterialSemantic::GenericPbr;
	material.textureFilePath = "resources/white512x512.png";
	material.normalTextureFilePath = TextureManager::GetFlatNormalTexturePath();
	material.metallicRoughnessTextureFilePath = TextureManager::GetFlatNormalTexturePath();
	material.occlusionTextureFilePath = TextureManager::GetFlatNormalTexturePath();
	return material;
}

/// @brief Embeddedテクスチャキーを作成して返す。
std::string MakeEmbeddedTextureKey(
	const std::filesystem::path& sourcePath,
	const aiString& texturePath,
	TextureManager::TextureColorSpace colorSpace) {
	std::string key = sourcePath.generic_string() + "#embedded/" + texturePath.C_Str();
	if (colorSpace == TextureManager::TextureColorSpace::LinearData) {
		key += "#linear";
	}
	return key;
}

/// @brief スキニングテクスチャパスを解決する。
std::string ResolveSkinningTexturePath(
	SkinningModelAsset& asset,
	const aiScene& scene,
	const std::filesystem::path& sourcePath,
	const aiString& texturePath,
	TextureManager::TextureColorSpace colorSpace) {
	if (texturePath.length == 0) {
		return {};
	}

	const aiTexture* embedded = scene.GetEmbeddedTexture(texturePath.C_Str());
	if (embedded != nullptr) {
		if (embedded->mHeight == 0 && embedded->mWidth > 0) {
			const std::string textureKey = MakeEmbeddedTextureKey(sourcePath, texturePath, colorSpace);
			const auto alreadyCopied = std::find_if(
				asset.embeddedTextures.begin(), asset.embeddedTextures.end(),
				[&](const SkinningModelAsset::EmbeddedTexture& texture) {
					return texture.textureKey == textureKey;
				});
			if (alreadyCopied == asset.embeddedTextures.end()) {
				SkinningModelAsset::EmbeddedTexture texture;
				texture.textureKey = textureKey;
				texture.linearData = colorSpace == TextureManager::TextureColorSpace::LinearData;
				const auto* begin = reinterpret_cast<const uint8_t*>(embedded->pcData);
				texture.encodedData.assign(begin, begin + embedded->mWidth);
				asset.embeddedTextures.push_back(std::move(texture));
			}
			return textureKey;
		}
		return {};
	}

	std::filesystem::path resolvedPath(texturePath.C_Str());
	if (!resolvedPath.is_absolute()) {
		resolvedPath = sourcePath.parent_path() / resolvedPath;
	}
	return resolvedPath.lexically_normal().generic_string();
}

/// @brief スキニング材質テクスチャを返す。
std::string GetSkinningMaterialTexture(
	SkinningModelAsset& asset,
	const aiScene& scene,
	const aiMaterial& material,
	const std::filesystem::path& sourcePath,
	aiTextureType textureType,
	TextureManager::TextureColorSpace colorSpace) {
	aiString texturePath;
	if (material.GetTexture(textureType, 0, &texturePath) != AI_SUCCESS) {
		return {};
	}
	return ResolveSkinningTexturePath(asset, scene, sourcePath, texturePath, colorSpace);
}

/// @brief スキニング基準色を読み取る。
void ReadSkinningBaseColor(const aiMaterial& sourceMaterial, MaterialData& materialData) {
	aiColor4D color{};
	if (aiGetMaterialColor(&sourceMaterial, AI_MATKEY_BASE_COLOR, &color) == AI_SUCCESS ||
		aiGetMaterialColor(&sourceMaterial, AI_MATKEY_COLOR_DIFFUSE, &color) == AI_SUCCESS) {
		materialData.baseColorFactor = { color.r, color.g, color.b, color.a };
		materialData.hasBaseColorFactor = true;
	}
}

/// @brief スキニングPBRFactorsを読み取る。
void ReadSkinningPbrFactors(const aiMaterial& sourceMaterial, MaterialData& materialData) {
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

/// @brief スキニング自己発光を読み取る。
void ReadSkinningEmissive(const aiMaterial& sourceMaterial, MaterialData& materialData) {
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

/// @brief スキニングMaterialsを読み込む。
std::vector<MaterialData> LoadSkinningMaterials(
	SkinningModelAsset& asset,
	const aiScene& scene,
	const std::filesystem::path& sourcePath) {
	std::vector<MaterialData> materials;
	materials.reserve((std::max)(scene.mNumMaterials, 1u));

	for (uint32_t materialIndex = 0; materialIndex < scene.mNumMaterials; ++materialIndex) {
		const aiMaterial& sourceMaterial = *scene.mMaterials[materialIndex];
		MaterialData materialData = MakeDefaultSkinnedMaterial();
		aiString materialName;
		if (sourceMaterial.Get(AI_MATKEY_NAME, materialName) == AI_SUCCESS && materialName.length > 0) {
			materialData.materialName = materialName.C_Str();
		} else {
			materialData.materialName = "material" + std::to_string(materialIndex);
		}

		ReadSkinningBaseColor(sourceMaterial, materialData);
		ReadSkinningPbrFactors(sourceMaterial, materialData);
		ReadSkinningEmissive(sourceMaterial, materialData);

		std::string baseColorTexture = GetSkinningMaterialTexture(
			asset, scene, sourceMaterial, sourcePath,
			aiTextureType_BASE_COLOR, TextureManager::TextureColorSpace::SRGB);
		if (baseColorTexture.empty()) {
			baseColorTexture = GetSkinningMaterialTexture(
				asset, scene, sourceMaterial, sourcePath,
				aiTextureType_DIFFUSE, TextureManager::TextureColorSpace::SRGB);
		}
		if (!baseColorTexture.empty()) {
			materialData.textureFilePath = baseColorTexture;
		}

		std::string normalTexture = GetSkinningMaterialTexture(
			asset, scene, sourceMaterial, sourcePath,
			aiTextureType_NORMALS, TextureManager::TextureColorSpace::LinearData);
		if (normalTexture.empty()) {
			normalTexture = GetSkinningMaterialTexture(
				asset, scene, sourceMaterial, sourcePath,
				aiTextureType_NORMAL_CAMERA, TextureManager::TextureColorSpace::LinearData);
		}
		if (normalTexture.empty()) {
			normalTexture = GetSkinningMaterialTexture(
				asset, scene, sourceMaterial, sourcePath,
				aiTextureType_HEIGHT, TextureManager::TextureColorSpace::LinearData);
		}
		if (!normalTexture.empty()) {
			materialData.normalTextureFilePath = normalTexture;
			materialData.hasNormalTexture = true;
		}

		const std::string metallicTexture = GetSkinningMaterialTexture(
			asset, scene, sourceMaterial, sourcePath,
			aiTextureType_METALNESS, TextureManager::TextureColorSpace::LinearData);
		const std::string roughnessTexture = GetSkinningMaterialTexture(
			asset, scene, sourceMaterial, sourcePath,
			aiTextureType_DIFFUSE_ROUGHNESS, TextureManager::TextureColorSpace::LinearData);
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

		const std::string occlusionTexture = GetSkinningMaterialTexture(
			asset, scene, sourceMaterial, sourcePath,
			aiTextureType_AMBIENT_OCCLUSION, TextureManager::TextureColorSpace::LinearData);
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
		materials.push_back(MakeDefaultSkinnedMaterial());
	}
	return materials;
}

/// @brief スキニングモデルの頂点位置とUVから接線を生成する。
void GenerateSkinningTangents(ModelData& modelData) {
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
			const Vector3 faceNormal = NormalizeVector3Local(CrossVector3(edge1, edge2));
			tangent = BuildFallbackTangent(faceNormal);
			bitangent = NormalizeVector3Local(CrossVector3(faceNormal, tangent));
		}

		const uint32_t indices[3] = { index0, index1, index2 };
		for (uint32_t index : indices) {
			tangentSums[index] = AddVector3(tangentSums[index], tangent);
			bitangentSums[index] = AddVector3(bitangentSums[index], bitangent);
		}
	}

	for (size_t i = 0; i < modelData.vertices.size(); ++i) {
		VertexData& vertex = modelData.vertices[i];
		const Vector3 normal = NormalizeVector3Local(vertex.normal);
		Vector3 tangent = tangentSums[i];
		tangent = SubtractVector3(tangent, MultiplyVector3(normal, DotVector3(normal, tangent)));
		if (LengthVector3(tangent) <= 0.00001f) {
			tangent = BuildFallbackTangent(normal);
		} else {
			tangent = NormalizeVector3Local(tangent);
		}

		const Vector3 bitangent = bitangentSums[i];
		const float handedness = DotVector3(CrossVector3(normal, tangent), bitangent) < 0.0f ? -1.0f : 1.0f;
		vertex.tangent = { tangent.x, tangent.y, tangent.z, handedness };
	}
}

/// @brief スキニング材質をGPUへ渡せる状態へ整える。
void PrepareSkinningMaterialForGpu(MaterialData& material) {
	if (material.textureFilePath.empty()) {
		material.textureFilePath = "resources/white512x512.png";
	}
	if (material.materialName.empty()) {
		material.materialName = "default";
	}
	material.semantic = InferMaterialSemantic(material);
	material.semanticInferred = true;

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
}

/// @brief 頂点の関節と重みを利用可能な影響枠へ追加する。
void InsertInfluence(VertexInfluence& influence, float weight, int32_t jointIndex) {
	for (size_t slot = 0; slot < influence.weights.size(); ++slot) {
		if (influence.weights[slot] == 0.0f) {
			influence.weights[slot] = weight;
			influence.jointIndices[slot] = jointIndex;
			return;
		}
	}
	const auto smallest = std::min_element(influence.weights.begin(), influence.weights.end());
	if (weight > *smallest) {
		const size_t slot = static_cast<size_t>(std::distance(influence.weights.begin(), smallest));
		influence.weights[slot] = weight;
		influence.jointIndices[slot] = jointIndex;
	}
}

} // namespace

SkinningModelAsset SkinningModelLoader::LoadFromFile(const std::string& filePath) {
	Assimp::Importer importer;
	constexpr unsigned int kImportFlags =
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_GenSmoothNormals |
		aiProcess_CalcTangentSpace |
		aiProcess_LimitBoneWeights |
		aiProcess_ImproveCacheLocality;
	const aiScene* scene = importer.ReadFile(filePath, kImportFlags);
	if (scene == nullptr || scene->mRootNode == nullptr || scene->mNumMeshes == 0) {
		throw std::runtime_error("Failed to load skinned model: " + filePath + " / " + importer.GetErrorString());
	}

	SkinningModelAsset asset;
	asset.rootNode = ReadNode(*scene->mRootNode);
	const std::filesystem::path sourcePath(filePath);
	asset.modelData.materials = LoadSkinningMaterials(asset, *scene, sourcePath);

	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		const aiMesh& mesh = *scene->mMeshes[meshIndex];
		const uint32_t vertexOffset = static_cast<uint32_t>(asset.modelData.vertices.size());
		const uint32_t indexStart = static_cast<uint32_t>(asset.modelData.indices.size());
		asset.modelData.vertices.reserve(asset.modelData.vertices.size() + mesh.mNumVertices);
		for (uint32_t vertexIndex = 0; vertexIndex < mesh.mNumVertices; ++vertexIndex) {
			const aiVector3D& position = mesh.mVertices[vertexIndex];
			const aiVector3D normal = mesh.HasNormals() ? mesh.mNormals[vertexIndex] : aiVector3D{};
			const aiVector3D uv = mesh.HasTextureCoords(0) ? mesh.mTextureCoords[0][vertexIndex] : aiVector3D{};
			VertexData vertex{};
			vertex.position = { -position.x, position.y, position.z, 1.0f };
			// AssimpのglTFインポータはVを下原点へ変換して返すため、
			// DirectX/WICの上原点テクスチャへ合わせて元に戻す。
			vertex.texcoord = { uv.x, 1.0f - uv.y };
			vertex.normal = ConvertAssimpDirection(normal);
			if (mesh.HasTangentsAndBitangents()) {
				const Vector3 tangent = ConvertAssimpDirection(mesh.mTangents[vertexIndex]);
				vertex.tangent = { tangent.x, tangent.y, tangent.z, 1.0f };
			}
			asset.modelData.vertices.push_back(vertex);
		}

		for (uint32_t faceIndex = 0; faceIndex < mesh.mNumFaces; ++faceIndex) {
			const aiFace& face = mesh.mFaces[faceIndex];
			if (face.mNumIndices != 3) {
				continue;
			}
			asset.modelData.indices.push_back(vertexOffset + face.mIndices[2]);
			asset.modelData.indices.push_back(vertexOffset + face.mIndices[1]);
			asset.modelData.indices.push_back(vertexOffset + face.mIndices[0]);
		}

		for (uint32_t boneIndex = 0; boneIndex < mesh.mNumBones; ++boneIndex) {
			const aiBone& bone = *mesh.mBones[boneIndex];
			JointWeightData& jointWeight = asset.jointWeights[bone.mName.C_Str()];
			jointWeight.inverseBindPoseMatrix = ConvertInverseBindPose(bone.mOffsetMatrix);
			jointWeight.vertexWeights.reserve(jointWeight.vertexWeights.size() + bone.mNumWeights);
			for (uint32_t weightIndex = 0; weightIndex < bone.mNumWeights; ++weightIndex) {
				const aiVertexWeight& weight = bone.mWeights[weightIndex];
				jointWeight.vertexWeights.push_back({ weight.mWeight, vertexOffset + weight.mVertexId });
			}
		}

		const uint32_t indexCount = static_cast<uint32_t>(asset.modelData.indices.size()) - indexStart;
		if (indexCount > 0) {
			SkinningModelAsset::Submesh submesh;
			submesh.indexStart = indexStart;
			submesh.indexCount = indexCount;
			submesh.materialIndex = mesh.mMaterialIndex < asset.modelData.materials.size()
				? mesh.mMaterialIndex
				: 0u;
			if (submesh.materialIndex < asset.modelData.materials.size()) {
				submesh.textureKey = asset.modelData.materials[submesh.materialIndex].textureFilePath;
			} else {
				submesh.textureKey = "resources/white512x512.png";
			}
			if (mesh.mMaterialIndex < scene->mNumMaterials) {
				aiString materialName;
				if (scene->mMaterials[mesh.mMaterialIndex]->Get(AI_MATKEY_NAME, materialName) == AI_SUCCESS) {
					submesh.materialName = materialName.C_Str();
				}
				int twoSided = 0;
				if (scene->mMaterials[mesh.mMaterialIndex]->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS) {
					submesh.doubleSided = twoSided != 0;
				}
			}
			asset.modelData.submeshes.push_back({
				indexStart,
				indexCount,
				submesh.materialIndex,
				submesh.materialName,
			});
			asset.submeshes.push_back(std::move(submesh));
		}
	}

	if (asset.submeshes.empty()) {
		SkinningModelAsset::Submesh submesh;
		submesh.indexStart = 0;
		submesh.indexCount = static_cast<uint32_t>(asset.modelData.indices.size());
		submesh.materialIndex = 0;
		submesh.materialName = "default";
		submesh.textureKey = asset.modelData.materials.empty()
			? "resources/white512x512.png"
			: asset.modelData.materials.front().textureFilePath;
		asset.submeshes.push_back(submesh);
		asset.modelData.submeshes.push_back({
			submesh.indexStart,
			submesh.indexCount,
			submesh.materialIndex,
			submesh.materialName,
		});
	}
	if (asset.modelData.materials.empty()) {
		asset.modelData.materials.push_back(MakeDefaultSkinnedMaterial());
	}
	asset.modelData.material = asset.modelData.materials.front();
	GenerateSkinningTangents(asset.modelData);
	return asset;
}

SkinCluster SkinCluster::Create(const Skeleton& skeleton, const SkinningModelAsset& asset) {
	SkinCluster cluster;
	cluster.inverseBindPoseMatrices_.assign(skeleton.joints.size(), MakeIdentity4x4());
	cluster.palette_.resize(skeleton.joints.size());
	cluster.influences_.resize(asset.modelData.vertices.size());

	for (const auto& [jointName, jointWeight] : asset.jointWeights) {
		const auto joint = skeleton.jointMap.find(jointName);
		if (joint == skeleton.jointMap.end()) {
			continue;
		}
		const int32_t jointIndex = joint->second;
		cluster.inverseBindPoseMatrices_[jointIndex] = jointWeight.inverseBindPoseMatrix;
		for (const VertexWeightData& vertexWeight : jointWeight.vertexWeights) {
			if (vertexWeight.vertexIndex < cluster.influences_.size() && vertexWeight.weight > 0.0f) {
				InsertInfluence(cluster.influences_[vertexWeight.vertexIndex], vertexWeight.weight, jointIndex);
			}
		}
	}

	for (VertexInfluence& influence : cluster.influences_) {
		float total = 0.0f;
		for (float weight : influence.weights) {
			total += weight;
		}
		if (total > 0.0f) {
			for (float& weight : influence.weights) {
				weight /= total;
				if (weight > 0.0f) {
					++cluster.assignedInfluenceCount_;
				}
			}
		} else {
			influence.weights[0] = 1.0f;
			influence.jointIndices[0] = skeleton.root >= 0 ? skeleton.root : 0;
			++cluster.assignedInfluenceCount_;
		}
	}
	cluster.Update(skeleton);
	return cluster;
}

void SkinnedModel::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::string& filePath) {
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	asset_ = SkinningModelLoader::LoadFromFile(filePath);
	TextureManager::GetInstance()->CreateFlatNormalTexture();
	TextureManager::GetInstance()->CreateBrdfLutTexture();
	TextureManager::GetInstance()->CreatePbrIrradianceTexture();
	TextureManager::GetInstance()->CreatePbrPrefilteredEnvironmentTexture();

	if (asset_.modelData.materials.empty()) {
		asset_.modelData.materials.push_back(asset_.modelData.material);
	}
	for (MaterialData& material : asset_.modelData.materials) {
		PrepareSkinningMaterialForGpu(material);
	}
	asset_.modelData.material = asset_.modelData.materials.front();

	skeleton_ = SkeletonSystem::Create(asset_.rootNode);
	skinCluster_ = SkinCluster::Create(skeleton_, asset_);
	try {
		animations_ = AnimationLoader::LoadAllFromFile(filePath);
		currentAnimationIndex_ = 0;
		animationPlayer_.SetAnimation(&animations_[currentAnimationIndex_]);
	} catch (const std::exception&) {
		// 骨格付き静止モデルもSkinnedModelで確認できるよう、
		// アニメーションがない場合はバインドポーズを維持する空クリップを使う。
		Animation bindPose;
		bindPose.name = "BindPose";
		bindPose.duration = 0.0f;
		animations_.push_back(std::move(bindPose));
		currentAnimationIndex_ = 0;
		animationPlayer_.SetAnimation(&animations_[currentAnimationIndex_]);
		animationPlayer_.SetPlaying(false);
	}

	std::unordered_set<std::string> embeddedTextureKeys;
	std::unordered_set<std::string> failedEmbeddedTextureKeys;
	for (const SkinningModelAsset::EmbeddedTexture& embedded : asset_.embeddedTextures) {
		const TextureManager::TextureColorSpace colorSpace = embedded.linearData
			? TextureManager::TextureColorSpace::LinearData
			: TextureManager::TextureColorSpace::SRGB;
		if (TextureManager::GetInstance()->LoadTextureFromMemory(
			embedded.textureKey, embedded.encodedData.data(), embedded.encodedData.size(), colorSpace)) {
			embeddedTextureKeys.insert(embedded.textureKey);
		} else {
			failedEmbeddedTextureKeys.insert(embedded.textureKey);
		}
	}

	for (MaterialData& material : asset_.modelData.materials) {
		if (failedEmbeddedTextureKeys.contains(material.textureFilePath)) {
			material.textureFilePath = "resources/white512x512.png";
			material.hasBaseColorFactor = true;
		}
		if (failedEmbeddedTextureKeys.contains(material.normalTextureFilePath)) {
			material.normalTextureFilePath = TextureManager::GetFlatNormalTexturePath();
			material.hasNormalTexture = false;
		}
		if (failedEmbeddedTextureKeys.contains(material.metallicRoughnessTextureFilePath)) {
			material.metallicRoughnessTextureFilePath = TextureManager::GetFlatNormalTexturePath();
			material.hasMetallicRoughnessTexture = false;
			material.hasMetallicTexture = false;
			material.hasRoughnessTexture = false;
		}
		if (failedEmbeddedTextureKeys.contains(material.occlusionTextureFilePath)) {
			material.occlusionTextureFilePath = TextureManager::GetFlatNormalTexturePath();
			material.hasOcclusionTexture = false;
		}

		if (!embeddedTextureKeys.contains(material.textureFilePath)) {
			TextureManager::GetInstance()->LoadTexture(material.textureFilePath);
		}
		if (material.hasNormalTexture && !embeddedTextureKeys.contains(material.normalTextureFilePath)) {
			TextureManager::GetInstance()->LoadTexture(
				material.normalTextureFilePath,
				TextureManager::TextureColorSpace::LinearData);
		}
		if (material.hasMetallicRoughnessTexture &&
			!embeddedTextureKeys.contains(material.metallicRoughnessTextureFilePath)) {
			TextureManager::GetInstance()->LoadTexture(
				material.metallicRoughnessTextureFilePath,
				TextureManager::TextureColorSpace::LinearData);
		}
		if (material.hasOcclusionTexture && !embeddedTextureKeys.contains(material.occlusionTextureFilePath)) {
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
	asset_.modelData.material = asset_.modelData.materials.front();

	for (SkinningModelAsset::Submesh& submesh : asset_.submeshes) {
		const uint32_t materialIndex = submesh.materialIndex < asset_.modelData.materials.size()
			? submesh.materialIndex
			: 0u;
		submesh.textureKey = asset_.modelData.materials[materialIndex].textureFilePath;
	}

	const size_t vertexBytes = sizeof(VertexData) * asset_.modelData.vertices.size();
	vertexResource_ = dxCommon_->CreateBufferResource(vertexBytes);
	VertexData* mappedVertices = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertices));
	std::memcpy(mappedVertices, asset_.modelData.vertices.data(), vertexBytes);
	vertexBufferViews_[0].BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferViews_[0].SizeInBytes = static_cast<UINT>(vertexBytes);
	vertexBufferViews_[0].StrideInBytes = sizeof(VertexData);

	const size_t influenceBytes = sizeof(VertexInfluence) * skinCluster_.GetInfluences().size();
	influenceResource_ = dxCommon_->CreateBufferResource(influenceBytes);
	VertexInfluence* mappedInfluences = nullptr;
	influenceResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluences));
	std::memcpy(mappedInfluences, skinCluster_.GetInfluences().data(), influenceBytes);
	vertexBufferViews_[1].BufferLocation = influenceResource_->GetGPUVirtualAddress();
	vertexBufferViews_[1].SizeInBytes = static_cast<UINT>(influenceBytes);
	vertexBufferViews_[1].StrideInBytes = sizeof(VertexInfluence);

	const size_t indexBytes = sizeof(uint32_t) * asset_.modelData.indices.size();
	indexResource_ = dxCommon_->CreateBufferResource(indexBytes);
	uint32_t* mappedIndices = nullptr;
	indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndices));
	std::memcpy(mappedIndices, asset_.modelData.indices.data(), indexBytes);
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = static_cast<UINT>(indexBytes);
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	const size_t paletteBytes = sizeof(SkinningPaletteEntry) * skinCluster_.GetPalette().size();
	paletteResource_ = dxCommon_->CreateBufferResource(paletteBytes);
	paletteResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette_));
	std::memcpy(mappedPalette_, skinCluster_.GetPalette().data(), paletteBytes);
	paletteSrvIndex_ = srvManager_->Allocate();
	srvManager_->CreateSRVforStructuredBuffer(
		paletteSrvIndex_, paletteResource_.Get(),
		static_cast<UINT>(skinCluster_.GetPalette().size()), sizeof(SkinningPaletteEntry));

}

bool SkinnedModel::SetAnimation(const std::string& name, bool restart) {
	for (size_t index = 0; index < animations_.size(); ++index) {
		if (animations_[index].name == name) {
			return SetAnimation(index, restart);
		}
	}
	return false;
}

bool SkinnedModel::SetAnimation(size_t index, bool restart) {
	if (index >= animations_.size()) {
		return false;
	}
	if (index == currentAnimationIndex_ && animationPlayer_.GetAnimation() != nullptr) {
		return true;
	}
	currentAnimationIndex_ = index;
	animationPlayer_.SetAnimation(&animations_[currentAnimationIndex_], restart);
	animationTransitionActive_ = false;
	return true;
}

bool SkinnedModel::TransitionToAnimation(
	const std::string& name,
	float duration,
	bool synchronizeNormalizedTime) {
	for (size_t index = 0; index < animations_.size(); ++index) {
		if (animations_[index].name == name) {
			return TransitionToAnimation(index, duration, synchronizeNormalizedTime);
		}
	}
	return false;
}

bool SkinnedModel::TransitionToAnimation(
	size_t index,
	float duration,
	bool synchronizeNormalizedTime) {
	if (index >= animations_.size()) {
		return false;
	}
	if (index == currentAnimationIndex_) {
		return true;
	}
	if (duration <= 0.0f || animationPlayer_.GetAnimation() == nullptr) {
		return SetAnimation(index, true);
	}

	animationTransitionStartPose_.clear();
	animationTransitionStartPose_.reserve(skeleton_.joints.size());
	for (const Joint& joint : skeleton_.joints) {
		animationTransitionStartPose_.push_back(joint.transform);
	}
	float normalizedTime = 0.0f;
	if (synchronizeNormalizedTime && animationPlayer_.GetAnimation() != nullptr) {
		const float previousDuration = animationPlayer_.GetAnimation()->duration;
		if (previousDuration > 0.0f) {
			normalizedTime = animationPlayer_.GetTime() / previousDuration;
		}
	}

	currentAnimationIndex_ = index;
	animationPlayer_.SetAnimation(&animations_[currentAnimationIndex_], true);
	if (synchronizeNormalizedTime && animations_[currentAnimationIndex_].duration > 0.0f) {
		animationPlayer_.Seek(normalizedTime * animations_[currentAnimationIndex_].duration);
	}
	animationTransitionDuration_ = duration;
	animationTransitionElapsed_ = 0.0f;
	animationTransitionActive_ = true;
	return true;
}

void SkinnedModel::Update(float deltaTime) {
	if (animationTransitionActive_) {
		animationPlayer_.Update(deltaTime);
		animationTransitionElapsed_ += deltaTime;
		const float linearT = (std::clamp)(
			animationTransitionElapsed_ / animationTransitionDuration_, 0.0f, 1.0f);
		const float smoothT = linearT * linearT * (3.0f - 2.0f * linearT);
		SkeletonSystem::ApplyAnimationBlendFromPose(
			skeleton_, animationTransitionStartPose_, animationPlayer_, smoothT);
		if (linearT >= 1.0f) {
			animationTransitionActive_ = false;
		}
	} else {
		animationPlayer_.Update(deltaTime);
		SkeletonSystem::ApplyAnimation(skeleton_, animationPlayer_);
	}
	skinCluster_.Update(skeleton_);
	std::memcpy(
		mappedPalette_, skinCluster_.GetPalette().data(),
		sizeof(SkinningPaletteEntry) * skinCluster_.GetPalette().size());
}

void SkinnedModel::UpdateBlended(
	float deltaTime,
	AnimationPlayer& animationA,
	AnimationPlayer& animationB,
	float blendFactor) {
	// 補間中だけでなく両方を常時進めておくことで、切替時に時間が飛ばない。
	animationA.Update(deltaTime);
	animationB.Update(deltaTime);
	SkeletonSystem::ApplyAnimationBlend(skeleton_, animationA, animationB, blendFactor);
	skinCluster_.Update(skeleton_);
	std::memcpy(
		mappedPalette_, skinCluster_.GetPalette().data(),
		sizeof(SkinningPaletteEntry) * skinCluster_.GetPalette().size());
}

void SkinnedModel::BindGeometry() const {
	assert(dxCommon_ != nullptr);
	assert(vertexResource_ != nullptr && influenceResource_ != nullptr && indexResource_ != nullptr);
	auto commandList = dxCommon_->GetList();
	commandList->IASetVertexBuffers(0, 2, vertexBufferViews_);
	commandList->IASetIndexBuffer(&indexBufferView_);
}

void SkinnedModel::BindSkinningPalette(uint32_t rootParameterIndex) const {
	assert(srvManager_ != nullptr && paletteResource_ != nullptr);
	srvManager_->SetGraphicsRootDescriptorTable(rootParameterIndex, paletteSrvIndex_);
}

void SkinnedModel::DrawSubmesh(size_t index) const {
	const SkinningModelAsset::Submesh& submesh = GetSubmesh(index);
	assert(dxCommon_ != nullptr);
	dxCommon_->GetList()->DrawIndexedInstanced(
		submesh.indexCount, 1, submesh.indexStart, 0, 0);
}

void SkinnedModel::Draw(const std::vector<D3D12_GPU_VIRTUAL_ADDRESS>& materialCbvAddresses) {
	auto commandList = dxCommon_->GetList();
	BindGeometry();
	BindSkinningPalette(9);
	for (size_t submeshIndex = 0; submeshIndex < GetSubmeshCount(); ++submeshIndex) {
		const SkinningModelAsset::Submesh& submesh = GetSubmesh(submeshIndex);
		const uint32_t materialIndex = submesh.materialIndex < asset_.modelData.materials.size()
			? submesh.materialIndex
			: 0u;
		const MaterialData& material = asset_.modelData.materials.empty()
			? asset_.modelData.material
			: asset_.modelData.materials[materialIndex];

		auto& pso = submesh.doubleSided
			? dxCommon_->GetPSOSkinningDoubleSidedForScene()
			: dxCommon_->GetPSOSkinningForScene();
		commandList->SetPipelineState(pso.graphicsState_.Get());
		if (materialIndex < materialCbvAddresses.size() && materialCbvAddresses[materialIndex] != 0) {
			commandList->SetGraphicsRootConstantBufferView(0, materialCbvAddresses[materialIndex]);
			commandList->SetGraphicsRootConstantBufferView(10, materialCbvAddresses[materialIndex]);
		}
		commandList->SetGraphicsRootDescriptorTable(
			2,
			TextureManager::GetInstance()->GetSrvHandleGPU(material.textureFilePath));
		commandList->SetGraphicsRootDescriptorTable(
			11,
			TextureManager::GetInstance()->GetSrvHandleGPU(
				material.normalTextureFilePath,
				TextureManager::TextureColorSpace::LinearData));
		commandList->SetGraphicsRootDescriptorTable(
			12,
			TextureManager::GetInstance()->GetSrvHandleGPU(
				material.metallicRoughnessTextureFilePath,
				TextureManager::TextureColorSpace::LinearData));
		commandList->SetGraphicsRootDescriptorTable(
			13,
			TextureManager::GetInstance()->GetSrvHandleGPU(
				material.occlusionTextureFilePath,
				TextureManager::TextureColorSpace::LinearData));
		commandList->SetGraphicsRootDescriptorTable(
			14,
			TextureManager::GetInstance()->GetSrvHandleGPU(
				TextureManager::GetBrdfLutTexturePath(),
				TextureManager::TextureColorSpace::LinearData));
		commandList->SetGraphicsRootDescriptorTable(
			15,
			TextureManager::GetInstance()->GetSrvHandleGPU(
				TextureManager::GetPbrIrradianceTexturePath(),
				TextureManager::TextureColorSpace::LinearData));
		commandList->SetGraphicsRootDescriptorTable(
			16,
			TextureManager::GetInstance()->GetSrvHandleGPU(
				TextureManager::GetPbrPrefilteredEnvironmentTexturePath(),
				TextureManager::TextureColorSpace::LinearData));
		DrawSubmesh(submeshIndex);
	}
}

void SkinnedModel::DrawShadow() {
	auto commandList = dxCommon_->GetList();
	BindGeometry();
	BindSkinningPalette(1);
	commandList->DrawIndexedInstanced(static_cast<UINT>(asset_.modelData.indices.size()), 1, 0, 0, 0);
}

void SkinCluster::Update(const Skeleton& skeleton) {
	assert(skeleton.joints.size() == inverseBindPoseMatrices_.size());
	for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
		const Matrix4x4 skinningMatrix = Multiply(
			inverseBindPoseMatrices_[jointIndex], skeleton.joints[jointIndex].skeletonSpaceMatrix);
		palette_[jointIndex].skeletonSpaceMatrix = skinningMatrix;
		palette_[jointIndex].skeletonSpaceInverseTransposeMatrix = Transpose(Inverse(skinningMatrix));
	}
}

} // namespace cg2
