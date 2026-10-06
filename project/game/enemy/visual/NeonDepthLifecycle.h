#pragma once
// Presentation-only lifecycle. Keep this header free of D3D/model/math/actors
// so existing NeonBossVisualState tests retain their lightweight include graph.
#include <algorithm>
#include <cmath>

namespace neondepth {
enum class Life { Dormant, Intro, Active, Collapse, Dissolve, Finished };
struct LifecycleInput {
    bool encounterActive=false,alive=true,intro=false,aborted=false;
    int hp=1,maxHp=1;
    float introProgress=0;
};
struct LifecycleTiming { float collapseSeconds=.35f,dissolveSeconds=1.2f; };
class Lifecycle {
public:
    void Reset() { *this=Lifecycle{}; }
    void Update(const LifecycleInput& input,const LifecycleTiming& timing,float permittedDelta) {
        deathStarted_=freezeRequested_=releaseRequested_=false;
        collapseDelta_=0;
        if(life_==Life::Finished) return;
        if(life_==Life::Dormant && !input.encounterActive) return;
        if(input.aborted) { life_=Life::Finished; releaseRequested_=true; return; }
        if(life_==Life::Dormant) life_=input.intro?Life::Intro:Life::Active;
        if(life_==Life::Intro || life_==Life::Active) {
            if(!input.alive || input.hp<=0) {
                // Death is irreversible, even if subsequent inputs erroneously
                // raise HP/alive. Only explicit encounter Reset starts a new life.
                timing_={Bounded(timing.collapseSeconds,.15f,.8f)?timing.collapseSeconds:.35f,
                    Bounded(timing.dissolveSeconds,.4f,2)?timing.dissolveSeconds:1.2f};
                totalSeconds_=timing_.collapseSeconds+timing_.dissolveSeconds;
                life_=Life::Collapse; deathStarted_=true; return;
            }
            if(!input.encounterActive) return;
            hpRatio_=input.maxHp>0?(std::clamp)(float(input.hp)/float(input.maxHp),0.0f,1.0f):0;
            introProgress_=Unit(input.introProgress);
            life_=input.intro?Life::Intro:Life::Active;
            return;
        }
        const float delta=std::isfinite(permittedDelta)&&permittedDelta>0?
            (std::min)(permittedDelta,.1f):0;
        const float previous=deathElapsed_;
        deathElapsed_=(std::min)(totalSeconds_,deathElapsed_+delta);
        collapseDelta_=(std::min)(deathElapsed_,timing_.collapseSeconds)-
            (std::min)(previous,timing_.collapseSeconds);
        freezeRequested_=previous<timing_.collapseSeconds && deathElapsed_>=timing_.collapseSeconds;
        if(deathElapsed_>=timing_.collapseSeconds) life_=Life::Dissolve;
        if(deathElapsed_>=totalSeconds_) { life_=Life::Finished; releaseRequested_=true; }
    }
    Life GetLife() const { return life_; }
    float GetHpRatio() const { return hpRatio_; }
    float GetIntroProgress() const { return introProgress_; }
    float GetCollapseProgress() const { return Unit(deathElapsed_/timing_.collapseSeconds); }
    float GetDissolveProgress() const { return life_==Life::Finished?1:
        Unit((deathElapsed_-timing_.collapseSeconds)/timing_.dissolveSeconds); }
    float GetCollapseDelta() const { return collapseDelta_; }
    const LifecycleTiming& GetLatchedTiming() const { return timing_; }
    bool DidDeathStart() const { return deathStarted_; }
    bool ShouldFreezePose() const { return freezeRequested_; }
    bool ShouldReleaseResources() const { return releaseRequested_; }
private:
    static bool Bounded(float v,float low,float high) { return std::isfinite(v)&&v>=low&&v<=high; }
    static float Unit(float v) { return std::isfinite(v)?(std::clamp)(v,0.0f,1.0f):0; }
    Life life_=Life::Dormant;
    LifecycleTiming timing_{};
    float hpRatio_=1,introProgress_=0,deathElapsed_=0,collapseDelta_=0,totalSeconds_=.35f+1.2f;
    bool deathStarted_=false,freezeRequested_=false,releaseRequested_=false;
};
} // namespace neondepth
