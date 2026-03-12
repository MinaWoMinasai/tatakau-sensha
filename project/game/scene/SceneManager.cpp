#include "SceneManager.h"
#include "SceneFactory.h"

SceneManager* SceneManager::GetInstance()
{
    static SceneManager instance;
    return &instance;
}

void SceneManager::Initialize(const std::string& firstSceneName) {

    // Factory の生成
    sceneFactory_ = std::make_unique<SceneFactory>();

    // 最初のシーンを生成
    currentScene_ = sceneFactory_->CreateScene(firstSceneName);
    currentScene_->Initialize();
}

void SceneManager::Update() {
    // シーンが終了していたら切り替え
    if (currentScene_->IsFinished()) {
        // 次のシーン名をシーン自身から取得する
        std::string nextSceneName = currentScene_->GetNextSceneName();

        // Factory に新しいシーンを作ってもらう
        // ここで SceneManager は「何が作られるか」を具体的に知らなくて済む
        std::unique_ptr<IScene> nextScene = sceneFactory_->CreateScene(nextSceneName);

        if (nextScene) {
            currentScene_ = std::move(nextScene);
            currentScene_->Initialize();
        }
    }

    currentScene_->Update();
}

void SceneManager::Draw() {
    currentScene_->Draw();
}

void SceneManager::DrawShadow() {
    currentScene_->DrawShadow();
}

void SceneManager::DrawPostEffect3D() {
    currentScene_->DrawPostEffect3D();
}

void SceneManager::DrawSprite() {
    currentScene_->DrawSprite();
}
float SceneManager::GetFinalDeltaTime()
{
	return currentScene_->GetFinalDeltaTime();
}
