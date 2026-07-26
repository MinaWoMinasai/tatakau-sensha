#include "SceneManager.h"
#include "SceneFactory.h"
#include "LogWrite.h"

SceneManager* SceneManager::GetInstance()
{
    static SceneManager instance;
    return &instance;
}

void SceneManager::Initialize(const std::string& firstSceneName) {

    // Factory の生成
    sceneFactory_ = std::make_unique<SceneFactory>();

    // 最初のシーンを生成
    std::unique_ptr<IScene> firstScene = sceneFactory_->CreateScene(firstSceneName);
    if (!firstScene) {
        LogWrite().Log(
            "[SceneManager] Failed to initialize first scene '" + firstSceneName +
            "'. SceneManager will remain without an active scene.\n");
        currentScene_.reset();
        currentSceneName_.clear();
        return;
    }

    currentScene_ = std::move(firstScene);
    currentSceneName_ = firstSceneName;
    currentScene_->Initialize();
}

void SceneManager::Update() {
    if (!currentScene_) {
        return;
    }

    // シーンが終了していたら切り替え
    if (currentScene_->IsFinished()) {
        // 次のシーン名をシーン自身から取得する
        std::string nextSceneName = currentScene_->GetNextSceneName();

        // Factory に新しいシーンを作ってもらう
        // ここで SceneManager は「何が作られるか」を具体的に知らなくて済む
        std::unique_ptr<IScene> nextScene = sceneFactory_->CreateScene(nextSceneName);

        if (nextScene) {
            currentScene_ = std::move(nextScene);
            currentSceneName_ = nextSceneName;
            currentScene_->Initialize();
        } else {
            LogWrite().Log(
                "[SceneManager] Transition from '" + currentSceneName_ + "' to '" +
                nextSceneName + "' failed. The current scene remains active.\n");
        }
    }

    currentScene_->Update();
}

void SceneManager::Draw() {
    if (currentScene_) {
        currentScene_->Draw();
    }
}

void SceneManager::DrawShadow() {
    if (currentScene_) {
        currentScene_->DrawShadow();
    }
}

void SceneManager::DrawPostEffect3D() {
    if (currentScene_) {
        currentScene_->DrawPostEffect3D();
    }
}

void SceneManager::DrawAfterPostEffect3D() {
    if (currentScene_) {
        currentScene_->DrawAfterPostEffect3D();
    }
}

void SceneManager::DrawSprite() {
    if (currentScene_) {
        currentScene_->DrawSprite();
    }
}
float SceneManager::GetFinalDeltaTime()
{
	return currentScene_ ? currentScene_->GetFinalDeltaTime() : 1.0f / 60.0f;
}

float SceneManager::GetPostGaussianIntensity()
{
	return currentScene_ ? currentScene_->GetPostGaussianIntensity() : 0.0f;
}

IScene::PostEffectPulse SceneManager::GetPostEffectPulse()
{
	return currentScene_ ? currentScene_->GetPostEffectPulse() : IScene::PostEffectPulse{};
}

IScene::WaterPostProcessSettings SceneManager::GetWaterPostProcessSettings()
{
	return currentScene_
		? currentScene_->GetWaterPostProcessSettings()
		: IScene::WaterPostProcessSettings{};
}

void SceneManager::SetRenderProfile(const IScene::RenderProfile& profile)
{
	if (currentScene_) {
		currentScene_->SetRenderProfile(profile);
	}
}
