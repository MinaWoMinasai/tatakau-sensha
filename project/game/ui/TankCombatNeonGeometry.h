#pragma once
#include "game/render/neon/NeonGridRenderer.h"
#include <algorithm>
#include <cmath>

// ゲームワールドと報酬プレビューの両方がこの形状処理を呼ぶ。頂点を追加するだけで、
// アクター、全体の乱数、カメラは参照しない。
namespace tankneon {
/// @brief 機体の輪郭を描画待ちに積む。
inline void QueueBodyOutline(myEngine::NeonGridRenderer& renderer, const myEngine::Vector3& center, float radius, float lineWidth,
                             int segments, float rotation, const myEngine::Vector2& scale, const myEngine::Vector4& color,
                             const myEngine::Vector3& cameraRight, const myEngine::Vector3& cameraUp, bool luminousContour = false)
{
    constexpr float tau = 6.28318530718f;
    segments = (std::clamp)(segments, 3, 48);
    if (luminousContour) {
        myEngine::Vector3 points[48]{};
        for (int i = 0; i < segments; ++i) {
            const float angle = rotation + static_cast<float>(i) * tau / static_cast<float>(segments);
            points[i] = center + cameraRight * (std::cos(angle) * radius * scale.x) + cameraUp * (std::sin(angle) * radius * scale.y);
        }
        renderer.QueueContourPolygon(points, static_cast<uint32_t>(segments), lineWidth, color, myEngine::Cross(cameraRight, cameraUp),
                                     myEngine::ActorNeonContourStyle());
        return;
    }
    myEngine::Vector3 previous{};
    for (int i = 0; i <= segments; ++i) {
        const float angle = rotation + static_cast<float>(i % segments) * tau / static_cast<float>(segments);
        const myEngine::Vector3 current =
            center + cameraRight * (std::cos(angle) * radius * scale.x) + cameraUp * (std::sin(angle) * radius * scale.y);
        if (i > 0) {
            renderer.QueueLine(previous, current, lineWidth, color);
        }
        previous = current;
    }
}

/// @brief 砲身の輪郭を描画待ちに積む。
inline void QueueBarrelOutline(myEngine::NeonGridRenderer& renderer, const myEngine::Vector3& base, const myEngine::Vector3& tip,
                               const myEngine::Vector3& right, float halfWidth, float lineWidth, const myEngine::Vector4& color,
                               bool trapezoid, bool luminousContour = false, const myEngine::Vector3& cameraForward = {0, 0, 1})
{
    const float baseHalf = halfWidth * (trapezoid ? 1.28f : 1.0f), tipHalf = halfWidth * (trapezoid ? 0.72f : 1.0f);
    if (luminousContour) {
        const myEngine::Vector3 points[] = {base - right * baseHalf, tip - right * tipHalf, tip + right * tipHalf, base + right * baseHalf};
        renderer.QueueContourPolygon(points, 4, lineWidth, color, cameraForward, myEngine::ActorNeonContourStyle());
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

/// @brief 近接攻撃の刃を描画待ちに積む。
inline void QueueMeleeBlade(myEngine::NeonGridRenderer& renderer, const myEngine::Vector3& hilt, const myEngine::Vector3& direction,
                            float length, float width, const myEngine::Vector4& color, float coreAlpha, const BladeStyle& style)
{
    const myEngine::Vector3 tip = hilt + direction * length;
    myEngine::Vector4 outer{color.x * 1.20f, color.y * 1.20f, color.z * 1.20f, color.w * 0.30f};
    renderer.QueueLine(hilt, tip, width * style.outerWidth, outer);
    myEngine::Vector4 halo{color.x * 1.35f, color.y * 1.35f, color.z * 1.35f, color.w * 0.78f};
    renderer.QueueLine(hilt + direction * (length * 0.03f), tip, width * style.haloWidth, halo);
    renderer.QueueLine(hilt + direction * (length * 0.08f), tip, width * style.coreWidth, {1, 1, 1, color.w * coreAlpha});
    const myEngine::Vector3 right{-direction.y, direction.x, 0};
    myEngine::Vector4 guard = color;
    guard.w *= 0.62f;
    renderer.QueueLine(hilt - right * (width * 1.9f), hilt + right * (width * 1.9f), width * 0.42f, guard);
    myEngine::Vector4 tipColor = color;
    tipColor.w *= 0.58f;
    renderer.QueueLine(tip - right * (width * 0.90f), tip + right * (width * 0.90f), width * 0.34f, tipColor);
}
} // namespace tankneon
