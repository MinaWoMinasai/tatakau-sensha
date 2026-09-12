#include "../game/ink/InkSimulation.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace ink;
namespace {
constexpr float Dt=1.0f/120;
void Check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
void Steps(Simulation& sim,int count,const Controls& input,float dt=Dt) { while(count-->0) sim.Step(dt,input); }
Controls SwimForward() { Controls c; c.swim=true; c.moveZ=1; return c; }
void Fill(Simulation& sim,uint32_t surface,uint32_t team=1) {
    const auto& s=sim.Surfaces()[surface]; sim.Paint({surface,s.width/2,s.height/2,100,100,0,team});
}
void Lane(Simulation& sim,float start,float end,uint32_t team=1) {
    sim.Paint({0,15,(start+end)/2+14,2,(end-start)/2,0,team});
}
void StartLane(Simulation& sim) { Lane(sim,-12,-4); sim.SetPlayerPosition({0,0,-8}); sim.Step(Dt,{}); }
float CrossIntoDry(Simulation& sim,Controls c=SwimForward()) {
    bool sawOwn=false;
    for(int i=0;i<500;++i) {
        const auto before=sim.Player(); sim.Step(Dt,c);
        sawOwn=sawOwn||sim.Player().onOwnInk;
        if(sawOwn && sim.Player().grounded && sim.Player().state==PlayerState::Squid && !sim.Player().onEnemyInk)
            return before.speed;
    }
    throw std::runtime_error("scenario did not cross the actual owner-mask boundary");
}
void ContinuousBoundaryAndFiniteDecay() {
    Simulation sim; StartLane(sim); const float before=CrossIntoDry(sim);
    Check(before>5 && sim.Player().speed>before*.98f,"dry boundary preserves the preceding swim velocity without an immediate cap");
    Check(!sim.Player().onOwnInk && sim.Player().state==PlayerState::Squid,"carry does not fake submersion or paint ownership");
    Check(sim.DrySquidCarryRemaining()>0,"own-to-dry boundary starts a bounded carry");
    const auto stamps=sim.StampCount(); const float start=sim.Player().position.z;
    float last=sim.Player().speed; auto c=SwimForward();
    for(int n=0;n<36;++n) { sim.Step(Dt,c); Check(sim.Player().speed<=last+.0001f,"holding forward cannot accelerate dry carry"); last=sim.Player().speed; }
    Check(sim.Player().position.z-start>1.0f,"momentum produces visible travel after leaving ink");
    Steps(sim,60,c);
    Check(sim.DrySquidCarryRemaining()==0 && std::abs(sim.Player().speed-sim.movement.drySquidSpeed)<.001f,"carry returns to crawl speed by its deadline");
    Check(sim.StampCount()==stamps,"inertia never adds free paint");
}
void ShortGapAndReentry() {
    Simulation sim; StartLane(sim); Lane(sim,-3.55f,5); CrossIntoDry(sim);
    const auto stamps=sim.StampCount(); auto c=SwimForward(); bool returned=false;
    for(int n=0;n<24;++n) {
        sim.Step(Dt,c);
        if(sim.Player().onOwnInk) { returned=true; break; }
    }
    Check(returned && sim.Player().state==PlayerState::Swim && sim.Player().speed>4,"a .45-world-unit gap is crossed while substantial swim momentum remains");
    Check(sim.DrySquidCarryRemaining()==0,"reentering own ink clears dry carry");
    Steps(sim,30,c);
    Check(std::abs(sim.Player().speed-sim.movement.swimSpeed)<.001f,"reentry accelerates normally to the existing swim cap");
    Check(sim.StampCount()==stamps,"crossing a gap does not fill it with paint");
}
void ReleaseReverseAndSteer() {
    Simulation base; StartLane(base); CrossIntoDry(base);
    Simulation coast=base,released=base,reversed=base,turned=base;
    auto c=SwimForward(),stop=c,back=c,side=c; stop.moveZ=0; back.moveZ=-1; side.moveZ=0; side.moveX=1;
    Steps(coast,6,c); Steps(released,6,stop); Steps(reversed,6,back);
    Check(released.Player().speed<coast.Player().speed && released.Player().velocity.z>0,"release brakes more strongly without deleting momentum in one tick");
    Check(reversed.Player().speed<coast.Player().speed && reversed.Player().velocity.z>0,"reverse input brakes existing motion before turning around");
    float last=turned.Player().speed;
    for(int n=0;n<60;++n) {
        turned.Step(Dt,side);
        Check(turned.Player().speed<=last+.001f || turned.Player().speed<=turned.movement.drySquidSpeed+.001f,"steering never turns stored inertia into extra speed");
        last=turned.Player().speed;
    }
    Steps(released,60,stop); Steps(reversed,60,back);
    Check(released.Player().speed<.001f && released.DrySquidCarryRemaining()==0,"released input settles to rest");
    Check(reversed.Player().velocity.z<0 && reversed.Player().speed<=reversed.movement.drySquidSpeed+.001f,"reverse finishes at the ordinary dry speed");
    Steps(released,30,c); Check(released.Player().speed<=released.movement.drySquidSpeed+.001f,"new dry input cannot revive expired momentum");
}
void EnemyWallAndHuman() {
    Simulation enemy; StartLane(enemy); Lane(enemy,-4,10,2); auto c=SwimForward(); bool touched=false;
    for(int n=0;n<300;++n) {
        enemy.Step(Dt,c);
        if(enemy.Player().onEnemyInk) {
            touched=true; Check(enemy.Player().speed<=enemy.movement.enemyInkSpeed+.001f && enemy.DrySquidCarryRemaining()==0,"enemy ink applies its cap on the actual contact tick"); break;
        }
    }
    Check(touched,"enemy scenario reached enemy ink");
    Simulation wall; Lane(wall,6,12.5f); wall.SetPlayerPosition({0,0,9}); wall.Step(Dt,{}); CrossIntoDry(wall);
    Steps(wall,120,c);
    Check(wall.Player().position.z<=14-wall.movement.swimRadius+.001f && std::abs(wall.Player().velocity.z)<.001f,"dry carry cannot pass through the wall or restore blocked velocity");
    Check(wall.DrySquidCarryRemaining()==0,"wall-stopped carry expires");
    Simulation climb; Lane(climb,6,12.5f); Fill(climb,9); climb.SetPlayerPosition({0,0,9}); climb.Step(Dt,{}); CrossIntoDry(climb);
    for(int n=0;n<120 && climb.Player().state!=PlayerState::WallSwim;++n) climb.Step(Dt,c);
    Check(climb.Player().state==PlayerState::WallSwim && climb.DrySquidCarryRemaining()==0,"wall swim uses its own velocity state and cancels ground carry");
    Simulation human; StartLane(human); CrossIntoDry(human); c.swim=false; human.Step(Dt,c);
    Check(human.Player().state==PlayerState::Human && human.DrySquidCarryRemaining()==0 && human.Player().speed<=human.movement.humanSpeed+.001f,"human transformation cancels carried excess speed");
}
void AirborneAndLanding() {
    auto c=SwimForward(); Simulation swim; Fill(swim,0); Steps(swim,60,c);
    c.jump=true; swim.Step(Dt,c); c.jump=false; Steps(swim,24,c);
    Check(!swim.Player().grounded && swim.Player().speed>swim.movement.swimSpeed*.98f,"normal swim jumps retain their established horizontal speed");
    Simulation dry; StartLane(dry); CrossIntoDry(dry); Steps(dry,12,c);
    c.jump=true; dry.Step(Dt,c); c.jump=false; const float takeoff=dry.Player().speed;
    Check(!dry.Player().grounded && dry.DrySquidCarryRemaining()==0,"jump clears only the ground carry timer");
    Steps(dry,24,c);
    Check(std::abs(dry.Player().speed-takeoff)<.001f,"a jump from dry carry retains its current speed through normal air control");
    for(int n=0;n<120 && !dry.Player().grounded;++n) dry.Step(Dt,c);
    Check(dry.Player().grounded && dry.Player().speed<=dry.movement.drySquidSpeed+.001f && dry.DrySquidCarryRemaining()==0,"landing on dry ground cannot renew the carry or preserve permanent swim speed");
    Simulation ledge; Fill(ledge,10); ledge.SetPlayerPosition({0,6,16}); ledge.Step(Dt,{}); c.moveZ=-1;
    bool fell=false;
    for(int n=0;n<100;++n) { ledge.Step(Dt,c); if(!ledge.Player().grounded) { fell=true; break; } }
    Check(fell && ledge.Player().speed>ledge.movement.swimSpeed*.95f && ledge.DrySquidCarryRemaining()==0,"leaving an inked ledge preserves airborne speed without starting ground carry");
}
void RepeatedGapsAndReset() {
    Simulation sim; Fill(sim,0); sim.SetPlayerPosition({0,0,-10}); sim.Step(Dt,{});
    for(float z=-7;z<10;z+=1.0f) sim.Paint({0,15,z+14,100,.12f,0,0});
    auto c=SwimForward(); int exits=0; bool own=false;
    for(int n=0;n<360;++n) {
        sim.Step(Dt,c); if(own && !sim.Player().onOwnInk) ++exits; own=sim.Player().onOwnInk;
        Check(sim.Player().speed<=sim.movement.swimSpeed+.001f,"repeated gaps and reentry cannot multiply swim speed");
    }
    Check(exits>=8,"repeated-gap scenario traversed multiple actual mask boundaries");
    Simulation reset; StartLane(reset); CrossIntoDry(reset); reset.SetPlayerPosition({0,0,-8}); reset.Step(Dt,c);
    Check(reset.DrySquidCarryRemaining()==0,"debug teleport does not retain the old carry");
    CrossIntoDry(reset); reset.Reset(); reset.Step(Dt,c);
    Check(reset.DrySquidCarryRemaining()==0 && reset.Player().speed<=reset.movement.drySquidSpeed+.001f,"reset clears momentum and starts as normal exposed squid");
}
void RateIndependenceAndTuning() {
    float referenceSpeed=0,referenceDistance=0;
    for(const int fps:{60,120,240}) {
        const float dt=1.0f/fps; Simulation sim; Fill(sim,0); auto c=SwimForward(); Steps(sim,fps,c,dt);
        Fill(sim,0,0); const float start=sim.Player().position.z; Steps(sim,static_cast<int>(std::round(.3f*fps)),c,dt);
        if(fps==60) { referenceSpeed=sim.Player().speed; referenceDistance=sim.Player().position.z-start; }
        Check(std::abs(sim.Player().speed-referenceSpeed)<.003f,"carry decay time is independent of 60/120/240Hz stepping");
        Check(std::abs(sim.Player().position.z-start-referenceDistance)<.03f,"carry distance stays within one coarse integration step across rates");
    }
    Simulation disabled; StartLane(disabled); disabled.movement.drySquidCarryMaxTime=0; CrossIntoDry(disabled);
    Check(disabled.DrySquidCarryRemaining()==0 && disabled.Player().speed<=disabled.movement.drySquidSpeed+.001f,"zero duration intentionally disables ground carry");
    Simulation deadline; StartLane(deadline); deadline.movement.drySquidCarryDeceleration=.001f; deadline.movement.drySquidCarryMaxTime=.25f;
    CrossIntoDry(deadline); auto c=SwimForward(); float last=deadline.Player().speed;
    for(int n=0;n<31;++n) { deadline.Step(Dt,c); Check(last-deadline.Player().speed<.2f,"weak deceleration is corrected gradually instead of snapping at the deadline"); last=deadline.Player().speed; }
    Check(deadline.DrySquidCarryRemaining()==0 && deadline.Player().speed<=deadline.movement.drySquidSpeed+.001f,"the finite deadline still works with nearly zero configured braking");
}
}
int main() {
    int failed=0;
    auto run=[&](const char* name,void(*test)()) { try { test(); std::cout<<"PASS "<<name<<'\n'; } catch(const std::exception& e) { ++failed; std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n'; } };
    run("boundary continuity and finite decay",ContinuousBoundaryAndFiniteDecay);
    run("short gap and own-ink reentry",ShortGapAndReentry);
    run("release, reversal and steering",ReleaseReverseAndSteer);
    run("enemy ink, collision, wall swim and human form",EnemyWallAndHuman);
    run("airborne momentum and landing",AirborneAndLanding);
    run("repeated gaps, teleport and reset",RepeatedGapsAndReset);
    run("60/120/240Hz and tuning bounds",RateIndependenceAndTuning);
    return failed?1:0;
}
