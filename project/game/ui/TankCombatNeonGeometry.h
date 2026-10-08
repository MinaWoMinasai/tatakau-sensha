#pragma once
#include "NeonGridRenderer.h"
#include <algorithm>
#include <cmath>

// The game world and reward preview both call these geometry routines. They
// only append vertices: no actors, global random source or camera is consulted.
namespace tankneon {
/// @brief 機体輪郭を後で処理するために予約する。
inline void QueueBodyOutline(cg2::NeonGridRenderer& renderer, const cg2::Vector3& center, float radius, float lineWidth, int segments,
                             float rotation, const cg2::Vector2& scale, const cg2::Vector4& color, const cg2::Vector3& cameraRight,
                             const cg2::Vector3& cameraUp, bool luminousContour = false)
{
    constexpr float tau = 6.28318530718f;
    segments = (std::clamp)(segments, 3, 48);
    if (luminousContour) {
        cg2::Vector3 points[48]{};
        for (int i = 0; i < segments; ++i) {
            const float angle = rotation + static_cast<float>(i) * tau / static_cast<float>(segments);
            points[i] = center + cameraRight * (std::cos(angle) * radius * scale.x)
                               + cameraUp * (std::sin(angle) * radius * scale.y);
        }
        renderer.QueueContourPolygon(points, static_cast<uint32_t>(segments), lineWidth, color, cg2::Cross(cameraRight, cameraUp), cg2::ActorNeonContourStyle());
        return;
    }
    cg2::Vector3 previous{};
    for (int i = 0; i <= segments; ++i) {
        const float angle = rotation + static_cast<float>(i % segments) * tau / static_cast<float>(segments);
        const cg2::Vector3 current =
            center + cameraRight * (std::cos(angle) * radius * scale.x) + cameraUp * (std::sin(angle) * radius * scale.y);
        if (i > 0)
            renderer.QueueLine(previous, current, lineWidth, color);
        previous = current;
    }
}
/// @brief 砲塔輪郭を後で処理するために予約する。
inline void QueueBarrelOutline(cg2::NeonGridRenderer& renderer, const cg2::Vector3& base, const cg2::Vector3& tip,
                               const cg2::Vector3& right, float halfWidth, float lineWidth, const cg2::Vector4& color, bool trapezoid,
                               bool luminousContour = false, const cg2::Vector3& cameraForward = {0, 0, 1})
{
    const float baseHalf = halfWidth * (trapezoid ? 1.28f : 1.0f), tipHalf = halfWidth * (trapezoid ? 0.72f : 1.0f);
    if (luminousContour) {
        const cg2::Vector3 points[] = {base - right * baseHalf, tip - right * tipHalf,
                                     tip + right * tipHalf, base + right * baseHalf};
        renderer.QueueContourPolygon(points, 4, lineWidth, color, cameraForward, cg2::ActorNeonContourStyle());
        return;
    }
    renderer.QueueLine(base - right * baseHalf, tip - right * tipHalf, lineWidth, color);
    renderer.QueueLine(tip - right * tipHalf, tip + right * tipHalf, lineWidth, color);
    renderer.QueueLine(tip + right * tipHalf, base + right * baseHalf, lineWidth, color);
    renderer.QueueLine(base + right * baseHalf, base - right * baseHalf, lineWidth, color);
}
/// @brief 近接攻撃の刃の輪郭・発光・形状を指定する。
struct BladeStyle {
    float outerWidth = 2.8f, haloWidth = 1.0f, coreWidth = 0.24f;
};
/// @brief 近接攻撃刃を後で処理するために予約する。
inline void QueueMeleeBlade(cg2::NeonGridRenderer& renderer, const cg2::Vector3& hilt, const cg2::Vector3& direction, float length,
                            float width, const cg2::Vector4& color, float coreAlpha, const BladeStyle& style)
{
    const cg2::Vector3 tip = hilt + direction * length;
    cg2::Vector4 outer{color.x * 1.20f, color.y * 1.20f, color.z * 1.20f, color.w * 0.30f};
    renderer.QueueLine(hilt, tip, width * style.outerWidth, outer);
    cg2::Vector4 halo{color.x * 1.35f, color.y * 1.35f, color.z * 1.35f, color.w * 0.78f};
    renderer.QueueLine(hilt + direction * (length * 0.03f), tip, width * style.haloWidth, halo);
    renderer.QueueLine(hilt + direction * (length * 0.08f), tip, width * style.coreWidth, {1, 1, 1, color.w * coreAlpha});
    const cg2::Vector3 right{-direction.y, direction.x, 0};
    cg2::Vector4 guard = color;
    guard.w *= 0.62f;
    renderer.QueueLine(hilt - right * (width * 1.9f), hilt + right * (width * 1.9f), width * 0.42f, guard);
    cg2::Vector4 tipColor = color;
    tipColor.w *= 0.58f;
    renderer.QueueLine(tip - right * (width * 0.90f), tip + right * (width * 0.90f), width * 0.34f, tipColor);
}
} // namespace tankneon
