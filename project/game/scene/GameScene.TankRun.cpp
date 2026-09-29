#include "GameScene.h"
#include "game/run/TankRunCopy.h"
#include "game/player/TankRunModifiers.h"
#include "externals/DirectXTex/DirectXTex.h"
#include "ParticleManager.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {
using RunPhase = tankrun::Phase;
using namespace tankrun::copy;
bool IsDecision(RunPhase phase) { return phase==RunPhase::Loadout || phase==RunPhase::CoreChoice || phase==RunPhase::Draft; }
std::string RunClock(double time) {
    const int seconds=static_cast<int>((std::max)(0.0,time));
    std::ostringstream text; text<<seconds/60<<':'<<std::setfill('0')<<std::setw(2)<<seconds%60; return text.str();
}
std::string DirectionTo(const Vector3& delta) {
    std::string direction=delta.y>3?"北":delta.y<-3?"南":"";
    direction+=delta.x>3?"東":delta.x<-3?"西":"";
    return direction.empty()?"付近":direction;
}
}

bool GameScene::IsTankRunMenuOpen() const {
    if(expeditionAuthoringHubOpen_) return true;
    if(IsGuidedExpeditionPaused()||expeditionCollectAll_) return true;
    if(expeditionMapEnabled_ && expeditionTransition_.IsActive()) return true;
    if(expeditionMapEnabled_ && (expeditionMapPreview_ || expeditionRoomEditorOpen_ || expeditionMapEditorOpen_ || expeditionContentEditorOpen_)) return true;
    if(expeditionRun_ && (tankExpeditionDetailsOpen_ || tankExpeditionBalanceEditorOpen_)) return true;
    if(expeditionRun_ && tankExpedition_.GetPhase()!=tankexp::Phase::Dormant && !tankExpedition_.IsCombat()) return true;
    return prototypeRun_ && (tankRunPaused_ || IsDecision(tankRun_.GetPhase()) || tankRunMenuAge_<0);
}

void GameScene::InitializeTankRun() {
    wchar_t automatic[16]{};
    tankRunAutoTest_=!titleDemo_ && GetEnvironmentVariableW(L"CG2_TANK_AUTOTEST",automatic,16)>0 && automatic[0]==L'1';
    tankrun::Config config; if(tankRunAutoTest_) config.combatSeconds=24;
    if(expeditionRun_) config.combatSeconds=1000000;
    tankRun_=tankrun::RunDirector(tankRunAutoTest_?20260919u:static_cast<uint32_t>(GetTickCount64()),config);
    TankRunModifiers modifiers{}; modifiers.enabled=true; player_->SetRunModifiers(modifiers);
    enemy_->SetPrototypeMaxHp(1050); enemy_->EnablePrototypeCombat(true);
    enemy_->SetPrototypePressure(0); enemy_->SetPrototypeResourceFocus(true);
    debugPlayerNoDamage_=tankRunAutoTest_;
    showPostProfileOverlay_=false; showLevelAIDitorPreview_=false; showGameDebugConsole_=false;
    InitializeTankRunVisuals();
    if(tankRunAutoTest_ && !expeditionRun_) {
        std::filesystem::create_directories("generated/tank_run");
        std::ofstream("generated/tank_run/validation.json")<<"{\"completed\":false,\"testMode\":true}\n";
    }
    auto panel=[](Vector2 position,Vector2 size,Vector4 color) {
        auto item=std::make_unique<Sprite>(); item->Initialize(SpriteCommon::GetInstance(),"resources/white512x512.png");
        item->SetPosition(position); item->SetSize(size); item->SetColor(color); item->Update(); return item;
    };
    auto label=[](float size,Vector2 position,Vector4 color) {
        TextStyle style{}; style.fontFamily="Meiryo"; style.fontSize=size; style.color=color; style.outlineThickness=0; style.padding=4;
        auto item=std::make_unique<TextLabel>(); item->Initialize(SpriteCommon::GetInstance()," ",style); item->SetPosition(position); return item;
    };
    tankRunDimmer_=panel({0,0},{1280,720},{0.007f,0.012f,0.025f,0.97f});
    tankRunHudPanel_=panel({0,0},{1280,100},{0.009f,0.016f,0.03f,0.93f});
    tankRunBossTrack_=panel({756,74},{488,5},{0.12f,0.17f,0.22f,1});
    tankRunBossFill_=panel({756,74},{488,5},{1,0.29f,0.34f,1});
    tankRunHud_=label(21,{24,12},{0.88f,0.96f,1,1});
    tankRunBossText_=label(17,{752,14},{1,0.77f,0.75f,1});
    tankRunBuildText_=label(14,{24,103},{0.80f,0.89f,0.98f,1});
    tankRunObjectiveText_=label(17,{24,128},{1,0.84f,0.36f,1});
    tankRunHeading_=label(36,{64,151},{0.80f,1,0.96f,1});
    tankRunDescription_=label(18,{64,207},{0.75f,0.84f,0.93f,1});
    tankRunFooter_=label(17,{64,604},{0.77f,0.87f,0.94f,1});
    for(size_t i=0;i<3;++i) {
        const float x=64+static_cast<float>(i)*388;
        tankRunCards_[i]=panel({x,280},{368,280},{0.028f,0.045f,0.075f,1});
        tankRunCardTitles_[i]=label(20,{x+16,303},{0.82f,1,0.94f,1});
        tankRunCardBodies_[i]=label(18,{x+16,362},{0.84f,0.90f,0.96f,1});
    }
    if(expeditionRun_) { InitializeTankExpedition(); RefreshTankRunUi(); return; }
    const std::array<Vector3,3> centers={Vector3{25,18,0},Vector3{43,38,0},Vector3{65,20,0}};
    for(size_t i=0;i<centers.size();++i) {
        auto& resource=tankRunResources_[i]; resource.position=centers[i];
        bool found=false;
        for(int radius=0;radius<=12&&!found;radius+=2) for(int n=0;n<16&&!found;++n) {
            const float angle=static_cast<float>(n)*0.392699f;
            const Vector3 p=centers[i]+Vector3{std::cos(angle)*radius,std::sin(angle)*radius,0};
            if(p.x>5&&p.x<81&&p.y>5&&p.y<51&&!stage_->IsCollisionWithAnyBlock(p,2.3f)) { resource.position=p; found=true; }
        }
        for(int n=0;n<10;++n) {
            const float angle=static_cast<float>(n)*0.628319f;
            const Vector3 p=resource.position+Vector3{std::cos(angle)*6,std::sin(angle)*6,0};
            if(!stage_->IsCollisionWithAnyBlock(p,1.1f)&&Length(p-player_->GetWorldPosition())>4)
                enemyManager_->SpawnLevelEnemy(p,n%3==0?"Triangle":"Square",8);
        }
        EnemyManager::SpawnArea area{}; area.name="Core salvage "+std::to_string(i);
        area.prefab="Square"; area.center=resource.position; area.size={18,18,1};
        area.spawnInterval=2; area.maxAlive=10; area.hp=8; enemyManager_->AddLevelSpawnArea(area);
    }
    RefreshTankRunUi();
}

