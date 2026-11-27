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

/// @brief 色に明るさを掛ける（上限では切らない）。
inline myEngine::Vector4 Color(const Look& look, const myEngine::Vector4& color, float boost = 1.0f)
{
    const float scale = look.effectBrightness * boost;
    return {color.x * scale, color.y * scale, color.z * scale, color.w};
}

/// @brief 出続ける物（衛星・随伴機・Q の輪・火炎など）の色：一番明るい成分を 1 にそろえる（瞬間の光より明るくしない）。
inline myEngine::Vector4 Steady(const myEngine::Vector4& color)
{
    const float peak = (std::max)({color.x, color.y, color.z, 0.0001f});
    const float scale = (std::min)(peak, 1.0f) / peak;
    return {color.x * scale, color.y * scale, color.z * scale, color.w};
}

/// @brief 近いにじみの色：敵の近いにじみと同じく、色を濃くする（外ほど色が濃く見えるように）。強さは敵の近いにじみに対する倍。
inline myEngine::Vector4 GlowColor(const Look& look, const myEngine::Vector4& color, float boost = 1.0f)
{
    const float peak = (std::max)({color.x, color.y, color.z, 0.0001f});
    const float scale = (std::min)(peak, 1.0f) * look.nearGlow * look.effectGlow * look.effectBrightness * boost;
    return {std::pow(color.x / peak, 1.7f) * scale, std::pow(color.y / peak, 1.7f) * scale, std::pow(color.z / peak, 1.7f) * scale, color.w};
}

/// @brief 敵と同じ近いにじみの輪郭（線と同じ形の広い光）。
inline myEngine::NeonContourStyle GlowStyle()
{
    myEngine::NeonContourStyle glow = myEngine::ActorNeonContourStyle();
    glow.coreWidthRatio = 0.08f;
    glow.coreIntensity = 1.0f;
    glow.coreWhiteMix = 0.0f;
    glow.shoulderIntensity = 0.42f;
    glow.haloWidthScale = 1.4f;
    glow.haloIntensity = 0.3f;
    glow.haloAlpha = 0.4f;
    glow.roundCaps = false;
    return glow;
}

/// @brief 生まれた瞬間だけ強くする倍（age は生まれてからの秒）。
inline float Birth(const Look& look, float age)
{
    const float t = (std::clamp)(1.0f - age / 0.1f, 0.0f, 1.0f);
    return 1.0f + (look.effectBirth - 1.0f) * t * t;
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
                   const myEngine::Vector4& color, float phase, int sides, const myEngine::Vector3& cameraForward, float boost = 1.0f)
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
    if (look.effectGlow > 0.0f && look.nearGlow > 0.0f) {
        renderer.QueueContourPolygon(points, static_cast<uint32_t>(count), look.nearGlowRadius / 0.7f, GlowColor(look, color, boost), cameraForward,
                                     GlowStyle());
    }
    renderer.QueueContourPolygon(points, static_cast<uint32_t>(count), width * look.effectLine, Color(look, color, boost), cameraForward, Style(look));
}

/// @brief 1本の管の線を描く。
inline void Line(myEngine::NeonGridRenderer& renderer, const Look& look, const myEngine::Vector3& a, const myEngine::Vector3& b, float width,
                 const myEngine::Vector4& color, const myEngine::Vector3& cameraForward, float boost = 1.0f, float reachScale = 1.0f,
                 float glowScale = 1.0f)
{
    if (color.w <= 0.005f) {
        return;
    }
    if (look.effectGlow > 0.0f && look.nearGlow > 0.0f) {
        // 線の近いにじみは、線に沿った楕円の光にする（端が切れて見えない）。
        const myEngine::Vector3 half = (b - a) * 0.5f;
        const float length = myEngine::Length(half);
        if (length > 0.0001f) {
            const myEngine::Vector3 along = half / length;
            const myEngine::Vector3 side = myEngine::Cross(cameraForward, along);
            const float reach = look.nearGlowRadius * reachScale;
            // 楕円は中心の一点だけが明るいので、敵の線の帯より強めにする。
            const myEngine::Vector4 glow = GlowColor(look, color, boost * 2.2f * glowScale);
            renderer.QueueSoftEllipse((a + b) * 0.5f, along * (length + reach), side * reach, glow);
        }
    }
    renderer.QueueContourLine(a, b, width * look.effectLine, Color(look, color, boost), cameraForward, Style(look));
}
} // namespace tubefx
