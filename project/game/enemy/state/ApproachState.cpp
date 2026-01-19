#include "ApproachState.h"
#include "LeaveState.h"
using namespace KamataEngine;
using namespace MathUtility;

void ApproachState::Initialize(Enemy& enemy) {
	time_ = 0.0f;
	enemy.ApproachToPlayer(startPos_, targetPos_);
}

void ApproachState::Update(Enemy& enemy) {

	Vector3 pos;
	float distance;
	Vector3 direction;
	float speed;

	switch (phase_) {
	case ApproachState::Phase::kStart:

		// 　予備動作
		time_ += 1.0f / 60.0f;
		if (time_ >= 0.3f) {
			time_ = 0.0f;
			phase_ = Phase::kAttack;
		}

		break;
	case ApproachState::Phase::kAttack:

		// 現在位置
		pos = enemy.GetWorldTransform().translation_;

		// プレイヤー方向を取得（targetPos_ はプレイヤーの位置）
		direction = targetPos_ - pos;

		// 距離を取得
		distance = Length(direction);

		// 終了判定（一定距離以下になったら終了）
		if (distance < 1.0f) {
			phase_ = Phase::kEnd;
			time_ = 0.0f;
			break;
		}

		// 正規化（方向だけにする）
		direction = Normalize(direction);

		// 固定速度
		speed = 0.5f; // 調整可能！遠くても同じ速度

		// 移動
		pos = pos + direction * speed;

		enemy.GetWorldTransform().translation_ = pos;

		//// 敵の位置を補完
		// enemy.GetWorldTransform().translation_ = {
		//     startPos_.x + (targetPos_.x - startPos_.x) * (time_ / duration_),
		//     startPos_.y + (targetPos_.y - startPos_.y) * (time_ / duration_),
		//     startPos_.z + (targetPos_.z - startPos_.z) * (time_ / duration_),
		// };

		// time_ += 1.0f / 60.0f;
		// if (time_ >= duration_) {
		//	time_ = 0.0f;
		//	phase_ = Phase::kEnd;
		// }

		break;
	case ApproachState::Phase::kEnd:
		time_ += 1.0f / 60.0f;
		if (time_ >= 0.3f) {
			time_ = 0.0f;
			enemy.StateChange();
		}

		break;
	default:
		break;
	}
}