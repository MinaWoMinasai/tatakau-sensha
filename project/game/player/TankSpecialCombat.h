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
inline constexpr float kRailMaxChargeSeconds=1.0f;
inline constexpr float kRailTapDamageScale=0.65f;
inline constexpr float kRailChargeDamageGain=4.35f;
inline constexpr float kRailTapSpeedScale=1.8f;
inline constexpr float kRailChargeSpeedGain=1.8f;
inline constexpr float kMinAbilityPower=0.1f;
inline constexpr float kMaxAbilityPower=5.0f;
inline constexpr float kRailRecoveryReloadScale=0.8f;
inline constexpr float kRailMinRecoverySeconds=0.12f;
inline constexpr float kRailMaxRecoverySeconds=0.35f;
inline constexpr float kLinkHitIntervalSeconds=0.20f;
inline constexpr float kSlashWaveDamageScale=0.55f;
inline constexpr float kPerfectParryWindowSeconds=0.12f;
inline constexpr float kPerfectParryDurabilityDamage=8.0f;
struct RailCharge {
    float seconds=0.0f;
    bool held=false;
    void Reset() {seconds=0;held=false;}
    // A negative result means no shot; zero is a valid short tap.
    float Step(bool pressed,float dt,bool ready) {
        if(!ready) {Reset();return -1.0f;}
        if(pressed) {held=true;seconds=(std::min)(kRailMaxChargeSeconds,seconds+(std::max)(0.0f,dt));return -1.0f;}
        if(!held) return -1.0f;
        const float shot=seconds;Reset();return shot;
    }
};
inline float RailDamageScale(float charge,float power=1) {return kRailTapDamageScale+kRailChargeDamageGain*(std::clamp)(charge,0.0f,1.0f)*(std::clamp)(power,kMinAbilityPower,kMaxAbilityPower);}
inline float RailSpeedScale(float charge) {return kRailTapSpeedScale+kRailChargeSpeedGain*(std::clamp)(charge,0.0f,1.0f);}
inline float RailRecovery(float reload) {return (std::clamp)(reload*kRailRecoveryReloadScale,kRailMinRecoverySeconds,kRailMaxRecoverySeconds);}
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
            if(target.id==id) {if(seconds+1e-6f<target.next)return false;target.next=seconds+kLinkHitIntervalSeconds;return true;}
            if(!free&&(target.id==0||target.next<=seconds))free=&target;
        }
        if(!free)return false;*free={id,seconds+kLinkHitIntervalSeconds};return true;
    }
};
inline bool EmitsSlashWave(int comboStep) {return comboStep==2;}
inline uint32_t SlashDamage(uint32_t finisherDamage,float power=1) {return static_cast<uint32_t>((std::max)(1.0f,std::round(static_cast<float>(finisherDamage)*kSlashWaveDamageScale*power)));}
inline bool IsPerfectParry(float activeElapsed) {return activeElapsed>=0&&activeElapsed<=kPerfectParryWindowSeconds;}
inline float ParryDurabilityDamage(float hp,bool perfect,float power=1) {
    if(perfect)return kPerfectParryDurabilityDamage*(std::clamp)(power,kMinAbilityPower,kMaxAbilityPower);
    return hp<=kOrdinaryEnemyBulletHp?hp:0.0f;
}

