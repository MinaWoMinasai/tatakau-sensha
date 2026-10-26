#pragma once
#include "MaterialTypes.h"
#include <cstdint>
#include <string>
#include <vector>

namespace myEngine {

/// @brief モデルの頂点位置・UV・法線などをGPUへ渡す。
struct VertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
    Vector4 tangent = {1.0f, 0.0f, 0.0f, 1.0f};
};

/// @brief モデルをマテリアルごとの描画範囲に分ける。
struct ModelSubmesh {
    uint32_t startIndex = 0;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;
    std::string materialName;
};

/// @brief 読み込んだモデルの頂点・材質・ノード階層を保持する。
struct ModelData {
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
    MaterialData material;
    std::vector<MaterialData> materials;
    std::vector<ModelSubmesh> submeshes;
};

} // namespace myEngine
