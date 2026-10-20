#include "EntityScene.h"

#include "LogWrite.h"
#include "Object3dCommon.h"
#include "ResourcePaths.h"
#include "Utf8Path.h"

#include <utility>

EntityScene::EntityScene(std::string sceneFile, RegisterFunction registerGame)
    : sceneFile_(std::move(sceneFile)), registerGame_(std::move(registerGame))
{
}

EntityScene::~EntityScene()
{
    // 描画の状態（Object3d）は world と一緒に消えるので、先にカメラの参照を元へ戻す。
    myEngine::Object3dCommon* common = myEngine::Object3dCommon::GetInstance();
    if (renderSync_ && common->GetDefaultCamera() == renderSync_->GetCamera()) {
        common->SetDefaultCamera(previousCamera_);
    }
    editor_.reset();
    registry_.reset();
    world_.reset();
}

void EntityScene::Initialize()
{
    world_ = std::make_unique<flecs::world>();
    registry_ = std::make_unique<myEngine::ComponentRegistry>(*world_);
    myEngine::RegisterEngineComponents(*registry_);
    myEngine::RegisterRenderComponents(*world_);
    if (registerGame_) {
        registerGame_(*registry_);
    }

    renderSync_ = std::make_unique<myEngine::RenderSync>(*world_);
    myEngine::Object3dCommon* common = myEngine::Object3dCommon::GetInstance();
    previousCamera_ = common->GetDefaultCamera();
    // Object3d は作成時に既定のカメラを取り込むので、Object3d を作る前にシーンのカメラを設定する。
    common->SetDefaultCamera(renderSync_->GetCamera());

    if (!sceneFile_.empty()) {
        const myEngine::SceneSerializer serializer(*registry_);
        loadResult_ = serializer.LoadFromFile(myEngine::Utf8ToPath(myEngine::ResourcePaths::GameRoot() + "/" + sceneFile_));
        myEngine::LogWrite().Log("[EntityScene] Loaded '" + sceneFile_ + "': success=" + (loadResult_.success ? "true" : "false") +
                                 ", entities=" + std::to_string(loadResult_.entities.size()) +
                                 ", warnings=" + std::to_string(loadResult_.warnings.size()) + ".\n");
    } else {
        loadResult_.success = true;
    }
    if (editorOptions_.enabled) {
        editor_ = std::make_unique<myEngine::EntityEditor>(*registry_,
                                                           myEngine::Utf8ToPath(myEngine::ResourcePaths::GameRoot() + "/" + sceneFile_));
        if (editorOptions_.startPlaying) {
            editor_->Play();
        }
    }
    OnSceneLoaded();
    UseEditorCameraWhileEditing();
    renderSync_->Update();
}

void EntityScene::UseEditorCameraWhileEditing()
{
    renderSync_->SetEditorCamera(editor_ && !editor_->IsPlaying() ? &editor_->GetCamera() : nullptr);
}

void EntityScene::Update()
{
    if (editor_) {
        editor_->Draw();
    }
    // エディタの編集中はゲームのシステムを動かさない（値の変更は描画へ反映する）。
    if (!editor_ || editor_->IsPlaying()) {
        world_->progress(GetFinalDeltaTime());
    }
    UseEditorCameraWhileEditing();
    renderSync_->Update();
}

void EntityScene::DrawPostEffect3D()
{
    myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
    renderSync_->Draw();
}
