#pragma once
#include "DeveloperTools.h"

enum class GameStartMode {
    Normal,
    Tutorial,
};

namespace GameStartSession {

inline GameStartMode mode = GameStartMode::Normal;
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
inline bool developerBossStartRequested = false;

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
#endif

/// @brief 方式を設定する。
inline void SetMode(GameStartMode newMode)
{
    mode = newMode;
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    developerBossStartRequested = false;
#endif
}

/// @brief 方式を返す。
inline GameStartMode GetMode()
{
    return mode;
}

} // namespace GameStartSession
