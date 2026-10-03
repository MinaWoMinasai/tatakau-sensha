// 実SkinnedModel/Animation/Skeleton/SkinClusterを実GLBとWARP upload bufferで検証する。
// Texture/Descriptorサービスだけを隔離する。描画・画像decodeの成功はこの数値テストから主張しない。
#include "SkinCluster.h"
#include "Calculation.h"
#include "TextureManager.h"
#include "../game/debug/NeonPreviewAnimations.h"
#include <cmath>
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
uint32_t SrvManager::Allocate() { return useIndex_++; }
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
        std::cout<<"PASS: actual AvatarSample_B; 2 generated clips; validated names/keys/quaternions/joints; failed registration atomic; safe player lifetime.\n"
            <<"PASS: Idle loop/translation/scale/feet; forward Attack; "<<changedVertices<<" weighted vertices deformed by actual Palette.\n"
            <<"PASS: existing Animation/Skeleton sampling/blend, Pause/Resume/speed/Restart/Seek, repeated Attack and Idle return; const getters preserved.\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
