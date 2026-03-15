#pragma once
#include <memory>
#include "SceneManager.h"
#include "RenderTexture.h"
#include "PostEffect.h"
#include "ShadowMap.h"
#include "GameScene.h"
#include "TitleScene.h"
#include "Bloom.h"
#include "Shadow.h"

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
    std::unique_ptr<RtvManager> rtvManager_;

    std::unique_ptr<Bloom> bloom_;
    std::unique_ptr<Shadow> shadow_;
};