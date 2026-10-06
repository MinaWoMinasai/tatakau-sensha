#include "GameScene.h"
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "Enemy.h"
#include "Player.h"
#include "game/exp/ExpEnemyNavigation.h"
#include "game/player/TankSpecialCombat.h"
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace {
using Json = nlohmann::json;
struct NormalReplay {
    std::filesystem::path directory;
    std::ofstream trace;
    Json report = {{"schemaVersion",1},{"status","unarmed"},{"attempts",Json::array()},{"operations",Json::array()}};
    Json topology;
    std::chrono::steady_clock::time_point start;
    uint64_t frame = 0, generation = 0, recovered = 0, coreDamage = 0;
    unsigned attempts = 0, warmup = 0;
    std::array<unsigned,3> recoveries{};
    int style = -1, previousBossHp = 0;
    bool finished = false, retryPending = false, bossReached = false, shoot = false, dash = false;
    std::string active;
    cg2::Vector2 move{};
    cg2::Vector3 aim{};
    std::array<bool,45*30> navigationBlocked{};
    std::string navigationNode;
    uint64_t navigationGeneration=0, navigationBuilds=0, navigationUses=0, navigationRejected=0;
    float navigationRadius=0;
    int navigationWaypoint=-1;
    bool usingNavigation=false, incomingThreat=false;
    uint64_t incomingThreatFrames=0;
} replay;
bool ReplayDanger(const neondepth::Snapshot* snapshot, cg2::Vector3 point, float radius) {
    if (!snapshot || snapshot->phase < neondepth::Phase::Telegraph || snapshot->phase > neondepth::Phase::Active) return false;
    const auto& plan = snapshot->plan;
    auto circle = [&](const neondepth::Circle& c) { return std::hypot(point.x-c.center[0],point.y-c.center[1]) <= c.radius+radius; };
    if (plan.attack == neondepth::Attack::Volley) {
        for (unsigned i=0;i<plan.circleCount;++i) if (circle(plan.circles[i])) return true;
    } else if (plan.attack == neondepth::Attack::Dive) return circle(plan.dive);
    else for (unsigned i=0;i<plan.beam.warningCount;++i) {
        const auto& s=plan.beam.warning[i];
        if (tankspecial::SegmentTouches(s.start[0],s.start[1],s.end[0],s.end[1],point.x,point.y,s.halfWidth+radius)) return true;
    }
    return false;
}
// Read actual Stage geometry for both firing LOS and the whole proposed movement
// segment. Expanded boxes also protect Player's existing X/Y AABB movement.
bool ReplayPathClear(Stage& stage, cg2::Vector3 from, cg2::Vector3 to, float padding) {
    for(const auto& wall:stage.GetMergedBlocks())
        if(tankspecial::SegmentCrossesBox(from.x,from.y,to.x,to.y,
            wall.aabb.min.x-padding,wall.aabb.min.y-padding,wall.aabb.max.x+padding,wall.aabb.max.y+padding)) return false;
    return true;
}
int ReplayCell(cg2::Vector3 p) {
    return (std::clamp)(static_cast<int>(std::round(p.y/2)),0,29)*45+
        (std::clamp)(static_cast<int>(std::round(p.x/2)),0,44);
}
cg2::Vector3 ReplayCellPoint(int cell) { return {static_cast<float>(cell%45)*2,static_cast<float>(cell/45)*2,0}; }
void FlushReplay() {
    replay.trace.flush();
    std::ofstream out(replay.directory/"route-proof.json"); out << replay.report.dump(2);
    if (!out || !replay.trace) throw std::runtime_error("Normal replay evidence write failed");
}
void ReplayOperation(const char* operation, Json argument) {
    if (replay.report["operations"].size()>=192) throw std::runtime_error("Normal replay operation limit");
    replay.report["operations"].push_back({{"frame",replay.frame},{"attempt",replay.attempts},{"operation",operation},{"argument",std::move(argument)}});
}
}

