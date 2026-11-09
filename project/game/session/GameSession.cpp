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

void GameSession::ApplyArenaLookOverrides(IScene::ArenaLookSettings& look) const
{
    // 試作ではカメラが自機を追うので、自機に付いたHP・シールド・スタミナ・熱のゲージは、残光がワールドに置き去りになって
    // 何重にも見える。ゲージの外側（自機の描画半径の約2.2倍）までを、残光を出さない円にする。
    // 自機の本体の残像（PlayerNeonAfterimage）は毎フレーム描く形なので、この円の中でも残る。
    const auto& world = *world_;
    if (!world.run.IsSurvivorRun() || !world.resources.player_ || world.resources.player_->IsDead() || !world.gameplayHud) {
        return;
    }
    const myEngine::Vector3 position = world.resources.player_->GetWorldPosition();
    const float reach = world.presentation.playerNeonBillboardRadius_ * 2.2f;
    const myEngine::Vector2 center = world.gameplayHud->WorldToScreen(position);
    const myEngine::Vector2 edge = world.gameplayHud->WorldToScreen(position + myEngine::Vector3{0.0f, reach, 0.0f});
    const float height = static_cast<float>(myEngine::WinApp::kClientHeight);
    look.persistenceMaskCenter = {center.x / static_cast<float>(myEngine::WinApp::kClientWidth), center.y / height};
    look.persistenceMaskRadius = std::abs(edge.y - center.y) / height;
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
