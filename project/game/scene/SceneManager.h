#pragma once
#include <memory>
#include <string_view>
#include <vector>
#include "AbstractSceneFactory.h"

class SceneRegistry;

class SceneManager {
public:

	// シングルトン
	static SceneManager* GetInstance();

	bool Initialize(const std::string& firstSceneName);
	bool ContainsScene(std::string_view sceneName);
	std::vector<std::string> GetRegisteredSceneNames();
	const std::string& GetCurrentSceneName() const { return currentSceneName_; }
	void Update();
	void Draw();
	void DrawPostEffect3D();
	void DrawAfterPostEffect3D();
	void DrawShadow();
	void DrawSprite();

	float GetFinalDeltaTime();
	float GetPostGaussianIntensity();
	IScene::PostEffectPulse GetPostEffectPulse();
	IScene::WaterPostProcessSettings GetWaterPostProcessSettings();
	void SetRenderProfile(const IScene::RenderProfile& profile);

private:
	void EnsureSceneFactory();

	// 現在のシーンを抽象的な型で保持
	std::unique_ptr<IScene> currentScene_ = nullptr;
	std::unique_ptr<AbstractSceneFactory> sceneFactory_ = nullptr;
	const SceneRegistry* sceneRegistry_ = nullptr;

	// 現在のシーン名を保持（必要な場合のみ）
	std::string currentSceneName_;
	std::string failedTransitionFromSceneName_;
	std::string failedTransitionToSceneName_;
};
