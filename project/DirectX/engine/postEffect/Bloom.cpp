#include "Bloom.h"

void Bloom::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, RtvManager* rtvManager) {

    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    rtvManager_ = rtvManager;

    sceneRT_ = std::make_unique<RenderTexture>();

    sceneRT_->Initialize(
        dxCommon_,
        srvManager_,
        rtvManager_,
        WinApp::kClientWidth,
        WinApp::kClientHeight
    );

    // bloom用CBVの生成
    bloomCB_ = std::make_unique<BloomConstantBuffer>();
    bloomCB_->Initialize(dxCommon_);

    // ポストエフェクトの初期化
    postEffect_ = std::make_unique<PostEffect>();
    postEffect_->Initialize(dxCommon_, bloomCB_.get());

    bloomRT_A_ = std::make_unique<RenderTexture>();
    bloomRT_B_ = std::make_unique<RenderTexture>();
    uint32_t bloomWidth = WinApp::kClientWidth / 4;
    uint32_t bloomHeight = WinApp::kClientHeight / 4;

    bloomRT_A_->Initialize(
        dxCommon_,
        srvManager_,
        rtvManager_,
        bloomWidth,
        bloomHeight
    );

    bloomRT_B_->Initialize(
        dxCommon_,
        srvManager_,
        rtvManager_,
        bloomWidth,
        bloomHeight
    );

    // bloomRT_Half を追加
    bloomRT_Half_ = std::make_unique<RenderTexture>();
    // サイズは画面の半分
    bloomRT_Half_->Initialize(dxCommon_, srvManager_, rtvManager_, WinApp::kClientWidth / 2, WinApp::kClientHeight / 2);

    // ブルームパラメータ
    bloomParam_.threshold = 0.0f;
    bloomParam_.intensity = 1.2f;
    bloomParam_.vignetteIntensity = 0.5f;
    bloomParam_.vignetteScale = 1.0f;
    bloomParam_.chromAbAmount = 0.02f;
    bloomParam_.distortionAmount = 0.01f;
    bloomParam_.noiseIntensity = 0.5f;
    bloomParam_.scanlineIntensity = 1.0f;
    bloomParam_.scanlineFrequency = 30.0f;
    bloomParam_.curvature = 0.6f;
    bloomParam_.borderSharp = 0.02f;
    bloomParam_.glitchAmount = 0.0f;

    bloomCB_->Update(bloomParam_);

}

