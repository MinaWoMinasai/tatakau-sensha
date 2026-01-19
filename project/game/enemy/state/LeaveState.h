#include "BaseEnemyState.h"
#include "Enemy.h"

class LeaveState : public BaseEnemyState {
public:
	void Initialize(Enemy& enemy) override;
	void Update(Enemy& enemy) override;
};