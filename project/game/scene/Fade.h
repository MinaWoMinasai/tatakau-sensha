#pragma once
#include "Struct.h"
#include "Sprite.h"

class Fade {
public:
	enum class Status {
		None,    // フェードなし
		FadeIn,  // フェードイン中
		FadeOut, // フェードアウト中
	};

	void Initialize();
	void Update();
	void Draw();

	// フェード開始
	void Start(Status status, float duration);

	// フェード停止
	void Stop();

	// フェード終了判定
	bool IsFinished() const;

private:
	std::unique_ptr<Sprite> sprite;

	// 02_13 16枚目 現在のフェードの状態
	Status status_ = Status::None;

	// 02_13 17枚目 フェードの持続時間
	float duration_ = 0.0f;
	// 02_13 17枚目 経過時間カウンター
	float counter_ = 0.0f;
};
