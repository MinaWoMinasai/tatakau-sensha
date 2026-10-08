#include "Game.h"
#include "DeveloperTools.h"
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "game/debug/GameplayScenarioSession.h"
#endif
#include "SceneFactory.h"
#include "SceneManager.h"
#include "../modules/GameModuleBootstrap.h"
#include "../runtime/GameModuleRegistry.h"
#include "../runtime/SceneRegistry.h"
#include "Audio.h"
#include "TextRenderer.h"
#include "RuntimeProfiler.h"
#include "StartupTrace.h"
#include <chrono>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr const char* kDefaultProjectFilePath = "resources/projects/default.project.json";
constexpr const char* kFallbackGameModuleId = "builtin";
constexpr const char* kFallbackSceneName = "TITLE";

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
	cg2::LogWrite().Log(
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
			cg2::LogWrite().Log(
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
				cg2::LogWrite().Log(
					"[GameProject] Explicit project file is the default project file and "
					"failed to load. Using built-in safe defaults.\n");
			} else {
				cg2::LogWrite().Log(
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
	cg2::LogWrite().Log(
		"[GameProject] Default project file could not be loaded. "
		"Using built-in safe defaults.\n");
	LogActiveProject(activeProject_, activeProjectLoadedFromFallback_);
}

bool Game::ConfigureGameModuleAndSceneFactory()
{
	GameModuleRegistry moduleRegistry;
	if (!RegisterAvailableGameModules(moduleRegistry)) {
		cg2::LogWrite().Log(
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
		cg2::LogWrite().Log(
			"[GameModule] Requested game module is not available. projectName='" +
			activeProject_.projectName + "', projectFile='" +
			GetProjectSourceLabel(activeProject_) + "', requestedModule='" +
			activeProject_.gameModule + "', registeredModules=[" +
			JoinSceneNames(registeredModuleIds) + "].\n");

		if (activeProject_.gameModule != kFallbackGameModuleId) {
			cg2::LogWrite().Log(
				"[GameModule] Falling back to game module '" +
				std::string(kFallbackGameModuleId) + "'.\n");
			selectedModule = moduleRegistry.Create(kFallbackGameModuleId);
			gameModuleUsedFallback_ = true;
		}
	}

	if (!selectedModule) {
		cg2::LogWrite().Log(
			"[GameModule] FATAL: requested game module '" +
			activeProject_.gameModule + "' could not be created and required fallback "
			"module '" + std::string(kFallbackGameModuleId) +
			"' is unavailable. The game will not enter the main loop.\n");
		return false;
	}

	SceneRegistry sceneRegistry;
	if (!selectedModule->RegisterScenes(sceneRegistry)) {
		cg2::LogWrite().Log(
			"[GameModule] FATAL: game module '" +
			std::string(selectedModule->GetId()) +
			"' failed to register its scenes. The game will not enter the main loop.\n");
		return false;
	}

	const std::vector<std::string> registeredSceneNames =
		sceneRegistry.GetRegisteredNames();
	auto sceneFactory = std::make_unique<SceneFactory>(std::move(sceneRegistry));
	if (!SceneManager::GetInstance()->SetSceneFactory(std::move(sceneFactory))) {
		cg2::LogWrite().Log(
			"[GameModule] FATAL: failed to inject the scene factory for game module '" +
			std::string(selectedModule->GetId()) +
			"'. The game will not enter the main loop.\n");
		return false;
	}

	cg2::LogWrite().Log(
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
	cg2::LogWrite().Log(
		"[GameProject] Startup scene is not registered. projectName='" +
		activeProject_.projectName + "', projectFile='" +
		GetProjectSourceLabel(activeProject_) + "', startupScene='" +
		activeProject_.startupScene + "', registeredScenes=[" +
		JoinSceneNames(registeredNames) + "].\n");

	if (sceneManager->ContainsScene(kFallbackSceneName)) {
		resolvedStartupScene_ = kFallbackSceneName;
		startupSceneUsedFallback_ = true;
		cg2::LogWrite().Log(
			"[GameProject] Falling back to startup scene 'TITLE'.\n");
		return true;
	}

	resolvedStartupScene_.clear();
	cg2::LogWrite().Log(
		"[GameProject] FATAL: startup scene '" + activeProject_.startupScene +
		"' is unavailable and fallback scene 'TITLE' is not registered. "
		"The game will not enter the main loop.\n");
	return false;
}

bool Game::Initialize(const GameProjectCommandLineOptions& projectOptions) {
    cg2::StartupTrace::Scope startupScope("Game.Initialize");

    LoadActiveProject(projectOptions);
    if (!ConfigureGameModuleAndSceneFactory()) {
        return false;
    }
    if (!ResolveStartupScene()) {
        return false;
    }

    CoInitializeEx(0, COINIT_MULTITHREADED);

    cg2::Dump dump;
    SetUnhandledExceptionFilter(dump.Export);

    cg2::WinApp::GetInstance()->Initialize();

    InitializeEngine();
    InitializeImGui();
    cg2::StartupTrace::Count("build.developer_tools", cg2::kDeveloperTools ? 1 : 0);
    cg2::StartupTrace::Count("ui.imgui_initialized", imguiInitialized_ ? 1 : 0);
    LoadResources();

    SceneManager* sceneManager = SceneManager::GetInstance();
    bool sceneInitialized = sceneManager->Initialize(resolvedStartupScene_);
    if (!sceneInitialized &&
        resolvedStartupScene_ != kFallbackSceneName &&
        sceneManager->ContainsScene(kFallbackSceneName)) {
        cg2::LogWrite().Log(
            "[GameProject] Failed to create startup scene '" + resolvedStartupScene_ +
            "'. Falling back to 'TITLE'.\n");
        resolvedStartupScene_ = kFallbackSceneName;
        startupSceneUsedFallback_ = true;
        sceneInitialized = sceneManager->Initialize(resolvedStartupScene_);
    }
    if (!sceneInitialized) {
        cg2::LogWrite().Log(
            "[GameProject] FATAL: no startup scene could be initialized. "
            "The game will not enter the main loop.\n");
        return false;
    }
    cg2::LogWrite().Log(
        "[GameProject] Startup scene initialized: '" + resolvedStartupScene_ + "'.\n");

    rtvManager_ = std::make_unique<cg2::RtvManager>();
    rtvManager_->Initialize(dxCommon_.get());

    bloom_ = std::make_unique<cg2::Bloom>();
	bloom_->Initialize(dxCommon_.get(), srvManager_.get(), rtvManager_.get());
    char stressCount[16]{};
    if (cg2::kDeveloperTools && GetEnvironmentVariableA("CG2_PERF_STRESS_TRAILS", stressCount, sizeof(stressCount)) > 0) {
        const unsigned count = static_cast<unsigned>((std::clamp)(std::atoi(stressCount), 0, 512));
        if (count > 0) {
            trailStress_ = std::make_unique<cg2::TrailStressFixture>();
            trailStress_->Initialize(dxCommon_.get(), cg2::Object3dCommon::GetInstance(), count);
        }
    }

    cg2::ParticleManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());

    // 音声読み込み
    cg2::Audio::GetInstance()->Initialize();
    cg2::Audio::GetInstance()->LoadAudio(L"bulletShoot", L"resources/bulletShoot.mp3", 5);

    return true;
}

void Game::InitializeEngine() {
    cg2::StartupTrace::Scope startupScope("Engine.Initialize");
    const auto timed = [](const char* name, auto&& action) { cg2::StartupTrace::Scope scope(name); action(); };

    dxCommon_ = std::make_unique<cg2::DirectXCommon>();
    timed("Engine.DirectX", [&] { dxCommon_->Initialize(cg2::WinApp::GetInstance()); });
    cg2::RuntimeProfiler::Get().Initialize(dxCommon_.get());
    char frameLimit[8]{};
    if (GetEnvironmentVariableA("CG2_FRAME_LIMIT", frameLimit, sizeof(frameLimit)) == 1 && frameLimit[0] == '0') {
        dxCommon_->SetFrameLimitEnabled(false);
    }

    srvManager_ = std::make_unique<cg2::SrvManager>();
    srvManager_->Initialize(dxCommon_.get());

    shadow_ = std::make_unique<cg2::Shadow>();
    timed("Engine.Shadow", [&] { shadow_->Initialize(dxCommon_.get(), srvManager_.get()); });

    cg2::TextureManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());
    cg2::ModelManager::GetInstance()->Initialize(dxCommon_.get());

    timed("Engine.Object3d", [&] { cg2::Object3dCommon::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get(), shadow_->GetShadowMap()); });
    timed("Engine.Sprite", [&] { cg2::SpriteCommon::GetInstance()->Initialize(dxCommon_.get()); });

    cg2::Input::GetInstance()->Initialize(
        cg2::WinApp::GetInstance()->GetWindowClass(),
        cg2::WinApp::GetInstance()->GetHwnd()
    );
}

void Game::InitializeImGui() {
    cg2::StartupTrace::Scope startupScope("Engine.ImGui");


#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
#if !defined(USE_IMGUI)
    if (!cg2::RuntimeProfiler::Get().IsAllowed()) return;
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
        ImFont* japaneseFont = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/meiryo.ttc", 18.0f, &fontConfig, japaneseRanges);
        if (!japaneseFont) {
            io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msgothic.ttc", 18.0f, &fontConfig, japaneseRanges);
        }
    }
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(cg2::WinApp::GetInstance()->GetHwnd());

    ImGui_ImplDX12_InitInfo initInfo{};
    initInfo.Device = dxCommon_->GetDevice().Get();
    initInfo.CommandQueue = dxCommon_->GetQueue().Get();
    initInfo.NumFramesInFlight = dxCommon_->GetSwapChainDesc().BufferCount;
    initInfo.RTVFormat = dxCommon_->GetRtvDesc().Format;
    initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    initInfo.SrvDescriptorHeap = srvManager_->GetSrvHeap().Get();
    initInfo.UserData = srvManager_.get();
    initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle) {
        cg2::SrvManager* srvManager = static_cast<cg2::SrvManager*>(info->UserData);
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
    const bool startupValidation = testEnabled("CG2_STARTUP_AUTOTEST");
    char experienceMode[8]{};
    const bool experienceValidation = GetEnvironmentVariableA("CG2_TANK_EXPERIENCE_AUTOTEST", experienceMode, sizeof(experienceMode)) == 1 &&
        (experienceMode[0] == '1' || experienceMode[0] == '2');
    const bool backgroundValidation = startupValidation || testEnabled("CG2_TITLE_AUTOTEST") ||
        testEnabled("CG2_TANK_TUTORIAL_AUTOTEST") || testEnabled("CG2_TANK_AUTOTEST") || testEnabled("CG2_TANK_MAP_AUTOTEST") ||
        testEnabled("CG2_TANK_COMBAT_AUTOTEST") || testEnabled("CG2_TANK_SPECIAL_AUTOTEST") ||
        testEnabled("CG2_NEON_BOSS_AUTOTEST") || testEnabled("CG2_SUBMISSION_AUTOTEST") ||
        testEnabled("CG2_WINDMILL_AUTOTEST") || experienceValidation
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
        || GameplayScenarioSession::Get().IsActive()
#endif
        ;
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (GameplayScenarioSession::Get().IsActive() && SceneManager::GetInstance()->GetCurrentSceneName() != "TANK_EXPEDITION") {
        auto& session = GameplayScenarioSession::Get();
        session.Fail("Gameplay scenarios require TANK_EXPEDITION; launch with --project resources/projects/tank_expedition.project.json.");
        session.Finish();
        PostQuitMessage(9);
    }
#endif
    MSG msg{};
    std::string lastPresentedScene;
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
                cg2::Input::GetInstance()->RecordKeyDown(scanCode, (msg.lParam & (1LL << 30)) != 0);
            }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_F1 && !(msg.lParam & (1LL << 30))) {
                cg2::RuntimeProfiler::Get().HandleShortcut((GetKeyState(VK_SHIFT) & 0x8000) != 0);
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            // Clear stale keys at the focus event itself, before any subsequent
            // key-down in the same message batch can be recorded.
            if (cg2::WinApp::GetInstance()->ConsumeActivationChanged()) {
                cg2::Input::GetInstance()->OnFocusChanged(cg2::WinApp::GetInstance()->IsActive());
                dxCommon_->ResetFixFPS();
            }
        }
        if (msg.message == WM_QUIT) {
            break;
        }
		const float messagePumpMs = elapsedMs(messageStart, std::chrono::steady_clock::now());
        // インプットインスタンスを取得
        cg2::Input* input = cg2::Input::GetInstance();
        cg2::WinApp* winApp = cg2::WinApp::GetInstance();

        if (!winApp->IsActive() && !backgroundValidation) {
            dxCommon_->ResetFixFPS();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

		const auto inputImGuiStart = std::chrono::steady_clock::now();
		// 前のフレームのキー状態を保存
        input->BeforeFrameData();
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
        if (GameplayScenarioSession::Get().IsActive()) input->OverrideValidationFrame({640.0f, 360.0f});
#endif
        auto& runtime = cg2::RuntimeProfiler::Get();
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
            if (cg2::Object3dCommon::GetInstance()->GetIsDebugCamera()) {
                cg2::Object3dCommon::GetInstance()->SetIsDebugCamera(false);
            } else {
                cg2::Object3dCommon::GetInstance()->SetIsDebugCamera(true);
            }
        }
