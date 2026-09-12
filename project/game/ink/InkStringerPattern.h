#pragma once
#include "StringerWeaponParams.h"

namespace ink {
// Immutable release snapshot. Store these resolved values on each arrow; the
// player's subsequent charge, weapon switch or ImGui edits must not change it.
struct StringerChargeProfile {
    float chargeTime = 0;
    float normalizedCharge = 0;
    float firstLevelProgress = 0;
    float secondLevelProgress = 0;
    int level = 0; // 0 tap/partial, 1 first ring, 2 full
    float inkConsume = 0;
    float damage = 0;
    float projectileSpeed = 0;
    float spreadDegrees = 0;
    float paintRadius = 0;
    float freezeTime = 0;
    bool explosive = false;
};

// Piecewise linear interpolation between verified source endpoints is a CG2
// approximation; the native easing/rounding code has not been recovered.
inline StringerChargeProfile EvaluateStringerCharge(const StringerWeaponParams& p,
                                                   float chargeSeconds) {
    const float minimum = (std::max)(0.0f, p.minChargeTime);
    const float middle = (std::max)(minimum + 0.00001f, p.midChargeTime);
    const float full = (std::max)(middle + 0.00001f, p.fullChargeTime);
    const float elapsed = std::isfinite(chargeSeconds)
        ? std::clamp(chargeSeconds, 0.0f, full) : 0.0f;
    const float first = std::clamp((elapsed - minimum) / (middle - minimum), 0.0f, 1.0f);
    const float second = std::clamp((elapsed - middle) / (full - middle), 0.0f, 1.0f);
    auto blend = [first, second, elapsed, middle](float low, float mid, float high) {
        return elapsed < middle ? low + (mid - low) * first : mid + (high - mid) * second;
    };
    StringerChargeProfile result;
    result.chargeTime = elapsed;
    result.normalizedCharge = elapsed / full;
    result.firstLevelProgress = std::clamp(elapsed / middle, 0.0f, 1.0f);
    result.secondLevelProgress = second;
    result.level = elapsed + 0.000001f >= full ? 2 : (elapsed + 0.000001f >= middle ? 1 : 0);
    result.inkConsume = blend(p.minInkConsume, p.midInkConsume, p.fullInkConsume);
    result.damage = blend(p.minDamage, p.midDamage, p.fullDamage);
    result.projectileSpeed = blend(p.minProjectileSpeed, p.midProjectileSpeed, p.fullProjectileSpeed);
    result.spreadDegrees = blend(p.minSpreadDegrees, p.midSpreadDegrees, p.fullSpreadDegrees);
    result.paintRadius = blend(p.minPaintRadius, p.midPaintRadius, p.fullPaintRadius);
    result.freezeTime = blend(p.minFreezeTime, p.midFreezeTime, p.fullFreezeTime);
    result.explosive = result.level == 2 || (result.level >= 1 && p.explosiveAtMidCharge);
    return result;
}
} // namespace ink
