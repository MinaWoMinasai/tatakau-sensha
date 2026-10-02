#include "game/ui/NeonProgressBar.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr const char* kSolidTexture = "resources/white512x512.png";
constexpr const char* kFillMaskTexture = "resources/hpBarFillMask.png";
constexpr const char* kOutlineMaskTexture = "resources/hpBarOutlineMask.png";
constexpr float kFillMaskHeight = 24.0f;
constexpr float kFillCapSourceWidth = 12.0f;
constexpr float kOutlineMaskHeight = 32.0f;
constexpr float kOutlineCapSourceWidth = 16.0f;

float SmoothToward(float current, float target, float response, float deltaTime)
{
	const float safeDeltaTime = (std::max)(0.0f, deltaTime);
	const float blend = 1.0f - std::exp(-(std::max)(0.0f, response) * safeDeltaTime);
	const float value = current + (target - current) * blend;
	return std::abs(value - target) < 0.0001f ? target : value;
}

void SetFillSprite(cg2::Sprite* sprite, const cg2::Vector2& position, const cg2::Vector2& size, float ratio, const cg2::Vector4& color)
{
	if (!sprite) {
		return;
	}
	const float width = (std::max)(0.0f, size.x * (std::clamp)(ratio, 0.0f, 1.0f));
	sprite->SetPosition(position);
	sprite->SetSize({ width, size.y });
	cg2::Vector4 visibleColor = color;
	if (width <= 0.01f || size.y <= 0.01f) {
		visibleColor.w = 0.0f;
	}
	sprite->SetColor(visibleColor);
	sprite->Update();
}

void SetSolidSprite(cg2::Sprite* sprite, const cg2::Vector2& position, const cg2::Vector2& size, const cg2::Vector4& color)
{
	if (!sprite) {
		return;
	}
	sprite->SetPosition(position);
	sprite->SetSize(size);
	cg2::Vector4 visibleColor = color;
	if (size.x <= 0.01f || size.y <= 0.01f) {
		visibleColor.w = 0.0f;
	}
	sprite->SetColor(visibleColor);
	sprite->Update();
}

void SetCapsuleSegments(
	cg2::Sprite* left,
	cg2::Sprite* center,
	cg2::Sprite* right,
	const cg2::Vector2& position,
	const cg2::Vector2& size,
	const cg2::Vector4& color,
	bool closeRight)
{
	const float capWidth = (std::min)(size.x * 0.5f, size.y * 0.5f);
	const float rightCapWidth = closeRight ? capWidth : 0.0f;
	SetSolidSprite(left, position, { capWidth, size.y }, color);
	SetSolidSprite(
		center,
		{ position.x + capWidth, position.y },
		{ (std::max)(0.0f, size.x - capWidth - rightCapWidth), size.y },
		color);
	SetSolidSprite(
		right,
		{ position.x + (std::max)(0.0f, size.x - capWidth), position.y },
		{ rightCapWidth, size.y },
		color);
}

} // namespace

