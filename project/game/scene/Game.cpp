#include "Game.h"
#include "SceneManager.h"

bool Game::Initialize() {

    CoInitializeEx(0, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);

    Dump dump;
    SetUnhandledExceptionFilter(dump.Export);

    WinApp::GetInstance()->Initialize();

    InitializeEngine();
    InitializeImGui();
    LoadResources();

    sceneManager_ = std::make_unique<SceneManager>();
    sceneManager_->Initialize();

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

}

void Game::LoadResources() {

    const char* models[] = {
        "teapot.obj",
        "bunny.obj",
        "plane.obj",
        "cube.obj",
        "cubeDamage.obj",
        "axis.obj",
        "player.obj",
        "playerBullet.obj",
        "enemy.obj",
        "enemyBullet.obj",
        "playerParticle.obj",
        "enemyParticle.obj"
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

        Input* input = Input::GetInstance();
        input->BeforeFrameData();

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // デバッグカメラ切替
        if (input->IsPress(input->GetKey()[DIK_LSHIFT]) &&
            input->IsTrigger(input->GetKey()[DIK_D], input->GetPreKey()[DIK_D])) {

            auto objCommon = Object3dCommon::GetInstance();
            objCommon->SetIsDebugCamera(!objCommon->GetIsDebugCamera());
        }

        sceneManager_->Update();

        ImGui::Render();

        dxCommon_->PreDraw();
        srvManager_->PreDraw();

        sceneManager_->Draw();

        SpriteCommon::GetInstance()->PreDraw();
        sceneManager_->DrawSprite();

        ImGui_ImplDX12_RenderDrawData(
            ImGui::GetDrawData(),
            dxCommon_->GetList().Get()
        );

        dxCommon_->PostDraw();

        sceneManager_->ChangeScene();
    }
}

void Game::Finalize() {

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    TextureManager::GetInstance()->Finalize();
    ModelManager::GetInstance()->Finalize();

    CloseHandle(dxCommon_->GetFenceEvent());

    dxCommon_->Release();
    WinApp::GetInstance()->Finalize();

    CoUninitialize();
}