std::array<float,tankrun::CardCount> GameScene::ExpeditionEffectPowers() const {
    std::array<float,tankrun::CardCount> power{};power.fill(1);
    for(const auto& [id,purchased]:expeditionPurchasedModules_) {
        const auto* live=tankcontent::FindUpgrade(expeditionContent_,id);
        for(const auto effect:purchased.effects) {
            const auto index=static_cast<std::size_t>(effect);
            if(index<power.size())power[index]=live?live->effectPower[index]:purchased.effectPower[index];
        }
    }
    return power;
}

void GameScene::ApplyTankRunCards() {
    TankRunModifiers m{}; m.enabled=true; m.expedition=expeditionRun_;
    if(!expeditionMapEnabled_ && tankRun_.GetCore()!=tankrun::CoreId::Count && (!expeditionRun_ || tankRun_.GetDraftCount()>=2))
        m.core=static_cast<TankRunCore>(static_cast<int>(tankRun_.GetCore())+1);
    const auto& c=tankRun_.GetCardCounts();
    m.ricochet=c[0]>0; m.heavy=c[1]>0; m.rapid=c[2]>0; m.thrusters=c[3]>0;
    m.capacitor=c[4]>0; m.repair=c[5]>0; m.drones=c[6]>0; m.pierce=c[7]>0;
    m.scatterShot=c[8]>0; m.homing=c[9]>0; m.dashBurst=c[10]>0; m.overdrive=c[11]>0;
    m.meleeBlade=c[12]>0;m.bladeReach=c[13]>0;m.impactDrive=c[14]>0;m.perfectDodge=c[15]>0;
    m.droneFocus=c[16]>0;m.droneGuard=c[17]>0;m.meleeTempo=c[18]>0;m.finisherCharge=c[19]>0;
    m.railCannon=c[20]>0;m.droneLaserLink=c[21]>0;m.slashWave=c[22]>0;m.parryBlade=c[23]>0;
    m.extraBarrel1=c[24]>0;m.extraBarrel2=c[25]>0;m.fanMount=c[26]>0;m.alternatingFire=c[27]>0;
    m.heavyDroneCore=c[28]>0;m.lightBladeActuator=c[29]>0;m.heavyBladeEdge=c[30]>0;
    m.chainLightning=c[31]>0;m.markDetonation=c[32]>0;m.boomerangShell=c[33]>0;m.killBurst=c[34]>0;
    m.droneCharge=c[35]>0;m.droneRebuildBomb=c[36]>0;m.targetPainter=c[37]>0;m.autonomousSpread=c[38]>0;
    m.dashSlash=c[39]>0;m.spinBlade=c[40]>0;m.wallSmash=c[41]>0;
    if(expeditionMapEnabled_) m.effectPower=ExpeditionEffectPowers();
    player_->SetRunModifiers(m);
}

void GameScene::OnTankRunEnemyDefeated(const Vector3& position) {
    if(!tankRun_.IsCombat()) return;
    if(expeditionMapEnabled_) SpawnExpeditionCredits(position,player_->TakeRunCurrencyEarned());
    tankRun_.AddSalvage(1); tankRunCombo_=(tankRunComboTime_>0?tankRunCombo_:0)+1;
    tankRunComboTime_=2; tankRunBestCombo_=(std::max)(tankRunBestCombo_,tankRunCombo_);
    if(tankRunBursts_.size()<24) tankRunBursts_.push_back({position,0,false});
    screenEffectDirector_.TriggerEnemyDefeat(WorldToScreenUv(position),0.20f);
    if(expeditionRun_) {
        tankExpeditionAudio_.Kill(tankRunCombo_);
        ParticleManager::GetInstance()->EmitNeonImpactEffect(position,{0,1,0},{0.22f,1.1f,0.82f,1},10);
        if(expeditionMapEnabled_&&cameraShakeTimer_<=0) {cameraShakeDuration_=0.08f;cameraShakeTimer_=0.08f;cameraShakePower_=0.055f;}
    }
    if(tankRunCombo_%5==0) SetEventCallout(std::to_string(tankRunCombo_)+(expeditionRun_?"連続撃破":" CHAIN / 資材 +1"),0.55f);
    tankRunHudTimer_=0;
}

