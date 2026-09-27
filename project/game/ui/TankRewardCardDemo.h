#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "../player/TankRunModifiers.h"
#include "../player/TankCombatStyleBalance.h"
#include "../player/TankSpecialCombat.h"

// A bounded illustration, not a second game world. All coordinates are local to
// one card lane; this code never touches actors, economy, input or random state.
namespace tankreward {
struct Point { float x=0.0f,y=0.0f; };
inline Point Mix(Point a,Point b,float t) { return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t}; }
inline float Saturate(float v) { return (std::clamp)(v,0.0f,1.0f); }
inline float Ease(float v) { v=Saturate(v);return v*v*(3.0f-2.0f*v); }
inline float Length(Point p) {return std::sqrt(p.x*p.x+p.y*p.y);}
inline constexpr float kCycleSeconds=2.8f;
enum class DemoKind { Shooter,Drone,Melee,Homing,Ricochet,Pierce,BladeReach,ImpactDrive,Info,
    RailCannon,DroneLaserLink,SlashWave,ParryBlade,
    ChainLightning,MarkDetonation,BoomerangShell,KillBurst,DroneCharge,DroneRebuildBomb,
    TargetPainter,AutonomousSpread,DashSlash,SpinBlade,WallSmash };
struct DemoConfig {
    DemoKind kind=DemoKind::Shooter;
    bool homing=false,ricochet=false,pierce=false,bladeReach=false,impactDrive=false;
    bool droneFocus=false,droneGuard=false,meleeTempo=false,finisherCharge=false;
    bool heavy=false,rapid=false,thrusters=false,repair=false;
    bool meleeStyle=false;
    bool railCannon=false,droneLaserLink=false,slashWave=false,parryBlade=false;
    bool extraBarrel1=false,extraBarrel2=false,fanMount=false,alternatingFire=false;
    bool heavyDroneCore=false,lightBladeActuator=false,heavyBladeEdge=false;
    bool chainLightning=false,markDetonation=false,boomerangShell=false,killBurst=false;
    bool droneCharge=false,droneRebuildBomb=false,targetPainter=false,autonomousSpread=false;
    bool dashSlash=false,spinBlade=false,wallSmash=false;
    float damageScale=1.0f,reloadScale=1.0f,bulletSpeedScale=1.0f;
    int droneCount=3,barrels=1;
    float fanAngle=0;bool alternate=false;
    bool useProfile=false;
    TankCombatStyleProfile profile{};
    TankRunGrowth growth{};
    decltype(TankRunModifiers{}.effectPower) effectPower=TankRunModifiers{}.effectPower;
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
    std::array<float,6> barrelAngles{};
    int targetCount=1,droneCount=0,barrelCount=1;
    bool leftClick=false,slashing=false,dashing=false,wall=false;
    int comboStep=0;
    float slashAngle=0.0f,slashArc=0.0f,slashReach=0.0f;
    float progress=0.0f;
    float muzzleFlash=0.0f;
    int shots=0;
    bool reinforcedProjectiles=false;
    bool heavyProjectiles=false;
    float railCharge=0,railFlash=0,waveRadius=0,parryFlash=0;
    bool rail=false,links=false,wave=false,perfectParry=false,hostileBullet=false;
    Point wavePosition{},parryPosition{},hostilePosition{};
    std::array<bool,6> linkContact{};
    std::array<Point,3> chainPoints{};int chainCount=0;
    std::array<int,3> marks{};
    std::array<bool,6> droneDisabled{};
    Point blastPosition{};float blast=0,lock=0,droneChargeGlow=0,rebuild=0,spin=0;
    bool returning=false,verticalWall=false;
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
inline DemoSnapshot SampleBaseDemo(const DemoConfig& input,float time) {
    DemoConfig c=input;
    if(c.useProfile) {
        c.profile=SanitizeTankCombatStyleProfile(c.profile);
        c.damageScale*=c.profile.attackDamage/(c.meleeStyle?15.2f:4.0f);
        c.reloadScale*=c.profile.attackIntervalSeconds/(c.meleeStyle?.33f:(c.kind==DemoKind::Drone||c.kind==DemoKind::DroneLaserLink)?.5f:1.0f/3.0f);
        c.bulletSpeedScale*=c.profile.bulletSpeed/.27f;
    }
    TankRunModifiers modifiers;modifiers.enabled=true;modifiers.expedition=true;modifiers.effectPower=c.effectPower;
    modifiers.bladeReach=c.bladeReach;modifiers.impactDrive=c.impactDrive;modifiers.meleeTempo=c.meleeTempo;
    modifiers.finisherCharge=c.finisherCharge;modifiers.droneFocus=c.droneFocus;modifiers.droneGuard=c.droneGuard;
    modifiers.heavy=c.heavy;modifiers.rapid=c.rapid;modifiers.thrusters=c.thrusters;modifiers.repair=c.repair;
    modifiers.heavyDroneCore=c.heavyDroneCore;modifiers.lightBladeActuator=c.lightBladeActuator;modifiers.heavyBladeEdge=c.heavyBladeEdge;
    const auto runTuning=MakeTankRunTuning(modifiers,c.growth);
    c.damageScale*=runTuning.damage;c.reloadScale*=runTuning.reloadInterval;c.bulletSpeedScale*=runTuning.bulletSpeed;
    DemoSnapshot s;
    const float t=std::fmod((std::max)(0.0f,time),kCycleSeconds);s.progress=t/kCycleSeconds;
    s.leftClick=t>=0.45f&&t<2.15f;
    if(c.kind==DemoKind::Info)return s;
    if(c.kind==DemoKind::Melee||c.kind==DemoKind::BladeReach||c.kind==DemoKind::SlashWave||c.kind==DemoKind::ParryBlade||
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
            if(i==2&&c.slashWave) {
                const float age=t-at;
                const float start=s.player.x+reach*.5f;
                const float range=(std::min)(.65f,reach*1.5f);
                s.waveRadius=(std::min)(.12f,reach*.28f);
                s.wave=age>=0&&age<range/.9f;
                s.wavePosition={(std::clamp)(start+.9f*(std::max)(0.0f,age),.05f,.94f),.52f};
                s.targets[1]={.77f,.52f};s.targetCount=2;
                const float hitAt=at+(.77f-start)/.9f;
                if(.77f-start<=range+.03f&&t>=hitAt) {
                    s.damage[1]+=.55f*tuning.damage*c.damageScale*.17f;s.hit[1]=HitPulse(t,hitAt);
                }
            }
            // Incoming bullet reaches the real attack cone just after the
            // first windup, then either passes through or is cut/reflected.
            if(i==0&&c.kind==DemoKind::ParryBlade) {
                const float contact=at+.06f;
                s.parryPosition={s.player.x+(std::min)(reach,.25f),.52f};
                s.hostilePosition={(std::clamp)(s.parryPosition.x+(contact-t)*.72f,.08f,.92f),.52f};
                s.hostileBullet=t>=.15f&&t<contact+(c.parryBlade?0:.35f);
                if(c.parryBlade) {
                    s.perfectParry=t>=contact&&t<contact+.28f;s.parryFlash=HitPulse(t,contact);
                    int mask=0;s.bullets[1]=SampleProjectile(s.parryPosition,{.79f,.52f},t-contact,0,false,false,false,&mask,1.5f);
                    s.targets[1]={.79f,.52f};s.targetCount=2;
                    if(mask){s.damage[1]=.3f*c.damageScale;s.hit[1]=.6f;}
                }
            }
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
    if(c.kind==DemoKind::Drone||c.kind==DemoKind::DroneLaserLink) {
        const float move=(c.useProfile?(std::clamp)(c.profile.moveSpeed/.23f,.1f,3.0f):1.0f)*runTuning.moveSpeed;
        const float formation=(std::clamp)(((c.useProfile?c.profile.droneFormationRadius:1.5f)+(c.droneLaserLink?2.0f:0.0f))/1.5f,0.0f,3.0f);
        const float lag=c.useProfile?(std::clamp)(.1f*.25f/c.profile.droneFollowSpeed*5/c.profile.droneResponse,.015f,.45f):.1f;
        auto owner=[&](float at){return Point{(c.droneLaserLink?.32f:.22f)+.055f*std::sin(at*2*move),.56f+.1f*std::sin(at*2.5f*move)};};
        auto follower=[&](float at,float angle) {
            auto p=owner(at-lag);p.x+=.11f*formation*std::cos(angle);p.y+=.16f*formation*std::sin(angle);
            // Large authored formations are cropped to the miniature arena.
            p.x=(std::clamp)(p.x,.065f,.9f);p.y=(std::clamp)(p.y,.1f,.9f);return p;
        };
        s.player=owner(t);
        s.cursor={0.80f,0.43f};s.targets[0]=s.cursor;
        s.droneCount=(std::clamp)(c.droneCount,1,6);s.barrelCount=0;
        s.reinforcedProjectiles=c.droneGuard;
        s.heavyProjectiles=c.heavyDroneCore;
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
        s.links=c.droneLaserLink&&s.droneCount>=2;
        if(c.kind==DemoKind::DroneLaserLink) {
            // The enemy crosses the moving formation; contact uses segment
            // distance, with only one tick per enemy even at a shared vertex.
            auto crossing=[](float at){return Point{.08f+.48f*Ease((at-.3f)/1.8f),.56f};};
            s.targets[1]=crossing(t);s.targetCount=2;
            bool touching=false;const int count=tankspecial::LinkCount(s.droneCount);
            for(int n=0;s.links&&n<count;++n) {
                const auto a=s.drones[n],b=s.drones[(n+1)%s.droneCount],p=s.targets[1];
                s.linkContact[n]=tankspecial::SegmentTouches(a.x,a.y,b.x,b.y,p.x,p.y,.036f);
                touching|=s.linkContact[n];
            }
            if(touching)s.hit[1]=.8f;
            for(float tick=0;s.links&&tick<=t;tick+=.20f) {
                const auto p=crossing(tick);bool hit=false;
                for(int n=0;n<count&&!hit;++n) {
                    const auto a=follower(tick,n*6.2831853f/s.droneCount),b=follower(tick,((n+1)%s.droneCount)*6.2831853f/s.droneCount);
                    hit=tankspecial::SegmentTouches(a.x,a.y,b.x,b.y,p.x,p.y,.036f);
                }
                if(hit)s.damage[1]+=.10f*c.damageScale;
            }
        }
        return s;
    }
    if(c.railCannon) {
        const float release=1.45f;
        s.rail=true;s.barrelCount=(std::clamp)(c.barrels,1,6);s.targetCount=2;
        s.targets[0]={.57f,.54f};s.targets[1]={.83f,.54f};
        s.leftClick=t>=.35f&&t<release;s.railCharge=s.leftClick?Saturate((t-.35f)/1.0f):0;
        s.railFlash=HitPulse(t,release);s.muzzleFlash=s.railFlash;
        std::array<int,2> hits{};
        for(int n=0;n<s.barrelCount;++n) {
            const float offset=(n-(s.barrelCount-1)*.5f);
            s.barrelAngles[n]=offset*c.fanAngle*.0174532925f;
            if(c.alternate&&n!=0)continue;
            int mask=0;auto origin=s.player;origin.y+=offset*.035f;
            s.bullets[n]=SampleProjectile(origin,s.targets[0],t-release,s.barrelAngles[n],false,false,true,&mask,2);
            for(int target=0;target<2;++target)if(mask&(1<<target))++hits[target];
        }
        if(t>=release)s.shots=c.alternate?1:s.barrelCount;
        for(int n=0;n<2;++n)if(hits[n]){s.damage[n]=.4f*c.damageScale*tankspecial::RailDamageScale(1,c.effectPower[20])*hits[n]*(c.alternate?s.barrelCount:1);s.hit[n]=.8f;}
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
    s.muzzleFlash=HitPulse(t,shotAt);
    std::array<int,3> hits{};
    for(int n=0;n<s.barrelCount;++n) {
        int shotHits=0;Point origin=launch;
        origin.y+=(static_cast<float>(n)-static_cast<float>(s.barrelCount-1)*0.5f)*0.035f;
        const float at=shotAt+(c.alternate?static_cast<float>(n)*.16f*c.reloadScale:0);
        s.barrelAngles[n]=(static_cast<float>(n)-static_cast<float>(s.barrelCount-1)*.5f)*c.fanAngle*.0174532925f;
        if(t>=at)++s.shots;
        if(at>=2.15f)continue;
        s.bullets[n]=SampleProjectile(origin,s.targets[0],t-at,angle+s.barrelAngles[n],c.homing,c.ricochet,c.pierce,&shotHits,c.bulletSpeedScale,c.effectPower[9]);
        for(int target=0;target<s.targetCount;++target)if(shotHits&(1<<target))++hits[target];
    }
    // Hit visualization is taken from the simulation, including the stopping
    // projectile. A miss never turns into a scripted successful hit.
    for(int n=0;n<s.targetCount;++n)if(hits[n]>0) {s.hit[n]=0.6f;s.damage[n]=0.4f*c.damageScale*hits[n];}
    return s;
}
inline DemoSnapshot SampleDemo(const DemoConfig& input,float time) {
    DemoConfig c=input;
    const auto kind=c.kind;
    if(kind>=DemoKind::ChainLightning&&kind<=DemoKind::KillBurst)c.kind=DemoKind::Shooter;
    if(kind>=DemoKind::DroneCharge&&kind<=DemoKind::AutonomousSpread)c.kind=DemoKind::Drone;
    if(kind>=DemoKind::DashSlash&&kind<=DemoKind::WallSmash)c.kind=DemoKind::Melee;
    auto s=SampleBaseDemo(c,time);
    if(kind==DemoKind::WallSmash){s.wall=true;s.verticalWall=true;}
    const float t=std::fmod((std::max)(0.0f,time),kCycleSeconds);
    auto shot=[&](Point from,Point to,float at,int slot,float speed=1.0f) {
        int mask=0;
        s.bullets[slot]=SampleProjectile(from,to,t-at,std::atan2(to.y-from.y,to.x-from.x),false,false,false,&mask,speed);
        return mask;
    };
    if(kind==DemoKind::ChainLightning) {
        s.targetCount=3;s.targets={Point{.66f,.52f},Point{.79f,.32f},Point{.86f,.70f}};
        const bool impact=shot(s.player,s.targets[0],.45f,0)!=0;
        if(c.chainLightning&&impact) {
            s.chainCount=t<1.65f?3:0;s.chainPoints=s.targets;
            for(int i=1;i<3;++i){s.hit[i]=HitPulse(t,1.3f);s.damage[i]=.25f*c.effectPower[31];}
        }
    } else if(kind==DemoKind::MarkDetonation) {
        s.targets[0]={.57f,.54f};s.bullets={};s.damage[0]=0;
        int hits=0;
        for(int n=0;n<4;++n)if(shot(s.player,s.targets[0],.2f+n*.28f,n,1.5f))++hits;
        s.damage[0]=hits*.07f;
        if(c.markDetonation){s.marks[0]=hits<4?hits:0;if(hits==4){s.blast=HitPulse(t,1.5f);s.blastPosition=s.targets[0];s.damage[0]+=.32f*c.effectPower[32];}}
    } else if(kind==DemoKind::BoomerangShell&&c.boomerangShell) {
        s.bullets={};auto& b=s.bullets[0];s.targets[0]={.49f,.54f};
        const float age=t-.4f;b.visible=age>=0&&age<1.4f;s.returning=age>=.7f;
        auto path=[](float age){return Point{.20f+.62f*(age<.7f?Saturate(age/.7f):1-Saturate((age-.7f)/.7f)),.54f};};
        b.position=path((std::max)(0.0f,age));b.angle=s.returning?3.14159f:0;
        for(int n=0;n<8;++n)b.trail[n]=path((std::max)(0.0f,age-(7-n)*.025f));b.trailCount=8;
        s.hit[0]=(std::max)(HitPulse(t,.73f),HitPulse(t,1.47f));s.damage[0]=(t>.73f?.22f:0)+(t>1.47f?.22f:0);
        s.blast=t>=1.1f&&t<1.25f?HitPulse(t,1.1f)*.4f:0;s.blastPosition={.82f,.54f};
    } else if(kind==DemoKind::KillBurst&&c.killBurst&&t>1.25f) {
        s.targetCount=3;s.targets={Point{.68f,.54f},Point{.79f,.32f},Point{.81f,.75f}};
        s.damage[0]=1;s.blast=HitPulse(t,1.25f);s.blastPosition=s.targets[0];
        for(int n=0;n<6;++n) {
            const float a=n*1.04719755f,age=t-1.25f;
            auto& b=s.bullets[n];b.position={.68f+std::cos(a)*age*.35f,.54f+std::sin(a)*age*.35f};
            b.visible=WithinLane(b.position);b.angle=a;b.trailCount=0;
            b.position.x=(std::clamp)(b.position.x,.05f,.95f);b.position.y=(std::clamp)(b.position.y,.08f,.92f);
        }
    } else if((kind==DemoKind::DroneCharge&&c.droneCharge)||(kind==DemoKind::DroneRebuildBomb&&c.droneRebuildBomb)) {
        const Point home=s.drones[0],target=s.targets[0];
        const bool bomb=kind==DemoKind::DroneRebuildBomb;
        s.droneChargeGlow=t>.45f&&t<.8f?(.55f+.4f*std::sin(t*35)):0;
        if(t>=.8f&&t<1.1f)s.drones[0]=Mix(home,target,Ease((t-.8f)/.3f));
        if(t>=1.1f) {
            s.hit[0]=HitPulse(t,1.1f);s.damage[0]+=(bomb?.65f:.28f);
            if(bomb) {
                s.blast=HitPulse(t,1.1f);s.blastPosition=target;s.droneDisabled[0]=t<2.5f;s.bullets[0].visible=false;
                s.rebuild=t>1.8f?Saturate((t-1.8f)/.7f):0;
            } else s.drones[0]=Mix(target,home,Ease((t-1.1f)/.55f));
        }
    } else if(kind==DemoKind::TargetPainter&&c.targetPainter) {
        s.lock=t>=1.15f?Saturate((t-1.15f)/.3f):0;
        if(t>1.5f)s.damage[0]*=1.35f;
    } else if(kind==DemoKind::AutonomousSpread&&c.autonomousSpread) {
        s.targetCount=3;s.targets={Point{.75f,.3f},Point{.82f,.52f},Point{.73f,.76f}};s.damage={};s.hit={};
        for(int n=0;n<s.droneCount;++n)if(shot(s.drones[n],s.targets[n%3],.45f+n*.05f,n)){s.hit[n%3]=.65f;s.damage[n%3]+=.14f;}
    } else if(kind==DemoKind::DashSlash&&c.dashSlash) {
        s.player=Mix({.18f,.52f},{.53f,.52f},Ease((t-.48f)/.30f));
        s.dashing=t>.48f&&t<.80f;
        if(s.dashing){s.slashing=true;s.slashReach=.30f;s.slashArc=1.7f;s.slashAngle=.1f;s.comboStep=0;}
        if(t>=.66f){s.hit[0]=(std::max)(s.hit[0],HitPulse(t,.66f));s.damage[0]+=.30f;}
    } else if(kind==DemoKind::SpinBlade&&c.spinBlade&&t>=1.8f&&t<2.6f) {
        s.spin=(t-1.8f)/.8f;s.slashing=true;s.slashReach=.22f;s.slashAngle=s.spin*18.8495559f;s.slashArc=6.2831853f;
        s.targetCount=3;s.targets={Point{.44f,.52f},Point{.23f,.27f},Point{.10f,.68f}};
        for(int n=0;n<3;++n){s.hit[n]=HitPulse(std::fmod(t-1.8f,.2f),0);s.damage[n]+=.08f*std::floor((t-1.8f)/.2f);}
        s.parryFlash=c.parryBlade?s.hit[0]*.45f:0;s.parryPosition={.40f,.50f};s.perfectParry=false;
    } else if(kind==DemoKind::WallSmash&&c.wallSmash) {
        s.wall=true;s.targetCount=2;s.targets[1]={.81f,.35f};
        s.targets[0]=Mix({.46f,.52f},{.87f,.52f},Ease((t-1.1f)/.35f));
        if(t>1.45f){s.blast=HitPulse(t,1.45f);s.blastPosition=s.targets[0];s.damage[0]+=.45f;s.damage[1]=.18f;s.hit[1]=s.blast;}
    }
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