void NeonProgressBar::Initialize(cg2::SpriteCommon* spriteCommon)
{
	spriteCommon_ = spriteCommon;
	if (!spriteCommon_) {
		return;
	}
	auto makeSprite = [this](const char* texturePath) {
		auto sprite = std::make_unique<cg2::Sprite>();
		sprite->Initialize(spriteCommon_, texturePath);
		return sprite;
	};
	background_ = makeSprite(kSolidTexture);
	delayedFill_ = makeSprite(kSolidTexture);
	fill_ = makeSprite(kSolidTexture);
	outlineTop_ = makeSprite(kSolidTexture);
	outlineBottom_ = makeSprite(kSolidTexture);
	outlineLeft_ = makeSprite(kSolidTexture);
	outlineRight_ = makeSprite(kSolidTexture);
	bloomFill_ = makeSprite(kSolidTexture);
	bloomOutlineTop_ = makeSprite(kSolidTexture);
	bloomOutlineBottom_ = makeSprite(kSolidTexture);
	bloomOutlineLeft_ = makeSprite(kSolidTexture);
	bloomOutlineRight_ = makeSprite(kSolidTexture);

	auto makeMaskSegment = [spriteCommon](const char* texturePath, float sourceX, float sourceWidth, float sourceHeight) {
		auto sprite = std::make_unique<cg2::Sprite>();
		sprite->Initialize(spriteCommon, texturePath);
		sprite->SetTextureLeftTop({ sourceX, 0.0f });
		sprite->SetTextureSize({ sourceWidth, sourceHeight });
		return sprite;
	};
	roundedBackgroundLeft_ = makeMaskSegment(kFillMaskTexture, 0.0f, kFillCapSourceWidth, kFillMaskHeight);
	roundedBackgroundRight_ = makeMaskSegment(kFillMaskTexture, 128.0f - kFillCapSourceWidth, kFillCapSourceWidth, kFillMaskHeight);
	roundedDelayedLeft_ = makeMaskSegment(kFillMaskTexture, 0.0f, kFillCapSourceWidth, kFillMaskHeight);
	roundedDelayedRight_ = makeMaskSegment(kFillMaskTexture, 128.0f - kFillCapSourceWidth, kFillCapSourceWidth, kFillMaskHeight);
	roundedFillLeft_ = makeMaskSegment(kFillMaskTexture, 0.0f, kFillCapSourceWidth, kFillMaskHeight);
	roundedFillRight_ = makeMaskSegment(kFillMaskTexture, 128.0f - kFillCapSourceWidth, kFillCapSourceWidth, kFillMaskHeight);
	roundedOutlineLeft_ = makeMaskSegment(kOutlineMaskTexture, 0.0f, kOutlineCapSourceWidth, kOutlineMaskHeight);
	roundedOutlineCenter_ = makeMaskSegment(kOutlineMaskTexture, kOutlineCapSourceWidth, 128.0f - kOutlineCapSourceWidth * 2.0f, kOutlineMaskHeight);
	roundedOutlineRight_ = makeMaskSegment(kOutlineMaskTexture, 128.0f - kOutlineCapSourceWidth, kOutlineCapSourceWidth, kOutlineMaskHeight);
	roundedBloomFillLeft_ = makeMaskSegment(kFillMaskTexture, 0.0f, kFillCapSourceWidth, kFillMaskHeight);
	roundedBloomFillRight_ = makeMaskSegment(kFillMaskTexture, 128.0f - kFillCapSourceWidth, kFillCapSourceWidth, kFillMaskHeight);
	roundedBloomOutlineLeft_ = makeMaskSegment(kOutlineMaskTexture, 0.0f, kOutlineCapSourceWidth, kOutlineMaskHeight);
	roundedBloomOutlineCenter_ = makeMaskSegment(kOutlineMaskTexture, kOutlineCapSourceWidth, 128.0f - kOutlineCapSourceWidth * 2.0f, kOutlineMaskHeight);
	roundedBloomOutlineRight_ = makeMaskSegment(kOutlineMaskTexture, 128.0f - kOutlineCapSourceWidth, kOutlineCapSourceWidth, kOutlineMaskHeight);
	dirty_ = true;
}

void NeonProgressBar::SetStyle(const NeonProgressBarStyle& style)
{
	style_ = style;
	style_.fillResponse = (std::clamp)(style_.fillResponse, 0.0f, 40.0f);
	style_.delayedResponse = (std::clamp)(style_.delayedResponse, 0.0f, 40.0f);
	style_.bloomBrightness = (std::clamp)(style_.bloomBrightness, 0.0f, 8.0f);
	style_.bloomAlpha = (std::clamp)(style_.bloomAlpha, 0.0f, 1.0f);
	style_.outlineWidth = (std::clamp)(style_.outlineWidth, 0.5f, 8.0f);
	dirty_ = true;
}

void NeonProgressBar::SetBounds(const cg2::Vector2& position, const cg2::Vector2& size)
{
	const cg2::Vector2 clampedSize = { (std::max)(0.0f, size.x), (std::max)(0.0f, size.y) };
	if (position_.x == position.x && position_.y == position.y &&
		size_.x == clampedSize.x && size_.y == clampedSize.y) {
		return;
	}
	position_ = position;
	size_ = clampedSize;
	dirty_ = true;
}

void NeonProgressBar::SetTarget(float normalizedValue)
{
	const float target = (std::clamp)(normalizedValue, 0.0f, 1.0f);
	if (rolloverActive_) {
		rolloverTargetValue_ = target;
		return;
	}
	targetValue_ = target;
	if (!hasValue_) {
		SnapTo(targetValue_);
	}
}

void NeonProgressBar::SnapTo(float normalizedValue)
{
	targetValue_ = (std::clamp)(normalizedValue, 0.0f, 1.0f);
	displayValue_ = targetValue_;
	delayedValue_ = targetValue_;
	hasValue_ = true;
	rolloverActive_ = false;
	dirty_ = true;
}

