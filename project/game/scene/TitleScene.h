#pragma once
#include "Player.h"
#include "Fade.h"
#include "IScene.h"

#include <string_view>

class TitleScene : public IScene {
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

	void Initialize() override;

	void Update() override;

	void Draw() override;
	void DrawSprite() override;

	// デスフラグのgetter
	bool IsFinished() const override { return finished_; }
	void UpdateLogoChar(LogoChar& c, float deltaTime);

	std::string GetNextSceneName() const override;

private:
	bool IsSceneAvailable(std::string_view sceneName) const;
	bool StartTransitionIfAvailable(
		std::string_view sceneName,
		float fadeDuration);

	// ビュープロジェクション
	std::unique_ptr<Camera> camera;

	// 終了フラグ
	bool finished_ = false;
	std::string nextSceneName_;

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
	int menuSelection_ = 0;
	std::unique_ptr<TextLabel> modeMenuText_;
	std::unique_ptr<TextLabel> startHintText_;
};
