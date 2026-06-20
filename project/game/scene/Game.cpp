#include "Game.h"
#include "SceneManager.h"
#include "Audio.h"
#include "TextRenderer.h"
#include <chrono>
#include <thread>

bool Game::Initialize() {

    CoInitializeEx(0, COINIT_MULTITHREADED);

    Dump dump;
    SetUnhandledExceptionFilter(dump.Export);

    WinApp::GetInstance()->Initialize();

    InitializeEngine();
    InitializeImGui();
    LoadResources();

    SceneManager::GetInstance()->Initialize("TITLE");

    rtvManager_ = std::make_unique<RtvManager>();
    rtvManager_->Initialize(dxCommon_.get());

    bloom_ = std::make_unique<Bloom>();
	bloom_->Initialize(dxCommon_.get(), srvManager_.get(), rtvManager_.get());

    ParticleManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());

    // 音声読み込み
    Audio::GetInstance()->Initialize();
    Audio::GetInstance()->LoadAudio(L"BGM", L"resources/BGM_shining_star.mp3");
    Audio::GetInstance()->LoadAudio(L"bulletShoot", L"resources/bulletShoot.mp3", 5);
    
    // 再生
    //Audio::GetInstance()->PlayAudio(L"BGM", true, 0.1f);

    return true;
}

void Game::InitializeEngine() {

    dxCommon_ = std::make_unique<DirectXCommon>();
    dxCommon_->Initialize(WinApp::GetInstance());

    srvManager_ = std::make_unique<SrvManager>();
    srvManager_->Initialize(dxCommon_.get());
    
    shadow_ = std::make_unique<Shadow>();
    shadow_->Initialize(dxCommon_.get(), srvManager_.get());

    TextureManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());
    ModelManager::GetInstance()->Initialize(dxCommon_.get());

    Object3dCommon::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get(), shadow_->GetShadowMap());
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
    {
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImFontConfig fontConfig{};
        fontConfig.MergeMode = false;
        const ImWchar* japaneseRanges = io.Fonts->GetGlyphRangesJapanese();
        ImFont* japaneseFont = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/meiryo.ttc", 18.0f, &fontConfig, japaneseRanges);
        if (!japaneseFont) {
            io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msgothic.ttc", 18.0f, &fontConfig, japaneseRanges);
        }
    }
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(WinApp::GetInstance()->GetHwnd());

    ImGui_ImplDX12_InitInfo initInfo{};
    initInfo.Device = dxCommon_->GetDevice().Get();
    initInfo.CommandQueue = dxCommon_->GetQueue().Get();
    initInfo.NumFramesInFlight = dxCommon_->GetSwapChainDesc().BufferCount;
    initInfo.RTVFormat = dxCommon_->GetRtvDesc().Format;
    initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    initInfo.SrvDescriptorHeap = srvManager_->GetSrvHeap().Get();
    initInfo.UserData = srvManager_.get();
    initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle) {
        SrvManager* srvManager = static_cast<SrvManager*>(info->UserData);
        const uint32_t srvIndex = srvManager->Allocate();
        *outCpuHandle = srvManager->GetCPUDescriptorHandle(srvIndex);
        *outGpuHandle = srvManager->GetGPUDescriptorHandle(srvIndex);
    };
    initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {
    };
    ImGui_ImplDX12_Init(&initInfo);

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
        "jewelry.obj",
        "ground.obj",
        "weapon.obj",
        "player3D.obj",
        "enemy3D.obj",
        "expBlock.obj",
        "expTriangle.obj",
        "expEnemy.obj",
        "expPentagon.obj",
        "gunBarrel.obj",
        "bullet.obj",
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
		const auto frameStart = std::chrono::steady_clock::now();
		auto elapsedMs = [](auto start, auto end) {
			return std::chrono::duration<float, std::milli>(end - start).count();
		};

		const auto messageStart = std::chrono::steady_clock::now();
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (msg.message == WM_QUIT) {
            break;
        }
		const float messagePumpMs = elapsedMs(messageStart, std::chrono::steady_clock::now());
        // インプットインスタンスを取得
        Input* input = Input::GetInstance();
        WinApp* winApp = WinApp::GetInstance();

        if (winApp->ConsumeActivationChanged()) {
            input->OnFocusChanged(winApp->IsActive());
            dxCommon_->ResetFixFPS();
        }
        if (!winApp->IsActive()) {
            dxCommon_->ResetFixFPS();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

		const auto inputImGuiStart = std::chrono::steady_clock::now();
		// 前のフレームのキー状態を保存
        input->BeforeFrameData();

#ifdef USE_IMGUI

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

#endif // USE_IMGUI
		const float inputImGuiBeginMs = elapsedMs(inputImGuiStart, std::chrono::steady_clock::now());

		const auto engineUpdateStart = std::chrono::steady_clock::now();
        bloom_->Update();

        if (input->IsPress(input->GetKey()[DIK_LSHIFT]) && input->IsTrigger(input->GetKey()[DIK_D], input->GetPreKey()[DIK_D])) {
            if (Object3dCommon::GetInstance()->GetIsDebugCamera()) {
                Object3dCommon::GetInstance()->SetIsDebugCamera(false);
            } else {
                Object3dCommon::GetInstance()->SetIsDebugCamera(true);
            }
        }

        Object3dCommon::GetInstance()->Update();
		const float engineUpdateMs = elapsedMs(engineUpdateStart, std::chrono::steady_clock::now());
		const auto sceneUpdateStart = std::chrono::steady_clock::now();
        SceneManager::GetInstance()->Update();
		const float sceneUpdateMs = elapsedMs(sceneUpdateStart, std::chrono::steady_clock::now());
        bloom_->SetGrayscaleEnabled(SceneManager::GetInstance()->GetFinalDeltaTime() < (1.0f / 60.0f) * 0.98f);
        bloom_->SetGaussianOverride(SceneManager::GetInstance()->GetPostGaussianIntensity());
        const IScene::PostEffectPulse postPulse = SceneManager::GetInstance()->GetPostEffectPulse();
        bloom_->SetTransientPulse(
            postPulse.bloomBoost,
            postPulse.chromAbAmount,
            postPulse.center,
            postPulse.radius,
            postPulse.width,
            postPulse.strength);
        
		const auto imguiBuildStart = std::chrono::steady_clock::now();
#ifdef USE_IMGUI
        // ImGuiの内部コマンドを生成する
        ImGui::Render();

#endif // USE_IMGUI
		const float imguiBuildMs = elapsedMs(imguiBuildStart, std::chrono::steady_clock::now());

		const auto drawSetupStart = std::chrono::steady_clock::now();
        dxCommon_->PreDraw(); // バックバッファのバリアはここで行われている
        srvManager_->PreDraw();
       
        shadow_->PreDraw();
        
        bloom_->PreDraw();
		const float drawSetupMs = elapsedMs(drawSetupStart, std::chrono::steady_clock::now());

        IScene::RenderProfile renderProfile{};
        auto measureMs = [](auto&& func) {
            const auto start = std::chrono::steady_clock::now();
            func();
            const auto end = std::chrono::steady_clock::now();
            return std::chrono::duration<float, std::milli>(end - start).count();
        };

		const auto drawRecordStart = std::chrono::steady_clock::now();
		renderProfile.scenePostMs = measureMs([&]() {
            SceneManager::GetInstance()->DrawPostEffect3D(); // ここで Object3d::Draw が呼ばれる
        });

        renderProfile.globalBloomMs = measureMs([&]() {
            bloom_->PostDraw();
        });

        renderProfile.afterPostMs = measureMs([&]() {
            SceneManager::GetInstance()->DrawAfterPostEffect3D();
        });

        renderProfile.spriteMs = measureMs([&]() {
            SpriteCommon::GetInstance()->PreDraw(kNormal);
            SceneManager::GetInstance()->DrawSprite();
        });
		renderProfile.drawRecordMs = elapsedMs(drawRecordStart, std::chrono::steady_clock::now());


		const auto imguiDrawStart = std::chrono::steady_clock::now();
#ifdef USE_IMGUI
        // 実際のcommandListのImGuiの描画コマンドを組む
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon_->GetList().Get());

