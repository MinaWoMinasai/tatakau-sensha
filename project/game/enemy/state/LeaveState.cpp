#include "LeaveState.h"
using namespace KamataEngine;
using namespace MathUtility;

void LeaveState::Initialize(Enemy& enemy) { (void)enemy; }

void LeaveState::Update(Enemy& enemy) {
	const Vector3 leaveMove = {0.1f, 0.1f, -0.1f};
	enemy.GetWorldTransform().translation_ += leaveMove;
}