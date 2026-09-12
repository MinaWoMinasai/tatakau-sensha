#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include "../game/ink/InkStringerPattern.h"

static bool Near(float a, float b) { return std::abs(a - b) < 0.0001f; }
int main() {
    ink::StringerWeaponParams p;
    const auto tap = ink::EvaluateStringerCharge(p, p.minChargeTime);
    const auto mid = ink::EvaluateStringerCharge(p, p.midChargeTime);
    const auto full = ink::EvaluateStringerCharge(p, p.fullChargeTime);
    assert(tap.level == 0 && !tap.explosive && Near(tap.damage * 3, 90));
    assert(mid.level == 1 && mid.explosive && Near(mid.damage * 3, 105));
    assert(full.level == 2 && full.explosive && Near(full.damage * 3, 105));
    assert(Near(tap.inkConsume, .05f) && Near(mid.inkConsume, .06f) && Near(full.inkConsume, .085f));
    assert(Near(tap.projectileSpeed, 63) && Near(mid.projectileSpeed, 63) && Near(full.projectileSpeed, 115.5f));
    assert(Near(tap.spreadDegrees, 8) && Near(mid.spreadDegrees, 8) && Near(full.spreadDegrees, 0));
    assert(!ink::EvaluateStringerCharge(p, p.midChargeTime - 1.0f / 120).explosive);
    assert(ink::EvaluateStringerCharge(p, p.fullChargeTime - 1.0f / 120).level == 1);
    assert(Near(full.firstLevelProgress, 1) && Near(full.secondLevelProgress, 1));
    assert(Near(ink::EvaluateStringerCharge(p, 0).damage, 30));
    assert(Near(ink::EvaluateStringerCharge(p, 99).chargeTime, p.fullChargeTime));
    assert(Near(ink::EvaluateStringerCharge(p, -1).chargeTime, 0));
    assert(Near(ink::EvaluateStringerCharge(p, std::numeric_limits<float>::quiet_NaN()).chargeTime, 0));
    assert(Near(ink::EvaluateStringerCharge(p, std::numeric_limits<float>::infinity()).chargeTime, 0));
    float previousCost = 0, previousSpeed = 0, previousDamage = 0, previousSpread = 8;
    for (int step = 0; step <= 1000; ++step) {
        const auto shot = ink::EvaluateStringerCharge(p, p.fullChargeTime * step / 1000);
        assert(shot.inkConsume >= previousCost && shot.projectileSpeed >= previousSpeed);
        assert(shot.damage >= previousDamage && shot.spreadDegrees <= previousSpread);
        assert(shot.inkConsume >= .05f && shot.inkConsume <= .085f + .00001f);
        previousCost = shot.inkConsume; previousSpeed = shot.projectileSpeed;
        previousDamage = shot.damage; previousSpread = shot.spreadDegrees;
    }
    // Profiles are values: subsequent live parameter edits cannot alter a release.
    p.fullDamage = 100; p.fullInkConsume = .5f;
    assert(Near(full.damage, 35) && Near(full.inkConsume, .085f));
    assert(Near(ink::EvaluateStringerCharge(p, p.fullChargeTime).damage, 100));
    p.explosiveAtMidCharge = false;
    assert(!ink::EvaluateStringerCharge(p, p.midChargeTime).explosive);
    assert(ink::EvaluateStringerCharge(p, p.fullChargeTime).explosive);
    // Reversed and equal editor endpoints must not divide by zero.
    p.minChargeTime = .5f; p.midChargeTime = 0; p.fullChargeTime = 0;
    const auto invalidOrder = ink::EvaluateStringerCharge(p, .5f);
    assert(std::isfinite(invalidOrder.projectileSpeed));
    std::cout << "Tri-Stringer charge endpoints, boundaries, monotonicity and immutable snapshots PASS\n";
}
