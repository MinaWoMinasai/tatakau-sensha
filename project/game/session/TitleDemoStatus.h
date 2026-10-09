#pragma once
#include <cstdint>

namespace gameplay {
/// @brief タイトル背景のデモの進行と表示用の集計値を表す。
struct TitleDemoStatus {
    int stage = 0;
    float stageSeconds = 0, totalSeconds = 0;
    uint32_t stagesVisitedMask = 0;
    int shots = 0, dashes = 0, kills = 0, rewards = 0, routes = 0, maxPlayerBullets = 0;
};
} // namespace gameplay
