#include "Game.h"
#include "SceneFactory.h"
#include "SceneManager.h"
#include "../modules/BuiltInGameModule.h"
#include "../runtime/GameModuleRegistry.h"
#include "../runtime/SceneRegistry.h"
#include "Audio.h"
#include "TextRenderer.h"
#include <chrono>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr const char* kDefaultProjectFilePath = "resources/projects/default.project.json";
constexpr const char* kFallbackSceneName = "TITLE";

std::string GetProjectSourceLabel(const GameProject& project)
{
	return project.sourceFilePath.empty() ? "<built-in defaults>" : project.sourceFilePath;
}

std::string JoinSceneNames(const std::vector<std::string>& names)
{
	if (names.empty()) {
		return "<none>";
	}

	std::string joinedNames;
	for (std::size_t index = 0; index < names.size(); ++index) {
		if (index != 0) {
			joinedNames += ", ";
		}
		joinedNames += names[index];
	}
	return joinedNames;
}

void LogActiveProject(const GameProject& project, bool loadedFromFallback)
{
	LogWrite().Log(
		"[GameProject] Active project: name='" + project.projectName +
		"', source='" + GetProjectSourceLabel(project) +
		"', gameModule='" + project.gameModule +
		"', startupScene='" + project.startupScene +
		"', resourceRoot='" + project.resourceRoot +
		"', loadedFromFallback=" + (loadedFromFallback ? "true" : "false") +
		". resourceRoot is metadata only in this stage.\n");
}

}

void Game::LoadActiveProject(const GameProjectCommandLineOptions& projectOptions)
{
	GameProjectLoader loader;
	GameProject loadedProject;
	bool defaultProjectAlreadyAttempted = false;

	if (projectOptions.projectOptionProvided) {
		if (projectOptions.projectFilePath.empty()) {
			LogWrite().Log(
				"[GameProject] The explicit --project path is empty. "
				"Trying the default project file.\n");
		} else if (loader.Load(projectOptions.projectFilePath, loadedProject)) {
			activeProject_ = std::move(loadedProject);
			activeProjectLoadedFromFallback_ = false;
			LogActiveProject(activeProject_, activeProjectLoadedFromFallback_);
			return;
		} else {
			defaultProjectAlreadyAttempted =
				projectOptions.projectFilePath == kDefaultProjectFilePath;
			if (defaultProjectAlreadyAttempted) {
				LogWrite().Log(
					"[GameProject] Explicit project file is the default project file and "
					"failed to load. Using built-in safe defaults.\n");
			} else {
				LogWrite().Log(
					"[GameProject] Explicit project file '" + projectOptions.projectFilePath +
					"' failed to load. Trying '" + kDefaultProjectFilePath + "'.\n");
			}
		}
	}

	if (!defaultProjectAlreadyAttempted &&
		loader.Load(kDefaultProjectFilePath, loadedProject)) {
		activeProject_ = std::move(loadedProject);
		activeProjectLoadedFromFallback_ = projectOptions.projectOptionProvided;
		LogActiveProject(activeProject_, activeProjectLoadedFromFallback_);
		return;
	}

	activeProject_ = GameProject{};
	activeProjectLoadedFromFallback_ = true;
	LogWrite().Log(
		"[GameProject] Default project file could not be loaded. "
		"Using built-in safe defaults.\n");
	LogActiveProject(activeProject_, activeProjectLoadedFromFallback_);
}

bool Game::ConfigureGameModuleAndSceneFactory()
{
	GameModuleRegistry moduleRegistry;
	const std::string builtInModuleId(BuiltInGameModule::kId);
	if (!moduleRegistry.Register<BuiltInGameModule>(builtInModuleId)) {
		LogWrite().Log(
			"[GameModule] FATAL: failed to register the required built-in game module. "
			"The game will not enter the main loop.\n");
		return false;
	}

	gameModuleUsedFallback_ = false;
	std::unique_ptr<IGameModule> selectedModule =
		moduleRegistry.Create(activeProject_.gameModule);
	if (!selectedModule) {
		const std::vector<std::string> registeredModuleIds =
			moduleRegistry.GetRegisteredIds();
		LogWrite().Log(
			"[GameModule] Requested game module is not available. projectName='" +
			activeProject_.projectName + "', projectFile='" +
			GetProjectSourceLabel(activeProject_) + "', requestedModule='" +
			activeProject_.gameModule + "', registeredModules=[" +
			JoinSceneNames(registeredModuleIds) + "].\n");

		if (activeProject_.gameModule != builtInModuleId) {
			LogWrite().Log(
				"[GameModule] Falling back to game module '" + builtInModuleId + "'.\n");
			selectedModule = moduleRegistry.Create(builtInModuleId);
			gameModuleUsedFallback_ = true;
		}
	}

	if (!selectedModule) {
		LogWrite().Log(
			"[GameModule] FATAL: requested game module '" +
			activeProject_.gameModule + "' could not be created and required fallback "
			"module '" + builtInModuleId +
			"' is unavailable. The game will not enter the main loop.\n");
		return false;
	}

	SceneRegistry sceneRegistry;
	if (!selectedModule->RegisterScenes(sceneRegistry)) {
		LogWrite().Log(
			"[GameModule] FATAL: game module '" +
			std::string(selectedModule->GetId()) +
			"' failed to register its scenes. The game will not enter the main loop.\n");
		return false;
	}

	const std::vector<std::string> registeredSceneNames =
		sceneRegistry.GetRegisteredNames();
	auto sceneFactory = std::make_unique<SceneFactory>(std::move(sceneRegistry));
	if (!SceneManager::GetInstance()->SetSceneFactory(std::move(sceneFactory))) {
		LogWrite().Log(
			"[GameModule] FATAL: failed to inject the scene factory for game module '" +
			std::string(selectedModule->GetId()) +
			"'. The game will not enter the main loop.\n");
		return false;
	}

	LogWrite().Log(
		"[GameModule] Active module: id='" +
		std::string(selectedModule->GetId()) + "', displayName='" +
		std::string(selectedModule->GetDisplayName()) +
		"', loadedFromFallback=" + (gameModuleUsedFallback_ ? "true" : "false") +
		", registeredScenes=[" + JoinSceneNames(registeredSceneNames) + "].\n");
	activeGameModule_ = std::move(selectedModule);
	return true;
}

