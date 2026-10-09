#include "game/run/session/ExpeditionController.h"
#include "game/session/GameplaySystems.h"
#include "game/weapon/CombatTypes.h"
#include "StartupTrace.h"
#include "game/run/TankRunCopy.h"
#include "game/run/TankExpeditionEncounters.h"
#include "game/run/TankSubmissionValidation.h"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace gameplay {

namespace {
using EPhase = tankexp::Phase;
using Room = tankexp::RoomKind;
using namespace tankrun::copy;
/// @brief 現在の部屋の表示名を返す。
const char* RoomName(Room room)
{
    switch (room) {
    case Room::Skirmish:
        return "外周警備";
    case Room::Resource:
        return "宝物庫";
    case Room::Elite:
        return "精鋭部隊";
    case Room::Reflection:
        return "反射実験区画";
    case Room::Drone:
        return "ドローン格納庫";
    case Room::Guard:
        return "制御装置の制圧";
    default:
        return "最深部 / 最終決戦";
    }
}
/// @brief 生存している戦闘対象の数を返す。
int LivingThreats(EnemyManager* manager)
{
    int count = 0;
    for (auto* actor : manager->GetEnemyPtrs())
        if (actor && actor->IsCombatThreat())
            ++count;
    return count;
}
/// @brief 遠征検証の出力先ディレクトリーを返す。
std::string ExpeditionDirectory(int variant)
{
    return "generated/tank_expedition/variant_" + std::to_string(variant);
}
/// @brief チュートリアル設定の保存先パスを返す。
const char* TutorialSettingsPath()
{
    return "resources/configs/expedition_user.json";
}
} // namespace

void ExpeditionController::InitializeTankExpedition()
{
    cg2::StartupTrace::Scope scope("Expedition.Initialize");
    world_.resources.player_->SetRunCheckpointEvolution(true);
    world_.resources.enemy_->SetRunEncounterEnabled(false);
    world_.resources.enemyManager_->ClearRunActors();
    world_.resources.bulletManager_->ClearAll();
    world_.resources.stage_->LoadRunMap("resources/maps/expedition_crossfire.csv");
    world_.presentation.stagePostCacheValid_ = false;
    world_.resources.player_->ResetRunRoomState({26, 28, 0});
    world_.resources.camera->SetTranslate({26, 28, world_.resources.camera->GetTranslate().z});
    world_.resources.camera->Update();
    cg2::TextStyle style{};
    style.fontFamily = "Meiryo";
    style.fontSize = 14;
    style.color = {0.68f, 0.86f, 0.95f, 1};
    style.outlineThickness = 0;
    style.padding = 4;
    world_.run.tankExpeditionMapText_ = std::make_unique<cg2::TextLabel>();
    world_.run.tankExpeditionMapText_->Initialize(cg2::SpriteCommon::GetInstance(), " ", style);
    world_.run.tankExpeditionMapText_->SetPosition({24, 670});
    style.fontSize = 17;
    style.color = {0.78f, 0.96f, 1, 1};
    world_.run.tankExpeditionMaintenanceText_ = std::make_unique<cg2::TextLabel>();
    world_.run.tankExpeditionMaintenanceText_->Initialize(cg2::SpriteCommon::GetInstance(), " ", style);
    world_.run.tankExpeditionMaintenanceText_->SetPosition({76, 570});
    world_.run.tankExpeditionMaintenanceButton_ = std::make_unique<cg2::Sprite>();
    world_.run.tankExpeditionMaintenanceButton_->Initialize(cg2::SpriteCommon::GetInstance(), "resources/white512x512.png");
    world_.run.tankExpeditionMaintenanceButton_->SetPosition({64, 568});
    world_.run.tankExpeditionMaintenanceButton_->SetSize({1144, 32});
    world_.run.tankExpeditionMaintenanceButton_->SetColor({0.035f, 0.105f, 0.14f, 1});
    world_.run.tankExpeditionMaintenanceButton_->Update();
    auto panel = [](cg2::Vector2 position, cg2::Vector2 size, const cg2::Vector4& color) {
        auto item = std::make_unique<cg2::Sprite>();
        item->Initialize(cg2::SpriteCommon::GetInstance(), "resources/white512x512.png");
        item->SetPosition(position);
        item->SetSize(size);
        item->SetColor(color);
        item->Update();
        return item;
    };
    auto label = [](float size, cg2::Vector2 position, const cg2::Vector4& color) {
        cg2::TextStyle s{};
        s.fontFamily = "Meiryo";
        s.fontSize = size;
        s.color = color;
        s.outlineThickness = 0;
        s.padding = 4;
        auto item = std::make_unique<cg2::TextLabel>();
        item->Initialize(cg2::SpriteCommon::GetInstance(), " ", s);
        item->SetPosition(position);
        return item;
    };
    world_.run.tankExpeditionHpTrack_ = panel({24, 53}, {230, 6}, {0.10f, 0.16f, 0.21f, 0.9f});
    world_.run.tankExpeditionHpFill_ = panel({24, 53}, {230, 6}, {0.23f, 1.0f, 0.65f, 1});
    world_.run.tankExpeditionExpTrack_ = panel({24, 702}, {1232, 6}, {0.10f, 0.16f, 0.21f, 0.9f});
    world_.run.tankExpeditionExpFill_ = panel({24, 702}, {1, 6}, {0.24f, 0.73f, 1, 1});
    world_.run.tankExpeditionBuildPanel_ = panel({988, 10}, {280, 150}, {0.009f, 0.016f, 0.03f, 0.60f});
    world_.run.tankExpeditionExpText_ = label(12, {24, 677}, {0.68f, 0.83f, 0.93f, 1});
    world_.run.tankExpeditionDetailsText_ = label(18, {280, 140}, {0.83f, 0.94f, 1, 1});
    // Reuse the existing Tutorial UI primitives, but keep expedition progression
    // separate from the arena's combat-suppressing introduction.
    world_.combat.tutorialPanel_ = panel({410, 596}, {460, 70}, {0.008f, 0.025f, 0.04f, 0.84f});
    world_.combat.tutorialTitleText_ = label(12, {640, 601}, {0.43f, 0.81f, 0.93f, 1});
    world_.combat.tutorialInputText_ = label(20, {640, 620}, {0.93f, 1, 1, 1});
    world_.combat.tutorialDescriptionText_ = label(12, {640, 651}, {0.68f, 0.84f, 0.91f, 1});
    world_.combat.tutorialTitleText_->SetAnchorPoint({0.5f, 0});
    world_.combat.tutorialInputText_->SetAnchorPoint({0.5f, 0});
    world_.combat.tutorialDescriptionText_->SetAnchorPoint({0.5f, 0});
    bool completed = false;
    try {
        std::ifstream f(TutorialSettingsPath());
        if (f) {
            nlohmann::json j;
            f >> j;
            completed = j.value("tutorialCompleted", false);
        }
    }
    catch (...) {
    }
    world_.run.expeditionTutorialPreviouslyCompleted_ = completed;
    wchar_t tutorialMode[8]{};
    const bool forceTutorial = GetEnvironmentVariableW(L"CG2_EXPEDITION_TUTORIAL", tutorialMode, 8) > 0 && tutorialMode[0] == L'1';
    wchar_t validateTutorial[8]{};
    world_.run.tankExpeditionTutorialValidation_.enabled =
        !world_.demo.titleDemo_ && GetEnvironmentVariableW(L"CG2_TANK_TUTORIAL_AUTOTEST", validateTutorial, 8) > 0 &&
        validateTutorial[0] == L'1';
    if (world_.run.tankExpeditionTutorialValidation_.enabled) {
        world_.run.tankRunAutoTest_ = false;
        completed = false;
        world_.presentation.debugPlayerNoDamage_ = true;
        std::filesystem::create_directories("generated/tank_expedition/tutorial_validation");
        std::ofstream("generated/tank_expedition/tutorial_validation/validation.json") << "{\"completed\":false}\n";
    }
    world_.run.tankExpeditionTutorial_.Reset(completed, world_.demo.titleDemo_ || (world_.run.tankRunAutoTest_ && !forceTutorial));
    world_.run.tankExpeditionTutorialPrevious_ = world_.resources.player_->GetWorldPosition();
    world_.run.tankExpeditionTutorialKills_ = world_.combat.defeatedEnemies_;
    world_.expeditionBalanceEditor->InitializeTankExpeditionBalance();
    if (!world_.demo.titleDemo_ && !world_.run.tankRunAutoTest_ && !world_.run.tankExpeditionTutorialValidation_.enabled)
        world_.expeditionMapController->InitializeExpeditionMap();
    wchar_t variant[16]{};
    if (GetEnvironmentVariableW(L"CG2_TANK_EXPEDITION_VARIANT", variant, 16) > 0)
        world_.run.tankExpeditionAutoVariant_ = (std::clamp)(_wtoi(variant), 0, 5);
    if (world_.run.tankRunAutoTest_) {
        std::filesystem::create_directories(ExpeditionDirectory(world_.run.tankExpeditionAutoVariant_));
        std::ofstream(ExpeditionDirectory(world_.run.tankExpeditionAutoVariant_) + "/validation.json")
            << "{\"completed\":false,\"testMode\":true}\n";
    }
}

