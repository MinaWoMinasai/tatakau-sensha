#pragma once

namespace ink {
// Splattershot, S3 11.3.0: Leanny's 1130 parameter dump and Inkipedia timing.
// Spatial values below convert Nintendo units at 0.5 world units / raw unit.
// Time is seconds at 60 source frames/second. Unverified engine-specific choices
// are explicitly named as prototype tuning, rather than asserted game internals.
struct ShooterWeaponParams {
    const char* name = "Splattershot-inspired";
    float repeatFrame = 6.0f;
    float inkConsume = 0.0092f;
    float inkRecoverStop = 20.0f / 60.0f;
    float initialShotDelay = 3.0f / 60.0f;
    float swimInitialShotDelay = 12.0f / 60.0f;
    float postShotDelay = 4.0f / 60.0f;
    float projectileSpeed = 2.266f * 60.0f * 0.5f;
    float projectileGravity = 0.016f * 60.0f * 60.0f * 0.5f;
    float straightFlightTime = 4.0f / 60.0f;
    float brakeInitialSpeed = 1.493f * 60.0f * 0.5f;
    // Community-inferred inherited shooter defaults (Inkipedia Shooter_data_S3).
    float brakeAirResistance = 0.36f; // velocity loss per source frame
    float brakeGravity = 0.07f * 60.0f * 60.0f * 0.5f;
    float brakeToFreeSpeedXZ = 0.2355f * 60.0f * 0.5f;
    float brakeToFreeSpeedY = -0.15f * 60.0f * 0.5f;
    float freeAirResistance = 0.02f;
    float effectiveRange = 11.56f * 0.5f; // community empirical target-hit range, angle dependent
    float projectileLifetime = 2.0f; // prototype cleanup limit
    float playerHitRadius = 0.285f * 0.5f; // reserved for future targets
    float stageHitRadius = 0.2f * 0.5f;
    float baseDamage = 36.0f; // reserved; this prototype has no damageable targets
    float minimumDamage = 18.0f;
    float damageFalloffStart = 8.0f / 60.0f;
    float damageFalloffEnd = 40.0f / 60.0f;
    float groundSpread = 4.86f;
    float jumpSpread = 11.66f;
    float accuracyBiasMinimum = 0.01f;
    float accuracyBiasPerShot = 0.01f;
    float accuracyBiasMaximum = 0.25f;
    float jumpAccuracyBiasMaximum = 0.40f;
    float accuracyRecovery = 0.015f * 60.0f;
    float accuracyRecoveryDelay = 6.0f / 60.0f;
    float jumpAccuracyRecoveryStart = 25.0f / 60.0f;
    float jumpAccuracyRecoveryEnd = 70.0f / 60.0f;
    float moveSpeedWhileFiring = 0.072f * 60.0f * 0.5f;
    // Source SplashSpawn spacing=9.2 raw, nearest=1.2 raw, SpawnNum=1.5,
    // SplitNum=8. Their native spawn algorithm is unavailable: distribute eight
    // visible falling drops over this prototype's shortened ballistic path.
    int paintDropletCount = 8;
    float sourcePaintDropletSpacing = 9.2f * 0.5f;
    float sourcePaintDropletSpawnCount = 1.5f;
    float paintDropletSpacing = 1.15f; // prototype distribution
    float firstPaintDropletDistance = 1.2f * 0.5f;
    float paintDropletRadius = 1.472f * 0.5f;
    float nearestPaintDropletRadius = 2.0608f * 0.5f;
    float impactPaintRadius = 1.93f * 0.5f;
    float distantImpactPaintRadius = 1.71f * 0.5f;
    float FireInterval() const { return repeatFrame / 60.0f; }
};

struct MovementParams {
    float humanSpeed = 0.096f * 60.0f * 0.5f;
    float swimSpeed = 0.192f * 60.0f * 0.5f;
    float humanInkRecovery = 1.0f / 10.0f;
    float swimInkRecovery = 1.0f / 3.0f;
    // Collision, acceleration, jump and form timings below are prototype tuning.
    float acceleration = 28.0f;
    float gravity = 20.0f;
    float jumpSpeed = 7.0f;
    float wallSwimSpeed = 4.32f;
    float humanRadius = 0.28f;
    float swimRadius = 0.16f;
    float stepHeight = 0.30f;
    float formTransitionTime = 0.12f;
};
} // namespace ink
