#include "Model.h"
#include <algorithm>
#include <unordered_map>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
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

std::string ToLowerAscii(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return value;
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
	InitializeFromModelData(modelCommon, LoadObjFile(directorypath, filename));
}

void Model::InitializeFromModelData(ModelCommon* modelCommon, const ModelData& modelData)
{
	modelCommon_ = modelCommon;
	modelData_ = modelData;
	if (modelData_.material.textureFilePath.empty()) {
		modelData_.material.textureFilePath = "resources/white512x512.png";
	}
	TextureManager::GetInstance()->CreateFlatNormalTexture();
	TextureManager::GetInstance()->CreateBrdfLutTexture();
	TextureManager::GetInstance()->CreatePbrIrradianceTexture();
	TextureManager::GetInstance()->CreatePbrPrefilteredEnvironmentTexture();
	if (!modelData_.material.hasNormalTexture || modelData_.material.normalTextureFilePath.empty()) {
		modelData_.material.normalTextureFilePath = TextureManager::GetFlatNormalTexturePath();
		modelData_.material.hasNormalTexture = false;
	}
	if (!modelData_.material.hasMetallicRoughnessTexture || modelData_.material.metallicRoughnessTextureFilePath.empty()) {
		modelData_.material.metallicRoughnessTextureFilePath = TextureManager::GetFlatNormalTexturePath();
		modelData_.material.hasMetallicRoughnessTexture = false;
	}
	if (!modelData_.material.hasOcclusionTexture || modelData_.material.occlusionTextureFilePath.empty()) {
		modelData_.material.occlusionTextureFilePath = TextureManager::GetFlatNormalTexturePath();
		modelData_.material.hasOcclusionTexture = false;
	}
	GenerateModelTangents(modelData_);

	CreateGpuResources();

	// objの参照しているテクスチャファイル読み込み
	TextureManager::GetInstance()->LoadTexture(modelData_.material.textureFilePath);
	if (modelData_.material.hasNormalTexture) {
		TextureManager::GetInstance()->LoadTexture(
			modelData_.material.normalTextureFilePath,
			TextureManager::TextureColorSpace::LinearData);
	}
	if (modelData_.material.hasMetallicRoughnessTexture) {
		TextureManager::GetInstance()->LoadTexture(
			modelData_.material.metallicRoughnessTextureFilePath,
			TextureManager::TextureColorSpace::LinearData);
	}
	if (modelData_.material.hasOcclusionTexture) {
		TextureManager::GetInstance()->LoadTexture(
			modelData_.material.occlusionTextureFilePath,
			TextureManager::TextureColorSpace::LinearData);
	}
	// 読み込んだテクスチャの番号を取得
	modelData_.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexbyFilePath(modelData_.material.textureFilePath);
	modelData_.material.normalTextureIndex = TextureManager::GetInstance()->GetTextureIndexbyFilePath(
		modelData_.material.normalTextureFilePath,
		TextureManager::TextureColorSpace::LinearData);
	modelData_.material.metallicRoughnessTextureIndex = TextureManager::GetInstance()->GetTextureIndexbyFilePath(
		modelData_.material.metallicRoughnessTextureFilePath,
		TextureManager::TextureColorSpace::LinearData);
	modelData_.material.occlusionTextureIndex = TextureManager::GetInstance()->GetTextureIndexbyFilePath(
		modelData_.material.occlusionTextureFilePath,
		TextureManager::TextureColorSpace::LinearData);
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

void Model::Draw() {
	
	modelCommon_->GetDxCommon()->GetList()->IASetVertexBuffers(0, 1, &vertexBufferView); //VBVを設定
	modelCommon_->GetDxCommon()->GetList()->IASetIndexBuffer(&indexBufferView);
	modelCommon_->GetDxCommon()->GetList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(modelData_.material.textureFilePath));
	modelCommon_->GetDxCommon()->GetList()->SetGraphicsRootDescriptorTable(
		11,
		TextureManager::GetInstance()->GetSrvHandleGPU(
			modelData_.material.normalTextureFilePath,
			TextureManager::TextureColorSpace::LinearData));
	modelCommon_->GetDxCommon()->GetList()->SetGraphicsRootDescriptorTable(
		12,
		TextureManager::GetInstance()->GetSrvHandleGPU(
			modelData_.material.metallicRoughnessTextureFilePath,
			TextureManager::TextureColorSpace::LinearData));
	modelCommon_->GetDxCommon()->GetList()->SetGraphicsRootDescriptorTable(
		13,
		TextureManager::GetInstance()->GetSrvHandleGPU(
			modelData_.material.occlusionTextureFilePath,
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
	modelCommon_->GetDxCommon()->GetList()->DrawIndexedInstanced(UINT(modelData_.indices.size()), 1, 0, 0, 0);

}

void Model::DrawOnlyMesh() {
	// 頂点バッファのセットと描画コマンドのみ
	modelCommon_->GetDxCommon()->GetList()->IASetVertexBuffers(0, 1, &vertexBufferView);
	modelCommon_->GetDxCommon()->GetList()->IASetIndexBuffer(&indexBufferView);
	modelCommon_->GetDxCommon()->GetList()->DrawIndexedInstanced(UINT(modelData_.indices.size()), 1, 0, 0, 0);
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
		} else if (identifierLower == "map_orm" || identifierLower == "map_rma" ||
			identifierLower == "map_mr" || identifierLower == "map_metallicroughness") {
			std::string textureFilename = ReadTextureFilename(s);
			const std::string texturePath = ResolveTexturePath(directoryPath, textureFilename);
			materialData.metallicRoughnessTextureFilePath = texturePath;
			materialData.hasMetallicRoughnessTexture = !texturePath.empty();
			materialData.occlusionTextureFilePath = texturePath;
			materialData.hasOcclusionTexture = !texturePath.empty();
		} else if (identifierLower == "map_ao" || identifierLower == "map_occlusion") {
			std::string textureFilename = ReadTextureFilename(s);
			materialData.occlusionTextureFilePath = ResolveTexturePath(directoryPath, textureFilename);
			materialData.hasOcclusionTexture = !materialData.occlusionTextureFilePath.empty();
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
	std::string currentMaterialName;
	std::unordered_map<std::string, uint32_t> vertexDefinitionToIndex;

	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // 開けなかったらエラー

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier; // 先端の識別子を読む

		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			position.x *= 1.0f;
			position.y *= 1.0f;
			position.z *= -1.0f;
			positions.push_back(position);
		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			//texcoord.x = 1.0f - texcoord.x;
			texcoords.push_back(texcoord);
		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f;
			normals.push_back(normal);
		} else if (identifier == "f") {

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
		} else if (identifier == "mtllib") {
			// materialTemplateLiblaryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;
			// 基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		} else if (identifier == "newmtl") {
			s >> currentMaterialName;
		}
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
	modelData.material.textureFilePath = "resources/white512x512.png";
	modelData.material.normalTextureFilePath = TextureManager::GetFlatNormalTexturePath();
	modelData.material.metallicRoughnessTextureFilePath = TextureManager::GetFlatNormalTexturePath();
	modelData.material.occlusionTextureFilePath = TextureManager::GetFlatNormalTexturePath();
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
