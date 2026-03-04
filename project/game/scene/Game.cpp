#include "Game.h"
#include "SceneManager.h"
#include "Audio.h"

bool Game::Initialize() {

    CoInitializeEx(0, COINIT_MULTITHREADED);

    Dump dump;
    SetUnhandledExceptionFilter(dump.Export);

    WinApp::GetInstance()->Initialize();

    InitializeEngine();
    InitializeImGui();
    LoadResources();

    sceneManager_ = std::make_unique<SceneManager>();
    sceneManager_->Initialize();

    rtvManager_ = std::make_unique<RtvManager>();
    rtvManager_->Initialize(dxCommon_.get());

    sceneRenderTexture_ = std::make_unique<RenderTexture>();

    sceneRenderTexture_->Initialize(
        dxCommon_.get(),
        srvManager_.get(),
        rtvManager_.get(),
        WinApp::kClientWidth,
        WinApp::kClientHeight
    );

    // bloom用CBVの生成
    bloomCB_ = std::make_unique<BloomConstantBuffer>();
    bloomCB_->Initialize(dxCommon_.get());

    // ポストエフェクトの初期化
    postEffect_ = std::make_unique<PostEffect>();
    postEffect_->Initialize(dxCommon_.get(), bloomCB_.get());

    bloomRT_A_ = std::make_unique<RenderTexture>();
    bloomRT_B_ = std::make_unique<RenderTexture>();
    uint32_t bloomWidth = WinApp::kClientWidth / 4;
    uint32_t bloomHeight = WinApp::kClientHeight / 4;

    bloomRT_A_->Initialize(
        dxCommon_.get(),
        srvManager_.get(),
        rtvManager_.get(),
        bloomWidth,
        bloomHeight
    );

    bloomRT_B_->Initialize(
        dxCommon_.get(),
        srvManager_.get(),
        rtvManager_.get(),
        bloomWidth,
        bloomHeight
    );

    // bloomRT_Half を追加
    bloomRT_Half_ = std::make_unique<RenderTexture>();
    // サイズは画面の半分
    bloomRT_Half_->Initialize(dxCommon_.get(), srvManager_.get(), rtvManager_.get(), WinApp::kClientWidth / 2, WinApp::kClientHeight / 2);

    // ブルームパラメータ
    bloomParam_.threshold = 0.1f;
    bloomParam_.intensity = 0.9f;
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

    ParticleManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());

    // 音声読み込み
    Audio::GetInstance()->Initialize();
    Audio::GetInstance()->LoadAudio(L"BGM", L"resources/BGM_shining_star.mp3");
    Audio::GetInstance()->LoadAudio(L"bulletShoot", L"resources/bulletShoot.mp3", 5);
    
    // 再生
    Audio::GetInstance()->PlayAudio(L"BGM", true, 0.1f);

    return true;
}

void Game::InitializeEngine() {

    dxCommon_ = std::make_unique<DirectXCommon>();
    dxCommon_->Initialize(WinApp::GetInstance());

    srvManager_ = std::make_unique<SrvManager>();
    srvManager_->Initialize(dxCommon_.get());

    TextureManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());
    ModelManager::GetInstance()->Initialize(dxCommon_.get());

    Object3dCommon::GetInstance()->Initialize(dxCommon_.get());
    SpriteCommon::GetInstance()->Initialize(dxCommon_.get());

    Input::GetInstance()->Initialize(
        WinApp::GetInstance()->GetWindowClass(),
        WinApp::GetInstance()->GetHwnd()
    );
}

void Game::InitializeImGui() {


#ifdef USE_IMGUI

    // Imguiの初期化
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(WinApp::GetInstance()->GetHwnd());
    ImGui_ImplDX12_Init(dxCommon_->GetDevice().Get(),
        dxCommon_->GetSwapChainDesc().BufferCount,
        dxCommon_->GetRtvDesc().Format,
        srvManager_->GetSrvHeap().Get(),
        srvManager_->GetSrvHeap()->GetCPUDescriptorHandleForHeapStart(),
        srvManager_->GetSrvHeap()->GetGPUDescriptorHandleForHeapStart());

#endif // USE_IMGUI

}

