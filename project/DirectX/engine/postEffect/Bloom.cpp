#include "Bloom.h"
#include <algorithm>

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

   /* bloomParam_.threshold = 0.0f;
    bloomParam_.intensity = 1.2f;
    bloomParam_.vignetteIntensity = 1.0f;
    bloomParam_.vignetteScale = 1.7f;
    bloomParam_.chromAbAmount = 0.0f;
    bloomParam_.distortionAmount = 0.0f;
    bloomParam_.noiseIntensity = 0.2f;
    bloomParam_.scanlineIntensity = 2.5f;
    bloomParam_.scanlineFrequency = 50.0f;
    bloomParam_.curvature = 0.0f;
    bloomParam_.borderSharp = 0.0f;
    bloomParam_.glitchAmount = 0.005f;*/
    baseGaussianIntensity_ = bloomParam_.gaussianIntensity;
    baseFullScreenBoxBlurBlend_ = bloomParam_.fullScreenBoxBlurBlend;
    baseBloomIntensity_ = bloomParam_.intensity;
    baseDistortionAmount_ = bloomParam_.distortionAmount;
    baseChromAbAmount_ = bloomParam_.chromAbAmount;

    bloomCB_->Update(bloomParam_);

}

void Bloom::Update() {

#ifdef USE_IMGUI

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
        "SSAO"
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

#endif // USE_IMGUI

    renderDebugMode_ = (std::clamp)(renderDebugMode_, 0, 7);
    bloomParam_.isGrayscale = (manualGrayscale_ || forceGrayscale_) ? 1.0f : 0.0f;
    bloomParam_.depthOutlineEnabled = enableDepthOutline_ ? 1.0f : 0.0f;
    bloomParam_.depthFogEnabled = enableDepthFog_ ? 1.0f : 0.0f;
    bloomParam_.ssaoEnabled = enableSSAO_ ? 1.0f : 0.0f;
    bloomParam_.ssaoSampleCount = (std::clamp)(bloomParam_.ssaoSampleCount, 1.0f, 16.0f);
    bloomParam_.renderDebugMode = static_cast<float>(renderDebugMode_);
    bloomParam_.intensity = baseBloomIntensity_ + transientBloomBoost_;
    bloomParam_.distortionAmount = baseDistortionAmount_;
    bloomParam_.chromAbAmount = baseChromAbAmount_ + transientChromAbAmount_;
    const float manualGaussian = fullScreenSmoothingMode_ == 1 ? baseGaussianIntensity_ : 0.0f;
    const float manualBox = fullScreenSmoothingMode_ == 2 ? baseFullScreenBoxBlurBlend_ : 0.0f;
    bloomParam_.gaussianIntensity = (std::max)(manualGaussian, gaussianOverrideIntensity_);
    bloomParam_.fullScreenBoxBlurBlend = manualBox;
    bloomCB_->Update(bloomParam_);
    timer_ += SceneManager::GetInstance()->GetFinalDeltaTime();
    bloomParam_.timer = timer_;
}

void Bloom::SetGrayscaleEnabled(bool enabled) {
    forceGrayscale_ = enabled;
    bloomParam_.isGrayscale = (manualGrayscale_ || forceGrayscale_) ? 1.0f : 0.0f;
    bloomCB_->Update(bloomParam_);
}

void Bloom::SetGaussianOverride(float intensity) {
    gaussianOverrideIntensity_ = (std::clamp)(intensity, 0.0f, 1.0f);
    const float manualGaussian = fullScreenSmoothingMode_ == 1 ? baseGaussianIntensity_ : 0.0f;
    bloomParam_.gaussianIntensity = (std::max)(manualGaussian, gaussianOverrideIntensity_);
    bloomParam_.fullScreenBoxBlurBlend = fullScreenSmoothingMode_ == 2 ? baseFullScreenBoxBlurBlend_ : 0.0f;
    bloomCB_->Update(bloomParam_);
}

void Bloom::SetTransientPulse(
    float bloomBoost,
    float chromAbAmount,
    const Vector2& center,
    float radius,
    float width,
    float strength) {
    transientBloomBoost_ = (std::max)(0.0f, bloomBoost);
    transientChromAbAmount_ = (std::max)(0.0f, chromAbAmount);
    bloomParam_.intensity = baseBloomIntensity_ + transientBloomBoost_;
    bloomParam_.distortionAmount = baseDistortionAmount_;
    bloomParam_.chromAbAmount = baseChromAbAmount_ + transientChromAbAmount_;
    bloomParam_.shockwaveCenter = center;
    bloomParam_.shockwaveRadius = (std::max)(0.0f, radius);
    bloomParam_.shockwaveWidth = (std::max)(0.001f, width);
    bloomParam_.shockwaveStrength = (std::max)(0.0f, strength);
    bloomCB_->Update(bloomParam_);
}

