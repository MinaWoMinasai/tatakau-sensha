#include "game/ui/NeonSegmentedBar.h"

#include <algorithm>

namespace {

constexpr const char* kSolidTexture = "resources/white512x512.png";
constexpr const char* kFillMaskTexture = "resources/hpBarFillMask.png";
constexpr float kFillMaskWidth = 128.0f;
constexpr float kFillMaskHeight = 24.0f;

/// @brief 単色スプライトを設定する。
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

// 最初と最後のセルだけを片側丸端にする。中央側は直線のままなので、
// 区切りセルの見た目を維持したまま、外枠から四角がはみ出さない。
void SetSegmentShape(cg2::Sprite* sprite, int index, int segmentCount, const cg2::Vector2& size, bool roundedFrame)
{
	if (!sprite) {
		return;
	}
	const bool first = index == 0;
	const bool last = index == segmentCount - 1;
	if (!roundedFrame || segmentCount <= 1 || (!first && !last) || size.y <= 0.01f) {
		sprite->SetTexture(kSolidTexture);
		sprite->SetTextureLeftTop({ 0.0f, 0.0f });
		sprite->SetTextureSize({ 512.0f, 512.0f });
		return;
	}

	// 画面上の縦横倍率と同じ比率のソース領域を使うことで、丸端を潰さない。
	const float sourceWidth = (std::clamp)(size.x / size.y * kFillMaskHeight, 12.0f, kFillMaskWidth);
	sprite->SetTexture(kFillMaskTexture);
	sprite->SetTextureLeftTop({ first ? 0.0f : kFillMaskWidth - sourceWidth, 0.0f });
	sprite->SetTextureSize({ sourceWidth, kFillMaskHeight });
}

} // namespace

void NeonSegmentedBar::Initialize(cg2::SpriteCommon* spriteCommon)
{
	spriteCommon_ = spriteCommon;
	if (!spriteCommon_) {
		return;
	}
	frame_ = std::make_unique<NeonProgressBar>();
	frame_->Initialize(spriteCommon_);
	bloomBackdrop_ = std::make_unique<cg2::Sprite>();
	bloomBackdrop_->Initialize(spriteCommon_, kFillMaskTexture);
	EnsureSegmentSprites();
	dirty_ = true;
}

void NeonSegmentedBar::SetStyle(const NeonSegmentedBarStyle& style)
{
	style_ = style;
	style_.innerPadding = (std::clamp)(style_.innerPadding, 0.0f, 16.0f);
	style_.segmentGap = (std::clamp)(style_.segmentGap, 0.0f, 16.0f);
	style_.bloomBrightness = (std::clamp)(style_.bloomBrightness, 0.0f, 8.0f);
	style_.bloomAlpha = (std::clamp)(style_.bloomAlpha, 0.0f, 1.0f);
	style_.backdropBloomBrightness = (std::clamp)(style_.backdropBloomBrightness, 0.0f, 8.0f);
	style_.backdropBloomAlpha = (std::clamp)(style_.backdropBloomAlpha, 0.0f, 1.0f);
	dirty_ = true;
}

void NeonSegmentedBar::SetBounds(const cg2::Vector2& position, const cg2::Vector2& size)
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

void NeonSegmentedBar::SetSegmentCount(int count)
{
	const int clampedCount = (std::clamp)(count, 1, 16);
	if (segmentCount_ != clampedCount) {
		segmentCount_ = clampedCount;
		dirty_ = true;
	}
}

void NeonSegmentedBar::SetFilledSegments(int count)
{
	const int clampedCount = (std::clamp)(count, 0, segmentCount_);
	if (filledSegments_ != clampedCount) {
		filledSegments_ = clampedCount;
		dirty_ = true;
	}
}

