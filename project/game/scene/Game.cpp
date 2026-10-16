#include "Dump.h"
#include "Game.h"
#include "DeveloperTools.h"
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "game/debug/GameplayScenarioSession.h"
#endif
#include "SceneFactory.h"
#include "SceneManager.h"
#include "../modules/GameModuleBootstrap.h"
#include "../runtime/GameModuleRegistry.h"
#include "../runtime/SceneRegistry.h"
#include "Audio.h"
#include "TextRenderer.h"
#include "TextLabel.h"
#include "RuntimeProfiler.h"
#include "StartupTrace.h"
#include "BackgroundLoader.h"
#include "SkinCluster.h"
#include "TitleScene.h"
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "game/debug/NeonShowcaseCapture.h"
#endif
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr const char* kDefaultProjectFilePath = "resources/projects/default.project.json";
constexpr const char* kFallbackGameModuleId = "builtin";
constexpr const char* kFallbackSceneName = "TITLE";

/// @brief Windowsの実際のインストール先からUI用のフォントを探す。
std::filesystem::path GetSystemFontPath(const wchar_t* filename)
{
    std::array<wchar_t, MAX_PATH> directory{};
    const UINT length = GetWindowsDirectoryW(directory.data(), static_cast<UINT>(directory.size()));
    if (length == 0 || length >= directory.size()) return {};
    return std::filesystem::path(directory.data()) / L"Fonts" / filename;
}

/// @brief プロジェクト元データ文字を返す。
std::string GetProjectSourceLabel(const GameProject& project)
{
	return project.sourceFilePath.empty() ? "<built-in defaults>" : project.sourceFilePath;
}

/// @brief シーン名の一覧を診断用の文字列にする。
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

