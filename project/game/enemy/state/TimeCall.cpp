#include "TimeCall.h"
using namespace KamataEngine;

// メンバ初期化子で初期化
TimeCall::TimeCall(std::function<void(void)> func, uint32_t time) : func_(std::move(func)), time_(time) {}

void TimeCall::Update() {

	if (isFinished_) {
		return;
	}

	time_--;
	if (time_ <= 0) {

		isFinished_ = true;
		// コールバックを呼び出す
		func_();
	}
}