bool Game::ResolveStartupScene()
{
	SceneManager* sceneManager = SceneManager::GetInstance();
	resolvedStartupScene_ = activeProject_.startupScene;
	startupSceneUsedFallback_ = false;

	if (sceneManager->ContainsScene(resolvedStartupScene_)) {
		return true;
	}

	const std::vector<std::string> registeredNames =
		sceneManager->GetRegisteredSceneNames();
	LogWrite().Log(
		"[GameProject] Startup scene is not registered. projectName='" +
		activeProject_.projectName + "', projectFile='" +
		GetProjectSourceLabel(activeProject_) + "', startupScene='" +
		activeProject_.startupScene + "', registeredScenes=[" +
		JoinSceneNames(registeredNames) + "].\n");

	if (sceneManager->ContainsScene(kFallbackSceneName)) {
		resolvedStartupScene_ = kFallbackSceneName;
		startupSceneUsedFallback_ = true;
		LogWrite().Log(
			"[GameProject] Falling back to startup scene 'TITLE'.\n");
		return true;
	}

	resolvedStartupScene_.clear();
	LogWrite().Log(
		"[GameProject] FATAL: startup scene '" + activeProject_.startupScene +
		"' is unavailable and fallback scene 'TITLE' is not registered. "
		"The game will not enter the main loop.\n");
	return false;
}

bool Game::Initialize(const GameProjectCommandLineOptions& projectOptions) {

    LoadActiveProject(projectOptions);
    if (!ConfigureGameModuleAndSceneFactory()) {
        return false;
    }
    if (!ResolveStartupScene()) {
        return false;
    }

    CoInitializeEx(0, COINIT_MULTITHREADED);

    Dump dump;
    SetUnhandledExceptionFilter(dump.Export);

    WinApp::GetInstance()->Initialize();

    InitializeEngine();
    InitializeImGui();
    LoadResources();

    SceneManager* sceneManager = SceneManager::GetInstance();
    bool sceneInitialized = sceneManager->Initialize(resolvedStartupScene_);
    if (!sceneInitialized &&
        resolvedStartupScene_ != kFallbackSceneName &&
        sceneManager->ContainsScene(kFallbackSceneName)) {
        LogWrite().Log(
            "[GameProject] Failed to create startup scene '" + resolvedStartupScene_ +
            "'. Falling back to 'TITLE'.\n");
        resolvedStartupScene_ = kFallbackSceneName;
        startupSceneUsedFallback_ = true;
        sceneInitialized = sceneManager->Initialize(resolvedStartupScene_);
    }
    if (!sceneInitialized) {
        LogWrite().Log(
            "[GameProject] FATAL: no startup scene could be initialized. "
            "The game will not enter the main loop.\n");
        return false;
    }
    LogWrite().Log(
        "[GameProject] Startup scene initialized: '" + resolvedStartupScene_ + "'.\n");

    rtvManager_ = std::make_unique<RtvManager>();
    rtvManager_->Initialize(dxCommon_.get());

    bloom_ = std::make_unique<Bloom>();
	bloom_->Initialize(dxCommon_.get(), srvManager_.get(), rtvManager_.get());

    ParticleManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());

    // 音声読み込み
    Audio::GetInstance()->Initialize();
    //Audio::GetInstance()->LoadAudio(L"BGM", L"resources/BGM_shining_star.mp3");
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
    // Keep startup focused on the title scene. Models are now loaded on demand
    // from Object3d::SetModel() or explicitly by the scene that needs them.
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
