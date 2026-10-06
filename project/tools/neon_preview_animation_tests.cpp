// 実SkinnedModel/Animation/Skeleton/SkinClusterを実GLBとWARP upload bufferで検証する。
// Texture/Descriptorサービスだけを隔離する。描画・画像decodeの成功はこの数値テストから主張しない。
#include "SkinCluster.h"
#include "Calculation.h"
#include "TextureManager.h"
#include "../game/debug/NeonPreviewAnimations.h"
#include "../game/debug/NeonDissolvePreviewController.h"
#include "../game/enemy/visual/NeonDepthPresentation.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"dxguid.lib")
using namespace cg2;
using Microsoft::WRL::ComPtr;
namespace {
void Require(bool success, const char* message) { if (!success) throw std::runtime_error(message); }
void Check(HRESULT hr) { Require(SUCCEEDED(hr), "WARP resource setup failed"); }
DirectXCommon* testDx = nullptr;
ComPtr<ID3D12DescriptorHeap> testHeap;
unsigned testDescriptorFrees = 0;
bool Near(float a,float b,float e=1e-5f) { return std::abs(a-b)<e; }
bool Near(const Vector3& a,const Vector3& b) { return Near(a.x,b.x)&&Near(a.y,b.y)&&Near(a.z,b.z); }
bool Near(const Quaternion& a,const Quaternion& b) { return std::abs(DotQuaternion(a,b))>0.99999f; }
bool Near(const QuaternionTransform& a,const QuaternionTransform& b) {
    return Near(a.translate,b.translate)&&Near(a.scale,b.scale)&&Near(a.rotate,b.rotate);
}
std::vector<QuaternionTransform> Pose(const SkinnedModel& model) {
    std::vector<QuaternionTransform> result;
    for(const auto& joint:model.GetSkeleton().joints) result.push_back(joint.transform);
    return result;
}
void RequirePose(const SkinnedModel& model,const std::vector<QuaternionTransform>& pose) {
    for(size_t i=0;i<pose.size();++i) Require(Near(model.GetSkeleton().joints[i].transform,pose[i]),"Pose changed while paused");
}
const Joint& JointByName(const SkinnedModel& model,const char* name) {
    return model.GetSkeleton().joints.at(model.GetSkeleton().jointMap.at(name));
}
Vector3 SkinnedPosition(const SkinnedModel& model,size_t index) {
    const auto& v=model.GetAsset().modelData.vertices.at(index).position;
    const auto& influence=model.GetSkinCluster().GetInfluences().at(index);
    Vector3 result{};
    for(size_t i=0;i<4;++i) {
        const auto& m=model.GetSkinCluster().GetPalette().at(influence.jointIndices[i]).skeletonSpaceMatrix;
        result.x+=influence.weights[i]*(v.x*m.m[0][0]+v.y*m.m[1][0]+v.z*m.m[2][0]+m.m[3][0]);
        result.y+=influence.weights[i]*(v.x*m.m[0][1]+v.y*m.m[1][1]+v.z*m.m[2][1]+m.m[3][1]);
        result.z+=influence.weights[i]*(v.x*m.m[0][2]+v.y*m.m[1][2]+v.z*m.m[2][2]+m.m[3][2]);
    }
    return result;
}
void TestSkinnedBounds(const SkinnedModel& model) {
    const auto before=Pose(model);
    const float time=model.GetCurrentAnimationTime();
    for(Vector3 direction : {Vector3{1,0,0},Vector3{0,1,0},Vector3{0,0,1},Vector3{1,-1,.3f}}) {
        Require(NormalizeNeonDirection(direction),"Bounds test direction invalid");
        float low=0,high=0;
        Require(ComputeNeonSkinnedProjectionBounds(model,direction,low,high),"Current actual-model Palette bounds failed");
        float expectedLow=(std::numeric_limits<float>::max)(),expectedHigh=(std::numeric_limits<float>::lowest)();
        for(size_t vertex=0;vertex<model.GetAsset().modelData.vertices.size();++vertex) {
            const auto p=SkinnedPosition(model,vertex);
            const float projection=p.x*direction.x+p.y*direction.y+p.z*direction.z;
            expectedLow=(std::min)(expectedLow,projection); expectedHigh=(std::max)(expectedHigh,projection);
            Require(projection>=low && projection<=high,"Global pose bounds omit a weighted vertex");
        }
        const float padding=(std::max)(1.0e-5f,(std::max)(std::abs(expectedLow),std::abs(expectedHigh))*1.0e-5f);
        Require(Near(low,expectedLow-padding) && Near(high,expectedHigh+padding) && high>low,
            "Bounds must reflect actual four-influence Palette geometry and documented conservative padding");
    }
    float low=123,high=456;
    Require(!ComputeNeonSkinnedProjectionBounds(model,{std::numeric_limits<float>::quiet_NaN(),0,0},low,high)
        && low==123 && high==456,"Failed bounds query must leave prior output intact");
    Require(!ComputeNeonSkinnedProjectionBounds(model,{0,0,0},low,high) && low==123 && high==456,
        "Zero direction must reject without corrupting existing scan bounds");
    SkinnedModel empty;
    Require(!ComputeNeonSkinnedProjectionBounds(empty,{1,0,0},low,high) && low==123 && high==456,
        "Uninitialized model must reject bounds query without corrupting existing output");
    RequirePose(model,before);
    Require(model.GetCurrentAnimationTime()==time,"Start-time bounds scan must not advance animation");
}
void RequireSamePlayback(const SkinnedModel::AnimationPlaybackState& actual,
    const SkinnedModel::AnimationPlaybackState& expected) {
    Require(actual.animationIndex==expected.animationIndex && Near(actual.time,expected.time) && Near(actual.speed,expected.speed)
        && actual.playing==expected.playing && actual.loop==expected.loop && actual.paused==expected.paused
        && actual.transitionActive==expected.transitionActive && Near(actual.transitionDuration,expected.transitionDuration)
        && Near(actual.transitionElapsed,expected.transitionElapsed) && actual.transitionStartPose.size()==expected.transitionStartPose.size(),
        "Dissolve reset must restore the full playback checkpoint, including an in-flight blend");
    for(size_t i=0;i<actual.transitionStartPose.size();++i)
        Require(Near(actual.transitionStartPose[i],expected.transitionStartPose[i]),"Dissolve checkpoint blend source changed");
}
void TestDissolveController(SkinnedModel& model) {
    Matrix4x4 world{},camera{};
    for(size_t i=0;i<4;++i) world.m[i][i]=camera.m[i][i]=1;
    world.m[0][0]=2; world.m[1][1]=.7f; world.m[2][2]=1.5f;
    model.SetAnimation("BindPose"); model.SetAnimationPlaying(true); model.Update(0);
    model.TransitionToAnimation("Preview_Idle",.3f); model.Update(.07f);
    const auto original=model.CaptureAnimationPlaybackState();
    const auto frozen=Pose(model);
    Require(original.transitionActive && !original.paused,"Controller test must start with a live in-flight blend");
    NeonDissolvePreviewController controller;
    const auto originalControllerParams=controller.GetParams();
    NeonDissolveParams settings; settings.seed=771; settings.noiseStrength=.12f;
    Require(controller.Trigger(model,world,camera,NeonDissolveDirection::UpperLeftToLowerRight,settings,.15f,2),
        "Dissolve Trigger failed on actual weighted model");
    const auto originalDissolve=controller.GetParams();
    Require(controller.IsActive() && controller.IsPlaying() && model.IsAnimationPaused() && controller.GetParams().progress==0,
        "Trigger must freeze displayed pose and start intact");
    RequireSamePlayback(controller.GetSavedPlayback(),original);
    Require(controller.GetFrozenPose().size()==frozen.size(),"Frozen actual pose metadata must own all model joints");
    for(size_t i=0;i<frozen.size();++i) Require(Near(controller.GetFrozenPose()[i],frozen[i]),"Frozen actual pose metadata differs from displayed blend pose");
    controller.Update(.1f); model.Update(.5f); RequirePose(model,frozen);
    Require(controller.GetParams().progress==0 && Near(model.GetCurrentAnimationTime(),original.time),
        "Start wait must preserve intact mask and frozen clip/blend");
    controller.Update(.3f);
    Require(Near(controller.GetParams().progress,.125f),"Dissolve wait/duration timing is wrong");
    controller.SetPlaying(false);
    const float stopped=controller.GetElapsed();
    for(float dt:{.5f,-1.0f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) controller.Update(dt);
    model.Update(.5f); RequirePose(model,frozen);
    Require(controller.GetElapsed()==stopped,"Dissolve Pause must freeze its timeline as well as pose");
    Require(controller.SetPlaybackSpeed(2) && !controller.SetPlaybackSpeed(0) && !controller.SetPlaybackSpeed(std::numeric_limits<float>::quiet_NaN()),
        "Dissolve speed must reject invalid values without losing prior valid speed");
    controller.SetPlaying(true); controller.Update(.25f);
    Require(Near(controller.GetParams().progress,.375f),"Resume/speed must advance from current dissolve time");
    const float validElapsed=controller.GetElapsed();
    for(float dt:{-1.0f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) controller.Update(dt);
    Require(controller.GetElapsed()==validElapsed,"Invalid delta time must not mutate an actively playing dissolve clock");
    Require(controller.Seek(.6f) && !controller.IsPlaying() && Near(controller.GetParams().progress,.6f)
        && !controller.Seek(std::numeric_limits<float>::quiet_NaN()) && Near(controller.GetParams().progress,.6f),
        "Seek must pause at exact progress and reject nonfinite input atomically");
    Require(controller.Restart(model),"Same-pose restart failed"); model.Update(0); RequirePose(model,frozen);
    Require(controller.GetParams().progress==0 && controller.GetParams().seed==originalDissolve.seed
        && Near(controller.GetParams().scanMin,originalDissolve.scanMin) && Near(controller.GetParams().scanMax,originalDissolve.scanMax)
        && Near(controller.GetParams().direction,originalDissolve.direction),"Restart must preserve initial direction/seed/frozen-pose bounds");
    auto movedCamera=camera; movedCamera.m[0][0]=-1;
    settings.seed=999;
    Require(controller.Trigger(model,world,movedCamera,NeonDissolveDirection::LeftToRight,settings),"Repeated Trigger restart failed");
    Require(controller.GetParams().seed==originalDissolve.seed && Near(controller.GetParams().direction,originalDissolve.direction)
        && controller.GetStartCameraWorld().m[0][0]==1,"Repeated Trigger/camera change must not recapture a different scan plane");
    controller.Update(100); model.Update(.8f); RequirePose(model,frozen);
    Require(controller.GetParams().progress==1 && !controller.IsPlaying(),"Dissolve must end completely and stop");
    controller.SetPlaying(true); controller.Update(.1f);
    Require(!controller.IsPlaying() && controller.GetParams().progress==1,"Completed dissolve must require Restart/Seek instead of implicitly restarting playback");
    Require(controller.Reset(model) && !controller.IsActive() && controller.GetParams().enabled==0,"Reset must disable dissolve");
    Require(std::memcmp(&controller.GetParams(),&originalControllerParams,sizeof(originalControllerParams))==0,
        "Reset must restore the entire pre-Trigger dissolve configuration, not just enabled/progress");
    model.Update(0); RequirePose(model,frozen);
    RequireSamePlayback(model.CaptureAnimationPlaybackState(),original);
    model.Update(.03f);
    Require(!Near(JointByName(model,"J_Bip_L_UpperArm").transform.rotate,frozen[JointByName(model,"J_Bip_L_UpperArm").index].rotate),
        "Reset of originally running blend must resume rather than restarting Idle/bind pose");
    model.SetAnimation("Preview_Attack"); model.SeekCurrentAnimation(.82f); model.SetAnimationPlaying(false); model.Update(0);
    TestSkinnedBounds(model);
    const auto paused=model.CaptureAnimationPlaybackState(); const auto attackPose=Pose(model);
    for(int invalid=0;invalid<5;++invalid) {
        auto badWorld=world; auto badSettings=settings;
        float wait=.15f,duration=2;
        if(invalid==0) duration=0;
        if(invalid==1) wait=std::numeric_limits<float>::quiet_NaN();
        if(invalid==2) badSettings.noiseScale=std::numeric_limits<float>::infinity();
        if(invalid==3) badWorld.m[0][0]=0;
        if(invalid==4) badSettings.edgeColor.y=std::numeric_limits<float>::quiet_NaN();
        Require(!controller.Trigger(model,badWorld,camera,NeonDissolveDirection::UpperLeftToLowerRight,badSettings,wait,duration)
            && !controller.IsActive() && !controller.GetError().empty(),"Invalid Trigger must fail without changing model/controller state");
        RequireSamePlayback(model.CaptureAnimationPlaybackState(),paused); RequirePose(model,attackPose);
    }
    Require(controller.Trigger(model,world,camera,NeonDissolveDirection::UpperLeftToLowerRight,settings),"Paused Attack Trigger failed");
    controller.Update(.9f); model.Update(.9f); RequirePose(model,attackPose);
    Require(controller.Reset(model),"Paused Attack Reset failed"); model.Update(0); RequirePose(model,attackPose);
    RequireSamePlayback(model.CaptureAnimationPlaybackState(),paused);
    std::cout<<"PASS: actual skinned global projection bounds for Bind/Idle/Attack; dissolve Trigger freezes running blend/paused Attack, wait/Pause/Resume/speed/Seek/end, same-pose Restart and repeated Trigger preserve seed/direction, invalid Trigger atomic and Reset restores full original checkpoint.\n";
}
}

namespace {
void TestDepthAnimations(SkinnedModel& model) {
    model.SetAnimation("BindPose"); model.SetAnimationPlaying(false); model.Update(0);
    const auto originalBind = Pose(model);
    std::string error;
    Require(neondepth::ValidateDepthBindSkeleton(model.GetSkeleton(), &error), error.c_str());
    neondepth::BindBounds bounds;
    Require(ComputeNeonSkinnedProjectionBounds(model, {1,0,0}, bounds.low.x, bounds.high.x) &&
        ComputeNeonSkinnedProjectionBounds(model, {0,1,0}, bounds.low.y, bounds.high.y) &&
        ComputeNeonSkinnedProjectionBounds(model, {0,0,1}, bounds.low.z, bounds.high.z), "Actual Depth bind bounds invalid");
    neondepth::BindPlacement foot;
    Require(neondepth::TryBindPlacement(model.GetSkeleton(), bounds, foot) && foot.footBonesFound == 2 &&
        Near(foot.stableFootLocal.y, bounds.low.y), "Actual loaded sole anchor invalid");
    auto generated = neondepth::CreateDepthAnimations(model.GetSkeleton());
    Require(generated.safeToRegister && generated.skippedMotionJoints.empty() && generated.animations.size() == 8,
        "Actual Depth motion bones/clips invalid");
    const auto previousCount = model.GetAnimations().size();
    Require(model.RegisterAnimations(std::move(generated.animations), &error), error.c_str());
    Require(model.GetAnimations().size() == previousCount + 8, "Depth registration did not append eight clips once");
    model.SetAnimation(neondepth::kMotionNames[0]); model.SeekCurrentAnimation(0); model.Update(0);
    const auto hover0 = Pose(model);
    std::vector<Vector3> restVertices;
    for (size_t i = 0; i < model.GetAsset().modelData.vertices.size(); ++i) restVertices.push_back(SkinnedPosition(model, i));
    model.SeekCurrentAnimation(3); model.Update(0); RequirePose(model, hover0);
    for (size_t motion = 0; motion < neondepth::kMotionNames.size(); ++motion) {
        Require(model.SetAnimation(neondepth::kMotionNames[motion], true), "Registered Depth clip not selectable");
        model.SetAnimationLoop(false); model.SetAnimationPlaying(false);
        model.SeekCurrentAnimation(motion == 0 ? .375f : .55f); model.Update(0);
        const auto sampled = Pose(model);
        const auto sampledTime = model.GetCurrentAnimationTime();
        for (int repeat = 0; repeat < 3; ++repeat) { model.Update(0); RequirePose(model, sampled);
            Require(model.GetCurrentAnimationTime() == sampledTime, "Depth same-time update advanced clip"); }
        size_t changed = 0;
        for (size_t i = 0; i < restVertices.size(); ++i) if (!Near(restVertices[i], SkinnedPosition(model, i))) ++changed;
        Require(changed > 100, "Depth clip failed to deform actual weighted vertices");
        for (const auto& joint : model.GetSkeleton().joints) {
            Require(Near(joint.transform.translate, joint.bindTransform.translate) && Near(joint.transform.scale, joint.bindTransform.scale),
                "Depth clip changed bind translation/scale");
            Require(Near(DotQuaternion(joint.transform.rotate, joint.transform.rotate), 1), "Depth sampled quaternion not unit");
            Require(neondepth::Finite(joint.skeletonSpaceMatrix), "Depth sampled joint matrix not finite");
        }
        for (const auto& entry : model.GetSkinCluster().GetPalette())
            Require(neondepth::Finite(entry.skeletonSpaceMatrix), "Depth actual palette not finite");
    }
    model.SetAnimation("Depth_Hover"); model.SetAnimationPlaying(true); model.SetAnimationLoop(true);
    model.SetAnimationPlaybackSpeed(1); model.Update(.2f);
    neondepth::MotionGate gate; neondepth::Snapshot snapshot;
    snapshot.generation = 7; snapshot.plan.instance = 1; snapshot.phase = neondepth::Phase::Telegraph;
    snapshot.plan.attack = neondepth::Attack::Volley; snapshot.duration = .85f;
    auto command = gate.Step(snapshot);
    Require(command.select && model.TransitionToAnimation("Depth_VolleyCharge", .12f), "Depth entry blend failed");
    model.Update(.05f); model.SetAnimationPlaying(false);
    const auto frozen = Pose(model); const auto frozenTime = model.GetCurrentAnimationTime();
    const auto checkpoint = model.CaptureAnimationPlaybackState();
    Require(checkpoint.transitionActive && checkpoint.paused, "Depth did not pause inside a real blend");
    for (int frame = 0; frame < 10; ++frame) {
        command = gate.Step(snapshot);
        Require(!command.select && !command.phaseChanged, "Depth same-phase command restarted blend");
        model.Update(.1f); RequirePose(model, frozen);
        Require(model.GetCurrentAnimationTime() == frozenTime, "Depth pause advanced actual blend/time");
    }
    model.SetAnimationPlaying(true); model.Update(.1f);
    Require(!model.CaptureAnimationPlaybackState().transitionActive, "Depth resumed blend never completed");
    ++snapshot.plan.instance; command = gate.Step(snapshot);
    Require(command.select && command.restartSameClip && model.SetAnimation("Depth_VolleyCharge", true) &&
        model.GetCurrentAnimationTime() == 0, "New Depth same-clip instance did not restart once");
    Require(!gate.Step(snapshot).select, "New Depth instance reissued selection");
    model.SetAnimation("Depth_Defeat"); model.SetAnimationPlaying(false); model.SeekCurrentAnimation(1); model.Update(0);
    const auto defeat = Pose(model); TestSkinnedBounds(model); model.Update(0); RequirePose(model, defeat);
    Vector3 socket;
    Require(neondepth::TrySocketWorld(model.GetSkeleton(), MakeIdentity4x4(), neondepth::Socket::Chest, socket) &&
        neondepth::Finite(socket), "Actual frozen Depth socket invalid");
    auto renamed = model.GetSkeleton(); renamed.jointMap.clear();
    for (auto& joint : renamed.joints) { joint.name = "Renamed_" + joint.name; renamed.jointMap.emplace(joint.name, joint.index); }
    auto fallback = neondepth::CreateDepthAnimations(renamed);
    Require(fallback.safeToRegister && fallback.rootOnlyFallback, "Actual renamed valid bind did not fall back");
    for (auto& clip : fallback.animations) { Require(clip.nodeAnimations.empty(), "All-name fallback retained motion keys");
        clip.name = "Fallback_" + clip.name; }
    Require(model.RegisterAnimations(std::move(fallback.animations), &error), "Valid empty Depth clips could not register");
    model.SetAnimation("Fallback_Depth_Hover"); model.SeekCurrentAnimation(1); model.Update(0); RequirePose(model, originalBind);
    renamed.joints.back().bindTransform.rotate.x = std::numeric_limits<float>::quiet_NaN();
    const auto rejected = neondepth::CreateDepthAnimations(renamed);
    Require(!rejected.safeToRegister && rejected.animations.empty() && !rejected.bindError.empty(), "Corrupt unanimated bind claimed safe fallback");
}
}

namespace cg2 {
ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t bytes) {
    D3D12_HEAP_PROPERTIES heap{}; heap.Type=D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC desc{}; desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER; desc.Width=bytes;
    desc.Height=1; desc.DepthOrArraySize=1; desc.MipLevels=1; desc.SampleDesc.Count=1; desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> resource;
    Check(device_->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&resource)));
    return resource;
}
uint32_t SrvManager::Allocate() {
    if (useIndex_ == 0) useIndex_ = 1; // Match the production reserved-zero slot.
    ++allocatedCount_;
    return useIndex_++;
}
void SrvManager::Free(uint32_t index) {
    Require(index == 1 && allocatedCount_ == 1, "Palette descriptor was invalid or returned twice");
    --allocatedCount_; ++testDescriptorFrees;
}
void SrvManager::SetGraphicsRootDescriptorTable(UINT, uint32_t) { throw std::runtime_error("Rendering is outside the numeric test"); }
void SrvManager::CreateSRVforStructuredBuffer(uint32_t index,ID3D12Resource* resource,UINT count,UINT stride) {
    D3D12_SHADER_RESOURCE_VIEW_DESC desc{}; desc.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;
    desc.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Buffer.NumElements=count; desc.Buffer.StructureByteStride=stride;
    auto handle=testHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr+=static_cast<SIZE_T>(index)*testDx->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    testDx->GetDevice()->CreateShaderResourceView(resource,&desc,handle);
}
TextureManager* TextureManager::GetInstance() { static TextureManager instance; return &instance; }
void TextureManager::CreateFlatNormalTexture() {}
void TextureManager::CreateBrdfLutTexture() {}
void TextureManager::CreatePbrIrradianceTexture() {}
void TextureManager::CreatePbrPrefilteredEnvironmentTexture() {}
const std::string& TextureManager::GetFlatNormalTexturePath() { static const std::string path="test-flat-normal"; return path; }
const std::string& TextureManager::GetBrdfLutTexturePath() { return GetFlatNormalTexturePath(); }
const std::string& TextureManager::GetPbrIrradianceTexturePath() { return GetFlatNormalTexturePath(); }
const std::string& TextureManager::GetPbrPrefilteredEnvironmentTexturePath() { return GetFlatNormalTexturePath(); }
D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string&,TextureColorSpace) { throw std::runtime_error("Rendering is outside the numeric test"); }
void TextureManager::LoadTexture(const std::string&,TextureColorSpace) {}
bool TextureManager::LoadTextureFromMemory(const std::string&,const void*,size_t,TextureColorSpace) { return true; }
uint32_t TextureManager::GetTextureIndexByFilePath(const std::string&,TextureColorSpace) { return 0; }
std::string TextureManager::CreatePackedPbrMaterialTexture(const std::string&,float,const std::string&,float,const std::string&,float,float,float,float) { return "test-packed"; }
}

