#pragma once
#include <KamataEngine.h>
#include <functional>

class TimeCall {

public:
	// コンストラクタ
	TimeCall(std::function<void(void)> func, uint32_t time);

	// 更新
	void Update();

	// 完了ならtrueを返す
	bool IsFinished() { return isFinished_; }

private:
	// コールバック
	std::function<void(void)> func_;
	// 残り時間
	uint32_t time_;
	// 完了フラグ
	bool isFinished_ = false;
};
