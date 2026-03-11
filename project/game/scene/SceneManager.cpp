#include "SceneManager.h"
#include "SceneFactory.h"

void SceneManager::Initialize() {

    sceneFactory_ = std::make_unique<SceneFactory>();
    currentType_ = SceneType::kTitle;
    currentScene_ = sceneFactory_->CreateScene("TITLE");
    currentScene_->Initialize();
}

void SceneManager::ChangeScene() {
    if (!currentScene_->IsFinished()) return;

    if (currentType_ == SceneType::kTitle) {
        currentType_ = SceneType::kGame;
        currentScene_ = sceneFactory_->CreateScene("GAME"); // Factory経由にする
    } else {
        currentType_ = SceneType::kTitle;
        currentScene_ = sceneFactory_->CreateScene("TITLE");
    }
    currentScene_->Initialize();
}

void SceneManager::Update() {
    ChangeScene(); // 終了チェックと切り替え
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
	if (currentType_ == SceneType::kGame) {
		return currentScene_->GetFinalDeltaTime();
	}
	return 1.0f / 60.0f;
}
