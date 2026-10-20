#include "Game.h"
#include "../modules/GameModuleBootstrap.h"
#include "Audio.h"
#include "Input.h"
#include "SceneManager.h"
#include "TitleScene.h"
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "game/debug/GameplayScenarioSession.h"
#include "game/debug/NeonShowcaseCapture.h"
#endif

namespace {

/// @brief 環境変数が"1"のときtrueを返す。
bool TestEnabled(const char* name)
{
    char flag[8]{};
    return GetEnvironmentVariableA(name, flag, sizeof(flag)) == 1 && flag[0] == '1';
}

} // namespace

Game::Game() = default;
Game::~Game() = default;

bool Game::RegisterGameModules(GameModuleRegistry& registry)
{
    return RegisterAvailableGameModules(registry);
}

std::string_view Game::GetFallbackGameModuleId() const
{
    return "builtin";
}

std::string_view Game::GetFallbackSceneName() const
{
    return "TITLE";
}

std::string_view Game::GetProgressiveStartupSceneName() const
{
    return "TITLE";
}

std::string_view Game::GetStartupValidationSceneName() const
{
    return "TANK_EXPEDITION";
}

void Game::OnBeforeEngineInitialize(const std::string& startupScene)
{
    if (startupScene == "TITLE") {
        TitleScene::StartAssetPreloads();
    }
}

void Game::OnLoadSharedAssets()
{
    myEngine::Audio::GetInstance()->LoadAudio(L"bulletShoot", L"resources/bulletShoot.mp3", 5);
}

bool Game::IsBackgroundValidationActive() const
{
    // 非表示で行う決定的な検証は、ユーザーが別のアプリに切り替えても一時停止してはならない。
    char experienceMode[8]{};
    const bool experienceValidation =
        GetEnvironmentVariableA("MYENGINE_TANK_EXPERIENCE_AUTOTEST", experienceMode, sizeof(experienceMode)) == 1 &&
        (experienceMode[0] == '1' || experienceMode[0] == '2');
    return TestEnabled("MYENGINE_TITLE_AUTOTEST") || TestEnabled("MYENGINE_TANK_TUTORIAL_AUTOTEST") ||
           TestEnabled("MYENGINE_TANK_AUTOTEST") || TestEnabled("MYENGINE_TANK_MAP_AUTOTEST") ||
           TestEnabled("MYENGINE_TANK_COMBAT_AUTOTEST") || TestEnabled("MYENGINE_TANK_SPECIAL_AUTOTEST") ||
           TestEnabled("MYENGINE_NEON_BOSS_AUTOTEST") || TestEnabled("MYENGINE_SUBMISSION_AUTOTEST") || experienceValidation
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
           || GameplayScenarioSession::Get().IsActive()
#endif
        ;
}

void Game::OnMainLoopStarting()
{
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (GameplayScenarioSession::Get().IsActive() && SceneManager::GetInstance()->GetCurrentSceneName() != "TANK_EXPEDITION") {
        auto& session = GameplayScenarioSession::Get();
        session.Fail("Gameplay scenarios require TANK_EXPEDITION; launch with --project resources/projects/tank_expedition.project.json.");
        session.Finish();
        PostQuitMessage(9);
    }
    startupCapture_ = std::make_unique<NeonShowcaseCapture>();
    captureStartup_ = TestEnabled("MYENGINE_STARTUP_CAPTURE");
#endif
}

void Game::OnMainLoopFinished()
{
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    // キャプチャはGPU資源を持つので、エンジンの終了処理より前に解放する。
    startupCapture_.reset();
#endif
}

void Game::OnFrameInputReady([[maybe_unused]] myEngine::Input& input)
{
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (GameplayScenarioSession::Get().IsActive()) {
        input.OverrideValidationFrame({640.0f, 360.0f});
    }
#endif
}

void Game::OnBeforeSceneUpdate()
{
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    depthSceneEpochBefore_ = GameplayScenarioSession::Get().GetSceneEpoch();
    depthSceneNameBefore_ = SceneManager::GetInstance()->GetCurrentSceneName();
#endif
}

void Game::OnAfterSceneUpdate()
{
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    auto& depthSession = GameplayScenarioSession::Get();
    const auto& actualSceneName = SceneManager::GetInstance()->GetCurrentSceneName();
    if (depthSession.IsActive() && gameplaytest::IsNeonDepthScenario(depthSession.GetSettings().scenario) &&
        (actualSceneName != depthSceneNameBefore_ || depthSession.GetSceneEpoch() != depthSceneEpochBefore_)) {
        if (depthSession.NotifyDepthSceneEntered(actualSceneName)) {
            PostQuitMessage(depthSession.GetErrors().empty() ? 0 : 9);
        }
    }
#endif
}

void Game::AdjustGrayscale([[maybe_unused]] float deltaTime, [[maybe_unused]] const myEngine::DeveloperShowcaseState& showcase,
                           [[maybe_unused]] bool& grayscale)
{
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (GameplayScenarioSession::Get().IsActive()) {
        grayscale = gameplaytest::ScenarioGrayscaleEnabled(deltaTime, GameplayScenarioSession::Get().GetSettings().fixedDeltaTime,
                                                           showcase.comparisonFreeze, showcase.active);
    }
#endif
}

void Game::OnStartupFrameRecorded([[maybe_unused]] myEngine::DirectXCommon& dx, [[maybe_unused]] bool firstFrame)
{
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (captureStartup_ && firstFrame) {
        startupCapture_->Request("generated/async_startup_capture", "first_title", {{"stage", "first_title"}});
    }
    startupCapture_->Record(dx);
#endif
}

void Game::OnStartupFramePresented([[maybe_unused]] myEngine::DirectXCommon& dx)
{
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    startupCapture_->Resolve(dx);
#endif
}
