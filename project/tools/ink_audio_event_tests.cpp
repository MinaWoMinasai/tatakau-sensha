#include "../game/ink/InkSimulation.h"
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
void Check(bool condition,const char* message) { if(!condition) { std::cerr<<"FAIL "<<message<<'\n'; std::exit(1); } }
int Count(const std::vector<ink::AudioEvent>& events,ink::AudioCue cue) {
    int count=0; for(const auto& e:events) if(e.cue==cue) ++count; return count;
}
void Tick(ink::Simulation& sim,ink::Controls c,int steps) { for(int i=0;i<steps;++i) sim.Step(1.0f/120,c); }
ink::WeaponDefinition Bow() { ink::WeaponDefinition d; d.id="test_bow"; d.type=ink::WeaponClass::Stringer; return d; }
}

int main() {
    ink::Simulation sim; ink::Controls c; c.fire=true;
    Tick(sim,c,120);
    auto events=sim.TakeAudioEvents();
    Check(Count(events,ink::AudioCue::ShooterShot)==static_cast<int>(sim.ShotsFired()),"one sound per successful shooter shot");
    Check(sim.TakeAudioEvents().empty(),"event drain only once");
    sim.Equip(Bow()); sim.Reset();
    Tick(sim,c,180); // hold full charge without releasing
    events=sim.TakeAudioEvents();
    Check(Count(events,ink::AudioCue::ChargeStart)==1,"one charge start");
    Check(Count(events,ink::AudioCue::ChargeFirst)==1 && Count(events,ink::AudioCue::ChargeFull)==1,"stage cues once while holding");
    Check(Count(events,ink::AudioCue::StringerShot)==0,"no premature release sound");
    c.fire=false; Tick(sim,c,1); events=sim.TakeAudioEvents();
    Check(Count(events,ink::AudioCue::StringerShot)==1 && events.back().chargeLevel==2,"three-arrow volley is one full shot sound");
    sim.Reset(); c.fire=true; Tick(sim,c,80); sim.TakeAudioEvents();
    c.swim=true; Tick(sim,c,30); events=sim.TakeAudioEvents();
    Check(Count(events,ink::AudioCue::ChargeCancel)==1 && Count(events,ink::AudioCue::StringerShot)==0,"held Shift cancels once without firing sound");
    sim.Reset(); c={}; c.fire=true; Tick(sim,c,80); sim.TakeAudioEvents();
    sim.CancelCharge(); events=sim.TakeAudioEvents();
    Check(Count(events,ink::AudioCue::ChargeCancel)==1,"editor interruption cancels charge cue");
    sim.Reset(); c={}; c.fire=true; Tick(sim,c,160);
    sim.Reset(); Check(sim.TakeAudioEvents().empty(),"reset removes stale audio events");
    sim.SetPlayerPosition({0,0,7}); c={}; c.fire=true; c.aimPoint={0,2,14}; Tick(sim,c,150);
    sim.TakeAudioEvents(); c.fire=false; Tick(sim,c,1); sim.TakeAudioEvents();
    Tick(sim,c,18); events=sim.TakeAudioEvents();
    Check(Count(events,ink::AudioCue::ArrowStick)==3 && Count(events,ink::AudioCue::ArrowBurst)==0,"terrain sticks before delay");
    Tick(sim,c,100); events=sim.TakeAudioEvents();
    Check(Count(events,ink::AudioCue::ArrowBurst)==3,"delayed burst follows actual arrows");
    sim.Equip(ink::WeaponDefinition{}); sim.Reset(); c={}; c.fire=true;
    sim.weapon.inkConsume=0; sim.weapon.repeatFrame=0.5f;
    Tick(sim,c,1200); Check(sim.TakeAudioEvents().size()<=128,"unconsumed presentation queue bounded");
    std::cout<<"Audio events PASS: successful shots, stages, cancel, delayed burst, reset and queue bound\n";
}
