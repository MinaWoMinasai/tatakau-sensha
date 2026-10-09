#include "game/session/GameSession.h"
#include "game/session/GameplaySystems.h"

namespace gameplay {
GameSession::GameSession(bool prototypeRun, bool expeditionRun) : world_(std::make_unique<GameWorld>(prototypeRun, expeditionRun)) {}
GameSession::~GameSession() = default;

void GameSession::Initialize()
{
    world_->sessionBootstrap->Initialize();
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

cg2::Object3d* GameSession::GetBallObj()
{
    return world_->gameplayQueries->GetBallObj();
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

void GameSession::RecordDeveloperPostParameters(const cg2::BloomParam& param)
{
    world_->gameplayQueries->RecordDeveloperPostParameters(param);
}

void GameSession::RecordDeveloperFrame(cg2::DirectXCommon& dx)
{
    world_->gameplayQueries->RecordDeveloperFrame(dx);
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
