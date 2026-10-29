#include "game/run/session/SurvivorRunController.h"
#include "game/editor/SurvivorTuningPanel.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/session/GameplaySystems.h"
#include "game/player/TankRunModifiers.h"
#include "game/render/settings/ArenaCameraProfile.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>

// 育つ戦車の試作の進行の中心（初期化・毎フレームの更新・調整パネル・画面の上の表示）。
// 湧き方は SurvivorRunSpawns.cpp、部品と3択は SurvivorRunLoadout.cpp、結果画面は SurvivorRunResult.cpp、
// 自動確認は SurvivorRunAutoTest.cpp に分けてある。

namespace gameplay {

using namespace survivorrun;

void SurvivorRunController::Initialize()
{
    // 遠征と同じ自機にする（遠征も ArenaRunController の初期化で改造の倍率を有効にしている）。
    TankRunModifiers modifiers{};
    modifiers.enabled = true;
    modifiers.survivor = true;
    world_.resources.player_->SetRunModifiers(modifiers);
    world_.presentation.showPostProfileOverlay_ = false;
    world_.presentation.showLevelAiEditorPreview_ = false;
    world_.presentation.showGameDebugConsole_ = false;
    world_.runVisualSettings->InitializeTankRunVisuals();

    // 湧き方の設定と、敵戦車の性能（遠征と同じ制作カタログ）を読む。読めなければ既定値で続ける。
    std::string error;
    if (!survivor::LoadSpawnConfig(kConfigPath, config_, error)) {
        OutputDebugStringA(("[Survivor] " + error + "\n").c_str());
    }
    tankcontent::Catalog content = tankcontent::DefaultCatalog();
    if (tankcontent::LoadCatalog(kContentPath, content, error)) {
        world_.resources.enemyManager_->SetExpeditionContent(content);
    }

    // レベルデータが置いた敵と出現領域を片付け、宿敵も出さない。
    world_.resources.enemy_->SetRunEncounterEnabled(false);
    world_.resources.enemyManager_->ClearRunActors();
    world_.resources.bulletManager_->ClearAll();

    // 試作用の広い部屋に差し替え、自機とカメラを中央に置く。カメラは CombatFramePipeline が自機を追い、
    // 部屋の外枠（ArenaBounds）からはみ出さないように止める。
    if (!world_.resources.stage_->LoadRunMap(kArenaMap)) {
        OutputDebugStringA("[Survivor] arena map could not be loaded\n");
    }
    world_.presentation.stagePostCacheValid_ = false;
    world_.resources.player_->ResetRunRoomState(kPlayerStart);
    // 試作では熱とシールドを基本で持つ。シールドの上限は最大HPに合わせて変わる。
    Player::ShieldConfig shield{};
    shield.enabled = true;
    shield.maxHpRatio = config_.shieldMaxHpRatio;
    shield.regenDelay = config_.shieldRegenDelay;
    shield.regenPerSecond = config_.shieldRegenPerSecond;
    world_.resources.player_->ConfigureSurvivorResources(config_.heat, shield);
    world_.resources.player_->SetShiftDashEnabled(true);
    world_.resources.camera->SetTranslate({kPlayerStart.x, kPlayerStart.y, world_.resources.camera->GetTranslate().z});
    world_.resources.camera->Update();

    // 自機が倒した敵は、経験値を直接渡さずに欠片を落とす。コールバックは衝突処理の途中から呼ばれるので、欠片の列に足すだけにする。
    ConfigurePickups();
    // 大五角は宝箱：倒すと、軽い部品の出ない3択を1つ貯める。
    ExpEnemy::SetPlayerExpDropCallback([this](const ExpEnemy& enemy, uint32_t value) {
        pickups_.Drop(enemy.GetWorldPosition(), value, random_);
        expDropped_ += static_cast<int>(value);
        if (enemy.GetType() == ExpEnemyType::Pentagon && enemy.GetSizeScale() > 1.5f) {
            ++pendingChoices_;
            ++pendingPremium_;
            ++premiumTotal_;
            premiumGained_ = true;
            pendingSince_.push_back(elapsed_);
        }
    });

    nextSwarm_ = config_.swarm.from;
    nextTank_ = config_.tanks.from;
    nextBigPentagon_ = config_.bigPentagon.from;

    myEngine::TextStyle style{};
    style.fontFamily = "Meiryo";
    style.fontSize = 22;
    style.color = {0.82f, 0.97f, 1.0f, 1.0f};
    style.outlineThickness = 0;
    style.padding = 4;
    hudText_ = std::make_unique<myEngine::TextLabel>();
    hudText_->Initialize(myEngine::SpriteCommon::GetInstance(), " ", style);
    hudText_->SetPosition({24.0f, 16.0f});
    auto bar = [](const myEngine::Vector2& position, const myEngine::Vector2& size, const myEngine::Vector4& color) {
        auto sprite = std::make_unique<myEngine::Sprite>();
        sprite->Initialize(myEngine::SpriteCommon::GetInstance(), "resources/white512x512.png");
        sprite->SetPosition(position);
        sprite->SetSize(size);
        sprite->SetColor(color);
        sprite->Update();
        return sprite;
    };
    // 画面の上端の経験値のゲージ。
    expTrack_ = bar({0.0f, 0.0f}, {static_cast<float>(myEngine::WinApp::kClientWidth), 8.0f}, {0.05f, 0.09f, 0.12f, 0.85f});
    expFill_ = bar({0.0f, 0.0f}, {0.0f, 8.0f}, {0.35f, 1.0f, 0.78f, 1.0f});
    style.fontSize = 26;
    style.color = {1.0f, 0.86f, 0.36f, 1.0f};
    exitText_ = std::make_unique<myEngine::TextLabel>();
    exitText_->Initialize(myEngine::SpriteCommon::GetInstance(), "出口が開いた  ─  光る輪に入ると区画クリア", style);
    exitText_->SetPosition({24.0f, 52.0f});
    // Q の部品の状態（画面の下の中ほど）。
    style.fontSize = 24;
    style.color = {1.0f, 0.62f, 0.30f, 1.0f};
    qText_ = std::make_unique<myEngine::TextLabel>();
    qText_->Initialize(myEngine::SpriteCommon::GetInstance(), " ", style);
    qText_->SetPosition({520.0f, 676.0f});
    // 演出の上下の黒い帯と題字。
    cineBarTop_ = bar({0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.92f});
    cineBarBottom_ = bar({0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.92f});
    style.fontSize = 40;
    style.color = {1.0f, 0.55f, 0.45f, 1.0f};
    // 題字は文字ごとに1つ作り、遊んでいる間は文字を変えない。
    auto title = [&](const char* text) {
        auto label = std::make_unique<myEngine::TextLabel>();
        label->Initialize(myEngine::SpriteCommon::GetInstance(), text, style);
        label->SetAnchorPoint({0.5f, 0.5f});
        label->SetPosition({static_cast<float>(myEngine::WinApp::kClientWidth) * 0.5f, static_cast<float>(myEngine::WinApp::kClientHeight) - 43.0f});
        return label;
    };
    cineTitleIntro_ = title("宿敵　出現");
    cineTitleDefeat_ = title("宿敵　撃破");
    // 群れの予告の「！」。
    style.fontSize = 64;
    style.color = {1.0f, 0.25f, 0.25f, 1.0f};
    for (auto& warning : warningTexts_) {
        warning = std::make_unique<myEngine::TextLabel>();
        warning->Initialize(myEngine::SpriteCommon::GetInstance(), "！", style);
    }
    choiceHud_.Initialize();
    choiceHud_.SetPending(0, false);
    loadoutHud_.Initialize();
    loadoutHud_.SetLoadout(config_.parts, partLevels_, Capacity());

    // 結果画面：暗い板の上に、生き延びた時間・レベル・撃破数・形態・部品と、もう1回／タイトルへ。
    resultPanel_ = bar({240.0f, 205.0f}, {800.0f, 500.0f}, {0.01f, 0.03f, 0.05f, 0.82f});
    style.fontSize = 24;
    style.color = {0.86f, 0.97f, 1.0f, 1.0f};
    style.outlineThickness = 2;
    resultText_ = std::make_unique<myEngine::TextLabel>();
    resultText_->Initialize(myEngine::SpriteCommon::GetInstance(), " ", style);
    resultText_->SetPosition({290.0f, 225.0f});
    style.fontSize = 28;
    style.color = {0.48f, 1.0f, 0.66f, 1.0f};
    resultMenu_ = std::make_unique<myEngine::TextLabel>();
    resultMenu_->Initialize(myEngine::SpriteCommon::GetInstance(), " ", style);
    resultMenu_->SetPosition({290.0f, 612.0f});
    fpsClock_ = std::chrono::steady_clock::now();
    RefreshHud();

    wchar_t automatic[8]{};
    const DWORD length = GetEnvironmentVariableW(L"MYENGINE_SURVIVOR_AUTOTEST", automatic, 8);
    if (!world_.demo.titleDemo_ && length > 0) {
        const std::wstring mode(automatic);
        autoTest_ = mode == L"tour"   ? AutoTest::Tour
                    : mode == L"perf" ? AutoTest::Perf
                    : mode[0] == L'1' ? AutoTest::Sector
                                      : AutoTest::None;
    }
    if (autoTest_ == AutoTest::Perf) {
        // 図形を200体に保ち、敵戦車8体と大五角3体を最初から出す。形態の2択と出口は出さない。
        config_.shapeTargetAlive = {{0.0f, 200.0f}};
        config_.shapeOvertimePerMinute = 0.0f;
        config_.maxSpawnsPerSecond = 100.0f;
        config_.swarmCount = 0;
        config_.tanks = {0.0f, 1.0f, 8};
        config_.bigPentagon = {0.0f, 4.0f, 3};
        config_.exitOpenSeconds = 9999.0f;
        config_.choiceAutoOpen = false;
        formChosen_ = true;
    }
    random_.seed(autoTest_ != AutoTest::None ? 20261028u : static_cast<uint32_t>(GetTickCount64()));
    if (autoTest_ != AutoTest::None) {
        world_.presentation.debugPlayerNoDamage_ = true;
        world_.resources.player_->SetDebugNoDamage(true);
        std::filesystem::create_directories(kAutoTestDirectory);
        sampleStart_ = std::chrono::steady_clock::now();
        wchar_t formName[16]{};
        const DWORD formLength = GetEnvironmentVariableW(L"MYENGINE_SURVIVOR_FORM", formName, 16);
        if (formLength > 0 && std::wstring(formName) == L"twin") {
            autoFormIndex_ = static_cast<size_t>(survivor::FormId::Twin);
        }
        // MYENGINE_SURVIVOR_SEPARATION=0：敵どうしの押し離しを止める（fps の比べ方の確認用）。
        wchar_t separation[4]{};
        if (GetEnvironmentVariableW(L"MYENGINE_SURVIVOR_SEPARATION", separation, 4) > 0 && separation[0] == L'0') {
            separationEnabled_ = false;
        }
        // 四隅の確認では、指定があれば最初から形態に変えて、止まった自機の形を写す。
        autoTourForm_ = formLength > 0;
        // MYENGINE_SURVIVOR_LIGHT3=1：軽い部品をすべて3段から始め、3段の変化をまとめて確かめる。
        wchar_t light3[4]{};
        if (GetEnvironmentVariableW(L"MYENGINE_SURVIVOR_LIGHT3", light3, 4) > 0 && light3[0] == L'1') {
            for (size_t i = 0; i < survivor::kPartCount; ++i) {
                if (config_.parts[i].light) {
                    partLevels_[i] = survivor::kLightChangeLevel;
                }
            }
            ApplyParts();
        }
        // MYENGINE_SURVIVOR_RIVAL_AT=秒：宿敵が現れる区画の時刻を早める（宿敵の演出の確認用）。
        wchar_t rivalAt[16]{};
        if (GetEnvironmentVariableW(L"MYENGINE_SURVIVOR_RIVAL_AT", rivalAt, 16) > 0) {
            config_.exitOpenSeconds = (std::max)(1.0f, static_cast<float>(_wtof(rivalAt)));
        }
        // MYENGINE_SURVIVOR_START_PARTS=heatVent=1,pierce=3：指定した部品を、その Lv で積んだ状態から始める（部品ごとの確認用）。
        wchar_t startParts[256]{};
        const DWORD startLength = GetEnvironmentVariableW(L"MYENGINE_SURVIVOR_START_PARTS", startParts, 256);
        if (startLength > 0 && startLength < 256) {
            std::wstringstream list(startParts);
            std::wstring item;
            while (std::getline(list, item, L',')) {
                const size_t equal = item.find(L'=');
                // 部品の名前は英字だけなので、1文字ずつ char に移す。
                std::string key;
                for (size_t c = 0; c < (equal == std::wstring::npos ? item.size() : equal); ++c) {
                    key.push_back(static_cast<char>(item[c]));
                }
                const int level = equal == std::wstring::npos ? 1 : _wtoi(item.c_str() + equal + 1);
                for (size_t i = 0; i < survivor::kPartCount; ++i) {
                    if (key == config_.parts[i].key) {
                        partLevels_[i] = (std::clamp)(level, 0, config_.parts[i].maxLevel);
                    }
                }
            }
            ApplyParts();
        }
    }
}

void SurvivorRunController::Update(float dt)
{
    if (world_.combat.phase_ != Phase::kMain) {
        return;
    }
    if (autoTest_ == AutoTest::Tour) {
        UpdateTourAutoTest(dt);
        return;
    }
    UpdateTuningPanel();
    UpdateCinematic(dt);
    SampleFps();
    if (formChoiceOpen_) {
        UpdateFormChoiceInput();
    } else if (world_.combat.combatFlow_.GetState() == GameFlowState::Playing && !cleared_) {
        UpdateChoiceInput();
    } else if (choiceOpen_) {
        CloseChoices();
    }
    if (choiceOpen_) {
        choiceOpenSeconds_ += dt;
    }
    // 区画の時計・湧き方・欠片は、3択を開いている間のスローに合わせて遅くし、形態を選ぶ間と調整パネルで止めた間は止める。
    const float gameDt = IsPaused() ? 0.0f : dt * GetTimeScale();
    if (world_.combat.combatFlow_.GetState() == GameFlowState::Playing && !cleared_) {
        elapsed_ += gameDt;
        const PlayerHeat& heat = world_.resources.player_->GetHeat();
        if (heat.GetHeat() >= heat.GetMaxHeat() * 0.7f) {
            heatHighSeconds_ += gameDt;
        }
        UpdateSpawns(gameDt);
        AddExperience(pickups_.Update(gameDt, world_.resources.player_->GetWorldPosition(),
                                      survivor::CollectorMagnetScale(config_.parts, partLevels_),
                                      survivor::HasChange(partLevels_, survivor::PartId::Collector)));
        if (gameDt > 0.0f && survivor::HasChange(partLevels_, survivor::PartId::Ram)) {
            UpdateSpikes();
        }
        // Q は3択を開いている間（スロー中）には使わない。
        if (gameDt > 0.0f && !choiceOpen_) {
            UpdateQ(gameDt);
        }
        RefreshMorale(dt);
        if (gameDt > 0.0f && separationEnabled_) {
            const auto separationStart = std::chrono::steady_clock::now();
            SeparateEnemies();
            separationSeconds_ += std::chrono::duration<double>(std::chrono::steady_clock::now() - separationStart).count();
            ++separationCalls_;
        }
        if (premiumGained_) {
            premiumGained_ = false;
            world_.combatFlow->SetEventCallout("宝箱：質の高い選択 +1", 0.9f);
            choiceHud_.SetPending(pendingChoices_, choiceOpen_);
        }
        UpdateThreat();
        // exitOpenSeconds で宿敵が現れ、倒すと出口が開く（OnRivalDefeated）。宿敵を使わない設定なら、時間で出口を開く。
        if (!exitOpen_ && !rivalSpawned_ && elapsed_ >= config_.exitOpenSeconds) {
            if (config_.rival.enabled) {
                SpawnRival();
            } else {
                OpenExit();
            }
        }
        if (exitOpen_) {
            const myEngine::Vector3 offset = world_.resources.player_->GetWorldPosition() - exitPosition_;
            if (myEngine::Length(myEngine::Vector3{offset.x, offset.y, 0.0f}) <= config_.exitRadius) {
                // 更新の初めなので、衝突処理の途中ではない。敵と弾を片付けてから結果画面へ移る。
                cleared_ = true;
                CloseChoices();
                pickups_.Clear();
                world_.resources.enemyManager_->ClearRunActors();
                world_.resources.bulletManager_->ClearAll();
                world_.combatFlow->EnterResultState(true);
            }
        }
    }
    hudRefresh_ -= dt;
    if (hudRefresh_ <= 0.0f) {
        hudRefresh_ = 0.1f;
        RefreshHud();
    }
    // 結果画面：出た最初の更新で遊んだ記録を書く。R キーで、その場からもう1回。
    if (IsResultVisible()) {
        if (!runLogWritten_) {
            runLogWritten_ = true;
            WriteRunLog();
            RefreshResult(world_.combat.resultSelection_);
        }
        if (autoTest_ == AutoTest::None && world_.resources.input_->IsKeyTriggered(DIK_R)) {
            world_.combat.resultSelection_ = 0;
            world_.combatFlow->ConfirmResultSelection();
            return;
        }
    }
    if (autoTest_ == AutoTest::Sector) {
        UpdateSectorAutoTest(dt);
    } else if (autoTest_ == AutoTest::Perf) {
        UpdatePerfAutoTest(dt);
    }
}

void SurvivorRunController::ConfigurePickups()
{
    SurvivorPickups::Config pickupConfig;
    pickupConfig.magnetRadius = config_.magnetRadius;
    pickupConfig.collectRadius = config_.collectRadius;
    pickupConfig.valuePerShard = config_.valuePerShard;
    pickupConfig.maxShards = static_cast<size_t>((std::max)(1, config_.maxShards));
    pickups_.SetConfig(pickupConfig);
}

void SurvivorRunController::UpdateTuningPanel()
{
#if defined(USE_IMGUI)
    if (autoTest_ == AutoTest::None && world_.resources.input_->IsKeyTriggered(DIK_F2)) {
        tuningOpen_ = !tuningOpen_;
        tuningStatus_.clear();
    }
    if (!tuningOpen_) {
        return;
    }
    const PlayerHeat& heat = world_.resources.player_->GetHeat();
    survivor::TuningSnapshot snapshot;
    snapshot.elapsed = elapsed_;
    snapshot.level = level_;
    snapshot.exp = exp_;
    snapshot.alive = CountAlive().total;
    snapshot.fps = fpsSamples_.empty() ? 0.0f : fpsSamples_.back();
    snapshot.heat = heat.GetHeat();
    snapshot.heatMax = heat.GetMaxHeat();
    snapshot.overheats = heat.GetOverheatCount();
    const survivor::TuningPanelResult result = survivor::DrawTuningPanel(config_, tuningOpen_, tuningPause_, tuningStatus_, snapshot);
    if (result.reloadRequested) {
        survivor::SpawnConfig loaded;
        std::string error;
        if (survivor::LoadSpawnConfig(kConfigPath, loaded, error)) {
            config_ = loaded;
            tuningStatus_ = "再読込しました";
        } else {
            tuningStatus_ = error;
        }
    }
    if (result.changed || result.reloadRequested) {
        // 部品の倍率・熱・シールド・欠片を、今の値で作り直す。形態の数値は次に選んだときから効く。
        ConfigurePickups();
        ApplyParts();
        if (result.changed) {
            tuningStatus_ = "変更あり（未保存）";
        }
    }
    if (result.saveRequested) {
        std::string error;
        tuningStatus_ = survivor::SaveSpawnConfig(kConfigPath, config_, error) ? std::string("保存しました") : error;
    }
#endif
}

void SurvivorRunController::OnEnemyDefeated(const myEngine::Vector3& position)
{
    world_.combat.screenEffectDirector_.TriggerEnemyDefeat(world_.gameplayQueries->WorldToScreenUv(position), 1.0f);
    // 戦意：積んでいる間に倒した数を数える（倍率への反映は RefreshMorale がまとめて行う）。
    if (survivor::LevelOf(partLevels_, survivor::PartId::Morale) > 0) {
        ++moraleKills_;
    }
}

void SurvivorRunController::QueueWorldVisuals()
{
    if (cleared_ || !world_.resources.neonGridRenderer_) {
        return;
    }
    // 欠片：カメラを向いた小さな菱形。画面の近くのものだけを積む。
    const myEngine::Matrix4x4 cameraWorld = myEngine::Inverse(world_.resources.camera->GetViewMatrix());
    const myEngine::Vector3 cameraRight = myEngine::Normalize({cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2]});
    const myEngine::Vector3 cameraUp = myEngine::Normalize({cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2]});
    pickups_.Queue(*world_.resources.neonGridRenderer_, cameraRight, cameraUp, world_.resources.camera->GetTranslate(),
                   arenacamera::HalfViewHeight() * arenacamera::kAspect + 2.0f, arenacamera::HalfViewHeight() + 2.0f);
    // 予告：輪は自機のまわりに、群れは湧く場所に、赤い破線の円を描く。湧く時刻が近いほど速く点滅する。
    for (const PendingWave& wave : pendingWaves_) {
        const float left = (std::max)(0.0f, wave.at - elapsed_);
        const float blink = 0.5f + 0.5f * std::sin(elapsed_ * (10.0f + 14.0f * (1.0f - left / (std::max)(0.1f, config_.telegraph.lead))));
        const myEngine::Vector3 center = wave.ring ? world_.resources.player_->GetWorldPosition() : wave.center;
        const float radius = wave.ring ? config_.threat.ringRadius : config_.swarmSpread + 1.5f;
        const myEngine::Vector4 color{1.8f, 0.18f, 0.22f, 0.45f + 0.55f * blink};
        constexpr int kDashes = 48;
        for (int n = 0; n < kDashes; n += 2) {
            const float a = 6.2831853f * static_cast<float>(n) / kDashes, b = 6.2831853f * static_cast<float>(n + 1) / kDashes;
            world_.resources.neonGridRenderer_->QueueLine(center + myEngine::Vector3{std::cos(a) * radius, std::sin(a) * radius, -0.3f},
                                                          center + myEngine::Vector3{std::cos(b) * radius, std::sin(b) * radius, -0.3f},
                                                          0.16f, color);
        }
    }
    if (!exitOpen_) {
        return;
    }
    // 出口：脈打つ2重の輪。輪の中に入ると区画クリア。
    const float pulse = 0.5f + 0.5f * std::sin(elapsed_ * 5.0f);
    auto circle = [&](float radius, float width, const myEngine::Vector4& color) {
        constexpr int kSides = 40;
        for (int n = 0; n < kSides; ++n) {
            const float a = 6.2831853f * static_cast<float>(n) / kSides, b = 6.2831853f * static_cast<float>(n + 1) / kSides;
            world_.resources.neonGridRenderer_->QueueLine(
                exitPosition_ + myEngine::Vector3{std::cos(a) * radius, std::sin(a) * radius, -0.3f},
                exitPosition_ + myEngine::Vector3{std::cos(b) * radius, std::sin(b) * radius, -0.3f}, width, color);
        }
    };
    circle(config_.exitRadius, 0.22f, {1.6f, 1.25f, 0.35f, 1.0f});
    circle(config_.exitRadius * (0.55f + 0.25f * pulse), 0.14f, {1.2f, 0.95f, 0.25f, 0.8f});
}

