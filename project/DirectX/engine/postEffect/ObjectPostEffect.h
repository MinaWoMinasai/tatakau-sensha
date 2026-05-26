#pragma once
#include "BloomConstantBuffer.h"
#include "PostEffect.h"
#include "RenderTexture.h"
#include "RtvManager.h"
#include <memory>

class ObjectPostEffect {
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, RtvManager* rtvManager, float renderScale = 1.0f);
    void Update(float deltaTime = 1.0f / 60.0f);

    void BeginCapture();
    void EndCapture();
    void EndCaptureAdditiveOnly();
    void EndCaptureBloomOnly();
    void EndCaptureBloomOnlyToCache();
    void DrawCachedBloom(const Vector2& uvOffset);

    BloomParam& GetParam() { return param_; }
    const BloomParam& GetParam() const { return param_; }
    void SetParam(const BloomParam& param);

private:
    enum class FinishMode {
        CompositeAndAdd,
        AdditiveOnly,
        BloomOnly,
        BloomOnlyCache,
    };
    void FinishCapture(FinishMode mode);
    void Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
    void ClearTransparent(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle);

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    RtvManager* rtvManager_ = nullptr;

    std::unique_ptr<RenderTexture> objectRT_;
    std::unique_ptr<RenderTexture> bloomRT_A_;
    std::unique_ptr<RenderTexture> bloomRT_B_;
    std::unique_ptr<RenderTexture> bloomRT_Half_;

    std::unique_ptr<PostEffect> postEffect_;
    std::unique_ptr<BloomConstantBuffer> cb_;
    std::unique_ptr<RtvManager> ownedRtvManager_;
    BloomParam param_{};
    float timer_ = 0.0f;
    uint32_t renderWidth_ = WinApp::kClientWidth;
    uint32_t renderHeight_ = WinApp::kClientHeight;
    uint32_t halfWidth_ = WinApp::kClientWidth / 2;
    uint32_t halfHeight_ = WinApp::kClientHeight / 2;
    uint32_t quarterWidth_ = WinApp::kClientWidth / 4;
    uint32_t quarterHeight_ = WinApp::kClientHeight / 4;
    D3D12_CPU_DESCRIPTOR_HANDLE restoreRtvHandle_{};
    D3D12_CPU_DESCRIPTOR_HANDLE restoreDsvHandle_{};
    bool restoreHasDsv_ = false;
};