void Game::LoadResources() {

    const char* models[] = {
        "cube.obj",
        "cubeDamage.obj",
        "player.obj",
        "playerBullet.obj",
        "enemy.obj",
        "enemyBullet.obj",
        "playerParticle.obj",
        "enemyParticle.obj",
        "playerHPBar.obj",
        "playerHPBarGreen.obj",
        "playerHPBarLong.obj",
        "playerHPBarGreenLong.obj",
        "ball.obj",
        "bloomBall.obj",
        "bloomBlock.obj",
    };

    for (auto& model : models) {
        ModelManager::GetInstance()->LoadModel(model);
    }

}

void Game::Run() {
    MainLoop();
}

void Game::MainLoop() {

    MSG msg{};
    while (msg.message != WM_QUIT) {

        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }
        // インプットインスタンスを取得
        Input* input = Input::GetInstance();

        // 前のフレームのキー状態を保存
        input->BeforeFrameData();

#ifdef USE_IMGUI

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

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
        ImGui::End();

#endif // USE_IMGUI

        if (input->IsPress(input->GetKey()[DIK_LSHIFT]) && input->IsTrigger(input->GetKey()[DIK_D], input->GetPreKey()[DIK_D])) {
            if (Object3dCommon::GetInstance()->GetIsDebugCamera()) {
                Object3dCommon::GetInstance()->SetIsDebugCamera(false);
            } else {
                Object3dCommon::GetInstance()->SetIsDebugCamera(true);
            }
        }

        bloomCB_->Update(bloomParam_);

        sceneManager_->Update();
        timer += sceneManager_->GetFinalDeltaTime();

        bloomParam_.timer = timer;
       
#ifdef USE_IMGUI
        // ImGuiの内部コマンドを生成する
        ImGui::Render();

#endif // USE_IMGUI

        dxCommon_->PreDraw(); // バックバッファのバリアはここで行われている
        srvManager_->PreDraw();

        // ==========================================
        // 1. シーン描画 (SceneRT)
        // ==========================================
        // 書き込むので SRV -> RenderTarget に変更
        TransitionResource(dxCommon_.get(), sceneRenderTexture_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

        // Scene → SceneRT
        dxCommon_->SetRenderTarget(sceneRenderTexture_->GetRTVHandle());
        dxCommon_->ClearRenderTarget(sceneRenderTexture_->GetRTVHandle());
        dxCommon_->ClearDepthBuffer();

        sceneManager_->DrawPostEffect3D(); // 3Dオブジェクト描画

        SpriteCommon::GetInstance()->PreDraw(kNone);
        sceneManager_->DrawSprite();

        // 描き終わったので RenderTarget -> SRV (次の工程で読むため) に戻す
        TransitionResource(dxCommon_.get(), sceneRenderTexture_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


        // ==========================================
        // 2. 高輝度抽出 (SceneRT -> BloomHalf)
        // ==========================================
        // 書き込む BloomHalf を RT化
        TransitionResource(dxCommon_.get(), bloomRT_Half_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

        dxCommon_->SetRenderTarget(bloomRT_Half_->GetRTVHandle());
        dxCommon_->SetViewport(WinApp::kClientWidth / 2, WinApp::kClientHeight / 2);
        dxCommon_->ClearRenderTarget(bloomRT_Half_->GetRTVHandle());

        // 入力は sceneRenderTexture (SRV状態になっているのでOK)
        postEffect_->Draw(
            sceneRenderTexture_->GetSrvManager()->GetGPUDescriptorHandle(sceneRenderTexture_->GetSrvIndex()),
            kAdd_Bloom_Extract
        );

        // 書き込み完了、BloomHalf を SRV化
        TransitionResource(dxCommon_.get(), bloomRT_Half_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


        // ==========================================
        // 3. ダウンサンプリング (BloomHalf -> BloomA)
        // ==========================================
        // 書き込む BloomA を RT化
        TransitionResource(dxCommon_.get(), bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

        uint32_t bloomWidth = WinApp::kClientWidth / 4;
        uint32_t bloomHeight = WinApp::kClientHeight / 4;

        dxCommon_->SetRenderTarget(bloomRT_A_->GetRTVHandle());
        dxCommon_->SetViewport(bloomWidth, bloomHeight);
        dxCommon_->ClearRenderTarget(bloomRT_A_->GetRTVHandle());

        // 入力は BloomHalf (SRV状態なのでOK)
        postEffect_->Draw(
            bloomRT_Half_->GetSrvManager()->GetGPUDescriptorHandle(bloomRT_Half_->GetSrvIndex()),
            kAdd_Bloom_Downsample
        );

        // 書き込み完了、BloomA を SRV化
        TransitionResource(dxCommon_.get(), bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


        // ==========================================
        // 4. ブラー水平 (BloomA -> BloomB)
        // ==========================================
        // 書き込む BloomB を RT化
        TransitionResource(dxCommon_.get(), bloomRT_B_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

        dxCommon_->SetRenderTarget(bloomRT_B_->GetRTVHandle());
        dxCommon_->ClearRenderTarget(bloomRT_B_->GetRTVHandle());

        // 入力は BloomA (SRV状態なのでOK)
        postEffect_->Draw(
            bloomRT_A_->GetSrvManager()->GetGPUDescriptorHandle(bloomRT_A_->GetSrvIndex()),
            kAdd_Bloom_BlurH
        );

        // 書き込み完了、BloomB を SRV化
        TransitionResource(dxCommon_.get(), bloomRT_B_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


        // ==========================================
        // 5. ブラー垂直 (BloomB -> BloomA)
        // ==========================================
        // ★注意: BloomA はさっきSRVにしたばかりだが、また書き込むので RT化
        TransitionResource(dxCommon_.get(), bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

        dxCommon_->SetRenderTarget(bloomRT_A_->GetRTVHandle());
        dxCommon_->ClearRenderTarget(bloomRT_A_->GetRTVHandle());

        // 入力は BloomB (SRV状態なのでOK)
        postEffect_->Draw(
            bloomRT_B_->GetSrvManager()->GetGPUDescriptorHandle(bloomRT_B_->GetSrvIndex()),
            kAdd_Bloom_BlurV
        );

        // 書き込み完了、BloomA を SRV化 (これでCompositeで使える)
        TransitionResource(dxCommon_.get(), bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


        // ==========================================
        // 6. 合成 (SceneRT + BloomA -> BackBuffer)
        // ==========================================
        dxCommon_->SetBackBuffer(); // BackBufferへのバリアは内部で行われているはず（PreDraw参照）
        dxCommon_->SetViewport(WinApp::kClientWidth, WinApp::kClientHeight);

        // SceneRT も BloomA も ここまでの処理で SRV に戻っているので安全に読める
        postEffect_->DrawComposite(
            srvManager_->GetGPUDescriptorHandle(sceneRenderTexture_->GetSrvIndex()),
            srvManager_->GetGPUDescriptorHandle(bloomRT_A_->GetSrvIndex())
        );
        sceneManager_->Draw();

#ifdef USE_IMGUI
        // 実際のcommandListのImGuiの描画コマンドを組む
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon_->GetList().Get());

#endif // USE_IMGUI
        

        dxCommon_->PostDraw();

        sceneManager_->ChangeScene();
    }
}

void Game::TransitionResource(DirectXCommon* dxCommon, ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = stateAfter;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    dxCommon->GetList()->ResourceBarrier(1, &barrier);
}

void Game::Finalize() {

#ifdef USE_IMGUI
    // 実際のcommandListのImGuiの描画コマンドを組む
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

#endif // USE_IMGUI

    TextureManager::GetInstance()->Finalize();
    ModelManager::GetInstance()->Finalize();

    CloseHandle(dxCommon_->GetFenceEvent());

    dxCommon_->Release();
    WinApp::GetInstance()->Finalize();

    CoUninitialize();
    MFShutdown();
}