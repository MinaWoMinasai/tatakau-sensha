#pragma once
#include <memory>
#include "SceneManager.h"

class Game {
public:
    bool Initialize();
    void Run();
    void Finalize();

private:
    void InitializeEngine();
    void InitializeImGui();
    void LoadResources();
    void MainLoop();

private:
    std::unique_ptr<DirectXCommon> dxCommon_;
    std::unique_ptr<SrvManager> srvManager_;
    std::unique_ptr<SceneManager> sceneManager_;
};