#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace myEngine {
// この値は手続き的なパーティクルシェーダーでも使う。通常のエミッターは
// Triangleのままで、戦闘演出だけが他のシルエットを明示的に使う。
enum class NeonParticleShape : uint32_t {
    Triangle,
    Shard,
    Sliver,
    Spark,
    Flash,
    Ring
};

/// @brief パーティクルの形ごとの寿命に応じたフェード値（0..1）を返す。
inline float NeonParticleFade(float age, NeonParticleShape shape)
{
    const float hold = shape == NeonParticleShape::Flash      ? 0.0f
                       : shape == NeonParticleShape::Ring     ? 0.08f
                       : shape == NeonParticleShape::Triangle ? 0.10f
                                                              : 0.28f;
    const float t = std::clamp((age - hold) / (1.0f - hold), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/// @brief パーティクルの形ごとの空気抵抗の強さを返す。
inline float NeonParticleDrag(NeonParticleShape shape)
{
    return shape == NeonParticleShape::Shard || shape == NeonParticleShape::Sliver ? 2.8f : 0.0f;
}

// 単位サイズのシルエット。パーティクルの四角形内にハロー用の余白を持つ。
// 輪郭描画とCPUテストで共有し、SDFシェーダーも同じ座標を使う。
inline uint32_t NeonParticlePoints(NeonParticleShape shape, std::array<std::array<float, 2>, 4>& points)
{
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
} // namespace myEngine
