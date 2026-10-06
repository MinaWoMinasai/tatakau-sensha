#pragma once
// Generated Depth clips and all-bind validation for the existing Skeleton API.
#include "Calculation.h"
#include "Skeleton.h"
#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace neondepth {
enum class Motion { Hover, VolleyCharge, VolleyRelease, DivePull, DiveDrop, DiveLand, BeamTwist, Defeat };
inline constexpr std::array<const char*, 8> kMotionNames = {
    "Depth_Hover", "Depth_VolleyCharge", "Depth_VolleyRelease", "Depth_DivePull",
    "Depth_DiveDrop", "Depth_DiveLand", "Depth_BeamTwist", "Depth_Defeat"};

// Names and hierarchies were read from the actual AvatarSample_B GLB. The asset
// contains no embedded animation. Preview_Idle/Attack remain a separate profile.
inline constexpr std::array<const char*, 10> kMotionJoints = {
    "J_Bip_C_Chest", "J_Bip_C_Neck", "J_Bip_L_UpperArm", "J_Bip_R_UpperArm",
    "J_Bip_L_LowerArm", "J_Bip_R_LowerArm", "J_Bip_L_UpperLeg", "J_Bip_R_UpperLeg",
    "J_Bip_L_LowerLeg", "J_Bip_R_LowerLeg"};
using MotionAngles = std::array<cg2::Vector3, kMotionJoints.size()>;
struct MotionBuildResult {
    std::vector<cg2::Animation> animations;
    std::vector<std::string> skippedMotionJoints;
    bool safeToRegister = false;
    bool rootOnlyFallback = false;
    std::string bindError;
    // Owner records this diagnosis once at model load; never logs every frame.
};

// Missing expected motion names are safe only inside an otherwise valid bind
// skeleton. ApplyAnimation restores every unanimated joint's original bind;
// skipping a corrupt quaternion cannot repair that fallback. Validate BEFORE
// clip registration, visual model Update or any Draw submission.
inline bool ValidateDepthBindSkeleton(const cg2::Skeleton& skeleton, std::string* error = nullptr) {
    auto fail = [error](const char* message) { if(error) *error=message; return false; };
    const auto count=skeleton.joints.size();
    if(count==0 || count>4096 || skeleton.root!=0 || skeleton.jointMap.size()!=count)
        return fail("Depth bind skeleton has invalid root/count/name map.");
    std::vector<unsigned> childReferences(count,0);
    for(size_t i=0;i<count;++i) {
        const auto& joint=skeleton.joints[i];
        const auto found=skeleton.jointMap.find(joint.name);
        if(joint.index!=static_cast<int>(i) || joint.name.empty() || joint.name.size()>256 ||
            found==skeleton.jointMap.end() || found->second!=joint.index)
            return fail("Depth bind skeleton has invalid joint identity.");
        if(i==0 ? joint.parent.has_value() :
            (!joint.parent || *joint.parent<0 || static_cast<size_t>(*joint.parent)>=i))
            return fail("Depth bind skeleton has invalid parent order.");
        const auto& bind=joint.bindTransform;
        for(float v:{bind.translate.x,bind.translate.y,bind.translate.z})
            if(!std::isfinite(v) || std::abs(v)>10000) return fail("Depth bind translation is invalid.");
        for(float v:{bind.scale.x,bind.scale.y,bind.scale.z})
            if(!std::isfinite(v) || v<1.0e-6f || v>1000) return fail("Depth bind scale is invalid.");
        const auto& q=bind.rotate;
        const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
        if(!std::isfinite(q.x) || !std::isfinite(q.y) || !std::isfinite(q.z) || !std::isfinite(q.w) ||
            !std::isfinite(norm) || norm<=1.0e-8f)
            return fail("Depth bind quaternion is invalid.");
        for(const auto* matrix:{&joint.localMatrix,&joint.skeletonSpaceMatrix})
            for(const auto& row:matrix->m) for(float v:row)
                if(!std::isfinite(v)) return fail("Depth bind matrix is invalid.");
        for(int child:joint.children) {
            if(child<=static_cast<int>(i) || static_cast<size_t>(child)>=count ||
                skeleton.joints[static_cast<size_t>(child)].parent!=joint.index ||
                ++childReferences[static_cast<size_t>(child)]!=1)
                return fail("Depth bind skeleton has invalid child links.");
        }
    }
    for(size_t i=1;i<count;++i) if(childReferences[i]!=1)
        return fail("Depth bind skeleton contains a disconnected joint.");
    if(error) error->clear();
    return true;
}

inline cg2::Quaternion QuaternionProduct(const cg2::Quaternion& a, const cg2::Quaternion& b) {
    return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y, a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w, a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}
inline cg2::Quaternion BindRelativeRotation(const cg2::Quaternion& bind, const cg2::Vector3& angles) {
    const auto x = cg2::MakeRotateAxisAngleQuaternion({1,0,0},angles.x);
    const auto y = cg2::MakeRotateAxisAngleQuaternion({0,1,0},angles.y);
    const auto z = cg2::MakeRotateAxisAngleQuaternion({0,0,1},angles.z);
    // Row-vector rule: R(qA*qB)=R(qB)*R(qA). Same composition as Preview.
    return cg2::NormalizeQuaternion(QuaternionProduct(bind,QuaternionProduct(z,QuaternionProduct(y,x))));
}
inline MotionAngles RestAngles() {
    MotionAngles result{};
    result[2] = {0,0,-1.12f}; result[3] = {0,0,1.12f};
    result[4] = {0,.22f,0}; result[5] = {0,-.22f,0};
    return result;
}

