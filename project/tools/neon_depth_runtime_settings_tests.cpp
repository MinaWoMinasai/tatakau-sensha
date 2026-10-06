// Draft, NOT RUN. One TU includes actual Session implementation to test its
// strict private parser. Do not also link GameplayScenarioSession.cpp.
#include "game/debug/GameplayScenarioSession.cpp"
#include <cassert>
#include <iostream>

namespace {
nlohmann::json Manifest(const char* scenario="neon_depth_cycles") {
    return {{"scenario",scenario},{"frames",120},{"enemyCount",0},{"captureFrames",nlohmann::json::array()},
        {"depthFixture",{{"probe","none"},{"attack","volley"},{"phase","active"},{"minimumProgress",0},
            {"visualEnabled",true},{"reducedMotion",false},{"injectPhaseTwo",false}}}};
}
template<class Function> void Reject(Function&& function) {
    bool failed=false; try {function();}catch(const std::exception&){failed=true;} assert(failed);
}
void ParserAndBounds() {
    using nlohmann::json;
    for(int index=0;index<=static_cast<int>(gameplaytest::Scenario::NeonDepthParity);++index) {
        const auto scenario=static_cast<gameplaytest::Scenario>(index); gameplaytest::Scenario parsed{};
        assert(gameplaytest::ParseScenario(gameplaytest::Name(scenario),parsed) && parsed==scenario);
    }
    assert(static_cast<int>(gameplaytest::Scenario::PreviewLifecycle)==12);
    for(int index=0;index<=12;++index) {
        gameplaytest::Settings legacy; legacy.scenario=static_cast<gameplaytest::Scenario>(index);
        std::string error; assert(gameplaytest::ValidateSettings(legacy,error));
        auto wrong=Manifest(gameplaytest::Name(legacy.scenario)); Reject([&]{(void)ReadSettings(wrong);});
    }
    auto valid=ReadSettings(Manifest());
    assert(valid.scenario==gameplaytest::Scenario::NeonDepthCycles && valid.enemyCount==0);
    for(const char* key:{"visualEnabled","reducedMotion","injectPhaseTwo"}) {
        auto wrong=Manifest();wrong["depthFixture"][key]=1;Reject([&]{(void)ReadSettings(wrong);});
    }
    for(const char* key:{"probe","phase","attack"}) {
        auto wrong=Manifest();wrong["depthFixture"][key]=42;Reject([&]{(void)ReadSettings(wrong);});
        wrong=Manifest();wrong["depthFixture"][key]="invalid";Reject([&]{(void)ReadSettings(wrong);});
    }
    for(double value:{-.1,.951,1.0,1.0e99}) {
        auto wrong=Manifest();wrong["depthFixture"]["minimumProgress"]=value;Reject([&]{(void)ReadSettings(wrong);});
    }
    auto wrong=Manifest();wrong["depthFixture"]["unknown"]=true;Reject([&]{(void)ReadSettings(wrong);});
    wrong=Manifest();wrong["depthFixture"]["minimumProgress"]=true;Reject([&]{(void)ReadSettings(wrong);});
    wrong=Manifest();wrong["enemyCount"]=1;Reject([&]{(void)ReadSettings(wrong);});
    wrong=Manifest();wrong["initialProjectiles"]=1;Reject([&]{(void)ReadSettings(wrong);});
    wrong=Manifest("neon_depth_damage");wrong["depthFixture"]["injectPhaseTwo"]=true;Reject([&]{(void)ReadSettings(wrong);});
    wrong=Manifest("neon_depth_lifecycle");Reject([&]{(void)ReadSettings(wrong);});
    auto title=Manifest("neon_depth_lifecycle");title["depthFixture"]["probe"]="title_return";
    assert(ReadSettings(title).depthFixture.probe=="title_return");
    title["recording"]={{"enabled",true},{"firstFrame",1},{"frameCount",120},{"encodedFps",60}};
    Reject([&]{(void)ReadSettings(title);});
    for(const char* probe:{"pause","abort","hp0","intro_skip","player_death","simultaneous_death","retry","title_return","multidraw","stationary_damage"}) {
        auto object=Manifest("neon_depth_lifecycle");object["depthFixture"]["probe"]=probe;
        assert(ReadSettings(object).depthFixture.probe==probe);
    }
    auto beam=Manifest("neon_depth_lifecycle");beam["depthFixture"]["probe"]="hp0";
    beam["depthFixture"]["attack"]="beam";beam["depthFixture"]["phase"]="airborne";
    Reject([&]{(void)ReadSettings(beam);});
}
void ActualSession(const std::string& mode) {
    auto& session=GameplayScenarioSession::Get();assert(session.IsActive());session.BeginScene();
    session.ReportDetail("cpuDepthContract",{{"injectedRows",true},{"actualActorsProduced",false},{"actualSceneInitializationProduced",false}});
    const bool title=mode=="title_notify";
    const unsigned count=title?3:120;
    for(unsigned frame=1;frame<=count;++frame) {
        gameplaytest::Snapshot snapshot;snapshot.playerHp=snapshot.playerMaxHp=10;
        snapshot.bossActive=true;snapshot.bossHp=10;snapshot.bossEncounterGeneration=1;
        session.Record(snapshot);
        nlohmann::json row={{"frame",frame},{"sceneEpoch",session.GetSceneEpoch()},{"cpuInjected",true}};
        if(mode=="row_gap" && frame==1) continue;
        if(mode=="row_nan" && frame==1) row["nan"]=std::numeric_limits<double>::quiet_NaN();
        if(mode=="row_oversize" && frame==1) row["oversized"]=std::string(65536,'x');
        session.ReportDepthFrame(row);
        if(mode=="draw_duplicate" && frame==1) {
            session.ReportDepthDraw({{"cpuInjected",true}});session.ReportDepthDraw({{"cpuInjected",true}});
        }
    }
    const bool success=mode=="row_valid" || title;
    if(title) assert(session.NotifyDepthSceneEntered("TITLE") && session.IsFinished());
    else assert(!session.NotifyDepthSceneEntered("TANK_EXPEDITION"));
    assert(session.Finish()==success);
    if(success) assert(session.GetErrors().empty());else assert(!session.GetErrors().empty());
}
}
int main(int argc,char** argv) {
    ParserAndBounds();
    if(argc>1) ActualSession(argv[1]);
    std::cout<<"PASS: actual Depth parser/session contracts (CPU injected rows, no gameplay/GPU evidence)\n";
}
