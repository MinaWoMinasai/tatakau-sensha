#include "Bloom.h"
#include "Object3dCommon.h"
#include <algorithm>
#include <cmath>

namespace cg2 {

namespace {

/// @brief 指定基数のHalton列を求め、射影ジッターのサンプルに使う。
float Halton(uint32_t index, uint32_t base) {
    float result = 0.0f;
    float fraction = 1.0f / static_cast<float>(base);

    while (index > 0) {
        result += fraction * static_cast<float>(index % base);
        index /= base;
        fraction /= static_cast<float>(base);
    }

    return result;
}

}

void Bloom::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, RtvManager* rtvManager) {

    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    rtvManager_ = rtvManager;
    bloomParam_ = {};

    sceneRT_ = std::make_unique<RenderTexture>();
    const DXGI_FORMAT hdrFormat = DirectXCommon::kSceneRenderTargetFormat;

    sceneRT_->Initialize(
        dxCommon_,
        srvManager_,
        rtvManager_,
        WinApp::kClientWidth,
        WinApp::kClientHeight,
        { 0.0f, 0.0f, 0.0f, 1.0f },
        true,
        hdrFormat
    );

    normalRT_ = std::make_unique<RenderTexture>();
    normalRT_->Initialize(
        dxCommon_,
        srvManager_,
        rtvManager_,
        WinApp::kClientWidth,
        WinApp::kClientHeight,
        { 0.5f, 0.5f, 1.0f, 0.0f },
        false,
        DirectXCommon::kNormalBufferFormat
    );

    materialRT_ = std::make_unique<RenderTexture>();
    materialRT_->Initialize(
        dxCommon_,
        srvManager_,
        rtvManager_,
        WinApp::kClientWidth,
        WinApp::kClientHeight,
        { 1.0f, 0.0f, 1.0f, 0.0f },
        false,
        DirectXCommon::kMaterialBufferFormat
    );

    // bloom用CBVの生成
    bloomCB_ = std::make_unique<BloomConstantBuffer>();
    bloomCB_->Initialize(dxCommon_);

    // ポストエフェクトの初期化
    postEffect_ = std::make_unique<PostEffect>();
    postEffect_->Initialize(dxCommon_, bloomCB_.get());

    bloomRT_A_ = std::make_unique<RenderTexture>();
    bloomRT_B_ = std::make_unique<RenderTexture>();
    uint32_t bloomWidth = WinApp::kClientWidth / 2;
    uint32_t bloomHeight = WinApp::kClientHeight / 2;

    bloomRT_A_->Initialize(
        dxCommon_,
        srvManager_,
        rtvManager_,
        bloomWidth,
        bloomHeight,
        { 0.0f, 0.0f, 0.0f, 1.0f },
        false,
        hdrFormat
    );

    bloomRT_B_->Initialize(
        dxCommon_,
        srvManager_,
        rtvManager_,
        bloomWidth,
        bloomHeight,
        { 0.0f, 0.0f, 0.0f, 1.0f },
        false,
        hdrFormat
    );

    // bloomRT_Half を追加
    bloomRT_Half_ = std::make_unique<RenderTexture>();
    // サイズは画面の半分
    bloomRT_Half_->Initialize(
        dxCommon_,
        srvManager_,
        rtvManager_,
        WinApp::kClientWidth / 2,
        WinApp::kClientHeight / 2,
        { 0.0f, 0.0f, 0.0f, 1.0f },
        false,
        hdrFormat
    );

	// Random課題用の独立した全画面パス。入力画像へGPU乱数を乗算する。
	randomRT_ = std::make_unique<RenderTexture>();
	randomRT_->Initialize(
		dxCommon_, srvManager_, rtvManager_,
		WinApp::kClientWidth, WinApp::kClientHeight,
		{ 0.0f, 0.0f, 0.0f, 1.0f }, false, hdrFormat);

    ssaoResolveRT_ = std::make_unique<RenderTexture>();
    ssaoResolveRT_->Initialize(
        dxCommon_, srvManager_, rtvManager_,
        WinApp::kClientWidth, WinApp::kClientHeight,
        { 1.0f, 1.0f, 1.0f, 1.0f }, false, DirectXCommon::kAmbientOcclusionBufferFormat);

    ssaoDenoiseRT_ = std::make_unique<RenderTexture>();
    ssaoDenoiseRT_->Initialize(
        dxCommon_, srvManager_, rtvManager_,
        WinApp::kClientWidth, WinApp::kClientHeight,
        { 1.0f, 1.0f, 1.0f, 1.0f }, false, DirectXCommon::kAmbientOcclusionBufferFormat);

    ssrResolveRT_ = std::make_unique<RenderTexture>();
    ssrResolveRT_->Initialize(
        dxCommon_, srvManager_, rtvManager_,
        WinApp::kClientWidth, WinApp::kClientHeight,
        { 0.0f, 0.0f, 0.0f, 0.0f }, false, hdrFormat);

    ssrDenoiseRT_ = std::make_unique<RenderTexture>();
    ssrDenoiseRT_->Initialize(
        dxCommon_, srvManager_, rtvManager_,
        WinApp::kClientWidth, WinApp::kClientHeight,
        { 0.0f, 0.0f, 0.0f, 0.0f }, false, hdrFormat);

    motionVectorRT_ = std::make_unique<RenderTexture>();
    motionVectorRT_->Initialize(
        dxCommon_, srvManager_, rtvManager_,
        WinApp::kClientWidth, WinApp::kClientHeight,
        { 0.0f, 0.0f, 0.0f, 0.0f }, false, DirectXCommon::kMotionVectorBufferFormat);

    for (auto& temporalRT : temporalHistoryRT_) {
        temporalRT = std::make_unique<RenderTexture>();
        temporalRT->Initialize(
            dxCommon_, srvManager_, rtvManager_,
            WinApp::kClientWidth, WinApp::kClientHeight,
            { 0.0f, 0.0f, 0.0f, 1.0f }, false, hdrFormat);
    }

    // ブルームパラメータ
    bloomParam_.threshold = 1.0f;
    bloomParam_.intensity = 0.35f;
    bloomParam_.vignetteIntensity = 0.0f;
    bloomParam_.vignetteScale = 0.0f;
    bloomParam_.chromAbAmount = 0.0f;
    bloomParam_.distortionAmount = 0.0f;
    bloomParam_.noiseIntensity = 0.0f;
    bloomParam_.scanlineIntensity = 0.0f;
    bloomParam_.scanlineFrequency = 0.0f;
    bloomParam_.curvature = 0.0f;
    bloomParam_.borderSharp = 0.0f;
    bloomParam_.glitchAmount = 0.00f;
    bloomParam_.gaussianIntensity = 0.0f;
    bloomParam_.boxBlurIntensity = 0.0f;
    bloomParam_.boxBlurRadius = 1.0f;
    bloomParam_.fullScreenBoxBlurBlend = 0.0f;
    bloomParam_.depthOutlineEnabled = 1.0f;
    bloomParam_.depthNearClip = 0.1f;
    bloomParam_.depthFarClip = 5000.0f;
    bloomParam_.depthOutlineScale = 1.0f;
    bloomParam_.shockwaveCenter = { 0.5f, 0.5f };
    bloomParam_.shockwaveRadius = 0.0f;
    bloomParam_.shockwaveWidth = 0.05f;
    bloomParam_.shockwaveStrength = 0.0f;
	bloomParam_.radialBlurCenter = { 0.5f, 0.5f };
	bloomParam_.radialBlurWidth = 0.01f;
	bloomParam_.radialBlurIntensity = 0.0f;
	bloomParam_.dissolveEdgeColor = { 1.0f, 0.4f, 0.3f };
	bloomParam_.dissolveEdgeWidth = 0.03f;
	bloomParam_.dissolveNoiseScale = 100.0f;
	bloomParam_.dissolveNoiseSpeed = 0.0f;
	bloomParam_.randomIntensity = 0.0f;
	bloomParam_.randomScale = 160.0f;
	bloomParam_.randomTimeScale = 8.0f;
	bloomParam_.randomGrayscalePreview = 0.0f;
    bloomParam_.exposure = 1.0f;
    bloomParam_.toneMappingMode = 1.0f;
    bloomParam_.hdrWhitePoint = 11.2f;
    bloomParam_.renderDebugMode = 0.0f;
    bloomParam_.linearDepthDebugRange = 800.0f;
    bloomParam_.depthNormalScale = 0.035f;
    bloomParam_.depthFogEnabled = 0.0f;
    bloomParam_.depthFogColor = { 0.62f, 0.78f, 0.84f };
    bloomParam_.depthFogStart = 180.0f;
    bloomParam_.depthFogEnd = 1400.0f;
    bloomParam_.depthFogDensity = 0.0012f;
    bloomParam_.depthFogMaxOpacity = 0.55f;
    bloomParam_.ssaoEnabled = 1.0f;
    bloomParam_.ssaoRadius = 18.0f;
    bloomParam_.ssaoIntensity = 0.55f;
    bloomParam_.ssaoBias = 0.08f;
    bloomParam_.ssaoPower = 1.25f;
    bloomParam_.ssaoSampleCount = 12.0f;
    bloomParam_.ssaoNormalInfluence = 0.65f;
    bloomParam_.ssaoDistanceFalloff = 22.0f;
    bloomParam_.ssaoDenoiseEnabled = 1.0f;
    bloomParam_.ssaoDenoiseRadius = 2.0f;
    bloomParam_.ssaoDenoiseDepthSigma = 24.0f;
    bloomParam_.ssaoDenoiseNormalSigma = 18.0f;
    bloomParam_.ssrViewMatrix = MakeIdentity4x4();
    const float aspectRatio = static_cast<float>(WinApp::kClientWidth) / static_cast<float>(WinApp::kClientHeight);
    const float tanHalfFovY = std::tan(0.45f * 0.5f);
    bloomParam_.ssrProjectionScale = { aspectRatio * tanHalfFovY, tanHalfFovY };
    bloomParam_.ssrEnabled = 1.0f;
    bloomParam_.ssrIntensity = 0.32f;
    bloomParam_.ssrMaxDistance = 420.0f;
    bloomParam_.ssrThickness = 2.5f;
    bloomParam_.ssrStepCount = 28.0f;
    bloomParam_.ssrStride = 1.0f;
    bloomParam_.ssrFresnelPower = 2.2f;
    bloomParam_.ssrEdgeFade = 0.075f;
    bloomParam_.ssrDepthFade = 1800.0f;
    bloomParam_.ssrNormalFade = 0.85f;
    bloomParam_.ssrMaskPower = 1.0f;
    bloomParam_.ssrBlurRadius = 3.5f;
    bloomParam_.ssrDenoiseEnabled = 1.0f;
    bloomParam_.ssrDenoiseRadius = 2.0f;
    bloomParam_.ssrDenoiseDepthSigma = 28.0f;
    bloomParam_.ssrDenoiseNormalSigma = 20.0f;
    bloomParam_.motionMatrixPadding[0] = 0.0f;
    bloomParam_.motionMatrixPadding[1] = 0.0f;
    bloomParam_.motionCurrentViewProjection = MakeIdentity4x4();
    bloomParam_.motionPreviousViewProjection = MakeIdentity4x4();
    bloomParam_.motionInverseCurrentViewProjection = MakeIdentity4x4();
    bloomParam_.motionVectorEnabled = 1.0f;
    bloomParam_.motionVectorScale = 1.0f;
    bloomParam_.motionVectorDebugScale = 80.0f;
    bloomParam_.temporalEnabled = 1.0f;
    bloomParam_.temporalHistoryValid = 0.0f;
    bloomParam_.temporalBlendFactor = 0.88f;
    bloomParam_.temporalMotionRejection = 24.0f;
    bloomParam_.temporalJitter = { 0.0f, 0.0f };
    bloomParam_.temporalPreviousJitter = { 0.0f, 0.0f };
    bloomParam_.temporalJitterEnabled = 1.0f;
    bloomParam_.temporalJitterScale = temporalJitterScale_;
    bloomParam_.temporalJitterPadding[0] = 0.0f;
    bloomParam_.temporalJitterPadding[1] = 0.0f;
    bloomParam_.waterDiagnosticsEnabled = 0.0f;
    bloomParam_.waterTaaEnabled = 1.0f;
    bloomParam_.waterBloomEnabled = 1.0f;
    bloomParam_.waterHistoryWeight = 0.08f;
    bloomParam_.waterDebugMode = 0.0f;
    bloomParam_.waterPostPadding[0] = 0.0f;
    bloomParam_.waterPostPadding[1] = 0.0f;
    bloomParam_.waterPostPadding[2] = 0.0f;


    baseGaussianIntensity_ = bloomParam_.gaussianIntensity;
    baseFullScreenBoxBlurBlend_ = bloomParam_.fullScreenBoxBlurBlend;
    baseBloomIntensity_ = bloomParam_.intensity;
    baseDistortionAmount_ = bloomParam_.distortionAmount;
    baseChromAbAmount_ = bloomParam_.chromAbAmount;
	CaptureScreenEffectBase();

    bloomCB_->Update(bloomParam_);

}

