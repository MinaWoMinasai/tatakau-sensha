#pragma once
// Pure bounded geometry commands built from immutable gameplay snapshots.
#include "NeonDepthPresentation.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

namespace neondepth {
struct EffectSockets {
    cg2::Vector3 chest{},leftHand{},rightHand{};
    bool chestValid=false,leftHandValid=false,rightHandValid=false;
};
struct EffectsInput {
    uint64_t frameId=0;
    Snapshot snapshot{};
    cg2::Vector3 core{};
    float coreRadius=2,hitFeedback=0;
    Formation formation{};
    Life life=Life::Active;
    float dissolveProgress=0;
    PresentationConfig profile{};
    EffectSockets sockets{};
    cg2::Matrix4x4 viewProjection{};
    cg2::Vector3 cameraRight{1,0,0},cameraUp{0,1,0},cameraForward{0,0,1};
    bool encounterActive=false,bodyVisualEnabled=true;
};
enum class Cue { Core, WarningEdge, ActiveEdge, ActiveStripe, Connector, Projectile, GroundedBeam };
struct EffectLine {
    cg2::Vector3 start{},end{};
    float width=.10f;
    cg2::Vector4 color{};
    Cue cue=Cue::Core;
};
struct EffectFill {
    bool rectangle=false;
    cg2::Vector3 center{},right{1,0,0},up{0,1,0};
    float radius=1;
    cg2::Vector2 size{};
    cg2::Vector4 color{};
};
inline constexpr unsigned kMaximumAirLines=256,kMaximumFloorLines=2048,kMaximumFloorFills=64;
inline constexpr unsigned kEffectCircleSteps=48,kEffectCapSteps=16;
// Each soft line uses at most18 vertices; each48-step circle fill at most144.
inline constexpr unsigned kMaximumEffectVertices=(kMaximumAirLines+kMaximumFloorLines)*18+kMaximumFloorFills*144;
struct EffectGeometryFrame {
    std::array<EffectLine,kMaximumAirLines> air{};
    std::array<EffectLine,kMaximumFloorLines> floor{};
    std::array<EffectFill,kMaximumFloorFills> fills{};
    unsigned airCount=0,floorCount=0,fillCount=0;
    std::array<Circle,kMaximumCircles> dangerCircles{};
    std::array<Segment,kMaximumWarningSegments> dangerSegments{};
    unsigned circleCount=0,segmentCount=0;
    std::array<cg2::Vector3,kMaximumCircles> projectilePositions{};
    unsigned projectileCount=0;
    bool valid=true,capacityReached=false;
};
inline bool Contains(const Circle& circle,Point point,float margin=0) {
    const float radius=circle.radius-margin;
    return radius>=0 && tankspecial::SegmentTouches(circle.center[0],circle.center[1],circle.center[0],circle.center[1],point[0],point[1],radius);
}
inline bool Contains(const Segment& capsule,Point point,float margin=0) {
    const float radius=capsule.halfWidth-margin;
    return radius>=0 && tankspecial::SegmentTouches(capsule.start[0],capsule.start[1],capsule.end[0],capsule.end[1],point[0],point[1],radius);
}
struct Stripe { Point start{},end{}; };
namespace effectdetail {
inline bool BoundedWorld(cg2::Vector3 value) {
    return Finite(value) && std::abs(value.x)<=10000 && std::abs(value.y)<=10000 && std::abs(value.z)<=10000;
}
inline bool CameraAxes(const EffectsInput& input) {
    if(!Finite(input.cameraRight) || !Finite(input.cameraUp) || !Finite(input.cameraForward)) return false;
    return Within(cg2::Dot(input.cameraRight,input.cameraRight),.99f,1.01f) &&
        Within(cg2::Dot(input.cameraUp,input.cameraUp),.99f,1.01f) &&
        Within(cg2::Dot(input.cameraForward,input.cameraForward),.99f,1.01f) &&
        std::abs(cg2::Dot(input.cameraRight,input.cameraUp))<.01f &&
        std::abs(cg2::Dot(input.cameraRight,input.cameraForward))<.01f &&
        std::abs(cg2::Dot(input.cameraUp,input.cameraForward))<.01f;
}
struct Interval { double low=0,high=1; };
inline std::optional<Interval> CircleInterval(Point center,float radius,Point a,Point b) {
    const double dx=double(b[0])-a[0],dy=double(b[1])-a[1];
    const double x=double(a[0])-center[0],y=double(a[1])-center[1];
    const double length=dx*dx+dy*dy;
    if(length<=1.0e-12) return x*x+y*y<=double(radius)*radius?std::optional<Interval>{{0,1}}:std::nullopt;
    const double projection=x*dx+y*dy;
    const double discriminant=projection*projection-length*(x*x+y*y-double(radius)*radius);
    if(discriminant<0 || !std::isfinite(discriminant)) return std::nullopt;
    const double root=std::sqrt(discriminant);
    const double low=(std::max)(0.0,(-projection-root)/length),high=(std::min)(1.0,(-projection+root)/length);
    return low<=high?std::optional<Interval>{{low,high}}:std::nullopt;
}
inline std::optional<Interval> BoxInterval(Point a,Point b,float lowX,float highX,float lowY,float highY) {
    Interval result;
    for(unsigned axis=0;axis<2;++axis) {
        const double delta=double(b[axis])-a[axis],low=axis?lowY:lowX,high=axis?highY:highX;
        if(std::abs(delta)<1.0e-12) { if(a[axis]<low || a[axis]>high) return std::nullopt; }
        else { double first=(low-a[axis])/delta,last=(high-a[axis])/delta;
            if(first>last) std::swap(first,last);
            result.low=(std::max)(result.low,first); result.high=(std::min)(result.high,last);
            if(result.low>result.high) return std::nullopt;
        }
    }
    return result;
}
inline Stripe At(Point a,Point b,Interval interval) {
    Stripe result;
    for(unsigned axis=0;axis<2;++axis) {
        result.start[axis]=float(a[axis]+(double(b[axis])-a[axis])*interval.low);
        result.end[axis]=float(a[axis]+(double(b[axis])-a[axis])*interval.high);
    }
    return result;
}
inline cg2::Vector3 Floor(Point point,float z=-.035f) { return {point[0],point[1],z}; }
inline Point Shift(Point point,float x,float y) { return {point[0]+x,point[1]+y}; }
}
// Stripe clipping shrinks only decorative stroke coverage by half its width.
// The canonical radius/halfWidth and Gameplay contact shape stay unchanged.
inline std::optional<Stripe> ClipStripe(const Circle& circle,Point a,Point b,float strokeWidth) {
    if(!Valid(circle) || !Finite(a) || !Finite(b) || !Within(strokeWidth,.01f,.4f)) return std::nullopt;
    const float radius=circle.radius-strokeWidth*.5f-.001f;
    if(radius<=0) return std::nullopt;
    const auto interval=effectdetail::CircleInterval(circle.center,radius,a,b);
    return interval?std::optional<Stripe>{effectdetail::At(a,b,*interval)}:std::nullopt;
}
inline std::optional<Stripe> ClipStripe(const Segment& capsule,Point a,Point b,float strokeWidth) {
    if(!Valid(capsule) || !Finite(a) || !Finite(b) || !Within(strokeWidth,.01f,.4f)) return std::nullopt;
    const float radius=capsule.halfWidth-strokeWidth*.5f-.001f;
    if(radius<=0) return std::nullopt;
    const float dx=capsule.end[0]-capsule.start[0],dy=capsule.end[1]-capsule.start[1],lengthSquared=dx*dx+dy*dy;
    if(lengthSquared<=.00001f) return ClipStripe(Circle{capsule.start,capsule.halfWidth},a,b,strokeWidth);
    const float length=std::sqrt(lengthSquared),tx=dx/length,ty=dy/length,nx=-ty,ny=tx;
    auto local=[&](Point point) { const float x=point[0]-capsule.start[0],y=point[1]-capsule.start[1]; return Point{x*tx+y*ty,x*nx+y*ny}; };
    std::array<std::optional<effectdetail::Interval>,3> intervals{
        effectdetail::CircleInterval(capsule.start,radius,a,b),effectdetail::CircleInterval(capsule.end,radius,a,b),
        effectdetail::BoxInterval(local(a),local(b),0,length,-radius,radius)};
    std::optional<effectdetail::Interval> result;
    for(const auto& interval:intervals) if(interval) {
        if(!result) result=interval;
        else { result->low=(std::min)(result->low,interval->low); result->high=(std::max)(result->high,interval->high); }
    }
    return result?std::optional<Stripe>{effectdetail::At(a,b,*result)}:std::nullopt;
}
inline cg2::Vector3 VolleyArc(cg2::Vector3 launch,Point target,float progress,float lift) {
    const float p=Unit(progress); const cg2::Vector3 finish{target[0],target[1],0};
    if(p<=0) return launch; if(p>=1) return finish;
    const cg2::Vector3 control{(launch.x+finish.x)*.5f,(launch.y+finish.y)*.5f,(launch.z+finish.z)*.5f-lift};
    const float a=(1-p)*(1-p),b=2*(1-p)*p,c=p*p;
    return {launch.x*a+control.x*b+finish.x*c,launch.y*a+control.y*b+finish.y*c,launch.z*a+control.z*b+finish.z*c};
}

class EffectGeometryBuilder {
public:
    void Reset() { frame_={}; generation_=instance_=0; lastPhase_=Phase::Aborted; launchValid_=false; launchCount_=0; lastCoreRadius_=2; }
    const EffectGeometryFrame& GetFrame() const { return frame_; }
    const cg2::Vector3& GetLaunchSocket() const { return launch_; }
    unsigned GetLaunchCount() const { return launchCount_; }
    bool Build(const EffectsInput& input) {
        frame_.airCount=frame_.floorCount=frame_.fillCount=frame_.circleCount=frame_.segmentCount=frame_.projectileCount=0;
        frame_.valid=true; frame_.capacityReached=false;
        const auto& snapshot=input.snapshot;
        if(snapshot.generation!=generation_) { generation_=snapshot.generation; instance_=0; lastPhase_=Phase::Aborted; launchValid_=false; launchCount_=0; lastCoreRadius_=2; }
        const bool dead=input.life==Life::Collapse || input.life==Life::Dissolve;
        if((!input.encounterActive && !dead) || input.life==Life::Dormant || input.life==Life::Finished || snapshot.phase==Phase::Aborted) return true;
        if(!effectdetail::BoundedWorld(input.core) || std::abs(input.core.z)>.0001f || !Valid(input.profile) || !effectdetail::CameraAxes(input) || !Finite(input.viewProjection))
            return frame_.valid=false;
        if(!dead && !Within(input.coreRadius,.25f,10)) return frame_.valid=false;
        // Invalid frames cannot consume an attack entry or poison the cached
        // launch. Valid missing sockets remain an honest no-projectile fallback.
        const bool entry=snapshot.phase==Phase::Airborne && (lastPhase_!=Phase::Airborne || instance_!=snapshot.plan.instance);
        if(!dead && entry && snapshot.plan.attack==Attack::Volley) {
            if(snapshot.plan.circleCount>kMaximumCircles) return frame_.valid=false;
            for(unsigned index=0;index<snapshot.plan.circleCount;++index)
                if(!Valid(snapshot.plan.circles[index])) return frame_.valid=false;
            launchValid_=TakeSocket(input.sockets,launch_); ++launchCount_;
            launchTargets_=snapshot.plan.circles; launchTargetCount_=snapshot.plan.circleCount;
        }
        lastPhase_=snapshot.phase; instance_=snapshot.plan.instance;
        if(Within(input.coreRadius,.25f,10)) lastCoreRadius_=input.coreRadius;
        const float remaining=dead?1-Unit(input.dissolveProgress):1;
        const float coreVisibility=(std::min)(Unit(input.formation.core),remaining);
        if(coreVisibility>0) Core(input,lastCoreRadius_,coreVisibility);
        if(input.bodyVisualEnabled && input.sockets.chestValid && effectdetail::BoundedWorld(input.sockets.chest)) {
            const float alpha=(std::min)(Unit(input.formation.connector),remaining);
            if(alpha>0) Air(input.core,input.sockets.chest,.09f,{.12f,.68f,.90f,.42f*alpha},Cue::Connector);
        }
        if(dead || snapshot.phase==Phase::Defeated) return !frame_.capacityReached;
        const bool warning=snapshot.phase==Phase::Telegraph || snapshot.phase==Phase::Locked || snapshot.phase==Phase::Airborne;
        const bool active=snapshot.phase==Phase::Active;
        const float decoration=input.profile.reducedMotion?.35f:1;
        const float pulse=1+.12f*decoration*input.profile.motionAmplitude*std::sin(cg2::pi*Unit(snapshot.progress));
        if(warning || active) {
            if(snapshot.plan.attack==Attack::Beam) {
                if(warning) {
                    if(snapshot.plan.beam.warningCount>kMaximumWarningSegments) return frame_.valid=false;
                    for(unsigned index=0;index<snapshot.plan.beam.warningCount;++index) Capsule(snapshot.plan.beam.warning[index],false,1);
                } else if(snapshot.activeBeamClipped) {
                    Capsule(snapshot.activeBeam,true,pulse);
                    cg2::Vector3 origin;
                    if(input.bodyVisualEnabled && TakeSocket(input.sockets,origin))
                        Air(origin,{snapshot.activeBeam.start[0],snapshot.activeBeam.start[1],0},.16f,{.8f,.20f,.95f,.65f},Cue::GroundedBeam);
                }
            } else {
                const unsigned count=snapshot.plan.attack==Attack::Dive?1:snapshot.plan.circleCount;
                if(count>kMaximumCircles) return frame_.valid=false;
                for(unsigned index=0;index<count;++index) if(warning || (snapshot.activeCircleMask&(1u<<index)))
                    Disc(snapshot.plan.attack==Attack::Dive?snapshot.plan.dive:snapshot.plan.circles[index],active,pulse);
            }
        }
        if(snapshot.phase==Phase::Airborne && snapshot.plan.attack==Attack::Volley && input.bodyVisualEnabled && launchValid_)
            Bolts(input);
        return frame_.valid && !frame_.capacityReached;
    }
private:
    static bool TakeSocket(const EffectSockets& sockets,cg2::Vector3& output) {
        if(sockets.rightHandValid && effectdetail::BoundedWorld(sockets.rightHand)) { output=sockets.rightHand; return true; }
        if(sockets.leftHandValid && effectdetail::BoundedWorld(sockets.leftHand)) { output=sockets.leftHand; return true; }
        if(sockets.chestValid && effectdetail::BoundedWorld(sockets.chest)) { output=sockets.chest; return true; }
        return false;
    }
    void Air(cg2::Vector3 a,cg2::Vector3 b,float width,cg2::Vector4 color,Cue cue) {
        if(frame_.airCount==kMaximumAirLines) { frame_.capacityReached=true; return; }
        frame_.air[frame_.airCount++]={a,b,width,color,cue};
    }
    void Floor(Point a,Point b,float width,cg2::Vector4 color,Cue cue) {
        if(frame_.floorCount==kMaximumFloorLines) { frame_.capacityReached=true; return; }
        frame_.floor[frame_.floorCount++]={effectdetail::Floor(a),effectdetail::Floor(b),width,color,cue};
    }
    void FillDisc(Point center,float radius,cg2::Vector4 color) {
        if(frame_.fillCount==kMaximumFloorFills) { frame_.capacityReached=true; return; }
        frame_.fills[frame_.fillCount++]={false,effectdetail::Floor(center),{1,0,0},{0,1,0},radius,{},color};
    }
    void Core(const EffectsInput& input,float radius,float visibility) {
        const Point center{input.core.x,input.core.y};
        const float hit=Unit(input.hitFeedback);
        const cg2::Vector4 color{.10f+hit*.5f,.85f,.95f,visibility*(input.snapshot.vulnerable?.85f:.35f)};
        // Broken hexagon + central diamond is an attack target, visually distinct
        // from the closed danger circle/capsule and its active striped fill.
        for(unsigned index=0;index<6;++index) {
            const float a=cg2::pi*2*float(index)/6,b=cg2::pi*2*float(index+1)/6;
            const Point first=effectdetail::Shift(center,std::cos(a)*radius,std::sin(a)*radius),last=effectdetail::Shift(center,std::cos(b)*radius,std::sin(b)*radius);
            Floor({first[0]+(last[0]-first[0])*.12f,first[1]+(last[1]-first[1])*.12f},
                {first[0]+(last[0]-first[0])*.88f,first[1]+(last[1]-first[1])*.88f},.13f,color,Cue::Core);
        }
        const float r=radius*.34f;
        for(unsigned index=0;index<4;++index) {
            const float a=cg2::pi*.5f*float(index),b=cg2::pi*.5f*float(index+1);
            Floor(effectdetail::Shift(center,std::cos(a)*r,std::sin(a)*r),effectdetail::Shift(center,std::cos(b)*r,std::sin(b)*r),.10f,color,Cue::Core);
        }
    }
    void Disc(Circle circle,bool active,float pulse) {
        if(!Valid(circle)) { frame_.valid=false; return; }
        if(frame_.circleCount<kMaximumCircles) frame_.dangerCircles[frame_.circleCount++]=circle;
        const cg2::Vector4 color=active?cg2::Vector4{1,.20f,.08f,.76f*pulse}:cg2::Vector4{.90f,.45f,.10f,.52f};
        FillDisc(circle.center,circle.radius,active?cg2::Vector4{.9f,.1f,.04f,.065f}:cg2::Vector4{.7f,.35f,.08f,.025f});
        for(unsigned index=0;index<kEffectCircleSteps;++index) {
            const float a=cg2::pi*2*float(index)/kEffectCircleSteps,b=cg2::pi*2*float(index+1)/kEffectCircleSteps;
            Floor(effectdetail::Shift(circle.center,std::cos(a)*circle.radius,std::sin(a)*circle.radius),
                effectdetail::Shift(circle.center,std::cos(b)*circle.radius,std::sin(b)*circle.radius),.12f,color,active?Cue::ActiveEdge:Cue::WarningEdge);
        }
        if(active) for(int index=-18;index<=18;++index) {
            const float y=circle.center[1]+float(index)*.45f;
            const auto stripe=ClipStripe(circle,{circle.center[0]-circle.radius,y},{circle.center[0]+circle.radius,y},.07f);
            if(stripe) Floor(stripe->start,stripe->end,.07f,{1,.23f,.08f,.32f},Cue::ActiveStripe);
        }
    }
    void Capsule(Segment capsule,bool active,float pulse) {
        if(!Valid(capsule)) { frame_.valid=false; return; }
        if(frame_.segmentCount<kMaximumWarningSegments) frame_.dangerSegments[frame_.segmentCount++]=capsule;
        const float dx=capsule.end[0]-capsule.start[0],dy=capsule.end[1]-capsule.start[1],lengthSquared=dx*dx+dy*dy;
        if(lengthSquared<=.00001f) {
            // Semantic descriptor remains a capsule, including its degenerate
            // Gameplay SegmentClosestFraction convention. Disc appends no copy.
            const unsigned previous=frame_.circleCount;
            Disc({capsule.start,capsule.halfWidth},active,pulse); frame_.circleCount=previous; return;
        }
        const float length=std::sqrt(lengthSquared),tx=dx/length,ty=dy/length,nx=-ty,ny=tx,theta=std::atan2(ty,tx),r=capsule.halfWidth;
        const cg2::Vector4 edge=active?cg2::Vector4{.9f,.14f,.95f,.76f*pulse}:cg2::Vector4{.65f,.3f,.85f,.38f};
        const cg2::Vector4 fill=active?cg2::Vector4{.8f,.08f,.9f,.055f}:cg2::Vector4{.5f,.12f,.75f,.012f};
        FillDisc(capsule.start,r,fill); FillDisc(capsule.end,r,fill);
        if(frame_.fillCount==kMaximumFloorFills) frame_.capacityReached=true;
        else frame_.fills[frame_.fillCount++]={true,effectdetail::Floor({(capsule.start[0]+capsule.end[0])*.5f,(capsule.start[1]+capsule.end[1])*.5f}),
            {tx,ty,0},{nx,ny,0},1,{length,2*r},fill};
        const Cue cue=active?Cue::ActiveEdge:Cue::WarningEdge;
        Floor(effectdetail::Shift(capsule.start,nx*r,ny*r),effectdetail::Shift(capsule.end,nx*r,ny*r),.11f,edge,cue);
        Floor(effectdetail::Shift(capsule.end,-nx*r,-ny*r),effectdetail::Shift(capsule.start,-nx*r,-ny*r),.11f,edge,cue);
        for(unsigned index=0;index<kEffectCapSteps;++index) for(unsigned end=0;end<2;++end) {
            const float startAngle=theta+(end?cg2::pi*.5f:-cg2::pi*.5f);
            const float a=startAngle-cg2::pi*float(index)/kEffectCapSteps,b=startAngle-cg2::pi*float(index+1)/kEffectCapSteps;
            const auto center=end?capsule.end:capsule.start;
            Floor(effectdetail::Shift(center,std::cos(a)*r,std::sin(a)*r),effectdetail::Shift(center,std::cos(b)*r,std::sin(b)*r),.11f,edge,cue);
        }
        if(active) for(int index=-12;index<=72;++index) {
            const float along=float(index)*.6f;
            const Point center=effectdetail::Shift(capsule.start,tx*along,ty*along);
            const auto stripe=ClipStripe(capsule,effectdetail::Shift(center,-nx*r*2,-ny*r*2),effectdetail::Shift(center,nx*r*2,ny*r*2),.07f);
            if(stripe) Floor(stripe->start,stripe->end,.07f,{1,.16f,.9f,.30f},Cue::ActiveStripe);
        }
    }
    void Bolts(const EffectsInput& input) {
        const float p=Unit(input.snapshot.progress),amplitude=input.profile.motionAmplitude*(input.profile.reducedMotion?.35f:1);
        for(unsigned index=0;index<launchTargetCount_;++index) {
            if(!Valid(launchTargets_[index])) { frame_.valid=false; continue; }
            const auto target=launchTargets_[index].center;
            const float lift=2+3*amplitude;
            const auto position=VolleyArc(launch_,target,p,lift);
            frame_.projectilePositions[frame_.projectileCount++]=position;
            auto previous=VolleyArc(launch_,target,(std::max)(0.0f,p-.25f),lift);
            for(unsigned step=1;step<=8;++step) {
                const float t=(std::max)(0.0f,p-.25f)+(p-(std::max)(0.0f,p-.25f))*float(step)/8;
                const auto current=VolleyArc(launch_,target,t,lift);
                Air(previous,current,.13f,{1,.40f,.10f,.58f},Cue::Projectile); previous=current;
            }
            const auto right=input.cameraRight*.24f,up=input.cameraUp*.24f;
            Air(position-right,position+right,.12f,{1,.85f,.45f,.8f},Cue::Projectile);
            Air(position-up,position+up,.12f,{1,.85f,.45f,.8f},Cue::Projectile);
        }
    }
    EffectGeometryFrame frame_{};
    uint64_t generation_=0,instance_=0;
    Phase lastPhase_=Phase::Aborted;
    cg2::Vector3 launch_{};
    std::array<Circle,kMaximumCircles> launchTargets_{};
    unsigned launchTargetCount_=0,launchCount_=0;
    bool launchValid_=false;
    float lastCoreRadius_=2;
};
} // namespace neondepth