void GameScene::OnTankRunResourceClaim(size_t index,bool playerOwned) {
    if(index>=tankRunResources_.size()||!tankRunResources_[index].active) return;
    auto& resource=tankRunResources_[index]; resource.active=false; resource.respawn=20;
    if(expeditionRun_) {
        ++tankExpeditionNodes_;
        tankExpeditionResourceWon_|=playerOwned;
        if(playerOwned) {player_->AddExp(30);player_->HealRunPlayer(8);if(expeditionMapEnabled_) {SpawnExpeditionCredits(resource.position,player_->TakeRunCurrencyEarned());tankExpeditionTutorial_.RecordKill();}}
        if(tankExpedition_.GetRoomKind()==tankexp::RoomKind::Resource || tankExpeditionNodes_>=3) tankExpeditionRoomPending_=true;
        SetEventCallout(playerOwned?"通貨を回収！":"相手が通貨を回収 / 次の戦闘へ",1.0f);
        tankExpeditionAudio_.Kill();
        if(tankRunBursts_.size()<24) tankRunBursts_.push_back({resource.position,0,true});
        return;
    }
    if(!tankRun_.ClaimResource(playerOwned)) return;
    if(playerOwned) {
        player_->AddExp(30); player_->HealRunPlayer(12);
        screenEffectDirector_.TriggerUpgradeConfirmed(WorldToScreenUv(resource.position));
        SetEventCallout("コア確保 / 資材 +12・HP +12 / 次の改造にレア候補",1.6f);
    } else { enemy_->RegisterRunResourceClaim(); SetEventCallout("ライバルがコアを確保",1.2f); }
    if(tankRunBursts_.size()<24) tankRunBursts_.push_back({resource.position,0,true});
    ParticleManager::GetInstance()->EmitNeonImpactEffect(resource.position,{0,1,0},{1,0.72f,0.15f,1},18);
    tankRunHudTimer_=0;
}

void GameScene::UpdateTankRunResources(float dt) {
    tankRunComboTime_=(std::max)(0.0f,tankRunComboTime_-dt);
    for(auto& burst:tankRunBursts_) burst.age+=dt;
    std::erase_if(tankRunBursts_,[](const RunBurst& b){return b.age>(b.resource?0.7f:0.35f);});
    for(size_t i=0;i<tankRunResources_.size();++i) {
        auto& resource=tankRunResources_[i]; if(resource.active) continue;
        resource.respawn-=dt;
        if(resource.respawn<=0) {
            resource.active=true;
            if(!enemyManager_->SpawnRunResource(resource.position,42,[this,i](bool owned){OnTankRunResourceClaim(i,owned);})) {
                resource.active=false; resource.respawn=1;
            }
        }
    }
    if(tankRun_.GetPhase()==RunPhase::Boss&&!tankRunFinalStarted_) {
        tankRunFinalStarted_=true; auto progress=enemy_->GetEnemyProgressConfig();
        progress.levelingModeEnabled=false; enemy_->SetEnemyProgressConfig(progress);
        enemy_->SetPrototypePressure(1);
        screenEffectDirector_.TriggerBossEntry(); SetEventCallout("最終決戦 / ライバルを撃破せよ",1.5f);
    }
}

void GameScene::SelectTankRunOption(int index) {
    if(expeditionRun_ && !tankRunPaused_ && tankExpedition_.GetPhase()!=tankexp::Phase::Dormant) {SelectTankExpeditionOption(index);return;}
    const auto phase=tankRun_.GetPhase();
    if(tankRunPaused_) {
        if(expeditionMapEnabled_) {tankExpeditionAudio_.UiConfirm();expeditionLastFocus_.clear();}
        if(index==0) tankRunPaused_=false;
        else { nextSceneName_="TITLE"; fade_->Start(Fade::Status::FadeOut,0.4f); phase_=Phase::kFadeOut; }
    } else if(phase==RunPhase::Loadout) {
        if(!tankRun_.ChooseLoadout(index)) return;
        player_->ConfigurePrototypeLoadout(index);
    } else if(phase==RunPhase::CoreChoice) {
        if(!tankRun_.ChooseCore(index)) return;
        ApplyTankRunCards();
        if(expeditionRun_) {tankExpedition_.Start();StartTankExpeditionRoom();}
        else {UpdateTankRunResources(0);SetEventCallout("金色のコアを狙おう / 資材で E 改造",1.5f);}
    } else if(phase==RunPhase::Draft) {
        if(index<0||static_cast<size_t>(index)>=tankRun_.GetOfferCount()) return;
        const auto chosen=tankRun_.GetOffers()[static_cast<size_t>(index)];
        if(!tankRun_.ChooseCard(static_cast<size_t>(index))) return;
        ApplyTankRunCards(); if(chosen==tankrun::CardId::Repair) player_->HealRunPlayer(30);
        screenEffectDirector_.TriggerUpgradeConfirmed(WorldToScreenUv(player_->GetWorldPosition()));
        SetEventCallout(std::string("改造: ")+kRunCards[static_cast<size_t>(chosen)].title,1.2f);
    }
    tankRunSelection_=0; tankRunMenuAge_=-0.04f; RefreshTankRunUi();
}

