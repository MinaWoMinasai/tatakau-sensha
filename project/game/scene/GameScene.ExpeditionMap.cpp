#include "GameScene.h"
#include "StartupTrace.h"
#include <fstream>
#include <iomanip>
#include <numeric>
#include <optional>
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
#include "externals/imgui/imgui.h"
#endif

namespace {
using NK=tankexp::NodeKind;
std::optional<tankexp::MapDefinition> sessionMap;
std::optional<tankexp::RoomCatalog> sessionRooms;
std::optional<tankcontent::Catalog> sessionContent;
std::unique_ptr<Sprite> MapRect(Vector2 p,Vector2 size,Vector4 color) {
    auto s=std::make_unique<Sprite>();s->Initialize(SpriteCommon::GetInstance(),"resources/white512x512.png");
    s->SetPosition(p);s->SetSize(size);s->SetColor(color);s->Update();return s;
}
std::unique_ptr<TextLabel> MapLabel(float size,Vector2 p,Vector4 color) {
    TextStyle style{};style.fontFamily="Meiryo";style.fontSize=size;style.color=color;style.padding=4;style.outlineThickness=0;
    auto t=std::make_unique<TextLabel>();t->Initialize(SpriteCommon::GetInstance()," ",style);t->SetPosition(p);return t;
}
Vector4 NodeColor(NK kind) {
    switch(kind) {
    case NK::Combat:return {0.28f,0.78f,1,1};case NK::Elite:return {1,0.49f,0.28f,1};
    case NK::Upgrade:case NK::Evolution:return {1,0.83f,0.27f,1};
    case NK::Heal:return {0.27f,1,0.63f,1};case NK::Currency:return {1,0.82f,0.27f,1};default:return {1,0.25f,0.45f,1};
    }
}
const char* NodeIcon(NK kind) {
    switch(kind) {case NK::Combat:return "戦";case NK::Elite:return "宝";case NK::Upgrade:return "改";
    case NK::Evolution:return "改";case NK::Heal:return "+";case NK::Currency:return "〇";default:return "核";}
}
const char* NodeName(NK kind) {
    switch(kind) {case NK::Combat:return "戦闘";case NK::Elite:return "宝物庫";case NK::Upgrade:return "強化工房";
    case NK::Evolution:return "強化工房";case NK::Heal:return "修理";case NK::Currency:return "通貨を受け取る";default:return "最終決戦";}
}
std::string WrapMapText(const std::string& text,float width,int maxLines) {
    std::string result;float used=0;int line=1;
    for(size_t pos=0;pos<text.size();) {
        const auto c=static_cast<unsigned char>(text[pos]);
        const size_t count=c<128?1:c<224?2:c<240?3:4;
        const bool newline=c=='\n';const float units=c<128?0.58f:1.0f;
        if(newline||used+units>width) {
            if(line>=maxLines) {result+="…";break;}
            result+='\n';used=0;++line;
            if(newline) {++pos;continue;}
        }
        result.append(text,pos,count);used+=units;pos+=count;
    }
    return result;
}
int ThreatCount(EnemyManager* enemies) {
    int n=0;for(auto* e:enemies->GetEnemyPtrs()) if(e&&!e->IsDead()&&!e->IsRunResource()) ++n;return n;
}
bool Press(Input* input,int key) {return input->IsKeyTriggered(static_cast<uint8_t>(key));}
bool Inside(Vector2 mouse,float x,float y,float w,float h) {return mouse.x>=x&&mouse.x<=x+w&&mouse.y>=y&&mouse.y<=y+h;}
}

void GameScene::InitializeExpeditionMap() {
    StartupTrace::Scope scope("Expedition.Map");
    expeditionMapEnabled_=true;
    expeditionMapDefinition_=tankexp::DefaultExpeditionMap();expeditionRooms_=tankexp::DefaultRoomCatalog();
    expeditionContent_=tankcontent::DefaultCatalog();std::string error;
    if(!tankexp::LoadExpeditionMap(tankexp::kExpeditionMapPath,expeditionMapDefinition_,error)) expeditionMapStatus_=error;
    if(!tankexp::LoadRoomCatalog(tankexp::kRoomCatalogPath,expeditionRooms_,error)) expeditionMapStatus_=error;
    if(!tankcontent::LoadCatalog("resources/configs/expedition_content.json",expeditionContent_,error)) expeditionMapStatus_=error;
    if(sessionMap) expeditionMapDefinition_=*sessionMap;
    if(sessionRooms) expeditionRooms_=*sessionRooms;
    if(sessionContent) expeditionContent_=*sessionContent;
    wchar_t mapTest[8]{};expeditionMapAutoTest_=GetEnvironmentVariableW(L"CG2_TANK_MAP_AUTOTEST",mapTest,8)>0&&mapTest[0]==L'1';
    expeditionSeed_=static_cast<uint32_t>(GetTickCount64())^expeditionMapDefinition_.generationSeed;
    if(expeditionMapAutoTest_) expeditionMapDefinition_=tankexp::DefaultExpeditionMap();
    else if(expeditionMapDefinition_.procedural) {
        tankexp::MapDefinition generated;
        if(tankexp::GenerateExpeditionMap(expeditionMapDefinition_,expeditionSeed_,generated,error,30,8))expeditionMapDefinition_=std::move(generated);
        else expeditionMapStatus_="生成設定を確認してください: "+error;
    }
    expeditionIntroOffers_=tankcontent::IntroUpgradeIds(expeditionContent_,expeditionSeed_);
    if(!tankexp::FindRoom(expeditionRooms_,"tutorial_training")) {
        auto room=tankexp::MakeEmptyRoom("tutorial_training","チュートリアル・戦闘");
        room.playerStart={34,30};room.spawns={{"practice_target","tutorial_target",44,30,12}};
        expeditionRooms_.rooms.push_back(std::move(room));
    }
    auto trainingEnemy=[this](const char* id,tankcontent::EnemyBehavior behavior,int credits) {
        if(tankcontent::FindEnemy(expeditionContent_,id)) return;
        tankcontent::Enemy e;e.id=id;e.name=id;e.behavior=behavior;e.hp=24;e.creditDrop=credits;
        e.contactDamage=2;e.bulletDamage=3;e.fireIntervalScale=2.0f;e.color={0.55f,0.95f,1.5f,1};
        expeditionContent_.enemies.push_back(e);
    };
    trainingEnemy("tutorial_target",tankcontent::EnemyBehavior::Square,4);
    trainingEnemy("tutorial_shooter",tankcontent::EnemyBehavior::Shooter,4);
    trainingEnemy("tutorial_retry",tankcontent::EnemyBehavior::Shooter,0);
    std::vector<std::string> knownEnemies;for(const auto& enemy:expeditionContent_.enemies)knownEnemies.push_back(enemy.id);
    if(!tankexp::ValidateExpeditionMapRooms(expeditionMapDefinition_,expeditionRooms_,error,&knownEnemies))expeditionMapStatus_="F4/F5/F6で参照を確認: "+error;
    InitializeCombatValidationFixture();
    InitializeExperienceValidation();
    InitializeSpecialValidationFixture();
    expeditionContentEditor_.Open(expeditionContent_);
    expeditionMapRun_.Reset(expeditionMapDefinition_,error);
    enemyManager_->SetExpeditionContent(expeditionContent_);player_->InstallRunAuthoredClasses(expeditionContent_);
    SetExpeditionBlueprint(0);player_->SetRunCurrencyMode(true);tankExpedition_.OpenMap();
    tankExpeditionTutorial_.Skip();InitializeExpeditionExperience();
    const auto initialNodes=expeditionMapRun_.GetAvailableNodeIds();
    if(!initialNodes.empty()) expeditionMapSelection_=initialNodes.front();
    // Prefer learning the controls even when an authored map lists skip first.
    for(const auto& id:initialNodes) if(const auto* n=tankexp::FindMapNode(expeditionMapRun_.GetDefinition(),id);
        n&&n->role==tankexp::NodeRole::TutorialCombat) {expeditionMapSelection_=id;break;}
    const Vector4 white{0.83f,0.96f,1,1},muted{0.42f,0.64f,0.75f,1};
    expeditionMapTitle_=MapLabel(32,{44,85},white);
    expeditionMapSubtitle_=MapLabel(16,{46,133},muted);
    expeditionMapLegend_=MapLabel(14,{46,195},muted);
    expeditionMapInfo_=MapLabel(18,{46,586},white);
    expeditionMapHelp_=MapLabel(13,{46,695},muted);
    expeditionCurtain_=MapRect({0,0},{1280,720},{0.006f,0.015f,0.03f,0});
    expeditionTransitionPanel_=MapRect({260,270},{760,156},{0.008f,0.024f,0.04f,0});
    expeditionTransitionRail_=MapRect({440,405},{400,2},{0.15f,0.35f,0.45f,1});
    expeditionTransitionProgress_=MapRect({440,405},{1,2},white);
    expeditionTransitionTitle_=MapLabel(34,{640,308},white);expeditionTransitionTitle_->SetAnchorPoint({0.5f,0.5f});
    expeditionTransitionDetail_=MapLabel(18,{640,360},muted);expeditionTransitionDetail_->SetAnchorPoint({0.5f,0.5f});
    for(int x=36;x<1260;x+=36) expeditionMapGrid_.push_back(MapRect({static_cast<float>(x),152},{1,420},{0.08f,0.26f,0.35f,0.13f}));
    for(int y=152;y<=572;y+=35) expeditionMapGrid_.push_back(MapRect({36,static_cast<float>(y)},{1208,1},{0.08f,0.26f,0.35f,0.13f}));
    for(const auto& node:expeditionMapRun_.GetDefinition().nodes) {
        MapNodeVisual v;v.halo=MapRect({0,0},{55,55},NodeColor(node.kind));v.rim=MapRect({0,0},{46,46},NodeColor(node.kind));
        v.fill=MapRect({0,0},{42,42},{0.016f,0.040f,0.065f,1});
        for(auto* s:{v.halo.get(),v.rim.get(),v.fill.get()}) {s->SetAnchorPoint({0.5f,0.5f});s->SetRotation(0.78539816f);}
        v.icon=MapLabel(23,{0,0},NodeColor(node.kind));v.icon->SetAnchorPoint({0.5f,0.5f});
        v.label=MapLabel(13,{0,0},white);v.label->SetAnchorPoint({0.5f,0});
        v.state=MapLabel(11,{0,0},muted);v.state->SetAnchorPoint({0.5f,0});
        expeditionMapVisuals_.push_back(std::move(v));
        for(const auto& next:node.next) {
            MapEdgeVisual edge;edge.from=node.id;edge.to=next;
            edge.glow=MapRect({0,0},{1,7},{0.12f,0.6f,0.78f,0.10f});edge.line=MapRect({0,0},{1,2},{0.2f,0.45f,0.57f,0.5f});
            edge.pulse=MapRect({0,0},{5,5},{0.45f,1,1,0});edge.pulse->SetAnchorPoint({0.5f,0.5f});edge.pulse->SetRotation(0.78539816f);
            edge.glow->SetAnchorPoint({0,0.5f});edge.line->SetAnchorPoint({0,0.5f});expeditionMapEdges_.push_back(std::move(edge));
        }
    }
    for(int i=0;i<3;++i) {
        const float x=46.0f+i*394.0f;expeditionBlueprintButtons_[i]=MapRect({x,650},{378,34},{0.035f,0.11f,0.16f,1});
        expeditionBlueprintLabels_[i]=MapLabel(15,{x+12,654},white);expeditionBlueprintLabels_[i]->SetText(i==0?"←":i==1?"→":" ");
    }
    wchar_t flag[8]{};expeditionMapAutoTest_=GetEnvironmentVariableW(L"CG2_TANK_MAP_AUTOTEST",flag,8)>0&&flag[0]==L'1';
    if(expeditionMapAutoTest_) {
        debugPlayerNoDamage_=true;tankExpeditionTutorial_.Skip();
        std::filesystem::create_directories("generated/expedition_map");
        std::ofstream("generated/expedition_map/validation.json")<<"{\"completed\":false}\n";
    }
    InitializeExpeditionBuildCards();
    RefreshExpeditionMapUi();
}

