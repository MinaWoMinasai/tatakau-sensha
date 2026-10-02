#pragma once
#include <cstdint>

// Game combat and scene data stay outside the reusable rendering engine.
enum Phase {
    kFadeIn,
    kMain,
    kFadeOut,
};

class Player;
/// @brief 発射する弾の位置・方向・速度・威力をまとめる。
struct AttackParam {
    int sourceDroneIndex = -1;
    Player* sourcePlayer = nullptr;
    bool shooterChain = false, shooterMark = false, shooterBoomerang = false, shooterKillBurst = false;
    float shooterChainPower = 1, shooterMarkPower = 1, shooterBoomerangPower = 1, shooterKillBurstPower = 1;
    float bulletVisualScale = 1, bulletTrailScale = 1;
    float bulletSpeed = 0.0f;
    int bulletCount = 1;
    float spreadAngleDeg = 0.0f;
    bool randomSpread = false;

    bool reflect = false;
    bool penetrate = false;

    float cooldown = 0.0f;

    uint32_t damage = 0;
    float bulletHp = 0.0f;
    float bulletPenetration = 0.0f;
    // Neutral Shooter projectiles must not claim a rival's shared resource.
    bool canClaimRunResource = true;
    // Expedition growth is opt-in. Existing arena shots retain unlimited
    // reflection when reflect=true and no actor piercing/impact splitting.
    int maxWallBounces = -1;
    int actorPierceCount = 0;
    int impactSplitCount = 0;
    float impactSplitDamageScale = 0.55f;
};

enum BulletOwner {
    kPlayer,
    kEnemy,
    kExpEnemyHostile
};
