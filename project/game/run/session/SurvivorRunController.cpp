#include "game/run/session/SurvivorRunController.h"
#include "game/session/GameplaySystems.h"
#include "game/player/TankRunModifiers.h"
#include <filesystem>
#include <fstream>

namespace gameplay {

namespace {
constexpr const char* kAutoTestDirectory = "generated/tank_survivor";
} // namespace

void SurvivorRunController::Initialize()
{
    // 遠征と同じ自機にする（遠征も ArenaRunController の初期化で改造の倍率を有効にしている）。
    TankRunModifiers modifiers{};
    modifiers.enabled = true;
    world_.resources.player_->SetRunModifiers(modifiers);
    world_.presentation.showPostProfileOverlay_ = false;
    world_.presentation.showLevelAiEditorPreview_ = false;
    world_.presentation.showGameDebugConsole_ = false;
    world_.runVisualSettings->InitializeTankRunVisuals();

    // レベルデータが置いた敵と出現領域を片付け、宿敵も出さない。
    world_.resources.enemy_->SetRunEncounterEnabled(false);
    world_.resources.enemyManager_->ClearRunActors();
    world_.resources.bulletManager_->ClearAll();

    wchar_t automatic[4]{};
    autoTest_ = !world_.demo.titleDemo_ && GetEnvironmentVariableW(L"MYENGINE_SURVIVOR_AUTOTEST", automatic, 4) > 0 && automatic[0] == L'1';
    if (autoTest_) {
        world_.presentation.debugPlayerNoDamage_ = true;
        world_.resources.player_->SetDebugNoDamage(true);
        std::filesystem::create_directories(kAutoTestDirectory);
    }
}

void SurvivorRunController::Update(float dt)
{
    if (world_.combat.phase_ != Phase::kMain) {
        return;
    }
    if (autoTest_) {
        UpdateAutoTest(dt);
    }
}

void SurvivorRunController::OnEnemyDefeated(const myEngine::Vector3& position)
{
    world_.combat.screenEffectDirector_.TriggerEnemyDefeat(world_.gameplayQueries->WorldToScreenUv(position), 1.0f);
}

void SurvivorRunController::DrawUi() {}

void SurvivorRunController::UpdateAutoTest(float dt)
{
    // 0.3秒で開始時の画面、右下へ2秒動いた後に移動後の画面を保存し、結果を書いて終了する。
    auto& player = *world_.resources.player_;
    auto capture = [&](const char* name) {
        if (world_.run.tankRunCapturePath_.empty()) {
            world_.run.tankRunCapturePath_ = std::string(kAutoTestDirectory) + "/" + name + ".png";
        }
    };
    autoTestTime_ += dt;
    const myEngine::Vector3 aim = player.GetWorldPosition() + myEngine::Vector3{6.0f, 0.0f, 0.0f};
    if (autoTestStep_ == 0) {
        autoTestStart_ = player.GetWorldPosition();
        player.SetDemoInput(true, {0.0f, 0.0f}, aim, false, false);
        if (autoTestTime_ >= 0.3f) {
            capture("start");
            autoTestStep_ = 1;
        }
    } else if (autoTestStep_ == 1) {
        player.SetDemoInput(true, {1.0f, -0.6f}, aim, true, false);
        if (autoTestTime_ >= 2.3f) {
            player.SetDemoInput(true, {0.0f, 0.0f}, aim, false, false);
            autoTestStep_ = 2;
        }
    } else if (autoTestStep_ == 2) {
        if (autoTestTime_ >= 2.6f) {
            capture("moved");
            autoTestStep_ = 3;
        }
    } else if (autoTestStep_ == 3 && autoTestTime_ >= 3.0f && world_.run.tankRunCapturePath_.empty()) {
        const myEngine::Vector3 end = player.GetWorldPosition();
        std::ofstream(std::string(kAutoTestDirectory) + "/autotest.json")
            << "{\"completed\":true,\"start\":[" << autoTestStart_.x << "," << autoTestStart_.y << "],\"end\":[" << end.x << "," << end.y
            << "],\"enemies\":" << world_.resources.enemyManager_->GetEnemyCount()
            << ",\"rivalActive\":" << (world_.gameplayQueries->IsRunRivalActive() ? "true" : "false")
            << ",\"menuOpen\":" << (world_.arenaRunController->IsTankRunMenuOpen() ? "true" : "false")
            << ",\"playerDead\":" << (player.IsDead() ? "true" : "false") << "}\n";
        autoTestStep_ = 4;
        PostQuitMessage(0);
    }
}

} // namespace gameplay
