#include "SniperState.h"

void SniperState::Initialize(Enemy& enemy) { (void)enemy; }

void SniperState::Update(Enemy& enemy) {

	time_--;
	if (time_ % 15 == 0) {
		enemy.ShotgunFire(1, 75.0f, false);
	}

	if (time_ <= 0) {
		time_ = 60;
		enemy.StateChange();
	}
}
