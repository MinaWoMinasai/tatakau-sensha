#include <cassert>
#include <cmath>
#include <iostream>
#include "../game/player/TankSpecialCombat.h"
#include "../game/player/TankRunModifiers.h"

int main() {
    using namespace tankspecial;
    DroneMission charge;
    assert(charge.Start(false));assert(!charge.Start(true));
    charge.Step(.29f);assert(charge.phase==DronePhase::Warning);
    charge.Step(.011f);assert(charge.phase==DronePhase::Charging);
    charge.Arrive();assert(charge.impact&&charge.phase==DronePhase::Returning);
    charge.Step(.01f,true);assert(charge.phase==DronePhase::Escort);
    DroneMission blocked;blocked.Start(false);blocked.Step(.31f);blocked.Step(1.01f);
    assert(!blocked.impact&&blocked.phase==DronePhase::Returning);
    DroneMission bomb;bomb.Start(true);bomb.Step(.64f);assert(bomb.phase==DronePhase::Warning);
    bomb.Step(.011f);bomb.Arrive();assert(bomb.impact&&!bomb.Available());
    bomb.Step(5.49f);assert(!bomb.Available());assert(bomb.Step(.011f));assert(bomb.Available());
    PainterLock lock;
    for(int i=0;i<8;++i)assert(!lock.Hit(0)); // One drone cannot lock a target alone.
    assert(lock.Hit(1));assert(lock.remaining==4);
    assert(std::abs(lock.DamageScale(false)-1.35f)<.001f);
    assert(std::abs(lock.DamageScale(true)-1.18f)<.001f);
    lock.Advance(4.01f);assert(lock.DamageScale(false)==1&&lock.hits==0);
    lock.Hit(0);lock.Advance(2.1f);assert(lock.hits==0);
    const float distances[]{4,1,3};bool used[]{false,false,false};
    assert(ChooseSpreadTarget(distances,used,3)==1);used[1]=true;
    assert(ChooseSpreadTarget(distances,used,3)==2);used[2]=true;
    assert(ChooseSpreadTarget(distances,used,3)==0);used[0]=true;
    assert(ChooseSpreadTarget(distances,used,3)==1);assert(ChooseSpreadTarget(distances,used,0)==-1);
    SpinCycle spin;float stamina=.9f;
    assert(!spin.Start(true,true,stamina));stamina=3;
    assert(!spin.Start(true,false,stamina));assert(spin.Start(true,true,stamina));assert(stamina==2);
    assert(!spin.Start(true,true,stamina));int ticks=0;
    for(int i=0;i<120;++i)if(spin.Step(1.f/60))++ticks;
    assert(ticks==4&&spin.remaining==0);
    assert(CanDashSlash(true,true,0));assert(CanDashSlash(true,false,.1f));
    assert(!CanDashSlash(true,false,0));assert(!CanDashSlash(false,true,1));
    assert(EmpDuration(2,2.5f)==2.5f&&EmpDuration(2.5f,1)==2.5f&&EmpDuration(0,99)==3);
    assert(WallSmashDamage(20)==35);
    TankRunModifiers modifiers;modifiers.enabled=true;modifiers.expedition=true;
    const auto plain=MakeTankMeleeCombo(0,modifiers);
    modifiers.lightBladeActuator=true;const auto light=MakeTankMeleeCombo(0,modifiers);
    modifiers.heavyBladeEdge=true;const auto both=MakeTankMeleeCombo(0,modifiers);
    assert(light.damage<plain.damage&&light.duration<plain.duration);
    assert(both.damage>plain.damage&&both.duration>light.duration);
    modifiers={};modifiers.enabled=true;modifiers.expedition=true;
    modifiers.drones=true;modifiers.heavyDroneCore=true;
    const auto heavy=MakeTankDroneTuning(modifiers,true);
    assert(std::abs(heavy.damageScale-1.45f)<.001f&&std::abs(heavy.reloadSeconds-.6f)<.001f);
    assert(modifiers.drones&&heavy.sizeScale>1&&heavy.trailScale>1);
    std::cout<<"PASS real drone mission, rebuild, painter lock, spread selection, spin tick, EMP refresh and additive tradeoffs\n";
}
