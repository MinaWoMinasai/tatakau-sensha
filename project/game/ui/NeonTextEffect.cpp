#include "game/ui/NeonTextEffect.h"

#include <algorithm>

namespace {

void ApplyBloomStyle(cg2::ObjectPostEffect& effect, const NeonTextEffectStyle& style, float intensity)
{
	cg2::BloomParam param = effect.GetParam();
	param.threshold = (std::max)(0.0f, style.threshold);
	param.intensity = (std::max)(0.0f, intensity);
	param.outlineWidth = 0.0f;
	param.outlineBloomIntensity = 0.0f;
	effect.SetParam(param);
	effect.Update(0.0f);
}

} // namespace

void NeonTextEffect::Initialize(
	cg2::DirectXCommon* dxCommon,
	cg2::SrvManager* srvManager,
	cg2::RtvManager* rtvManager)
{
	spriteCommon_ = cg2::SpriteCommon::GetInstance();
	innerEffect_ = std::make_unique<cg2::ObjectPostEffect>();
	innerEffect_->Initialize(dxCommon, srvManager, rtvManager, 1.0f);
	outerEffect_ = std::make_unique<cg2::ObjectPostEffect>();
	outerEffect_->Initialize(dxCommon, srvManager, rtvManager, 0.4f);
	SetStyle(style_);
}

void NeonTextEffect::SetStyle(const NeonTextEffectStyle& style)
{
	style_ = style;
	style_.sourceBrightness = (std::max)(0.0f, style_.sourceBrightness);
	style_.innerIntensity = (std::max)(0.0f, style_.innerIntensity);
	style_.outerIntensity = (std::max)(0.0f, style_.outerIntensity);
	if (innerEffect_) {
		ApplyBloomStyle(*innerEffect_, style_, style_.innerIntensity);
	}
	if (outerEffect_) {
		ApplyBloomStyle(*outerEffect_, style_, style_.outerIntensity);
	}
}

void NeonTextEffect::DrawBloom(const std::vector<cg2::TextLabel*>& labels)
{
	if (!style_.enabled || !innerEffect_ || !outerEffect_ || labels.empty()) {
		return;
	}
	SynchronizeSources(labels);
	DrawLayer(*outerEffect_, labels);
	DrawLayer(*innerEffect_, labels);
}

void NeonTextEffect::ClearSources()
{
	sources_.clear();
}

void NeonTextEffect::SynchronizeSources(const std::vector<cg2::TextLabel*>& labels)
{
	for (cg2::TextLabel* label : labels) {
		if (!label) {
			continue;
		}
		label->PrepareForDraw();
		if (!label->GetSprite() || label->GetTexturePath().empty()) {
			continue;
		}
		GlowSource& source = sources_[label];
		if (!source.sprite) {
			source.sprite = std::make_unique<cg2::Sprite>();
			source.sprite->Initialize(spriteCommon_, label->GetTexturePath());
			source.texturePath = label->GetTexturePath();
		} else if (source.texturePath != label->GetTexturePath()) {
			source.sprite->SetTexture(label->GetTexturePath());
			source.texturePath = label->GetTexturePath();
		}

		cg2::Sprite* sharp = label->GetSprite();
		source.sprite->SetPosition(sharp->GetPosition());
		source.sprite->SetRotation(sharp->GetRotation());
		source.sprite->SetSize(sharp->GetSize());
		source.sprite->SetAnchorPoint(sharp->GetAnchorPoint());
		const float sourceAlpha = sharp->GetColor().w * style_.glowColor.w;
		source.sprite->SetColor({
			style_.glowColor.x * style_.sourceBrightness,
			style_.glowColor.y * style_.sourceBrightness,
			style_.glowColor.z * style_.sourceBrightness,
			sourceAlpha
		});
		source.sprite->Update();
	}
}

void NeonTextEffect::DrawLayer(cg2::ObjectPostEffect& effect, const std::vector<cg2::TextLabel*>& labels)
{
	effect.BeginCapture();
	cg2::SpriteCommon::GetInstance()->PreDrawForScene(cg2::kNormal);
	for (cg2::TextLabel* label : labels) {
		const auto found = sources_.find(label);
		if (found != sources_.end() && found->second.sprite) {
			found->second.sprite->Draw();
		}
	}
	effect.EndCaptureBloomOnlyToBackBuffer();
}