void ExpeditionController::StartTankExpeditionRoom()
{
    if (!world_.run.tankExpedition_.IsCombat())
        return;
    // 更新の冒頭か停止中のメニューから部屋を開始する。資源の命中コールバックは完了待ちなどを更新する。
    // ここで前の敵・弾・場の攻撃を消去し、衝突走査中に対象の実体を削除しない。
    world_.resources.enemy_->SetRunEncounterEnabled(false);
    world_.run.tankExpeditionRivalActive_ = false;
    world_.resources.enemyManager_->ClearRunActors();
    world_.resources.bulletManager_->ClearAll();
    world_.presentation.playerLaserBeams_.clear();
    world_.presentation.playerMines_.clear();
    world_.presentation.playerMineExplosions_.clear();
    world_.presentation.playerMeleeSlashes_.clear();
    world_.presentation.playerNeonAfterimages_.clear();
    world_.presentation.neonTriangleParticles_.clear();
    if (world_.resources.playerMeleeTrailManager_)
        world_.resources.playerMeleeTrailManager_->ClearInstances();
    world_.combat.hpBarVisibility_.clear();
    world_.run.tankRunBursts_.clear();
    for (auto& node : world_.run.tankRunResources_)
        node = {};
    world_.run.tankExpeditionRoomPending_ = false;
    world_.run.tankExpeditionResourceWon_ = false;
    world_.run.tankExpeditionRewardOpen_ = false;
    world_.run.tankExpeditionNodes_ = 0;
    world_.run.tankExpeditionSpawned_ = 0;
    world_.run.tankExpeditionMaintenanceOpen_ = false;
    world_.run.tankExpeditionResourceReleased_ = false;
    world_.run.tankExpeditionEnemyHp_.clear();
    world_.run.tankExpeditionEnemyWarning_.clear();
    world_.run.tankExpeditionArrival_ = 0;
    world_.run.tankRunComboTime_ = 0;
    const Room room = world_.run.tankExpedition_.GetRoomKind();
    const char* map = room == Room::Resource || room == Room::Drone     ? "resource_fork"
                      : room == Room::Elite || room == Room::Reflection ? "hazard_lane"
                      : room == Room::Boss                              ? "final_duel"
                                                                        : "crossfire";
    if (!world_.resources.stage_->LoadRunMap(std::string("resources/maps/expedition_") + map + ".csv"))
        OutputDebugStringA("[TankExpedition] map could not be loaded\n");
    world_.levelRuntime->ApplyTankExpeditionRoomGeometry();
    world_.presentation.stagePostCacheValid_ = false;
    // この部屋のダメージ地形へ渡す値を12に設定する。衝突時の位置補正とダメージ適用は別の処理。
    world_.resources.stage_->SetDamageBlockDamage(12);
    // 生存中で遠征成長が有効な自機の部屋状態と位置をリセットする。機体/成長・現在HP・カード等は保持する。
    world_.resources.player_->ResetRunRoomState({26, 28, 0});
    world_.run.tankExpeditionTutorialPrevious_ = world_.resources.player_->GetWorldPosition();
    world_.run.tankExpeditionDetailsOpen_ = false;
    world_.resources.camera->SetTranslate({26, 28, world_.resources.camera->GetTranslate().z});
    world_.resources.camera->Update();
    auto freePosition = [this](const cg2::Vector3& desired) {
        if (!world_.resources.stage_->IsCollisionWithAnyBlock(desired, 1.3f))
            return desired;
        for (int r = 2; r <= 10; r += 2)
            for (int y = -r; y <= r; y += 2)
                for (int x = -r; x <= r; x += 2) {
                    cg2::Vector3 p = desired + cg2::Vector3{static_cast<float>(x), static_cast<float>(y), 0};
                    if (p.x > 23 && p.x < 65 && p.y > 17 && p.y < 41 && !world_.resources.stage_->IsCollisionWithAnyBlock(p, 1.3f))
                        return p;
                }
        return cg2::Vector3{44, 30, 0};
    };
    const std::array<cg2::Vector3, 6> shapes = {cg2::Vector3{34, 22, 0}, cg2::Vector3{44, 22, 0}, cg2::Vector3{54, 22, 0},
                                                cg2::Vector3{34, 36, 0}, cg2::Vector3{44, 36, 0}, cg2::Vector3{54, 36, 0}};
    const auto encounter = tankexp::GetEncounter(room, world_.run.tankExpedition_.GetRoomIndex());
    for (int i = 0; i < encounter.count; ++i) {
        const auto& unit = encounter.units[i];
        if (world_.resources.enemyManager_->SpawnLevelEnemy(freePosition({unit.x, unit.y, 0}), unit.prefab, unit.hp))
            ++world_.run.tankExpeditionSpawned_;
    }
    const int shapeCount = room == Room::Boss ? 2 : 4;
    for (int i = 0; i < shapeCount; ++i)
        world_.resources.enemyManager_->SpawnLevelEnemy(freePosition(shapes[i]), i % 2 ? "Triangle" : "Square", 8);
    if (room == Room::Resource)
        world_.run.tankRunResources_[0].position = freePosition({44, 30, 0});
    if (room == Room::Guard) {
        const int count = 3;
        const std::array<cg2::Vector3, 3> centers = {cg2::Vector3{44, 30, 0}, cg2::Vector3{36, 38, 0}, cg2::Vector3{54, 20, 0}};
        for (int i = 0; i < count; ++i) {
            auto& node = world_.run.tankRunResources_[i];
            node.position = freePosition(centers[i]);
            node.active = world_.resources.enemyManager_->SpawnRunResource(node.position, 60, [this, i](bool owned) {
                world_.arenaRunController->OnTankRunResourceClaim(i, owned);
            });
        }
    }
    if (room == Room::Resource || room == Room::Boss) {
        world_.run.tankExpeditionRivalActive_ = true;
        const int configuredBossHp = world_.run.tankExpeditionBalance_.value("bossMaxHp", 900);
        world_.resources.enemy_->ResetRunEncounter(
            {62, 30, 0}, room == Room::Boss ? configuredBossHp : (std::max)(1, static_cast<int>(std::round(configuredBossHp * 0.52f))),
            room == Room::Boss ? 1 : 0, true);
        auto progress = world_.resources.enemy_->GetEnemyProgressConfig();
        progress.levelingModeEnabled = room == Room::Resource;
        world_.resources.enemy_->SetEnemyProgressConfig(progress);
        if (room == Room::Boss)
            world_.combat.screenEffectDirector_.TriggerBossEntry();
    }
    world_.expeditionBalanceEditor->ApplyTankExpeditionRoomBalance();
    world_.depthEncounter->ConfigureNeonDepthEncounter();
    world_.combat.previousPlayerHp_ = world_.resources.player_->GetHp();
    world_.combat.previousBossHp_ = world_.resources.enemy_->GetHp();
    world_.combat.bossDefeatHandled_ = false;
    world_.run.tankRunSelection_ = 0;
    world_.run.tankRunMenuAge_ = -0.15f;
    if (!world_.resources.enemy_->IsNeonDepthEncounterEnabled())
        world_.combatFlow->SetEventCallout(
            std::string("第") + std::to_string(world_.run.tankExpedition_.GetRoomIndex() + 1) + "区画 / " + RoomName(room), 1.6f);
    RefreshTankExpeditionUi();
}

void ExpeditionController::FinishTankExpeditionRoom()
{
    // マップ式と従来の遠征で完了処理を分ける。死亡時・非戦闘時・ボス部屋はこの入口では完了しない。
    if (world_.run.expeditionMapEnabled_) {
        world_.expeditionMapController->CompleteExpeditionMapCombat();
        return;
    }
    if (world_.resources.player_->IsDead() || !world_.run.tankExpedition_.IsCombat() ||
        world_.run.tankExpedition_.GetRoomKind() == Room::Boss)
        return;
    if (!world_.run.tankExpedition_.CompleteRoom())
        return;
    // 進行を報酬/進路選択へ切り替えるとIsTankRunMenuOpenが戦闘を止める。実体の消去は次の部屋開始時。
    world_.run.tankExpeditionTutorial_.RecordRoomClear();
    world_.resources.player_->AwardRunMaintenancePoint(world_.run.tankExpedition_.GetRoomIndex() + 1);
    world_.run.tankExpeditionMaintenanceOpen_ = false;
    world_.run.tankExpeditionRoomPending_ = false;
    world_.run.tankExpeditionRewardOpen_ =
        world_.run.tankExpedition_.GetPhase() == EPhase::Reward &&
        world_.run.tankRun_.OpenExpeditionRewardDraft(world_.run.tankExpedition_.GetRoomIndex(),
                                                      world_.run.tankExpedition_.GetRewardRare() || world_.run.tankExpeditionResourceWon_,
                                                      world_.run.tankExpedition_.GetRewardAffinity());
    world_.run.tankRunSelection_ = 0;
    world_.run.tankRunMenuAge_ = 0;
    world_.run.tankExpeditionAudio_.Upgrade();
    world_.combatFlow->SetEventCallout(
        world_.run.tankExpedition_.GetPhase() == EPhase::Route ? "区画突破 / 次の進路を選ぼう" : "区画突破 / 改造 ＋ 整備ポイント", 1.2f);
    RefreshTankExpeditionUi();
}

int ExpeditionController::GetTankExpeditionOptionCount() const
{
    if (world_.run.tankRunPaused_)
        return 2;
    switch (world_.run.tankExpedition_.GetPhase()) {
    case EPhase::Dormant:
        return 3;
    case EPhase::Reward:
        return world_.run.tankExpeditionMaintenanceOpen_ ? 3
               : world_.run.tankExpeditionRewardOpen_    ? static_cast<int>(world_.run.tankRun_.GetOfferCount())
                                                         : 1;
    case EPhase::Evolution:
        return (std::min)(3, static_cast<int>(world_.run.tankExpeditionEvolutions_.size()) + 1);
    case EPhase::Route:
    case EPhase::Event:
    case EPhase::Clear:
    case EPhase::Dead:
        return 2;
    default:
        return 0;
    }
}