#endif // USE_IMGUI
		renderProfile.imguiDrawMs = elapsedMs(imguiDrawStart, std::chrono::steady_clock::now());

		const auto postDrawStart = std::chrono::steady_clock::now();
        dxCommon_->PostDraw();
		renderProfile.postDrawMs = elapsedMs(postDrawStart, std::chrono::steady_clock::now());
		const auto& submit = dxCommon_->GetFrameSubmitProfile();
		renderProfile.submitCloseMs = submit.closeMs;
		renderProfile.submitExecuteMs = submit.executeMs;
		renderProfile.presentMs = submit.presentMs;
		renderProfile.fenceWaitMs = submit.fenceWaitMs;
		renderProfile.fpsLimitMs = submit.fpsLimitMs;
		renderProfile.submitResetMs = submit.resetMs;
		renderProfile.messagePumpMs = messagePumpMs;
		renderProfile.inputImGuiBeginMs = inputImGuiBeginMs;
		renderProfile.engineUpdateMs = engineUpdateMs;
		renderProfile.sceneUpdateMs = sceneUpdateMs;
		renderProfile.imguiBuildMs = imguiBuildMs;
		renderProfile.drawSetupMs = drawSetupMs;
		renderProfile.frameTotalMs = elapsedMs(frameStart, std::chrono::steady_clock::now());
		SceneManager::GetInstance()->SetRenderProfile(renderProfile);
    }
}

void Game::Finalize() {

#ifdef USE_IMGUI
    // 実際のcommandListのImGuiの描画コマンドを組む
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

#endif // USE_IMGUI

    TextureManager::GetInstance()->Finalize();
    TextRenderer::GetInstance()->Finalize();
    ModelManager::GetInstance()->Finalize();

    CloseHandle(dxCommon_->GetFenceEvent());

    dxCommon_->Release();
    WinApp::GetInstance()->Finalize();

    CoUninitialize();
    MFShutdown();
}
