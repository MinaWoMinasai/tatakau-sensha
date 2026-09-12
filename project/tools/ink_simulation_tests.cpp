// Standalone: compile with InkSimulation.cpp; no DirectX, windows.h or game assets.
#include "../game/ink/InkSimulation.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace ink;
namespace {
void Require(bool condition,const char* message) {
    if(!condition) { std::cerr<<"FAIL: "<<message<<'\n'; std::exit(1); }
}
void Advance(Simulation& sim,float seconds,const Controls& controls,float dt=1.0f/120.0f) {
    const int steps=static_cast<int>(std::round(seconds/dt));
    for(int i=0;i<steps;++i) sim.Step(dt,controls);
}
void Fill(Simulation& sim,uint32_t surface,uint32_t team=1) {
    const auto& plane=sim.Surfaces().at(surface);
    sim.Paint({surface,plane.width/2,plane.height/2,100,100,0,team});
}
void MasksAndGeometry() {
    Simulation sim;
    Require(sim.Surfaces().size()==16,"stage fits renderer's sixteen layers");
    const PaintStamp ellipse={0,15,6,1.2f,0.4f,0.6f,1};
    sim.Paint(ellipse);
    Require(sim.GetInkAtWorldPosition({0,0,-8})==1,"paint can be queried at the player's feet");
    Require(sim.GetInkAtWorldPosition({0,1,-8})==0,"ink is local to a surface, not an infinite volume");
    const auto& plane=sim.Surfaces()[0];
    const auto& mask=sim.Mask(0);
    const float c=std::cos(ellipse.angle),s=std::sin(ellipse.angle);
    for(int y=0;y<Simulation::MaskResolution;++y) for(int x=0;x<Simulation::MaskResolution;++x) {
        const float du=(x+0.5f)*plane.width/Simulation::MaskResolution-ellipse.u;
        const float dv=(y+0.5f)*plane.height/Simulation::MaskResolution-ellipse.v;
        const float a=(c*du+s*dv)/ellipse.radiusU,b=(-s*du+c*dv)/ellipse.radiusV;
        Require((mask[y*Simulation::MaskResolution+x]==1)==(a*a+b*b<=1),"CPU mask matches GPU ellipse sample equation");
    }
    Fill(sim,0,2);
    Require(sim.GetInkAtWorldPosition({0,0,-8})==2,"team identity overwrites previous paint");
    Fill(sim,0,0);
    Require(sim.GetInkAtWorldPosition({0,0,-8})==0,"team zero erases paint");
    const auto before=sim.StampCount(); Fill(sim,15);
    Require(sim.StampCount()==before,"metal panel rejects paint");
    Fill(sim,0,3);
    Require(sim.StampCount()==before,"unsupported team IDs are rejected by both CPU and GPU contracts");
    Require(sim.GetInkAtWorldPosition({-2,1,4})==0,"non-inkable panel has no ink");
    Fill(sim,9);
    Require(sim.GetInkAtWorldPosition({0,0,13.8f})==0,"wall ink does not leak onto adjacent dry floor");
    const auto ramp=sim.Raycast({7.5f,6,4},{0,-1,0},10);
    Require(ramp.hit && ramp.surface==4 && std::abs(ramp.position.y-1.5f)<0.001f,"downward ray hits finite inclined ramp");
    Require(!sim.Raycast({7.5f,1,4},{0,-1,0},5).hit,"no hidden floor underneath the ramp");
    const auto wall=sim.Raycast({0,1,10},{0,0,1},10,0.1f);
    Require(wall.hit && wall.surface==9 && std::abs(wall.distance-3.9f)<0.001f,"swept radius detects wall before center crosses it");
    auto pending=sim.TakePendingStamps();
    Require(!pending.empty() && sim.TakePendingStamps().empty(),"GPU stamp transfer drains the queue");
}
void ShootingAndInk() {
    Simulation at60,at120;
    Controls fire; fire.fire=true; fire.aimPoint={0,1,14};
    Advance(at60,2,fire,1.0f/60.0f); Advance(at120,2,fire);
    Require(at60.ShotsFired()==20 && at120.ShotsFired()==20,"six-frame cadence emits twenty shots in two seconds at 60 and 120 Hz");
    Require(std::abs(at60.Player().ink-(1-20*0.0092f))<0.0001f,"tank consumes researched amount per shot");
    Require(at120.StampCount()>20,"projectiles paint irregular impact and airborne droplet stamps");
    const auto painted=at120.GetInkAtWorldPosition({0,0,-7});
    Require(painted==1,"flight droplets make a traversable trail near the muzzle");
    Advance(at120,10,fire);
    Require(at120.ShotsFired()==108,"empty tank prevents additional projectiles");
    Require(at120.Player().ink>=0 && at120.Player().ink<at120.weapon.inkConsume,"tank stays bounded after empty fire");
    const float empty=at120.Player().ink;
    Controls idle; Advance(at120,0.5f,idle);
    Require(at120.Player().ink>empty,"human ink recovery starts after release and recovery lock");
    Simulation human,swim;
    Advance(human,3,fire); Advance(swim,3,fire);
    Fill(swim,0);
    Advance(human,0.5f,idle); Controls submerged; submerged.swim=true; Advance(swim,0.5f,submerged);
    const float humanStart=human.Player().ink,swimStart=swim.Player().ink;
    Advance(human,0.5f,idle); Advance(swim,0.5f,submerged);
    Require(swim.Player().ink-swimStart>2*(human.Player().ink-humanStart),"own-ink swim regenerates over twice as fast as human form");
    Simulation delayed; Advance(delayed,0.1f,fire); const float afterShot=delayed.Player().ink;
    Advance(delayed,0.2f,idle);
    Require(std::abs(delayed.Player().ink-afterShot)<0.0001f,"recovery lock prevents immediate refill after shooting");
    Simulation postShot; Fill(postShot,0);
    while(postShot.ShotsFired()==0) postShot.Step(1.0f/120.0f,fire);
    postShot.Step(1.0f/120.0f,submerged);
    Require(postShot.Player().state==PlayerState::Human,"postshot delay prevents immediate swim transition");
    Advance(postShot,0.1f,submerged);
    Require(postShot.Player().state==PlayerState::Swim,"swim transition becomes available after postshot delay");
}
void MovementAndWalls() {
    Simulation human,swim;
    Fill(swim,0);
    Controls move; move.moveZ=1;
    Controls dive=move; dive.swim=true;
    Advance(human,1,move); Advance(swim,1,dive);
    Require(human.Player().grounded && swim.Player().state==PlayerState::Swim,"human remains grounded and swimmer enters own ink");
    Require(swim.Player().position.z+8>1.7f*(human.Player().position.z+8),"own-ink swim travels substantially faster");
    Fill(swim,0,0); Advance(swim,0.05f,dive);
    Require(swim.Player().state==PlayerState::Human && swim.Player().speed<=swim.movement.humanSpeed+0.001f,"leaving ink exits swim and immediately limits dry-ground speed");
    Simulation side;
    side.SetPlayerPosition({14,0,-8}); Controls right; right.moveX=1; Advance(side,1,right);
    Require(side.Player().position.x<=15-side.movement.humanRadius+0.001f,"human capsule remains inside finite stage walls");
    Simulation ramp; ramp.SetPlayerPosition({7.5f,0,-1}); Advance(ramp,2.5f,move);
    Require(ramp.Player().grounded && ramp.Player().position.y>1.8f,"walking follows ramp height without falling through");
    Require(std::abs(ramp.Player().position.y-ramp.Player().position.z*0.375f)<0.02f,"ramp support follows its real plane equation");
    Simulation rampSide; rampSide.SetPlayerPosition({4,0,4}); Advance(rampSide,1,right);
    Require(rampSide.Player().grounded && rampSide.Player().position.x<=5-rampSide.movement.humanRadius+0.01f && std::abs(rampSide.Player().position.y)<0.001f,"ramp's elevated side blocks entry into the cutout beneath its top");
    Simulation jump; Controls leap; leap.jump=true; jump.Step(1.0f/120.0f,leap);
    Require(!jump.Player().grounded && jump.Player().velocity.y>0,"jump applies upward velocity");
    Require(jump.Player().accuracy>jump.weapon.groundSpread*2,"jump initially increases spread");
    Advance(jump,2,Controls{});
    Require(jump.Player().grounded && std::abs(jump.Player().position.y)<0.001f,"gravity returns player to floor");
    Require(std::abs(jump.Player().accuracy-jump.weapon.groundSpread)<0.01f,"jump accuracy recovers with elapsed time");
    Simulation wall; Fill(wall,9); wall.SetPlayerPosition({0,0,13.7f});
    Advance(wall,0.5f,dive);
    Require(wall.Player().state==PlayerState::WallSwim && wall.Player().position.y>1.5f,"painted wall attaches swimmer and allows upward motion");
    Advance(wall,1.0f,dive);
    Require(wall.Player().position.z>14 && std::abs(wall.Player().position.y-6)<0.01f,"swimmer mantles onto real platform behind wall");
    wall.Reset(); Fill(wall,9); wall.SetPlayerPosition({0,0,13.7f}); Advance(wall,0.3f,dive);
    dive.jump=true; wall.Step(1.0f/120.0f,dive);
    Require(wall.Player().state==PlayerState::Human && wall.Player().velocity.z<0 && wall.Player().velocity.y>0,"wall jump detaches and kicks away from wall");
}
void SustainedResourceBounds() {
    Simulation sim;
    Fill(sim,0); sim.TakePendingStamps();
    size_t mostProjectiles=0,mostDroplets=0,mostStampsPerStep=0,totalStamps=0;
    constexpr int frames=7200;
    for(int frame=0;frame<frames;++frame) {
        Controls controls;
        controls.yaw=frame*0.0028f;
        controls.moveZ=0.55f;
        controls.moveX=0.25f*std::sin(frame*0.003f);
        controls.fire=frame%1440<960;
        controls.swim=!controls.fire;
        controls.jump=frame%480==0;
        const Vec3 forward={std::sin(controls.yaw),0,std::cos(controls.yaw)};
        controls.aimPoint=sim.Player().position+forward*20+Vec3{0,1+std::sin(frame*0.008f)*6,0};
        sim.Step(1.0f/120.0f,controls);
        const auto& player=sim.Player();
        Require(std::isfinite(player.position.x) && std::isfinite(player.position.y) && std::isfinite(player.position.z) &&
            std::isfinite(player.speed) && std::isfinite(player.ink),"sustained movement and firing keep simulation state finite");
        Require(player.ink>=0 && player.ink<=1,"tank stays bounded throughout stress scenario");
        Require(player.position.x>=-15 && player.position.x<=15 && player.position.z>=-14 && player.position.z<=20,"stress movement stays within finite stage");
        mostProjectiles=std::max(mostProjectiles,sim.Projectiles().size());
        mostDroplets=std::max(mostDroplets,sim.Droplets().size());
        const auto pending=sim.TakePendingStamps();
        mostStampsPerStep=std::max(mostStampsPerStep,pending.size()); totalStamps+=pending.size();
        Require(sim.Projectiles().size()<=24,"live projectiles stay bounded by cadence and lifetime");
        Require(sim.Droplets().size()<=400,"live falling droplets stay bounded by spawn cap and lifetime");
        Require(pending.size()<=4096,"render upload stays within per-frame stamp budget");
        Require(sim.TakePendingStamps().empty(),"consumed render stamps are not retained during long play");
    }
    Advance(sim,5,Controls{}); sim.TakePendingStamps();
    Require(sim.Projectiles().empty() && sim.Droplets().empty(),"all projectiles and droplets expire after firing stops");
    Require(sim.ShotsFired()>150 && totalStamps>1000,"stress test exercises multiple tanks and substantial persistent painting");
    for(uint32_t i=0;i<sim.Surfaces().size();++i)
        Require(sim.Mask(i).size()==Simulation::MaskResolution*Simulation::MaskResolution,"persistent mask storage stays fixed in size");
    std::cout<<"Stress: "<<frames<<" frames @120Hz, "<<sim.ShotsFired()<<" shots, peak projectiles="<<mostProjectiles
             <<", droplets="<<mostDroplets<<", stamps/step="<<mostStampsPerStep<<'\n';
}
}
int main() {
    MasksAndGeometry(); ShootingAndInk(); MovementAndWalls(); SustainedResourceBounds();
    std::cout<<"PASS: ink masks, finite surface sweeps, firing cadence, tank/recovery, paint trails, human/swim movement, ramp, jump, wall swim/mantle/jump, sustained resource bounds\n";
}
