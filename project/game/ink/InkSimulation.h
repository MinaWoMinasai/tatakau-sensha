#pragma once
#include "InkTypes.h"
#include "ShooterWeaponParams.h"
#include "WeaponDefinition.h"
#include "InkStringerPattern.h"
#include <cstddef>
#include <vector>

namespace ink {
class Simulation {
public:
    ShooterWeaponParams weapon;
    StringerWeaponParams stringer;
    MovementParams movement;
    Simulation();
    void Reset();
    void Step(float dt, const Controls& controls);
    void Equip(const WeaponDefinition& definition);
    WeaponClass ActiveWeaponClass() const { return weaponClass_; }
    const std::string& ActiveWeaponId() const { return activeWeaponId_; }
    const std::string& ActiveWeaponName() const { return activeWeaponName_; }
    bool IsCharging() const { return charging_; }
    float ChargeTime() const { return chargeTime_; }
    StringerChargeProfile ChargeProfile() const { return EvaluateStringerCharge(stringer,chargeTime_); }
    const std::vector<EmbeddedArrow>& EmbeddedArrows() const { return embeddedArrows_; }
    void CancelCharge();
    const std::vector<Surface>& Surfaces() const { return surfaces_; }
    const PlayerStatus& Player() const { return player_; }
    const std::vector<Projectile>& Projectiles() const { return projectiles_; }
    const std::vector<Projectile>& Droplets() const { return droplets_; }
    std::vector<PaintStamp> TakePendingStamps();
    std::vector<ImpactEvent> TakeImpactEvents();
    std::vector<AudioEvent> TakeAudioEvents() {
        std::vector<AudioEvent> result; result.swap(audioEvents_); return result;
    }
    const DummyStatus& Dummy() const { return dummy_; }
    void SetDummyPosition(Vec3 position);
    static float CalculateDamage(float age, const ShooterWeaponParams& params);
    // Deterministic geometry helper: incidence is measured from the actual plane.
    static PaintStamp ImpactBrush(const Surface& plane, uint32_t surface, Vec3 point,
        Vec3 incomingVelocity, float radius, PaintKind kind, const ShooterWeaponParams& params);
    RayHit Raycast(Vec3 origin, Vec3 direction, float maxDistance, float radius=0) const;
    uint32_t GetInkAtWorldPosition(Vec3 position) const;
    size_t StampCount() const { return stampCount_; }
    void Paint(const PaintStamp& stamp);
    Vec3 Muzzle(const Controls& controls) const;
    float InkRecoveryDelay() const { return recoveryLock_; }
    float DrySquidCarryRemaining() const { return drySquidCarryRemaining_; }
    uint64_t ShotsFired() const { return shotsFired_; }
    static constexpr int MaskResolution = 512;
    const std::vector<uint8_t>& Mask(uint32_t surface) const { return masks_.at(surface); }
    // Useful for deterministic scenario tests and the scene's reset/debug tools.
    void SetPlayerPosition(Vec3 position);
private:
    uint32_t InkOnSurface(uint32_t surface, Vec3 position) const;
    void BuildStage();
    void MovePlayer(float dt, const Controls& controls, bool jumpPressed);
    void ResolveWalls(Vec3 previous, float radius);
    bool TryWallSwim(float dt, const Controls& controls, bool jumpPressed, Vec3 movementDirection);
    void Fire(const Controls& controls);
    void StepStringer(float dt, const Controls& controls);
    void FireStringer(const Controls& controls);
    void UpdateEmbeddedArrows(float dt);
    void DamageDummy(float damage, float distance);
    float PostShotDelay() const;
    float FiringMoveSpeed() const;
    void UpdateProjectiles(float dt);
    void Splash(uint32_t surface, Vec3 point, Vec3 incomingVelocity, float radius, PaintKind kind, int scatterCount=0,
        const ShooterWeaponParams* tuning=nullptr);
    float Random();
    float PaintRandom();
    void EmitImpact(Vec3 position, Vec3 normal, Vec3 velocity, float radius, PaintKind kind);
    void EmitAudio(AudioCue cue, Vec3 position, int level=0) {
        if(audioEvents_.size()<128) audioEvents_.push_back({cue,position,level});
    }
    float GroundHeight(Vec3 point, float maxHeight, int* surface=nullptr) const;
    std::vector<Surface> surfaces_;
    std::vector<std::vector<uint8_t>> masks_;
    std::vector<PaintStamp> pendingStamps_;
    std::vector<Projectile> projectiles_, droplets_;
    std::vector<ImpactEvent> impactEvents_;
    std::vector<AudioEvent> audioEvents_;
    std::vector<EmbeddedArrow> embeddedArrows_;
    DummyStatus dummy_;
    PlayerStatus player_;
    EmissionPattern emissionPattern_;
    EmissionPattern stringerEmission_{0x33bc1907u};
    WeaponClass weaponClass_=WeaponClass::Shooter;
    std::string activeWeaponId_="splattershot",activeWeaponName_="スプラシューター参考";
    float chargeTime_=0,chargeAge_=0;
    bool charging_=false,releaseQueued_=false,requireTriggerRelease_=false;
    size_t stampCount_=0;
    uint64_t shotsFired_=0;
    uint32_t randomState_=0x7541a349u;
    uint32_t paintRandomState_=0x2189a3c7u;
    uint64_t impactSequence_=0;
    float fireCooldown_=0, recoveryLock_=0, timeSinceShot_=10;
    float accuracyBias_=0.01f, timeSinceJump_=10, wallReattachLock_=0;
    float airMoveSpeed_=2.88f;
    float drySquidCarryRemaining_=0;
    bool wasFire_=false, wasJump_=false, wasSwim_=false;
    int wallSurface_=-1;
};
} // namespace ink
