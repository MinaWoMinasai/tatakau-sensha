#pragma once

enum class GameStartMode {
	Normal,
	Tutorial,
};

namespace GameStartSession {

inline GameStartMode mode = GameStartMode::Normal;

inline void SetMode(GameStartMode newMode)
{
	mode = newMode;
}

inline GameStartMode GetMode()
{
	return mode;
}

} // namespace GameStartSession
