#include "FallState.h"
#include "SniperState.h"
#include "WaveState.h"

void FallState::Initialize(Enemy& enemy) { (void)enemy; }

void FallState::Update(Enemy& enemy) {

	switch (fallPhase_) {
	case FallState::FallPhase::kStart:

		// 予備動作で震える

		enemy.IsShaking() = true;
		time_--;
		if (time_ <= 0) {
			time_ = 60;
			fallPhase_ = FallPhase::kFalling;
			enemy.IsShaking() = false;
		}

		break;
	case FallState::FallPhase::kFalling:

		fallSpeed_ += fallAcceleration_;
		enemy.GetWorldTransform().translation_.y -= fallSpeed_;
		if (enemy.GetWorldTransform().translation_.y < 2.0f) {
			enemy.GetWorldTransform().translation_.y = 2.0f;
			fallPhase_ = FallPhase::kRockfalling;
			enemy.Avalanche();
		}

		break;
	case FallState::FallPhase::kRockfalling:

		time_--;
		if (time_ <= 0) {
			time_ = 60;
			fallPhase_ = FallPhase::kEnd;
		}

		break;
	case FallState::FallPhase::kEnd:
		enemy.GetWorldTransform().translation_.y += 0.1f;
		if (enemy.GetWorldTransform().translation_.y > 5.0f) {

			enemy.GetWorldTransform().translation_.y = 5.0f;
			fallPhase_ = FallPhase::kStart;

			// enemy.SetState(std::make_unique<SniperState>());
			// enemy.Fire(true);
			enemy.StateChange();
		}
		break;
	default:
		break;
	}
}