#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Bounded state shared by combat and graphics-free tests. No scene resources.
namespace tankshooter {
inline constexpr int kChainTargets=2;
inline constexpr int kBurstChildren=6;
inline constexpr float kChainRadius=7.0f;
struct ReturnFlight {
    float age=0;
    bool returning=false;
    bool Step(float dt) {
        age+=(std::max)(0.0f,dt);
        if(!returning&&age>=.60f) {returning=true;return true;}
        return false;
    }
};
struct MarkLedger {
    struct Mark {uint64_t id=0;int stacks=0;float remaining=0;};
    std::array<Mark,128> entries{};
    void Update(float dt) {for(auto& m:entries)if(m.id) {m.remaining-=(std::max)(0.0f,dt);if(m.remaining<=0)m={};}}
    bool Hit(uint64_t id,bool boss) {
        Mark* slot=nullptr;
        for(auto& m:entries) {if(m.id==id){slot=&m;break;}if(!slot&&!m.id)slot=&m;}
        if(!slot)return false;
        if(slot->id!=id)*slot={id,0,4};
        slot->remaining=4;
        if(++slot->stacks>=(boss?6:4)) {*slot={};return true;}
        return false;
    }
};
inline uint32_t Damage(uint32_t source,float scale,float power=1) {
    return static_cast<uint32_t>((std::max)(1.0f,std::round(static_cast<float>(source)*scale*(std::clamp)(power,.1f,5.0f))));
}
}
