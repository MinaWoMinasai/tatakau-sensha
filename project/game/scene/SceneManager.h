#pragma once
#include <memory>
#include "AbstractSceneFactory.h"

class SceneManager {
public:
	void Initialize();
	void Update();
	void Draw();
	void DrawPostEffect3D();
	void DrawShadow();
	void DrawSprite();

	float GetFinalDeltaTime();
	
	// シーン切り替えロジック
	void ChangeScene();

private:
	// 現在のシーンを抽象的な型で保持
	std::unique_ptr<IScene> currentScene_ = nullptr;
	std::unique_ptr<AbstractSceneFactory> sceneFactory_ = nullptr;

	// 現在どのシーンか識別するための型（切り替え判定用）
	enum class SceneType { kTitle, kGame };
	SceneType currentType_ = SceneType::kTitle;
};