#pragma once
#include "BaseEnemyState.h"
#include "Enemy.h"

class WaveState : public BaseEnemyState {
public:
	void Initialize(Enemy& enemy) override;

	void Update(Enemy& enemy) override;

private:
	float time_;
};
