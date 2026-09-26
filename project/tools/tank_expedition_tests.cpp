#include "../game/run/TankExpeditionDirector.h"
#include "../game/run/TankExpeditionEncounters.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace {
using tankexp::ExpeditionDirector;
using tankexp::Phase;
using tankexp::RoomKind;
using tankrun::CardId;

void Check(bool condition,const char* message) {
    if(!condition) { std::cerr<<"FAIL "<<message<<'\n'; std::exit(1); }
}

void CheckPaused(ExpeditionDirector& run) {
    const auto phase=run.GetPhase();
    const auto total=run.GetRunElapsedSeconds(),room=run.GetRoomElapsedSeconds();
    Check(!run.IsCombat()&&!run.Update(1000),"all noncombat phases pause");
    Check(run.GetPhase()==phase&&run.GetRunElapsedSeconds()==total&&run.GetRoomElapsedSeconds()==room,
        "paused phase and both timers remain unchanged");
}

void CheckTerminal(ExpeditionDirector& run,Phase expected) {
    Check(run.GetPhase()==expected,"expected terminal outcome");
    const auto rewards=run.GetRewardCount(),room=run.GetRoomIndex();
    const auto routes=run.GetVisitedRouteIndexes();
    CheckPaused(run);
    Check(!run.Start()&&!run.CompleteRoom()&&!run.ChooseRewardDone(),"terminal rejects start and completion");
    Check(!run.ChooseRoute(0)&&!run.ChooseEvent(0)&&!run.CompleteEvolution()&&!run.MarkDead(),
        "terminal rejects choices and outcome replacement");
    Check(run.GetPhase()==expected&&run.GetRewardCount()==rewards&&run.GetRoomIndex()==room&&
        run.GetVisitedRouteIndexes()==routes,"terminal data remains stable");
}

void CheckReward(ExpeditionDirector& run,bool rare,CardId affinity) {
    Check(run.GetPhase()==Phase::Reward,"completed objective opens a reward");
    Check(run.GetRewardRare()==rare&&run.GetRewardAffinity()==affinity,"reward matches visible route promise");
    CheckPaused(run);
    Check(!run.Start()&&!run.CompleteRoom()&&!run.ChooseRoute(0)&&!run.ChooseEvent(0)&&
        !run.CompleteEvolution()&&!run.MarkDead(),"reward rejects unrelated actions");
    Check(run.ChooseRewardDone(),"reward acknowledged once");
    Check(!run.ChooseRewardDone(),"repeated reward input cannot advance twice");
    Check(!run.GetRewardRare()&&run.GetRewardAffinity()==CardId::Count,"old reward metadata is cleared");
}

void CheckRoom(ExpeditionDirector& run,int index,RoomKind expected) {
    Check(run.GetPhase()==Phase::Combat&&run.IsCombat(),"room enters combat");
    Check(run.GetRoomIndex()==index&&run.GetRoomKind()==expected,"room order and chosen type");
    Check(run.GetRoomElapsedSeconds()==0,"room timer starts from zero");
    Check(!run.Start()&&!run.ChooseRewardDone()&&!run.ChooseRoute(0)&&!run.ChooseEvent(0)&&
        !run.CompleteEvolution(),"combat rejects menu actions");
    const double invalid[]={0,-1,std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()};
    const auto before=run.GetRunElapsedSeconds();
    for(const auto dt:invalid) Check(!run.Update(dt),"invalid combat time is ignored");
    Check(run.GetRoomElapsedSeconds()==0&&run.GetRunElapsedSeconds()==before,"invalid time has no side effects");
    Check(run.Update(1.25)&&run.GetRoomElapsedSeconds()==1.25&&run.GetRunElapsedSeconds()==before+1.25,
        "only combat contributes to both timers");
    Check(run.GetPhase()==Phase::Combat,"time does not complete an objective");
}

