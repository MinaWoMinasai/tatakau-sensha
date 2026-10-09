#include "game/run/session/ExpeditionMapController.h"
#include "game/session/GameplaySystems.h"
#include "game/weapon/CombatTypes.h"
#include "StartupTrace.h"
#include <fstream>
#include <iomanip>
#include <numeric>
#include <optional>
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
#include "externals/imgui/imgui.h"
#endif

namespace gameplay {
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
#endif

namespace {
using NK = tankexp::NodeKind;
std::optional<tankexp::MapDefinition> sessionMap;
std::optional<tankexp::RoomCatalog> sessionRooms;
std::optional<tankcontent::Catalog> sessionContent;
/// @brief 遠征マップの矩形表示用スプライトを生成する。
std::unique_ptr<cg2::Sprite> MapRect(cg2::Vector2 p, cg2::Vector2 size, const cg2::Vector4& color)
{
    auto s = std::make_unique<cg2::Sprite>();
    s->Initialize(cg2::SpriteCommon::GetInstance(), "resources/white512x512.png");
    s->SetPosition(p);
    s->SetSize(size);
    s->SetColor(color);
    s->Update();
    return s;
}
/// @brief 遠征マップの表示用文字を生成する。
std::unique_ptr<cg2::TextLabel> MapLabel(float size, cg2::Vector2 p, const cg2::Vector4& color)
{
    cg2::TextStyle style{};
    style.fontFamily = "Meiryo";
    style.fontSize = size;
    style.color = color;
    style.padding = 4;
    style.outlineThickness = 0;
    auto t = std::make_unique<cg2::TextLabel>();
    t->Initialize(cg2::SpriteCommon::GetInstance(), " ", style);
    t->SetPosition(p);
    return t;
}
/// @brief ノードの種類と状態に対応する色を返す。
cg2::Vector4 NodeColor(NK kind)
{
    const auto& color = tankexp::GetNodeKindDefinition(kind).color;
    return {color[0], color[1], color[2], color[3]};
}
/// @brief ノードの種類に対応するアイコンを返す。
const char* NodeIcon(NK kind)
{
    return tankexp::GetNodeKindDefinition(kind).icon;
}
/// @brief ノードの種類に対応する表示名を返す。
const char* NodeName(NK kind)
{
    return tankexp::GetNodeKindDefinition(kind).name;
}
/// @brief 遠征マップの説明文を表示幅に合わせて改行する。
std::string WrapMapText(const std::string& text, float width, int maxLines)
{
    std::string result;
    float used = 0;
    int line = 1;
    for (size_t pos = 0; pos < text.size();) {
        const auto c = static_cast<unsigned char>(text[pos]);
        const size_t count = c < 128 ? 1 : c < 224 ? 2 : c < 240 ? 3 : 4;
        const bool newline = c == '\n';
        const float units = c < 128 ? 0.58f : 1.0f;
        if (newline || used + units > width) {
            if (line >= maxLines) {
                result += "…";
                break;
            }
            result += '\n';
            used = 0;
            ++line;
            if (newline) {
                ++pos;
                continue;
            }
        }
        result.append(text, pos, count);
        used += units;
        pos += count;
    }
    return result;
}
/// @brief 戦闘中に残っている脅威の数を返す。
int ThreatCount(EnemyManager* enemies)
{
    int n = 0;
    for (auto* e : enemies->GetEnemyPtrs())
        if (e && !e->IsDead() && !e->IsRunResource())
            ++n;
    return n;
}
/// @brief 入力の押下開始を判定する。
bool Press(cg2::Input* input, int key)
{
    return input->IsKeyTriggered(static_cast<uint8_t>(key));
}
/// @brief 指定位置が対象の範囲内か判定する。
bool Inside(cg2::Vector2 mouse, float x, float y, float w, float h)
{
    return mouse.x >= x && mouse.x <= x + w && mouse.y >= y && mouse.y <= y + h;
}
} // namespace

void ExpeditionMapController::InitializeExpeditionMap()
{
    cg2::StartupTrace::Scope scope("Expedition.Map");
    world_.run.expeditionMapEnabled_ = true;
    world_.run.expeditionMapDefinition_ = tankexp::DefaultExpeditionMap();
    world_.run.expeditionRooms_ = tankexp::DefaultRoomCatalog();
    world_.run.expeditionContent_ = tankcontent::DefaultCatalog();
    std::string error;
    if (!tankexp::LoadExpeditionMap(tankexp::kExpeditionMapPath, world_.run.expeditionMapDefinition_, error))
        world_.run.expeditionMapStatus_ = error;
    if (!tankexp::LoadRoomCatalog(tankexp::kRoomCatalogPath, world_.run.expeditionRooms_, error))
        world_.run.expeditionMapStatus_ = error;
    if (!tankcontent::LoadCatalog("resources/configs/expedition_content.json", world_.run.expeditionContent_, error))
        world_.run.expeditionMapStatus_ = error;
    if (sessionMap)
        world_.run.expeditionMapDefinition_ = *sessionMap;
    if (sessionRooms)
        world_.run.expeditionRooms_ = *sessionRooms;
    if (sessionContent)
        world_.run.expeditionContent_ = *sessionContent;
    wchar_t mapTest[8]{};
    world_.run.expeditionMapAutoTest_ = GetEnvironmentVariableW(L"CG2_TANK_MAP_AUTOTEST", mapTest, 8) > 0 && mapTest[0] == L'1';
    world_.run.expeditionSeed_ = static_cast<uint32_t>(GetTickCount64()) ^ world_.run.expeditionMapDefinition_.generationSeed;
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (GameplayScenarioSession::Get().IsActive())
        world_.run.expeditionSeed_ = GameplayScenarioSession::Get().GetSettings().seed;
#endif
    if (world_.run.expeditionMapAutoTest_)
        world_.run.expeditionMapDefinition_ = tankexp::DefaultExpeditionMap();
    else if (world_.run.expeditionMapDefinition_.procedural) {
        tankexp::MapDefinition generated;
        if (tankexp::GenerateExpeditionMap(world_.run.expeditionMapDefinition_, world_.run.expeditionSeed_, generated, error, 30, 8))
            world_.run.expeditionMapDefinition_ = std::move(generated);
        else
            world_.run.expeditionMapStatus_ = "生成設定を確認してください: " + error;
    }
    world_.run.expeditionIntroOffers_ = tankcontent::IntroUpgradeIds(world_.run.expeditionContent_, world_.run.expeditionSeed_);
    if (!tankexp::FindRoom(world_.run.expeditionRooms_, "tutorial_training")) {
        auto room = tankexp::MakeEmptyRoom("tutorial_training", "チュートリアル・戦闘");
        room.playerStart = {34, 30};
        room.spawns = {{"practice_target", "tutorial_target", 44, 30, 12}};
        world_.run.expeditionRooms_.rooms.push_back(std::move(room));
    }
    auto trainingEnemy = [this](const char* id, tankcontent::EnemyBehavior behavior, int credits) {
        if (tankcontent::FindEnemy(world_.run.expeditionContent_, id))
            return;
        tankcontent::Enemy e;
        e.id = id;
        e.name = id;
        e.behavior = behavior;
        e.hp = 24;
        e.creditDrop = credits;
        e.contactDamage = 2;
        e.bulletDamage = 3;
        e.fireIntervalScale = 2.0f;
        e.color = {0.55f, 0.95f, 1.5f, 1};
        world_.run.expeditionContent_.enemies.push_back(e);
    };
    trainingEnemy("tutorial_target", tankcontent::EnemyBehavior::Square, 4);
    trainingEnemy("tutorial_shooter", tankcontent::EnemyBehavior::Shooter, 4);
    trainingEnemy("tutorial_retry", tankcontent::EnemyBehavior::Shooter, 0);
    std::vector<std::string> knownEnemies;
    for (const auto& enemy : world_.run.expeditionContent_.enemies)
        knownEnemies.push_back(enemy.id);
    if (!tankexp::ValidateExpeditionMapRooms(world_.run.expeditionMapDefinition_, world_.run.expeditionRooms_, error, &knownEnemies))
        world_.run.expeditionMapStatus_ = "F4/F5/F6で参照を確認: " + error;
    world_.combatValidation->InitializeCombatValidationFixture();
    world_.experienceValidation->InitializeExperienceValidation();
    world_.specialValidation->InitializeSpecialValidationFixture();
    world_.run.expeditionContentEditor_.Open(world_.run.expeditionContent_);
    world_.run.expeditionMapRun_.Reset(world_.run.expeditionMapDefinition_, error);
    world_.resources.enemyManager_->SetExpeditionContent(world_.run.expeditionContent_);
    world_.resources.player_->InstallRunAuthoredClasses(world_.run.expeditionContent_);
    SetExpeditionBlueprint(0);
    world_.resources.player_->SetRunCurrencyMode(true);
    world_.run.tankExpedition_.OpenMap();
    world_.run.tankExpeditionTutorial_.Skip();
    world_.expeditionExperience->InitializeExpeditionExperience();
    const auto initialNodes = world_.run.expeditionMapRun_.GetAvailableNodeIds();
    if (!initialNodes.empty())
        world_.run.expeditionMapSelection_ = initialNodes.front();
    // Prefer learning the controls even when an authored map lists skip first.
    for (const auto& id : initialNodes)
        if (const auto* n = tankexp::FindMapNode(world_.run.expeditionMapRun_.GetDefinition(), id);
            n && n->role == tankexp::NodeRole::TutorialCombat) {
            world_.run.expeditionMapSelection_ = id;
            break;
        }
    const cg2::Vector4 white{0.83f, 0.96f, 1, 1}, muted{0.42f, 0.64f, 0.75f, 1};
    world_.run.expeditionMapTitle_ = MapLabel(32, {44, 85}, white);
    world_.run.expeditionMapSubtitle_ = MapLabel(16, {46, 133}, muted);
    world_.run.expeditionMapLegend_ = MapLabel(14, {46, 195}, muted);
    world_.run.expeditionMapInfo_ = MapLabel(18, {46, 586}, white);
    world_.run.expeditionMapHelp_ = MapLabel(13, {46, 695}, muted);
    world_.run.expeditionCurtain_ = MapRect({0, 0}, {1280, 720}, {0.006f, 0.015f, 0.03f, 0});
    world_.run.expeditionTransitionPanel_ = MapRect({260, 270}, {760, 156}, {0.008f, 0.024f, 0.04f, 0});
    world_.run.expeditionTransitionRail_ = MapRect({440, 405}, {400, 2}, {0.15f, 0.35f, 0.45f, 1});
    world_.run.expeditionTransitionProgress_ = MapRect({440, 405}, {1, 2}, white);
    world_.run.expeditionTransitionTitle_ = MapLabel(34, {640, 308}, white);
    world_.run.expeditionTransitionTitle_->SetAnchorPoint({0.5f, 0.5f});
    world_.run.expeditionTransitionDetail_ = MapLabel(18, {640, 360}, muted);
    world_.run.expeditionTransitionDetail_->SetAnchorPoint({0.5f, 0.5f});
    for (int x = 36; x < 1260; x += 36)
        world_.run.expeditionMapGrid_.push_back(MapRect({static_cast<float>(x), 152}, {1, 420}, {0.08f, 0.26f, 0.35f, 0.13f}));
    for (int y = 152; y <= 572; y += 35)
        world_.run.expeditionMapGrid_.push_back(MapRect({36, static_cast<float>(y)}, {1208, 1}, {0.08f, 0.26f, 0.35f, 0.13f}));
    for (const auto& node : world_.run.expeditionMapRun_.GetDefinition().nodes) {
        MapNodeVisual v;
        v.halo = MapRect({0, 0}, {55, 55}, NodeColor(node.kind));
        v.rim = MapRect({0, 0}, {46, 46}, NodeColor(node.kind));
        v.fill = MapRect({0, 0}, {42, 42}, {0.016f, 0.040f, 0.065f, 1});
        for (auto* s : {v.halo.get(), v.rim.get(), v.fill.get()}) {
            s->SetAnchorPoint({0.5f, 0.5f});
            s->SetRotation(0.78539816f);
        }
        v.icon = MapLabel(23, {0, 0}, NodeColor(node.kind));
        v.icon->SetAnchorPoint({0.5f, 0.5f});
        v.label = MapLabel(13, {0, 0}, white);
        v.label->SetAnchorPoint({0.5f, 0});
        v.state = MapLabel(11, {0, 0}, muted);
        v.state->SetAnchorPoint({0.5f, 0});
        world_.run.expeditionMapVisuals_.push_back(std::move(v));
        for (const auto& next : node.next) {
            MapEdgeVisual edge;
            edge.from = node.id;
            edge.to = next;
            edge.glow = MapRect({0, 0}, {1, 7}, {0.12f, 0.6f, 0.78f, 0.10f});
            edge.line = MapRect({0, 0}, {1, 2}, {0.2f, 0.45f, 0.57f, 0.5f});
            edge.pulse = MapRect({0, 0}, {5, 5}, {0.45f, 1, 1, 0});
            edge.pulse->SetAnchorPoint({0.5f, 0.5f});
            edge.pulse->SetRotation(0.78539816f);
            edge.glow->SetAnchorPoint({0, 0.5f});
            edge.line->SetAnchorPoint({0, 0.5f});
            world_.run.expeditionMapEdges_.push_back(std::move(edge));
        }
    }
    for (int i = 0; i < 3; ++i) {
        const float x = 46.0f + i * 394.0f;
        world_.run.expeditionBlueprintButtons_[i] = MapRect({x, 650}, {378, 34}, {0.035f, 0.11f, 0.16f, 1});
        world_.run.expeditionBlueprintLabels_[i] = MapLabel(15, {x + 12, 654}, white);
        world_.run.expeditionBlueprintLabels_[i]->SetText(i == 0 ? "←" : i == 1 ? "→" : " ");
    }
    wchar_t flag[8]{};
    world_.run.expeditionMapAutoTest_ = GetEnvironmentVariableW(L"CG2_TANK_MAP_AUTOTEST", flag, 8) > 0 && flag[0] == L'1';
    if (world_.run.expeditionMapAutoTest_) {
        world_.presentation.debugPlayerNoDamage_ = true;
        world_.run.tankExpeditionTutorial_.Skip();
        std::filesystem::create_directories("generated/expedition_map");
        std::ofstream("generated/expedition_map/validation.json") << "{\"completed\":false}\n";
    }
    world_.expeditionBuildSelection->InitializeExpeditionBuildCards();
    RefreshExpeditionMapUi();
}

void ExpeditionMapController::BeginExpeditionPresentation(int action, const std::string& title, const std::string& detail,
                                                          const cg2::Vector4& color)
{
    if (!world_.run.expeditionTransition_.Begin())
        return;
    world_.run.expeditionTransitionAction_ = action;
    world_.run.expeditionTransitionColor_ = color;
    world_.run.expeditionTransitionTitle_->SetText(title);
    world_.run.expeditionTransitionDetail_->SetText(detail);
    world_.run.expeditionTransitionTitle_->PrepareForDraw();
    world_.run.expeditionTransitionDetail_->PrepareForDraw();
    world_.run.tankExpeditionAudio_.UiConfirm();
}

void ExpeditionMapController::RequestExpeditionMapNode(const std::string& id)
{
    if (world_.run.expeditionTransition_.IsActive() || world_.run.expeditionBuildChoice_)
        return;
    if (!world_.run.expeditionMapRun_.CanSelectNode(id)) {
        world_.run.expeditionUiErrorAge_ = 0.35f;
        world_.run.tankExpeditionAudio_.UiDenied();
        world_.run.expeditionMapStatus_ = "明るく光る、接続された地点を選んでください。";
        RefreshExpeditionMapUi();
        return;
    }
    const auto* node = tankexp::FindMapNode(world_.run.expeditionMapRun_.GetDefinition(), id);
    if (!node)
        return;
    if (!world_.run.expeditionBuildChosen_ && node->role == tankexp::NodeRole::None && !world_.run.combatValidationEnabled_) {
        world_.run.expeditionBuildChoice_ = true;
        world_.run.tankRunMenuAge_ = 0;
        world_.expeditionController->RefreshTankExpeditionUi();
        return;
    }
    world_.run.expeditionPendingNode_ = id;
    BeginExpeditionPresentation(1, tankexp::IsCombatNode(node->kind) ? "出撃" : "入場",
                                node->role == tankexp::NodeRole::TutorialCombat ? "操作を学ぶ"
                                : node->role == tankexp::NodeRole::TutorialSkip ? "説明をスキップ / 同じ通貨を受け取ります"
                                                                                : NodeName(node->kind),
                                NodeColor(node->kind));
}

void ExpeditionMapController::UpdateExpeditionPresentation(float dt)
{
    world_.run.expeditionPresentationClock_ += dt;
    world_.expeditionBuildSelection->UpdateExpeditionBuildCards(dt);
    world_.run.expeditionUiErrorAge_ = (std::max)(0.0f, world_.run.expeditionUiErrorAge_ - dt);
    world_.run.expeditionHitSparkCooldown_ = (std::max)(0.0f, world_.run.expeditionHitSparkCooldown_ - dt);
    for (auto& hit : world_.run.expeditionHitSparks_)
        hit.age += dt;
    std::erase_if(world_.run.expeditionHitSparks_, [](const ExpeditionHitSpark& hit) {
        return hit.age >= 0.18f;
    });
    const float blend = 1 - std::exp(-dt * 16);
    const auto& nodes = world_.run.expeditionMapRun_.GetDefinition().nodes;
    for (size_t i = 0; i < world_.run.expeditionMapVisuals_.size(); ++i) {
        auto& focus = world_.run.expeditionMapVisuals_[i].focus;
        focus += ((nodes[i].id == world_.run.expeditionMapSelection_ ? 1.0f : 0.0f) - focus) * blend;
    }
    for (int i = 0; i < 3; ++i)
        world_.run.expeditionCardFocus_[i] +=
            ((i == world_.run.tankRunSelection_ ? 1.0f : 0.0f) - world_.run.expeditionCardFocus_[i]) * blend;
    if (world_.run.expeditionTransition_.IsActive()) {
        if (world_.run.expeditionMapAutoTest_ && world_.run.expeditionTransition_.Age() > 0.26f &&
            world_.run.expeditionTransition_.Age() < 0.45f && world_.run.tankRunCapturePath_.empty()) {
            const std::string capture = "transition_" + std::to_string(world_.run.expeditionTransitionAction_);
            if (std::find(world_.run.expeditionMapTestVisited_.begin(), world_.run.expeditionMapTestVisited_.end(), capture) ==
                world_.run.expeditionMapTestVisited_.end()) {
                world_.run.expeditionMapTestVisited_.push_back(capture);
                world_.run.tankRunCapturePath_ = "generated/expedition_map/" + capture + ".png";
            }
        }
        // 遷移中はIsTankRunMenuOpenで戦闘を止めるが、最後の撃破フラッシュは基準時間で消えるまで進める。
        for (auto& burst : world_.run.tankRunBursts_)
            burst.age += dt;
        std::erase_if(world_.run.tankRunBursts_, [](const RunBurst& b) {
            return b.age > (b.resource ? 0.7f : 0.35f);
        });
        // Advanceが反映時点を通知した回で予約actionを取り出して0へ戻す。遷移全体の終了とは別。
        if (world_.run.expeditionTransition_.Advance(dt)) {
            const int action = world_.run.expeditionTransitionAction_;
            world_.run.expeditionTransitionAction_ = 0;
            if (action == 1)
                EnterExpeditionMapNode(world_.run.expeditionPendingNode_);
            else if (action == 2)
                CompleteExpeditionMapCombat();
            else if (action == 3)
                SelectExpeditionService(world_.run.expeditionPendingService_);
            else if (action == 4)
                world_.expeditionBuildSelection->SelectExpeditionBuildStyle(world_.run.expeditionPendingBuild_);
        }
        if (!world_.run.expeditionTransition_.IsActive()) {
            world_.run.tankRunMenuAge_ = 0;
            world_.run.expeditionPendingNode_.clear();
        }
    }
}

void ExpeditionMapController::DrawExpeditionPresentation()
{
    if (!world_.run.expeditionTransition_.IsActive() || !world_.run.expeditionCurtain_)
        return;
    cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
    world_.run.expeditionCurtain_->SetColor({0.006f, 0.015f, 0.03f, world_.run.expeditionTransition_.Cover()});
    world_.run.expeditionCurtain_->Update();
    world_.run.expeditionCurtain_->Draw();
    const float alpha = world_.run.expeditionTransition_.LabelAlpha();
    world_.run.expeditionTransitionPanel_->SetColor({0.008f, 0.024f, 0.04f, alpha * 0.94f});
    world_.run.expeditionTransitionPanel_->Update();
    world_.run.expeditionTransitionPanel_->Draw();
    auto color = world_.run.expeditionTransitionColor_;
    color.w = alpha;
    const float width = 400 * tankexp::PresentationTransition::Smooth(world_.run.expeditionTransition_.Age() / 0.70f);
    world_.run.expeditionTransitionRail_->SetColor({0.12f, 0.30f, 0.40f, alpha});
    world_.run.expeditionTransitionRail_->Update();
    world_.run.expeditionTransitionRail_->Draw();
    world_.run.expeditionTransitionProgress_->SetSize({(std::max)(1.0f, width), 2});
    world_.run.expeditionTransitionProgress_->SetColor(color);
    world_.run.expeditionTransitionProgress_->Update();
    world_.run.expeditionTransitionProgress_->Draw();
    world_.run.expeditionTransitionTitle_->SetAlpha(alpha);
    world_.run.expeditionTransitionDetail_->SetAlpha(alpha);
    world_.run.expeditionTransitionTitle_->Draw();
    world_.run.expeditionTransitionDetail_->Draw();
}

void ExpeditionMapController::SetExpeditionBlueprint(int index)
{
    if (index < 0 || index > 2 || !world_.run.expeditionMapRun_.GetChosenNodeIds().empty())
        return;
    world_.run.expeditionBlueprint_ = index;
    tankrun::Config config;
    config.combatSeconds = 1000000;
    uint32_t blueprintSeed = static_cast<uint32_t>(GetTickCount64());
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (GameplayScenarioSession::Get().IsActive())
        blueprintSeed = GameplayScenarioSession::Get().GetSettings().seed;
#endif
    world_.run.tankRun_ = tankrun::RunDirector(blueprintSeed, config);
    world_.run.tankRun_.ChooseLoadout(index);
    world_.resources.player_->ConfigurePrototypeLoadout(index);
    world_.run.tankRun_.ChooseCore(index);
    world_.arenaRunController->ApplyTankRunCards();
}

void ExpeditionMapController::UpdateExpeditionAuthoring()
{
    // Keep queued purchases and destinations stable until their transition commits.
    if (world_.run.expeditionTransition_.IsActive())
        return;
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
    if (!ImGui::GetCurrentContext())
        return;
#else
    return;
#endif
    // Authoring runs before the paused-frame decision. Applying content never
    // restarts a fight or resets the run's wallet/history.
    if (Press(world_.resources.input_, DIK_F4))
        world_.run.expeditionRoomEditorOpen_ = !world_.run.expeditionRoomEditorOpen_;
    if (Press(world_.resources.input_, DIK_F5))
        world_.run.expeditionMapEditorOpen_ = !world_.run.expeditionMapEditorOpen_;
    if (Press(world_.resources.input_, DIK_F6))
        world_.run.expeditionContentEditorOpen_ = !world_.run.expeditionContentEditorOpen_;
    std::vector<std::string> enemies, rooms;
    for (const auto& e : world_.run.expeditionContent_.enemies)
        enemies.push_back(e.id);
    for (const auto& r : world_.run.expeditionRooms_.rooms)
        rooms.push_back(r.id);
    if (world_.run.expeditionRoomEditor_.Draw(&world_.run.expeditionRoomEditorOpen_, world_.run.expeditionRooms_, enemies,
                                              &world_.run.expeditionMapDefinition_, &world_.run.expeditionMapRun_.GetDefinition())) {
        sessionRooms = world_.run.expeditionRooms_;
        world_.run.expeditionMapStatus_ = "配置を適用しました。次の区画への入場時に反映します。";
    }
    if (world_.run.expeditionMapEditor_.Draw(world_.run.expeditionMapEditorOpen_, world_.run.expeditionMapDefinition_,
                                             world_.run.expeditionRooms_, &enemies)) {
        sessionMap = world_.run.expeditionMapDefinition_;
        world_.run.expeditionMapStatus_ = "作戦マップを適用しました。次の遠征から反映します。";
    }
    std::vector<std::string> usedEnemyIds;
    for (const auto& room : world_.run.expeditionRooms_.rooms)
        for (const auto& spawn : room.spawns)
            usedEnemyIds.push_back(spawn.type);
    if (world_.run.expeditionContentEditor_.Draw(world_.run.expeditionContentEditorOpen_, world_.run.expeditionContent_, usedEnemyIds)) {
        world_.resources.enemyManager_->SetExpeditionContent(world_.run.expeditionContent_);
        world_.resources.player_->InstallRunAuthoredClasses(world_.run.expeditionContent_);
        world_.run.expeditionIntroOffers_ = tankcontent::IntroUpgradeIds(world_.run.expeditionContent_, world_.run.expeditionSeed_);
        sessionContent = world_.run.expeditionContent_;
        world_.arenaRunController->ApplyTankRunCards();
        world_.run.expeditionMapStatus_ = "種類を適用しました。次の敵出現・工房で選択できます。";
        RefreshExpeditionServiceOffers();
    }
}

void ExpeditionMapController::EnterExpeditionMapNode(const std::string& id)
{
    if (!world_.run.expeditionMapRun_.CanSelectNode(id))
        return;
    const auto* node = tankexp::FindMapNode(world_.run.expeditionMapRun_.GetDefinition(), id);
    if (!node)
        return;
    if (tankexp::IsCombatNode(node->kind)) {
        const auto* room = tankexp::FindRoom(world_.run.expeditionRooms_, node->roomTemplate);
        if (!room) {
            world_.run.expeditionMapStatus_ = "部屋が見つかりません。F4 / F5で配置と部屋の関連付けを確認してください。";
            return;
        }
        std::string error;
        if (!tankexp::ValidateRoomCatalog(world_.run.expeditionRooms_, error)) {
            world_.run.expeditionMapStatus_ = error;
            return;
        }
        for (const auto& spawn : room->spawns)
            if (!tankcontent::FindEnemy(world_.run.expeditionContent_, spawn.type)) {
                world_.run.expeditionMapStatus_ = "必要な敵の設定が見つかりません。F4 / F6で設定を確認してください。";
                return;
            }
        if ((node->kind == NK::Boss) != (room->objective == "boss")) {
            world_.run.expeditionMapStatus_ = "最終決戦にはボス目標の部屋、それ以外には通常目標の部屋を指定してください。";
            return;
        }
    }
    const auto before = world_.run.expeditionMapRun_;
    const auto beforeDirector = world_.run.tankExpedition_;
    if (!world_.run.expeditionMapRun_.SelectNode(id))
        return;
    if (node->role == tankexp::NodeRole::TutorialCombat) {
        world_.run.expeditionGuideActive_ = true;
        world_.run.expeditionGuideShooterSpawned_ = false;
        world_.run.expeditionGuide_.Begin(1, 4);
        world_.run.expeditionGuideAge_ = 0;
        world_.run.expeditionGuideLastKills_ = world_.combat.defeatedEnemies_;
        world_.run.expeditionGuideDamageCount_ = world_.resources.player_->GetDamageTakenCount();
        world_.run.expeditionGuideAttackCount_ = world_.resources.player_->GetPrimaryAttackCount();
    } else if (node->role == tankexp::NodeRole::TutorialUpgrade) {
        world_.run.expeditionGuideActive_ = true;
        world_.run.expeditionGuide_.BeginUpgradeOnly();
        world_.run.expeditionGuideAge_ = 0;
    } else if (node->role == tankexp::NodeRole::TutorialSkip)
        world_.run.expeditionGuideActive_ = false;
    world_.run.tankExpeditionTutorial_.RecordRoute();
    world_.run.expeditionMapStatus_.clear();
    world_.run.expeditionServicePage_ = 0;
    world_.run.tankRunSelection_ = 0;
    world_.run.tankRunMenuAge_ = 0;
    if (tankexp::IsCombatNode(node->kind)) {
        const auto* room = tankexp::FindRoom(world_.run.expeditionRooms_, node->roomTemplate);
        const auto kind = room->objective == "boss"      ? tankexp::RoomKind::Boss
                          : room->objective == "control" ? tankexp::RoomKind::Guard
                          : node->kind == NK::Elite      ? tankexp::RoomKind::Elite
                                                         : tankexp::RoomKind::Skirmish;
        bool entered = false;
        try {
            entered = world_.run.tankExpedition_.BeginMapRoom(node->combatStage, kind) && StartAuthoredExpeditionRoom();
        }
        catch (const std::exception& e) {
            world_.run.expeditionMapStatus_ = e.what();
        }
        if (!entered) {
            world_.run.expeditionMapRun_ = before;
            world_.run.tankExpedition_ = beforeDirector;
            world_.run.expeditionMapStatus_ = "区画を読み込めませんでした。進行と通貨を保持しています。 " + world_.run.expeditionMapStatus_;
        }
    } else {
        if (node->kind == NK::Currency) {
            world_.expeditionExperience->SpawnExpeditionCredits(world_.resources.player_->GetWorldPosition(), node->clearReward, true);
            world_.run.expeditionCollectAll_ = true;
        }
        RefreshExpeditionServiceOffers();
    }
    world_.expeditionController->RefreshTankExpeditionUi();
}

bool ExpeditionMapController::StartAuthoredExpeditionRoom()
{
    const auto* node = world_.run.expeditionMapRun_.GetActiveNode();
    if (!node)
        return false;
    const auto* room = tankexp::FindRoom(world_.run.expeditionRooms_, node->roomTemplate);
    if (!room)
        return false;
    // 制作部屋をCSVへ書き出し、Stageの既存の読み込み/統合ブロック生成を使う。
    // ここまでの失敗では、以下の敵・弾・自機状態の初期化へ進まない。
    std::filesystem::create_directories("generated/expedition_map");
    const std::string path = "generated/expedition_map/active_room.csv";
    {
        std::ofstream out(path);
        out << tankexp::RoomToCsv(*room);
        if (!out) {
            world_.run.expeditionMapStatus_ = "配置プレビューを書き込めませんでした。";
            return false;
        }
    }
    if (!world_.resources.stage_->LoadRunMap(path))
        return false;
    // 地形を読み込めてから旧戦闘の実体を消去する。部屋遷移の反映は衝突走査中には呼ばれない。
    world_.resources.enemy_->SetRunEncounterEnabled(false);
    world_.run.tankExpeditionRivalActive_ = false;
    world_.resources.enemyManager_->ClearRunActors();
    world_.resources.bulletManager_->ClearAll();
    world_.run.expeditionHitSparks_.clear();
    world_.run.expeditionBossPhase2Seen_ = false;
    world_.run.expeditionCollectAll_ = false;
    world_.run.expeditionClearRewardQueued_ = false;
    world_.run.expeditionCredits_.clear();
    world_.presentation.playerLaserBeams_.clear();
    world_.presentation.playerMines_.clear();
    world_.presentation.playerMineExplosions_.clear();
    world_.presentation.playerMeleeSlashes_.clear();
    world_.presentation.playerNeonAfterimages_.clear();
    world_.presentation.neonTriangleParticles_.clear();
    world_.run.tankRunBursts_.clear();
    world_.combat.hpBarVisibility_.clear();
    if (world_.resources.playerMeleeTrailManager_)
        world_.resources.playerMeleeTrailManager_->ClearInstances();
    for (auto& resource : world_.run.tankRunResources_)
        resource = {};
    world_.run.tankExpeditionNodes_ = 0;
    world_.run.tankExpeditionSpawned_ = 0;
    world_.run.tankExpeditionRoomPending_ = false;
    world_.run.tankExpeditionResourceWon_ = false;
    world_.run.tankExpeditionRewardOpen_ = false;
    world_.run.tankExpeditionMaintenanceOpen_ = false;
    world_.run.tankExpeditionArrival_ = 0;
    world_.run.tankExpeditionEnemyHp_.clear();
    world_.run.tankExpeditionEnemyWarning_.clear();
    world_.run.tankRunComboTime_ = 0;
    world_.run.tankExpeditionDetailsOpen_ = false;
    world_.presentation.stagePostCacheValid_ = false;
    world_.resources.stage_->SetDamageBlockDamage(12);
    // 生存中で遠征成長が有効な自機の部屋状態をリセットし、開始位置へ置く。現在HP/成長は部屋をまたいで保持する。
    const cg2::Vector3 start{room->playerStart.x, room->playerStart.y, 0};
    world_.resources.player_->ResetRunRoomState(start);
    world_.run.tankExpeditionTutorialPrevious_ = start;
    world_.resources.camera->SetTranslate({start.x, start.y, world_.resources.camera->GetTranslate().z});
    world_.resources.camera->Update();
    for (const auto& spawn : room->spawns) {
        if (world_.resources.enemyManager_->SpawnLevelEnemy({spawn.x, spawn.y, 0}, spawn.type, spawn.hp > 0 ? spawn.hp : -1))
            ++world_.run.tankExpeditionSpawned_;
    }
    if (room->objective == "control")
        for (size_t i = 0; i < room->objectiveTargets.size() && i < world_.run.tankRunResources_.size(); ++i) {
            auto& r = world_.run.tankRunResources_[i];
            r.position = {room->objectiveTargets[i].x, room->objectiveTargets[i].y, 0};
            r.active = world_.resources.enemyManager_->SpawnRunResource(r.position, 60, [this, i](bool owned) {
                world_.arenaRunController->OnTankRunResourceClaim(i, owned);
            });
        }
    if (room->objective == "boss") {
        const auto& p = room->objectiveTargets.front();
        world_.run.tankExpeditionRivalActive_ = true;
        world_.resources.enemy_->ResetRunEncounter({p.x, p.y, 0}, world_.run.tankExpeditionBalance_.value("bossMaxHp", 900), 1, true);
        world_.resources.enemy_->EnableExpeditionRival(true);
        auto progress = world_.resources.enemy_->GetEnemyProgressConfig();
        progress.levelingModeEnabled = false;
        world_.resources.enemy_->SetEnemyProgressConfig(progress);
        world_.combat.screenEffectDirector_.TriggerBossEntry();
    }
    // 共有の部屋調整を適用する。マップ式の制作敵はApplyTankExpeditionRoomBalance側で
    // HasAuthoredDefinitionを確認し、個別設定の接触ダメージを全体値で上書きしない。
    world_.expeditionBalanceEditor->ApplyTankExpeditionRoomBalance();
    world_.depthEncounter->ConfigureNeonDepthEncounter();
    world_.combat.previousPlayerHp_ = world_.resources.player_->GetHp();
    world_.combat.previousBossHp_ = world_.resources.enemy_->GetHp();
    world_.combat.bossDefeatHandled_ = false;
    if (!world_.run.expeditionTransition_.IsActive() && !world_.resources.enemy_->IsNeonDepthEncounterEnabled())
        world_.combatFlow->SetEventCallout(std::string(NodeName(node->kind)) + " / " +
                                               (room->objective == "control" ? "通貨ボックスを3つ壊そう"
                                                : room->objective == "boss"  ? "ボスを撃破"
                                                                             : "敵を全滅"),
                                           1.4f);
    return true;
}

void ExpeditionMapController::CompleteExpeditionMapCombat()
{
    // 生存中の非ボス戦だけを地図選択へ移す。報酬通貨の生成/回収は呼び出し側で既に処理する。
    if (!world_.run.tankExpedition_.IsCombat() || world_.resources.player_->IsDead())
        return;
    const auto* node = world_.run.expeditionMapRun_.GetActiveNode();
    if (!node || node->kind == NK::Boss)
        return;
    const int reward = node->clearReward;
    if (!world_.run.expeditionMapRun_.CompleteCombat(false))
        return;
    world_.run.tankExpedition_.OpenMap();
    world_.run.tankExpeditionTutorial_.RecordRoomClear();
    world_.run.expeditionMapSelection_ = world_.run.expeditionMapRun_.GetAvailableNodeIds().front();
    world_.run.expeditionMapStatus_ = "戦闘クリア！ 光る地点を選んで進もう。";
    if (const auto* next = tankexp::FindMapNode(world_.run.expeditionMapRun_.GetDefinition(), world_.run.expeditionMapSelection_))
        world_.run.expeditionMapScroll_ = (std::max)(0.0f, 120.0f * (next->column - 8));
    world_.run.tankExpeditionAudio_.Upgrade();
    world_.run.tankRunMenuAge_ = 0;
    world_.expeditionController->RefreshTankExpeditionUi();
}

void ExpeditionMapController::RefreshExpeditionServiceOffers()
{
    world_.run.expeditionServiceOffers_.clear();
    const auto* node = world_.run.expeditionMapRun_.GetActiveNode();
    if (!node)
        return;
    if (node->kind == NK::Upgrade || node->kind == NK::Evolution) {
        if (IsIntroExpeditionService())
            world_.run.expeditionServiceOffers_ = world_.run.expeditionIntroOffers_;
        else {
            uint32_t state = world_.run.expeditionSeed_;
            for (const unsigned char c : node->id)
                state = (state ^ c) * 16777619u;
            world_.run.expeditionServiceOffers_ =
                tankcontent::BuildShopOffers(world_.run.expeditionContent_, world_.run.expeditionBuildStyle_,
                                             world_.run.tankRun_.GetCardCounts(), world_.run.expeditionPurchases_, state);
        }
    }
    world_.run.expeditionServicePage_ = (std::clamp)(world_.run.expeditionServicePage_, 0,
                                                     (std::max)(0, (static_cast<int>(world_.run.expeditionServiceOffers_.size()) - 1) / 3));
}

bool ExpeditionMapController::IsIntroExpeditionService() const
{
    const auto* node = world_.run.expeditionMapRun_.GetActiveNode();
    return node && tankexp::IsIntroUpgrade(node->role);
}

int ExpeditionMapController::ExpeditionServicePrice(const std::string& id) const
{
    const auto* node = world_.run.expeditionMapRun_.GetActiveNode();
    if (!node)
        return 0;
    if (IsIntroExpeditionService())
        return node->serviceCost;
    if (node->kind == NK::Upgrade || node->kind == NK::Evolution) {
        const auto* u = tankcontent::FindUpgrade(world_.run.expeditionContent_, id);
        return (std::max)(u ? u->price : 0, node->serviceCost);
    }
    return node->serviceCost;
}

void ExpeditionMapController::SelectExpeditionService(int option)
{
    const auto* node = world_.run.expeditionMapRun_.GetActiveNode();
    if (!node || tankexp::IsCombatNode(node->kind) || node->kind == NK::Currency || option < 0 || option > 3)
        return;
    auto reject = [this] {
        world_.run.expeditionUiErrorAge_ = 0.35f;
        world_.run.tankExpeditionAudio_.UiDenied();
        world_.expeditionController->RefreshTankExpeditionUi();
    };
    if (!world_.run.expeditionTransition_.IsActive()) {
        int price = 0;
        if (option != 3) {
            if (node->kind == NK::Heal) {
                if (option != 0)
                    return;
                if (world_.resources.player_->GetHp() >= world_.resources.player_->GetMaxHp()) {
                    world_.run.expeditionMapStatus_ = "HPは満タンです。「修理せず進む」を選んでください。";
                    reject();
                    return;
                }
                price = node->serviceCost;
            } else {
                const size_t index = static_cast<size_t>(world_.run.expeditionServicePage_ * 3 + option);
                if (index >= world_.run.expeditionServiceOffers_.size()) {
                    reject();
                    return;
                }
                const auto& id = world_.run.expeditionServiceOffers_[index];
                price = ExpeditionServicePrice(id);
            }
            if (!world_.run.expeditionMapRun_.CanAfford(price)) {
                world_.run.expeditionMapStatus_ =
                    std::string("通貨が足りません。") + (node->kind == NK::Heal ? "修理せず進めます。" : "購入せず進めます。");
                reject();
                return;
            }
        }
        world_.run.expeditionPendingService_ = option;
        if (option < 3 && world_.expeditionBuildSelection->IsExpeditionBuildCardScreen())
            world_.run.expeditionRewardCards_[option]->PlayAcquire();
        BeginExpeditionPresentation(3,
                                    option == 3              ? "次の地点へ"
                                    : node->kind == NK::Heal ? "装甲を修理"
                                                             : "強化を装備",
                                    option == 3 ? "残りの通貨を持って先へ進みます"
                                                : (node->kind == NK::Heal ? "修理しています" : "装備しています"),
                                    NodeColor(node->kind));
        return;
    }
    bool complete = false;
    if (option == 3)
        complete = world_.run.expeditionMapRun_.CompleteService(false);
    else if (node->kind == NK::Heal) {
        if (option != 0)
            return;
        if (world_.resources.player_->GetHp() >= world_.resources.player_->GetMaxHp()) {
            world_.run.expeditionMapStatus_ = "HPは満タンです。「修理せず進む」を選んでください。";
            reject();
            return;
        }

        if (!world_.run.expeditionMapRun_.CompleteService(true)) {
            world_.run.expeditionMapStatus_ = "通貨が足りません。修理せず進めます。";
            reject();
            return;
        }
        world_.resources.player_->HealRunPlayer((std::max)(1, world_.resources.player_->GetMaxHp() / 2));
        world_.combat.previousPlayerHp_ = world_.resources.player_->GetHp();
        world_.run.expeditionMapStatus_ = "HPを50%回復しました";
        complete = true;
        ++world_.run.expeditionMapTestHeals_;
    } else {
        const size_t index = static_cast<size_t>(world_.run.expeditionServicePage_ * 3 + option);
        if (index >= world_.run.expeditionServiceOffers_.size())
            return;
        const auto id = world_.run.expeditionServiceOffers_[index];
        if (node->kind == NK::Upgrade || node->kind == NK::Evolution) {
            const auto* upgrade = tankcontent::FindUpgrade(world_.run.expeditionContent_, id);
            if (!upgrade)
                return;
            if (!tankcontent::EligibleUpgrade(*upgrade, world_.run.expeditionBuildStyle_, world_.run.tankRun_.GetCardCounts())) {
                world_.run.expeditionMapStatus_ = "このスタイルには装備できない強化です。";
                reject();
                return;
            }
            const int price = ExpeditionServicePrice(id);
            if (!world_.run.expeditionMapRun_.CanAfford(price)) {
                world_.run.expeditionMapStatus_ = "通貨が足りません。別の強化を選ぶか次へ進めます。";
                reject();
                return;
            }
            const int hpBeforeUpgrade = world_.resources.player_->GetHp(), maxHpBeforeUpgrade = world_.resources.player_->GetMaxHp();
            const int creditsBeforeUpgrade = world_.run.expeditionMapRun_.GetCurrency();
            const auto cardsBeforeUpgrade = world_.run.tankRun_.GetCardCounts();
            const auto classBeforeUpgrade = world_.resources.player_->GetCurrentClassName();
            if (!world_.run.tankRun_.GrantExpeditionModules(upgrade->effects)) {
                world_.run.expeditionMapStatus_ = "この強化の効果はすでにすべて装備済みです。";
                return;
            }
            world_.run.expeditionMapRun_.TrySpendCurrency(price);
            ++world_.run.expeditionPurchases_[id];
            ++world_.run.expeditionMapTestPurchases_;
            world_.run.expeditionPurchasedModules_[id] = *upgrade;
            world_.arenaRunController->ApplyTankRunCards();
            if (std::find(upgrade->effects.begin(), upgrade->effects.end(), tankrun::CardId::Repair) != upgrade->effects.end()) {
                const int desiredHp = (std::min)(world_.resources.player_->GetMaxHp(),
                                                 hpBeforeUpgrade + world_.resources.player_->GetMaxHp() - maxHpBeforeUpgrade);
                world_.resources.player_->HealRunPlayer((std::max)(0, desiredHp - world_.resources.player_->GetHp()));
                world_.combat.previousPlayerHp_ = world_.resources.player_->GetHp();
            }
            if (world_.validation.experienceValidationVariant_ || world_.run.expeditionMapAutoTest_) {
                bool preserved = creditsBeforeUpgrade - price == world_.run.expeditionMapRun_.GetCurrency() &&
                                 classBeforeUpgrade == world_.resources.player_->GetCurrentClassName();
                for (std::size_t c = 0; c < cardsBeforeUpgrade.size(); ++c)
                    preserved &= world_.run.tankRun_.GetCardCounts()[c] >= cardsBeforeUpgrade[c];
                for (const auto effect : upgrade->effects)
                    preserved &= world_.run.tankRun_.GetCardCount(effect) == 1;
                if (std::find(upgrade->effects.begin(), upgrade->effects.end(), tankrun::CardId::Repair) == upgrade->effects.end())
                    preserved &=
                        hpBeforeUpgrade == world_.resources.player_->GetHp() && maxHpBeforeUpgrade == world_.resources.player_->GetMaxHp();
                const bool additive = std::any_of(upgrade->effects.begin(), upgrade->effects.end(), [](auto effect) {
                    return effect >= tankrun::CardId::ExtraBarrel1;
                });
                if (additive && preserved) {
                    world_.validation.experienceEvolutionVerified_ = true;
                    ++world_.run.expeditionMapTestEvolutions_;
                }
                if (!preserved && world_.validation.experienceValidationVariant_)
                    world_.validation.experienceValidationErrors_.push_back(
                        "Upgrade replaced class, lost modules, changed health or charged incorrect currency");
            }
            world_.run.tankExpeditionTutorial_.RecordUpgrade();
            world_.run.expeditionMapStatus_ = upgrade->name + "を装備しました";
            complete = world_.run.expeditionMapRun_.CompleteService(false);
        }
    }
    if (complete) {
        if (world_.run.expeditionGuideActive_ && tankexp::IsIntroUpgrade(node->role)) {
            world_.run.expeditionGuide_.ResolveUpgrade(option != 3);
            if (world_.run.expeditionGuide_.IsComplete())
                world_.expeditionController->SaveExpeditionTutorialCompletion();
        }
        if (tankexp::IsIntroUpgrade(node->role) && !world_.run.expeditionBuildChosen_)
            world_.run.expeditionBuildChoice_ = true;
        world_.run.tankExpeditionAudio_.Upgrade();
        world_.run.tankRunSelection_ = 0;
        world_.run.tankRunMenuAge_ = 0;
        const auto next = world_.run.expeditionMapRun_.GetAvailableNodeIds();
        if (!next.empty())
            world_.run.expeditionMapSelection_ = next.front();
        if (const auto* focus = tankexp::FindMapNode(world_.run.expeditionMapRun_.GetDefinition(), world_.run.expeditionMapSelection_))
            world_.run.expeditionMapScroll_ = (std::max)(0.0f, 120.0f * (focus->column - 8));
        world_.run.expeditionTransitionTitle_->SetText(option == 3              ? "次の地点へ"
                                                       : node->kind == NK::Heal ? "修理完了"
                                                                                : "強化を装備しました");
        world_.run.expeditionTransitionDetail_->SetText(option == 3 ? "残りの通貨を持って先へ進みます"
                                                                    : WrapMapText(world_.run.expeditionMapStatus_, 46, 2));
        world_.run.expeditionTransitionTitle_->PrepareForDraw();
        world_.run.expeditionTransitionDetail_->PrepareForDraw();
    }
    world_.expeditionController->RefreshTankExpeditionUi();
}

void ExpeditionMapController::RefreshExpeditionMapUi()
{
    // The hidden map must not overwrite the pause menu's focus or emit hover
    // audio. Both screens share expeditionLastFocus_; only the visible one owns it.
    if (!world_.run.expeditionMapTitle_ || world_.run.tankRunPaused_)
        return;
    if (world_.run.expeditionBuildChoice_ && !world_.run.tankRunPaused_) {
        world_.run.expeditionMapTitle_->SetText("戦闘スタイルを選ぼう");
        world_.run.expeditionMapTitle_->PrepareForDraw();
        world_.run.expeditionMapSubtitle_->SetText("カーソルを合わせると攻撃方法を確認できます。");
        world_.run.expeditionMapSubtitle_->PrepareForDraw();
        world_.run.tankRunDescription_->SetText("選んだスタイルに合う強化が工房に並びます。");
        world_.run.tankRunDescription_->SetPosition({64, 209});
        world_.run.tankRunDescription_->PrepareForDraw();
        world_.expeditionBuildSelection->RefreshExpeditionBuildCards();
        return;
    }
    const auto& definition = world_.run.expeditionMapRun_.GetDefinition();
    const auto* active = world_.run.expeditionMapRun_.GetActiveNode();
    const bool service = active && !tankexp::IsCombatNode(active->kind) && !world_.run.expeditionMapPreview_ && !world_.run.tankRunPaused_;
    world_.run.expeditionMapTitle_->SetText(service ? NodeName(active->kind) : " ");
    world_.run.expeditionMapSubtitle_->SetText(" ");
    world_.run.expeditionMapHelp_->SetText(" ");
    world_.run.expeditionMapLegend_->SetPosition({46, 111});
    world_.run.expeditionMapLegend_->SetText("戦  戦闘     宝  宝物庫     改  工房     +  修理     核  最終決戦");
    if (service) {
        world_.run.tankRunDescription_->SetPosition({64, 209});
        world_.run.tankRunDescription_->SetText(
            active->kind == NK::Currency                  ? "操作を学ぶルートと同じ通貨を受け取っています。次の工房で同じ強化を選べます。"
            : active->kind == NK::Heal                    ? "次の戦いに備えて、装甲を修理できます。"
            : world_.run.expeditionServiceOffers_.empty() ? "ここで購入できる強化はありません。次の地点へ進めます。"
                                                          : "強化を1つ購入できます。購入せず進むこともできます。");
        if (world_.run.expeditionGuideActive_ && active->role == tankexp::NodeRole::TutorialUpgrade &&
            !world_.run.expeditionGuide_.IsComplete())
            world_.run.tankRunDescription_->SetText(" ");
        if (active->kind == NK::Heal) {
            const bool full = world_.resources.player_->GetHp() >= world_.resources.player_->GetMaxHp(),
                       affordable = world_.run.expeditionMapRun_.CanAfford(active->serviceCost);
            world_.run.tankRunCardTitles_[0]->SetText("装甲を修理");
            world_.run.tankRunCardBodies_[0]->SetText("最大HPの50%を回復\n価格        " + std::to_string(active->serviceCost) +
                                                      "\n\n現在HP  " + std::to_string(world_.resources.player_->GetHp()) + " / " +
                                                      std::to_string(world_.resources.player_->GetMaxHp()) + "\n\n" +
                                                      (full          ? "HPが満タンのため修理できません"
                                                       : !affordable ? "通貨が足りないため修理できません"
                                                                     : "左クリックで修理"));
        }
        const bool repair = active->kind == NK::Heal;
        world_.run.expeditionSkipButton_->SetPosition(repair ? cg2::Vector2{490, 560} : cg2::Vector2{966, 620});
        world_.run.expeditionSkipButton_->SetSize(repair ? cg2::Vector2{300, 52} : cg2::Vector2{242, 44});
        world_.run.expeditionSkipText_->SetPosition(repair ? cg2::Vector2{640, 574} : cg2::Vector2{1087, 629});
        world_.run.expeditionSkipText_->SetText(repair ? "修理せず進む" : "購入せず進む");
        const auto mouse = world_.resources.input_->GetMousePosition();
        const bool skipFocus = world_.run.tankRunSelection_ == 3 || Inside(mouse, repair ? 490.0f : 966.0f, repair ? 560.0f : 620.0f,
                                                                           repair ? 300.0f : 242.0f, repair ? 52.0f : 44.0f);
        world_.run.expeditionSkipButton_->SetColor(skipFocus ? cg2::Vector4{0.065f, 0.19f, 0.22f, 1}
                                                             : cg2::Vector4{0.045f, 0.09f, 0.13f, 1});
        world_.run.expeditionSkipButton_->Update();
        world_.run.expeditionSkipText_->PrepareForDraw();
        world_.run.expeditionMapInfo_->SetPosition({46, 674});
        world_.run.expeditionMapInfo_->SetText(WrapMapText(world_.run.expeditionMapStatus_, 60, 1));
    } else {
        world_.run.expeditionMapInfo_->SetPosition({46, 586});
        const auto* selected = tankexp::FindMapNode(definition, world_.run.expeditionMapSelection_);
        std::string info;
        if (selected) {
            info = "       ";
            info += tankexp::IsCombatNode(selected->kind) ? std::to_string(selected->clearReward) + "  / " + NodeName(selected->kind) +
                                                                (selected->kind == NK::Elite ? "：通貨を多く獲得" : "のクリア報酬")
                    : selected->kind == NK::Upgrade || selected->kind == NK::Evolution
                        ? std::to_string(selected->serviceCost) + "〜  / 強化工房"
                    : selected->kind == NK::Currency ? std::to_string(selected->clearReward) + "  / 通貨を受け取る"
                                                     : std::to_string(selected->serviceCost) + "  / HPを50%修理";
            if (!world_.run.expeditionMapRun_.CanSelectNode(selected->id) && !world_.run.expeditionMapPreview_)
                info += "   / まだ選択できません";
        }
        world_.run.expeditionMapInfo_->SetText(info);
        world_.run.expeditionMapHelp_->SetPosition({218, 630});
        world_.run.expeditionMapHelp_->SetText(WrapMapText(world_.run.expeditionMapStatus_, 48, 1));
    }
    auto position = [this](const tankexp::MapNode& n) {
        return cg2::Vector2{90 + 120.0f * n.column - world_.run.expeditionMapScroll_, 186 + 84.0f * n.row};
    };
    const auto available = world_.run.expeditionMapRun_.GetAvailableNodeIds();
    world_.expeditionExperience->EnsureExpeditionPointers((std::max)(size_t{6}, available.size() * 6));
    const auto* current = tankexp::FindMapNode(definition, world_.run.expeditionMapRun_.GetCurrentNodeId());
    std::set<std::string> future;
    std::vector<std::string> pending = available;
    if (active)
        pending = active->next;
    while (!pending.empty()) {
        auto id = pending.back();
        pending.pop_back();
        if (!future.insert(id).second)
            continue;
        if (const auto* n = tankexp::FindMapNode(definition, id))
            pending.insert(pending.end(), n->next.begin(), n->next.end());
    }
    for (size_t i = 0; i < definition.nodes.size() && i < world_.run.expeditionMapVisuals_.size(); ++i) {
        const auto& n = definition.nodes[i];
        auto& v = world_.run.expeditionMapVisuals_[i];
        v.center = position(n);
        const bool reachable = world_.run.expeditionMapRun_.CanSelectNode(n.id), visited = world_.run.expeditionMapRun_.HasVisited(n.id);
        const bool selected = n.id == world_.run.expeditionMapSelection_;
        const bool intro = n.role == tankexp::NodeRole::TutorialCombat || n.role == tankexp::NodeRole::TutorialSkip;
        const bool learning = reachable && n.role == tankexp::NodeRole::TutorialCombat;
        const bool abandoned = !visited && !reachable && !future.contains(n.id) && current;
        auto tint = visited ? cg2::Vector4{0.25f, 1.35f, 0.62f, 1} : NodeColor(n.kind);
        tint.w = abandoned ? 0.09f : reachable || selected ? 1.0f : visited ? 0.9f : 0.35f;
        const float pulse = 0.5f + 0.5f * std::sin(world_.run.expeditionPresentationClock_ * 3.8f - static_cast<float>(i) * 0.4f);
        const float focus = v.focus;
        if (selected && world_.run.expeditionUiErrorAge_ > 0)
            tint = {1, 0.25f, 0.25f, 1};
        v.rim->SetColor(tint);
        auto halo = tint;
        halo.w = abandoned ? 0 : visited ? 0.18f : 0.02f + focus * 0.23f + (reachable ? 0.08f * pulse : 0) + (learning ? 0.06f * pulse : 0);
        v.halo->SetColor(halo);
        v.halo->SetSize({55 + focus * 10 + pulse * (learning ? 5.0f : 3.0f), 55 + focus * 10 + pulse * (learning ? 5.0f : 3.0f)});
        v.rim->SetSize({46 + focus * 5, 46 + focus * 5});
        v.fill->SetSize({42 + focus * 5, 42 + focus * 5});
        v.fill->SetColor(visited ? cg2::Vector4{0.03f, 0.14f, 0.16f, 1} : cg2::Vector4{0.016f, 0.040f, 0.065f, 1});
        for (auto* s : {v.halo.get(), v.rim.get(), v.fill.get()}) {
            s->SetPosition(v.center);
            s->Update();
        }
        auto style = v.icon->GetStyle();
        style.color = visited ? cg2::Vector4{0.3f, 1.4f, 0.65f, 1} : NodeColor(n.kind);
        v.icon->SetStyle(style);
        v.icon->SetText(visited ? "完" : NodeIcon(n.kind));
        v.icon->SetPosition({v.center.x, v.center.y - 2});
        v.icon->SetAlpha(abandoned ? 0.13f : reachable || visited || selected ? 1.0f : 0.48f);
        v.label->SetText(n.role == tankexp::NodeRole::TutorialCombat ? "操作を学ぶ"
                         : n.role == tankexp::NodeRole::TutorialSkip ? "説明をスキップ"
                                                                     : " ");
        // Reuse each node's existing labels; keep both introductory captions in
        // the visible map even at the left edge without allocating new UI.
        const float captionX = intro ? (std::max)(112.0f, v.center.x) : v.center.x;
        v.label->SetPosition({captionX, v.center.y + 39});
        v.label->SetAlpha(abandoned ? 0.2f : reachable && intro ? 1.0f : 0.85f);
        v.state->SetText(n.role == tankexp::NodeRole::TutorialCombat ? "初回プレイにおすすめ"
                         : n.role == tankexp::NodeRole::TutorialSkip ? "操作を知っている人向け"
                                                                     : " ");
        v.state->SetPosition({captionX, v.center.y + 60});
        v.state->SetAlpha(abandoned ? 0.2f : reachable ? 0.95f : 0.48f);
        v.icon->PrepareForDraw();
        v.label->PrepareForDraw();
        v.state->PrepareForDraw();
    }
    for (auto& edge : world_.run.expeditionMapEdges_) {
        auto a = position(*tankexp::FindMapNode(definition, edge.from)), b = position(*tankexp::FindMapNode(definition, edge.to));
        const float dx = b.x - a.x, dy = b.y - a.y;
        const float from = (std::clamp)((36 - a.x) / dx, 0.0f, 1.0f), to = (std::clamp)((1244 - a.x) / dx, 0.0f, 1.0f);
        b = {a.x + dx * to, a.y + dy * to};
        a = {a.x + dx * from, a.y + dy * from};
        const float length = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
        const bool chosen = world_.run.expeditionMapRun_.HasVisited(edge.from) &&
                            (world_.run.expeditionMapRun_.HasVisited(edge.to) || world_.run.expeditionMapRun_.CanSelectNode(edge.to));
        for (auto* s : {edge.glow.get(), edge.line.get()}) {
            s->SetPosition(a);
            s->SetSize({length, s == edge.line.get() ? 2.0f : 7.0f});
            s->SetRotation(std::atan2(b.y - a.y, b.x - a.x));
            s->Update();
        }
        const bool focusPath = edge.to == world_.run.expeditionMapSelection_ || edge.from == world_.run.expeditionMapSelection_;
        const bool abandoned =
            current && !chosen && (!future.contains(edge.to) || (!future.contains(edge.from) && edge.from != current->id));
        edge.line->SetColor(abandoned   ? cg2::Vector4{0.16f, 0.24f, 0.30f, 0.10f}
                            : chosen    ? cg2::Vector4{0.2f, 1.1f, 0.62f, 0.85f}
                            : focusPath ? cg2::Vector4{0.25f, 0.86f, 0.85f, 0.9f}
                                        : cg2::Vector4{0.24f, 0.42f, 0.56f, 0.35f});
        edge.glow->SetColor({0.14f, 0.70f, 0.82f, chosen || focusPath ? 0.15f : 0.03f});
        const float travel = std::fmod(world_.run.expeditionPresentationClock_ * 0.60f, 1.0f);
        edge.pulse->SetPosition({a.x + (b.x - a.x) * travel, a.y + (b.y - a.y) * travel});
        edge.pulse->SetColor({0.45f, 1, 1, (chosen || focusPath) && length > 1 ? 0.85f : 0});
        edge.pulse->Update();
    }
    for (int i = 0; i < 3; ++i) {
        world_.run.expeditionBlueprintLabels_[i]->SetText(i == 0 ? "←" : i == 1 ? "→" : " ");
        world_.run.expeditionBlueprintButtons_[i]->SetPosition({i == 0 ? 46.0f : 130.0f, 620});
        world_.run.expeditionBlueprintButtons_[i]->SetSize({68, 44});
        world_.run.expeditionBlueprintButtons_[i]->Update();
        world_.run.expeditionBlueprintLabels_[i]->SetPosition({i == 0 ? 68.0f : 152.0f, 628});
        world_.run.expeditionBlueprintButtons_[i]->SetColor(
            Inside(world_.resources.input_->GetMousePosition(), i == 0 ? 46.0f : 130.0f, 620, 68, 44)
                ? cg2::Vector4{0.04f, 0.21f, 0.24f, 1}
                : cg2::Vector4{0.025f, 0.07f, 0.11f, 1});
        world_.run.expeditionBlueprintLabels_[i]->PrepareForDraw();
    }
    for (auto* t : {world_.run.expeditionMapTitle_.get(), world_.run.expeditionMapSubtitle_.get(), world_.run.expeditionMapLegend_.get(),
                    world_.run.expeditionMapInfo_.get(), world_.run.expeditionMapHelp_.get()})
        t->PrepareForDraw();
    if (service) {
        world_.run.tankRunDescription_->PrepareForDraw();
        if (active->kind == NK::Heal) {
            world_.run.tankRunCardTitles_[0]->PrepareForDraw();
            world_.run.tankRunCardBodies_[0]->PrepareForDraw();
        }
    }
    std::string focus =
        service ? active->id + ":" + std::to_string(world_.run.expeditionServicePage_) + ":" + std::to_string(world_.run.tankRunSelection_)
                : world_.run.expeditionMapSelection_;
    if (!service || world_.run.expeditionServiceOffers_.size() > 3)
        for (int i = 0; i < 2; ++i)
            if (Inside(world_.resources.input_->GetMousePosition(), i == 0 ? 46.0f : 130.0f, 620, 68, 44))
                focus = "scroll:" + std::to_string(i);
    if (focus != world_.run.expeditionLastFocus_) {
        if (!world_.run.expeditionLastFocus_.empty() && !world_.run.expeditionTransition_.IsActive())
            world_.run.tankExpeditionAudio_.UiHover();
        world_.run.expeditionLastFocus_ = focus;
    }
    world_.expeditionBuildSelection->RefreshExpeditionBuildCards();
}

void ExpeditionMapController::DrawExpeditionMapUi()
{
    const auto cardPresent = [this](int i) {
        const size_t index = static_cast<size_t>(world_.run.expeditionServicePage_ * 3 + i);
        return world_.run.expeditionBuildChoice_ ||
               (index < world_.run.expeditionServiceOffers_.size() &&
                tankcontent::FindUpgrade(world_.run.expeditionContent_, world_.run.expeditionServiceOffers_[index]));
    };
    if (world_.expeditionBuildSelection->IsExpeditionBuildCardScreen())
        for (int i = 0; i < 3; ++i)
            if (cardPresent(i) && world_.run.expeditionRewardCards_[i])
                world_.run.expeditionRewardCards_[i]->PreparePreviewRender();
    cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
    world_.run.tankRunDimmer_->Draw();
    world_.run.tankRunHudPanel_->Draw();
    world_.run.tankRunHud_->Draw();
    world_.run.tankExpeditionHpTrack_->Draw();
    world_.run.tankExpeditionHpFill_->Draw();
    world_.expeditionExperience->DrawExpeditionVitals();
    world_.run.expeditionMapTitle_->Draw();
    world_.run.expeditionMapSubtitle_->Draw();
    if (world_.run.expeditionBuildChoice_) {
        world_.run.tankRunDescription_->Draw();
        for (auto& card : world_.run.expeditionRewardCards_)
            card->Draw();
        return;
    }
    const auto* active = world_.run.expeditionMapRun_.GetActiveNode();
    const bool service = active && !tankexp::IsCombatNode(active->kind) && !world_.run.expeditionMapPreview_;
    if (service) {
        world_.run.tankRunDescription_->Draw();
        if (active->kind == NK::Currency)
            return;
        if (world_.expeditionBuildSelection->IsExpeditionBuildCardScreen()) {
            for (int i = 0; i < 3; ++i)
                if (cardPresent(i))
                    world_.run.expeditionRewardCards_[i]->Draw();
        } else if (active->kind == NK::Heal) {
            const bool enabled = world_.resources.player_->GetHp() < world_.resources.player_->GetMaxHp() &&
                                 world_.run.expeditionMapRun_.CanAfford(active->serviceCost);
            const float focus = enabled ? world_.run.expeditionCardFocus_[0] : 0;
            world_.run.tankRunCards_[0]->SetPosition({424, 270});
            world_.run.tankRunCards_[0]->SetSize({432, 260});
            world_.run.tankRunCards_[0]->SetColor(
                enabled ? cg2::Vector4{0.028f + 0.012f * focus, 0.065f + 0.105f * focus, 0.075f + 0.105f * focus, 1}
                        : cg2::Vector4{0.025f, 0.035f, 0.045f, 1});
            world_.run.tankRunCardTitles_[0]->SetPosition({446, 292});
            world_.run.tankRunCardBodies_[0]->SetPosition({446, 338});
            world_.run.tankRunCards_[0]->Update();
            world_.run.tankRunCards_[0]->Draw();
            world_.run.tankRunCardTitles_[0]->Draw();
            world_.run.tankRunCardBodies_[0]->Draw();
            world_.expeditionExperience->DrawCurrencyIcon({506, 383}, 28);
            // These UI objects also serve the pause/result screens. Restore
            // their normal layout immediately after the dedicated repair draw.
            world_.run.tankRunCards_[0]->SetPosition({64, 280});
            world_.run.tankRunCards_[0]->SetSize({368, 280});
            world_.run.tankRunCardTitles_[0]->SetPosition({80, 303});
            world_.run.tankRunCardBodies_[0]->SetPosition({80, 362});
        }
        if (world_.run.expeditionServiceOffers_.size() > 3)
            for (int i = 0; i < 2; ++i) {
                world_.run.expeditionBlueprintButtons_[i]->Draw();
                world_.run.expeditionBlueprintLabels_[i]->Draw();
            }
        if (active->kind != NK::Currency) {
            world_.run.expeditionSkipButton_->Draw();
            world_.run.expeditionSkipText_->Draw();
        }
    } else {
        for (auto& s : world_.run.expeditionMapGrid_)
            s->Draw();
        for (auto& e : world_.run.expeditionMapEdges_) {
            e.glow->Draw();
            e.line->Draw();
            e.pulse->Draw();
        }
        std::vector<cg2::TextLabel*> completed;
        const auto& nodes = world_.run.expeditionMapRun_.GetDefinition().nodes;
        for (size_t i = 0; i < world_.run.expeditionMapVisuals_.size(); ++i) {
            const auto& v = world_.run.expeditionMapVisuals_[i];
            if (v.center.x >= 66 && v.center.x <= 1214 && world_.run.expeditionMapRun_.HasVisited(nodes[i].id))
                completed.push_back(v.icon.get());
        }
        world_.run.expeditionCompleteGlow_->DrawBloom(completed);
        cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
        for (auto& n : world_.run.expeditionMapVisuals_)
            if (n.center.x >= 66 && n.center.x <= 1214) {
                n.halo->Draw();
                n.rim->Draw();
                n.fill->Draw();
                n.icon->Draw();
                n.label->Draw();
                n.state->Draw();
            }
        world_.run.expeditionMapLegend_->Draw();
        for (int i = 0; i < 2; ++i) {
            world_.run.expeditionBlueprintButtons_[i]->Draw();
            world_.run.expeditionBlueprintLabels_[i]->Draw();
        }
        const auto drawPointer = [this, &nodes]() {
            for (size_t i = 0; i < world_.run.expeditionMapVisuals_.size(); ++i)
                if (world_.run.expeditionMapRun_.CanSelectNode(nodes[i].id)) {
                    const auto p = world_.run.expeditionMapVisuals_[i].center;
                    if (p.x >= 66 && p.x <= 1214) {
                        world_.expeditionExperience->DrawExpeditionPointer({p.x - 35, p.y});
                    }
                }
            return false;
        };
        drawPointer();
        world_.expeditionExperience->DrawCurrencyIcon({58, 604});
    }
    world_.run.expeditionMapInfo_->Draw();
    world_.run.expeditionMapHelp_->Draw();
}

void ExpeditionMapController::UpdateExpeditionMap(float dt)
{
    if (world_.combat.phase_ != Phase::kMain)
        return;
    if (world_.specialValidation->UpdateSpecialValidation(dt))
        return;
    if (world_.experienceValidation->UpdateExperienceValidation(dt))
        return;
    if (world_.combatValidation->UpdateCombatValidation(dt))
        return;
    const int earnings = world_.resources.player_->TakeRunCurrencyEarned();
    if (earnings > 0)
        world_.expeditionExperience->SpawnExpeditionCredits(world_.resources.player_->GetWorldPosition(), earnings);
    if (world_.run.expeditionAuthoringHubOpen_ || world_.run.tankExpeditionBalanceEditorOpen_ || world_.run.expeditionRoomEditorOpen_ ||
        world_.run.expeditionMapEditorOpen_ || world_.run.expeditionContentEditorOpen_)
        return;
    const bool wasTransitioning = world_.run.expeditionTransition_.IsActive();
    UpdateExpeditionPresentation(dt);
    if (wasTransitioning) {
        world_.expeditionController->RefreshTankExpeditionUi();
        return;
    }
    world_.run.tankRunMenuAge_ += dt;
    world_.run.tankRunAutoTime_ += dt;
    if (world_.run.expeditionMapAutoTest_)
        UpdateExpeditionMapValidation(dt);
    if (cg2::kDeveloperTools && Press(world_.resources.input_, DIK_F10))
        world_.arenaRunController->RequestTankRunCapture("map_manual");
    if (Press(world_.resources.input_, DIK_M)) {
        world_.run.tankExpeditionMusicEnabled_ = !world_.run.tankExpeditionMusicEnabled_;
        world_.run.tankExpeditionAudio_.SetMusicVolume(world_.run.tankExpeditionMusicEnabled_ ? 0.55f : 0);
    }
    if (Press(world_.resources.input_, DIK_N)) {
        world_.run.tankExpeditionEffectsEnabled_ = !world_.run.tankExpeditionEffectsEnabled_;
        world_.run.tankExpeditionAudio_.SetEffectsVolume(world_.run.tankExpeditionEffectsEnabled_ ? 0.8f : 0);
    }
    if (world_.combat.combatFlow_.GetState() != GameFlowState::Playing) {
        world_.expeditionExperience->UpdateExpeditionCredits(dt, true);
        world_.expeditionController->RefreshTankExpeditionUi();
        return;
    }
    if (world_.run.tankExpedition_.IsCombat() && !world_.run.tankRunPaused_) {
        if (Press(world_.resources.input_, DIK_G)) {
            world_.run.expeditionMapPreview_ = !world_.run.expeditionMapPreview_;
            world_.run.tankExpeditionDetailsOpen_ = false;
            world_.expeditionController->RefreshTankExpeditionUi();
        }
        if (Press(world_.resources.input_, DIK_TAB)) {
            world_.run.tankExpeditionDetailsOpen_ = !world_.run.tankExpeditionDetailsOpen_;
            world_.run.expeditionMapPreview_ = false;
            world_.expeditionController->RefreshTankExpeditionUi();
        }
    }
    if (Press(world_.resources.input_, DIK_ESCAPE)) {
        if (world_.run.expeditionMapPreview_ || world_.run.tankExpeditionDetailsOpen_) {
            world_.run.expeditionMapPreview_ = false;
            world_.run.tankExpeditionDetailsOpen_ = false;
            return;
        }
        world_.run.tankRunPaused_ = !world_.run.tankRunPaused_;
        world_.run.tankRunSelection_ = 0;
        world_.run.tankRunMenuAge_ = 0;
        world_.run.expeditionLastFocus_.clear();
        world_.expeditionController->RefreshTankExpeditionUi();
        return;
    }
    const auto mouse = world_.resources.input_->GetMousePosition();
    const auto motion = world_.resources.input_->GetMouseState();
    const bool click = world_.resources.input_->IsTrigger(motion.rgbButtons[0], world_.resources.input_->GetPreMouseState().rgbButtons[0]);
    if (world_.run.tankRunPaused_) {
        int hovered = -1;
        for (int i = 0; i < 2; ++i)
            if (Inside(mouse, 64 + i * 388.0f, 280, 368, 280))
                hovered = i;
        const std::string focus = "pause:" + std::to_string(hovered);
        if (focus != world_.run.expeditionLastFocus_) {
            if (hovered >= 0) {
                world_.run.tankRunSelection_ = hovered;
                world_.run.tankExpeditionAudio_.UiHover();
            }
            world_.run.expeditionLastFocus_ = focus;
        }
        if (hovered >= 0 && (motion.lX || motion.lY || click))
            world_.run.tankRunSelection_ = hovered;
        if (Press(world_.resources.input_, DIK_LEFT) || Press(world_.resources.input_, DIK_RIGHT)) {
            world_.run.tankRunSelection_ = 1 - world_.run.tankRunSelection_;
            world_.run.tankExpeditionAudio_.UiHover();
        }
        for (int i = 0; i < 2; ++i)
            if (Press(world_.resources.input_, DIK_1 + i) || (click && Inside(mouse, 64 + i * 388.0f, 280, 368, 280))) {
                world_.arenaRunController->SelectTankRunOption(i);
                return;
            }
        if (Press(world_.resources.input_, DIK_RETURN))
            world_.arenaRunController->SelectTankRunOption(world_.run.tankRunSelection_);
        world_.expeditionController->RefreshTankExpeditionUi();
        return;
    }
    if (world_.run.tankExpeditionDetailsOpen_)
        return;
    world_.expeditionExperience->UpdateGuidedExpedition(dt);
    world_.expeditionExperience->UpdateExpeditionCredits(dt, world_.run.expeditionCollectAll_);
    if (world_.expeditionExperience->IsGuidedExpeditionPaused()) {
        world_.expeditionController->RefreshTankExpeditionUi();
        return;
    }
    if (world_.run.expeditionBuildChoice_) {
        if (world_.run.tankRunMenuAge_ > 0.2f) {
            if (Press(world_.resources.input_, DIK_LEFT))
                world_.run.tankRunSelection_ = (world_.run.tankRunSelection_ + 2) % 3;
            if (Press(world_.resources.input_, DIK_RIGHT))
                world_.run.tankRunSelection_ = (world_.run.tankRunSelection_ + 1) % 3;
            for (int i = 0; i < 3; ++i) {
                if (Inside(mouse, 64 + i * 388.0f, 260, 368, 330) && (motion.lX || motion.lY))
                    world_.run.tankRunSelection_ = i;
                if (Press(world_.resources.input_, DIK_1 + i) || (click && Inside(mouse, 64 + i * 388.0f, 260, 368, 330))) {
                    world_.expeditionBuildSelection->SelectExpeditionBuildStyle(i);
                    return;
                }
            }
            if (Press(world_.resources.input_, DIK_RETURN) || Press(world_.resources.input_, DIK_SPACE)) {
                world_.expeditionBuildSelection->SelectExpeditionBuildStyle(world_.run.tankRunSelection_);
                return;
            }
        }
        world_.expeditionController->RefreshTankExpeditionUi();
        return;
    }
    if (world_.run.expeditionMapPreview_ || world_.run.expeditionMapRun_.IsChoosing()) {
        const auto available = world_.run.expeditionMapRun_.GetAvailableNodeIds();
        if (!world_.run.expeditionMapPreview_ && world_.run.tankRunMenuAge_ > 0.15f) {
            if ((Press(world_.resources.input_, DIK_LEFT) || Press(world_.resources.input_, DIK_RIGHT)) && !available.empty()) {
                const auto it = std::find(available.begin(), available.end(), world_.run.expeditionMapSelection_);
                int i = it == available.end() ? 0 : static_cast<int>(std::distance(available.begin(), it));
                i = (i + (Press(world_.resources.input_, DIK_LEFT) ? static_cast<int>(available.size()) - 1 : 1)) %
                    static_cast<int>(available.size());
                world_.run.expeditionMapSelection_ = available[i];
                const auto* n = tankexp::FindMapNode(world_.run.expeditionMapRun_.GetDefinition(), world_.run.expeditionMapSelection_);
                world_.run.expeditionMapScroll_ = (std::max)(0.0f, 120.0f * (n->column - 8));
            }
            bool enter = Press(world_.resources.input_, DIK_RETURN) || Press(world_.resources.input_, DIK_SPACE);
            for (size_t i = 0; i < available.size(); ++i)
                if (Press(world_.resources.input_, DIK_1 + static_cast<int>(i))) {
                    world_.run.expeditionMapSelection_ = available[i];
                    enter = true;
                }
            for (size_t i = 0; i < world_.run.expeditionMapVisuals_.size(); ++i) {
                const auto p = world_.run.expeditionMapVisuals_[i].center;
                if (Inside(mouse, p.x - 34, p.y - 34, 68, 68) && p.x >= 66 && p.x <= 1214) {
                    if (motion.lX || motion.lY || click)
                        world_.run.expeditionMapSelection_ = world_.run.expeditionMapRun_.GetDefinition().nodes[i].id;
                    if (click)
                        enter = true;
                }
            }
            if (enter) {
                RequestExpeditionMapNode(world_.run.expeditionMapSelection_);
                return;
            }
        }
        int lastColumn = 0;
        for (const auto& n : world_.run.expeditionMapRun_.GetDefinition().nodes)
            lastColumn = (std::max)(lastColumn, n.column);
        if (Press(world_.resources.input_, DIK_Q) || (click && Inside(mouse, 46, 620, 68, 44))) {
            world_.run.expeditionMapScroll_ -= 360;
            world_.run.tankExpeditionAudio_.UiConfirm();
        }
        if (Press(world_.resources.input_, DIK_E) || (click && Inside(mouse, 130, 620, 68, 44))) {
            world_.run.expeditionMapScroll_ += 360;
            world_.run.tankExpeditionAudio_.UiConfirm();
        }
        if (motion.lZ)
            world_.run.expeditionMapScroll_ -= static_cast<float>(motion.lZ);
        world_.run.expeditionMapScroll_ = (std::clamp)(world_.run.expeditionMapScroll_, 0.0f, (std::max)(0.0f, 120.0f * (lastColumn - 9)));
        if (!world_.run.expeditionMapPreview_)
            world_.expeditionController->UpdateTankExpeditionTutorial(dt);
        world_.expeditionController->RefreshTankExpeditionUi();
        return;
    }
    const auto* active = world_.run.expeditionMapRun_.GetActiveNode();
    if (active && !tankexp::IsCombatNode(active->kind)) {
        if (active->kind == NK::Currency) {
            if (world_.run.expeditionCredits_.empty()) {
                world_.run.expeditionMapRun_.CompleteCurrencyGrant(false);
                world_.run.expeditionCollectAll_ = false;
                const auto available = world_.run.expeditionMapRun_.GetAvailableNodeIds();
                if (!available.empty())
                    world_.run.expeditionMapSelection_ = available.front();
                world_.run.tankRunMenuAge_ = 0;
            }
            world_.expeditionController->RefreshTankExpeditionUi();
            return;
        }
        if (active->kind == NK::Heal) {
            if (world_.run.tankRunMenuAge_ > 0.15f) {
                if (Press(world_.resources.input_, DIK_LEFT) || Press(world_.resources.input_, DIK_RIGHT) ||
                    Press(world_.resources.input_, DIK_UP) || Press(world_.resources.input_, DIK_DOWN))
                    world_.run.tankRunSelection_ = world_.run.tankRunSelection_ == 0 ? 3 : 0;
                const bool repairHovered = Inside(mouse, 424, 270, 432, 260), skipHovered = Inside(mouse, 490, 560, 300, 52);
                if (motion.lX || motion.lY) {
                    if (repairHovered)
                        world_.run.tankRunSelection_ = 0;
                    else if (skipHovered)
                        world_.run.tankRunSelection_ = 3;
                }
                if (Press(world_.resources.input_, DIK_1) || (click && repairHovered)) {
                    SelectExpeditionService(0);
                    return;
                }
                if (Press(world_.resources.input_, DIK_2) || (click && skipHovered)) {
                    SelectExpeditionService(3);
                    return;
                }
                if (Press(world_.resources.input_, DIK_RETURN) || Press(world_.resources.input_, DIK_SPACE)) {
                    SelectExpeditionService(world_.run.tankRunSelection_ == 3 ? 3 : 0);
                    return;
                }
            }
            world_.expeditionController->RefreshTankExpeditionUi();
            return;
        }
        if (world_.run.tankRunMenuAge_ > 0.15f) {
            const int pages = (std::max)(1, (static_cast<int>(world_.run.expeditionServiceOffers_.size()) + 2) / 3);
            if (Press(world_.resources.input_, DIK_Q) || (click && Inside(mouse, 46, 620, 68, 44)))
                world_.run.expeditionServicePage_ = (world_.run.expeditionServicePage_ + pages - 1) % pages;
            if (Press(world_.resources.input_, DIK_E) || (click && Inside(mouse, 130, 620, 68, 44)))
                world_.run.expeditionServicePage_ = (world_.run.expeditionServicePage_ + 1) % pages;
            if (click && Inside(mouse, 966, 620, 242, 44)) {
                SelectExpeditionService(3);
                return;
            }
            std::vector<int> selectable;
            for (int i = 0; i < 3; ++i) {
                const size_t index = static_cast<size_t>(world_.run.expeditionServicePage_ * 3 + i);
                if (index >= world_.run.expeditionServiceOffers_.size() ||
                    !tankcontent::FindUpgrade(world_.run.expeditionContent_, world_.run.expeditionServiceOffers_[index]))
                    continue;
                selectable.push_back(i);
                if (Inside(mouse, 64 + i * 388.0f, 260, 368, 330) && (motion.lX || motion.lY))
                    world_.run.tankRunSelection_ = i;
                if (Press(world_.resources.input_, DIK_1 + i) || (click && Inside(mouse, 64 + i * 388.0f, 260, 368, 330))) {
                    SelectExpeditionService(i);
                    return;
                }
            }
            selectable.push_back(3);
            if (std::find(selectable.begin(), selectable.end(), world_.run.tankRunSelection_) == selectable.end())
                world_.run.tankRunSelection_ = selectable.front();
            if (Press(world_.resources.input_, DIK_LEFT) || Press(world_.resources.input_, DIK_RIGHT)) {
                const int selected =
                    static_cast<int>(std::find(selectable.begin(), selectable.end(), world_.run.tankRunSelection_) - selectable.begin());
                const int count = static_cast<int>(selectable.size());
                world_.run.tankRunSelection_ = selectable[(selected + (Press(world_.resources.input_, DIK_LEFT) ? count - 1 : 1)) % count];
            }
            if (Press(world_.resources.input_, DIK_RETURN) || Press(world_.resources.input_, DIK_SPACE)) {
                SelectExpeditionService(world_.run.tankRunSelection_);
                return;
            }
        }
        world_.expeditionController->UpdateTankExpeditionTutorial(dt);
        world_.expeditionController->RefreshTankExpeditionUi();
        return;
    }
    if (world_.run.tankExpedition_.IsCombat()) {
        world_.expeditionController->UpdateTankExpeditionTutorial(dt);
        const bool ready =
            !world_.run.expeditionGuideActive_ || world_.run.expeditionGuide_.IsCombatReadyToClear() || world_.run.expeditionMapAutoTest_;
        const auto kind = world_.run.tankExpedition_.GetRoomKind();
        if (ready && kind != tankexp::RoomKind::Boss && ThreatCount(world_.resources.enemyManager_.get()) == 0 &&
            (kind != tankexp::RoomKind::Guard || world_.run.tankExpeditionNodes_ >= 3)) {
            const auto* cleared = world_.run.expeditionMapRun_.GetActiveNode();
            // 完了演出より先に報酬通貨を一度だけ生成し、残る通貨の回収中は戦闘を停止する。
            if (!world_.run.expeditionClearRewardQueued_) {
                if (cleared)
                    world_.expeditionExperience->SpawnExpeditionCredits(world_.resources.player_->GetWorldPosition(), cleared->clearReward,
                                                                        true);
                world_.run.expeditionClearRewardQueued_ = true;
                world_.run.expeditionCollectAll_ = true;
            }
            if (!world_.run.expeditionCredits_.empty()) {
                world_.expeditionController->RefreshTankExpeditionUi();
                return;
            }
            world_.run.expeditionCollectAll_ = false;
            BeginExpeditionPresentation(2, "戦闘クリア", cleared ? "通貨を回収しました / 次の地点へ" : "次の目的地を選ぼう",
                                        {0.25f, 1, 0.72f, 1});
            return;
        }
        world_.run.tankExpedition_.Update(dt);
        world_.run.tankRun_.Update(dt);
        world_.run.tankExpeditionArrival_ += dt;
        world_.run.tankRunComboTime_ = (std::max)(0.0f, world_.run.tankRunComboTime_ - dt);
        for (auto& burst : world_.run.tankRunBursts_)
            burst.age += dt;
        std::erase_if(world_.run.tankRunBursts_, [](const RunBurst& b) {
            return b.age > (b.resource ? 0.7f : 0.35f);
        });
    }
    world_.run.tankRunHudTimer_ -= dt;
    if (world_.run.tankRunHudTimer_ <= 0) {
        world_.expeditionController->RefreshTankExpeditionUi();
        world_.run.tankRunHudTimer_ = 0.10f;
    }
}

void ExpeditionMapController::UpdateExpeditionMapValidation(float dt)
{
    world_.run.expeditionMapTestElapsed_ += dt;
    const std::string state = world_.run.expeditionMapRun_.IsChoosing() ? "map_" + world_.run.expeditionMapRun_.GetCurrentNodeId()
                                                                        : world_.run.expeditionMapRun_.GetActiveNodeId();
    if (state != world_.run.expeditionMapTestState_) {
        world_.run.expeditionMapTestState_ = state;
        world_.run.expeditionMapTestAge_ = 0;
    }
    world_.run.expeditionMapTestAge_ += dt;
    if (world_.run.expeditionMapTestAge_ > 0.4f &&
        std::find(world_.run.expeditionMapTestVisited_.begin(), world_.run.expeditionMapTestVisited_.end(), state) ==
            world_.run.expeditionMapTestVisited_.end() &&
        !state.empty()) {
        world_.run.expeditionMapTestVisited_.push_back(state);
        world_.run.tankRunCapturePath_ = "generated/expedition_map/" + state + ".png";
    }
    if (world_.combat.combatFlow_.GetState() == GameFlowState::StageClear || world_.run.expeditionMapTestElapsed_ > 140) {
        if (world_.combat.combatFlow_.GetState() == GameFlowState::StageClear && world_.run.expeditionMapTestAge_ < 1.0f)
            return;
        const bool success = world_.run.expeditionMapRun_.IsComplete() && world_.resources.player_->GetLevel() == 1 &&
                             world_.resources.player_->GetExp() == 0 && world_.run.expeditionMapTestPurchases_ > 0 &&
                             world_.run.expeditionMapTestEvolutions_ > 0 && world_.run.expeditionMapTestHeals_ > 0;
        nlohmann::json report = {{"completed", success},
                                 {"testMode", true},
                                 {"forcedCombatClear", true},
                                 {"elapsed", world_.run.expeditionMapTestElapsed_},
                                 {"visited", world_.run.expeditionMapRun_.GetVisitedNodeIds()},
                                 {"credits", world_.run.expeditionMapRun_.GetCurrency()},
                                 {"level", world_.resources.player_->GetLevel()},
                                 {"experience", world_.resources.player_->GetExp()},
                                 {"purchases", world_.run.expeditionMapTestPurchases_},
                                 {"refits", 0},
                                 {"evolutions", 0},
                                 {"additiveGrowthPurchases", world_.run.expeditionMapTestEvolutions_},
                                 {"repairs", world_.run.expeditionMapTestHeals_},
                                 {"class", world_.resources.player_->GetCurrentClassName()},
                                 {"cards", world_.run.tankRun_.GetCardCounts()},
                                 {"roomTemplates", world_.run.expeditionRooms_.rooms.size()},
                                 {"upgradeTypes", world_.run.expeditionContent_.upgrades.size()},
                                 {"enemyTypes", world_.run.expeditionContent_.enemies.size()},
                                 {"playerTypes", world_.run.expeditionContent_.players.size()}};
        std::ofstream("generated/expedition_map/validation.json") << std::setw(2) << report << '\n';
        PostQuitMessage(success ? 0 : 4);
        return;
    }
    if (world_.run.expeditionMapTestAge_ < 0.9f || !world_.run.tankRunCapturePath_.empty())
        return;
    if (world_.run.expeditionBuildChoice_) {
        world_.expeditionBuildSelection->SelectExpeditionBuildStyle(0);
        return;
    }
    if (world_.run.expeditionMapRun_.IsChoosing()) {
        auto options = world_.run.expeditionMapRun_.GetAvailableNodeIds();
        if (options.empty())
            return;
        std::string id = options.front();
        // Exercise repair and sequential additive upgrades through real transactions.
        for (const auto& choice : options)
            if (choice == "field_repair" || choice == "arsenal" || choice == "final_upgrade")
                id = choice;
        RequestExpeditionMapNode(id);
        return;
    }
    const auto* node = world_.run.expeditionMapRun_.GetActiveNode();
    if (!node)
        return;
    if (tankexp::IsCombatNode(node->kind)) {
        if (world_.run.expeditionMapTestAge_ > 2.0f) {
            for (auto* e : world_.resources.enemyManager_->GetEnemyPtrs())
                if (e && !e->IsDead())
                    e->TakeDamageFromPlayer(100000);
            if (node->kind == NK::Boss)
                world_.resources.enemy_->TakeDamage(100000);
        }
    } else if (node->kind == NK::Heal) {
        world_.resources.player_->SpendRunHealth(20);
        SelectExpeditionService(0);
    } else if (node->kind == NK::Upgrade) {
        // Keep the transaction deterministic while checking cumulative equipment.
        for (const char* id : {"ExtraBarrel1", "ExtraBarrel2", "FanMount", "AlternatingFire"}) {
            const auto* u = tankcontent::FindUpgrade(world_.run.expeditionContent_, id);
            if (u && tankcontent::EligibleUpgrade(*u, world_.run.expeditionBuildStyle_, world_.run.tankRun_.GetCardCounts())) {
                world_.run.expeditionServiceOffers_ = {u->id};
                world_.expeditionBuildSelection->RefreshExpeditionBuildCards();
                SelectExpeditionService(0);
                return;
            }
        }
        SelectExpeditionService(0);
    } else
        SelectExpeditionService(0);
}

} // namespace gameplay