void GameScene::InitializeNormalRouteReplay() {
    wchar_t value[8]{};
    if (titleDemo_ || GetEnvironmentVariableW(L"CG2_NEON_DEPTH_NORMAL_REPLAY_STYLE",value,8)!=1 || value[0]<L'0' || value[0]>L'2' || replay.finished) return;
    try {
    if (!replay.attempts) {
        replay.style=static_cast<int>(value[0]-L'0'); replay.start=std::chrono::steady_clock::now();
        replay.directory=std::filesystem::path("generated/neon-boss-depth/normal-route")/(std::string(tankbuild::Ids[replay.style])+"-"+std::to_string(GetCurrentProcessId()));
        std::filesystem::create_directories(replay.directory); replay.trace.open(replay.directory/"frames.jsonl");
        replay.report["inputAdapter"]="existing SetDemoInput; menus use existing ordinary handlers";
        replay.report["limits"]={{"maximumUpdates",54000},{"maximumWallSeconds",900},{"maximumAttempts",3},{"maximumOperations",192}};
        replay.report["directGameplayWrites"]={{"hp",false},{"invulnerability",false},{"position",false},{"wallet",false},{"stats",false},{"map",false},{"forcedDamageOrKill",false},{"autoCollect",false},{"combatClock",false},{"wallPolicy",false}};
    }
    normalRouteReplayActive_=true;
    for (const wchar_t* flag : {L"CG2_TITLE_AUTOTEST",L"CG2_STARTUP_AUTOTEST",L"CG2_SUBMISSION_AUTOTEST"}) {
        wchar_t enabled[8]{};
        if (GetEnvironmentVariableW(flag,enabled,8)>0 && enabled[0]==L'1') {
            StopNormalRouteReplay("rejected-title-automation"); return;
        }
    }
    int lastColumn=-1; for (const auto& n:expeditionMapRun_.GetDefinition().nodes) lastColumn=(std::max)(lastColumn,n.column);
    const auto& map=expeditionMapRun_.GetDefinition();
    if (!expeditionRun_ || !expeditionMapEnabled_ || !map.procedural || map.generationSeed!=expeditionSeed_ || lastColumn<17 || lastColumn>21 ||
        map.generationMinColumns!=18 || map.generationMaxColumns!=22 || map.nodes.size()>256 || !expeditionMapRun_.GetChosenNodeIds().empty() ||
        GameplayScenarioSession::Get().IsActive() || neonBossAutoTest_ || neonBossDeveloperStartPending_ || debugPlayerNoDamage_ || player_->IsDebugNoDamage() ||
        tankRunAutoTest_ || expeditionMapAutoTest_ || experienceValidationVariant_ || combatValidationEnabled_ || specialValidationEnabled_ || tankExpeditionTutorialValidation_.enabled) {
        StopNormalRouteReplay("rejected-nonnormal-state"); return;
    }
    if (++replay.attempts>3) { StopNormalRouteReplay("retry-limit"); return; }
    replay.topology=tankexp::ExpeditionMapJson(map); replay.active.clear(); replay.warmup=0; replay.retryPending=false;
    replay.generation=replay.recovered=replay.coreDamage=0; replay.recoveries={}; replay.bossReached=false;
    replay.navigationNode.clear(); replay.navigationWaypoint=-1; replay.navigationRadius=0; replay.usingNavigation=false;
    replay.report["status"]="running";
    replay.report["attempts"].push_back({{"attempt",replay.attempts},{"tickDerivedSeed",expeditionSeed_},{"columns",lastColumn+1},{"initialMap",replay.topology},{"initialWallet",expeditionMapRun_.GetCurrency()},{"initialHp",player_->GetHp()}});
    FlushReplay();
    } catch (const std::exception& error) { StopNormalRouteReplay(error.what()); }
}

