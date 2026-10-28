#include "game/run/session/SurvivorRunController.h"
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
    world_.resources.camera->SetTranslate({kPlayerStart.x, kPlayerStart.y, world_.resources.camera->GetTranslate().z});
    world_.resources.camera->Update();

    // 自機が倒した敵は、経験値を直接渡さずに欠片を落とす。コールバックは衝突処理の途中から呼ばれるので、欠片の列に足すだけにする。
    SurvivorPickups::Config pickupConfig;
    pickupConfig.magnetRadius = config_.magnetRadius;
    pickupConfig.collectRadius = config_.collectRadius;
    pickupConfig.valuePerShard = config_.valuePerShard;
    pickupConfig.maxShards = static_cast<size_t>((std::max)(1, config_.maxShards));
    pickups_.SetConfig(pickupConfig);
    ExpEnemy::SetPlayerExpDropCallback([this](const myEngine::Vector3& position, uint32_t value) {
        pickups_.Drop(position, value, random_);
        expDropped_ += static_cast<int>(value);
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
    RefreshHud();

    wchar_t automatic[8]{};
    const DWORD length = GetEnvironmentVariableW(L"MYENGINE_SURVIVOR_AUTOTEST", automatic, 8);
    if (!world_.demo.titleDemo_ && length > 0) {
        autoTest_ = std::wstring(automatic) == L"tour" ? AutoTest::Tour : automatic[0] == L'1' ? AutoTest::Sector : AutoTest::None;
    }
    random_.seed(autoTest_ != AutoTest::None ? 20261028u : static_cast<uint32_t>(GetTickCount64()));
    if (autoTest_ != AutoTest::None) {
        world_.presentation.debugPlayerNoDamage_ = true;
        world_.resources.player_->SetDebugNoDamage(true);
        std::filesystem::create_directories(kAutoTestDirectory);
        sampleStart_ = std::chrono::steady_clock::now();
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
    if (world_.combat.combatFlow_.GetState() == GameFlowState::Playing && !cleared_) {
        UpdateChoiceInput();
    } else if (choiceOpen_) {
        CloseChoices();
    }
    // 区画の時計・湧き方・欠片は、3択を開いている間のスローに合わせて遅くする。
    const float gameDt = dt * GetTimeScale();
    if (world_.combat.combatFlow_.GetState() == GameFlowState::Playing && !cleared_) {
        elapsed_ += gameDt;
        UpdateSpawns(gameDt);
        AddExperience(pickups_.Update(gameDt, world_.resources.player_->GetWorldPosition(),
                                      survivor::CollectorMagnetScale(config_.parts, partLevels_)));
        if (!exitOpen_ && elapsed_ >= config_.exitOpenSeconds) {
            OpenExit();
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
    if (autoTest_ == AutoTest::Sector) {
        UpdateSectorAutoTest(dt);
    }
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
        world_.combatFlow->SetEventCallout("レベル " + std::to_string(level_), 0.8f);
    }
    choiceHud_.SetPending(pendingChoices_, choiceOpen_);
    if (config_.choiceAutoOpen && pendingChoices_ > 0 && !choiceOpen_) {
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
    if (pendingChoices_ <= 0 || choiceOpen_) {
        return;
    }
    choiceOpen_ = true;
    offers_ = survivor::DrawOffers(config_.parts, partLevels_, config_.choiceLightBias, random_);
    choiceHud_.SetOffers(offers_, config_.parts);
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
    if (pendingChoices_ > 0) {
        // まだ貯まっていれば、続けて次の3択を出す。
        offers_ = survivor::DrawOffers(config_.parts, partLevels_, config_.choiceLightBias, random_);
        choiceHud_.SetOffers(offers_, config_.parts);
        choiceHud_.SetPending(pendingChoices_, true);
    } else {
        CloseChoices();
    }
}

void SurvivorRunController::ApplyParts()
{
    TankRunModifiers modifiers{};
    modifiers.enabled = true;
    modifiers.partScales = survivor::MakePartScales(config_.parts, partLevels_);
    world_.resources.player_->SetRunModifiers(modifiers);
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
    if (config_.swarmCount > 0 && elapsed_ >= nextSwarm_) {
        nextSwarm_ += config_.swarm.interval;
        if (const auto center = FindSpawnPosition(config_.ringMinDistance, config_.ringMaxDistance)) {
            std::uniform_real_distribution<float> offset(-config_.swarmSpread, config_.swarmSpread);
            for (int i = 0; i < config_.swarmCount; ++i) {
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
        if (alive.tanks < config_.tanks.maxAlive) {
            if (const auto position = FindSpawnPosition(config_.ringMinDistance, config_.ringMaxDistance)) {
                const std::string& type = config_.tankTypes[nextTankType_ % config_.tankTypes.size()];
                if (world_.resources.enemyManager_->SpawnLevelEnemy(*position, type)) {
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
                if (world_.resources.enemyManager_->SpawnLevelEnemy(*position, "Pentagon", config_.bigPentagonHp)) {
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
    if (triangleChase) {
        world_.resources.enemyManager_->GetEnemyPtrs().back()->SetPlayerChase(config_.swarmAcceleration, config_.swarmMaxSpeed);
    }
    return true;
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
    text << "Lv " << level_ << "   区画  " << Clock(elapsed_);
    if (!exitOpen_) {
        text << "   出口まで " << Clock(config_.exitOpenSeconds - elapsed_);
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
    choiceHud_.Draw(choiceOpen_, pendingChoices_);
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
    myEngine::Vector3 goal{};
    myEngine::Vector3 shard{};
    if (exitOpen_ && elapsed_ >= kSectorAutoTestExitSeconds) {
        goal = exitPosition_;
    } else if (pickups_.FindNearest(position, 12.0f, shard)) {
        goal = shard;
    } else {
        const float angle = autoTestTime_ * 0.35f;
        goal = {44.0f + std::cos(angle) * 18.0f, 28.0f + std::sin(angle) * 11.0f, 0.0f};
    }
    myEngine::Vector3 move{goal.x - position.x, goal.y - position.y, 0.0f};
    const float length = myEngine::Length(move);
    move = length > 0.5f ? move / length : myEngine::Vector3{};
    player.SetDemoInput(true, {move.x, move.y}, aim, true, false);
    autoMaxHeat_ = (std::max)(autoMaxHeat_, player.GetHeat().GetHeat());

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
                     << ",\"overheats\":" << player.GetHeat().GetOverheatCount() << "}";
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
        std::ofstream(std::string(kAutoTestDirectory) + "/autotest.json")
            << "{\"completed\":" << (cleared_ ? "true" : "false") << ",\"cleared\":" << (cleared_ ? "true" : "false")
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
            << ",\"overheats\":" << player.GetHeat().GetOverheatCount() << ",\"shieldCheck\":[" << autoShieldLog_.str() << "]"
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

} // namespace gameplay
