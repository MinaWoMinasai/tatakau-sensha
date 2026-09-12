#pragma once
#include <cmath>

namespace ink::reticle {
inline float ClampFinite(float value, float low, float high, float fallback) {
    if (!std::isfinite(value)) return fallback;
    return value < low ? low : value > high ? high : value;
}

// Angular half-width projected by a perspective camera. This is CG2 geometry,
// not an assertion about the original game's camera or HUD scaling constants.
inline float ProjectSpreadPixels(float spreadDegrees, float verticalFovRadians,
                                 float viewportHeight) {
    if (!std::isfinite(spreadDegrees) || !std::isfinite(verticalFovRadians) ||
        !std::isfinite(viewportHeight) || spreadDegrees <= 0.0f ||
        viewportHeight <= 0.0f || verticalFovRadians <= 0.0f ||
        verticalFovRadians >= 3.14159265359f) return 0.0f;
    constexpr float RadiansPerDegree = 0.01745329251994329577f;
    const float angle = ClampFinite(spreadDegrees, 0.0f, 80.0f, 0.0f);
    const float focalPixels = viewportHeight * 0.5f / std::tan(verticalFovRadians * 0.5f);
    const float projected = focalPixels * std::tan(angle * RadiansPerDegree);
    return std::isfinite(projected) ? projected : 0.0f;
}

struct ChargeArcs { float first = 0.0f, second = 0.0f; };
inline ChargeArcs SplitCharge(float charge, float firstChargeRatio) {
    charge = ClampFinite(charge, 0.0f, 1.0f, 0.0f);
    const float threshold = ClampFinite(firstChargeRatio, 0.01f, 0.99f, 0.416667f);
    return {ClampFinite(charge / threshold, 0.0f, 1.0f, 0.0f),
        ClampFinite((charge - threshold) / (1.0f - threshold), 0.0f, 1.0f, 0.0f)};
}
} // namespace ink::reticle