void SurvivorRunController::RefreshHud()
{
    if (!hudText_) {
        return;
    }
    const AliveCounts alive = CountAlive();
    std::ostringstream text;
    text << "Lv " << level_ << "   区画  " << Clock(elapsed_) << "   脅威 Lv" << threatStage_;
    if (IsRivalActive()) {
        const auto& rival = *world_.resources.enemy_;
        text << "   宿敵 " << (std::max)(0, rival.GetHp() * 100 / (std::max)(1, rival.GetMaxHp())) << "%";
    } else if (!exitOpen_) {
        text << (config_.rival.enabled ? "   宿敵まで " : "   出口まで ") << Clock(config_.exitOpenSeconds - elapsed_);
    }
    text << "   敵 " << alive.total;
    hudText_->SetText(text.str());
    if (qText_) {
        const std::string q = DescribeQ();
        qText_->SetText(q.empty() ? std::string(" ") : q);
    }
    if (expFill_) {
        const float ratio = level_ >= config_.maxLevel
                                ? 1.0f
                                : (std::clamp)(static_cast<float>(exp_) / static_cast<float>(config_.ExpToNextLevel(level_)), 0.0f, 1.0f);
        expFill_->SetSize({static_cast<float>(myEngine::WinApp::kClientWidth) * ratio, 8.0f});
        expFill_->Update();
    }
}

