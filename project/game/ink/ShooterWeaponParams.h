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
    float playerHitRadius = 0.285f * 0.5f;
    float stageHitRadius = 0.2f * 0.5f;
    float baseDamage = 36.0f;
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
    // SplitNum=8. Native phase order and fractional-count rounding are unknown.
    // CG2 stratifies eight phases and alternates a 1/2-drop emission budget.
    int paintDropletCount = 12; // allocation cap; zero disables flight drops
    float sourcePaintDropletSpacing = 9.2f * 0.5f;
    float sourcePaintDropletSpawnCount = 1.5f;
    float paintDropletSpacing = 9.2f * 0.5f;
    float firstPaintDropletDistance = 1.2f * 0.5f;
    float paintDropletRadius = 1.472f * 0.5f;
    float nearestPaintDropletRadius = 2.0608f * 0.5f;
    float impactPaintRadius = 1.93f * 0.5f;
    float distantImpactPaintRadius = 1.71f * 0.5f;
    // 11.3.0 SplashPaint/ImpactPaint source values. Width is a brush envelope;
    // opaque core fill and the lobe placement below remain CG2 approximations.
    float sourceImpactDepthScaleMin = 1.31f;
    float sourceImpactDepthScaleMax = 2.24f;
    float sourceBreakFreeDepthScaleMin = 1.12f;
    float sourceBreakFreeDepthScaleMax = 2.24f;
    float sourceDepthAngleMin = 35.0f; // shared source degree threshold, from tangent
    float sourceDepthAngleMax = 10.0f;
    float sourceImpactWidthHalfMiddle = 1.93f * 0.5f;
    float sourceImpactDistanceMiddle = 1.1f * 0.5f;
    int sourcePaintDropletSplitCount = 8;
    int sourceForceNearestAddCount = 4;
    float sourceDropletDepthMaxDropHeight = 3.0f * 0.5f;
    float sourceDropletDepthMinDropHeight = 10.0f * 0.5f;
    float impactCoreScale = 0.68f; // CG2 envelope-to-solid-core approximation
    float dropletCoreScale = 0.55f;
    float footCoreScale = 0.43f;
    float scatterRadiusScale = 0.11f;
    float impactNormalDepthScale = 1.0f; // CG2 normal-incidence roundness (not source)
    int footRescueEveryShots = 4; // CG2 dry-foot condition, not an interpretation of the source array
    float FireInterval() const { return repeatFrame / 60.0f; }
};

struct MovementParams {
    float humanSpeed = 0.096f * 60.0f * 0.5f;
    float swimSpeed = 0.192f * 60.0f * 0.5f;
    float enemyInkSpeed = 0.024f * 60.0f * 0.5f;
    float enemyInkShotSpeed = 0.012f * 60.0f * 0.5f;
    float enemyInkJumpSpeed = 0.08f * 60.0f * 0.5f;
    float drySquidSpeed = 0.9f; // CG2 exposed-form crawl, native value unverified
    // CG2 boundary inertia: only carry existing floor-swim velocity onto dry
    // ground. Native friction/timing are unverified; these are explicit tuning.
    float drySquidCarryDeceleration = 9.0f;
    float drySquidCarryMaxTime = 0.60f;
    float drySquidCarryBrakeMultiplier = 2.2f;
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