void GameScene::BeginExpeditionPresentation(int action,const std::string& title,const std::string& detail,const Vector4& color) {
    if(!expeditionTransition_.Begin()) return;
    expeditionTransitionAction_=action;expeditionTransitionColor_=color;
    expeditionTransitionTitle_->SetText(title);expeditionTransitionDetail_->SetText(detail);
    expeditionTransitionTitle_->PrepareForDraw();expeditionTransitionDetail_->PrepareForDraw();
    tankExpeditionAudio_.UiConfirm();
}

void GameScene::RequestExpeditionMapNode(const std::string& id) {
    if(expeditionTransition_.IsActive()||expeditionBuildChoice_) return;
    if(!expeditionMapRun_.CanSelectNode(id)) {
        expeditionUiErrorAge_=0.35f;tankExpeditionAudio_.UiDenied();
        expeditionMapStatus_="明るく光る、接続された地点を選んでください。";
        RefreshExpeditionMapUi();return;
    }
    const auto* node=tankexp::FindMapNode(expeditionMapRun_.GetDefinition(),id);if(!node) return;
    if(!expeditionBuildChosen_&&node->role==tankexp::NodeRole::None&&!combatValidationEnabled_) {
        expeditionBuildChoice_=true;tankRunMenuAge_=0;RefreshTankExpeditionUi();return;
    }
    expeditionPendingNode_=id;
    BeginExpeditionPresentation(1,tankexp::IsCombatNode(node->kind)?"出撃":"入場",
        node->role==tankexp::NodeRole::TutorialCombat?"操作を学ぶ":node->role==tankexp::NodeRole::TutorialSkip?"説明をスキップ / 同じ通貨を受け取ります":NodeName(node->kind),NodeColor(node->kind));
}

void GameScene::UpdateExpeditionPresentation(float dt) {
    expeditionPresentationClock_+=dt;
    UpdateExpeditionBuildCards(dt);
    expeditionUiErrorAge_=(std::max)(0.0f,expeditionUiErrorAge_-dt);
    expeditionHitSparkCooldown_=(std::max)(0.0f,expeditionHitSparkCooldown_-dt);
    for(auto& hit:expeditionHitSparks_) hit.age+=dt;
    std::erase_if(expeditionHitSparks_,[](const ExpeditionHitSpark& hit){return hit.age>=0.18f;});
    const float blend=1-std::exp(-dt*16);
    const auto& nodes=expeditionMapRun_.GetDefinition().nodes;
    for(size_t i=0;i<expeditionMapVisuals_.size();++i) {
        auto& focus=expeditionMapVisuals_[i].focus;
        focus+=((nodes[i].id==expeditionMapSelection_?1.0f:0.0f)-focus)*blend;
    }
    for(int i=0;i<3;++i) expeditionCardFocus_[i]+=((i==tankRunSelection_?1.0f:0.0f)-expeditionCardFocus_[i])*blend;
    if(expeditionTransition_.IsActive()) {
        if(expeditionMapAutoTest_&&expeditionTransition_.Age()>0.26f&&expeditionTransition_.Age()<0.45f&&tankRunCapturePath_.empty()) {
            const std::string capture="transition_"+std::to_string(expeditionTransitionAction_);
            if(std::find(expeditionMapTestVisited_.begin(),expeditionMapTestVisited_.end(),capture)==expeditionMapTestVisited_.end()) {
                expeditionMapTestVisited_.push_back(capture);tankRunCapturePath_="generated/expedition_map/"+capture+".png";
            }
        }
        // Let the final kill's local burst finish while gameplay remains frozen.
        for(auto& burst:tankRunBursts_) burst.age+=dt;
        std::erase_if(tankRunBursts_,[](const RunBurst& b){return b.age>(b.resource?0.7f:0.35f);});
        if(expeditionTransition_.Advance(dt)) {
            const int action=expeditionTransitionAction_;expeditionTransitionAction_=0;
            if(action==1) EnterExpeditionMapNode(expeditionPendingNode_);
            else if(action==2) CompleteExpeditionMapCombat();
            else if(action==3) SelectExpeditionService(expeditionPendingService_);
            else if(action==4) SelectExpeditionBuildStyle(expeditionPendingBuild_);
        }
        if(!expeditionTransition_.IsActive()) {tankRunMenuAge_=0;expeditionPendingNode_.clear();}
    }
}

void GameScene::DrawExpeditionPresentation() {
    if(!expeditionTransition_.IsActive()||!expeditionCurtain_) return;
    SpriteCommon::GetInstance()->PreDraw(kNormal);
    expeditionCurtain_->SetColor({0.006f,0.015f,0.03f,expeditionTransition_.Cover()});expeditionCurtain_->Update();expeditionCurtain_->Draw();
    const float alpha=expeditionTransition_.LabelAlpha();
    expeditionTransitionPanel_->SetColor({0.008f,0.024f,0.04f,alpha*0.94f});expeditionTransitionPanel_->Update();expeditionTransitionPanel_->Draw();
    auto color=expeditionTransitionColor_;color.w=alpha;
    const float width=400*tankexp::PresentationTransition::Smooth(expeditionTransition_.Age()/0.70f);
    expeditionTransitionRail_->SetColor({0.12f,0.30f,0.40f,alpha});expeditionTransitionRail_->Update();expeditionTransitionRail_->Draw();
    expeditionTransitionProgress_->SetSize({(std::max)(1.0f,width),2});expeditionTransitionProgress_->SetColor(color);
    expeditionTransitionProgress_->Update();expeditionTransitionProgress_->Draw();
    expeditionTransitionTitle_->SetAlpha(alpha);expeditionTransitionDetail_->SetAlpha(alpha);
    expeditionTransitionTitle_->Draw();expeditionTransitionDetail_->Draw();
}

void GameScene::SetExpeditionBlueprint(int index) {
    if(index<0||index>2||!expeditionMapRun_.GetChosenNodeIds().empty()) return;
    expeditionBlueprint_=index;
    tankrun::Config config;config.combatSeconds=1000000;
    tankRun_=tankrun::RunDirector(static_cast<uint32_t>(GetTickCount64()),config);
    tankRun_.ChooseLoadout(index);player_->ConfigurePrototypeLoadout(index);tankRun_.ChooseCore(index);ApplyTankRunCards();
}

