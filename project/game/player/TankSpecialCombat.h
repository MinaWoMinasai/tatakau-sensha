#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Shared by live combat, card demonstrations and graphics-free regression tests.
// No textures, UI, timers tied to wall time, or eager GPU allocations live here.
namespace tankspecial {
// Live enemy volleys and melee interception share the same durability scale.
// Ordinary shots are cut in one successful hit; boss/sniper shots survive.
inline constexpr float kOrdinaryEnemyBulletHp=6.0f;
inline constexpr float kBossEnemyBulletHp=12.0f;
inline constexpr float kArmoredEnemyBulletHp=24.0f;
struct RailCharge {
    float seconds=0.0f;
    bool held=false;
    void Reset() {seconds=0;held=false;}
    // A negative result means no shot; zero is a valid short tap.
    float Step(bool pressed,float dt,bool ready) {
        if(!ready) {Reset();return -1.0f;}
        if(pressed) {held=true;seconds=(std::min)(1.0f,seconds+(std::max)(0.0f,dt));return -1.0f;}
        if(!held) return -1.0f;
        const float shot=seconds;Reset();return shot;
    }
};
inline float RailDamageScale(float charge,float power=1) {return .65f+4.35f*(std::clamp)(charge,0.0f,1.0f)*(std::clamp)(power,.1f,5.0f);}
inline float RailSpeedScale(float charge) {return 1.8f+1.8f*(std::clamp)(charge,0.0f,1.0f);}
inline float RailRecovery(float reload) {return (std::clamp)(reload*.8f,.12f,.35f);}
inline int LinkCount(int drones) {return drones<2?0:drones==2?1:drones;}
inline float SegmentClosestFraction(float ax,float ay,float bx,float by,float px,float py) {
    const float dx=bx-ax,dy=by-ay,len=dx*dx+dy*dy;
    return len>.00001f?(std::clamp)(((px-ax)*dx+(py-ay)*dy)/len,0.0f,1.0f):0.0f;
}
inline bool SegmentTouches(float ax,float ay,float bx,float by,float px,float py,float radius) {
    const float t=SegmentClosestFraction(ax,ay,bx,by,px,py);
    const float x=ax+(bx-ax)*t-px,y=ay+(by-ay)*t-py;return x*x+y*y<=radius*radius;
}
inline bool SegmentCrossesBox(float ax,float ay,float bx,float by,float minX,float minY,float maxX,float maxY) {
    float nearT=0,farT=1;
    auto slab=[&](float start,float delta,float lo,float hi) {
        if(std::abs(delta)<.00001f)return start>=lo&&start<=hi;
        float a=(lo-start)/delta,b=(hi-start)/delta;if(a>b)std::swap(a,b);
        nearT=(std::max)(nearT,a);farT=(std::min)(farT,b);return nearT<=farT;
    };
    return slab(ax,bx-ax,minX,maxX)&&slab(ay,by-ay,minY,maxY);
}
struct LinkDamageClock {
    struct Target {uint64_t id=0;float next=0;};
    std::array<Target,256> targets{};
    float seconds=0;
    void Advance(float dt) {seconds+=(std::max)(0.0f,dt);}
    bool Claim(uint64_t id) {
        Target* free=nullptr;
        for(auto& target:targets) {
            if(target.id==id) {if(seconds+1e-6f<target.next)return false;target.next=seconds+.20f;return true;}
            if(!free&&(target.id==0||target.next<=seconds))free=&target;
        }
        if(!free)return false;*free={id,seconds+.20f};return true;
    }
};
inline bool EmitsSlashWave(int comboStep) {return comboStep==2;}
inline uint32_t SlashDamage(uint32_t finisherDamage,float power=1) {return static_cast<uint32_t>((std::max)(1.0f,std::round(static_cast<float>(finisherDamage)*.55f*power)));}
inline bool IsPerfectParry(float activeElapsed) {return activeElapsed>=0&&activeElapsed<=.12f;}
inline float ParryDurabilityDamage(float hp,bool perfect,float power=1) {
    if(perfect)return 8.0f*(std::clamp)(power,.1f,5.0f);
    return hp<=kOrdinaryEnemyBulletHp?hp:0.0f;
}
}
