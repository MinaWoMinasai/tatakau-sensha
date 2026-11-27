#pragma once
#include "game/scene/IScene.h"
#include "DirectX/engine/common/DeveloperTools.h"

/// @brief 遊ぶ人が変えられる設定（計画書 p11 の R2）。save/settings.json に残る。
/// @note 自動確認（MYENGINE_HIDDEN_WINDOW=1）では、読みも書きもせずに初めの値を使う。MYENGINE_SETTINGS_FILE で場所を替えられる。
struct UserSettings {
    /// @brief 音楽・効果音の大きさ（0〜10）。
    int music = 7;
    int effects = 8;
    /// @brief 全画面か（開発用の構成では、初めは窓）。
    bool fullscreen = !myEngine::kDeveloperTools;
    /// @brief 画面のゆれ（0〜10）。
    int shake = 6;
    /// @brief 点滅を弱めるか。
    bool calm = false;
    /// @brief 画面の曲がり（0〜10）。
    int curve = 4;
    /// @brief 押さなくても撃つか。
    bool autoFire = true;
    /// @brief はじめての説明を終えたか。
    bool tutorialDone = false;
    /// @brief 一番長く生きた秒（結果の画面の白い印）。
    float bestSeconds = 0.0f;
};

namespace usersettings {
/// @brief 今の設定。最初に呼んだときに読み込む。
UserSettings& Get();
/// @brief 保存する。書けなければ何もしない（遊びは止めない）。
void Save();
/// @brief 窓の大きさなど、すぐ効く物を合わせる。
void ApplyWindow();
/// @brief 音楽・効果音・ゆれの倍率（初めの値で1）。
float MusicScale();
float EffectsScale();
float ShakeScale();
/// @brief 画面の曲がりを合わせる（初めの値で、元の設定のまま）。
void ApplyLook(IScene::ArenaLookSettings& look);
} // namespace usersettings
