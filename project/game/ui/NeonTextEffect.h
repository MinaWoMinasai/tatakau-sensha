#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ObjectPostEffect.h"
#include "TextLabel.h"

struct NeonTextEffectStyle {
	bool enabled = true;
	cg2::Vector4 glowColor{ 0.18f, 1.0f, 0.48f, 1.0f };
	float sourceBrightness = 2.2f;
	float threshold = 0.0f;
	float innerIntensity = 0.82f;
	float outerIntensity = 0.48f;
};

class NeonTextEffect {
public:
	void Initialize(
		cg2::DirectXCommon* dxCommon,
		cg2::SrvManager* srvManager,
		cg2::RtvManager* rtvManager = nullptr);
	void SetStyle(const NeonTextEffectStyle& style);
	void DrawBloom(const std::vector<cg2::TextLabel*>& labels);
	void ClearSources();

private:
	struct GlowSource {
		std::unique_ptr<cg2::Sprite> sprite;
		std::string texturePath;
	};

	void SynchronizeSources(const std::vector<cg2::TextLabel*>& labels);
	void DrawLayer(cg2::ObjectPostEffect& effect, const std::vector<cg2::TextLabel*>& labels);

	cg2::SpriteCommon* spriteCommon_ = nullptr;
	std::unique_ptr<cg2::ObjectPostEffect> innerEffect_;
	std::unique_ptr<cg2::ObjectPostEffect> outerEffect_;
	std::unordered_map<const cg2::TextLabel*, GlowSource> sources_;
	NeonTextEffectStyle style_{};
};
