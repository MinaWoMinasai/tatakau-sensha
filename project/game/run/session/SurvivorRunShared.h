#pragma once
#include "Struct.h"
#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <string>

namespace gameplay::survivorrun {
// SurvivorRunController の実装を分けたファイル（SurvivorRun*.cpp）で共有する定数。

/// @brief 自動確認と遊んだ記録の出力先。
inline constexpr const char* kAutoTestDirectory = "generated/tank_survivor";
inline constexpr const char* kConfigPath = "resources/configs/survivor_proto.json";
inline constexpr const char* kContentPath = "resources/configs/expedition_content.json";
// 格子 45×30 全体を外壁で囲んだ部屋（約90×60）。画面の約2×2.4倍の広さ。
inline constexpr const char* kArenaMap = "resources/maps/survivor_arena.csv";
inline const myEngine::Vector3 kPlayerStart{44.0f, 28.0f, 0.0f};
// 外壁の内側で、敵を置いてよい範囲（ワールド座標）。
inline constexpr float kSpawnMinX = 3.0f, kSpawnMaxX = 85.0f, kSpawnMinY = 3.0f, kSpawnMaxY = 55.0f;
// 出口の候補。辺の中ほどの柱の外側（壁際）に置く。
inline const std::array<myEngine::Vector3, 4> kExitCandidates{
    {{44.0f, 52.0f, 0.0f}, {44.0f, 5.0f, 0.0f}, {6.0f, 28.0f, 0.0f}, {82.0f, 28.0f, 0.0f}}};

/// @brief 秒を「分:秒」の文字にする。
inline std::string Clock(float seconds)
{
    const int total = (std::max)(0, static_cast<int>(seconds));
    std::ostringstream text;
    text << total / 60 << ":" << std::setw(2) << std::setfill('0') << total % 60;
    return text.str();
}
} // namespace gameplay::survivorrun
