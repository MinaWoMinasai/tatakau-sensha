#pragma once
#include "Struct.h"
#include <algorithm>
#include <cmath>

namespace stagesurface {
// Presentation only. Never pass this transform to Stage's collision builders.
struct Settings {
    bool enabled = false;
    bool floorEnabled = true;
    bool blocksEnabled = true;
    int wallStyle = 1; // 0: Phase 1 beveled tiles; 1: continuous rectilinear walls.
    bool exteriorEnabled = true;
    float exteriorHeight = 8.0f, exteriorGap = 2.0f, exteriorWidth = 2.0f;
    int stackLevels = 3;
    float wallLineWidth = 0.028f;
    bool floorLocalLight = true;
    myEngine::Vector4 floorLightColor{0.20f, 0.85f, 0.65f, 1.0f};
    float floorLocalIntensity = 24.0f, floorLocalRadius = 11.0f, floorLocalHeight = 3.0f;
    float floorDetail = 0.0f; // The generic sinusoidal detail produces bands on a large XY floor.
    myEngine::Vector4 floorColor{0.035f, 0.045f, 0.060f, 1.0f};
    float floorMetallic = 0.20f;
    float floorRoughness = 0.52f;
    float floorDepth = 1.10f; // Camera is on -Z; +Z is behind the XY gameplay plane.
    float floorPadding = 8.0f;
    float floorLightIntensity = 1.6f;
    float gridMinPixelWidth = 1.15f;
    myEngine::Vector4 blockColor{0.032f, 0.040f, 0.055f, 1.0f};
    myEngine::Vector4 damageBlockColor{0.140f, 0.035f, 0.045f, 1.0f};
    float blockMetallic = 0.30f;
    float blockRoughness = 0.56f;
    float blockHeight = 4.8f; // Full connected-wall height; isolated cells use one tier.
    float blockEmission = 0.65f;
    float blockLightIntensity = 2.2f;
    int sideLineDivisions = 1;
};

inline constexpr float kBevel = 0.14f;
inline constexpr float kGridClearance = 0.025f;
inline constexpr const char* kFloorModel = "__tank_surface_floor_xy";
inline constexpr const char* kBlockModel = "__tank_surface_beveled_block";
inline constexpr const char* kWallModel = "__tank_surface_rectilinear_wall";

inline float FiniteClamp(float value, float fallback, float low, float high)
{
    return std::isfinite(value) ? (std::clamp)(value, low, high) : fallback;
}

inline void Sanitize(Settings& settings)
{
    const Settings defaults;
    auto color = [&](myEngine::Vector4& value, const myEngine::Vector4& fallback) {
        value.x = FiniteClamp(value.x, fallback.x, 0.0f, 1.0f);
        value.y = FiniteClamp(value.y, fallback.y, 0.0f, 1.0f);
        value.z = FiniteClamp(value.z, fallback.z, 0.0f, 1.0f);
        value.w = 1.0f; // Opaque bodies must write real scene depth.
    };
    color(settings.floorColor, defaults.floorColor);
    color(settings.blockColor, defaults.blockColor);
    color(settings.damageBlockColor, defaults.damageBlockColor);
    color(settings.floorLightColor, defaults.floorLightColor);
    settings.wallStyle = (std::clamp)(settings.wallStyle, 0, 1);
    settings.stackLevels = (std::clamp)(settings.stackLevels, 1, 5);
    settings.exteriorHeight = FiniteClamp(settings.exteriorHeight, defaults.exteriorHeight, 1.0f, 12.0f);
    settings.exteriorGap = FiniteClamp(settings.exteriorGap, defaults.exteriorGap, 0.0f, 12.0f);
    // A wide rampart reaches past the screen edge, so no bare floor shows outside the arena.
    settings.exteriorWidth = FiniteClamp(settings.exteriorWidth, defaults.exteriorWidth, 0.5f, 64.0f);
    settings.wallLineWidth = FiniteClamp(settings.wallLineWidth, defaults.wallLineWidth, 0.008f, 0.08f);
    settings.floorLocalIntensity = FiniteClamp(settings.floorLocalIntensity, defaults.floorLocalIntensity, 0.0f, 80.0f);
    settings.floorLocalRadius = FiniteClamp(settings.floorLocalRadius, defaults.floorLocalRadius, 2.0f, 24.0f);
    settings.floorLocalHeight = FiniteClamp(settings.floorLocalHeight, defaults.floorLocalHeight, 0.5f, 8.0f);
    settings.floorDetail = FiniteClamp(settings.floorDetail, defaults.floorDetail, 0.0f, 0.2f);
    settings.floorMetallic = FiniteClamp(settings.floorMetallic, defaults.floorMetallic, 0.0f, 1.0f);
    settings.floorRoughness = FiniteClamp(settings.floorRoughness, defaults.floorRoughness, 0.15f, 1.0f);
    settings.floorDepth = FiniteClamp(settings.floorDepth, defaults.floorDepth, 1.02f, 8.0f);
    settings.floorPadding = FiniteClamp(settings.floorPadding, defaults.floorPadding, 0.0f, 64.0f);
    settings.floorLightIntensity = FiniteClamp(settings.floorLightIntensity, defaults.floorLightIntensity, 0.0f, 5.0f);
    settings.gridMinPixelWidth = FiniteClamp(settings.gridMinPixelWidth, defaults.gridMinPixelWidth, 0.5f, 2.0f);
    settings.blockMetallic = FiniteClamp(settings.blockMetallic, defaults.blockMetallic, 0.0f, 1.0f);
    settings.blockRoughness = FiniteClamp(settings.blockRoughness, defaults.blockRoughness, 0.15f, 1.0f);
    settings.blockHeight = FiniteClamp(settings.blockHeight, defaults.blockHeight, 0.5f, 6.0f);
    settings.blockEmission = FiniteClamp(settings.blockEmission, defaults.blockEmission, 0.0f, 2.0f);
    settings.blockLightIntensity = FiniteClamp(settings.blockLightIntensity, defaults.blockLightIntensity, 0.0f, 5.0f);
    settings.sideLineDivisions = (std::clamp)(settings.sideLineDivisions, 1, 8);
}

inline myEngine::Transform BlockTransform(const myEngine::Transform& gameplay, const Settings& settings)
{
    myEngine::Transform visual = gameplay;
    visual.scale.z *= settings.blockHeight * 0.5f;
    // Anchor the back to the floor. This changes only the render object, never AABB/OBB.
    visual.translate.z += settings.floorDepth - 0.03f - std::abs(visual.scale.z);
    return visual;
}

inline Settings Phase1Settings()
{
    Settings result;
    result.enabled = true;
    result.wallStyle = 0;
    result.exteriorEnabled = false;
    result.floorLocalLight = false;
    result.floorDetail = 0.0f;
    result.floorRoughness = 0.72f;
    result.blockHeight = 2.8f;
    result.blockColor = {0.085f, 0.105f, 0.140f, 1.0f};
    result.sideLineDivisions = 1;
    return result;
}
} // namespace stagesurface
