#include "game/weapon/CombatTypes.h"
#include "GameScene.h"
#include "StartupTrace.h"
#include "game/run/TankRunCopy.h"
#include "game/run/TankExpeditionEncounters.h"
#include "game/run/TankSubmissionValidation.h"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace {
using EPhase=tankexp::Phase;
using Room=tankexp::RoomKind;
using namespace tankrun::copy;
/// @brief 現在の部屋の表示名を返す。
const char* RoomName(Room room) {
    switch(room) {
    case Room::Skirmish:return "外周警備";
    case Room::Resource:return "宝物庫";
    case Room::Elite:return "精鋭部隊";
    case Room::Reflection:return "反射実験区画";
    case Room::Drone:return "ドローン格納庫";
    case Room::Guard:return "制御装置の制圧";
    default:return "最深部 / 最終決戦";
    }
}
/// @brief 生存している戦闘対象の数を返す。
int LivingThreats(EnemyManager* manager) {
    int count=0;
    for(auto* actor:manager->GetEnemyPtrs()) if(actor&&actor->IsCombatThreat()) ++count;
    return count;
}
/// @brief 遠征検証の出力先ディレクトリーを返す。
std::string ExpeditionDirectory(int variant) {return "generated/tank_expedition/variant_"+std::to_string(variant);}
/// @brief チュートリアル設定の保存先パスを返す。
const char* TutorialSettingsPath() { return "resources/configs/expedition_user.json"; }
}

void GameScene::InitializeTankExpedition() {
    cg2::StartupTrace::Scope scope("Expedition.Initialize");
    player_->SetRunCheckpointEvolution(true);
    enemy_->SetRunEncounterEnabled(false);
    enemyManager_->ClearRunActors();
    bulletManager_->ClearAll();
    stage_->LoadRunMap("resources/maps/expedition_crossfire.csv");
    stagePostCacheValid_=false;
    player_->ResetRunRoomState({26,28,0});
    camera->SetTranslate({26,28,camera->GetTranslate().z}); camera->Update();
    cg2::TextStyle style{};style.fontFamily="Meiryo";style.fontSize=14;
    style.color={0.68f,0.86f,0.95f,1};style.outlineThickness=0;style.padding=4;
    tankExpeditionMapText_=std::make_unique<cg2::TextLabel>();
    tankExpeditionMapText_->Initialize(cg2::SpriteCommon::GetInstance()," ",style);
    tankExpeditionMapText_->SetPosition({24,670});
    style.fontSize=17;style.color={0.78f,0.96f,1,1};
    tankExpeditionMaintenanceText_=std::make_unique<cg2::TextLabel>();
    tankExpeditionMaintenanceText_->Initialize(cg2::SpriteCommon::GetInstance()," ",style);
    tankExpeditionMaintenanceText_->SetPosition({76,570});
    tankExpeditionMaintenanceButton_=std::make_unique<cg2::Sprite>();
    tankExpeditionMaintenanceButton_->Initialize(cg2::SpriteCommon::GetInstance(),"resources/white512x512.png");
    tankExpeditionMaintenanceButton_->SetPosition({64,568});
    tankExpeditionMaintenanceButton_->SetSize({1144,32});
    tankExpeditionMaintenanceButton_->SetColor({0.035f,0.105f,0.14f,1});
    tankExpeditionMaintenanceButton_->Update();
    auto panel=[](cg2::Vector2 position,cg2::Vector2 size,const cg2::Vector4& color) {
        auto item=std::make_unique<cg2::Sprite>();item->Initialize(cg2::SpriteCommon::GetInstance(),"resources/white512x512.png");
        item->SetPosition(position);item->SetSize(size);item->SetColor(color);item->Update();return item;
    };
    auto label=[](float size,cg2::Vector2 position,const cg2::Vector4& color) {
        cg2::TextStyle s{};s.fontFamily="Meiryo";s.fontSize=size;s.color=color;s.outlineThickness=0;s.padding=4;
        auto item=std::make_unique<cg2::TextLabel>();item->Initialize(cg2::SpriteCommon::GetInstance()," ",s);item->SetPosition(position);return item;
    };
    tankExpeditionHpTrack_=panel({24,53},{230,6},{0.10f,0.16f,0.21f,0.9f});
    tankExpeditionHpFill_=panel({24,53},{230,6},{0.23f,1.0f,0.65f,1});
    tankExpeditionExpTrack_=panel({24,702},{1232,6},{0.10f,0.16f,0.21f,0.9f});
    tankExpeditionExpFill_=panel({24,702},{1,6},{0.24f,0.73f,1,1});
    tankExpeditionBuildPanel_=panel({988,10},{280,150},{0.009f,0.016f,0.03f,0.60f});
    tankExpeditionExpText_=label(12,{24,677},{0.68f,0.83f,0.93f,1});
    tankExpeditionDetailsText_=label(18,{280,140},{0.83f,0.94f,1,1});
    // Reuse the existing Tutorial UI primitives, but keep expedition progression
    // separate from the arena's combat-suppressing introduction.
    tutorialPanel_=panel({410,596},{460,70},{0.008f,0.025f,0.04f,0.84f});
    tutorialTitleText_=label(12,{640,601},{0.43f,0.81f,0.93f,1});
    tutorialInputText_=label(20,{640,620},{0.93f,1,1,1});
    tutorialDescriptionText_=label(12,{640,651},{0.68f,0.84f,0.91f,1});
    tutorialTitleText_->SetAnchorPoint({0.5f,0});tutorialInputText_->SetAnchorPoint({0.5f,0});
    tutorialDescriptionText_->SetAnchorPoint({0.5f,0});
    bool completed=false;
    try {std::ifstream f(TutorialSettingsPath());if(f) {nlohmann::json j;f>>j;completed=j.value("tutorialCompleted",false);}} catch(...) {}
    expeditionTutorialPreviouslyCompleted_=completed;
    wchar_t tutorialMode[8]{};
    const bool forceTutorial=GetEnvironmentVariableW(L"CG2_EXPEDITION_TUTORIAL",tutorialMode,8)>0&&tutorialMode[0]==L'1';
    wchar_t validateTutorial[8]{};
    tankExpeditionTutorialValidation_.enabled=!titleDemo_&&GetEnvironmentVariableW(L"CG2_TANK_TUTORIAL_AUTOTEST",validateTutorial,8)>0&&validateTutorial[0]==L'1';
    if(tankExpeditionTutorialValidation_.enabled) {
        tankRunAutoTest_=false;completed=false;debugPlayerNoDamage_=true;
        std::filesystem::create_directories("generated/tank_expedition/tutorial_validation");
        std::ofstream("generated/tank_expedition/tutorial_validation/validation.json")<<"{\"completed\":false}\n";
    }
    tankExpeditionTutorial_.Reset(completed,titleDemo_||(tankRunAutoTest_&&!forceTutorial));
    tankExpeditionTutorialPrevious_=player_->GetWorldPosition();
    tankExpeditionTutorialKills_=defeatedEnemies_;
    InitializeTankExpeditionBalance();
    if(!titleDemo_&&!tankRunAutoTest_&&!tankExpeditionTutorialValidation_.enabled) InitializeExpeditionMap();
    wchar_t variant[16]{};
    if(GetEnvironmentVariableW(L"CG2_TANK_EXPEDITION_VARIANT",variant,16)>0)
        tankExpeditionAutoVariant_=(std::clamp)(_wtoi(variant),0,5);
    if(tankRunAutoTest_) {
        std::filesystem::create_directories(ExpeditionDirectory(tankExpeditionAutoVariant_));
        std::ofstream(ExpeditionDirectory(tankExpeditionAutoVariant_)+"/validation.json")<<"{\"completed\":false,\"testMode\":true}\n";
    }
}

