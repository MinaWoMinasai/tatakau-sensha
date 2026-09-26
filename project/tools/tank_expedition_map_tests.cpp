#include "../game/run/TankExpeditionMap.h"
#include "../game/run/TankRunDirector.h"
#include "../game/run/TankGuidedCombatTutorial.h"
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
    // Legacy authored JSON still accepts evolution, but the active run enters
    // an ordinary workshop without changing edges, prices or route length.
    auto legacyJson=ExpeditionMapJson(original);
    for(auto& node:legacyJson["nodes"])if(node["id"]=="evolution")node["kind"]="evolution";
    MapDefinition legacy;assert(ParseExpeditionMap(legacyJson,legacy,error));
    ExpeditionMapRun migrated;assert(migrated.Reset(legacy,error));
    const auto* workshop=FindMapNode(migrated.GetDefinition(),"evolution");
    assert(workshop&&workshop->kind==NodeKind::Upgrade&&workshop->next==FindMapNode(original,"evolution")->next);
    assert(migrated.GetDefinition().nodes.size()==original.nodes.size()&&migrated.GetCurrency()==original.startingCurrency);
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
        {CardId::Rapid,static_cast<CardId>(-1)},{static_cast<CardId>(999),CardId::Heavy},{CardId::Rapid,CardId::ScatterShot}};
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
    for(std::size_t i=0;i<tankrun::CardCount;++i) if(tankrun::IsAvailableCard(static_cast<CardId>(i))) all.push_back(static_cast<CardId>(i));
    assert(run.GrantExpeditionModules(all)&&run.GetDraftCount()==3);
    assert(!run.GrantExpeditionModules(all)&&run.GetDraftCount()==3);
    for(std::size_t i=0;i<tankrun::CardCount;++i)
        assert(run.GetCardCounts()[i]==(tankrun::IsAvailableCard(static_cast<CardId>(i))?1:0));
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
            evolution=evolution||node.id=="evolution";assert(node.kind!=NodeKind::Evolution);
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
void ProceduralProperties() {
    std::set<int> lengths,counts,edgeCounts;
    for(uint32_t seed=0;seed<2048;++seed) {
        const auto map=GenerateExpeditionMap(seed);std::string error;
        assert(ValidateExpeditionMap(map,error)&&!HasThreeConsecutiveNodeKinds(map));
        assert(map.procedural&&map.generationSeed==seed&&map.startingCurrency==20);
        assert(ExpeditionMapJson(map)==ExpeditionMapJson(GenerateExpeditionMap(seed)));
        const auto* upper=FindMapNode(map,"tutorial_combat");const auto* lower=FindMapNode(map,"tutorial_skip");
        const auto* upgrade=FindMapNode(map,"tutorial_upgrade");const auto* skipUpgrade=FindMapNode(map,"skip_upgrade");
        assert(upper&&lower&&upgrade&&skipUpgrade);
        assert(map.startNodes==(std::vector<std::string>{"tutorial_combat","tutorial_skip"}));
        assert(upper->role==NodeRole::TutorialCombat&&lower->role==NodeRole::TutorialSkip);
        assert(upper->roomTemplate=="tutorial_training"&&upper->row==1&&lower->row==3);
        assert(upper->next==std::vector<std::string>{"tutorial_upgrade"}&&lower->next==std::vector<std::string>{"skip_upgrade"});
        assert(upgrade->row==1&&skipUpgrade->row==3&&upgrade->column==1&&skipUpgrade->column==1);
        assert(upgrade->role==NodeRole::TutorialUpgrade&&skipUpgrade->role==NodeRole::IntroUpgrade);
        assert(upgrade->next==skipUpgrade->next&&upgrade->next.size()==1);
        assert(FindMapNode(map,upgrade->next.front())->column==2);
        assert(upgrade->serviceCost==15&&skipUpgrade->serviceCost==15);
        assert(lower->clearReward==upper->clearReward+8);
        const auto* boss=FindMapNode(map,"core");
        assert(boss&&boss->kind==NodeKind::Boss&&boss->roomTemplate=="final_duel"&&boss->next.empty());
        const int length=boss->column+1;
        assert(length>=18&&length<=22);lengths.insert(length);counts.insert(static_cast<int>(map.nodes.size()));
        // Dynamic path lengths prove ALL branches have the requested visit count.
        // Also propagate a route that never enters evolution, proving it optional.
        std::vector<int> minimum(map.nodes.size(),1000),maximum(map.nodes.size(),0);
        std::vector<bool> noEvolution(map.nodes.size(),false);
        int edges=0,evolutionNodes=0;
        for(const auto& id:map.startNodes) {
            const size_t i=static_cast<size_t>(FindMapNode(map,id)-map.nodes.data());
            minimum[i]=maximum[i]=1;noEvolution[i]=true;
        }
        for(size_t i=0;i<map.nodes.size();++i) {
            const auto& node=map.nodes[i];
            assert(minimum[i]!=1000);
            if(node.kind==NodeKind::Evolution) ++evolutionNodes;
            for(const auto& id:node.next) {
                ++edges;
                const auto* next=FindMapNode(map,id);assert(next&&next->column==node.column+1);
                const size_t n=static_cast<size_t>(next-map.nodes.data());
                minimum[n]=(std::min)(minimum[n],minimum[i]+1);maximum[n]=(std::max)(maximum[n],maximum[i]+1);
                noEvolution[n]=noEvolution[n]||(noEvolution[i]&&next->kind!=NodeKind::Evolution);
                if(node.kind==next->kind) for(const auto& thirdId:next->next)
                    assert(FindMapNode(map,thirdId)->kind!=node.kind); // independent triple-path check
            }
        }
        edgeCounts.insert(edges);
        const size_t b=static_cast<size_t>(boss-map.nodes.data());
        assert(minimum[b]==length&&maximum[b]==length&&noEvolution[b]&&evolutionNodes==0);
        if(seed<64) for(int branch=0;branch<4;++branch) {
            ExpeditionMapRun run;assert(run.Reset(map,error));
            while(!run.IsComplete()) {
                const auto options=run.GetAvailableNodeIds();assert(!options.empty());
                assert(run.SelectNode(options[(run.GetVisitedNodeIds().size()+static_cast<size_t>(branch))%options.size()]));
                const auto node=*run.GetActiveNode();
                if(node.kind==NodeKind::Currency) {
                    const int before=run.GetCurrency();assert(!run.CompleteService(false));
                    assert(run.CompleteCurrencyGrant()&&run.GetCurrency()==before+node.clearReward);
                    assert(!run.CompleteCurrencyGrant());
                } else if(IsCombatNode(node.kind)) assert(run.CompleteCombat());
                else assert(run.CompleteService(false));
            }
            assert(run.GetVisitedNodeIds().size()==static_cast<size_t>(length));
        }
    }
    assert(lengths.size()==5&&counts.size()>=10&&edgeCounts.size()>=15);
    auto map=GenerateExpeditionMap(43);
    auto* a=&map.nodes[4];auto* b=&map.nodes[static_cast<size_t>(FindMapNode(map,a->next.front())-map.nodes.data())];
    auto* c=&map.nodes[static_cast<size_t>(FindMapNode(map,b->next.front())-map.nodes.data())];
    for(auto* node:{a,b,c}) {node->kind=NodeKind::Upgrade;node->combatStage=-1;node->clearReward=0;node->roomTemplate.clear();node->role=NodeRole::None;}
    std::string error;assert(HasThreeConsecutiveNodeKinds(map)&&!ValidateExpeditionMap(map,error));
    map.procedural=false;assert(ValidateExpeditionMap(map,error)); // Authored layouts remain author-controlled.

    // Wallet delivery by animated pickups must not be rewarded a second time.
    map=GenerateExpeditionMap(9);ExpeditionMapRun upperRun,lowerRun;
    assert(upperRun.Reset(map,error)&&lowerRun.Reset(map,error));
    assert(upperRun.SelectNode("tutorial_combat")&&upperRun.EarnCurrency(38)&&upperRun.CompleteCombat(false));
    assert(lowerRun.SelectNode("tutorial_skip")&&lowerRun.EarnCurrency(38)&&lowerRun.CompleteCurrencyGrant(false));
    assert(upperRun.GetCurrency()==58&&upperRun.GetCurrency()==lowerRun.GetCurrency());
    assert(!upperRun.CompleteCombat(false)&&!lowerRun.CompleteCurrencyGrant(false));
    const auto custom=GenerateExpeditionMap(9,40,12);assert(FindMapNode(custom,"tutorial_skip")->clearReward==52);
}
void MetadataCompatibility() {
    std::string error;MapDefinition map;
    auto old=ExpeditionMapJson(DefaultExpeditionMap());old["schemaVersion"]=1;old.erase("procedural");old.erase("generationSeed");old.erase("generation");
    for(auto& node:old["nodes"]) node.erase("role");
    assert(ParseExpeditionMap(old,map,error)&&!map.procedural);
    assert(ExpeditionMapJson(map)==ExpeditionMapJson(DefaultExpeditionMap()));
    const auto generated=GenerateExpeditionMap(0xffffffffu);
    auto json=ExpeditionMapJson(generated);
    assert(ParseExpeditionMap(json,map,error)&&ExpeditionMapJson(map)==json);
    for(const auto invalid:{nlohmann::json(-1),nlohmann::json(1.5),nlohmann::json(4294967296ull),nlohmann::json(false)}) {
        auto bad=json;bad["generationSeed"]=invalid;assert(!ParseExpeditionMap(bad,map,error));
    }
    auto bad=json;bad["procedural"]="true";assert(!ParseExpeditionMap(bad,map,error));
    bad=json;bad["nodes"][0]["role"]="unknown";assert(!ParseExpeditionMap(bad,map,error));
    bad=json;bad["nodes"][0]["role"]="intro_upgrade";assert(!ParseExpeditionMap(bad,map,error));
    bad=json;bad["nodes"][1]["serviceCost"]=1;assert(!ParseExpeditionMap(bad,map,error));
    bad=json;bad["schemaVersion"]=4;assert(!ParseExpeditionMap(bad,map,error));
}
void AuthoredGenerationRules() {
    auto settings=DefaultExpeditionMap();settings.procedural=true;settings.startingCurrency=37;
    settings.generationMinColumns=settings.generationMaxColumns=18;
    settings.generationRooms={{NodeKind::Combat,2,7,1,"early"},{NodeKind::Combat,8,13,1,"middle"},
        {NodeKind::Combat,14,31,1,"late"},{NodeKind::Elite,2,31,1,"elite_custom"},{NodeKind::Boss,2,31,1,"boss_custom"}};
    std::string error;
    for(uint32_t seed=0;seed<128;++seed) {
        MapDefinition map,again;
        assert(GenerateExpeditionMap(settings,seed,map,error)&&GenerateExpeditionMap(settings,seed,again,error));
        assert(ExpeditionMapJson(map)==ExpeditionMapJson(again)&&map.startingCurrency==37);
        assert(FindMapNode(map,"core")->column==17&&FindMapNode(map,"core")->roomTemplate=="boss_custom");
        assert(FindMapNode(map,"tutorial_combat")->roomTemplate=="tutorial_training");
        assert(FindMapNode(map,"tutorial_upgrade")->column==1&&FindMapNode(map,"skip_upgrade")->column==1);
        for(const auto& node:map.nodes) {
            if(node.column<2)continue;
            if(node.kind==NodeKind::Combat)assert(node.roomTemplate==(node.column<=7?"early":node.column<=13?"middle":"late"));
            if(node.kind==NodeKind::Elite)assert(node.roomTemplate=="elite_custom");
        }
    }
    // The configured weights must affect actual generated room selection.
    settings.generationRooms={{NodeKind::Combat,2,31,9,"common"},{NodeKind::Combat,2,31,1,"rare"},
        {NodeKind::Elite,2,31,1,"elite_custom"},{NodeKind::Boss,2,31,1,"boss_custom"}};
    int common=0,rare=0;
    for(uint32_t seed=0;seed<4096;++seed) {
        MapDefinition map;assert(GenerateExpeditionMap(settings,seed,map,error));
        const auto* first=FindMapNode(map,"route_2_0");assert(first);
        common+=first->roomTemplate=="common";rare+=first->roomTemplate=="rare";
    }
    assert(common+rare==4096&&common>rare*7&&common<rare*12);
    for(int length=8;length<=32;++length) {
        settings.generationMinColumns=settings.generationMaxColumns=length;
        for(uint32_t seed=0;seed<32;++seed) {
            MapDefinition map;assert(GenerateExpeditionMap(settings,seed,map,error));
            assert(FindMapNode(map,"core")->column==length-1&&!HasThreeConsecutiveNodeKinds(map));
        }
    }
    MapDefinition preserved=DefaultExpeditionMap();const auto before=ExpeditionMapJson(preserved);
    auto checkBad=[&](const MapDefinition& bad) {
        assert(!GenerateExpeditionMap(bad,7,preserved,error)&&!error.empty());
        assert(ExpeditionMapJson(preserved)==before);
    };
    auto bad=settings;bad.generationMinColumns=33;checkBad(bad);
    bad=settings;bad.generationMaxColumns=7;checkBad(bad);
    bad=settings;bad.generationRooms[0].weight=0;checkBad(bad);
    bad=settings;bad.generationRooms[0].firstColumn=1;checkBad(bad);
    bad=settings;bad.generationRooms[0].kind=NodeKind::Upgrade;checkBad(bad);
    bad=settings;bad.generationRooms.push_back(bad.generationRooms[0]);checkBad(bad);
    bad=settings;bad.generationRooms.erase(bad.generationRooms.begin()+2);checkBad(bad);
    bad=settings;bad.generationRooms.back().lastColumn=30;checkBad(bad);
    auto json=ExpeditionMapJson(settings);MapDefinition parsed;
    assert(ParseExpeditionMap(json,parsed,error)&&ExpeditionMapJson(parsed)==json);
    for(const auto invalid:{nlohmann::json(false),nlohmann::json(1.5),nlohmann::json(1001),nlohmann::json(-1)}) {
        auto broken=json;broken["generation"]["rooms"][0]["weight"]=invalid;
        assert(!ParseExpeditionMap(broken,preserved,error)&&ExpeditionMapJson(preserved)==before);
    }
    auto broken=json;broken.erase("generation");assert(!ParseExpeditionMap(broken,preserved,error));
    for(int schema:{1,2}) {
        auto legacy=ExpeditionMapJson(DefaultExpeditionMap());legacy["schemaVersion"]=schema;legacy.erase("generation");
        legacy["startingCurrency"]=59;assert(ParseExpeditionMap(legacy,parsed,error));
        assert(parsed.startingCurrency==59&&parsed.generationRooms.size()==DefaultGenerationRooms().size());
    }
}
void GuidedTutorial() {
    using Tutorial=GuidedCombatTutorial;using Stage=Tutorial::Stage;
    Tutorial guide;guide.Begin(1,4);
    guide.RecordShot();guide.RecordEnemyDefeat();guide.RecordCurrencyCollected(10);
    guide.ObserveDash(true,100);guide.ObserveDash(false,100);
    assert(guide.GetStage()==Stage::Briefing&&!guide.IsCombatReadyToClear()&&!guide.ResolveUpgrade(true));
    assert(guide.Acknowledge()&&guide.GetStage()==Stage::ShootKill);
    assert(!guide.Acknowledge());guide.RecordShot();assert(guide.HasFired());
    guide.RecordEnemyDefeat();assert(guide.GetStage()==Stage::Collect);
    guide.RecordCurrencyCollected(0);guide.RecordCurrencyCollected(-5);guide.RecordCurrencyCollected(3);
    assert(guide.GetStage()==Stage::Collect);
    guide.RecordCurrencyCollected(1);assert(guide.GetStage()==Stage::Vitals);
    guide.ObserveDash(true,100);guide.ObserveDash(false,100);assert(guide.GetStage()==Stage::Vitals);
    assert(guide.Acknowledge()&&guide.GetStage()==Stage::Dash);
    for(int i=0;i<1000;++i) guide.ObserveDash(false,100);
    assert(guide.GetStage()==Stage::Dash&&!guide.IsCombatReadyToClear());
    guide.ObserveDash(true,100);guide.ObserveDash(true,95);guide.ObserveDash(false,100);
    assert(guide.GetStage()==Stage::Dash&&guide.GetFailedDashAttempts()==1);
    guide.RecordDamage();guide.ObserveDash(true,95);guide.ObserveDash(false,95);
    assert(guide.GetStage()==Stage::Dash&&guide.GetFailedDashAttempts()==2); // first-frame hit is not lost
    guide.ObserveDash(true,95);guide.RecordDamage();guide.ObserveDash(true,95);guide.ObserveDash(false,95);
    assert(guide.GetStage()==Stage::Dash&&guide.GetFailedDashAttempts()==3); // damage/heal between samples
    guide.ObserveDash(true,95);guide.ObserveDash(true,95);guide.ObserveDash(false,95);
    assert(guide.GetStage()==Stage::Upgrade&&guide.IsCombatReadyToClear()&&!guide.IsComplete());
    assert(guide.ResolveUpgrade(false)&&guide.IsComplete()&&!guide.DidPurchaseUpgrade()&&!guide.ResolveUpgrade(true));
    guide.BeginUpgradeOnly();assert(guide.GetStage()==Stage::Upgrade&&guide.ResolveUpgrade(true)&&guide.DidPurchaseUpgrade());
    guide.Begin();assert(guide.GetStage()==Stage::Briefing&&guide.GetFailedDashAttempts()==0);
    // Pickup notification may arrive immediately before the defeat callback.
    assert(guide.Acknowledge());guide.RecordShot();guide.RecordCurrencyCollected(4);guide.RecordEnemyDefeat();
    assert(guide.GetStage()==Stage::Vitals);
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
#else
int main(int argc,char** argv) {
#endif
    Validation();Persistence();WalletAndTransitions();AuthoredUpgradeBundles();ProceduralProperties();MetadataCompatibility();GuidedTutorial();AuthoredGenerationRules();
    assert(WalkAllPaths(ExpeditionMapRun{},false)==32);
    assert(WalkAllPaths(ExpeditionMapRun{},true)==32);
    if(argc>1) {
        MapDefinition authored;std::string error;
        const auto path=std::filesystem::path(argv[1]).u8string();
        assert(LoadExpeditionMap(std::string(reinterpret_cast<const char*>(path.data()),path.size()),authored,error));
        assert(ValidateExpeditionMap(authored,error));
    }
    std::cout<<"Expedition map passed: 2048 generated seeds/all-path properties, equal intro economy, guided damage-free dash, 32 legacy routes, transactional JSON and atomic modules.\n";
}
