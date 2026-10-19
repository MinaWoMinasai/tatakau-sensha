#define DIRECTINPUT_VERSION 0x0800
#include "Game.h"
#include "game/runtime/ApplicationMain.h"

#include <Windows.h>

/// @brief Windowsアプリの入口としてゲームを起動し、終了コードを返す。
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    // ゲーム固有の処理（フック）は、RunApplicationから戻るまで有効にしておく。
    Game gameHooks;
    return RunApplication(gameHooks);
}
