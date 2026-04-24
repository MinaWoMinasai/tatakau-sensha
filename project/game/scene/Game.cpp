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
        "jewelry.obj",
        "ground.obj",
        "weapon.obj",
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

#endif // USE_IMGUI

        bloom_->Update();

        if (input->IsPress(input->GetKey()[DIK_LSHIFT]) && input->IsTrigger(input->GetKey()[DIK_D], input->GetPreKey()[DIK_D])) {
            if (Object3dCommon::GetInstance()->GetIsDebugCamera()) {
                Object3dCommon::GetInstance()->SetIsDebugCamera(false);
            } else {
                Object3dCommon::GetInstance()->SetIsDebugCamera(true);
            }
        }

        Object3dCommon::GetInstance()->Update();
        SceneManager::GetInstance()->Update();
        
#ifdef USE_IMGUI
        // ImGuiの内部コマンドを生成する
        ImGui::Render();

#endif // USE_IMGUI

        dxCommon_->PreDraw(); // バックバッファのバリアはここで行われている
        srvManager_->PreDraw();
       
        shadow_->PreDraw();
        
        bloom_->PreDraw();
        
        SceneManager::GetInstance()->DrawPostEffect3D(); // ここで Object3d::Draw が呼ばれる
        
        SpriteCommon::GetInstance()->PreDraw(kNormal);
        SceneManager::GetInstance()->DrawSprite();
        
        bloom_->PostDraw();


#ifdef USE_IMGUI
        // 実際のcommandListのImGuiの描画コマンドを組む
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon_->GetList().Get());

#endif // USE_IMGUI
        
        dxCommon_->PostDraw();
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
    ModelManager::GetInstance()->Finalize();

    CloseHandle(dxCommon_->GetFenceEvent());

    dxCommon_->Release();
    WinApp::GetInstance()->Finalize();

    CoUninitialize();
    MFShutdown();
}