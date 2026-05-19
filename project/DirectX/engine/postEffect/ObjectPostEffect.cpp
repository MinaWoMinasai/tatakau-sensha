#include "ObjectPostEffect.h"
#include <cassert>

void ObjectPostEffect::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, RtvManager* rtvManager) {
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

    objectRT_ = std::make_unique<RenderTexture>();
    objectRT_->Initialize(dxCommon_, srvManager_, rtvManager_, WinApp::kClientWidth, WinApp::kClientHeight, transparent);

    bloomRT_A_ = std::make_unique<RenderTexture>();
    bloomRT_A_->Initialize(dxCommon_, srvManager_, rtvManager_, WinApp::kClientWidth / 4, WinApp::kClientHeight / 4, transparent, false);

    bloomRT_B_ = std::make_unique<RenderTexture>();
    bloomRT_B_->Initialize(dxCommon_, srvManager_, rtvManager_, WinApp::kClientWidth / 4, WinApp::kClientHeight / 4, transparent, false);

    bloomRT_Half_ = std::make_unique<RenderTexture>();
    bloomRT_Half_->Initialize(dxCommon_, srvManager_, rtvManager_, WinApp::kClientWidth / 2, WinApp::kClientHeight / 2, transparent, false);

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
    param_.gaussianIntensity = 0.0f;
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

    dxCommon_->SetRenderTarget(objectRT_->GetRTVHandle(), objectRT_->GetDSVHandle());
    ClearTransparent(objectRT_->GetRTVHandle());
    dxCommon_->GetList()->ClearDepthStencilView(objectRT_->GetDSVHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    dxCommon_->SetViewport(WinApp::kClientWidth, WinApp::kClientHeight);
}

void ObjectPostEffect::EndCapture() {
    Transition(objectRT_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    Transition(bloomRT_Half_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_Half_->GetRTVHandle());
    dxCommon_->SetViewport(WinApp::kClientWidth / 2, WinApp::kClientHeight / 2);
    ClearTransparent(bloomRT_Half_->GetRTVHandle());
    postEffect_->Draw(objectRT_->GetGPUHandle(), kAdd_Bloom_Extract);
    Transition(bloomRT_Half_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_A_->GetRTVHandle());
    dxCommon_->SetViewport(WinApp::kClientWidth / 4, WinApp::kClientHeight / 4);
    ClearTransparent(bloomRT_A_->GetRTVHandle());
    postEffect_->Draw(bloomRT_Half_->GetGPUHandle(), kAdd_Bloom_Downsample);
    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    Transition(bloomRT_B_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_B_->GetRTVHandle());
    ClearTransparent(bloomRT_B_->GetRTVHandle());
    postEffect_->Draw(bloomRT_A_->GetGPUHandle(), kAdd_Bloom_BlurH);
    Transition(bloomRT_B_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_A_->GetRTVHandle());
    ClearTransparent(bloomRT_A_->GetRTVHandle());
    postEffect_->Draw(bloomRT_B_->GetGPUHandle(), kAdd_Bloom_BlurV);
    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    if (restoreHasDsv_) {
        dxCommon_->SetRenderTarget(restoreRtvHandle_, restoreDsvHandle_);
    } else {
        dxCommon_->SetRenderTargetNoDepth(restoreRtvHandle_);
    }
    dxCommon_->SetViewport(WinApp::kClientWidth, WinApp::kClientHeight);
    postEffect_->DrawObjectComposite(objectRT_->GetGPUHandle(), bloomRT_A_->GetGPUHandle());
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
