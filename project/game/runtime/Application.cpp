#include "Application.h"
#include "FixedTimestep.h"
#include "Dump.h"
#include "DeveloperTools.h"
#include "../scene/SceneFactory.h"
#include "../scene/SceneManager.h"
#include "GameModuleRegistry.h"
#include "SceneRegistry.h"
#include "Audio.h"
#include "TextRenderer.h"
#include "TextLabel.h"
#include "RuntimeProfiler.h"
#include "StartupTrace.h"
#include "BackgroundLoader.h"
#include "SkinCluster.h"
#include "ResourcePaths.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr const char* kDefaultProjectFilePath = "resources/projects/default.project.json";

/// @brief Windowsの実際のインストール先からUI用のフォントを探す。
std::filesystem::path GetSystemFontPath(const wchar_t* filename)
{
    std::array<wchar_t, MAX_PATH> directory{};
    const UINT length = GetWindowsDirectoryW(directory.data(), static_cast<UINT>(directory.size()));
    if (length == 0 || length >= directory.size()) {
        return {};
    }
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
    myEngine::LogWrite().Log("[GameProject] Active project: name='" + project.projectName + "', source='" + GetProjectSourceLabel(project) +
                             "', gameModule='" + project.gameModule + "', startupScene='" + project.startupScene + "', resourceRoot='" +
                             project.resourceRoot + "', loadedFromFallback=" + (loadedFromFallback ? "true" : "false") + ".\n");
}

} // namespace

void Application::LoadActiveProject(const GameProjectCommandLineOptions& projectOptions)
{
    GameProjectLoader loader;
    GameProject loadedProject;
    bool defaultProjectAlreadyAttempted = false;

    if (projectOptions.projectOptionProvided) {
        if (projectOptions.projectFilePath.empty()) {
            myEngine::LogWrite().Log("[GameProject] The explicit --project path is empty. "
                                     "Trying the default project file.\n");
        } else if (loader.Load(projectOptions.projectFilePath, loadedProject)) {
            activeProject_ = std::move(loadedProject);
            activeProjectLoadedFromFallback_ = false;
            LogActiveProject(activeProject_, activeProjectLoadedFromFallback_);
            return;
        } else {
            defaultProjectAlreadyAttempted = projectOptions.projectFilePath == kDefaultProjectFilePath;
            if (defaultProjectAlreadyAttempted) {
                myEngine::LogWrite().Log("[GameProject] Explicit project file is the default project file and "
                                         "failed to load. Using built-in safe defaults.\n");
            } else {
                myEngine::LogWrite().Log("[GameProject] Explicit project file '" + projectOptions.projectFilePath +
                                         "' failed to load. Trying '" + kDefaultProjectFilePath + "'.\n");
            }
        }
    }

    if (!defaultProjectAlreadyAttempted && loader.Load(kDefaultProjectFilePath, loadedProject)) {
        activeProject_ = std::move(loadedProject);
        activeProjectLoadedFromFallback_ = projectOptions.projectOptionProvided;
        LogActiveProject(activeProject_, activeProjectLoadedFromFallback_);
        return;
    }

    activeProject_ = GameProject{};
    activeProjectLoadedFromFallback_ = true;
    myEngine::LogWrite().Log("[GameProject] Default project file could not be loaded. "
                             "Using built-in safe defaults.\n");
    LogActiveProject(activeProject_, activeProjectLoadedFromFallback_);
}

