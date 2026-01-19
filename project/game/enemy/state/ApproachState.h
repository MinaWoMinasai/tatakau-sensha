#pragma once
#include "BaseEnemyState.h"
#include "Enemy.h"

class ApproachState : public BaseEnemyState {
public:
	void Initialize(Enemy& enemy) override;

	void Update(Enemy& enemy) override;

private:
	enum class Phase {
		kStart,  // 前隙・予備動作
		kAttack, // 行動中
		kEnd,    // 後隙
	};

	Phase phase_ = Phase::kStart;

	float time_ = 0.0f;
	float duration_ = 1.0f;
	KamataEngine::Vector3 startPos_ = {};
	KamataEngine::Vector3 targetPos_ = {};
};
