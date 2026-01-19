#include "SceneManager.h"

void SceneManager::Initialize() {
	// 最初のシーンを Title に設定
	scene = Scene::kTitle;

	// TitleScene を生成して初期化
	titleScene_ = std::make_unique<TitleScene>();
	titleScene_->Initialize();
}

void SceneManager::ChangeScene() {
	switch (scene) {
	case Scene::kTitle:
		if (titleScene_->IsFinished()) {
			// シーン変更
			scene = Scene::kGame;
			// タイトルシーンを削除
			titleScene_ = nullptr;
			gameScene_ = std::make_unique<GameScene>();
			gameScene_->Initialize();
		}
		break;
	case Scene::kGame:
		if (gameScene_->IsFinished()) {
			// シーン変更
			scene = Scene::kTitle;
			// タイトルシーンを削除
			gameScene_ = nullptr;
			titleScene_ = std::make_unique<TitleScene>();
			titleScene_->Initialize();
		}
		break;
	}
}

void SceneManager::Update() {
	switch (scene) {
	case Scene::kTitle:
		titleScene_->Update();
		break;
	case Scene::kGame:
		gameScene_->Update();
		break;
	}
}

void SceneManager::Draw() {
	switch (scene) {
	case Scene::kTitle:
		titleScene_->Draw();
		break;
	case Scene::kGame:
		gameScene_->Draw();
		break;
	}
}

void SceneManager::DrawSprite()
{
	switch (scene) {
	case Scene::kTitle:
		titleScene_->DrawSprite();
		break;
	case Scene::kGame:
		gameScene_->DrawSprite();
		break;
	}
}
