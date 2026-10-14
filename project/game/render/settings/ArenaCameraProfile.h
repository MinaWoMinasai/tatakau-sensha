#pragma once
#include <cmath>

namespace arenacamera {
// 見下ろし戦闘カメラ。旧カメラ（FOV 0.45rad・距離55）とXY平面上の可視範囲を一致させたまま
// 画角を広げ、壁や段積みの側面が画面外側へ倒れ込む強いパースにする（表示のみ、衝突・照準の基準平面は不変）。
inline constexpr float kLegacyFovY = 0.45f;
inline constexpr float kLegacyDistance = 55.0f;
inline constexpr float kFovY = 1.02f;

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
} // namespace arenacamera
