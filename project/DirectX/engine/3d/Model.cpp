#include "Model.h"
#include "ModelInternal.h"
#include "ResourcePaths.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>

namespace myEngine {

using namespace modelDetail;

namespace {
/// @brief 共有する頂点の法線を集計するための位置キーを表す。
struct SmoothNormalKey {
    int64_t x;
    int64_t y;
    int64_t z;

    /// @brief 値が等しいか判定する。
    bool operator==(const SmoothNormalKey& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

/// @brief 法線集計用の位置キーをハッシュ値に変換する。
struct SmoothNormalKeyHash {
    /// @brief 入力値をこの型の規約に従って評価する。
    size_t operator()(const SmoothNormalKey& key) const
    {
        size_t h = std::hash<int64_t>{}(key.x);
        h ^= std::hash<int64_t>{}(key.y) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        h ^= std::hash<int64_t>{}(key.z) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        return h;
    }
};

/// @brief 滑らかな法線キーを作成して返す。
SmoothNormalKey MakeSmoothNormalKey(const Vector4& position)
{
    constexpr float kScale = 10000.0f;
    return {
        static_cast<int64_t>(std::llround(position.x * kScale)),
        static_cast<int64_t>(std::llround(position.y * kScale)),
        static_cast<int64_t>(std::llround(position.z * kScale)),
    };
}
} // namespace

void Model::Initialize(DirectXCommon* dxCommon, const std::string& directorypath, const std::string& filename)
{
    if (IsObjModel(filename)) {
        InitializeFromModelData(dxCommon, LoadObjFile(directorypath, filename));
        return;
    }
    InitializeFromModelData(dxCommon, LoadAssimpFile(directorypath, filename));
}

void Model::InitializeFromModelData(DirectXCommon* dxCommon, const ModelData& modelData)
{
    dxCommon_ = dxCommon;
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
            material.textureFilePath = ResourcePaths::DefaultWhiteTexture();
        }