bool Application::ConfigureGameModuleAndSceneFactory()
{
    const std::string fallbackGameModuleId(hooks_.GetFallbackGameModuleId());
    GameModuleRegistry moduleRegistry;
    if (!hooks_.RegisterGameModules(moduleRegistry)) {
        myEngine::LogWrite().Log("[GameModule] FATAL: failed to register the available game modules. "
                                 "The game will not enter the main loop.\n");
        return false;
    }

    gameModuleUsedFallback_ = false;
    std::unique_ptr<IGameModule> selectedModule = moduleRegistry.Create(activeProject_.gameModule);
    if (!selectedModule) {
        const std::vector<std::string> registeredModuleIds = moduleRegistry.GetRegisteredIds();
        myEngine::LogWrite().Log("[GameModule] Requested game module is not available. projectName='" + activeProject_.projectName +
                                 "', projectFile='" + GetProjectSourceLabel(activeProject_) + "', requestedModule='" +
                                 activeProject_.gameModule + "', registeredModules=[" + JoinSceneNames(registeredModuleIds) + "].\n");

        if (activeProject_.gameModule != fallbackGameModuleId) {
            myEngine::LogWrite().Log("[GameModule] Falling back to game module '" + fallbackGameModuleId + "'.\n");
            selectedModule = moduleRegistry.Create(fallbackGameModuleId);
            gameModuleUsedFallback_ = true;
        }
    }

    if (!selectedModule) {
        myEngine::LogWrite().Log("[GameModule] FATAL: requested game module '" + activeProject_.gameModule +
                                 "' could not be created and required fallback "
                                 "module '" +
                                 fallbackGameModuleId + "' is unavailable. The game will not enter the main loop.\n");
        return false;
    }

    SceneRegistry sceneRegistry;
    if (!selectedModule->RegisterScenes(sceneRegistry)) {
        myEngine::LogWrite().Log("[GameModule] FATAL: game module '" + std::string(selectedModule->GetId()) +
                                 "' failed to register its scenes. The game will not enter the main loop.\n");
        return false;
    }

    const std::vector<std::string> registeredSceneNames = sceneRegistry.GetRegisteredNames();
    auto sceneFactory = std::make_unique<SceneFactory>(std::move(sceneRegistry));
    if (!SceneManager::GetInstance()->SetSceneFactory(std::move(sceneFactory))) {
        myEngine::LogWrite().Log("[GameModule] FATAL: failed to inject the scene factory for game module '" +
                                 std::string(selectedModule->GetId()) + "'. The game will not enter the main loop.\n");
        return false;
    }

    myEngine::LogWrite().Log("[GameModule] Active module: id='" + std::string(selectedModule->GetId()) + "', displayName='" +
                             std::string(selectedModule->GetDisplayName()) +
                             "', loadedFromFallback=" + (gameModuleUsedFallback_ ? "true" : "false") + ", registeredScenes=[" +
                             JoinSceneNames(registeredSceneNames) + "].\n");
    activeGameModule_ = std::move(selectedModule);
    return true;
}

bool Application::ResolveStartupScene()
{
    const std::string fallbackSceneName(hooks_.GetFallbackSceneName());
    SceneManager* sceneManager = SceneManager::GetInstance();
    resolvedStartupScene_ = activeProject_.startupScene;
    startupSceneUsedFallback_ = false;

    if (sceneManager->ContainsScene(resolvedStartupScene_)) {
        return true;
    }

    const std::vector<std::string> registeredNames = sceneManager->GetRegisteredSceneNames();
    myEngine::LogWrite().Log("[GameProject] Startup scene is not registered. projectName='" + activeProject_.projectName +
                             "', projectFile='" + GetProjectSourceLabel(activeProject_) + "', startupScene='" +
                             activeProject_.startupScene + "', registeredScenes=[" + JoinSceneNames(registeredNames) + "].\n");

    if (sceneManager->ContainsScene(fallbackSceneName)) {
        resolvedStartupScene_ = fallbackSceneName;
        startupSceneUsedFallback_ = true;
        myEngine::LogWrite().Log("[GameProject] Falling back to startup scene '" + fallbackSceneName + "'.\n");
        return true;
    }

    resolvedStartupScene_.clear();
    myEngine::LogWrite().Log("[GameProject] FATAL: startup scene '" + activeProject_.startupScene +
                             "' is unavailable and fallback scene '" + fallbackSceneName +
                             "' is not registered. "
                             "The game will not enter the main loop.\n");
    return false;
}

