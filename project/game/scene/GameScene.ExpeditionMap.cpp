#include "GameScene.h"
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
    case NK::Upgrade:return {1,0.83f,0.27f,1};case NK::Evolution:return {0.79f,0.49f,1,1};
    case NK::Heal:return {0.27f,1,0.63f,1};default:return {1,0.25f,0.45f,1};
    }
}
const char* NodeIcon(NK kind) {
    switch(kind) {case NK::Combat:return "戦";case NK::Elite:return "強";case NK::Upgrade:return "改";
    case NK::Evolution:return "進";case NK::Heal:return "+";default:return "核";}
}
const char* NodeName(NK kind) {
    switch(kind) {case NK::Combat:return "戦闘";case NK::Elite:return "精鋭戦闘";case NK::Upgrade:return "改造工房";
    case NK::Evolution:return "機体進化";case NK::Heal:return "修理ステーション";default:return "最終決戦";}
}
std::string ShortMapName(const std::string& name) {
    size_t pos=0;int chars=0;
    while(pos<name.size()&&chars<8) {const auto c=static_cast<unsigned char>(name[pos]);pos+=c<128?1:c<224?2:c<240?3:4;++chars;}
    return pos<name.size()?name.substr(0,pos)+"…":name;
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
    expeditionMapEnabled_=true;
    expeditionMapDefinition_=tankexp::DefaultExpeditionMap();expeditionRooms_=tankexp::DefaultRoomCatalog();
    expeditionContent_=tankcontent::DefaultCatalog();std::string error;
    if(!tankexp::LoadExpeditionMap(tankexp::kExpeditionMapPath,expeditionMapDefinition_,error)) expeditionMapStatus_=error;
    if(!tankexp::LoadRoomCatalog(tankexp::kRoomCatalogPath,expeditionRooms_,error)) expeditionMapStatus_=error;
    if(!tankcontent::LoadCatalog("resources/configs/expedition_content.json",expeditionContent_,error)) expeditionMapStatus_=error;
    if(sessionMap) expeditionMapDefinition_=*sessionMap;
    if(sessionRooms) expeditionRooms_=*sessionRooms;
    if(sessionContent) expeditionContent_=*sessionContent;
    expeditionContentEditor_.Open(expeditionContent_);
    expeditionMapRun_.Reset(expeditionMapDefinition_,error);
    enemyManager_->SetExpeditionContent(expeditionContent_);player_->InstallRunAuthoredClasses(expeditionContent_);
    SetExpeditionBlueprint(0);player_->SetRunCurrencyMode(true);tankExpedition_.OpenMap();
    expeditionMapSelection_=expeditionMapRun_.GetAvailableNodeIds().front();
    const Vector4 white{0.83f,0.96f,1,1},muted{0.42f,0.64f,0.75f,1};
    expeditionMapTitle_=MapLabel(32,{44,85},white);
    expeditionMapSubtitle_=MapLabel(16,{46,133},muted);
    expeditionMapLegend_=MapLabel(14,{46,195},muted);
    expeditionMapInfo_=MapLabel(18,{46,586},white);
    expeditionMapHelp_=MapLabel(13,{46,695},muted);
    for(int x=36;x<1260;x+=36) expeditionMapGrid_.push_back(MapRect({static_cast<float>(x),224},{1,350},{0.08f,0.26f,0.35f,0.13f}));
    for(int y=224;y<=574;y+=35) expeditionMapGrid_.push_back(MapRect({36,static_cast<float>(y)},{1208,1},{0.08f,0.26f,0.35f,0.13f}));
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
            edge.glow->SetAnchorPoint({0,0.5f});edge.line->SetAnchorPoint({0,0.5f});expeditionMapEdges_.push_back(std::move(edge));
        }
    }
    const char* plans[]={"CYCLE 01 / 跳弾設計","CYCLE 02 / 突撃設計","CYCLE 03 / 群体設計"};
    for(int i=0;i<3;++i) {
        const float x=46.0f+i*394.0f;expeditionBlueprintButtons_[i]=MapRect({x,650},{378,34},{0.035f,0.11f,0.16f,1});
        expeditionBlueprintLabels_[i]=MapLabel(15,{x+12,654},white);expeditionBlueprintLabels_[i]->SetText(plans[i]);
    }
    wchar_t flag[8]{};expeditionMapAutoTest_=GetEnvironmentVariableW(L"CG2_TANK_MAP_AUTOTEST",flag,8)>0&&flag[0]==L'1';
    if(expeditionMapAutoTest_) {
        debugPlayerNoDamage_=true;tankExpeditionTutorial_.Skip();
        std::filesystem::create_directories("generated/expedition_map");
        std::ofstream("generated/expedition_map/validation.json")<<"{\"completed\":false}\n";
    }
    RefreshExpeditionMapUi();
}