void ExpeditionController::SelectTankExpeditionOption(int index)
{
    if (index < 0 || index >= GetTankExpeditionOptionCount())
        return;
    const EPhase before = world_.run.tankExpedition_.GetPhase();
    if (before == EPhase::Reward) {
        if (world_.run.tankExpeditionMaintenanceOpen_) {
            if (world_.resources.player_->SpendRunMaintenancePoint(index))
                world_.run.tankExpeditionAudio_.Upgrade();
            else
                world_.combatFlow->SetEventCallout("整備ポイント不足、または強化上限です", 0.8f);
            RefreshTankExpeditionUi();
            return;
        }
        if (world_.run.tankExpeditionRewardOpen_) {
            const auto card = world_.run.tankRun_.GetOffers()[index];
            if (!world_.run.tankRun_.ChooseCard(index))
                return;
            world_.run.tankExpeditionTutorial_.RecordUpgrade();
            world_.arenaRunController->ApplyTankRunCards();
            if (card == tankrun::CardId::Repair)
                world_.resources.player_->HealRunPlayer(30);
            world_.combat.screenEffectDirector_.TriggerUpgradeConfirmed(
                world_.gameplayQueries->WorldToScreenUv(world_.resources.player_->GetWorldPosition()));
            world_.run.tankExpeditionAudio_.Upgrade();
        }
        world_.run.tankExpeditionRewardOpen_ = false;
        world_.run.tankExpedition_.ChooseRewardDone();
    } else if (before == EPhase::Route) {
        if (!world_.run.tankExpedition_.ChooseRoute(index))
            return;
        world_.run.tankExpeditionTutorial_.RecordRoute();
        if (world_.run.tankExpedition_.GetPhase() == EPhase::Reward)
            world_.run.tankExpeditionRewardOpen_ = world_.run.tankRun_.OpenExpeditionRewardDraft(
                world_.run.tankExpedition_.GetRoomIndex(), world_.run.tankExpedition_.GetRewardRare(),
                world_.run.tankExpedition_.GetRewardAffinity());
    } else if (before == EPhase::Event) {
        if (index == 1 && !world_.resources.player_->SpendRunHealth(20)) {
            world_.combatFlow->SetEventCallout("HPが21以上あると選べます", 1.3f);
            return;
        }
        if (index == 0)
            world_.resources.player_->HealRunPlayer(40);
        world_.run.tankExpedition_.ChooseEvent(index);
        if (index == 1)
            world_.run.tankExpeditionRewardOpen_ =
                world_.run.tankRun_.OpenExpeditionRewardDraft(world_.run.tankExpedition_.GetRoomIndex(), true);
        world_.combat.previousPlayerHp_ = world_.resources.player_->GetHp();
    } else if (before == EPhase::Evolution) {
        const int hp = world_.resources.player_->GetHp();
        const std::array<int, 3> ranks = {world_.resources.player_->GetRunMaintenanceRank(0),
                                          world_.resources.player_->GetRunMaintenanceRank(1),
                                          world_.resources.player_->GetRunMaintenanceRank(2)};
        const auto cards = world_.run.tankRun_.GetCardCounts();
        if (index < static_cast<int>(world_.run.tankExpeditionEvolutions_.size()) &&
            !world_.resources.player_->ChooseRunEvolution(world_.run.tankExpeditionEvolutions_[index].id))
            return;
        if (world_.run.tankRunAutoTest_) {
            if (hp != world_.resources.player_->GetHp() || cards != world_.run.tankRun_.GetCardCounts())
                ++world_.run.tankExpeditionValidationErrors_;
            for (int i = 0; i < 3; ++i)
                if (ranks[i] != world_.resources.player_->GetRunMaintenanceRank(i))
                    ++world_.run.tankExpeditionValidationErrors_;
        }
        world_.run.tankExpedition_.CompleteEvolution();
    } else
        return;
    if (world_.run.tankExpedition_.GetPhase() == EPhase::Evolution) {
        world_.resources.player_->PrepareRunEvolution();
        world_.run.tankExpeditionEvolutions_ = world_.resources.player_->GetRunEvolutionChoices();
    }
    world_.run.tankExpeditionMaintenanceOpen_ = false;
    world_.run.tankRunSelection_ = 0;
    world_.run.tankRunMenuAge_ = -0.12f;
    if (world_.run.tankExpedition_.IsCombat())
        StartTankExpeditionRoom();
    RefreshTankExpeditionUi();
}

