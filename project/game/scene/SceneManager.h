#pragma once
#include <memory>
#include "AbstractSceneFactory.h"

class SceneManager {
public:

	// シングルトン
	static SceneManager* GetInstance();

	void Initialize(const std::string& firstSceneName);
	void Update();
	void Draw();
	void DrawPostEffect3D();
	void DrawShadow();
	void DrawSprite();

	float GetFinalDeltaTime();

private:
	// 現在のシーンを抽象的な型で保持
	std::unique_ptr<IScene> currentScene_ = nullptr;
	std::unique_ptr<AbstractSceneFactory> sceneFactory_ = nullptr;

	// 現在のシーン名を保持（必要な場合のみ）
	std::string currentSceneName_;
};