#pragma once
#include "GameScene.h"
#include "TitleScene.h"

enum class Scene {
	kUnknown = 0,
	kTitle,
	kGame,
};

class SceneManager {
public:
	void Initialize();

	void ChangeScene();

	void Update();

	void Draw();

	void DrawPostEffect3D();

	void DrawSprite();

	//Difficult difficult = NORMAL;

private:
	// 現在シーン（型）
	Scene scene = Scene::kUnknown;

	std::unique_ptr<TitleScene> titleScene_ = nullptr;
	std::unique_ptr<GameScene> gameScene_ = nullptr;
};