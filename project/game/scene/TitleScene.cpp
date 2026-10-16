#include "game/weapon/CombatTypes.h"
#include "TitleScene.h"
#include "game/render/settings/ArenaLookConfig.h"
#include "GameScene.h"
#include "GameStartMode.h"
#include "SceneManager.h"
#include "StartupTrace.h"
#include "SkinCluster.h"
#include "game/run/TankSubmissionValidation.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <fstream>
#include <thread>

namespace {
/// @brief 表示内容を指定した領域へ収める。
void Fit(myEngine::TextLabel& label, const myEngine::Vector2& bounds)
{
    label.PrepareForDraw();
    if (auto* sprite = label.GetSprite()) {
        const auto size = sprite->GetSize();
        const float scale = (std::min)(bounds.x / (std::max)(size.x, 1.0f), bounds.y / (std::max)(size.y, 1.0f));
        sprite->SetSize({size.x * scale, size.y * scale});
    }
}

// 非表示ワールドの初期化中にフォントを登録すると、古いラベルが無効になることがある。
// Gameがタイトルの描画パスを記録し始める前に、テクスチャの作り直しを済ませる。
void PrepareLabels(std::initializer_list<myEngine::TextLabel*> labels)
{
    for (auto* label : labels) {
        if (label) {
            const bool hadSprite = label->GetSprite() != nullptr;
            const auto size = hadSprite ? label->GetSprite()->GetSize() : myEngine::Vector2{};
            label->PrepareForDraw();
            // フォント登録後に作り直しても、タイトルに合わせた範囲は保つ必要がある。
            if (hadSprite && label->GetSprite()) {
                label->GetSprite()->SetSize(size);
            }
        }
    }
}
} // namespace

TitleScene::TitleScene() = default;
TitleScene::~TitleScene() = default;

