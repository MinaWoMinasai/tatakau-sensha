#include "ObjectPostEffect.h"
#include <algorithm>
#include <cassert>

void ObjectPostEffect::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, RtvManager* rtvManager, float renderScale) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    if (rtvManager) {
        rtvManager_ = rtvManager;
    } else {
        ownedRtvManager_ = std::make_unique<RtvManager>();
        ownedRtvManager_->Initialize(dxCommon_);
        rtvManager_ = ownedRtvManager_.get();
    }

    const std::array<float, 4> transparent = { 0.0f, 0.0f, 0.0f, 0.0f };
    const float safeScale = (std::clamp)(renderScale, 0.25f, 1.0f);
    renderWidth_ = (std::max)(1u, static_cast<uint32_t>(static_cast<float>(WinApp::kClientWidth) * safeScale));
    renderHeight_ = (std::max)(1u, static_cast<uint32_t>(static_cast<float>(WinApp::kClientHeight) * safeScale));
    halfWidth_ = (std::max)(1u, renderWidth_ / 2);
    halfHeight_ = (std::max)(1u, renderHeight_ / 2);
    quarterWidth_ = (std::max)(1u, renderWidth_ / 4);
    quarterHeight_ = (std::max)(1u, renderHeight_ / 4);

    objectRT_ = std::make_unique<RenderTexture>();
    objectRT_->Initialize(dxCommon_, srvManager_, rtvManager_, renderWidth_, renderHeight_, transparent, false);

    bloomRT_A_ = std::make_unique<RenderTexture>();
    bloomRT_A_->Initialize(dxCommon_, srvManager_, rtvManager_, quarterWidth_, quarterHeight_, transparent, false);

    bloomRT_B_ = std::make_unique<RenderTexture>();
    bloomRT_B_->Initialize(dxCommon_, srvManager_, rtvManager_, quarterWidth_, quarterHeight_, transparent, false);

    bloomRT_Half_ = std::make_unique<RenderTexture>();
    bloomRT_Half_->Initialize(dxCommon_, srvManager_, rtvManager_, halfWidth_, halfHeight_, transparent, false);

    cb_ = std::make_unique<BloomConstantBuffer>();
    cb_->Initialize(dxCommon_);

    postEffect_ = std::make_unique<PostEffect>();
    postEffect_->Initialize(dxCommon_, cb_.get());

    param_.threshold = 0.0f;
    param_.intensity = 1.0f;
    param_.vignetteIntensity = 0.0f;
    param_.vignetteScale = 0.0f;
    param_.distortionAmount = 0.0f;
    param_.chromAbAmount = 0.0f;
    param_.noiseIntensity = 0.0f;
    param_.scanlineIntensity = 0.0f;
    param_.scanlineFrequency = 100.0f;
    param_.curvature = 0.0f;
    param_.borderSharp = 0.0f;
    param_.glitchAmount = 0.0f;
    param_.fullScreenBoxBlurBlend = 0.0f;
    param_.dissolveThreshold = 0.0f;
    param_.outlineWidth = 0.0f;
    param_.outlineThreshold = 0.5f;
    param_.outlineColor = { 1.0f, 1.0f, 1.0f };
    param_.outlineBloomIntensity = 0.0f;
    param_.outlineBloomWidth = 6.0f;

    cb_->Update(param_);
}

void ObjectPostEffect::Update(float deltaTime) {
    timer_ += deltaTime;
    param_.timer = timer_;
    cb_->Update(param_);
}