void NeonProgressBar::BeginRollover(float normalizedValue)
{
	const float target = (std::clamp)(normalizedValue, 0.0f, 1.0f);
	if (!hasValue_) {
		SnapTo(target);
		return;
	}
	rolloverTargetValue_ = target;
	if (rolloverActive_) {
		return;
	}
	if (displayValue_ >= 0.995f) {
		displayValue_ = 0.0f;
		delayedValue_ = 0.0f;
		targetValue_ = rolloverTargetValue_;
		dirty_ = true;
		return;
	}
	rolloverActive_ = true;
	targetValue_ = 1.0f;
}

void NeonProgressBar::Update(float deltaTime)
{
	if (!hasValue_) {
		return;
	}
	const float previousDisplayValue = displayValue_;
	const float previousDelayedValue = delayedValue_;
	displayValue_ = SmoothToward(displayValue_, targetValue_, style_.fillResponse, deltaTime);
	delayedValue_ = SmoothToward(delayedValue_, displayValue_, style_.delayedResponse, deltaTime);
	if (rolloverActive_ && displayValue_ >= 0.995f) {
		displayValue_ = 0.0f;
		delayedValue_ = 0.0f;
		targetValue_ = rolloverTargetValue_;
		rolloverActive_ = false;
	}
	dirty_ = dirty_ || previousDisplayValue != displayValue_ || previousDelayedValue != delayedValue_;
	UpdateSprites();
}

void NeonProgressBar::Draw()
{
	if (dirty_) {
		UpdateSprites();
	}
	if (style_.roundedEnds) {
		if (roundedBackgroundLeft_) roundedBackgroundLeft_->Draw();
		if (background_) background_->Draw();
		if (roundedBackgroundRight_) roundedBackgroundRight_->Draw();
		if (delayedValue_ > displayValue_ + 0.001f) {
			if (roundedDelayedLeft_) roundedDelayedLeft_->Draw();
			if (delayedFill_) delayedFill_->Draw();
			if (roundedDelayedRight_) roundedDelayedRight_->Draw();
		}
		if (displayValue_ > 0.001f) {
			if (roundedFillLeft_) roundedFillLeft_->Draw();
			if (fill_) fill_->Draw();
			if (roundedFillRight_) roundedFillRight_->Draw();
		}
		if (roundedOutlineLeft_) roundedOutlineLeft_->Draw();
		if (roundedOutlineCenter_) roundedOutlineCenter_->Draw();
		if (roundedOutlineRight_) roundedOutlineRight_->Draw();
		return;
	}
	if (background_) background_->Draw();
	if (delayedValue_ > displayValue_ + 0.001f && delayedFill_) delayedFill_->Draw();
	if (fill_) fill_->Draw();
	if (outlineTop_) outlineTop_->Draw();
	if (outlineBottom_) outlineBottom_->Draw();
	if (outlineLeft_) outlineLeft_->Draw();
	if (outlineRight_) outlineRight_->Draw();
}

void NeonProgressBar::DrawBloomSource()
{
	if (dirty_) {
		UpdateSprites();
	}
	if (style_.roundedEnds) {
		if (displayValue_ > 0.001f) {
			if (roundedBloomFillLeft_) roundedBloomFillLeft_->Draw();
			if (bloomFill_) bloomFill_->Draw();
			if (roundedBloomFillRight_) roundedBloomFillRight_->Draw();
		}
		if (roundedBloomOutlineLeft_) roundedBloomOutlineLeft_->Draw();
		if (roundedBloomOutlineCenter_) roundedBloomOutlineCenter_->Draw();
		if (roundedBloomOutlineRight_) roundedBloomOutlineRight_->Draw();
		return;
	}
	if (bloomFill_) bloomFill_->Draw();
	if (bloomOutlineTop_) bloomOutlineTop_->Draw();
	if (bloomOutlineBottom_) bloomOutlineBottom_->Draw();
	if (bloomOutlineLeft_) bloomOutlineLeft_->Draw();
	if (bloomOutlineRight_) bloomOutlineRight_->Draw();
}

