#include "game/run/session/SurvivorRunController.h"
#include "game/session/GameplaySystems.h"
#include "game/player/TankRunModifiers.h"
#include "game/render/settings/ArenaCameraProfile.h"
#include <array>
#include <filesystem>
#include <fstream>

namespace gameplay {

namespace {
constexpr const char* kAutoTestDirectory = "generated/tank_survivor";
// 格子 45×30 全体を外壁で囲んだ部屋（約90×60）。画面の約2×2.4倍の広さ。
constexpr const char* kArenaMap = "resources/maps/survivor_arena.csv";
const myEngine::Vector3 kPlayerStart{44.0f, 28.0f, 0.0f};

struct AutoTestWaypoint {
    const char* name;
    myEngine::Vector3 target;
};

// 中央から四隅を回って中央へ戻る。柱の並ぶ線（x=16〜18・30・58・70〜72、y=8・28〜30・50）を避けた経路にする。
const std::array<AutoTestWaypoint, 5> kAutoTestWaypoints{{{"northeast", {82, 54, 0}},
                                                          {"southeast", {82, 4, 0}},
                                                          {"southwest", {6, 4, 0}},
                                                          {"northwest", {6, 54, 0}},
                                                          {"center", {44, 28, 0}}}};
// 入力を止めても少し滑るので、着いたかどうかはこの距離で見る。
constexpr float kAutoTestReachDistance = 4.0f;
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

    // 試作用の広い部屋に差し替え、自機とカメラを中央に置く。カメラは CombatFramePipeline が自機を追い、
    // 部屋の外枠（ArenaBounds）からはみ出さないように止める。
    if (!world_.resources.stage_->LoadRunMap(kArenaMap)) {
        OutputDebugStringA("[Survivor] arena map could not be loaded\n");
    }
    world_.presentation.stagePostCacheValid_ = false;
    world_.resources.player_->ResetRunRoomState(kPlayerStart);
    world_.resources.camera->SetTranslate({kPlayerStart.x, kPlayerStart.y, world_.resources.camera->GetTranslate().z});
    world_.resources.camera->Update();

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

float SurvivorRunController::MeasureAimRoundTripError() const
{
    const myEngine::Vector3 target = world_.resources.player_->GetWorldPosition() + myEngine::Vector3{5.0f, 3.0f, 0.0f};
    const myEngine::Vector2 screen = world_.gameplayHud->WorldToScreen(target);
    const myEngine::Vector3 back =
        myEngine::ScreenToWorldOnZ0(screen, world_.resources.camera->GetViewMatrix(), world_.resources.camera->GetProjectionMatrix(),
                                    myEngine::WinApp::kClientWidth, myEngine::WinApp::kClientHeight);
    return myEngine::Length(myEngine::Vector3{back.x - target.x, back.y - target.y, 0.0f});
}

void SurvivorRunController::UpdateAutoTest(float dt)
{
    // 各地点へまっすぐ進み、着いたら（または10秒で）止まってカメラが落ち着くのを待ち、画面を保存する。
    auto& player = *world_.resources.player_;
    autoTestTime_ += dt;
    autoTestLegTime_ += dt;
    const myEngine::Vector3 position = player.GetWorldPosition();
    const myEngine::Vector3 aim = position + myEngine::Vector3{6.0f, 0.0f, 0.0f};

    if (autoTestCaptureQueued_) {
        // 保存の要求は描画で写され、次の更新の初めに書き出されて空に戻る。
        if (!world_.run.tankRunCapturePath_.empty()) {
            return;
        }
        autoTestCaptureQueued_ = false;
        autoTestSettling_ = false;
        autoTestLegTime_ = 0.0f;
        ++autoTestWaypoint_;
    }

    if (autoTestWaypoint_ >= kAutoTestWaypoints.size()) {
        float minX = 0, minY = 0, maxX = 0, maxY = 0;
        arenacamera::ArenaBounds(world_.resources.stage_->GetBlocks(), minX, minY, maxX, maxY);
        std::ofstream(std::string(kAutoTestDirectory) + "/autotest.json")
            << "{\"completed\":true,\"arena\":[" << minX << "," << minY << "," << maxX << "," << maxY << "],\"halfView\":["
            << arenacamera::HalfViewHeight() * arenacamera::kAspect << "," << arenacamera::HalfViewHeight() << "],\"waypoints\":["
            << autoTestLog_.str() << "],\"enemies\":" << world_.resources.enemyManager_->GetEnemyCount()
            << ",\"rivalActive\":" << (world_.gameplayQueries->IsRunRivalActive() ? "true" : "false")
            << ",\"menuOpen\":" << (world_.arenaRunController->IsTankRunMenuOpen() ? "true" : "false")
            << ",\"playerDead\":" << (player.IsDead() ? "true" : "false") << ",\"seconds\":" << autoTestTime_ << "}\n";
        autoTest_ = false;
        PostQuitMessage(0);
        return;
    }

    const AutoTestWaypoint& waypoint = kAutoTestWaypoints[autoTestWaypoint_];
    const myEngine::Vector3 offset{waypoint.target.x - position.x, waypoint.target.y - position.y, 0.0f};
    const float distance = myEngine::Length(offset);
    if (!autoTestSettling_ && (distance < 1.5f || autoTestLegTime_ > 10.0f)) {
        autoTestSettling_ = true;
        autoTestLegTime_ = 0.0f;
    }
    if (!autoTestSettling_) {
        player.SetDemoInput(true, {offset.x / distance, offset.y / distance}, aim, false, false);
        return;
    }
    player.SetDemoInput(true, {0.0f, 0.0f}, aim, false, false);
    if (autoTestLegTime_ < 1.2f) {
        return;
    }
    const myEngine::Vector3 camera = world_.resources.camera->GetTranslate();
    if (autoTestWaypoint_ > 0) {
        autoTestLog_ << ",";
    }
    autoTestLog_ << "{\"name\":\"" << waypoint.name << "\",\"target\":[" << waypoint.target.x << "," << waypoint.target.y
                 << "],\"player\":[" << position.x << "," << position.y << "],\"camera\":[" << camera.x << "," << camera.y
                 << "],\"reached\":" << (distance < kAutoTestReachDistance ? "true" : "false") << ",\"aimError\":" << MeasureAimRoundTripError() << "}";
    world_.run.tankRunCapturePath_ = std::string(kAutoTestDirectory) + "/" + waypoint.name + ".png";
    autoTestCaptureQueued_ = true;
}

} // namespace gameplay