void GameScene::StartTankExpeditionRoom() {
    if(!tankExpedition_.IsCombat()) return;
    // 更新の冒頭か停止中のメニューから部屋を開始する。資源の命中コールバックは完了待ちなどを更新する。
    // ここで前の敵・弾・場の攻撃を消去し、衝突走査中に対象の実体を削除しない。
    enemy_->SetRunEncounterEnabled(false); tankExpeditionRivalActive_=false;
    enemyManager_->ClearRunActors(); bulletManager_->ClearAll();
    playerLaserBeams_.clear();playerMines_.clear();playerMineExplosions_.clear();
    playerMeleeSlashes_.clear();playerNeonAfterimages_.clear();neonTriangleParticles_.clear();
    if(playerMeleeTrailManager_) playerMeleeTrailManager_->ClearInstances();
    hpBarVisibility_.clear();tankRunBursts_.clear();
    for(auto& node:tankRunResources_) node={};
    tankExpeditionRoomPending_=false;tankExpeditionResourceWon_=false;
    tankExpeditionRewardOpen_=false;tankExpeditionNodes_=0;tankExpeditionSpawned_=0;
    tankExpeditionMaintenanceOpen_=false;tankExpeditionResourceReleased_=false;
    tankExpeditionEnemyHp_.clear();tankExpeditionEnemyWarning_.clear();
    tankExpeditionArrival_=0;tankRunComboTime_=0;
    const Room room=tankExpedition_.GetRoomKind();
    const char* map=room==Room::Resource||room==Room::Drone?"resource_fork":
        room==Room::Elite||room==Room::Reflection?"hazard_lane":room==Room::Boss?"final_duel":"crossfire";
    if(!stage_->LoadRunMap(std::string("resources/maps/expedition_")+map+".csv"))
        OutputDebugStringA("[TankExpedition] map could not be loaded\n");
    ApplyTankExpeditionRoomGeometry();
    stagePostCacheValid_=false;
    // この部屋のダメージ地形へ渡す値を12に設定する。衝突時の位置補正とダメージ適用は別の処理。
    stage_->SetDamageBlockDamage(12);
    // 生存中で遠征成長が有効な自機の部屋状態と位置をリセットする。機体/成長・現在HP・カード等は保持する。
    player_->ResetRunRoomState({26,28,0});
    tankExpeditionTutorialPrevious_=player_->GetWorldPosition();
    tankExpeditionDetailsOpen_=false;
    camera->SetTranslate({26,28,camera->GetTranslate().z});camera->Update();
    auto freePosition=[this](const cg2::Vector3& desired) {
        if(!stage_->IsCollisionWithAnyBlock(desired,1.3f)) return desired;
        for(int r=2;r<=10;r+=2) for(int y=-r;y<=r;y+=2) for(int x=-r;x<=r;x+=2) {
            cg2::Vector3 p=desired+cg2::Vector3{static_cast<float>(x),static_cast<float>(y),0};
            if(p.x>23&&p.x<65&&p.y>17&&p.y<41&&!stage_->IsCollisionWithAnyBlock(p,1.3f)) return p;
        }
        return cg2::Vector3{44,30,0};
    };
    const std::array<cg2::Vector3,6> shapes={cg2::Vector3{34,22,0},cg2::Vector3{44,22,0},cg2::Vector3{54,22,0},cg2::Vector3{34,36,0},cg2::Vector3{44,36,0},cg2::Vector3{54,36,0}};
    const auto encounter=tankexp::GetEncounter(room,tankExpedition_.GetRoomIndex());
    for(int i=0;i<encounter.count;++i) {
        const auto& unit=encounter.units[i];
        if(enemyManager_->SpawnLevelEnemy(freePosition({unit.x,unit.y,0}),unit.prefab,unit.hp)) ++tankExpeditionSpawned_;
    }
    const int shapeCount=room==Room::Boss?2:4;
    for(int i=0;i<shapeCount;++i)
        enemyManager_->SpawnLevelEnemy(freePosition(shapes[i]),i%2?"Triangle":"Square",8);
    if(room==Room::Resource) tankRunResources_[0].position=freePosition({44,30,0});
    if(room==Room::Guard) {
        const int count=3;
        const std::array<cg2::Vector3,3> centers={cg2::Vector3{44,30,0},cg2::Vector3{36,38,0},cg2::Vector3{54,20,0}};
        for(int i=0;i<count;++i) {
            auto& node=tankRunResources_[i];node.position=freePosition(centers[i]);
            node.active=enemyManager_->SpawnRunResource(node.position,60,
                [this,i](bool owned){OnTankRunResourceClaim(i,owned);});
        }
    }
    if(room==Room::Resource||room==Room::Boss) {
        tankExpeditionRivalActive_=true;
        const int configuredBossHp=tankExpeditionBalance_.value("bossMaxHp",900);
        enemy_->ResetRunEncounter({62,30,0},room==Room::Boss?configuredBossHp:
            (std::max)(1,static_cast<int>(std::round(configuredBossHp*0.52f))),room==Room::Boss?1:0,true);
        auto progress=enemy_->GetEnemyProgressConfig();
        progress.levelingModeEnabled=room==Room::Resource;
        enemy_->SetEnemyProgressConfig(progress);
        if(room==Room::Boss) screenEffectDirector_.TriggerBossEntry();
    }
    ApplyTankExpeditionRoomBalance();
    ConfigureNeonDepthEncounter();
    previousPlayerHp_=player_->GetHp();previousBossHp_=enemy_->GetHp();
    bossDefeatHandled_=false;
    tankRunSelection_=0;tankRunMenuAge_=-0.15f;
    if(!enemy_->IsNeonDepthEncounterEnabled()) SetEventCallout(std::string("第")+std::to_string(tankExpedition_.GetRoomIndex()+1)+"区画 / "+RoomName(room),1.6f);
    RefreshTankExpeditionUi();
}

void GameScene::FinishTankExpeditionRoom() {
    // マップ式と従来の遠征で完了処理を分ける。死亡時・非戦闘時・ボス部屋はこの入口では完了しない。
    if(expeditionMapEnabled_) {CompleteExpeditionMapCombat();return;}
    if(player_->IsDead()||!tankExpedition_.IsCombat()||tankExpedition_.GetRoomKind()==Room::Boss) return;
    if(!tankExpedition_.CompleteRoom()) return;
    // 進行を報酬/進路選択へ切り替えるとIsTankRunMenuOpenが戦闘を止める。実体の消去は次の部屋開始時。
    tankExpeditionTutorial_.RecordRoomClear();
    player_->AwardRunMaintenancePoint(tankExpedition_.GetRoomIndex()+1);
    tankExpeditionMaintenanceOpen_=false;
    tankExpeditionRoomPending_=false;
    tankExpeditionRewardOpen_=tankExpedition_.GetPhase()==EPhase::Reward&&tankRun_.OpenExpeditionRewardDraft(tankExpedition_.GetRoomIndex(),tankExpedition_.GetRewardRare()||tankExpeditionResourceWon_,tankExpedition_.GetRewardAffinity());
    tankRunSelection_=0;tankRunMenuAge_=0;
    tankExpeditionAudio_.Upgrade();
    SetEventCallout(tankExpedition_.GetPhase()==EPhase::Route?"区画突破 / 次の進路を選ぼう":"区画突破 / 改造 ＋ 整備ポイント",1.2f);
    RefreshTankExpeditionUi();
}

int GameScene::GetTankExpeditionOptionCount() const {
    if(tankRunPaused_) return 2;
    switch(tankExpedition_.GetPhase()) {
    case EPhase::Dormant:return 3;
    case EPhase::Reward:return tankExpeditionMaintenanceOpen_?3:tankExpeditionRewardOpen_?static_cast<int>(tankRun_.GetOfferCount()):1;
    case EPhase::Evolution:return (std::min)(3,static_cast<int>(tankExpeditionEvolutions_.size())+1);
    case EPhase::Route:case EPhase::Event:case EPhase::Clear:case EPhase::Dead:return 2;
    default:return 0;
    }
}

