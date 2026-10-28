#include "game/session/GameSession.h"
#include "game/session/GameplaySystems.h"

namespace gameplay {
GameSession::GameSession(RunMode runMode) : world_(std::make_unique<GameWorld>(runMode)) {}

GameSession::~GameSession() = default;

void GameSession::Initialize()
{
    world_->sessionBootstrap->Initialize();
}

bool GameSession::InitializeStep()
{
    return world_->sessionBootstrap->InitializeStep();
}

void GameSession::Update()
{
    world_->combatFramePipeline->Update();
}

void GameSession::Draw()
{
    world_->gameplayRenderer->Draw();
}

void GameSession::DrawShadow()
{
    world_->gameplayRenderer->DrawShadow();
}

void GameSession::DrawPostEffect3D()
{
    world_->gameplayRenderer->DrawPostEffect3D();
}

void GameSession::DrawAfterPostEffect3D()
{
    world_->gameplayRenderer->DrawAfterPostEffect3D();
}

void GameSession::DrawSprite()
{
    world_->gameplayHud->DrawSprite();
}

bool GameSession::IsFinished() const
{
    return world_->gameplayQueries->IsFinished();
}

float GameSession::GetFinalDeltaTime() const
{
    return world_->gameplayQueries->GetFinalDeltaTime();
}

float GameSession::GetPostGaussianIntensity() const
{
    return world_->gameplayQueries->GetPostGaussianIntensity();
}

PostEffectPulse GameSession::GetPostEffectPulse() const
{
    return world_->gameplayQueries->GetPostEffectPulse();
}

IScene::BloomViewSettings GameSession::GetBloomViewSettings() const
{
    IScene::BloomViewSettings settings;
    const int view = world_->presentation.bloomView_;
    if (view == 1) {
        settings.view = 1;
    } else if (view >= 2 && view <= 6) {
        settings.view = 2;
        settings.level = view - 2;
    } else if (view == 7) {
        settings.view = 3;
    }
    return settings;
}

ScreenEffectState GameSession::GetScreenEffectState() const
{
    return world_->gameplayQueries->GetScreenEffectState();
}

bool GameSession::IsNeonShowcaseActive() const
{
    return world_->gameplayQueries->IsNeonShowcaseActive();
}

DeveloperShowcaseState GameSession::GetDeveloperShowcaseState()
{
    return world_->gameplayQueries->GetDeveloperShowcaseState();
}

void GameSession::RecordDeveloperPostParameters(const myEngine::BloomParam& param)
{
    world_->gameplayQueries->RecordDeveloperPostParameters(param);
}

void GameSession::RecordDeveloperFrame(myEngine::DirectXCommon& dx)
{
    world_->gameplayQueries->RecordDeveloperFrame(dx);
    if (world_->run.UsesPrototypeRules()) {
        world_->arenaRunController->CopyTankRunCapture();
    }
}

void GameSession::SetRenderProfile(const IScene::RenderProfile& profile)
{
    world_->performanceMonitor->SetRenderProfile(profile);
}

void GameSession::EnableTitleDemo()
{
    world_->titleDemoController->EnableTitleDemo();
}

bool GameSession::IsTitleDemo() const
{
    return world_->titleDemoController->IsTitleDemo();
}

const TitleDemoStatus& GameSession::GetTitleDemoStatus() const
{
    return world_->titleDemoController->GetTitleDemoStatus();
}

float GameSession::GetTitleDemoFade() const
{
    return world_->titleDemoController->GetTitleDemoFade();
}

void GameSession::RequestTitleDemoCapture(const std::string& name)
{
    world_->titleDemoController->RequestTitleDemoCapture(name);
}

void GameSession::CopyTitleDemoCapture()
{
    world_->titleDemoController->CopyTitleDemoCapture();
}

void GameSession::FlushTitleDemoCapture()
{
    world_->titleDemoController->FlushTitleDemoCapture();
}

nlohmann::json GameSession::GetTitleDemoBuild() const
{
    return world_->titleDemoController->GetTitleDemoBuild();
}

std::string GameSession::GetNextSceneName() const
{
    return world_->gameplayQueries->GetNextSceneName();
}
} // namespace gameplay