void GameScene::SetExpeditionBlueprint(int index) {
    if(index<0||index>2||!expeditionMapRun_.GetChosenNodeIds().empty()) return;
    expeditionBlueprint_=index;
    tankrun::Config config;config.combatSeconds=1000000;
    tankRun_=tankrun::RunDirector(static_cast<uint32_t>(GetTickCount64()),config);
    tankRun_.ChooseLoadout(index);player_->ConfigurePrototypeLoadout(index);tankRun_.ChooseCore(index);ApplyTankRunCards();
}

void GameScene::UpdateExpeditionAuthoring() {
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
    if(expeditionRoomEditor_.Draw(&expeditionRoomEditorOpen_,expeditionRooms_,enemies)) {sessionRooms=expeditionRooms_;expeditionMapStatus_="配置を適用しました。次の区画への入場時に反映します。";}
    if(expeditionMapEditor_.Draw(expeditionMapEditorOpen_,expeditionMapDefinition_,rooms)) {sessionMap=expeditionMapDefinition_;expeditionMapStatus_="作戦マップを適用しました。次の遠征から反映します。";}
    if(expeditionContentEditor_.Draw(expeditionContentEditorOpen_,expeditionContent_)) {
        enemyManager_->SetExpeditionContent(expeditionContent_);player_->InstallRunAuthoredClasses(expeditionContent_);
        sessionContent=expeditionContent_;
        if(const auto* active=expeditionMapRun_.GetActiveNode();active&&active->kind==NK::Evolution) {
            tankExpeditionEvolutions_=player_->GetRunEvolutionChoices();const auto custom=player_->GetRunAuthoredEvolutionChoices();
            tankExpeditionEvolutions_.insert(tankExpeditionEvolutions_.end(),custom.begin(),custom.end());
        }
        expeditionMapStatus_="種類を適用しました。次の敵出現・工房・進化で選択できます。";RefreshExpeditionServiceOffers();
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
            expeditionMapStatus_="敵の種類がありません: "+spawn.type+" / F6で追加してください。";return;
        }
        if((node->kind==NK::Boss)!=(room->objective=="boss")) {
            expeditionMapStatus_="最終決戦ノードにはボス目標の部屋、それ以外には通常目標の部屋を指定してください。";return;
        }
    }
    const auto before=expeditionMapRun_;const auto beforeDirector=tankExpedition_;
    if(!expeditionMapRun_.SelectNode(id)) return;
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
        if(node->kind==NK::Evolution) {
            player_->PrepareRunEvolution();tankExpeditionEvolutions_=player_->GetRunEvolutionChoices();
            const auto custom=player_->GetRunAuthoredEvolutionChoices();tankExpeditionEvolutions_.insert(tankExpeditionEvolutions_.end(),custom.begin(),custom.end());
        }
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
        auto progress=enemy_->GetEnemyProgressConfig();progress.levelingModeEnabled=false;enemy_->SetEnemyProgressConfig(progress);
        screenEffectDirector_.TriggerBossEntry();
    }
    // Apply shared boss pressure, then restore per-prefab parameters after the
    // global balancing pass (otherwise authored enemy contact damage is lost).
    ApplyTankExpeditionRoomBalance();
    previousPlayerHp_=player_->GetHp();previousBossHp_=enemy_->GetHp();bossDefeatHandled_=false;
    SetEventCallout(node->label+" / "+(room->objective=="control"?"制御装置を3つ確保":room->objective=="boss"?"最深部のライバルを撃破":"敵を全滅"),1.4f);
    return true;
}