void GameScene::UpdateExpeditionAuthoring() {
    // Keep queued purchases and destinations stable until their transition commits.
    if(expeditionTransition_.IsActive()) return;
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
    if(!ImGui::GetCurrentContext()) return;
#else
    return;
#endif
    // Authoring runs before the paused-frame decision. Applying content never
    // restarts a fight or resets the run's wallet/history.
    if(Press(input_,DIK_F4)) expeditionRoomEditorOpen_=!expeditionRoomEditorOpen_;
    if(Press(input_,DIK_F5)) expeditionMapEditorOpen_=!expeditionMapEditorOpen_;
    if(Press(input_,DIK_F6)) expeditionContentEditorOpen_=!expeditionContentEditorOpen_;
    std::vector<std::string> enemies,rooms;
    for(const auto& e:expeditionContent_.enemies) enemies.push_back(e.id);
    for(const auto& r:expeditionRooms_.rooms) rooms.push_back(r.id);
    if(expeditionRoomEditor_.Draw(&expeditionRoomEditorOpen_,expeditionRooms_,enemies,&expeditionMapDefinition_,&expeditionMapRun_.GetDefinition())) {sessionRooms=expeditionRooms_;expeditionMapStatus_="配置を適用しました。次の区画への入場時に反映します。";}
    if(expeditionMapEditor_.Draw(expeditionMapEditorOpen_,expeditionMapDefinition_,expeditionRooms_,&enemies)) {sessionMap=expeditionMapDefinition_;expeditionMapStatus_="作戦マップを適用しました。次の遠征から反映します。";}
    std::vector<std::string> usedEnemyIds;
    for(const auto& room:expeditionRooms_.rooms)for(const auto& spawn:room.spawns)usedEnemyIds.push_back(spawn.type);
    if(expeditionContentEditor_.Draw(expeditionContentEditorOpen_,expeditionContent_,usedEnemyIds)) {
        enemyManager_->SetExpeditionContent(expeditionContent_);player_->InstallRunAuthoredClasses(expeditionContent_);
        expeditionIntroOffers_=tankcontent::IntroUpgradeIds(expeditionContent_,expeditionSeed_);
        sessionContent=expeditionContent_;ApplyTankRunCards();
        expeditionMapStatus_="種類を適用しました。次の敵出現・工房で選択できます。";RefreshExpeditionServiceOffers();
    }
}

void GameScene::EnterExpeditionMapNode(const std::string& id) {
    if(!expeditionMapRun_.CanSelectNode(id)) return;
    const auto* node=tankexp::FindMapNode(expeditionMapRun_.GetDefinition(),id);if(!node) return;
    if(tankexp::IsCombatNode(node->kind)) {
        const auto* room=tankexp::FindRoom(expeditionRooms_,node->roomTemplate);
        if(!room) {expeditionMapStatus_="部屋が見つかりません。F4 / F5で配置と部屋の関連付けを確認してください。";return;}
        std::string error;
        if(!tankexp::ValidateRoomCatalog(expeditionRooms_,error)) {expeditionMapStatus_=error;return;}
        for(const auto& spawn:room->spawns) if(!tankcontent::FindEnemy(expeditionContent_,spawn.type)) {
            expeditionMapStatus_="必要な敵の設定が見つかりません。F4 / F6で設定を確認してください。";return;
        }
        if((node->kind==NK::Boss)!=(room->objective=="boss")) {
            expeditionMapStatus_="最終決戦にはボス目標の部屋、それ以外には通常目標の部屋を指定してください。";return;
        }
    }
    const auto before=expeditionMapRun_;const auto beforeDirector=tankExpedition_;
    if(!expeditionMapRun_.SelectNode(id)) return;
    if(node->role==tankexp::NodeRole::TutorialCombat) {
        expeditionGuideActive_=true;expeditionGuideShooterSpawned_=false;expeditionGuide_.Begin(1,4);
        expeditionGuideAge_=0;expeditionGuideLastKills_=defeatedEnemies_;expeditionGuideDamageCount_=player_->GetDamageTakenCount();expeditionGuideAttackCount_=player_->GetPrimaryAttackCount();
    } else if(node->role==tankexp::NodeRole::TutorialUpgrade) {
        expeditionGuideActive_=true;expeditionGuide_.BeginUpgradeOnly();expeditionGuideAge_=0;
    } else if(node->role==tankexp::NodeRole::TutorialSkip) expeditionGuideActive_=false;
    tankExpeditionTutorial_.RecordRoute();expeditionMapStatus_.clear();expeditionServicePage_=0;tankRunSelection_=0;tankRunMenuAge_=0;
    if(tankexp::IsCombatNode(node->kind)) {
        const auto* room=tankexp::FindRoom(expeditionRooms_,node->roomTemplate);
        const auto kind=room->objective=="boss"?tankexp::RoomKind::Boss:room->objective=="control"?tankexp::RoomKind::Guard:
            node->kind==NK::Elite?tankexp::RoomKind::Elite:tankexp::RoomKind::Skirmish;
        bool entered=false;
        try {entered=tankExpedition_.BeginMapRoom(node->combatStage,kind)&&StartAuthoredExpeditionRoom();} catch(const std::exception& e) {expeditionMapStatus_=e.what();}
        if(!entered) {
            expeditionMapRun_=before;tankExpedition_=beforeDirector;
            expeditionMapStatus_="区画を読み込めませんでした。進行と通貨を保持しています。 "+expeditionMapStatus_;
        }
    } else {
        if(node->kind==NK::Currency) {SpawnExpeditionCredits(player_->GetWorldPosition(),node->clearReward,true);expeditionCollectAll_=true;}
        RefreshExpeditionServiceOffers();
    }
    RefreshTankExpeditionUi();
}

bool GameScene::StartAuthoredExpeditionRoom() {
    const auto* node=expeditionMapRun_.GetActiveNode();if(!node) return false;
    const auto* room=tankexp::FindRoom(expeditionRooms_,node->roomTemplate);if(!room) return false;
    // Stage retains its existing merged-block collision/bloom batching path.
    std::filesystem::create_directories("generated/expedition_map");
    const std::string path="generated/expedition_map/active_room.csv";
    {std::ofstream out(path);out<<tankexp::RoomToCsv(*room);if(!out) {expeditionMapStatus_="配置プレビューを書き込めませんでした。";return false;}}
    if(!stage_->LoadRunMap(path)) return false;
    enemy_->SetRunEncounterEnabled(false);tankExpeditionRivalActive_=false;
    enemyManager_->ClearRunActors();bulletManager_->ClearAll();
    expeditionHitSparks_.clear();expeditionBossPhase2Seen_=false;
    expeditionCollectAll_=false;expeditionClearRewardQueued_=false;expeditionCredits_.clear();
    playerLaserBeams_.clear();playerMines_.clear();playerMineExplosions_.clear();playerMeleeSlashes_.clear();
    playerNeonAfterimages_.clear();neonTriangleParticles_.clear();tankRunBursts_.clear();hpBarVisibility_.clear();
    if(playerMeleeTrailManager_) playerMeleeTrailManager_->ClearInstances();
    for(auto& resource:tankRunResources_) resource={};
    tankExpeditionNodes_=0;tankExpeditionSpawned_=0;tankExpeditionRoomPending_=false;
    tankExpeditionResourceWon_=false;tankExpeditionRewardOpen_=false;tankExpeditionMaintenanceOpen_=false;
    tankExpeditionArrival_=0;tankExpeditionEnemyHp_.clear();tankExpeditionEnemyWarning_.clear();
    tankRunComboTime_=0;tankExpeditionDetailsOpen_=false;stagePostCacheValid_=false;
    stage_->SetDamageBlockDamage(12);
    const Vector3 start{room->playerStart.x,room->playerStart.y,0};player_->ResetRunRoomState(start);
    tankExpeditionTutorialPrevious_=start;camera->SetTranslate({start.x,start.y,camera->GetTranslate().z});camera->Update();
    for(const auto& spawn:room->spawns) {
        if(enemyManager_->SpawnLevelEnemy({spawn.x,spawn.y,0},spawn.type,spawn.hp>0?spawn.hp:-1)) ++tankExpeditionSpawned_;
    }
    if(room->objective=="control") for(size_t i=0;i<room->objectiveTargets.size()&&i<tankRunResources_.size();++i) {
        auto& r=tankRunResources_[i];r.position={room->objectiveTargets[i].x,room->objectiveTargets[i].y,0};
        r.active=enemyManager_->SpawnRunResource(r.position,60,[this,i](bool owned){OnTankRunResourceClaim(i,owned);});
    }
    if(room->objective=="boss") {
        const auto& p=room->objectiveTargets.front();tankExpeditionRivalActive_=true;
        enemy_->ResetRunEncounter({p.x,p.y,0},tankExpeditionBalance_.value("bossMaxHp",900),1,true);
        enemy_->EnableExpeditionRival(true);
        auto progress=enemy_->GetEnemyProgressConfig();progress.levelingModeEnabled=false;enemy_->SetEnemyProgressConfig(progress);
        screenEffectDirector_.TriggerBossEntry();
    }
    // Apply shared boss pressure, then restore per-prefab parameters after the
    // global balancing pass (otherwise authored enemy contact damage is lost).
    ApplyTankExpeditionRoomBalance();
    previousPlayerHp_=player_->GetHp();previousBossHp_=enemy_->GetHp();bossDefeatHandled_=false;
    if(!expeditionTransition_.IsActive()) SetEventCallout(std::string(NodeName(node->kind))+" / "+(room->objective=="control"?"通貨ボックスを3つ壊そう":room->objective=="boss"?"ボスを撃破":"敵を全滅"),1.4f);
    return true;
}

