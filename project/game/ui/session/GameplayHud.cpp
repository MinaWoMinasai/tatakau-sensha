#include "game/ui/session/GameplayHud.h"
#include "game/session/GameplaySystems.h"
#include "game/weapon/CombatTypes.h"
#include "RuntimeProfiler.h"
#include "StartupTrace.h"
#include "GameStartMode.h"
#include "game/ui/TankCombatNeonGeometry.h"
#include "game/effects/TankSpecialNeonGeometry.h"
#include "CollisionConfig.h"
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include "game/session/GameplayHelpers.h"

namespace gameplay {

using namespace detail;

void GameplayHud::ApplyGameTextAppearance()
{
    myEngine::TextFontOverride fontOverride{};
    fontOverride.enabled = true;
    if (world_.combat.gameTextFontMode_ == 0) {
        fontOverride.fontFamily = "Meiryo";
        fontOverride.fontWeight = 400;
    } else {
        fontOverride.fontFamily = "Zen Maru Gothic";
        fontOverride.fontPath = "resources/fonts/ZenMaruGothic-Bold.ttf";
        fontOverride.fontWeight = 700;
        fontOverride.overrideOutline = true;
        fontOverride.outlineColor =
            world_.combat.gameTextOutlineEnabled_ ? world_.combat.gameTextOutlineColor_ : myEngine::Vector4{0.0f, 0.0f, 0.0f, 0.0f};
        fontOverride.outlineThickness = world_.combat.gameTextOutlineEnabled_ ? world_.combat.gameTextOutlineThickness_ : 0.0f;
    }
    myEngine::TextRenderer::GetInstance()->SetFontOverride(fontOverride);
    if (world_.resources.player_) {
        world_.resources.player_->PrepareUpgradeHudTextTextures();
    }

    world_.combat.gameTextNeonStyle_.enabled = world_.combat.gameTextNeonEnabled_;
    if (world_.combat.gameTextNeonEffect_) {
        world_.combat.gameTextNeonEffect_->SetStyle(world_.combat.gameTextNeonStyle_);
    }
}

void GameplayHud::DrawGameTextBloom()
{
    if (!world_.combat.gameTextNeonEnabled_ || !world_.combat.gameTextNeonEffect_ || !world_.resources.player_ ||
        world_.resources.player_->IsChangeMode()) {
        return;
    }

    std::vector<myEngine::TextLabel*> labels;
    // 強化段数バー表示中は、項目名と +/- のネオン源も追加される。
    labels.reserve(32);
    if (world_.run.expeditionMapEnabled_ && world_.run.tankExpedition_.IsCombat() && !world_.run.tankRunPaused_ &&
        world_.run.tankRunObjectiveText_) {
        labels.push_back(world_.run.tankRunObjectiveText_.get());
    }
    if (world_.combat.combatFlow_.GetState() == GameFlowState::Playing) {
        if (!world_.run.expeditionRun_) {
            world_.resources.player_->AppendGameplayNeonTextLabels(labels);
        }
        if (world_.combat.tutorialConfig_.enabled && world_.combat.tutorialUiVisible_) {
            if (world_.combat.tutorialTitleText_) {
                labels.push_back(world_.combat.tutorialTitleText_.get());
            }
            if (world_.combat.tutorialInputText_) {
                labels.push_back(world_.combat.tutorialInputText_.get());
            }
        }
    }
    if (world_.combat.eventCalloutTimer_ > 0.0f && world_.combat.eventCalloutText_) {
        labels.push_back(world_.combat.eventCalloutText_.get());
    }
    const auto& cine = world_.presentation.bossCinematic_;
    const bool cinematicTitle = world_.resources.neonDepthCameraScoped_ && cine.titleMode != 0 && cine.titleAlpha > 0.05f &&
                                world_.combat.cinematicTitle_ && world_.combat.cinematicTitleMode_ == cine.titleMode;
    if (cinematicTitle) {
        labels.push_back(world_.combat.cinematicTitle_.get());
    }
    if (world_.combat.combatFlow_.GetState() != GameFlowState::Playing && world_.combat.flowBannerText_ &&
        !(world_.resources.neonDepthCameraScoped_ && cine.defeatTime >= 0.0f)) {
        labels.push_back(world_.combat.flowBannerText_.get());
    }
    const bool showResult =
        world_.combat.combatFlow_.GetState() == GameFlowState::StageClear ||
        (world_.combat.combatFlow_.GetState() == GameFlowState::GameOver && world_.combat.combatFlow_.GetTimer() <= 0.0f);
    if (showResult && !world_.run.prototypeRun_) {
        if (world_.combat.resultSummaryText_) {
            labels.push_back(world_.combat.resultSummaryText_.get());
        }
        if (world_.combat.resultMenuText_) {
            labels.push_back(world_.combat.resultMenuText_.get());
        }
    }
    auto glow = world_.combat.gameTextNeonStyle_;
    if (world_.combat.eventCalloutTimer_ > 0 && world_.combat.eventCalloutText_ &&
        (world_.combat.eventCalloutText_->GetText() == "ダメージ" || world_.combat.eventCalloutText_->GetText() == "戦闘不能")) {
        glow.glowColor = {1, 0.08f, 0.025f, 1};
    }
    world_.combat.gameTextNeonEffect_->SetStyle(glow);
    world_.combat.gameTextNeonEffect_->DrawBloom(labels);
    world_.combat.gameTextNeonEffect_->SetStyle(world_.combat.gameTextNeonStyle_);
}

void GameplayHud::DrawBossCinematic()
{
    const auto& cine = world_.presentation.bossCinematic_;
    if (!world_.resources.neonDepthCameraScoped_ || cine.letterbox < 0.01f) {
        return;
    }
    auto& ui = world_.combat;
    if (!ui.cinematicBarTop_ || !ui.cinematicTitle_) {
        return;
    }
    const float width = static_cast<float>(myEngine::WinApp::kClientWidth);
    const float height = static_cast<float>(myEngine::WinApp::kClientHeight);
    const float bar = 86.0f * cine.letterbox;
    ui.cinematicBarTop_->SetAnchorPoint({0.0f, 0.0f});
    ui.cinematicBarBottom_->SetAnchorPoint({0.0f, 0.0f});
    ui.cinematicBarTop_->SetPosition({0.0f, 0.0f});
    ui.cinematicBarTop_->SetSize({width, bar});
    ui.cinematicBarBottom_->SetPosition({0.0f, height - bar});
    ui.cinematicBarBottom_->SetSize({width, bar});
    ui.cinematicBarTop_->Update();
    ui.cinematicBarBottom_->Update();
    ui.cinematicBarTop_->Draw();
    ui.cinematicBarBottom_->Draw();

    if (cine.titleMode == 0 || cine.titleAlpha < 0.01f || ui.cinematicTitleMode_ != cine.titleMode) {
        return;
    }
    // The title slides in slightly while it fades, then holds.
    const float settle = 1.0f - std::exp(-cine.titleAge * 5.0f);
    const float alpha = (std::clamp)(cine.titleAlpha, 0.0f, 1.0f);
    ui.cinematicTitle_->SetPosition({width * 0.5f, height * 0.72f + (1.0f - settle) * 18.0f});
    ui.cinematicTitle_->SetAlpha(alpha);
    ui.cinematicSubtitle_->SetPosition({width * 0.5f, height * 0.72f + 54.0f});
    ui.cinematicSubtitle_->SetAlpha(alpha * settle);
    // Below the title, in the lower letterbox where it never covers the actors.
    if (cine.titleMode == 1) {
        ui.cinematicSubtitle_->SetPosition({width * 0.5f, height - bar * 0.5f});
    }
    ui.cinematicTitle_->Draw();
    ui.cinematicSubtitle_->Draw();
}

void GameplayHud::DrawSprite()
{
    if (world_.gameplayQueries->IsNeonShowcaseActive()) {
        return;
    }
    world_.run.expeditionPointerCursor_ = 0;

    if (!world_.run.expeditionRun_ && !world_.gameplayQueries->IsTutorialCombatSuppressed()) {
        world_.resources.enemy_->DrawSprite();
    }
    world_.combat.followHpBarIndex_ = 0;
    for (auto& vertices : world_.combat.hpBarBackgroundVertices_) {
        vertices.clear();
    }
    for (auto& vertices : world_.combat.hpBarFillVertices_) {
        vertices.clear();
    }
    world_.combat.staminaBarFillVertices_.clear();
    for (auto& vertices : world_.combat.hpBarOutlineVertices_) {
        vertices.clear();
    }
    if (world_.combat.showFollowHpBars_) {
        // The neon player draws its HP / stamina as segmented arcs around the body instead.
        const bool playerArcs = world_.run.prototypeRun_ && world_.presentation.playerNeonRenderMode_ == 1;
        if (!playerArcs && !world_.resources.player_->IsDead() && (!world_.run.expeditionRun_ || world_.run.tankExpedition_.IsCombat())) {
            DrawFollowHpBar(world_.resources.player_.get(), world_.resources.player_->GetWorldPosition(), world_.resources.player_->GetHp(),
                            world_.resources.player_->GetMaxHp(), 58.0f, -1.65f);
            if (world_.combat.showPlayerStaminaBar_) {
                const Player::PlayerStats& stats = world_.resources.player_->GetStats();
                DrawFollowStaminaBar(world_.resources.player_->GetWorldPosition(), stats.stamina, stats.maxStamina, 54.0f, -2.35f);
            }
        }
        for (PlayerDrone* drone : world_.resources.player_->GetDronePtrs()) {
            if (drone && !drone->IsDead()) {
                DrawFollowHpBar(drone, drone->GetWorldPosition(), drone->GetHp(), drone->GetMaxHp(), 42.0f, -1.45f);
            }
        }
        if (!world_.gameplayQueries->IsTutorialCombatSuppressed() &&
            (world_.gameplayQueries->IsRunRivalActive() && !world_.resources.enemy_->IsDead())) {
            DrawFollowHpBar(world_.resources.enemy_.get(), world_.resources.enemy_->GetWorldPosition(), world_.resources.enemy_->GetHp(),
                            world_.resources.enemy_->GetMaxHp(), 92.0f, -3.25f);
        }
        for (ExpEnemy* expEnemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
            if (expEnemy && !expEnemy->IsDead()) {
                DrawFollowHpBar(expEnemy, expEnemy->GetWorldPosition(), expEnemy->GetHp(), expEnemy->GetMaxHp(), 36.0f, -1.05f);
            }
        }
    }
    DrawHpBarBatches();
    myEngine::SpriteCommon::GetInstance()->PreDraw(myEngine::kNormal);
    DrawBossCinematic();
    if (!world_.run.expeditionRun_) {
        world_.resources.player_->DrawSprite();
    }
    if (world_.combat.combatFlow_.GetState() == GameFlowState::Playing) {
        world_.resources.player_->DrawEncyclopedia();
    }

    if (world_.combat.controlGuideText_ && !world_.run.prototypeRun_ && !world_.combat.tutorialConfig_.enabled &&
        world_.combat.combatFlow_.GetState() == GameFlowState::Playing && !world_.resources.player_->IsChangeMode()) {
        world_.combat.controlGuideText_->SetPosition(world_.combat.showControlGuide_ ? myEngine::Vector2{22.0f, 636.0f}
                                                                                     : myEngine::Vector2{22.0f, 690.0f});
        world_.combat.controlGuideText_->Draw();
    }
    world_.combatTutorial->DrawTutorialUi();
    const auto& presentation = world_.presentation;
    if (world_.combat.bloomViewText_ &&
        (presentation.bloomView_ != 0 || presentation.bloomViewFreeze_ || presentation.bloomViewLabelTimer_ > 0.0f)) {
        world_.combat.bloomViewText_->SetPosition({myEngine::WinApp::kClientWidth * 0.5f, myEngine::WinApp::kClientHeight - 40.0f});
        world_.combat.bloomViewText_->Draw();
    }
#if defined(USE_IMGUI) && !defined(NDEBUG)
    if (world_.combat.fpsText_ && !world_.run.prototypeRun_) {
        world_.combat.fpsText_->Draw();
    }
    if (world_.presentation.showPostProfileOverlay_ && world_.combat.postProfileText_) {
        world_.combat.postProfileText_->Draw();
    }
    world_.gameplayEditor->DrawBulletStatusDebugOverlay();
#endif // defined(USE_IMGUI) && !defined(NDEBUG)
    if (world_.combat.eventCalloutTimer_ > 0.0f && world_.combat.eventCalloutText_) {
        world_.combat.eventCalloutText_->SetPosition(
            {myEngine::WinApp::kClientWidth * 0.5f, world_.run.expeditionMapEnabled_ ? 155.0f : 112.0f});
        world_.combat.eventCalloutText_->Draw();
    }
    const bool showResult =
        world_.combat.combatFlow_.GetState() == GameFlowState::StageClear ||
        (world_.combat.combatFlow_.GetState() == GameFlowState::GameOver && world_.combat.combatFlow_.GetTimer() <= 0.0f);
    const bool cinematicTitle = world_.resources.neonDepthCameraScoped_ && world_.presentation.bossCinematic_.defeatTime >= 0.0f;
    if (world_.combat.flowBannerText_ && world_.combat.combatFlow_.GetState() != GameFlowState::Playing &&
        !(world_.run.prototypeRun_ && showResult) && !cinematicTitle) {
        world_.combat.flowBannerText_->Draw();
    }
    if (showResult && !world_.run.prototypeRun_) {
        if (world_.combat.resultSummaryText_) {
            world_.combat.resultSummaryText_->Draw();
        }
        if (world_.combat.resultMenuText_) {
            world_.combat.resultMenuText_->Draw();
        }
    }
    if (world_.combat.phase_ != Phase::kFadeIn) {
        if (world_.run.prototypeRun_) {
            world_.arenaRunController->DrawTankRunUi();
        }
        if (world_.run.expeditionMapEnabled_) {
            world_.expeditionExperience->DrawExpeditionCredits();
            world_.expeditionExperience->DrawGuidedExpedition();
        }
        if (world_.run.expeditionMapEnabled_) {
            world_.expeditionMapController->DrawExpeditionPresentation();
        }
        world_.combat.fade_->Draw();
    }
}

void GameplayHud::InitializeFollowHpBars(size_t count)
{
    world_.combat.followHpBars_.clear();
    world_.combat.followHpBars_.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        FollowHpBar bar{};
        bar.outline = std::make_unique<myEngine::Sprite>();
        bar.background = std::make_unique<myEngine::Sprite>();
        bar.fill = std::make_unique<myEngine::Sprite>();

        bar.outline->Initialize(myEngine::SpriteCommon::GetInstance(), "resources/hpBarFrame.png");
        bar.background->Initialize(myEngine::SpriteCommon::GetInstance(), "resources/hpBarMask.png");
        bar.fill->Initialize(myEngine::SpriteCommon::GetInstance(), "resources/hpBarFillMask.png");

        bar.outline->SetAnchorPoint({0.5f, 0.5f});
        bar.background->SetAnchorPoint({0.5f, 0.5f});
        bar.fill->SetAnchorPoint({0.0f, 0.5f});

        bar.outline->SetColor({1.0f, 1.0f, 1.0f, 0.95f});
        bar.background->SetColor({0.0f, 0.0f, 0.0f, 0.92f});
        bar.fill->SetColor({0.35f, 1.0f, 0.42f, 1.0f});

        world_.combat.followHpBars_.push_back(std::move(bar));
    }
}