void Bloom::Update() {

#ifdef USE_IMGUI

    ImGui::Begin("BloomAndVignette");
    ImGui::DragFloat("Threshold", &bloomParam_.threshold, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("insensity", &bloomParam_.intensity, 0.01f);
    ImGui::DragFloat("vinetteInsencity", &bloomParam_.vignetteIntensity, 0.01f);
    ImGui::DragFloat("vinetteScale", &bloomParam_.vignetteScale, 0.01f);
    ImGui::DragFloat("distortionAmount", &bloomParam_.distortionAmount, 0.001f);
    ImGui::DragFloat("chromAbAmount", &bloomParam_.chromAbAmount, 0.001f);
    ImGui::DragFloat("Noise", &bloomParam_.noiseIntensity, 0.01f);
    ImGui::DragFloat("Scanline Intensity", &bloomParam_.scanlineIntensity, 0.01f);
    ImGui::DragFloat("Scanline Frequency", &bloomParam_.scanlineFrequency, 0.01f);
    ImGui::DragFloat("Curvature", &bloomParam_.curvature, 0.001f);   // 画面の膨らみ
    ImGui::DragFloat("Border Sharp", &bloomParam_.borderSharp, 0.001f); // 角の丸み
    ImGui::DragFloat("glitchAmount", &bloomParam_.glitchAmount, 0.001f); // 角の丸み

    // bool値を float(0.0 or 1.0) に変換して送る
    static bool grayFlag = false;
    static bool invertFlag = false;
    if (ImGui::Checkbox("Grayscale", &grayFlag)) {
        bloomParam_.isGrayscale = grayFlag ? 1.0f : 0.0f;
    }
    if (ImGui::Checkbox("Invert Color", &invertFlag)) {
        bloomParam_.isInverted = invertFlag ? 1.0f : 0.0f;
    }

    // リセットするボタン
    if (ImGui::Button("Reset")) {
        bloomParam_.threshold = 0.0f;
        bloomParam_.intensity = 0.0f;
        bloomParam_.vignetteIntensity = 0.0f;
        bloomParam_.vignetteScale = 0.0f;
        bloomParam_.chromAbAmount = 0.0f;
        bloomParam_.distortionAmount = 0.0f;
        bloomParam_.noiseIntensity = 0.0f;
        bloomParam_.scanlineIntensity = 0.0f;
        bloomParam_.scanlineFrequency = 0.0f;
        bloomParam_.curvature = 0.0f;
        bloomParam_.borderSharp = 0.0f;
        bloomParam_.glitchAmount = 0.0f;
        grayFlag = false;
        invertFlag = false;
    }

    ImGui::End();

#endif // USE_IMGUI

    bloomCB_->Update(bloomParam_);
    timer_ += SceneManager::GetInstance()->GetFinalDeltaTime();

    bloomParam_.timer = timer_;

}

void Bloom::PreDraw() {
    // 1. SceneRT を RenderTarget 状態へ
    Transition(sceneRT_->GetResource(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    // 2. レンダーターゲット設定とクリア
    dxCommon_->SetRenderTarget(sceneRT_->GetRTVHandle(), sceneRT_->GetDSVHandle());
    dxCommon_->GetList()->ClearDepthStencilView(sceneRT_->GetDSVHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    dxCommon_->ClearRenderTarget(sceneRT_->GetRTVHandle());

    dxCommon_->SetViewport(WinApp::kClientWidth, WinApp::kClientHeight);
}

void Bloom::PostDraw() {
    auto commandList = dxCommon_->GetList();

    // --- A. SceneRT の描画終了 (RT -> SRV) ---
    Transition(sceneRT_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // --- B. 高輝度抽出 (SceneRT -> BloomHalf) ---
    Transition(bloomRT_Half_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_Half_->GetRTVHandle());
    dxCommon_->SetViewport(WinApp::kClientWidth / 2, WinApp::kClientHeight / 2);
    dxCommon_->ClearRenderTarget(bloomRT_Half_->GetRTVHandle());

    postEffect_->Draw(sceneRT_->GetGPUHandle(), kAdd_Bloom_Extract);
    Transition(bloomRT_Half_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // --- C. ダウンサンプリング (BloomHalf -> BloomA) ---
    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_A_->GetRTVHandle());
    dxCommon_->SetViewport(WinApp::kClientWidth / 4, WinApp::kClientHeight / 4);
    dxCommon_->ClearRenderTarget(bloomRT_A_->GetRTVHandle());

    postEffect_->Draw(bloomRT_Half_->GetGPUHandle(), kAdd_Bloom_Downsample);
    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // --- D. ブラー (BloomA <-> BloomB) ---
    // Horizontal
    Transition(bloomRT_B_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_B_->GetRTVHandle());
    postEffect_->Draw(bloomRT_A_->GetGPUHandle(), kAdd_Bloom_BlurH);
    Transition(bloomRT_B_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // Vertical
    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_A_->GetRTVHandle());
    postEffect_->Draw(bloomRT_B_->GetGPUHandle(), kAdd_Bloom_BlurV);
    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // --- E. 最終合成 (SceneRT + BloomA -> BackBuffer) ---
    dxCommon_->SetBackBuffer(); // 内部でバックバッファを RT 状態にする想定
    dxCommon_->SetViewport(WinApp::kClientWidth, WinApp::kClientHeight);

    postEffect_->DrawComposite(sceneRT_->GetGPUHandle(), bloomRT_A_->GetGPUHandle());
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