void ExpeditionController::UpdateTankExpedition(float dt)
{
    if (world_.demo.titleDemo_)
        return;
    UpdateTankExpeditionAudio(dt);
    if (world_.run.expeditionMapEnabled_) {
        world_.expeditionMapController->UpdateExpeditionMap(dt);
        return;
    }
    if (world_.combat.phase_ != Phase::kMain)
        return;
    if (world_.run.tankExpeditionBalanceEditorOpen_ || (cg2::kDeveloperTools && world_.resources.input_->IsKeyTriggered(DIK_F2)))
        return;
    world_.run.tankRunMenuAge_ += dt;
    world_.run.tankRunAutoTime_ += dt;
    if (world_.run.tankExpeditionTutorialValidation_.enabled)
        UpdateTankExpeditionTutorialValidation(dt);
    const auto triggered = [this](int key) {
        return world_.resources.input_->IsKeyTriggered(static_cast<uint8_t>(key));
    };
    if (world_.run.tankExpedition_.IsCombat() && !world_.run.tankRunPaused_ &&
        world_.combat.combatFlow_.GetState() == GameFlowState::Playing && triggered(DIK_TAB)) {
        world_.run.tankExpeditionDetailsOpen_ = !world_.run.tankExpeditionDetailsOpen_;
        RefreshTankExpeditionUi();
    }
    if (world_.run.tankExpeditionDetailsOpen_) {
        if (triggered(DIK_ESCAPE))
            world_.run.tankExpeditionDetailsOpen_ = false;
        return;
    }
    if (!world_.run.tankRunPaused_ && world_.combat.combatFlow_.GetState() == GameFlowState::Playing)
        UpdateTankExpeditionTutorial(dt);
    if (cg2::kDeveloperTools && triggered(DIK_F10))
        world_.arenaRunController->RequestTankRunCapture("manual");
    if (triggered(DIK_M)) {
        world_.run.tankExpeditionMusicEnabled_ = !world_.run.tankExpeditionMusicEnabled_;
        world_.run.tankExpeditionAudio_.SetMusicVolume(world_.run.tankExpeditionMusicEnabled_ ? 0.55f : 0.0f);
        world_.combatFlow->SetEventCallout(world_.run.tankExpeditionMusicEnabled_ ? "BGM ON" : "BGM OFF", 0.7f);
    }
    if (triggered(DIK_N)) {
        world_.run.tankExpeditionEffectsEnabled_ = !world_.run.tankExpeditionEffectsEnabled_;
        world_.run.tankExpeditionAudio_.SetEffectsVolume(world_.run.tankExpeditionEffectsEnabled_ ? 0.80f : 0.0f);
        world_.combatFlow->SetEventCallout(world_.run.tankExpeditionEffectsEnabled_ ? "SE ON" : "SE OFF", 0.7f);
    }
    if (world_.combat.combatFlow_.GetState() == GameFlowState::Playing && !world_.resources.player_->IsChangeMode() &&
        triggered(DIK_ESCAPE)) {
        world_.run.tankRunPaused_ = !world_.run.tankRunPaused_;
        world_.run.tankRunSelection_ = 0;
        world_.run.tankRunMenuAge_ = 0;
        world_.arenaRunController->RefreshTankRunUi();
    }
    if (world_.combat.combatFlow_.GetState() != GameFlowState::Playing) {
        if (world_.run.tankRunAutoTest_ && world_.combat.combatFlow_.GetState() == GameFlowState::StageClear) {
            if (world_.run.tankRunAutoStep_ < 100) {
                world_.arenaRunController->RequestTankRunCapture("result");
                world_.run.tankRunAutoStep_ = 100;
                world_.run.tankRunAutoTime_ = 0;
            }
            if (world_.run.tankRunAutoTime_ > 1.2f) {
                std::ofstream log(ExpeditionDirectory(world_.run.tankExpeditionAutoVariant_) + "/validation.json");
                log << "{\"completed\":true,\"testMode\":true,\"rooms\":" << world_.run.tankExpedition_.GetRoomIndex() + 1
                    << ",\"routes\":[" << world_.run.tankExpedition_.GetRouteChoice(0) << ","
                    << world_.run.tankExpedition_.GetRouteChoice(1) << "],\"event\":" << world_.run.tankExpedition_.GetEventChoice()
                    << ",\"rewards\":" << world_.run.tankExpedition_.GetRewardCount()
                    << ",\"playerHp\":" << world_.resources.player_->GetHp() << ",\"maxHp\":" << world_.resources.player_->GetMaxHp()
                    << ",\"maintenanceRemaining\":" << world_.resources.player_->GetRunMaintenancePoints() << ",\"maintenanceRanks\":["
                    << world_.resources.player_->GetRunMaintenanceRank(0) << "," << world_.resources.player_->GetRunMaintenanceRank(1)
                    << "," << world_.resources.player_->GetRunMaintenanceRank(2)
                    << "],\"validationErrors\":" << world_.run.tankExpeditionValidationErrors_
                    << ",\"audioLoaded\":" << world_.run.tankExpeditionAudio_.LoadedClipCount()
                    << ",\"audioEvents\":" << world_.run.tankExpeditionAudio_.PlayCount()
                    << ",\"musicPlaying\":" << (world_.run.tankExpeditionAudio_.IsMusicPlaying() ? "true" : "false") << ",\"class\":\""
                    << world_.resources.player_->GetCurrentClassName() << "\",\"cards\":[";
                for (size_t i = 0; i < tankrun::CardCount; ++i)
                    log << (i ? "," : "") << world_.run.tankRun_.GetCardCounts()[i];
                log << "]}\n";
                log.close();
                PostQuitMessage(0);
            }
        }
    } else if (!world_.resources.player_->IsChangeMode()) {
        const EPhase phase = world_.run.tankExpedition_.GetPhase();
        const bool menu = world_.run.tankRunPaused_ || phase == EPhase::Dormant || phase == EPhase::Reward || phase == EPhase::Route ||
                          phase == EPhase::Event || phase == EPhase::Evolution;
        if (menu) {
            int count = GetTankExpeditionOptionCount();
            if (world_.run.tankRunMenuAge_ > 0.16f && count > 0) {
                if (triggered(DIK_LEFT) || triggered(DIK_A))
                    world_.run.tankRunSelection_ = (world_.run.tankRunSelection_ + count - 1) % count;
                if (triggered(DIK_RIGHT) || triggered(DIK_D))
                    world_.run.tankRunSelection_ = (world_.run.tankRunSelection_ + 1) % count;
                const auto mouse = world_.resources.input_->GetMousePosition();
                const auto motion = world_.resources.input_->GetMouseState();
                int hovered = -1;
                const bool leftClick =
                    world_.resources.input_->IsTrigger(motion.rgbButtons[0], world_.resources.input_->GetPreMouseState().rgbButtons[0]);
                if (!world_.run.tankRunPaused_ && phase == EPhase::Reward &&
                    (triggered(DIK_E) || (leftClick && mouse.x >= 64 && mouse.x <= 1208 && mouse.y >= 568 && mouse.y <= 600))) {
                    world_.run.tankExpeditionMaintenanceOpen_ = !world_.run.tankExpeditionMaintenanceOpen_;
                    world_.run.tankRunSelection_ = 0;
                    count = GetTankExpeditionOptionCount();
                    RefreshTankExpeditionUi();
                }
                for (int i = 0; i < count; ++i) {
                    const float x = 64 + static_cast<float>(i) * 388;
                    if (mouse.x >= x && mouse.x <= x + 368 && mouse.y >= 280 && mouse.y <= 560)
                        hovered = i;
                }
                if (hovered >= 0 && (motion.lX || motion.lY))
                    world_.run.tankRunSelection_ = hovered;
                bool confirm = triggered(DIK_RETURN) || triggered(DIK_SPACE);
                for (int i = 0; i < count; ++i)
                    if (triggered(DIK_1 + i)) {
                        world_.run.tankRunSelection_ = i;
                        confirm = true;
                    }
                if (hovered >= 0 && leftClick) {
                    world_.run.tankRunSelection_ = hovered;
                    confirm = true;
                }
                const bool refund =
                    !world_.run.tankRunPaused_ && world_.run.tankExpeditionMaintenanceOpen_ &&
                    ((confirm && (world_.resources.input_->GetKey()[DIK_LSHIFT] || world_.resources.input_->GetKey()[DIK_RSHIFT])) ||
                     (hovered >= 0 &&
                      world_.resources.input_->IsTrigger(motion.rgbButtons[1], world_.resources.input_->GetPreMouseState().rgbButtons[1])));
                if (refund) {
                    if (hovered >= 0 &&
                        world_.resources.input_->IsTrigger(motion.rgbButtons[1], world_.resources.input_->GetPreMouseState().rgbButtons[1]))
                        world_.run.tankRunSelection_ = hovered;
                    world_.resources.player_->RefundRunMaintenancePoint(world_.run.tankRunSelection_);
                    RefreshTankExpeditionUi();
                } else if (confirm)
                    world_.arenaRunController->SelectTankRunOption(world_.run.tankRunSelection_);
            }
            if (world_.run.tankRunAutoTest_ && !world_.run.tankRunPaused_) {
                if (world_.run.tankRunMenuAge_ > 0.4f && world_.run.tankExpeditionCaptureIndex_ == world_.run.tankRunAutoMenuIndex_) {
                    world_.arenaRunController->RequestTankRunCapture("choice_" + std::to_string(world_.run.tankRunAutoMenuIndex_));
                    ++world_.run.tankExpeditionCaptureIndex_;
                }
                if (phase == EPhase::Reward && world_.run.tankRunMenuAge_ > 0.7f &&
                    world_.run.tankExpeditionAutoMaintainedRoom_ != world_.run.tankExpedition_.GetRoomIndex()) {
                    world_.run.tankExpeditionMaintenanceOpen_ = true;
                    const int rank = (world_.run.tankExpedition_.GetRoomIndex() + world_.run.tankExpeditionAutoVariant_) % 3;
                    SelectTankExpeditionOption(rank);
                    const int hp = world_.resources.player_->GetHp();
                    world_.resources.player_->RefundRunMaintenancePoint(rank);
                    SelectTankExpeditionOption(rank);
                    if (world_.resources.player_->GetHp() != hp)
                        ++world_.run.tankExpeditionValidationErrors_;
                    world_.run.tankExpeditionAutoMaintainedRoom_ = world_.run.tankExpedition_.GetRoomIndex();
                    world_.arenaRunController->RequestTankRunCapture("maintenance_" +
                                                                     std::to_string(world_.run.tankExpedition_.GetRoomIndex() + 1));
                }
                if (world_.run.tankRunMenuAge_ > 1.1f) {
                    world_.run.tankExpeditionMaintenanceOpen_ = false;
                    int option = 0;
                    const int loadouts[] = {0, 2, 0, 1, 1, 2};
                    const int evolutions[] = {0, 0, 1, 0, 1, 1};
                    if (phase == EPhase::Dormant)
                        option = loadouts[world_.run.tankExpeditionAutoVariant_];
                    if (phase == EPhase::Route)
                        option = world_.run.tankExpedition_.GetRouteRound() == 0 ? world_.run.tankExpeditionAutoVariant_ % 2
                                                                                 : (world_.run.tankExpeditionAutoVariant_ / 2) % 2;
                    if (phase == EPhase::Event)
                        option = world_.run.tankExpeditionAutoVariant_ % 3 == 0 ? 0 : 1;
                    if (phase == EPhase::Evolution)
                        option = evolutions[world_.run.tankExpeditionAutoVariant_];
                    world_.arenaRunController->SelectTankRunOption(option);
                    ++world_.run.tankRunAutoMenuIndex_;
                }
            }
        } else if (phase == EPhase::Combat && world_.run.tankRunMenuAge_ >= 0) {
            if (world_.resources.player_->IsDead()) {
                world_.combatFlow->BeginGameOver();
                return;
            }
            const Room room = world_.run.tankExpedition_.GetRoomKind();
            // 通常敵の撃破/資源取得コールバックで更新された状態から目標を判定する。
            // 死亡済みの敵は脅威数へ含めず、初回チュートリアルの退出条件も確認する。
            const int threats = LivingThreats(world_.resources.enemyManager_.get());
            if (room == Room::Resource && !world_.run.tankExpeditionResourceReleased_ && threats == 0 &&
                world_.run.tankExpeditionSpawned_ > 0) {
                auto& node = world_.run.tankRunResources_[0];
                node.active = world_.resources.enemyManager_->SpawnRunResource(node.position, 90, [this](bool owned) {
                    world_.arenaRunController->OnTankRunResourceClaim(0, owned);
                });
                world_.run.tankExpeditionResourceReleased_ = node.active;
                world_.combatFlow->SetEventCallout("敵を撃破！ 通貨ボックスが出現", 1.4f);
                world_.run.tankExpeditionAudio_.EnemyWarning();
            }
            if (room != Room::Boss && world_.run.tankExpeditionSpawned_ > 0 &&
                (world_.run.tankExpedition_.GetRoomIndex() != 0 || world_.run.tankExpeditionTutorial_.CanLeaveFirstRoom()) &&
                tankexp::IsRoomObjectiveComplete(room, threats, world_.run.tankExpeditionNodes_, world_.run.tankExpeditionRoomPending_,
                                                 world_.resources.enemy_->IsDead()))
                FinishTankExpeditionRoom();
            else {
                world_.run.tankExpedition_.Update(dt);
                world_.run.tankRun_.Update(dt);
                world_.run.tankExpeditionArrival_ += dt;
                world_.run.tankRunComboTime_ = (std::max)(0.0f, world_.run.tankRunComboTime_ - dt);
                for (auto& burst : world_.run.tankRunBursts_)
                    burst.age += dt;
                std::erase_if(world_.run.tankRunBursts_, [](const RunBurst& b) {
                    return b.age > (b.resource ? 0.7f : 0.35f);
                });
                // Deterministic rendering/transition smoke test; this does not measure balance.
                if (world_.run.tankRunAutoTest_) {
                    const int step = world_.run.tankExpedition_.GetRoomIndex() * 2;
                    if (world_.run.tankRunAutoStep_ == step && world_.run.tankExpeditionArrival_ > 1.5f) {
                        world_.arenaRunController->RequestTankRunCapture("room_" +
                                                                         std::to_string(world_.run.tankExpedition_.GetRoomIndex() + 1));
                        ++world_.run.tankRunAutoStep_;
                    }
                    if (world_.run.tankExpeditionArrival_ > 3 && room != Room::Boss) {
                        for (auto* actor : world_.resources.enemyManager_->GetEnemyPtrs())
                            if (actor && !actor->IsDead()) {
                                if (!actor->IsCombatThreat() && !actor->IsRunResource())
                                    continue;
                                if (room == Room::Resource && world_.run.tankExpeditionAutoVariant_ == 2)
                                    actor->TakeDamageFromEnemy(100000);
                                else
                                    actor->TakeDamageFromPlayer(100000);
                            }
                        world_.run.tankRunAutoStep_ = step + 2;
                    }
                    if (room == Room::Boss && world_.run.tankExpeditionArrival_ > 5)
                        world_.resources.enemy_->TakeDamage(100000);
                }
            }
        }
    }
    world_.run.tankRunHudTimer_ -= dt;
    if (world_.run.tankRunHudTimer_ <= 0) {
        world_.arenaRunController->RefreshTankRunUi();
        world_.run.tankRunHudTimer_ = 0.10f;
    }
}

