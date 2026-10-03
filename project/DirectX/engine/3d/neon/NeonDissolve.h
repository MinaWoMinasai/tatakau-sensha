#pragma once
#include "SkinCluster.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace cg2 {

// Direction and all distances are in the frozen, skinned model space, before World.
// scanMin/Max bound the directional projection; noise adds +/- noiseStrength/2.
struct NeonDissolveParams {
    Vector3 direction{0.70710678f, -0.70710678f, 0.0f};
    float scanMin = 0.0f;
    float scanMax = 1.0f;
    float progress = 0.0f;
    float noiseStrength = 0.08f; // model units, full noise range
    float noiseScale = 12.0f; // lattice cells per model unit
    uint32_t enabled = 0;
    uint32_t edgeEnabled = 1;
    uint32_t seed = 1337;
    uint32_t padding = 0;
    Vector3 edgeColor{1.0f, 0.025f, 0.35f};
    float edgeIntensity = 4.0f;
    float edgeWidth = 0.008f; // surviving-side width, in projected model units
    float edgePadding[3]{};
};
static_assert(sizeof(NeonDissolveParams) == 80);
static_assert(offsetof(NeonDissolveParams, scanMin) == 12);
static_assert(offsetof(NeonDissolveParams, scanMax) == 16);
static_assert(offsetof(NeonDissolveParams, enabled) == 32);
static_assert(offsetof(NeonDissolveParams, edgeColor) == 48);
static_assert(offsetof(NeonDissolveParams, edgeWidth) == 64);

enum class NeonDissolveDirection { UpperLeftToLowerRight, UpperRightToLowerLeft, TopToBottom, LeftToRight };

