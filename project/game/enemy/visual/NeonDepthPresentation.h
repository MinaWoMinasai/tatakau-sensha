#pragma once
// Value-only settings, actual-bind placement and immutable attack presentation.
#include "game/enemy/actor/NeonDepthCombat.h"
#include "NeonDepthPlacement.h"
#include "NeonDepthAnimations.h"
#include "NeonDepthLifecycle.h"
#include <algorithm>
#include <cstdint>

namespace neondepth {

// Copy-only presentation configuration. No Enemy, Camera, Stage or Input pointer.
struct PresentationConfig {
    float worldHeight=18.0f;
    float hoverHeight=3.0f;
    cg2::Vector2 floorOffset{0,2};
    bool reducedMotion=false;
    float motionAmplitude=1.0f;
    float collapseSeconds=.35f;
    float dissolveSeconds=1.2f;
    cg2::Vector3 footOffsetLocal{};
    float forwardYawOffsetRadians=cg2::pi*.5f;
};
inline bool Valid(const PresentationConfig& value) {
    return InRange(value.worldHeight,4,40) && InRange(value.hoverHeight,0,15) &&
        InRange(value.floorOffset.x,-12,12) && InRange(value.floorOffset.y,-12,12) &&
        InRange(value.motionAmplitude,0,1.5f) && InRange(value.collapseSeconds,.15f,.8f) &&
        InRange(value.dissolveSeconds,.4f,2) &&
        InRange(value.footOffsetLocal.x,-.5f,.5f) && InRange(value.footOffsetLocal.y,-.5f,.5f) &&
        InRange(value.footOffsetLocal.z,-.5f,.5f) &&
        InRange(value.forwardYawOffsetRadians,-2*cg2::pi,2*cg2::pi);
}
inline float Unit(float value) { return Finite(value)?(std::clamp)(value,0.0f,1.0f):0; }
inline float Smooth(float value) { const float t=Unit(value); return t*t*(3-2*t); }
inline float PresentationDelta(float value) { return InRange(value,0,.1f)?value:
    (Finite(value)&&value>0?.1f:0); }

struct BindBounds { cg2::Vector3 low{}, high{}; };
struct BindPlacement {
    cg2::Vector3 stableFootLocal{};
    float boundsHeight=0;
    unsigned footBonesFound=0;
};
// Called immediately after Initialize, before any generated animation is sampled.
// Actual loader-transformed skeleton matrices determine X/Z. Bottom bind bounds
// determine sole Y; the ankle joint's ~.1155 Y is deliberately not used as a sole.
inline bool TryBindPlacement(const cg2::Skeleton& skeleton, const BindBounds& bounds,
    BindPlacement& output) {
    if(!ValidateDepthBindSkeleton(skeleton) || !Finite(bounds.low) || !Finite(bounds.high) ||
        bounds.high.x<bounds.low.x || bounds.high.z<bounds.low.z ||
        !InRange(bounds.high.y-bounds.low.y,.001f,1000)) return false;
    BindPlacement candidate;
    candidate.boundsHeight=bounds.high.y-bounds.low.y;
    candidate.stableFootLocal={(bounds.low.x+bounds.high.x)*.5f,bounds.low.y,
        (bounds.low.z+bounds.high.z)*.5f};
    float footX=0,footZ=0;
    for(const char* name:{"J_Bip_L_Foot","J_Bip_R_Foot"}) {
        const auto found=skeleton.jointMap.find(name);
        if(found==skeleton.jointMap.end()) continue;
        const auto& matrix=skeleton.joints[static_cast<size_t>(found->second)].skeletonSpaceMatrix;
        const cg2::Vector3 point{matrix.m[3][0],matrix.m[3][1],matrix.m[3][2]};
        if(!Finite(point)) return false;
        footX+=point.x; footZ+=point.z; ++candidate.footBonesFound;
    }
    if(candidate.footBonesFound) {
        candidate.stableFootLocal.x=footX/candidate.footBonesFound;
        candidate.stableFootLocal.z=footZ/candidate.footBonesFound;
    }
    output=candidate; return true;
}
inline PlacementConfig MakePlacementConfig(const BindPlacement& bind, const PresentationConfig& profile) {
    PlacementConfig result;
    result.stableFootLocal={bind.stableFootLocal.x+profile.footOffsetLocal.x,
        bind.stableFootLocal.y+profile.footOffsetLocal.y,bind.stableFootLocal.z+profile.footOffsetLocal.z};
    result.boundsHeight=bind.boundsHeight; result.worldHeight=profile.worldHeight;
    result.forwardYawOffset=profile.forwardYawOffsetRadians;
    return result;
}

enum class Socket { Chest, LeftHand, RightHand };
// Model is already sampled for this frame. A socket is a display endpoint only;
// its height/animation never changes the floor danger or wall-clipped hit segment.
inline bool TrySocketWorld(const cg2::Skeleton& skeleton,const cg2::Matrix4x4& world,
    Socket socket,cg2::Vector3& output) {
    const char* name=socket==Socket::Chest?"J_Bip_C_Chest":
        socket==Socket::LeftHand?"J_Bip_L_Hand":"J_Bip_R_Hand";
    const auto found=skeleton.jointMap.find(name);
    if(found==skeleton.jointMap.end() || found->second<0 ||
        static_cast<size_t>(found->second)>=skeleton.joints.size() || !Finite(world)) return false;
    const auto& matrix=skeleton.joints[static_cast<size_t>(found->second)].skeletonSpaceMatrix;
    if(!Finite(matrix)) return false;
    const auto jointWorld=cg2::Multiply(matrix,world);
    cg2::Vector3 candidate;
    if(!TryHomogeneousPoint({0,0,0},jointWorld,candidate)) return false;
    output=candidate; return true;
}

struct MotionMapping {
    Motion motion=Motion::Hover;
    float speed=1;
    bool loop=true, playing=true;
    float entryTime=-1; // Seek only when phase changes; -1 preserves clip time/blend.
};
inline MotionMapping MapDepthMotion(const Snapshot& snapshot, bool collapsing=false,
    bool frozen=false, float collapseSeconds=.35f) {
    if(collapsing || frozen) return {Motion::Defeat,1/(std::max)(collapseSeconds,.15f),false,!frozen,-1};
    if(snapshot.phase==Phase::Intro || snapshot.phase==Phase::Reposition ||
        snapshot.phase==Phase::Defeated || snapshot.phase==Phase::Aborted) return {};
    const float duration=(std::max)(Finite(snapshot.duration)?snapshot.duration:0,.05f);
    if(snapshot.plan.attack==Attack::Beam) {
        switch(snapshot.phase) {
        case Phase::Telegraph: return {Motion::BeamTwist,.35f/duration,false,true,-1};
        case Phase::Locked: case Phase::Airborne: return {Motion::BeamTwist,0,false,false,.35f};
        case Phase::Active: return {Motion::BeamTwist,.37f/duration,false,true,.35f};
        case Phase::Recovery: return {Motion::BeamTwist,.28f/duration,false,true,.72f};
        default: return {};
        }
    }
    const bool dive=snapshot.plan.attack==Attack::Dive;
    switch(snapshot.phase) {
    case Phase::Telegraph: return {dive?Motion::DivePull:Motion::VolleyCharge,1/duration,false,true,-1};
    case Phase::Locked: return {dive?Motion::DivePull:Motion::VolleyCharge,0,false,false,1};
    case Phase::Airborne: return {dive?Motion::DiveDrop:Motion::VolleyRelease,1/duration,false,true,-1};
    case Phase::Active: return dive?MotionMapping{Motion::DiveLand,1/duration,false,true,-1}:MotionMapping{};
    default: return {};
    }
}
struct MotionCommand {
    MotionMapping mapping{};
    bool select=false, restartSameClip=false, phaseChanged=false;
};
// Runtime selection guard only. It never controls Gameplay phase or hit events.
class MotionGate {
public:
    void Reset() { *this=MotionGate{}; }
    MotionCommand Step(const Snapshot& snapshot,bool collapsing=false,bool frozen=false,float collapseSeconds=.35f) {
        MotionCommand command;
        command.mapping=MapDepthMotion(snapshot,collapsing,frozen,collapseSeconds);
        command.select=!valid_ || generation_!=snapshot.generation || instance_!=snapshot.plan.instance ||
            motion_!=command.mapping.motion;
        command.restartSameClip=valid_ && command.select && motion_==command.mapping.motion;
        command.phaseChanged=command.select || phase_!=snapshot.phase || collapsing_!=collapsing || frozen_!=frozen;
        generation_=snapshot.generation; instance_=snapshot.plan.instance; motion_=command.mapping.motion;
        phase_=snapshot.phase; collapsing_=collapsing; frozen_=frozen; valid_=true;
        return command;
    }
private:
    uint64_t generation_=0,instance_=0;
    Motion motion_=Motion::Hover;
    Phase phase_=Phase::Aborted;
    bool valid_=false,collapsing_=false,frozen_=false;
};

struct Formation {
    float core=1,connector=1,body=1;
};
inline Formation MapFormation(const Snapshot& snapshot) {
    if(snapshot.phase==Phase::Aborted) return {0,0,0};
    if(snapshot.phase!=Phase::Intro) return {};
    const float p=Unit(snapshot.progress);
    return {Smooth(p/.25f),Smooth((p-.12f)/.38f),Smooth((p-.25f)/.65f)};
}
struct BodyMotion {
    cg2::Vector2 extraFloorOffset{};
    float extraHoverHeight=0;
    cg2::Vector3 localLean{};
};
// Immutable plan progress -> fresh bounded transform. No previous pose additions,
// wall/core resolution, screen displacement, attack activation or shared RNG.
inline BodyMotion MapBodyMotion(const Snapshot& snapshot,const PresentationConfig& profile,float hitFeedback=0) {
    BodyMotion result;
    const float p=Unit(snapshot.progress);
    const float amplitude=profile.motionAmplitude*(profile.reducedMotion?.35f:1.0f);
    const float wave=p>0 && p<1?std::sin(cg2::pi*p):0;
    const bool dive=snapshot.plan.attack==Attack::Dive;
    const bool beam=snapshot.plan.attack==Attack::Beam;
    switch(snapshot.phase) {
    case Phase::Telegraph: case Phase::Locked:
        result.extraFloorOffset.y=amplitude*3*(snapshot.phase==Phase::Locked?1:Smooth(p));
        result.extraHoverHeight=amplitude*(dive?4:1.5f)*(snapshot.phase==Phase::Locked?1:Smooth(p));
        result.localLean.x=amplitude*.08f*(snapshot.phase==Phase::Locked?1:Smooth(p));
        break;
    case Phase::Airborne:
        result.extraFloorOffset.x=dive?-profile.floorOffset.x*Smooth(p):0;
        result.extraFloorOffset.y=amplitude*3*(1-Smooth(p))-
            (dive?profile.floorOffset.y*Smooth(p):0);
        result.extraHoverHeight=dive?amplitude*4*(1-Smooth(p))-profile.hoverHeight*Smooth(p):
            amplitude*1.5f*(1-Smooth(p));
        result.localLean.x=amplitude*(.08f*(1-Smooth(p))+(dive?.10f:-.06f)*wave);
        break;
    case Phase::Active:
        if(dive) {
            result.extraFloorOffset={-profile.floorOffset.x,-profile.floorOffset.y};
            result.extraHoverHeight=-profile.hoverHeight*(1-Smooth(p));
            result.localLean.x=amplitude*.08f*wave;
        } else if(beam) {
            result.extraFloorOffset.y=amplitude*3; result.extraHoverHeight=amplitude*1.5f;
            result.localLean.x=amplitude*.08f;
        }
        break;
    case Phase::Recovery:
        if(dive) result.extraFloorOffset={-profile.floorOffset.x*(1-Smooth(p)),
            -profile.floorOffset.y*(1-Smooth(p))};
        else if(beam) {
            result.extraFloorOffset.y=amplitude*3*(1-Smooth(p));
            result.extraHoverHeight=amplitude*1.5f*(1-Smooth(p));
            result.localLean.x=amplitude*.08f*(1-Smooth(p));
        }
        result.extraHoverHeight+=amplitude*.15f*wave; break;
    case Phase::Intro: result.extraHoverHeight=amplitude*2*(1-Smooth(p)); break;
    default: break;
    }
    result.localLean.z+=amplitude*.035f*Unit(hitFeedback);
    return result;
}

} // namespace neondepth