void GameplayHud::InitializeFollowHpBarBatch()
{
    myEngine::TextureManager::GetInstance()->LoadTexture("resources/hpBarOutlineMask.png");
    myEngine::TextureManager::GetInstance()->LoadTexture("resources/hpBarFillMask.png");

    const uint32_t kMaxHpBarVertices = 4096;
    world_.combat.hpBarVertexResource_ =
        myEngine::Object3dCommon::GetInstance()->GetDxCommon()->CreateBufferResource(sizeof(myEngine::VertexData) * kMaxHpBarVertices);
    world_.combat.hpBarVertexBufferView_.BufferLocation = world_.combat.hpBarVertexResource_->GetGPUVirtualAddress();
    world_.combat.hpBarVertexBufferView_.SizeInBytes = sizeof(myEngine::VertexData) * kMaxHpBarVertices;
    world_.combat.hpBarVertexBufferView_.StrideInBytes = sizeof(myEngine::VertexData);
    world_.combat.hpBarVertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&world_.combat.hpBarVertexData_));

    for (HpBarMaterialBuffer& material : world_.combat.hpBarMaterials_) {
        material.resource = myEngine::Object3dCommon::GetInstance()->GetDxCommon()->CreateBufferResource(sizeof(myEngine::Material));
        material.resource->Map(0, nullptr, reinterpret_cast<void**>(&material.data));
        *material.data = myEngine::MakeDefaultMaterial();
        material.data->shininess = 1.0f;
    }
    for (size_t i = 0; i < 4; ++i) {
        const float alpha = static_cast<float>(i + 1) / 4.0f;
        world_.combat.hpBarMaterials_[i * 3 + 0].data->color = {0.0f, 0.0f, 0.0f, 0.94f * alpha};
        world_.combat.hpBarMaterials_[i * 3 + 1].data->color = {0.46f, 1.0f, 0.56f, alpha};
        world_.combat.hpBarMaterials_[i * 3 + 2].data->color = {0.92f, 1.0f, 0.94f, 0.98f * alpha};
    }
    world_.combat.hpBarMaterials_[12].data->color = {1.0f, 0.82f, 0.12f, 1};

    world_.combat.hpBarTransformResource_ =
        myEngine::Object3dCommon::GetInstance()->GetDxCommon()->CreateBufferResource(sizeof(myEngine::TransformationMatrixWithShadow));
    world_.combat.hpBarTransformResource_->Map(0, nullptr, reinterpret_cast<void**>(&world_.combat.hpBarTransformData_));
    world_.combat.hpBarTransformData_->World = myEngine::MakeIdentity4x4();
    world_.combat.hpBarTransformData_->WorldInverseTranspose = myEngine::MakeIdentity4x4();
    world_.combat.hpBarTransformData_->LightWVP = myEngine::MakeIdentity4x4();
    world_.combat.hpBarTransformData_->WVP = myEngine::MakeOrthographicMatrix(0.0f, 0.0f, float(myEngine::WinApp::kClientWidth),
                                                                              float(myEngine::WinApp::kClientHeight), 0.0f, 100.0f);
}

