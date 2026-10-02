#pragma once

#include <memory>
#include <vector>

#include "game/ui/NeonProgressBar.h"

// 任意の最大強化段数を、小さな区切りセルとして表示するHUD部品。
struct NeonSegmentedBarStyle {
	cg2::Vector4 backgroundColor{ 0.012f, 0.030f, 0.052f, 0.90f };
	cg2::Vector4 emptyColor{ 0.055f, 0.100f, 0.145f, 0.94f };
	cg2::Vector4 filledColor{ 0.38f, 1.00f, 0.62f, 0.98f };
	cg2::Vector4 outlineColor{ 0.40f, 0.94f, 1.00f, 0.92f };
	float innerPadding = 2.0f;
	float segmentGap = 2.0f;
	float bloomBrightness = 1.75f;
	float bloomAlpha = 0.62f;
	float backdropBloomBrightness = 1.65f;
	float backdropBloomAlpha = 0.16f;
	bool roundedFrame = true;
};

class NeonSegmentedBar {
public:
	void Initialize(cg2::SpriteCommon* spriteCommon);
	void SetStyle(const NeonSegmentedBarStyle& style);
	void SetBounds(const cg2::Vector2& position, const cg2::Vector2& size);
	void SetSegmentCount(int count);
	void SetFilledSegments(int count);
	void Update();
	void Draw();
	void DrawBloomSource();

private:
	void EnsureSegmentSprites();
	static cg2::Vector4 MultiplyBrightness(const cg2::Vector4& color, float brightness, float alpha);

	cg2::SpriteCommon* spriteCommon_ = nullptr;
	NeonSegmentedBarStyle style_{};
	cg2::Vector2 position_{};
	cg2::Vector2 size_{};
	int segmentCount_ = 1;
	int filledSegments_ = 0;
	bool dirty_ = true;
	std::unique_ptr<NeonProgressBar> frame_;
	std::unique_ptr<cg2::Sprite> bloomBackdrop_;
	std::vector<std::unique_ptr<cg2::Sprite>> segments_;
	std::vector<std::unique_ptr<cg2::Sprite>> bloomSegments_;
};