void TestEveryExpeditionPath() {
    std::vector<ExpeditionDirector> combatSnapshots;
    for(int first=0;first<2;++first) for(int second=0;second<2;++second) for(int event=0;event<2;++event) {
        ExpeditionDirector run;
        Check(run.GetPhase()==Phase::Dormant&&run.GetRoomIndex()==-1,"dormant expedition has no room");
        Check(run.GetRewardCount()==0&&run.GetEventChoice()==-1&&run.GetRoomCount()==5,"initial counters");
        Check(run.GetVisitedRouteIndexes()==std::array<int,2>{-1,-1},"routes are initially unvisited");
        Check(run.GetRouteChoice(-1)==-1&&run.GetRouteChoice(2)==-1&&
            run.GetRouteChoice((std::numeric_limits<int>::max)())==-1,"invalid route lookup is safe");
        CheckPaused(run);
        Check(!run.CompleteRoom()&&!run.ChooseRewardDone()&&!run.ChooseRoute(0)&&!run.ChooseEvent(0)&&
            !run.CompleteEvolution()&&!run.MarkDead(),"dormant rejects gameplay actions");
        Check(run.Start(),"start expedition");
        CheckRoom(run,0,RoomKind::Skirmish); combatSnapshots.push_back(run);
        Check(run.CompleteRoom(),"first room objective completed");
        Check(run.GetPhase()==Phase::Route&&run.GetRouteRound()==0,"first clear opens route before first upgrade");
        CheckPaused(run);
        Check(!run.ChooseRoute(-1)&&!run.ChooseRoute(2)&&!run.ChooseRoute((std::numeric_limits<int>::max)()),
            "invalid first route has no effect");
        Check(!run.ChooseEvent(0)&&!run.CompleteEvolution()&&!run.MarkDead(),"route rejects unrelated selections");
        Check(run.ChooseRoute(first)&&run.GetRouteChoice(0)==first,"first route recorded");
        CheckReward(run,false,CardId::Rapid);
        CheckRoom(run,1,first==0?RoomKind::Resource:RoomKind::Elite); combatSnapshots.push_back(run);
        Check(run.CompleteRoom(),"second room objective completed");
        CheckReward(run,first==1,CardId::Count);
        Check(run.GetPhase()==Phase::Event&&run.GetRewardCount()==2,"second reward opens event");
        CheckPaused(run);
        Check(!run.ChooseEvent(-1)&&!run.ChooseEvent(2)&&!run.ChooseEvent((std::numeric_limits<int>::max)()),
            "invalid event has no effect");
        Check(!run.CompleteRoom()&&!run.ChooseRoute(0)&&!run.CompleteEvolution()&&!run.MarkDead(),
            "event rejects unrelated actions");
        Check(run.ChooseEvent(event)&&run.GetEventChoice()==event,"event selected and recorded");
        Check(!run.ChooseEvent(event),"event cannot charge or heal twice");
        if(event==1) CheckReward(run,true,CardId::Count);
        Check(run.GetPhase()==Phase::Evolution&&run.GetRewardCount()==2+event,"both events reach evolution");
        CheckPaused(run);
        Check(!run.ChooseRoute(0)&&!run.ChooseEvent(0)&&!run.CompleteRoom()&&!run.MarkDead(),
            "evolution rejects unrelated actions");
        Check(run.CompleteEvolution()&&!run.CompleteEvolution(),"evolution completes once");
        Check(run.GetPhase()==Phase::Route&&run.GetRouteRound()==1,"evolution opens second route");
        CheckPaused(run);
        Check(!run.ChooseRoute(-1)&&!run.ChooseRoute(2),"invalid second route has no effect");
        Check(run.ChooseRoute(second)&&run.GetRouteChoice(1)==second,"second route recorded");
        CheckRoom(run,2,second==0?RoomKind::Reflection:RoomKind::Drone); combatSnapshots.push_back(run);
        Check(run.CompleteRoom(),"third room objective completed");
        CheckReward(run,false,second==0?CardId::Ricochet:CardId::Drones);
        CheckRoom(run,3,RoomKind::Guard); combatSnapshots.push_back(run);
        Check(run.CompleteRoom(),"guard objective completed");
        CheckReward(run,true,CardId::Count);
        CheckRoom(run,4,RoomKind::Boss); combatSnapshots.push_back(run);
        Check(run.GetRunElapsedSeconds()==6.25,"menu time never leaked into expedition combat time");
        Check(run.CompleteRoom(),"boss clear ends expedition");
        Check(run.GetRewardCount()==4+event,"normal route has four rewards and risk route has five");
        Check(run.GetVisitedRouteIndexes()==std::array<int,2>{first,second},"both route choices survive to result");
        CheckTerminal(run,Phase::Clear);
        run.Reset();
        Check(run.GetPhase()==Phase::Dormant&&run.GetRoomIndex()==-1&&run.GetRewardCount()==0&&
            run.GetRunElapsedSeconds()==0&&run.GetRoomElapsedSeconds()==0&&run.GetEventChoice()==-1,
            "reset clears progression and both clocks");
        Check(run.GetVisitedRouteIndexes()==std::array<int,2>{-1,-1}&&run.Start(),"reset permits fresh expedition");
    }
    for(auto run:combatSnapshots) {
        Check(run.MarkDead(),"death is accepted in every combat room");
        CheckTerminal(run,Phase::Dead);
        run.Reset();
        Check(run.Start(),"death can restart after reset");
    }
    ExpeditionDirector large;
    Check(large.Start()&&large.Update((std::numeric_limits<double>::max)()),"finite time accepted without completing room");
    Check(!large.Update((std::numeric_limits<double>::max)())&&std::isfinite(large.GetRunElapsedSeconds()),
        "time addition cannot overflow");
}

