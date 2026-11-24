#pragma once
#include "game/run/SurvivorSpawnConfig.h"
#include <array>
#include <string>

namespace survivor {
/// @brief 調整パネルの上に出す、今の区画の様子。
struct TuningSnapshot {
    float elapsed = 0.0f;
    int level = 1;
    int exp = 0;
    int alive = 0;
    float fps = 0.0f;
    float heat = 0.0f;
    float heatMax = 100.0f;
    int overheats = 0;
    /// @brief 無限の場（計画書 p8 の G2）：今の湧き（大群を含む）、敵の HP と速さの倍率、保存先。
    float growthInterval = 1.0f;
    float growthCount = 0.0f;
    float growthMaxAlive = 0.0f;
    float enemyHpScale = 1.0f;
    float enemySpeedScale = 1.0f;
    std::string configPath;
    /// @brief このランで選んだレベルアップの数と、まだ選んでいない数。自機のならした速さ。湧いた向きの数（前・横・後ろ・向きなし）。
    int levelUps = 0;
    int pendingLevelUps = 0;
    float moveSpeed = 0.0f;
    std::array<int, 4> spawnSectors{};
};

/// @brief 調整パネルで押されたこと。
struct TuningPanelResult {
    /// @brief 数値を変えた。呼び出し側は、自機と欠片の設定に反映し直す。
    bool changed = false;
    bool saveRequested = false;
    bool reloadRequested = false;
    /// @brief 時間割の出来事を足した・消した・時刻を変えた。呼び出し側は並べ直し、過ぎた出来事を済んだことにする。
    bool timelineChanged = false;
};

/// @brief 育つ戦車の試作の調整パネル（ImGui）を描く。熱・シールド・湧き方・経験値の曲線・部品の数値・スローの倍率を、その場で変える。
/// @param config 書き換える設定。変えた値はすぐに config に入る。
/// @param open 閉じるボタンで false になる。
/// @param pauseWhileOpen 開いている間ゲームを止めるか（パネルの中のチェックで切り替える）。
/// @note ImGui の無い構成では何もしない。
TuningPanelResult DrawTuningPanel(SpawnConfig& config, bool& open, bool& pauseWhileOpen, const std::string& status,
                                  const TuningSnapshot& snapshot);
} // namespace survivor