void ObjectPostEffect::BeginCapture() {
    restoreRtvHandle_ = dxCommon_->GetCurrentRTVHandle();
    restoreDsvHandle_ = dxCommon_->GetCurrentDSVHandle();
    restoreHasDsv_ = dxCommon_->HasCurrentDSV();

    Transition(objectRT_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

    dxCommon_->SetRenderTargetNoDepth(objectRT_->GetRTVHandle());
    ClearTransparent(objectRT_->GetRTVHandle());
    dxCommon_->SetViewport(renderWidth_, renderHeight_);
}

void ObjectPostEffect::EndCapture() {
    FinishCapture(FinishMode::CompositeAndAdd);
}

void ObjectPostEffect::EndCaptureAdditiveOnly() {
    FinishCapture(FinishMode::AdditiveOnly);
}

void ObjectPostEffect::EndCaptureBloomOnly() {
    FinishCapture(FinishMode::BloomOnly);
}

void ObjectPostEffect::EndCaptureBloomOnlyToCache() {
    FinishCapture(FinishMode::BloomOnlyCache);
}

void ObjectPostEffect::DrawCachedBloom(const Vector2& uvOffset) {
    const float savedOffsetX = param_.boxBlurRadius;
    const float savedOffsetY = param_.fullScreenBoxBlurBlend;
    param_.boxBlurRadius = uvOffset.x;
    param_.fullScreenBoxBlurBlend = uvOffset.y;
    cb_->Update(param_);
    postEffect_->DrawObjectBloomAdd(bloomRT_A_->GetGPUHandle());
    param_.boxBlurRadius = savedOffsetX;
    param_.fullScreenBoxBlurBlend = savedOffsetY;
    cb_->Update(param_);
}

void ObjectPostEffect::FinishCapture(FinishMode mode) {
    Transition(objectRT_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    Transition(bloomRT_Half_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_Half_->GetRTVHandle());
    dxCommon_->SetViewport(halfWidth_, halfHeight_);
    ClearTransparent(bloomRT_Half_->GetRTVHandle());
    postEffect_->Draw(objectRT_->GetGPUHandle(), kAdd_Bloom_Extract);
    Transition(bloomRT_Half_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_A_->GetRTVHandle());
    dxCommon_->SetViewport(quarterWidth_, quarterHeight_);
    ClearTransparent(bloomRT_A_->GetRTVHandle());
    postEffect_->Draw(bloomRT_Half_->GetGPUHandle(), kAdd_Bloom_Downsample);
    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    Transition(bloomRT_B_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_B_->GetRTVHandle());
    dxCommon_->SetViewport(quarterWidth_, quarterHeight_);
    ClearTransparent(bloomRT_B_->GetRTVHandle());
    postEffect_->Draw(bloomRT_A_->GetGPUHandle(), kAdd_Bloom_BlurH);
    Transition(bloomRT_B_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_A_->GetRTVHandle());
    dxCommon_->SetViewport(quarterWidth_, quarterHeight_);
    ClearTransparent(bloomRT_A_->GetRTVHandle());
    postEffect_->Draw(bloomRT_B_->GetGPUHandle(), kAdd_Bloom_BlurV);
    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    if (restoreHasDsv_) {
        dxCommon_->SetRenderTarget(restoreRtvHandle_, restoreDsvHandle_);
    } else {
        dxCommon_->SetRenderTargetNoDepth(restoreRtvHandle_);
    }
    dxCommon_->SetViewport(WinApp::kClientWidth, WinApp::kClientHeight);
    if (mode == FinishMode::BloomOnlyCache) {
        return;
    }
    if (mode == FinishMode::BloomOnly) {
        postEffect_->DrawObjectBloomAdd(bloomRT_A_->GetGPUHandle());
        return;
    }
    if (mode == FinishMode::CompositeAndAdd) {
        postEffect_->DrawObjectComposite(objectRT_->GetGPUHandle(), bloomRT_A_->GetGPUHandle());
    }
    postEffect_->DrawObjectOutlineAdd(objectRT_->GetGPUHandle(), bloomRT_A_->GetGPUHandle());
}

void ObjectPostEffect::SetParam(const BloomParam& param) {
    param_ = param;
    cb_->Update(param_);
}

void ObjectPostEffect::Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    assert(resource != nullptr);
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    dxCommon_->GetList()->ResourceBarrier(1, &barrier);
}

void ObjectPostEffect::ClearTransparent(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle) {
    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    dxCommon_->GetList()->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
}
