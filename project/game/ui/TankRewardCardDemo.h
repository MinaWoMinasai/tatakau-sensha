#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "../player/TankRunModifiers.h"
#include "../player/TankCombatStyleBalance.h"

// A bounded illustration, not a second game world. All coordinates are local to
// one card lane; this code never touches actors, economy, input or random state.
namespace tankreward {
struct Point { float x=0.0f,y=0.0f; };
inline Point Mix(Point a,Point b,float t) { return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t}; }
inline float Saturate(float v) { return (std::clamp)(v,0.0f,1.0f); }
inline float Ease(float v) { v=Saturate(v);return v*v*(3.0f-2.0f*v); }
inline float Length(Point p) {return std::sqrt(p.x*p.x+p.y*p.y);}
inline constexpr float kCycleSeconds=2.8f;
enum class DemoKind { Shooter,Drone,Melee,Homing,Ricochet,Pierce,BladeReach,ImpactDrive,Info };
struct DemoConfig {
    DemoKind kind=DemoKind::Shooter;
    bool homing=false,ricochet=false,pierce=false,bladeReach=false,impactDrive=false;
    bool droneFocus=false,droneGuard=false,meleeTempo=false,finisherCharge=false;
    bool heavy=false,rapid=false,thrusters=false,repair=false;
    bool meleeStyle=false;
    float damageScale=1.0f,reloadScale=1.0f,bulletSpeedScale=1.0f;
    int droneCount=3,barrels=1;
    bool useProfile=false;
    TankCombatStyleProfile profile{};
    TankRunGrowth growth{};
    std::array<float,20> effectPower{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
};
struct Bullet {
    Point position{};
    std::array<Point,8> trail{};
    std::size_t trailCount=0;
    float angle=0.0f;
    bool visible=false;
    int bounces=0;
};
struct DemoSnapshot {
    Point player{0.18f,0.54f},cursor{0.78f,0.54f};
    std::array<Point,3> targets{{{0.72f,0.54f},{0.85f,0.54f},{0.65f,0.54f}}};
    std::array<float,3> hit{};
    std::array<float,3> damage{};
    std::array<Point,6> drones{};
    std::array<Bullet,6> bullets{};
    int targetCount=1,droneCount=0,barrelCount=1;
    bool leftClick=false,slashing=false,dashing=false,wall=false;
    int comboStep=0;
    float slashAngle=0.0f,slashArc=0.0f,slashReach=0.0f;
    float progress=0.0f;
    float muzzleFlash=0.0f;
    int shots=0;
    bool reinforcedProjectiles=false;
};
inline float HitPulse(float now,float at) {return at>=0.0f?Saturate(1.0f-(now-at)/0.28f)*(now>=at?1.0f:0.0f):0.0f;}
inline bool WithinLane(Point p) {return std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>=0.05f&&p.x<=0.95f&&p.y>=0.08f&&p.y<=0.92f;}

// A fixed-step, angle-limited projectile makes homing visibly fallible. The
// direction is never snapped onto a target; no bounce creates another bullet.
inline Bullet SampleProjectile(Point start,Point target,float age,float initialAngle,
    bool homing,bool bounce,bool pierce,int* hitMask=nullptr,float speedScale=1.0f,float homingPower=1.0f) {
    Bullet out;out.position=start;out.angle=initialAngle;
    if(age<0.0f)return out;
    constexpr float step=1.0f/120.0f,turnLimit=1.6f;
    const float speed=0.57f*(std::clamp)(speedScale,0.3f,2.0f);
    out.visible=true;int mask=0;float elapsed=0.0f;
    std::array<Point,3> targets{{target,{0.83f,target.y},{0.92f,target.y}}};
    for(int i=0;i<240&&elapsed<age&&out.visible;++i) {
        const float dt=(std::min)(step,age-elapsed);elapsed+=dt;
        if(homing) {
            const float desired=std::atan2(target.y-out.position.y,target.x-out.position.x);
            const float diff=std::atan2(std::sin(desired-out.angle),std::cos(desired-out.angle));
            const float distance=Length({target.x-out.position.x,target.y-out.position.y});
            // The miniature uses a 20-unit-wide field: 0.70 corresponds to
            // the actor's 14-unit acquisition radius, with the same front cone.
            if(distance<0.70f&&distance>0.01f&&std::cos(diff)>=0.35f)
                out.angle+=(std::clamp)(diff,-turnLimit*dt*homingPower,turnLimit*dt*homingPower);
        }
        out.position.x+=std::cos(out.angle)*speed*dt;
        out.position.y+=std::sin(out.angle)*speed*dt;
        if(out.position.y<0.22f) {
            if(bounce&&out.bounces==0) {out.position.y=0.44f-out.position.y;out.angle=-out.angle;++out.bounces;}
            else {out.position.y=0.22f;out.visible=false;}
        }
        if(out.position.x>0.92f||out.position.y>0.84f||out.position.x<0.08f) {
            out.position.x=(std::clamp)(out.position.x,0.08f,0.92f);
            out.position.y=(std::clamp)(out.position.y,0.22f,0.84f);out.visible=false;
        }
        for(int n=0;n<(pierce?2:1);++n) {
            if((mask&(1<<n))==0&&Length({targets[n].x-out.position.x,targets[n].y-out.position.y})<0.045f) {
                mask|=1<<n;
                if(!pierce||n==1)out.visible=false;
            }
        }
        if(i%5==0) {
            if(out.trailCount<out.trail.size())out.trail[out.trailCount++]=out.position;
            else {for(std::size_t j=1;j<out.trail.size();++j)out.trail[j-1]=out.trail[j];out.trail.back()=out.position;}
        }
    }
    if(hitMask)*hitMask=mask;
    return out;
}
inline DemoSnapshot SampleDemo(const DemoConfig& input,float time) {
    DemoConfig c=input;
    if(c.useProfile) {
        c.profile=SanitizeTankCombatStyleProfile(c.profile);
        c.damageScale*=c.profile.attackDamage/(c.meleeStyle?15.2f:4.0f);
        c.reloadScale*=c.profile.attackIntervalSeconds/(c.meleeStyle?.33f:c.kind==DemoKind::Drone?.5f:1.0f/3.0f);
        c.bulletSpeedScale*=c.profile.bulletSpeed/.27f;
    }
    TankRunModifiers modifiers;modifiers.enabled=true;modifiers.expedition=true;modifiers.effectPower=c.effectPower;
    modifiers.bladeReach=c.bladeReach;modifiers.impactDrive=c.impactDrive;modifiers.meleeTempo=c.meleeTempo;
    modifiers.finisherCharge=c.finisherCharge;modifiers.droneFocus=c.droneFocus;modifiers.droneGuard=c.droneGuard;
    modifiers.heavy=c.heavy;modifiers.rapid=c.rapid;modifiers.thrusters=c.thrusters;modifiers.repair=c.repair;
    const auto runTuning=MakeTankRunTuning(modifiers,c.growth);
    c.damageScale*=runTuning.damage;c.reloadScale*=runTuning.reloadInterval;c.bulletSpeedScale*=runTuning.bulletSpeed;
    DemoSnapshot s;
    const float t=std::fmod((std::max)(0.0f,time),kCycleSeconds);s.progress=t/kCycleSeconds;
    s.leftClick=t>=0.45f&&t<2.15f;
    if(c.kind==DemoKind::Info)return s;
    if(c.kind==DemoKind::Melee||c.kind==DemoKind::BladeReach||
       (c.kind==DemoKind::ImpactDrive&&c.meleeStyle)) {
        s.barrelCount=0;
        s.player={0.23f,0.52f};s.cursor={0.76f,0.52f};s.targets[0]={0.45f,0.52f};
        s.targets[1]={0.56f,0.52f};s.targetCount=c.kind==DemoKind::BladeReach?2:1;
        float nextStart=0.45f;
        for(int i=0;i<3;++i) {
            const auto tuning=MakeTankMeleeCombo(i,modifiers);
            const float reload=(std::clamp)(c.reloadScale,0.01f,60.0f);
            const float at=nextStart+tuning.windup*reload;
            const float duration=tuning.duration*reload;
            nextStart+= (tuning.windup+tuning.duration+tuning.recovery)*reload+0.06f;
            const float reach=0.28f*tuning.range*(c.useProfile?c.profile.meleeRange/4.3f:1.0f);
            if(t>=at&&t<at+duration) {
                s.slashing=true;s.comboStep=i;s.slashReach=reach;
                s.slashArc=tuning.arc*0.0174532925f;
                const float p=(t-at)/duration;
                s.slashAngle=(i==1?-1.0f:1.0f)*(p-0.5f)*s.slashArc;
            }
            const float impactAt=at+duration*0.4f;
            for(int n=0;n<s.targetCount;++n) {
                if(s.targets[n].x-s.player.x>reach+0.02f)continue;
                const float force=tuning.knockback*0.15f*(c.useProfile?c.profile.meleeKnockback/.16f:1.0f);
                s.targets[n].x=(std::min)(.90f,s.targets[n].x+force*Ease((t-impactAt)/0.16f));
                s.hit[n]=(std::max)(s.hit[n],HitPulse(t,impactAt));
                if(t>=impactAt)s.damage[n]+=tuning.damage*c.damageScale*0.17f;
            }
        }
        return s;
    }
    if(c.kind==DemoKind::ImpactDrive) {
        s.player=Mix({0.19f,0.54f},{0.48f,0.54f},Ease((t-0.82f)/0.20f));
        s.dashing=t>=0.82f&&t<1.05f;
        s.targets[0]={(std::min)(.90f,0.55f+.15f*(c.impactDrive?1+.5f*c.effectPower[14]:1)*Ease((t-0.98f)/0.24f)),0.54f};
        s.hit[0]=HitPulse(t,0.98f);return s;
    }
    if(c.kind==DemoKind::Drone) {
        const float move=(c.useProfile?(std::clamp)(c.profile.moveSpeed/.23f,.1f,3.0f):1.0f)*runTuning.moveSpeed;
        const float formation=c.useProfile?(std::clamp)(c.profile.droneFormationRadius/1.5f,0.0f,1.5f):1.0f;
        const float lag=c.useProfile?(std::clamp)(.1f*.25f/c.profile.droneFollowSpeed*5/c.profile.droneResponse,.015f,.45f):.1f;
        auto owner=[&](float at){return Point{.22f+.055f*std::sin(at*2*move),.56f+.1f*std::sin(at*2.5f*move)};};
        auto follower=[&](float at,float angle) {
            auto p=owner(at-lag);p.x+=.11f*formation*std::cos(angle);p.y+=.16f*formation*std::sin(angle);
            // Large authored formations are cropped to the miniature arena.
            p.x=(std::clamp)(p.x,.065f,.9f);p.y=(std::clamp)(p.y,.1f,.9f);return p;
        };
        s.player=owner(t);
        s.cursor={0.80f,0.43f};s.targets[0]=s.cursor;
        s.droneCount=(std::clamp)(c.droneCount,1,6);s.barrelCount=0;
        s.reinforcedProjectiles=c.droneGuard;
        const auto droneTuning=MakeTankDroneTuning(modifiers,true);
        for(int n=0;n<s.droneCount;++n) {
            const float angle=static_cast<float>(n)/static_cast<float>(s.droneCount)*6.2831853f;
            s.drones[n]=follower(t,angle);
            // Holding LEFT CLICK is explicitly displayed during every firing period.
            const float at=.45f+(.17f+static_cast<float>(n)*.09f)*droneTuning.reloadSeconds/.5f*c.reloadScale;
            if(at>=2.15f)continue; // An edited slow attack never fires after LEFT CLICK is released.
            if(t>=at)++s.shots;
            s.muzzleFlash=(std::max)(s.muzzleFlash,HitPulse(t,at));
            const Point launch=follower(at,angle);
            const Point target=s.targets[0];
            int mask=0;s.bullets[n]=SampleProjectile(launch,target,t-at,
                std::atan2(target.y-launch.y,target.x-launch.x),c.homing,false,false,&mask,c.bulletSpeedScale,c.effectPower[9]);
            if(mask) {s.hit[0]=(std::max)(s.hit[0],HitPulse(t,at+Length({target.x-launch.x,target.y-launch.y})/(0.57f*c.bulletSpeedScale)));s.damage[0]+=0.15f*c.damageScale*droneTuning.damageScale;}
        }
        return s;
    }
    Point launch=s.player;float angle=0.0f;
    if(c.kind==DemoKind::Homing) {s.targets[0]={0.73f,0.30f};s.cursor={0.84f,0.54f};}
    if(c.kind==DemoKind::Ricochet) {
        launch={0.16f,0.60f};s.player=launch;s.wall=true;angle=-0.78539816f;
        s.targets[0]={0.77f,0.45f};s.cursor={0.56f,0.22f};
    }
    if(c.kind==DemoKind::Pierce) {s.targetCount=2;s.targets[0]={0.57f,0.54f};s.targets[1]={0.83f,0.54f};}
    s.barrelCount=c.kind==DemoKind::Shooter?(std::clamp)(c.barrels,1,6):1;
    const float shotAt=.45f+.10f*c.reloadScale;
    if(shotAt>=2.15f)return s;
    if(t>=shotAt)s.shots=s.barrelCount;
    s.muzzleFlash=HitPulse(t,shotAt);
    int hits=0;
    for(int n=0;n<s.barrelCount;++n) {
        int shotHits=0;Point origin=launch;
        origin.y+=(static_cast<float>(n)-static_cast<float>(s.barrelCount-1)*0.5f)*0.035f;
        s.bullets[n]=SampleProjectile(origin,s.targets[0],t-shotAt,angle,c.homing,c.ricochet,c.pierce,&shotHits,c.bulletSpeedScale,c.effectPower[9]);
        hits|=shotHits;
    }
    // Hit visualization is taken from the simulation, including the stopping
    // projectile. A miss never turns into a scripted successful hit.
    for(int n=0;n<s.targetCount;++n)if((hits&(1<<n))!=0) {s.hit[n]=0.6f;s.damage[n]=0.4f*c.damageScale;}
    return s;
}
class DemoClock {
public:
    void Update(float dt,bool hovered) {
        if(!hovered){elapsed_=0.0f;return;}
        if(std::isfinite(dt)&&dt>0.0f){elapsed_=std::fmod(elapsed_+(std::clamp)(dt,0.0f,0.1f),kCycleSeconds);++advanceCount_;}
    }
    void Reset(){elapsed_=0.0f;}
    float Elapsed()const{return elapsed_;}
    std::uint64_t AdvanceCount()const{return advanceCount_;}
private:float elapsed_=0.0f;std::uint64_t advanceCount_=0;
};
} // namespace tankreward