void GameplayHud::DrawFollowHpBar(const void* ownerKey, const myEngine::Vector3& worldPos, int hp, int maxHp, float width, float yOffset)
{
    if (maxHp <= 0 || hp <= 0) {
        return;
    }

    HpBarVisibility& visibility = world_.combat.hpBarVisibility_[ownerKey];
    if (visibility.lastHp < 0) {
        visibility.lastHp = hp;
        visibility.lastMaxHp = maxHp;
    }
    const bool hpChanged = hp != visibility.lastHp || maxHp != visibility.lastMaxHp;
    if (hpChanged) {
        visibility.visibleTimer = 1.2f;
    } else if (visibility.lastHp < visibility.lastMaxHp && hp >= maxHp) {
        visibility.visibleTimer = 0.35f;
    }
    visibility.lastHp = hp;
    visibility.lastMaxHp = maxHp;

    const float targetAlpha = ownerKey == world_.resources.player_.get() || hp < maxHp || visibility.visibleTimer > 0.0f ? 1.0f : 0.0f;
    if (ownerKey == world_.resources.player_.get()) {
        visibility.alpha = 1.0f;
    }
    const float fadeSpeed = targetAlpha > visibility.alpha ? 10.0f : 4.0f;
    if (visibility.alpha < targetAlpha) {
        visibility.alpha = (std::min)(targetAlpha, visibility.alpha + world_.combat.finalDeltaTime * fadeSpeed);
    } else if (visibility.alpha > targetAlpha) {
        visibility.alpha = (std::max)(targetAlpha, visibility.alpha - world_.combat.finalDeltaTime * fadeSpeed);
    }
    visibility.visibleTimer = (std::max)(0.0f, visibility.visibleTimer - world_.combat.finalDeltaTime);

    if (visibility.alpha <= 0.0f) {
        return;
    }
    const float alpha = visibility.alpha;

    const float ratio = (std::clamp)(static_cast<float>(hp) / static_cast<float>(maxHp), 0.0f, 1.0f);
    const float height = (std::max)(9.0f, width * 0.18f);
    const myEngine::Vector2 screenPos = WorldToScreen(worldPos + myEngine::Vector3{0.0f, yOffset, 0.0f});
    const float kCullMargin = 80.0f;
    if (screenPos.x < -kCullMargin || screenPos.x > myEngine::WinApp::kClientWidth + kCullMargin || screenPos.y < -kCullMargin ||
        screenPos.y > myEngine::WinApp::kClientHeight + kCullMargin) {
        return;
    }

    const float inset = 3.0f;
    const float innerWidth = (std::max)(1.0f, width - inset * 2.0f);
    const float innerHeight = (std::max)(1.0f, height - inset * 2.0f);
    const float fillWidth = (std::max)(1.0f, innerWidth * ratio);
    const size_t alphaBucket = static_cast<size_t>((std::clamp)(static_cast<int>(std::ceil(alpha * 4.0f)) - 1, 0, 3));

    QueueHpBarQuad(world_.combat.hpBarBackgroundVertices_[alphaBucket], screenPos, {innerWidth, innerHeight});
    QueueHpBarQuad(world_.combat.hpBarFillVertices_[alphaBucket], {screenPos.x - innerWidth * 0.5f + fillWidth * 0.5f, screenPos.y},
                   {fillWidth, innerHeight});
    QueueHpBarQuad(world_.combat.hpBarOutlineVertices_[alphaBucket], screenPos, {width, height});
}

