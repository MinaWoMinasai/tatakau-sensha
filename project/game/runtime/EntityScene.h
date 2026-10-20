#pragma once

#include "../scene/IScene.h"
#include "ComponentRegistry.h"
#include "RenderSync.h"
#include "SceneSerializer.h"

#include <functional>
#include <memory>
#include <string>

namespace myEngine {
class Camera;
} // namespace myEngine

/// @brief flecs の world を持ち、シーンファイル（*.scene.json）から作るシーン。
/// 毎フレーム world のシステムを実行してから、RenderSync で描画へ反映する。
/// ゲームは、自分のコンポーネントとシステムを registerGame で登録する。
class EntityScene : public IScene {
public:
    /// @brief world 作成後に、ゲームのコンポーネント・システムを登録する関数。
    using RegisterFunction = std::function<void(myEngine::ComponentRegistry&)>;

    /// @param sceneFile ゲームの素材ディレクトリ（ResourcePaths::GameRoot）からのシーンファイルのパス（UTF-8）。空なら空のシーンで始める。
    /// @param registerGame ゲームのコンポーネント・システムを登録する関数。不要なら空。
    explicit EntityScene(std::string sceneFile, RegisterFunction registerGame = {});
    ~EntityScene() override;

    void Initialize() override;
    void Update() override;

    void Draw() override {}

    void DrawPostEffect3D() override;

    void DrawSprite() override {}

    bool IsFinished() const override
    {
        return false;
    }

    flecs::world& GetWorld()
    {
        return *world_;
    }

    myEngine::ComponentRegistry& GetRegistry()
    {
        return *registry_;
    }

    /// @brief シーンファイルの読み込み結果を返す。
    const myEngine::SceneLoadResult& GetLoadResult() const
    {
        return loadResult_;
    }

protected:
    /// @brief シーンファイルを読み込んだ直後に呼ぶ。派生クラスで追加の準備をする。
    virtual void OnSceneLoaded() {}

private:
    std::string sceneFile_;
    RegisterFunction registerGame_;
    std::unique_ptr<flecs::world> world_;
    std::unique_ptr<myEngine::ComponentRegistry> registry_;
    std::unique_ptr<myEngine::RenderSync> renderSync_;
    myEngine::SceneLoadResult loadResult_;
    // このシーンに入る前の既定のカメラ。シーンを抜けるときに戻す。
    myEngine::Camera* previousCamera_ = nullptr;
};