bool Application::Initialize(const GameProjectCommandLineOptions& projectOptions)
{
    myEngine::StartupTrace::Scope startupScope("Game.Initialize");

    LoadActiveProject(projectOptions);
    // 素材の置き場所は、バックグラウンド読み込みやエンジンの初期化より前に決める。
    myEngine::ResourcePaths::SetEngineRoot(hooks_.GetEngineResourceRoot());
    myEngine::ResourcePaths::SetGameRoot(activeProject_.resourceRoot);
    if (!ConfigureGameModuleAndSceneFactory()) {
        return false;
    }
    if (!ResolveStartupScene()) {
        return false;
    }

    myEngine::Dump dump;
    SetUnhandledExceptionFilter(dump.Export);

    myEngine::WinApp::GetInstance()->Initialize();

    char asyncFlag[8]{};
    progressiveStartupScene_ = hooks_.GetProgressiveStartupSceneName();
    titleFirst_ = !progressiveStartupScene_.empty() && resolvedStartupScene_ == progressiveStartupScene_ &&
                  !(GetEnvironmentVariableA("MYENGINE_ASYNC_STARTUP", asyncFlag, sizeof(asyncFlag)) == 1 && asyncFlag[0] == '0');
    // ファイルのデコードはデバイスを必要としないため、D3D12のデバイスやパイプラインの作成と並行して行う。
    hooks_.OnBeforeEngineInitialize(resolvedStartupScene_);
    InitializeEngine();
    if (!titleFirst_) {
        InitializeImGui();
    }
    myEngine::StartupTrace::Count("build.developer_tools", myEngine::kDeveloperTools ? 1 : 0);
    myEngine::StartupTrace::Count("ui.imgui_initialized", imguiInitialized_ ? 1 : 0);
    LoadResources();

    SceneManager* sceneManager = SceneManager::GetInstance();
    const std::string fallbackSceneName(hooks_.GetFallbackSceneName());
    bool sceneInitialized = sceneManager->Initialize(resolvedStartupScene_);
    if (!sceneInitialized && resolvedStartupScene_ != fallbackSceneName && sceneManager->ContainsScene(fallbackSceneName)) {
        myEngine::LogWrite().Log("[GameProject] Failed to create startup scene '" + resolvedStartupScene_ + "'. Falling back to '" +
                                 fallbackSceneName + "'.\n");
        resolvedStartupScene_ = fallbackSceneName;
        startupSceneUsedFallback_ = true;
        sceneInitialized = sceneManager->Initialize(resolvedStartupScene_);
    }
    if (!sceneInitialized) {
        myEngine::LogWrite().Log("[GameProject] FATAL: no startup scene could be initialized. "
                                 "The game will not enter the main loop.\n");
        return false;
    }
    myEngine::LogWrite().Log("[GameProject] Startup scene initialized: '" + resolvedStartupScene_ + "'.\n");

    if (titleFirst_) {
        presentationTask_ = InitializePresentation();
    } else {
        auto presentation = InitializePresentation();
        while (!presentation.Advance()) {
        }
        engineReady_ = true;
        sceneManager->OnEngineReady();
    }
    return true;
}

myEngine::InitializationTask Application::InitializePresentation()
{
    if (titleFirst_) {
        InitializeImGui();
    }
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
    hooks_.OnLoadSharedAssets();

    myEngine::StartupTrace::Mark("engine.ready");
}

