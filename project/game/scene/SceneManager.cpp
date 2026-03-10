#include "SceneManager.h"
#include "TitleScene.h"
#include "GameScene.h"

void SceneManager::Initialize() {
    currentType_ = SceneType::kTitle;
    currentScene_ = std::make_unique<TitleScene>();
    currentScene_->Initialize();
}

void SceneManager::ChangeScene() {
    // 現在のシーンが終了していなければ何もしない
    if (!currentScene_->IsFinished()) return;

    // 次のシーンを生成
    if (currentType_ == SceneType::kTitle) {
        currentType_ = SceneType::kGame;
        currentScene_ = std::make_unique<GameScene>();
    } else {
        currentType_ = SceneType::kTitle;
        currentScene_ = std::make_unique<TitleScene>();
    }

    // 新しいシーンを初期化
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