void GameScene::CompleteExpeditionMapCombat() {
    if(!tankExpedition_.IsCombat()||player_->IsDead()) return;
    const auto* node=expeditionMapRun_.GetActiveNode();if(!node||node->kind==NK::Boss) return;
    const int reward=node->clearReward;
    expeditionMapRun_.EarnCurrency(player_->TakeRunCurrencyEarned());
    if(!expeditionMapRun_.CompleteCombat()) return;
    tankExpedition_.OpenMap();tankExpeditionTutorial_.RecordRoomClear();
    expeditionMapSelection_=expeditionMapRun_.GetAvailableNodeIds().front();
    expeditionMapStatus_="区画突破 / +"+std::to_string(reward)+" Cr を回収。光る地点から次の目的地を選べます。";
    if(const auto* next=tankexp::FindMapNode(expeditionMapRun_.GetDefinition(),expeditionMapSelection_)) expeditionMapScroll_=(std::max)(0.0f,120.0f*(next->column-8));
    tankExpeditionAudio_.Upgrade();tankRunMenuAge_=0;RefreshTankExpeditionUi();
}

void GameScene::RefreshExpeditionServiceOffers() {
    expeditionServiceOffers_.clear();const auto* node=expeditionMapRun_.GetActiveNode();if(!node) return;
    if(node->kind==NK::Upgrade) for(const auto& upgrade:expeditionContent_.upgrades) {
        bool useful=false;for(const auto effect:upgrade.effects) useful|=tankRun_.GetCardCount(effect)==0;
        if(useful&&expeditionPurchases_[upgrade.id]<upgrade.maxPurchases) expeditionServiceOffers_.push_back(upgrade.id);
    }
    if(node->kind==NK::Evolution) for(const auto& evolution:tankExpeditionEvolutions_) expeditionServiceOffers_.push_back(evolution.id);
    expeditionServicePage_=(std::clamp)(expeditionServicePage_,0,(std::max)(0,(static_cast<int>(expeditionServiceOffers_.size())-1)/2));
}

void GameScene::SelectExpeditionService(int option) {
    const auto* node=expeditionMapRun_.GetActiveNode();if(!node||tankexp::IsCombatNode(node->kind)||option<0||option>2) return;
    bool complete=false;
    if(option==2) complete=expeditionMapRun_.CompleteService(false);
    else if(node->kind==NK::Heal) {
        if(option!=0) return;
        if(player_->GetHp()>=player_->GetMaxHp()) {expeditionMapStatus_="装甲は完全です。通貨を使わず「次へ」で進めます。";return;}
        const int cost=node->serviceCost;
        if(!expeditionMapRun_.CompleteService(true)) {expeditionMapStatus_="通貨が不足しています。購入せず次へ進めます。";return;}
        player_->HealRunPlayer((std::max)(1,player_->GetMaxHp()/2));previousPlayerHp_=player_->GetHp();
        expeditionMapStatus_="装甲を50%修理 / -"+std::to_string(cost)+" Cr";complete=true;++expeditionMapTestHeals_;
    } else {
        const size_t index=static_cast<size_t>(expeditionServicePage_*2+option);if(index>=expeditionServiceOffers_.size()) return;
        const auto id=expeditionServiceOffers_[index];
        if(node->kind==NK::Upgrade) {
            const auto* upgrade=tankcontent::FindUpgrade(expeditionContent_,id);if(!upgrade) return;
            const int price=(std::max)(upgrade->price,node->serviceCost);
            if(!expeditionMapRun_.CanAfford(price)) {expeditionMapStatus_="通貨不足 / 必要 "+std::to_string(price)+" Cr。別の改造を選ぶか次へ進めます。";return;}
            const bool repairNew=tankRun_.GetCardCount(tankrun::CardId::Repair)==0;
            if(!tankRun_.GrantExpeditionModules(upgrade->effects)) {expeditionMapStatus_="この改造の効果はすでにすべて装備済みです。";return;}
            expeditionMapRun_.TrySpendCurrency(price);++expeditionPurchases_[id];++expeditionMapTestPurchases_;
            ApplyTankRunCards();tankExpeditionTutorial_.RecordUpgrade();
            if(repairNew&&std::find(upgrade->effects.begin(),upgrade->effects.end(),tankrun::CardId::Repair)!=upgrade->effects.end()) player_->HealRunPlayer(30);
            expeditionMapStatus_=upgrade->name+"を装備 / -"+std::to_string(price)+" Cr";
            complete=expeditionMapRun_.CompleteService(false);
        } else if(node->kind==NK::Evolution) {
            const auto* authored=tankcontent::FindPlayer(expeditionContent_,id);
            const int price=(std::max)(authored?authored->price:0,node->serviceCost);
            if(!expeditionMapRun_.CanAfford(price)) {expeditionMapStatus_="進化に必要な通貨が不足しています。";return;}
            const bool changed=authored?player_->ChooseRunAuthoredClass(id):player_->ChooseRunEvolution(id);
            if(!changed) {expeditionMapStatus_="この機体へは進化できませんでした。";return;}
            expeditionMapRun_.TrySpendCurrency(price);complete=expeditionMapRun_.CompleteService(false);++expeditionMapTestEvolutions_;
            expeditionMapStatus_="機体を進化しました。取得済みの改造と現在HPを引き継ぎます。";
        }
    }
    if(complete) {
        tankExpeditionAudio_.Upgrade();tankRunSelection_=0;tankRunMenuAge_=0;
        const auto next=expeditionMapRun_.GetAvailableNodeIds();if(!next.empty()) expeditionMapSelection_=next.front();
        if(const auto* focus=tankexp::FindMapNode(expeditionMapRun_.GetDefinition(),expeditionMapSelection_)) expeditionMapScroll_=(std::max)(0.0f,120.0f*(focus->column-8));
    }
    RefreshTankExpeditionUi();
}

