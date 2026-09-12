#pragma once
#include <algorithm>
#include <cmath>

namespace ink {
// Tri-Stringer versus parameters, S3 11.3.0, Leanny commit
// 7280ff9cde8bb1c5dcef46c700c326471584d2e6 / WeaponStringerNormal.
// Seconds use 60 source frames/s; world lengths use 0.5 world/raw unit.
// This is a separate weapon definition: never borrow shooter flight/damage state.
struct StringerWeaponParams {
    const char* name = "Tri-Stringer-inspired";
    int arrowCount = 3; // Nintendo's three-arrow description; inherited raw default
    float minChargeTime = 9.0f / 60.0f;
    float midChargeTime = 30.0f / 60.0f;
    float fullChargeTime = 72.0f / 60.0f;
    float minFreezeTime = 12.0f / 60.0f;
    float midFreezeTime = 15.0f / 60.0f;
    float fullFreezeTime = 15.0f / 60.0f;
    float postShotDelay = 10.0f / 60.0f;
    float minInkConsume = 0.05f;
    float midInkConsume = 0.06f;
    float fullInkConsume = 0.085f;
    float minDamage = 30.0f;
    float midDamage = 35.0f;
    float fullDamage = 35.0f;
    float minProjectileSpeed = 2.1f * 60.0f * 0.5f;
    float midProjectileSpeed = 2.1f * 60.0f * 0.5f;
    float fullProjectileSpeed = 3.85f * 60.0f * 0.5f;
    // Source names describe angle extrema, not charge extrema: AngleMax=8,
    // AngleMid=8, inherited AngleMin=0. Full charge closes the fan.
    float minSpreadDegrees = 8.0f;
    float midSpreadDegrees = 8.0f;
    float fullSpreadDegrees = 0.0f;
    float arrowSpacing = 0.4f * 0.5f;
    float straightFlightTime = 4.0f / 60.0f;
    float brakeFlightTime = 1.0f / 60.0f;
    float brakeAirResistance = 0.1f; // velocity loss per source frame
    float brakeGravity = 0.04f * 60.0f * 60.0f * 0.5f;
    float brakeToFreeSpeedXZ = 20.0f * 60.0f * 0.5f;
    float brakeToFreeSpeedY = 10.0f * 60.0f * 0.5f;
    float freeAirResistance = 0.24f;
    float freeGravity = 0.15f * 60.0f * 60.0f * 0.5f;
    float playerHitRadius = 0.205f * 0.5f;
    float stageHitRadius = 0.2f * 0.5f; // inherited default; see research notes
    float minPaintRadius = 2.5f * 0.5f;
    float midPaintRadius = 2.5f * 0.5f;
    float fullPaintRadius = 3.0f * 0.5f;
    float paintDropletRadius = 1.39f * 0.5f;
    float nearestPaintDropletRadius = 1.683f * 0.5f;
    float sourceDropletDepthScaleMax = 2.7f;
    float sourceDropletInterval = 10.0f;
    int sourceDropletMaxCount = 5;
    int sourceDropletSplitCount = 5;
    float detonationTime = 45.0f / 60.0f;
    float explosionDamage = 30.0f;
    float explosionDamageRadius = 2.05f * 0.5f;
    float explosionPaintRadius = 2.0f * 0.5f;
    float explosionOffset = 0.6f * 0.5f;
    float moveSpeedWhileCharging = 0.068f * 60.0f * 0.5f;
    bool enableChargeKeep = false;
    bool explosiveAtMidCharge = true;
    // Inherited values and version-diff inferences, not present as these fields
    // in the weapon override. REEF-LUX AirChargeRateByInkEmpty changed 1 -> 3 in
    // 8.0.0 when Nintendo made airborne charge match grounded charge. Normal's
    // retained value 1 supports the 1/3 rate; native code is not available.
    float inkRecoverStop = 20.0f / 60.0f;
    float airborneChargeRate = 1.0f / 3.0f;
    float projectileLifetime = 3.0f; // CG2 cleanup bound, not an effective range
    float impactCoreScale = 0.68f; // CG2 brush envelope interpretation
    float dropletCoreScale = 0.55f;
};
} // namespace ink
