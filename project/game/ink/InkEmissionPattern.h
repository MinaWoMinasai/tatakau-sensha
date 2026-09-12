#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace ink {
// Source values: Leanny's S3 11.3.0 WeaponShooterNormal parameter dump,
// commit 7280ff9cde8bb1c5dcef46c700c326471584d2e6. Distances use CG2 scale 0.5.
// Neither that JSON nor XarrotD's parameter notes publishes the native phase
// order, fractional-count rounding, or RNG algorithm. The planner below is an
// explicit CG2 approximation: fractional budgets + stratified phase coverage,
// not a claim that Splatoon chooses an independent random count each shot.
struct EmissionSettings {
    float sourceSpawnNum=1.5f;
    int sourceSplitNum=8;
    float sourceBetweenDistance=9.2f*0.5f;
    float sourceNearestDistance=1.2f*0.5f;
    float maxTravelDistance=6.75f; // CG2 allocation bound, not a main-shot lifetime
    float widthScaleMin=0.92f, widthScaleMax=1.12f;
    float lateralJitter=0.06f;
    float phaseJitter=0.12f; // fraction of a phase stratum, not a world distance
    int maxDryShots=4; // CG2 bound; not a decoded ForceSpawnNearestAddNumArray
};

struct DropEmission {
    float distance=0;
    float widthScale=1;
    float lateralOffset=0; // along the shot's horizontal right axis
};

struct ShotEmissionPlan {
    static constexpr int Capacity=12;
    std::array<DropEmission,Capacity> drops{};
    int count=0;
    int requestedDropCount=0; // before range clipping; fractional-budget evidence
    int phase=0;
    bool footRescue=false;
    float footForward=0.36f, footLateral=0.10f, footWidthScale=1.06f;
    int scatterCount=3;
};

class EmissionPattern {
public:
    explicit EmissionPattern(uint32_t seed=0x6c8e9cf5u) { Reset(seed); }

    void Reset(uint32_t seed=0x6c8e9cf5u) {
        randomState_=seed?seed:0x6c8e9cf5u;
        budgetCarry_=0; phaseCount_=0; phaseCursor_=0;
        dryShots_=0; nextDryLimit_=3;
    }

    // Call exactly once per actual main shot, store the result on that shot,
    // then consume its distances only at swept travel crossings. Rendering,
    // impact ordering, brushes and frame rate must never call this function.
    // groundedDryFeet must include grounding: airborne shots do not get a free
    // projection onto a floor. Rescue is still a real colliding droplet.
    ShotEmissionPlan NextShot(const EmissionSettings& settings,bool groundedDryFeet) {
        ShotEmissionPlan plan;
        const int phaseCount=(std::clamp)(settings.sourceSplitNum,1,MaxPhases);
        if(phaseCount!=phaseCount_) RefillPhases(phaseCount);
        else if(phaseCursor_>=phaseCount_) phaseCursor_=0;
        plan.phase=phases_[phaseCursor_++];

        const float spawnNum=FiniteClamp(settings.sourceSpawnNum,0.0f,static_cast<float>(ShotEmissionPlan::Capacity),1.5f);
        budgetCarry_+=spawnNum;
        plan.requestedDropCount=static_cast<int>(std::floor(budgetCarry_));
        budgetCarry_-=plan.requestedDropCount;
        budgetCarry_=(std::clamp)(budgetCarry_,0.0,0.999999999);

        const float between=FiniteClamp(settings.sourceBetweenDistance,0.1f,50.0f,4.6f);
        const float nearest=FiniteClamp(settings.sourceNearestDistance,0.0f,50.0f,0.6f);
        const float end=FiniteClamp(settings.maxTravelDistance,0.0f,100.0f,6.75f);
        const float jitter=FiniteClamp(settings.phaseJitter,0.0f,0.24f,0.12f);
        const float phaseOffset=(std::max)(0.0f,static_cast<float>(plan.phase)+(Unit()*2-1)*jitter);
        const float first=nearest+between/phaseCount*phaseOffset;
        const float widthMin=FiniteClamp(settings.widthScaleMin,0.1f,2.0f,0.92f);
        const float widthMax=(std::max)(widthMin,FiniteClamp(settings.widthScaleMax,0.1f,2.0f,1.12f));
        const float lateral=FiniteClamp(settings.lateralJitter,0.0f,0.3f,0.06f);
        // Correlated shot width gives a coherent amount of liquid, with smaller
        // variation between drops. It avoids unrelated large/small coin flips.
        const float shotWidth=Unit();
        for(int n=0;n<plan.requestedDropCount;++n) {
            const float distance=first+between*n;
            const float widthUnit=(std::clamp)(shotWidth+(Unit()-0.5f)*0.18f,0.0f,1.0f);
            const float offset=(Unit()*2-1)*lateral;
            if(distance>end) continue;
            plan.drops[plan.count++]={distance,widthMin+(widthMax-widthMin)*widthUnit,offset};
        }

        const int maxDry=(std::clamp)(settings.maxDryShots,1,12);
        if(groundedDryFeet) {
            ++dryShots_;
            if(dryShots_>=(std::min)(nextDryLimit_,maxDry)) {
                plan.footRescue=true;
                dryShots_=0;
                // Limited variation avoids permanent dry feet from an unlucky
                // run. It intentionally does not reinterpret the source [4].
                nextDryLimit_=(std::max)(1,maxDry-static_cast<int>(Unit()*2));
            }
        } else dryShots_=0;
        // Narrow bounds keep both the player's foot point and the nearest
        // phase drop reachable; this does not guarantee route continuity on
        // arbitrary surfaces. The simulation must test its real owner masks.
        plan.footForward=0.34f+Unit()*0.05f;
        plan.footLateral=0.07f+Unit()*0.06f;
        plan.footWidthScale=1.02f+Unit()*0.14f;
        plan.scatterCount=1+static_cast<int>(Unit()*5);
        return plan;
    }

private:
    static constexpr int MaxPhases=16;
    static float FiniteClamp(float value,float low,float high,float fallback) {
        return (std::clamp)(std::isfinite(value)?value:fallback,low,high);
    }
    float Unit() {
        randomState_^=randomState_<<13; randomState_^=randomState_>>17; randomState_^=randomState_<<5;
        return static_cast<float>(randomState_&0x00ffffffu)/16777216.0f;
    }
    void RefillPhases(int count) {
        phaseCount_=count; phaseCursor_=0;
        for(int n=0;n<count;++n) phases_[n]=n;
        // Shuffle once, then retain this order until reset/count change. Thus
        // ANY eight consecutive shots cover the eight strata, including across
        // a stop/restart boundary. Reshuffling every cycle can omit a stratum
        // in a moving window and leave an avoidable dry gap. Per-shot position,
        // width and satellite variation still prevent identical paint shapes.
        for(int n=count-1;n>0;--n) {
            const int other=static_cast<int>(Unit()*(n+1));
            std::swap(phases_[n],phases_[other]);
        }
    }
    std::array<int,MaxPhases> phases_{};
    uint32_t randomState_=0;
    double budgetCarry_=0;
    int phaseCount_=0,phaseCursor_=0,dryShots_=0,nextDryLimit_=3;
};
} // namespace ink