void Application::InitializeEngine()
{
    myEngine::StartupTrace::Scope startupScope("Engine.Initialize");
    const auto timed = [](const char* name, auto&& action) {
        myEngine::StartupTrace::Scope scope(name);
        action();
    };

    dxCommon_ = std::make_unique<myEngine::DirectXCommon>();
    timed("Engine.DirectX", [&] {
        dxCommon_->Initialize(myEngine::WinApp::GetInstance(), titleFirst_);
    });
    myEngine::RuntimeProfiler::Get().Initialize(dxCommon_.get());
    char frameLimit[8]{};
    if (GetEnvironmentVariableA("MYENGINE_FRAME_LIMIT", frameLimit, sizeof(frameLimit)) == 1 && frameLimit[0] == '0') {
        dxCommon_->SetFrameLimitEnabled(false);
    }

    srvManager_ = std::make_unique<myEngine::SrvManager>();
    srvManager_->Initialize(dxCommon_.get());

    shadow_ = std::make_unique<myEngine::Shadow>();
    timed("Engine.Shadow", [&] {
        shadow_->Initialize(dxCommon_.get(), srvManager_.get());
    });

    myEngine::TextureManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());
    myEngine::ModelManager::GetInstance()->Initialize(dxCommon_.get());

    timed("Engine.Object3d", [&] {
        myEngine::Object3dCommon::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get(), shadow_->GetShadowMap());
    });
    timed("Engine.Sprite", [&] {
        myEngine::SpriteCommon::GetInstance()->Initialize(dxCommon_.get());
    });
    // スプライトはObjectシェーダーを共有する。3Dワールドがカメラ・ライト・航跡の定数を
    // 設定する前でも、ライティングなしの経路には有効なルートのバインドが必要。
    startupConstants_ = dxCommon_->CreateBufferResource(1024);
    void* startupData = nullptr;
    startupConstants_->Map(0, nullptr, &startupData);
    std::memset(startupData, 0, 1024);
    startupConstants_->Unmap(0, nullptr);

    myEngine::Input::GetInstance()->Initialize(myEngine::WinApp::GetInstance()->GetWindowClass(),
                                               myEngine::WinApp::GetInstance()->GetHwnd());
}