void Bloom::Update() {

#ifdef USE_IMGUI

	if (!screenEffectState_.suppressPostEffectDebugUi) {
		ImGui::Begin("BloomAndVignette");

    // --- 既存の項目 ---
    ImGui::Text("HDR Output");
    ImGui::DragFloat("Exposure", &bloomParam_.exposure, 0.01f, 0.01f, 10.0f);
    bool useAcesToneMapping = bloomParam_.toneMappingMode > 0.5f;
    if (ImGui::Checkbox("ACES Tone Mapping", &useAcesToneMapping)) {
        bloomParam_.toneMappingMode = useAcesToneMapping ? 1.0f : 0.0f;
    }
    ImGui::DragFloat("HDR White Point", &bloomParam_.hdrWhitePoint, 0.1f, 1.0f, 32.0f);
    ImGui::DragFloat("Threshold", &bloomParam_.threshold, 0.01f, 0.0f, 20.0f);
    ImGui::DragFloat("Intensity", &baseBloomIntensity_, 0.01f);
    ImGui::DragFloat("Vignette Intensity", &bloomParam_.vignetteIntensity, 0.01f);
    ImGui::DragFloat("Vignette Scale", &bloomParam_.vignetteScale, 0.01f);
    ImGui::DragFloat("Distortion Amount", &baseDistortionAmount_, 0.001f);
    ImGui::DragFloat("ChromAb Amount", &baseChromAbAmount_, 0.001f);
    ImGui::DragFloat("Noise", &bloomParam_.noiseIntensity, 0.01f);
    ImGui::DragFloat("Scanline Intensity", &bloomParam_.scanlineIntensity, 0.01f);
    ImGui::DragFloat("Scanline Frequency", &bloomParam_.scanlineFrequency, 0.01f);
    ImGui::DragFloat("Curvature", &bloomParam_.curvature, 0.001f);
    ImGui::DragFloat("Border Sharp", &bloomParam_.borderSharp, 0.1f);
    ImGui::DragFloat("Glitch Amount", &bloomParam_.glitchAmount, 0.001f);
    const char* smoothingModes[] = { "Off", "Gaussian Blur", "5x5 Box Filter" };
    ImGui::Combo("Full-Screen Smoothing Filter", &fullScreenSmoothingMode_, smoothingModes, IM_ARRAYSIZE(smoothingModes));
    ImGui::DragFloat("Gaussian Blur Blend", &baseGaussianIntensity_, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("5x5 Box Filter Blend", &baseFullScreenBoxBlurBlend_, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Final 3x3 Box Smoothing Blend", &bloomParam_.boxBlurIntensity, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Final 3x3 Box Smoothing Radius", &bloomParam_.boxBlurRadius, 0.1f, 0.0f, 8.0f);

    ImGui::Separator(); // 区切り線
    ImGui::Text("New Effects");

	// --- Radial Blur（放射状ブラー） ---
	ImGui::Text("Radial Blur");
	ImGui::DragFloat2("Radial Center", &bloomParam_.radialBlurCenter.x, 0.005f, 0.0f, 1.0f);
	ImGui::DragFloat("Radial Width", &bloomParam_.radialBlurWidth, 0.001f, 0.0f, 0.1f);
	ImGui::DragFloat("Radial Intensity", &bloomParam_.radialBlurIntensity, 0.01f, 0.0f, 1.0f);

    // --- Dissolve (ディゾルブ) ---
    ImGui::DragFloat("Dissolve Progress", &bloomParam_.dissolveThreshold, 0.01f, 0.0f, 1.0f);
	ImGui::ColorEdit3("Dissolve Edge Color", &bloomParam_.dissolveEdgeColor.x);
	ImGui::DragFloat("Dissolve Edge Width", &bloomParam_.dissolveEdgeWidth, 0.001f, 0.0f, 0.25f);
	ImGui::DragFloat("Dissolve Noise Scale", &bloomParam_.dissolveNoiseScale, 1.0f, 1.0f, 400.0f);
	ImGui::DragFloat("Dissolve Noise Speed", &bloomParam_.dissolveNoiseSpeed, 0.01f, 0.0f, 10.0f);

	ImGui::Separator();
	ImGui::Text("Random GPU Noise (CG5_00_09)");
	bool randomPreview = bloomParam_.randomGrayscalePreview > 0.5f;
	if (ImGui::Checkbox("Random Grayscale Preview", &randomPreview)) {
		bloomParam_.randomGrayscalePreview = randomPreview ? 1.0f : 0.0f;
	}
	ImGui::DragFloat("Random Multiply Intensity", &bloomParam_.randomIntensity, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("Random Scale", &bloomParam_.randomScale, 1.0f, 1.0f, 1000.0f);
	ImGui::DragFloat("Random Time Seed Speed", &bloomParam_.randomTimeScale, 0.1f, 0.0f, 60.0f);

    // --- Outline (アウトライン) ---
    ImGui::DragFloat("Outline Width", &bloomParam_.outlineWidth, 0.1f, 0.0f, 10.0f);
    ImGui::DragFloat("Outline Threshold", &bloomParam_.outlineThreshold, 0.01f, 0.0f, 2.0f);
    ImGui::Checkbox("Depth Outline", &enableDepthOutline_);
    ImGui::DragFloat("Depth Outline Scale", &bloomParam_.depthOutlineScale, 0.01f, 0.01f, 10.0f);
    // Vector3がfloat[3]として解釈されるようにポインタを渡す
    ImGui::ColorEdit3("Outline Color", &bloomParam_.outlineColor.x);

    ImGui::Separator();
    ImGui::Text("Render Target Debug");
    const char* renderDebugModes[] = {
        "Final",
        "Scene Color",
        "Bloom Only",
        "Linear Depth",
        "Depth Edge",
        "Depth-derived Normal",
        "Normal Buffer",
        "SSAO",
        "SSR",
        "SSR Mask",
        "Material Roughness",
        "Material Metallic",
        "Material AO",
        "Material Class",
        "Motion Vector",
        "Motion Diagnostic",
        "Temporal Source"
    };
    ImGui::Combo("Debug View", &renderDebugMode_, renderDebugModes, IM_ARRAYSIZE(renderDebugModes));
    ImGui::DragFloat("Linear Depth Range", &bloomParam_.linearDepthDebugRange, 10.0f, 1.0f, 5000.0f);
    ImGui::DragFloat("Depth Normal Scale", &bloomParam_.depthNormalScale, 0.001f, 0.001f, 0.25f);

    ImGui::Separator();
    ImGui::Text("SSAO");
    ImGui::Checkbox("Enable SSAO", &enableSSAO_);
    ImGui::DragFloat("SSAO Radius", &bloomParam_.ssaoRadius, 0.5f, 1.0f, 96.0f);
    ImGui::DragFloat("SSAO Intensity", &bloomParam_.ssaoIntensity, 0.01f, 0.0f, 3.0f);
    ImGui::DragFloat("SSAO Bias", &bloomParam_.ssaoBias, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("SSAO Power", &bloomParam_.ssaoPower, 0.01f, 0.1f, 4.0f);
    ImGui::DragFloat("SSAO Samples", &bloomParam_.ssaoSampleCount, 1.0f, 1.0f, 16.0f);
    ImGui::DragFloat("SSAO Normal Influence", &bloomParam_.ssaoNormalInfluence, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("SSAO Distance Falloff", &bloomParam_.ssaoDistanceFalloff, 0.5f, 1.0f, 200.0f);
    ImGui::Checkbox("Enable SSAO Denoise", &enableSSAODenoise_);
    ImGui::DragFloat("SSAO Denoise Radius", &bloomParam_.ssaoDenoiseRadius, 0.1f, 0.0f, 8.0f);
    ImGui::DragFloat("SSAO Denoise Depth Sigma", &bloomParam_.ssaoDenoiseDepthSigma, 1.0f, 1.0f, 200.0f);
    ImGui::DragFloat("SSAO Denoise Normal Sigma", &bloomParam_.ssaoDenoiseNormalSigma, 0.5f, 0.0f, 80.0f);

    ImGui::Separator();
    ImGui::Text("SSR");
    ImGui::Checkbox("Enable SSR", &enableSSR_);
    ImGui::DragFloat("SSR Intensity", &bloomParam_.ssrIntensity, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("SSR Max Distance", &bloomParam_.ssrMaxDistance, 5.0f, 1.0f, 2500.0f);
    ImGui::DragFloat("SSR Thickness", &bloomParam_.ssrThickness, 0.1f, 0.01f, 40.0f);
    ImGui::DragFloat("SSR Steps", &bloomParam_.ssrStepCount, 1.0f, 1.0f, 48.0f);
    ImGui::DragFloat("SSR Stride", &bloomParam_.ssrStride, 0.01f, 0.1f, 4.0f);
    ImGui::DragFloat("SSR Fresnel Power", &bloomParam_.ssrFresnelPower, 0.01f, 0.1f, 8.0f);
    ImGui::DragFloat("SSR Edge Fade", &bloomParam_.ssrEdgeFade, 0.001f, 0.001f, 0.3f);
    ImGui::DragFloat("SSR Depth Fade", &bloomParam_.ssrDepthFade, 10.0f, 10.0f, 5000.0f);
    ImGui::DragFloat("SSR Normal Fade", &bloomParam_.ssrNormalFade, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("SSR Mask Power", &bloomParam_.ssrMaskPower, 0.01f, 0.1f, 4.0f);
    ImGui::DragFloat("SSR Blur Radius", &bloomParam_.ssrBlurRadius, 0.1f, 0.0f, 16.0f);
    ImGui::Checkbox("Enable SSR Denoise", &enableSSRDenoise_);
    ImGui::DragFloat("SSR Denoise Radius", &bloomParam_.ssrDenoiseRadius, 0.1f, 0.0f, 8.0f);
    ImGui::DragFloat("SSR Denoise Depth Sigma", &bloomParam_.ssrDenoiseDepthSigma, 1.0f, 1.0f, 200.0f);
    ImGui::DragFloat("SSR Denoise Normal Sigma", &bloomParam_.ssrDenoiseNormalSigma, 0.5f, 0.0f, 80.0f);

    ImGui::Separator();
    ImGui::Text("Motion Vector");
    ImGui::Checkbox("Enable Motion Vector", &enableMotionVector_);
    ImGui::DragFloat("Motion Vector Scale", &bloomParam_.motionVectorScale, 0.01f, -1.0f, 8.0f);
    ImGui::DragFloat("Motion Vector Debug Scale", &bloomParam_.motionVectorDebugScale, 1.0f, 1.0f, 500.0f);
    ImGui::Text("Motion matrix delta: %.6f", motionMatrixDelta_);
    ImGui::Text("Set Motion Vector Scale to -1.0 or choose Motion Diagnostic to test the RT path.");

    ImGui::Separator();
    ImGui::Text("Temporal");
    if (ImGui::Checkbox("Enable Temporal Accumulation", &enableTemporalAccumulation_)) {
        ResetTemporalHistory();
    }
    if (ImGui::Checkbox("Enable TAA Camera Jitter", &enableTemporalJitter_)) {
        ResetTemporalHistory();
    }
    ImGui::DragFloat("Temporal History Blend", &bloomParam_.temporalBlendFactor, 0.01f, 0.0f, 0.98f);
    ImGui::DragFloat("Temporal Motion Rejection", &bloomParam_.temporalMotionRejection, 0.5f, 0.0f, 96.0f);
    if (ImGui::DragFloat("Temporal Jitter Scale", &temporalJitterScale_, 0.01f, 0.0f, 2.0f)) {
        ResetTemporalHistory();
    }
    ImGui::Text("TAA jitter: %.6f, %.6f", temporalJitter_.x, temporalJitter_.y);
    ImGui::Text("Temporal history: %s", hasTemporalHistory_ ? "valid" : "reset");
    if (ImGui::Button("Reset Temporal History")) {
        ResetTemporalHistory();
    }

    ImGui::Separator();
    ImGui::Text("Depth Fog");
    ImGui::Checkbox("Enable Depth Fog", &enableDepthFog_);
    ImGui::ColorEdit3("Depth Fog Color", &bloomParam_.depthFogColor.x);
    ImGui::DragFloat("Depth Fog Start", &bloomParam_.depthFogStart, 5.0f, 0.0f, 5000.0f);
    ImGui::DragFloat("Depth Fog End", &bloomParam_.depthFogEnd, 5.0f, 1.0f, 5000.0f);
    ImGui::DragFloat("Depth Fog Density", &bloomParam_.depthFogDensity, 0.0001f, 0.0f, 0.02f, "%.4f");
    ImGui::DragFloat("Depth Fog Max Opacity", &bloomParam_.depthFogMaxOpacity, 0.01f, 0.0f, 1.0f);

    ImGui::Separator();

    static bool invertFlag = false;
    if (ImGui::Checkbox("Grayscale", &manualGrayscale_)) {
        bloomParam_.isGrayscale = (manualGrayscale_ || forceGrayscale_) ? 1.0f : 0.0f;
    }
    if (ImGui::Checkbox("Invert Color", &invertFlag)) {
        bloomParam_.isInverted = invertFlag ? 1.0f : 0.0f;
    }

    // リセットボタン
    if (ImGui::Button("Reset")) {
        bloomParam_.threshold = 1.0f;
        baseBloomIntensity_ = 0.35f;
        bloomParam_.exposure = 1.0f;
        bloomParam_.toneMappingMode = 1.0f;
        bloomParam_.hdrWhitePoint = 11.2f;
        renderDebugMode_ = 0;
        bloomParam_.linearDepthDebugRange = 800.0f;
        bloomParam_.depthNormalScale = 0.035f;
        enableDepthFog_ = false;
        bloomParam_.depthFogColor = { 0.62f, 0.78f, 0.84f };
        bloomParam_.depthFogStart = 180.0f;
        bloomParam_.depthFogEnd = 1400.0f;
        bloomParam_.depthFogDensity = 0.0012f;
        bloomParam_.depthFogMaxOpacity = 0.55f;
        enableSSAO_ = true;
        bloomParam_.ssaoRadius = 18.0f;
        bloomParam_.ssaoIntensity = 0.55f;
        bloomParam_.ssaoBias = 0.08f;
        bloomParam_.ssaoPower = 1.25f;
        bloomParam_.ssaoSampleCount = 12.0f;
        bloomParam_.ssaoNormalInfluence = 0.65f;
        bloomParam_.ssaoDistanceFalloff = 22.0f;
        enableSSAODenoise_ = true;
        bloomParam_.ssaoDenoiseRadius = 2.0f;
        bloomParam_.ssaoDenoiseDepthSigma = 24.0f;
        bloomParam_.ssaoDenoiseNormalSigma = 18.0f;
        enableSSR_ = true;
        bloomParam_.ssrIntensity = 0.32f;
        bloomParam_.ssrMaxDistance = 420.0f;
        bloomParam_.ssrThickness = 2.5f;
        bloomParam_.ssrStepCount = 28.0f;
        bloomParam_.ssrStride = 1.0f;
        bloomParam_.ssrFresnelPower = 2.2f;
        bloomParam_.ssrEdgeFade = 0.075f;
        bloomParam_.ssrDepthFade = 1800.0f;
        bloomParam_.ssrNormalFade = 0.85f;
        bloomParam_.ssrMaskPower = 1.0f;
        bloomParam_.ssrBlurRadius = 3.5f;
        enableSSRDenoise_ = true;
        bloomParam_.ssrDenoiseRadius = 2.0f;
        bloomParam_.ssrDenoiseDepthSigma = 28.0f;
        bloomParam_.ssrDenoiseNormalSigma = 20.0f;
        bloomParam_.motionMatrixPadding[0] = 0.0f;
        bloomParam_.motionMatrixPadding[1] = 0.0f;
        enableMotionVector_ = true;
        bloomParam_.motionVectorScale = 1.0f;
        bloomParam_.motionVectorDebugScale = 80.0f;
        enableTemporalAccumulation_ = true;
        enableTemporalJitter_ = true;
        temporalJitterScale_ = 1.0f;
        ResetTemporalHistory();
        bloomParam_.temporalBlendFactor = 0.88f;
        bloomParam_.temporalMotionRejection = 24.0f;
        bloomParam_.vignetteIntensity = 0.0f;
        bloomParam_.vignetteScale = 0.0f;
        baseChromAbAmount_ = 0.0f;
        baseDistortionAmount_ = 0.0f;
        bloomParam_.noiseIntensity = 0.0f;
        bloomParam_.scanlineIntensity = 0.0f;
        bloomParam_.scanlineFrequency = 0.0f;
        bloomParam_.curvature = 0.0f;
        bloomParam_.borderSharp = 0.0f;
        bloomParam_.glitchAmount = 0.0f;
        fullScreenSmoothingMode_ = 0;
        baseGaussianIntensity_ = 0.0f;
        baseFullScreenBoxBlurBlend_ = 0.0f;
        bloomParam_.boxBlurIntensity = 0.0f;
        bloomParam_.boxBlurRadius = 1.0f;
        // 追加分のリセット
        bloomParam_.dissolveThreshold = 0.0f;
		bloomParam_.radialBlurCenter = { 0.5f, 0.5f };
		bloomParam_.radialBlurWidth = 0.01f;
		bloomParam_.radialBlurIntensity = 0.0f;
		bloomParam_.dissolveEdgeColor = { 1.0f, 0.4f, 0.3f };
		bloomParam_.dissolveEdgeWidth = 0.03f;
		bloomParam_.dissolveNoiseScale = 100.0f;
		bloomParam_.dissolveNoiseSpeed = 0.0f;
		bloomParam_.randomIntensity = 0.0f;
		bloomParam_.randomScale = 160.0f;
		bloomParam_.randomTimeScale = 8.0f;
		bloomParam_.randomGrayscalePreview = 0.0f;
        bloomParam_.outlineWidth = 0.0f;
        bloomParam_.outlineThreshold = 0.5f;
        bloomParam_.outlineColor = { 1.0f, 1.0f, 1.0f };
        enableDepthOutline_ = true;
        bloomParam_.depthOutlineScale = 1.0f;
        bloomParam_.outlineBloomIntensity = 0.0f;
        bloomParam_.outlineBloomWidth = 6.0f;
        manualGrayscale_ = false;
        invertFlag = false;
    }

		ImGui::End();
	}

#endif // USE_IMGUI

    renderDebugMode_ = (std::clamp)(renderDebugMode_, 0, 16);
    bloomParam_.isGrayscale = (manualGrayscale_ || forceGrayscale_) ? 1.0f : 0.0f;
    bloomParam_.depthOutlineEnabled = enableDepthOutline_ ? 1.0f : 0.0f;
    bloomParam_.depthFogEnabled = enableDepthFog_ ? 1.0f : 0.0f;
    bloomParam_.ssaoEnabled = enableSSAO_ ? 1.0f : 0.0f;
    bloomParam_.ssaoSampleCount = (std::clamp)(bloomParam_.ssaoSampleCount, 1.0f, 16.0f);
    bloomParam_.ssaoRadius = (std::max)(bloomParam_.ssaoRadius, 0.0f);
    bloomParam_.ssaoIntensity = (std::max)(bloomParam_.ssaoIntensity, 0.0f);
    bloomParam_.ssaoDistanceFalloff = (std::max)(bloomParam_.ssaoDistanceFalloff, 0.001f);
    bloomParam_.ssaoDenoiseEnabled = enableSSAODenoise_ ? 1.0f : 0.0f;
    bloomParam_.ssaoDenoiseRadius = (std::max)(bloomParam_.ssaoDenoiseRadius, 0.0f);
    bloomParam_.ssaoDenoiseDepthSigma = (std::max)(bloomParam_.ssaoDenoiseDepthSigma, 0.001f);
    bloomParam_.ssaoDenoiseNormalSigma = (std::max)(bloomParam_.ssaoDenoiseNormalSigma, 0.0f);
    bloomParam_.ssrEnabled = enableSSR_ ? 1.0f : 0.0f;
    bloomParam_.ssrStepCount = (std::clamp)(bloomParam_.ssrStepCount, 1.0f, 48.0f);
    bloomParam_.ssrStride = (std::max)(bloomParam_.ssrStride, 0.1f);
    bloomParam_.ssrThickness = (std::max)(bloomParam_.ssrThickness, 0.01f);
    bloomParam_.ssrMaxDistance = (std::max)(bloomParam_.ssrMaxDistance, 1.0f);
    bloomParam_.ssrMaskPower = (std::max)(bloomParam_.ssrMaskPower, 0.1f);
    bloomParam_.ssrBlurRadius = (std::max)(bloomParam_.ssrBlurRadius, 0.0f);
    bloomParam_.ssrDenoiseEnabled = enableSSRDenoise_ ? 1.0f : 0.0f;
    bloomParam_.ssrDenoiseRadius = (std::max)(bloomParam_.ssrDenoiseRadius, 0.0f);
    bloomParam_.ssrDenoiseDepthSigma = (std::max)(bloomParam_.ssrDenoiseDepthSigma, 0.001f);
    bloomParam_.ssrDenoiseNormalSigma = (std::max)(bloomParam_.ssrDenoiseNormalSigma, 0.0f);
    bloomParam_.motionVectorEnabled = enableMotionVector_ ? 1.0f : 0.0f;
    bloomParam_.motionVectorScale = (std::clamp)(bloomParam_.motionVectorScale, -1.0f, 8.0f);
    bloomParam_.motionVectorDebugScale = (std::max)(bloomParam_.motionVectorDebugScale, 1.0f);
    bloomParam_.temporalEnabled = enableTemporalAccumulation_ ? 1.0f : 0.0f;
    bloomParam_.temporalBlendFactor = (std::clamp)(bloomParam_.temporalBlendFactor, 0.0f, 0.98f);
    bloomParam_.temporalMotionRejection = (std::max)(bloomParam_.temporalMotionRejection, 0.0f);
    temporalJitterScale_ = (std::clamp)(temporalJitterScale_, 0.0f, 2.0f);
    bloomParam_.temporalJitterEnabled = enableTemporalJitter_ ? 1.0f : 0.0f;
    bloomParam_.temporalJitterScale = temporalJitterScale_;
    const IScene::WaterPostProcessSettings waterSettings =
        SceneManager::GetInstance()->GetWaterPostProcessSettings();
    bloomParam_.waterDiagnosticsEnabled = waterSettings.diagnosticsEnabled ? 1.0f : 0.0f;
    bloomParam_.waterTaaEnabled = waterSettings.taaEnabled ? 1.0f : 0.0f;
    bloomParam_.waterBloomEnabled = waterSettings.bloomEnabled ? 1.0f : 0.0f;
    bloomParam_.waterHistoryWeight =
        (std::clamp)(waterSettings.historyWeight, 0.0f, 0.10f);
    bloomParam_.waterDebugMode =
        static_cast<float>((std::clamp)(waterSettings.debugMode, 0, 5));
    if (!enableTemporalAccumulation_) {
        hasTemporalHistory_ = false;
    }
    const float aspectRatio = static_cast<float>(WinApp::kClientWidth) / static_cast<float>(WinApp::kClientHeight);
    const float tanHalfFovY = std::tan(0.45f * 0.5f);
    bloomParam_.ssrProjectionScale = { aspectRatio * tanHalfFovY, tanHalfFovY };
    bloomParam_.renderDebugMode = (enableMotionVector_ && bloomParam_.motionVectorScale < -0.5f)
        ? 15.0f
        : static_cast<float>(renderDebugMode_);
	if (!screenEffectState_.active) {
		CaptureScreenEffectBase();
	}
	ComposeTransientEffects();
    bloomCB_->Update(bloomParam_);
    timer_ += SceneManager::GetInstance()->GetFinalDeltaTime();
    bloomParam_.timer = timer_;
}

void Bloom::UpdateFrameCameraParameters(bool advanceMotionHistory) {
    Object3dCommon* objectCommon = Object3dCommon::GetInstance();
    bloomParam_.ssrViewMatrix = MakeIdentity4x4();

    Matrix4x4 currentMotionViewProjection = MakeIdentity4x4();
    bool usingDebugCamera = false;
    if (developerShowcase_.active && developerShowcase_.camera) {
        Camera* camera = developerShowcase_.camera;
        bloomParam_.ssrViewMatrix = camera->GetViewMatrix();
        currentMotionViewProjection = camera->GetViewProjectionMatrix();
        bloomParam_.depthNearClip = camera->GetNearClip(); bloomParam_.depthFarClip = camera->GetFarClip();
    } else if (objectCommon->GetIsDebugCamera() && objectCommon->GetDebugCamera()) {
        DebugCamera* debugCamera = objectCommon->GetDebugCamera();
        bloomParam_.ssrViewMatrix = debugCamera->GetViewMatrix();
        currentMotionViewProjection = debugCamera->GetViewProjectionMatrix();
        bloomParam_.depthNearClip = debugCamera->GetNearClip();
        bloomParam_.depthFarClip = debugCamera->GetFarClip();
        usingDebugCamera = true;
    } else if (objectCommon->GetDefaultCamera()) {
        Camera* camera = objectCommon->GetDefaultCamera();
        bloomParam_.ssrViewMatrix = camera->GetViewMatrix();
        currentMotionViewProjection = camera->GetViewProjectionMatrix();
        bloomParam_.depthNearClip = camera->GetNearClip();
        bloomParam_.depthFarClip = camera->GetFarClip();
    }

    const bool resetHistory = !hasPreviousMotionViewProjection_ || previousMotionUsedDebugCamera_ != usingDebugCamera;
    if (resetHistory) {
        previousMotionViewProjection_ = currentMotionViewProjection;
        hasPreviousMotionViewProjection_ = true;
        ResetTemporalHistory();
    }

    motionMatrixDelta_ = 0.0f;
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            motionMatrixDelta_ += std::abs(
                currentMotionViewProjection.m[row][column] -
                previousMotionViewProjection_.m[row][column]);
        }
    }

    bloomParam_.motionCurrentViewProjection = currentMotionViewProjection;
    bloomParam_.motionPreviousViewProjection = previousMotionViewProjection_;
    bloomParam_.motionInverseCurrentViewProjection = Inverse(currentMotionViewProjection);

    if (advanceMotionHistory) {
        previousMotionViewProjection_ = currentMotionViewProjection;
        previousMotionUsedDebugCamera_ = usingDebugCamera;
    }
}

void Bloom::ApplyTemporalJitterToCameras() {
    previousTemporalJitter_ = temporalJitter_;

    const bool canUseJitter =
        enableTemporalAccumulation_ &&
        enableTemporalJitter_ &&
        enableMotionVector_ &&
        bloomParam_.motionVectorScale >= 0.0f &&
        renderDebugMode_ != 15;

    if (canUseJitter) {
        const uint32_t sampleIndex = (temporalFrameIndex_ % 8u) + 1u;
        const float jitterX = Halton(sampleIndex, 2u) - 0.5f;
        const float jitterY = Halton(sampleIndex, 3u) - 0.5f;
        temporalJitter_ = {
            (jitterX * 2.0f / static_cast<float>(WinApp::kClientWidth)) * temporalJitterScale_,
            (-jitterY * 2.0f / static_cast<float>(WinApp::kClientHeight)) * temporalJitterScale_
        };
        ++temporalFrameIndex_;
    } else {
        temporalJitter_ = { 0.0f, 0.0f };
        previousTemporalJitter_ = { 0.0f, 0.0f };
    }

    bloomParam_.temporalJitter = temporalJitter_;
    bloomParam_.temporalPreviousJitter = previousTemporalJitter_;
    bloomParam_.temporalJitterEnabled = enableTemporalJitter_ ? 1.0f : 0.0f;
    bloomParam_.temporalJitterScale = temporalJitterScale_;

    Object3dCommon* objectCommon = Object3dCommon::GetInstance();
    if (objectCommon->GetDefaultCamera()) {
        objectCommon->GetDefaultCamera()->SetProjectionJitter(temporalJitter_);
    }
    if (objectCommon->GetDebugCamera()) {
        objectCommon->GetDebugCamera()->SetProjectionJitter(temporalJitter_);
    }
}

void Bloom::ResetTemporalHistory() {
    hasTemporalHistory_ = false;
    temporalHistoryIndex_ = 0;
    temporalFrameIndex_ = 0;
    temporalJitter_ = { 0.0f, 0.0f };
    previousTemporalJitter_ = { 0.0f, 0.0f };
    bloomParam_.temporalHistoryValid = 0.0f;
    bloomParam_.temporalJitter = temporalJitter_;
    bloomParam_.temporalPreviousJitter = previousTemporalJitter_;
}

void Bloom::SetGrayscaleEnabled(bool enabled) {
    forceGrayscale_ = enabled;
	ComposeTransientEffects();
    bloomCB_->Update(bloomParam_);
}

void Bloom::SetGaussianOverride(float intensity) {
    gaussianOverrideIntensity_ = (std::clamp)(intensity, 0.0f, 1.0f);
	ComposeTransientEffects();
    bloomCB_->Update(bloomParam_);
}

void Bloom::SetTransientPulse(
    float bloomBoost,
    float chromAbAmount,
    const Vector2& center,
    float radius,
    float width,
    float strength) {
	transientPulse_.bloomBoost = (std::max)(0.0f, bloomBoost);
	transientPulse_.chromAbAmount = (std::max)(0.0f, chromAbAmount);
	transientPulse_.center = center;
	transientPulse_.radius = (std::max)(0.0f, radius);
	transientPulse_.width = (std::max)(0.001f, width);
	transientPulse_.strength = (std::max)(0.0f, strength);
    transientBloomBoost_ = (std::max)(0.0f, bloomBoost);
    transientChromAbAmount_ = (std::max)(0.0f, chromAbAmount);
	ComposeTransientEffects();
    bloomCB_->Update(bloomParam_);
}

void Bloom::SetScreenEffectState(const IScene::ScreenEffectState& state) {
	const bool wasActive = screenEffectState_.active;
	if (!wasActive) {
		CaptureScreenEffectBase();
	}
	screenEffectState_ = state;
	ComposeTransientEffects();
	if (wasActive && !state.active) {
		CaptureScreenEffectBase();
	}
	bloomCB_->Update(bloomParam_);
}

void Bloom::CaptureScreenEffectBase() {
	screenEffectBaseParam_.vignetteIntensity = bloomParam_.vignetteIntensity;
	screenEffectBaseParam_.vignetteScale = bloomParam_.vignetteScale;
	screenEffectBaseParam_.noiseIntensity = bloomParam_.noiseIntensity;
	screenEffectBaseParam_.scanlineIntensity = bloomParam_.scanlineIntensity;
	screenEffectBaseParam_.scanlineFrequency = bloomParam_.scanlineFrequency;
	screenEffectBaseParam_.glitchAmount = bloomParam_.glitchAmount;
	screenEffectBaseParam_.boxBlurRadius = bloomParam_.boxBlurRadius;
	screenEffectBaseParam_.radialBlurCenter = bloomParam_.radialBlurCenter;
	screenEffectBaseParam_.radialBlurWidth = bloomParam_.radialBlurWidth;
	screenEffectBaseParam_.radialBlurIntensity = bloomParam_.radialBlurIntensity;
	screenEffectBaseParam_.randomIntensity = bloomParam_.randomIntensity;
	screenEffectBaseParam_.randomScale = bloomParam_.randomScale;
	screenEffectBaseParam_.randomTimeScale = bloomParam_.randomTimeScale;
	screenEffectBaseParam_.exposure = bloomParam_.exposure;
	// A scene-local suppression must not become the next scene's saved baseline.
	if (!screenEffectState_.suppressOutlines) {
		screenEffectBaseParam_.outlineWidth = bloomParam_.outlineWidth;
		screenEffectBaseParam_.outlineThreshold = bloomParam_.outlineThreshold;
		screenEffectBaseParam_.outlineColor = bloomParam_.outlineColor;
		screenEffectBaseParam_.outlineBloomIntensity = bloomParam_.outlineBloomIntensity;
		screenEffectBaseParam_.outlineBloomWidth = bloomParam_.outlineBloomWidth;
		screenEffectBaseParam_.depthOutlineEnabled = bloomParam_.depthOutlineEnabled;
		screenEffectBaseParam_.depthOutlineScale = bloomParam_.depthOutlineScale;
	}
}

void Bloom::ComposeTransientEffects() {
	const BloomParam& effect = screenEffectState_.param;
	const BloomParam& base = screenEffectBaseParam_;

	bloomParam_.intensity = (std::clamp)(
		(baseBloomIntensity_ + transientBloomBoost_) *
			(std::clamp)(screenEffectState_.bloomScale, 0.0f, 1.0f) +
			(screenEffectState_.active ? effect.intensity : 0.0f),
		0.0f,
		4.0f);
	bloomParam_.distortionAmount = baseDistortionAmount_;
	bloomParam_.chromAbAmount = (std::clamp)(
		baseChromAbAmount_ + transientChromAbAmount_ +
			(screenEffectState_.active ? effect.chromAbAmount : 0.0f),
		0.0f,
		0.20f);
	bloomParam_.isGrayscale =
		(manualGrayscale_ || forceGrayscale_) ? 1.0f :
		(screenEffectState_.active ? (std::clamp)(effect.isGrayscale, 0.0f, 1.0f) : 0.0f);

	bloomParam_.vignetteIntensity = screenEffectState_.active
		? (std::max)(base.vignetteIntensity, effect.vignetteIntensity)
		: base.vignetteIntensity;
	bloomParam_.vignetteScale = screenEffectState_.active
		? (std::max)(base.vignetteScale, effect.vignetteScale)
		: base.vignetteScale;
	bloomParam_.noiseIntensity = screenEffectState_.active
		? (std::max)(base.noiseIntensity, effect.noiseIntensity)
		: base.noiseIntensity;
	bloomParam_.scanlineIntensity = screenEffectState_.active
		? (std::max)(base.scanlineIntensity, effect.scanlineIntensity)
		: base.scanlineIntensity;
	bloomParam_.scanlineFrequency =
		screenEffectState_.active && effect.scanlineIntensity > 0.0f
		? (std::max)(base.scanlineFrequency, effect.scanlineFrequency)
		: base.scanlineFrequency;
	bloomParam_.glitchAmount = screenEffectState_.active
		? (std::max)(base.glitchAmount, effect.glitchAmount)
		: base.glitchAmount;

	const float manualGaussian = fullScreenSmoothingMode_ == 1 ? baseGaussianIntensity_ : 0.0f;
	const float manualBox = fullScreenSmoothingMode_ == 2 ? baseFullScreenBoxBlurBlend_ : 0.0f;
	bloomParam_.gaussianIntensity = (std::max)(
		(std::max)(manualGaussian, gaussianOverrideIntensity_),
		screenEffectState_.active ? effect.gaussianIntensity : 0.0f);
	bloomParam_.fullScreenBoxBlurBlend = (std::max)(
		manualBox,
		screenEffectState_.active ? effect.fullScreenBoxBlurBlend : 0.0f);
	bloomParam_.boxBlurRadius =
		screenEffectState_.active && effect.fullScreenBoxBlurBlend > 0.0f
		? (std::max)(base.boxBlurRadius, effect.boxBlurRadius)
		: base.boxBlurRadius;

	bloomParam_.radialBlurCenter =
		screenEffectState_.active && effect.radialBlurIntensity > base.radialBlurIntensity
		? effect.radialBlurCenter
		: base.radialBlurCenter;
	bloomParam_.radialBlurWidth =
		screenEffectState_.active
		? (std::max)(base.radialBlurWidth, effect.radialBlurWidth)
		: base.radialBlurWidth;
	bloomParam_.radialBlurIntensity =
		screenEffectState_.active
		? (std::max)(base.radialBlurIntensity, effect.radialBlurIntensity)
		: base.radialBlurIntensity;
	bloomParam_.randomIntensity =
		screenEffectState_.active
		? (std::max)(base.randomIntensity, effect.randomIntensity)
		: base.randomIntensity;
	bloomParam_.randomScale =
		screenEffectState_.active && effect.randomIntensity > 0.0f
		? (std::max)(base.randomScale, effect.randomScale)
		: base.randomScale;
	bloomParam_.randomTimeScale =
		screenEffectState_.active && effect.randomIntensity > 0.0f
		? (std::max)(base.randomTimeScale, effect.randomTimeScale)
		: base.randomTimeScale;
	bloomParam_.exposure = (std::clamp)(
		base.exposure + (screenEffectState_.active ? effect.exposure : 0.0f),
		0.05f,
		4.0f);
	if (screenEffectState_.active && effect.outlineWidth > 0.0f) {
		bloomParam_.outlineWidth = effect.outlineWidth;
		bloomParam_.outlineThreshold = effect.outlineThreshold;
		bloomParam_.outlineColor = effect.outlineColor;
		bloomParam_.depthOutlineEnabled = effect.depthOutlineEnabled;
		bloomParam_.depthOutlineScale = effect.depthOutlineScale;
	} else {
		bloomParam_.outlineWidth = base.outlineWidth;
		bloomParam_.outlineThreshold = base.outlineThreshold;
		bloomParam_.outlineColor = base.outlineColor;
		bloomParam_.depthOutlineEnabled = base.depthOutlineEnabled;
		bloomParam_.depthOutlineScale = base.depthOutlineScale;
	}
	bloomParam_.outlineBloomIntensity = base.outlineBloomIntensity;
	bloomParam_.outlineBloomWidth = base.outlineBloomWidth;

	const bool useScreenShockwave =
		screenEffectState_.active &&
		effect.shockwaveStrength >= transientPulse_.strength;
	if (useScreenShockwave) {
		bloomParam_.shockwaveCenter = effect.shockwaveCenter;
		bloomParam_.shockwaveRadius = effect.shockwaveRadius;
		bloomParam_.shockwaveWidth = (std::max)(0.001f, effect.shockwaveWidth);
		bloomParam_.shockwaveStrength = (std::max)(0.0f, effect.shockwaveStrength);
	} else {
		bloomParam_.shockwaveCenter = transientPulse_.center;
		bloomParam_.shockwaveRadius = transientPulse_.radius;
		bloomParam_.shockwaveWidth = transientPulse_.width;
		bloomParam_.shockwaveStrength = transientPulse_.strength;
	}
	if (screenEffectState_.suppressOutlines) {
		bloomParam_.outlineWidth = 0.0f;
		bloomParam_.outlineBloomIntensity = 0.0f;
		bloomParam_.outlineBloomWidth = 0.0f;
		bloomParam_.depthOutlineEnabled = 0.0f;
		bloomParam_.depthOutlineScale = 0.0f;
	}
}

void Bloom::PreDraw() {
    if (!developerShowcase_.active) ApplyTemporalJitterToCameras();

    Transition(sceneRT_->GetDepthResource(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_DEPTH_WRITE);

    // 1. SceneRT を RenderTarget 状態へ
    Transition(sceneRT_->GetResource(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    Transition(normalRT_->GetResource(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    Transition(materialRT_->GetResource(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    // 2. レンダーターゲット設定とクリア
    dxCommon_->SetRenderTargets(
        sceneRT_->GetRTVHandle(),
        normalRT_->GetRTVHandle(),
        materialRT_->GetRTVHandle(),
        sceneRT_->GetDSVHandle());
    dxCommon_->GetList()->ClearDepthStencilView(sceneRT_->GetDSVHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    dxCommon_->ClearRenderTarget(sceneRT_->GetRTVHandle());
    const float normalClearColor[4] = { 0.5f, 0.5f, 1.0f, 0.0f };
    dxCommon_->ClearRenderTarget(normalRT_->GetRTVHandle(), normalClearColor);
    const float materialClearColor[4] = { 1.0f, 0.0f, 1.0f, 0.0f };
    dxCommon_->ClearRenderTarget(materialRT_->GetRTVHandle(), materialClearColor);

    dxCommon_->SetViewport(WinApp::kClientWidth, WinApp::kClientHeight);
}

void Bloom::PostDraw() {
    struct ParamRestore {
        BloomParam& value; BloomParam saved; bool active;
        ~ParamRestore() { if (active) value = saved; }
    } restore{bloomParam_,bloomParam_,developerShowcase_.active};
    if (developerShowcase_.active) {
        bloomParam_.threshold = developerShowcase_.threshold;
        bloomParam_.intensity = developerShowcase_.intensity;
        bloomParam_.exposure = developerShowcase_.exposure;
        bloomParam_.toneMappingMode = 1.0f;
        bloomParam_.renderDebugMode = static_cast<float>(developerShowcase_.diagnostic == 3 ? 2 : developerShowcase_.diagnostic);
        bloomParam_.isGrayscale = bloomParam_.isInverted = 0.0f;
        bloomParam_.vignetteIntensity = bloomParam_.distortionAmount = bloomParam_.chromAbAmount = 0.0f;
        bloomParam_.noiseIntensity = bloomParam_.scanlineIntensity = bloomParam_.curvature = bloomParam_.glitchAmount = 0.0f;
        bloomParam_.gaussianIntensity = bloomParam_.fullScreenBoxBlurBlend = bloomParam_.boxBlurIntensity = 0.0f;
        bloomParam_.outlineWidth = bloomParam_.outlineBloomIntensity = bloomParam_.depthOutlineEnabled = 0.0f;
        bloomParam_.shockwaveStrength = bloomParam_.radialBlurIntensity = bloomParam_.dissolveThreshold = 0.0f;
        bloomParam_.randomIntensity = bloomParam_.randomGrayscalePreview = 0.0f;
        bloomParam_.ssaoEnabled = bloomParam_.ssrEnabled = bloomParam_.depthFogEnabled = 0.0f;
        bloomParam_.temporalEnabled = bloomParam_.temporalHistoryValid = bloomParam_.motionVectorEnabled = 0.0f;
        bloomParam_.temporalJitterEnabled = 0.0f;
    }
    // All resolve/filter draws below overwrite their full target with depth
    // and blending disabled. Only a skipped resolve needs a fallback clear.

    // --- A. SceneRT の描画終了 (RT -> SRV) ---
    Transition(sceneRT_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    Transition(normalRT_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    Transition(materialRT_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    Transition(sceneRT_->GetDepthResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    UpdateFrameCameraParameters(true);
    bloomCB_->Update(bloomParam_);

	D3D12_GPU_DESCRIPTOR_HANDLE sceneSource = sceneRT_->GetGPUHandle();
	const bool useRandomPass = bloomParam_.randomGrayscalePreview > 0.5f || bloomParam_.randomIntensity > 0.0f;
	if (useRandomPass) {
		Transition(randomRT_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		dxCommon_->SetRenderTargetNoDepth(randomRT_->GetRTVHandle());
		dxCommon_->SetViewport(WinApp::kClientWidth, WinApp::kClientHeight);
		postEffect_->Draw(sceneRT_->GetGPUHandle(), kRandom);
		Transition(randomRT_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		sceneSource = randomRT_->GetGPUHandle();
	}

    // --- B. Motion vector pass (Depth + current/previous camera -> Motion Vector RT) ---
    Transition(motionVectorRT_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(motionVectorRT_->GetRTVHandle());
    dxCommon_->SetViewport(WinApp::kClientWidth, WinApp::kClientHeight);
    const float motionClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (enableMotionVector_ && !developerShowcase_.active) {
        postEffect_->DrawMotionVectorResolve(sceneRT_->GetDepthGPUHandle());
    } else {
        dxCommon_->ClearRenderTarget(motionVectorRT_->GetRTVHandle(), motionClearColor);
    }
    Transition(motionVectorRT_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // --- C. Temporal resolve pass (Scene + History + Motion -> Stable Scene) ---
    const bool canUseTemporalAccumulation =
        !developerShowcase_.active &&
        enableTemporalAccumulation_ &&
        enableMotionVector_ &&
        bloomParam_.motionVectorScale >= 0.0f &&
        renderDebugMode_ != 15;

    if (canUseTemporalAccumulation) {
        const int readHistoryIndex = temporalHistoryIndex_;
        const int writeHistoryIndex = 1 - temporalHistoryIndex_;
        D3D12_GPU_DESCRIPTOR_HANDLE historySource = hasTemporalHistory_
            ? temporalHistoryRT_[readHistoryIndex]->GetGPUHandle()
            : sceneSource;

        bloomParam_.temporalHistoryValid = hasTemporalHistory_ ? 1.0f : 0.0f;
        bloomCB_->Update(bloomParam_);

        Transition(temporalHistoryRT_[writeHistoryIndex]->GetResource(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        dxCommon_->SetRenderTargetNoDepth(temporalHistoryRT_[writeHistoryIndex]->GetRTVHandle());
        dxCommon_->SetViewport(WinApp::kClientWidth, WinApp::kClientHeight);
        postEffect_->DrawTemporalResolve(
            sceneSource,
            historySource,
            sceneRT_->GetDepthGPUHandle(),
            normalRT_->GetGPUHandle(),
            materialRT_->GetGPUHandle(),
            motionVectorRT_->GetGPUHandle());
        Transition(temporalHistoryRT_[writeHistoryIndex]->GetResource(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        sceneSource = temporalHistoryRT_[writeHistoryIndex]->GetGPUHandle();
        temporalHistoryIndex_ = writeHistoryIndex;
        hasTemporalHistory_ = true;
        bloomParam_.temporalHistoryValid = 1.0f;
        bloomCB_->Update(bloomParam_);
    } else {
        hasTemporalHistory_ = false;
        bloomParam_.temporalHistoryValid = 0.0f;
        bloomCB_->Update(bloomParam_);
    }

    // --- D. SSAO resolve pass (Depth/Normal/Material -> AO RT) ---
    Transition(ssaoResolveRT_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(ssaoResolveRT_->GetRTVHandle());
    dxCommon_->SetViewport(WinApp::kClientWidth, WinApp::kClientHeight);
    const float aoClearColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    if (enableSSAO_ && !developerShowcase_.active) {
        postEffect_->DrawSSAOResolve(
            sceneRT_->GetDepthGPUHandle(),
            normalRT_->GetGPUHandle(),
            materialRT_->GetGPUHandle());
    } else {
        dxCommon_->ClearRenderTarget(ssaoResolveRT_->GetRTVHandle(), aoClearColor);
    }
    Transition(ssaoResolveRT_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    D3D12_GPU_DESCRIPTOR_HANDLE ssaoCompositeSource = ssaoResolveRT_->GetGPUHandle();
    if (enableSSAO_ && enableSSAODenoise_ && !developerShowcase_.active) {
        Transition(ssaoDenoiseRT_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        dxCommon_->SetRenderTargetNoDepth(ssaoDenoiseRT_->GetRTVHandle());
        dxCommon_->SetViewport(WinApp::kClientWidth, WinApp::kClientHeight);
        postEffect_->DrawSSAODenoise(
            ssaoResolveRT_->GetGPUHandle(),
            sceneRT_->GetDepthGPUHandle(),
            normalRT_->GetGPUHandle(),
            materialRT_->GetGPUHandle());
        Transition(ssaoDenoiseRT_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        ssaoCompositeSource = ssaoDenoiseRT_->GetGPUHandle();
    }

    // --- E. SSR resolve pass (Scene/Depth/Normal -> SSR RT) ---
    Transition(ssrResolveRT_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(ssrResolveRT_->GetRTVHandle());
    dxCommon_->SetViewport(WinApp::kClientWidth, WinApp::kClientHeight);
    const float ssrClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (enableSSR_ && !developerShowcase_.active) {
        postEffect_->DrawSSRResolve(
            sceneSource,
            sceneRT_->GetDepthGPUHandle(),
            normalRT_->GetGPUHandle(),
            materialRT_->GetGPUHandle());
    } else {
        dxCommon_->ClearRenderTarget(ssrResolveRT_->GetRTVHandle(), ssrClearColor);
    }
    Transition(ssrResolveRT_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    D3D12_GPU_DESCRIPTOR_HANDLE ssrCompositeSource = ssrResolveRT_->GetGPUHandle();
    if (enableSSR_ && enableSSRDenoise_ && !developerShowcase_.active) {
        Transition(ssrDenoiseRT_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        dxCommon_->SetRenderTargetNoDepth(ssrDenoiseRT_->GetRTVHandle());
        dxCommon_->SetViewport(WinApp::kClientWidth, WinApp::kClientHeight);
        postEffect_->DrawSSRDenoise(
            ssrResolveRT_->GetGPUHandle(),
            sceneRT_->GetDepthGPUHandle(),
            normalRT_->GetGPUHandle(),
            materialRT_->GetGPUHandle());
        Transition(ssrDenoiseRT_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        ssrCompositeSource = ssrDenoiseRT_->GetGPUHandle();
    }

    // --- F. 抽出パス (SceneRT -> BloomHalf) ---
    Transition(bloomRT_Half_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_Half_->GetRTVHandle());
    dxCommon_->SetViewport(WinApp::kClientWidth / 2, WinApp::kClientHeight / 2);

    // Full-screen smoothing modes skip bright-pass extraction and blur the whole scene.
    if (bloomParam_.gaussianIntensity > 0.0f || bloomParam_.fullScreenBoxBlurBlend > 0.0f) {
        postEffect_->Draw(sceneSource, kAdd_Bloom_Downsample);
    } else {
        postEffect_->DrawBloomExtract(sceneSource, materialRT_->GetGPUHandle());
    }

    Transition(bloomRT_Half_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // --- G. Bloom prefilter (BloomHalf -> BloomA) ---
    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_A_->GetRTVHandle());
    dxCommon_->SetViewport(WinApp::kClientWidth / 2, WinApp::kClientHeight / 2);

    postEffect_->Draw(bloomRT_Half_->GetGPUHandle(), kAdd_Bloom_Downsample);
    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // --- H. Blur filter ---
    Transition(bloomRT_B_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_B_->GetRTVHandle());
    postEffect_->Draw(bloomRT_A_->GetGPUHandle(), kAdd_Bloom_BlurH);
    Transition(bloomRT_B_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_A_->GetRTVHandle());
    postEffect_->Draw(bloomRT_B_->GetGPUHandle(), kAdd_Bloom_BlurV);
    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // --- I. 最終合成 (SceneRT + BloomA + AO + SSR + Motion -> BackBuffer) ---
    dxCommon_->SetBackBuffer();
    dxCommon_->SetViewport(WinApp::kClientWidth, WinApp::kClientHeight);

    // 最終的に DrawComposite で HLSL 側のメイン処理が走ります
    postEffect_->DrawComposite(
        sceneSource,
        developerShowcase_.active && developerShowcase_.diagnostic == 3 ? bloomRT_Half_->GetGPUHandle() : bloomRT_A_->GetGPUHandle(),
        sceneRT_->GetDepthGPUHandle(),
        normalRT_->GetGPUHandle(),
        ssrCompositeSource,
        materialRT_->GetGPUHandle(),
        ssaoCompositeSource,
        motionVectorRT_->GetGPUHandle());
}

void Bloom::SetDeveloperShowcaseState(const IScene::DeveloperShowcaseState& state) {
    if (developerShowcase_.active != state.active) { ResetTemporalHistory(); hasPreviousMotionViewProjection_ = false; }
    developerShowcase_ = state;
}

void Bloom::Transition(ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {

    assert(res != nullptr);
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = res;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    dxCommon_->GetList()->ResourceBarrier(1, &barrier);

}

} // namespace cg2