void GameScene::PrepareNormalRouteReplay() {
    if (!normalRouteReplayActive_ || replay.finished) return;
    try {
        gameplayScenarioCombatDt_=0; // Observe only a real Player update later in this frame.
        replay.move={}; replay.aim=player_->GetWorldPosition(); replay.shoot=replay.dash=false; replay.usingNavigation=replay.incomingThreat=false;
        player_->SetDemoInput(false,{},replay.aim,false,false);
        if (++replay.frame>54000 || std::chrono::steady_clock::now()-replay.start>std::chrono::seconds(900)) { StopNormalRouteReplay("bounded-timeout"); return; }
        if (GameplayScenarioSession::Get().IsActive() || debugPlayerNoDamage_ || player_->IsDebugNoDamage() || IsNeonShowcaseActive() || developerBloomFreeze_ || neonBossDeveloperFreeze_ ||
            expeditionAuthoringHubOpen_ || expeditionRoomEditorOpen_ || expeditionMapEditorOpen_ || expeditionContentEditorOpen_ || tankExpeditionBalanceEditorOpen_ ||
            player_->IsChangeMode() || tankRunPaused_ || expeditionMapPreview_ || tankExpeditionDetailsOpen_ || IsGuidedExpeditionPaused()) { StopNormalRouteReplay("external-pause-or-override"); return; }
        if (phase_!=Phase::kMain || expeditionTransition_.IsActive()) { replay.warmup=0; return; }
        const auto flow=combatFlow_.GetState();
        if (flow==GameFlowState::GameOver && combatFlow_.GetTimer()<=0 && !replay.retryPending) {
            if (replay.attempts>=3) { StopNormalRouteReplay("retry-limit"); return; }
            ReplayOperation("ordinary-result-retry",{{"hp",player_->GetHp()},{"wallet",expeditionMapRun_.GetCurrency()}});
            replay.retryPending=true; resultSelection_=0; ConfirmResultSelection(); return;
        }
        if (flow!=GameFlowState::Playing || player_->IsDead()) return;
        const auto* node=expeditionMapRun_.GetActiveNode();
        const std::string id=node?node->id:"";
        if (replay.active!=id) { replay.active=id; replay.warmup=0; }
        if (tankRunMenuAge_>.25f && expeditionBuildChoice_) {
            ReplayOperation("ordinary-build-choice",replay.style); SelectExpeditionBuildStyle(replay.style); return;
        }
        if (tankRunMenuAge_>.25f && expeditionMapRun_.IsChoosing()) {
            const auto available=expeditionMapRun_.GetAvailableNodeIds(); const tankexp::MapNode* choice=nullptr; int best=999;
            for (const auto& option:available) if (expeditionMapRun_.CanSelectNode(option)) {
                const auto* n=tankexp::FindMapNode(expeditionMapRun_.GetDefinition(),option); if (!n) continue;
                const int score=n->role==tankexp::NodeRole::TutorialSkip?-100:n->kind==tankexp::NodeKind::Heal?(player_->GetHp()<player_->GetMaxHp()?0:2):
                    n->kind==tankexp::NodeKind::Upgrade || n->kind==tankexp::NodeKind::Evolution?1:n->kind==tankexp::NodeKind::Currency?3:n->kind==tankexp::NodeKind::Elite?5:4;
                if (score<best) {best=score;choice=n;}
            }
            if (choice) { ReplayOperation("ordinary-map-choice",{{"id",choice->id},{"available",available},{"wallet",expeditionMapRun_.GetCurrency()}}); RequestExpeditionMapNode(choice->id); }
            return;
        }
        if (node && !tankexp::IsCombatNode(node->kind)) {
            replay.warmup=0; if (tankRunMenuAge_<=.25f || node->kind==tankexp::NodeKind::Currency) return;
            int option=3,price=0; std::string offer;
            if (node->kind==tankexp::NodeKind::Heal && player_->GetHp()<player_->GetMaxHp() && expeditionMapRun_.CanAfford(node->serviceCost)) {option=0;price=node->serviceCost;}
            else for (int i=0;i<3;++i) {
                const auto index=static_cast<size_t>(expeditionServicePage_*3+i); if (index>=expeditionServiceOffers_.size()) continue;
                const auto& candidate=expeditionServiceOffers_[index]; const auto* upgrade=tankcontent::FindUpgrade(expeditionContent_,candidate);
                if (upgrade && tankcontent::EligibleUpgrade(*upgrade,expeditionBuildStyle_,tankRun_.GetCardCounts()) && expeditionMapRun_.CanAfford(ExpeditionServicePrice(candidate))) {option=i;offer=candidate;price=ExpeditionServicePrice(candidate);break;}
            }
            ReplayOperation("ordinary-service-choice",{{"option",option},{"offer",offer},{"price",price},{"walletBefore",expeditionMapRun_.GetCurrency()},{"hpBefore",player_->GetHp()}});
            SelectExpeditionService(option); return;
        }
        if (!node || !tankExpedition_.IsCombat() || IsTankRunMenuOpen()) return;
        const neondepth::Snapshot* depth=enemy_->IsNeonDepthEncounterEnabled()?&enemy_->GetNeonDepthSnapshot():nullptr;
        if (depth && depth->inputLocked) {replay.warmup=0;return;}
        if (replay.warmup<2) return; // Two real positive Player updates with physical buttons released precede this API's release-guard side effect.
        const auto position=player_->GetWorldPosition(); bool target=false;
        if (const auto* nearest=enemyManager_->FindNearestEnemy(position,10000.0f)) {replay.aim=nearest->GetWorldPosition();target=true;}
        else if (enemy_->IsRunEncounterEnabled() && !enemy_->IsDead()) {replay.aim=enemy_->GetWorldPosition();target=true;}
        if (!target) return;
        const float desired=player_->IsMeleeBuild()?2.8f:10.0f, radius=player_->GetRadius();
        const auto observedBullets=bulletManager_->GetBulletPtrs();
        const auto bulletRisk=[&](cg2::Vector3 point, bool current=false) {
            float risk=0;
            for (const auto* bullet:observedBullets) if (bullet && !bullet->IsDead() && bullet->GetOwner()==BulletOwner::kEnemy) {
                const auto p=bullet->GetWorldPosition(),v=bullet->GetMove();
                // GetMove is distance per 60 FPS tick: 18 ticks is the existing .30 s dash window.
                const float endX=p.x+v.x*18, endY=p.y+v.y*18, reach=radius+bullet->GetRadius()+.35f;
                if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(endX) || !std::isfinite(endY) || !std::isfinite(reach) || reach<0) continue;
                const float t=tankspecial::SegmentClosestFraction(p.x,p.y,endX,endY,point.x,point.y);
                const float gap=std::hypot(p.x+(endX-p.x)*t-point.x,p.y+(endY-p.y)*t-point.y)-reach;
                if (!std::isfinite(gap)) continue;
                if (gap<=0) {risk+=100; if(current) replay.incomingThreat=true;}
                else risk+=8/(1+gap*gap);
            }
            return (std::min)(500.0f,risk); // Preserve the existing Depth-warning penalty of 1000.
        };
        bulletRisk(position,true); if(replay.incomingThreat) ++replay.incomingThreatFrames;
        constexpr std::array<cg2::Vector2,9> directions{{{0,0},{1,0},{.7071f,.7071f},{0,1},{-.7071f,.7071f},{-1,0},{-.7071f,-.7071f},{0,-1},{.7071f,-.7071f}}};
        float best=std::numeric_limits<float>::max();
        for (const auto direction:directions) {
            const cg2::Vector3 probe{position.x+direction.x*2,position.y+direction.y*2,0};
            if (stage_->IsCollisionWithAnyBlock(probe,radius+.15f)) continue;
            const float dx=replay.aim.x-probe.x,dy=replay.aim.y-probe.y;
            const float cost=std::abs(std::hypot(dx,dy)-desired)+(ReplayDanger(depth,probe,radius+.25f)?1000.0f:0.0f)-.08f*(direction.x*dy-direction.y*dx)+bulletRisk(probe);
            if (cost<best) {best=cost;replay.move=direction;}
        }
        const bool needsRoute=!ReplayPathClear(*stage_,position,replay.aim,0) ||
            std::hypot(replay.aim.x-position.x,replay.aim.y-position.y)>desired;
        if(needsRoute && !replay.incomingThreat && !ReplayDanger(depth,position,radius+.25f)) {
            const float padding=radius+.02f; const auto generation=enemy_->GetEncounterGeneration();
            if(replay.navigationNode!=id || replay.navigationGeneration!=generation || replay.navigationRadius!=padding) {
                for(int cell=0;cell<45*30;++cell) replay.navigationBlocked[cell]=stage_->IsCollisionWithAnyBlock(ReplayCellPoint(cell),padding);
                replay.navigationNode=id; replay.navigationGeneration=generation; replay.navigationRadius=padding;
                replay.navigationWaypoint=-1; ++replay.navigationBuilds;
            }
            if(replay.navigationWaypoint<0 || std::hypot(ReplayCellPoint(replay.navigationWaypoint).x-position.x,
                ReplayCellPoint(replay.navigationWaypoint).y-position.y)<.12f)
                replay.navigationWaypoint=FindExpEnemyNextCell<45,30>(replay.navigationBlocked,ReplayCell(position),ReplayCell(replay.aim));
            if(replay.navigationWaypoint>=0) {
                const auto waypoint=ReplayCellPoint(replay.navigationWaypoint);
                bool safe=ReplayPathClear(*stage_,position,waypoint,padding) && !stage_->IsCollisionWithAnyBlock(waypoint,padding);
                const float distance=std::hypot(waypoint.x-position.x,waypoint.y-position.y);
                const int steps=(std::clamp)(static_cast<int>(std::ceil(distance/.25f)),1,128);
                if(distance>32) safe=false; // A blocked-start relocation must still be a bounded legal segment.
                for(int step=0;safe && step<=steps;++step) {
                    const float t=static_cast<float>(step)/static_cast<float>(steps);
                    safe=!ReplayDanger(depth,{position.x+(waypoint.x-position.x)*t,position.y+(waypoint.y-position.y)*t,0},radius+.25f);
                }
                if(safe) {
                    // Read current inertia only; regular input still goes through actual PlayerMovement and Stage.
                    const auto velocity=player_->GetMove(); const float speed=(std::max)(.01f,player_->GetStats().moveSpeed);
                    replay.move={((waypoint.x-position.x)/30-velocity.x)/speed,((waypoint.y-position.y)/30-velocity.y)/speed};
                    const float magnitude=std::hypot(replay.move.x,replay.move.y);
                    if(magnitude>1) {replay.move.x/=magnitude;replay.move.y/=magnitude;}
                    replay.usingNavigation=true; ++replay.navigationUses;
                } else {replay.navigationWaypoint=-1; ++replay.navigationRejected;}
            }
        } else replay.navigationWaypoint=-1;
        replay.shoot=!replay.usingNavigation; replay.dash=ReplayDanger(depth,position,radius) && depth && depth->phase>=neondepth::Phase::Locked && !player_->IsDashing();
        if(replay.incomingThreat && std::hypot(replay.move.x,replay.move.y)>.01f && !player_->IsDashing()) replay.dash=true;
        player_->SetDemoInput(true,replay.move,replay.aim,replay.shoot,replay.dash);
    } catch (const std::exception& error) { StopNormalRouteReplay(error.what()); }
}