void ExpeditionController::RefreshTankExpeditionUi()
{
    const EPhase phase = world_.run.tankExpedition_.GetPhase();
    const Room room = world_.run.tankExpedition_.GetRoomKind();
    const bool terminal = phase == EPhase::Clear || phase == EPhase::Dead;
    const bool inCombat = phase == EPhase::Combat && !world_.run.tankRunPaused_;
    world_.run.tankRunHudPanel_->SetSize({264, 78});
    world_.run.tankRunHudPanel_->SetColor({0.009f, 0.016f, 0.03f, 0.66f});
    world_.run.tankRunHudPanel_->Update();
    world_.run.tankRunHud_->SetText(
        "HP " + std::to_string(world_.resources.player_->GetHp()) + " / " + std::to_string(world_.resources.player_->GetMaxHp()) +
        (world_.run.expeditionMapEnabled_ ? "" : "   Lv." + std::to_string(world_.resources.player_->GetLevel())));
    auto hudStyle = world_.run.tankRunHud_->GetStyle();
    hudStyle.fontSize = 17;
    world_.run.tankRunHud_->SetStyle(hudStyle);
    world_.run.tankRunHud_->SetPosition({20, 16});
    const float playerHp = static_cast<float>(world_.resources.player_->GetHp()) / (std::max)(1, world_.resources.player_->GetMaxHp());
    world_.run.tankExpeditionHpFill_->SetSize({230 * (std::clamp)(playerHp, 0.0f, 1.0f), 6});
    world_.run.tankExpeditionHpFill_->Update();
    const float xp =
        static_cast<float>(world_.resources.player_->GetExp()) / (std::max)(1, world_.resources.player_->GetNextLevelExpValue());
    world_.run.tankExpeditionExpFill_->SetSize({1232 * (std::clamp)(xp, 0.0f, 1.0f), 6});
    world_.run.tankExpeditionExpFill_->Update();
    world_.run.tankExpeditionExpText_->SetText(world_.run.expeditionMapEnabled_
                                                   ? " "
                                                   : "EXP " + std::to_string(world_.resources.player_->GetExp()) + " / " +
                                                         std::to_string(world_.resources.player_->GetNextLevelExpValue()));
    int threats = LivingThreats(world_.resources.enemyManager_.get());
    if (world_.run.expeditionMapEnabled_) {
        threats = 0;
        for (auto* actor : world_.resources.enemyManager_->GetEnemyPtrs())
            if (actor && !actor->IsDead() && !actor->IsRunResource())
                ++threats;
    }
    const std::string objective =
        room == Room::Resource
            ? (world_.run.tankExpeditionResourceReleased_ ? "金色の通貨ボックスを壊そう"
                                                          : "敵を倒して宝物を手に入れよう  あと " + std::to_string(threats))
        : room == Room::Guard
            ? "通貨ボックス " + std::to_string(world_.run.tankExpeditionNodes_) + " / 3   敵 あと " + std::to_string(threats)
        : room == Room::Boss
            ? "ボスを倒そう"
            : (world_.run.expeditionMapEnabled_ ? "敵とブロックを倒そう  あと " : "敵を全滅させろ  残り ") + std::to_string(threats);
    world_.run.tankRunObjectiveText_->SetAlpha(0.90f + 0.10f * std::sin(world_.run.expeditionPresentationClock_ * 2.0f));
    world_.run.tankRunObjectiveText_->SetAnchorPoint({0.5f, 0});
    world_.run.tankRunObjectiveText_->SetPosition({640, 12});
    const auto* mapNode = world_.run.expeditionMapEnabled_ ? world_.run.expeditionMapRun_.GetActiveNode() : nullptr;
    world_.run.tankRunObjectiveText_->SetText(mapNode ? (inCombat ? objective : std::string(" "))
                                                      : "ROOM " + std::to_string(world_.run.tankExpedition_.GetRoomIndex() + 1) + " / 5\n" +
                                                            (inCombat ? objective : std::string(RoomName(room))));
    if (world_.run.expeditionMapEnabled_ && !mapNode)
        world_.run.tankRunObjectiveText_->SetText("作戦マップ");
    world_.run.tankRunBossText_->SetAnchorPoint({0.5f, 0});
    world_.run.tankRunBossText_->SetPosition({640, 80});
    world_.run.tankRunBossText_->SetText("最深部のボス");
    if (world_.resources.enemy_->IsNeonDepthEncounterEnabled()) {
        const auto& status = world_.resources.enemy_->GetNeonDepthSnapshot();
        const char* attack = status.plan.attack == neondepth::Attack::Volley ? "奥行き弾幕"
                             : status.plan.attack == neondepth::Attack::Dive ? "降下突撃"
                                                                             : "床接続ビーム";
        const char* action = status.phase == neondepth::Phase::Intro       ? "投影体が形成中 / Enterでスキップ"
                             : status.phase == neondepth::Phase::Telegraph ? "予告線を見て離れよう"
                             : status.phase == neondepth::Phase::Locked    ? "照準固定"
                             : status.phase == neondepth::Phase::Airborne  ? "発動まであと少し"
                             : status.phase == neondepth::Phase::Active    ? "危険エリアの外へ"
                             : status.phase == neondepth::Phase::Recovery  ? "反撃のチャンス / 床コアを攻撃"
                                                                           : "床コアを攻撃";
        world_.run.tankRunBossText_->SetText(std::string(status.plan.phaseTwo ? "ボス・第2段階 / " : "ボス / ") + attack + "  " + action);
    } else if (world_.resources.enemy_->IsExpeditionRivalEnabled()) {
        const auto status = world_.resources.enemy_->GetRivalCombatStatus();
        using P = RivalBossCombat::Phase;
        const char* action = status.phase == P::Reload        ? "装填中 / 反撃のチャンス"
                             : status.phase == P::DashWarning ? "突進予告"
                             : status.phase == P::Dash        ? "突進中"
                             : status.phase == P::Locked      ? "照準固定 / 射線から離れよう"
                             : status.phase == P::Volley      ? "連続射撃"
                                                              : "移動中";
        world_.run.tankRunBossText_->SetText(std::string(status.phase2 ? "ボス・第2段階 / " : "ボス / ") + action + "  残弾 " +
                                             std::to_string(status.ammo) + " / " + std::to_string(status.capacity));
    }
    world_.run.tankRunBossTrack_->SetPosition({460, 111});
    world_.run.tankRunBossTrack_->SetSize({360, 5});
    world_.run.tankRunBossTrack_->Update();
    const float hp = static_cast<float>(world_.resources.enemy_->GetHp()) / (std::max)(1, world_.resources.enemy_->GetMaxHp());
    world_.run.tankRunBossFill_->SetPosition({460, 111});
    world_.run.tankRunBossFill_->SetSize({360 * (std::clamp)(hp, 0.0f, 1.0f), 5});
    world_.run.tankRunBossFill_->Update();
    const char* features[] = {"跳弾ビルド", "突撃ビルド", "群体ビルド", "標準戦車"};
    const auto core = (std::clamp)(static_cast<int>(world_.run.tankRun_.GetCore()), 0, 3);
    std::string build = std::string("BUILD / ") + (world_.run.tankRun_.GetDraftCount() < 2 ? "基本射撃" : features[core]);
    const std::string combatStyle = world_.resources.player_->IsMeleeBuild()   ? "近接ブレード"
                                    : world_.resources.player_->IsDroneBuild() ? "ドローン編隊"
                                                                               : "シューター";
    if (world_.run.expeditionMapEnabled_)
        build = combatStyle;
    const auto& counts = world_.run.tankRun_.GetCardCounts();
    const auto visible = tankexp::CompactBuildCards(counts);
    int shown = 0;
    std::ostringstream details;
    details << (world_.run.expeditionMapEnabled_ ? "取得した強化   [Tab / Escで閉じる]\n\n" : "BUILD / 改造一覧   [TAB / ESC で閉じる]\n\n")
            << "機体: " << world_.resources.player_->GetCurrentClassName() << "  /  "
            << (world_.run.expeditionMapEnabled_ ? combatStyle : features[core]) << "\n";
    if (world_.run.tankRun_.GetDraftCount() < 2 && !world_.run.expeditionMapEnabled_)
        details << "主軸コアは改造を2つ取得すると起動\n";
    for (size_t i = 0; i < tankrun::CardCount; ++i)
        if (counts[i] > 0) {
            const std::string name = std::string(ExpeditionCardCopy(static_cast<int>(i)).title) +
                                     (world_.run.expeditionMapEnabled_ ? "" : " Lv." + std::to_string(counts[i]));
            details << "\n" << name;
            if (visible[i]) {
                build += "\n" + name;
                ++shown;
            }
        }
    if (!world_.run.tankRun_.GetDraftCount()) {
        build += world_.resources.player_->IsMeleeBuild()   ? "\n基本装備 / 3段斬り"
                 : world_.resources.player_->IsDroneBuild() ? "\n基本装備 / 左クリックで指揮"
                                                            : "\n基本装備 / 単発射撃";
        details << (world_.run.expeditionMapEnabled_ ? "\nまだ強化を取得していません" : "\nまだ改造を取得していません");
    }
    if (!world_.run.expeditionMapEnabled_)
        build += "\nTAB 詳細";
    auto buildStyle = world_.run.tankRunBuildText_->GetStyle();
    buildStyle.fontSize = 13;
    world_.run.tankRunBuildText_->SetStyle(buildStyle);
    world_.run.tankRunBuildText_->SetPosition({996, 14});
    world_.run.tankRunBuildText_->SetText(build);
    world_.run.tankExpeditionBuildPanel_->SetSize({280, static_cast<float>(shown + 3) * 21});
    world_.run.tankExpeditionBuildPanel_->Update();
    if (!world_.run.expeditionMapEnabled_)
        details << "\n\n整備: 機動 " << world_.resources.player_->GetRunMaintenanceRank(0) << " / 装填 "
                << world_.resources.player_->GetRunMaintenanceRank(1) << " / 装甲 " << world_.resources.player_->GetRunMaintenanceRank(2)
                << "   残り " << world_.resources.player_->GetRunMaintenancePoints() << " pt";
    if (world_.run.expeditionMapEnabled_)
        details << "\n\n通貨: " << world_.run.expeditionMapRun_.GetCurrency() << "\n工房の強化・修理に使います";
    world_.run.tankExpeditionDetailsText_->SetText(details.str());
    std::string route = "外周 → ";
    route += world_.run.tankExpedition_.GetRouteChoice(0) < 0    ? "[資源 / 精鋭]"
             : world_.run.tankExpedition_.GetRouteChoice(0) == 0 ? "資源"
                                                                 : "精鋭";
    route += " → イベント・進化 → ";
    route += world_.run.tankExpedition_.GetRouteChoice(1) < 0    ? "[反射 / ドローン]"
             : world_.run.tankExpedition_.GetRouteChoice(1) == 0 ? "反射"
                                                                 : "ドローン";
    route += " → 制圧 → ボス";
    world_.run.tankExpeditionMapText_->SetText(world_.run.expeditionMapEnabled_ ? "" : route);
    world_.run.tankExpeditionMapText_->SetPosition({24, inCombat ? 625.0f : 670.0f});
    auto card = [this](int index, const std::string& title, const std::string& body) {
        world_.run.tankRunCardTitles_[index]->SetText(std::to_string(index + 1) + "  " + title);
        world_.run.tankRunCardBodies_[index]->SetText(body);
    };
    if (terminal) {
        world_.run.tankRunHeading_->SetText(phase == EPhase::Clear ? "最深部突破 / 遠征成功" : "戦車が大破 / 遠征終了");
        world_.run.tankRunDescription_->SetText(
            "到達区画 " + std::to_string(world_.run.tankExpedition_.GetRoomIndex() + 1) + " / 5    撃破 " +
            std::to_string(world_.combat.defeatedEnemies_) + "    最長 " + std::to_string(world_.run.tankRunBestCombo_) + " CHAIN\n改造 " +
            std::to_string(world_.run.tankRun_.GetDraftCount()) + "個 / 次は違う機体・主軸・進路も試してみよう。");
        if (world_.run.expeditionMapEnabled_)
            world_.run.tankRunDescription_->SetText(
                "クリアした地点 " + std::to_string(world_.run.expeditionMapRun_.GetVisitedNodeIds().size()) + "  /  強化 " +
                std::to_string(world_.run.tankRun_.GetDraftCount()) + "個  /  残り " +
                std::to_string(world_.run.expeditionMapRun_.GetCurrency()) + "\n次は別の戦闘スタイルや強化を試してみよう。");
        card(0, "もう一度挑戦",
             world_.run.expeditionMapEnabled_ ? "新しい遠征をはじめます。" : "作戦マップから再スタート。\n\n新しい主軸や改造を試す。");
        card(1, "タイトルへ",
             world_.run.expeditionMapEnabled_ ? "今回の遠征を終えます。"
                                              : "今回の遠征を終える。\n\nタイトルのF10で\nコア争奪アリーナも遊べます。");
    } else if (world_.run.tankRunPaused_) {
        world_.run.tankRunHeading_->SetText("一時停止");
        world_.run.tankRunDescription_->SetText(
            "敵・弾・戦闘時間は停止中です。\nM: BGM " + std::string(world_.run.tankExpeditionMusicEnabled_ ? "ON" : "OFF") + " / N: SE " +
            std::string(world_.run.tankExpeditionEffectsEnabled_ ? "ON" : "OFF") + "（押すと切り替え）");
        card(0, "続ける", "現在の区画に戻ります。");
        card(1, "タイトルへ",
             world_.run.expeditionMapEnabled_ ? "今回の強化と進行状況は\nリセットされます。"
                                              : "今回の改造と進行状況は\nリセットされます。");
    } else if (phase == EPhase::Reward && world_.run.tankExpeditionMaintenanceOpen_) {
        world_.run.tankRunHeading_->SetText("機体整備 / 弱点を補う");
        world_.run.tankRunDescription_->SetText(
            "残り " + std::to_string(world_.resources.player_->GetRunMaintenancePoints()) +
            " ポイント / 区画突破ごとに +1、各項目は3段階まで。\n報酬画面で振り直し可能。装甲は被ダメージを軽減し、HPは回復しません。");
        const auto choices = world_.resources.player_->GetRunMaintenanceChoices();
        for (int i = 0; i < 3; ++i)
            card(i, choices[i].name + "  " + std::to_string(choices[i].rank) + " / 3",
                 choices[i].description + (choices[i].rank >= 3  ? "\n\n強化上限"
                                           : choices[i].canSpend ? "\n\n選択で1ポイント使用"
                                                                 : "\n\nポイントを持ち越して整備可能"));
    } else if (phase == EPhase::Reward) {
        world_.run.tankRunHeading_->SetText("区画報酬 / 改造を1つ選ぶ");
        if (world_.run.tankExpedition_.GetEventChoice() == 1 && world_.run.tankExpedition_.GetRoomIndex() == 1)
            world_.run.tankRunHeading_->SetText("禁制の改造 / レア候補を獲得");
        world_.run.tankRunDescription_->SetText(
            "機体を進化させても主軸・改造・整備は引き継ぎます。\nE または下の整備ボタンで能力を調整。改造を選ぶと先へ進みます。");
        if (world_.run.tankExpeditionRewardOpen_)
            for (size_t i = 0; i < world_.run.tankRun_.GetOfferCount(); ++i) {
                const auto id = world_.run.tankRun_.GetOffers()[i];
                const auto copy = ExpeditionCardCopy(static_cast<int>(id));
                const std::string title = copy.title;
                card(static_cast<int>(i), tankrun::IsRare(id) ? "レア\n" + title : title, copy.body);
            }
        else
            card(0, "次へ", "改造の取得上限に達しました。\n現在の構成で進みます。");
    } else if (phase == EPhase::Route) {
        world_.run.tankRunHeading_->SetText("次の区画を選ぶ");
        world_.run.tankRunDescription_->SetText(
            "敵の配置・地形と、欲しい報酬から進路を選ぼう。\nHPと改造は持ち越し。移動時にスタミナが全回復します。");
        if (world_.run.tankExpedition_.GetRouteRound() == 0) {
            card(0, "宝物庫",
                 "護衛を倒すとコアが出現。\nライバルより先に確保しよう。\n\n報酬: 改造1つ・整備 +1\n確保なら HP +8・レア候補。");
            card(1, "精鋭部隊",
                 "突進兵と狙撃兵の混成部隊。\n危険な壁に触れず突破しよう。\n\n報酬: 改造1つ・整備 +1\nレア候補を1つ以上保証。");
        } else {
            card(0, "反射実験区画",
                 "狙撃兵が多い遮蔽物の区画。\n射線を切り、壁反射を活かす。\n\n報酬: 改造1つ・整備 +1\n未所持なら反射改造が候補に。");
            card(1, "ドローン格納庫",
                 "突進兵が通路から迫る区画。\n本体と群れの位置取りを試す。\n\n報酬: 改造1つ・整備 +1\n未所持なら援護ドローン候補。");
        }
    } else if (phase == EPhase::Event) {
        world_.run.tankRunHeading_->SetText("中継地点 / 放棄された整備庫");
        world_.run.tankRunDescription_->SetText(
            "まだ使える修理装置と、危険な試作品を発見した。\nどちらかを利用したら、次は機体の進化を選びます。");
        card(0, "機体を修理する", "HPを40回復する。\n最大HPを超えて回復しない。\n\n残り3区画に備える。");
        card(1, world_.resources.player_->GetHp() > 20 ? "禁制の改造を取り出す" : "禁制の改造 / HP不足",
             "HPを20支払う。\nレアを含む3候補から1つ獲得。\n\nHPが21以上必要。\n組み合わせを増やす選択。");
    } else if (phase == EPhase::Evolution) {
        world_.run.tankRunHeading_->SetText("機体進化 / 次の戦い方へ");
        world_.run.tankRunDescription_->SetText(
            "現在: " + std::string(world_.resources.player_->GetCurrentClassName()) +
            "  / 同じ機体から異なる2つの方向へ。\n主軸・改造・整備・現在HPを保ち、残り3区画の戦い方を選びます。");
        for (size_t i = 0; i < world_.run.tankExpeditionEvolutions_.size(); ++i)
            card(static_cast<int>(i), world_.run.tankExpeditionEvolutions_[i].name, world_.run.tankExpeditionEvolutions_[i].description);
        if (world_.run.tankExpeditionEvolutions_.size() < 3)
            card(static_cast<int>(world_.run.tankExpeditionEvolutions_.size()), "現在の機体で進む",
                 "今回は進化せず、\n今の射撃感を維持する。\n\n主軸・改造・整備で\n現在の機体を伸ばす。");
    }
    world_.run.tankRunFooter_->SetText(
        terminal
            ? "数字キー / クリック: 決定    ← → + Enter: 選択\nもう一度挑戦するか、タイトルへ戻ります。"
            : "数字キー / クリック: 決定    ← → + Enter: 選択    Esc: 一時停止\n選択後は次の区画へ。手に入れた改造を試しながら最深部を目指そう。");
    if (world_.run.tankRunPaused_ && world_.run.expeditionMapEnabled_)
        world_.run.tankRunFooter_->SetText("クリックで決定    Esc: ゲームに戻る\n続けるか、タイトルへ戻るかを選べます。");
    if (phase == EPhase::Reward && !world_.run.tankRunPaused_) {
        world_.run.tankExpeditionMaintenanceText_->SetText(world_.run.tankExpeditionMaintenanceOpen_
                                                               ? "E / クリック: 改造の選択へ戻る"
                                                               : "E / クリック: 機体整備   残り " +
                                                                     std::to_string(world_.resources.player_->GetRunMaintenancePoints()) +
                                                                     " ポイント（未使用分は持ち越し）");
        if (world_.run.tankExpeditionMaintenanceOpen_)
            world_.run.tankRunFooter_->SetText(
                "数字キー / 左クリック: 強化    Shift + 数字 / 右クリック: 1段階戻す\n整備が済んだら E で改造の選択へ戻ります。選択中は戦闘が停止します。");
    }
    RefreshTankExpeditionTutorialUi();
    if (world_.run.expeditionMapEnabled_)
        world_.expeditionMapController->RefreshExpeditionMapUi();
}

