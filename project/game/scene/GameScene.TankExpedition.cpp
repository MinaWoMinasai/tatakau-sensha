#include "GameScene.h"
#include "game/run/TankRunCopy.h"
#include "game/run/TankExpeditionEncounters.h"
#include <fstream>
#include <sstream>

namespace {
using EPhase=tankexp::Phase;
using Room=tankexp::RoomKind;
using namespace tankrun::copy;
const char* RoomName(Room room) {
    switch(room) {
    case Room::Skirmish:return "外周警備";
    case Room::Resource:return "動力コア争奪";
    case Room::Elite:return "精鋭部隊";
    case Room::Reflection:return "反射実験区画";
    case Room::Drone:return "ドローン格納庫";
    case Room::Guard:return "制御装置の制圧";
    default:return "最深部 / 最終決戦";
    }
}
int LivingThreats(EnemyManager* manager) {
    int count=0;
    for(auto* actor:manager->GetEnemyPtrs()) if(actor&&actor->IsCombatThreat()) ++count;
    return count;
}
std::string ExpeditionDirectory(int variant) {return "generated/tank_expedition/variant_"+std::to_string(variant);}
}

void GameScene::InitializeTankExpedition() {
    player_->SetRunCheckpointEvolution(true);
    enemy_->SetRunEncounterEnabled(false);
    enemyManager_->ClearRunActors();
    bulletManager_->ClearAll();
    stage_->LoadRunMap("resources/maps/expedition_crossfire.csv");
    stagePostCacheValid_=false;
    player_->ResetRunRoomState({26,28,0});
    camera->SetTranslate({26,28,camera->GetTranslate().z}); camera->Update();
    TextStyle style{};style.fontFamily="Meiryo";style.fontSize=14;
    style.color={0.68f,0.86f,0.95f,1};style.outlineThickness=0;style.padding=4;
    tankExpeditionMapText_=std::make_unique<TextLabel>();
    tankExpeditionMapText_->Initialize(SpriteCommon::GetInstance()," ",style);
    tankExpeditionMapText_->SetPosition({24,670});
    style.fontSize=17;style.color={0.78f,0.96f,1,1};
    tankExpeditionMaintenanceText_=std::make_unique<TextLabel>();
    tankExpeditionMaintenanceText_->Initialize(SpriteCommon::GetInstance()," ",style);
    tankExpeditionMaintenanceText_->SetPosition({76,570});
    tankExpeditionMaintenanceButton_=std::make_unique<Sprite>();
    tankExpeditionMaintenanceButton_->Initialize(SpriteCommon::GetInstance(),"resources/white512x512.png");
    tankExpeditionMaintenanceButton_->SetPosition({64,568});
    tankExpeditionMaintenanceButton_->SetSize({1144,32});
    tankExpeditionMaintenanceButton_->SetColor({0.035f,0.105f,0.14f,1});
    tankExpeditionMaintenanceButton_->Update();
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
    // This function is called at the frame boundary or from a stopped menu.
    // Resource damage callbacks only mark completion; they never delete actors.
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
    stagePostCacheValid_=false;
    // Hazard walls are a positional cost, not an instant run-ending collision.
    stage_->SetDamageBlockDamage(12);
    player_->ResetRunRoomState({26,28,0});
    camera->SetTranslate({26,28,camera->GetTranslate().z});camera->Update();
    auto freePosition=[this](Vector3 desired) {
        if(!stage_->IsCollisionWithAnyBlock(desired,1.3f)) return desired;
        for(int r=2;r<=10;r+=2) for(int y=-r;y<=r;y+=2) for(int x=-r;x<=r;x+=2) {
            Vector3 p=desired+Vector3{static_cast<float>(x),static_cast<float>(y),0};
            if(p.x>23&&p.x<65&&p.y>17&&p.y<41&&!stage_->IsCollisionWithAnyBlock(p,1.3f)) return p;
        }
        return Vector3{44,30,0};
    };
    const std::array<Vector3,6> shapes={Vector3{34,22,0},Vector3{44,22,0},Vector3{54,22,0},Vector3{34,36,0},Vector3{44,36,0},Vector3{54,36,0}};
    const auto encounter=tankexp::GetEncounter(room);
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
        const std::array<Vector3,3> centers={Vector3{44,30,0},Vector3{36,38,0},Vector3{54,20,0}};
        for(int i=0;i<count;++i) {
            auto& node=tankRunResources_[i];node.position=freePosition(centers[i]);
            node.active=enemyManager_->SpawnRunResource(node.position,60,
                [this,i](bool owned){OnTankRunResourceClaim(i,owned);});
        }
    }
    if(room==Room::Resource||room==Room::Boss) {
        tankExpeditionRivalActive_=true;
        enemy_->ResetRunEncounter({62,30,0},room==Room::Boss?1050:550,room==Room::Boss?1:0,true);
        auto progress=enemy_->GetEnemyProgressConfig();
        progress.levelingModeEnabled=room==Room::Resource;
        enemy_->SetEnemyProgressConfig(progress);
        if(room==Room::Boss) screenEffectDirector_.TriggerBossEntry();
    }
    previousPlayerHp_=player_->GetHp();previousBossHp_=enemy_->GetHp();
    bossDefeatHandled_=false;
    tankRunSelection_=0;tankRunMenuAge_=-0.15f;
    SetEventCallout(std::string("第")+std::to_string(tankExpedition_.GetRoomIndex()+1)+"区画 / "+RoomName(room),1.6f);
    RefreshTankExpeditionUi();
}