void TitleScene::Initialize()
{
    myEngine::StartupTrace::Scope startupScope("TitleScene.Initialize");
    input_ = myEngine::Input::GetInstance();
    myEngine::Object3dCommon::GetInstance()->SetDefaultCamera(nullptr);
    myEngine::Object3dCommon::GetInstance()->SetDebugDefaultCamera(nullptr);
    finished_ = false;
    demoFrozen_ = false;
    nextSceneName_.clear();
    phase_ = Phase::kFadeIn;
    blinkTimer_ = 0;
    capturedStages_ = 0;
    previousMousePosition_ = input_->GetMousePosition();
    wchar_t test[8]{};
    autoTest_ = GetEnvironmentVariableW(L"MYENGINE_TITLE_AUTOTEST", test, 8) > 0 && test[0] == L'1';
    startupAutoTest_ = GetEnvironmentVariableW(L"MYENGINE_STARTUP_AUTOTEST", test, 8) > 0 && test[0] == L'1';
    if (autoTest_) {
        std::filesystem::create_directories("generated/title_demo");
        std::ofstream("generated/title_demo/validation.json") << "{\"completed\":false}\n";
    }
    char asyncFlag[8]{};
    asyncStartup_ = !(GetEnvironmentVariableA("MYENGINE_ASYNC_STARTUP", asyncFlag, sizeof(asyncFlag)) == 1 && asyncFlag[0] == '0');
    engineReady_ = myEngine::SpriteCommon::GetInstance()->GetDxCommon()->ArePipelinesReady();
    stageSamples_ = {};
    fade_ = std::make_unique<Fade>();
    fade_->Initialize();
    fade_->Start(Fade::Status::FadeIn, 0.65f);
    const float w = static_cast<float>(myEngine::WinApp::kClientWidth);
    const float h = static_cast<float>(myEngine::WinApp::kClientHeight);
    backgroundVeil_ = std::make_unique<myEngine::Sprite>();
    backgroundVeil_->Initialize(myEngine::SpriteCommon::GetInstance(), "resources/white512x512.png");
    backgroundVeil_->SetPosition({0, 0});
    backgroundVeil_->SetSize({w, h});
    backgroundVeil_->SetColor({0.004f, 0.009f, 0.016f, 0.42f});
    backgroundVeil_->Update();
    title_.reset();
    subtitle_.reset();
    hint_.reset();
    demoCaption_.reset();
    menu_[0].reset();
    // デモはゲームのフォント上書きを適用する。ラベルはその後で作るため、
    // 最初に表示するタイトルのフレームから最終的なフォントになる。
    StartAssetPreloads();
    demoReady_ = false;
    revealed_ = false;
    if (!asyncStartup_) {
        while (!demoReady_) {
            if (!InitializeDemoStep()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        revealed_ = true;
    }
}

void TitleScene::CreateLabels()
{
    const float w = static_cast<float>(myEngine::WinApp::kClientWidth);
    const float h = static_cast<float>(myEngine::WinApp::kClientHeight);
    const auto label = [](const std::string& text, float size, myEngine::Vector2 position, const myEngine::Vector4& color,
                          bool rounded = false) {
        myEngine::TextStyle style{};
        style.fontFamily = rounded ? "Zen Maru Gothic" : "Meiryo";
        if (rounded) {
            style.fontPath = "resources/fonts/ZenMaruGothic-Bold.ttf";
        }
        style.fontSize = size;
        style.fontWeight = rounded ? 700 : 400;
        style.color = color;
        style.outlineThickness = 0;
        style.padding = 8;
        auto result = std::make_unique<myEngine::TextLabel>();
        result->Initialize(myEngine::SpriteCommon::GetInstance(), text, style);
        result->SetAnchorPoint({0.5f, 0.5f});
        result->SetPosition(position);
        return result;
    };
    title_ = label("たたかうせんしゃ", 100, {w * 0.5f, h * 0.32f}, {0.91f, 1, 0.96f, 1}, true);
    Fit(*title_, {w * 0.76f, 118});
    subtitle_ = label("強化を重ねて最深部を目指す、ローグライトアクション", 23, {w * 0.5f, h * 0.44f}, {0.73f, 0.91f, 0.94f, 1});
    Fit(*subtitle_, {w * 0.78f, 42});
    menu_[0] = label("遠征をはじめる", 31, {w * 0.5f, h * 0.60f}, {0.47f, 1, 0.76f, 1});
    for (auto& item : menu_) {
        Fit(*item, {w * 0.72f, 58});
    }
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    hint_ = label("クリックで決定 / F7 ボス戦へ直接移動（制作確認・自機無敵）", 15, {w * 0.5f, h - 36}, {0.57f, 0.71f, 0.77f, 1});
#else
    hint_ = label("クリックで決定", 15, {w * 0.5f, h - 36}, {0.57f, 0.71f, 0.77f, 1});
#endif
    demoCaption_ = label(" ", 13, {w - 130, 30}, {0.47f, 0.62f, 0.67f, 1});
    menuSelection_ = 0;
    menuHovered_ = HitTestMenu(input_->GetMousePosition()) == 0;
    UpdateMenuVisuals();
    if (tanksubmission::Enabled()) {
        ++tanksubmission::state.titles;
        tanksubmission::state.returned = tanksubmission::state.titles == 2;
        tanksubmission::state.screens["title"] =
            nlohmann::json::array({title_->GetText(), subtitle_->GetText(), menu_[0]->GetText(), hint_->GetText()});
        tanksubmission::Write();
    }
}

void TitleScene::StartAssetPreloads()
{
    // 何度呼んでも同じ結果になる。GameはDirectXの初期化前にこれを呼び、ファイルのデコードを
    // デバイスやパイプラインの作成と並行させる。タイトルに戻ったときは何もしない。
    auto* textures = myEngine::TextureManager::GetInstance();
    // ゲームが依存するものだけを調べる。動的な文字や無関係なプロジェクト素材は調べない。
    for (const auto* path :
         {"resources/white512x512.png", "resources/skybox.dds", "resources/circle.png", "resources/fade.png", "resources/gradationLine.png",
          "resources/wasd.png", "resources/dashGide.png", "resources/toTitle.png", "resources/ui/salvage_orb.png"}) {
        textures->PreloadTexture(path);
    }
    auto* models = myEngine::ModelManager::GetInstance();
    for (const auto* path : {"ground.obj", "enemy3D.obj", "plane.obj", "player3D.obj", "bloomBlock.obj", "jewelry.obj", "cube.obj",
                             "cubeDamage.obj", "playerHPBarGreenLong.obj", "playerHPBarLong.obj", "expTriangle.obj", "expBlock.obj"}) {
        models->PreloadObjModel(path);
    }
    myEngine::SkinnedModel::PreloadAsset("resources/models/neon_hologram/AvatarSample_B.glb");
}

bool TitleScene::InitializeDemoStep()
{
    myEngine::ModelManager::GetInstance()->PumpPreloads();
    if (!myEngine::ModelManager::GetInstance()->ArePreloadsReady() || !myEngine::TextureManager::GetInstance()->ArePreloadsReady()) {
        return false;
    }
    if (!titleTextNeonEffect_) {
        NeonTextEffectStyle neon{};
        neon.enabled = true;
        neon.glowColor = {0.18f, 1, 0.56f, 1};
        neon.sourceBrightness = 1.55f;
        neon.threshold = 0;
        neon.innerIntensity = 0.48f;
        neon.outerIntensity = 0.22f;
        titleTextNeonEffect_ = std::make_unique<NeonTextEffect>();
        titleTextNeonEffect_->Initialize(myEngine::Object3dCommon::GetInstance()->GetDxCommon(),
                                         myEngine::Object3dCommon::GetInstance()->GetSrvManager());
        titleTextNeonEffect_->SetStyle(neon);
        return true;
    }
    if (!demo_) {
        demo_ = std::make_unique<GameScene>(false, true);
        demo_->EnableTitleDemo();
        myEngine::StartupTrace::Mark("demo.initialize_begin");
        myEngine::StartupTrace::Flush();
    }
    if (demo_->InitializeStep()) {
        demoReady_ = true;
        stageSamples_[0].build = demo_->GetTitleDemoBuild().dump();
        CreateLabels();
        // タイトルとデモを実際に表示した最初のフレームからフェードインする。
        phase_ = Phase::kFadeIn;
        blinkTimer_ = 0;
        fade_->Start(Fade::Status::FadeIn, 0.65f);
        myEngine::StartupTrace::Mark("title.demo_ready");
        myEngine::StartupTrace::Flush();
    }
    return true;
}

void TitleScene::PrepareForTransition()
{
    // 次のワールドを初期化する前に、シミュレーションのコールバックとカメラを切り離す。
    // 画面は既に黒くフェードしているので黒いまま保ち、小さな静止した読み込み表示だけを出して、
    // デモなしでタイトルが再び表示されることがないようにする。
    demo_.reset();
    demoReady_ = false;
    loadingGame_ = true;
    myEngine::Object3dCommon::GetInstance()->SetDefaultCamera(nullptr);
    myEngine::Object3dCommon::GetInstance()->SetDebugDefaultCamera(nullptr);
    demoCaption_->SetText("遠征を準備しています…");
    demoCaption_->SetAnchorPoint({1.0f, 1.0f});
    demoCaption_->SetPosition(
        {static_cast<float>(myEngine::WinApp::kClientWidth) - 28, static_cast<float>(myEngine::WinApp::kClientHeight) - 24});
}

void TitleScene::Update()
{
    constexpr float dt = 1.0f / 60.0f;
    blinkTimer_ += dt;
    if (asyncStartup_ && titleFrames_++ > 0) {
        myEngine::TextureManager::GetInstance()->PumpPreloads();
    }
    if (loadingGame_) {
        PrepareLabels({demoCaption_.get()});
        return;
    }
    if (!demoReady_) {
        // まだ何も表示していないので、vsyncごとにコルーチンを1ステップずつ進めるのではなく、
        // 約100msずつまとめてデモを構築する。メッセージは各区切りで処理する。
        if (!engineReady_) {
            return;
        }
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        while (!demoReady_ && std::chrono::steady_clock::now() < deadline) {
            if (!InitializeDemoStep()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        return;
    }
    demo_->FlushTitleDemoCapture();
    const auto triggered = [this](int key) {
        return input_->IsKeyTriggered(static_cast<uint8_t>(key));
    };
    // 決定入力はバックグラウンドのシミュレーションより前に判定する。そのため
    // 最初の入力でデモ内の射撃・移動・報酬の選択が起こることはない。
    if (phase_ != Phase::kFadeOut) {
        const myEngine::Vector2 mouse = input_->GetMousePosition();
        previousMousePosition_ = mouse;
        const bool click = input_->IsTrigger(input_->GetMouseState().rgbButtons[0], input_->GetPreMouseState().rgbButtons[0]);
        const int hovered = HitTestMenu(mouse);
        if (menuHovered_ != (hovered == 0)) {
            menuHovered_ = hovered == 0;
            UpdateMenuVisuals();
        }
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
        if (triggered(DIK_F7)) {
            if (StartTransitionIfAvailable("TANK_EXPEDITION", 0.5f)) {
                GameStartSession::RequestDeveloperBossStart();
            }
        } else
#endif
            if (myEngine::kDeveloperTools && triggered(DIK_F9)) {
            StartTransitionIfAvailable("TANK_EXPEDITION", 0.5f);
        } else if (triggered(DIK_RETURN) || triggered(DIK_SPACE) || (click && hovered >= 0)) {
            if (menuSelection_ == 0) {
                StartTransitionIfAvailable("TANK_EXPEDITION", 0.65f);
            }
        }
        if (autoTest_ && demoReady_ && capturedStages_ == 15 && demo_->GetTitleDemoStatus().totalSeconds >= 81.0f) {
            StartTransitionIfAvailable("TANK_EXPEDITION", 0.65f);
        }
        if (startupAutoTest_ && blinkTimer_ >= 1.0f) {
            StartTransitionIfAvailable("TANK_EXPEDITION", 0.65f);
        }
        if (tanksubmission::Enabled()) {
            if (demoReady_ && !submissionCaptured_ && blinkTimer_ >= 0.8f) {
                demo_->RequestTitleDemoCapture("submission_title_" + std::to_string(tanksubmission::state.titles));
                submissionCaptured_ = true;
            }
            if (blinkTimer_ >= 1.5f) {
                StartTransitionIfAvailable("TANK_EXPEDITION", 0.65f);
            }
        }
    }
    {
        const auto before = demo_->GetTitleDemoStatus();
        // ゲームが次の固定シーンを復元する前に、一時的なエフェクトをキャプチャする。
        // 6秒時点の静止画1枚だけでは、Overdriveの発動を証明できない。
        if (autoTest_) {
            const auto combat = demo_->GetTitleDemoBuild();
            maxObservedBurstAge_ = (std::max)(maxObservedBurstAge_, combat.value("oldestBurstAge", 0.0f));
            auto& observed = stageSamples_[before.stage];
            observed.maxHomingTurnRate = (std::max)(observed.maxHomingTurnRate, combat.value("homingTurnRate", 0.0f));
            observed.maxDashExplosionsEmitted = (std::max)(observed.maxDashExplosionsEmitted, combat.value("dashExplosionsEmitted", 0u));
            observed.wallBouncesObserved = (std::max)(observed.wallBouncesObserved, combat.value("wallBouncesObserved", uint64_t{0}));
            observed.actorPiercesObserved = (std::max)(observed.actorPiercesObserved, combat.value("actorPiercesObserved", uint64_t{0}));
            observed.splitChildrenObserved = (std::max)(observed.splitChildrenObserved, combat.value("splitChildrenObserved", uint64_t{0}));
        }
        if (!demoFrozen_) {
            demo_->Update();
        }
        const auto& status = demo_->GetTitleDemoStatus();
        if (autoTest_ && status.stage == 0 && status.stageSeconds > 19.85f && !capturedSceneFadeOut_) {
            demo_->RequestTitleDemoCapture("scene_fade_out");
            capturedSceneFadeOut_ = true;
        }
        if (autoTest_ && status.stage == 1 && status.stageSeconds < 0.10f && !capturedSceneFadeIn_) {
            demo_->RequestTitleDemoCapture("scene_fade_in");
            capturedSceneFadeIn_ = true;
        }
        auto& sample = stageSamples_[before.stage];
        sample.shots += (std::max)(0, status.shots - before.shots);
        sample.kills += (std::max)(0, status.kills - before.kills);
        sample.dashes += (std::max)(0, status.dashes - before.dashes);
        if (stageSamples_[status.stage].build.empty()) {
            stageSamples_[status.stage].build = demo_->GetTitleDemoBuild().dump();
        }
        const char* captions[] = {"プレイ例 / 序盤", "プレイ例 / 反射・分裂", "プレイ例 / 強化した戦車", "プレイ例 / ボス戦"};
        demoCaption_->SetText(captions[status.stage]);
        if (autoTest_ && status.stageSeconds >= 6 && !(capturedStages_ & (1u << status.stage))) {
            demo_->RequestTitleDemoCapture("stage_" + std::to_string(status.stage));
            capturedStages_ |= 1u << status.stage;
            WriteDemoValidation(false);
        }
    }
    menu_[menuSelection_]->SetAlpha(menuHovered_ ? 0.90f + std::sin(blinkTimer_ * 3.2f) * 0.10f : 1.0f);
    if (phase_ == Phase::kFadeIn || phase_ == Phase::kFadeOut) {
        fade_->Update();
        if (fade_->IsFinished()) {
            if (phase_ == Phase::kFadeIn) {
                phase_ = Phase::kMain;
            } else {
                finished_ = true;
                if (autoTest_ && demoReady_) {
                    WriteDemoValidation(true);
                }
            }
        }
    }
    PrepareLabels({title_.get(), subtitle_.get(), hint_.get(), demoCaption_.get(), menu_[0].get()});
}

void TitleScene::Draw()
{
    if (demoReady_) {
        demo_->Draw();
    }
}

void TitleScene::DrawShadow()
{
    if (demoReady_) {
        demo_->DrawShadow();
    }
}

void TitleScene::DrawPostEffect3D()
{
    if (demoReady_) {
        demo_->DrawPostEffect3D();
    }
}

void TitleScene::DrawAfterPostEffect3D()
{
    // 暗くするのはゲームの背景だけ。文字の発光と鮮明な元の字形の両方がこの幕に従うため、
    // 動く壁や一斉射撃でメニューのラベルが白飛びすることはない。
    myEngine::SpriteCommon::GetInstance()->PreDraw(myEngine::kNormal);
    backgroundVeil_->SetColor({0.004f, 0.009f, 0.016f, demoReady_ ? 0.42f + 0.58f * demo_->GetTitleDemoFade() : 1.0f});
    backgroundVeil_->Update();
    backgroundVeil_->Draw();
    if (titleTextNeonEffect_) {
        if (menuHovered_) {
            titleTextNeonEffect_->DrawBloom({title_.get(), menu_[0].get()});
        } else {
            titleTextNeonEffect_->DrawBloom({title_.get()});
        }
    }
}

void TitleScene::DrawSprite()
{
    // デモが完了したフレームは、まだ簡易経路で記録される。
    // タイトルがデモと同時に初めて表示されるよう、そのフレームは黒のままにする。
    if (!title_ || !revealed_) {
        revealed_ = demoReady_ || loadingGame_;
        return;
    }
    if (loadingGame_) {
        demoCaption_->Draw();
        return;
    }
    title_->Draw();
    subtitle_->Draw();
    for (int i = 0; i < 1; ++i) {
        if (IsMenuAvailable(i)) {
            menu_[i]->Draw();
        }
    }
    hint_->Draw();
    demoCaption_->Draw();
    if (demoReady_ || phase_ == Phase::kFadeOut) {
        fade_->Draw();
    }
}

void TitleScene::RecordDeveloperFrame(myEngine::DirectXCommon&)
{
    if (demoReady_) {
        demo_->CopyTitleDemoCapture(); // CRT合成後の最終バックバッファ。
    }
}

IScene::ArenaLookSettings TitleScene::GetArenaLookSettings() const
{
    return arenalook::Get();
}

IScene::ScreenEffectState TitleScene::GetScreenEffectState() const
{
    ScreenEffectState state{};
    state.active = true;
    state.suppressOutlines = true;
    state.suppressPostEffectDebugUi = true;
    state.param.gaussianIntensity = 0.88f;
    state.param.isGrayscale = 0.28f;
    state.param.exposure = -0.22f;
    state.param.vignetteIntensity = 0.30f;
    state.param.vignetteScale = 1.2f;
    return state;
}

void TitleScene::UpdateMenuVisuals()
{
    for (int i = 0; i < 1; ++i) {
        const auto size = menu_[i]->GetSprite()->GetSize();
        auto style = menu_[i]->GetStyle();
        style.color = menuHovered_ ? myEngine::Vector4{0.47f, 1, 0.76f, 1} : myEngine::Vector4{0.48f, 0.52f, 0.55f, 1};
        menu_[i]->SetStyle(style);
        menu_[i]->PrepareForDraw();
        menu_[i]->GetSprite()->SetSize(size);
    }
}

bool TitleScene::IsSceneAvailable(std::string_view name) const
{
    return SceneManager::GetInstance()->ContainsScene(name);
}

bool TitleScene::IsMenuAvailable(int selection) const
{
    return selection == 0 && IsSceneAvailable("TANK_EXPEDITION");
}

int TitleScene::HitTestMenu(const myEngine::Vector2& mouse) const
{
    for (int i = 0; i < 1; ++i) {
        if (IsMenuAvailable(i)) {
            auto* sprite = menu_[i]->GetSprite();
            const auto center = sprite->GetPosition(), size = sprite->GetSize();
            if (std::abs(mouse.x - center.x) <= (std::max)(150.0f, size.x * 0.5f) && std::abs(mouse.y - center.y) <= 28) {
                return i;
            }
        }
    }
    return -1;
}

bool TitleScene::StartTransitionIfAvailable(std::string_view name, float duration)
{
    if (!IsSceneAvailable(name) || phase_ == Phase::kFadeOut) {
        return false;
    }
    myEngine::StartupTrace::Mark("transition.request." + std::string(name));
    demoFrozen_ = true;
    frozenAt_ = demoReady_ ? demo_->GetTitleDemoStatus().totalSeconds : 0;
    nextSceneName_ = name;
    if (name == "TANK_EXPEDITION") {
        GameStartSession::SetMode(GameStartMode::Normal);
    }
    fade_->Start(Fade::Status::FadeOut, duration);
    phase_ = Phase::kFadeOut;
    return true;
}

void TitleScene::WriteDemoValidation(bool fadeComplete)
{
    const auto& s = demo_->GetTitleDemoStatus();
    std::ofstream report("generated/title_demo/validation.json");
    report << "{\"completed\":false,\"testMode\":true,\"fixedSeed\":20260925,\"stagesVisited\":" << s.stagesVisitedMask
           << ",\"sceneFadeCaptured\":" << (capturedSceneFadeOut_ && capturedSceneFadeIn_ ? "true" : "false")
           << ",\"maxBurstAge\":" << maxObservedBurstAge_ << ",\"capturedStages\":" << capturedStages_ << ",\"seconds\":" << s.totalSeconds
           << ",\"projectileEmissionSamples\":" << s.shots << ",\"dashes\":" << s.dashes << ",\"kills\":" << s.kills
           << ",\"rewards\":" << s.rewards << ",\"routes\":" << s.routes << ",\"maxPlayerBullets\":" << s.maxPlayerBullets
           << ",\"fadeComplete\":" << (fadeComplete ? "true" : "false")
           << ",\"demoFrozenDuringFade\":" << (demoFrozen_ && s.totalSeconds == frozenAt_ ? "true" : "false")
           << ",\"transitionRequested\":\"" << nextSceneName_ << "\",\"stageSamples\":[";
    for (size_t i = 0; i < stageSamples_.size(); ++i) {
        const auto& sample = stageSamples_[i];
        report << (i ? "," : "") << "{\"build\":" << (sample.build.empty() ? "{}" : sample.build)
               << ",\"projectileEmissionSamples\":" << sample.shots << ",\"kills\":" << sample.kills << ",\"dashes\":" << sample.dashes
               << ",\"maxHomingTurnRate\":" << sample.maxHomingTurnRate
               << ",\"maxDashExplosionsEmitted\":" << sample.maxDashExplosionsEmitted
               << ",\"wallBouncesObserved\":" << sample.wallBouncesObserved << ",\"actorPiercesObserved\":" << sample.actorPiercesObserved
               << ",\"splitChildrenObserved\":" << sample.splitChildrenObserved << "}";
    }
    report << "]}\n";
}