void GameScene::SelectTankExpeditionOption(int index) {
    if(index<0||index>=GetTankExpeditionOptionCount()) return;
    const EPhase before=tankExpedition_.GetPhase();
    if(before==EPhase::Reward) {
        if(tankExpeditionMaintenanceOpen_) {
            if(player_->SpendRunMaintenancePoint(index)) tankExpeditionAudio_.Upgrade();
            else SetEventCallout("整備ポイント不足、または強化上限です",0.8f);
            RefreshTankExpeditionUi();return;
        }
        if(tankExpeditionRewardOpen_) {
            const auto card=tankRun_.GetOffers()[index];
            if(!tankRun_.ChooseCard(index)) return;
            tankExpeditionTutorial_.RecordUpgrade();
            ApplyTankRunCards();if(card==tankrun::CardId::Repair) player_->HealRunPlayer(30);
            screenEffectDirector_.TriggerUpgradeConfirmed(WorldToScreenUv(player_->GetWorldPosition()));
            tankExpeditionAudio_.Upgrade();
        }
        tankExpeditionRewardOpen_=false;tankExpedition_.ChooseRewardDone();
    } else if(before==EPhase::Route) {
        if(!tankExpedition_.ChooseRoute(index)) return;
        tankExpeditionTutorial_.RecordRoute();
        if(tankExpedition_.GetPhase()==EPhase::Reward)
            tankExpeditionRewardOpen_=tankRun_.OpenExpeditionRewardDraft(tankExpedition_.GetRoomIndex(),tankExpedition_.GetRewardRare(),tankExpedition_.GetRewardAffinity());
    }
    else if(before==EPhase::Event) {
        if(index==1&&!player_->SpendRunHealth(20)) {SetEventCallout("HPが21以上あると選べます",1.3f);return;}
        if(index==0) player_->HealRunPlayer(40);
        tankExpedition_.ChooseEvent(index);
        if(index==1) tankExpeditionRewardOpen_=tankRun_.OpenExpeditionRewardDraft(tankExpedition_.GetRoomIndex(),true);
        previousPlayerHp_=player_->GetHp();
    } else if(before==EPhase::Evolution) {
        const int hp=player_->GetHp();
        const std::array<int,3> ranks={player_->GetRunMaintenanceRank(0),player_->GetRunMaintenanceRank(1),player_->GetRunMaintenanceRank(2)};
        const auto cards=tankRun_.GetCardCounts();
        if(index<static_cast<int>(tankExpeditionEvolutions_.size()) && !player_->ChooseRunEvolution(tankExpeditionEvolutions_[index].id)) return;
        if(tankRunAutoTest_) {
            if(hp!=player_->GetHp()||cards!=tankRun_.GetCardCounts()) ++tankExpeditionValidationErrors_;
            for(int i=0;i<3;++i) if(ranks[i]!=player_->GetRunMaintenanceRank(i)) ++tankExpeditionValidationErrors_;
        }
        tankExpedition_.CompleteEvolution();
    } else return;
    if(tankExpedition_.GetPhase()==EPhase::Evolution) {
        player_->PrepareRunEvolution();tankExpeditionEvolutions_=player_->GetRunEvolutionChoices();
    }
    tankExpeditionMaintenanceOpen_=false;
    tankRunSelection_=0;tankRunMenuAge_=-0.12f;
    if(tankExpedition_.IsCombat()) StartTankExpeditionRoom();
    RefreshTankExpeditionUi();
}