void NeonSegmentedBar::Update()
{
	if (!dirty_ || !spriteCommon_) {
		return;
	}
	EnsureSegmentSprites();

	NeonProgressBarStyle frameStyle{};
	frameStyle.backgroundColor = style_.backgroundColor;
	frameStyle.delayedFillColor = { 0.0f, 0.0f, 0.0f, 0.0f };
	frameStyle.fillColor = { 0.0f, 0.0f, 0.0f, 0.0f };
	frameStyle.outlineColor = style_.outlineColor;
	frameStyle.bloomBrightness = style_.bloomBrightness;
	frameStyle.bloomAlpha = style_.bloomAlpha;
	// 下部EXP/Levelバーと同じ太さにして、丸端の外枠を共通化する。
	frameStyle.outlineWidth = 1.5f;
	frameStyle.roundedEnds = style_.roundedFrame;
	frame_->SetStyle(frameStyle);
	frame_->SetBounds(position_, size_);
	frame_->SetTarget(0.0f);
	frame_->Update(0.0f);

	const float minimumRoundedPadding = style_.roundedFrame
		? (std::max)(frameStyle.outlineWidth + 1.0f, size_.y * 0.12f + 0.5f)
		: frameStyle.outlineWidth;
	const float padding = (std::min)(
		(std::max)(style_.innerPadding, minimumRoundedPadding),
		(std::min)(size_.x, size_.y) * 0.35f);
	const cg2::Vector2 innerPosition = { position_.x + padding, position_.y + padding };
	const cg2::Vector2 innerSize = {
		(std::max)(0.0f, size_.x - padding * 2.0f),
		(std::max)(0.0f, size_.y - padding * 2.0f)
	};
	const float totalGap = style_.segmentGap * static_cast<float>((std::max)(0, segmentCount_ - 1));
	const float segmentWidth = (std::max)(0.0f, (innerSize.x - totalGap) / static_cast<float>(segmentCount_));
	const cg2::Vector4 bloomFilled = MultiplyBrightness(style_.filledColor, style_.bloomBrightness, style_.bloomAlpha);
	if (bloomBackdrop_) {
		bloomBackdrop_->SetTexture(kFillMaskTexture);
		bloomBackdrop_->SetTextureLeftTop({ 0.0f, 0.0f });
		bloomBackdrop_->SetTextureSize({ kFillMaskWidth, kFillMaskHeight });
		SetSolidSprite(
			bloomBackdrop_.get(),
			position_,
			size_,
			MultiplyBrightness(style_.outlineColor, style_.backdropBloomBrightness, style_.backdropBloomAlpha));
	}

	for (int i = 0; i < segmentCount_; ++i) {
		const cg2::Vector2 segmentPosition = {
			innerPosition.x + static_cast<float>(i) * (segmentWidth + style_.segmentGap),
			innerPosition.y
		};
		const bool filled = i < filledSegments_;
		SetSegmentShape(segments_[i].get(), i, segmentCount_, { segmentWidth, innerSize.y }, style_.roundedFrame);
		SetSegmentShape(bloomSegments_[i].get(), i, segmentCount_, { segmentWidth, innerSize.y }, style_.roundedFrame);
		SetSolidSprite(segments_[i].get(), segmentPosition, { segmentWidth, innerSize.y }, filled ? style_.filledColor : style_.emptyColor);
		SetSolidSprite(
			bloomSegments_[i].get(),
			segmentPosition,
			{ segmentWidth, innerSize.y },
			filled ? bloomFilled : cg2::Vector4{ 0.0f, 0.0f, 0.0f, 0.0f });
	}
	dirty_ = false;
}

void NeonSegmentedBar::Draw()
{
	Update();
	if (frame_) {
		frame_->Draw();
	}
	for (int i = 0; i < segmentCount_; ++i) {
		if (segments_[i]) {
			segments_[i]->Draw();
		}
	}
}

void NeonSegmentedBar::DrawBloomSource()
{
	Update();
	// 低輝度の下地でバー全体の輪郭を発光させ、取得済みセルはより強く光らせる。
	if (bloomBackdrop_) {
		bloomBackdrop_->Draw();
	}
	for (int i = 0; i < filledSegments_; ++i) {
		if (bloomSegments_[i]) {
			bloomSegments_[i]->Draw();
		}
	}
}

void NeonSegmentedBar::EnsureSegmentSprites()
{
	if (!spriteCommon_) {
		return;
	}
	while (static_cast<int>(segments_.size()) < segmentCount_) {
		auto segment = std::make_unique<cg2::Sprite>();
		segment->Initialize(spriteCommon_, kSolidTexture);
		segments_.push_back(std::move(segment));
		auto bloomSegment = std::make_unique<cg2::Sprite>();
		bloomSegment->Initialize(spriteCommon_, kSolidTexture);
		bloomSegments_.push_back(std::move(bloomSegment));
	}
}

cg2::Vector4 NeonSegmentedBar::MultiplyBrightness(const cg2::Vector4& color, float brightness, float alpha)
{
	return { color.x * brightness, color.y * brightness, color.z * brightness, color.w * alpha };
}