void GameScene::RefreshExpeditionMapUi() {
    if(!expeditionMapTitle_) return;
    const auto& definition=expeditionMapRun_.GetDefinition();
    const auto* active=expeditionMapRun_.GetActiveNode();
    const bool service=active&&!tankexp::IsCombatNode(active->kind)&&!expeditionMapPreview_&&!tankRunPaused_;
    expeditionMapTitle_->SetText(service?active->label:"NEON FRONTIER / 作戦マップ");
    const std::string funds="SALVAGE  "+std::to_string(expeditionMapRun_.GetCurrency())+" Cr";
    expeditionMapSubtitle_->SetText(service?funds+"   /   購入は1地点につき1回。改造と進化は今回の遠征中ずっと有効です。":
        funds+"   /   戦闘で回収 → 改造・進化・修理へ。光る地点を選んで最深部を目指そう。");
    expeditionMapHelp_->SetText(service?"1〜3 / クリックで選択   Q・E / 候補ページ   ESC / 一時停止     F2 数値   F4 配置   F5 進路   F6 種類":
        expeditionMapPreview_?"G / ESC で戦闘へ戻る   Q・E / ホイールで横移動   マップ表示中は戦闘が停止します":
        "光る地点をクリック / ← → + Enter / 数字キー   Q・E / 横移動   F2 数値   F4 配置   F5 進路   F6 種類");
    expeditionMapLegend_->SetText("戦  戦闘     強  精鋭     改  改造     進  進化     +  修理     核  最終決戦       ✓ 完了  /  GO 選択可能");
    if(service) {
        tankRunDescription_->SetPosition({64,209});
        tankRunDescription_->SetText(active->kind==NK::Heal?"次の戦いに備え、最大HPの50%を修理できます。満タンなら支払い不要です。":
            active->kind==NK::Evolution?"機体の射撃構成を変更します。現在HP・取得した改造・残り通貨は引き継ぎます。":
            "装備する改造を選びます。Q / E で全候補を見られます。所持済みの効果は重複しません。");
        for(int i=0;i<2;++i) {
            std::string title="候補なし",body="ほかのページを確認するか、\n次の地点へ進みましょう。";
            int price=0;
            const size_t index=static_cast<size_t>(expeditionServicePage_*2+i);
            if(active->kind==NK::Heal&&i==0) {title="装甲を修理";price=active->serviceCost;body="最大HPの50%を回復\n\n現在HP "+std::to_string(player_->GetHp())+" / "+std::to_string(player_->GetMaxHp());}
            else if(index<expeditionServiceOffers_.size()) {
                const auto& id=expeditionServiceOffers_[index];
                if(active->kind==NK::Upgrade) {
                    const auto* u=tankcontent::FindUpgrade(expeditionContent_,id);
                    if(u) {title=(u->rarity?"RARE / ":"")+u->name;price=(std::max)(u->price,active->serviceCost);body=u->description;}
                } else if(active->kind==NK::Evolution) {
                    const auto* p=tankcontent::FindPlayer(expeditionContent_,id);
                    if(p) {title=p->name;price=(std::max)(p->price,active->serviceCost);body=p->description;}
                    else for(const auto& evolution:tankExpeditionEvolutions_) if(evolution.id==id) {title=evolution.name;body=evolution.description;price=active->serviceCost;break;}
                }
            }
            const std::string legacyMaintenance="・整備";
            if(const auto at=body.find(legacyMaintenance);at!=std::string::npos) body.erase(at,legacyMaintenance.size());
            tankRunCardTitles_[i]->SetText(WrapMapText(std::to_string(i+1)+"  "+title,15.5f,2));
            tankRunCardBodies_[i]->SetText(WrapMapText(std::to_string(price)+" Cr"+(expeditionMapRun_.CanAfford(price)?"":"  / 通貨不足")+"\n\n"+body,17.5f,8));
        }
        tankRunCardTitles_[2]->SetText("3  購入せず次へ");tankRunCardBodies_[2]->SetText("0 Cr\n\n今の構成で先へ進む。\n残り通貨は次の地点で使えます。");
        const int pages=(std::max)(1,(static_cast<int>(expeditionServiceOffers_.size())+1)/2);
        expeditionMapInfo_->SetText(expeditionMapStatus_+"\n候補 "+std::to_string(expeditionServicePage_+1)+" / "+std::to_string(pages)+" ページ  [Q / E]");
    } else {
        const auto* selected=tankexp::FindMapNode(definition,expeditionMapSelection_);
        std::string info;
        if(selected) {
            info=selected->label+" / "+NodeName(selected->kind);
            info+=tankexp::IsCombatNode(selected->kind)?"   突破報酬 +"+std::to_string(selected->clearReward)+" Cr":
                selected->kind==NK::Upgrade?"   改造 "+std::to_string(selected->serviceCost)+" Cr〜":
                selected->kind==NK::Evolution?"   基本進化 "+std::to_string(selected->serviceCost)+" Cr / 追加機体は個別価格":
                "   HP50%修理 / "+std::to_string(selected->serviceCost)+" Cr";
            if(!expeditionMapRun_.CanSelectNode(selected->id)&&!expeditionMapPreview_) info+="   / まだ選択できません";
        }
        expeditionMapInfo_->SetText(info+"\n"+expeditionMapStatus_);
    }
    auto position=[this](const tankexp::MapNode& n){return Vector2{90+120.0f*n.column-expeditionMapScroll_,250+70.0f*n.row};};
    const auto available=expeditionMapRun_.GetAvailableNodeIds();
    for(size_t i=0;i<definition.nodes.size()&&i<expeditionMapVisuals_.size();++i) {
        const auto& n=definition.nodes[i];auto& v=expeditionMapVisuals_[i];v.center=position(n);
        const bool reachable=expeditionMapRun_.CanSelectNode(n.id),visited=expeditionMapRun_.HasVisited(n.id);
        const bool selected=n.id==expeditionMapSelection_;
        auto tint=NodeColor(n.kind);tint.w=reachable||selected?1.0f:visited?0.75f:0.29f;
        v.rim->SetColor(tint);auto halo=tint;halo.w=selected?0.22f:reachable?0.13f:0.02f;v.halo->SetColor(halo);
        v.fill->SetColor(visited?Vector4{0.03f,0.14f,0.16f,1}:Vector4{0.016f,0.040f,0.065f,1});
        for(auto* s:{v.halo.get(),v.rim.get(),v.fill.get()}) {s->SetPosition(v.center);s->Update();}
        v.icon->SetText(visited?"✓":NodeIcon(n.kind));v.icon->SetPosition({v.center.x,v.center.y-2});v.icon->SetAlpha(reachable||visited||selected?1.0f:0.38f);
        v.label->SetText(ShortMapName(n.label));v.label->SetPosition({v.center.x,v.center.y+35});v.label->SetAlpha(reachable||visited||selected?1.0f:0.5f);
        auto at=std::find(available.begin(),available.end(),n.id);
        v.state->SetText(reachable?"GO  ["+std::to_string(std::distance(available.begin(),at)+1)+"]":visited?"CLEAR":selected?"PREVIEW":" ");
        v.state->SetPosition({v.center.x,v.center.y-54});
        v.icon->PrepareForDraw();v.label->PrepareForDraw();v.state->PrepareForDraw();
    }
    for(auto& edge:expeditionMapEdges_) {
        auto a=position(*tankexp::FindMapNode(definition,edge.from)),b=position(*tankexp::FindMapNode(definition,edge.to));
        const float dx=b.x-a.x,dy=b.y-a.y;const float from=(std::clamp)((36-a.x)/dx,0.0f,1.0f),to=(std::clamp)((1244-a.x)/dx,0.0f,1.0f);
        b={a.x+dx*to,a.y+dy*to};a={a.x+dx*from,a.y+dy*from};
        const float length=std::sqrt((b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y));
        const bool chosen=expeditionMapRun_.HasVisited(edge.from)&&(expeditionMapRun_.HasVisited(edge.to)||expeditionMapRun_.CanSelectNode(edge.to));
        for(auto* s:{edge.glow.get(),edge.line.get()}) {s->SetPosition(a);s->SetSize({length,s==edge.line.get()?2.0f:7.0f});s->SetRotation(std::atan2(b.y-a.y,b.x-a.x));s->Update();}
        edge.line->SetColor(chosen?Vector4{0.25f,0.86f,0.85f,0.9f}:Vector4{0.24f,0.42f,0.56f,0.35f});
        edge.glow->SetColor({0.14f,0.70f,0.82f,chosen?0.12f:0.03f});
    }
    for(int i=0;i<3;++i) {
        const char* plans[]={"CYCLE 01 / 跳弾設計","CYCLE 02 / 突撃設計","CYCLE 03 / 群体設計"};
        expeditionBlueprintLabels_[i]->SetText(service?(i==0?"← 前の候補 [Q]":i==1?"次の候補 [E] →":" "):plans[i]);
        expeditionBlueprintButtons_[i]->SetColor(i==expeditionBlueprint_?Vector4{0.04f,0.21f,0.24f,1}:Vector4{0.025f,0.07f,0.11f,1});
        expeditionBlueprintLabels_[i]->PrepareForDraw();
    }
    for(auto* t:{expeditionMapTitle_.get(),expeditionMapSubtitle_.get(),expeditionMapLegend_.get(),expeditionMapInfo_.get(),expeditionMapHelp_.get()}) t->PrepareForDraw();
    if(service) {tankRunDescription_->PrepareForDraw();for(int i=0;i<3;++i) {tankRunCardTitles_[i]->PrepareForDraw();tankRunCardBodies_[i]->PrepareForDraw();}}
}