void GameScene::UpdateTankExpedition(float dt) {
    if(titleDemo_) return;
    UpdateTankExpeditionAudio(dt);
    if(expeditionMapEnabled_) {UpdateExpeditionMap(dt);return;}
    if(phase_!=Phase::kMain) return;
    if(tankExpeditionBalanceEditorOpen_||(cg2::kDeveloperTools&&input_->IsKeyTriggered(DIK_F2))) return;
    tankRunMenuAge_+=dt;tankRunAutoTime_+=dt;
    if(tankExpeditionTutorialValidation_.enabled) UpdateTankExpeditionTutorialValidation(dt);
    const auto triggered=[this](int key){return input_->IsKeyTriggered(static_cast<uint8_t>(key));};
    if(tankExpedition_.IsCombat()&&!tankRunPaused_&&combatFlow_.GetState()==GameFlowState::Playing&&triggered(DIK_TAB)) {
        tankExpeditionDetailsOpen_=!tankExpeditionDetailsOpen_;RefreshTankExpeditionUi();
    }
    if(tankExpeditionDetailsOpen_) {
        if(triggered(DIK_ESCAPE)) tankExpeditionDetailsOpen_=false;
        return;
    }
    if(!tankRunPaused_&&combatFlow_.GetState()==GameFlowState::Playing) UpdateTankExpeditionTutorial(dt);
    if(cg2::kDeveloperTools&&triggered(DIK_F10)) RequestTankRunCapture("manual");
    if(triggered(DIK_M)) {
        tankExpeditionMusicEnabled_=!tankExpeditionMusicEnabled_;
        tankExpeditionAudio_.SetMusicVolume(tankExpeditionMusicEnabled_?0.55f:0.0f);
        SetEventCallout(tankExpeditionMusicEnabled_?"BGM ON":"BGM OFF",0.7f);
    }
    if(triggered(DIK_N)) {
        tankExpeditionEffectsEnabled_=!tankExpeditionEffectsEnabled_;
        tankExpeditionAudio_.SetEffectsVolume(tankExpeditionEffectsEnabled_?0.80f:0.0f);
        SetEventCallout(tankExpeditionEffectsEnabled_?"SE ON":"SE OFF",0.7f);
    }
    if(combatFlow_.GetState()==GameFlowState::Playing&&!player_->IsChangeMode()&&triggered(DIK_ESCAPE)) {
        tankRunPaused_=!tankRunPaused_;tankRunSelection_=0;tankRunMenuAge_=0;RefreshTankRunUi();
    }
    if(combatFlow_.GetState()!=GameFlowState::Playing) {
        if(tankRunAutoTest_&&combatFlow_.GetState()==GameFlowState::StageClear) {
            if(tankRunAutoStep_<100) {RequestTankRunCapture("result");tankRunAutoStep_=100;tankRunAutoTime_=0;}
            if(tankRunAutoTime_>1.2f) {
                std::ofstream log(ExpeditionDirectory(tankExpeditionAutoVariant_)+"/validation.json");
                log<<"{\"completed\":true,\"testMode\":true,\"rooms\":"<<tankExpedition_.GetRoomIndex()+1
                    <<",\"routes\":["<<tankExpedition_.GetRouteChoice(0)<<","<<tankExpedition_.GetRouteChoice(1)
                    <<"],\"event\":"<<tankExpedition_.GetEventChoice()<<",\"rewards\":"<<tankExpedition_.GetRewardCount()
                    <<",\"playerHp\":"<<player_->GetHp()<<",\"maxHp\":"<<player_->GetMaxHp()
                    <<",\"maintenanceRemaining\":"<<player_->GetRunMaintenancePoints()<<",\"maintenanceRanks\":["
                    <<player_->GetRunMaintenanceRank(0)<<","<<player_->GetRunMaintenanceRank(1)<<","<<player_->GetRunMaintenanceRank(2)
                    <<"],\"validationErrors\":"<<tankExpeditionValidationErrors_
                    <<",\"audioLoaded\":"<<tankExpeditionAudio_.LoadedClipCount()<<",\"audioEvents\":"<<tankExpeditionAudio_.PlayCount()
                    <<",\"musicPlaying\":"<<(tankExpeditionAudio_.IsMusicPlaying()?"true":"false")
                    <<",\"class\":\""<<player_->GetCurrentClassName()<<"\",\"cards\":[";
                for(size_t i=0;i<tankrun::CardCount;++i) log<<(i?",":"")<<tankRun_.GetCardCounts()[i];
                log<<"]}\n";log.close();PostQuitMessage(0);
            }
        }
    } else if(!player_->IsChangeMode()) {
        const EPhase phase=tankExpedition_.GetPhase();
        const bool menu=tankRunPaused_||phase==EPhase::Dormant||phase==EPhase::Reward||phase==EPhase::Route||phase==EPhase::Event||phase==EPhase::Evolution;
        if(menu) {
            int count=GetTankExpeditionOptionCount();
            if(tankRunMenuAge_>0.16f&&count>0) {
                if(triggered(DIK_LEFT)||triggered(DIK_A)) tankRunSelection_=(tankRunSelection_+count-1)%count;
                if(triggered(DIK_RIGHT)||triggered(DIK_D)) tankRunSelection_=(tankRunSelection_+1)%count;
                const auto mouse=input_->GetMousePosition();const auto motion=input_->GetMouseState();int hovered=-1;
                const bool leftClick=input_->IsTrigger(motion.rgbButtons[0],input_->GetPreMouseState().rgbButtons[0]);
                if(!tankRunPaused_&&phase==EPhase::Reward&&(triggered(DIK_E)||
                    (leftClick&&mouse.x>=64&&mouse.x<=1208&&mouse.y>=568&&mouse.y<=600))) {
                    tankExpeditionMaintenanceOpen_=!tankExpeditionMaintenanceOpen_;
                    tankRunSelection_=0;count=GetTankExpeditionOptionCount();RefreshTankExpeditionUi();
                }
                for(int i=0;i<count;++i) {const float x=64+static_cast<float>(i)*388;if(mouse.x>=x&&mouse.x<=x+368&&mouse.y>=280&&mouse.y<=560) hovered=i;}
                if(hovered>=0&&(motion.lX||motion.lY)) tankRunSelection_=hovered;
                bool confirm=triggered(DIK_RETURN)||triggered(DIK_SPACE);
                for(int i=0;i<count;++i) if(triggered(DIK_1+i)) {tankRunSelection_=i;confirm=true;}
                if(hovered>=0&&leftClick) {tankRunSelection_=hovered;confirm=true;}
                const bool refund=!tankRunPaused_&&tankExpeditionMaintenanceOpen_&&
                    ((confirm&&(input_->GetKey()[DIK_LSHIFT]||input_->GetKey()[DIK_RSHIFT]))||
                    (hovered>=0&&input_->IsTrigger(motion.rgbButtons[1],input_->GetPreMouseState().rgbButtons[1])));
                if(refund) {
                    if(hovered>=0&&input_->IsTrigger(motion.rgbButtons[1],input_->GetPreMouseState().rgbButtons[1])) tankRunSelection_=hovered;
                    player_->RefundRunMaintenancePoint(tankRunSelection_);RefreshTankExpeditionUi();
                } else if(confirm) SelectTankRunOption(tankRunSelection_);
            }
            if(tankRunAutoTest_&&!tankRunPaused_) {
                if(tankRunMenuAge_>0.4f&&tankExpeditionCaptureIndex_==tankRunAutoMenuIndex_) {
                    RequestTankRunCapture("choice_"+std::to_string(tankRunAutoMenuIndex_));++tankExpeditionCaptureIndex_;
                }
                if(phase==EPhase::Reward&&tankRunMenuAge_>0.7f&&tankExpeditionAutoMaintainedRoom_!=tankExpedition_.GetRoomIndex()) {
                    tankExpeditionMaintenanceOpen_=true;
                    const int rank=(tankExpedition_.GetRoomIndex()+tankExpeditionAutoVariant_)%3;
                    SelectTankExpeditionOption(rank);
                    const int hp=player_->GetHp();
                    player_->RefundRunMaintenancePoint(rank);
                    SelectTankExpeditionOption(rank);
                    if(player_->GetHp()!=hp) ++tankExpeditionValidationErrors_;
                    tankExpeditionAutoMaintainedRoom_=tankExpedition_.GetRoomIndex();
                    RequestTankRunCapture("maintenance_"+std::to_string(tankExpedition_.GetRoomIndex()+1));
                }
                if(tankRunMenuAge_>1.1f) {
                    tankExpeditionMaintenanceOpen_=false;
                    int option=0;
                    const int loadouts[]={0,2,0,1,1,2};
                    const int evolutions[]={0,0,1,0,1,1};
                    if(phase==EPhase::Dormant) option=loadouts[tankExpeditionAutoVariant_];
                    if(phase==EPhase::Route) option=tankExpedition_.GetRouteRound()==0?tankExpeditionAutoVariant_%2:(tankExpeditionAutoVariant_/2)%2;
                    if(phase==EPhase::Event) option=tankExpeditionAutoVariant_%3==0?0:1;
                    if(phase==EPhase::Evolution) option=evolutions[tankExpeditionAutoVariant_];
                    SelectTankRunOption(option);++tankRunAutoMenuIndex_;
                }
            }
        } else if(phase==EPhase::Combat&&tankRunMenuAge_>=0) {
            if(player_->IsDead()) {BeginGameOver();return;}
            const Room room=tankExpedition_.GetRoomKind();
            // 通常敵の撃破/資源取得コールバックで更新された状態から目標を判定する。
            // 死亡済みの敵は脅威数へ含めず、初回チュートリアルの退出条件も確認する。
            const int threats=LivingThreats(enemyManager_.get());
            if(room==Room::Resource&&!tankExpeditionResourceReleased_&&threats==0&&tankExpeditionSpawned_>0) {
                auto& node=tankRunResources_[0];
                node.active=enemyManager_->SpawnRunResource(node.position,90,[this](bool owned){OnTankRunResourceClaim(0,owned);});
                tankExpeditionResourceReleased_=node.active;
                SetEventCallout("敵を撃破！ 通貨ボックスが出現",1.4f);
                tankExpeditionAudio_.EnemyWarning();
            }
            if(room!=Room::Boss&&tankExpeditionSpawned_>0&&
                (tankExpedition_.GetRoomIndex()!=0||tankExpeditionTutorial_.CanLeaveFirstRoom())&&tankexp::IsRoomObjectiveComplete(
                room,threats,tankExpeditionNodes_,tankExpeditionRoomPending_,enemy_->IsDead())) FinishTankExpeditionRoom();
            else {
                tankExpedition_.Update(dt);tankRun_.Update(dt);tankExpeditionArrival_+=dt;
                tankRunComboTime_=(std::max)(0.0f,tankRunComboTime_-dt);
                for(auto& burst:tankRunBursts_) burst.age+=dt;
                std::erase_if(tankRunBursts_,[](const RunBurst& b){return b.age>(b.resource?0.7f:0.35f);});
                // Deterministic rendering/transition smoke test; this does not measure balance.
                if(tankRunAutoTest_) {
                    const int step=tankExpedition_.GetRoomIndex()*2;
                    if(tankRunAutoStep_==step&&tankExpeditionArrival_>1.5f) {RequestTankRunCapture("room_"+std::to_string(tankExpedition_.GetRoomIndex()+1));++tankRunAutoStep_;}
                    if(tankExpeditionArrival_>3&&room!=Room::Boss) {
                        for(auto* actor:enemyManager_->GetEnemyPtrs()) if(actor&&!actor->IsDead()) {
                            if(!actor->IsCombatThreat()&&!actor->IsRunResource()) continue;
                            if(room==Room::Resource&&tankExpeditionAutoVariant_==2) actor->TakeDamageFromEnemy(100000);
                            else actor->TakeDamageFromPlayer(100000);
                        }
                        tankRunAutoStep_=step+2;
                    }
                    if(room==Room::Boss&&tankExpeditionArrival_>5) enemy_->TakeDamage(100000);
                }
            }
        }
    }
    tankRunHudTimer_-=dt;
    if(tankRunHudTimer_<=0) {RefreshTankRunUi();tankRunHudTimer_=0.10f;}
}

