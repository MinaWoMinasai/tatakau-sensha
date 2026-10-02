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

/// @brief エンジンとゲームの初期化・フレーム更新・終了処理を統括する。
class Game {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    bool Initialize(const GameProjectCommandLineOptions& projectOptions = {});
    /// @brief 初期化後にメインループを実行し、終了処理まで進める。
    void Run();
    /// @brief 利用終了時の資源と状態を解放する。
    void Finalize();

    /// @brief 有効プロジェクトを返す。
    const GameProject& GetActiveProject() const
    {
        return activeProject_;
    }
    /// @brief 有効プロジェクトLoadedからのFallbackであるか判定する。
    bool IsActiveProjectLoadedFromFallback() const
    {
        return activeProjectLoadedFromFallback_;
    }
    /// @brief ゲームModuleUsingFallbackであるか判定する。
    bool IsGameModuleUsingFallback() const
    {
        return gameModuleUsedFallback_;
    }
    /// @brief 起動時シーンUsingFallbackであるか判定する。
    bool IsStartupSceneUsingFallback() const
    {
        return startupSceneUsedFallback_;
    }
    /// @brief Resolved起動時シーンを返す。
    const std::string& GetResolvedStartupScene() const
    {
        return resolvedStartupScene_;
    }

private:
    /// @brief 有効プロジェクトを読み込む。
    void LoadActiveProject(const GameProjectCommandLineOptions& projectOptions);
    /// @brief ゲームModuleとシーンファクトリーを利用条件を設定する。
    bool ConfigureGameModuleAndSceneFactory();
    /// @brief 起動時シーンを解決する。
    bool ResolveStartupScene();
    /// @brief Engineを初期化する。
    void InitializeEngine();
    /// @brief ImGUIを初期化する。
    void InitializeImGui();
    /// @brief リソースを読み込む。
    void LoadResources();
    /// @brief 終了要求まで入力・更新・描画を繰り返す。
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