void GameplayHud::DrawFollowStaminaBar(const myEngine::Vector3& worldPos, float stamina, float maxStamina, float width, float yOffset)
{
    if (maxStamina <= 0.0f) {
        return;
    }
    const float ratio = (std::clamp)(stamina / maxStamina, 0.0f, 1.0f);
    const myEngine::Vector2 screenPos = WorldToScreen(worldPos + myEngine::Vector3{0.0f, yOffset, 0.0f});
    const float kCullMargin = 80.0f;
    if (screenPos.x < -kCullMargin || screenPos.x > myEngine::WinApp::kClientWidth + kCullMargin || screenPos.y < -kCullMargin ||
        screenPos.y > myEngine::WinApp::kClientHeight + kCullMargin) {
        return;
    }

    const float height = 7.0f;
    const float inset = 2.0f;
    const float innerWidth = (std::max)(1.0f, width - inset * 2.0f);
    const float innerHeight = (std::max)(1.0f, height - inset * 2.0f);
    const float fillWidth = innerWidth * ratio;
    constexpr size_t kOpaqueBucket = 3;
    QueueHpBarQuad(world_.combat.hpBarBackgroundVertices_[kOpaqueBucket], screenPos, {innerWidth, innerHeight});
    if (fillWidth > 0.0f) {
        QueueHpBarQuad(world_.combat.staminaBarFillVertices_, {screenPos.x - innerWidth * 0.5f + fillWidth * 0.5f, screenPos.y},
                       {fillWidth, innerHeight});
    }
    QueueHpBarQuad(world_.combat.hpBarOutlineVertices_[kOpaqueBucket], screenPos, {width, height});
}