void Application::InitializeImGui()
{
    myEngine::StartupTrace::Scope startupScope("Engine.ImGui");

#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
#if !defined(USE_IMGUI)
    if (!myEngine::RuntimeProfiler::Get().IsAllowed()) {
        return;
    }
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
            if (fontPath.empty() || !std::filesystem::is_regular_file(fontPath, fontError)) {
                continue;
            }
            const auto utf8Path = fontPath.u8string();
            const std::string imguiPath(utf8Path.begin(), utf8Path.end());
            if (io.Fonts->AddFontFromFileTTF(imguiPath.c_str(), 18.0f, &fontConfig, japaneseRanges)) {
                break;
            }
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
    initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
                                       D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle) {
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

void Application::LoadResources()
{
    // 起動時はタイトルシーンに集中させる。モデルは必要になったときに
    // Object3d::SetModel()から、または必要とするシーンが明示的に読み込む。
}

int Application::Run()
{
    return MainLoop();
}

int Application::MainLoop()
{
    // 非表示で行う決定的な検証は、ユーザーが別のアプリに切り替えても一時停止してはならない。
    // 通常の操作プレイでは、フォーカスを失うと一時停止する。
    // 起動の検証は 1: デモの準備後にゲームへ進む、2: デモの準備を待たずに進む（TitleScene）。どちらもゲーム画面を出したら終了する。
    char startupAutoTest[8]{};
    const bool startupValidation = GetEnvironmentVariableA("MYENGINE_STARTUP_AUTOTEST", startupAutoTest, sizeof(startupAutoTest)) == 1 &&
                                   (startupAutoTest[0] == '1' || startupAutoTest[0] == '2');
    const bool backgroundValidation = startupValidation || hooks_.IsBackgroundValidationActive();
    const std::string startupValidationScene(hooks_.GetStartupValidationSceneName());

    // 通常のプレイでは、実時間を測って1/60秒の固定ステップを必要な回数だけ回し、描画は1回にする。
    // 決定的な検証（シナリオ・自動テスト）は、今までどおり描画1回につき固定ステップ1回で進める。
    bool oneStepPerFrame = backgroundValidation;
    bool keepRunningWhenInactive = backgroundValidation;
    // 開発用: MYENGINE_TIMESTEP_TEST_SECONDS 秒だけ実時間の方式で動かし、ステップ数を書き出して終了する。
    // MYENGINE_DEBUG_FRAME_DELAY_MS は、描画1回ごとに指定ミリ秒待って低いFPSを再現する。
    double timestepTestSeconds = 0.0;
    int debugFrameDelayMs = 0;
    if (myEngine::kDeveloperTools) {
        char value[32]{};
        if (GetEnvironmentVariableA("MYENGINE_TIMESTEP_TEST_SECONDS", value, sizeof(value)) > 0) {
            timestepTestSeconds = std::atof(value);
        }
        if (GetEnvironmentVariableA("MYENGINE_DEBUG_FRAME_DELAY_MS", value, sizeof(value)) > 0) {
            debugFrameDelayMs = (std::max)(0, std::atoi(value));
        }
        if (timestepTestSeconds > 0.0) {
            oneStepPerFrame = false;
            keepRunningWhenInactive = true;
        }
    }
    myEngine::FixedTimestep fixedTimestep;

    struct TimestepTestStats {
        bool started = false;
        std::chrono::steady_clock::time_point start{};
        long long frames = 0;
        long long steps = 0;
        long long zeroStepFrames = 0;
        long long multiStepFrames = 0;
        int maxSteps = 0;
    } timestepTest;

    hooks_.OnMainLoopStarting();
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
            // 後からキューに入ったメッセージで上書きされる前に、終了ステータスを保存しておく。
            if (msg.message == WM_QUIT) {
                break;
            }
            if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) {
                // DIKのスキャンコードでは、拡張キー（矢印キー、テンキーのEnter）にビット7を使う。
                unsigned int rawScan = static_cast<unsigned int>((msg.lParam >> 16) & 0xff);
                bool extended = (msg.lParam & (1LL << 24)) != 0;
                // ソフトウェアキーボードは仮想キーしか渡さないことがある。
                // ハードウェアのスキャンコードが無い場合も、同じメニュー動作を保つ。
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
            // 同じメッセージのまとまりの中で後続のキー押下が記録される前に、
            // フォーカスイベントの時点で古いキー状態を消す。
            if (myEngine::WinApp::GetInstance()->ConsumeActivationChanged()) {
                myEngine::Input::GetInstance()->OnFocusChanged(myEngine::WinApp::GetInstance()->IsActive());
                dxCommon_->ResetFixFPS();
                fixedTimestep.Reset();
            }
        }
        if (msg.message == WM_QUIT) {
            break;
        }
        const float messagePumpMs = elapsedMs(messageStart, std::chrono::steady_clock::now());
        // インプットインスタンスを取得
        myEngine::Input* input = myEngine::Input::GetInstance();
        myEngine::WinApp* winApp = myEngine::WinApp::GetInstance();

        if (!winApp->IsActive() && !keepRunningWhenInactive) {
            dxCommon_->ResetFixFPS();
            fixedTimestep.Reset();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        auto* scenes = SceneManager::GetInstance();
        const bool simpleRendering = !engineReady_ || scenes->UsesSimpleRendering();
        // この描画までに回す固定ステップの回数。0回なら更新せず、前の状態をもう一度描く。
        int steps = 1;
        if (simpleRendering || scenes->IsLoadingInBackground()) {
            // 読み込み中は描画1回につき更新1回。読み込みが終わった直後に、待っていた時間をまとめて進めない。
            fixedTimestep.Reset();
        } else if (!oneStepPerFrame) {
            steps = fixedTimestep.Advance(frameStart);
        }

        const auto inputImGuiStart = std::chrono::steady_clock::now();
        if (steps > 0) {
            // 前のフレームのキー状態を保存。更新しない描画では読まず、押した瞬間を次の更新まで残す。
            input->BeforeFrameData();
            // デバイスが切り替わったら、描画コマンドの記録の外で今すぐ操作案内を描き直す。
            myEngine::TextLabel::RefreshAllInputPrompts();
            hooks_.OnFrameInputReady(*input);
        }
        if (simpleRendering) {
            // 後回しにした処理を行う前に、必ずタイトルを1フレーム表示する。
            // この分岐では、シーン読み込み中も毎フレームメッセージと入力を処理する。
            if (!engineReady_ && !lastPresentedScene.empty()) {
                // タイトルはデモの準備ができるまで黒いままなので、残りのエンジン処理は
                // vsyncごとに1つずつではなく、約100msずつまとめて実行する。
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
                while (!engineReady_ && std::chrono::steady_clock::now() < deadline) {
                    if (!dxCommon_->ArePipelinesReady()) {
                        dxCommon_->InitializePipelinesStep();
                    } else if (presentationTask_.Advance()) {
                        engineReady_ = true;
                        scenes->OnEngineReady();
                    }
                }
            }
            scenes->Update();
            if (scenes->GetCurrentSceneName() != progressiveStartupScene_) {
                continue;
            }
            dxCommon_->PreDraw();
            srvManager_->PreDraw();
            myEngine::SpriteCommon::GetInstance()->PreDraw(myEngine::kNormal);
            auto* list = dxCommon_->GetList().Get();
            for (const unsigned slot : {3u, 4u, 5u, 6u, 17u}) {
                list->SetGraphicsRootConstantBufferView(slot, startupConstants_->GetGPUVirtualAddress());
            }
            auto* textures = myEngine::TextureManager::GetInstance();
            const auto black = textures->GetSrvHandleGPU(myEngine::TextureManager::GetBlackCausticsTexturePath(),
                                                         myEngine::TextureManager::TextureColorSpace::LinearData);
            list->SetGraphicsRootDescriptorTable(7, black);
            list->SetGraphicsRootDescriptorTable(18, black);
            list->SetGraphicsRootDescriptorTable(
                8, textures->GetSrvHandleGPU(myEngine::TextureManager::GetPbrPrefilteredEnvironmentTexturePath(),
                                             myEngine::TextureManager::TextureColorSpace::LinearData));
            scenes->DrawSprite();
            hooks_.OnStartupFrameRecorded(*dxCommon_, lastPresentedScene.empty());
            dxCommon_->PostDraw();
            hooks_.OnStartupFramePresented(*dxCommon_);
            if (lastPresentedScene != progressiveStartupScene_) {
                myEngine::StartupTrace::Mark("first_frame." + progressiveStartupScene_);
                myEngine::StartupTrace::Flush();
                lastPresentedScene = progressiveStartupScene_;
            }
            continue;
        }
        auto& runtime = myEngine::RuntimeProfiler::Get();
        runtime.BeginFrame();
        const int gpuFrame = runtime.BeginGpu("GPU frame");
        runtime.SetCounter("Fixed steps", static_cast<double>(steps));

#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
        // ImGuiの画面は更新の中で組み立てる。更新しない描画では組み立て直さず、前回の描画データをそのまま使う。
        const bool buildImGuiFrame = imguiInitialized_ && steps > 0;
        auto beginImGuiFrame = [] {
            ImGui::NewFrame();
#ifdef USE_IMGUI
            ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
#endif
        };
        if (buildImGuiFrame) {
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            {
                // スワップチェーン（kRenderWidth x kRenderHeight）は、ウィンドウの大きさに合わせて拡大縮小して表示される。
                // ImGui の座標はウィンドウのクライアント座標（マウスと同じ）のまま、スワップチェーン全体へ描くように倍率を合わせる。
                // 合わせないと、ウィンドウがスワップチェーンより小さいときに ImGui だけが縮んで表示され、マウスの位置とずれる。
                ImGuiIO& io = ImGui::GetIO();
                if (io.DisplaySize.x > 0.0f && io.DisplaySize.y > 0.0f) {
                    io.DisplayFramebufferScale = ImVec2(float(myEngine::WinApp::kRenderWidth) / io.DisplaySize.x,
                                                        float(myEngine::WinApp::kRenderHeight) / io.DisplaySize.y);
                }
            }
            beginImGuiFrame();
        }

#endif // USE_IMGUI
        const float inputImGuiBeginMs = elapsedMs(inputImGuiStart, std::chrono::steady_clock::now());

        float engineUpdateMs = 0.0f;
        float sceneUpdateMs = 0.0f;
        const std::string sceneBeforeSteps = scenes->GetCurrentSceneName();
        for (int step = 0; step < steps; ++step) {
            if (step > 0) {
                // 同じ描画フレームの2回目以降の更新。押した瞬間を繰り返さず、ImGuiは最後の更新で組んだ画面だけを描く。
                input->RepeatFrameData();
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
                if (imguiInitialized_) {
                    ImGui::EndFrame();
                    beginImGuiFrame();
                }
#endif
            }
            const auto engineUpdateStart = std::chrono::steady_clock::now();
            {
                // エンジンはシーンを直接参照しないので、現在のシーンが求める画面効果の設定をここで渡す。
                myEngine::BloomFrameSettings bloomFrameSettings{};
                bloomFrameSettings.water = scenes->GetWaterPostProcessSettings();
                bloomFrameSettings.arenaLook = scenes->GetArenaLookSettings();
                bloomFrameSettings.bloomView = scenes->GetBloomViewSettings();
                bloomFrameSettings.deltaTime = scenes->GetFinalDeltaTime();
                bloom_->Update(bloomFrameSettings);
            }

#ifdef USE_IMGUI
            if (input->IsPress(input->GetKey()[DIK_LCONTROL]) && input->IsPress(input->GetKey()[DIK_LSHIFT]) &&
                input->IsTrigger(input->GetKey()[DIK_D], input->GetPreKey()[DIK_D])) {
                if (myEngine::Object3dCommon::GetInstance()->GetIsDebugCamera()) {
                    myEngine::Object3dCommon::GetInstance()->SetIsDebugCamera(false);
                } else {
                    myEngine::Object3dCommon::GetInstance()->SetIsDebugCamera(true);
                }
            }
#endif // USE_IMGUI

            myEngine::Object3dCommon::GetInstance()->Update();
            engineUpdateMs += elapsedMs(engineUpdateStart, std::chrono::steady_clock::now());
            const auto sceneUpdateStart = std::chrono::steady_clock::now();
            hooks_.OnBeforeSceneUpdate();
            scenes->Update();
            hooks_.OnAfterSceneUpdate();
            if (trailStress_) {
                myEngine::RuntimeProfiler::CpuScope scope("Stress trails update");
                trailStress_->Update(myEngine::kFixedStepSeconds);
            }
            sceneUpdateMs += elapsedMs(sceneUpdateStart, std::chrono::steady_clock::now());
            if (scenes->UsesSimpleRendering() || scenes->GetCurrentSceneName() != sceneBeforeSteps) {
                // シーンが切り替わった。読み込みにかかった時間を後で追い上げないよう、残りのステップは回さず測定もやり直す。
                fixedTimestep.Reset();
                break;
            }
        }
        if (steps > 0) {
            const auto developerShowcase = scenes->GetDeveloperShowcaseState();
            const float combatDeltaTime = scenes->GetFinalDeltaTime();
            bool grayscale = combatDeltaTime < myEngine::kFixedStepSeconds * 0.98f;
            hooks_.AdjustGrayscale(combatDeltaTime, developerShowcase, grayscale);
            bloom_->SetGrayscaleEnabled(grayscale);
            bloom_->SetGaussianOverride(scenes->GetPostGaussianIntensity());
            const IScene::PostEffectPulse postPulse = scenes->GetPostEffectPulse();
            bloom_->SetTransientPulse(postPulse.bloomBoost, postPulse.chromAbAmount, postPulse.center, postPulse.radius, postPulse.width,
                                      postPulse.strength);
            bloom_->SetScreenEffectState(scenes->GetScreenEffectState());
            bloom_->SetDeveloperShowcaseState(developerShowcase);
        }

        const auto imguiBuildStart = std::chrono::steady_clock::now();
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
        if (buildImGuiFrame) {
            runtime.DrawOverlay(dxCommon_->IsFrameLimitEnabled(), scenes->GetCurrentSceneName().c_str());
            // ImGuiの内部コマンドを生成する
            ImGui::Render();
        }

#endif // USE_IMGUI
        const float imguiBuildMs = elapsedMs(imguiBuildStart, std::chrono::steady_clock::now());

        const auto drawSetupStart = std::chrono::steady_clock::now();
        dxCommon_->PreDraw(); // バックバッファのバリアはここで行われている
        srvManager_->PreDraw();

        shadow_->PreDraw([] {
            SceneManager::GetInstance()->DrawShadow();
        });

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

        if (debugFrameDelayMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(debugFrameDelayMs));
        }
        const auto postDrawStart = std::chrono::steady_clock::now();
        dxCommon_->PostDraw();

        if (timestepTestSeconds > 0.0) {
            if (!timestepTest.started) {
                timestepTest.started = true;
                timestepTest.start = frameStart;
            }
            ++timestepTest.frames;
            timestepTest.steps += steps;
            timestepTest.zeroStepFrames += steps == 0 ? 1 : 0;
            timestepTest.multiStepFrames += steps > 1 ? 1 : 0;
            timestepTest.maxSteps = (std::max)(timestepTest.maxSteps, steps);
            const double realSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - timestepTest.start).count();
            if (realSeconds >= timestepTestSeconds) {
                std::filesystem::create_directories("generated/timestep_test");
                char text[512]{};
                std::snprintf(text, sizeof(text),
                              "{\"realSeconds\":%.4f,\"gameSeconds\":%.4f,\"frames\":%lld,\"steps\":%lld,\"zeroStepFrames\":%lld,"
                              "\"multiStepFrames\":%lld,\"maxSteps\":%d,\"droppedSeconds\":%.4f,\"frameLimit\":%s,\"frameDelayMs\":%d}\n",
                              realSeconds, static_cast<double>(timestepTest.steps) * myEngine::kFixedStepSeconds, timestepTest.frames,
                              timestepTest.steps, timestepTest.zeroStepFrames, timestepTest.multiStepFrames, timestepTest.maxSteps,
                              fixedTimestep.GetDroppedSeconds(), dxCommon_->IsFrameLimitEnabled() ? "true" : "false", debugFrameDelayMs);
                std::ofstream("generated/timestep_test/result.json") << text;
                PostQuitMessage(0);
            }
        }

        const auto& presentedScene = SceneManager::GetInstance()->GetCurrentSceneName();
        if (lastPresentedScene != presentedScene) {
            myEngine::StartupTrace::Mark("first_frame." + presentedScene);
            myEngine::StartupTrace::Flush();
            lastPresentedScene = presentedScene;
        }
        if (startupValidation && !startupValidationScene.empty() && presentedScene == startupValidationScene &&
            ++startupValidationFrames >= 3) {
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
        if (runtime.IsCaptureComplete() &&
            GetEnvironmentVariableA("MYENGINE_PERF_EXIT_AFTER_CAPTURE", exitAfterCapture, sizeof(exitAfterCapture)) == 1 &&
            exitAfterCapture[0] == '1') {
            PostQuitMessage(0);
        }
    }
    hooks_.OnMainLoopFinished();
    return static_cast<int>(msg.wParam);
}

void Application::Finalize()
{
    myEngine::BackgroundLoader::Get().Shutdown();
    presentationTask_ = {};
    myEngine::SkinnedModel::ClearPreloadedAssets();
    // 現在のシーンは、SrvManagerを所有せずに指すRenderTextureなどのGPUリソースを持っている。
    // Gameのメンバー（つまりSrvManager）の破棄が始まる前に、シーンを解放する。
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

    dxCommon_->Release();
    myEngine::WinApp::GetInstance()->Finalize();
    MFShutdown();
}