int main() {
    try {
        static_assert(std::is_same_v<decltype(std::declval<SkinnedModel&>().GetSkeleton()),const Skeleton&>);
        static_assert(std::is_same_v<decltype(std::declval<SkinnedModel&>().GetAnimationPlayer()),const AnimationPlayer&>);
        DirectXCommon dx; testDx=&dx;
        ComPtr<IDXGIFactory4> factory; ComPtr<IDXGIAdapter> warp;
        Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))); Check(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
        Check(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&dx.GetDevice())));
        D3D12_DESCRIPTOR_HEAP_DESC heap{}; heap.NumDescriptors=8; heap.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        Check(dx.GetDevice()->CreateDescriptorHeap(&heap,IID_PPV_ARGS(&testHeap)));
        SrvManager srv; SkinnedModel model;
        model.Initialize(&dx,&srv,"resources/models/neon_hologram/AvatarSample_B.glb");
        Require(model.GetAnimation().name=="BindPose" && model.GetCurrentAnimationDuration()==0,"Original GLB fallback changed");
        auto clips=neonpreview::CreateAnimations(model.GetSkeleton());
        Require(clips.size()==2 && clips[0].name=="Preview_Idle" && Near(clips[0].duration,3.0f) &&
            clips[1].name=="Preview_Attack" && Near(clips[1].duration,1.8f),"Wrong preview clips");
        // 非単位bindでも差分の合成順を行列で検証する（実モデルの腕のbindは単位回転）。
        auto rotatedBind=model.GetSkeleton();
        auto& armBind=rotatedBind.joints.at(rotatedBind.jointMap.at("J_Bip_L_UpperArm")).bindTransform.rotate;
        armBind=MakeRotateAxisAngleQuaternion({0,1,0},.4f);
        const auto composed=neonpreview::CreateAnimations(rotatedBind)[0].nodeAnimations.at("J_Bip_L_UpperArm").rotate.keyframes.front().value;
        const auto actual=MakeRotateMatrix(composed);
        const auto expected=Multiply(MakeRotateZMatrix(-1.12f),MakeRotateMatrix(armBind));
        for(size_t row=0;row<4;++row) for(size_t col=0;col<4;++col)
            Require(Near(actual.m[row][col],expected.m[row][col]),"Local delta/bind quaternion composition reversed");
        for(const auto& clip:clips) for(const auto& [name,node]:clip.nodeAnimations) {
            Require(model.GetSkeleton().jointMap.contains(name),"Clip references missing joint");
            Require(node.translate.keyframes.empty()&&node.scale.keyframes.empty(),"Preview overrides bind translation/scale");
            float previous=-1;
            const Quaternion* previousRotation=nullptr;
            for(const auto& key:node.rotate.keyframes) {
                Require(std::isfinite(key.time)&&key.time>previous&&key.time<=clip.duration,"Invalid key time");
                Require(Near(DotQuaternion(key.value,key.value),1.0f),"Quaternion not normalized"); previous=key.time;
                Require(!previousRotation||DotQuaternion(*previousRotation,key.value)>=0,"Quaternion signs are discontinuous");
                previousRotation=&key.value;
            }
            Require(Near(node.rotate.keyframes.front().value,node.rotate.keyframes.back().value),"Clip does not return to idle");
        }
        auto missing=model.GetSkeleton(); missing.jointMap.erase("J_Bip_R_LowerArm");
        bool rejected=false; try { (void)neonpreview::CreateAnimations(missing); } catch(const std::runtime_error& e) {
            rejected=std::string(e.what()).find("J_Bip_R_LowerArm")!=std::string::npos;
        }
        Require(rejected,"Missing required bone not reported");
        std::string error; Require(model.RegisterAnimations(clips,&error),error.c_str());
        Require(model.GetAnimations().size()==3,"Generated/source clips not distinct");
        Animation validBatchEntry; validBatchEntry.name="MustNotBePartiallyAdded";
        Require(!model.RegisterAnimations({validBatchEntry,clips[0]},&error)&&model.GetAnimations().size()==3,"Invalid batch partially registered");
        for(int bad=0;bad<10;++bad) {
            auto invalid=clips[0]; invalid.name="Rejected";
            auto& curve=invalid.nodeAnimations.begin()->second.rotate.keyframes;
            if(bad==0) invalid.name="Preview_Idle";
            if(bad==1) invalid.name="";
            if(bad==2) invalid.duration=std::numeric_limits<float>::quiet_NaN();
            if(bad==3) curve[1].time=curve[0].time;
            if(bad==4) curve[1].time=-1;
            if(bad==5) curve[1].time=4;
            if(bad==6) curve[1].value={0,0,0,0};
            if(bad==7) curve[1].value.x=std::numeric_limits<float>::infinity();
            if(bad==8) invalid.nodeAnimations["MissingBone"]=invalid.nodeAnimations.begin()->second;
            if(bad==9) invalid.nodeAnimations.begin()->second.translate.keyframes.push_back({0,{std::numeric_limits<float>::quiet_NaN(),0,0}});
            const auto* pointer=model.GetAnimationPlayer().GetAnimation();
            Require(!model.RegisterAnimations({invalid},&error)&&!error.empty(),"Invalid registration accepted");
            Require(pointer==model.GetAnimationPlayer().GetAnimation()&&model.GetAnimations().size()==3,"Registration failure mutated state");
        }
        Require(neonpreview::SelectAnimation(model,neonpreview::Clip::Idle),"Idle selection failed");
        model.Update(.4f); const auto time=model.GetCurrentAnimationTime();
        Require(model.SetAnimation("Preview_Idle",false)&&Near(time,model.GetCurrentAnimationTime()),"restart=false reset current clip");
        Require(model.SetAnimation("Preview_Idle",true)&&model.GetCurrentAnimationTime()==0,"Same-clip restart ignored");
        model.Update(.7f);
        Animation extra; extra.name="Extra_BindPose";
        Require(model.RegisterAnimations({extra},&error),"Append registration failed");
        Require(Near(model.GetCurrentAnimationTime(),.7f)&&model.GetAnimationPlayer().GetAnimation()==&model.GetAnimations()[1],"Player not safely rebound after vector growth");

        model.SetAnimation("BindPose"); model.Update(0);
        TestSkinnedBounds(model);
        const auto bindPose=Pose(model);
        const float bindLeftHandY=JointByName(model,"J_Bip_L_Hand").skeletonSpaceMatrix.m[3][1];
        const float bindRightHandY=JointByName(model,"J_Bip_R_Hand").skeletonSpaceMatrix.m[3][1];
        model.TransitionToAnimation("Preview_Idle",.2f); model.Update(.07f);
        model.SetAnimationPlaying(false); const auto frozen=Pose(model); const auto frozenTime=model.GetCurrentAnimationTime();
        Animation pausedAppend; pausedAppend.name="AddedDuringPausedBlend";
        Require(model.RegisterAnimations({pausedAppend},&error)&&model.IsAnimationPaused()&&
            !model.IsCurrentAnimationPlaying()&&Near(model.GetCurrentAnimationTime(),frozenTime),"Append lost paused transition state");
        model.Update(.5f); RequirePose(model,frozen);
        Require(Near(frozenTime,model.GetCurrentAnimationTime()),"Pause advanced time");
        const auto checkpoint=model.CaptureAnimationPlaybackState();
        Require(checkpoint.transitionActive && checkpoint.paused,"Checkpoint did not preserve paused blend");
        model.SetAnimation("Preview_Attack"); model.SetAnimationPlaying(true); model.Update(.4f);
        Require(model.RestoreAnimationPlaybackState(checkpoint),"Checkpoint restore failed");
        model.Update(0); RequirePose(model,frozen);
        Require(model.IsAnimationPaused() && Near(model.GetCurrentAnimationTime(),frozenTime),"Checkpoint changed pause/time");
        for (int invalid=0;invalid<6;++invalid) {
            auto bad=checkpoint;
            if(invalid==0) bad.animationIndex=model.GetAnimations().size();
            if(invalid==1) bad.time=std::numeric_limits<float>::quiet_NaN();
            if(invalid==2) bad.speed=std::numeric_limits<float>::infinity();
            if(invalid==3) bad.transitionStartPose.clear();
            if(invalid==4) bad.transitionStartPose.front().rotate={0,0,0,0};
            if(invalid==5) bad.transitionElapsed=bad.transitionDuration+1;
            Require(!model.RestoreAnimationPlaybackState(bad),"Invalid checkpoint accepted");
            model.Update(0); RequirePose(model,frozen);
            Require(Near(model.GetCurrentAnimationTime(),frozenTime),"Failed checkpoint mutated state");
        }
        model.SetAnimationPlaying(true); model.Update(.13f);
        Require(!Near(JointByName(model,"J_Bip_R_UpperArm").transform.rotate,bindPose[JointByName(model,"J_Bip_R_UpperArm").index].rotate),"Resume did not move arm");
        model.SetAnimationPlaybackSpeed(2); model.Update(.1f); Require(Near(model.GetCurrentAnimationTime(),.4f),"Playback speed ignored");
        model.SetAnimationPlaybackSpeed(1);
        model.TransitionToAnimation("Preview_Attack",.5f); model.Update(.05f); model.SetAnimationPlaying(false);
        model.SeekCurrentAnimation(.82f); model.Update(0);
        for(const auto& joint:model.GetSkeleton().joints) Require(Near(joint.transform,model.GetAnimationPlayer().SampleNode(joint.name,joint.bindTransform)),"Seek retained transition instead of exact pose");
        const auto attackZ=JointByName(model,"J_Bip_R_Hand").skeletonSpaceMatrix.m[3][2];
        model.SetAnimation(model.GetCurrentAnimationIndex(),true); model.Update(.3f);
        Require(model.IsAnimationPaused()&&model.GetCurrentAnimationTime()==0,"Paused restart resumed playback");
        model.SetAnimation("Preview_Idle"); model.SeekCurrentAnimation(0); model.Update(0);
        TestSkinnedBounds(model);
        const auto idle0=Pose(model);
        const auto idleZ=JointByName(model,"J_Bip_R_Hand").skeletonSpaceMatrix.m[3][2];
        Require(attackZ<idleZ-.15f,"Attack does not push forward in engine coordinates");
        Require(JointByName(model,"J_Bip_L_Hand").skeletonSpaceMatrix.m[3][1]<bindLeftHandY-.2f &&
            JointByName(model,"J_Bip_R_Hand").skeletonSpaceMatrix.m[3][1]<bindRightHandY-.2f,"Idle does not lower both arms");
        model.SeekCurrentAnimation(3); model.Update(0); RequirePose(model,idle0);
        model.SeekCurrentAnimation(.75f); model.Update(0);
        Require(!Near(JointByName(model,"J_Bip_C_Neck").transform.rotate,idle0[JointByName(model,"J_Bip_C_Neck").index].rotate),"Idle is static");
        for(const float sampleTime : {.25f,1.25f,2.25f}) {
            model.SeekCurrentAnimation(sampleTime); model.Update(0);
            Require(!Near(JointByName(model,"J_Bip_C_Neck").transform.rotate,idle0[JointByName(model,"J_Bip_C_Neck").index].rotate),"Intermediate Idle pose is static");
        }
        for(const auto& joint:model.GetSkeleton().joints) {
            Require(Near(joint.transform.translate,joint.bindTransform.translate)&&Near(joint.transform.scale,joint.bindTransform.scale),"Bind translation/scale changed");
            if(joint.name.find("Leg")!=std::string::npos||joint.name.find("Foot")!=std::string::npos||joint.name=="J_Bip_C_Hips")
                Require(Near(joint.transform,joint.bindTransform),"Root/legs moved");
        }
        std::vector<Vector3> idleVertices;
        for(size_t i=0;i<model.GetAsset().modelData.vertices.size();++i) idleVertices.push_back(SkinnedPosition(model,i));
        neonpreview::SelectAnimation(model,neonpreview::Clip::Attack); model.SeekCurrentAnimation(.82f); model.Update(0);
        size_t changedVertices=0;
        for(size_t i=0;i<idleVertices.size();++i) if(!Near(idleVertices[i],SkinnedPosition(model,i))) ++changedVertices;
        Require(changedVertices>1000,"Palette does not deform actual weighted geometry");
        const auto comparison=Pose(model); const auto comparisonTime=model.GetCurrentAnimationTime();
        // 描画モードの切替はSelectAnimationを呼ばない。Update(0)も同じPaletteを保持する。
        model.Update(0); RequirePose(model,comparison); Require(Near(model.GetCurrentAnimationTime(),comparisonTime),"Paused comparison lost time");
        bool invalidSeekRejected=false;
        try { model.SeekCurrentAnimation(std::numeric_limits<float>::quiet_NaN()); }
        catch(const std::invalid_argument&) { invalidSeekRejected=true; }
        Require(invalidSeekRejected&&Near(model.GetCurrentAnimationTime(),comparisonTime),"Invalid Seek mutated playback");
        for(const float sampleTime : {.15f,.4f,.82f,1.35f}) {
            model.SeekCurrentAnimation(sampleTime); model.Update(0);
            Require(!Near(JointByName(model,"J_Bip_R_UpperArm").transform.rotate,
                idle0[JointByName(model,"J_Bip_R_UpperArm").index].rotate),"Intermediate Attack pose is static");
            for(const auto& joint:model.GetSkeleton().joints)
                Require(Near(joint.transform.translate,joint.bindTransform.translate)&&Near(joint.transform.scale,joint.bindTransform.scale),"Attack changed bind translation/scale");
        }
        model.SetAnimationPlaying(true);
        for(int repeat=0;repeat<2;++repeat) {
            neonpreview::SelectAnimation(model,neonpreview::Clip::Attack); neonpreview::UpdateAnimation(model,.3f);
            neonpreview::SelectAnimation(model,neonpreview::Clip::Attack); Require(model.GetCurrentAnimationTime()==0,"Repeated Attack did not restart");
            for(int i=0;i<120;++i) neonpreview::UpdateAnimation(model,1.0f/60);
            Require(model.GetAnimation().name=="Preview_Idle"&&model.IsCurrentAnimationLooping()&&model.IsCurrentAnimationPlaying(),"Attack failed to return to looping Idle");
        }
        model.SetAnimationPlaying(false);
        neonpreview::SelectAnimation(model,neonpreview::Clip::BindPose); model.Update(0); RequirePose(model,bindPose);
        Require(model.IsAnimationPaused(),"BindPose comparison resumed playback");
        model.SeekCurrentAnimation(0); model.Update(0);
        Require(model.GetCurrentAnimationDuration()==0&&model.GetCurrentAnimationTime()==0,"Zero-duration BindPose seek failed");
        TestDissolveController(model);
        TestDepthAnimations(model);
        Require(srv.GetAllocatedCount() == 1 && testDescriptorFrees == 0, "Model palette allocation changed during animation/dissolve updates");
        model.ReleaseGpuResources();
        Require(srv.GetAllocatedCount() == 0 && testDescriptorFrees == 1, "Model release did not return its palette descriptor");
        model.ReleaseGpuResources();
        Require(srv.GetAllocatedCount() == 0 && testDescriptorFrees == 1, "Repeated model release returned a descriptor twice");
        std::cout << "PASS: one palette descriptor throughout updates; explicit GPU resource release returns it once and is idempotent.\n";
        std::cout<<"PASS: actual AvatarSample_B; 2 generated clips; validated names/keys/quaternions/joints; failed registration atomic; safe player lifetime.\n"
            <<"PASS: Idle loop/translation/scale/feet; forward Attack; "<<changedVertices<<" weighted vertices deformed by actual Palette.\n"
            <<"PASS: existing Animation/Skeleton sampling/blend, Pause/Resume/speed/Restart/Seek, repeated Attack and Idle return; const getters preserved.\n";
        std::cout<<"PASS: Showcase checkpoint restores paused in-flight blend/time/clip; invalid snapshots are atomic.\n";
        std::cout<<"PASS: Depth actual GLB clips/palette/sole/socket; same-phase nonrestart, new-instance restart, pause-blend, renamed valid-bind and corrupt-bind distinction.\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