void GameScene::UpdateTankRun(float dt) {
    if(expeditionRun_) {UpdateTankExpedition(dt);return;}
    if(phase_!=Phase::kMain) return;
    tankRunMenuAge_+=dt; tankRunAutoTime_+=dt;
    const auto triggered=[this](int key){return input_->IsTrigger(input_->GetKey()[key],input_->GetPreKey()[key]);};
    if(cg2::kDeveloperTools&&triggered(DIK_F10)) RequestTankRunCapture("manual");
    if(gameFlowState_==GameFlowState::Playing&&!player_->IsChangeMode()&&triggered(DIK_ESCAPE)) {
        tankRunPaused_=!tankRunPaused_; tankRunSelection_=0; tankRunMenuAge_=0; RefreshTankRunUi();
    }
    if(gameFlowState_!=GameFlowState::Playing) {
        tankRunHudTimer_-=dt;
        if(tankRunHudTimer_<=0) {RefreshTankRunUi();tankRunHudTimer_=0.15f;}
        if(tankRunAutoTest_&&gameFlowState_==GameFlowState::StageClear) {
            if(tankRunAutoStep_<10) {RequestTankRunCapture("result");tankRunAutoStep_=10;tankRunAutoTime_=0;}
            if(tankRunAutoTime_>1) {
                std::ofstream log("generated/tank_run/validation.json");
                log<<"{\"completed\":true,\"testMode\":true,\"core\":"<<static_cast<int>(tankRun_.GetCore())
                    <<",\"playerClaims\":"<<tankRun_.GetPlayerClaims()<<",\"rivalClaims\":"<<tankRun_.GetRivalClaims()
                    <<",\"drafts\":"<<tankRun_.GetDraftCount()<<",\"playerHp\":"<<player_->GetHp()<<",\"cards\":[";
                for(size_t i=0;i<tankrun::CardCount;++i) log<<(i?",":"")<<tankRun_.GetCardCounts()[i];
                log<<"]}\n"; log.close(); PostQuitMessage(0);
            }
        }
        return;
    }
    if(!IsTankRunMenuOpen()&&!player_->IsChangeMode()&&triggered(DIK_E)&&tankRun_.TryOpenDraft()) {
        tankRunSelection_=0;tankRunMenuAge_=0;RefreshTankRunUi();
    }
    if(IsDecision(tankRun_.GetPhase())||tankRunPaused_) {
        const int count=tankRunPaused_?2:tankRun_.GetPhase()==RunPhase::Draft?static_cast<int>(tankRun_.GetOfferCount()):3;
        if(tankRunMenuAge_>0.16f) {
            if(triggered(DIK_LEFT)||triggered(DIK_A)) tankRunSelection_=(tankRunSelection_+count-1)%count;
            if(triggered(DIK_RIGHT)||triggered(DIK_D)) tankRunSelection_=(tankRunSelection_+1)%count;
            const auto mouse=input_->GetMousePosition(); const auto motion=input_->GetMouseState(); int hovered=-1;
            for(int i=0;i<count;++i) {
                const float x=64+static_cast<float>(i)*388;
                if(mouse.x>=x&&mouse.x<=x+368&&mouse.y>=280&&mouse.y<=560) hovered=i;
            }
            if(hovered>=0&&(motion.lX||motion.lY)) tankRunSelection_=hovered;
            bool confirm=triggered(DIK_RETURN)||triggered(DIK_SPACE);
            for(int i=0;i<count;++i) if(triggered(DIK_1+i)) {tankRunSelection_=i;confirm=true;}
            if(hovered>=0&&input_->IsTrigger(motion.rgbButtons[0],input_->GetPreMouseState().rgbButtons[0])) {tankRunSelection_=hovered;confirm=true;}
            if(confirm) SelectTankRunOption(tankRunSelection_);
        }
        if(tankRunAutoTest_&&tankRunMenuAge_>0.4f&&tankRunMenuAge_<0.4f+dt*1.5f) RequestTankRunCapture("choice_"+std::to_string(tankRunAutoMenuIndex_));
        if(tankRunAutoTest_&&tankRunMenuAge_>1.2f) {
            SelectTankRunOption(tankRun_.GetPhase()==RunPhase::CoreChoice?1:0); ++tankRunAutoMenuIndex_;
        }
    } else if(!player_->IsChangeMode()&&tankRunMenuAge_>=0) {
        tankRun_.Update(dt); UpdateTankRunResources(dt);
        // Exercise actual core damage/ownership callbacks, optional refits and
        // the ordinary victory path. This is a smoke test, not a balance test.
        if(tankRunAutoTest_) {
            const double elapsed=tankRun_.GetRunElapsedSeconds();
            if(tankRunAutoStep_==0&&elapsed>2) {RequestTankRunCapture("combat");tankRunAutoStep_=1;}
            if(tankRunAutoStep_==1&&elapsed>3) {
                if(auto* core=enemyManager_->FindNearestRunResource(tankRunResources_[0].position,3)) core->TakeDamageFromPlayer(10000);
                tankRunAutoStep_=2;
            }
            if(tankRunAutoStep_==2&&elapsed>4) {
                if(auto* core=enemyManager_->FindNearestRunResource(tankRunResources_[2].position,3)) core->TakeDamageFromEnemy(10000);
                tankRunAutoStep_=3;
            }
            if(tankRunAutoStep_==3&&elapsed>6) {
                if(auto* core=enemyManager_->FindNearestRunResource(tankRunResources_[1].position,3)) core->TakeDamageFromPlayer(10000);
                tankRun_.AddSalvage(30); tankRunAutoStep_=4;
            }
            if(tankRun_.CanOpenDraft()&&tankRun_.GetDraftCount()<3&&elapsed>4) {
                tankRun_.TryOpenDraft(); tankRunSelection_=0;tankRunMenuAge_=0;RefreshTankRunUi();
            }
            if(tankRunAutoStep_==4&&elapsed>9) {RequestTankRunCapture("build");tankRunAutoStep_=5;}
            if(tankRun_.GetPhase()==RunPhase::Boss&&tankRun_.GetBossElapsedSeconds()>5) enemy_->TakeDamage(static_cast<uint32_t>(enemy_->GetHp()+1));
        }
    }
    tankRunHudTimer_-=dt; if(tankRunHudTimer_<=0) {RefreshTankRunUi();tankRunHudTimer_=0.15f;}
}