void NeonProgressBar::UpdateSprites()
{
	if (!dirty_ || !spriteCommon_) {
		return;
	}
	const float outlineWidth = (std::min)(style_.outlineWidth, (std::min)(size_.x, size_.y) * 0.5f);
	const cg2::Vector2 innerPosition = { position_.x + outlineWidth, position_.y + outlineWidth };
	const cg2::Vector2 innerSize = {
		(std::max)(0.0f, size_.x - outlineWidth * 2.0f),
		(std::max)(0.0f, size_.y - outlineWidth * 2.0f)
	};
	const cg2::Vector2 bottomPosition = { position_.x, position_.y + (std::max)(0.0f, size_.y - outlineWidth) };
	const cg2::Vector2 rightPosition = { position_.x + (std::max)(0.0f, size_.x - outlineWidth), position_.y };
	const cg2::Vector4 bloomOutlineColor = MultiplyBrightness(style_.outlineColor, style_.bloomBrightness, style_.bloomAlpha);

	if (style_.roundedEnds) {
		const float roundedOutlineInset = (std::max)(outlineWidth, size_.y * 0.12f);
		const cg2::Vector2 roundedInnerPosition = { position_.x + roundedOutlineInset, position_.y + roundedOutlineInset };
		const cg2::Vector2 roundedInnerSize = {
			(std::max)(0.0f, size_.x - roundedOutlineInset * 2.0f),
			(std::max)(0.0f, size_.y - roundedOutlineInset * 2.0f)
		};
		const cg2::Vector4 bloomFillColor = MultiplyBrightness(style_.fillColor, style_.bloomBrightness, style_.bloomAlpha);
		const cg2::Vector4 bloomOutlineColor = MultiplyBrightness(style_.outlineColor, style_.bloomBrightness, style_.bloomAlpha);
		const bool fillComplete = displayValue_ >= 0.999f;
		const bool delayedComplete = delayedValue_ >= 0.999f;
		const cg2::Vector2 fillSize = { roundedInnerSize.x * displayValue_, roundedInnerSize.y };
		const cg2::Vector2 delayedSize = { roundedInnerSize.x * delayedValue_, roundedInnerSize.y };

		SetCapsuleSegments(roundedBackgroundLeft_.get(), background_.get(), roundedBackgroundRight_.get(), position_, size_, style_.backgroundColor, true);
		SetCapsuleSegments(roundedDelayedLeft_.get(), delayedFill_.get(), roundedDelayedRight_.get(), roundedInnerPosition, delayedSize, style_.delayedFillColor, delayedComplete);
		SetCapsuleSegments(roundedFillLeft_.get(), fill_.get(), roundedFillRight_.get(), roundedInnerPosition, fillSize, style_.fillColor, fillComplete);
		SetCapsuleSegments(roundedOutlineLeft_.get(), roundedOutlineCenter_.get(), roundedOutlineRight_.get(), position_, size_, style_.outlineColor, true);
		SetCapsuleSegments(roundedBloomFillLeft_.get(), bloomFill_.get(), roundedBloomFillRight_.get(), roundedInnerPosition, fillSize, bloomFillColor, fillComplete);
		SetCapsuleSegments(roundedBloomOutlineLeft_.get(), roundedBloomOutlineCenter_.get(), roundedBloomOutlineRight_.get(), position_, size_, bloomOutlineColor, true);
		dirty_ = false;
		return;
	}

	SetSolidSprite(background_.get(), position_, size_, style_.backgroundColor);
	SetFillSprite(delayedFill_.get(), innerPosition, innerSize, delayedValue_, style_.delayedFillColor);
	SetFillSprite(fill_.get(), innerPosition, innerSize, displayValue_, style_.fillColor);
	SetSolidSprite(outlineTop_.get(), position_, { size_.x, outlineWidth }, style_.outlineColor);
	SetSolidSprite(outlineBottom_.get(), bottomPosition, { size_.x, outlineWidth }, style_.outlineColor);
	SetSolidSprite(outlineLeft_.get(), position_, { outlineWidth, size_.y }, style_.outlineColor);
	SetSolidSprite(outlineRight_.get(), rightPosition, { outlineWidth, size_.y }, style_.outlineColor);

	SetFillSprite(bloomFill_.get(), innerPosition, innerSize, displayValue_, MultiplyBrightness(style_.fillColor, style_.bloomBrightness, style_.bloomAlpha));
	SetSolidSprite(bloomOutlineTop_.get(), position_, { size_.x, outlineWidth }, bloomOutlineColor);
	SetSolidSprite(bloomOutlineBottom_.get(), bottomPosition, { size_.x, outlineWidth }, bloomOutlineColor);
	SetSolidSprite(bloomOutlineLeft_.get(), position_, { outlineWidth, size_.y }, bloomOutlineColor);
	SetSolidSprite(bloomOutlineRight_.get(), rightPosition, { outlineWidth, size_.y }, bloomOutlineColor);

	dirty_ = false;
}

cg2::Vector4 NeonProgressBar::MultiplyBrightness(const cg2::Vector4& color, float brightness, float alpha)
{
	return { color.x * brightness, color.y * brightness, color.z * brightness, color.w * alpha };
}