/// @brief 有効プロジェクトを診断ログへ出力する。
void LogActiveProject(const GameProject& project, bool loadedFromFallback)
{
	myEngine::LogWrite().Log(
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
			myEngine::LogWrite().Log(
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
				myEngine::LogWrite().Log(
					"[GameProject] Explicit project file is the default project file and "
					"failed to load. Using built-in safe defaults.\n");
			} else {
				myEngine::LogWrite().Log(
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
	myEngine::LogWrite().Log(
		"[GameProject] Default project file could not be loaded. "
		"Using built-in safe defaults.\n");
	LogActiveProject(activeProject_, activeProjectLoadedFromFallback_);
}

bool Game::ConfigureGameModuleAndSceneFactory()
{
	GameModuleRegistry moduleRegistry;
	if (!RegisterAvailableGameModules(moduleRegistry)) {
		myEngine::LogWrite().Log(
			"[GameModule] FATAL: failed to register the available game modules. "
			"The game will not enter the main loop.\n");
		return false;
	}

	gameModuleUsedFallback_ = false;
	std::unique_ptr<IGameModule> selectedModule =
		moduleRegistry.Create(activeProject_.gameModule);
	if (!selectedModule) {
		const std::vector<std::string> registeredModuleIds =
			moduleRegistry.GetRegisteredIds();
		myEngine::LogWrite().Log(
			"[GameModule] Requested game module is not available. projectName='" +
			activeProject_.projectName + "', projectFile='" +
			GetProjectSourceLabel(activeProject_) + "', requestedModule='" +
			activeProject_.gameModule + "', registeredModules=[" +
			JoinSceneNames(registeredModuleIds) + "].\n");

		if (activeProject_.gameModule != kFallbackGameModuleId) {
			myEngine::LogWrite().Log(
				"[GameModule] Falling back to game module '" +
				std::string(kFallbackGameModuleId) + "'.\n");
			selectedModule = moduleRegistry.Create(kFallbackGameModuleId);
			gameModuleUsedFallback_ = true;
		}
	}

	if (!selectedModule) {
		myEngine::LogWrite().Log(
			"[GameModule] FATAL: requested game module '" +
			activeProject_.gameModule + "' could not be created and required fallback "
			"module '" + std::string(kFallbackGameModuleId) +
			"' is unavailable. The game will not enter the main loop.\n");
		return false;
	}

	SceneRegistry sceneRegistry;
	if (!selectedModule->RegisterScenes(sceneRegistry)) {
		myEngine::LogWrite().Log(
			"[GameModule] FATAL: game module '" +
			std::string(selectedModule->GetId()) +
			"' failed to register its scenes. The game will not enter the main loop.\n");
		return false;
	}

	const std::vector<std::string> registeredSceneNames =
		sceneRegistry.GetRegisteredNames();
	auto sceneFactory = std::make_unique<SceneFactory>(std::move(sceneRegistry));
	if (!SceneManager::GetInstance()->SetSceneFactory(std::move(sceneFactory))) {
		myEngine::LogWrite().Log(
			"[GameModule] FATAL: failed to inject the scene factory for game module '" +
			std::string(selectedModule->GetId()) +
			"'. The game will not enter the main loop.\n");
		return false;
	}

	myEngine::LogWrite().Log(
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
	myEngine::LogWrite().Log(
		"[GameProject] Startup scene is not registered. projectName='" +
		activeProject_.projectName + "', projectFile='" +
		GetProjectSourceLabel(activeProject_) + "', startupScene='" +
		activeProject_.startupScene + "', registeredScenes=[" +
		JoinSceneNames(registeredNames) + "].\n");

	if (sceneManager->ContainsScene(kFallbackSceneName)) {
		resolvedStartupScene_ = kFallbackSceneName;
		startupSceneUsedFallback_ = true;
		myEngine::LogWrite().Log(
			"[GameProject] Falling back to startup scene 'TITLE'.\n");
		return true;
	}

	resolvedStartupScene_.clear();
	myEngine::LogWrite().Log(
		"[GameProject] FATAL: startup scene '" + activeProject_.startupScene +
		"' is unavailable and fallback scene 'TITLE' is not registered. "
		"The game will not enter the main loop.\n");
	return false;
}

bool Game::Initialize(const GameProjectCommandLineOptions& projectOptions) {
    myEngine::StartupTrace::Scope startupScope("Game.Initialize");

    LoadActiveProject(projectOptions);
    if (!ConfigureGameModuleAndSceneFactory()) {
        return false;
    }
    if (!ResolveStartupScene()) {
        return false;
    }

    CoInitializeEx(0, COINIT_MULTITHREADED);

    myEngine::Dump dump;
    SetUnhandledExceptionFilter(dump.Export);

    myEngine::WinApp::GetInstance()->Initialize();

    char asyncFlag[8]{};
    titleFirst_ = resolvedStartupScene_ == "TITLE" &&
        !(GetEnvironmentVariableA("MYENGINE_ASYNC_STARTUP", asyncFlag, sizeof(asyncFlag)) == 1 && asyncFlag[0] == '0');
    // File decoding needs no device, so it overlaps D3D12 device and pipeline creation.
    if (resolvedStartupScene_ == "TITLE") TitleScene::StartAssetPreloads();
    InitializeEngine();
    if (!titleFirst_) InitializeImGui();
    myEngine::StartupTrace::Count("build.developer_tools", myEngine::kDeveloperTools ? 1 : 0);
    myEngine::StartupTrace::Count("ui.imgui_initialized", imguiInitialized_ ? 1 : 0);
    LoadResources();

    SceneManager* sceneManager = SceneManager::GetInstance();
    bool sceneInitialized = sceneManager->Initialize(resolvedStartupScene_);
    if (!sceneInitialized &&
        resolvedStartupScene_ != kFallbackSceneName &&
        sceneManager->ContainsScene(kFallbackSceneName)) {
        myEngine::LogWrite().Log(
            "[GameProject] Failed to create startup scene '" + resolvedStartupScene_ +
            "'. Falling back to 'TITLE'.\n");
        resolvedStartupScene_ = kFallbackSceneName;
        startupSceneUsedFallback_ = true;
        sceneInitialized = sceneManager->Initialize(resolvedStartupScene_);
    }
    if (!sceneInitialized) {
        myEngine::LogWrite().Log(
            "[GameProject] FATAL: no startup scene could be initialized. "
            "The game will not enter the main loop.\n");
        return false;
    }
    myEngine::LogWrite().Log(
        "[GameProject] Startup scene initialized: '" + resolvedStartupScene_ + "'.\n");

    if (titleFirst_) {
        presentationTask_ = InitializePresentation();
    } else {
        auto presentation = InitializePresentation();
        while (!presentation.Advance()) {}
        engineReady_ = true;
        sceneManager->OnEngineReady();
    }
    return true;
}

myEngine::InitializationTask Game::InitializePresentation()
{
    if (titleFirst_) InitializeImGui();
    co_await std::suspend_always{};
    rtvManager_ = std::make_unique<myEngine::RtvManager>();
    rtvManager_->Initialize(dxCommon_.get());

    co_await std::suspend_always{};
    bloom_ = std::make_unique<myEngine::Bloom>();
	bloom_->Initialize(dxCommon_.get(), srvManager_.get(), rtvManager_.get());
    char stressCount[16]{};
    if (myEngine::kDeveloperTools && GetEnvironmentVariableA("MYENGINE_PERF_STRESS_TRAILS", stressCount, sizeof(stressCount)) > 0) {
        const unsigned count = static_cast<unsigned>((std::clamp)(std::atoi(stressCount), 0, 512));
        if (count > 0) {
            trailStress_ = std::make_unique<myEngine::TrailStressFixture>();
            trailStress_->Initialize(dxCommon_.get(), myEngine::Object3dCommon::GetInstance(), count);
        }
    }

    co_await std::suspend_always{};
    myEngine::ParticleManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());

    co_await std::suspend_always{};
    // 音声読み込み
    myEngine::Audio::GetInstance()->Initialize();
    myEngine::Audio::GetInstance()->LoadAudio(L"bulletShoot", L"resources/bulletShoot.mp3", 5);

    myEngine::StartupTrace::Mark("engine.ready");
}

void Game::InitializeEngine() {
    myEngine::StartupTrace::Scope startupScope("Engine.Initialize");
    const auto timed = [](const char* name, auto&& action) { myEngine::StartupTrace::Scope scope(name); action(); };

    dxCommon_ = std::make_unique<myEngine::DirectXCommon>();
    timed("Engine.DirectX", [&] { dxCommon_->Initialize(myEngine::WinApp::GetInstance(), titleFirst_); });
    myEngine::RuntimeProfiler::Get().Initialize(dxCommon_.get());
    char frameLimit[8]{};
    if (GetEnvironmentVariableA("MYENGINE_FRAME_LIMIT", frameLimit, sizeof(frameLimit)) == 1 && frameLimit[0] == '0') {
        dxCommon_->SetFrameLimitEnabled(false);
    }

    srvManager_ = std::make_unique<myEngine::SrvManager>();
    srvManager_->Initialize(dxCommon_.get());

    shadow_ = std::make_unique<myEngine::Shadow>();
    timed("Engine.Shadow", [&] { shadow_->Initialize(dxCommon_.get(), srvManager_.get()); });

    myEngine::TextureManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());
    myEngine::ModelManager::GetInstance()->Initialize(dxCommon_.get());

    timed("Engine.Object3d", [&] { myEngine::Object3dCommon::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get(), shadow_->GetShadowMap()); });
    timed("Engine.Sprite", [&] { myEngine::SpriteCommon::GetInstance()->Initialize(dxCommon_.get()); });
    // Sprite shares the Object shader. Its unlit path still needs valid root
    // bindings before any 3D world has supplied camera/light/wake constants.
    startupConstants_ = dxCommon_->CreateBufferResource(1024);
    void* startupData = nullptr;
    startupConstants_->Map(0, nullptr, &startupData);
    std::memset(startupData, 0, 1024);
    startupConstants_->Unmap(0, nullptr);

    myEngine::Input::GetInstance()->Initialize(
        myEngine::WinApp::GetInstance()->GetWindowClass(),
        myEngine::WinApp::GetInstance()->GetHwnd()
    );
}

void Game::InitializeImGui() {
    myEngine::StartupTrace::Scope startupScope("Engine.ImGui");


#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
#if !defined(USE_IMGUI)
    if (!myEngine::RuntimeProfiler::Get().IsAllowed()) return;
#endif

    // Imguiの初期化
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    imguiInitialized_ = true;
    {
        ImGuiIO& io = ImGui::GetIO();
#ifdef USE_IMGUI
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#else
        io.IniFilename = nullptr;
#endif

        ImFontConfig fontConfig{};
        fontConfig.MergeMode = false;
        const ImWchar* japaneseRanges = io.Fonts->GetGlyphRangesJapanese();
        for (const auto* filename : {L"meiryo.ttc", L"msgothic.ttc"}) {
            const auto fontPath = GetSystemFontPath(filename);
            std::error_code fontError;
            if (fontPath.empty() || !std::filesystem::is_regular_file(fontPath, fontError)) continue;
            const auto utf8Path = fontPath.u8string();
            const std::string imguiPath(utf8Path.begin(), utf8Path.end());
            if (io.Fonts->AddFontFromFileTTF(imguiPath.c_str(), 18.0f, &fontConfig, japaneseRanges)) break;
        }
    }
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(myEngine::WinApp::GetInstance()->GetHwnd());

    ImGui_ImplDX12_InitInfo initInfo{};
    initInfo.Device = dxCommon_->GetDevice().Get();
    initInfo.CommandQueue = dxCommon_->GetQueue().Get();
    initInfo.NumFramesInFlight = dxCommon_->GetSwapChainDesc().BufferCount;
    initInfo.RTVFormat = dxCommon_->GetRtvDesc().Format;
    initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    initInfo.SrvDescriptorHeap = srvManager_->GetSrvHeap().Get();
    initInfo.UserData = srvManager_.get();
    initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle) {
        myEngine::SrvManager* srvManager = static_cast<myEngine::SrvManager*>(info->UserData);
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

int Game::Run() {
    return MainLoop();
}

int Game::MainLoop() {
    const auto testEnabled = [](const char* name) {
        char flag[8]{};
        return GetEnvironmentVariableA(name, flag, sizeof(flag)) == 1 && flag[0] == '1';
    };
    // Hidden deterministic verification must not pause when the user switches
    // applications. Ordinary interactive play still suspends on lost focus.
    const bool startupValidation = testEnabled("MYENGINE_STARTUP_AUTOTEST");
    char experienceMode[8]{};
    const bool experienceValidation = GetEnvironmentVariableA("MYENGINE_TANK_EXPERIENCE_AUTOTEST", experienceMode, sizeof(experienceMode)) == 1 &&
        (experienceMode[0] == '1' || experienceMode[0] == '2');
    const bool backgroundValidation = startupValidation || testEnabled("MYENGINE_TITLE_AUTOTEST") ||
        testEnabled("MYENGINE_TANK_TUTORIAL_AUTOTEST") || testEnabled("MYENGINE_TANK_AUTOTEST") || testEnabled("MYENGINE_TANK_MAP_AUTOTEST") ||
        testEnabled("MYENGINE_TANK_COMBAT_AUTOTEST") || testEnabled("MYENGINE_TANK_SPECIAL_AUTOTEST") ||
        testEnabled("MYENGINE_NEON_BOSS_AUTOTEST") || testEnabled("MYENGINE_SUBMISSION_AUTOTEST") ||
        testEnabled("MYENGINE_WINDMILL_AUTOTEST") || experienceValidation
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
        || GameplayScenarioSession::Get().IsActive()
#endif
        ;
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (GameplayScenarioSession::Get().IsActive() && SceneManager::GetInstance()->GetCurrentSceneName() != "TANK_EXPEDITION") {
        auto& session = GameplayScenarioSession::Get();
        session.Fail("Gameplay scenarios require TANK_EXPEDITION; launch with --project resources/projects/tank_expedition.project.json.");
        session.Finish();
        PostQuitMessage(9);
    }
#endif
    MSG msg{};
    std::string lastPresentedScene;
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    NeonShowcaseCapture startupCapture;
    const bool captureStartup = testEnabled("MYENGINE_STARTUP_CAPTURE");
#endif
    unsigned startupValidationFrames = 0;
    while (msg.message != WM_QUIT) {
		const auto frameStart = std::chrono::steady_clock::now();
		auto elapsedMs = [](auto start, auto end) {
			return std::chrono::duration<float, std::milli>(end - start).count();
		};

		const auto messageStart = std::chrono::steady_clock::now();
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            // Preserve the quit status before a later queued message can replace it.
            if (msg.message == WM_QUIT) {
                break;
            }
            if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) {
                // DIK scan codes use bit 7 for extended keys (arrows, numpad Enter).
                unsigned int rawScan = static_cast<unsigned int>((msg.lParam >> 16) & 0xff);
                bool extended = (msg.lParam & (1LL << 24)) != 0;
                // Software keyboards can supply only a virtual key. Preserve
                // the same menu behavior when the hardware scan code is absent.
                if (!rawScan) {
                    rawScan = MapVirtualKeyW(static_cast<UINT>(msg.wParam), MAPVK_VK_TO_VSC_EX);
                    extended = (rawScan & 0xff00u) == 0xe000u;
                }
                const unsigned int scanCode = (rawScan & 0x7fu) | (extended ? 0x80u : 0u);
                myEngine::Input::GetInstance()->RecordKeyDown(scanCode, (msg.lParam & (1LL << 30)) != 0);
            }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_F1 && !(msg.lParam & (1LL << 30))) {
                myEngine::RuntimeProfiler::Get().HandleShortcut((GetKeyState(VK_SHIFT) & 0x8000) != 0);
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            // Clear stale keys at the focus event itself, before any subsequent
            // key-down in the same message batch can be recorded.
            if (myEngine::WinApp::GetInstance()->ConsumeActivationChanged()) {
                myEngine::Input::GetInstance()->OnFocusChanged(myEngine::WinApp::GetInstance()->IsActive());
                dxCommon_->ResetFixFPS();
            }
        }
        if (msg.message == WM_QUIT) {
            break;
        }
		const float messagePumpMs = elapsedMs(messageStart, std::chrono::steady_clock::now());
        // インプットインスタンスを取得
        myEngine::Input* input = myEngine::Input::GetInstance();
        myEngine::WinApp* winApp = myEngine::WinApp::GetInstance();

        if (!winApp->IsActive() && !backgroundValidation) {
            dxCommon_->ResetFixFPS();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

		const auto inputImGuiStart = std::chrono::steady_clock::now();
		// 前のフレームのキー状態を保存
        input->BeforeFrameData();
        // Device switch: re-render operation prompts now, outside of draw command recording.
        myEngine::TextLabel::RefreshAllInputPrompts();
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
        if (GameplayScenarioSession::Get().IsActive()) input->OverrideValidationFrame({640.0f, 360.0f});
#endif
        auto* scenes = SceneManager::GetInstance();
        if (!engineReady_ || scenes->UsesSimpleRendering()) {
            // Always present one title frame before doing deferred work. This
            // branch also pumps messages/input every frame during scene loading.
            if (!engineReady_ && !lastPresentedScene.empty()) {
                // The title stays black until its demo is ready, so run the
                // remaining engine steps in ~100 ms slices instead of one per vsync.
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
                while (!engineReady_ && std::chrono::steady_clock::now() < deadline) {
                    if (!dxCommon_->ArePipelinesReady()) dxCommon_->InitializePipelinesStep();
                    else if (presentationTask_.Advance()) {
                        engineReady_ = true;
                        scenes->OnEngineReady();
                    }
                }
            }
            scenes->Update();
            if (scenes->GetCurrentSceneName() != "TITLE") continue;
            dxCommon_->PreDraw();
            srvManager_->PreDraw();
            myEngine::SpriteCommon::GetInstance()->PreDraw(myEngine::kNormal);
            auto* list = dxCommon_->GetList().Get();
            for (const unsigned slot : {3u, 4u, 5u, 6u, 17u})
                list->SetGraphicsRootConstantBufferView(slot, startupConstants_->GetGPUVirtualAddress());
            auto* textures = myEngine::TextureManager::GetInstance();
            const auto black = textures->GetSrvHandleGPU(myEngine::TextureManager::GetBlackCausticsTexturePath(),
                myEngine::TextureManager::TextureColorSpace::LinearData);
            list->SetGraphicsRootDescriptorTable(7, black);
            list->SetGraphicsRootDescriptorTable(18, black);
            list->SetGraphicsRootDescriptorTable(8, textures->GetSrvHandleGPU(
                myEngine::TextureManager::GetPbrPrefilteredEnvironmentTexturePath(), myEngine::TextureManager::TextureColorSpace::LinearData));
            scenes->DrawSprite();
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
            if (captureStartup && lastPresentedScene.empty())
                startupCapture.Request("generated/async_startup_capture", "first_title", {{"stage", "first_title"}});
            startupCapture.Record(*dxCommon_);
#endif
            dxCommon_->PostDraw();
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
            startupCapture.Resolve(*dxCommon_);
#endif
            if (lastPresentedScene != "TITLE") {
                myEngine::StartupTrace::Mark("first_frame.TITLE");
                myEngine::StartupTrace::Flush();
                lastPresentedScene = "TITLE";
            }
            continue;
        }
        auto& runtime = myEngine::RuntimeProfiler::Get();
        runtime.BeginFrame();
        const int gpuFrame = runtime.BeginGpu("GPU frame");

#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
        if (imguiInitialized_) {

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
#ifdef USE_IMGUI
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
#endif
        }

#endif // USE_IMGUI
		const float inputImGuiBeginMs = elapsedMs(inputImGuiStart, std::chrono::steady_clock::now());

		const auto engineUpdateStart = std::chrono::steady_clock::now();
        bloom_->Update();

#ifdef USE_IMGUI
        if (input->IsPress(input->GetKey()[DIK_LCONTROL]) && input->IsPress(input->GetKey()[DIK_LSHIFT]) && input->IsTrigger(input->GetKey()[DIK_D], input->GetPreKey()[DIK_D])) {
            if (myEngine::Object3dCommon::GetInstance()->GetIsDebugCamera()) {
                myEngine::Object3dCommon::GetInstance()->SetIsDebugCamera(false);
            } else {
                myEngine::Object3dCommon::GetInstance()->SetIsDebugCamera(true);
            }
        }
#endif // USE_IMGUI

        myEngine::Object3dCommon::GetInstance()->Update();
		const float engineUpdateMs = elapsedMs(engineUpdateStart, std::chrono::steady_clock::now());
		const auto sceneUpdateStart = std::chrono::steady_clock::now();
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
        const auto depthSceneEpochBefore = GameplayScenarioSession::Get().GetSceneEpoch();
        const auto depthSceneNameBefore = SceneManager::GetInstance()->GetCurrentSceneName();
#endif
        SceneManager::GetInstance()->Update();
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
        auto& depthSession = GameplayScenarioSession::Get();
        const auto& actualSceneName = SceneManager::GetInstance()->GetCurrentSceneName();
        if (depthSession.IsActive() && gameplaytest::IsNeonDepthScenario(depthSession.GetSettings().scenario) &&
            (actualSceneName != depthSceneNameBefore || depthSession.GetSceneEpoch() != depthSceneEpochBefore)) {
            if (depthSession.NotifyDepthSceneEntered(actualSceneName)) PostQuitMessage(depthSession.GetErrors().empty()?0:9);
        }
#endif
        if (trailStress_) {
            myEngine::RuntimeProfiler::CpuScope scope("Stress trails update");
            trailStress_->Update(1.0f/60.0f);
        }
		const float sceneUpdateMs = elapsedMs(sceneUpdateStart, std::chrono::steady_clock::now());
        const auto developerShowcase = SceneManager::GetInstance()->GetDeveloperShowcaseState();
        const float combatDeltaTime = SceneManager::GetInstance()->GetFinalDeltaTime();
        bool grayscale = combatDeltaTime < (1.0f / 60.0f) * 0.98f;
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
        if (GameplayScenarioSession::Get().IsActive())
            grayscale = gameplaytest::ScenarioGrayscaleEnabled(combatDeltaTime,
                GameplayScenarioSession::Get().GetSettings().fixedDeltaTime,
                developerShowcase.comparisonFreeze, developerShowcase.active);
#endif
        bloom_->SetGrayscaleEnabled(grayscale);
        bloom_->SetGaussianOverride(SceneManager::GetInstance()->GetPostGaussianIntensity());
        const IScene::PostEffectPulse postPulse = SceneManager::GetInstance()->GetPostEffectPulse();
        bloom_->SetTransientPulse(
            postPulse.bloomBoost,
            postPulse.chromAbAmount,
            postPulse.center,
            postPulse.radius,
            postPulse.width,
            postPulse.strength);
		bloom_->SetScreenEffectState(SceneManager::GetInstance()->GetScreenEffectState());
        bloom_->SetDeveloperShowcaseState(developerShowcase);

		const auto imguiBuildStart = std::chrono::steady_clock::now();
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
        if (imguiInitialized_) {
        runtime.DrawOverlay(dxCommon_->IsFrameLimitEnabled(), SceneManager::GetInstance()->GetCurrentSceneName().c_str());
        // ImGuiの内部コマンドを生成する
        ImGui::Render();
        }

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
            myEngine::RuntimeProfiler::GpuScope scope("Scene 3D (includes effects)");
            SceneManager::GetInstance()->DrawPostEffect3D(); // ここで Object3d::Draw が呼ばれる
            if (trailStress_) {
                myEngine::RuntimeProfiler::CpuScope cpuScope("Stress trails");
                myEngine::RuntimeProfiler::GpuScope gpuScope("Stress trails");
                trailStress_->Draw();
                const auto& stats = trailStress_->GetStats();
                runtime.SetCounter("Stress trails", static_cast<double>(stats.drawableInstances));
                runtime.SetCounter("Stress vertices", static_cast<double>(stats.generatedVertices));
                runtime.SetCounter("Stress draw calls", stats.drawCalls);
                runtime.SetCounter("Stress upload bytes", static_cast<double>(stats.uploadedBytes));
                runtime.SetCounter("Stress truncated vertices", static_cast<double>(stats.truncatedVertices));
            }
        });

        renderProfile.globalBloomMs = measureMs([&]() {
            myEngine::RuntimeProfiler::GpuScope scope("Global Bloom / Post");
            bloom_->PostDraw();
        });

        renderProfile.afterPostMs = measureMs([&]() {
            myEngine::RuntimeProfiler::GpuScope scope("After Post / Text Glow");
            SceneManager::GetInstance()->DrawAfterPostEffect3D();
        });

        renderProfile.spriteMs = measureMs([&]() {
            myEngine::RuntimeProfiler::GpuScope scope("2D / Game UI");
            myEngine::SpriteCommon::GetInstance()->PreDraw(myEngine::kNormal);
            SceneManager::GetInstance()->DrawSprite();
        });
        {
            myEngine::RuntimeProfiler::GpuScope scope("CRT Present");
            bloom_->PresentToBackBuffer();
        }
		renderProfile.drawRecordMs = elapsedMs(drawRecordStart, std::chrono::steady_clock::now());


		const auto imguiDrawStart = std::chrono::steady_clock::now();
        SceneManager::GetInstance()->RecordDeveloperPostParameters(bloom_->GetLastCompositeParams());
        SceneManager::GetInstance()->RecordDeveloperFrame(*dxCommon_);
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
        if (imguiInitialized_) {
        myEngine::RuntimeProfiler::GpuScope scope("Diagnostics UI");
        // 実際のcommandListのImGuiの描画コマンドを組む
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon_->GetList().Get());
        }