void GameScene::RefreshTankRunUi() {
    if(expeditionRun_ && tankExpedition_.GetPhase()!=tankexp::Phase::Dormant) {RefreshTankExpeditionUi();return;}
    const auto phase=tankRun_.GetPhase(); std::ostringstream hud;
    hud<<"コア争奪戦  /  "<<(phase==RunPhase::Clear?"勝利":phase==RunPhase::Dead?"戦闘終了":phase==RunPhase::Boss?"最終決戦":"決戦まで "+RunClock(tankRun_.GetContestSecondsRemaining()))<<"\n";
    hud<<"HP "<<player_->GetHp()<<" / "<<player_->GetMaxHp()<<"    ";
    if(tankRun_.GetDraftCount()>=tankRun_.GetMaxDrafts()) hud<<"改造完了 "<<tankRun_.GetDraftCount()<<"個";
    else hud<<"資材 "<<tankRun_.GetSalvage()<<" / "<<tankRun_.GetRefitCost()<<(tankRun_.CanOpenDraft()?"  [E] 改造できる":"  [E] 改造");
    if(tankRunComboTime_>0&&tankRunCombo_>1) hud<<"   "<<tankRunCombo_<<" CHAIN";
    tankRunHud_->SetText(hud.str());
    std::ostringstream boss;
    boss<<"ライバル Lv."<<enemy_->GetLevel()<<"   HP "<<enemy_->GetHp()<<" / "<<enemy_->GetMaxHp()<<"\n"
        <<(enemy_->IsDead()?"撃破":enemy_->IsLevelingModeActive()?"資源を狙っている":"こちらを狙っている");
    if(!enemy_->IsDead()) boss<<" / "<<DirectionTo(enemy_->GetWorldPosition()-player_->GetWorldPosition());
    tankRunBossText_->SetText(boss.str());
    const float ratio=static_cast<float>(enemy_->GetHp())/static_cast<float>((std::max)(1,enemy_->GetMaxHp()));
    tankRunBossFill_->SetSize({488*(std::clamp)(ratio,0.0f,1.0f),5});tankRunBossFill_->Update();
    std::string build=tankRun_.GetCore()==tankrun::CoreId::Count?"主軸コア: 未選択":"主軸: "+std::string(kCores[static_cast<size_t>(tankRun_.GetCore())].title);
    for(size_t i=0;i<tankrun::CardCount;++i) if(tankRun_.GetCardCounts()[i]) build+=" / "+std::string(kRunCards[i].title);
    // Two lines keep a full six-mod build within the viewport.
    if(build.size()>130) { const auto split=build.find(" / ",110); if(split!=std::string::npos) build.replace(split,3,"\n"); }
    tankRunBuildText_->SetText(build);
    std::ostringstream objective; objective<<"金色のコア: ";
    for(size_t i=0;i<tankRunResources_.size();++i) {
        const auto& node=tankRunResources_[i]; objective<<static_cast<char>('A'+i)<<" ";
        if(node.active) objective<<DirectionTo(node.position-player_->GetWorldPosition())<<" "<<static_cast<int>(Length(node.position-player_->GetWorldPosition()));
        else objective<<"再出現 "<<static_cast<int>(std::ceil(node.respawn))<<"秒";
        objective<<"    ";
    }
    tankRunObjectiveText_->SetPosition({24,build.find('\n')==std::string::npos?128.0f:150.0f});
    tankRunObjectiveText_->SetText(objective.str());
    const CardCopy* copies=nullptr;
    if(phase==RunPhase::Clear||phase==RunPhase::Dead) {
        tankRunHeading_->SetText(phase==RunPhase::Clear?"ライバル撃破 / 勝利":"戦車が大破 / 戦闘終了");
        std::ostringstream result; result<<"戦闘時間 "<<RunClock(tankRun_.GetRunElapsedSeconds())<<"    撃破 "<<defeatedEnemies_<<"    最長 "<<tankRunBestCombo_<<" CHAIN\n"
            <<"コア確保  自分 "<<tankRun_.GetPlayerClaims()<<" / ライバル "<<tankRun_.GetRivalClaims()<<"    改造 "<<tankRun_.GetDraftCount()<<"個    ジャスト回避 "<<justDodgeCount_;
        tankRunDescription_->SetText(result.str());
        tankRunCardTitles_[0]->SetText("1  もう一度挑戦");tankRunCardBodies_[0]->SetText("別の主軸コアや改造を試す。\n\n機体選択から再スタート。");
        tankRunCardTitles_[1]->SetText("2  タイトルへ");tankRunCardBodies_[1]->SetText("今回の戦闘を終える。\n\n改造は毎回選び直せます。");
    } else if(tankRunPaused_) {
        tankRunHeading_->SetText("一時停止");tankRunDescription_->SetText("戦闘・資源の再出現・決戦タイマーは停止しています。");
        tankRunCardTitles_[0]->SetText("1  戦闘を続ける");tankRunCardBodies_[0]->SetText("現在の構成で戦闘へ戻る。");
        tankRunCardTitles_[1]->SetText("2  タイトルへ");tankRunCardBodies_[1]->SetText("今回の改造と進行状況は\nリセットされます。");
    } else if(phase==RunPhase::Loadout) {
        tankRunHeading_->SetText("今回の戦車を選ぶ");
        tankRunDescription_->SetText("図形を倒す → 資材で E 改造。金色のコアはレア候補・回復・EXPを獲得。\nライバルもコアを狙います。2分30秒後に決戦。途中撃破でも勝利。");copies=kLoadouts;
    } else if(phase==RunPhase::CoreChoice) {
        tankRunHeading_->SetText("最初から使う主軸コアを選ぶ");
        tankRunDescription_->SetText("選んだ軸に、戦闘中の改造を組み合わせる。進化後も効果を引き継ぎます。\n指揮機体の弾にも反射・誘導・分裂が適用されます。");copies=kCores;
    } else if(phase==RunPhase::Draft) {
        tankRunHeading_->SetText("資材で改造 / "+std::to_string(tankRun_.GetDraftCount()+1)+"個目");
        tankRunDescription_->SetText("主軸コアとの組み合わせを選ぶ。所持済みの改造は再登場しません。\n金色のコア確保後は、未所持のレア候補があれば1つ以上出現。");
        for(size_t i=0;i<tankRun_.GetOfferCount();++i) {
            const auto& card=kRunCards[static_cast<size_t>(tankRun_.GetOffers()[i])];
            const std::string title=card.title;
            tankRunCardTitles_[i]->SetText(std::to_string(i+1)+"  "+(tankrun::IsRare(tankRun_.GetOffers()[i])?"RARE\n"+title.substr(6):title));
            tankRunCardBodies_[i]->SetText(card.body);
        }
    }
    if(copies) for(int i=0;i<3;++i) {tankRunCardTitles_[i]->SetText(std::to_string(i+1)+"  "+copies[i].title);tankRunCardBodies_[i]->SetText(copies[i].body);}
    tankRunFooter_->SetText("数字キー / クリック: 決定   ← → + Enter: 選択   Esc: 一時停止\nWASD: 移動   マウス: 照準   左クリック: 射撃   右クリック: ダッシュ   E: 改造   C: 進化");
    if(expeditionRun_) {
        tankRunHud_->SetText("分岐遠征 / 地下施設を突破せよ\n単発の戦車から、改造・進化で自分だけのビルドへ");
        tankRunBossText_->SetText("全5戦闘エリア\n所持改造とHPを次の部屋へ引き継ぐ");
        if(!tankRunPaused_&&phase==RunPhase::Loadout) {
            tankRunHeading_->SetText("将来の進化系統を選ぶ");
            tankRunDescription_->SetText("どの系統も、砲身1本の標準戦車で出発します。\n2区画目の突破後、選んだ系統の2種類から機体を進化させます。");
            tankRunCardBodies_[0]->SetText("集中射撃の進化設計図。\n\n序盤は単発の標準戦車。\n中間地点で多砲身や\n跳弾機体へ進化できる。");
            tankRunCardBodies_[1]->SetText("連射と弾幕の進化設計図。\n\n序盤は単発の標準戦車。\n中間地点で高速連射や\n広角射撃へ進化できる。");
            tankRunCardBodies_[2]->SetText("群体と援護の進化設計図。\n\n序盤は単発の標準戦車。\n中間地点でドローンを\n指揮する機体へ進化できる。");
        } else if(!tankRunPaused_&&phase==RunPhase::CoreChoice) {
            tankRunHeading_->SetText("成長させる主軸コアを選ぶ");
            tankRunDescription_->SetText("主軸は改造を2つ取得すると起動します。出発時は通常弾だけのシンプルな戦車。\nまずは1区画目の改造で強くなり、進化と主軸を組み合わせよう。");
        }
        tankRunFooter_->SetText("数字 / クリック: 決定   ← → + Enter: 選択\n操作は出発後に順番に案内します。改造の詳細は戦闘中に TAB。");
    }
}