void Bloom::PreDraw() {
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

    // 2. レンダーターゲット設定とクリア
    dxCommon_->SetRenderTargets(sceneRT_->GetRTVHandle(), normalRT_->GetRTVHandle(), sceneRT_->GetDSVHandle());
    dxCommon_->GetList()->ClearDepthStencilView(sceneRT_->GetDSVHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    dxCommon_->ClearRenderTarget(sceneRT_->GetRTVHandle());
    const float normalClearColor[4] = { 0.5f, 0.5f, 1.0f, 0.0f };
    dxCommon_->ClearRenderTarget(normalRT_->GetRTVHandle(), normalClearColor);

    dxCommon_->SetViewport(WinApp::kClientWidth, WinApp::kClientHeight);
}

void Bloom::PostDraw() {
    auto commandList = dxCommon_->GetList();

    // --- A. SceneRT の描画終了 (RT -> SRV) ---
    Transition(sceneRT_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    Transition(normalRT_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    Transition(sceneRT_->GetDepthResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	D3D12_GPU_DESCRIPTOR_HANDLE sceneSource = sceneRT_->GetGPUHandle();
	const bool useRandomPass = bloomParam_.randomGrayscalePreview > 0.5f || bloomParam_.randomIntensity > 0.0f;
	if (useRandomPass) {
		Transition(randomRT_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		dxCommon_->SetRenderTargetNoDepth(randomRT_->GetRTVHandle());
		dxCommon_->SetViewport(WinApp::kClientWidth, WinApp::kClientHeight);
		dxCommon_->ClearRenderTarget(randomRT_->GetRTVHandle());
		postEffect_->Draw(sceneRT_->GetGPUHandle(), kRandom);
		Transition(randomRT_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		sceneSource = randomRT_->GetGPUHandle();
	}

    // --- B. 抽出パス (SceneRT -> BloomHalf) ---
    Transition(bloomRT_Half_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_Half_->GetRTVHandle());
    dxCommon_->SetViewport(WinApp::kClientWidth / 2, WinApp::kClientHeight / 2);
    dxCommon_->ClearRenderTarget(bloomRT_Half_->GetRTVHandle());

    // Full-screen smoothing modes skip bright-pass extraction and blur the whole scene.
    if (bloomParam_.gaussianIntensity > 0.0f || bloomParam_.fullScreenBoxBlurBlend > 0.0f) {
        postEffect_->Draw(sceneSource, kAdd_Bloom_Downsample);
    } else {
        postEffect_->Draw(sceneSource, kAdd_Bloom_Extract);
    }

    Transition(bloomRT_Half_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // --- C. Bloom prefilter (BloomHalf -> BloomA) ---
    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_A_->GetRTVHandle());
    dxCommon_->SetViewport(WinApp::kClientWidth / 2, WinApp::kClientHeight / 2);
    dxCommon_->ClearRenderTarget(bloomRT_A_->GetRTVHandle());

    postEffect_->Draw(bloomRT_Half_->GetGPUHandle(), kAdd_Bloom_Downsample);
    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // --- D. Blur filter ---
    Transition(bloomRT_B_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_B_->GetRTVHandle());
    postEffect_->Draw(bloomRT_A_->GetGPUHandle(), kAdd_Bloom_BlurH);
    Transition(bloomRT_B_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_A_->GetRTVHandle());
    postEffect_->Draw(bloomRT_B_->GetGPUHandle(), kAdd_Bloom_BlurV);
    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // --- E. 最終合成 (SceneRT + BloomA -> BackBuffer) ---
    dxCommon_->SetBackBuffer();
    dxCommon_->SetViewport(WinApp::kClientWidth, WinApp::kClientHeight);

    // 最終的に DrawComposite で HLSL 側のメイン処理が走ります
    postEffect_->DrawComposite(
        sceneSource,
        bloomRT_A_->GetGPUHandle(),
        sceneRT_->GetDepthGPUHandle(),
        normalRT_->GetGPUHandle());
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
