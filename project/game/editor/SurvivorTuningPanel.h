#pragma once
#include "game/run/SurvivorSpawnConfig.h"
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
};

/// @brief 調整パネルで押されたこと。
struct TuningPanelResult {
    /// @brief 数値を変えた。呼び出し側は、自機と欠片の設定に反映し直す。
    bool changed = false;
    bool saveRequested = false;
    bool reloadRequested = false;
};

/// @brief 育つ戦車の試作の調整パネル（ImGui）を描く。熱・シールド・湧き方・経験値の曲線・部品の数値・スローの倍率を、その場で変える。
/// @param config 書き換える設定。変えた値はすぐに config に入る。
/// @param open 閉じるボタンで false になる。
/// @param pauseWhileOpen 開いている間ゲームを止めるか（パネルの中のチェックで切り替える）。
/// @note ImGui の無い構成では何もしない。
TuningPanelResult DrawTuningPanel(SpawnConfig& config, bool& open, bool& pauseWhileOpen, const std::string& status,
                                  const TuningSnapshot& snapshot);
} // namespace survivor
