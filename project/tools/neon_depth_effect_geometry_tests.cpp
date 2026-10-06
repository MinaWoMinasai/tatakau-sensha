// Geometry and ownership contracts. Renderer/bindings/visual quality are runtime gates.
#include "game/enemy/visual/NeonDepthEffectGeometry.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
bool Near(float a,float b,float tolerance=.0002f) { return std::abs(a-b)<=tolerance; }
bool Near(cg2::Vector3 a,cg2::Vector3 b,float tolerance=.0002f) {
    return Near(a.x,b.x,tolerance)&&Near(a.y,b.y,tolerance)&&Near(a.z,b.z,tolerance);
}
bool Same(cg2::Vector4 a,cg2::Vector4 b) { return a.x==b.x&&a.y==b.y&&a.z==b.z&&a.w==b.w; }
bool SameFloor(const neondepth::EffectGeometryFrame& a,const neondepth::EffectGeometryFrame& b) {
    if(a.floorCount!=b.floorCount||a.fillCount!=b.fillCount||a.circleCount!=b.circleCount||a.segmentCount!=b.segmentCount) return false;
    for(unsigned i=0;i<a.floorCount;++i) {
        const auto& x=a.floor[i];const auto& y=b.floor[i];
        if(!Near(x.start,y.start,0)||!Near(x.end,y.end,0)||x.width!=y.width||!Same(x.color,y.color)||x.cue!=y.cue) return false;
    }
    for(unsigned i=0;i<a.fillCount;++i) {
        const auto& x=a.fills[i];const auto& y=b.fills[i];
        if(x.rectangle!=y.rectangle||!Near(x.center,y.center,0)||!Near(x.right,y.right,0)||!Near(x.up,y.up,0)||
            x.radius!=y.radius||x.size.x!=y.size.x||x.size.y!=y.size.y||!Same(x.color,y.color)) return false;
    }
    for(unsigned i=0;i<a.circleCount;++i) if(a.dangerCircles[i]!=b.dangerCircles[i]) return false;
    for(unsigned i=0;i<a.segmentCount;++i) if(a.dangerSegments[i]!=b.dangerSegments[i]) return false;
    return true;
}
unsigned Count(const neondepth::EffectGeometryFrame& frame,neondepth::Cue cue,bool air=false) {
    unsigned count=0;
    const auto limit=air?frame.airCount:frame.floorCount;
    for(unsigned i=0;i<limit;++i) if((air?frame.air[i]:frame.floor[i]).cue==cue) ++count;
    return count;
}
neondepth::EffectsInput Input(neondepth::Attack attack=neondepth::Attack::Volley,neondepth::Phase phase=neondepth::Phase::Telegraph) {
    neondepth::EffectsInput input;
    input.encounterActive=true;input.core={10,15,0};input.viewProjection=cg2::MakeIdentity4x4();
    input.snapshot.generation=1;input.snapshot.phase=phase;input.snapshot.vulnerable=true;input.snapshot.progress=.4f;
    auto& plan=input.snapshot.plan;plan.generation=1;plan.instance=1;plan.attack=attack;
    plan.circleCount=5;
    for(unsigned i=0;i<5;++i) plan.circles[i]={{float(20+i*4),25},2.1f};
    plan.dive={{14,21},3.4f};
    input.sockets.chest={10,19,-15};input.sockets.rightHand={14,18,-13};input.sockets.leftHand={6,18,-13};
    input.sockets.chestValid=input.sockets.leftHandValid=input.sockets.rightHandValid=true;
    return input;
}
template<class Shape> void StrokeInside(const Shape& shape,const neondepth::Stripe& stripe,float width) {
    for(unsigned i=0;i<=100;++i) {
        const float p=float(i)/100;
        const neondepth::Point point{stripe.start[0]+(stripe.end[0]-stripe.start[0])*p,stripe.start[1]+(stripe.end[1]-stripe.start[1])*p};
        Require(neondepth::Contains(shape,point,width*.5f),"clipped active stripe stroke extends outside canonical contact geometry");
    }
}
void StripeGeometry() {
    const neondepth::Circle circle{{3,-2},3.4f};
    const auto circleBefore=circle;
    for(float y:{-5.3f,-4.f,-2.f,0.f,1.3f}) {
        const auto stripe=neondepth::ClipStripe(circle,{-10,y},{15,y},.07f);
        Require(bool(stripe),"circle stripe inside edge rejected");StrokeInside(circle,*stripe,.07f);
    }
    Require(!neondepth::ClipStripe(circle,{-10,2},{15,2},.07f),"outside circle stripe accepted");
    Require(circle==circleBefore,"circle clipping changed canonical shape");
    for(const neondepth::Segment shape:{neondepth::Segment{{-3,1},{9,1},.8f},{{-3,-5},{8,7},1.3f},{{3,4},{3,4},2},{{3,4},{3.001f,4},2}}) {
        const auto before=shape;
        for(unsigned angle=0;angle<24;++angle) for(int offset=-2;offset<=2;++offset) {
            const float theta=cg2::pi*2*float(angle)/24,dx=std::cos(theta),dy=std::sin(theta);
            const neondepth::Point center{(shape.start[0]+shape.end[0])*.5f-dy*float(offset)*.3f,
                (shape.start[1]+shape.end[1])*.5f+dx*float(offset)*.3f};
            const auto stripe=neondepth::ClipStripe(shape,{center[0]-dx*30,center[1]-dy*30},{center[0]+dx*30,center[1]+dy*30},.07f);
            Require(bool(stripe),"capsule crossing stripe rejected");StrokeInside(shape,*stripe,.07f);
        }
        Require(shape==before,"capsule clipping changed stored halfWidth/endpoints");
    }
    const neondepth::Segment horizontal{{0,0},{8,0},1};
    Require(!neondepth::ClipStripe(horizontal,{0,3},{10,3},.07f),"outside capsule stripe accepted");
    Require(neondepth::Contains(horizontal,{9,0}) && !neondepth::Contains(horizontal,{9.01f,0}),"capsule boundary differs from gameplay SegmentTouches");
    Require(neondepth::Contains(circle,{3,-2}) && !neondepth::Contains(circle,{8,-2}),"circle boundary differs from gameplay SegmentTouches");
}
void CanonicalDangerAndBudget() {
    neondepth::EffectGeometryBuilder builder;
    auto input=Input();const auto snapshotBefore=input.snapshot;
    Require(builder.Build(input),"volley warning build rejected");
    const auto warning=builder.GetFrame();
    Require(warning.circleCount==5&&warning.segmentCount==0&&warning.fillCount==5,"warning copied wrong circle count");
    Require(Count(warning,neondepth::Cue::WarningEdge)==5*neondepth::kEffectCircleSteps&&Count(warning,neondepth::Cue::ActiveStripe)==0,"warning/active pattern not separated");
    Require(Count(warning,neondepth::Cue::Core)==10,"core broken hexagon/diamond target pattern missing");
    for(unsigned i=0;i<5;++i) Require(warning.dangerCircles[i]==input.snapshot.plan.circles[i],"floor danger changed plan circle");
    input.bodyVisualEnabled=false;input.profile.reducedMotion=true;input.profile.motionAmplitude=0;
    Require(builder.Build(input)&&SameFloor(warning,builder.GetFrame())&&builder.GetFrame().airCount==0,"body toggle/reduced motion changed warning geometry");
    input.snapshot.phase=neondepth::Phase::Active;input.snapshot.activeCircleMask=0b10101;
    Require(builder.Build(input)&&builder.GetFrame().circleCount==3&&Count(builder.GetFrame(),neondepth::Cue::ActiveStripe)>0&&
        Count(builder.GetFrame(),neondepth::Cue::WarningEdge)==0,"active mask or stripes differ from active gameplay circles");
    for(unsigned i=0;i<3;++i) Require(builder.GetFrame().dangerCircles[i]==input.snapshot.plan.circles[i*2],"masked active circle selection changed");
    input.snapshot.activeCircleMask=0;
    Require(builder.Build(input)&&builder.GetFrame().circleCount==0&&builder.GetFrame().fillCount==0,"hidden inactive circle survived active mask");
    Require(snapshotBefore.plan==input.snapshot.plan,"geometry mutated authoritative plan");
    input=Input(neondepth::Attack::Beam);
    input.snapshot.plan.beam.warningCount=17;
    for(unsigned i=0;i<17;++i) {
        const float theta=-.6f+1.2f*float(i)/16;
        input.snapshot.plan.beam.warning[i]={{10,15},{10+28*std::cos(theta),15+28*std::sin(theta)},1.7f};
    }
    // Stored width is intentionally different from nominal/derived width:
    // drawing must use precisely the canonical adapter-resolved capsules.
    input.snapshot.plan.beam.halfWidth=.3f;input.snapshot.plan.beam.range=8;
    Require(builder.Build(input),"maximum beam warning rejected");
    const auto beamWarning=builder.GetFrame();
    Require(beamWarning.segmentCount==17&&beamWarning.circleCount==0&&beamWarning.fillCount==51,"bounded beam warning geometry count changed");
    for(unsigned i=0;i<17;++i) Require(beamWarning.dangerSegments[i]==input.snapshot.plan.beam.warning[i],"beam warning recomputed stored halfWidth/endpoints");
    Require(beamWarning.airCount<=neondepth::kMaximumAirLines&&beamWarning.floorCount<=neondepth::kMaximumFloorLines&&
        beamWarning.fillCount<=neondepth::kMaximumFloorFills&&!beamWarning.capacityReached,"bounded queue capacity exceeded");
    const auto vertices=(beamWarning.airCount+beamWarning.floorCount)*18+beamWarning.fillCount*144;
    Require(vertices<=neondepth::kMaximumEffectVertices&&neondepth::kMaximumEffectVertices<196608,"effect vertex budget exceeds existing renderer");
    input.snapshot.phase=neondepth::Phase::Active;input.snapshot.activeBeamClipped=true;
    input.snapshot.activeBeam={{11,15},{23,20},.8f};
    Require(builder.Build(input)&&builder.GetFrame().segmentCount==1&&builder.GetFrame().dangerSegments[0]==input.snapshot.activeBeam,
        "active beam changed actual clipped segment/halfWidth");
    Require(Count(builder.GetFrame(),neondepth::Cue::GroundedBeam,true)==1,"grounded beam omitted actual socket connector");
    for(unsigned i=0;i<builder.GetFrame().airCount;++i) if(builder.GetFrame().air[i].cue==neondepth::Cue::GroundedBeam)
        Require(Near(builder.GetFrame().air[i].start,input.sockets.rightHand)&&Near(builder.GetFrame().air[i].end,{11,15,0}),"grounded beam did not join actual socket to clipped segment start");
    input.snapshot.activeBeamClipped=false;
    Require(builder.Build(input)&&builder.GetFrame().segmentCount==0&&Count(builder.GetFrame(),neondepth::Cue::GroundedBeam,true)==0,"unclipped hidden beam drew contact");
    input=Input(neondepth::Attack::Dive);input.snapshot.phase=neondepth::Phase::Active;input.snapshot.activeCircleMask=1;
    Require(builder.Build(input)&&builder.GetFrame().circleCount==1&&builder.GetFrame().dangerCircles[0]==input.snapshot.plan.dive,"Dive danger changed authoritative landing circle");
}
void ActualLaunchAndReadOnly() {
    neondepth::EffectGeometryBuilder builder;
    auto input=Input(neondepth::Attack::Volley,neondepth::Phase::Airborne);input.snapshot.progress=0;
    const auto before=input.snapshot;
    Require(builder.Build(input)&&builder.GetLaunchCount()==1&&builder.GetFrame().projectileCount==5,"airborne launch not latched exactly once");
    const auto launch=input.sockets.rightHand;
    for(unsigned i=0;i<5;++i) Require(Near(builder.GetFrame().projectilePositions[i],launch,0),"projectile did not start at actual hand socket");
    input.snapshot.progress=.5f;input.sockets.rightHand={99,99,-99};
    Require(builder.Build(input)&&builder.GetLaunchCount()==1&&Near(builder.GetLaunchSocket(),launch,0),"same airborne phase relatched moving socket");
    const auto middle=builder.GetFrame();
    Require(middle.projectilePositions[0].z<launch.z*.5f,"3D arc has no height beyond endpoint interpolation");
    for(unsigned repeat=0;repeat<512;++repeat) {
        Require(builder.Build(input)&&SameFloor(middle,builder.GetFrame())&&builder.GetLaunchCount()==1,"repeated geometry accumulated or consumed launch again");
        for(unsigned i=0;i<5;++i) Require(Near(middle.projectilePositions[i],builder.GetFrame().projectilePositions[i],0),"same-time arc is nondeterministic");
    }
    input.snapshot.progress=1;
    Require(builder.Build(input),"arrival frame rejected");
    for(unsigned i=0;i<5;++i) Require(Near(builder.GetFrame().projectilePositions[i],{input.snapshot.plan.circles[i].center[0],input.snapshot.plan.circles[i].center[1],0},0),"arc arrival differs from fixed canonical floor target");
    Require(before.plan==input.snapshot.plan&&before.acceptedHits==input.snapshot.acceptedHits&&before.activationEvents==input.snapshot.activationEvents,"presentation modified gameplay plan/events");
    input.snapshot.plan.instance=2;input.snapshot.progress=0;
    Require(builder.Build(input)&&builder.GetLaunchCount()==2&&Near(builder.GetLaunchSocket(),input.sockets.rightHand,0),"new attack instance did not relatch actual socket");
    input.snapshot.phase=neondepth::Phase::Active;input.snapshot.activeCircleMask=31;
    Require(builder.Build(input)&&builder.GetFrame().projectileCount==0,"projectiles continued after floor arrival phase");
    input.snapshot.phase=neondepth::Phase::Airborne;input.snapshot.plan.instance=3;
    input.sockets.chestValid=input.sockets.leftHandValid=input.sockets.rightHandValid=false;
    Require(builder.Build(input)&&builder.GetFrame().projectileCount==0,"missing sockets invented a projected launch point");
    input.snapshot.plan.instance=4;input.sockets.chestValid=true;
    Require(builder.Build(input)&&Near(builder.GetLaunchSocket(),input.sockets.chest,0),"valid chest-only fallback failed");
}
void IntroDeathAndInvalid() {
    neondepth::EffectGeometryBuilder builder;
    auto input=Input();input.snapshot.phase=neondepth::Phase::Intro;input.formation={0,0,0};input.life=neondepth::Life::Intro;
    Require(builder.Build(input)&&builder.GetFrame().floorCount==0&&builder.GetFrame().airCount==0,"intro zero formation was visible");
    input.formation={1,0,0};
    Require(builder.Build(input)&&Count(builder.GetFrame(),neondepth::Cue::Core)==10&&builder.GetFrame().airCount==0,"intro core reveal built body connector early");
    input.formation={1,1,1};
    Require(builder.Build(input)&&Count(builder.GetFrame(),neondepth::Cue::Connector,true)==1,"intro connector missing actual chest");
    input.life=neondepth::Life::Collapse;input.encounterActive=false;input.snapshot.phase=neondepth::Phase::Defeated;input.coreRadius=0;
    Require(builder.Build(input)&&builder.GetFrame().floorCount==10&&builder.GetFrame().airCount==1&&builder.GetFrame().circleCount==0,"dead actor teardown removed collapsing core/line or retained hazards");
    const float full=builder.GetFrame().floor[0].color.w;
    input.life=neondepth::Life::Dissolve;input.dissolveProgress=.5f;
    Require(builder.Build(input)&&Near(builder.GetFrame().floor[0].color.w,full*.5f),"core did not follow death dissolve visibility");
    input.dissolveProgress=1;
    Require(builder.Build(input)&&builder.GetFrame().floorCount==0&&builder.GetFrame().airCount==0,"dissolve one left core/connector residue");
    input.life=neondepth::Life::Finished;input.dissolveProgress=0;
    Require(builder.Build(input)&&builder.GetFrame().floorCount==0&&builder.GetFrame().airCount==0,"finished life resurrected floor target");
    input=Input();input.core.z=-2;
    Require(!builder.Build(input)&&!builder.GetFrame().valid,"lifted authoritative core accepted as floor target");
    input=Input();input.cameraForward={0,0,0};
    Require(!builder.Build(input),"collapsed camera facing basis accepted");
    input=Input();input.coreRadius=std::numeric_limits<float>::quiet_NaN();
    Require(!builder.Build(input),"nonfinite alive core radius accepted");
    input=Input();input.snapshot.plan.circleCount=6;
    Require(!builder.Build(input),"oversized forged circle plan accepted");
    input=Input();input.snapshot.plan.circles[0].radius=std::numeric_limits<float>::infinity();
    Require(!builder.Build(input),"nonfinite danger accepted");
    input=Input(neondepth::Attack::Volley,neondepth::Phase::Airborne);
    input.viewProjection.m[0][0]=std::numeric_limits<float>::quiet_NaN();
    builder.Reset();
    Require(!builder.Build(input)&&builder.GetLaunchCount()==0&&builder.GetFrame().floorCount==0,"invalid camera consumed entry or exposed previous geometry");
    input.viewProjection=cg2::MakeIdentity4x4();
    Require(builder.Build(input)&&builder.GetLaunchCount()==1,"repaired camera did not retain launch entry");
    input.snapshot.generation=2;input.snapshot.plan.generation=2;
    input.snapshot.plan.circles[0].radius=std::numeric_limits<float>::infinity();
    Require(!builder.Build(input)&&builder.GetLaunchCount()==0,"invalid Airborne target consumed or poisoned launch entry");
    input.snapshot.plan.circles[0].radius=2.1f;
    Require(builder.Build(input)&&builder.GetLaunchCount()==1&&builder.GetFrame().projectileCount==5,
        "repaired same-instance Airborne targets could not recover presentation");
}
}
int main() {
    try {
        StripeGeometry();CanonicalDangerAndBudget();ActualLaunchAndReadOnly();IntroDeathAndInvalid();
        std::cout<<"Neon Depth immutable effect geometry contracts passed. HDR/DSV/visual gates require runtime.\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