void GameScene::DrawTankRunUi() {
    if(expeditionRun_ && tankExpedition_.GetPhase()!=tankexp::Phase::Dormant) {DrawTankExpeditionUi();return;}
    if(player_->IsChangeMode()) return;
    SpriteCommon::GetInstance()->PreDraw(kNormal); const auto phase=tankRun_.GetPhase();
    const bool result=gameFlowState_==GameFlowState::StageClear||(gameFlowState_==GameFlowState::GameOver&&gameFlowTimer_<=0);
    const bool decision=IsDecision(phase)||tankRunPaused_;
    if(decision||result) tankRunDimmer_->Draw();
    tankRunHudPanel_->Draw();tankRunHud_->Draw();tankRunBossText_->Draw();
    if(!expeditionRun_) {tankRunBossTrack_->Draw();tankRunBossFill_->Draw();}
    tankRunBuildText_->Draw();
    if(!decision&&!result) {tankRunObjectiveText_->Draw();return;}
    tankRunHeading_->Draw();tankRunDescription_->Draw();
    const size_t options=result||tankRunPaused_?2:phase==RunPhase::Draft?tankRun_.GetOfferCount():3;
    for(size_t i=0;i<options;++i) {
        const bool selected=static_cast<int>(i)==(result?resultSelection_:tankRunSelection_);
        const bool rare=phase==RunPhase::Draft&&!tankRunPaused_&&tankrun::IsRare(tankRun_.GetOffers()[i]);
        tankRunCards_[i]->SetColor(selected?(rare?Vector4{0.21f,0.12f,0.045f,1}:Vector4{0.045f,0.16f,0.19f,1}):Vector4{0.028f,0.045f,0.075f,1});
        tankRunCards_[i]->Update();tankRunCards_[i]->Draw();tankRunCardTitles_[i]->Draw();tankRunCardBodies_[i]->Draw();
    }
    tankRunFooter_->Draw();
}

