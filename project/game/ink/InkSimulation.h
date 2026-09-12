#pragma once
#include "InkTypes.h"
#include "ShooterWeaponParams.h"
#include <cstddef>
#include <vector>

namespace ink {
class Simulation {
public:
    ShooterWeaponParams weapon;
    MovementParams movement;
    Simulation();
    void Reset();
    void Step(float dt, const Controls& controls);
    const std::vector<Surface>& Surfaces() const { return surfaces_; }
    const PlayerStatus& Player() const { return player_; }
    const std::vector<Projectile>& Projectiles() const { return projectiles_; }
    const std::vector<Projectile>& Droplets() const { return droplets_; }
    std::vector<PaintStamp> TakePendingStamps();
    RayHit Raycast(Vec3 origin, Vec3 direction, float maxDistance, float radius=0) const;
    uint32_t GetInkAtWorldPosition(Vec3 position) const;
    size_t StampCount() const { return stampCount_; }
    void Paint(const PaintStamp& stamp);
    Vec3 Muzzle(const Controls& controls) const;
    float InkRecoveryDelay() const { return recoveryLock_; }
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
    void UpdateProjectiles(float dt);
    void Splash(uint32_t surface, Vec3 point, float radius, bool droplet);
    float Random();
    float GroundHeight(Vec3 point, float maxHeight, int* surface=nullptr) const;
    std::vector<Surface> surfaces_;
    std::vector<std::vector<uint8_t>> masks_;
    std::vector<PaintStamp> pendingStamps_;
    std::vector<Projectile> projectiles_, droplets_;
    PlayerStatus player_;
    size_t stampCount_=0;
    uint64_t shotsFired_=0;
    uint32_t randomState_=0x7541a349u;
    float fireCooldown_=0, recoveryLock_=0, timeSinceShot_=10;
    float accuracyBias_=0.01f, timeSinceJump_=10, wallReattachLock_=0;
    bool wasFire_=false, wasJump_=false, wasSwim_=false;
    int wallSurface_=-1;
};
} // namespace ink
