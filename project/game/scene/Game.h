#pragma once
#include <memory>
#include "../runtime/GameProject.h"
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
    bool Initialize(const GameProjectCommandLineOptions& projectOptions = {});
    void Run();
    void Finalize();

    const GameProject& GetActiveProject() const { return activeProject_; }
    bool IsActiveProjectLoadedFromFallback() const { return activeProjectLoadedFromFallback_; }
    bool IsStartupSceneUsingFallback() const { return startupSceneUsedFallback_; }
    const std::string& GetResolvedStartupScene() const { return resolvedStartupScene_; }

private:
    void LoadActiveProject(const GameProjectCommandLineOptions& projectOptions);
    bool ResolveStartupScene();
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

    GameProject activeProject_;
    bool activeProjectLoadedFromFallback_ = false;
    bool startupSceneUsedFallback_ = false;
    std::string resolvedStartupScene_ = "TITLE";
};