void GameScene::RecordNormalRouteReplay() {
    if (!normalRouteReplayActive_ || replay.finished) return;
    try {
        if (tankexp::ExpeditionMapJson(expeditionMapRun_.GetDefinition())!=replay.topology) {StopNormalRouteReplay("map-topology-changed");return;}
        if (!player_->IsDemoInputEnabled() && gameplayScenarioCombatDt_>0 && !input_->IsPress(input_->GetMouseState().rgbButtons[0]) && !input_->IsPress(input_->GetMouseState().rgbButtons[1])) ++replay.warmup;
        const auto p=player_->GetWorldPosition(); const auto& stats=player_->GetStats(); const auto* node=expeditionMapRun_.GetActiveNode();
        Json frame={{"frame",replay.frame},{"attempt",replay.attempts},{"combatDt",gameplayScenarioCombatDt_},{"node",replay.active},{"visited",expeditionMapRun_.GetVisitedNodeIds()},
            {"chosen",expeditionMapRun_.GetChosenNodeIds()},{"wallet",expeditionMapRun_.GetCurrency()},{"purchases",expeditionPurchases_},{"cards",tankRun_.GetCardCounts()},
            {"hp",player_->GetHp()},{"maxHp",player_->GetMaxHp()},{"position",{p.x,p.y,p.z}},{"style",static_cast<int>(player_->GetExpeditionCombatStyle())},
            {"stats",{{"damage",stats.bulletDamage},{"reload",stats.reloadSpeed},{"move",stats.moveSpeed},{"stamina",stats.stamina}}},{"attacks",player_->GetPrimaryAttackCount()},
            {"dashes",player_->GetDashStartedCount()},{"damageTaken",player_->GetDamageTakenCount()},{"perfectDodges",player_->GetPerfectDodgeCount()},{"invincibilitySeconds",player_->GetInvincibilityRemainingSeconds()},
            {"flow",static_cast<int>(combatFlow_.GetState())},{"debugNoDamage",player_->IsDebugNoDamage()},{"input",{{"enabled",player_->IsDemoInputEnabled()},{"move",{replay.move.x,replay.move.y}},{"aim",{replay.aim.x,replay.aim.y,replay.aim.z}},{"shoot",replay.shoot},{"dash",replay.dash}}}};
        frame["navigation"]={{"enabled",replay.usingNavigation},{"waypointCell",replay.navigationWaypoint},{"cacheBuilds",replay.navigationBuilds},
            {"uses",replay.navigationUses},{"rejected",replay.navigationRejected},{"node",replay.navigationNode},{"generation",replay.navigationGeneration}};
        frame["incomingThreat"]=replay.incomingThreat; frame["incomingThreatFrames"]=replay.incomingThreatFrames;
        if (enemy_->IsNeonDepthEncounterEnabled()) {
            const auto& depth=enemy_->GetNeonDepthSnapshot();
            if (replay.generation!=depth.generation) {replay.generation=depth.generation;replay.previousBossHp=enemy_->GetHp();}
            replay.coreDamage+=static_cast<uint64_t>((std::max)(0,replay.previousBossHp-enemy_->GetHp())); replay.previousBossHp=enemy_->GetHp();
            if (depth.phase==neondepth::Phase::Recovery && depth.plan.instance && replay.recovered!=depth.plan.instance) {replay.recovered=depth.plan.instance;++replay.recoveries[static_cast<size_t>(depth.plan.attack)];}
            replay.bossReached|=node && node->kind==tankexp::NodeKind::Boss && node->column+1==replay.report["attempts"].back()["columns"].get<int>();
            frame["depth"]={{"generation",depth.generation},{"instance",depth.plan.instance},{"attack",static_cast<int>(depth.plan.attack)},{"phase",static_cast<int>(depth.phase)},{"phaseTwo",depth.plan.phaseTwo},
                {"hp",enemy_->GetHp()},{"coreDamage",replay.coreDamage},{"recoveries",replay.recoveries},{"fullCycles",(std::min)({replay.recoveries[0],replay.recoveries[1],replay.recoveries[2]})},
                {"contactClaims",enemy_->GetNeonDepthContactClaims()},{"acceptedHits",enemy_->GetNeonDepthAcceptedHits()},{"planFailure",enemy_->HasNeonDepthPlanFailure()}};
        }
        frame["finalBossReached"]=replay.bossReached; frame["topologyUnchanged"]=true;
        frame["enemies"]=Json::array(); for (const auto* actor:enemyManager_->GetEnemyPtrs()) if (actor) {const auto enemyPosition=actor->GetWorldPosition();frame["enemies"].push_back({{"hp",actor->GetHp()},{"dead",actor->IsDead()},{"position",{enemyPosition.x,enemyPosition.y,enemyPosition.z}}});}
        replay.trace << frame.dump() << '\n'; replay.report["attempts"].back()["latest"]=frame;
        if (replay.frame%60==0 || replay.retryPending) FlushReplay();
        if (combatFlow_.GetState()==GameFlowState::StageClear) StopNormalRouteReplay("ordinary-victory");
    } catch (const std::exception& error) { StopNormalRouteReplay(error.what()); }
}

void GameScene::StopNormalRouteReplay(const char* reason) {
    normalRouteReplayActive_=false; replay.finished=true; replay.report["status"]=reason;
    if (player_) player_->SetDemoInput(false,{},player_->GetWorldPosition(),false,false);
    try { FlushReplay(); } catch (const std::exception&) { replay.report["evidenceWriteFailed"]=true; }
}
Json GameScene::MakeNormalRouteReplayState() const { return replay.report; }
#endif