// Run-local, bounded state shared by the real actors and headless tests.
enum class DronePhase { Escort, Warning, Charging, Returning, Rebuilding };
class DroneMission {
public:
    static constexpr float kWarningSeconds = 0.30f;
    static constexpr float kBombWarningSeconds = 0.65f;
    static constexpr float kChargingTimeoutSeconds = 1.0f;
    static constexpr float kReturnTimeoutSeconds = 2.5f;
    static constexpr float kRebuildSeconds = 5.5f;
    DronePhase GetPhase() const { return phase_; }
    float GetElapsed() const { return elapsed_; }
    float GetWarningDuration() const { return bomb_ ? kBombWarningSeconds : kWarningSeconds; }
    bool IsBomb() const { return bomb_; }
    bool HasImpact() const { return impact_; }
    bool ConsumeImpact() { const bool hit = impact_; impact_ = false; return hit; }
    bool Start(bool explosive) {
        if(phase_!=DronePhase::Escort)return false;
        Enter(DronePhase::Warning);bomb_=explosive;impact_=false;return true;
    }
    void Arrive() {
        if(phase_!=DronePhase::Charging)return;
        impact_=true;Enter(bomb_?DronePhase::Rebuilding:DronePhase::Returning);
    }
    bool Step(float dt,bool home=false) {
        elapsed_+=(std::max)(0.0f,dt);
        const State& state = GetState(phase_);
        return state.Update(*this, home);
    }
    bool Available() const {return phase_!=DronePhase::Rebuilding;}
private:
    class State {
    public:
        virtual ~State() = default;
        virtual bool Update(DroneMission& mission, bool home) const = 0;
    };
    class EscortState final : public State {
    public:
        bool Update(DroneMission&, bool) const override { return false; }
    };
    class WarningState final : public State {
    public:
        bool Update(DroneMission& mission, bool) const override {
            if (mission.elapsed_ >= mission.GetWarningDuration()) mission.Enter(DronePhase::Charging);
            return false;
        }
    };
    class ChargingState final : public State {
    public:
        bool Update(DroneMission& mission, bool) const override {
            if (mission.elapsed_ >= kChargingTimeoutSeconds) mission.Enter(DronePhase::Returning);
            return false;
        }
    };
    class ReturningState final : public State {
    public:
        bool Update(DroneMission& mission, bool home) const override {
            if (home || mission.elapsed_ >= kReturnTimeoutSeconds) mission.Enter(DronePhase::Escort);
            return false;
        }
    };
    class RebuildingState final : public State {
    public:
        bool Update(DroneMission& mission, bool) const override {
            if (mission.elapsed_ < kRebuildSeconds) return false;
            mission.Enter(DronePhase::Escort);
            return true;
        }
    };
    static const State& GetState(DronePhase phase) {
        static const EscortState escort;
        static const WarningState warning;
        static const ChargingState charging;
        static const ReturningState returning;
        static const RebuildingState rebuilding;
        static const std::array<const State*, 5> states{&escort, &warning, &charging, &returning, &rebuilding};
        static_assert(states.size() == static_cast<std::size_t>(DronePhase::Rebuilding) + 1);
        const auto index = static_cast<std::size_t>(phase);
        return *states[index < states.size() ? index : 0];
    }
    void Enter(DronePhase next) { phase_ = next; elapsed_ = 0; }
    DronePhase phase_=DronePhase::Escort;
    float elapsed_=0;
    bool bomb_=false, impact_=false;
};
struct PainterLock {
    uint64_t id=0;
    uint32_t droneMask=0;
    int hits=0;
    float remaining=0, buildup=0;
    void Advance(float dt) {
        remaining=(std::max)(0.0f,remaining-dt);buildup=(std::max)(0.0f,buildup-dt);
        if(buildup==0&&remaining==0){droneMask=0;hits=0;}
    }
    bool Hit(int drone) {
        if(remaining>0||drone<0||drone>=32)return false;
        droneMask|=uint32_t{1}<<drone;++hits;buildup=2.0f;
        const bool distinct=(droneMask&(droneMask-1))!=0;
        if(distinct&&hits>=4) {remaining=4.0f;return true;}return false;
    }
    float DamageScale(bool boss,float power=1) const {return remaining>0?1.0f+(boss?.18f:.35f)*power:1.0f;}
};
inline int ChooseSpreadTarget(const float* distances,const bool* used,int count) {
    int selected=-1;float best=1e30f;
    for(int pass=0;pass<2&&selected<0;++pass)for(int i=0;i<count;++i) {
        if(pass==0&&used[i])continue;
        if(distances[i]<best){best=distances[i];selected=i;}
    }
    return selected;
}
struct SpinCycle {
    float remaining=0,nextTick=0;
    int tickCount=0;
    bool Start(bool held,bool afterFinisher,float& stamina) {
        if(remaining>0||!held||!afterFinisher||stamina<1)return false;
        stamina-=1;remaining=.8f;nextTick=0;tickCount=0;return true;
    }
    bool Step(float dt) {
        if(remaining<=0)return false;
        remaining=(std::max)(0.0f,remaining-dt);nextTick-=dt;
        if(nextTick<=0&&tickCount<4){nextTick+=.20f;++tickCount;return true;}return false;
    }
};
inline float EmpDuration(float current,float requested) {return (std::max)(current,(std::clamp)(requested,0.0f,3.0f));}
inline bool CanDashSlash(bool enabled,bool dashing,float recentDash) {return enabled&&(dashing||recentDash>0);}
inline float WallSmashDamage(float melee,float power=1) {return (std::max)(1.0f,std::round(melee*1.75f*power));}
}
