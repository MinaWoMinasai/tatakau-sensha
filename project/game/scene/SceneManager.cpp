#include "SceneManager.h"
#include "SceneFactory.h"
#include "LogWrite.h"

SceneManager* SceneManager::GetInstance()
{
    static SceneManager instance;
    return &instance;
}

void SceneManager::EnsureSceneFactory()
{
    if (sceneFactory_) {
        return;
    }

    auto sceneFactory = std::make_unique<SceneFactory>();
    sceneRegistry_ = &sceneFactory->GetRegistry();
    sceneFactory_ = std::move(sceneFactory);
}

bool SceneManager::ContainsScene(std::string_view sceneName)
{
    EnsureSceneFactory();
    return sceneRegistry_ && sceneRegistry_->Contains(sceneName);
}

std::vector<std::string> SceneManager::GetRegisteredSceneNames()
{
    EnsureSceneFactory();
    return sceneRegistry_ ? sceneRegistry_->GetRegisteredNames() : std::vector<std::string>{};
}

bool SceneManager::Initialize(const std::string& firstSceneName) {

    EnsureSceneFactory();

    // 最初のシーンを生成
    std::unique_ptr<IScene> firstScene = sceneFactory_->CreateScene(firstSceneName);
    if (!firstScene) {
        LogWrite().Log(
            "[SceneManager] Failed to initialize first scene '" + firstSceneName +
            "'. The existing active scene, if any, remains unchanged.\n");
        return false;
    }

    currentScene_ = std::move(firstScene);
    currentSceneName_ = firstSceneName;
    failedTransitionFromSceneName_.clear();
    failedTransitionToSceneName_.clear();
    currentScene_->Initialize();
    return true;
}

void SceneManager::Update() {
    if (!currentScene_) {
        return;
    }

    // シーンが終了していたら切り替え
    if (currentScene_->IsFinished()) {
        // 次のシーン名をシーン自身から取得する
        std::string nextSceneName = currentScene_->GetNextSceneName();

        const bool alreadyReported =
            failedTransitionFromSceneName_ == currentSceneName_ &&
            failedTransitionToSceneName_ == nextSceneName;
        if (!alreadyReported) {
            // Factory に新しいシーンを作ってもらう
            // ここで SceneManager は「何が作られるか」を具体的に知らなくて済む
            std::unique_ptr<IScene> nextScene = sceneFactory_->CreateScene(nextSceneName);

            if (nextScene) {
                currentScene_ = std::move(nextScene);
                currentSceneName_ = nextSceneName;
                failedTransitionFromSceneName_.clear();
                failedTransitionToSceneName_.clear();
                currentScene_->Initialize();
            } else {
                failedTransitionFromSceneName_ = currentSceneName_;
                failedTransitionToSceneName_ = nextSceneName;
                LogWrite().Log(
                    "[SceneManager] Transition from '" + currentSceneName_ + "' to '" +
                    nextSceneName + "' failed. The current scene remains active.\n");
            }
        }
    } else {
        failedTransitionFromSceneName_.clear();
        failedTransitionToSceneName_.clear();
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