inline MotionBuildResult CreateDepthAnimations(const cg2::Skeleton& skeleton) {
    MotionBuildResult result;
    if(!ValidateDepthBindSkeleton(skeleton,&result.bindError)) return result;
    result.safeToRegister=true;
    std::array<int, kMotionJoints.size()> indices{};
    indices.fill(-1);
    for (size_t i=0;i<kMotionJoints.size();++i) {
        const auto found = skeleton.jointMap.find(kMotionJoints[i]);
        if (found==skeleton.jointMap.end() || found->second<0 ||
            static_cast<size_t>(found->second)>=skeleton.joints.size()) {
            result.skippedMotionJoints.emplace_back(kMotionJoints[i]); continue;
        }
        indices[i]=found->second;
    }
    for (const auto pair : {std::array<size_t,2>{2,4}, {3,5}, {6,8}, {7,9}}) {
        if (indices[pair[0]]>=0 && indices[pair[1]]>=0 &&
            skeleton.joints[static_cast<size_t>(indices[pair[1]])].parent!=indices[pair[0]]) {
            result.skippedMotionJoints.emplace_back(std::string(kMotionJoints[pair[1]])+" (unexpected parent)");
            indices[pair[1]]=-1;
        }
    }
    result.rootOnlyFallback = true;
    for (int index : indices) if (index>=0) result.rootOnlyFallback=false;
    auto addPose = [&](cg2::Animation& clip, float time, const MotionAngles& angles) {
        for(size_t i=0;i<indices.size();++i) {
            if(indices[i]<0) continue;
            const auto& bind = skeleton.joints[static_cast<size_t>(indices[i])].bindTransform;
            auto& node = clip.nodeAnimations[kMotionJoints[i]];
            auto q = BindRelativeRotation(bind.rotate,angles[i]);
            if(!node.rotate.keyframes.empty() && cg2::DotQuaternion(node.rotate.keyframes.back().value,q)<0)
                q={-q.x,-q.y,-q.z,-q.w};
            node.rotate.keyframes.push_back({time,q});
            // Empty translation/scale curves use the fresh bind fallback. No root motion.
        }
    };
    auto makeClip = [&](Motion motion, const MotionAngles& pose, bool holdEnd) {
        cg2::Animation clip;
        clip.name=kMotionNames[static_cast<size_t>(motion)]; clip.duration=1.0f;
        addPose(clip,0,RestAngles()); addPose(clip,.35f,pose); addPose(clip,.72f,pose);
        addPose(clip,1,holdEnd?pose:RestAngles()); result.animations.push_back(std::move(clip));
    };
    cg2::Animation hover;
    hover.name=kMotionNames[0]; hover.duration=3.0f;
    for(int i=0;i<=8;++i) {
        auto pose=RestAngles();
        const float wave=(i==0||i==8)?0.0f:std::sin(float(i)*cg2::pi/4.0f);
        pose[0].x=.035f*wave; pose[1].y=.055f*wave;
        pose[2].z+=.035f*wave; pose[3].z-=.035f*wave;
        addPose(hover,3.0f*float(i)/8.0f,pose);
    }
    result.animations.push_back(std::move(hover));
    auto charge=RestAngles();
    charge[0]={.15f,.14f,0}; charge[2]={-.18f,-.4f,-.75f}; charge[3]={-.18f,.4f,.75f};
    charge[4]={0,1.0f,0}; charge[5]={0,-1.0f,0};
    makeClip(Motion::VolleyCharge,charge,true);
    auto release=RestAngles();
    release[0]={-.17f,0,0}; release[2]={-.12f,.95f,-.55f}; release[3]={-.12f,-.95f,.55f};
    release[4]={0,.32f,0}; release[5]={0,-.32f,0};
    makeClip(Motion::VolleyRelease,release,false);
    auto pull=charge;
    pull[0]={.32f,0,0}; pull[6]={-.35f,0,0}; pull[7]={-.35f,0,0};
    pull[8]={.55f,0,0}; pull[9]={.55f,0,0};
    makeClip(Motion::DivePull,pull,true);
    auto drop=RestAngles();
    drop[0]={.19f,0,0}; drop[2]={-.45f,0,-.25f}; drop[3]={-.45f,0,.25f};
    drop[6]={-.22f,0,0}; drop[7]={-.22f,0,0}; drop[8]={.30f,0,0}; drop[9]={.30f,0,0};
    makeClip(Motion::DiveDrop,drop,true);
    auto land=pull;
    land[0]={.40f,0,0}; land[6].x=-.52f; land[7].x=-.52f; land[8].x=.82f; land[9].x=.82f;
    makeClip(Motion::DiveLand,land,false);
    auto beam=RestAngles();
    beam[0]={.06f,.42f,0}; beam[1]={0,-.15f,0}; beam[3]={0,-.95f,.65f}; beam[5]={0,-.28f,0};
    makeClip(Motion::BeamTwist,beam,false);
    auto defeat=land;
    defeat[0]={.48f,-.12f,.08f}; defeat[1]={.23f,0,0};
    defeat[2]={.15f,0,-.95f}; defeat[3]={.15f,0,.95f};
    makeClip(Motion::Defeat,defeat,true);
    return result;
}
} // namespace neondepth
