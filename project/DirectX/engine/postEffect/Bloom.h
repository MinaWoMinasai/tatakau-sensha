#pragma once
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "RenderTexture.h"
#include "Struct.h"
#include "PostEffect.h"
#include "BloomConstantBuffer.h" 
#include "RtvManager.h"
#include "SceneManager.h"
#include <cstdint>

namespace cg2 {

class Bloom {
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, RtvManager* rtvManager);
    void Update();   // ImGuiと定数バッファ更新
    void PreDraw();  // 1. SceneRTをセット
    void PostDraw(); // 2. 抽出・ぼかし・合成を実行
    void SetGrayscaleEnabled(bool enabled);
    void SetGaussianOverride(float intensity);
    void SetTransientPulse(float bloomBoost, float chromAbAmount, const Vector2& center,
        float radius, float width, float strength);
	void SetScreenEffectState(const IScene::ScreenEffectState& state);

private:
    // 便利関数：リソースバリアの切り替え
    void Transition(ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
    void UpdateFrameCameraParameters(bool advanceMotionHistory);
    void ApplyTemporalJitterToCameras();
    void ResetTemporalHistory();
	void ComposeTransientEffects();
	void CaptureScreenEffectBase();

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    RtvManager* rtvManager_ = nullptr;

    // 各種レンダーターゲット（クラス化したRenderTexture等があると仮定）
    std::unique_ptr<RenderTexture> sceneRT_;
    std::unique_ptr<RenderTexture> normalRT_;
    std::unique_ptr<RenderTexture> materialRT_;
    std::unique_ptr<RenderTexture> bloomRT_Half_;
    std::unique_ptr<RenderTexture> bloomRT_A_;
    std::unique_ptr<RenderTexture> bloomRT_B_;
    std::unique_ptr<RenderTexture> randomRT_;
    std::unique_ptr<RenderTexture> ssaoResolveRT_;
    std::unique_ptr<RenderTexture> ssaoDenoiseRT_;
    std::unique_ptr<RenderTexture> ssrResolveRT_;
    std::unique_ptr<RenderTexture> ssrDenoiseRT_;
    std::unique_ptr<RenderTexture> motionVectorRT_;
    std::unique_ptr<RenderTexture> temporalHistoryRT_[2];

    // ポストエフェクト実行クラス
    std::unique_ptr<PostEffect> postEffect_;

    // パラメータと定数バッファ
    BloomParam bloomParam_;
    std::unique_ptr<BloomConstantBuffer> bloomCB_;
    float timer_ = 0.0f;
    bool manualGrayscale_ = false;
    bool forceGrayscale_ = false;
    int fullScreenSmoothingMode_ = 0; // 0: Off, 1: Gaussian, 2: 5x5 Box
    float baseGaussianIntensity_ = 0.0f;
    float baseFullScreenBoxBlurBlend_ = 0.0f;
    float gaussianOverrideIntensity_ = 0.0f;
    float baseBloomIntensity_ = 0.0f;
    float baseDistortionAmount_ = 0.0f;
    float baseChromAbAmount_ = 0.0f;
    float transientBloomBoost_ = 0.0f;
    float transientChromAbAmount_ = 0.0f;
	IScene::PostEffectPulse transientPulse_{};
	IScene::ScreenEffectState screenEffectState_{};
	BloomParam screenEffectBaseParam_{};
    bool enableDepthOutline_ = true;
    bool enableDepthFog_ = false;
    bool enableSSAO_ = true;
    bool enableSSAODenoise_ = true;
    bool enableSSR_ = true;
    bool enableSSRDenoise_ = true;
    bool enableMotionVector_ = true;
    bool enableTemporalAccumulation_ = true;
    bool enableTemporalJitter_ = true;
    bool hasTemporalHistory_ = false;
    int temporalHistoryIndex_ = 0;
    uint32_t temporalFrameIndex_ = 0;
    Vector2 temporalJitter_ = { 0.0f, 0.0f };
    Vector2 previousTemporalJitter_ = { 0.0f, 0.0f };
    float temporalJitterScale_ = 1.0f;
    bool hasPreviousMotionViewProjection_ = false;
    bool previousMotionUsedDebugCamera_ = false;
    Matrix4x4 previousMotionViewProjection_ = MakeIdentity4x4();
    float motionMatrixDelta_ = 0.0f;
    int renderDebugMode_ = 0;
};

} // namespace cg2