void GameScene::CompleteExpeditionMapCombat() {
    if(!tankExpedition_.IsCombat()||player_->IsDead()) return;
    const auto* node=expeditionMapRun_.GetActiveNode();if(!node||node->kind==NK::Boss) return;
    const int reward=node->clearReward;
    if(!expeditionMapRun_.CompleteCombat(false)) return;
    tankExpedition_.OpenMap();tankExpeditionTutorial_.RecordRoomClear();
    expeditionMapSelection_=expeditionMapRun_.GetAvailableNodeIds().front();
    expeditionMapStatus_="戦闘クリア！ 光る地点を選んで進もう。";
    if(const auto* next=tankexp::FindMapNode(expeditionMapRun_.GetDefinition(),expeditionMapSelection_)) expeditionMapScroll_=(std::max)(0.0f,120.0f*(next->column-8));
    tankExpeditionAudio_.Upgrade();tankRunMenuAge_=0;RefreshTankExpeditionUi();
}

void GameScene::RefreshExpeditionServiceOffers() {
    expeditionServiceOffers_.clear();const auto* node=expeditionMapRun_.GetActiveNode();if(!node) return;
    if(node->kind==NK::Upgrade||node->kind==NK::Evolution) {
        if(IsIntroExpeditionService()) expeditionServiceOffers_=expeditionIntroOffers_;
        else {
            uint32_t state=expeditionSeed_;for(const unsigned char c:node->id) state=(state^c)*16777619u;
            expeditionServiceOffers_=tankcontent::BuildShopOffers(expeditionContent_,expeditionBuildStyle_,tankRun_.GetCardCounts(),expeditionPurchases_,state);
        }
    }
    expeditionServicePage_=(std::clamp)(expeditionServicePage_,0,(std::max)(0,(static_cast<int>(expeditionServiceOffers_.size())-1)/3));
}

bool GameScene::IsIntroExpeditionService() const {
    const auto* node=expeditionMapRun_.GetActiveNode();return node&&tankexp::IsIntroUpgrade(node->role);
}

int GameScene::ExpeditionServicePrice(const std::string& id) const {
    const auto* node=expeditionMapRun_.GetActiveNode();if(!node) return 0;
    if(IsIntroExpeditionService()) return node->serviceCost;
    if(node->kind==NK::Upgrade||node->kind==NK::Evolution) {const auto* u=tankcontent::FindUpgrade(expeditionContent_,id);return (std::max)(u?u->price:0,node->serviceCost);}
    return node->serviceCost;
}

void GameScene::SelectExpeditionService(int option) {
    const auto* node=expeditionMapRun_.GetActiveNode();if(!node||tankexp::IsCombatNode(node->kind)||node->kind==NK::Currency||option<0||option>3) return;
    auto reject=[this] {expeditionUiErrorAge_=0.35f;tankExpeditionAudio_.UiDenied();RefreshTankExpeditionUi();};
    if(!expeditionTransition_.IsActive()) {
        int price=0;
        if(option!=3) {
            if(node->kind==NK::Heal) {
                if(option!=0) return;
                if(player_->GetHp()>=player_->GetMaxHp()) {expeditionMapStatus_="HPは満タンです。「修理せず進む」を選んでください。";reject();return;}
                price=node->serviceCost;
            } else {
                const size_t index=static_cast<size_t>(expeditionServicePage_*3+option);
                if(index>=expeditionServiceOffers_.size()) {reject();return;}
                const auto& id=expeditionServiceOffers_[index];
                price=ExpeditionServicePrice(id);
            }
            if(!expeditionMapRun_.CanAfford(price)) {expeditionMapStatus_=std::string("通貨が足りません。")+(node->kind==NK::Heal?"修理せず進めます。":"購入せず進めます。");reject();return;}
        }
        expeditionPendingService_=option;
        if(option<3&&IsExpeditionBuildCardScreen()) expeditionRewardCards_[option]->PlayAcquire();
        BeginExpeditionPresentation(3,option==3?"次の地点へ":node->kind==NK::Heal?"装甲を修理":"強化を装備",
            option==3?"残りの通貨を持って先へ進みます":(node->kind==NK::Heal?"修理しています":"装備しています"),NodeColor(node->kind));
        return;
    }
    bool complete=false;
    if(option==3) complete=expeditionMapRun_.CompleteService(false);
    else if(node->kind==NK::Heal) {
        if(option!=0) return;
        if(player_->GetHp()>=player_->GetMaxHp()) {expeditionMapStatus_="HPは満タンです。「修理せず進む」を選んでください。";reject();return;}

        if(!expeditionMapRun_.CompleteService(true)) {expeditionMapStatus_="通貨が足りません。修理せず進めます。";reject();return;}
        player_->HealRunPlayer((std::max)(1,player_->GetMaxHp()/2));previousPlayerHp_=player_->GetHp();
        expeditionMapStatus_="HPを50%回復しました";complete=true;++expeditionMapTestHeals_;
    } else {
        const size_t index=static_cast<size_t>(expeditionServicePage_*3+option);if(index>=expeditionServiceOffers_.size()) return;
        const auto id=expeditionServiceOffers_[index];
        if(node->kind==NK::Upgrade||node->kind==NK::Evolution) {
            const auto* upgrade=tankcontent::FindUpgrade(expeditionContent_,id);if(!upgrade) return;
            if(!tankcontent::EligibleUpgrade(*upgrade,expeditionBuildStyle_,tankRun_.GetCardCounts())) {expeditionMapStatus_="このスタイルには装備できない強化です。";reject();return;}
            const int price=ExpeditionServicePrice(id);
            if(!expeditionMapRun_.CanAfford(price)) {expeditionMapStatus_="通貨が足りません。別の強化を選ぶか次へ進めます。";reject();return;}
            const int hpBeforeUpgrade=player_->GetHp(),maxHpBeforeUpgrade=player_->GetMaxHp();
            const int creditsBeforeUpgrade=expeditionMapRun_.GetCurrency();
            const auto cardsBeforeUpgrade=tankRun_.GetCardCounts();
            const auto classBeforeUpgrade=player_->GetCurrentClassName();
            if(!tankRun_.GrantExpeditionModules(upgrade->effects)) {expeditionMapStatus_="この強化の効果はすでにすべて装備済みです。";return;}
            expeditionMapRun_.TrySpendCurrency(price);++expeditionPurchases_[id];++expeditionMapTestPurchases_;
            expeditionPurchasedModules_[id]=*upgrade;
            ApplyTankRunCards();
            if(std::find(upgrade->effects.begin(),upgrade->effects.end(),tankrun::CardId::Repair)!=upgrade->effects.end()) {
                const int desiredHp=(std::min)(player_->GetMaxHp(),hpBeforeUpgrade+player_->GetMaxHp()-maxHpBeforeUpgrade);
                player_->HealRunPlayer((std::max)(0,desiredHp-player_->GetHp()));previousPlayerHp_=player_->GetHp();
            }
            if(experienceValidationVariant_||expeditionMapAutoTest_) {
                bool preserved=creditsBeforeUpgrade-price==expeditionMapRun_.GetCurrency()&&classBeforeUpgrade==player_->GetCurrentClassName();
                for(std::size_t c=0;c<cardsBeforeUpgrade.size();++c)preserved&=tankRun_.GetCardCounts()[c]>=cardsBeforeUpgrade[c];
                for(const auto effect:upgrade->effects)preserved&=tankRun_.GetCardCount(effect)==1;
                if(std::find(upgrade->effects.begin(),upgrade->effects.end(),tankrun::CardId::Repair)==upgrade->effects.end())preserved&=hpBeforeUpgrade==player_->GetHp()&&maxHpBeforeUpgrade==player_->GetMaxHp();
                const bool additive=std::any_of(upgrade->effects.begin(),upgrade->effects.end(),[](auto effect){return effect>=tankrun::CardId::ExtraBarrel1;});
                if(additive&&preserved){experienceEvolutionVerified_=true;++expeditionMapTestEvolutions_;}
                if(!preserved&&experienceValidationVariant_)experienceValidationErrors_.push_back("Upgrade replaced class, lost modules, changed health or charged incorrect currency");
            }
            tankExpeditionTutorial_.RecordUpgrade();
            expeditionMapStatus_=upgrade->name+"を装備しました";
            complete=expeditionMapRun_.CompleteService(false);
        }
    }
    if(complete) {
        if(expeditionGuideActive_&&tankexp::IsIntroUpgrade(node->role)) {
            expeditionGuide_.ResolveUpgrade(option!=3);
            if(expeditionGuide_.IsComplete()) SaveExpeditionTutorialCompletion();
        }
        if(tankexp::IsIntroUpgrade(node->role)&&!expeditionBuildChosen_) expeditionBuildChoice_=true;
        tankExpeditionAudio_.Upgrade();tankRunSelection_=0;tankRunMenuAge_=0;
        const auto next=expeditionMapRun_.GetAvailableNodeIds();if(!next.empty()) expeditionMapSelection_=next.front();
        if(const auto* focus=tankexp::FindMapNode(expeditionMapRun_.GetDefinition(),expeditionMapSelection_)) expeditionMapScroll_=(std::max)(0.0f,120.0f*(focus->column-8));
        expeditionTransitionTitle_->SetText(option==3?"次の地点へ":node->kind==NK::Heal?"修理完了":"強化を装備しました");
        expeditionTransitionDetail_->SetText(option==3?"残りの通貨を持って先へ進みます":WrapMapText(expeditionMapStatus_,46,2));
        expeditionTransitionTitle_->PrepareForDraw();expeditionTransitionDetail_->PrepareForDraw();
    }
    RefreshTankExpeditionUi();
}