        if (!material.metallicTextureFilePath.empty() || !material.roughnessTextureFilePath.empty()) {
            const std::string packedMaterialPath = TextureManager::GetInstance()->CreatePackedPbrMaterialTexture(
                material.metallicTextureFilePath, material.metallicMapChannel, material.roughnessTextureFilePath,
                material.roughnessMapChannel, material.occlusionTextureFilePath, material.occlusionMapChannel, material.metallicFactor,
                material.roughnessFactor, material.ambientOcclusionFactor);
            if (!packedMaterialPath.empty()) {
                material.metallicRoughnessTextureFilePath = packedMaterialPath;
                material.hasMetallicRoughnessTexture = material.hasMetallicTexture || material.hasRoughnessTexture;
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
            TextureManager::GetInstance()->LoadTexture(material.normalTextureFilePath, TextureManager::TextureColorSpace::LinearData);
        }
        if (material.hasMetallicRoughnessTexture) {
            TextureManager::GetInstance()->LoadTexture(material.metallicRoughnessTextureFilePath,
                                                       TextureManager::TextureColorSpace::LinearData);
        }
        if (material.hasOcclusionTexture) {
            TextureManager::GetInstance()->LoadTexture(material.occlusionTextureFilePath, TextureManager::TextureColorSpace::LinearData);
        }

        material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(material.textureFilePath);
        material.normalTextureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(
            material.normalTextureFilePath, TextureManager::TextureColorSpace::LinearData);
        material.metallicRoughnessTextureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(
            material.metallicRoughnessTextureFilePath, TextureManager::TextureColorSpace::LinearData);
        material.occlusionTextureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(
            material.occlusionTextureFilePath, TextureManager::TextureColorSpace::LinearData);
    }
    modelData_.material = modelData_.materials.front();
}

void Model::CreateGpuResources()
{
    // 用の頂点リソースを作る
    vertexResource =
        texture.CreateBufferResource(dxCommon_->GetDevice(), sizeof(VertexData) * modelData_.vertices.size());

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
    indexResource = texture.CreateBufferResource(dxCommon_->GetDevice(), sizeof(uint32_t) * modelData_.indices.size());
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

void Model::Draw(const std::vector<D3D12_GPU_VIRTUAL_ADDRESS>& materialCbvAddresses, const D3D12_GPU_DESCRIPTOR_HANDLE* baseColorTexture)
{
    dxCommon_->GetList()->IASetVertexBuffers(0, 1, &vertexBufferView); //VBVを設定
    dxCommon_->GetList()->IASetIndexBuffer(&indexBufferView);

    auto drawSubmesh = [&](const ModelSubmesh& submesh) {
        const uint32_t materialIndex = submesh.materialIndex < modelData_.materials.size() ? submesh.materialIndex : 0u;
        const MaterialData& material = modelData_.materials.empty() ? modelData_.material : modelData_.materials[materialIndex];

        if (materialIndex < materialCbvAddresses.size() && materialCbvAddresses[materialIndex] != 0) {
            dxCommon_->GetList()->SetGraphicsRootConstantBufferView(0, materialCbvAddresses[materialIndex]);
            dxCommon_->GetList()->SetGraphicsRootConstantBufferView(10, materialCbvAddresses[materialIndex]);
        }

        dxCommon_->GetList()->SetGraphicsRootDescriptorTable(
            2, baseColorTexture ? *baseColorTexture : TextureManager::GetInstance()->GetSrvHandleGPU(material.textureFilePath));
        dxCommon_->GetList()->SetGraphicsRootDescriptorTable(
            11,
            TextureManager::GetInstance()->GetSrvHandleGPU(material.normalTextureFilePath, TextureManager::TextureColorSpace::LinearData));
        dxCommon_->GetList()->SetGraphicsRootDescriptorTable(
            12, TextureManager::GetInstance()->GetSrvHandleGPU(material.metallicRoughnessTextureFilePath,
                                                               TextureManager::TextureColorSpace::LinearData));
        dxCommon_->GetList()->SetGraphicsRootDescriptorTable(
            13, TextureManager::GetInstance()->GetSrvHandleGPU(material.occlusionTextureFilePath,
                                                               TextureManager::TextureColorSpace::LinearData));
        dxCommon_->GetList()->SetGraphicsRootDescriptorTable(
            14, TextureManager::GetInstance()->GetSrvHandleGPU(TextureManager::GetBrdfLutTexturePath(),
                                                               TextureManager::TextureColorSpace::LinearData));
        dxCommon_->GetList()->SetGraphicsRootDescriptorTable(
            15, TextureManager::GetInstance()->GetSrvHandleGPU(TextureManager::GetPbrIrradianceTexturePath(),
                                                               TextureManager::TextureColorSpace::LinearData));
        dxCommon_->GetList()->SetGraphicsRootDescriptorTable(
            16, TextureManager::GetInstance()->GetSrvHandleGPU(TextureManager::GetPbrPrefilteredEnvironmentTexturePath(),
                                                               TextureManager::TextureColorSpace::LinearData));
        dxCommon_->GetList()->DrawIndexedInstanced(submesh.indexCount, 1, submesh.startIndex, 0, 0);
    };

    if (modelData_.submeshes.empty()) {
        drawSubmesh({0, static_cast<uint32_t>(modelData_.indices.size()), 0, "default"});
    } else {
        for (const ModelSubmesh& submesh : modelData_.submeshes) {
            drawSubmesh(submesh);
        }
    }
}

void Model::DrawOnlyMesh()
{
    // 頂点バッファのセットと描画コマンドのみ
    dxCommon_->GetList()->IASetVertexBuffers(0, 1, &vertexBufferView);
    dxCommon_->GetList()->IASetIndexBuffer(&indexBufferView);
    if (modelData_.submeshes.empty()) {
        dxCommon_->GetList()->DrawIndexedInstanced(UINT(modelData_.indices.size()), 1, 0, 0, 0);
    } else {
        for (const ModelSubmesh& submesh : modelData_.submeshes) {
            dxCommon_->GetList()->DrawIndexedInstanced(submesh.indexCount, 1, submesh.startIndex, 0, 0);
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
        if (index0 >= modelData_.vertices.size() || index1 >= modelData_.vertices.size() || index2 >= modelData_.vertices.size()) {
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
        faceNormal = {faceNormal.x / faceLength, faceNormal.y / faceLength, faceNormal.z / faceLength};

        const uint32_t indices[3] = {index0, index1, index2};
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
            normal = {-normal.x, -normal.y, -normal.z};
        }
        vertex.normal = normal;
    }

    GenerateModelTangents(modelData_);

    if (vertexData) {
        std::memcpy(vertexData, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
    }
}

} // namespace myEngine
