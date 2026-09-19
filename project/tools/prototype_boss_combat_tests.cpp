#include "../game/enemy/actor/PrototypeBossCombat.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

using Combat = PrototypeBossCombat;

static void BeginWarning(Combat& combat, float angle, float hpRatio, int pressure = 0, bool foraging = false)
{
	for (int i = 0; i < 200 && combat.GetPhase() != Combat::Phase::Telegraph; ++i) {
		const auto shot = combat.Step(0.02f, true, angle, hpRatio, pressure, foraging);
		assert(!shot.fire);
	}
	assert(combat.GetPhase() == Combat::Phase::Telegraph);
	assert(combat.GetProgress() == 0.0f);
}

static Combat::Shot WaitForShot(Combat& combat, float changedTargetAngle = 2.5f)
{
	for (int i = 0; i < 100; ++i) {
		auto shot = combat.Step(0.02f, true, changedTargetAngle, 0.1f, 99, false);
		if (shot.fire) return shot;
	}
	assert(false && "warning never fired");
	return {};
}

int main()
{
	{
		Combat combat;
		for (int i = 0; i < 300; ++i) {
			assert(!combat.Step(0.02f, false, 0.0f, 1.0f, 0, false).fire);
			assert(combat.GetPhase() == Combat::Phase::Recovery);
		}
		BeginWarning(combat, 0.25f, 1.0f);
		for (int i = 0; i < 40; ++i) {
			assert(!combat.Step(0.02f, true, -2.0f, 0.1f, 4, false).fire);
			assert(std::abs(combat.GetAimAngle() - 0.25f) < 0.0001f);
		}
		const auto shot = WaitForShot(combat);
		assert(shot.type == Combat::AttackType::AimedSpread);
		assert(std::abs(shot.angleRadians - 0.25f) < 0.0001f);
		assert(shot.pressure == 0); // Pressure changes apply to the next warning.
		assert(combat.GetPhase() == Combat::Phase::Recovery);
		for (int i = 0; i < 50; ++i) assert(!combat.Step(0.02f, true, 0, 0.2f, 4, false).fire);
		assert(combat.GetPhase() == Combat::Phase::Recovery);
	}
	{
		Combat combat;
		BeginWarning(combat, 0, 0.3f, 2);
		WaitForShot(combat);
		BeginWarning(combat, 0.5f, 0.3f, 2);
		assert(combat.GetAttackType() == Combat::AttackType::GapRing);
		assert(combat.GetSpreadAngleDeg() == 70.0f);
		const auto ring = WaitForShot(combat);
		assert(std::abs(ring.angleRadians - 0.5f) < 0.0001f);
		for (int i = 0; i < Combat::GetProjectileCount(ring.type); ++i) {
			const float offset = Combat::GetProjectileOffsetDeg(ring.type, i);
			assert(offset >= 35.0f && offset <= 325.0f); // No bullet inside the safe opening.
		}
		BeginWarning(combat, 0.75f, 0.3f, 2);
		assert(combat.GetAttackType() == Combat::AttackType::Sweep);
		std::vector<float> sweepAngles;
		for (int i = 0; i < 150; ++i) {
			const auto shot = combat.Step(0.02f, true, -2.5f, 0.3f, 2, false);
			if (shot.fire) sweepAngles.push_back(shot.angleRadians);
			if (combat.GetPhase() == Combat::Phase::Recovery) break;
		}
		assert(sweepAngles.size() == 7);
		assert(std::abs(sweepAngles.front() - (0.75f - 3.1415926535f / 4)) < 0.0001f);
		assert(std::abs(sweepAngles.back() - (0.75f + 3.1415926535f / 4)) < 0.0001f);
		for (size_t i = 1; i < sweepAngles.size(); ++i) assert(sweepAngles[i] > sweepAngles[i - 1]);
	}
	{
		Combat combat;
		BeginWarning(combat, 1.0f, 0.1f, 100, true);
		assert(!combat.Step(100.0f, true, 2.0f, 0.1f, 100, true).fire);
		assert(combat.GetPhase() == Combat::Phase::Telegraph);
		const float progress = combat.GetProgress();
		combat.Step(-1.0f, true, 0, 0.1f, 0, true);
		combat.Step(std::numeric_limits<float>::quiet_NaN(), true, 0, 0.1f, 0, true);
		assert(combat.GetProgress() == progress);
		assert(WaitForShot(combat).pressure == 4);
		BeginWarning(combat, 1.0f, 0.1f, 4, true);
		assert(combat.GetAttackType() == Combat::AttackType::AimedSpread); // Forage stays directed at food.
		combat.Reset();
		assert(combat.GetPhase() == Combat::Phase::Recovery);
		assert(!combat.HoldsPosition());
	}
	std::cout << "Prototype boss timing, locked aim, safe gap, sweep, forage, pressure and hitch tests passed.\n";
}