void GameScene::RefreshExpeditionMapUi() {
    // The hidden map must not overwrite the pause menu's focus or emit hover
    // audio. Both screens share expeditionLastFocus_; only the visible one owns it.
    if(!expeditionMapTitle_||tankRunPaused_) return;
    if(expeditionBuildChoice_&&!tankRunPaused_) {
        expeditionMapTitle_->SetText("戦闘スタイルを選ぼう");expeditionMapTitle_->PrepareForDraw();
        expeditionMapSubtitle_->SetText("カーソルを合わせると攻撃方法を確認できます。");expeditionMapSubtitle_->PrepareForDraw();
        tankRunDescription_->SetText("選んだスタイルに合う強化が工房に並びます。");tankRunDescription_->SetPosition({64,209});tankRunDescription_->PrepareForDraw();
        RefreshExpeditionBuildCards();return;
    }
    const auto& definition=expeditionMapRun_.GetDefinition();
    const auto* active=expeditionMapRun_.GetActiveNode();
    const bool service=active&&!tankexp::IsCombatNode(active->kind)&&!expeditionMapPreview_&&!tankRunPaused_;
    expeditionMapTitle_->SetText(service?NodeName(active->kind):" ");
    expeditionMapSubtitle_->SetText(" ");expeditionMapHelp_->SetText(" ");
    expeditionMapLegend_->SetPosition({46,111});
    expeditionMapLegend_->SetText("戦  戦闘     宝  宝物庫     改  工房     +  修理     核  最終決戦");
    if(service) {
        tankRunDescription_->SetPosition({64,209});
        tankRunDescription_->SetText(active->kind==NK::Currency?"操作を学ぶルートと同じ通貨を受け取っています。次の工房で同じ強化を選べます。":active->kind==NK::Heal?"次の戦いに備えて、装甲を修理できます。":
            expeditionServiceOffers_.empty()?"ここで購入できる強化はありません。次の地点へ進めます。":"強化を1つ購入できます。購入せず進むこともできます。");
        if(expeditionGuideActive_&&active->role==tankexp::NodeRole::TutorialUpgrade&&!expeditionGuide_.IsComplete())
            tankRunDescription_->SetText(" ");
        if(active->kind==NK::Heal) {
            const bool full=player_->GetHp()>=player_->GetMaxHp(),affordable=expeditionMapRun_.CanAfford(active->serviceCost);
            tankRunCardTitles_[0]->SetText("装甲を修理");
            tankRunCardBodies_[0]->SetText("最大HPの50%を回復\n価格        "+std::to_string(active->serviceCost)+"\n\n現在HP  "+
                std::to_string(player_->GetHp())+" / "+std::to_string(player_->GetMaxHp())+"\n\n"+
                (full?"HPが満タンのため修理できません":!affordable?"通貨が足りないため修理できません":"左クリックで修理"));
        }
        const bool repair=active->kind==NK::Heal;
        expeditionSkipButton_->SetPosition(repair?Vector2{490,560}:Vector2{966,620});
        expeditionSkipButton_->SetSize(repair?Vector2{300,52}:Vector2{242,44});
        expeditionSkipText_->SetPosition(repair?Vector2{640,574}:Vector2{1087,629});
        expeditionSkipText_->SetText(repair?"修理せず進む":"購入せず進む");
        const auto mouse=input_->GetMousePosition();
        const bool skipFocus=tankRunSelection_==3||Inside(mouse,repair?490.0f:966.0f,repair?560.0f:620.0f,repair?300.0f:242.0f,repair?52.0f:44.0f);
        expeditionSkipButton_->SetColor(skipFocus?Vector4{0.065f,0.19f,0.22f,1}:Vector4{0.045f,0.09f,0.13f,1});
        expeditionSkipButton_->Update();expeditionSkipText_->PrepareForDraw();
        expeditionMapInfo_->SetPosition({46,674});
        expeditionMapInfo_->SetText(WrapMapText(expeditionMapStatus_,60,1));
    } else {
        expeditionMapInfo_->SetPosition({46,586});
        const auto* selected=tankexp::FindMapNode(definition,expeditionMapSelection_);
        std::string info;
        if(selected) {
            info="       ";
            info+=tankexp::IsCombatNode(selected->kind)?std::to_string(selected->clearReward)+"  / "+NodeName(selected->kind)+(selected->kind==NK::Elite?"：通貨を多く獲得":"のクリア報酬"):
                selected->kind==NK::Upgrade||selected->kind==NK::Evolution?std::to_string(selected->serviceCost)+"〜  / 強化工房":
                selected->kind==NK::Currency?std::to_string(selected->clearReward)+"  / 通貨を受け取る":std::to_string(selected->serviceCost)+"  / HPを50%修理";
            if(!expeditionMapRun_.CanSelectNode(selected->id)&&!expeditionMapPreview_) info+="   / まだ選択できません";
        }
        expeditionMapInfo_->SetText(info);
        expeditionMapHelp_->SetPosition({218,630});expeditionMapHelp_->SetText(WrapMapText(expeditionMapStatus_,48,1));
    }
    auto position=[this](const tankexp::MapNode& n){return Vector2{90+120.0f*n.column-expeditionMapScroll_,186+84.0f*n.row};};
    const auto available=expeditionMapRun_.GetAvailableNodeIds();
    EnsureExpeditionPointers((std::max)(size_t{6},available.size()*6));
    const auto* current=tankexp::FindMapNode(definition,expeditionMapRun_.GetCurrentNodeId());
    std::set<std::string> future;std::vector<std::string> pending=available;
    if(active) pending=active->next;
    while(!pending.empty()) {auto id=pending.back();pending.pop_back();if(!future.insert(id).second) continue;if(const auto* n=tankexp::FindMapNode(definition,id)) pending.insert(pending.end(),n->next.begin(),n->next.end());}
    for(size_t i=0;i<definition.nodes.size()&&i<expeditionMapVisuals_.size();++i) {
        const auto& n=definition.nodes[i];auto& v=expeditionMapVisuals_[i];v.center=position(n);
        const bool reachable=expeditionMapRun_.CanSelectNode(n.id),visited=expeditionMapRun_.HasVisited(n.id);
        const bool selected=n.id==expeditionMapSelection_;
        const bool intro=n.role==tankexp::NodeRole::TutorialCombat||n.role==tankexp::NodeRole::TutorialSkip;
        const bool learning=reachable&&n.role==tankexp::NodeRole::TutorialCombat;
        const bool abandoned=!visited&&!reachable&&!future.contains(n.id)&&current;
        auto tint=visited?Vector4{0.25f,1.35f,0.62f,1}:NodeColor(n.kind);tint.w=abandoned?0.09f:reachable||selected?1.0f:visited?0.9f:0.35f;
        const float pulse=0.5f+0.5f*std::sin(expeditionPresentationClock_*3.8f-static_cast<float>(i)*0.4f);
        const float focus=v.focus;
        if(selected&&expeditionUiErrorAge_>0) tint={1,0.25f,0.25f,1};
        v.rim->SetColor(tint);auto halo=tint;halo.w=abandoned?0:visited?0.18f:0.02f+focus*0.23f+(reachable?0.08f*pulse:0)+(learning?0.06f*pulse:0);v.halo->SetColor(halo);
        v.halo->SetSize({55+focus*10+pulse*(learning?5.0f:3.0f),55+focus*10+pulse*(learning?5.0f:3.0f)});
        v.rim->SetSize({46+focus*5,46+focus*5});v.fill->SetSize({42+focus*5,42+focus*5});
        v.fill->SetColor(visited?Vector4{0.03f,0.14f,0.16f,1}:Vector4{0.016f,0.040f,0.065f,1});
        for(auto* s:{v.halo.get(),v.rim.get(),v.fill.get()}) {s->SetPosition(v.center);s->Update();}
        auto style=v.icon->GetStyle();style.color=visited?Vector4{0.3f,1.4f,0.65f,1}:NodeColor(n.kind);v.icon->SetStyle(style);
        v.icon->SetText(visited?"完":NodeIcon(n.kind));v.icon->SetPosition({v.center.x,v.center.y-2});v.icon->SetAlpha(abandoned?0.13f:reachable||visited||selected?1.0f:0.48f);
        v.label->SetText(n.role==tankexp::NodeRole::TutorialCombat?"操作を学ぶ":n.role==tankexp::NodeRole::TutorialSkip?"説明をスキップ":" ");
        // Reuse each node's existing labels; keep both introductory captions in
        // the visible map even at the left edge without allocating new UI.
        const float captionX=intro?(std::max)(112.0f,v.center.x):v.center.x;
        v.label->SetPosition({captionX,v.center.y+39});v.label->SetAlpha(abandoned?0.2f:reachable&&intro?1.0f:0.85f);
        v.state->SetText(n.role==tankexp::NodeRole::TutorialCombat?"初回プレイにおすすめ":n.role==tankexp::NodeRole::TutorialSkip?"操作を知っている人向け":" ");
        v.state->SetPosition({captionX,v.center.y+60});v.state->SetAlpha(abandoned?0.2f:reachable?0.95f:0.48f);
        v.icon->PrepareForDraw();v.label->PrepareForDraw();v.state->PrepareForDraw();
    }
    for(auto& edge:expeditionMapEdges_) {
        auto a=position(*tankexp::FindMapNode(definition,edge.from)),b=position(*tankexp::FindMapNode(definition,edge.to));
        const float dx=b.x-a.x,dy=b.y-a.y;const float from=(std::clamp)((36-a.x)/dx,0.0f,1.0f),to=(std::clamp)((1244-a.x)/dx,0.0f,1.0f);
        b={a.x+dx*to,a.y+dy*to};a={a.x+dx*from,a.y+dy*from};
        const float length=std::sqrt((b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y));
        const bool chosen=expeditionMapRun_.HasVisited(edge.from)&&(expeditionMapRun_.HasVisited(edge.to)||expeditionMapRun_.CanSelectNode(edge.to));
        for(auto* s:{edge.glow.get(),edge.line.get()}) {s->SetPosition(a);s->SetSize({length,s==edge.line.get()?2.0f:7.0f});s->SetRotation(std::atan2(b.y-a.y,b.x-a.x));s->Update();}
        const bool focusPath=edge.to==expeditionMapSelection_||edge.from==expeditionMapSelection_;
        const bool abandoned=current&&!chosen&&(!future.contains(edge.to)||(!future.contains(edge.from)&&edge.from!=current->id));
        edge.line->SetColor(abandoned?Vector4{0.16f,0.24f,0.30f,0.10f}:chosen?Vector4{0.2f,1.1f,0.62f,0.85f}:focusPath?Vector4{0.25f,0.86f,0.85f,0.9f}:Vector4{0.24f,0.42f,0.56f,0.35f});
        edge.glow->SetColor({0.14f,0.70f,0.82f,chosen||focusPath?0.15f:0.03f});
        const float travel=std::fmod(expeditionPresentationClock_*0.60f,1.0f);
        edge.pulse->SetPosition({a.x+(b.x-a.x)*travel,a.y+(b.y-a.y)*travel});
        edge.pulse->SetColor({0.45f,1,1,(chosen||focusPath)&&length>1?0.85f:0});edge.pulse->Update();
    }
    for(int i=0;i<3;++i) {
        expeditionBlueprintLabels_[i]->SetText(i==0?"←":i==1?"→":" ");
        expeditionBlueprintButtons_[i]->SetPosition({i==0?46.0f:130.0f,620});expeditionBlueprintButtons_[i]->SetSize({68,44});expeditionBlueprintButtons_[i]->Update();
        expeditionBlueprintLabels_[i]->SetPosition({i==0?68.0f:152.0f,628});
        expeditionBlueprintButtons_[i]->SetColor(Inside(input_->GetMousePosition(),i==0?46.0f:130.0f,620,68,44)?Vector4{0.04f,0.21f,0.24f,1}:Vector4{0.025f,0.07f,0.11f,1});
        expeditionBlueprintLabels_[i]->PrepareForDraw();
    }
    for(auto* t:{expeditionMapTitle_.get(),expeditionMapSubtitle_.get(),expeditionMapLegend_.get(),expeditionMapInfo_.get(),expeditionMapHelp_.get()}) t->PrepareForDraw();
    if(service) {
        tankRunDescription_->PrepareForDraw();
        if(active->kind==NK::Heal) {tankRunCardTitles_[0]->PrepareForDraw();tankRunCardBodies_[0]->PrepareForDraw();}
    }
    std::string focus=service?active->id+":"+std::to_string(expeditionServicePage_)+":"+std::to_string(tankRunSelection_):expeditionMapSelection_;
    if(!service||expeditionServiceOffers_.size()>3) for(int i=0;i<2;++i) if(Inside(input_->GetMousePosition(),i==0?46.0f:130.0f,620,68,44)) focus="scroll:"+std::to_string(i);
    if(focus!=expeditionLastFocus_) {
        if(!expeditionLastFocus_.empty()&&!expeditionTransition_.IsActive()) tankExpeditionAudio_.UiHover();
        expeditionLastFocus_=focus;
    }
    RefreshExpeditionBuildCards();
}