void GameScene::RefreshTankExpeditionUi() {
    const EPhase phase=tankExpedition_.GetPhase();
    const Room room=tankExpedition_.GetRoomKind();
    const bool terminal=phase==EPhase::Clear||phase==EPhase::Dead;
    const bool inCombat=phase==EPhase::Combat&&!tankRunPaused_;
    tankRunHudPanel_->SetSize({264,78});tankRunHudPanel_->SetColor({0.009f,0.016f,0.03f,0.66f});tankRunHudPanel_->Update();
    tankRunHud_->SetText("HP "+std::to_string(player_->GetHp())+" / "+std::to_string(player_->GetMaxHp())+
        (expeditionMapEnabled_?"":"   Lv."+std::to_string(player_->GetLevel())));
    auto hudStyle=tankRunHud_->GetStyle();hudStyle.fontSize=17;tankRunHud_->SetStyle(hudStyle);
    tankRunHud_->SetPosition({20,16});
    const float playerHp=static_cast<float>(player_->GetHp())/(std::max)(1,player_->GetMaxHp());
    tankExpeditionHpFill_->SetSize({230*(std::clamp)(playerHp,0.0f,1.0f),6});tankExpeditionHpFill_->Update();
    const float xp=static_cast<float>(player_->GetExp())/(std::max)(1,player_->GetNextLevelExpValue());
    tankExpeditionExpFill_->SetSize({1232*(std::clamp)(xp,0.0f,1.0f),6});tankExpeditionExpFill_->Update();
    tankExpeditionExpText_->SetText(expeditionMapEnabled_?
        " ":
        "EXP "+std::to_string(player_->GetExp())+" / "+std::to_string(player_->GetNextLevelExpValue()));
    int threats=LivingThreats(enemyManager_.get());
    if(expeditionMapEnabled_) {threats=0;for(auto* actor:enemyManager_->GetEnemyPtrs()) if(actor&&!actor->IsDead()&&!actor->IsRunResource()) ++threats;}
    const std::string objective=room==Room::Resource?(tankExpeditionResourceReleased_?"金色の通貨ボックスを壊そう":"敵を倒して宝物を手に入れよう  あと "+std::to_string(threats)):
        room==Room::Guard?"通貨ボックス "+std::to_string(tankExpeditionNodes_)+" / 3   敵 あと "+std::to_string(threats):
        room==Room::Boss?"ボスを倒そう":(expeditionMapEnabled_?"敵とブロックを倒そう  あと ":"敵を全滅させろ  残り ")+std::to_string(threats);
    tankRunObjectiveText_->SetAlpha(0.90f+0.10f*std::sin(expeditionPresentationClock_*2.0f));
    tankRunObjectiveText_->SetAnchorPoint({0.5f,0});tankRunObjectiveText_->SetPosition({640,12});
    const auto* mapNode=expeditionMapEnabled_?expeditionMapRun_.GetActiveNode():nullptr;
    tankRunObjectiveText_->SetText(mapNode?(inCombat?objective:std::string(" ")):
        "ROOM "+std::to_string(tankExpedition_.GetRoomIndex()+1)+" / 5\n"+(inCombat?objective:std::string(RoomName(room))));
    if(expeditionMapEnabled_&&!mapNode) tankRunObjectiveText_->SetText("作戦マップ");
    tankRunBossText_->SetAnchorPoint({0.5f,0});tankRunBossText_->SetPosition({640,80});
    tankRunBossText_->SetText("最深部のボス");
    if(enemy_->IsNeonDepthEncounterEnabled()) {
        const auto& status=enemy_->GetNeonDepthSnapshot();
        const char* attack=status.plan.attack==neondepth::Attack::Volley?"奥行き弾幕":
            status.plan.attack==neondepth::Attack::Dive?"降下突撃":"床接続ビーム";
        const char* action=status.phase==neondepth::Phase::Intro?"投影体が形成中 / Enterでスキップ":
            status.phase==neondepth::Phase::Telegraph?"予告線を見て離れよう":
            status.phase==neondepth::Phase::Locked?"照準固定":
            status.phase==neondepth::Phase::Airborne?"発動まであと少し":
            status.phase==neondepth::Phase::Active?"危険エリアの外へ":
            status.phase==neondepth::Phase::Recovery?"反撃のチャンス / 床コアを攻撃":"床コアを攻撃";
        tankRunBossText_->SetText(std::string(status.plan.phaseTwo?"ボス・第2段階 / ":"ボス / ")+attack+"  "+action);
    } else if(enemy_->IsExpeditionRivalEnabled()) {
        const auto status=enemy_->GetRivalCombatStatus();using P=RivalBossCombat::Phase;
        const char* action=status.phase==P::Reload?"装填中 / 反撃のチャンス":
            status.phase==P::DashWarning?"突進予告":status.phase==P::Dash?"突進中":
            status.phase==P::Locked?"照準固定 / 射線から離れよう":status.phase==P::Volley?"連続射撃":"移動中";
        tankRunBossText_->SetText(std::string(status.phase2?"ボス・第2段階 / ":"ボス / ")+action+"  残弾 "+std::to_string(status.ammo)+" / "+std::to_string(status.capacity));
    }
    tankRunBossTrack_->SetPosition({460,111});tankRunBossTrack_->SetSize({360,5});tankRunBossTrack_->Update();
    const float hp=static_cast<float>(enemy_->GetHp())/(std::max)(1,enemy_->GetMaxHp());
    tankRunBossFill_->SetPosition({460,111});tankRunBossFill_->SetSize({360*(std::clamp)(hp,0.0f,1.0f),5});tankRunBossFill_->Update();
    const char* features[]={"跳弾ビルド","突撃ビルド","群体ビルド","標準戦車"};
    const auto core=(std::clamp)(static_cast<int>(tankRun_.GetCore()),0,3);
    std::string build=std::string("BUILD / ")+(tankRun_.GetDraftCount()<2?"基本射撃":features[core]);
    const std::string combatStyle=player_->IsMeleeBuild()?"近接ブレード":player_->IsDroneBuild()?"ドローン編隊":"シューター";
    if(expeditionMapEnabled_) build=combatStyle;
    const auto& counts=tankRun_.GetCardCounts();const auto visible=tankexp::CompactBuildCards(counts);
    int shown=0;
    std::ostringstream details;details<<(expeditionMapEnabled_?"取得した強化   [Tab / Escで閉じる]\n\n":"BUILD / 改造一覧   [TAB / ESC で閉じる]\n\n")
        <<"機体: "<<player_->GetCurrentClassName()<<"  /  "<<(expeditionMapEnabled_?combatStyle:features[core])<<"\n";
    if(tankRun_.GetDraftCount()<2&&!expeditionMapEnabled_) details<<"主軸コアは改造を2つ取得すると起動\n";
    for(size_t i=0;i<tankrun::CardCount;++i) if(counts[i]>0) {
        const std::string name=std::string(ExpeditionCardCopy(static_cast<int>(i)).title)+(expeditionMapEnabled_?"":" Lv."+std::to_string(counts[i]));
        details<<"\n"<<name;
        if(visible[i]) {build+="\n"+name;++shown;}
    }
    if(!tankRun_.GetDraftCount()) {
        build+=player_->IsMeleeBuild()?"\n基本装備 / 3段斬り":player_->IsDroneBuild()?"\n基本装備 / 左クリックで指揮":"\n基本装備 / 単発射撃";
        details<<(expeditionMapEnabled_?"\nまだ強化を取得していません":"\nまだ改造を取得していません");
    }
    if(!expeditionMapEnabled_) build+="\nTAB 詳細";
    auto buildStyle=tankRunBuildText_->GetStyle();buildStyle.fontSize=13;tankRunBuildText_->SetStyle(buildStyle);
    tankRunBuildText_->SetPosition({996,14});tankRunBuildText_->SetText(build);
    tankExpeditionBuildPanel_->SetSize({280,static_cast<float>(shown+3)*21});tankExpeditionBuildPanel_->Update();
    if(!expeditionMapEnabled_) details<<"\n\n整備: 機動 "<<player_->GetRunMaintenanceRank(0)<<" / 装填 "<<player_->GetRunMaintenanceRank(1)
        <<" / 装甲 "<<player_->GetRunMaintenanceRank(2)<<"   残り "<<player_->GetRunMaintenancePoints()<<" pt";
    if(expeditionMapEnabled_) details<<"\n\n通貨: "<<expeditionMapRun_.GetCurrency()<<"\n工房の強化・修理に使います";
    tankExpeditionDetailsText_->SetText(details.str());
    std::string route="外周 → ";
    route+=tankExpedition_.GetRouteChoice(0)<0?"[資源 / 精鋭]":tankExpedition_.GetRouteChoice(0)==0?"資源":"精鋭";
    route+=" → イベント・進化 → ";
    route+=tankExpedition_.GetRouteChoice(1)<0?"[反射 / ドローン]":tankExpedition_.GetRouteChoice(1)==0?"反射":"ドローン";
    route+=" → 制圧 → ボス";tankExpeditionMapText_->SetText(expeditionMapEnabled_?"":route);
    tankExpeditionMapText_->SetPosition({24,inCombat?625.0f:670.0f});
    auto card=[this](int index,const std::string& title,const std::string& body) {tankRunCardTitles_[index]->SetText(std::to_string(index+1)+"  "+title);tankRunCardBodies_[index]->SetText(body);};
    if(terminal) {
        tankRunHeading_->SetText(phase==EPhase::Clear?"最深部突破 / 遠征成功":"戦車が大破 / 遠征終了");
        tankRunDescription_->SetText("到達区画 "+std::to_string(tankExpedition_.GetRoomIndex()+1)+" / 5    撃破 "+std::to_string(defeatedEnemies_)+"    最長 "+std::to_string(tankRunBestCombo_)+" CHAIN\n改造 "+std::to_string(tankRun_.GetDraftCount())+"個 / 次は違う機体・主軸・進路も試してみよう。");
        if(expeditionMapEnabled_) tankRunDescription_->SetText("クリアした地点 "+std::to_string(expeditionMapRun_.GetVisitedNodeIds().size())+"  /  強化 "+std::to_string(tankRun_.GetDraftCount())+"個  /  残り "+std::to_string(expeditionMapRun_.GetCurrency())+"\n次は別の戦闘スタイルや強化を試してみよう。");
        card(0,"もう一度挑戦",expeditionMapEnabled_?"新しい遠征をはじめます。":"作戦マップから再スタート。\n\n新しい主軸や改造を試す。");
        card(1,"タイトルへ",expeditionMapEnabled_?"今回の遠征を終えます。":"今回の遠征を終える。\n\nタイトルのF10で\nコア争奪アリーナも遊べます。");
    } else if(tankRunPaused_) {
        tankRunHeading_->SetText("一時停止");tankRunDescription_->SetText("敵・弾・戦闘時間は停止中です。\nM: BGM "+std::string(tankExpeditionMusicEnabled_?"ON":"OFF")+" / N: SE "+std::string(tankExpeditionEffectsEnabled_?"ON":"OFF")+"（押すと切り替え）");
        card(0,"続ける","現在の区画に戻ります。");card(1,"タイトルへ",expeditionMapEnabled_?"今回の強化と進行状況は\nリセットされます。":"今回の改造と進行状況は\nリセットされます。");
    } else if(phase==EPhase::Reward&&tankExpeditionMaintenanceOpen_) {
        tankRunHeading_->SetText("機体整備 / 弱点を補う");
        tankRunDescription_->SetText("残り "+std::to_string(player_->GetRunMaintenancePoints())+" ポイント / 区画突破ごとに +1、各項目は3段階まで。\n報酬画面で振り直し可能。装甲は被ダメージを軽減し、HPは回復しません。");
        const auto choices=player_->GetRunMaintenanceChoices();
        for(int i=0;i<3;++i) card(i,choices[i].name+"  "+std::to_string(choices[i].rank)+" / 3",choices[i].description+
            (choices[i].rank>=3?"\n\n強化上限":choices[i].canSpend?"\n\n選択で1ポイント使用":"\n\nポイントを持ち越して整備可能"));
    } else if(phase==EPhase::Reward) {
        tankRunHeading_->SetText("区画報酬 / 改造を1つ選ぶ");
        if(tankExpedition_.GetEventChoice()==1&&tankExpedition_.GetRoomIndex()==1) tankRunHeading_->SetText("禁制の改造 / レア候補を獲得");
        tankRunDescription_->SetText("機体を進化させても主軸・改造・整備は引き継ぎます。\nE または下の整備ボタンで能力を調整。改造を選ぶと先へ進みます。");
        if(tankExpeditionRewardOpen_) for(size_t i=0;i<tankRun_.GetOfferCount();++i) {
            const auto id=tankRun_.GetOffers()[i];const auto copy=ExpeditionCardCopy(static_cast<int>(id));
            const std::string title=copy.title;card(static_cast<int>(i),tankrun::IsRare(id)?"レア\n"+title:title,copy.body);
        } else card(0,"次へ","改造の取得上限に達しました。\n現在の構成で進みます。");
    } else if(phase==EPhase::Route) {
        tankRunHeading_->SetText("次の区画を選ぶ");
        tankRunDescription_->SetText("敵の配置・地形と、欲しい報酬から進路を選ぼう。\nHPと改造は持ち越し。移動時にスタミナが全回復します。");
        if(tankExpedition_.GetRouteRound()==0) {
            card(0,"宝物庫","護衛を倒すとコアが出現。\nライバルより先に確保しよう。\n\n報酬: 改造1つ・整備 +1\n確保なら HP +8・レア候補。");
            card(1,"精鋭部隊","突進兵と狙撃兵の混成部隊。\n危険な壁に触れず突破しよう。\n\n報酬: 改造1つ・整備 +1\nレア候補を1つ以上保証。");
        } else {
            card(0,"反射実験区画","狙撃兵が多い遮蔽物の区画。\n射線を切り、壁反射を活かす。\n\n報酬: 改造1つ・整備 +1\n未所持なら反射改造が候補に。");
            card(1,"ドローン格納庫","突進兵が通路から迫る区画。\n本体と群れの位置取りを試す。\n\n報酬: 改造1つ・整備 +1\n未所持なら援護ドローン候補。");
        }
    } else if(phase==EPhase::Event) {
        tankRunHeading_->SetText("中継地点 / 放棄された整備庫");
        tankRunDescription_->SetText("まだ使える修理装置と、危険な試作品を発見した。\nどちらかを利用したら、次は機体の進化を選びます。");
        card(0,"機体を修理する","HPを40回復する。\n最大HPを超えて回復しない。\n\n残り3区画に備える。");
        card(1,player_->GetHp()>20?"禁制の改造を取り出す":"禁制の改造 / HP不足","HPを20支払う。\nレアを含む3候補から1つ獲得。\n\nHPが21以上必要。\n組み合わせを増やす選択。");
    } else if(phase==EPhase::Evolution) {
        tankRunHeading_->SetText("機体進化 / 次の戦い方へ");
        tankRunDescription_->SetText("現在: "+std::string(player_->GetCurrentClassName())+"  / 同じ機体から異なる2つの方向へ。\n主軸・改造・整備・現在HPを保ち、残り3区画の戦い方を選びます。");
        for(size_t i=0;i<tankExpeditionEvolutions_.size();++i) card(static_cast<int>(i),tankExpeditionEvolutions_[i].name,tankExpeditionEvolutions_[i].description);
        if(tankExpeditionEvolutions_.size()<3) card(static_cast<int>(tankExpeditionEvolutions_.size()),"現在の機体で進む","今回は進化せず、\n今の射撃感を維持する。\n\n主軸・改造・整備で\n現在の機体を伸ばす。");
    }
    tankRunFooter_->SetText(terminal?
        "数字キー / クリック: 決定    ← → + Enter: 選択\nもう一度挑戦するか、タイトルへ戻ります。":
        "数字キー / クリック: 決定    ← → + Enter: 選択    Esc: 一時停止\n選択後は次の区画へ。手に入れた改造を試しながら最深部を目指そう。");
    if(tankRunPaused_&&expeditionMapEnabled_)
        tankRunFooter_->SetText("クリックで決定    Esc: ゲームに戻る\n続けるか、タイトルへ戻るかを選べます。");
    if(phase==EPhase::Reward&&!tankRunPaused_) {
        tankExpeditionMaintenanceText_->SetText(tankExpeditionMaintenanceOpen_?
            "E / クリック: 改造の選択へ戻る":"E / クリック: 機体整備   残り "+std::to_string(player_->GetRunMaintenancePoints())+" ポイント（未使用分は持ち越し）");
        if(tankExpeditionMaintenanceOpen_) tankRunFooter_->SetText("数字キー / 左クリック: 強化    Shift + 数字 / 右クリック: 1段階戻す\n整備が済んだら E で改造の選択へ戻ります。選択中は戦闘が停止します。");
    }
    RefreshTankExpeditionTutorialUi();
    if(expeditionMapEnabled_) RefreshExpeditionMapUi();
}