inline bool IsFiniteNeonPosition(const Vector3& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

inline bool NormalizeNeonDirection(Vector3& direction) {
    if (!IsFiniteNeonPosition(direction)) return false;
    const double length = std::sqrt(double(direction.x) * direction.x + double(direction.y) * direction.y +
        double(direction.z) * direction.z);
    if (!std::isfinite(length) || length < 1.0e-8) return false;
    direction = {float(direction.x / length), float(direction.y / length), float(direction.z / length)};
    return true;
}

inline NeonDissolveParams SanitizeNeonDissolveParams(NeonDissolveParams value) {
    const auto clean = [](float x, float low, float high, float fallback) {
        return std::isfinite(x) ? (std::clamp)(x, low, high) : fallback;
    };
    value.enabled = value.enabled ? 1u : 0u;
    value.edgeEnabled = value.edgeEnabled ? 1u : 0u;
    // Invalid scan data falls back to intact rendering rather than disappearing unpredictably.
    if (!NormalizeNeonDirection(value.direction) || !std::isfinite(value.scanMin) ||
        !std::isfinite(value.scanMax) || value.scanMax < value.scanMin ||
        std::abs(value.scanMin) > 1.0e6f || std::abs(value.scanMax) > 1.0e6f) {
        value.enabled = 0;
        value.direction = {1, 0, 0}; value.scanMin = 0; value.scanMax = 1;
    }
    value.progress = clean(value.progress, 0, 1, 0);
    value.noiseStrength = clean(value.noiseStrength, 0, 1.0e4f, 0);
    value.noiseScale = clean(value.noiseScale, 0, 128, 0);
    value.edgeColor = {clean(value.edgeColor.x, 0, 100, 0), clean(value.edgeColor.y, 0, 100, 0),
        clean(value.edgeColor.z, 0, 100, 0)};
    value.edgeIntensity = clean(value.edgeIntensity, 0, 100, 0);
    value.edgeWidth = clean(value.edgeWidth, 0, 1.0e4f, 0);
    value.padding = 0;
    value.edgePadding[0] = value.edgePadding[1] = value.edgePadding[2] = 0;
    return value;
}

// Row-vector positions: pWorld = pModel * World. A plane is a covector, so its
// model coefficient is World_linear * nWorld, not the vector transform nWorld * World.
// World translation is constant for all points and cancels when computing scan bounds.
inline bool MakeNeonDissolveDirection(const Matrix4x4& world, const Matrix4x4& cameraWorld,
    NeonDissolveDirection preset, Vector3& output, std::string* error = nullptr) {
    const auto fail = [&](const char* message) { if (error) *error = message; return false; };
    for (size_t row = 0; row < 4; ++row) for (size_t column = 0; column < 4; ++column) {
        if (!std::isfinite(world.m[row][column]) || !std::isfinite(cameraWorld.m[row][column]))
            return fail("Dissolve requires finite World and camera matrices.");
    }
    if (std::abs(world.m[0][3]) > 1.0e-6f || std::abs(world.m[1][3]) > 1.0e-6f ||
        std::abs(world.m[2][3]) > 1.0e-6f || std::abs(world.m[3][3] - 1.0f) > 1.0e-6f)
        return fail("Dissolve World must be affine.");
    const double determinant = double(world.m[0][0]) * (double(world.m[1][1]) * world.m[2][2] - double(world.m[1][2]) * world.m[2][1])
        - double(world.m[0][1]) * (double(world.m[1][0]) * world.m[2][2] - double(world.m[1][2]) * world.m[2][0])
        + double(world.m[0][2]) * (double(world.m[1][0]) * world.m[2][1] - double(world.m[1][1]) * world.m[2][0]);
    if (!std::isfinite(determinant) || std::abs(determinant) < 1.0e-12)
        return fail("Dissolve cannot scan a singular World transform.");
    Vector3 right{cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2]};
    Vector3 up{cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2]};
    if (!NormalizeNeonDirection(right) || !NormalizeNeonDirection(up))
        return fail("Dissolve camera axes are degenerate.");
    Vector3 normal{};
    switch (preset) {
    case NeonDissolveDirection::UpperLeftToLowerRight: normal = {right.x - up.x, right.y - up.y, right.z - up.z}; break;
    case NeonDissolveDirection::UpperRightToLowerLeft: normal = {-right.x - up.x, -right.y - up.y, -right.z - up.z}; break;
    case NeonDissolveDirection::TopToBottom: normal = {-up.x, -up.y, -up.z}; break;
    case NeonDissolveDirection::LeftToRight: normal = right; break;
    default: return fail("Unknown dissolve direction preset.");
    }
    if (!NormalizeNeonDirection(normal)) return fail("Dissolve camera axes do not define a scan plane.");
    Vector3 modelDirection{
        world.m[0][0] * normal.x + world.m[0][1] * normal.y + world.m[0][2] * normal.z,
        world.m[1][0] * normal.x + world.m[1][1] * normal.y + world.m[1][2] * normal.z,
        world.m[2][0] * normal.x + world.m[2][1] * normal.y + world.m[2][2] * normal.z};
    if (!NormalizeNeonDirection(modelDirection)) return fail("Model scan plane is degenerate.");
    output = modelDirection;
    if (error) error->clear();
    return true;
}