void GameScene::FinishTankExpeditionRoom() {
    if(player_->IsDead()||!tankExpedition_.IsCombat()||tankExpedition_.GetRoomKind()==Room::Boss) return;
    if(!tankExpedition_.CompleteRoom()) return;
    player_->AwardRunMaintenancePoint(tankExpedition_.GetRoomIndex()+1);
    tankExpeditionMaintenanceOpen_=false;
    tankExpeditionRoomPending_=false;
    tankExpeditionRewardOpen_=tankRun_.OpenRewardDraft(tankExpedition_.GetRewardRare()||tankExpeditionResourceWon_,tankExpedition_.GetRewardAffinity());
    tankRunSelection_=0;tankRunMenuAge_=0;
    tankExpeditionAudio_.Upgrade();
    SetEventCallout("区画突破 / 改造 ＋ 整備ポイント",1.2f);
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
            ApplyTankRunCards();if(card==tankrun::CardId::Repair) player_->HealRunPlayer(30);
            screenEffectDirector_.TriggerUpgradeConfirmed(WorldToScreenUv(player_->GetWorldPosition()));
            tankExpeditionAudio_.Upgrade();
        }
        tankExpeditionRewardOpen_=false;tankExpedition_.ChooseRewardDone();
    } else if(before==EPhase::Route) tankExpedition_.ChooseRoute(index);
    else if(before==EPhase::Event) {
        if(index==1&&!player_->SpendRunHealth(20)) {SetEventCallout("HPが21以上あると選べます",1.3f);return;}
        if(index==0) player_->HealRunPlayer(40);
        tankExpedition_.ChooseEvent(index);
        if(index==1) tankExpeditionRewardOpen_=tankRun_.OpenRewardDraft(true);
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
    UpdateTankExpeditionAudio(dt);
    if(phase_!=Phase::kMain) return;
    tankRunMenuAge_+=dt;tankRunAutoTime_+=dt;
    const auto triggered=[this](int key){return input_->IsTrigger(input_->GetKey()[key],input_->GetPreKey()[key]);};
    if(triggered(DIK_F10)) RequestTankRunCapture("manual");
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
    if(gameFlowState_==GameFlowState::Playing&&!player_->IsChangeMode()&&triggered(DIK_ESCAPE)) {
        tankRunPaused_=!tankRunPaused_;tankRunSelection_=0;tankRunMenuAge_=0;RefreshTankRunUi();
    }
    if(gameFlowState_!=GameFlowState::Playing) {
        if(tankRunAutoTest_&&gameFlowState_==GameFlowState::StageClear) {
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
                if(!tankRunPaused_&&phase==EPhase::Reward&&(triggered(DIK_TAB)||
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
            const int threats=LivingThreats(enemyManager_.get());
            if(room==Room::Resource&&!tankExpeditionResourceReleased_&&threats==0&&tankExpeditionSpawned_>0) {
                auto& node=tankRunResources_[0];
                node.active=enemyManager_->SpawnRunResource(node.position,90,[this](bool owned){OnTankRunResourceClaim(0,owned);});
                tankExpeditionResourceReleased_=node.active;
                SetEventCallout("護衛撃破 / 金色の動力コアが出現",1.4f);
                tankExpeditionAudio_.EnemyWarning();
            }
            if(room!=Room::Boss&&tankExpeditionSpawned_>0&&tankexp::IsRoomObjectiveComplete(
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
    std::ostringstream hud;hud<<"分岐遠征  "<<tankExpedition_.GetRoomIndex()+1<<" / 5  "<<RoomName(room)<<"\n"
        <<"HP "<<player_->GetHp()<<" / "<<player_->GetMaxHp()<<"    "<<player_->GetCurrentClassName()<<"    改造 "<<tankRun_.GetDraftCount()<<"個";
    tankRunHud_->SetText(hud.str());
    const bool inCombat=phase==EPhase::Combat&&!tankRunPaused_;
    const int threats=LivingThreats(enemyManager_.get());
    const std::string objective=room==Room::Resource?(tankExpeditionResourceReleased_?"金色のコアを先に確保 / レア候補":"護衛を撃破してコアを解放 / 残り "+std::to_string(threats)):
        room==Room::Guard?"装置 "+std::to_string(tankExpeditionNodes_)+" / 3   護衛 残り "+std::to_string(threats):
        room==Room::Boss?"最深部のライバルを撃破せよ":"攻撃部隊を撃破 / 残り "+std::to_string(threats)+"  図形の破壊は任意";
    std::string nextTarget="突破後に改造を1つ選択";
    float nearest=10000;Vector3 target{};bool targetFound=false;
    for(auto* actor:enemyManager_->GetEnemyPtrs()) if(actor&&!actor->IsDead()) {
        if(!actor->IsCombatThreat()&&!actor->IsRunResource()) continue;
        const Vector3 delta=actor->GetWorldPosition()-player_->GetWorldPosition();
        if(Length(delta)<nearest) {nearest=Length(delta);target=delta;targetFound=true;}
    }
    if(inCombat&&targetFound) {
        std::string direction=target.y>2?"北":target.y<-2?"南":"";
        direction+=target.x>2?"東":target.x<-2?"西":"";
        nextTarget="近くの目標: "+(direction.empty()?std::string("付近"):direction)+" / 距離 "+std::to_string(static_cast<int>(nearest));
    }
    if(inCombat) tankRunBossText_->SetText(tankExpeditionRivalActive_?std::string(room==Room::Boss?"BOSS":"RIVAL")+"  HP "+std::to_string(enemy_->GetHp())+" / "+std::to_string(enemy_->GetMaxHp())+"\n"+objective:objective+"\n"+nextTarget);
    else tankRunBossText_->SetText(terminal?"今回の構成と進路を振り返ろう\n次は別の組み合わせにも挑戦":"区画突破 / 戦闘は停止中\nHP・主軸・改造を次の区画へ");
    const float hp=static_cast<float>(enemy_->GetHp())/(std::max)(1,enemy_->GetMaxHp());
    tankRunBossFill_->SetSize({488*(std::clamp)(hp,0.0f,1.0f),5});tankRunBossFill_->Update();
    std::string build="主軸: "+std::string(kCores[static_cast<size_t>(tankRun_.GetCore())].title);
    int mods=0;for(size_t i=0;i<tankrun::CardCount;++i) if(tankRun_.GetCardCounts()[i]) {build+=(mods==2?"\n":" / ")+std::string(kRunCards[i].title);++mods;}
    tankRunBuildText_->SetText(build);
    tankRunObjectiveText_->SetPosition({24,160});
    tankRunObjectiveText_->SetText(std::string(tankexp::GetEncounter(room).hint)+"\nWASD 移動 / 左 射撃 / 右 ダッシュ / Esc 停止");
    std::string route="外周 → ";
    route+=tankExpedition_.GetRouteChoice(0)<0?"[資源 / 精鋭]":tankExpedition_.GetRouteChoice(0)==0?"資源":"精鋭";
    route+=" → イベント・進化 → ";
    route+=tankExpedition_.GetRouteChoice(1)<0?"[反射 / ドローン]":tankExpedition_.GetRouteChoice(1)==0?"反射":"ドローン";
    route+=" → 制圧 → ボス";tankExpeditionMapText_->SetText(route);
    tankExpeditionMapText_->SetPosition({24,inCombat?625.0f:670.0f});
    auto card=[this](int index,const std::string& title,const std::string& body) {tankRunCardTitles_[index]->SetText(std::to_string(index+1)+"  "+title);tankRunCardBodies_[index]->SetText(body);};
    if(terminal) {
        tankRunHeading_->SetText(phase==EPhase::Clear?"最深部突破 / 遠征成功":"戦車が大破 / 遠征終了");
        tankRunDescription_->SetText("到達区画 "+std::to_string(tankExpedition_.GetRoomIndex()+1)+" / 5    撃破 "+std::to_string(defeatedEnemies_)+"    最長 "+std::to_string(tankRunBestCombo_)+" CHAIN\n改造 "+std::to_string(tankRun_.GetDraftCount())+"個 / 次は違う機体・主軸・進路も試してみよう。");
        card(0,"もう一度挑戦","機体選択から再スタート。\n\n新しい主軸や改造を試す。");card(1,"タイトルへ","今回の遠征を終える。\n\nタイトルのF10で\nコア争奪アリーナも遊べます。");
    } else if(tankRunPaused_) {
        tankRunHeading_->SetText("一時停止");tankRunDescription_->SetText("敵・弾・戦闘時間は停止中です。\nM: BGM "+std::string(tankExpeditionMusicEnabled_?"ON":"OFF")+" / N: SE "+std::string(tankExpeditionEffectsEnabled_?"ON":"OFF")+"（押すと切り替え）");
        card(0,"続ける","現在の区画に戻ります。");card(1,"タイトルへ","今回の改造と進行状況は\nリセットされます。");
    } else if(phase==EPhase::Reward&&tankExpeditionMaintenanceOpen_) {
        tankRunHeading_->SetText("機体整備 / 弱点を補う");
        tankRunDescription_->SetText("残り "+std::to_string(player_->GetRunMaintenancePoints())+" ポイント / 区画突破ごとに +1、各項目は3段階まで。\n報酬画面で振り直し可能。装甲は被ダメージを軽減し、HPは回復しません。");
        const auto choices=player_->GetRunMaintenanceChoices();
        for(int i=0;i<3;++i) card(i,choices[i].name+"  "+std::to_string(choices[i].rank)+" / 3",choices[i].description+
            (choices[i].rank>=3?"\n\n強化上限":choices[i].canSpend?"\n\n選択で1ポイント使用":"\n\nポイントを持ち越して整備可能"));
    } else if(phase==EPhase::Reward) {
        tankRunHeading_->SetText("区画報酬 / 改造を1つ選ぶ");
        if(tankExpedition_.GetEventChoice()==1&&tankExpedition_.GetRoomIndex()==1) tankRunHeading_->SetText("禁制の改造 / レア候補を獲得");
        tankRunDescription_->SetText("機体を進化させても主軸・改造・整備は引き継ぎます。\nTab または下の整備ボタンで能力を調整。改造を選ぶと先へ進みます。");
        if(tankExpeditionRewardOpen_) for(size_t i=0;i<tankRun_.GetOfferCount();++i) {
            const auto id=tankRun_.GetOffers()[i];const auto& copy=kRunCards[static_cast<size_t>(id)];
            const std::string title=copy.title;card(static_cast<int>(i),tankrun::IsRare(id)?"RARE\n"+title.substr(6):title,copy.body);
        } else card(0,"次へ","改造の取得上限に達しました。\n現在の構成で進みます。");
    } else if(phase==EPhase::Route) {
        tankRunHeading_->SetText("次の区画を選ぶ");
        tankRunDescription_->SetText("敵の配置・地形と、欲しい報酬から進路を選ぼう。\nHPと改造は持ち越し。移動時にスタミナが全回復します。");
        if(tankExpedition_.GetRouteRound()==0) {
            card(0,"動力コア争奪","護衛を倒すとコアが出現。\nライバルより先に確保しよう。\n\n報酬: 改造1つ・整備 +1\n確保なら HP +8・レア候補。");
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
    tankRunFooter_->SetText("数字キー / クリック: 決定    ← → + Enter: 選択    Esc: 一時停止\n選択後は次の区画へ。手に入れた改造を試しながら最深部を目指そう。");
    if(phase==EPhase::Reward&&!tankRunPaused_) {
        tankExpeditionMaintenanceText_->SetText(tankExpeditionMaintenanceOpen_?
            "Tab / クリック: 改造の選択へ戻る":"Tab / クリック: 機体整備   残り "+std::to_string(player_->GetRunMaintenancePoints())+" ポイント（未使用分は持ち越し）");
        if(tankExpeditionMaintenanceOpen_) tankRunFooter_->SetText("数字キー / 左クリック: 強化    Shift + 数字 / 右クリック: 1段階戻す\n整備が済んだら Tab で改造の選択へ戻ります。選択中は戦闘が停止します。");
    }
}

void GameScene::DrawTankExpeditionUi() {
    if(player_->IsChangeMode()) return;
    SpriteCommon::GetInstance()->PreDraw(kNormal);
    const auto phase=tankExpedition_.GetPhase();
    const bool result=gameFlowState_==GameFlowState::StageClear||(gameFlowState_==GameFlowState::GameOver&&gameFlowTimer_<=0);
    const bool decision=tankRunPaused_||phase==EPhase::Reward||phase==EPhase::Route||phase==EPhase::Event||phase==EPhase::Evolution;
    if(decision||result) tankRunDimmer_->Draw();
    tankRunHudPanel_->Draw();tankRunHud_->Draw();tankRunBossText_->Draw();
    if(tankExpeditionRivalActive_&&!decision&&!result) {tankRunBossTrack_->Draw();tankRunBossFill_->Draw();}
    tankRunBuildText_->Draw();tankExpeditionMapText_->Draw();
    if(!decision&&!result) {tankRunObjectiveText_->Draw();return;}
    tankRunHeading_->Draw();tankRunDescription_->Draw();
    const int count=result?2:GetTankExpeditionOptionCount();
    for(int i=0;i<count;++i) {
        const bool selected=i==(result?resultSelection_:tankRunSelection_);
        const bool rare=phase==EPhase::Reward&&!tankRunPaused_&&!tankExpeditionMaintenanceOpen_&&tankExpeditionRewardOpen_&&tankrun::IsRare(tankRun_.GetOffers()[i]);
        tankRunCards_[i]->SetColor(selected?(rare?Vector4{0.21f,0.12f,0.045f,1}:Vector4{0.045f,0.16f,0.19f,1}):Vector4{0.028f,0.045f,0.075f,1});
        tankRunCards_[i]->Update();tankRunCards_[i]->Draw();tankRunCardTitles_[i]->Draw();tankRunCardBodies_[i]->Draw();
    }
    if(phase==EPhase::Reward&&!tankRunPaused_) {tankExpeditionMaintenanceButton_->Draw();tankExpeditionMaintenanceText_->Draw();}
    tankRunFooter_->Draw();
}

void GameScene::UpdateTankExpeditionAudio(float dt) {
    const bool combat=tankExpedition_.IsCombat()&&gameFlowState_==GameFlowState::Playing;
    tankExpeditionAudio_.SetCombat(combat);
    tankExpeditionAudio_.SetBoss(tankExpedition_.GetRoomKind()==Room::Boss);
    tankExpeditionAudio_.SetDucked(tankRunPaused_||phase_==Phase::kFadeOut);
    tankExpeditionAudio_.Update(dt);
    if(!combat||tankRunPaused_) return;
    std::unordered_map<const ExpEnemy*,int> hp;
    std::unordered_map<const ExpEnemy*,bool> warning;
    for(auto* actor:enemyManager_->GetEnemyPtrs()) if(actor&&!actor->IsDead()) {
        const auto previous=tankExpeditionEnemyHp_.find(actor);
        if(previous!=tankExpeditionEnemyHp_.end()&&actor->GetHp()<previous->second) tankExpeditionAudio_.Hit();
        hp[actor]=actor->GetHp();
        const bool locked=actor->IsExpeditionCombatRole()&&actor->IsAttackAimLocked();
        const auto old=tankExpeditionEnemyWarning_.find(actor);
        if(locked&&(old==tankExpeditionEnemyWarning_.end()||!old->second)) tankExpeditionAudio_.EnemyWarning();
        warning[actor]=locked;
    }
    tankExpeditionEnemyHp_=std::move(hp);tankExpeditionEnemyWarning_=std::move(warning);
}
