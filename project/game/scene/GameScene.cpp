#include "GameScene.h"
#include "game/session/GameSession.h"

GameScene::GameScene(bool prototypeRun, bool expeditionRun) : session_(std::make_unique<gameplay::GameSession>(prototypeRun, expeditionRun))
{
}
GameScene::~GameScene() = default;

void GameScene::Initialize()
{
    session_->Initialize();
}

void GameScene::Update()
{
    session_->Update();
}

void GameScene::Draw()
{
    session_->Draw();
}

void GameScene::DrawShadow()
{
    session_->DrawShadow();
}

void GameScene::DrawPostEffect3D()
{
    session_->DrawPostEffect3D();
}

void GameScene::DrawAfterPostEffect3D()
{
    session_->DrawAfterPostEffect3D();
}

void GameScene::DrawSprite()
{
    session_->DrawSprite();
}

bool GameScene::IsFinished() const
{
    return session_->IsFinished();
}

cg2::Object3d* GameScene::GetBallObj()
{
    return session_->GetBallObj();
}

float GameScene::GetFinalDeltaTime() const
{
    return session_->GetFinalDeltaTime();
}

float GameScene::GetPostGaussianIntensity() const
{
    return session_->GetPostGaussianIntensity();
}

IScene::PostEffectPulse GameScene::GetPostEffectPulse() const
{
    return session_->GetPostEffectPulse();
}

IScene::ScreenEffectState GameScene::GetScreenEffectState() const
{
    return session_->GetScreenEffectState();
}

bool GameScene::IsNeonShowcaseActive() const
{
    return session_->IsNeonShowcaseActive();
}

IScene::DeveloperShowcaseState GameScene::GetDeveloperShowcaseState()
{
    return session_->GetDeveloperShowcaseState();
}

void GameScene::RecordDeveloperPostParameters(const cg2::BloomParam& param)
{
    session_->RecordDeveloperPostParameters(param);
}

void GameScene::RecordDeveloperFrame(cg2::DirectXCommon& dx)
{
    session_->RecordDeveloperFrame(dx);
}

void GameScene::SetRenderProfile(const IScene::RenderProfile& profile)
{
    session_->SetRenderProfile(profile);
}

void GameScene::EnableTitleDemo()
{
    session_->EnableTitleDemo();
}

bool GameScene::IsTitleDemo() const
{
    return session_->IsTitleDemo();
}

const GameScene::TitleDemoStatus& GameScene::GetTitleDemoStatus() const
{
    return session_->GetTitleDemoStatus();
}

float GameScene::GetTitleDemoFade() const
{
    return session_->GetTitleDemoFade();
}

void GameScene::RequestTitleDemoCapture(const std::string& name)
{
    session_->RequestTitleDemoCapture(name);
}

void GameScene::CopyTitleDemoCapture()
{
    session_->CopyTitleDemoCapture();
}

void GameScene::FlushTitleDemoCapture()
{
    session_->FlushTitleDemoCapture();
}

nlohmann::json GameScene::GetTitleDemoBuild() const
{
    return session_->GetTitleDemoBuild();
}

std::string GameScene::GetNextSceneName() const
{
    return session_->GetNextSceneName();
}
