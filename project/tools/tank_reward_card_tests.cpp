#include "../game/ui/TankRewardCardDemo.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
using namespace tankreward;
static bool Near(float a,float b){return std::abs(a-b)<0.00001f;}
int main() {
    DemoClock clock;clock.Update(0.04f,true);assert(Near(clock.Elapsed(),0.04f));
    const auto count=clock.AdvanceCount();
    for(int i=0;i<1000;++i)clock.Update(0.016f,false);
    assert(clock.Elapsed()==0&&clock.AdvanceCount()==count);
    clock.Update(std::numeric_limits<float>::quiet_NaN(),true);assert(clock.Elapsed()==0);
    clock.Update(-1,true);assert(clock.Elapsed()==0);
    clock.Update(100,true);assert(Near(clock.Elapsed(),0.1f));
    // Every demo, both lanes, all modifier combinations, and a full loop remain
    // finite and inside the local illustration bounds. No random input exists.
    for(int kind=0;kind<=static_cast<int>(DemoKind::ParryBlade);++kind)for(int flags=0;flags<256;++flags) {
        DemoConfig c;c.kind=static_cast<DemoKind>(kind);c.homing=(flags&1)!=0;c.ricochet=(flags&2)!=0;
        c.pierce=(flags&4)!=0;c.bladeReach=(flags&8)!=0;c.impactDrive=(flags&16)!=0;
        c.meleeTempo=(flags&32)!=0;c.finisherCharge=(flags&64)!=0;c.droneFocus=(flags&128)!=0;
        c.droneCount=3;c.meleeStyle=true;
        c.railCannon=kind==static_cast<int>(DemoKind::RailCannon)&&(flags&1);
        c.droneLaserLink=kind==static_cast<int>(DemoKind::DroneLaserLink)&&(flags&1);
        c.slashWave=kind==static_cast<int>(DemoKind::SlashWave)&&(flags&1);
        c.parryBlade=kind==static_cast<int>(DemoKind::ParryBlade)&&(flags&1);
        for(int step=0;step<168;++step) {
            const auto s=SampleDemo(c,static_cast<float>(step)/60);
            assert(WithinLane(s.player));assert(WithinLane(s.cursor));
            for(int n=0;n<s.targetCount;++n)assert(WithinLane(s.targets[n]));
            for(int n=0;n<s.droneCount;++n)assert(WithinLane(s.drones[n]));
            for(const auto& b:s.bullets)if(b.visible){assert(WithinLane(b.position));assert(b.bounces<=1);}
            assert(s.shots<=3); // no projectile multiplication from hover/effects
            if(s.wave)assert(WithinLane(s.wavePosition));
            if(s.hostileBullet)assert(WithinLane(s.hostilePosition));
        }
    }
    DemoConfig base; base.kind=DemoKind::Homing;auto improved=base;improved.homing=true;
    assert(SampleDemo(base,2.0f).damage[0]==0);assert(SampleDemo(improved,2.0f).damage[0]>0);
    const auto b0=SampleProjectile({.18f,.54f},{.73f,.30f},.2f,0,true,false,false);
    const auto b1=SampleProjectile({.18f,.54f},{.73f,.30f},.2f+1.f/120.f,0,true,false,false);
    assert(std::abs(b1.angle-b0.angle)<=1.6f/120.f+0.00001f);
    assert(SampleProjectile({.18f,.54f},{.08f,.54f},.05f,0,true,false,false).angle==0);
    assert(SampleProjectile({.08f,.84f},{.92f,.22f},.05f,0,true,false,false).angle==0);
    base={};base.kind=DemoKind::Ricochet;improved=base;improved.ricochet=true;
    assert(SampleDemo(base,2.4f).damage[0]==0);assert(SampleDemo(improved,2.4f).damage[0]>0);
    assert(SampleDemo(improved,2.4f).bullets[0].bounces==1);
    base={};base.kind=DemoKind::Pierce;improved=base;improved.pierce=true;
    assert(SampleDemo(base,2.2f).damage[0]>0&&SampleDemo(base,2.2f).damage[1]==0);
    assert(SampleDemo(improved,2.2f).damage[1]>0);
    base={};base.kind=DemoKind::BladeReach;improved=base;improved.bladeReach=true;
    assert(SampleDemo(base,2.2f).damage[1]==0);assert(SampleDemo(improved,2.2f).damage[1]>0);
    base={};base.kind=DemoKind::Melee;improved=base;improved.impactDrive=true;
    assert(SampleDemo(improved,2.2f).targets[0].x>SampleDemo(base,2.2f).targets[0].x);
    improved=base;improved.reloadScale=.85f;
    assert(SampleDemo(improved,1.32f).damage[0]>SampleDemo(base,1.32f).damage[0]);
    improved=base;improved.damageScale=1.45f;
    assert(SampleDemo(improved,2.2f).damage[0]>SampleDemo(base,2.2f).damage[0]);
    base={};base.kind=DemoKind::Drone;base.droneCount=3;
    assert(SampleDemo(base,.4f).shots==0&&!SampleDemo(base,.4f).leftClick);
    assert(SampleDemo(base,1.0f).shots==3&&SampleDemo(base,1.0f).leftClick);
    assert(SampleDemo(base,2.3f).shots==3&&!SampleDemo(base,2.3f).leftClick);
    auto moreDrones=base;moreDrones.droneCount=5;
    assert(SampleDemo(moreDrones,1.3f).shots==5&&SampleDemo(moreDrones,1.3f).droneCount==5);
    DemoConfig multiBarrel;multiBarrel.barrels=3;
    assert(SampleDemo(multiBarrel,1.0f).shots==3);
    auto repeated=SampleDemo(base,1.7f);auto again=SampleDemo(base,1.7f);
    assert(repeated.player.x==again.player.x&&repeated.bullets[0].position.x==again.bullets[0].position.x);
    // F2 profile changes and authored effect powers use the shared combat
    // formula; the private simulation never silently falls back to defaults.
    DemoConfig melee;melee.kind=DemoKind::Melee;melee.meleeStyle=true;melee.useProfile=true;
    melee.profile=DefaultTankCombatStyleBalances()[2];
    auto strong=melee;strong.profile.attackDamage*=2;
    assert(Near(SampleDemo(strong,2.2f).damage[0],SampleDemo(melee,2.2f).damage[0]*2));
    auto slow=melee;slow.profile.attackIntervalSeconds=10;
    assert(SampleDemo(slow,1.0f).damage[0]==0&&!SampleDemo(slow,1.0f).slashing);
    auto reach=melee;reach.bladeReach=true;reach.effectPower[13]=2;
    assert(SampleDemo(reach,.56f).slashReach>SampleDemo(melee,.56f).slashReach*1.59f);
    DemoConfig drone;drone.kind=DemoKind::Drone;drone.useProfile=true;drone.profile=DefaultTankCombatStyleBalances()[1];
    auto wide=drone;wide.profile.droneFormationRadius*=1.5f;
    assert(SampleDemo(wide,1).drones[0].x>SampleDemo(drone,1).drones[0].x);
    auto slowDrone=drone;slowDrone.profile.attackIntervalSeconds=10;
    assert(SampleDemo(slowDrone,2.1f).shots==0&&SampleDemo(slowDrone,2.7f).shots==0);
    DemoConfig heavy;heavy.heavy=true;heavy.growth.damage=.8f;
    auto mild=heavy;mild.growth.damage=.1f;
    assert(SampleDemo(heavy,2).damage[0]>SampleDemo(mild,2).damage[0]);
    for(auto kind:{DemoKind::Melee,DemoKind::Drone,DemoKind::Shooter})for(int high=0;high<2;++high) {
        DemoConfig extreme;extreme.kind=kind;extreme.meleeStyle=kind==DemoKind::Melee;extreme.useProfile=true;
        auto& p=extreme.profile;p.attackIntervalSeconds=high?10:.05f;p.moveSpeed=high?2:.01f;
        p.droneFormationRadius=high?8.0f:0.0f;p.droneFollowSpeed=high?2.0f:.01f;p.droneResponse=high?30.0f:.1f;
        p.meleeRange=high?20.0f:.5f;p.meleeKnockback=high?.95f:0.0f;extreme.droneCount=12;
        for(int frame=0;frame<168;++frame) {
            const auto s=SampleDemo(extreme,frame/60.f);assert(WithinLane(s.player));
            for(int n=0;n<s.droneCount;++n)assert(WithinLane(s.drones[n]));
            for(int n=0;n<s.targetCount;++n)assert(WithinLane(s.targets[n]));
            assert(std::isfinite(s.slashReach)&&std::isfinite(s.damage[0]));
        }
    }
    DemoConfig rail;rail.kind=DemoKind::RailCannon;rail.railCannon=true;
    assert(SampleDemo(rail,1.0f).railCharge>.6f&&SampleDemo(rail,1.0f).shots==0);
    assert(SampleDemo(rail,1.44f).railCharge==1&&SampleDemo(rail,1.46f).shots==1);
    assert(SampleDemo(rail,2.2f).damage[0]>0&&SampleDemo(rail,2.2f).damage[1]>0);
    DemoConfig wave;wave.kind=DemoKind::SlashWave;wave.meleeStyle=true;wave.slashWave=true;
    assert(!SampleDemo(wave,.6f).wave&&!SampleDemo(wave,1.0f).wave);
    bool sawWave=false;for(int i=0;i<168;++i)sawWave|=SampleDemo(wave,i/60.f).wave;assert(sawWave);
    assert(SampleDemo(wave,2.4f).damage[1]>0);
    DemoConfig parry;parry.kind=DemoKind::ParryBlade;parry.meleeStyle=true;parry.parryBlade=true;
    assert(SampleDemo(parry,.3f).hostileBullet);assert(SampleDemo(parry,.58f).perfectParry);
    assert(!SampleDemo(parry,.58f).hostileBullet&&SampleDemo(parry,.58f).bullets[1].visible);
    DemoConfig link;link.kind=DemoKind::DroneLaserLink;link.droneLaserLink=true;
    bool contact=false;for(int i=0;i<168;++i)contact|=SampleDemo(link,i/60.f).hit[1]>0;assert(contact);
    link.droneCount=1;assert(!SampleDemo(link,1).links);
    DemoConfig fan;fan.barrels=3;fan.fanAngle=14;
    const auto spread=SampleDemo(fan,.7f);assert(spread.barrelAngles[0]<0&&spread.barrelAngles[2]>0);
    fan.alternate=true;assert(SampleDemo(fan,.7f).shots<3);
    std::cout<<"Reward card demos passed: 559104 bounded samples, all four special previews, refit geometry, modifier comparisons and inactive clocks.\n";
}