void GameScene::DrawTankExpeditionUi() {
    if(expeditionMapEnabled_ && (expeditionMapPreview_ || (tankExpedition_.GetPhase()==EPhase::Map&&!tankRunPaused_))) {DrawExpeditionMapUi();return;}
    if(player_->IsChangeMode()) return;
    cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
    const auto phase=tankExpedition_.GetPhase();
    const bool result=combatFlow_.GetState()==GameFlowState::StageClear||(combatFlow_.GetState()==GameFlowState::GameOver&&combatFlow_.GetTimer()<=0);
    const bool decision=tankRunPaused_||phase==EPhase::Reward||phase==EPhase::Route||phase==EPhase::Event||phase==EPhase::Evolution;
    if(decision||result||tankExpeditionDetailsOpen_) tankRunDimmer_->Draw();
    tankRunHudPanel_->Draw();tankRunHud_->Draw();tankExpeditionHpTrack_->Draw();tankExpeditionHpFill_->Draw();
    if(expeditionMapEnabled_) DrawExpeditionVitals();
    tankRunObjectiveText_->Draw();
    if(tankExpeditionDetailsOpen_) {tankExpeditionDetailsText_->Draw();tankExpeditionMapText_->Draw();return;}
    if(tankExpedition_.GetRoomKind()==Room::Boss&&tankExpeditionRivalActive_&&!decision&&!result) {
        tankRunBossText_->Draw();tankRunBossTrack_->Draw();tankRunBossFill_->Draw();
    }
    if(!decision&&!result) {
        tankExpeditionBuildPanel_->Draw();tankRunBuildText_->Draw();
        tankExpeditionExpText_->Draw();
        if(!expeditionMapEnabled_) {tankExpeditionExpTrack_->Draw();tankExpeditionExpFill_->Draw();}
        DrawTankExpeditionTutorial();return;
    }
    tankRunHeading_->Draw();tankRunDescription_->Draw();
    const int count=result?2:GetTankExpeditionOptionCount();
    for(int i=0;i<count;++i) {
        const bool selected=i==(result?resultSelection_:tankRunSelection_);
        const bool rare=phase==EPhase::Reward&&!tankRunPaused_&&!tankExpeditionMaintenanceOpen_&&tankExpeditionRewardOpen_&&tankrun::IsRare(tankRun_.GetOffers()[i]);
        tankRunCards_[i]->SetColor(selected?(rare?cg2::Vector4{0.21f,0.12f,0.045f,1}:cg2::Vector4{0.045f,0.16f,0.19f,1}):cg2::Vector4{0.028f,0.045f,0.075f,1});
        tankRunCards_[i]->Update();tankRunCards_[i]->Draw();tankRunCardTitles_[i]->Draw();tankRunCardBodies_[i]->Draw();
    }
    if(phase==EPhase::Reward&&!tankRunPaused_) {tankExpeditionMaintenanceButton_->Draw();tankExpeditionMaintenanceText_->Draw();}
    if(!tankExpeditionTutorial_.IsVisible()||tankRunPaused_||result) tankRunFooter_->Draw();
    if(!tankRunPaused_&&!result) DrawTankExpeditionTutorial();
}

