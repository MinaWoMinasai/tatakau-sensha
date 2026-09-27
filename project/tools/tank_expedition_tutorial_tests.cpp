#include "../game/run/TankExpeditionTutorial.h"
#include "../game/run/TankTutorialCopy.h"
#include "../game/run/TankRunCopy.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <string>

using tankexp::ExpeditionTutorial;
using tankexp::TutorialStep;

static void Next(ExpeditionTutorial& tutorial) {
    tutorial.Update(0.01f);
    assert(tutorial.IsSuccess());
    tutorial.Update(0.7f);
}

int main() {
    for(int i=0;i<=static_cast<int>(tankexp::GuidedCombatTutorial::Stage::Complete);++i) {
        const auto copy=tankexp::GuidedTutorialCopy(static_cast<tankexp::GuidedCombatTutorial::Stage>(i));
        assert(copy.heading&&copy.detail);
        assert(std::string(copy.heading).size()<=90&&std::string(copy.detail).size()<=150);
        for(const auto* retired:{"進化","換装","キット"})assert(std::string(copy.detail).find(retired)==std::string::npos);
    }
    assert(tankexp::ShouldSaveTutorialCompletion(false,false,false));
    assert(!tankexp::ShouldSaveTutorialCompletion(false,true,false));
    assert(tankexp::ShouldSaveTutorialCompletion(false,true,true));
    assert(!tankexp::ShouldSaveTutorialCompletion(true,true,true));
    assert(!tankexp::ShouldSaveTutorialCompletion(true,false,false));
    for(int i=0;i<static_cast<int>(tankrun::CardCount);++i) {
        if(i==8||i==12)continue;
        const auto copy=tankrun::copy::ExpeditionCardCopy(i);
        const std::string title=copy.title,body=copy.body;
        assert(!title.empty()&&body.size()<360);
        const auto firstLine=body.find('\n');assert(firstLine!=std::string::npos&&firstLine>0);
        assert(std::count(body.begin(),body.end(),'\n')<=3);
        for(const auto* retired:{"進化","換装","キット","LOCK","PERFECT","RARE","SYNERGY"}) {
            assert(body.find(retired)==std::string::npos&&title.find(retired)==std::string::npos);
        }
    }
    // Expedition wording must not overwrite arena-only damage/tradeoff copy.
    assert(std::string(tankrun::copy::kRunCards[1].body).find("+70%")!=std::string::npos);
    assert(std::string(tankrun::copy::ExpeditionCardCopy(-1).body).size()>0);
    assert(std::string(tankrun::copy::ExpeditionCardCopy(999).body).size()>0);
    ExpeditionTutorial tutorial;
    tutorial.Reset(false);
    assert(!tutorial.Skip());
    tutorial.AddMovement(100); // Room teleport must not satisfy movement.
    tutorial.AddMovement(std::numeric_limits<float>::quiet_NaN());
    tutorial.AddMovement(-2);
    tutorial.Update(1);
    assert(tutorial.GetStep()==TutorialStep::Move && !tutorial.IsSuccess());
    tutorial.AddMovement(2.5f);tutorial.AddMovement(2.5f);Next(tutorial);
    assert(tutorial.GetStep()==TutorialStep::Shoot);
    tutorial.Update(100);
    assert(tutorial.GetStep()==TutorialStep::Shoot); // Shooting alone is not a kill.
    tutorial.RecordKill();Next(tutorial);
    assert(tutorial.GetStep()==TutorialStep::Dash && !tutorial.CanLeaveFirstRoom());
    tutorial.RecordDash();Next(tutorial);
    assert(tutorial.GetStep()==TutorialStep::ClearRoom && tutorial.CanLeaveFirstRoom());
    tutorial.RecordRoomClear();Next(tutorial);
    assert(tutorial.GetStep()==TutorialStep::Route);
    tutorial.RecordRoute();Next(tutorial);
    assert(tutorial.GetStep()==TutorialStep::Upgrade);
    tutorial.RecordUpgrade();Next(tutorial);
    assert(tutorial.GetStep()==TutorialStep::Complete && tutorial.IsComplete() && tutorial.IsVisible());
    tutorial.Update(2);assert(tutorial.IsVisible());
    tutorial.Update(1);assert(!tutorial.IsVisible());

    // Fast play latches events during success flashes; no menu event is lost.
    tutorial.Reset(false);
    tutorial.AddMovement(3);tutorial.AddMovement(2);
    tutorial.RecordKill();tutorial.RecordDash();tutorial.RecordRoomClear();tutorial.RecordRoute();tutorial.RecordUpgrade();
    for(int i=0;i<6;++i) Next(tutorial);
    assert(tutorial.IsComplete());
    tutorial.Reset(true);assert(tutorial.CanSkip() && tutorial.Skip() && !tutorial.IsVisible());
    assert(tutorial.CanLeaveFirstRoom());
    tutorial.Reset(false,true);assert(!tutorial.IsVisible() && tutorial.CanLeaveFirstRoom() && !tutorial.IsComplete());

    tankrun::CardCounts cards{};cards.fill(1);
    auto visible=tankexp::CompactBuildCards(cards);
    int ordinary=0;
    for(size_t i=0;i<cards.size();++i) {
        if(tankrun::IsRare(static_cast<tankrun::CardId>(i))) assert(visible[i]);
        else ordinary+=visible[i]?1:0;
    }
    assert(ordinary==2);
    cards.fill(0);visible=tankexp::CompactBuildCards(cards);
    for(bool shown:visible) assert(!shown);
    std::cout<<"PASS: six actionable tutorial steps, event latching, completion timeout, skip and compact build selection\n";
}
