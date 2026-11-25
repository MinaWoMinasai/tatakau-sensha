#pragma once
#include <algorithm>
#include <cmath>
#include "game/render/neon/NeonGridRenderer.h"
#include "game/session/state/NeonPresentationState.h"

/// @brief 爆発・破片・副武器を、敵や自機と同じネオン管の線で描くための部品（計画書 p10 の16章の M3）。
/// 光り方は敵の管と同じ芯とにじみ。色は一番明るい成分をそろえてから明るさを掛ける（白く飛ばさない）。
namespace tubefx {
using Look = gameplay::NeonPresentationState::RunNeonLook;

inline myEngine::NeonContourStyle Style(const Look& look)
{
    myEngine::NeonContourStyle style = myEngine::ActorNeonContourStyle();
    if (look.enemyCoreWhite >= 0.0f) {
        style.coreWhiteMix = look.enemyCoreWhite;
        style.coreIntensity = look.enemyCoreIntensity;
        style.haloIntensity = look.enemyHaloIntensity;
        style.haloWidthScale = look.enemyHaloWidth;
    }
    style.roundCaps = false;
    return style;
}

/// @brief 色の一番明るい成分を 1 にそろえて、明るさを掛ける。淡い色（成分が 1 より小さい）は、その分だけ暗いまま。
inline myEngine::Vector4 Color(const Look& look, const myEngine::Vector4& color)
{
    const float peak = (std::max)({color.x, color.y, color.z, 0.0001f});
    const float scale = (std::min)(peak, 1.0f) / peak * look.effectBrightness;
    return {color.x * scale, color.y * scale, color.z * scale, color.w};
}

/// @brief 自機のまわりの光を薄くする割合（自機から離れていれば 1）。
inline float Clear(const Look& look, const myEngine::Vector3& position)
{
    if (look.effectClearRadius <= 0.0f) {
        return 1.0f;
    }
    const float dx = position.x - look.effectClearCenter.x, dy = position.y - look.effectClearCenter.y;
    const float t = (std::clamp)(std::sqrt(dx * dx + dy * dy) / look.effectClearRadius * 1.6f - 0.6f, 0.0f, 1.0f);
    return look.effectClearAlpha + (1.0f - look.effectClearAlpha) * t * t * (3.0f - 2.0f * t);
}

/// @brief 床と平行な輪（または多角形）を1本の管で描く。
inline void Circle(myEngine::NeonGridRenderer& renderer, const Look& look, const myEngine::Vector3& center, float radius, float width,
                   const myEngine::Vector4& color, float phase, int sides, const myEngine::Vector3& cameraForward)
{
    if (radius <= 0.01f || color.w <= 0.005f) {
        return;
    }
    myEngine::Vector3 points[48]{};
    const int count = (std::clamp)(sides, 3, 48);
    for (int i = 0; i < count; ++i) {
        const float angle = phase + 6.2831853f * static_cast<float>(i) / static_cast<float>(count);
        points[i] = center + myEngine::Vector3{std::cos(angle) * radius, std::sin(angle) * radius, -0.3f};
    }
    renderer.QueueContourPolygon(points, static_cast<uint32_t>(count), width * look.effectLine, Color(look, color), cameraForward, Style(look));
}

/// @brief 1本の管の線を描く。
inline void Line(myEngine::NeonGridRenderer& renderer, const Look& look, const myEngine::Vector3& a, const myEngine::Vector3& b, float width,
                 const myEngine::Vector4& color, const myEngine::Vector3& cameraForward)
{
    if (color.w <= 0.005f) {
        return;
    }
    renderer.QueueContourLine(a, b, width * look.effectLine, Color(look, color), cameraForward, Style(look));
}
} // namespace tubefx
