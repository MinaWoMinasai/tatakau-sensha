#pragma once
#include "BaseEnemyState.h"
#include "Enemy.h"

class FallState : public BaseEnemyState {
public:
	void Initialize(Enemy& enemy) override;

	void Update(Enemy& enemy) override;

private:
	enum class FallPhase {
		kStart,       // 開始
		kFalling,     // 落下中
		kRockfalling, // がれき
		kEnd,         // 終了
	};

	FallPhase fallPhase_ = FallPhase::kStart;

	uint32_t time_ = 60;
	float fallAcceleration_ = 0.01f;
	float fallSpeed_ = 0.0f;
};