void StartCards(tankrun::RunDirector& cards) {
    Check(cards.ChooseLoadout(0)&&cards.ChooseCore(0),"start card inventory");
}

void CheckOffers(const tankrun::RunDirector& cards,bool rare,CardId preferred) {
    std::size_t available=0;
    bool rareAvailable=false,hasRare=false;
    for(std::size_t i=0;i<tankrun::CardCount;++i) if(tankrun::IsAvailableCard(static_cast<CardId>(i))&&cards.GetCardCount(static_cast<CardId>(i))==0) {
        ++available; rareAvailable|=tankrun::IsRare(static_cast<CardId>(i));
    }
    Check(cards.GetOfferCount()==(std::min)(available,std::size_t{3}),"reward fills available distinct choices");
    for(std::size_t i=0;i<cards.GetOfferCount();++i) {
        const auto card=cards.GetOffers()[i];
        Check(tankrun::IsAvailableCard(card)&&cards.GetCardCount(card)==0,"reward offers only unowned available cards");
        for(std::size_t j=0;j<i;++j) Check(cards.GetOffers()[j]!=card,"reward options never duplicate");
        hasRare|=tankrun::IsRare(card);
    }
    if(rare&&rareAvailable) Check(hasRare,"rare reward guarantees unowned rare when available");
    if(tankrun::IsAvailableCard(preferred)&&cards.GetCardCount(preferred)==0)
        Check(cards.GetOffers()[0]==preferred,"route affinity is offered first when unowned");
}

void TestFreeRewardDrafts() {
    tankrun::Config config;
    config.maxDrafts=static_cast<int>(tankrun::AvailableCardCount);
    for(std::uint32_t seed=0;seed<64;++seed) for(int requested=-1;requested<=static_cast<int>(tankrun::CardCount);++requested) {
        tankrun::RunDirector cards(seed,config),replay(seed,config);
        Check(!cards.OpenRewardDraft(true),"cannot grant reward before class and core");
        StartCards(cards); StartCards(replay);
        Check(cards.AddSalvage(37)&&replay.AddSalvage(37),"bank resources before free rewards");
        const auto preferred=static_cast<CardId>(requested);
        for(std::size_t round=0;round<tankrun::AvailableCardCount;++round) {
            const bool rare=round%2==0;
            Check(cards.OpenRewardDraft(rare,preferred)&&replay.OpenRewardDraft(rare,preferred),"free room reward opens without funding");
            CheckOffers(cards,rare,preferred);
            Check(cards.GetSalvage()==37&&cards.GetDraftCount()==static_cast<int>(round),"free reward neither spends resources nor counts before selection");
            const auto offers=cards.GetOffers();
            Check(offers==replay.GetOffers(),"free rewards preserve seeded reproducibility");
            Check(!cards.OpenRewardDraft(false)&&!cards.TryOpenDraft()&&!cards.ChooseCard(cards.GetOfferCount()),
                "open reward rejects reentry and invalid selections");
            Check(cards.GetOffers()==offers&&!cards.Update(1000),"invalid actions and menu time do not change rewards");
            const auto choice=round%cards.GetOfferCount();
            Check(cards.ChooseCard(choice)&&replay.ChooseCard(choice),"select one offered card");
            Check(cards.GetPhase()==tankrun::Phase::Combat&&cards.GetDraftCount()==static_cast<int>(round+1),
                "room reward returns to combat and counts once");
            Check(cards.GetOfferCount()==0&&!cards.ChooseCard(0),"selected reward clears offers and cannot grant twice");
        }
        Check(!cards.OpenRewardDraft(true)&&cards.GetSalvage()==37,"exhausted inventory cannot create empty reward");
    }
    tankrun::RunDirector allRare(7,config);
    StartCards(allRare);
    for(int i=static_cast<int>(CardId::ScatterShot);i<static_cast<int>(CardId::Count);++i) {
        const auto preferred=static_cast<CardId>(i);
        if(!tankrun::IsRare(preferred))continue;
        Check(allRare.OpenRewardDraft(true,preferred),"specific rare reward opens");
        CheckOffers(allRare,true,preferred);
        Check(allRare.ChooseCard(0),"acquire preferred rare");
    }
    Check(allRare.OpenRewardDraft(true,CardId::ScatterShot),"exhausted rare pool falls back to common choices");
    CheckOffers(allRare,true,CardId::ScatterShot);
    for(std::size_t i=0;i<allRare.GetOfferCount();++i) Check(!tankrun::IsRare(allRare.GetOffers()[i]),"owned rares cannot reappear");
    tankrun::RunDirector capped;
    StartCards(capped);
    for(int i=0;i<capped.GetMaxDrafts();++i) Check(capped.OpenRewardDraft(false)&&capped.ChooseCard(0),"configured reward cap permits intended picks");
    Check(!capped.OpenRewardDraft(true),"configured cap also applies to free rewards");
    tankrun::RunDirector boss;
    StartCards(boss);
    Check(boss.Update(150)&&boss.GetPhase()==tankrun::Phase::Boss,"reach old arena boss phase");
    Check(boss.OpenRewardDraft(false)&&boss.ChooseCard(0)&&boss.GetPhase()==tankrun::Phase::Combat,
        "external room reward resumes combat as promised to expedition scene");
    Check(boss.CompleteBoss()&&!boss.OpenRewardDraft(true),"clear cannot open a reward");
    tankrun::RunDirector dead;
    StartCards(dead);
    Check(dead.MarkDead()&&!dead.OpenRewardDraft(true),"dead cannot open a reward");
}