void GameScene::DrawExpeditionMapUi() {
    const auto cardPresent=[this](int i) {
        const size_t index=static_cast<size_t>(expeditionServicePage_*3+i);
        return expeditionBuildChoice_||(index<expeditionServiceOffers_.size()&&tankcontent::FindUpgrade(expeditionContent_,expeditionServiceOffers_[index]));
    };
    if(IsExpeditionBuildCardScreen())for(int i=0;i<3;++i)if(cardPresent(i)&&expeditionRewardCards_[i])expeditionRewardCards_[i]->PreparePreviewRender();
    SpriteCommon::GetInstance()->PreDraw(kNormal);tankRunDimmer_->Draw();
    tankRunHudPanel_->Draw();tankRunHud_->Draw();tankExpeditionHpTrack_->Draw();tankExpeditionHpFill_->Draw();
    DrawExpeditionVitals();
    expeditionMapTitle_->Draw();expeditionMapSubtitle_->Draw();
    if(expeditionBuildChoice_) {
        tankRunDescription_->Draw();
        for(auto& card:expeditionRewardCards_) card->Draw();
        return;
    }
    const auto* active=expeditionMapRun_.GetActiveNode();
    const bool service=active&&!tankexp::IsCombatNode(active->kind)&&!expeditionMapPreview_;
    if(service) {
        tankRunDescription_->Draw();
        if(active->kind==NK::Currency) return;
        if(IsExpeditionBuildCardScreen()) {for(int i=0;i<3;++i)if(cardPresent(i))expeditionRewardCards_[i]->Draw();}
        else if(active->kind==NK::Heal) {
            const bool enabled=player_->GetHp()<player_->GetMaxHp()&&expeditionMapRun_.CanAfford(active->serviceCost);
            const float focus=enabled?expeditionCardFocus_[0]:0;
            tankRunCards_[0]->SetPosition({424,270});tankRunCards_[0]->SetSize({432,260});
            tankRunCards_[0]->SetColor(enabled?Vector4{0.028f+0.012f*focus,0.065f+0.105f*focus,0.075f+0.105f*focus,1}:Vector4{0.025f,0.035f,0.045f,1});
            tankRunCardTitles_[0]->SetPosition({446,292});tankRunCardBodies_[0]->SetPosition({446,338});
            tankRunCards_[0]->Update();tankRunCards_[0]->Draw();tankRunCardTitles_[0]->Draw();tankRunCardBodies_[0]->Draw();DrawCurrencyIcon({506,383},28);
            // These UI objects also serve the pause/result screens. Restore
            // their normal layout immediately after the dedicated repair draw.
            tankRunCards_[0]->SetPosition({64,280});tankRunCards_[0]->SetSize({368,280});
            tankRunCardTitles_[0]->SetPosition({80,303});tankRunCardBodies_[0]->SetPosition({80,362});
        }
        if(expeditionServiceOffers_.size()>3) for(int i=0;i<2;++i) {expeditionBlueprintButtons_[i]->Draw();expeditionBlueprintLabels_[i]->Draw();}
        if(active->kind!=NK::Currency) {expeditionSkipButton_->Draw();expeditionSkipText_->Draw();}
    } else {
        for(auto& s:expeditionMapGrid_) s->Draw();
        for(auto& e:expeditionMapEdges_) {e.glow->Draw();e.line->Draw();e.pulse->Draw();}
        std::vector<TextLabel*> completed;
        const auto& nodes=expeditionMapRun_.GetDefinition().nodes;
        for(size_t i=0;i<expeditionMapVisuals_.size();++i) {const auto& v=expeditionMapVisuals_[i];if(v.center.x>=66&&v.center.x<=1214&&expeditionMapRun_.HasVisited(nodes[i].id)) completed.push_back(v.icon.get());}
        expeditionCompleteGlow_->DrawBloom(completed);SpriteCommon::GetInstance()->PreDraw(kNormal);
        for(auto& n:expeditionMapVisuals_) if(n.center.x>=66&&n.center.x<=1214) {n.halo->Draw();n.rim->Draw();n.fill->Draw();n.icon->Draw();n.label->Draw();n.state->Draw();}
        expeditionMapLegend_->Draw();
        for(int i=0;i<2;++i) {expeditionBlueprintButtons_[i]->Draw();expeditionBlueprintLabels_[i]->Draw();}
        const auto drawPointer=[this,&nodes]() {
            for(size_t i=0;i<expeditionMapVisuals_.size();++i) if(expeditionMapRun_.CanSelectNode(nodes[i].id)) {
                const auto p=expeditionMapVisuals_[i].center;if(p.x>=66&&p.x<=1214) {DrawExpeditionPointer({p.x-35,p.y});}
            }
            return false;
        };
        drawPointer();
        DrawCurrencyIcon({58,604});
    }
    expeditionMapInfo_->Draw();expeditionMapHelp_->Draw();
}

