#pragma once

#include <memory>

#include "Sprite.h"

struct NeonProgressBarStyle {
	Vector4 backgroundColor{ 0.025f, 0.040f, 0.075f, 0.88f };
	Vector4 delayedFillColor{ 0.30f, 0.80f, 1.00f, 0.48f };
	Vector4 fillColor{ 0.96f, 0.83f, 0.24f, 0.96f };
	Vector4 outlineColor{ 0.90f, 1.00f, 0.72f, 0.92f };
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
	void Initialize(SpriteCommon* spriteCommon);
	void SetStyle(const NeonProgressBarStyle& style);
	void SetBounds(const Vector2& position, const Vector2& size);
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
	static Vector4 MultiplyBrightness(const Vector4& color, float brightness, float alpha);

	SpriteCommon* spriteCommon_ = nullptr;
	NeonProgressBarStyle style_{};
	Vector2 position_{};
	Vector2 size_{};
	float targetValue_ = 0.0f;
	float displayValue_ = 0.0f;
	float delayedValue_ = 0.0f;
	float rolloverTargetValue_ = 0.0f;
	bool hasValue_ = false;
	bool rolloverActive_ = false;
	bool dirty_ = true;

	std::unique_ptr<Sprite> background_;
	std::unique_ptr<Sprite> delayedFill_;
	std::unique_ptr<Sprite> fill_;
	std::unique_ptr<Sprite> outlineTop_;
	std::unique_ptr<Sprite> outlineBottom_;
	std::unique_ptr<Sprite> outlineLeft_;
	std::unique_ptr<Sprite> outlineRight_;
	std::unique_ptr<Sprite> bloomFill_;
	std::unique_ptr<Sprite> bloomOutlineTop_;
	std::unique_ptr<Sprite> bloomOutlineBottom_;
	std::unique_ptr<Sprite> bloomOutlineLeft_;
	std::unique_ptr<Sprite> bloomOutlineRight_;

	// カプセル版: 中央は矩形、左右だけをマスクから等倍率で描く。
	std::unique_ptr<Sprite> roundedBackgroundLeft_;
	std::unique_ptr<Sprite> roundedBackgroundRight_;
	std::unique_ptr<Sprite> roundedDelayedLeft_;
	std::unique_ptr<Sprite> roundedDelayedRight_;
	std::unique_ptr<Sprite> roundedFillLeft_;
	std::unique_ptr<Sprite> roundedFillRight_;
	std::unique_ptr<Sprite> roundedOutlineLeft_;
	std::unique_ptr<Sprite> roundedOutlineCenter_;
	std::unique_ptr<Sprite> roundedOutlineRight_;
	std::unique_ptr<Sprite> roundedBloomFillLeft_;
	std::unique_ptr<Sprite> roundedBloomFillRight_;
	std::unique_ptr<Sprite> roundedBloomOutlineLeft_;
	std::unique_ptr<Sprite> roundedBloomOutlineCenter_;
	std::unique_ptr<Sprite> roundedBloomOutlineRight_;
};
