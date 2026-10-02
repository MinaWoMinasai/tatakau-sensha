#pragma once
#include <memory>
#include "../runtime/GameProject.h"
#include "../runtime/IGameModule.h"
#include "SceneManager.h"
#include "RenderTexture.h"
#include "PostEffect.h"
#include "ShadowMap.h"
#include "GameScene.h"
#include "TitleScene.h"
#include "Bloom.h"
#include "Shadow.h"
#include "TrailStressFixture.h"

class Game {
public:
    bool Initialize(const GameProjectCommandLineOptions& projectOptions = {});
    void Run();
    void Finalize();

    const GameProject& GetActiveProject() const { return activeProject_; }
    bool IsActiveProjectLoadedFromFallback() const { return activeProjectLoadedFromFallback_; }
    bool IsGameModuleUsingFallback() const { return gameModuleUsedFallback_; }
    bool IsStartupSceneUsingFallback() const { return startupSceneUsedFallback_; }
    const std::string& GetResolvedStartupScene() const { return resolvedStartupScene_; }

private:
    void LoadActiveProject(const GameProjectCommandLineOptions& projectOptions);
    bool ConfigureGameModuleAndSceneFactory();
    bool ResolveStartupScene();
    void InitializeEngine();
    void InitializeImGui();
    void LoadResources();
    void MainLoop();

private:
    std::unique_ptr<cg2::DirectXCommon> dxCommon_;
    std::unique_ptr<cg2::SrvManager> srvManager_;
    std::unique_ptr<cg2::RtvManager> rtvManager_;

    std::unique_ptr<cg2::Bloom> bloom_;
    std::unique_ptr<cg2::Shadow> shadow_;
    std::unique_ptr<IGameModule> activeGameModule_;

    GameProject activeProject_;
    bool activeProjectLoadedFromFallback_ = false;
    bool gameModuleUsedFallback_ = false;
    bool startupSceneUsedFallback_ = false;
    std::string resolvedStartupScene_ = "TITLE";
    bool imguiInitialized_ = false;
    std::unique_ptr<cg2::TrailStressFixture> trailStress_;
};