void GameScene::DrawExpeditionMapUi() {
    SpriteCommon::GetInstance()->PreDraw(kNormal);tankRunDimmer_->Draw();
    tankRunHudPanel_->Draw();tankRunHud_->Draw();tankExpeditionHpTrack_->Draw();tankExpeditionHpFill_->Draw();
    expeditionMapTitle_->Draw();expeditionMapSubtitle_->Draw();
    const auto* active=expeditionMapRun_.GetActiveNode();
    const bool service=active&&!tankexp::IsCombatNode(active->kind)&&!expeditionMapPreview_;
    if(service) {
        tankRunDescription_->Draw();
        for(int i=0;i<3;++i) {
            tankRunCards_[i]->SetColor(i==tankRunSelection_?Vector4{0.04f,0.17f,0.20f,1}:Vector4{0.028f,0.045f,0.075f,1});
            tankRunCards_[i]->Update();tankRunCards_[i]->Draw();tankRunCardTitles_[i]->Draw();tankRunCardBodies_[i]->Draw();
        }
        if(expeditionServiceOffers_.size()>2) for(int i=0;i<2;++i) {expeditionBlueprintButtons_[i]->Draw();expeditionBlueprintLabels_[i]->Draw();}
    } else {
        for(auto& s:expeditionMapGrid_) s->Draw();
        for(auto& e:expeditionMapEdges_) {e.glow->Draw();e.line->Draw();}
        for(auto& n:expeditionMapVisuals_) if(n.center.x>=66&&n.center.x<=1214) {n.halo->Draw();n.rim->Draw();n.fill->Draw();n.icon->Draw();n.label->Draw();n.state->Draw();}
        expeditionMapLegend_->Draw();
        if(expeditionMapRun_.GetChosenNodeIds().empty()) for(int i=0;i<3;++i) {expeditionBlueprintButtons_[i]->Draw();expeditionBlueprintLabels_[i]->Draw();}
    }
    expeditionMapInfo_->Draw();expeditionMapHelp_->Draw();
}

