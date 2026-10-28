#include "game/run/session/SurvivorRunController.h"
#include "game/editor/SurvivorTuningPanel.h"
#include "game/session/GameplaySystems.h"
#include "game/player/TankRunModifiers.h"
#include "game/render/settings/ArenaCameraProfile.h"
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>

namespace gameplay {

namespace {
constexpr const char* kAutoTestDirectory = "generated/tank_survivor";
constexpr const char* kConfigPath = "resources/configs/survivor_proto.json";
constexpr const char* kContentPath = "resources/configs/expedition_content.json";
// 格子 45×30 全体を外壁で囲んだ部屋（約90×60）。画面の約2×2.4倍の広さ。
constexpr const char* kArenaMap = "resources/maps/survivor_arena.csv";
const myEngine::Vector3 kPlayerStart{44.0f, 28.0f, 0.0f};
// 外壁の内側で、敵を置いてよい範囲（ワールド座標）。
constexpr float kSpawnMinX = 3.0f, kSpawnMaxX = 85.0f, kSpawnMinY = 3.0f, kSpawnMaxY = 55.0f;
// 出口の候補。辺の中ほどの柱の外側（壁際）に置く。
const std::array<myEngine::Vector3, 4> kExitCandidates{
    {{44.0f, 52.0f, 0.0f}, {44.0f, 5.0f, 0.0f}, {6.0f, 28.0f, 0.0f}, {82.0f, 28.0f, 0.0f}}};

struct AutoTestWaypoint {
    const char* name;
    myEngine::Vector3 target;
};

// 中央から四隅を回って中央へ戻る。柱の並ぶ線（x=16〜18・30・58・70〜72、y=8・28〜30・50）を避けた経路にする。
const std::array<AutoTestWaypoint, 5> kTourWaypoints{
    {{"northeast", {82, 54, 0}}, {"southeast", {82, 4, 0}}, {"southwest", {6, 4, 0}}, {"northwest", {6, 54, 0}}, {"center", {44, 28, 0}}}};
// 入力を止めても少し滑るので、着いたかどうかはこの距離で見る。
constexpr float kAutoTestReachDistance = 4.0f;
// 区画の自動確認で画面を保存する時刻（秒）。
constexpr std::array<float, 3> kSectorCaptureSeconds{30.0f, 125.0f, 183.0f};
// 区画の自動確認が出口へ向かい始める時刻（秒）。
constexpr float kSectorAutoTestExitSeconds = 240.0f;

std::string Clock(float seconds)
{
    const int total = (std::max)(0, static_cast<int>(seconds));
    std::ostringstream text;
    text << total / 60 << ":" << std::setw(2) << std::setfill('0') << total % 60;
    return text.str();
}
} // namespace

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
    choiceHud_.Initialize();
    choiceHud_.SetPending(0, false);

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

bool SurvivorRunController::IsResultVisible() const
{
    const auto& flow = world_.combat.combatFlow_;
    return flow.GetState() == GameFlowState::StageClear || (flow.GetState() == GameFlowState::GameOver && flow.GetTimer() <= 0.0f);
}

std::string SurvivorRunController::FormName() const
{
    for (const auto& form : config_.forms) {
        if (formKey_ == form.key) {
            return form.name;
        }
    }
    return "基本砲";
}

std::string SurvivorRunController::DescribeParts(size_t perLine) const
{
    std::string text;
    size_t count = 0;
    for (size_t i = 0; i < survivor::kPartCount; ++i) {
        if (partLevels_[i] <= 0) {
            continue;
        }
        if (count > 0) {
            text += count % perLine == 0 ? "\n" : "　";
        }
        text += std::string(config_.parts[i].name) + " Lv" + std::to_string(partLevels_[i]);
        ++count;
    }
    return count > 0 ? text : std::string("なし");
}

void SurvivorRunController::RefreshResult(int selection)
{
    if (!resultText_ || !resultMenu_) {
        return;
    }
    const auto& player = *world_.resources.player_;
    std::ostringstream text;
    // 見出し（区画クリア・撃破された）は、上の帯の文字（CombatFlow）が出す。
    text << (cleared_ ?"区画の時間　　" : "生き延びた時間　") << Clock(elapsed_) << "\n"
         << "レベル　　　　" << level_ << "\n"
         << "撃破　　　　　" << world_.combat.defeatedEnemies_ << "\n"
         << "形態　　　　　" << FormName() << "\n"
         << "脅威　　　　　Lv" << threatStage_ << "　　宿敵 "
         << (rivalDefeated_ ? "撃破（" + Clock(rivalDefeatedAt_) + "）" : rivalSpawned_ ? std::string("未撃破") : std::string("出る前")) << "\n"
         << "受けたダメージ　" << world_.combat.damageTaken_ << "　　オーバーヒート " << player.GetHeat().GetOverheatCount() << " 回\n"
         << "部品\n"
         << DescribeParts(4);
    resultText_->SetText(text.str());
    std::ostringstream menu;
    menu << (selection == 0 ? "> " : "   ") << "もう1回（R）\n"
         << (selection == 1 ? "> " : "   ") << "タイトルへ";
    resultMenu_->SetText(menu.str());
}

void SurvivorRunController::SampleFps()
{
    const auto now = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(now - fpsClock_).count();
    if (seconds < 5.0) {
        return;
    }
    fpsSamples_.push_back(static_cast<float>(static_cast<double>(drawnFrames_ - fpsFrames_) / seconds));
    fpsClock_ = now;
    fpsFrames_ = drawnFrames_;
}

void SurvivorRunController::WriteRunLog()
{
    if (!myEngine::kDeveloperTools || world_.demo.titleDemo_) {
        return;
    }
    // 自分で遊んだ回を後で見比べるための記録。自動確認では同じ名前に上書きする。
    const std::string directory = std::string(kAutoTestDirectory) + "/runs";
    std::error_code ignored;
    std::filesystem::create_directories(directory, ignored);
    std::string name = "autotest";
    if (autoTest_ == AutoTest::None) {
        SYSTEMTIME time{};
        GetLocalTime(&time);
        std::ostringstream stamp;
        stamp << std::setfill('0') << time.wYear << std::setw(2) << time.wMonth << std::setw(2) << time.wDay << "_" << std::setw(2)
              << time.wHour << std::setw(2) << time.wMinute << std::setw(2) << time.wSecond;
        name = stamp.str();
    }
    const auto& player = *world_.resources.player_;
    float fpsMin = 0.0f, fpsSum = 0.0f;
    for (size_t i = 0; i < fpsSamples_.size(); ++i) {
        fpsMin = i == 0 ? fpsSamples_[i] : (std::min)(fpsMin, fpsSamples_[i]);
        fpsSum += fpsSamples_[i];
    }
    std::ofstream output(directory + "/run_" + name + ".json");
    output << "{\"cleared\":" << (cleared_ ? "true" : "false") << ",\"seconds\":" << elapsed_ << ",\"level\":" << level_
           << ",\"kills\":" << world_.combat.defeatedEnemies_ << ",\"damageTaken\":" << world_.combat.damageTaken_
           << ",\"form\":\"" << formKey_ << "\",\"threatStage\":" << threatStage_ << ",\"rings\":" << ringsSpawned_
           << ",\"rivalSpawnedAt\":" << rivalSpawnedAt_ << ",\"rivalDefeatedAt\":" << rivalDefeatedAt_ << ",\"levelTimes\":[";
    for (size_t i = 0; i < levelTimes_.size(); ++i) {
        output << (i ? "," : "") << std::lround(levelTimes_[i]);
    }
    output << "],\"partLevels\":{";
    for (size_t i = 0; i < survivor::kPartCount; ++i) {
        output << (i ? "," : "") << "\"" << config_.parts[i].key << "\":" << partLevels_[i];
    }
    output << "},\"choices\":[" << choiceRecord_.str() << "],\"pendingAtEnd\":" << pendingChoices_
           << ",\"heat\":{\"overheats\":" << player.GetHeat().GetOverheatCount() << ",\"secondsAbove70\":" << heatHighSeconds_
           << ",\"railShots\":" << player.GetSpecialCombatStats().railShots << "},\"fps\":{\"min\":" << fpsMin << ",\"avg\":"
           << (fpsSamples_.empty() ? 0.0f : fpsSum / static_cast<float>(fpsSamples_.size())) << ",\"samples\":"
           << fpsSamples_.size() << "}}\n";
}

void SurvivorRunController::OnEnemyDefeated(const myEngine::Vector3& position)
{
    world_.combat.screenEffectDirector_.TriggerEnemyDefeat(world_.gameplayQueries->WorldToScreenUv(position), 1.0f);
}

void SurvivorRunController::AddExperience(int amount)
{
    if (amount <= 0) {
        return;
    }
    expCollected_ += amount;
    if (level_ >= config_.maxLevel) {
        return;
    }
    exp_ += amount;
    while (level_ < config_.maxLevel && exp_ >= config_.ExpToNextLevel(level_)) {
        exp_ -= config_.ExpToNextLevel(level_);
        ++level_;
        ++pendingChoices_;
        levelTimes_.push_back(elapsed_);
        pendingSince_.push_back(elapsed_);
        world_.combatFlow->SetEventCallout("レベル " + std::to_string(level_), 0.8f);
    }
    choiceHud_.SetPending(pendingChoices_, choiceOpen_);
    if (!formChosen_ && level_ >= config_.formLevel) {
        OpenFormChoice();
    } else if (config_.choiceAutoOpen && pendingChoices_ > 0 && !choiceOpen_) {
        OpenChoices();
    }
    if (level_ >= config_.maxLevel) {
        exp_ = 0;
    }
}

void SurvivorRunController::UpdateChoiceInput()
{
    auto& input = *world_.resources.input_;
    if (input.IsKeyTriggered(DIK_TAB)) {
        if (choiceOpen_) {
            CloseChoices();
        } else {
            OpenChoices();
        }
        return;
    }
    if (!choiceOpen_) {
        return;
    }
    // 1〜3キーは3択を開いている間だけ使う。
    for (size_t i = 0; i < offers_.size() && i < 3; ++i) {
        if (input.IsKeyTriggered(static_cast<uint8_t>(DIK_1 + i))) {
            ChooseOffer(i);
            return;
        }
    }
    const myEngine::Vector2 mouse = input.GetMousePosition();
    choiceHud_.Update(mouse);
    if (input.IsTrigger(input.GetMouseState().rgbButtons[0], input.GetPreMouseState().rgbButtons[0])) {
        const int hit = choiceHud_.HitTest(mouse);
        if (hit >= 0) {
            ChooseOffer(static_cast<size_t>(hit));
        }
    }
}

void SurvivorRunController::OpenChoices()
{
    if (pendingChoices_ <= 0 || choiceOpen_ || formChoiceOpen_) {
        return;
    }
    choiceOpen_ = true;
    DrawNextOffers();
}

void SurvivorRunController::DrawNextOffers()
{
    // 宝箱の選択が貯まっていれば、先にそれを出す。
    offersPremium_ = pendingPremium_ > 0;
    offers_ = survivor::DrawOffers(config_.parts, partLevels_, config_.choiceLightBias, random_, offersPremium_);
    choiceHud_.SetOffers(offers_, config_.parts, offersPremium_);
    choiceHud_.SetPending(pendingChoices_, true);
}

void SurvivorRunController::CloseChoices()
{
    choiceOpen_ = false;
    offers_.clear();
    choiceHud_.SetPending(pendingChoices_, false);
}

void SurvivorRunController::ChooseOffer(size_t index)
{
    if (!choiceOpen_ || index >= offers_.size() || pendingChoices_ <= 0) {
        return;
    }
    const survivor::PartOffer offer = offers_[index];
    auto& player = *world_.resources.player_;
    // 遊んだ記録：出た3枚・選んだ札・貯まってから選ぶまでの区画の秒・3択を開いていた秒。
    auto key = [&](const survivor::PartOffer& item) {
        return item.part == survivor::PartId::Count ? std::string("repair") : std::string(config_.parts[static_cast<size_t>(item.part)].key);
    };
    float waited = 0.0f;
    if (!pendingSince_.empty()) {
        waited = elapsed_ - pendingSince_.front();
        pendingSince_.erase(pendingSince_.begin());
    }
    choiceRecord_ << (choiceRecord_.tellp() > 0 ? "," : "") << "{\"t\":" << std::lround(elapsed_) << ",\"offered\":[";
    for (size_t i = 0; i < offers_.size(); ++i) {
        choiceRecord_ << (i ? "," : "") << "\"" << key(offers_[i]) << "\"";
    }
    choiceRecord_ << "],\"picked\":\"" << key(offer) << "\",\"premium\":" << (offersPremium_ ? "true" : "false")
                  << ",\"waited\":" << std::lround(waited) << ",\"open\":" << std::round(choiceOpenSeconds_ * 10.0f) / 10.0f << "}";
    choiceOpenSeconds_ = 0.0f;
    if (offer.part == survivor::PartId::Count) {
        player.HealRunPlayer(static_cast<int>(static_cast<float>(player.GetMaxHp()) * 0.3f));
        world_.combatFlow->SetEventCallout("修理キット", 0.7f);
    } else {
        const size_t part = static_cast<size_t>(offer.part);
        partLevels_[part] = (std::min)(partLevels_[part] + 1, config_.parts[part].maxLevel);
        ApplyParts();
        world_.combatFlow->SetEventCallout(std::string(config_.parts[part].name) + " Lv" + std::to_string(partLevels_[part]), 0.7f);
    }
    --pendingChoices_;
    ++choicesTaken_;
    if (offersPremium_ && pendingPremium_ > 0) {
        --pendingPremium_;
    }
    if (pendingChoices_ > 0) {
        // まだ貯まっていれば、続けて次の3択を出す。
        DrawNextOffers();
    } else {
        CloseChoices();
    }
}

void SurvivorRunController::OpenFormChoice()
{
    if (formChoiceOpen_ || formChosen_) {
        return;
    }
    if (choiceOpen_) {
        CloseChoices();
    }
    formChoiceOpen_ = true;
    std::vector<std::pair<std::string, std::string>> cards;
    for (size_t i = 0; i < survivor::kFormCount; ++i) {
        const auto& form = config_.forms[i];
        cards.emplace_back(std::to_string(i + 1) + "  " + form.name, survivor::DescribeForm(form, static_cast<survivor::FormId>(i)));
    }
    choiceHud_.SetCards("形態を選ぶ（Lv" + std::to_string(config_.formLevel) + "）", "1・2 キーかクリックで選ぶ　/　選ぶまで止まる",
                        cards);
    world_.combatFlow->SetEventCallout("形態の進化", 0.8f);
}

void SurvivorRunController::UpdateFormChoiceInput()
{
    auto& input = *world_.resources.input_;
    for (size_t i = 0; i < survivor::kFormCount; ++i) {
        if (input.IsKeyTriggered(static_cast<uint8_t>(DIK_1 + i))) {
            ChooseForm(i);
            return;
        }
    }
    const myEngine::Vector2 mouse = input.GetMousePosition();
    choiceHud_.Update(mouse);
    if (input.IsTrigger(input.GetMouseState().rgbButtons[0], input.GetPreMouseState().rgbButtons[0])) {
        const int hit = choiceHud_.HitTest(mouse);
        if (hit >= 0) {
            ChooseForm(static_cast<size_t>(hit));
        }
    }
}

void SurvivorRunController::ChooseForm(size_t index)
{
    if (!formChoiceOpen_ || index >= survivor::kFormCount) {
        return;
    }
    auto& player = *world_.resources.player_;
    const auto id = static_cast<survivor::FormId>(index);
    const auto& form = config_.forms[index];
    const PlayerClassConfig* source = player.FindCatalogClassConfig(form.sourceClass);
    if (!source) {
        source = player.GetActiveClassConfig();
    }
    if (source) {
        Player::SurvivorFormTuning tuning{};
        tuning.heatScale = form.heatScale;
        tuning.railPower = form.railPower;
        tuning.railHeatBase = form.railHeatBase;
        tuning.railHeatPerCharge = form.railHeatPerCharge;
        player.ApplySurvivorForm(survivor::BuildFormConfig(*source, form, id), tuning);
    }
    formKey_ = form.key;
    formChosen_ = true;
    formChoiceOpen_ = false;
    choiceHud_.SetPending(pendingChoices_, false);
    world_.combatFlow->SetEventCallout(std::string("形態：") + form.name, 1.0f);
}

void SurvivorRunController::ApplyParts()
{
    using survivor::PartId;
    auto& player = *world_.resources.player_;
    TankRunModifiers modifiers{};
    modifiers.enabled = true;
    modifiers.survivor = true;
    modifiers.partScales = survivor::MakePartScales(config_.parts, partLevels_);
    survivor::ApplyPartCards(config_.parts, partLevels_, modifiers);
    player.SetRunModifiers(modifiers);

    // 熱とシールドの部品。冷却器は冷える速さ（撃っている間も）、3段で上限+30。シールド容量は上限、3段で回復までの時間が半分。
    PlayerHeat::Config heat = config_.heat;
    const float cool = survivor::LightBonus(config_.parts, partLevels_, PartId::Cooler);
    heat.coolPerSecond *= cool;
    heat.coolPerSecondWhileFiring *= cool;
    if (survivor::HasChange(partLevels_, PartId::Cooler)) {
        heat.maxHeat += 30.0f;
    }
    player.SetHeatConfig(heat);
    Player::ShieldConfig shield{};
    shield.enabled = true;
    shield.maxHpRatio = config_.shieldMaxHpRatio * survivor::LightBonus(config_.parts, partLevels_, PartId::ShieldCapacity);
    shield.regenDelay = config_.shieldRegenDelay * (survivor::HasChange(partLevels_, PartId::ShieldCapacity) ? 0.5f : 1.0f);
    shield.regenPerSecond = config_.shieldRegenPerSecond;
    player.SetShieldConfig(shield);

    // 軽い部品の3段の変化と、重弾頭の熱。
    Player::SurvivorPartEffects effects{};
    effects.extraHeatPerShot = 2.0f * survivor::EffectPower(config_.parts, partLevels_, PartId::HeavyWarhead);
    effects.critChance = survivor::HasChange(partLevels_, PartId::Warhead) ? 0.15f : 0.0f;
    effects.loaderRamp = survivor::HasChange(partLevels_, PartId::Loader);
    effects.spikes = survivor::HasChange(partLevels_, PartId::Ram);
    effects.invincibleScale = survivor::HasChange(partLevels_, PartId::Armor) ? 1.8f : 1.0f;
    player.SetSurvivorPartEffects(effects);
}

void SurvivorRunController::UpdateSpikes()
{
    // 衝角の3段：トゲの生えた車体に触れた小さなシェイプ（四角・三角・ふつうの五角）は、その場で壊れる。
    auto& player = *world_.resources.player_;
    const myEngine::Vector3 position = player.GetWorldPosition();
    const float reach = player.GetRadius() + 0.35f;
    for (ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
        if (!enemy || enemy->IsDead() || enemy->IsRunResource() || enemy->GetSizeScale() > 1.01f) {
            continue;
        }
        const ExpEnemyType type = enemy->GetType();
        if (type != ExpEnemyType::Square && type != ExpEnemyType::Triangle && type != ExpEnemyType::Pentagon) {
            continue;
        }
        const myEngine::Vector3 offset = enemy->GetWorldPosition() - position;
        const float limit = reach + enemy->GetRadius();
        if (offset.x * offset.x + offset.y * offset.y <= limit * limit && enemy->TakeDamageFromPlayer(1000000u)) {
            ++spikeKills_;
        }
    }
}

SurvivorRunController::AliveCounts SurvivorRunController::CountAlive() const
{
    AliveCounts counts;
    for (const ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
        if (!enemy || enemy->IsDead()) {
            continue;
        }
        ++counts.total;
        if (enemy->GetType() == ExpEnemyType::Shooter || enemy->IsExpeditionCombatRole()) {
            ++counts.tanks;
        } else if (enemy->GetType() == ExpEnemyType::Pentagon && enemy->GetRadius() > 1.0f) {
            ++counts.bigPentagons;
        } else {
            ++counts.shapes;
        }
    }
    return counts;
}

void SurvivorRunController::UpdateSpawns(float dt)
{
    spawnBudget_ = (std::min)(spawnBudget_ + config_.maxSpawnsPerSecond * dt, config_.maxSpawnsPerSecond);
    const AliveCounts alive = CountAlive();

    // 三角の群れ：輪の1点から、まとまって自機へ寄ってくる。群れは予算と別に湧かせる。
    // 脅威の段階が上がるほど、群れは大きく、間隔は短くなる。
    const int extraStages = threatStage_ - 1;
    if (config_.swarmCount > 0 && elapsed_ >= nextSwarm_) {
        nextSwarm_ += config_.swarm.interval * std::pow(config_.threat.swarmIntervalScalePerStage, static_cast<float>(extraStages));
        const int swarmCount = config_.swarmCount + config_.threat.swarmCountPerStage * extraStages;
        if (const auto center = FindSpawnPosition(config_.ringMinDistance, config_.ringMaxDistance)) {
            std::uniform_real_distribution<float> offset(-config_.swarmSpread, config_.swarmSpread);
            for (int i = 0; i < swarmCount; ++i) {
                myEngine::Vector3 position = *center + myEngine::Vector3{offset(random_), offset(random_), 0.0f};
                position.x = (std::clamp)(position.x, kSpawnMinX, kSpawnMaxX);
                position.y = (std::clamp)(position.y, kSpawnMinY, kSpawnMaxY);
                if (!world_.resources.stage_->IsCollisionWithAnyBlock(position, 1.2f)) {
                    SpawnShape("Triangle", position, true);
                }
            }
        }
    }

    // 敵戦車：一定の間隔で、同時に maxAlive 体まで。種類は順番に回す。
    if (!config_.tankTypes.empty() && elapsed_ >= nextTank_ && spawnBudget_ >= 1.0f) {
        nextTank_ = elapsed_ + config_.tanks.interval;
        if (alive.tanks < config_.tanks.maxAlive + config_.threat.tankMaxAlivePerStage * extraStages) {
            if (const auto position = FindSpawnPosition(config_.ringMinDistance, config_.ringMaxDistance)) {
                const std::string& type = config_.tankTypes[nextTankType_ % config_.tankTypes.size()];
                if (world_.resources.enemyManager_->SpawnLevelEnemy(*position, type)) {
                    ScaleLastSpawnHp();
                    ++nextTankType_;
                    spawnBudget_ -= 1.0f;
                }
            }
        }
    }

    // 大五角：五角を大きく硬くしたもの。
    if (elapsed_ >= nextBigPentagon_ && spawnBudget_ >= 1.0f) {
        nextBigPentagon_ = elapsed_ + config_.bigPentagon.interval;
        if (alive.bigPentagons < config_.bigPentagon.maxAlive) {
            if (const auto position = FindSpawnPosition(config_.ringMinDistance, config_.ringMaxDistance)) {
                const int hp = static_cast<int>(std::lround(static_cast<float>(config_.bigPentagonHp) * EnemyHpScale()));
                if (world_.resources.enemyManager_->SpawnLevelEnemy(*position, "Pentagon", hp)) {
                    ExpEnemy* enemy = world_.resources.enemyManager_->GetEnemyPtrs().back();
                    enemy->SetSizeScale(config_.bigPentagonScale);
                    enemy->SetExpValue(static_cast<uint32_t>(config_.bigPentagonExp));
                    spawnBudget_ -= 1.0f;
                }
            }
        }
    }

    // 図形：生存数が目標に届くまで、予算の分だけ湧かせる。
    int missing = static_cast<int>(config_.ShapeTarget(elapsed_)) - alive.shapes;
    const auto& weights = config_.ShapeWeightsAt(elapsed_);
    std::discrete_distribution<int> pick(weights.begin(), weights.end());
    static constexpr std::array<const char*, 3> kShapes{"Square", "Triangle", "Pentagon"};
    while (missing > 0 && spawnBudget_ >= 1.0f) {
        const auto position = FindSpawnPosition(config_.ringMinDistance, config_.ringMaxDistance);
        if (!position) {
            break;
        }
        SpawnShape(kShapes[static_cast<size_t>(pick(random_))], *position, false);
        spawnBudget_ -= 1.0f;
        --missing;
    }
}

std::optional<myEngine::Vector3> SurvivorRunController::FindSpawnPosition(float minDistance, float maxDistance)
{
    // 自機の周りの輪から探す。壁際で輪が部屋の外に出るときは、画面の外でありさえすればよい。
    const myEngine::Vector3 player = world_.resources.player_->GetWorldPosition();
    const myEngine::Vector3 camera = world_.resources.camera->GetTranslate();
    const float halfHeight = arenacamera::HalfViewHeight() + 2.0f;
    const float halfWidth = arenacamera::HalfViewHeight() * arenacamera::kAspect + 2.0f;
    std::uniform_real_distribution<float> angle(0.0f, 6.2831853f);
    std::uniform_real_distribution<float> distance(minDistance, maxDistance);
    std::uniform_real_distribution<float> anyX(kSpawnMinX, kSpawnMaxX), anyY(kSpawnMinY, kSpawnMaxY);
    for (int attempt = 0; attempt < 24; ++attempt) {
        myEngine::Vector3 position{};
        if (attempt < 16) {
            const float a = angle(random_), d = distance(random_);
            position = {player.x + std::cos(a) * d, player.y + std::sin(a) * d, 0.0f};
            position.x = (std::clamp)(position.x, kSpawnMinX, kSpawnMaxX);
            position.y = (std::clamp)(position.y, kSpawnMinY, kSpawnMaxY);
        } else {
            position = {anyX(random_), anyY(random_), 0.0f};
        }
        const bool offScreen = std::abs(position.x - camera.x) > halfWidth || std::abs(position.y - camera.y) > halfHeight;
        if (offScreen && !world_.resources.stage_->IsCollisionWithAnyBlock(position, 1.2f)) {
            return position;
        }
    }
    return std::nullopt;
}

bool SurvivorRunController::SpawnShape(const char* prefab, const myEngine::Vector3& position, bool triangleChase)
{
    if (!world_.resources.enemyManager_->SpawnLevelEnemy(position, prefab)) {
        return false;
    }
    ScaleLastSpawnHp();
    if (triangleChase) {
        // 段階が上がるほど速く寄る。加速度は最高速度と同じ割合で上げる。
        const float maxSpeed = config_.swarmMaxSpeed + config_.threat.swarmSpeedPerStage * static_cast<float>(threatStage_ - 1);
        const float acceleration = config_.swarmMaxSpeed > 0.0f ? config_.swarmAcceleration * maxSpeed / config_.swarmMaxSpeed : 0.0f;
        world_.resources.enemyManager_->GetEnemyPtrs().back()->SetPlayerChase(acceleration, maxSpeed);
    }
    return true;
}

float SurvivorRunController::EnemyHpScale() const
{
    return (std::max)(0.1f, 1.0f + config_.threat.enemyHpPerStage * static_cast<float>(threatStage_ - 1));
}

void SurvivorRunController::ScaleLastSpawnHp()
{
    const float scale = EnemyHpScale();
    if (scale == 1.0f || world_.resources.enemyManager_->GetEnemyPtrs().empty()) {
        return;
    }
    ExpEnemy* enemy = world_.resources.enemyManager_->GetEnemyPtrs().back();
    enemy->SetHp(static_cast<int>(std::lround(static_cast<float>(enemy->GetMaxHp()) * scale)));
}

void SurvivorRunController::UpdateThreat()
{
    const int stage = config_.ThreatStage(elapsed_);
    if (stage != threatStage_) {
        threatStage_ = stage;
        std::string text = "脅威 Lv" + std::to_string(stage);
        if (stage == config_.threat.ringFromStage) {
            text += "　─　包囲が来る";
            nextRing_ = elapsed_ + 3.0f;
        }
        world_.combatFlow->SetEventCallout(text, 1.2f);
    }
    if (config_.threat.ringCount > 0 && threatStage_ >= config_.threat.ringFromStage && elapsed_ >= nextRing_) {
        nextRing_ = elapsed_ + config_.threat.ringInterval;
        SpawnRing();
    }
}

void SurvivorRunController::SpawnRing()
{
    // 輪の上の点のうち、部屋の中で壁と柱に重ならないところにだけ置く。一部が欠けても、残りで囲む。
    const myEngine::Vector3 center = world_.resources.player_->GetWorldPosition();
    const int count = config_.threat.ringCount;
    std::uniform_real_distribution<float> turn(0.0f, 6.2831853f);
    const float start = turn(random_);
    int placed = 0;
    for (int i = 0; i < count; ++i) {
        const float angle = start + 6.2831853f * static_cast<float>(i) / static_cast<float>(count);
        const myEngine::Vector3 position{center.x + std::cos(angle) * config_.threat.ringRadius,
                                         center.y + std::sin(angle) * config_.threat.ringRadius, 0.0f};
        if (position.x < kSpawnMinX || position.x > kSpawnMaxX || position.y < kSpawnMinY || position.y > kSpawnMaxY ||
            world_.resources.stage_->IsCollisionWithAnyBlock(position, 1.2f)) {
            continue;
        }
        if (SpawnShape("Triangle", position, true)) {
            ++placed;
        }
    }
    if (placed > 0) {
        ++ringsSpawned_;
        world_.combatFlow->SetEventCallout("包囲", 0.8f);
    }
}

bool SurvivorRunController::IsRivalActive() const
{
    return rivalSpawned_ && !rivalDefeated_;
}

void SurvivorRunController::SpawnRival()
{
    // 出口の候補のうち、自機から最も遠いところに出す。
    const myEngine::Vector3 player = world_.resources.player_->GetWorldPosition();
    float best = -1.0f;
    myEngine::Vector3 position = kExitCandidates.front();
    for (const auto& candidate : kExitCandidates) {
        const float distance = myEngine::Length(candidate - player);
        if (distance > best) {
            best = distance;
            position = candidate;
        }
    }
    rivalSpawned_ = true;
    rivalSpawnedAt_ = elapsed_;
    auto& rival = *world_.resources.enemy_;
    world_.run.tankExpeditionRivalActive_ = true;
    rival.ResetRunEncounter(position, (std::max)(1, config_.rival.hp), 1, true);
    rival.EnableExpeditionRival(true);
    auto progress = rival.GetEnemyProgressConfig();
    progress.levelingModeEnabled = false;
    rival.SetEnemyProgressConfig(progress);
    world_.combat.previousBossHp_ = rival.GetHp();
    world_.combat.bossDefeatHandled_ = false;
    world_.combat.screenEffectDirector_.TriggerBossEntry();
    world_.combatFlow->SetEventCallout("宿敵が現れた　─　倒すと出口が開く", 1.6f);
}

void SurvivorRunController::OnRivalDefeated()
{
    if (!IsRivalActive()) {
        return;
    }
    auto& rival = *world_.resources.enemy_;
    const myEngine::Vector3 position = rival.GetWorldPosition();
    rivalDefeated_ = true;
    rivalDefeatedAt_ = elapsed_;
    // 倒した宿敵は、遭遇戦を止めて描画と当たり判定から外す。
    rival.SetRunEncounterEnabled(false);
    world_.run.tankExpeditionRivalActive_ = false;
    pickups_.Drop(position, static_cast<uint32_t>((std::max)(0, config_.rival.exp)), random_);
    expDropped_ += (std::max)(0, config_.rival.exp);
    ++pendingChoices_;
    ++pendingPremium_;
    ++premiumTotal_;
    pendingSince_.push_back(elapsed_);
    choiceHud_.SetPending(pendingChoices_, choiceOpen_);
    world_.combatEffects->TriggerDeathPostPulse(position, 1.55f);
    world_.presentation.cameraShakeDuration_ = 0.5f;
    world_.presentation.cameraShakeTimer_ = world_.presentation.cameraShakeDuration_;
    world_.presentation.cameraShakePower_ = 0.9f;
    OpenExit();
    world_.combatFlow->SetEventCallout("宿敵を撃破　─　出口が開いた", 1.6f);
}

void SurvivorRunController::OpenExit()
{
    exitOpen_ = true;
    const myEngine::Vector3 player = world_.resources.player_->GetWorldPosition();
    float best = -1.0f;
    for (const auto& candidate : kExitCandidates) {
        const float distance = myEngine::Length(candidate - player);
        if (distance > best) {
            best = distance;
            exitPosition_ = candidate;
        }
    }
    world_.combatFlow->SetEventCallout("出口が開いた", 1.4f);
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
    choiceHud_.Draw(choiceOpen_ || formChoiceOpen_, formChoiceOpen_ ? 0 : pendingChoices_);
}

float SurvivorRunController::MeasureAimRoundTripError() const
{
    const myEngine::Vector3 target = world_.resources.player_->GetWorldPosition() + myEngine::Vector3{5.0f, 3.0f, 0.0f};
    const myEngine::Vector2 screen = world_.gameplayHud->WorldToScreen(target);
    const myEngine::Vector3 back =
        myEngine::ScreenToWorldOnZ0(screen, world_.resources.camera->GetViewMatrix(), world_.resources.camera->GetProjectionMatrix(),
                                    myEngine::WinApp::kClientWidth, myEngine::WinApp::kClientHeight);
    return myEngine::Length(myEngine::Vector3{back.x - target.x, back.y - target.y, 0.0f});
}

void SurvivorRunController::RequestCapture(const std::string& name)
{
    if (world_.run.tankRunCapturePath_.empty()) {
        world_.run.tankRunCapturePath_ = std::string(kAutoTestDirectory) + "/" + name + ".png";
    }
}

void SurvivorRunController::UpdateSectorAutoTest(float dt)
{
    // 部屋の中央の周りを回りながら近い敵を撃ち続け、近くの欠片は拾いに行く。4:00 を過ぎたら出口へ向かう
    // （レベルの曲線を4分で確かめるため、出口が開いてもすぐには入らない）。自機は無敵。
    auto& player = *world_.resources.player_;
    autoTestTime_ += dt;
    const myEngine::Vector3 position = player.GetWorldPosition();
    myEngine::Vector3 aim = position + myEngine::Vector3{6.0f, 0.0f, 0.0f};
    if (const ExpEnemy* nearest = world_.resources.enemyManager_->FindNearestEnemy(position, 30.0f)) {
        aim = nearest->GetWorldPosition();
    }
    // 宿敵がいて近ければ、宿敵を撃つ。
    if (IsRivalActive()) {
        const myEngine::Vector3 rival = world_.resources.enemy_->GetWorldPosition();
        if (myEngine::Length(myEngine::Vector3{rival.x - position.x, rival.y - position.y, 0.0f}) < 30.0f) {
            aim = rival;
        }
    }
    myEngine::Vector3 goal{};
    myEngine::Vector3 shard{};
    if (exitOpen_ && elapsed_ >= kSectorAutoTestExitSeconds) {
        goal = exitPosition_;
    } else if (IsRivalActive()) {
        // 宿敵がいる間は、宿敵から8ほど離れた位置へ寄る。
        const myEngine::Vector3 rival = world_.resources.enemy_->GetWorldPosition();
        myEngine::Vector3 away{position.x - rival.x, position.y - rival.y, 0.0f};
        const float distance = myEngine::Length(away);
        goal = distance > 0.1f ? rival + away * (8.0f / distance) : rival;
    } else if (pickups_.FindNearest(position, 12.0f, shard)) {
        goal = shard;
    } else {
        const float angle = autoTestTime_ * 0.35f;
        goal = {44.0f + std::cos(angle) * 18.0f, 28.0f + std::sin(angle) * 11.0f, 0.0f};
    }
    myEngine::Vector3 move{goal.x - position.x, goal.y - position.y, 0.0f};
    const float length = myEngine::Length(move);
    move = length > 0.5f ? move / length : myEngine::Vector3{};
    // トゲ：5秒で一度、自機のすぐ横に四角を置き、すぐにトゲの判定をして壊れるかを見る
    // （同じフレームの体当たりや弾で壊れる前に確かめる）。
    if (player.HasSpikes() && autoSpikeProbe_ == 0 && autoTestTime_ >= 5.0f) {
        const myEngine::Vector3 beside = position + myEngine::Vector3{0.0f, -(player.GetRadius() + 0.5f), 0.0f};
        const int before = spikeKills_;
        if (SpawnShape("Square", beside, false)) {
            UpdateSpikes();
            autoSpikeProbe_ = spikeKills_ > before ? 2 : 3;
        } else {
            autoSpikeProbe_ = -1;
        }
    }
    // 3秒ごとに1回ダッシュする（衝撃放電・推進器の確認）。
    const bool dash = static_cast<int>(autoTestTime_ / 3.0f) != static_cast<int>((autoTestTime_ - dt) / 3.0f);
    player.SetDemoInput(true, {move.x, move.y}, aim, true, dash);
    autoMaxHeat_ = (std::max)(autoMaxHeat_, player.GetHeat().GetHeat());

    // 形態：2択が開いたら0.3秒（実時間）で画面を保存し、0.6秒で選ぶ。選んで2秒後に、変わった自機の画面を保存する。
    if (formChoiceOpen_) {
        autoFormTimer_ += dt;
        if (!autoFormCaptured_ && autoFormTimer_ > 0.3f && world_.run.tankRunCapturePath_.empty()) {
            RequestCapture("form_open");
            autoFormCaptured_ = true;
        }
        if (autoFormCaptured_ && autoFormTimer_ > 0.6f) {
            ChooseForm(autoFormIndex_);
            autoFormChosenAt_ = elapsed_;
            autoFormCaptureAt_ = autoTestTime_ + 2.0f;
        }
        return;
    }
    if (autoFormCaptureAt_ > 0.0f && autoTestTime_ >= autoFormCaptureAt_ && world_.run.tankRunCapturePath_.empty()) {
        RequestCapture("form_" + formKey_);
        autoFormCaptureAt_ = -1.0f;
    }
    // 長砲身：5秒ごとに0.7秒、右クリックを押してため撃ちする。
    player.SetDemoSpecialHold(player.HasRailChargeForm() && std::fmod(autoTestTime_, 5.0f) < 0.7f);

    // シールド：8秒で一度だけ無敵を外して2回当て、先にシールドが減ること、3秒は戻らず、その後に満タンへ戻ることを見る。
    // 無敵の間の被弾は TakeDamage の入口で弾かれるので、回復の待ち時間は動かない。
    constexpr std::array<float, 4> kShieldCheckSeconds{8.0f, 10.5f, 12.0f, 16.0f};
    if (autoShieldStep_ < static_cast<int>(kShieldCheckSeconds.size()) &&
        autoTestTime_ >= kShieldCheckSeconds[static_cast<size_t>(autoShieldStep_)]) {
        auto record = [&](const char* label) {
            autoShieldLog_ << (autoShieldLog_.tellp() > 0 ? "," : "") << "{\"at\":\"" << label << "\",\"t\":" << autoTestTime_
                           << ",\"shield\":" << player.GetShield() << ",\"shieldMax\":" << player.GetShieldMax()
                           << ",\"hp\":" << player.GetHp() << "}";
        };
        if (autoShieldStep_ == 0) {
            record("before");
            player.SetDebugNoDamage(false);
            player.TakeDamage(10, 0.0f);
            record("hit10");
            player.TakeDamage(40, 0.0f);
            record("hit40");
            player.SetDebugNoDamage(true);
        } else {
            record(autoShieldStep_ == 1 ? "wait2.5" : autoShieldStep_ == 2 ? "wait4" : "wait8");
        }
        ++autoShieldStep_;
    }

    // 3択：選択が2つ以上貯まるか、1つを3秒持ったら開き、0.6秒（実時間）見てから札を順番に選ぶ。
    if (choiceOpen_) {
        autoMinTimeScale_ = (std::min)(autoMinTimeScale_, world_.combat.finalDeltaTime / myEngine::kFixedStepSeconds);
        autoChoiceTimer_ += dt;
        if (!autoChoiceCaptured_ && autoChoiceTimer_ > 0.3f) {
            RequestCapture("choice_open");
            autoChoiceCaptured_ = true;
        }
        if (autoChoiceTimer_ > 0.6f && !offers_.empty()) {
            const size_t index = autoPick_++ % offers_.size();
            const survivor::PartOffer offer = offers_[index];
            const int maxHpBefore = player.GetMaxHp();
            ChooseOffer(index);
            if (autoChoiceLog_.tellp() > 0) {
                autoChoiceLog_ << ",";
            }
            autoChoiceLog_ << "{\"t\":" << static_cast<int>(elapsed_) << ",\"part\":\""
                           << (offer.part == survivor::PartId::Count ? "repair" : config_.parts[static_cast<size_t>(offer.part)].key)
                           << "\",\"level\":" << offer.nextLevel << ",\"maxHp\":[" << maxHpBefore << "," << player.GetMaxHp() << "]}";
            autoChoiceTimer_ = 0.0f;
        }
    } else if (pendingChoices_ > 0) {
        autoChoiceTimer_ += dt;
        if (pendingChoices_ >= 2 || autoChoiceTimer_ > 3.0f) {
            OpenChoices();
            autoChoiceTimer_ = 0.0f;
        }
    }

    // 包囲の輪は最初の輪の1秒後、宿敵は現れて10秒後の画面を保存する。
    if (ringsSpawned_ == 1 && !autoRingCaptured_ && elapsed_ >= nextRing_ - config_.threat.ringInterval + 1.0f &&
        world_.run.tankRunCapturePath_.empty()) {
        RequestCapture("ring");
        autoRingCaptured_ = true;
    }
    if (IsRivalActive() && !autoRivalCaptured_ && elapsed_ >= rivalSpawnedAt_ + 10.0f && world_.run.tankRunCapturePath_.empty()) {
        RequestCapture("rival");
        autoRivalCaptured_ = true;
    }
    if (autoTestNextCapture_ < static_cast<int>(kSectorCaptureSeconds.size()) &&
        elapsed_ >= kSectorCaptureSeconds[static_cast<size_t>(autoTestNextCapture_)]) {
        RequestCapture("sector_" + std::to_string(static_cast<int>(kSectorCaptureSeconds[static_cast<size_t>(autoTestNextCapture_)])));
        ++autoTestNextCapture_;
    }

    const AliveCounts alive = CountAlive();
    if (elapsed_ >= 120.0f) {
        maxAliveAfterTwoMinutes_ = (std::max)(maxAliveAfterTwoMinutes_, alive.total);
    }
    if (autoTestTime_ >= autoTestNextSample_) {
        autoTestNextSample_ += 10.0f;
        const auto now = std::chrono::steady_clock::now();
        const double seconds = std::chrono::duration<double>(now - sampleStart_).count();
        const double fps = seconds > 0.0 ? static_cast<double>(drawnFrames_ - sampleFrames_) / seconds : 0.0;
        sampleStart_ = now;
        sampleFrames_ = drawnFrames_;
        if (autoTestLog_.tellp() > 0) {
            autoTestLog_ << ",";
        }
        autoTestLog_ << "{\"t\":" << static_cast<int>(elapsed_) << ",\"shapes\":" << alive.shapes << ",\"tanks\":" << alive.tanks
                     << ",\"big\":" << alive.bigPentagons << ",\"total\":" << alive.total << ",\"fps\":" << std::fixed
                     << std::setprecision(1) << fps << std::defaultfloat << ",\"hp\":" << player.GetHp() << ",\"level\":" << level_
                     << ",\"collected\":" << expCollected_ << ",\"dropped\":" << expDropped_ << ",\"shards\":" << pickups_.GetCount()
                     << ",\"shots\":" << player.GetPrimaryAttackCount() - autoSampleShots_ << ",\"heat\":" << player.GetHeat().GetHeat()
                     << ",\"overheats\":" << player.GetHeat().GetOverheatCount() << ",\"threat\":" << threatStage_
                     << ",\"rivalHp\":" << (IsRivalActive() ? world_.resources.enemy_->GetHp() : -1) << "}";
        autoSampleShots_ = player.GetPrimaryAttackCount();
    }

    if (cleared_ && autoTestClearedTime_ < 0.0f) {
        autoTestClearedTime_ = autoTestTime_;
    }
    const bool timedOut = autoTestTime_ > kSectorAutoTestExitSeconds + 90.0f;
    if ((autoTestClearedTime_ >= 0.0f && autoTestTime_ - autoTestClearedTime_ > 1.5f) || timedOut) {
        if (!autoTestCaptureQueued_) {
            RequestCapture(cleared_ ? "cleared" : "timeout");
            autoTestCaptureQueued_ = true;
            return;
        }
        if (!world_.run.tankRunCapturePath_.empty()) {
            return;
        }
        // 調整パネルの保存：今の設定を別のファイルへ書いて読み戻し、主な値が同じになるかを見る。
        bool roundTrip = false;
        {
            const std::string path = std::string(kAutoTestDirectory) + "/config_roundtrip.json";
            std::error_code ignored;
            std::filesystem::copy_file(kConfigPath, path, std::filesystem::copy_options::overwrite_existing, ignored);
            std::string error;
            survivor::SpawnConfig loaded;
            if (survivor::SaveSpawnConfig(path, config_, error) && survivor::LoadSpawnConfig(path, loaded, error)) {
                roundTrip = loaded.ExpToNextLevel(10) == config_.ExpToNextLevel(10) && loaded.heat.maxHeat == config_.heat.maxHeat &&
                            loaded.heat.coolPerSecondWhileFiring == config_.heat.coolPerSecondWhileFiring &&
                            loaded.shapeTargetAlive == config_.shapeTargetAlive && loaded.choiceSlowTimeScale == config_.choiceSlowTimeScale &&
                            loaded.parts[0].perLevel == config_.parts[0].perLevel && loaded.forms[1].damageScale == config_.forms[1].damageScale &&
                            loaded.tankTypes == config_.tankTypes;
            }
        }
        std::ofstream(std::string(kAutoTestDirectory) + "/autotest.json")
            << "{\"configRoundTrip\":" << (roundTrip ? "true" : "false") << ",\"completed\":" << (cleared_ ? "true" : "false") << ",\"cleared\":" << (cleared_ ? "true" : "false")
            << ",\"sectorSeconds\":" << elapsed_ << ",\"exitOpen\":" << (exitOpen_ ? "true" : "false") << ",\"exit\":[" << exitPosition_.x
            << "," << exitPosition_.y << "],\"maxAliveAfterTwoMinutes\":" << maxAliveAfterTwoMinutes_
            << ",\"resultState\":" << static_cast<int>(world_.combat.combatFlow_.GetState()) << ",\"level\":" << level_
            << ",\"pendingChoices\":" << pendingChoices_ << ",\"choicesTaken\":" << choicesTaken_
            << ",\"minTimeScaleWhileOpen\":" << autoMinTimeScale_ << ",\"partLevels\":{" <<
            [&] {
                std::ostringstream levels;
                for (size_t i = 0; i < survivor::kPartCount; ++i) {
                    levels << (i ? "," : "") << "\"" << config_.parts[i].key << "\":" << partLevels_[i];
                }
                return levels.str();
            }()
            << "},\"maxHp\":" << player.GetMaxHp() << ",\"choices\":[" << autoChoiceLog_.str() << "]"
            << ",\"expCollected\":" << expCollected_ << ",\"expDropped\":" << expDropped_ << ",\"maxHeat\":" << autoMaxHeat_
            << ",\"overheats\":" << player.GetHeat().GetOverheatCount() << ",\"form\":\"" << formKey_
            << "\",\"formChosenAt\":" << autoFormChosenAt_ << ",\"formClass\":\""
            << (player.GetActiveClassConfig() ? player.GetActiveClassConfig()->id : std::string("none"))
            << "\",\"railShots\":" << player.GetSpecialCombatStats().railShots << ",\"critShots\":" << player.GetCritShots()
            << ",\"spikeKills\":" << spikeKills_ << ",\"spikeProbe\":" << autoSpikeProbe_ << ",\"premiumChests\":" << premiumTotal_
            << ",\"dashExplosions\":" << player.GetDashExplosionCount() << ",\"maxStamina\":" << player.GetStats().maxStamina
            << ",\"heatMax\":" << player.GetHeat().GetMaxHeat() << ",\"shieldMax\":" << player.GetShieldMax() << ",\"shieldCheck\":[" << autoShieldLog_.str() << "]"
            << ",\"threatStage\":" << threatStage_ << ",\"rings\":" << ringsSpawned_ << ",\"rivalSpawnedAt\":" << rivalSpawnedAt_
            << ",\"rivalDefeatedAt\":" << rivalDefeatedAt_
            << ",\"samples\":[" << autoTestLog_.str() << "]}\n";
        autoTest_ = AutoTest::None;
        PostQuitMessage(0);
    }
}

void SurvivorRunController::UpdateTourAutoTest(float dt)
{
    // 各地点へまっすぐ進み、着いたら（または10秒で）止まってカメラが落ち着くのを待ち、画面を保存する。
    auto& player = *world_.resources.player_;
    autoTestTime_ += dt;
    autoTestLegTime_ += dt;
    if (autoTourForm_ && !formChosen_) {
        formChoiceOpen_ = true;
        ChooseForm(autoFormIndex_);
    }
    const myEngine::Vector3 position = player.GetWorldPosition();
    const myEngine::Vector3 aim = position + myEngine::Vector3{6.0f, 0.0f, 0.0f};

    if (autoTestCaptureQueued_) {
        // 保存の要求は描画で写され、次の更新の初めに書き出されて空に戻る。
        if (!world_.run.tankRunCapturePath_.empty()) {
            return;
        }
        autoTestCaptureQueued_ = false;
        autoTestSettling_ = false;
        autoTestLegTime_ = 0.0f;
        ++autoTestWaypoint_;
    }

    if (autoTestWaypoint_ >= kTourWaypoints.size()) {
        float minX = 0, minY = 0, maxX = 0, maxY = 0;
        arenacamera::ArenaBounds(world_.resources.stage_->GetBlocks(), minX, minY, maxX, maxY);
        std::ofstream(std::string(kAutoTestDirectory) + "/autotest.json")
            << "{\"completed\":true,\"arena\":[" << minX << "," << minY << "," << maxX << "," << maxY << "],\"halfView\":["
            << arenacamera::HalfViewHeight() * arenacamera::kAspect << "," << arenacamera::HalfViewHeight() << "],\"waypoints\":["
            << autoTestLog_.str() << "],\"enemies\":" << world_.resources.enemyManager_->GetEnemyCount()
            << ",\"rivalActive\":" << (world_.gameplayQueries->IsRunRivalActive() ? "true" : "false")
            << ",\"menuOpen\":" << (world_.arenaRunController->IsTankRunMenuOpen() ? "true" : "false")
            << ",\"playerDead\":" << (player.IsDead() ? "true" : "false") << ",\"seconds\":" << autoTestTime_ << "}\n";
        autoTest_ = AutoTest::None;
        PostQuitMessage(0);
        return;
    }

    const AutoTestWaypoint& waypoint = kTourWaypoints[autoTestWaypoint_];
    const myEngine::Vector3 offset{waypoint.target.x - position.x, waypoint.target.y - position.y, 0.0f};
    const float distance = myEngine::Length(offset);
    if (!autoTestSettling_ && (distance < 1.5f || autoTestLegTime_ > 10.0f)) {
        autoTestSettling_ = true;
        autoTestLegTime_ = 0.0f;
    }
    if (!autoTestSettling_) {
        player.SetDemoInput(true, {offset.x / distance, offset.y / distance}, aim, false, false);
        return;
    }
    // 止まっている間は撃ち、主砲のまわりの熱の輪が画面に写るようにする。
    player.SetDemoInput(true, {0.0f, 0.0f}, aim, true, false);
    if (autoTestLegTime_ < 1.2f) {
        return;
    }
    const myEngine::Vector3 camera = world_.resources.camera->GetTranslate();
    if (autoTestWaypoint_ > 0) {
        autoTestLog_ << ",";
    }
    autoTestLog_ << "{\"name\":\"" << waypoint.name << "\",\"target\":[" << waypoint.target.x << "," << waypoint.target.y
                 << "],\"player\":[" << position.x << "," << position.y << "],\"camera\":[" << camera.x << "," << camera.y
                 << "],\"reached\":" << (distance < kAutoTestReachDistance ? "true" : "false")
                 << ",\"aimError\":" << MeasureAimRoundTripError() << ",\"heat\":" << player.GetHeat().GetHeat() << "}";
    RequestCapture(waypoint.name);
    autoTestCaptureQueued_ = true;
}

void SurvivorRunController::UpdatePerfAutoTest(float dt)
{
    // 0〜15秒：敵を200体まで湧かせる（自機は中央で止まり、撃たない）。
    // 15〜35秒：散らばった場面を測る（敵はほぼ画面の外）。35秒から：画面の外の図形を2秒ごとに画面の中へ移し、
    // 自機は撃ち始める。45〜65秒：画面が敵で埋まった場面を測る。
    constexpr float kSpreadFrom = 15.0f, kSpreadTo = 35.0f, kCrowdFrom = 45.0f, kCrowdTo = 65.0f;
    auto& player = *world_.resources.player_;
    autoTestTime_ += dt;
    const myEngine::Vector3 position = player.GetWorldPosition();
    myEngine::Vector3 aim = position + myEngine::Vector3{6.0f, 0.0f, 0.0f};
    if (const ExpEnemy* nearest = world_.resources.enemyManager_->FindNearestEnemy(position, 30.0f)) {
        aim = nearest->GetWorldPosition();
    }
    player.SetDemoInput(true, {0.0f, 0.0f}, aim, perfCrowded_, false);
    if (autoTestTime_ >= kSpreadTo && (!perfCrowded_ || std::fmod(autoTestTime_ - kSpreadTo, 2.0f) < dt)) {
        perfCrowded_ = true;
        const myEngine::Vector3 camera = world_.resources.camera->GetTranslate();
        const float halfHeight = arenacamera::HalfViewHeight() - 1.5f;
        const float halfWidth = arenacamera::HalfViewHeight() * arenacamera::kAspect - 1.5f;
        std::uniform_real_distribution<float> x(camera.x - halfWidth, camera.x + halfWidth), y(camera.y - halfHeight, camera.y + halfHeight);
        for (ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
            if (!enemy || enemy->IsDead() || enemy->IsExpeditionCombatRole() || enemy->GetType() == ExpEnemyType::Shooter) {
                continue;
            }
            const myEngine::Vector3 at = enemy->GetWorldPosition();
            if (std::abs(at.x - camera.x) <= halfWidth && std::abs(at.y - camera.y) <= halfHeight) {
                continue;
            }
            for (int attempt = 0; attempt < 8; ++attempt) {
                const myEngine::Vector3 target{x(random_), y(random_), at.z};
                const float dx = target.x - position.x, dy = target.y - position.y;
                if (dx * dx + dy * dy > 16.0f && !world_.resources.stage_->IsCollisionWithAnyBlock(target, 1.2f)) {
                    enemy->SetWorldPosition(target);
                    break;
                }
            }
        }
    }
    if (autoTestTime_ >= 30.0f && autoTestNextCapture_ == 0 && world_.run.tankRunCapturePath_.empty()) {
        RequestCapture("perf_spread");
        ++autoTestNextCapture_;
    }
    if (autoTestTime_ >= 60.0f && autoTestNextCapture_ == 1 && world_.run.tankRunCapturePath_.empty()) {
        RequestCapture("perf_crowd");
        ++autoTestNextCapture_;
    }

    // 1秒ごとに、その1秒の fps・敵の数・画面の中の敵の数を記録する。
    if (autoTestTime_ >= perfNextSample_) {
        perfNextSample_ += 1.0f;
        const auto now = std::chrono::steady_clock::now();
        const double seconds = std::chrono::duration<double>(now - sampleStart_).count();
        const double fps = seconds > 0.0 ? static_cast<double>(drawnFrames_ - sampleFrames_) / seconds : 0.0;
        sampleStart_ = now;
        sampleFrames_ = drawnFrames_;
        const myEngine::Vector3 camera = world_.resources.camera->GetTranslate();
        const float halfHeight = arenacamera::HalfViewHeight();
        const float halfWidth = halfHeight * arenacamera::kAspect;
        int alive = 0, onScreen = 0;
        for (const ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
            if (!enemy || enemy->IsDead()) {
                continue;
            }
            ++alive;
            const myEngine::Vector3 at = enemy->GetWorldPosition();
            if (std::abs(at.x - camera.x) <= halfWidth && std::abs(at.y - camera.y) <= halfHeight) {
                ++onScreen;
            }
        }
        const float t = autoTestTime_ - 1.0f;
        const int phase = t >= kSpreadFrom && t < kSpreadTo ? 0 : t >= kCrowdFrom && t < kCrowdTo ? 1 : -1;
        if (phase >= 0) {
            PerfPhase& sum = perfPhases_[static_cast<size_t>(phase)];
            ++sum.samples;
            sum.fpsSum += fps;
            sum.fpsMin = (std::min)(sum.fpsMin, fps);
            sum.aliveSum += alive;
            sum.onScreenSum += onScreen;
        }
        perfLog_ << (perfLog_.tellp() > 0 ? "," : "") << "{\"t\":" << static_cast<int>(t) << ",\"fps\":" << std::fixed << std::setprecision(1)
                 << fps << std::defaultfloat << ",\"alive\":" << alive << ",\"onScreen\":" << onScreen
                 << ",\"bullets\":" << world_.resources.bulletManager_->GetBulletCount() << "}";
    }

    if (autoTestTime_ < kCrowdTo + 0.5f || !world_.run.tankRunCapturePath_.empty()) {
        return;
    }
    auto phaseJson = [](const char* name, const PerfPhase& sum) {
        std::ostringstream text;
        const double n = (std::max)(1, sum.samples);
        text << "{\"name\":\"" << name << "\",\"samples\":" << sum.samples << ",\"fpsAvg\":" << std::fixed << std::setprecision(1)
             << sum.fpsSum / n << ",\"fpsMin\":" << (sum.samples > 0 ? sum.fpsMin : 0.0) << ",\"aliveAvg\":" << sum.aliveSum / n
             << ",\"onScreenAvg\":" << sum.onScreenSum / n << "}";
        return text.str();
    };
    std::ofstream(std::string(kAutoTestDirectory) + "/perf.json")
        << "{\"completed\":true,\"phases\":[" << phaseJson("spread", perfPhases_[0]) << "," << phaseJson("crowd", perfPhases_[1])
        << "],\"samples\":[" << perfLog_.str() << "]}\n";
    autoTest_ = AutoTest::None;
    PostQuitMessage(0);
}

} // namespace gameplay
