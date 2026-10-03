#include "NeonPreviewAnimations.h"
#include "SkinCluster.h"
#include "Calculation.h"
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

namespace neonpreview {
namespace {
constexpr std::array<const char*, 6> kJoints = {
    "J_Bip_C_Chest", "J_Bip_C_Neck", "J_Bip_L_UpperArm", "J_Bip_R_UpperArm",
    "J_Bip_L_LowerArm", "J_Bip_R_LowerArm"
};
constexpr std::array<const char*, 3> kClips = { "BindPose", "Preview_Idle", "Preview_Attack" };
using Angles = std::array<cg2::Vector3, kJoints.size()>;

cg2::Quaternion Product(const cg2::Quaternion& a, const cg2::Quaternion& b) {
    return { a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z };
}

cg2::Quaternion Rotation(const cg2::Quaternion& bind, const cg2::Vector3& angles) {
    const auto x = cg2::MakeRotateAxisAngleQuaternion({1,0,0}, angles.x);
    const auto y = cg2::MakeRotateAxisAngleQuaternion({0,1,0}, angles.y);
    const auto z = cg2::MakeRotateAxisAngleQuaternion({0,0,1}, angles.z);
    // 行ベクトルではR(qA*qB)=R(qB)*R(qA)。局所差分Rx*Ry*Rzの後にbind回転を適用する。
    return cg2::NormalizeQuaternion(Product(bind, Product(z, Product(y, x))));
}

Angles IdleAngles(float wave) {
    return { cg2::Vector3{0.016f*wave,0,0}, cg2::Vector3{0,0.04f*wave,0},
        cg2::Vector3{0,0,-1.12f+0.028f*wave}, cg2::Vector3{0,0,1.12f-0.028f*wave},
        cg2::Vector3{0,0.22f+0.02f*wave,0}, cg2::Vector3{0,-0.22f-0.02f*wave,0} };
}

void AddPose(cg2::Animation& clip, const cg2::Skeleton& skeleton, float time, const Angles& angles) {
    for (size_t i=0; i<kJoints.size(); ++i) {
        const auto& bind = skeleton.joints.at(skeleton.jointMap.at(kJoints[i])).bindTransform;
        auto& node = clip.nodeAnimations[kJoints[i]];
        auto q = Rotation(bind.rotate, angles[i]);
        if (!node.rotate.keyframes.empty() && cg2::DotQuaternion(node.rotate.keyframes.back().value, q) < 0.0f)
            q = {-q.x,-q.y,-q.z,-q.w};
        node.rotate.keyframes.push_back({time,q});
        // 回転だけを作る。Translation/Scale曲線は空のまま、既存Samplerのbind fallbackを使う。
    }
}
}

std::vector<cg2::Animation> CreateAnimations(const cg2::Skeleton& skeleton) {
    std::string missing;
    for (const auto* name : kJoints) if (!skeleton.jointMap.contains(name)) missing += std::string(name) + " ";
    if (!missing.empty()) throw std::runtime_error("Preview motion missing joints: " + missing);
    // 実モデルで確認した階層を検証する。別モデルへ名前だけで誤適用しない。
    for (const auto side : {"L", "R"}) {
        const auto upperName = std::string("J_Bip_") + side + "_UpperArm";
        const auto lowerName = std::string("J_Bip_") + side + "_LowerArm";
        const auto& lower = skeleton.joints.at(skeleton.jointMap.at(lowerName));
        const auto& upper = skeleton.joints.at(skeleton.jointMap.at(upperName));
        if (lower.parent != upper.index || (side[0]=='L' ? lower.bindTransform.translate.x <= 0 : lower.bindTransform.translate.x >= 0))
            throw std::runtime_error("Unexpected AvatarSample_B arm bind axes: " + lowerName);
    }
    cg2::Animation idle;
    idle.name = kClips[1]; idle.duration = 3.0f;
    for (int i=0; i<=8; ++i) {
        const float wave = (i==0 || i==8) ? 0.0f : std::sin(static_cast<float>(i)*cg2::pi/4.0f);
        AddPose(idle, skeleton, 3.0f*static_cast<float>(i)/8.0f, IdleAngles(wave));
    }
    cg2::Animation attack;
    attack.name = kClips[2]; attack.duration = 1.8f;
    const auto rest = IdleAngles(0.0f);
    auto pull = rest;
    pull[0] = {0.04f,0.06f,0}; pull[3] = {0,0.4f,1.0f}; pull[5] = {0,-1.1f,0};
    auto push = rest;
    push[0] = {-0.08f,-0.04f,0}; push[1] = {0,0.04f,0};
    push[3] = {0,-1.15f,0.55f}; push[5] = {0,-0.25f,0};
    auto recover = rest;
    recover[0] = {-0.025f,0,0}; recover[3] = {0,-0.35f,0.9f}; recover[5] = {0,-0.4f,0};
    AddPose(attack,skeleton,0.0f,rest);
    AddPose(attack,skeleton,0.3f,pull);
    AddPose(attack,skeleton,0.55f,pull); // 短い溜め。
    AddPose(attack,skeleton,0.82f,push);
    AddPose(attack,skeleton,1.05f,push);
    AddPose(attack,skeleton,1.35f,recover);
    AddPose(attack,skeleton,1.8f,rest);
    return {std::move(idle),std::move(attack)};
}

bool SelectAnimation(cg2::SkinnedModel& model, Clip clip) {
    const auto* name = kClips.at(static_cast<size_t>(clip));
    // 比較用BindPoseはPause中でも即座に元姿勢へ戻す（duration=0なのでSeek UIは使えない）。
    const bool selected = clip == Clip::BindPose || model.GetAnimation().name == name ? model.SetAnimation(name, true)
        : model.TransitionToAnimation(name, 0.2f);
    if (selected) model.SetAnimationLoop(clip==Clip::Idle);
    return selected;
}

void UpdateAnimation(cg2::SkinnedModel& model, float deltaTime) {
    model.Update(deltaTime);
    if (!model.IsAnimationPaused() && model.GetAnimation().name == kClips[2] &&
        !model.IsCurrentAnimationPlaying()) SelectAnimation(model, Clip::Idle);
}
}
