// Pure CPU presentation/bind/motion/lifecycle regression tests; actual asset
// registration and WARP palette coverage remain in neon_preview_animation_tests.
#include "game/enemy/visual/NeonDepthPresentation.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

namespace {
void Require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
bool Near(float a,float b,float tolerance=1.0e-4f) { return std::abs(a-b)<=tolerance; }
cg2::SkeletonNode Node(const char* name,cg2::Vector3 translate={}) {
    cg2::SkeletonNode result; result.name=name; result.transform.translate=translate; return result;
}
cg2::Skeleton Synthetic() {
    auto root=Node("Root");
    auto chest=Node("J_Bip_C_Chest",{0,1,0});
    chest.children.push_back(Node("J_Bip_C_Neck",{0,.3f,0}));
    auto leftArm=Node("J_Bip_L_UpperArm",{-.25f,.2f,0});
    auto leftLower=Node("J_Bip_L_LowerArm",{-.25f,0,0});
    leftLower.children.push_back(Node("J_Bip_L_Hand",{-.2f,0,0}));
    leftArm.children.push_back(leftLower); chest.children.push_back(leftArm);
    auto rightArm=Node("J_Bip_R_UpperArm",{.25f,.2f,0});
    auto rightLower=Node("J_Bip_R_LowerArm",{.25f,0,0});
    rightLower.children.push_back(Node("J_Bip_R_Hand",{.2f,0,0}));
    rightArm.children.push_back(rightLower); chest.children.push_back(rightArm);
    root.children.push_back(chest);
    auto leftLeg=Node("J_Bip_L_UpperLeg",{-.1f,.8f,0});
    auto leftShin=Node("J_Bip_L_LowerLeg",{0,-.4f,0});
    leftShin.children.push_back(Node("J_Bip_L_Foot",{0,-.4f,.03f}));
    leftLeg.children.push_back(leftShin); root.children.push_back(leftLeg);
    auto rightLeg=Node("J_Bip_R_UpperLeg",{.1f,.8f,0});
    auto rightShin=Node("J_Bip_R_LowerLeg",{0,-.4f,0});
    rightShin.children.push_back(Node("J_Bip_R_Foot",{0,-.4f,.03f}));
    rightLeg.children.push_back(rightShin); root.children.push_back(rightLeg);
    return cg2::SkeletonSystem::Create(root);
}
void RebuildMap(cg2::Skeleton& skeleton) {
    skeleton.jointMap.clear();
    for(const auto& joint:skeleton.joints) skeleton.jointMap.emplace(joint.name,joint.index);
}
void ValidAndCorruptBind() {
    auto skeleton=Synthetic();
    Require(neondepth::ValidateDepthBindSkeleton(skeleton),"valid bind rejected");
    auto built=neondepth::CreateDepthAnimations(skeleton);
    Require(built.safeToRegister && built.animations.size()==8 && !built.rootOnlyFallback,"normal build rejected");
    Require(built.skippedMotionJoints.empty(),"actual synthetic names not found");
    std::set<std::string> names;
    for(const auto& clip:built.animations) {
        Require(names.insert(clip.name).second,"duplicate generated names");
        for(const auto& [name,node]:clip.nodeAnimations) {
            Require(skeleton.jointMap.contains(name),"invented joint");
            Require(node.translate.keyframes.empty() && node.scale.keyframes.empty(),"generated root motion");
            float previous=-1;
            for(const auto& key:node.rotate.keyframes) {
                Require(key.time>previous && key.time<=clip.duration,"unordered/outside clip keys");
                Require(Near(cg2::DotQuaternion(key.value,key.value),1,.001f),"nonunit generated quaternion");
                previous=key.time;
            }
        }
    }
    auto missing=skeleton;
    for(auto& joint:missing.joints) joint.name="Renamed_"+joint.name;
    RebuildMap(missing);
    auto fallback=neondepth::CreateDepthAnimations(missing);
    Require(fallback.safeToRegister && fallback.rootOnlyFallback && fallback.animations.size()==8,"valid renamed bind fallback rejected");
    for(const auto& clip:fallback.animations) Require(clip.nodeAnimations.empty(),"missing-name fallback animated unknown bones");
    auto reject=[&](cg2::Skeleton corrupt) {
        std::string error;
        Require(!neondepth::ValidateDepthBindSkeleton(corrupt,&error) && !error.empty(),"corrupt bind accepted");
        const auto result=neondepth::CreateDepthAnimations(corrupt);
        Require(!result.safeToRegister && result.animations.empty() && !result.bindError.empty(),"corrupt bind claimed finite fallback");
    };
    auto corrupt=missing; corrupt.joints.back().bindTransform.rotate.x=std::numeric_limits<float>::quiet_NaN(); reject(corrupt);
    corrupt=missing; corrupt.joints.back().bindTransform.rotate={0,0,0,0}; reject(corrupt);
    corrupt=missing; corrupt.joints.back().bindTransform.translate.y=std::numeric_limits<float>::infinity(); reject(corrupt);
    corrupt=missing; corrupt.joints.back().bindTransform.scale.x=-1; reject(corrupt);
    corrupt=missing; corrupt.root=1; reject(corrupt);
    corrupt=missing; corrupt.joints[1].parent=1; reject(corrupt);
    corrupt=missing; corrupt.joints[0].children.push_back(1); reject(corrupt);
    corrupt=missing; corrupt.jointMap.begin()->second=999; reject(corrupt);
}
void SamplingAndFoot() {
    auto skeleton=Synthetic();
    neondepth::BindPlacement bind;
    Require(neondepth::TryBindPlacement(skeleton,{{-.7f,0,-.2f},{.7f,1.6f,.2f}},bind),"bind foot failed");
    Require(bind.footBonesFound==2 && Near(bind.stableFootLocal.x,0) && Near(bind.stableFootLocal.y,0) &&
        Near(bind.stableFootLocal.z,.03f),"sole did not use loaded bind matrices/bounds");
    const auto clips=neondepth::CreateDepthAnimations(skeleton).animations;
    cg2::AnimationPlayer player; player.SetAnimation(&clips[0]); player.SetLoop(false); player.Seek(.375f);
    cg2::SkeletonSystem::ApplyAnimation(skeleton,player);
    const auto expected=skeleton.joints;
    for(int iteration=0;iteration<1000;++iteration) {
        // Deliberate accumulated/dirty prior pose must be replaced by a fresh sample.
        for(auto& joint:skeleton.joints) joint.transform.translate={50,60,70};
        cg2::SkeletonSystem::ApplyAnimation(skeleton,player);
        for(size_t i=0;i<expected.size();++i) {
            Require(std::memcmp(&expected[i].skeletonSpaceMatrix,&skeleton.joints[i].skeletonSpaceMatrix,
                sizeof(cg2::Matrix4x4))==0,"same-time pose drifted");
            Require(neondepth::Finite(skeleton.joints[i].skeletonSpaceMatrix),"sample matrix not finite");
        }
    }
    // Sole anchor is cached once, not redefined by animation's moving limbs.
    neondepth::PresentationConfig profile; auto config=neondepth::MakePlacementConfig(bind,profile);
    neondepth::PlacementResult placed;
    Require(neondepth::TryPlacement({},config,{{10,20,0},profile.floorOffset,profile.hoverHeight,-cg2::pi*.5f,{}},placed),"foot world failed");
    cg2::Vector3 socket{-999,-999,-999};
    Require(neondepth::TrySocketWorld(skeleton,placed.world,neondepth::Socket::Chest,socket),"real sampled socket failed");
    const auto chestIndex=skeleton.jointMap.at("J_Bip_C_Chest");
    const auto& chest=skeleton.joints[static_cast<size_t>(chestIndex)].skeletonSpaceMatrix;
    const auto expectedSocket=cg2::TransformMatrix(cg2::Vector3{chest.m[3][0],chest.m[3][1],chest.m[3][2]},placed.world);
    Require(Near(socket.x,expectedSocket.x) && Near(socket.y,expectedSocket.y) && Near(socket.z,expectedSocket.z),"socket did not use current pose/world");
    auto missing=skeleton; missing.jointMap.erase("J_Bip_C_Chest");
    const auto previous=socket;
    Require(!neondepth::TrySocketWorld(missing,placed.world,neondepth::Socket::Chest,socket) &&
        Near(socket.x,previous.x)&&Near(socket.y,previous.y)&&Near(socket.z,previous.z),"missing socket destroyed fallback endpoint");
}
void NonrestartAndFormation() {
    neondepth::Snapshot snapshot; snapshot.generation=7; snapshot.plan.instance=3;
    snapshot.plan.attack=neondepth::Attack::Volley; snapshot.phase=neondepth::Phase::Telegraph; snapshot.duration=.85f;
    neondepth::MotionGate gate;
    Require(gate.Step(snapshot).select,"entry did not select");
    for(int i=0;i<1000;++i) {
        snapshot.progress=float(i)/1000;
        const auto command=gate.Step(snapshot);
        Require(!command.select && !command.phaseChanged,"same phase restarted/seeking");
    }
    snapshot.phase=neondepth::Phase::Locked; snapshot.duration=.3f;
    const auto locked=gate.Step(snapshot);
    Require(!locked.select && locked.phaseChanged && !locked.mapping.playing && locked.mapping.entryTime>=0,"locked charge not held once");
    Require(!gate.Step(snapshot).phaseChanged,"locked repeats sought again");
    ++snapshot.plan.instance; snapshot.phase=neondepth::Phase::Telegraph;
    const auto next=gate.Step(snapshot);
    Require(next.select && next.restartSameClip,"new same-clip attack did not restart at entry");
    Require(!gate.Step(snapshot).select,"new instance restarted twice");
    snapshot.phase=neondepth::Phase::Intro; snapshot.progress=0;
    auto formation=neondepth::MapFormation(snapshot);
    Require(formation.core==0 && formation.body==0,"intro first frame fully formed");
    snapshot.progress=.2f; formation=neondepth::MapFormation(snapshot);
    Require(formation.core>0 && formation.body==0,"body precedes core");
    snapshot.phase=neondepth::Phase::Reposition; formation=neondepth::MapFormation(snapshot);
    Require(formation.core==1 && formation.connector==1 && formation.body==1,"natural/skip intro endpoint differs");
    snapshot.plan.attack=neondepth::Attack::Dive; snapshot.phase=neondepth::Phase::Airborne; snapshot.progress=1;
    neondepth::PresentationConfig profile;
    Require(Near(profile.hoverHeight+neondepth::MapBodyMotion(snapshot,profile).extraHoverHeight,0),"dive never reached floor");
    snapshot.phase=neondepth::Phase::Active; snapshot.progress=0;
    Require(Near(profile.hoverHeight+neondepth::MapBodyMotion(snapshot,profile).extraHoverHeight,0),"landing jumped to floor");
    profile.floorOffset={2,4};
    const cg2::Vector3 anchor{12,15,0};
    const auto placementConfig=neondepth::MakePlacementConfig({{0,0,.03f},1.6f,2},profile);
    auto footAt=[&](neondepth::Phase phase,float progress) {
        snapshot.phase=phase; snapshot.progress=progress;
        const auto motion=neondepth::MapBodyMotion(snapshot,profile);
        neondepth::PlacementResult result;
        Require(neondepth::TryPlacement({},placementConfig,{anchor,
            {profile.floorOffset.x+motion.extraFloorOffset.x,profile.floorOffset.y+motion.extraFloorOffset.y},
            profile.hoverHeight+motion.extraHoverHeight,-cg2::pi*.5f,motion.localLean},result),"phase placement failed");
        return result.footWorld;
    };
    auto samePosition=[&](cg2::Vector3 a,cg2::Vector3 b) {
        return Near(a.x,b.x)&&Near(a.y,b.y)&&Near(a.z,b.z);
    };
    const auto flightEnd=footAt(neondepth::Phase::Airborne,1);
    const auto landing=footAt(neondepth::Phase::Active,0);
    Require(samePosition(flightEnd,anchor) && samePosition(landing,anchor),"visible dive landed outside authoritative floor core");
    Require(samePosition(footAt(neondepth::Phase::Locked,1),footAt(neondepth::Phase::Airborne,0)),"dive flight start discontinuity");
    Require(samePosition(footAt(neondepth::Phase::Active,1),footAt(neondepth::Phase::Recovery,0)),"dive recovery start discontinuity");
    Require(samePosition(footAt(neondepth::Phase::Recovery,1),footAt(neondepth::Phase::Reposition,0)),"dive rest position discontinuity");
    snapshot.plan.attack=neondepth::Attack::Beam;
    Require(samePosition(footAt(neondepth::Phase::Locked,1),footAt(neondepth::Phase::Active,0)),"beam activation position jumped");
    Require(samePosition(footAt(neondepth::Phase::Active,1),footAt(neondepth::Phase::Recovery,0)),"beam recovery position jumped");
}
void Lifecycle() {
    neondepth::Lifecycle life;
    neondepth::LifecycleInput input; input.encounterActive=true; input.intro=true; input.introProgress=.5f;
    life.Update(input,{},.1f); Require(life.GetLife()==neondepth::Life::Intro,"intro state lost");
    input.intro=false; life.Update(input,{},0); Require(life.GetLife()==neondepth::Life::Active,"skip endpoint differs");
    input.hp=0; input.alive=false; life.Update(input,{},.1f);
    Require(life.GetLife()==neondepth::Life::Collapse && life.DidDeathStart() && life.GetCollapseProgress()==0,"death advanced before start");
    life.Update(input,{},1); Require(life.GetLife()==neondepth::Life::Collapse && Near(life.GetCollapseDelta(),.1f),"spike skipped collapse");
    life.Update(input,{},0); Require(Near(life.GetCollapseDelta(),0),"pause advanced collapse");
    life.Update(input,{},.1f); life.Update(input,{},.1f); life.Update(input,{},.05f);
    Require(life.GetLife()==neondepth::Life::Dissolve && life.ShouldFreezePose() &&
        Near(life.GetCollapseDelta(),.05f) && Near(life.GetCollapseProgress(),1) && Near(life.GetDissolveProgress(),0),"freeze pose boundary wrong");
    input.hp=99; input.alive=true;
    life.Update(input,{.8f,2},0);
    Require(life.GetLife()==neondepth::Life::Dissolve && !life.ShouldFreezePose() &&
        Near(life.GetLatchedTiming().collapseSeconds,.35f),"death revived or timing edit altered latch");
    unsigned releases=0;
    for(int i=0;i<16;++i) { life.Update(input,{},.1f); releases+=life.ShouldReleaseResources(); }
    Require(life.GetLife()==neondepth::Life::Finished && life.GetDissolveProgress()==1 && releases==1,"terminal/release contract wrong");
    life.Update(input,{},1); Require(!life.ShouldReleaseResources(),"terminal released twice");
    life.Reset(); input.aborted=true; life.Update(input,{},0);
    Require(life.GetLife()==neondepth::Life::Finished && life.ShouldReleaseResources(),"abort did not request release");
    // Finish is governed by capped elapsed >= latched total, never by a rounded
    // progress ratio. .35 + .8 previously risked a ratio permanently below1.
    for(int collapseIndex=0;collapseIndex<=13;++collapseIndex) {
        for(int dissolveIndex=0;dissolveIndex<=32;++dissolveIndex) {
            const neondepth::LifecycleTiming timing{.15f+.65f*float(collapseIndex)/13,
                .4f+1.6f*float(dissolveIndex)/32};
            neondepth::Lifecycle candidate;
            neondepth::LifecycleInput alive; alive.encounterActive=true;
            candidate.Update(alive,timing,0); alive.hp=0; alive.alive=false;
            candidate.Update(alive,timing,0);
            unsigned releaseCount=0;
            for(int step=0;step<40;++step) { candidate.Update(alive,timing,.1f);
                releaseCount+=candidate.ShouldReleaseResources(); }
            Require(candidate.GetLife()==neondepth::Life::Finished && candidate.GetDissolveProgress()==1 &&
                releaseCount==1,"valid timing grid could never reach/release terminal");
        }
    }
    neondepth::Lifecycle rounded;
    input={}; input.encounterActive=true; rounded.Update(input,{.35f,.8f},0);
    input.hp=0; input.alive=false; rounded.Update(input,{.35f,.8f},0);
    for(int step=0;step<20;++step) rounded.Update(input,{.35f,.8f},.1f);
    Require(rounded.GetLife()==neondepth::Life::Finished && rounded.GetDissolveProgress()==1,
        "explicit .35+.8 timing regression remained in Dissolve");
}
}
int main() {
    try { ValidAndCorruptBind(); SamplingAndFoot(); NonrestartAndFormation(); Lifecycle();
        std::cout<<"Depth presentation tests passed\n"; return 0; }
    catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
