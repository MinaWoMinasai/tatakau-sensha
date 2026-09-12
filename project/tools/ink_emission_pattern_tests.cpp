// Standalone deterministic planner tests. Compile as C++17; no graphics/assets.
#include "../game/ink/InkEmissionPattern.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
void Require(bool okay,const char* text) { if(!okay) { std::cerr<<"FAIL: "<<text<<'\n'; std::exit(1); } }
bool Same(const ink::ShotEmissionPlan& a,const ink::ShotEmissionPlan& b) {
    if(a.phase!=b.phase || a.count!=b.count || a.requestedDropCount!=b.requestedDropCount ||
        a.footRescue!=b.footRescue || a.footForward!=b.footForward || a.footLateral!=b.footLateral ||
        a.footWidthScale!=b.footWidthScale || a.scatterCount!=b.scatterCount) return false;
    for(int i=0;i<a.count;++i) if(a.drops[i].distance!=b.drops[i].distance ||
        a.drops[i].widthScale!=b.drops[i].widthScale || a.drops[i].lateralOffset!=b.drops[i].lateralOffset) return false;
    return true;
}
}
int main() {
    ink::EmissionPattern first(123),second(123);
    const ink::EmissionSettings defaults;
    int allocated=0,emitted=0,dryRun=0,maxDryRun=0,ones=0,twos=0;
    int small=100,large=0; unsigned phases=0;
    bool variableWidth=false,variableFoot=false;
    ink::ShotEmissionPlan previous;
    int firstCycle[8]{};
    for(int i=0;i<1024;++i) {
        const auto a=first.NextShot(defaults,true),b=second.NextShot(defaults,true);
        Require(Same(a,b),"same seed and shot inputs reproduce all emission fields");
        allocated+=a.requestedDropCount; emitted+=a.count;
        ones+=a.requestedDropCount==1; twos+=a.requestedDropCount==2;
        Require(a.count>=1 && a.count<=2,"standard source budget emits a bounded sparse count");
        Require(a.phase>=0 && a.phase<8 && !(phases&(1u<<a.phase)),"each phase occurs once per cycle");
        phases|=1u<<a.phase;
        if(i%8==7) { Require(phases==255,"every cycle covers all eight distance strata"); phases=0; }
        if(i<8) firstCycle[i]=a.phase;
        else Require(a.phase==firstCycle[i%8],"a stable phase order preserves coverage across arbitrary cycle boundaries");
        for(int n=0;n<a.count;++n) {
            const auto& d=a.drops[n];
            Require(d.distance>=defaults.sourceNearestDistance && d.distance<=defaults.maxTravelDistance,
                "all planned drops lie in the allowed travel interval");
            Require(n==0 || d.distance>a.drops[n-1].distance,"drop crossings are strictly ordered");
            Require(d.widthScale>=.92f && d.widthScale<=1.12f && std::abs(d.lateralOffset)<=.06f,
                "width and lateral jitter stay bounded");
        }
        ++dryRun;
        if(a.footRescue) { maxDryRun=(std::max)(maxDryRun,dryRun);dryRun=0; }
        Require(dryRun<4,"unlucky paint random draws cannot indefinitely deny dry-foot rescue");
        Require(a.footForward>=.34f && a.footForward<=.39f && a.footLateral>=.07f && a.footLateral<=.13f,
            "rescue position remains close enough to the feet and first phase");
        small=(std::min)(small,a.scatterCount);large=(std::max)(large,a.scatterCount);
        if(i) { variableWidth|=a.drops[0].widthScale!=previous.drops[0].widthScale;variableFoot|=a.footForward!=previous.footForward; }
        previous=a;
    }
    Require(allocated==1536 && ones==512 && twos==512,"fractional source budget retains a 1.5 mean without rounding it to two");
    Require(small==1 && large==5 && variableWidth && variableFoot,
        "positions, coherent widths and satellite counts vary over sustained fire");
    for(int start=0;start<8;++start) {
        unsigned slidingPhases=0;
        for(int n=0;n<8;++n) slidingPhases|=1u<<firstCycle[(start+n)%8];
        Require(slidingPhases==255,"any eight consecutive shots cover every distance stratum");
    }
    Require(maxDryRun==4,"dry rescue uses its full bounded interval without a fixed fourth-shot rule");
    first.Reset(123); second.Reset(123);
    for(int i=0;i<40;++i) {
        const auto plan=first.NextShot(defaults,false);
        Require(!plan.footRescue,"painted or airborne feet do not request rescue");
        Require(Same(plan,second.NextShot(defaults,false)),"reset restarts all deterministic state");
    }
    ink::EmissionSettings invalid;
    invalid.sourceSplitNum=10000;invalid.sourceSpawnNum=10000;invalid.maxTravelDistance=10000;
    invalid.sourceBetweenDistance=0;invalid.widthScaleMin=std::numeric_limits<float>::quiet_NaN();
    for(int i=0;i<100;++i) {
        const auto plan=first.NextShot(invalid,true);
        Require(plan.count<=ink::ShotEmissionPlan::Capacity && plan.phase<16,"edited settings cannot overflow fixed storage");
        for(int n=0;n<plan.count;++n) Require(std::isfinite(plan.drops[n].distance) && std::isfinite(plan.drops[n].widthScale),"edited settings stay finite");
    }
    invalid.maxTravelDistance=0;
    Require(first.NextShot(invalid,false).count==0,"zero available travel rejects all forward drop allocations");
    const float nan=std::numeric_limits<float>::quiet_NaN(),infinity=std::numeric_limits<float>::infinity();
    for(float count:{-infinity,-1.0f,0.0f,.1f,.5f,1.5f,12.0f,13.0f,infinity,nan})
        for(int split:{-10,0,1,8,100}) for(float end:{0.0f,.1f,.6f,6.75f,infinity,nan}) {
            ink::EmissionSettings edited;
            edited.sourceSpawnNum=count;edited.sourceSplitNum=split;edited.maxTravelDistance=end;
            edited.sourceBetweenDistance=nan;edited.widthScaleMax=-infinity;edited.phaseJitter=infinity;
            for(int shot=0;shot<32;++shot) {
                const auto plan=first.NextShot(edited,shot%3!=0);
                Require(plan.requestedDropCount>=0 && plan.requestedDropCount<=ink::ShotEmissionPlan::Capacity &&
                    plan.count>=0 && plan.count<=plan.requestedDropCount,"fractional carry and edited source settings cannot overflow the drop budget");
                for(int n=0;n<plan.count;++n) Require(std::isfinite(plan.drops[n].distance) &&
                    std::isfinite(plan.drops[n].widthScale) && plan.drops[n].widthScale>0,"invalid tuning values produce finite positive brush inputs");
            }
        }
    std::cout<<"PASS: fractional allocation="<<allocated<<", range-clipped drops="<<emitted
        <<", shuffled phase coverage, deterministic reset, bounded jitter, bounded dry-foot rescue, edited setting guards\n";
}