#endif // USE_IMGUI

        cg2::Object3dCommon::GetInstance()->Update();
		const float engineUpdateMs = elapsedMs(engineUpdateStart, std::chrono::steady_clock::now());
		const auto sceneUpdateStart = std::chrono::steady_clock::now();
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
        const auto depthSceneEpochBefore = GameplayScenarioSession::Get().GetSceneEpoch();
        const auto depthSceneNameBefore = SceneManager::GetInstance()->GetCurrentSceneName();
#endif
        SceneManager::GetInstance()->Update();
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
        auto& depthSession = GameplayScenarioSession::Get();
        const auto& actualSceneName = SceneManager::GetInstance()->GetCurrentSceneName();
        if (depthSession.IsActive() && gameplaytest::IsNeonDepthScenario(depthSession.GetSettings().scenario) &&
            (actualSceneName != depthSceneNameBefore || depthSession.GetSceneEpoch() != depthSceneEpochBefore)) {
            if (depthSession.NotifyDepthSceneEntered(actualSceneName)) PostQuitMessage(depthSession.GetErrors().empty()?0:9);
        }
#endif
        if (trailStress_) {
            cg2::RuntimeProfiler::CpuScope scope("Stress trails update");
            trailStress_->Update(1.0f/60.0f);
        }
		const float sceneUpdateMs = elapsedMs(sceneUpdateStart, std::chrono::steady_clock::now());
        const auto developerShowcase = SceneManager::GetInstance()->GetDeveloperShowcaseState();
        const float combatDeltaTime = SceneManager::GetInstance()->GetFinalDeltaTime();
        bool grayscale = combatDeltaTime < (1.0f / 60.0f) * 0.98f;
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
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
            cg2::RuntimeProfiler::GpuScope scope("Scene 3D (includes effects)");
            SceneManager::GetInstance()->DrawPostEffect3D(); // ここで Object3d::Draw が呼ばれる
            if (trailStress_) {
                cg2::RuntimeProfiler::CpuScope cpuScope("Stress trails");
                cg2::RuntimeProfiler::GpuScope gpuScope("Stress trails");
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
            cg2::RuntimeProfiler::GpuScope scope("Global Bloom / Post");
            bloom_->PostDraw();
        });

        renderProfile.afterPostMs = measureMs([&]() {
            cg2::RuntimeProfiler::GpuScope scope("After Post / Text Glow");
            SceneManager::GetInstance()->DrawAfterPostEffect3D();
        });

        renderProfile.spriteMs = measureMs([&]() {
            cg2::RuntimeProfiler::GpuScope scope("2D / Game UI");
            cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
            SceneManager::GetInstance()->DrawSprite();
        });
		renderProfile.drawRecordMs = elapsedMs(drawRecordStart, std::chrono::steady_clock::now());


		const auto imguiDrawStart = std::chrono::steady_clock::now();
        SceneManager::GetInstance()->RecordDeveloperPostParameters(bloom_->GetLastCompositeParams());
        SceneManager::GetInstance()->RecordDeveloperFrame(*dxCommon_);
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
        if (imguiInitialized_) {
        cg2::RuntimeProfiler::GpuScope scope("Diagnostics UI");
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
            cg2::StartupTrace::Mark("first_frame." + presentedScene);
            cg2::StartupTrace::Flush();
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
        if (runtime.IsCaptureComplete() && GetEnvironmentVariableA("CG2_PERF_EXIT_AFTER_CAPTURE", exitAfterCapture, sizeof(exitAfterCapture)) == 1 && exitAfterCapture[0] == '1') {
            PostQuitMessage(0);
        }
    }
    return static_cast<int>(msg.wParam);
}

void Game::Finalize() {
	// The current scene owns RenderTextures and other GPU resources that keep
	// non-owning pointers to SrvManager.  Release the scene before the Game
	// members (and therefore SrvManager) begin their destruction.
	if (dxCommon_) {
		dxCommon_->ExecuteCommandListAndWait();
	}
	SceneManager::GetInstance()->Finalize();
    trailStress_.reset();
    cg2::RuntimeProfiler::Get().Shutdown();

#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
    if (imguiInitialized_) {
    // 実際のcommandListのImGuiの描画コマンドを組む
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    imguiInitialized_ = false;
    }

#endif // USE_IMGUI

    cg2::TextureManager::GetInstance()->Finalize();
    cg2::TextRenderer::GetInstance()->Finalize();
    cg2::ModelManager::GetInstance()->Finalize();

    CloseHandle(dxCommon_->GetFenceEvent());

    dxCommon_->Release();
    cg2::WinApp::GetInstance()->Finalize();

    CoUninitialize();
    MFShutdown();
}