void GameScene::QueueTankRunTelegraph() {
    constexpr float pi=3.14159265359f;
    auto circle=[&](Vector3 origin,float radius,float width,Vector4 tint,int sides) {
        origin.z=-0.3f;
        for(int n=0;n<sides;++n) {
            const float a=2*pi*static_cast<float>(n)/sides,b=2*pi*static_cast<float>(n+1)/sides;
            neonGridRenderer_->QueueLine(origin+Vector3{std::cos(a)*radius,std::sin(a)*radius,0},origin+Vector3{std::cos(b)*radius,std::sin(b)*radius,0},width,tint);
        }
    };
    for(size_t i=0;i<tankRunResources_.size();++i) {
        const auto& node=tankRunResources_[i]; if(!node.active) continue;
        const float pulse=0.5f+0.5f*std::sin(static_cast<float>(tankRun_.GetRunElapsedSeconds())*3);
        circle(node.position,1.9f+0.12f*pulse,0.08f,{1,0.69f,0.13f,0.85f},6);
        // One/two/three ticks identify A/B/C without a world-space text pass.
        for(size_t n=0;n<=i;++n) {
            const float x=static_cast<float>(n)*0.45f-static_cast<float>(i)*0.225f;
            neonGridRenderer_->QueueLine(node.position+Vector3{x,2.4f,-0.3f},node.position+Vector3{x,2.9f,-0.3f},0.10f,{1,0.85f,0.4f,1});
        }
    }
    for(const auto& burst:tankRunBursts_) {
        const float duration=burst.resource?0.7f:0.35f; const float t=burst.age/duration;
        circle(burst.position,0.7f+t*(burst.resource?5.0f:2.2f),0.09f*(1-t),burst.resource?Vector4{1,0.72f,0.15f,1-t}:Vector4{0.25f,1,0.8f,1-t},20);
        if(expeditionRun_&&!burst.resource) for(int i=0;i<8;++i) {
            const float a=static_cast<float>(i)*pi/4+burst.position.x;
            const Vector3 direction{std::cos(a),std::sin(a),0};
            const Vector3 start=burst.position+direction*(0.6f+3.8f*t)+Vector3{0,0,-0.35f};
            neonGridRenderer_->QueueLine(start,start+direction*(0.45f*(1-t)),0.075f*(1-t),{0.45f,1.2f,0.85f,1-t});
        }
    }
    for(const auto& hit:expeditionHitSparks_) {
        const float t=hit.age/0.18f;
        const Vector3 center=hit.position+Vector3{0,0,-0.35f};
        for(int i=0;i<5;++i) {
            const float a=std::atan2(hit.direction.y,hit.direction.x)+(i-2)*0.55f;
            const Vector3 ray{std::cos(a),std::sin(a),0};
            const auto start=center+ray*(0.50f+t*1.5f);
            neonGridRenderer_->QueueLine(start,start+ray*(0.40f*(1-t)),0.08f*(1-t),{1.9f,1.4f,0.65f,1-t});
        }
    }
    if(!IsRunRivalActive() || enemy_->IsDead()) return;
    if(enemy_->IsExpeditionRivalEnabled()) {
        const auto status=enemy_->GetRivalCombatStatus();using P=RivalBossCombat::Phase;
        const Vector3 origin=enemy_->GetWorldPosition()+Vector3{0,0,-0.35f};
        const bool reload=status.phase==P::Reload;
        for(int i=0;i<status.capacity;++i) {
            const float x=(static_cast<float>(i)-(status.capacity-1)*0.5f)*0.65f;
            const bool full=i<status.ammo;
            neonGridRenderer_->QueueLine(origin+Vector3{x,3.2f,0},origin+Vector3{x,3.6f,0},0.16f,
                reload?Vector4{0.2f,1.2f,1.1f,1}:full?Vector4{1.5f,0.65f,0.25f,1}:Vector4{0.20f,0.16f,0.16f,0.7f});
        }
        if(reload) {
            const int segments=static_cast<int>(32*status.progress);
            for(int i=0;i<segments;++i) {
                const float a=2*pi*i/32,b=2*pi*(i+1)/32;
                neonGridRenderer_->QueueLine(origin+Vector3{std::cos(a)*2.5f,std::sin(a)*2.5f,0},
                    origin+Vector3{std::cos(b)*2.5f,std::sin(b)*2.5f,0},0.13f,{0.25f,1.2f,1,0.9f});
            }
        }
        if(status.phase==P::DashWarning||status.phase==P::Dash) {
            const Vector3 side{-status.dashDirection.y,status.dashDirection.x,0};
            const Vector3 end=origin+status.dashDirection*status.dashDistance;
            const Vector4 tint{1.5f,0.45f,0.16f,0.65f+0.3f*status.progress};
            for(float offset:{-1.4f,1.4f}) neonGridRenderer_->QueueLine(origin+side*offset,end+side*offset,0.10f,tint);
            neonGridRenderer_->QueueLine(end-side*1.4f,end+side*1.4f,0.10f,tint);
            if(status.phase==P::Dash) for(int i=1;i<=3;++i) circle(origin-status.dashDirection*(0.85f*i),2.0f,0.10f,{1,0.35f,0.12f,0.4f/i},16);
        }
        if(status.phase==P::Tracking||status.phase==P::Locked||status.phase==P::Volley) {
            const float base=std::atan2(status.direction.y,status.direction.x);
            const float half=RivalBossCombat::WarningHalfAngle(status.pattern)*pi/180;
            const Vector4 tint{1.4f,0.45f+0.3f*status.progress,0.14f,status.phase==P::Volley?0.32f:0.65f};
            auto clipDistance=[&](const Vector3& ray) {
                float nearest=24.0f;
                for(const auto& row:stage_->GetBlocks()) for(const auto& block:row) if(block.isActive) {
                    float enter=0,leave=nearest;
                    auto slab=[&](float start,float direction,float low,float high) {
                        low-=0.15f;high+=0.15f;
                        if(std::abs(direction)<0.0001f) return start>=low&&start<=high;
                        float a=(low-start)/direction,b=(high-start)/direction;
                        if(a>b) std::swap(a,b);enter=(std::max)(enter,a);leave=(std::min)(leave,b);
                        return enter<=leave;
                    };
                    if(slab(origin.x,ray.x,block.aabb.min.x,block.aabb.max.x)&&slab(origin.y,ray.y,block.aabb.min.y,block.aabb.max.y)) nearest=(std::max)(0.0f,enter);
                }
                return nearest;
            };
            for(int i=0;i<3;++i) {
                const float angle=base+half*(i-1);const Vector3 ray{std::cos(angle),std::sin(angle),0};
                const float distance=clipDistance(ray);
                if(distance>1.8f) neonGridRenderer_->QueueLine(origin+ray*1.8f,origin+ray*distance,0.06f,tint);
            }
        }
        return;
    }
    const auto telegraph=enemy_->GetPrototypeTelegraph();if(!telegraph.active) return;
    Vector3 origin=enemy_->GetWorldPosition();origin.z=-0.3f;
    const float base=std::atan2(telegraph.direction.y,telegraph.direction.x),half=telegraph.spreadAngleDeg*pi/360;
    const bool ring=telegraph.attackType==Enemy::PrototypeAttackType::GapRing;
    const Vector4 color{1,0.30f+0.25f*telegraph.progress,0.08f,0.75f};
    auto radial=[&](float angle,float length,float width,Vector4 tint){
        neonGridRenderer_->QueueLine(origin,origin+Vector3{std::cos(angle)*length,std::sin(angle)*length,0},width,tint);
    };
    if(ring) {
        for(int i=0;i<48;++i) {
            const float a=base+half+(2*pi-2*half)*static_cast<float>(i)/48,b=base+half+(2*pi-2*half)*static_cast<float>(i+1)/48;
            const float radius=4+telegraph.progress*2;
            neonGridRenderer_->QueueLine(origin+Vector3{std::cos(a)*radius,std::sin(a)*radius,0},origin+Vector3{std::cos(b)*radius,std::sin(b)*radius,0},0.09f,color);
        }
        radial(base-half,12,0.065f,{0.12f,0.95f,0.88f,0.7f});radial(base+half,12,0.065f,{0.12f,0.95f,0.88f,0.7f});
    } else {
        radial(base-half,20,0.07f,color);radial(base+half,20,0.07f,color);
        for(int i=1;i<5;++i) radial(base-half+2*half*static_cast<float>(i)/5,20,0.025f,{0.8f,0.18f,0.06f,0.28f});
    }
}


