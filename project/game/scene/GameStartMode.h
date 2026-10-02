#pragma once

enum class GameStartMode {
    Normal,
    Tutorial,
};

namespace GameStartSession {

inline GameStartMode mode = GameStartMode::Normal;

/// @brief 方式を設定する。
inline void SetMode(GameStartMode newMode)
{
    mode = newMode;
}

/// @brief 方式を返す。
inline GameStartMode GetMode()
{
    return mode;
}

} // namespace GameStartSession
