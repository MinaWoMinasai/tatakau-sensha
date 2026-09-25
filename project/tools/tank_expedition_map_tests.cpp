#include "../game/run/TankExpeditionMap.h"
#include "../game/run/TankRunDirector.h"
#include <cassert>
#include <iostream>
#include <limits>

using namespace tankexp;
namespace {
void CheckInvalid(MapDefinition map) {
    std::string error;
    assert(!ValidateExpeditionMap(map,error)&&!error.empty());
    ExpeditionMapRun run;
    assert(run.SelectNode("outskirts"));
    assert(run.EarnCurrency(7));
    const auto wallet=run.GetCurrency();
    assert(!run.Reset(map,error));
    assert(run.GetCurrency()==wallet&&run.GetActiveNodeId()=="outskirts");
}
void Validation() {
    auto original=DefaultExpeditionMap();std::string error;
    assert(ValidateExpeditionMap(original,error));
    auto map=original;map.nodes[0].next={"missing"};CheckInvalid(map);
    map=original;map.nodes[1].next={"outskirts"};CheckInvalid(map);
    map=original;map.nodes[1].next={"first_upgrade"};CheckInvalid(map);
    map=original;map.nodes[0].next.clear();CheckInvalid(map);
    map=original;map.nodes.back().next={"outskirts"};CheckInvalid(map);
    map=original;map.nodes[0].next={"first_upgrade","first_upgrade"};CheckInvalid(map);
    map=original;map.nodes[2].id=map.nodes[3].id;CheckInvalid(map);
    map=original;map.startNodes={"core"};CheckInvalid(map);
    map=original;map.startNodes={"outskirts","outskirts"};CheckInvalid(map);
    map=original;map.nodes[1].next={"salvage"};CheckInvalid(map);
    map=original;map.nodes[0].clearReward=-1;CheckInvalid(map);
    map=original;map.nodes[1].serviceCost=-1;CheckInvalid(map);
    map=original;map.nodes[0].roomTemplate="../file";CheckInvalid(map);
    map=original;map.nodes[0].column=32;CheckInvalid(map);
    map=original;map.nodes[2].row=map.nodes[3].row;CheckInvalid(map);
    map=original;map.nodes[0].kind=static_cast<NodeKind>(999);CheckInvalid(map);
    map=original;map.nodes[0].combatStage=-1;CheckInvalid(map);
    map=original;map.nodes[1].clearReward=1;CheckInvalid(map);
    map=original;map.nodes[1].roomTemplate="outskirts";CheckInvalid(map);
    map=original;map.startingCurrency=(std::numeric_limits<int>::max)();CheckInvalid(map);
    map=original;map.nodes.back().kind=NodeKind::Combat;CheckInvalid(map);

    auto destination=original;destination.startingCurrency=123;
    auto goodJson=ExpeditionMapJson(original);
    for(const char* key:{"schemaVersion","startingCurrency","nodes","startNodes"}) {
        auto bad=goodJson;bad[key]=false;
        assert(!ParseExpeditionMap(bad,destination,error)&&destination.startingCurrency==123);
    }
    for(const char* key:{"id","label","kind","column","row","combatStage","roomTemplate","clearReward","serviceCost","next"}) {
        auto bad=goodJson;bad["nodes"][0][key]=nullptr;
        assert(!ParseExpeditionMap(bad,destination,error)&&destination.startingCurrency==123);
    }
    for(const auto badNumber:{-1.0,0.5,1e20}) {
        auto bad=goodJson;bad["nodes"][1]["serviceCost"]=badNumber;
        assert(!ParseExpeditionMap(bad,destination,error)&&destination.startingCurrency==123);
    }
    auto bad=goodJson;bad["nodes"][0]["kind"]="mystery";
    assert(!ParseExpeditionMap(bad,destination,error));
    bad=goodJson;bad["startingCurrency"]=(std::numeric_limits<std::uint64_t>::max)();
    assert(!ParseExpeditionMap(bad,destination,error));
    assert(ParseExpeditionMap(goodJson,destination,error));
    assert(ExpeditionMapJson(destination)==goodJson);
}
void Persistence() {
    auto map=DefaultExpeditionMap();std::string error;
    assert(SaveExpeditionMap("map-roundtrip.json",map,error));
    map.startingCurrency=97;
    assert(SaveExpeditionMap("map-roundtrip.json",map,error)); // Existing-file atomic replacement.
    MapDefinition loaded;
    assert(LoadExpeditionMap("map-roundtrip.json",loaded,error));
    assert(ExpeditionMapJson(loaded)==ExpeditionMapJson(map));
    auto bad=map;bad.nodes[0].next.clear();
    assert(!SaveExpeditionMap("map-roundtrip.json",bad,error));
    assert(LoadExpeditionMap("map-roundtrip.json",loaded,error)&&loaded.startingCurrency==97);
    std::ofstream("map-broken.json")<<"{broken";
    assert(!LoadExpeditionMap("map-broken.json",loaded,error)&&loaded.startingCurrency==97);
    assert(!LoadExpeditionMap("missing-map.json",loaded,error)&&loaded.startingCurrency==97);
    assert(!SaveExpeditionMap("missing-folder/map.json",map,error));
}
void WalletAndTransitions() {
    ExpeditionMapRun run;
    assert(run.IsChoosing()&&run.GetCurrency()==20&&run.GetActiveNode()==nullptr);
    assert(run.GetAvailableNodeIds()==std::vector<std::string>{"outskirts"});
    assert(!run.SelectNode("core")&&!run.SelectNode("missing")&&!run.MarkDead());
    assert(!run.CompleteCombat()&&!run.CompleteService());
    assert(!run.EarnCurrency(0)&&!run.EarnCurrency(-1)&&!run.EarnCurrency((std::numeric_limits<int>::max)()));
    assert(!run.TrySpendCurrency(-1)&&!run.TrySpendCurrency(41)&&run.TrySpendCurrency(0));
    assert(run.SelectNode("outskirts")&&run.GetAvailableNodeIds().empty());
    assert(!run.SelectNode("first_upgrade")&&!run.CompleteService());
    assert(run.GetChosenNodeIds().size()==1&&run.GetVisitedNodeIds().empty());
    assert(run.EarnCurrency(8)&&run.CompleteCombat());
    assert(run.GetCurrency()==58&&run.HasVisited("outskirts"));
    assert(!run.CompleteCombat()&&run.GetCurrency()==58);
    assert(run.SelectNode("first_upgrade"));
    assert(run.TrySpendCurrency(58)&&run.GetCurrency()==0);
    assert(!run.CompleteService()&&run.GetActiveNodeId()=="first_upgrade");
    assert(run.CompleteService(false)&&run.GetCurrency()==0); // No currency softlock.
    assert(!run.CompleteService(false)&&!run.SelectNode("outskirts"));
    assert(run.SelectNode("salvage")&&run.MarkDead());
    assert(run.IsDead()&&!run.IsComplete()&&run.GetAvailableNodeIds().empty());
    assert(!run.CompleteCombat()&&!run.CompleteService()&&!run.MarkDead());
    assert(!run.EarnCurrency(1)&&!run.TrySpendCurrency(0));
    run.Reset();assert(run.IsChoosing()&&run.GetCurrency()==20&&run.GetChosenNodeIds().empty());
    assert(run.EarnCurrency(kExpeditionCurrencyLimit));
    assert(run.GetCurrency()==kExpeditionCurrencyLimit);
    assert(run.EarnCurrency(500)&&run.GetCurrency()==kExpeditionCurrencyLimit);
    assert(run.TrySpendCurrency(kExpeditionCurrencyLimit)&&run.GetCurrency()==0);
}
void AuthoredUpgradeBundles() {
    using tankrun::CardId;using tankrun::RunDirector;
    tankrun::Config config;config.maxDrafts=1;
    RunDirector run(17,config);
    assert(!run.GrantExpeditionModules({CardId::Rapid}));
    assert(run.ChooseLoadout(0)&&!run.GrantExpeditionModules({CardId::Rapid}));
    assert(run.ChooseCore(0));
    assert(!run.GrantExpeditionModules({}));
    const std::vector<std::vector<CardId>> invalid={{CardId::Rapid,CardId::Count},
        {CardId::Rapid,static_cast<CardId>(-1)},{static_cast<CardId>(999),CardId::Heavy}};
    for(const auto& bundle:invalid) {
        const auto before=run.GetCardCounts();const int purchases=run.GetDraftCount();
        assert(!run.GrantExpeditionModules(bundle));
        assert(run.GetCardCounts()==before&&run.GetDraftCount()==purchases); // No partial grant before invalid effect.
    }
    assert(run.GrantExpeditionModules({CardId::Rapid,CardId::Rapid,CardId::Heavy}));
    assert(run.GetCardCount(CardId::Rapid)==1&&run.GetCardCount(CardId::Heavy)==1&&run.GetDraftCount()==1);
    assert(!run.GrantExpeditionModules({CardId::Rapid,CardId::Heavy,CardId::Rapid}));
    assert(run.GetDraftCount()==1);
    // Catalog purchases are not constrained by the arena's free draft count.
    assert(run.GrantExpeditionModules({CardId::Rapid,CardId::Homing,CardId::Homing}));
    assert(run.GetCardCount(CardId::Rapid)==1&&run.GetCardCount(CardId::Homing)==1&&run.GetDraftCount()==2);
    std::vector<CardId> all;
    for(std::size_t i=0;i<tankrun::CardCount;++i) all.push_back(static_cast<CardId>(i));
    assert(run.GrantExpeditionModules(all)&&run.GetDraftCount()==3);
    assert(!run.GrantExpeditionModules(all)&&run.GetDraftCount()==3);
    for(const int count:run.GetCardCounts()) assert(count==1);
    assert(run.CompleteBoss());const auto completeCards=run.GetCardCounts();
    assert(!run.GrantExpeditionModules({CardId::Rapid})&&run.GetCardCounts()==completeCards);
    run.Reset();assert(run.ChooseLoadout(0)&&run.ChooseCore(0)&&run.MarkDead());
    assert(!run.GrantExpeditionModules({CardId::Rapid})&&run.GetDraftCount()==0);
    run.Reset();assert(run.ChooseLoadout(0)&&run.ChooseCore(0)&&run.OpenRewardDraft(false));
    const auto offers=run.GetOffers();
    assert(!run.GrantExpeditionModules({CardId::Rapid})&&run.GetDraftCount()==0&&run.GetOffers()==offers);
}
int WalkAllPaths(ExpeditionMapRun run,bool skipAll,int fights=0,bool hadEvolution=false) {
    if(run.IsComplete()) {
        assert(fights==5&&hadEvolution&&run.GetVisitedNodeIds().size()==10);
        assert(!run.SelectNode("core")&&!run.CompleteCombat()&&!run.EarnCurrency(1)&&!run.TrySpendCurrency(0));
        return 1;
    }
    const auto available=run.GetAvailableNodeIds();assert(!available.empty());
    int routes=0;
    for(const auto& id:available) {
        auto branch=run;assert(branch.SelectNode(id));const auto node=*branch.GetActiveNode();
        int nextFights=fights;bool evolution=hadEvolution;
        if(IsCombatNode(node.kind)) {
            assert(node.combatStage==fights);++nextFights;
            const int before=branch.GetCurrency();assert(branch.CompleteCombat());
            assert(branch.GetCurrency()==before+node.clearReward);
            assert(!branch.CompleteCombat());
        } else {
            evolution=evolution||node.kind==NodeKind::Evolution;
            // The foundation upgrade, evolution and first repair stay accessible.
            // Later spending requires a route tradeoff or salvage from enemy kills.
            if(node.id=="first_upgrade"||node.id=="evolution"||node.id=="field_repair") assert(branch.CanAfford(node.serviceCost));
            const int before=branch.GetCurrency();const bool buy=!skipAll&&branch.CanAfford(node.serviceCost);
            if(!skipAll&&!buy) assert(!branch.CompleteService()&&branch.GetCurrency()==before&&branch.GetActiveNodeId()==node.id);
            assert(branch.CompleteService(buy));
            assert(branch.GetCurrency()==before-(buy?node.serviceCost:0));
            assert(!branch.CompleteService(buy));
        }
        routes+=WalkAllPaths(branch,skipAll,nextFights,evolution);
    }
    return routes;
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
#else
int main(int argc,char** argv) {
#endif
    Validation();Persistence();WalletAndTransitions();AuthoredUpgradeBundles();
    assert(WalkAllPaths(ExpeditionMapRun{},false)==32);
    assert(WalkAllPaths(ExpeditionMapRun{},true)==32);
    if(argc>1) {
        MapDefinition authored;std::string error;
        const auto path=std::filesystem::path(argv[1]).u8string();
        assert(LoadExpeditionMap(std::string(reinterpret_cast<const char*>(path.data()),path.size()),authored,error));
        assert(ExpeditionMapJson(authored)==ExpeditionMapJson(DefaultExpeditionMap()));
    }
    std::cout<<"Expedition map passed: 32 complete routes, service skips, currency guards, atomic upgrade bundles, graph validation, transactional JSON.\n";
}
