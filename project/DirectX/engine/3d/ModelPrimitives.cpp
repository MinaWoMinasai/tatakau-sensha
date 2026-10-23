#include "Model.h"
#include "ModelInternal.h"
#include "ResourcePaths.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace myEngine {

using namespace modelDetail;

namespace modelDetail {

/// @brief 材質の指定がない図形に使う、既定の材質を作って返す。
MaterialData MakeDefaultPrimitiveMaterial()
{
    MaterialData material;
    material.materialName = "default";
    material.semantic = MaterialSemantic::GenericPbr;
    material.textureFilePath = ResourcePaths::DefaultWhiteTexture();
    material.normalTextureFilePath = TextureManager::GetFlatNormalTexturePath();
    material.metallicRoughnessTextureFilePath = TextureManager::GetFlatNormalTexturePath();
    material.occlusionTextureFilePath = TextureManager::GetFlatNormalTexturePath();
    return material;
}

} // namespace modelDetail

namespace {
/// @brief 位置を作成して返す。
Vector4 MakePosition(float x, float y, float z)
{
    return {x, y, z, 1.0f};
}

/// @brief 両面面三角形を追加する。
void AddDoubleSidedTriangle(std::vector<uint32_t>& indices, uint32_t index0, uint32_t index1, uint32_t index2)
{
    indices.push_back(index0);
    indices.push_back(index1);
    indices.push_back(index2);
    indices.push_back(index2);
    indices.push_back(index1);
    indices.push_back(index0);
}
} // namespace

ModelData Model::CreatePlane(float width, float depth)
{
    width = (std::max)(width, 0.001f);
    depth = (std::max)(depth, 0.001f);

    const float halfWidth = width * 0.5f;
    const float halfDepth = depth * 0.5f;
    const Vector3 normal = {0.0f, 1.0f, 0.0f};

    ModelData modelData;
    modelData.material = MakeDefaultPrimitiveMaterial();
    modelData.vertices = {
        {MakePosition(-halfWidth, 0.0f, -halfDepth), {0.0f, 1.0f}, normal},
        {MakePosition(halfWidth, 0.0f, -halfDepth), {1.0f, 1.0f}, normal},
        {MakePosition(-halfWidth, 0.0f, halfDepth), {0.0f, 0.0f}, normal},
        {MakePosition(halfWidth, 0.0f, halfDepth), {1.0f, 0.0f}, normal},
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
    const Vector3 normal = {0.0f, 1.0f, 0.0f};

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
                {u, 1.0f - v},
                normal,
                {1.0f, 0.0f, 0.0f, 1.0f},
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
        modelData.vertices.push_back({p0, {0.0f, 1.0f}, normal});
        modelData.vertices.push_back({p1, {1.0f, 1.0f}, normal});
        modelData.vertices.push_back({p2, {0.0f, 0.0f}, normal});
        modelData.vertices.push_back({p3, {1.0f, 0.0f}, normal});
        AddDoubleSidedTriangle(modelData.indices, base + 0, base + 1, base + 2);
        AddDoubleSidedTriangle(modelData.indices, base + 2, base + 1, base + 3);
    };

    addQuad(MakePosition(-halfX, -halfY, halfZ), MakePosition(halfX, -halfY, halfZ), MakePosition(-halfX, halfY, halfZ),
            MakePosition(halfX, halfY, halfZ), {0.0f, 0.0f, 1.0f});
    addQuad(MakePosition(halfX, -halfY, -halfZ), MakePosition(-halfX, -halfY, -halfZ), MakePosition(halfX, halfY, -halfZ),
            MakePosition(-halfX, halfY, -halfZ), {0.0f, 0.0f, -1.0f});
    addQuad(MakePosition(halfX, -halfY, halfZ), MakePosition(halfX, -halfY, -halfZ), MakePosition(halfX, halfY, halfZ),
            MakePosition(halfX, halfY, -halfZ), {1.0f, 0.0f, 0.0f});
    addQuad(MakePosition(-halfX, -halfY, -halfZ), MakePosition(-halfX, -halfY, halfZ), MakePosition(-halfX, halfY, -halfZ),
            MakePosition(-halfX, halfY, halfZ), {-1.0f, 0.0f, 0.0f});
    addQuad(MakePosition(-halfX, halfY, halfZ), MakePosition(halfX, halfY, halfZ), MakePosition(-halfX, halfY, -halfZ),
            MakePosition(halfX, halfY, -halfZ), {0.0f, 1.0f, 0.0f});
    addQuad(MakePosition(-halfX, -halfY, -halfZ), MakePosition(halfX, -halfY, -halfZ), MakePosition(-halfX, -halfY, halfZ),
            MakePosition(halfX, -halfY, halfZ), {0.0f, -1.0f, 0.0f});

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
        const float phase = static_cast<float>(side) * 2.0f * kPi / static_cast<float>(sides);
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
        Vector3 normal = NormalizeVector3(CrossVector3(SubtractVector3(p1, p0), SubtractVector3(p2, p0)));
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
        modelData.vertices.push_back({MakePosition(p0.x, p0.y, p0.z), {0.5f, 0.0f}, normal});
        modelData.vertices.push_back({MakePosition(p1.x, p1.y, p1.z), {0.0f, 1.0f}, normal});
        modelData.vertices.push_back({MakePosition(p2.x, p2.y, p2.z), {1.0f, 1.0f}, normal});
        AddDoubleSidedTriangle(modelData.indices, base, base + 1u, base + 2u);
    };

    const Vector3 top = {0.0f, halfHeight, 0.0f};
    const Vector3 bottom = {0.0f, -halfHeight, 0.0f};
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
        const Vector3 normal = NormalizeVector3({x, 0.0f, z});
        modelData.vertices.push_back({MakePosition(x, -halfHeight, z), {u, 1.0f}, normal});
        modelData.vertices.push_back({MakePosition(x, halfHeight, z), {u, 0.0f}, normal});
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
    modelData.vertices.push_back({MakePosition(0.0f, halfHeight, 0.0f), {0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}});
    const uint32_t topRingBase = static_cast<uint32_t>(modelData.vertices.size());
    for (uint32_t i = 0; i <= segments; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(segments);
        const float phi = u * kPi * 2.0f;
        const float x = std::cos(phi) * radius;
        const float z = std::sin(phi) * radius;
        modelData.vertices.push_back({
            MakePosition(x, halfHeight, z),
            {x / (radius * 2.0f) + 0.5f, 0.5f - z / (radius * 2.0f)},
            {0.0f, 1.0f, 0.0f},
        });
    }

    const uint32_t bottomCenter = static_cast<uint32_t>(modelData.vertices.size());
    modelData.vertices.push_back({MakePosition(0.0f, -halfHeight, 0.0f), {0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}});
    const uint32_t bottomRingBase = static_cast<uint32_t>(modelData.vertices.size());
    for (uint32_t i = 0; i <= segments; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(segments);
        const float phi = u * kPi * 2.0f;
        const float x = std::cos(phi) * radius;
        const float z = std::sin(phi) * radius;
        modelData.vertices.push_back({
            MakePosition(x, -halfHeight, z),
            {x / (radius * 2.0f) + 0.5f, z / (radius * 2.0f) + 0.5f},
            {0.0f, -1.0f, 0.0f},
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
                {normal.x * radius, normal.y * radius, normal.z * radius, 1.0f},
                {u, v},
                normal,
                {std::sin(phi), 0.0f, -std::cos(phi), 1.0f},
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

} // namespace myEngine
