#define DIRECTINPUT_VERSION 0x0800
#include "Game.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

    D3DResouceLeakCheaker leakCheck;

    Game game;
    if (!game.Initialize()) {
        return -1;
    }

    game.Run();
    game.Finalize();

    return 0;
}