void GameScene::UpdateExpeditionMap(float dt) {
    if(phase_!=Phase::kMain) return;
    if(UpdateSpecialValidation(dt)) return;
    if(UpdateExperienceValidation(dt)) return;
    if(UpdateCombatValidation(dt)) return;
    const int earnings=player_->TakeRunCurrencyEarned();if(earnings>0) SpawnExpeditionCredits(player_->GetWorldPosition(),earnings);
    if(expeditionAuthoringHubOpen_||tankExpeditionBalanceEditorOpen_||expeditionRoomEditorOpen_||expeditionMapEditorOpen_||expeditionContentEditorOpen_) return;
    const bool wasTransitioning=expeditionTransition_.IsActive();
    UpdateExpeditionPresentation(dt);
    if(wasTransitioning) {RefreshTankExpeditionUi();return;}
    tankRunMenuAge_+=dt;tankRunAutoTime_+=dt;
    if(expeditionMapAutoTest_) UpdateExpeditionMapValidation(dt);
    if(Press(input_,DIK_F10)) RequestTankRunCapture("map_manual");
    if(Press(input_,DIK_M)) {tankExpeditionMusicEnabled_=!tankExpeditionMusicEnabled_;tankExpeditionAudio_.SetMusicVolume(tankExpeditionMusicEnabled_?0.55f:0);}
    if(Press(input_,DIK_N)) {tankExpeditionEffectsEnabled_=!tankExpeditionEffectsEnabled_;tankExpeditionAudio_.SetEffectsVolume(tankExpeditionEffectsEnabled_?0.8f:0);}
    if(gameFlowState_!=GameFlowState::Playing) {UpdateExpeditionCredits(dt,true);RefreshTankExpeditionUi();return;}
    if(tankExpedition_.IsCombat()&&!tankRunPaused_) {
        if(Press(input_,DIK_G)) {expeditionMapPreview_=!expeditionMapPreview_;tankExpeditionDetailsOpen_=false;RefreshTankExpeditionUi();}
        if(Press(input_,DIK_TAB)) {tankExpeditionDetailsOpen_=!tankExpeditionDetailsOpen_;expeditionMapPreview_=false;RefreshTankExpeditionUi();}
    }
    if(Press(input_,DIK_ESCAPE)) {
        if(expeditionMapPreview_||tankExpeditionDetailsOpen_) {expeditionMapPreview_=false;tankExpeditionDetailsOpen_=false;return;}
        tankRunPaused_=!tankRunPaused_;tankRunSelection_=0;tankRunMenuAge_=0;expeditionLastFocus_.clear();RefreshTankExpeditionUi();return;
    }
    const auto mouse=input_->GetMousePosition();const auto motion=input_->GetMouseState();
    const bool click=input_->IsTrigger(motion.rgbButtons[0],input_->GetPreMouseState().rgbButtons[0]);
    if(tankRunPaused_) {
        int hovered=-1;
        for(int i=0;i<2;++i) if(Inside(mouse,64+i*388.0f,280,368,280)) hovered=i;
        const std::string focus="pause:"+std::to_string(hovered);
        if(focus!=expeditionLastFocus_) {
            if(hovered>=0) {tankRunSelection_=hovered;tankExpeditionAudio_.UiHover();}
            expeditionLastFocus_=focus;
        }
        if(hovered>=0&&(motion.lX||motion.lY||click)) tankRunSelection_=hovered;
        if(Press(input_,DIK_LEFT)||Press(input_,DIK_RIGHT)) {tankRunSelection_=1-tankRunSelection_;tankExpeditionAudio_.UiHover();}
        for(int i=0;i<2;++i) if(Press(input_,DIK_1+i)||(click&&Inside(mouse,64+i*388.0f,280,368,280))) {SelectTankRunOption(i);return;}
        if(Press(input_,DIK_RETURN)) SelectTankRunOption(tankRunSelection_);
        RefreshTankExpeditionUi();return;
    }
    if(tankExpeditionDetailsOpen_) return;
    UpdateGuidedExpedition(dt);
    UpdateExpeditionCredits(dt,expeditionCollectAll_);
    if(IsGuidedExpeditionPaused()) {RefreshTankExpeditionUi();return;}
    if(expeditionBuildChoice_) {
        if(tankRunMenuAge_>0.2f) {
            if(Press(input_,DIK_LEFT)) tankRunSelection_=(tankRunSelection_+2)%3;
            if(Press(input_,DIK_RIGHT)) tankRunSelection_=(tankRunSelection_+1)%3;
            for(int i=0;i<3;++i) {
                if(Inside(mouse,64+i*388.0f,260,368,330)&&(motion.lX||motion.lY)) tankRunSelection_=i;
                if(Press(input_,DIK_1+i)||(click&&Inside(mouse,64+i*388.0f,260,368,330))) {SelectExpeditionBuildStyle(i);return;}
            }
            if(Press(input_,DIK_RETURN)||Press(input_,DIK_SPACE)) {SelectExpeditionBuildStyle(tankRunSelection_);return;}
        }
        RefreshTankExpeditionUi();return;
    }
    if(expeditionMapPreview_||expeditionMapRun_.IsChoosing()) {
        const auto available=expeditionMapRun_.GetAvailableNodeIds();
        if(!expeditionMapPreview_&&tankRunMenuAge_>0.15f) {
            if((Press(input_,DIK_LEFT)||Press(input_,DIK_RIGHT))&&!available.empty()) {
                const auto it=std::find(available.begin(),available.end(),expeditionMapSelection_);
                int i=it==available.end()?0:static_cast<int>(std::distance(available.begin(),it));
                i=(i+(Press(input_,DIK_LEFT)?static_cast<int>(available.size())-1:1))%static_cast<int>(available.size());
                expeditionMapSelection_=available[i];
                const auto* n=tankexp::FindMapNode(expeditionMapRun_.GetDefinition(),expeditionMapSelection_);
                expeditionMapScroll_=(std::max)(0.0f,120.0f*(n->column-8));
            }
            bool enter=Press(input_,DIK_RETURN)||Press(input_,DIK_SPACE);
            for(size_t i=0;i<available.size();++i) if(Press(input_,DIK_1+static_cast<int>(i))) {expeditionMapSelection_=available[i];enter=true;}
            for(size_t i=0;i<expeditionMapVisuals_.size();++i) {
                const auto p=expeditionMapVisuals_[i].center;
                if(Inside(mouse,p.x-34,p.y-34,68,68)&&p.x>=66&&p.x<=1214) {
                    if(motion.lX||motion.lY||click) expeditionMapSelection_=expeditionMapRun_.GetDefinition().nodes[i].id;
                    if(click) enter=true;
                }
            }
            if(enter) {RequestExpeditionMapNode(expeditionMapSelection_);return;}
        }
        int lastColumn=0;for(const auto& n:expeditionMapRun_.GetDefinition().nodes) lastColumn=(std::max)(lastColumn,n.column);
        if(Press(input_,DIK_Q)||(click&&Inside(mouse,46,620,68,44))) {expeditionMapScroll_-=360;tankExpeditionAudio_.UiConfirm();}
        if(Press(input_,DIK_E)||(click&&Inside(mouse,130,620,68,44))) {expeditionMapScroll_+=360;tankExpeditionAudio_.UiConfirm();}
        if(motion.lZ) expeditionMapScroll_-=static_cast<float>(motion.lZ);
        expeditionMapScroll_=(std::clamp)(expeditionMapScroll_,0.0f,(std::max)(0.0f,120.0f*(lastColumn-9)));
        if(!expeditionMapPreview_) UpdateTankExpeditionTutorial(dt);
        RefreshTankExpeditionUi();return;
    }
    const auto* active=expeditionMapRun_.GetActiveNode();
    if(active&&!tankexp::IsCombatNode(active->kind)) {
        if(active->kind==NK::Currency) {
            if(expeditionCredits_.empty()) {
                expeditionMapRun_.CompleteCurrencyGrant(false);expeditionCollectAll_=false;
                const auto available=expeditionMapRun_.GetAvailableNodeIds();if(!available.empty()) expeditionMapSelection_=available.front();
                tankRunMenuAge_=0;
            }
            RefreshTankExpeditionUi();return;
        }
        if(active->kind==NK::Heal) {
            if(tankRunMenuAge_>0.15f) {
                if(Press(input_,DIK_LEFT)||Press(input_,DIK_RIGHT)||Press(input_,DIK_UP)||Press(input_,DIK_DOWN)) tankRunSelection_=tankRunSelection_==0?3:0;
                const bool repairHovered=Inside(mouse,424,270,432,260),skipHovered=Inside(mouse,490,560,300,52);
                if(motion.lX||motion.lY) {if(repairHovered)tankRunSelection_=0;else if(skipHovered)tankRunSelection_=3;}
                if(Press(input_,DIK_1)||(click&&repairHovered)) {SelectExpeditionService(0);return;}
                if(Press(input_,DIK_2)||(click&&skipHovered)) {SelectExpeditionService(3);return;}
                if(Press(input_,DIK_RETURN)||Press(input_,DIK_SPACE)) {SelectExpeditionService(tankRunSelection_==3?3:0);return;}
            }
            RefreshTankExpeditionUi();return;
        }
        if(tankRunMenuAge_>0.15f) {
            const int pages=(std::max)(1,(static_cast<int>(expeditionServiceOffers_.size())+2)/3);
            if(Press(input_,DIK_Q)||(click&&Inside(mouse,46,620,68,44))) expeditionServicePage_=(expeditionServicePage_+pages-1)%pages;
            if(Press(input_,DIK_E)||(click&&Inside(mouse,130,620,68,44))) expeditionServicePage_=(expeditionServicePage_+1)%pages;
            if(click&&Inside(mouse,966,620,242,44)) {SelectExpeditionService(3);return;}
            std::vector<int> selectable;
            for(int i=0;i<3;++i) {
                const size_t index=static_cast<size_t>(expeditionServicePage_*3+i);
                if(index>=expeditionServiceOffers_.size()||!tankcontent::FindUpgrade(expeditionContent_,expeditionServiceOffers_[index])) continue;
                selectable.push_back(i);
                if(Inside(mouse,64+i*388.0f,260,368,330)&&(motion.lX||motion.lY)) tankRunSelection_=i;
                if(Press(input_,DIK_1+i)||(click&&Inside(mouse,64+i*388.0f,260,368,330))) {SelectExpeditionService(i);return;}
            }
            selectable.push_back(3);
            if(std::find(selectable.begin(),selectable.end(),tankRunSelection_)==selectable.end())tankRunSelection_=selectable.front();
            if(Press(input_,DIK_LEFT)||Press(input_,DIK_RIGHT)) {
                const int selected=static_cast<int>(std::find(selectable.begin(),selectable.end(),tankRunSelection_)-selectable.begin());
                const int count=static_cast<int>(selectable.size());
                tankRunSelection_=selectable[(selected+(Press(input_,DIK_LEFT)?count-1:1))%count];
            }
            if(Press(input_,DIK_RETURN)||Press(input_,DIK_SPACE)) {SelectExpeditionService(tankRunSelection_);return;}
        }
        UpdateTankExpeditionTutorial(dt);RefreshTankExpeditionUi();return;
    }
    if(tankExpedition_.IsCombat()) {
        UpdateTankExpeditionTutorial(dt);
        const bool ready=!expeditionGuideActive_||expeditionGuide_.IsCombatReadyToClear()||expeditionMapAutoTest_;
        const auto kind=tankExpedition_.GetRoomKind();
        if(ready&&kind!=tankexp::RoomKind::Boss&&ThreatCount(enemyManager_.get())==0&&
            (kind!=tankexp::RoomKind::Guard||tankExpeditionNodes_>=3)) {
            const auto* cleared=expeditionMapRun_.GetActiveNode();
            if(!expeditionClearRewardQueued_) {
                if(cleared) SpawnExpeditionCredits(player_->GetWorldPosition(),cleared->clearReward,true);
                expeditionClearRewardQueued_=true;expeditionCollectAll_=true;
            }
            if(!expeditionCredits_.empty()) {RefreshTankExpeditionUi();return;}
            expeditionCollectAll_=false;
            BeginExpeditionPresentation(2,"戦闘クリア",
                cleared?"通貨を回収しました / 次の地点へ":"次の目的地を選ぼう",{0.25f,1,0.72f,1});
            return;
        }
        tankExpedition_.Update(dt);tankRun_.Update(dt);tankExpeditionArrival_+=dt;
        tankRunComboTime_=(std::max)(0.0f,tankRunComboTime_-dt);
        for(auto& burst:tankRunBursts_) burst.age+=dt;
        std::erase_if(tankRunBursts_,[](const RunBurst& b){return b.age>(b.resource?0.7f:0.35f);});
    }
    tankRunHudTimer_-=dt;if(tankRunHudTimer_<=0) {RefreshTankExpeditionUi();tankRunHudTimer_=0.10f;}
}