// Start-time CPU scan only. Positions match the GPU's four raw influences and row-vector
// palette multiplication, including source position.w. No GPU resource or skeleton is mutated.
inline bool ComputeNeonSkinnedProjectionBounds(const SkinnedModel& model, const Vector3& direction,
    float& scanMin, float& scanMax) {
    Vector3 unitDirection = direction;
    if (!NormalizeNeonDirection(unitDirection)) return false;
    const auto& vertices = model.GetAsset().modelData.vertices;
    const auto& influences = model.GetSkinCluster().GetInfluences();
    const auto& palette = model.GetSkinCluster().GetPalette();
    if (vertices.empty() || vertices.size() != influences.size() || palette.empty()) return false;
    float low = (std::numeric_limits<float>::max)(), high = (std::numeric_limits<float>::lowest)();
    for (size_t vertex = 0; vertex < vertices.size(); ++vertex) {
        const auto& source = vertices[vertex].position;
        if (!std::isfinite(source.x) || !std::isfinite(source.y) || !std::isfinite(source.z) || !std::isfinite(source.w)) return false;
        Vector3 skinned{};
        for (size_t influence = 0; influence < 4; ++influence) {
            const auto joint = influences[vertex].jointIndices[influence];
            const float weight = influences[vertex].weights[influence];
            if (joint < 0 || size_t(joint) >= palette.size() || !std::isfinite(weight) || weight < 0) return false;
            const auto& m = palette[size_t(joint)].skeletonSpaceMatrix;
            skinned.x += (source.x * m.m[0][0] + source.y * m.m[1][0] + source.z * m.m[2][0] + source.w * m.m[3][0]) * weight;
            skinned.y += (source.x * m.m[0][1] + source.y * m.m[1][1] + source.z * m.m[2][1] + source.w * m.m[3][1]) * weight;
            skinned.z += (source.x * m.m[0][2] + source.y * m.m[1][2] + source.z * m.m[2][2] + source.w * m.m[3][2]) * weight;
        }
        const float projection = skinned.x * unitDirection.x + skinned.y * unitDirection.y + skinned.z * unitDirection.z;
        if (!std::isfinite(projection)) return false;
        low = (std::min)(low, projection); high = (std::max)(high, projection);
    }
    const float padding = (std::max)(1.0e-5f, (std::max)(std::abs(low), std::abs(high)) * 1.0e-5f);
    scanMin = low - padding; scanMax = high + padding;
    return std::isfinite(scanMin) && std::isfinite(scanMax);
}

// CPU reference of the stable trilinear value noise used by NeonDissolve.hlsli.
inline uint32_t NeonDissolveHash(uint32_t x, uint32_t y, uint32_t z, uint32_t seed) {
    uint32_t h = x * 0x8da6b343u ^ y * 0xd8163841u ^ z * 0xcb1ab31fu ^ seed;
    h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;
    return h;
}

inline float NeonDissolveValueNoise(const Vector3& position, float scale, uint32_t seed) {
    if (!IsFiniteNeonPosition(position) || !std::isfinite(scale) || scale <= 0) return 0.5f;
    const auto component = [&](float x) { return (std::clamp)(x * scale, -1048576.0f, 1048576.0f); };
    const float x = component(position.x), y = component(position.y), z = component(position.z);
    const int32_t ix = int32_t(std::floor(x)), iy = int32_t(std::floor(y)), iz = int32_t(std::floor(z));
    const auto smooth = [](float f) { return f * f * (3.0f - 2.0f * f); };
    const float sx = smooth(x - float(ix)), sy = smooth(y - float(iy)), sz = smooth(z - float(iz));
    const auto lattice = [&](int32_t dx, int32_t dy, int32_t dz) {
        return float(NeonDissolveHash(uint32_t(ix + dx), uint32_t(iy + dy), uint32_t(iz + dz), seed) & 0x00ffffffu) / 16777215.0f;
    };
    const auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    const float a = lerp(lerp(lattice(0,0,0), lattice(1,0,0), sx), lerp(lattice(0,1,0), lattice(1,1,0), sx), sy);
    const float b = lerp(lerp(lattice(0,0,1), lattice(1,0,1), sx), lerp(lattice(0,1,1), lattice(1,1,1), sx), sy);
    return lerp(a, b, sz);
}

inline float EvaluateNeonDissolveSignedDistance(const Vector3& position, const NeonDissolveParams& params) {
    if (params.enabled == 0 || params.progress <= 0) return 1.0f;
    if (params.progress >= 1) return -1.0f;
    const float strength = params.noiseScale > 0 ? params.noiseStrength : 0;
    const float low = params.scanMin - strength * 0.5f;
    const float high = params.scanMax + strength * 0.5f;
    const float range = (std::max)(high - low, 1.0e-5f);
    const float threshold = low + params.progress * range;
    const float projected = position.x * params.direction.x + position.y * params.direction.y + position.z * params.direction.z;
    const float noise = params.noiseStrength > 0 && params.noiseScale > 0
        ? (NeonDissolveValueNoise(position, params.noiseScale, params.seed) - 0.5f) * params.noiseStrength : 0;
    return projected + noise - threshold;
}

} // namespace cg2