void ExpeditionController::DrawTankExpeditionUi()
{
    if (world_.run.expeditionMapEnabled_ &&
        (world_.run.expeditionMapPreview_ || (world_.run.tankExpedition_.GetPhase() == EPhase::Map && !world_.run.tankRunPaused_))) {
        world_.expeditionMapController->DrawExpeditionMapUi();
        return;
    }
    if (world_.resources.player_->IsChangeMode())
        return;
    cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
    const auto phase = world_.run.tankExpedition_.GetPhase();
    const bool result = world_.combat.combatFlow_.GetState() == GameFlowState::StageClear ||
                        (world_.combat.combatFlow_.GetState() == GameFlowState::GameOver && world_.combat.combatFlow_.GetTimer() <= 0);
    const bool decision = world_.run.tankRunPaused_ || phase == EPhase::Reward || phase == EPhase::Route || phase == EPhase::Event ||
                          phase == EPhase::Evolution;
    if (decision || result || world_.run.tankExpeditionDetailsOpen_)
        world_.run.tankRunDimmer_->Draw();
    world_.run.tankRunHudPanel_->Draw();
    world_.run.tankRunHud_->Draw();
    world_.run.tankExpeditionHpTrack_->Draw();
    world_.run.tankExpeditionHpFill_->Draw();
    if (world_.run.expeditionMapEnabled_)
        world_.expeditionExperience->DrawExpeditionVitals();
    world_.run.tankRunObjectiveText_->Draw();
    if (world_.run.tankExpeditionDetailsOpen_) {
        world_.run.tankExpeditionDetailsText_->Draw();
        world_.run.tankExpeditionMapText_->Draw();
        return;
    }
    if (world_.run.tankExpedition_.GetRoomKind() == Room::Boss && world_.run.tankExpeditionRivalActive_ && !decision && !result) {
        world_.run.tankRunBossText_->Draw();
        world_.run.tankRunBossTrack_->Draw();
        world_.run.tankRunBossFill_->Draw();
    }
    if (!decision && !result) {
        world_.run.tankExpeditionBuildPanel_->Draw();
        world_.run.tankRunBuildText_->Draw();
        world_.run.tankExpeditionExpText_->Draw();
        if (!world_.run.expeditionMapEnabled_) {
            world_.run.tankExpeditionExpTrack_->Draw();
            world_.run.tankExpeditionExpFill_->Draw();
        }
        DrawTankExpeditionTutorial();
        return;
    }
    world_.run.tankRunHeading_->Draw();
    world_.run.tankRunDescription_->Draw();
    const int count = result ? 2 : GetTankExpeditionOptionCount();
    for (int i = 0; i < count; ++i) {
        const bool selected = i == (result ? world_.combat.resultSelection_ : world_.run.tankRunSelection_);
        const bool rare = phase == EPhase::Reward && !world_.run.tankRunPaused_ && !world_.run.tankExpeditionMaintenanceOpen_ &&
                          world_.run.tankExpeditionRewardOpen_ && tankrun::IsRare(world_.run.tankRun_.GetOffers()[i]);
        world_.run.tankRunCards_[i]->SetColor(selected
                                                  ? (rare ? cg2::Vector4{0.21f, 0.12f, 0.045f, 1} : cg2::Vector4{0.045f, 0.16f, 0.19f, 1})
                                                  : cg2::Vector4{0.028f, 0.045f, 0.075f, 1});
        world_.run.tankRunCards_[i]->Update();
        world_.run.tankRunCards_[i]->Draw();
        world_.run.tankRunCardTitles_[i]->Draw();
        world_.run.tankRunCardBodies_[i]->Draw();
    }
    if (phase == EPhase::Reward && !world_.run.tankRunPaused_) {
        world_.run.tankExpeditionMaintenanceButton_->Draw();
        world_.run.tankExpeditionMaintenanceText_->Draw();
    }
    if (!world_.run.tankExpeditionTutorial_.IsVisible() || world_.run.tankRunPaused_ || result)
        world_.run.tankRunFooter_->Draw();
    if (!world_.run.tankRunPaused_ && !result)
        DrawTankExpeditionTutorial();
}

