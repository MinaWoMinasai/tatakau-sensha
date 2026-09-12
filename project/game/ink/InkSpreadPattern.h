#pragma once
#include "InkTypes.h"
#include <cmath>

namespace ink {

// Shooter spread is a signed horizontal angular deviation, not a circular cone.
// Bias controls the distribution: the median absolute deviation is spread*bias.
// Source and integration notes: docs/ink_shooter_phase3_shooting_research.md.
// Supply independent uniform values; this helper never advances random state.
inline float SampleSignedSpreadDegrees(float uniformMagnitude, float uniformSign,
                                      float spreadDegrees, float bias) {
    if (!std::isfinite(uniformMagnitude) || !std::isfinite(uniformSign) ||
        !std::isfinite(spreadDegrees) || !std::isfinite(bias) ||
        spreadDegrees <= 0.0f || bias <= 0.0f) return 0.0f;

    const float magnitude = uniformMagnitude < 0.0f ? 0.0f :
                            (uniformMagnitude > 1.0f ? 1.0f : uniformMagnitude);
    // The game values used here are <=0.5. Handle the full mathematical domain
    // [0,1] explicitly, including bias=1 where log(bias)=0 and pow(0,0) is unclear.
    const float fraction = bias >= 1.0f ? 1.0f :
        std::pow(magnitude, std::log(bias) / std::log(0.5f));
    const float degrees = spreadDegrees * fraction;
    return uniformSign < 0.5f ? -degrees : degrees;
}

// Rotate in the aim/right plane so the angular width does not shrink when
// looking up or down. A world-Y yaw rotation would narrow it at steep pitches.
// Gravity and player motion belong to projectile integration, not this sampler.
inline Vec3 PerturbHorizontal(Vec3 direction, float deltaDegrees) {
    if (!std::isfinite(direction.x) || !std::isfinite(direction.y) ||
        !std::isfinite(direction.z)) return {};
    const Vec3 forward = Normalize(direction);
    if (Length(forward) < 0.5f) return {};
    if (!std::isfinite(deltaDegrees)) return forward;

    const float horizontalLength = std::hypot(forward.x, forward.z);
    // Exactly vertical aim has no unique horizontal right. Keep a stable axis.
    const Vec3 right = horizontalLength > 0.0f ?
        Vec3{forward.z / horizontalLength, 0.0f, -forward.x / horizontalLength} :
        Vec3{1.0f, 0.0f, 0.0f};
    constexpr float RadiansPerDegree = 0.01745329251994329577f;
    const float radians = deltaDegrees * RadiansPerDegree;
    return Normalize(forward * std::cos(radians) + right * std::sin(radians));
}

} // namespace ink
