#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ObjectPostEffect.h"
#include "TextLabel.h"

struct NeonTextEffectStyle {
	bool enabled = true;
	Vector4 glowColor{ 0.18f, 1.0f, 0.48f, 1.0f };
	float sourceBrightness = 2.2f;
	float threshold = 0.0f;
	float innerIntensity = 0.82f;
	float outerIntensity = 0.48f;
};

class NeonTextEffect {
public:
	void Initialize(
		DirectXCommon* dxCommon,
		SrvManager* srvManager,
		RtvManager* rtvManager = nullptr);
	void SetStyle(const NeonTextEffectStyle& style);
	void DrawBloom(const std::vector<TextLabel*>& labels);
	void ClearSources();

private:
	struct GlowSource {
		std::unique_ptr<Sprite> sprite;
		std::string texturePath;
	};

	void SynchronizeSources(const std::vector<TextLabel*>& labels);
	void DrawLayer(ObjectPostEffect& effect, const std::vector<TextLabel*>& labels);

	SpriteCommon* spriteCommon_ = nullptr;
	std::unique_ptr<ObjectPostEffect> innerEffect_;
	std::unique_ptr<ObjectPostEffect> outerEffect_;
	std::unordered_map<const TextLabel*, GlowSource> sources_;
	NeonTextEffectStyle style_{};
};
