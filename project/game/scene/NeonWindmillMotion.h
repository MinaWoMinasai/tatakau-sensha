#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace neonwindmill {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kLoopSeconds = 16.0f;

/// @brief 生成した形状と動きで共有する、モデル座標の点を表す。
struct Point {
    float x = 0, y = 0, z = 0;
};
/// @brief 元動画の観察をもとにした演出区間を表す。
enum class Phase {
    Orbit,
    Approach,
    Lock,
    Recognize,
    Surge,
    Reset
};
/// @brief 秒単位の時刻から求めた、顔と四本の手の配置を表す。
struct Pose {
    Phase phase = Phase::Orbit;
    float seconds = 0, angle = 0, recognition = 0, visibility = 1;
    Point center{};
    std::array<Point, 4> hands{};
};
/// @brief 区間の両端で速度がゼロになる補間値を返す。
inline float Smooth(float begin, float end, float value)
{
    const float t = (std::clamp)((value - begin) / (end - begin), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
/// @brief 再生時刻を16秒周期に折り返し、四本の手を90度ずつ離して配置する。
inline Pose Evaluate(double seconds)
{
    if (!std::isfinite(seconds))
        seconds = 0;
    Pose pose;
    pose.seconds = static_cast<float>(std::fmod(seconds, static_cast<double>(kLoopSeconds)));
    if (pose.seconds < 0)
        pose.seconds += kLoopSeconds;
    const float t = pose.seconds;
    pose.phase = t < 7       ? Phase::Orbit
                 : t < 12    ? Phase::Approach
                 : t < 13.4f ? Phase::Lock
                 : t < 14.2f ? Phase::Recognize
                 : t < 14.9f ? Phase::Surge
                             : Phase::Reset;
    // 最初は約1.27秒で一周。減速中は角速度を積分して、停止境界を連続にする。
    const float omega = 2 * kPi / 1.27f;
    if (t < 12)
        pose.angle = omega * t;
    else if (t < 13.4f) {
        const float u = (t - 12) / 1.4f;
        pose.angle = omega * (12 + 1.4f * (u - u * u * u + 0.5f * u * u * u * u));
    } else
        pose.angle = omega * 12.7f;
    pose.recognition = Smooth(13.4f, 13.85f, t);
    pose.center = {0, 3.3f + 0.12f * std::sin(t * 2.1f), -1.5f * Smooth(7, 12, t) - 2.6f * Smooth(14.2f, 14.9f, t)};
    if (t >= 14.9f)
        pose.visibility = 1 - Smooth(14.9f, 15.3f, t);
    if (t < 0.35f)
        pose.visibility = Smooth(0, 0.35f, t);
    for (std::size_t i = 0; i < pose.hands.size(); ++i) {
        const float a = pose.angle + static_cast<float>(i) * kPi * 0.5f;
        pose.hands[i] = {pose.center.x + std::cos(a) * 1.65f, pose.center.y + std::sin(a) * 1.65f, pose.center.z};
    }
    return pose;
}
} // namespace neonwindmill
