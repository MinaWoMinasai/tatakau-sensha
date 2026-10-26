#include "ModelInternal.h"
#include <cmath>
#include <vector>

namespace myEngine::modelDetail {

/// @brief 入力の3成分をエンジンのVector3へ変換する。
Vector3 ToVector3(const Vector4& value)
{
    return {value.x, value.y, value.z};
}

/// @brief 2つの3Dベクトルの差を返す。
Vector3 SubtractVector3(const Vector3& a, const Vector3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

/// @brief 2つの3Dベクトルの外積を返す。
Vector3 CrossVector3(const Vector3& a, const Vector3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

/// @brief 2つの3Dベクトルの内積を返す。
float DotVector3(const Vector3& a, const Vector3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/// @brief 3Dベクトルの長さを返す。
float LengthVector3(const Vector3& value)
{
    return std::sqrt(DotVector3(value, value));
}

/// @brief ベクトル3を正規化する。
Vector3 NormalizeVector3(const Vector3& value)
{
    const float length = LengthVector3(value);
    if (length <= 0.00001f) {
        return {0.0f, 1.0f, 0.0f};
    }
    return {value.x / length, value.y / length, value.z / length};
}

/// @brief 3Dベクトルへ指定倍率を掛けて返す。
Vector3 MultiplyVector3(const Vector3& value, float scalar)
{
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

/// @brief ベクトル3を追加する。
Vector3 AddVector3(const Vector3& a, const Vector3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

namespace {
/// @brief Fallback接線を組み立てる。
Vector3 BuildFallbackTangent(const Vector3& normal)
{
    const Vector3 up = std::abs(normal.y) < 0.95f ? Vector3{0.0f, 1.0f, 0.0f} : Vector3{1.0f, 0.0f, 0.0f};
    return NormalizeVector3(CrossVector3(up, normal));
}
} // namespace

/// @brief 頂点位置とUVからモデルの接線を生成する。
void GenerateModelTangents(ModelData& modelData)
{
    std::vector<Vector3> tangentSums(modelData.vertices.size(), {});
    std::vector<Vector3> bitangentSums(modelData.vertices.size(), {});

    for (size_t i = 0; i + 2 < modelData.indices.size(); i += 3) {
        const uint32_t index0 = modelData.indices[i + 0];
        const uint32_t index1 = modelData.indices[i + 1];
        const uint32_t index2 = modelData.indices[i + 2];
        if (index0 >= modelData.vertices.size() || index1 >= modelData.vertices.size() || index2 >= modelData.vertices.size()) {
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
        const Vector2 deltaUv1 = {v1.texcoord.x - v0.texcoord.x, v1.texcoord.y - v0.texcoord.y};
        const Vector2 deltaUv2 = {v2.texcoord.x - v0.texcoord.x, v2.texcoord.y - v0.texcoord.y};
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

        const uint32_t indices[3] = {index0, index1, index2};
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
        vertex.tangent = {tangent.x, tangent.y, tangent.z, handedness};
    }
}

} // namespace myEngine::modelDetail
