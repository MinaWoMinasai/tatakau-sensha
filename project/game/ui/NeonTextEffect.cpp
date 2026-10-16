#include "game/ui/NeonTextEffect.h"

#include <algorithm>

namespace {

/// @brief ブルーム外観を現在の状態へ適用する。
void ApplyBloomStyle(myEngine::ObjectPostEffect& effect, const NeonTextEffectStyle& style, float intensity, float qualityGain,
                     float qualityScatter, float qualityRadius)
{
    myEngine::BloomParam param = effect.GetParam();
    param.threshold = (std::max)(0.0f, style.threshold);
    param.intensity = (std::max)(0.0f, intensity);
    // Legacyは従来の3乗の強度を保ち、新しいフィルタは線形の係数を1つ使う。
    param.bloomGain = qualityGain;
    param.bloomScatter = qualityScatter;
    param.bloomRadius = qualityRadius;
    param.outlineWidth = 0.0f;
    param.outlineBloomIntensity = 0.0f;
    effect.SetParam(param);
    effect.Update(0.0f);
}

} // namespace

void NeonTextEffect::Initialize(myEngine::DirectXCommon* dxCommon, myEngine::SrvManager* srvManager, myEngine::RtvManager* rtvManager)
{
    spriteCommon_ = myEngine::SpriteCommon::GetInstance();
    innerEffect_ = std::make_unique<myEngine::ObjectPostEffect>();
    innerEffect_->Initialize(dxCommon, srvManager, rtvManager, 1.0f);
    outerEffect_ = std::make_unique<myEngine::ObjectPostEffect>();
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
        // 内側の層は、別に描く鮮明な文字の横に締まった色付きの発光を加える。
        ApplyBloomStyle(*innerEffect_, style_, style_.innerIntensity, 0.55f * style_.innerIntensity / 0.82f, 0.35f, 0.7f);
    }
    if (outerEffect_) {
        // 粗いレベルの重みを増やし、外側の層に広く見えるハローを付ける。
        ApplyBloomStyle(*outerEffect_, style_, style_.outerIntensity, 0.75f * style_.outerIntensity / 0.48f, 0.55f, 1.0f);
    }
}

void NeonTextEffect::DrawBloom(const std::vector<myEngine::TextLabel*>& labels)
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

void NeonTextEffect::SynchronizeSources(const std::vector<myEngine::TextLabel*>& labels)
{
    for (myEngine::TextLabel* label : labels) {
        if (!label) {
            continue;
        }
        label->PrepareForDraw();
        if (!label->GetSprite() || label->GetTexturePath().empty()) {
            continue;
        }
        GlowSource& source = sources_[label];
        if (!source.sprite) {
            source.sprite = std::make_unique<myEngine::Sprite>();
            source.sprite->Initialize(spriteCommon_, label->GetTexturePath());
            source.texturePath = label->GetTexturePath();
        } else if (source.texturePath != label->GetTexturePath()) {
            source.sprite->SetTexture(label->GetTexturePath());
            source.texturePath = label->GetTexturePath();
        }

        myEngine::Sprite* sharp = label->GetSprite();
        source.sprite->SetPosition(sharp->GetPosition());
        source.sprite->SetRotation(sharp->GetRotation());
        source.sprite->SetSize(sharp->GetSize());
        source.sprite->SetAnchorPoint(sharp->GetAnchorPoint());
        const float sourceAlpha = sharp->GetColor().w * style_.glowColor.w;
        source.sprite->SetColor({style_.glowColor.x * style_.sourceBrightness, style_.glowColor.y * style_.sourceBrightness,
                                 style_.glowColor.z * style_.sourceBrightness, sourceAlpha});
        source.sprite->Update();
    }
}

void NeonTextEffect::DrawLayer(myEngine::ObjectPostEffect& effect, const std::vector<myEngine::TextLabel*>& labels)
{
    effect.BeginCapture();
    myEngine::SpriteCommon::GetInstance()->PreDrawForScene(myEngine::kNormal);
    for (myEngine::TextLabel* label : labels) {
        const auto found = sources_.find(label);
        if (found != sources_.end() && found->second.sprite) {
            found->second.sprite->Draw();
        }
    }
    effect.EndCaptureBloomOnlyToBackBuffer();
}
