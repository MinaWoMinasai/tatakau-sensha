#pragma once

#include <memory>

#include "Sprite.h"

struct NeonProgressBarStyle {
	cg2::Vector4 backgroundColor{ 0.025f, 0.040f, 0.075f, 0.88f };
	cg2::Vector4 delayedFillColor{ 0.30f, 0.80f, 1.00f, 0.48f };
	cg2::Vector4 fillColor{ 0.96f, 0.83f, 0.24f, 0.96f };
	cg2::Vector4 outlineColor{ 0.90f, 1.00f, 0.72f, 0.92f };
	float fillResponse = 13.0f;
	float delayedResponse = 4.0f;
	float bloomBrightness = 2.15f;
	float bloomAlpha = 0.72f;
	float outlineWidth = 1.5f;
	// 端だけを等倍率で描くため、長さによって端部が太らない。
	bool roundedEnds = false;
};

class NeonProgressBar {
public:
	void Initialize(cg2::SpriteCommon* spriteCommon);
	void SetStyle(const NeonProgressBarStyle& style);
	void SetBounds(const cg2::Vector2& position, const cg2::Vector2& size);
	void SetTarget(float normalizedValue);
	void SnapTo(float normalizedValue);
	// レベルアップ時は満タンまで補間してから新しいレベルの経験値へ移る。
	void BeginRollover(float normalizedValue);
	void Update(float deltaTime);
	void Draw();
	void DrawBloomSource();

	float GetDisplayValue() const { return displayValue_; }

private:
	void UpdateSprites();
	static cg2::Vector4 MultiplyBrightness(const cg2::Vector4& color, float brightness, float alpha);

	cg2::SpriteCommon* spriteCommon_ = nullptr;
	NeonProgressBarStyle style_{};
	cg2::Vector2 position_{};
	cg2::Vector2 size_{};
	float targetValue_ = 0.0f;
	float displayValue_ = 0.0f;
	float delayedValue_ = 0.0f;
	float rolloverTargetValue_ = 0.0f;
	bool hasValue_ = false;
	bool rolloverActive_ = false;
	bool dirty_ = true;

	std::unique_ptr<cg2::Sprite> background_;
	std::unique_ptr<cg2::Sprite> delayedFill_;
	std::unique_ptr<cg2::Sprite> fill_;
	std::unique_ptr<cg2::Sprite> outlineTop_;
	std::unique_ptr<cg2::Sprite> outlineBottom_;
	std::unique_ptr<cg2::Sprite> outlineLeft_;
	std::unique_ptr<cg2::Sprite> outlineRight_;
	std::unique_ptr<cg2::Sprite> bloomFill_;
	std::unique_ptr<cg2::Sprite> bloomOutlineTop_;
	std::unique_ptr<cg2::Sprite> bloomOutlineBottom_;
	std::unique_ptr<cg2::Sprite> bloomOutlineLeft_;
	std::unique_ptr<cg2::Sprite> bloomOutlineRight_;

	// カプセル版: 中央は矩形、左右だけをマスクから等倍率で描く。
	std::unique_ptr<cg2::Sprite> roundedBackgroundLeft_;
	std::unique_ptr<cg2::Sprite> roundedBackgroundRight_;
	std::unique_ptr<cg2::Sprite> roundedDelayedLeft_;
	std::unique_ptr<cg2::Sprite> roundedDelayedRight_;
	std::unique_ptr<cg2::Sprite> roundedFillLeft_;
	std::unique_ptr<cg2::Sprite> roundedFillRight_;
	std::unique_ptr<cg2::Sprite> roundedOutlineLeft_;
	std::unique_ptr<cg2::Sprite> roundedOutlineCenter_;
	std::unique_ptr<cg2::Sprite> roundedOutlineRight_;
	std::unique_ptr<cg2::Sprite> roundedBloomFillLeft_;
	std::unique_ptr<cg2::Sprite> roundedBloomFillRight_;
	std::unique_ptr<cg2::Sprite> roundedBloomOutlineLeft_;
	std::unique_ptr<cg2::Sprite> roundedBloomOutlineCenter_;
	std::unique_ptr<cg2::Sprite> roundedBloomOutlineRight_;
};