void ExpeditionController::UpdateTankExpeditionTutorial(float dt)
{
    if (world_.demo.titleDemo_ || world_.run.tankExpedition_.GetPhase() == EPhase::Dormant ||
        !world_.run.tankExpeditionTutorial_.IsVisible())
        return;
    const auto position = world_.resources.player_->GetWorldPosition();
    if (world_.run.tankExpedition_.IsCombat()) {
        if (world_.resources.player_->HasMovementInput() && !world_.resources.player_->IsDashing())
            world_.run.tankExpeditionTutorial_.AddMovement(cg2::Length(position - world_.run.tankExpeditionTutorialPrevious_));
        if (world_.combat.defeatedEnemies_ > world_.run.tankExpeditionTutorialKills_)
            world_.run.tankExpeditionTutorial_.RecordKill();
        // Observe the actual dash state without consuming the effect/audio event.
        if (world_.resources.player_->IsDashing())
            world_.run.tankExpeditionTutorial_.RecordDash();
    }
    world_.run.tankExpeditionTutorialPrevious_ = position;
    world_.run.tankExpeditionTutorialKills_ = world_.combat.defeatedEnemies_;
    if (cg2::kDeveloperTools && world_.resources.input_->IsKeyTriggered(DIK_F3))
        world_.run.tankExpeditionTutorial_.Skip();
    world_.run.tankExpeditionTutorial_.Update(dt);
    if (world_.run.tankExpeditionTutorial_.IsComplete())
        SaveExpeditionTutorialCompletion();
    RefreshTankExpeditionTutorialUi();
}

