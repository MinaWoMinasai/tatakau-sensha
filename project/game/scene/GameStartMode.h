#pragma once
#include "DeveloperTools.h"

/// @brief ゲームの開始方法。
enum class GameStartMode {
    Normal,
    Tutorial,
};

namespace GameStartSession {

inline GameStartMode mode = GameStartMode::Normal;
// ボス戦へすぐ移る（F7）は、講評会の Release でも使う。
inline bool developerBossStartRequested = false;
// 設定の「説明をもう一度」（計画書 p11 の R3）。
inline bool tutorialRequested = false;

/// @brief 次の出撃を、はじめての説明から始める。
inline void RequestTutorial()
{
    tutorialRequested = true;
}

/// @brief 説明の要求を一度だけ取り出す。
inline bool ConsumeTutorialRequest()
{
    const bool requested = tutorialRequested;
    tutorialRequested = false;
    return requested;
}

/// @brief 次の遠征だけをボス確認から開始する。タイトルの背景デモには消費させない。
inline void RequestDeveloperBossStart()
{
    developerBossStartRequested = true;
}

/// @brief ボス確認の開始要求を一度だけ取り出す。
inline bool ConsumeDeveloperBossStart()
{
    const bool requested = developerBossStartRequested;
    developerBossStartRequested = false;
    return requested;
}

/// @brief 方式を設定する。
inline void SetMode(GameStartMode newMode)
{
    mode = newMode;
    developerBossStartRequested = false;
}

/// @brief 方式を返す。
inline GameStartMode GetMode()
{
    return mode;
}

} // namespace GameStartSession