#endif // USE_IMGUI
		renderProfile.imguiDrawMs = elapsedMs(imguiDrawStart, std::chrono::steady_clock::now());
        runtime.EndGpu(gpuFrame);
        runtime.ResolveGpu();

		const auto postDrawStart = std::chrono::steady_clock::now();
        dxCommon_->PostDraw();

        const auto& presentedScene = SceneManager::GetInstance()->GetCurrentSceneName();
        if (lastPresentedScene != presentedScene) {
            myEngine::StartupTrace::Mark("first_frame." + presentedScene);
            myEngine::StartupTrace::Flush();
            lastPresentedScene = presentedScene;
        }
        if (startupValidation && presentedScene == "TANK_EXPEDITION" && ++startupValidationFrames >= 3) {
            PostQuitMessage(0);
        }
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
        runtime.AddCpu("Scene Update (total)", sceneUpdateMs);
        runtime.AddCpu("Engine Update", engineUpdateMs);
        runtime.AddCpu("Draw Record (total)", renderProfile.drawRecordMs);
        runtime.AddCpu("Global Bloom record", renderProfile.globalBloomMs);
        runtime.AddCpu("2D / UI record", renderProfile.spriteMs);
        runtime.AddCpu("Diagnostics UI", imguiBuildMs + renderProfile.imguiDrawMs);
        runtime.FinishFrame(dxCommon_->GetFramePacingStats().frameMs, submit.presentMs, submit.fenceWaitMs, submit.fpsLimitMs);
        char exitAfterCapture[8]{};
        if (runtime.IsCaptureComplete() && GetEnvironmentVariableA("MYENGINE_PERF_EXIT_AFTER_CAPTURE", exitAfterCapture, sizeof(exitAfterCapture)) == 1 && exitAfterCapture[0] == '1') {
            PostQuitMessage(0);
        }
    }
    return static_cast<int>(msg.wParam);
}

void Game::Finalize() {
    myEngine::BackgroundLoader::Get().Shutdown();
    presentationTask_ = {};
    myEngine::SkinnedModel::ClearPreloadedAssets();
	// The current scene owns RenderTextures and other GPU resources that keep
	// non-owning pointers to SrvManager.  Release the scene before the Game
	// members (and therefore SrvManager) begin their destruction.
	if (dxCommon_) {
		dxCommon_->ExecuteCommandListAndWait();
	}
	SceneManager::GetInstance()->Finalize();
    startupConstants_.Reset();
    trailStress_.reset();
    myEngine::RuntimeProfiler::Get().Shutdown();

#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
    if (imguiInitialized_) {
    // 実際のcommandListのImGuiの描画コマンドを組む
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    imguiInitialized_ = false;
    }

#endif // USE_IMGUI

    myEngine::TextureManager::GetInstance()->Finalize();
    myEngine::TextRenderer::GetInstance()->Finalize();
    myEngine::ModelManager::GetInstance()->Finalize();

    CloseHandle(dxCommon_->GetFenceEvent());

    dxCommon_->Release();
    myEngine::WinApp::GetInstance()->Finalize();

    CoUninitialize();
    MFShutdown();
}