void GameScene::RequestTankRunCapture(const std::string& name) {
    if(!tankRunCapturePath_.empty()) return;
    const std::string directory=expeditionRun_?"generated/tank_expedition/variant_"+std::to_string(tankExpeditionAutoVariant_):"generated/tank_run";
    std::filesystem::create_directories(directory);
    tankRunCapturePath_=directory+"/"+name+".png";
}

void GameScene::CopyTankRunCapture() {
    if(tankRunCapturePath_.empty()||tankRunCaptureCopied_) return;
    auto dx=Object3dCommon::GetInstance()->GetDxCommon();
    Microsoft::WRL::ComPtr<ID3D12Resource> source;
    if(FAILED(dx->GetSwapChain()->GetBuffer(dx->GetSwapChain()->GetCurrentBackBufferIndex(),IID_PPV_ARGS(&source)))) return;
    auto desc=source->GetDesc(); UINT64 size=0;
    dx->GetDevice()->GetCopyableFootprints(&desc,0,1,0,&tankRunCaptureLayout_,nullptr,nullptr,&size);
    D3D12_HEAP_PROPERTIES heap{}; heap.Type=D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC buffer{}; buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width=size; buffer.Height=1; buffer.DepthOrArraySize=1; buffer.MipLevels=1;
    buffer.SampleDesc.Count=1; buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if(FAILED(dx->GetDevice()->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&tankRunCaptureReadback_)))) {
        tankRunCapturePath_.clear(); return;
    }
    D3D12_RESOURCE_BARRIER barrier{}; barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource=source.Get(); barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_RENDER_TARGET; barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
    auto list=dx->GetList(); list->ResourceBarrier(1,&barrier);
    D3D12_TEXTURE_COPY_LOCATION from{},to{};
    from.pResource=source.Get(); from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    to.pResource=tankRunCaptureReadback_.Get(); to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; to.PlacedFootprint=tankRunCaptureLayout_;
    list->CopyTextureRegion(&to,0,0,0,&from,nullptr);
    std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter); list->ResourceBarrier(1,&barrier);
    tankRunCaptureCopied_=true;
}

void GameScene::FinishTankRunCapture() {
    if(!tankRunCaptureCopied_) return;
    void* pixels=nullptr;
    if(SUCCEEDED(tankRunCaptureReadback_->Map(0,nullptr,&pixels))) {
        const auto& layout=tankRunCaptureLayout_.Footprint;
        DirectX::Image photo{}; photo.width=layout.Width; photo.height=layout.Height;
        photo.format=layout.Format; photo.rowPitch=layout.RowPitch; photo.slicePitch=photo.rowPitch*photo.height;
        photo.pixels=static_cast<uint8_t*>(pixels)+tankRunCaptureLayout_.Offset;
        const auto path=std::filesystem::path(tankRunCapturePath_);
        if(FAILED(DirectX::SaveToWICFile(photo,DirectX::WIC_FLAGS_NONE,DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),path.c_str())))
            OutputDebugStringA("[TankRun] capture failed\n");
        D3D12_RANGE writes{0,0}; tankRunCaptureReadback_->Unmap(0,&writes);
    }
    tankRunCaptureReadback_.Reset(); tankRunCapturePath_.clear(); tankRunCaptureCopied_=false;
}