void ExpeditionController::SaveExpeditionTutorialCompletion()
{
    const bool isolatedSubmission = tanksubmission::Enabled();
    const bool validation = world_.run.tankRunAutoTest_ || world_.run.expeditionMapAutoTest_ || world_.run.combatValidationEnabled_ ||
                            world_.validation.experienceValidationVariant_ != 0 || world_.run.specialValidationEnabled_ ||
                            world_.run.tankExpeditionTutorialValidation_.enabled;
    if (world_.run.tankExpeditionTutorialSaved_ ||
        !tankexp::ShouldSaveTutorialCompletion(world_.demo.titleDemo_, validation, isolatedSubmission))
        return;
    // Both tutorial flows share the existing settings file. Preserve unknown
    // settings and publish the complete document with one atomic replacement.
    try {
        nlohmann::json settings = nlohmann::json::object();
        {
            std::ifstream in(TutorialSettingsPath());
            if (in) {
                settings = nlohmann::json::parse(in, nullptr, false);
                if (!settings.is_object())
                    return; // Preserve a damaged file for recovery.
            }
        }
        settings["tutorialCompleted"] = true;
        const std::string temporary = std::string(TutorialSettingsPath()) + ".tmp";
        std::ofstream out(temporary);
        out << std::setw(2) << settings << '\n';
        out.close();
        if (out)
            world_.run.tankExpeditionTutorialSaved_ =
                MoveFileExA(temporary.c_str(), TutorialSettingsPath(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
        if (world_.run.tankExpeditionTutorialSaved_)
            world_.run.expeditionTutorialPreviouslyCompleted_ = true;
    }
    catch (...) {
        OutputDebugStringA("[TankExpedition] Tutorial settings could not be saved\n");
    }
}

void ExpeditionController::UpdateTankExpeditionTutorialValidation(float dt)
{
    auto& validation = world_.run.tankExpeditionTutorialValidation_;
    validation.elapsed += dt;
    const int step = static_cast<int>(world_.run.tankExpeditionTutorial_.GetStep());
    if (step != validation.lastStep) {
        validation.lastStep = step;
        validation.stepAge = 0;
    }
    validation.stepAge += dt;
    const bool started = world_.run.tankExpedition_.GetPhase() != EPhase::Dormant;
    const unsigned bit = 1u << step;
    if (started && validation.stepAge > 0.15f && !(validation.observedSteps & bit) && world_.run.tankRunCapturePath_.empty()) {
        world_.run.tankRunCapturePath_ = "generated/tank_expedition/tutorial_validation/step_" + std::to_string(step + 1) + ".png";
        validation.observedSteps |= bit;
    }
    if (validation.elapsed > 120 ||
        (step == static_cast<int>(tankexp::TutorialStep::Hidden) && validation.stepAge > 0.8f && world_.run.tankRunCapturePath_.empty())) {
        const bool done = world_.run.tankExpeditionTutorial_.IsComplete() && validation.observedSteps == 255u &&
                          world_.combat.defeatedEnemies_ > 0 && world_.run.tankExpedition_.GetRouteChoice(0) >= 0 &&
                          world_.run.tankRun_.GetDraftCount() > 0;
        nlohmann::json result = {{"completed", done},
                                 {"testMode", true},
                                 {"realProjectileKills", world_.combat.defeatedEnemies_},
                                 {"movementDistance", world_.run.tankExpeditionTutorial_.GetMoveDistance()},
                                 {"observedStepMask", validation.observedSteps},
                                 {"route", world_.run.tankExpedition_.GetRouteChoice(0)},
                                 {"cards", world_.run.tankRun_.GetDraftCount()},
                                 {"tutorialVisible", world_.run.tankExpeditionTutorial_.IsVisible()},
                                 {"elapsed", validation.elapsed},
                                 {"forcedDamage", false},
                                 {"invulnerable", true}};
        std::ofstream("generated/tank_expedition/tutorial_validation/validation.json") << std::setw(2) << result << '\n';
        PostQuitMessage(done ? 0 : 3);
        return;
    }
    if (!started) {
        world_.resources.player_->SetDemoInput(true, {0, 0}, world_.resources.player_->GetWorldPosition() + cg2::Vector3{1, 0, 0}, false,
                                               false);
        if (world_.run.tankRunMenuAge_ > 0.85f)
            world_.arenaRunController->SelectTankRunOption(0);
        return;
    }
    world_.resources.player_->SetDemoInput(true, {0, 0}, world_.resources.player_->GetWorldPosition() + cg2::Vector3{1, 0, 0}, false,
                                           false);
    if (validation.stepAge < 0.6f || world_.run.tankExpeditionTutorial_.IsSuccess())
        return;
    using Step = tankexp::TutorialStep;
    const auto lesson = world_.run.tankExpeditionTutorial_.GetStep();
    const auto position = world_.resources.player_->GetWorldPosition();
    cg2::Vector3 aim = position + cg2::Vector3{1, 0, 0};
    float distance = 10000;
    for (auto* actor : world_.resources.enemyManager_->GetEnemyPtrs())
        if (actor && actor->IsCombatThreat()) {
            const float candidate = cg2::Length(actor->GetWorldPosition() - position);
            if (candidate < distance) {
                distance = candidate;
                aim = actor->GetWorldPosition();
            }
        }
    cg2::Vector2 move{};
    if (lesson == Step::Move)
        move = {1, 0};
    const bool shoot = lesson == Step::Shoot || lesson == Step::ClearRoom;
    if (shoot && distance > 13 && distance < 1000) {
        const auto direction = cg2::Normalize(aim - position);
        move = {direction.x, direction.y};
    }
    if (lesson == Step::Dash)
        move = {0, 1};
    world_.resources.player_->SetDemoInput(true, move, aim, shoot, lesson == Step::Dash);
    if (lesson == Step::Route && world_.run.tankExpedition_.GetPhase() == EPhase::Route && world_.run.tankRunMenuAge_ > 1.0f)
        SelectTankExpeditionOption(1);
    if (lesson == Step::Upgrade && world_.run.tankExpedition_.GetPhase() == EPhase::Reward && world_.run.tankRunMenuAge_ > 1.0f)
        SelectTankExpeditionOption(0);
}

void ExpeditionController::RefreshTankExpeditionTutorialUi()
{
    if (world_.demo.titleDemo_ || !world_.run.tankExpeditionTutorial_.IsVisible())
        return;
    using Step = tankexp::TutorialStep;
    const auto step = world_.run.tankExpeditionTutorial_.GetStep();
    const char* actions[] = {
        "WASDで移動しよう", "左クリックで敵を1体倒そう", "右クリックでダッシュしよう", "敵を全滅させて区画を突破しよう",
        "次の区画を選ぼう", "改造カードを1枚選ぼう",     "準備完了 / 最深部を目指せ！"};
    const char* success[] = {"✓ 移動", "✓ 敵を撃破", "✓ ダッシュ", "✓ 区画突破", "✓ 進路を選択", "✓ 改造を取得"};
    const int index = static_cast<int>(step);
    world_.combat.tutorialTitleText_->SetText(step == Step::Complete ? "操作練習完了" : "操作練習  " + std::to_string(index + 1) + " / 6");
    world_.combat.tutorialInputText_->SetText(world_.run.tankExpeditionTutorial_.IsSuccess()              ? success[index]
                                              : world_.run.expeditionMapEnabled_ && step == Step::Route   ? "マップの光る地点を選ぼう"
                                              : world_.run.expeditionMapEnabled_ && step == Step::Upgrade ? "回収資材で改造を1つ購入しよう"
                                                                                                          : actions[index]);
    world_.combat.tutorialDescriptionText_->SetText(world_.run.tankExpeditionTutorial_.CanSkip()   ? "F3 / チュートリアルをスキップ"
                                                    : step == Step::Move                           ? "移動した距離で達成"
                                                    : step == Step::Route || step == Step::Upgrade ? "数字キー / クリック / ← → + Enter"
                                                                                                   : " ");
    world_.combat.tutorialPanel_->SetColor(world_.run.tankExpeditionTutorial_.IsSuccess() ? cg2::Vector4{0.01f, 0.14f, 0.10f, 0.9f}
                                                                                          : cg2::Vector4{0.008f, 0.025f, 0.04f, 0.84f});
    const float y = world_.run.tankExpedition_.IsCombat() ? 590.0f : 606.0f;
    world_.combat.tutorialPanel_->SetPosition({410, y});
    world_.combat.tutorialPanel_->SetSize({460, 70});
    world_.combat.tutorialTitleText_->SetPosition({640, y + 4});
    world_.combat.tutorialInputText_->SetPosition({640, y + 21});
    world_.combat.tutorialDescriptionText_->SetPosition({640, y + 50});
    world_.combat.tutorialPanel_->Update();
    world_.combat.tutorialTitleText_->PrepareForDraw();
    world_.combat.tutorialInputText_->PrepareForDraw();
    world_.combat.tutorialDescriptionText_->PrepareForDraw();
}

void ExpeditionController::DrawTankExpeditionTutorial()
{
    if (world_.run.expeditionMapEnabled_)
        return;
    if (world_.demo.titleDemo_ || !world_.run.tankExpeditionTutorial_.IsVisible())
        return;
    world_.combat.tutorialPanel_->Draw();
    world_.combat.tutorialTitleText_->Draw();
    world_.combat.tutorialInputText_->Draw();
    world_.combat.tutorialDescriptionText_->Draw();
}

void ExpeditionController::UpdateTankExpeditionAudio(float dt)
{
    const bool combat = world_.run.tankExpedition_.IsCombat() && world_.combat.combatFlow_.GetState() == GameFlowState::Playing;
    world_.run.tankExpeditionAudio_.SetCombat(combat);
    world_.run.tankExpeditionAudio_.SetBoss(world_.run.tankExpedition_.GetRoomKind() == Room::Boss);
    world_.run.tankExpeditionAudio_.SetDucked(world_.arenaRunController->IsTankRunMenuOpen() || world_.combat.phase_ == Phase::kFadeOut);
    world_.run.tankExpeditionAudio_.Update(dt);
    if (!combat || world_.arenaRunController->IsTankRunMenuOpen())
        return;
    if (world_.resources.enemy_->IsExpeditionRivalEnabled() && world_.resources.enemy_->GetRivalCombatStatus().phase2 &&
        !world_.run.expeditionBossPhase2Seen_) {
        world_.run.expeditionBossPhase2Seen_ = true;
        world_.combat.screenEffectDirector_.TriggerBossPhaseChange();
        world_.combatFlow->SetEventCallout("ボス / 第二形態", 1.1f);
        world_.run.tankExpeditionAudio_.EnemyWarning();
    }
    std::unordered_map<const ExpEnemy*, int> hp;
    std::unordered_map<const ExpEnemy*, bool> warning;
    std::unordered_map<const ExpEnemy*, std::pair<uint64_t, uint64_t>> guardCounts;
    for (auto* actor : world_.resources.enemyManager_->GetEnemyPtrs())
        if (actor && !actor->IsDead()) {
            const auto counts = std::make_pair(actor->GetShieldBlockCount(), actor->GetBladeSwingCount());
            const auto oldGuard = world_.run.guardAudioCounts_.find(actor);
            const auto before = oldGuard == world_.run.guardAudioCounts_.end() ? std::pair<uint64_t, uint64_t>{} : oldGuard->second;
            if (counts.first > before.first)
                world_.run.tankExpeditionAudio_.ArmorBreak();
            if (counts.second > before.second)
                world_.run.tankExpeditionAudio_.Slash();
            guardCounts[actor] = counts;
            const auto previous = world_.run.tankExpeditionEnemyHp_.find(actor);
            if (previous != world_.run.tankExpeditionEnemyHp_.end() && actor->GetHp() < previous->second) {
                world_.run.tankExpeditionAudio_.Hit();
                if (world_.run.expeditionMapEnabled_ && world_.run.expeditionHitSparkCooldown_ <= 0 &&
                    world_.run.expeditionHitSparks_.size() < 12) {
                    auto direction = actor->GetWorldPosition() - world_.resources.player_->GetWorldPosition();
                    direction = cg2::Length(direction) > 0.01f ? cg2::Normalize(direction) : cg2::Vector3{0, 1, 0};
                    world_.run.expeditionHitSparks_.push_back({actor->GetWorldPosition(), direction, 0});
                    world_.run.expeditionHitSparkCooldown_ = 0.045f;
                }
            }
            hp[actor] = actor->GetHp();
            const bool locked = actor->IsExpeditionCombatRole() && actor->IsAttackAimLocked();
            const auto old = world_.run.tankExpeditionEnemyWarning_.find(actor);
            if (locked && (old == world_.run.tankExpeditionEnemyWarning_.end() || !old->second))
                world_.run.tankExpeditionAudio_.EnemyWarning();
            warning[actor] = locked;
        }
    world_.run.tankExpeditionEnemyHp_ = std::move(hp);
    world_.run.tankExpeditionEnemyWarning_ = std::move(warning);
    world_.run.guardAudioCounts_ = std::move(guardCounts);
}

} // namespace gameplay