void TestFreeRewardsDoNotSpendArenaRareCredits() {
    bool observedCredit=false;
    for(std::uint32_t seed=0;seed<64;++seed) {
        tankrun::RunDirector banked(seed),control(seed);
        StartCards(banked); StartCards(control);
        Check(banked.ClaimResource(true)&&control.AddSalvage(12),"prepare same salvage with one banked rare credit");
        Check(banked.OpenRewardDraft(false,CardId::Heavy)&&control.OpenRewardDraft(false,CardId::Heavy),"free reward ignores arena credit");
        Check(banked.GetOffers()==control.GetOffers(),"arena credit does not influence external reward");
        Check(banked.ChooseCard(0)&&control.ChooseCard(0),"choose same external reward");
        Check(banked.AddSalvage(4)&&control.AddSalvage(4)&&banked.TryOpenDraft()&&control.TryOpenDraft(),
            "next funded arena refit opens at updated price");
        CheckOffers(banked,true,CardId::Count);
        observedCredit|=banked.GetOffers()!=control.GetOffers();
    }
    Check(observedCredit,"free reward preserved a credit that still affects a later arena refit");
}
} // namespace

int main() {
    for(int i=0;i<=static_cast<int>(RoomKind::Boss);++i) {
        const auto room=static_cast<RoomKind>(i);
        const auto encounter=tankexp::GetEncounter(room);
        Check(encounter.count>0&&encounter.count<=6,"every room has a bounded active enemy formation");
        for(int n=0;n<encounter.count;++n) Check(encounter.units[n].hp>0&&
            (std::string(encounter.units[n].prefab)=="Charger"||std::string(encounter.units[n].prefab)=="Sniper"),
            "formation uses active, damageable enemy roles");
    }
    Check(!tankexp::IsRoomObjectiveComplete(RoomKind::Skirmish,1,0,false,false)&&
        tankexp::IsRoomObjectiveComplete(RoomKind::Skirmish,0,0,false,false),"passive salvage is not an elimination objective");
    Check(!tankexp::IsRoomObjectiveComplete(RoomKind::Guard,1,3,false,false)&&
        !tankexp::IsRoomObjectiveComplete(RoomKind::Guard,0,2,false,false)&&
        tankexp::IsRoomObjectiveComplete(RoomKind::Guard,0,3,false,false),"guard room requires both devices and guards");
    Check(!tankexp::IsRoomObjectiveComplete(RoomKind::Resource,0,0,false,false)&&
        tankexp::IsRoomObjectiveComplete(RoomKind::Resource,0,1,true,false),"core room waits for actual claim");
    Check(!tankexp::IsRoomObjectiveComplete(RoomKind::Boss,0,0,true,false)&&
        tankexp::IsRoomObjectiveComplete(RoomKind::Boss,1,0,false,true),"final objective is the rival, not passive cleanup");
    TestEveryExpeditionPath();
    TestFreeRewardDrafts();
    TestFreeRewardsDoNotSpendArenaRareCredits();
    std::cout<<"Tank expedition PASS: all 8 paths, 5 combat rooms, event/evolution transitions, 4/5 rewards, paused clocks, invalid/terminal states, free seeded rewards, rare/affinity guarantees and duplicate/cap handling\n";
}
