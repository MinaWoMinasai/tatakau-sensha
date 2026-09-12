#include "InkSimulation.h"
#include <algorithm>
#include <cmath>

namespace ink {
void Simulation::Equip(const WeaponDefinition& definition) {
    CancelCharge();
    weaponClass_=definition.type;
    activeWeaponId_=definition.id; activeWeaponName_=definition.displayNameJa;
    weapon=definition.shooter; stringer=definition.stringer;
    accuracyBias_=weapon.accuracyBiasMinimum;
    // Existing projectiles, stuck arrows, ink and paint keep their state. A
    // held trigger must be released before the newly equipped weapon starts.
    requireTriggerRelease_=true;
}

void Simulation::CancelCharge() {
    if(charging_ || releaseQueued_) {
        requireTriggerRelease_=true;
        EmitAudio(AudioCue::ChargeCancel,player_.position);
    }
    charging_=false; releaseQueued_=false; chargeTime_=0; chargeAge_=0;
}

float Simulation::PostShotDelay() const {
    return weaponClass_==WeaponClass::Stringer?stringer.postShotDelay:weapon.postShotDelay;
}

float Simulation::FiringMoveSpeed() const {
    return weaponClass_==WeaponClass::Stringer?stringer.moveSpeedWhileCharging:weapon.moveSpeedWhileFiring;
}

void Simulation::StepStringer(float dt,const Controls& controls) {
    if(player_.state!=PlayerState::Human || requireTriggerRelease_) return;
    if(!charging_ && controls.fire && fireCooldown_<=0.000001f) {
        charging_=true; chargeTime_=0; chargeAge_=0; releaseQueued_=false;
        EmitAudio(AudioCue::ChargeStart,player_.position);
    }
    if(!charging_) return;
    if(!controls.fire) releaseQueued_=true;
    chargeAge_+=dt;
    // Charging is slower in the air. A quick click still queues a single tap
    // shot at the minimum action delay instead of dropping the input entirely.
    const float rate=player_.grounded?1.0f:stringer.airborneChargeRate;
    const int oldLevel=ChargeProfile().level;
    if(controls.fire || chargeAge_<stringer.minChargeTime)
        chargeTime_=std::min(stringer.fullChargeTime,chargeTime_+dt*std::max(0.0f,rate));
    const int level=ChargeProfile().level;
    if(oldLevel<1 && level>=1) EmitAudio(AudioCue::ChargeFirst,player_.position,1);
    if(oldLevel<2 && level>=2) EmitAudio(AudioCue::ChargeFull,player_.position,2);
    if(releaseQueued_ && chargeAge_+0.000001f>=stringer.minChargeTime) {
        FireStringer(controls);
        charging_=false; releaseQueued_=false; chargeTime_=0; chargeAge_=0;
    }
}

void Simulation::FireStringer(const Controls& controls) {
    const auto profile=EvaluateStringerCharge(stringer,chargeTime_);
    // Charging/canceling does not spend ink in CG2; the resolved release does.
    // Consumption timing is explicitly an approximation, unlike the cost values.
    if(player_.ink+0.000001f<profile.inkConsume) { EmitAudio(AudioCue::Empty,player_.position); return; }
    const auto muzzle=Muzzle(controls);
    auto direction=Normalize(controls.aimPoint-muzzle);
    if(Length(direction)<0.1f) direction={std::sin(controls.yaw),0,std::cos(controls.yaw)};
    auto right=Normalize(Cross({0,1,0},direction));
    if(Length(right)<0.1f) right={std::cos(controls.yaw),0,-std::sin(controls.yaw)};
    const auto up=Normalize(Cross(direction,right));
    const auto fan=player_.grounded?right:up;

    ShooterWeaponParams tuning;
    tuning.projectileSpeed=profile.projectileSpeed;
    tuning.straightFlightTime=stringer.straightFlightTime;
    tuning.brakeAirResistance=stringer.brakeAirResistance;
    tuning.brakeGravity=stringer.brakeGravity;
    tuning.projectileLifetime=stringer.projectileLifetime;
    tuning.playerHitRadius=stringer.playerHitRadius; tuning.stageHitRadius=stringer.stageHitRadius;
    tuning.impactPaintRadius=profile.paintRadius; tuning.distantImpactPaintRadius=profile.paintRadius;
    tuning.paintDropletRadius=stringer.paintDropletRadius;
    tuning.nearestPaintDropletRadius=stringer.nearestPaintDropletRadius;
    tuning.impactCoreScale=stringer.impactCoreScale; tuning.dropletCoreScale=stringer.dropletCoreScale;
    // Stringer incidence endpoints, not the shooter's weapon width settings.
    tuning.sourceImpactDepthScaleMin=1.4f; tuning.sourceImpactDepthScaleMax=2.4f;

    EmissionSettings emission;
    emission.sourceSpawnNum=static_cast<float>(stringer.sourceDropletMaxCount);
    emission.sourceSplitNum=stringer.sourceDropletSplitCount;
    emission.sourceBetweenDistance=stringer.sourceDropletInterval*0.5f;
    emission.sourceNearestDistance=1.25f*0.5f;
    emission.maxTravelDistance=60;
    // The shared planner's phase/jitter is still the documented CG2 paint
    // approximation. It does not change the three deterministic arrow rays.
    const int count=std::clamp(stringer.arrowCount,1,3);
    for(int n=0;n<count;++n) {
        const float side=static_cast<float>(n)-(count-1)*0.5f;
        const float angle=profile.spreadDegrees*side*3.14159265358979323846f/180;
        const auto arrowDirection=Normalize(direction*std::cos(angle)+fan*std::sin(angle));
        Projectile arrow;
        const auto proposed=muzzle+fan*(side*stringer.arrowSpacing);
        const auto offset=proposed-muzzle;
        const auto obstruction=Raycast(muzzle,offset,Length(offset),stringer.stageHitRadius);
        arrow.position=obstruction.hit?muzzle+Normalize(offset)*std::max(0.0f,obstruction.distance-0.002f):proposed;
        arrow.velocity=arrowDirection*profile.projectileSpeed;
        arrow.tuning=tuning; arrow.projectileKind=ProjectileKind::StringerArrow;
        arrow.directDamage=profile.damage;
        arrow.brakeDuration=stringer.brakeFlightTime; arrow.freeDrag=stringer.freeAirResistance;
        arrow.freeGravity=stringer.freeGravity;
        arrow.explosive=profile.explosive;
        arrow.explosionDelay=stringer.detonationTime; arrow.explosionDamage=stringer.explosionDamage;
        arrow.explosionRadius=stringer.explosionDamageRadius; arrow.explosionPaintRadius=stringer.explosionPaintRadius;
        arrow.explosionOffset=stringer.explosionOffset;
        arrow.emission=stringerEmission_.NextShot(emission,false);
        arrow.nextDroplet=arrow.emission.count?arrow.emission.drops[0].distance:1000;
        projectiles_.push_back(arrow);
    }
    player_.ink=std::max(0.0f,player_.ink-profile.inkConsume);
    recoveryLock_=stringer.inkRecoverStop; timeSinceShot_=0;
    fireCooldown_=std::max(fireCooldown_,profile.freezeTime);
    ++shotsFired_;
    EmitAudio(AudioCue::StringerShot,muzzle,profile.level);
}

void Simulation::DamageDummy(float damage,float distance) {
    if(dummy_.hp<=0 || damage<=0) return;
    dummy_.lastDamage=damage; dummy_.lastHitDistance=distance;
    dummy_.hp=std::max(0.0f,dummy_.hp-damage); ++dummy_.hits;
    if(dummy_.hp<=0) {
        dummy_.lastShotsToKill=dummy_.hits; ++dummy_.kills; dummy_.resetRemaining=2;
    }
}

void Simulation::UpdateEmbeddedArrows(float dt) {
    for(size_t n=0;n<embeddedArrows_.size();) {
        auto& arrow=embeddedArrows_[n]; arrow.remaining-=dt;
        if(arrow.remaining>0.000001f) { ++n; continue; }
        const Vec3 center=arrow.position+Vec3{0,arrow.offset,0};
        const Vec3 origin=center+arrow.normal*0.04f;
        const auto toTarget=dummy_.position-origin;
        const float distance=Length(toTarget);
        if(dummy_.hp>0 && distance<=arrow.radius+dummy_.radius &&
            !Raycast(origin,toTarget,std::max(0.0f,distance-0.01f)).hit)
            DamageDummy(arrow.damage,distance);
        if(arrow.surface<surfaces_.size() && surfaces_[arrow.surface].inkable) {
            // Project the burst onto the attached finite plane. Adjacent-plane
            // paint propagation is not assumed from an undocumented volume rule.
            const auto& plane=surfaces_[arrow.surface];
            const auto onPlane=center-plane.normal*Dot(center-plane.origin,plane.normal);
            auto stamp=ImpactBrush(plane,arrow.surface,onPlane,
                arrow.normal*-1,arrow.paintRadius,PaintKind::Explosion,arrow.tuning);
            Paint(stamp);
        }
        EmitImpact(center,arrow.normal,{},arrow.radius,PaintKind::Explosion);
        EmitAudio(AudioCue::ArrowBurst,center);
        embeddedArrows_[n]=embeddedArrows_.back(); embeddedArrows_.pop_back();
    }
}
} // namespace ink
