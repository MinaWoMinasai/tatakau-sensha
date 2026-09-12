#include "InkShooterScene.h"
#include <algorithm>
#include <cmath>

namespace {
Vector3 WeaponVector(ink::Vec3 value) { return {value.x,value.y,value.z}; }

// The procedural box is one unit long on local Z. These endpoints keep both
// the bow and the hands aligned without allocating additional model resources.
void WeaponSegment(Object3d& object,ink::Vec3 from,ink::Vec3 to,float thickness) {
    const auto delta=to-from;
    const float horizontal=std::hypot(delta.x,delta.z);
    object.SetTranslate(WeaponVector((from+to)*0.5f));
    object.SetScale({thickness,thickness,(std::max)(0.001f,ink::Length(delta))});
    object.SetRotate({-std::atan2(delta.y,horizontal),std::atan2(delta.x,delta.z),0});
    object.Update();
}
}

void InkShooterScene::UpdateWeaponVisuals() {
    // Embedded shots outlive the currently equipped weapon. Their appearance
    // uses only the immutable arrow snapshot and never changes the fuse timer.
    for(const auto& arrow:simulation_.EmbeddedArrows()) {
        const auto direction=ink::Normalize(arrow.direction);
        const auto normal=ink::Normalize(arrow.normal);
        const float fuse=1-std::clamp(arrow.remaining/(std::max)(0.001f,arrow.duration),0.0f,1.0f);
        const float pulse=0.5f+0.5f*std::sin(fuse*(13.0f+18.0f*fuse));
        const auto tip=arrow.position+normal*0.035f;
        // A pale cooled head and dark shaft remain distinct from the wet team
        // ink underneath. The small color pulse is shaded, never emissive.
        const Vector4 inkColor={0.72f+0.08f*pulse,0.94f+0.04f*pulse,0.98f,1};
        liquid_.QueueBlob(WeaponVector(tip-direction*0.09f),WeaponVector(direction),
            0.058f+0.005f*pulse,inkColor,2.8f);
        liquid_.QueueBlob(WeaponVector(tip-direction*0.24f),WeaponVector(direction),
            0.034f,{0.008f,0.030f,0.042f,1},2.8f,0,0,InkLiquidKind::Droplet);
        liquid_.QueueBlob(WeaponVector(tip-direction*0.32f),WeaponVector(direction),
            0.040f,{0.34f,0.76f+0.08f*pulse,0.80f,1},1.3f,0,0,InkLiquidKind::Droplet);
        liquid_.QueueRipple(WeaponVector(arrow.position+normal*0.020f),WeaponVector(normal),
            0.10f+0.045f*fuse,{0.018f,0.47f,0.30f,0.26f+0.18f*pulse},0,0,0.12f);
    }

    const auto& player=simulation_.Player();
    const bool stringer=simulation_.ActiveWeaponClass()==ink::WeaponClass::Stringer;
    const float human=std::clamp(1-player.formBlend,0.0f,1.0f);
    if(!stringer || player.state!=ink::PlayerState::Human || human<0.01f) {
        for(auto& part:bow_) {
            part->SetScale({0.001f,0.001f,0.001f});
            part->Update();
        }
        // UpdateModels restores the shooter model each frame; a Stringer must
        // hide it even during a transition to or from exposed squid form.
        if(stringer) {
            actor_[3]->SetScale({0.001f,0.001f,0.001f});
            actor_[3]->Update();
        }
        return;
    }
    actor_[3]->SetScale({0.001f,0.001f,0.001f});
    actor_[3]->Update();

    const auto profile=simulation_.ChargeProfile();
    const float charge=simulation_.IsCharging()?profile.normalizedCharge:0;
    const ink::Vec3 right={std::cos(yaw_),0,-std::sin(yaw_)};
    const ink::Vec3 forward={std::sin(yaw_)*std::cos(pitch_),-std::sin(pitch_),std::cos(yaw_)*std::cos(pitch_)};
    const auto up=ink::Normalize(ink::Cross(forward,right));
    // Fan orientation matches release: horizontal on ground, vertical in air.
    const auto span=player.grounded?right:up;
    const auto handle=ink::Normalize(ink::Cross(forward,span));
    const auto muzzle=simulation_.Muzzle(controls_);
    const auto grip=muzzle-forward*(0.15f+visualKick_*0.06f);
    const auto leftTip=grip-span*(0.57f*human)+forward*((0.13f-0.035f*charge)*human);
    const auto rightTip=grip+span*(0.57f*human)+forward*((0.13f-0.035f*charge)*human);
    const auto drawPoint=grip-forward*((0.12f+0.25f*charge)*human);

    bow_[0]->SetColor({0.09f,0.12f,0.15f,1});
    bow_[1]->SetColor({0.88f,0.46f,0.065f,1});
    bow_[2]->SetColor({0.96f,0.65f,0.13f,1});
    const Vector4 stringColor={0.12f,0.37f+0.18f*charge,0.30f+0.12f*charge,1};
    bow_[3]->SetColor(stringColor);
    bow_[4]->SetColor(stringColor);
    WeaponSegment(*bow_[0],grip-handle*(0.115f*human),grip+handle*(0.115f*human),0.11f*human);
    WeaponSegment(*bow_[1],grip-span*(0.055f*human),leftTip,0.080f*human);
    WeaponSegment(*bow_[2],grip+span*(0.055f*human),rightTip,0.080f*human);
    WeaponSegment(*bow_[3],leftTip,drawPoint,0.018f*human);
    WeaponSegment(*bow_[4],rightTip,drawPoint,0.018f*human);

    const auto shoulders=player.position+ink::Vec3{0,1.02f*human,0};
    WeaponSegment(*actor_[5],shoulders-right*(0.28f*human),grip,0.145f*human);
    WeaponSegment(*actor_[6],shoulders+right*(0.28f*human),drawPoint,0.145f*human);
    if(simulation_.IsCharging()) {
        // Small ink beads make the loading state legible. They are normal
        // shaded, alpha-blended liquid, with no additive light or bloom pass.
        const int count=std::clamp(simulation_.stringer.arrowCount,1,3);
        for(int index=0;index<count;++index) {
            const float slot=static_cast<float>(index)-static_cast<float>(count-1)*0.5f;
            const auto bead=drawPoint+span*(slot*0.082f*human)+forward*(0.075f*human);
            liquid_.QueueBlob(WeaponVector(bead),WeaponVector(forward),
                (0.023f+0.010f*profile.firstLevelProgress)*human,
                {0.006f,0.43f+0.16f*charge,0.29f+0.12f*charge,1},2.5f);
        }
    }
}
