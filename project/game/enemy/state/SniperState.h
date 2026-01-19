#pragma once
#include "BaseEnemyState.h"
#include "Enemy.h"

class SniperState : public BaseEnemyState {
public:
	void Initialize(Enemy& enemy) override;

	void Update(Enemy& enemy) override;

private:
	uint32_t time_ = 120;
};
