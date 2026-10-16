#pragma once
#include <algorithm>
#include <cmath>

namespace arenacamera {
// 見下ろし戦闘カメラ。旧カメラ（FOV 0.45rad・距離55）とXY平面上の可視範囲を一致させたまま
// 画角を広げ、壁や段積みの側面が画面外側へ倒れ込む強いパースにする（表示のみ、衝突・照準の基準平面は不変）。
inline constexpr float kLegacyFovY = 0.45f;
inline constexpr float kLegacyDistance = 55.0f;
inline constexpr float kFovY = 1.02f;
inline constexpr float kAspect = 16.0f / 9.0f;
// 床の見える範囲がアリーナの外壁をはみ出してよい量（ワールド単位）。
inline constexpr float kOverscan = 1.0f;

/// @brief 旧カメラと同じXY可視高さになるカメラ距離（正の値）。
inline float Distance()
{
    return kLegacyDistance * std::tan(kLegacyFovY * 0.5f) / std::tan(kFovY * 0.5f);
}

/// @brief カメラのZ座標。ゲーム平面はZ=0、カメラは-Z側から+Zを向く。
inline float CameraZ()
{
    return -Distance();
}

/// @brief 通常距離で床に見える範囲の半分（高さ方向）。
inline float HalfViewHeight()
{
    return Distance() * std::tan(kFovY * 0.5f);
}

/// @brief 有効なブロックの外枠（アリーナの壁の外側）を求める。マップ上の配置物は含めない。
template <class Blocks>
inline bool ArenaBounds(const Blocks& blocks, float& minX, float& minY, float& maxX, float& maxY)
{
    minX = minY = 1.0e6f;
    maxX = maxY = -1.0e6f;
    for (const auto& row : blocks)
        for (const auto& block : row) {
            if (!block.isActive || block.isLevelObject) continue;
            minX = (std::min)(minX, block.aabb.min.x); minY = (std::min)(minY, block.aabb.min.y);
            maxX = (std::max)(maxX, block.aabb.max.x); maxY = (std::max)(maxY, block.aabb.max.y);
        }
    return minX < maxX && minY < maxY;
}

/// @brief 床の見える範囲が外壁を少し越える程度に注視点を制限する。アリーナが画面より狭ければ中央に置く。
inline float ClampFocus(float value, float low, float high, float halfView)
{
    float outMin = low + halfView - kOverscan;
    float outMax = high - halfView + kOverscan;
    if (outMin > outMax) outMin = outMax = (low + high) * 0.5f;
    return (std::clamp)(value, outMin, outMax);
}
} // namespace arenacamera
