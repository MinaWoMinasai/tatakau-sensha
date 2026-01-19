#include "WaveState.h"

void WaveState::Initialize(Enemy& enemy) { (void)enemy; }

void WaveState::Update(Enemy& enemy) {
	// 上下に動く
	time_ += 0.5f;

	if (int(time_ * 2) % 60 == 0) {
		enemy.Fire(true);
	}

	const float waveAmplitude = 0.1f;  // 振幅
	const float waveFrequency = 0.05f; // 周波数

	float t = time_;

	enemy.GetWorldTransform().translation_.y = enemy.GetWorldPosition().y + std::cos(t * waveFrequency) * waveAmplitude;

	if (enemy.GetWorldTransform().translation_.y > 15.0f) {
		enemy.GetWorldTransform().translation_.y = 15.0f;
	}

	if (time_ >= 180.0f) {
		enemy.StateChange();
	}
}