void GameScene::UpdateExpeditionMapValidation(float dt) {
    expeditionMapTestElapsed_+=dt;
    const std::string state=expeditionMapRun_.IsChoosing()?"map_"+expeditionMapRun_.GetCurrentNodeId():expeditionMapRun_.GetActiveNodeId();
    if(state!=expeditionMapTestState_) {expeditionMapTestState_=state;expeditionMapTestAge_=0;}
    expeditionMapTestAge_+=dt;
    if(expeditionMapTestAge_>0.4f&&std::find(expeditionMapTestVisited_.begin(),expeditionMapTestVisited_.end(),state)==expeditionMapTestVisited_.end()&&!state.empty()) {
        expeditionMapTestVisited_.push_back(state);tankRunCapturePath_="generated/expedition_map/"+state+".png";
    }
    if(gameFlowState_==GameFlowState::StageClear||expeditionMapTestElapsed_>140) {
        if(gameFlowState_==GameFlowState::StageClear&&expeditionMapTestAge_<1.0f) return;
        const bool success=expeditionMapRun_.IsComplete()&&player_->GetLevel()==1&&player_->GetExp()==0&&expeditionMapTestPurchases_>0&&expeditionMapTestEvolutions_>0&&expeditionMapTestHeals_>0;
        nlohmann::json report={{"completed",success},{"testMode",true},{"forcedCombatClear",true},{"elapsed",expeditionMapTestElapsed_},
            {"visited",expeditionMapRun_.GetVisitedNodeIds()},{"credits",expeditionMapRun_.GetCurrency()},
            {"level",player_->GetLevel()},{"experience",player_->GetExp()},{"purchases",expeditionMapTestPurchases_},
            {"refits",0},{"evolutions",0},{"additiveGrowthPurchases",expeditionMapTestEvolutions_},{"repairs",expeditionMapTestHeals_},{"class",player_->GetCurrentClassName()},
            {"cards",tankRun_.GetCardCounts()},{"roomTemplates",expeditionRooms_.rooms.size()},
            {"upgradeTypes",expeditionContent_.upgrades.size()},{"enemyTypes",expeditionContent_.enemies.size()},{"playerTypes",expeditionContent_.players.size()}};
        std::ofstream("generated/expedition_map/validation.json")<<std::setw(2)<<report<<'\n';PostQuitMessage(success?0:4);return;
    }
    if(expeditionMapTestAge_<0.9f||!tankRunCapturePath_.empty()) return;
    if(expeditionBuildChoice_) {SelectExpeditionBuildStyle(0);return;}
    if(expeditionMapRun_.IsChoosing()) {
        auto options=expeditionMapRun_.GetAvailableNodeIds();if(options.empty()) return;
        std::string id=options.front();
        // Exercise repair and sequential additive upgrades through real transactions.
        for(const auto& choice:options) if(choice=="field_repair"||choice=="arsenal"||choice=="final_upgrade") id=choice;
        RequestExpeditionMapNode(id);return;
    }
    const auto* node=expeditionMapRun_.GetActiveNode();if(!node) return;
    if(tankexp::IsCombatNode(node->kind)) {
        if(expeditionMapTestAge_>2.0f) {
            for(auto* e:enemyManager_->GetEnemyPtrs()) if(e&&!e->IsDead()) e->TakeDamageFromPlayer(100000);
            if(node->kind==NK::Boss) enemy_->TakeDamage(100000);
        }
    } else if(node->kind==NK::Heal) {player_->SpendRunHealth(20);SelectExpeditionService(0);}
    else if(node->kind==NK::Upgrade) {
        // Keep the transaction deterministic while checking cumulative equipment.
        for(const char* id:{"ExtraBarrel1","ExtraBarrel2","FanMount","AlternatingFire"}) {
            const auto* u=tankcontent::FindUpgrade(expeditionContent_,id);
            if(u&&tankcontent::EligibleUpgrade(*u,expeditionBuildStyle_,tankRun_.GetCardCounts())) {
                expeditionServiceOffers_={u->id};RefreshExpeditionBuildCards();SelectExpeditionService(0);return;
            }
        }
        SelectExpeditionService(0);
    }
    else SelectExpeditionService(0);
}