void GameScene::UpdateTankExpeditionTutorial(float dt) {
    if(titleDemo_||tankExpedition_.GetPhase()==EPhase::Dormant||!tankExpeditionTutorial_.IsVisible()) return;
    const auto position=player_->GetWorldPosition();
    if(tankExpedition_.IsCombat()) {
        if(player_->HasMovementInput()&&!player_->IsDashing())
            tankExpeditionTutorial_.AddMovement(cg2::Length(position-tankExpeditionTutorialPrevious_));
        if(defeatedEnemies_>tankExpeditionTutorialKills_) tankExpeditionTutorial_.RecordKill();
        // Observe the actual dash state without consuming the effect/audio event.
        if(player_->IsDashing()) tankExpeditionTutorial_.RecordDash();
    }
    tankExpeditionTutorialPrevious_=position;tankExpeditionTutorialKills_=defeatedEnemies_;
    if(cg2::kDeveloperTools&&input_->IsKeyTriggered(DIK_F3)) tankExpeditionTutorial_.Skip();
    tankExpeditionTutorial_.Update(dt);
    if(tankExpeditionTutorial_.IsComplete())SaveExpeditionTutorialCompletion();
    RefreshTankExpeditionTutorialUi();
}

void GameScene::SaveExpeditionTutorialCompletion() {
    const bool isolatedSubmission=tanksubmission::Enabled();
    const bool validation=tankRunAutoTest_||expeditionMapAutoTest_||combatValidationEnabled_||
        experienceValidationVariant_!=0||specialValidationEnabled_||tankExpeditionTutorialValidation_.enabled;
    if(tankExpeditionTutorialSaved_||!tankexp::ShouldSaveTutorialCompletion(titleDemo_,validation,isolatedSubmission))return;
    // Both tutorial flows share the existing settings file. Preserve unknown
    // settings and publish the complete document with one atomic replacement.
    try {
        nlohmann::json settings=nlohmann::json::object();
        {std::ifstream in(TutorialSettingsPath());if(in) {
            settings=nlohmann::json::parse(in,nullptr,false);
            if(!settings.is_object())return; // Preserve a damaged file for recovery.
        }}
        settings["tutorialCompleted"]=true;
        const std::string temporary=std::string(TutorialSettingsPath())+".tmp";
        std::ofstream out(temporary);out<<std::setw(2)<<settings<<'\n';out.close();
        if(out)tankExpeditionTutorialSaved_=MoveFileExA(temporary.c_str(),TutorialSettingsPath(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
        if(tankExpeditionTutorialSaved_)expeditionTutorialPreviouslyCompleted_=true;
    } catch(...) {OutputDebugStringA("[TankExpedition] Tutorial settings could not be saved\n");}
}

void GameScene::UpdateTankExpeditionTutorialValidation(float dt) {
    auto& validation=tankExpeditionTutorialValidation_;
    validation.elapsed+=dt;
    const int step=static_cast<int>(tankExpeditionTutorial_.GetStep());
    if(step!=validation.lastStep) {validation.lastStep=step;validation.stepAge=0;}
    validation.stepAge+=dt;
    const bool started=tankExpedition_.GetPhase()!=EPhase::Dormant;
    const unsigned bit=1u<<step;
    if(started&&validation.stepAge>0.15f&&!(validation.observedSteps&bit)&&tankRunCapturePath_.empty()) {
        tankRunCapturePath_="generated/tank_expedition/tutorial_validation/step_"+std::to_string(step+1)+".png";
        validation.observedSteps|=bit;
    }
    if(validation.elapsed>120||(step==static_cast<int>(tankexp::TutorialStep::Hidden)&&validation.stepAge>0.8f&&tankRunCapturePath_.empty())) {
        const bool done=tankExpeditionTutorial_.IsComplete()&&validation.observedSteps==255u&&
            defeatedEnemies_>0&&tankExpedition_.GetRouteChoice(0)>=0&&tankRun_.GetDraftCount()>0;
        nlohmann::json result={{"completed",done},{"testMode",true},{"realProjectileKills",defeatedEnemies_},
            {"movementDistance",tankExpeditionTutorial_.GetMoveDistance()},{"observedStepMask",validation.observedSteps},
            {"route",tankExpedition_.GetRouteChoice(0)},{"cards",tankRun_.GetDraftCount()},
            {"tutorialVisible",tankExpeditionTutorial_.IsVisible()},{"elapsed",validation.elapsed},
            {"forcedDamage",false},{"invulnerable",true}};
        std::ofstream("generated/tank_expedition/tutorial_validation/validation.json")<<std::setw(2)<<result<<'\n';
        PostQuitMessage(done?0:3);return;
    }
    if(!started) {
        player_->SetDemoInput(true,{0,0},player_->GetWorldPosition()+cg2::Vector3{1,0,0},false,false);
        if(tankRunMenuAge_>0.85f) SelectTankRunOption(0);
        return;
    }
    player_->SetDemoInput(true,{0,0},player_->GetWorldPosition()+cg2::Vector3{1,0,0},false,false);
    if(validation.stepAge<0.6f||tankExpeditionTutorial_.IsSuccess()) return;
    using Step=tankexp::TutorialStep;
    const auto lesson=tankExpeditionTutorial_.GetStep();
    const auto position=player_->GetWorldPosition();cg2::Vector3 aim=position+cg2::Vector3{1,0,0};
    float distance=10000;
    for(auto* actor:enemyManager_->GetEnemyPtrs()) if(actor&&actor->IsCombatThreat()) {
        const float candidate=cg2::Length(actor->GetWorldPosition()-position);
        if(candidate<distance) {distance=candidate;aim=actor->GetWorldPosition();}
    }
    cg2::Vector2 move{};
    if(lesson==Step::Move) move={1,0};
    const bool shoot=lesson==Step::Shoot||lesson==Step::ClearRoom;
    if(shoot&&distance>13&&distance<1000) {const auto direction=cg2::Normalize(aim-position);move={direction.x,direction.y};}
    if(lesson==Step::Dash) move={0,1};
    player_->SetDemoInput(true,move,aim,shoot,lesson==Step::Dash);
    if(lesson==Step::Route&&tankExpedition_.GetPhase()==EPhase::Route&&tankRunMenuAge_>1.0f) SelectTankExpeditionOption(1);
    if(lesson==Step::Upgrade&&tankExpedition_.GetPhase()==EPhase::Reward&&tankRunMenuAge_>1.0f) SelectTankExpeditionOption(0);
}

void GameScene::RefreshTankExpeditionTutorialUi() {
    if(titleDemo_||!tankExpeditionTutorial_.IsVisible()) return;
    using Step=tankexp::TutorialStep;
    const auto step=tankExpeditionTutorial_.GetStep();
    const char* actions[]={"WASDで移動しよう","左クリックで敵を1体倒そう","右クリックでダッシュしよう",
        "敵を全滅させて区画を突破しよう","次の区画を選ぼう","改造カードを1枚選ぼう","準備完了 / 最深部を目指せ！"};
    const char* success[]={"✓ 移動","✓ 敵を撃破","✓ ダッシュ","✓ 区画突破","✓ 進路を選択","✓ 改造を取得"};
    const int index=static_cast<int>(step);
    tutorialTitleText_->SetText(step==Step::Complete?"操作練習完了":"操作練習  "+std::to_string(index+1)+" / 6");
    tutorialInputText_->SetText(tankExpeditionTutorial_.IsSuccess()?success[index]:
        expeditionMapEnabled_&&step==Step::Route?"マップの光る地点を選ぼう":expeditionMapEnabled_&&step==Step::Upgrade?"回収資材で改造を1つ購入しよう":actions[index]);
    tutorialDescriptionText_->SetText(tankExpeditionTutorial_.CanSkip()?"F3 / チュートリアルをスキップ":
        step==Step::Move?"移動した距離で達成":step==Step::Route||step==Step::Upgrade?"数字キー / クリック / ← → + Enter":" ");
    tutorialPanel_->SetColor(tankExpeditionTutorial_.IsSuccess()?cg2::Vector4{0.01f,0.14f,0.10f,0.9f}:cg2::Vector4{0.008f,0.025f,0.04f,0.84f});
    const float y=tankExpedition_.IsCombat()?590.0f:606.0f;
    tutorialPanel_->SetPosition({410,y});tutorialPanel_->SetSize({460,70});
    tutorialTitleText_->SetPosition({640,y+4});tutorialInputText_->SetPosition({640,y+21});tutorialDescriptionText_->SetPosition({640,y+50});
    tutorialPanel_->Update();
    tutorialTitleText_->PrepareForDraw();tutorialInputText_->PrepareForDraw();tutorialDescriptionText_->PrepareForDraw();
}

void GameScene::DrawTankExpeditionTutorial() {
    if(expeditionMapEnabled_) return;
    if(titleDemo_||!tankExpeditionTutorial_.IsVisible()) return;
    tutorialPanel_->Draw();tutorialTitleText_->Draw();tutorialInputText_->Draw();tutorialDescriptionText_->Draw();
}

void GameScene::UpdateTankExpeditionAudio(float dt) {
    const bool combat=tankExpedition_.IsCombat()&&combatFlow_.GetState()==GameFlowState::Playing;
    tankExpeditionAudio_.SetCombat(combat);
    tankExpeditionAudio_.SetBoss(tankExpedition_.GetRoomKind()==Room::Boss);
    tankExpeditionAudio_.SetDucked(IsTankRunMenuOpen()||phase_==Phase::kFadeOut);
    tankExpeditionAudio_.Update(dt);
    if(!combat||IsTankRunMenuOpen()) return;
    if(enemy_->IsExpeditionRivalEnabled()&&enemy_->GetRivalCombatStatus().phase2&&!expeditionBossPhase2Seen_) {
        expeditionBossPhase2Seen_=true;screenEffectDirector_.TriggerBossPhaseChange();
        SetEventCallout("ボス / 第二形態",1.1f);tankExpeditionAudio_.EnemyWarning();
    }
    std::unordered_map<const ExpEnemy*,int> hp;
    std::unordered_map<const ExpEnemy*,bool> warning;
    std::unordered_map<const ExpEnemy*,std::pair<uint64_t,uint64_t>> guardCounts;
    for(auto* actor:enemyManager_->GetEnemyPtrs()) if(actor&&!actor->IsDead()) {
        const auto counts=std::make_pair(actor->GetShieldBlockCount(),actor->GetBladeSwingCount());
        const auto oldGuard=guardAudioCounts_.find(actor);
        const auto before=oldGuard==guardAudioCounts_.end()?std::pair<uint64_t,uint64_t>{}:oldGuard->second;
        if(counts.first>before.first)tankExpeditionAudio_.ArmorBreak();
        if(counts.second>before.second)tankExpeditionAudio_.Slash();
        guardCounts[actor]=counts;
        const auto previous=tankExpeditionEnemyHp_.find(actor);
        if(previous!=tankExpeditionEnemyHp_.end()&&actor->GetHp()<previous->second) {
            tankExpeditionAudio_.Hit();
            if(expeditionMapEnabled_&&expeditionHitSparkCooldown_<=0&&expeditionHitSparks_.size()<12) {
                auto direction=actor->GetWorldPosition()-player_->GetWorldPosition();
                direction=cg2::Length(direction)>0.01f?cg2::Normalize(direction):cg2::Vector3{0,1,0};
                expeditionHitSparks_.push_back({actor->GetWorldPosition(),direction,0});expeditionHitSparkCooldown_=0.045f;
            }
        }
        hp[actor]=actor->GetHp();
        const bool locked=actor->IsExpeditionCombatRole()&&actor->IsAttackAimLocked();
        const auto old=tankExpeditionEnemyWarning_.find(actor);
        if(locked&&(old==tankExpeditionEnemyWarning_.end()||!old->second)) tankExpeditionAudio_.EnemyWarning();
        warning[actor]=locked;
    }
    tankExpeditionEnemyHp_=std::move(hp);tankExpeditionEnemyWarning_=std::move(warning);
    guardAudioCounts_=std::move(guardCounts);
}
