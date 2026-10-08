#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace cg2 {
// Values are also used by the procedural particle shaders. Ordinary emitters
// retain Triangle; only the combat presentation opts into the other silhouettes.
enum class NeonParticleShape : uint32_t { Triangle, Shard, Sliver, Spark, Flash, Ring };

inline float NeonParticleFade(float age, NeonParticleShape shape) {
    const float hold = shape == NeonParticleShape::Flash ? 0.0f :
        shape == NeonParticleShape::Ring ? 0.08f : shape == NeonParticleShape::Triangle ? 0.10f : 0.28f;
    const float t = std::clamp((age - hold) / (1.0f - hold), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
inline float NeonParticleDrag(NeonParticleShape shape) {
    return shape == NeonParticleShape::Shard || shape == NeonParticleShape::Sliver ? 2.8f : 0.0f;
}
// Unit silhouettes, with space for a halo inside the particle quad. Shared by
// the outline renderer and CPU tests; the SDF shader uses the same coordinates.
inline uint32_t NeonParticlePoints(NeonParticleShape shape, std::array<std::array<float, 2>, 4>& points) {
    if (shape == NeonParticleShape::Shard) {
        points = {{{0.0f, 0.95f}, {-0.42f, -0.40f}, {-0.12f, -0.62f}, {0.45f, -0.12f}}};
        return 4;
    }
    if (shape == NeonParticleShape::Sliver) {
        points = {{{0.12f, 1.0f}, {-0.17f, -0.50f}, {0.10f, -0.58f}, {0.0f, 0.0f}}};
        return 3;
    }
    points = {{{0.0f, 1.0f}, {-0.8660254f, -0.50f}, {0.8660254f, -0.50f}, {0.0f, 0.0f}}};
    return 3;
}
} // namespace cg2
