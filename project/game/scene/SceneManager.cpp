#include "SceneManager.h"
#include "LogWrite.h"
#include "StartupTrace.h"

#include <utility>

SceneManager* SceneManager::GetInstance()
{
    static SceneManager instance;
    return &instance;
}

bool SceneManager::SetSceneFactory(
    std::unique_ptr<AbstractSceneFactory> sceneFactory)
{
    if (!sceneFactory) {
        cg2::LogWrite().Log(
            "[SceneManager] Rejected a nullptr scene factory. "
            "The existing factory remains unchanged.\n");
        return false;
    }
    if (currentScene_) {
        cg2::LogWrite().Log(
            "[SceneManager] Cannot set a scene factory after a scene has been initialized. "
            "The existing factory remains unchanged.\n");
        return false;
    }
    if (sceneFactory_) {
        cg2::LogWrite().Log(
            "[SceneManager] Cannot replace an already configured scene factory. "
            "The existing factory remains unchanged.\n");
        return false;
    }

    sceneFactory_ = std::move(sceneFactory);
    return true;
}

bool SceneManager::ContainsScene(std::string_view sceneName) const
{
    return sceneFactory_ && sceneFactory_->ContainsScene(sceneName);
}

std::vector<std::string> SceneManager::GetRegisteredSceneNames() const
{
    return sceneFactory_
        ? sceneFactory_->GetRegisteredSceneNames()
        : std::vector<std::string>{};
}

bool SceneManager::Initialize(const std::string& firstSceneName) {
    cg2::StartupTrace::Scope startupScope("Scene.Initialize." + firstSceneName);

    if (!sceneFactory_) {
        cg2::LogWrite().Log(
            "[SceneManager] Cannot initialize scene '" + firstSceneName +
            "' because no scene factory has been configured.\n");
        return false;
    }

    // 最初のシーンを生成
    std::unique_ptr<IScene> firstScene = sceneFactory_->CreateScene(firstSceneName);
    if (!firstScene) {
        cg2::LogWrite().Log(
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

void SceneManager::Finalize()
{
    // Scene resources can refer to engine-owned managers such as SrvManager.
    // Destroy them explicitly while those managers are still alive instead of
    // relying on static-destruction order at process shutdown.
    currentScene_.reset();
    sceneFactory_.reset();
    currentSceneName_.clear();
    failedTransitionFromSceneName_.clear();
    failedTransitionToSceneName_.clear();
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
                cg2::StartupTrace::Mark("transition.begin." + currentSceneName_ + "." + nextSceneName);
                {
                    cg2::StartupTrace::Scope scope("Scene.DestroyPrevious." + currentSceneName_);
                    currentScene_ = std::move(nextScene);
                }
                currentSceneName_ = nextSceneName;
                failedTransitionFromSceneName_.clear();
                failedTransitionToSceneName_.clear();
                {
                    cg2::StartupTrace::Scope scope("Scene.Initialize." + nextSceneName);
                    currentScene_->Initialize();
                }
                cg2::StartupTrace::Mark("transition.initialized." + nextSceneName);
                cg2::StartupTrace::Flush();
            } else {
                failedTransitionFromSceneName_ = currentSceneName_;
                failedTransitionToSceneName_ = nextSceneName;
                cg2::LogWrite().Log(
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

IScene::ScreenEffectState SceneManager::GetScreenEffectState()
{
	return currentScene_ ? currentScene_->GetScreenEffectState() : IScene::ScreenEffectState{};
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