void SurvivorRunController::DrawUi()
{
    ++drawnFrames_;
    if (IsResultVisible() && resultPanel_ && resultText_ && resultMenu_) {
        myEngine::SpriteCommon::GetInstance()->PreDraw(myEngine::kNormal);
        resultPanel_->Draw();
        resultText_->Draw();
        resultMenu_->Draw();
        return;
    }
    if (world_.combat.combatFlow_.GetState() != GameFlowState::Playing || !hudText_) {
        // 自機が倒れた演出の間も、上下の黒い帯は描く。
        DrawCinematic();
        return;
    }
    myEngine::SpriteCommon::GetInstance()->PreDraw(myEngine::kNormal);
    if (expTrack_ && expFill_) {
        expTrack_->Draw();
        expFill_->Draw();
    }
    hudText_->Draw();
    if (exitOpen_ && exitText_) {
        exitText_->Draw();
    }
    if (qText_ && survivor::OwnedQSkill(config_.parts, partLevels_) != survivor::PartId::Count) {
        qText_->Draw();
    }
    // 予告の「！」：湧く群れの方向へ、画面の端（画面の中なら、その場所）に出す。
    size_t warning = 0;
    for (const PendingWave& wave : pendingWaves_) {
        if (wave.ring || warning >= warningTexts_.size() || !warningTexts_[warning]) {
            continue;
        }
        const myEngine::Vector2 screen = world_.gameplayHud->WorldToScreen(wave.center);
        const float x = (std::clamp)(screen.x, 96.0f, static_cast<float>(myEngine::WinApp::kClientWidth) - 96.0f);
        const float y = (std::clamp)(screen.y, 120.0f, static_cast<float>(myEngine::WinApp::kClientHeight) - 110.0f);
        const float blink = 0.5f + 0.5f * std::sin(elapsed_ * 16.0f);
        warningTexts_[warning]->SetPosition({x - 20.0f, y - 34.0f});
        warningTexts_[warning]->SetAlpha(0.55f + 0.45f * blink);
        warningTexts_[warning]->Draw();
        ++warning;
    }
    // 外す画面は持っている部品を並べるので、左下の積載の表示は出さない。
    if (removalOpen_ && choiceOpen_) {
        loadoutHud_.DrawRemoval();
        return;
    }
    loadoutHud_.DrawLoadout();
    choiceHud_.Draw(choiceOpen_ || formChoiceOpen_, formChoiceOpen_ ? 0 : pendingChoices_);
    DrawCinematic();
}

} // namespace gameplay