void GameScene::UpdateExpeditionMap(float dt) {
    if(phase_!=Phase::kMain) return;
    const int earnings=player_->TakeRunCurrencyEarned();if(earnings>0) expeditionMapRun_.EarnCurrency(earnings);
    if(tankExpeditionBalanceEditorOpen_||expeditionRoomEditorOpen_||expeditionMapEditorOpen_||expeditionContentEditorOpen_) return;
    tankRunMenuAge_+=dt;tankRunAutoTime_+=dt;
    if(expeditionMapAutoTest_) UpdateExpeditionMapValidation(dt);
    if(Press(input_,DIK_F10)) RequestTankRunCapture("map_manual");
    if(Press(input_,DIK_M)) {tankExpeditionMusicEnabled_=!tankExpeditionMusicEnabled_;tankExpeditionAudio_.SetMusicVolume(tankExpeditionMusicEnabled_?0.55f:0);}
    if(Press(input_,DIK_N)) {tankExpeditionEffectsEnabled_=!tankExpeditionEffectsEnabled_;tankExpeditionAudio_.SetEffectsVolume(tankExpeditionEffectsEnabled_?0.8f:0);}
    if(gameFlowState_!=GameFlowState::Playing) {RefreshTankExpeditionUi();return;}
    if(tankExpedition_.IsCombat()&&!tankRunPaused_) {
        if(Press(input_,DIK_G)) {expeditionMapPreview_=!expeditionMapPreview_;tankExpeditionDetailsOpen_=false;RefreshTankExpeditionUi();}
        if(Press(input_,DIK_TAB)) {tankExpeditionDetailsOpen_=!tankExpeditionDetailsOpen_;expeditionMapPreview_=false;RefreshTankExpeditionUi();}
    }
    if(Press(input_,DIK_ESCAPE)) {
        if(expeditionMapPreview_||tankExpeditionDetailsOpen_) {expeditionMapPreview_=false;tankExpeditionDetailsOpen_=false;return;}
        tankRunPaused_=!tankRunPaused_;tankRunSelection_=0;tankRunMenuAge_=0;RefreshTankExpeditionUi();return;
    }
    const auto mouse=input_->GetMousePosition();const auto motion=input_->GetMouseState();
    const bool click=input_->IsTrigger(motion.rgbButtons[0],input_->GetPreMouseState().rgbButtons[0]);
    if(tankRunPaused_) {
        if(Press(input_,DIK_LEFT)||Press(input_,DIK_RIGHT)) tankRunSelection_=1-tankRunSelection_;
        for(int i=0;i<2;++i) if(Press(input_,DIK_1+i)||(click&&Inside(mouse,64+i*388.0f,280,368,280))) {SelectTankRunOption(i);return;}
        if(Press(input_,DIK_RETURN)) SelectTankRunOption(tankRunSelection_);return;
    }
    if(tankExpeditionDetailsOpen_) return;
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
            if(enter) {EnterExpeditionMapNode(expeditionMapSelection_);return;}
            if(expeditionMapRun_.GetChosenNodeIds().empty()) {
                if(Press(input_,DIK_C)) SetExpeditionBlueprint((expeditionBlueprint_+1)%3);
                for(int i=0;i<3;++i) if(click&&Inside(mouse,46+i*394.0f,650,378,34)) SetExpeditionBlueprint(i);
            }
        }
        int lastColumn=0;for(const auto& n:expeditionMapRun_.GetDefinition().nodes) lastColumn=(std::max)(lastColumn,n.column);
        if(Press(input_,DIK_Q)) expeditionMapScroll_-=240;if(Press(input_,DIK_E)) expeditionMapScroll_+=240;
        if(motion.lZ) expeditionMapScroll_-=static_cast<float>(motion.lZ);
        expeditionMapScroll_=(std::clamp)(expeditionMapScroll_,0.0f,(std::max)(0.0f,120.0f*(lastColumn-9)));
        if(!expeditionMapPreview_) UpdateTankExpeditionTutorial(dt);
        RefreshTankExpeditionUi();return;
    }
    const auto* active=expeditionMapRun_.GetActiveNode();
    if(active&&!tankexp::IsCombatNode(active->kind)) {
        if(tankRunMenuAge_>0.15f) {
            const int pages=(std::max)(1,(static_cast<int>(expeditionServiceOffers_.size())+1)/2);
            if(Press(input_,DIK_Q)||(click&&Inside(mouse,46,650,378,34))) expeditionServicePage_=(expeditionServicePage_+pages-1)%pages;
            if(Press(input_,DIK_E)||(click&&Inside(mouse,440,650,378,34))) expeditionServicePage_=(expeditionServicePage_+1)%pages;
            if(Press(input_,DIK_LEFT)) tankRunSelection_=(tankRunSelection_+2)%3;
            if(Press(input_,DIK_RIGHT)) tankRunSelection_=(tankRunSelection_+1)%3;
            for(int i=0;i<3;++i) {
                if(Inside(mouse,64+i*388.0f,280,368,280)&&(motion.lX||motion.lY)) tankRunSelection_=i;
                if(Press(input_,DIK_1+i)||(click&&Inside(mouse,64+i*388.0f,280,368,280))) {SelectExpeditionService(i);return;}
            }
            if(Press(input_,DIK_RETURN)||Press(input_,DIK_SPACE)) {SelectExpeditionService(tankRunSelection_);return;}
        }
        UpdateTankExpeditionTutorial(dt);RefreshTankExpeditionUi();return;
    }
    if(tankExpedition_.IsCombat()) {
        UpdateTankExpeditionTutorial(dt);
        const bool ready=tankExpedition_.GetRoomIndex()!=0||tankExpeditionTutorial_.CanLeaveFirstRoom()||expeditionMapAutoTest_;
        const auto kind=tankExpedition_.GetRoomKind();
        if(ready&&kind!=tankexp::RoomKind::Boss&&ThreatCount(enemyManager_.get())==0&&
            (kind!=tankexp::RoomKind::Guard||tankExpeditionNodes_>=3)) {CompleteExpeditionMapCombat();return;}
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
            {"evolutions",expeditionMapTestEvolutions_},{"repairs",expeditionMapTestHeals_},{"class",player_->GetCurrentClassName()},
            {"cards",tankRun_.GetCardCounts()},{"roomTemplates",expeditionRooms_.rooms.size()},
            {"upgradeTypes",expeditionContent_.upgrades.size()},{"enemyTypes",expeditionContent_.enemies.size()},{"playerTypes",expeditionContent_.players.size()}};
        std::ofstream("generated/expedition_map/validation.json")<<std::setw(2)<<report<<'\n';PostQuitMessage(success?0:4);return;
    }
    if(expeditionMapTestAge_<0.9f||!tankRunCapturePath_.empty()) return;
    if(expeditionMapRun_.IsChoosing()) {
        auto options=expeditionMapRun_.GetAvailableNodeIds();if(options.empty()) return;
        std::string id=options.front();
        // Exercise a repair, an authored evolution and a final paid upgrade.
        for(const auto& choice:options) if(choice=="field_repair"||choice=="arsenal"||choice=="final_upgrade") id=choice;
        EnterExpeditionMapNode(id);return;
    }
    const auto* node=expeditionMapRun_.GetActiveNode();if(!node) return;
    if(tankexp::IsCombatNode(node->kind)) {
        if(expeditionMapTestAge_>2.0f) {
            for(auto* e:enemyManager_->GetEnemyPtrs()) if(e&&!e->IsDead()) e->TakeDamageFromPlayer(100000);
            if(node->kind==NK::Boss) enemy_->TakeDamage(100000);
        }
    } else if(node->kind==NK::Heal) {player_->SpendRunHealth(20);SelectExpeditionService(0);}
    else if(node->kind==NK::Evolution) {expeditionServicePage_=1;SelectExpeditionService(0);}
    else SelectExpeditionService(0);
}
