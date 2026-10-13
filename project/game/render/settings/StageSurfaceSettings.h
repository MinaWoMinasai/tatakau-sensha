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
    cg2::Vector4 floorColor{0.035f, 0.045f, 0.060f, 1.0f};
    float floorMetallic = 0.20f;
    float floorRoughness = 0.72f;
    float floorDepth = 1.10f; // Camera is on -Z; +Z is behind the XY gameplay plane.
    float floorPadding = 8.0f;
    float floorLightIntensity = 1.6f;
    float gridMinPixelWidth = 1.15f;
    cg2::Vector4 blockColor{0.085f, 0.105f, 0.140f, 1.0f};
    cg2::Vector4 damageBlockColor{0.140f, 0.035f, 0.045f, 1.0f};
    float blockMetallic = 0.30f;
    float blockRoughness = 0.56f;
    float blockHeight = 2.8f; // Full visual height of a unit map cell; XY stays unchanged.
    float blockEmission = 0.65f;
    float blockLightIntensity = 2.2f;
    int sideLineDivisions = 1;
};

inline constexpr float kBevel = 0.14f;
inline constexpr float kGridClearance = 0.025f;
inline constexpr const char* kFloorModel = "__tank_surface_floor_xy";
inline constexpr const char* kBlockModel = "__tank_surface_beveled_block";

inline float FiniteClamp(float value, float fallback, float low, float high) {
    return std::isfinite(value) ? (std::clamp)(value, low, high) : fallback;
}

inline void Sanitize(Settings& settings) {
    const Settings defaults;
    auto color = [&](cg2::Vector4& value, const cg2::Vector4& fallback) {
        value.x = FiniteClamp(value.x, fallback.x, 0.0f, 1.0f);
        value.y = FiniteClamp(value.y, fallback.y, 0.0f, 1.0f);
        value.z = FiniteClamp(value.z, fallback.z, 0.0f, 1.0f);
        value.w = 1.0f; // Opaque bodies must write real scene depth.
    };
    color(settings.floorColor, defaults.floorColor);
    color(settings.blockColor, defaults.blockColor);
    color(settings.damageBlockColor, defaults.damageBlockColor);
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

inline cg2::Transform BlockTransform(const cg2::Transform& gameplay, const Settings& settings) {
    cg2::Transform visual = gameplay;
    visual.scale.z *= settings.blockHeight * 0.5f;
    // Anchor the back to the floor. This changes only the render object, never AABB/OBB.
    visual.translate.z += settings.floorDepth - 0.03f - std::abs(visual.scale.z);
    return visual;
}
} // namespace stagesurface
