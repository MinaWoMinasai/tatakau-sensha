#pragma once
#include <memory>
#include <string>
#include "GameProject.h"
#include "IApplicationHooks.h"
#include "IGameModule.h"
#include "RenderTexture.h"
#include "PostEffect.h"
#include "ShadowMap.h"
#include "Bloom.h"
#include "Shadow.h"
#include "TrailStressFixture.h"
#include "InitializationTask.h"

/// @brief エンジンの初期化・メインループ・終了処理を統括する。ゲーム固有の処理はIApplicationHooksから呼ぶ。
class Application {
public:
    /// @brief 実行ファイルごとの処理を受け取る。hooksはこのApplicationより長く有効に保つ。
    explicit Application(IApplicationHooks& hooks) : hooks_(hooks) {}

    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    bool Initialize(const GameProjectCommandLineOptions& projectOptions = {});
    /// @brief メインループを実行し、終了要求のコードを返す。資源解放はFinalizeで行う。
    int Run();
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
    /// @brief 画面表示まわりの初期化を分割して行うコルーチンを作る。
    myEngine::InitializationTask InitializePresentation();
    /// @brief ImGUIを初期化する。
    void InitializeImGui();
    /// @brief リソースを読み込む。
    void LoadResources();
    /// @brief 終了要求まで入力・更新・描画を繰り返し、そのコードを返す。
    int MainLoop();

private:
    IApplicationHooks& hooks_;
    std::unique_ptr<myEngine::DirectXCommon> dxCommon_;
    Microsoft::WRL::ComPtr<ID3D12Resource> startupConstants_;
    std::unique_ptr<myEngine::SrvManager> srvManager_;
    std::unique_ptr<myEngine::RtvManager> rtvManager_;

    std::unique_ptr<myEngine::Bloom> bloom_;
    std::unique_ptr<myEngine::Shadow> shadow_;
    std::unique_ptr<IGameModule> activeGameModule_;

    GameProject activeProject_;
    bool activeProjectLoadedFromFallback_ = false;
    bool gameModuleUsedFallback_ = false;
    bool startupSceneUsedFallback_ = false;
    std::string resolvedStartupScene_;
    // 先に1フレーム表示してから残りの初期化を進めるシーン名（IApplicationHooksから受け取る）。
    std::string progressiveStartupScene_;
    bool imguiInitialized_ = false;
    bool titleFirst_ = false, engineReady_ = false;
    myEngine::InitializationTask presentationTask_;
    std::unique_ptr<myEngine::TrailStressFixture> trailStress_;
};