void GameplayHud::QueueHpBarQuad(std::vector<myEngine::VertexData>& vertices, const myEngine::Vector2& center,
                                 const myEngine::Vector2& size)
{
    const float left = center.x - size.x * 0.5f;
    const float right = center.x + size.x * 0.5f;
    const float top = center.y - size.y * 0.5f;
    const float bottom = center.y + size.y * 0.5f;

    vertices.push_back({{left, bottom, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}});
    vertices.push_back({{left, top, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}});
    vertices.push_back({{right, bottom, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}});
    vertices.push_back({{left, top, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}});
    vertices.push_back({{right, top, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}});
    vertices.push_back({{right, bottom, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}});
}

void GameplayHud::DrawHpBarBatch(uint32_t startVertex, uint32_t vertexCount, const std::string& textureFilePath,
                                 const HpBarMaterialBuffer& material)
{
    if (vertexCount == 0 || !world_.combat.hpBarVertexData_) {
        return;
    }

    auto* dxCommon = myEngine::Object3dCommon::GetInstance()->GetDxCommon();
    auto* commandList = dxCommon->GetList().Get();
    commandList->IASetVertexBuffers(0, 1, &world_.combat.hpBarVertexBufferView_);
    commandList->SetGraphicsRootConstantBufferView(0, material.resource->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, world_.combat.hpBarTransformResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(10, material.resource->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(2, myEngine::TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath));
    commandList->DrawInstanced(vertexCount, 1, startVertex, 0);
}

void GameplayHud::DrawHpBarBatches()
{
    /// @brief HPバーの一括描画へ渡す位置・サイズ・色を表す。
    struct HpBarDrawCommand {
        uint32_t startVertex = 0;
        uint32_t vertexCount = 0;
        const char* textureFilePath = nullptr;
        const HpBarMaterialBuffer* material = nullptr;
    };

    std::array<HpBarDrawCommand, 13> drawCommands{};
    uint32_t currentVertex = 0;
    auto appendVertices = [&](const std::vector<myEngine::VertexData>& vertices, const char* textureFilePath,
                              const HpBarMaterialBuffer& material, size_t commandIndex) {
        if (vertices.empty() || currentVertex >= 4096u) {
            return;
        }
        const uint32_t count = (std::min)(static_cast<uint32_t>(vertices.size()), 4096u - currentVertex);
        std::copy_n(vertices.data(), count, world_.combat.hpBarVertexData_ + currentVertex);
        drawCommands[commandIndex] = {currentVertex, count, textureFilePath, &material};
        currentVertex += count;
    };

    for (size_t i = 0; i < world_.combat.hpBarBackgroundVertices_.size(); ++i) {
        appendVertices(world_.combat.hpBarBackgroundVertices_[i], "resources/hpBarFillMask.png", world_.combat.hpBarMaterials_[i * 3 + 0],
                       i * 3 + 0);
        appendVertices(world_.combat.hpBarFillVertices_[i], "resources/hpBarFillMask.png", world_.combat.hpBarMaterials_[i * 3 + 1],
                       i * 3 + 1);
        appendVertices(world_.combat.hpBarOutlineVertices_[i], "resources/hpBarOutlineMask.png", world_.combat.hpBarMaterials_[i * 3 + 2],
                       i * 3 + 2);
    }
    appendVertices(world_.combat.staminaBarFillVertices_, "resources/hpBarFillMask.png", world_.combat.hpBarMaterials_[12], 12);
    if (currentVertex == 0) {
        return;
    }

    world_.combat.hpBarTransformData_->WVP = myEngine::MakeOrthographicMatrix(0.0f, 0.0f, float(myEngine::WinApp::kClientWidth),
                                                                              float(myEngine::WinApp::kClientHeight), 0.0f, 100.0f);
    myEngine::SpriteCommon::GetInstance()->PreDraw(myEngine::kNormal);
    for (const HpBarDrawCommand& command : drawCommands) {
        if (command.vertexCount == 0) {
            continue;
        }
        DrawHpBarBatch(command.startVertex, command.vertexCount, command.textureFilePath, *command.material);
    }
}

myEngine::Vector2 GameplayHud::WorldToScreen(const myEngine::Vector3& worldPos) const
{
    myEngine::Matrix4x4 matVP = myEngine::Object3dCommon::GetInstance()->GetIsDebugCamera()
                                    ? world_.resources.debugCamera->GetViewMatrix() * world_.resources.debugCamera->GetProjectionMatrix()
                                    : world_.resources.camera->GetViewMatrix() * world_.resources.camera->GetProjectionMatrix();
    myEngine::Vector3 ndcPos = myEngine::TransformMatrix(worldPos, matVP);

    float screenX = (ndcPos.x + 1.0f) * 0.5f * myEngine::WinApp::kClientWidth;
    float screenY = (1.0f - ndcPos.y) * 0.5f * myEngine::WinApp::kClientHeight;
    return {screenX, screenY};
}
} // namespace gameplay
