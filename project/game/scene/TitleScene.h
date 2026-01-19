#pragma once
#include "Player.h"
#include "Fade.h"

class TitleScene {
public:
	
	struct LogoChar {
		std::unique_ptr<Sprite> sprite;

		Vector2 startPos;
		Vector2 targetPos;

		Vector2 baseSize;
		float timer;
		float delay;
		float fallSpeed;
		bool landed;
	};

	void Initialize();

	void Update();

	void Draw();
	void DrawSprite();

	// デスフラグのgetter
	bool IsFinished() const { return finished_; }
	void UpdateLogoChar(LogoChar& c, float deltaTime);
private:
	// ビュープロジェクション
	std::unique_ptr<Camera> camera;

	// 終了フラグ
	bool finished_ = false;

	// スプライト
	std::unique_ptr<Sprite> rule;


	// 点滅用タイマー
	float blinkTimer_ = 0.0f;

	//Fade
	std::unique_ptr<Fade> fade_ = nullptr;
	Phase phase_ = Phase::kFadeIn;

	// 入力
	Input* input_;

	std::vector<LogoChar> logoChars;

	const float deltaTime = 1.0f / 60.0f;

	LogoChar startLogo;
	bool startVisible = false;
	LogoChar ruleLogo;
	bool startVisibleRule = false;

	bool ruleGide = false;
};