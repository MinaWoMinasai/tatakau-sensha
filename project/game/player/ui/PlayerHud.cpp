#include "game/player/ui/PlayerHud.h"
#include "game/player/ui/PlayerPresentation.h"
#include "game/weapon/CombatTypes.h"
#include "Player.h"
#include "game/player/PlayerMovement.h"
#include "PlayerUiHelpers.h"
#include "game/enemy/visual/NeonDepthPlacement.h"
#include "StartupTrace.h"
#include "Stage.h"
#include "game/exp/ExpEnemy.h"
#include "game/enemy/actor/Enemy.h"
#include "game/exp/EnemyManager.h"
#include "Audio.h"
#include "game/ui/TankButtonUI.h"
#include "game/ui/NeonTextEffect.h"
#include "ObjectPostEffect.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
using playerui::ReadVector2Object;
using playerui::SetLabel;
using playerui::Vector4ToJson;
using playerui::WriteVector2Object;

/// @brief 係数を0〜1に制限し、2つのRGBA色を線形補間して返す。
cg2::Vector4 LerpColor(const cg2::Vector4& a, const cg2::Vector4& b, float t)
{
    t = (std::clamp)(t, 0.0f, 1.0f);
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

/// @brief 機体の種類に対応する画像パスを返す。
const char* ClassTexturePath(ClassType type)
{
    switch (type) {
    case ClassType::Twin:
        return "resources/twin.png";
    case ClassType::MachineGun:
        return "resources/machineGun.png";
    case ClassType::Overseer:
    case ClassType::Summoner:
        return "resources/drone.png";
    default:
        return "resources/normalTank.png";
    }
}

/// @brief 強化HUDの項目名一覧を返す。
const std::array<const char*, 7>& UpgradeHudNames()
{
    static const std::array<const char*, 7> names = {"自動回復", "最大HP", "体当たり", "弾速", "弾ダメージ", "リロード", "移動速度"};
    return names;
}

/// @brief 強化HUDの行ごとの表示色を返す。
const std::array<cg2::Vector4, 7>& UpgradeHudRowColors()
{
    // diepio風の能力ごとの色分け。ゲーム内のネオン表現と衝突しないよう、
    // 発光は塗り全体ではなく、セルと細い外周に限定する。
    static const std::array<cg2::Vector4, 7> colors = {{
        {0.90f, 0.36f, 0.86f, 1.0f}, // 自動回復
        {0.67f, 0.36f, 0.95f, 1.0f}, // 最大HP
        {0.49f, 0.39f, 0.98f, 1.0f}, // 体当たり
        {0.35f, 0.58f, 1.00f, 1.0f}, // 弾速
        {1.00f, 0.86f, 0.24f, 1.0f}, // 弾ダメージ
        {1.00f, 0.38f, 0.42f, 1.0f}, // リロード
        {0.32f, 1.00f, 0.56f, 1.0f}  // 移動速度
    }};
    return colors;
}

/// @brief 強化HUD区切り外観を作成して返す。
NeonSegmentedBarStyle MakeUpgradeHudSegmentStyle(int index)
{
    const cg2::Vector4 baseColor = UpgradeHudRowColors()[(std::clamp)(index, 0, 6)];
    NeonSegmentedBarStyle style{};
    // 外周の半円部にも未取得セルと同じ不透明な色を入れ、端だけが薄く
    // 見えないようにする。
    style.backgroundColor = {baseColor.x * 0.16f, baseColor.y * 0.16f, baseColor.z * 0.16f, 0.98f};
    style.emptyColor = {baseColor.x * 0.16f, baseColor.y * 0.16f, baseColor.z * 0.16f, 0.92f};
    style.filledColor = baseColor;
    style.outlineColor = {baseColor.x * 0.82f + 0.12f, baseColor.y * 0.82f + 0.12f, baseColor.z * 0.82f + 0.12f, 0.94f};
    // 連続バーと同じく、丸端の外枠より内側へセルを収める。
    // 枠へ重ねないため、端部のはみ出し・細い線の乱れを防ぐ。
    style.innerPadding = 3.0f;
    style.segmentGap = 1.5f;
    style.bloomBrightness = 1.55f;
    style.bloomAlpha = 0.58f;
    style.backdropBloomBrightness = 1.70f;
    style.backdropBloomAlpha = 0.18f;
    style.roundedFrame = true;
    return style;
}

/// @brief 強化HUD下端バー文字外観を作成して返す。
cg2::TextStyle MakeUpgradeHudBottomBarTextStyle()
{
    cg2::TextStyle style{};
    style.fontFamily = "Meiryo";
    style.fontSize = 15.0f;
    style.color = {0.90f, 0.94f, 1.0f, 1.0f};
    style.outlineColor = {0.0f, 0.0f, 0.0f, 0.92f};
    style.outlineThickness = 1.0f;
    style.padding = 3.0f;
    style.preserveOutline = true;
    return style;
}

/// @brief 強化HUDSmall文字外観を作成して返す。
cg2::TextStyle MakeUpgradeHudSmallTextStyle()
{
    cg2::TextStyle style{};
    style.fontFamily = "Meiryo";
    style.fontSize = 15.0f;
    style.color = {0.90f, 0.94f, 1.0f, 1.0f};
    style.outlineColor = {0.0f, 0.03f, 0.05f, 0.95f};
    style.outlineThickness = 2.0f;
    style.padding = 5.0f;
    return style;
}

/// @brief 強化HUD重ね表示文字外観を作成して返す。
cg2::TextStyle MakeUpgradeHudOverlayTextStyle()
{
    cg2::TextStyle style = MakeUpgradeHudSmallTextStyle();
    style.outlineColor = {0.0f, 0.0f, 0.0f, 0.92f};
    style.outlineThickness = 1.0f;
    style.padding = 3.0f;
    style.preserveOutline = true;
    return style;
}

/// @brief 強化HUD文字進行を返す。
float GetUpgradeHudTextAdvance(const cg2::TextLabel* label, const cg2::TextStyle& style)
{
    if (!label || !label->GetSprite()) {
        return style.fontSize * 0.55f;
    }
    const float padding = std::ceil(style.padding + style.outlineThickness);
    const float fallback = style.fontSize * (label->GetText() == " " ? 0.34f : 0.55f);
    return (std::max)(fallback, label->GetSprite()->GetSize().x - padding * 2.0f);
}

} // namespace

void PlayerHud::InitializeUpgradeHud()
{
    cg2::StartupTrace::Scope scope("Player.UpgradeHud");
    cg2::SpriteCommon* spriteCommon = cg2::SpriteCommon::GetInstance();
    auto makePanel = [spriteCommon](const cg2::Vector2& pos, const cg2::Vector2& size, const cg2::Vector4& color) {
        auto panel = std::make_unique<cg2::Sprite>();
        panel->Initialize(spriteCommon, "resources/white512x512.png");
        panel->SetPosition(pos);
        panel->SetSize(size);
        panel->SetColor(color);
        return panel;
    };
    auto makePill = [spriteCommon](const cg2::Vector2& pos, const cg2::Vector2& size, const cg2::Vector4& color) {
        auto pill = std::make_unique<cg2::Sprite>();
        pill->Initialize(spriteCommon, "resources/hpBarFillMask.png");
        pill->SetPosition(pos);
        pill->SetSize(size);
        pill->SetColor(color);
        return pill;
    };

    ui_.upgradeHudBackdropSprite_ = makePanel(ui_.upgradeHudPanelPos_, ui_.upgradeHudPanelSize_, {0.03f, 0.04f, 0.06f, 0.58f});
    ui_.upgradeHudExpBackSprite_ = makePanel(ui_.upgradeHudExpBarPos_, ui_.upgradeHudExpBarSize_, {0.04f, 0.04f, 0.05f, 0.82f});
    ui_.upgradeHudExpFillSprite_ = makePanel(ui_.upgradeHudExpBarPos_, {0.0f, ui_.upgradeHudExpBarSize_.y}, {0.96f, 0.83f, 0.24f, 0.95f});
    ui_.upgradeHudLevelBackSprite_ = makePanel(ui_.upgradeHudLevelBarPos_, ui_.upgradeHudLevelBarSize_, {0.04f, 0.04f, 0.05f, 0.82f});
    ui_.upgradeHudLevelFillSprite_ = makePanel({785.0f, 786.0f}, {0.0f, 18.0f}, {0.36f, 1.0f, 0.56f, 0.92f});

    ui_.upgradeHudLevelProgressStyle_.backgroundColor = {0.018f, 0.042f, 0.062f, 0.88f};
    ui_.upgradeHudLevelProgressStyle_.delayedFillColor = {0.20f, 0.72f, 1.00f, 0.52f};
    ui_.upgradeHudLevelProgressStyle_.fillColor = {0.36f, 1.00f, 0.56f, 0.94f};
    ui_.upgradeHudLevelProgressStyle_.outlineColor = {0.40f, 0.94f, 1.00f, 0.92f};
    ui_.upgradeHudLevelProgressStyle_.outlineWidth = 1.5f;
    ui_.upgradeHudLevelProgressStyle_.roundedEnds = ui_.upgradeHudRoundedProgressBars_;
    ui_.upgradeHudExpProgressStyle_.backgroundColor = {0.050f, 0.042f, 0.022f, 0.88f};
    ui_.upgradeHudExpProgressStyle_.delayedFillColor = {0.36f, 0.96f, 0.82f, 0.52f};
    ui_.upgradeHudExpProgressStyle_.fillColor = {1.00f, 0.82f, 0.22f, 0.96f};
    ui_.upgradeHudExpProgressStyle_.outlineColor = {1.00f, 0.92f, 0.48f, 0.92f};
    ui_.upgradeHudExpProgressStyle_.outlineWidth = 1.5f;
    ui_.upgradeHudExpProgressStyle_.roundedEnds = ui_.upgradeHudRoundedProgressBars_;
    ui_.upgradeHudExpProgressBar_ = std::make_unique<NeonProgressBar>();
    ui_.upgradeHudExpProgressBar_->Initialize(spriteCommon);
    ui_.upgradeHudLevelProgressBar_ = std::make_unique<NeonProgressBar>();
    ui_.upgradeHudLevelProgressBar_->Initialize(spriteCommon);
    ApplyUpgradeHudProgressBarStyles();

    for (size_t i = 0; i < ui_.upgradeHudSegmentBars_.size(); ++i) {
        auto& segmentBar = ui_.upgradeHudSegmentBars_[i];
        segmentBar = std::make_unique<NeonSegmentedBar>();
        segmentBar->Initialize(spriteCommon);
        segmentBar->SetStyle(MakeUpgradeHudSegmentStyle(static_cast<int>(i)));
    }
    ui_.upgradeHudBarBloomEffect_ = std::make_unique<cg2::ObjectPostEffect>();
    ui_.upgradeHudBarBloomEffect_->Initialize(cg2::Object3dCommon::GetInstance()->GetDxCommon(),
                                              cg2::Object3dCommon::GetInstance()->GetSrvManager(), nullptr, 0.75f);
    cg2::BloomParam& barBloomParam = ui_.upgradeHudBarBloomEffect_->GetParam();
    barBloomParam.threshold = 0.0f;
    barBloomParam.intensity = 0.78f;
    barBloomParam.outlineWidth = 0.0f;
    barBloomParam.outlineBloomIntensity = 0.0f;

    const cg2::TextStyle smallStyle = MakeUpgradeHudSmallTextStyle();
    const cg2::TextStyle overlayTextStyle = MakeUpgradeHudOverlayTextStyle();
    SetLabel(ui_.upgradeHudTitleLabel_, spriteCommon, "強化", ui_.upgradeHudTitlePos_, smallStyle);
    SetLabel(ui_.upgradeHudPointLabel_, spriteCommon, "x0", ui_.upgradeHudPointPos_, smallStyle);
    const cg2::TextStyle bottomBarTextStyle = MakeUpgradeHudBottomBarTextStyle();
    SetLabel(ui_.upgradeHudLevelLabel_, spriteCommon, "Lv ", ui_.upgradeHudLevelTextPos_, bottomBarTextStyle);
    SetLabel(ui_.upgradeHudLevelClassLabel_, spriteCommon, player_.GetCurrentClassName(), ui_.upgradeHudLevelTextPos_, bottomBarTextStyle);
    for (auto& glyphLabel : ui_.upgradeHudExpGlyphLabels_) {
        glyphLabel = std::make_unique<cg2::TextLabel>();
        glyphLabel->Initialize(spriteCommon, " ", bottomBarTextStyle);
        glyphLabel->SetPosition(ui_.upgradeHudExpTextPos_);
        glyphLabel->SetAlpha(0.0f);
    }
    for (auto& glyphLabel : ui_.upgradeHudLevelGlyphLabels_) {
        glyphLabel = std::make_unique<cg2::TextLabel>();
        glyphLabel->Initialize(spriteCommon, " ", bottomBarTextStyle);
        glyphLabel->SetPosition(ui_.upgradeHudLevelTextPos_);
        glyphLabel->SetAlpha(0.0f);
    }

    const auto& names = UpgradeHudNames();
    for (int i = 0; i < 7; ++i) {
        const float y = ui_.upgradeHudRowStart_.y + static_cast<float>(i) * ui_.upgradeHudRowGap_;
        SetLabel(ui_.upgradeHudNameLabels_[i], spriteCommon, std::to_string(i + 1) + " " + names[i],
                 {ui_.upgradeHudNameX_, y + ui_.upgradeHudNameTextOffsetY_}, overlayTextStyle);
        SetLabel(ui_.upgradeHudLevelLabels_[i], spriteCommon, "Lv.0", {ui_.upgradeHudLevelX_, y + ui_.upgradeHudLevelTextOffsetY_},
                 smallStyle);
        SetLabel(ui_.upgradeHudMinusLabels_[i], spriteCommon, "-", {ui_.upgradeHudMinusLabelX_, y + ui_.upgradeHudMinusTextOffsetY_},
                 overlayTextStyle);
        SetLabel(ui_.upgradeHudPlusLabels_[i], spriteCommon, "+", {ui_.upgradeHudPlusLabelX_, y + ui_.upgradeHudPlusTextOffsetY_},
                 overlayTextStyle);
        ui_.upgradeHudNameLabels_[i]->SetAlpha(0.0f);
        ui_.upgradeHudLevelLabels_[i]->SetAlpha(0.0f);
        ui_.upgradeHudMinusLabels_[i]->SetAlpha(0.0f);
        ui_.upgradeHudPlusLabels_[i]->SetAlpha(0.0f);
    }

    for (int i = 0; i < 7; ++i) {
        const float y = ui_.upgradeHudRowStart_.y + static_cast<float>(i) * ui_.upgradeHudRowGap_;
        ui_.upgradeHudButtonSprites_[i] =
            makePanel({ui_.upgradeHudRowStart_.x, y}, ui_.upgradeHudButtonSize_, {0.10f, 0.12f, 0.15f, 0.84f});
        ui_.upgradeHudMinusSprites_[i] = makePill({ui_.upgradeHudMinusX_, y}, ui_.upgradeHudPlusSize_, {0.26f, 0.42f, 0.86f, 0.68f});
        ui_.upgradeHudPlusSprites_[i] = makePill({ui_.upgradeHudPlusX_, y}, ui_.upgradeHudPlusSize_, {0.34f, 0.95f, 0.64f, 0.88f});
    }
    InitializeUpgradeHudBatch();
}

void PlayerHud::PrepareUpgradeHudTextTextures()
{
    if (!ui_.arenaUiEnabled_)
        return;
    cg2::TextRenderer* textRenderer = cg2::TextRenderer::GetInstance();
    if (!textRenderer || (ui_.upgradeHudTextPrepared_ && ui_.upgradeHudTextFontRevision_ == textRenderer->GetFontRevision() &&
                          ui_.upgradeHudTextPreparedForSegmentedBars_ == ui_.upgradeHudUseSegmentedUpgradeBars_)) {
        return;
    }
    cg2::StartupTrace::Scope scope("Player.UpgradeHudTextPrewarm");

    const cg2::TextStyle bottomStyle = MakeUpgradeHudBottomBarTextStyle();
    const cg2::TextStyle smallStyle = MakeUpgradeHudSmallTextStyle();
    const cg2::TextStyle listControlStyle = ui_.upgradeHudUseSegmentedUpgradeBars_ ? MakeUpgradeHudOverlayTextStyle() : smallStyle;
    auto preload = [textRenderer](const std::string& text, const cg2::TextStyle& style) {
        const std::string texturePath = textRenderer->GetOrCreateTexture(text, style);
        cg2::TextureManager::GetInstance()->LoadTexture(texturePath);
    };

    constexpr char kGlyphs[] = "0123456789EXP /";
    for (const char glyph : kGlyphs) {
        if (glyph == '\0') {
            break;
        }
        preload(std::string(1, glyph), bottomStyle);
    }
    preload("Lv ", bottomStyle);
    preload(player_.GetCurrentClassName(), bottomStyle);

    preload("強化", smallStyle);
    for (int point = 0; point <= player_.kMaxLevel; ++point) {
        preload("x" + std::to_string(point), smallStyle);
    }
    for (int value = 0; value <= 10; ++value) {
        preload("Lv." + std::to_string(value), smallStyle);
    }
    const auto& names = UpgradeHudNames();
    for (int i = 0; i < 7; ++i) {
        preload(std::to_string(i + 1) + " " + names[i], listControlStyle);
    }
    preload("-", listControlStyle);
    preload("+", listControlStyle);
    preload("済", listControlStyle);

    if (ui_.upgradeHudTitleLabel_) {
        ui_.upgradeHudTitleLabel_->SetStyle(smallStyle);
        ui_.upgradeHudTitleLabel_->PrepareForDraw();
    }
    if (ui_.upgradeHudPointLabel_) {
        ui_.upgradeHudPointLabel_->SetStyle(smallStyle);
        ui_.upgradeHudPointLabel_->PrepareForDraw();
    }
    if (ui_.upgradeHudLevelLabel_)
        ui_.upgradeHudLevelLabel_->PrepareForDraw();
    if (ui_.upgradeHudLevelClassLabel_)
        ui_.upgradeHudLevelClassLabel_->PrepareForDraw();
    for (auto& label : ui_.upgradeHudExpGlyphLabels_) {
        if (label)
            label->PrepareForDraw();
    }
    for (auto& label : ui_.upgradeHudLevelGlyphLabels_) {
        if (label)
            label->PrepareForDraw();
    }
    for (int i = 0; i < 7; ++i) {
        if (ui_.upgradeHudNameLabels_[i]) {
            ui_.upgradeHudNameLabels_[i]->SetStyle(listControlStyle);
            ui_.upgradeHudNameLabels_[i]->PrepareForDraw();
        }
        if (ui_.upgradeHudLevelLabels_[i])
            ui_.upgradeHudLevelLabels_[i]->PrepareForDraw();
        if (ui_.upgradeHudMinusLabels_[i]) {
            ui_.upgradeHudMinusLabels_[i]->SetStyle(listControlStyle);
            ui_.upgradeHudMinusLabels_[i]->PrepareForDraw();
        }
        if (ui_.upgradeHudPlusLabels_[i]) {
            ui_.upgradeHudPlusLabels_[i]->SetStyle(listControlStyle);
            ui_.upgradeHudPlusLabels_[i]->PrepareForDraw();
        }
    }

    ui_.upgradeHudTextFontRevision_ = textRenderer->GetFontRevision();
    ui_.upgradeHudTextPrepared_ = true;
    ui_.upgradeHudTextPreparedForSegmentedBars_ = ui_.upgradeHudUseSegmentedUpgradeBars_;
    // フォント上書きが変わった場合は、次のHUD描画でグリフ配置も更新する。
    ui_.cachedUpgradeHudExp_ = -1;
    ui_.cachedUpgradeHudLevel_ = -1;
    ui_.cachedUpgradeHudListVisible_ = false;
}

void PlayerHud::UpdateUpgradeHudExpGlyphs(const std::string& text, const cg2::TextStyle& style)
{
    const size_t glyphCount = (std::min)(text.size(), ui_.upgradeHudExpGlyphLabels_.size());
    for (size_t i = 0; i < glyphCount; ++i) {
        cg2::TextLabel* label = ui_.upgradeHudExpGlyphLabels_[i].get();
        if (!label) {
            continue;
        }
        label->SetStyle(style);
        label->SetText(std::string(1, text[i]));
        label->PrepareForDraw();
        label->SetAlpha(1.0f);
    }
    for (size_t i = glyphCount; i < ui_.upgradeHudExpGlyphCount_; ++i) {
        if (ui_.upgradeHudExpGlyphLabels_[i]) {
            ui_.upgradeHudExpGlyphLabels_[i]->SetAlpha(0.0f);
        }
    }
    ui_.upgradeHudExpGlyphCount_ = glyphCount;
    PositionUpgradeHudExpGlyphs();
}

void PlayerHud::PositionUpgradeHudExpGlyphs()
{
    const cg2::TextStyle style = MakeUpgradeHudBottomBarTextStyle();
    float x = ui_.upgradeHudExpTextPos_.x;
    for (size_t i = 0; i < ui_.upgradeHudExpGlyphCount_; ++i) {
        cg2::TextLabel* label = ui_.upgradeHudExpGlyphLabels_[i].get();
        if (!label || !label->GetSprite()) {
            continue;
        }
        label->SetPosition({x, ui_.upgradeHudExpTextPos_.y});
        x += GetUpgradeHudTextAdvance(label, style);
    }
}

void PlayerHud::UpdateUpgradeHudLevelLabels(int level, const std::string& className, const cg2::TextStyle& style)
{
    const std::string levelText = std::to_string(level);
    const size_t glyphCount = (std::min)(levelText.size(), ui_.upgradeHudLevelGlyphLabels_.size());
    for (size_t i = 0; i < glyphCount; ++i) {
        cg2::TextLabel* label = ui_.upgradeHudLevelGlyphLabels_[i].get();
        if (!label)
            continue;
        label->SetStyle(style);
        label->SetText(std::string(1, levelText[i]));
        label->PrepareForDraw();
        label->SetAlpha(1.0f);
    }
    for (size_t i = glyphCount; i < ui_.upgradeHudLevelGlyphCount_; ++i) {
        if (ui_.upgradeHudLevelGlyphLabels_[i])
            ui_.upgradeHudLevelGlyphLabels_[i]->SetAlpha(0.0f);
    }
    ui_.upgradeHudLevelGlyphCount_ = glyphCount;
    SetLabel(ui_.upgradeHudLevelLabel_, cg2::SpriteCommon::GetInstance(), "Lv ", ui_.upgradeHudLevelTextPos_, style);
    SetLabel(ui_.upgradeHudLevelClassLabel_, cg2::SpriteCommon::GetInstance(), className, ui_.upgradeHudLevelTextPos_, style);
    PositionUpgradeHudLevelLabels();
}

void PlayerHud::PositionUpgradeHudLevelLabels()
{
    const cg2::TextStyle style = MakeUpgradeHudBottomBarTextStyle();
    float x = ui_.upgradeHudLevelTextPos_.x;
    if (ui_.upgradeHudLevelLabel_) {
        ui_.upgradeHudLevelLabel_->SetPosition({x, ui_.upgradeHudLevelTextPos_.y});
        x += GetUpgradeHudTextAdvance(ui_.upgradeHudLevelLabel_.get(), style);
    }
    for (size_t i = 0; i < ui_.upgradeHudLevelGlyphCount_; ++i) {
        cg2::TextLabel* label = ui_.upgradeHudLevelGlyphLabels_[i].get();
        if (!label)
            continue;
        label->SetPosition({x, ui_.upgradeHudLevelTextPos_.y});
        x += GetUpgradeHudTextAdvance(label, style);
    }
    x += style.fontSize * 0.34f;
    if (ui_.upgradeHudLevelClassLabel_) {
        ui_.upgradeHudLevelClassLabel_->SetPosition({x, ui_.upgradeHudLevelTextPos_.y});
    }
}

void PlayerHud::ApplyUpgradeHudProgressBarStyles()
{
    ui_.upgradeHudLevelProgressStyle_.roundedEnds = ui_.upgradeHudRoundedProgressBars_;
    ui_.upgradeHudExpProgressStyle_.roundedEnds = ui_.upgradeHudRoundedProgressBars_;
    if (ui_.upgradeHudLevelProgressBar_) {
        ui_.upgradeHudLevelProgressBar_->SetStyle(ui_.upgradeHudLevelProgressStyle_);
    }
    if (ui_.upgradeHudExpProgressBar_) {
        ui_.upgradeHudExpProgressBar_->SetStyle(ui_.upgradeHudExpProgressStyle_);
    }
}

void PlayerHud::PrepareUpgradeHudSegmentBars()
{
    for (size_t i = 0; i < ui_.upgradeHudSegmentBars_.size(); ++i) {
        NeonSegmentedBar* segmentBar = ui_.upgradeHudSegmentBars_[i].get();
        if (!segmentBar) {
            continue;
        }
        const float y = ui_.upgradeHudRowStart_.y + static_cast<float>(i) * ui_.upgradeHudRowGap_;
        segmentBar->SetBounds({ui_.upgradeHudRowStart_.x + ui_.upgradeHudSegmentBarOffset_.x, y + ui_.upgradeHudSegmentBarOffset_.y},
                              ui_.upgradeHudSegmentBarSize_);
        segmentBar->SetSegmentCount(player_.maxEnhancePoint);
        segmentBar->SetFilledSegments(0);
        segmentBar->Update();
    }
}

void PlayerHud::InitializeUpgradeHudBatch()
{
    cg2::DirectXCommon* dxCommon = cg2::SpriteCommon::GetInstance()->GetDxCommon();
    if (!dxCommon) {
        return;
    }
    cg2::TextureManager::GetInstance()->LoadTexture("resources/white512x512.png");

    ui_.upgradeHudBatchVertexResource_ = dxCommon->CreateBufferResource(sizeof(cg2::TrailVertex) * ui_.kUpgradeHudBatchMaxVertices);
    ui_.upgradeHudBatchVertexBufferView_.BufferLocation = ui_.upgradeHudBatchVertexResource_->GetGPUVirtualAddress();
    ui_.upgradeHudBatchVertexBufferView_.SizeInBytes = sizeof(cg2::TrailVertex) * ui_.kUpgradeHudBatchMaxVertices;
    ui_.upgradeHudBatchVertexBufferView_.StrideInBytes = sizeof(cg2::TrailVertex);
    ui_.upgradeHudBatchVertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&ui_.upgradeHudBatchVertexData_));

    ui_.upgradeHudBatchTransformResource_ = dxCommon->CreateBufferResource(sizeof(cg2::Matrix4x4));
    ui_.upgradeHudBatchTransformResource_->Map(0, nullptr, reinterpret_cast<void**>(&ui_.upgradeHudBatchTransformData_));
    *ui_.upgradeHudBatchTransformData_ =
        cg2::MakeOrthographicMatrix(0.0f, 0.0f, float(cg2::WinApp::kClientWidth), float(cg2::WinApp::kClientHeight), 0.0f, 100.0f);

    ui_.upgradeHudBatchMaterialResource_ = dxCommon->CreateBufferResource(sizeof(cg2::Material));
    ui_.upgradeHudBatchMaterialResource_->Map(0, nullptr, reinterpret_cast<void**>(&ui_.upgradeHudBatchMaterialData_));
    *ui_.upgradeHudBatchMaterialData_ = cg2::MakeDefaultMaterial();
    ui_.upgradeHudBatchMaterialData_->shininess = 1.0f;
}

void PlayerHud::UpdateUpgradeHud(float uiDeltaTime)
{
    ui_.upgradeHudMouseCaptured_ = false;
    if (!ui_.arenaUiEnabled_ || !ui_.upgradeHudVisible_ || player_.isChangeMode || player_.isDead_) {
        return;
    }
    const float safeUiDeltaTime = (std::max)(0.0f, uiDeltaTime);

    for (float& timer : ui_.upgradeHudFlashTimers_) {
        timer = (std::max)(0.0f, timer - safeUiDeltaTime);
    }
    for (float& timer : ui_.upgradeHudRefundFlashTimers_) {
        timer = (std::max)(0.0f, timer - safeUiDeltaTime);
    }
    for (float& timer : ui_.upgradeHudMissFlashTimers_) {
        timer = (std::max)(0.0f, timer - safeUiDeltaTime);
    }

    ApplyUpgradeHudLayout();
    if (ui_.upgradeHudUseNeonProgressBars_) {
        const int safeNextExp = (std::max)(1, player_.nextLevelExp_);
        const float expTarget = (std::clamp)(static_cast<float>(player_.exp_) / static_cast<float>(safeNextExp), 0.0f, 1.0f);
        const float levelTarget =
            (std::clamp)(static_cast<float>(player_.level_ - 1) / static_cast<float>((std::max)(1, player_.kMaxLevel - 1)), 0.0f, 1.0f);
        const bool levelChanged = ui_.upgradeHudAnimatedLevel_ != player_.level_;
        if (ui_.upgradeHudLevelProgressBar_) {
            ui_.upgradeHudLevelProgressBar_->SetBounds(ui_.upgradeHudLevelBarPos_, ui_.upgradeHudLevelBarSize_);
            ui_.upgradeHudLevelProgressBar_->SetTarget(levelTarget);
            ui_.upgradeHudLevelProgressBar_->Update(safeUiDeltaTime);
        }
        if (ui_.upgradeHudExpProgressBar_) {
            ui_.upgradeHudExpProgressBar_->SetBounds(ui_.upgradeHudExpBarPos_, ui_.upgradeHudExpBarSize_);
            if (levelChanged) {
                ui_.upgradeHudExpProgressBar_->BeginRollover(expTarget);
            } else {
                ui_.upgradeHudExpProgressBar_->SetTarget(expTarget);
            }
            ui_.upgradeHudExpProgressBar_->Update(safeUiDeltaTime);
        }
        ui_.upgradeHudAnimatedLevel_ = player_.level_;
    }
    const bool wantsUpgradeList = !player_.runModifiers_.enabled && (!ui_.upgradeHudHideListWithoutPoints_ || player_.skillPoints_ > 0);
    const float targetListVisibility = wantsUpgradeList ? 1.0f : 0.0f;
    const float listStep = safeUiDeltaTime * ui_.upgradeHudListAnimSpeed_;
    if (ui_.upgradeHudListVisibility_ < targetListVisibility) {
        ui_.upgradeHudListVisibility_ = (std::min)(targetListVisibility, ui_.upgradeHudListVisibility_ + listStep);
    } else if (ui_.upgradeHudListVisibility_ > targetListVisibility) {
        ui_.upgradeHudListVisibility_ = (std::max)(targetListVisibility, ui_.upgradeHudListVisibility_ - listStep);
    }
    const bool showUpgradeList = ui_.upgradeHudListVisibility_ > 0.01f;
    const float listAlpha = (std::clamp)(ui_.upgradeHudListVisibility_, 0.0f, 1.0f);
    const float easedListAlpha = listAlpha * listAlpha * (3.0f - 2.0f * listAlpha);
    const float listOffsetX = -(1.0f - easedListAlpha) * ui_.upgradeHudListSlideDistance_;
    if (!showUpgradeList) {
        if (ui_.upgradeHudExpBackSprite_)
            ui_.upgradeHudExpBackSprite_->Update();
        if (ui_.upgradeHudExpFillSprite_)
            ui_.upgradeHudExpFillSprite_->Update();
        if (ui_.upgradeHudLevelBackSprite_)
            ui_.upgradeHudLevelBackSprite_->Update();
        if (ui_.upgradeHudLevelFillSprite_)
            ui_.upgradeHudLevelFillSprite_->Update();
        return;
    }
    if (ui_.upgradeHudUseSegmentedUpgradeBars_) {
        for (int i = 0; i < 7; ++i) {
            if (!ui_.upgradeHudSegmentBars_[i]) {
                continue;
            }
            const float y = ui_.upgradeHudRowStart_.y + static_cast<float>(i) * ui_.upgradeHudRowGap_;
            ui_.upgradeHudSegmentBars_[i]->SetBounds(
                {ui_.upgradeHudRowStart_.x + ui_.upgradeHudSegmentBarOffset_.x + listOffsetX, y + ui_.upgradeHudSegmentBarOffset_.y},
                ui_.upgradeHudSegmentBarSize_);
            ui_.upgradeHudSegmentBars_[i]->SetSegmentCount(player_.maxEnhancePoint);
            ui_.upgradeHudSegmentBars_[i]->SetFilledSegments(player_.upgradeLevels_[i]);
            ui_.upgradeHudSegmentBars_[i]->Update();
        }
    }

    const bool canUpgrade = player_.skillPoints_ > 0;
    const bool click = player_.input_ && player_.input_->IsTrigger(player_.input_->GetMouseState().rgbButtons[0],
                                                                   player_.input_->GetPreMouseState().rgbButtons[0]);
    for (int i = 0; i < 7; ++i) {
        const float y = ui_.upgradeHudRowStart_.y + static_cast<float>(i) * ui_.upgradeHudRowGap_;
        const float controlOffsetY = ui_.upgradeHudUseSegmentedUpgradeBars_
                                         ? (std::max)(0.0f, (ui_.upgradeHudSegmentBarSize_.y - ui_.upgradeHudPlusSize_.y) * 0.5f)
                                         : 0.0f;
        cg2::Sprite* button = ui_.upgradeHudButtonSprites_[i].get();
        cg2::Sprite* plus = ui_.upgradeHudPlusSprites_[i].get();
        cg2::Sprite* minus = ui_.upgradeHudMinusSprites_[i].get();
        if (button) {
            button->SetPosition({ui_.upgradeHudRowStart_.x + listOffsetX, y});
        }
        if (minus) {
            minus->SetPosition({ui_.upgradeHudMinusX_ + listOffsetX, y + controlOffsetY});
        }
        if (plus) {
            plus->SetPosition({ui_.upgradeHudPlusX_ + listOffsetX, y + controlOffsetY});
        }
        const bool plusHovered = plus && plus->IsHovered(player_.mousePosition_);
        const bool minusHovered = minus && minus->IsHovered(player_.mousePosition_);
        const bool rowHovered = button && button->IsHovered(player_.mousePosition_);
        const bool hovered = rowHovered || plusHovered || minusHovered;
        ui_.upgradeHudMouseCaptured_ = ui_.upgradeHudMouseCaptured_ || hovered;
        const bool maxed = player_.upgradeLevels_[i] >= player_.maxEnhancePoint;
        if (wantsUpgradeList && click && plusHovered) {
            if (canUpgrade && !maxed && player_.ApplyStatUpgrade(i)) {
                ui_.upgradeHudFlashTimers_[i] = 0.22f;
            } else {
                ui_.upgradeHudMissFlashTimers_[i] = 0.26f;
            }
        } else if (wantsUpgradeList && click && minusHovered) {
            if (player_.RefundStatUpgrade(i)) {
                ui_.upgradeHudRefundFlashTimers_[i] = 0.22f;
            } else {
                ui_.upgradeHudMissFlashTimers_[i] = 0.26f;
            }
        }

        const float flash = (std::min)(1.0f, ui_.upgradeHudFlashTimers_[i] / 0.22f);
        const float refundFlash = (std::min)(1.0f, ui_.upgradeHudRefundFlashTimers_[i] / 0.22f);
        const float missFlash = (std::min)(1.0f, ui_.upgradeHudMissFlashTimers_[i] / 0.26f);
        const cg2::Vector4 rowColor = UpgradeHudRowColors()[i];
        if (button) {
            if (maxed) {
                button->SetColor({0.12f, 0.12f, 0.14f, 0.72f});
            } else if (rowHovered) {
                button->SetColor({0.18f + flash * 0.18f + missFlash * 0.22f, 0.28f + flash * 0.30f, 0.30f + refundFlash * 0.20f, 0.92f});
            } else {
                button->SetColor({0.10f + flash * 0.25f + missFlash * 0.20f, 0.12f + flash * 0.36f, 0.15f + refundFlash * 0.22f, 0.84f});
            }
            button->Update();
        }
        if (minus) {
            if (player_.upgradeLevels_[i] <= 0) {
                minus->SetColor({0.12f + missFlash * 0.32f, 0.14f, 0.18f, 0.42f + missFlash * 0.32f});
            } else {
                minus->SetColor(LerpColor({rowColor.x * 0.45f, rowColor.y * 0.45f, rowColor.z * 0.45f, 0.72f}, {0.88f, 0.94f, 1.0f, 0.98f},
                                          minusHovered ? 0.62f + refundFlash * 0.25f : refundFlash * 0.35f));
            }
            minus->Update();
        }
        if (plus) {
            if (maxed) {
                plus->SetColor({0.18f + missFlash * 0.30f, 0.18f, 0.20f, 0.72f + missFlash * 0.20f});
            } else if (canUpgrade) {
                plus->SetColor(LerpColor({rowColor.x * 0.82f, rowColor.y * 0.82f, rowColor.z * 0.82f, 0.90f}, {1.0f, 1.0f, 1.0f, 1.0f},
                                         plusHovered ? 0.42f : flash * 0.35f));
            } else {
                plus->SetColor({0.15f + missFlash * 0.34f, 0.22f, 0.24f, 0.64f + missFlash * 0.25f});
            }
            plus->Update();
        }
    }

    if (ui_.upgradeHudBackdropSprite_)
        ui_.upgradeHudBackdropSprite_->Update();
    if (ui_.upgradeHudExpBackSprite_)
        ui_.upgradeHudExpBackSprite_->Update();
    if (ui_.upgradeHudExpFillSprite_)
        ui_.upgradeHudExpFillSprite_->Update();
    if (ui_.upgradeHudLevelBackSprite_)
        ui_.upgradeHudLevelBackSprite_->Update();
    if (ui_.upgradeHudLevelFillSprite_)
        ui_.upgradeHudLevelFillSprite_->Update();
}

void PlayerHud::DrawUpgradeHud()
{
    ui_.upgradeHudProfile_ = {};
    if (!ui_.arenaUiEnabled_ || !ui_.upgradeHudVisible_ || player_.isChangeMode || player_.isDead_) {
        return;
    }
    ui_.upgradeHudProfile_.visible = true;
    const auto totalStart = std::chrono::steady_clock::now();

    cg2::SpriteCommon* spriteCommon = cg2::SpriteCommon::GetInstance();
    const cg2::TextStyle smallStyle = MakeUpgradeHudSmallTextStyle();
    const cg2::TextStyle upgradeOverlayTextStyle = MakeUpgradeHudOverlayTextStyle();
    const cg2::TextStyle bottomBarTextStyle = MakeUpgradeHudBottomBarTextStyle();

    const int safeNextExp = (std::max)(1, player_.nextLevelExp_);
    const float expRatio = (std::clamp)(static_cast<float>(player_.exp_) / static_cast<float>(safeNextExp), 0.0f, 1.0f);
    const float levelRatio =
        (std::clamp)(static_cast<float>(player_.level_ - 1) / static_cast<float>((std::max)(1, player_.kMaxLevel - 1)), 0.0f, 1.0f);
    if (ui_.upgradeHudExpFillSprite_) {
        ui_.upgradeHudExpFillSprite_->SetSize({ui_.upgradeHudExpBarSize_.x * expRatio, ui_.upgradeHudExpBarSize_.y});
        ui_.upgradeHudExpFillSprite_->Update();
    }
    if (ui_.upgradeHudLevelFillSprite_) {
        ui_.upgradeHudLevelFillSprite_->SetSize({ui_.upgradeHudLevelBarSize_.x * levelRatio, ui_.upgradeHudLevelBarSize_.y});
        ui_.upgradeHudLevelFillSprite_->Update();
    }

    const bool showUpgradeList = ui_.upgradeHudListVisibility_ > 0.01f;
    const float listAlpha = (std::clamp)(ui_.upgradeHudListVisibility_, 0.0f, 1.0f);
    const float easedListAlpha = listAlpha * listAlpha * (3.0f - 2.0f * listAlpha);
    const float listOffsetX = -(1.0f - easedListAlpha) * ui_.upgradeHudListSlideDistance_;
    const auto& names = UpgradeHudNames();
    const std::string className = player_.GetCurrentClassName();
    const bool baseTextDirty = ui_.cachedUpgradeHudExp_ != player_.exp_ || ui_.cachedUpgradeHudNextExp_ != player_.nextLevelExp_ ||
                               ui_.cachedUpgradeHudLevel_ != player_.level_ || ui_.cachedUpgradeHudClassName_ != className;
    if (baseTextDirty) {
#if defined(USE_IMGUI) && !defined(NDEBUG)
        ui_.upgradeHudProfile_.baseTextRefreshed = true;
        cg2::TextLabel::ResetProfileStats();
        const auto baseTextRefreshStart = std::chrono::steady_clock::now();
        const auto expLabelRefreshStart = baseTextRefreshStart;
#endif
        UpdateUpgradeHudExpGlyphs("EXP " + std::to_string(player_.exp_) + " / " + std::to_string(player_.nextLevelExp_),
                                  bottomBarTextStyle);
#if defined(USE_IMGUI) && !defined(NDEBUG)
        const auto expLabelRefreshEnd = std::chrono::steady_clock::now();
        const auto levelLabelRefreshStart = expLabelRefreshEnd;
#endif
        UpdateUpgradeHudLevelLabels(player_.level_, className, bottomBarTextStyle);
#if defined(USE_IMGUI) && !defined(NDEBUG)
        const auto levelLabelRefreshEnd = std::chrono::steady_clock::now();
#endif
        ui_.cachedUpgradeHudExp_ = player_.exp_;
        ui_.cachedUpgradeHudNextExp_ = player_.nextLevelExp_;
        ui_.cachedUpgradeHudLevel_ = player_.level_;
        ui_.cachedUpgradeHudClassName_ = className;
#if defined(USE_IMGUI) && !defined(NDEBUG)
        const cg2::TextLabel::ProfileStats& textLabelStats = cg2::TextLabel::GetProfileStats();
        ui_.upgradeHudProfile_.baseTextRefreshMs =
            std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - baseTextRefreshStart).count();
        ui_.upgradeHudProfile_.expLabelRefreshMs =
            std::chrono::duration<float, std::milli>(expLabelRefreshEnd - expLabelRefreshStart).count();
        ui_.upgradeHudProfile_.levelLabelRefreshMs =
            std::chrono::duration<float, std::milli>(levelLabelRefreshEnd - levelLabelRefreshStart).count();
        ui_.upgradeHudProfile_.baseTextSetStyleMs = textLabelStats.setStyleCpuMs;
        ui_.upgradeHudProfile_.baseTextSetTextMs = textLabelStats.setTextCpuMs;
        ui_.upgradeHudProfile_.baseTextSetTextRebuildMs = textLabelStats.setTextRebuildCpuMs;
        ui_.upgradeHudProfile_.baseTextRebuildTextureMs = textLabelStats.rebuildTextureCpuMs;
        ui_.upgradeHudProfile_.baseTextGetOrCreateTextureMs = textLabelStats.getOrCreateTextureCpuMs;
        ui_.upgradeHudProfile_.baseTextSpriteSetTextureMs = textLabelStats.spriteSetTextureCpuMs;
        ui_.upgradeHudProfile_.baseTextCacheFileExistedCount = static_cast<int>(textLabelStats.cacheFileExistedCount);
        ui_.upgradeHudProfile_.baseTextGeneratedPngCount = static_cast<int>(textLabelStats.generatedPngCount);
#endif
    } else {
        PositionUpgradeHudExpGlyphs();
        PositionUpgradeHudLevelLabels();
    }

    if (showUpgradeList) {
        const bool listDirty = !ui_.cachedUpgradeHudListVisible_ || ui_.cachedUpgradeHudSkillPoints_ != player_.skillPoints_ ||
                               ui_.cachedUpgradeHudMaxEnhancePoint_ != player_.maxEnhancePoint ||
                               ui_.cachedUpgradeHudSegmentedBars_ != ui_.upgradeHudUseSegmentedUpgradeBars_ ||
                               ui_.cachedUpgradeHudLevels_ != player_.upgradeLevels_;
        if (listDirty) {
#if defined(USE_IMGUI) && !defined(NDEBUG)
            ui_.upgradeHudProfile_.listTextRefreshed = true;
            cg2::TextLabel::ResetProfileStats();
            const auto listTextRefreshStart = std::chrono::steady_clock::now();
#endif
            SetLabel(ui_.upgradeHudPointLabel_, spriteCommon, "x" + std::to_string(player_.skillPoints_),
                     {ui_.upgradeHudPointPos_.x + listOffsetX, ui_.upgradeHudPointPos_.y}, smallStyle);
            SetLabel(ui_.upgradeHudTitleLabel_, spriteCommon, "強化", {ui_.upgradeHudTitlePos_.x + listOffsetX, ui_.upgradeHudTitlePos_.y},
                     smallStyle);
            for (int i = 0; i < 7; ++i) {
                const float y = ui_.upgradeHudRowStart_.y + static_cast<float>(i) * ui_.upgradeHudRowGap_;
                const bool compactGauge = ui_.upgradeHudUseSegmentedUpgradeBars_;
                const float controlOffsetY =
                    compactGauge ? (std::max)(0.0f, (ui_.upgradeHudSegmentBarSize_.y - ui_.upgradeHudPlusSize_.y) * 0.5f) : 0.0f;
                const cg2::Vector2 namePosition =
                    compactGauge ? cg2::Vector2{ui_.upgradeHudRowStart_.x + ui_.upgradeHudSegmentBarSize_.x * 0.5f + listOffsetX,
                                                y + ui_.upgradeHudSegmentBarSize_.y * 0.5f}
                                 : cg2::Vector2{ui_.upgradeHudNameX_ + listOffsetX, y + ui_.upgradeHudNameTextOffsetY_};
                SetLabel(ui_.upgradeHudNameLabels_[i], spriteCommon, std::to_string(i + 1) + " " + names[i], namePosition,
                         compactGauge ? upgradeOverlayTextStyle : smallStyle);
                ui_.upgradeHudNameLabels_[i]->SetAnchorPoint(compactGauge ? cg2::Vector2{0.5f, 0.5f} : cg2::Vector2{0.0f, 0.0f});
                SetLabel(ui_.upgradeHudLevelLabels_[i], spriteCommon, "Lv." + std::to_string(player_.upgradeLevels_[i]),
                         {ui_.upgradeHudLevelX_ + listOffsetX, y + ui_.upgradeHudLevelTextOffsetY_}, smallStyle);
                SetLabel(ui_.upgradeHudMinusLabels_[i], spriteCommon, "-",
                         compactGauge ? cg2::Vector2{ui_.upgradeHudMinusX_ + ui_.upgradeHudPlusSize_.x * 0.5f + listOffsetX,
                                                     y + controlOffsetY + ui_.upgradeHudPlusSize_.y * 0.5f}
                                      : cg2::Vector2{ui_.upgradeHudMinusLabelX_ + listOffsetX, y + ui_.upgradeHudMinusTextOffsetY_},
                         compactGauge ? upgradeOverlayTextStyle : smallStyle);
                ui_.upgradeHudMinusLabels_[i]->SetAnchorPoint(compactGauge ? cg2::Vector2{0.5f, 0.5f} : cg2::Vector2{0.0f, 0.0f});
                SetLabel(ui_.upgradeHudPlusLabels_[i], spriteCommon, player_.upgradeLevels_[i] >= player_.maxEnhancePoint ? "済" : "+",
                         compactGauge ? cg2::Vector2{ui_.upgradeHudPlusX_ + ui_.upgradeHudPlusSize_.x * 0.5f + listOffsetX,
                                                     y + controlOffsetY + ui_.upgradeHudPlusSize_.y * 0.5f}
                                      : cg2::Vector2{ui_.upgradeHudPlusLabelX_ + listOffsetX, y + ui_.upgradeHudPlusTextOffsetY_},
                         compactGauge ? upgradeOverlayTextStyle : smallStyle);
                ui_.upgradeHudPlusLabels_[i]->SetAnchorPoint(compactGauge ? cg2::Vector2{0.5f, 0.5f} : cg2::Vector2{0.0f, 0.0f});
            }
            ui_.cachedUpgradeHudSkillPoints_ = player_.skillPoints_;
            ui_.cachedUpgradeHudMaxEnhancePoint_ = player_.maxEnhancePoint;
            ui_.cachedUpgradeHudSegmentedBars_ = ui_.upgradeHudUseSegmentedUpgradeBars_;
            ui_.cachedUpgradeHudLevels_ = player_.upgradeLevels_;
#if defined(USE_IMGUI) && !defined(NDEBUG)
            const cg2::TextLabel::ProfileStats& textLabelStats = cg2::TextLabel::GetProfileStats();
            ui_.upgradeHudProfile_.listTextRefreshMs =
                std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - listTextRefreshStart).count();
            ui_.upgradeHudProfile_.listTextSetStyleMs = textLabelStats.setStyleCpuMs;
            ui_.upgradeHudProfile_.listTextSetTextMs = textLabelStats.setTextCpuMs;
            ui_.upgradeHudProfile_.listTextSetTextRebuildMs = textLabelStats.setTextRebuildCpuMs;
            ui_.upgradeHudProfile_.listTextRebuildTextureMs = textLabelStats.rebuildTextureCpuMs;
            ui_.upgradeHudProfile_.listTextGetOrCreateTextureMs = textLabelStats.getOrCreateTextureCpuMs;
            ui_.upgradeHudProfile_.listTextSpriteSetTextureMs = textLabelStats.spriteSetTextureCpuMs;
            ui_.upgradeHudProfile_.listTextCacheFileExistedCount = static_cast<int>(textLabelStats.cacheFileExistedCount);
            ui_.upgradeHudProfile_.listTextGeneratedPngCount = static_cast<int>(textLabelStats.generatedPngCount);
#endif
        } else {
            if (ui_.upgradeHudPointLabel_)
                ui_.upgradeHudPointLabel_->SetPosition({ui_.upgradeHudPointPos_.x + listOffsetX, ui_.upgradeHudPointPos_.y});
            if (ui_.upgradeHudTitleLabel_)
                ui_.upgradeHudTitleLabel_->SetPosition({ui_.upgradeHudTitlePos_.x + listOffsetX, ui_.upgradeHudTitlePos_.y});
            for (int i = 0; i < 7; ++i) {
                const float y = ui_.upgradeHudRowStart_.y + static_cast<float>(i) * ui_.upgradeHudRowGap_;
                const float controlOffsetY = ui_.upgradeHudUseSegmentedUpgradeBars_
                                                 ? (std::max)(0.0f, (ui_.upgradeHudSegmentBarSize_.y - ui_.upgradeHudPlusSize_.y) * 0.5f)
                                                 : 0.0f;
                if (ui_.upgradeHudNameLabels_[i]) {
                    ui_.upgradeHudNameLabels_[i]->SetPosition(
                        ui_.upgradeHudUseSegmentedUpgradeBars_
                            ? cg2::Vector2{ui_.upgradeHudRowStart_.x + ui_.upgradeHudSegmentBarSize_.x * 0.5f + listOffsetX,
                                           y + ui_.upgradeHudSegmentBarSize_.y * 0.5f}
                            : cg2::Vector2{ui_.upgradeHudNameX_ + listOffsetX, y + ui_.upgradeHudNameTextOffsetY_});
                }
                if (ui_.upgradeHudLevelLabels_[i])
                    ui_.upgradeHudLevelLabels_[i]->SetPosition({ui_.upgradeHudLevelX_ + listOffsetX, y + ui_.upgradeHudLevelTextOffsetY_});
                if (ui_.upgradeHudMinusLabels_[i])
                    ui_.upgradeHudMinusLabels_[i]->SetPosition(
                        ui_.upgradeHudUseSegmentedUpgradeBars_
                            ? cg2::Vector2{ui_.upgradeHudMinusX_ + ui_.upgradeHudPlusSize_.x * 0.5f + listOffsetX,
                                           y + controlOffsetY + ui_.upgradeHudPlusSize_.y * 0.5f}
                            : cg2::Vector2{ui_.upgradeHudMinusLabelX_ + listOffsetX, y + ui_.upgradeHudMinusTextOffsetY_});
                if (ui_.upgradeHudPlusLabels_[i])
                    ui_.upgradeHudPlusLabels_[i]->SetPosition(
                        ui_.upgradeHudUseSegmentedUpgradeBars_
                            ? cg2::Vector2{ui_.upgradeHudPlusX_ + ui_.upgradeHudPlusSize_.x * 0.5f + listOffsetX,
                                           y + controlOffsetY + ui_.upgradeHudPlusSize_.y * 0.5f}
                            : cg2::Vector2{ui_.upgradeHudPlusLabelX_ + listOffsetX, y + ui_.upgradeHudPlusTextOffsetY_});
            }
        }
        if (ui_.upgradeHudPointLabel_)
            ui_.upgradeHudPointLabel_->SetAlpha(easedListAlpha);
        if (ui_.upgradeHudTitleLabel_)
            ui_.upgradeHudTitleLabel_->SetAlpha(easedListAlpha);
        for (int i = 0; i < 7; ++i) {
            if (ui_.upgradeHudNameLabels_[i])
                ui_.upgradeHudNameLabels_[i]->SetAlpha(easedListAlpha);
            if (ui_.upgradeHudLevelLabels_[i])
                ui_.upgradeHudLevelLabels_[i]->SetAlpha(easedListAlpha);
            if (ui_.upgradeHudMinusLabels_[i])
                ui_.upgradeHudMinusLabels_[i]->SetAlpha(easedListAlpha);
            if (ui_.upgradeHudPlusLabels_[i])
                ui_.upgradeHudPlusLabels_[i]->SetAlpha(easedListAlpha);
        }
    }
    ui_.cachedUpgradeHudListVisible_ = showUpgradeList;

    const auto spriteStart = std::chrono::steady_clock::now();
    if (ui_.upgradeHudUseRectBatch_) {
        DrawUpgradeHudRectBatch(showUpgradeList, expRatio, levelRatio, easedListAlpha, listOffsetX);
    } else {
        cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
        if (showUpgradeList && ui_.upgradeHudDrawListPanels_) {
            if (!ui_.upgradeHudUseSegmentedUpgradeBars_ && ui_.upgradeHudBackdropSprite_) {
                ui_.upgradeHudBackdropSprite_->Draw();
                ++ui_.upgradeHudProfile_.spriteDraws;
            }
            for (int i = 0; i < 7; ++i) {
                if (!ui_.upgradeHudUseSegmentedUpgradeBars_ && ui_.upgradeHudButtonSprites_[i]) {
                    ui_.upgradeHudButtonSprites_[i]->Draw();
                    ++ui_.upgradeHudProfile_.spriteDraws;
                }
                if (!ui_.upgradeHudUseSegmentedUpgradeBars_ && ui_.upgradeHudMinusSprites_[i]) {
                    ui_.upgradeHudMinusSprites_[i]->Draw();
                    ++ui_.upgradeHudProfile_.spriteDraws;
                }
                if (!ui_.upgradeHudUseSegmentedUpgradeBars_ && ui_.upgradeHudPlusSprites_[i]) {
                    ui_.upgradeHudPlusSprites_[i]->Draw();
                    ++ui_.upgradeHudProfile_.spriteDraws;
                }
            }
        }
        if (ui_.upgradeHudDrawBottomBars_ && !ui_.upgradeHudUseNeonProgressBars_) {
            if (ui_.upgradeHudLevelBackSprite_) {
                ui_.upgradeHudLevelBackSprite_->Draw();
                ++ui_.upgradeHudProfile_.spriteDraws;
            }
            if (ui_.upgradeHudLevelFillSprite_) {
                ui_.upgradeHudLevelFillSprite_->Draw();
                ++ui_.upgradeHudProfile_.spriteDraws;
            }
            if (ui_.upgradeHudExpBackSprite_) {
                ui_.upgradeHudExpBackSprite_->Draw();
                ++ui_.upgradeHudProfile_.spriteDraws;
            }
            if (ui_.upgradeHudExpFillSprite_) {
                ui_.upgradeHudExpFillSprite_->Draw();
                ++ui_.upgradeHudProfile_.spriteDraws;
            }
        }
    }
    if (showUpgradeList && ui_.upgradeHudDrawListPanels_ && ui_.upgradeHudUseSegmentedUpgradeBars_) {
        cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
        for (int i = 0; i < 7; ++i) {
            if (ui_.upgradeHudMinusSprites_[i]) {
                ui_.upgradeHudMinusSprites_[i]->Draw();
                ++ui_.upgradeHudProfile_.spriteDraws;
            }
            if (ui_.upgradeHudPlusSprites_[i]) {
                ui_.upgradeHudPlusSprites_[i]->Draw();
                ++ui_.upgradeHudProfile_.spriteDraws;
            }
        }
    }
    const auto spriteEnd = std::chrono::steady_clock::now();

    const auto textStart = std::chrono::steady_clock::now();
    if (showUpgradeList && ui_.upgradeHudUseSegmentedUpgradeBars_) {
        cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
        for (int i = 0; i < 7; ++i) {
            if (ui_.upgradeHudSegmentBars_[i]) {
                ui_.upgradeHudSegmentBars_[i]->Draw();
            }
        }
        ui_.upgradeHudProfile_.spriteDraws += 7 * 11;
    }
    if (ui_.upgradeHudDrawBottomBars_ && ui_.upgradeHudUseNeonProgressBars_) {
        cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
        if (ui_.upgradeHudLevelProgressBar_)
            ui_.upgradeHudLevelProgressBar_->Draw();
        if (ui_.upgradeHudExpProgressBar_)
            ui_.upgradeHudExpProgressBar_->Draw();
        ui_.upgradeHudProfile_.spriteDraws += 10;
    }
    cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
    if (showUpgradeList && ui_.upgradeHudDrawListText_) {
        if (ui_.upgradeHudTitleLabel_) {
            ui_.upgradeHudTitleLabel_->Draw();
            ++ui_.upgradeHudProfile_.textDraws;
        }
        if (ui_.upgradeHudPointLabel_) {
            ui_.upgradeHudPointLabel_->Draw();
            ++ui_.upgradeHudProfile_.textDraws;
        }
        for (int i = 0; i < 7; ++i) {
            if (ui_.upgradeHudNameLabels_[i]) {
                ui_.upgradeHudNameLabels_[i]->Draw();
                ++ui_.upgradeHudProfile_.textDraws;
            }
            if (!ui_.upgradeHudUseSegmentedUpgradeBars_ && ui_.upgradeHudLevelLabels_[i]) {
                ui_.upgradeHudLevelLabels_[i]->Draw();
                ++ui_.upgradeHudProfile_.textDraws;
            }
            if (ui_.upgradeHudMinusLabels_[i]) {
                ui_.upgradeHudMinusLabels_[i]->Draw();
                ++ui_.upgradeHudProfile_.textDraws;
            }
            if (ui_.upgradeHudPlusLabels_[i]) {
                ui_.upgradeHudPlusLabels_[i]->Draw();
                ++ui_.upgradeHudProfile_.textDraws;
            }
        }
    }
    if (ui_.upgradeHudDrawBottomText_) {
        if (ui_.upgradeHudLevelLabel_) {
            ui_.upgradeHudLevelLabel_->Draw();
            ++ui_.upgradeHudProfile_.textDraws;
        }
        for (size_t i = 0; i < ui_.upgradeHudLevelGlyphCount_; ++i) {
            if (ui_.upgradeHudLevelGlyphLabels_[i]) {
                ui_.upgradeHudLevelGlyphLabels_[i]->Draw();
                ++ui_.upgradeHudProfile_.textDraws;
            }
        }
        if (ui_.upgradeHudLevelClassLabel_) {
            ui_.upgradeHudLevelClassLabel_->Draw();
            ++ui_.upgradeHudProfile_.textDraws;
        }
        for (size_t i = 0; i < ui_.upgradeHudExpGlyphCount_; ++i) {
            if (ui_.upgradeHudExpGlyphLabels_[i]) {
                ui_.upgradeHudExpGlyphLabels_[i]->Draw();
                ++ui_.upgradeHudProfile_.textDraws;
            }
        }
    }
    const auto textEnd = std::chrono::steady_clock::now();
    const auto totalEnd = std::chrono::steady_clock::now();

    ui_.upgradeHudProfile_.spriteMs = std::chrono::duration<float, std::milli>(spriteEnd - spriteStart).count();
    ui_.upgradeHudProfile_.textMs = std::chrono::duration<float, std::milli>(textEnd - textStart).count();
    ui_.upgradeHudProfile_.updateMs = std::chrono::duration<float, std::milli>(spriteStart - totalStart).count();
    ui_.upgradeHudProfile_.totalMs = std::chrono::duration<float, std::milli>(totalEnd - totalStart).count();
}

void PlayerHud::AppendGameplayNeonTextLabels(std::vector<cg2::TextLabel*>& labels) const
{
    if (player_.isChangeMode) {
        return;
    }
    if (ui_.upgradeHudDrawBottomText_ && ui_.upgradeHudLevelLabel_) {
        labels.push_back(ui_.upgradeHudLevelLabel_.get());
    }
    if (ui_.upgradeHudDrawBottomText_) {
        for (size_t i = 0; i < ui_.upgradeHudLevelGlyphCount_; ++i) {
            if (ui_.upgradeHudLevelGlyphLabels_[i]) {
                labels.push_back(ui_.upgradeHudLevelGlyphLabels_[i].get());
            }
        }
        if (ui_.upgradeHudLevelClassLabel_) {
            labels.push_back(ui_.upgradeHudLevelClassLabel_.get());
        }
    }
    if (ui_.upgradeHudDrawBottomText_) {
        for (size_t i = 0; i < ui_.upgradeHudExpGlyphCount_; ++i) {
            if (ui_.upgradeHudExpGlyphLabels_[i]) {
                labels.push_back(ui_.upgradeHudExpGlyphLabels_[i].get());
            }
        }
    }
    if (ui_.upgradeHudListTextBloomEnabled_ && ui_.upgradeHudListVisibility_ > 0.01f && ui_.upgradeHudDrawListText_) {
        if (ui_.upgradeHudTitleLabel_) {
            labels.push_back(ui_.upgradeHudTitleLabel_.get());
        }
        if (ui_.upgradeHudPointLabel_) {
            labels.push_back(ui_.upgradeHudPointLabel_.get());
        }
        // 段数バーの上に重なる文字と操作記号だけは、黒アウトラインを保った
        // ままネオンBloomの対象にする。タイトルや通常説明文には影響しない。
        if (ui_.upgradeHudUseSegmentedUpgradeBars_) {
            for (int i = 0; i < 7; ++i) {
                if (ui_.upgradeHudNameLabels_[i]) {
                    labels.push_back(ui_.upgradeHudNameLabels_[i].get());
                }
                if (ui_.upgradeHudMinusLabels_[i]) {
                    labels.push_back(ui_.upgradeHudMinusLabels_[i].get());
                }
                if (ui_.upgradeHudPlusLabels_[i]) {
                    labels.push_back(ui_.upgradeHudPlusLabels_[i].get());
                }
            }
        }
    }
}

void PlayerHud::QueueUpgradeHudRect(std::vector<cg2::TrailVertex>& vertices, const cg2::Vector2& pos, const cg2::Vector2& size,
                                    const cg2::Vector4& color) const
{
    const float left = pos.x;
    const float top = pos.y;
    const float right = pos.x + size.x;
    const float bottom = pos.y + size.y;
    const cg2::Vector3 p0{left, bottom, 0.0f};
    const cg2::Vector3 p1{left, top, 0.0f};
    const cg2::Vector3 p2{right, bottom, 0.0f};
    const cg2::Vector3 p3{right, top, 0.0f};

    vertices.push_back({p0, color, {0.0f, 1.0f}});
    vertices.push_back({p1, color, {0.0f, 0.0f}});
    vertices.push_back({p2, color, {1.0f, 1.0f}});
    vertices.push_back({p1, color, {0.0f, 0.0f}});
    vertices.push_back({p3, color, {1.0f, 0.0f}});
    vertices.push_back({p2, color, {1.0f, 1.0f}});
}

void PlayerHud::DrawUpgradeHudRectBatch(bool showUpgradeList, float expRatio, float levelRatio, float listAlpha, float listOffsetX)
{
    if (!ui_.upgradeHudBatchVertexData_ || !ui_.upgradeHudBatchTransformData_ || !ui_.upgradeHudBatchMaterialData_) {
        return;
    }

    std::vector<cg2::TrailVertex> vertices;
    vertices.reserve(ui_.kUpgradeHudBatchMaxVertices);
    auto withListAlpha = [listAlpha](const cg2::Vector4& color) {
        return cg2::Vector4{color.x, color.y, color.z, color.w * listAlpha};
    };
    auto offsetListPos = [listOffsetX](cg2::Vector2 pos) {
        pos.x += listOffsetX;
        return pos;
    };

    if (showUpgradeList && ui_.upgradeHudDrawListPanels_) {
        if (!ui_.upgradeHudUseSegmentedUpgradeBars_) {
            QueueUpgradeHudRect(vertices, offsetListPos(ui_.upgradeHudPanelPos_), ui_.upgradeHudPanelSize_,
                                withListAlpha({0.03f, 0.04f, 0.06f, 0.58f}));
        }
        for (int i = 0; i < 7; ++i) {
            const float y = ui_.upgradeHudRowStart_.y + static_cast<float>(i) * ui_.upgradeHudRowGap_;
            float flash = (std::min)(1.0f, ui_.upgradeHudFlashTimers_[i] / 0.22f);
            const float refundFlash = (std::min)(1.0f, ui_.upgradeHudRefundFlashTimers_[i] / 0.22f);
            const float missFlash = (std::min)(1.0f, ui_.upgradeHudMissFlashTimers_[i] / 0.26f);
            const cg2::Vector4 rowColor = UpgradeHudRowColors()[i];
            const cg2::Vector4 buttonColor =
                LerpColor({0.10f + missFlash * 0.20f, 0.12f, 0.15f + refundFlash * 0.16f, 0.84f}, {0.35f, 0.90f, 0.72f, 0.96f}, flash);
            const cg2::Vector4 minusColor = player_.upgradeLevels_[i] > 0
                                                ? LerpColor({rowColor.x * 0.45f, rowColor.y * 0.45f, rowColor.z * 0.45f, 0.72f},
                                                            {0.88f, 0.94f, 1.0f, 0.98f}, refundFlash)
                                                : cg2::Vector4{0.12f + missFlash * 0.30f, 0.14f, 0.18f, 0.42f + missFlash * 0.28f};
            const cg2::Vector4 plusColor =
                player_.skillPoints_ > 0
                    ? LerpColor({rowColor.x * 0.82f, rowColor.y * 0.82f, rowColor.z * 0.82f, 0.90f}, {1.0f, 1.0f, 1.0f, 1.0f}, flash)
                    : cg2::Vector4{0.18f + missFlash * 0.32f, 0.22f, 0.24f, 0.48f + missFlash * 0.28f};
            if (!ui_.upgradeHudUseSegmentedUpgradeBars_) {
                QueueUpgradeHudRect(vertices, offsetListPos({ui_.upgradeHudRowStart_.x, y}), ui_.upgradeHudButtonSize_,
                                    withListAlpha(buttonColor));
            }
            if (!ui_.upgradeHudUseSegmentedUpgradeBars_) {
                QueueUpgradeHudRect(vertices, offsetListPos({ui_.upgradeHudMinusX_, y}), ui_.upgradeHudPlusSize_,
                                    withListAlpha(minusColor));
                QueueUpgradeHudRect(vertices, offsetListPos({ui_.upgradeHudPlusX_, y}), ui_.upgradeHudPlusSize_, withListAlpha(plusColor));
            }
        }
    }

    if (ui_.upgradeHudDrawBottomBars_ && !ui_.upgradeHudUseNeonProgressBars_) {
        QueueUpgradeHudRect(vertices, ui_.upgradeHudLevelBarPos_, ui_.upgradeHudLevelBarSize_, {0.04f, 0.04f, 0.05f, 0.82f});
        QueueUpgradeHudRect(vertices, ui_.upgradeHudLevelBarPos_,
                            {ui_.upgradeHudLevelBarSize_.x * levelRatio, ui_.upgradeHudLevelBarSize_.y}, {0.36f, 1.0f, 0.56f, 0.92f});
        QueueUpgradeHudRect(vertices, ui_.upgradeHudExpBarPos_, ui_.upgradeHudExpBarSize_, {0.04f, 0.04f, 0.05f, 0.82f});
        QueueUpgradeHudRect(vertices, ui_.upgradeHudExpBarPos_, {ui_.upgradeHudExpBarSize_.x * expRatio, ui_.upgradeHudExpBarSize_.y},
                            {0.96f, 0.83f, 0.24f, 0.95f});
    }

    if (vertices.empty()) {
        return;
    }
    if (vertices.size() > ui_.kUpgradeHudBatchMaxVertices) {
        vertices.resize(ui_.kUpgradeHudBatchMaxVertices);
    }
    std::memcpy(ui_.upgradeHudBatchVertexData_, vertices.data(), sizeof(cg2::TrailVertex) * vertices.size());
    *ui_.upgradeHudBatchTransformData_ =
        cg2::MakeOrthographicMatrix(0.0f, 0.0f, float(cg2::WinApp::kClientWidth), float(cg2::WinApp::kClientHeight), 0.0f, 100.0f);

    cg2::DirectXCommon* dxCommon = cg2::SpriteCommon::GetInstance()->GetDxCommon();
    ID3D12GraphicsCommandList* commandList = dxCommon->GetList().Get();
    cg2::TextureManager::GetInstance()->PreDraw();
    commandList->SetGraphicsRootSignature(dxCommon->GetPSOHudRect().root_.GetSignature().Get());
    commandList->SetPipelineState(dxCommon->GetPSOHudRect().graphicsState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &ui_.upgradeHudBatchVertexBufferView_);
    commandList->SetGraphicsRootConstantBufferView(0, ui_.upgradeHudBatchMaterialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, ui_.upgradeHudBatchTransformResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(2, cg2::TextureManager::GetInstance()->GetSrvHandleGPU("resources/white512x512.png"));
    commandList->DrawInstanced(static_cast<UINT>(vertices.size()), 1, 0, 0);
    ui_.upgradeHudProfile_.spriteDraws += 1;
}

void PlayerHud::ApplyUpgradeHudLayout()
{
    if (ui_.upgradeHudBackdropSprite_) {
        ui_.upgradeHudBackdropSprite_->SetPosition(ui_.upgradeHudPanelPos_);
        ui_.upgradeHudBackdropSprite_->SetSize(ui_.upgradeHudPanelSize_);
    }
    if (ui_.upgradeHudExpBackSprite_) {
        ui_.upgradeHudExpBackSprite_->SetPosition(ui_.upgradeHudExpBarPos_);
        ui_.upgradeHudExpBackSprite_->SetSize(ui_.upgradeHudExpBarSize_);
    }
    if (ui_.upgradeHudExpFillSprite_) {
        ui_.upgradeHudExpFillSprite_->SetPosition(ui_.upgradeHudExpBarPos_);
    }
    if (ui_.upgradeHudLevelBackSprite_) {
        ui_.upgradeHudLevelBackSprite_->SetPosition(ui_.upgradeHudLevelBarPos_);
        ui_.upgradeHudLevelBackSprite_->SetSize(ui_.upgradeHudLevelBarSize_);
    }
    if (ui_.upgradeHudLevelFillSprite_) {
        ui_.upgradeHudLevelFillSprite_->SetPosition(ui_.upgradeHudLevelBarPos_);
    }
    for (int i = 0; i < 7; ++i) {
        const float y = ui_.upgradeHudRowStart_.y + static_cast<float>(i) * ui_.upgradeHudRowGap_;
        if (ui_.upgradeHudButtonSprites_[i]) {
            ui_.upgradeHudButtonSprites_[i]->SetPosition({ui_.upgradeHudRowStart_.x, y});
            ui_.upgradeHudButtonSprites_[i]->SetSize(ui_.upgradeHudButtonSize_);
        }
        if (ui_.upgradeHudMinusSprites_[i]) {
            ui_.upgradeHudMinusSprites_[i]->SetPosition({ui_.upgradeHudMinusX_, y});
            ui_.upgradeHudMinusSprites_[i]->SetSize(ui_.upgradeHudPlusSize_);
        }
        if (ui_.upgradeHudPlusSprites_[i]) {
            ui_.upgradeHudPlusSprites_[i]->SetPosition({ui_.upgradeHudPlusX_, y});
            ui_.upgradeHudPlusSprites_[i]->SetSize(ui_.upgradeHudPlusSize_);
        }
    }
}

bool PlayerHud::LoadUpgradeHudConfig(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        ui_.upgradeHudConfigStatus_ = "HUD設定ファイルが見つからないため初期値を使用します。";
        return false;
    }
    nlohmann::json json{};
    try {
        file >> json;
    }
    catch (...) {
        ui_.upgradeHudConfigStatus_ = "HUD設定JSONの読み込みに失敗しました。";
        return false;
    }
    if (!json.is_object()) {
        return false;
    }
    ui_.upgradeHudVisible_ = json.value("visible", ui_.upgradeHudVisible_);
    ui_.upgradeHudHideListWithoutPoints_ = json.value("hideListWithoutPoints", ui_.upgradeHudHideListWithoutPoints_);
    ui_.upgradeHudDrawListPanels_ = json.value("drawListPanels", ui_.upgradeHudDrawListPanels_);
    ui_.upgradeHudDrawListText_ = json.value("drawListText", ui_.upgradeHudDrawListText_);
    ui_.upgradeHudDrawBottomBars_ = json.value("drawBottomBars", ui_.upgradeHudDrawBottomBars_);
    ui_.upgradeHudDrawBottomText_ = json.value("drawBottomText", ui_.upgradeHudDrawBottomText_);
    ui_.upgradeHudUseRectBatch_ = json.value("useRectBatch", ui_.upgradeHudUseRectBatch_);
    ui_.upgradeHudRoundedProgressBars_ = json.value("roundedProgressBars", ui_.upgradeHudRoundedProgressBars_);
    ui_.upgradeHudUseSegmentedUpgradeBars_ = json.value("segmentedUpgradeBars", ui_.upgradeHudUseSegmentedUpgradeBars_);
    ui_.upgradeHudListTextBloomEnabled_ = json.value("listTextBloom", false);
    player_.maxEnhancePoint = (std::clamp)(json.value("maxEnhancePoint", player_.maxEnhancePoint), 1, 10);
    ui_.upgradeHudListAnimSpeed_ = json.value("listAnimSpeed", ui_.upgradeHudListAnimSpeed_);
    ui_.upgradeHudListSlideDistance_ = json.value("listSlideDistance", ui_.upgradeHudListSlideDistance_);
    ui_.upgradeHudPanelPos_ = ReadVector2Object(json.value("panelPos", nlohmann::json::object()), ui_.upgradeHudPanelPos_);
    ui_.upgradeHudPanelSize_ = ReadVector2Object(json.value("panelSize", nlohmann::json::object()), ui_.upgradeHudPanelSize_);
    ui_.upgradeHudRowStart_ = ReadVector2Object(json.value("rowStart", nlohmann::json::object()), ui_.upgradeHudRowStart_);
    ui_.upgradeHudButtonSize_ = ReadVector2Object(json.value("buttonSize", nlohmann::json::object()), ui_.upgradeHudButtonSize_);
    ui_.upgradeHudPlusSize_ = ReadVector2Object(json.value("plusSize", nlohmann::json::object()), ui_.upgradeHudPlusSize_);
    ui_.upgradeHudSegmentBarOffset_ =
        ReadVector2Object(json.value("segmentBarOffset", nlohmann::json::object()), ui_.upgradeHudSegmentBarOffset_);
    ui_.upgradeHudSegmentBarSize_ =
        ReadVector2Object(json.value("segmentBarSize", nlohmann::json::object()), ui_.upgradeHudSegmentBarSize_);
    ui_.upgradeHudRowGap_ = json.value("rowGap", ui_.upgradeHudRowGap_);
    ui_.upgradeHudNameX_ = json.value("nameX", ui_.upgradeHudNameX_);
    ui_.upgradeHudLevelX_ = json.value("levelX", ui_.upgradeHudLevelX_);
    ui_.upgradeHudMinusX_ = json.value("minusX", ui_.upgradeHudMinusX_);
    ui_.upgradeHudPlusX_ = json.value("plusX", ui_.upgradeHudPlusX_);
    ui_.upgradeHudMinusLabelX_ = json.value("minusLabelX", ui_.upgradeHudMinusLabelX_);
    ui_.upgradeHudPlusLabelX_ = json.value("plusLabelX", ui_.upgradeHudPlusLabelX_);
    ui_.upgradeHudNameTextOffsetY_ = json.value("nameTextOffsetY", ui_.upgradeHudNameTextOffsetY_);
    ui_.upgradeHudLevelTextOffsetY_ = json.value("levelTextOffsetY", ui_.upgradeHudLevelTextOffsetY_);
    ui_.upgradeHudMinusTextOffsetY_ = json.value("minusTextOffsetY", ui_.upgradeHudMinusTextOffsetY_);
    ui_.upgradeHudPlusTextOffsetY_ = json.value("plusTextOffsetY", ui_.upgradeHudPlusTextOffsetY_);
    ui_.upgradeHudTitlePos_ = ReadVector2Object(json.value("titlePos", nlohmann::json::object()), ui_.upgradeHudTitlePos_);
    ui_.upgradeHudPointPos_ = ReadVector2Object(json.value("pointPos", nlohmann::json::object()), ui_.upgradeHudPointPos_);
    ui_.upgradeHudLevelBarPos_ = ReadVector2Object(json.value("levelBarPos", nlohmann::json::object()), ui_.upgradeHudLevelBarPos_);
    ui_.upgradeHudLevelBarSize_ = ReadVector2Object(json.value("levelBarSize", nlohmann::json::object()), ui_.upgradeHudLevelBarSize_);
    ui_.upgradeHudLevelTextPos_ = ReadVector2Object(json.value("levelTextPos", nlohmann::json::object()), ui_.upgradeHudLevelTextPos_);
    ui_.upgradeHudExpBarPos_ = ReadVector2Object(json.value("expBarPos", nlohmann::json::object()), ui_.upgradeHudExpBarPos_);
    ui_.upgradeHudExpBarSize_ = ReadVector2Object(json.value("expBarSize", nlohmann::json::object()), ui_.upgradeHudExpBarSize_);
    ui_.upgradeHudExpTextPos_ = ReadVector2Object(json.value("expTextPos", nlohmann::json::object()), ui_.upgradeHudExpTextPos_);
    ApplyUpgradeHudLayout();
    ApplyUpgradeHudProgressBarStyles();
    ui_.upgradeHudConfigStatus_ = "HUD設定を読み込みました: " + path;
    return true;
}

bool PlayerHud::SaveUpgradeHudConfig(const std::string& path) const
{
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    nlohmann::json json = {{"version", 1},
                           {"visible", ui_.upgradeHudVisible_},
                           {"hideListWithoutPoints", ui_.upgradeHudHideListWithoutPoints_},
                           {"drawListPanels", ui_.upgradeHudDrawListPanels_},
                           {"drawListText", ui_.upgradeHudDrawListText_},
                           {"drawBottomBars", ui_.upgradeHudDrawBottomBars_},
                           {"drawBottomText", ui_.upgradeHudDrawBottomText_},
                           {"useRectBatch", ui_.upgradeHudUseRectBatch_},
                           {"roundedProgressBars", ui_.upgradeHudRoundedProgressBars_},
                           {"segmentedUpgradeBars", ui_.upgradeHudUseSegmentedUpgradeBars_},
                           {"listTextBloom", ui_.upgradeHudListTextBloomEnabled_},
                           {"maxEnhancePoint", player_.maxEnhancePoint},
                           {"listAnimSpeed", ui_.upgradeHudListAnimSpeed_},
                           {"listSlideDistance", ui_.upgradeHudListSlideDistance_},
                           {"panelPos", WriteVector2Object(ui_.upgradeHudPanelPos_)},
                           {"panelSize", WriteVector2Object(ui_.upgradeHudPanelSize_)},
                           {"rowStart", WriteVector2Object(ui_.upgradeHudRowStart_)},
                           {"buttonSize", WriteVector2Object(ui_.upgradeHudButtonSize_)},
                           {"plusSize", WriteVector2Object(ui_.upgradeHudPlusSize_)},
                           {"segmentBarOffset", WriteVector2Object(ui_.upgradeHudSegmentBarOffset_)},
                           {"segmentBarSize", WriteVector2Object(ui_.upgradeHudSegmentBarSize_)},
                           {"rowGap", ui_.upgradeHudRowGap_},
                           {"nameX", ui_.upgradeHudNameX_},
                           {"levelX", ui_.upgradeHudLevelX_},
                           {"minusX", ui_.upgradeHudMinusX_},
                           {"plusX", ui_.upgradeHudPlusX_},
                           {"minusLabelX", ui_.upgradeHudMinusLabelX_},
                           {"plusLabelX", ui_.upgradeHudPlusLabelX_},
                           {"nameTextOffsetY", ui_.upgradeHudNameTextOffsetY_},
                           {"levelTextOffsetY", ui_.upgradeHudLevelTextOffsetY_},
                           {"minusTextOffsetY", ui_.upgradeHudMinusTextOffsetY_},
                           {"plusTextOffsetY", ui_.upgradeHudPlusTextOffsetY_},
                           {"titlePos", WriteVector2Object(ui_.upgradeHudTitlePos_)},
                           {"pointPos", WriteVector2Object(ui_.upgradeHudPointPos_)},
                           {"levelBarPos", WriteVector2Object(ui_.upgradeHudLevelBarPos_)},
                           {"levelBarSize", WriteVector2Object(ui_.upgradeHudLevelBarSize_)},
                           {"levelTextPos", WriteVector2Object(ui_.upgradeHudLevelTextPos_)},
                           {"expBarPos", WriteVector2Object(ui_.upgradeHudExpBarPos_)},
                           {"expBarSize", WriteVector2Object(ui_.upgradeHudExpBarSize_)},
                           {"expTextPos", WriteVector2Object(ui_.upgradeHudExpTextPos_)}};
    file << json.dump(2);
    return true;
}

void PlayerHud::DrawUpgradeHudDebugImGui()
{
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("プレイヤー強化HUD", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    ImGui::Checkbox("HUDを表示", &ui_.upgradeHudVisible_);
    ImGui::Checkbox("スキルポイントがない時は強化リストを隠す", &ui_.upgradeHudHideListWithoutPoints_);
    ImGui::Checkbox("強化リスト背景/ボタンを描画", &ui_.upgradeHudDrawListPanels_);
    ImGui::Checkbox("強化リスト文字を描画", &ui_.upgradeHudDrawListText_);
#if !defined(NDEBUG)
    ImGui::Checkbox("強化段数ゲージ Bloom を描画", &ui_.upgradeHudSegmentedBarBloomEnabled_);
    ImGui::Checkbox("強化リスト文字 Bloom を描画", &ui_.upgradeHudListTextBloomEnabled_);
#endif
    ImGui::Checkbox("下部EXP/Levelバーを描画", &ui_.upgradeHudDrawBottomBars_);
    ImGui::Checkbox("下部EXP/Level文字を描画", &ui_.upgradeHudDrawBottomText_);
    ImGui::Checkbox("背景/バーを矩形バッチで描画", &ui_.upgradeHudUseRectBatch_);
    if (ImGui::Checkbox("下部EXP/Levelバーを丸端にする", &ui_.upgradeHudRoundedProgressBars_)) {
        ApplyUpgradeHudProgressBarStyles();
    }
    ImGui::Checkbox("強化段数ゲージを描画", &ui_.upgradeHudUseSegmentedUpgradeBars_);
    ImGui::DragInt("1項目の最大強化段数", &player_.maxEnhancePoint, 1.0f, 1, 10);
    ImGui::Text("矩形Draw数: %d", ui_.upgradeHudProfile_.spriteDraws);
    ImGui::DragFloat("リスト表示アニメ速度", &ui_.upgradeHudListAnimSpeed_, 0.1f, 1.0f, 30.0f);
    ImGui::DragFloat("リストスライド距離", &ui_.upgradeHudListSlideDistance_, 1.0f, 0.0f, 500.0f);
    if (ImGui::Button("強化HUD設定を保存")) {
        ui_.upgradeHudConfigStatus_ = SaveUpgradeHudConfig() ? "強化HUD設定を保存しました。" : "強化HUD設定の保存に失敗しました。";
    }
    ImGui::SameLine();
    if (ImGui::Button("強化HUD設定を再読み込み")) {
        LoadUpgradeHudConfig();
    }
    if (!ui_.upgradeHudConfigStatus_.empty()) {
        ImGui::TextWrapped("%s", ui_.upgradeHudConfigStatus_.c_str());
    }
    ImGui::DragFloat2("左パネル位置", &ui_.upgradeHudPanelPos_.x, 1.0f);
    ImGui::DragFloat2("左パネルサイズ", &ui_.upgradeHudPanelSize_.x, 1.0f, 0.0f, 2000.0f);
    ImGui::DragFloat2("強化行 開始位置", &ui_.upgradeHudRowStart_.x, 1.0f);
    ImGui::DragFloat2("強化ボタンサイズ", &ui_.upgradeHudButtonSize_.x, 1.0f, 0.0f, 1000.0f);
    ImGui::DragFloat2("プラスボタンサイズ", &ui_.upgradeHudPlusSize_.x, 1.0f, 0.0f, 300.0f);
    ImGui::DragFloat2("段数ゲージ オフセット", &ui_.upgradeHudSegmentBarOffset_.x, 1.0f);
    ImGui::DragFloat2("段数ゲージ サイズ", &ui_.upgradeHudSegmentBarSize_.x, 1.0f, 0.0f, 1000.0f);
    ImGui::DragFloat("強化行 間隔", &ui_.upgradeHudRowGap_, 1.0f, 10.0f, 100.0f);
    ImGui::DragFloat("項目名X", &ui_.upgradeHudNameX_, 1.0f);
    ImGui::DragFloat("Lv表示X", &ui_.upgradeHudLevelX_, 1.0f);
    ImGui::DragFloat("マイナスボタンX", &ui_.upgradeHudMinusX_, 1.0f);
    ImGui::DragFloat("プラスボタンX", &ui_.upgradeHudPlusX_, 1.0f);
    ImGui::DragFloat("マイナス文字X", &ui_.upgradeHudMinusLabelX_, 1.0f);
    ImGui::DragFloat("プラス文字X", &ui_.upgradeHudPlusLabelX_, 1.0f);
    ImGui::DragFloat("項目名文字Yオフセット", &ui_.upgradeHudNameTextOffsetY_, 0.25f, -20.0f, 40.0f);
    ImGui::DragFloat("Lv文字Yオフセット", &ui_.upgradeHudLevelTextOffsetY_, 0.25f, -20.0f, 40.0f);
    ImGui::DragFloat("マイナス文字Yオフセット", &ui_.upgradeHudMinusTextOffsetY_, 0.25f, -20.0f, 40.0f);
    ImGui::DragFloat("プラス文字Yオフセット", &ui_.upgradeHudPlusTextOffsetY_, 0.25f, -20.0f, 40.0f);
    ImGui::DragFloat2("タイトル位置", &ui_.upgradeHudTitlePos_.x, 1.0f);
    ImGui::DragFloat2("ポイント表示位置", &ui_.upgradeHudPointPos_.x, 1.0f);
    ImGui::Separator();
    ImGui::DragFloat2("レベルバー位置", &ui_.upgradeHudLevelBarPos_.x, 1.0f);
    ImGui::DragFloat2("レベルバーサイズ", &ui_.upgradeHudLevelBarSize_.x, 1.0f, 0.0f, 2000.0f);
    ImGui::DragFloat2("レベル文字位置", &ui_.upgradeHudLevelTextPos_.x, 1.0f);
    ImGui::DragFloat2("経験値バー位置", &ui_.upgradeHudExpBarPos_.x, 1.0f);
    ImGui::DragFloat2("経験値バーサイズ", &ui_.upgradeHudExpBarSize_.x, 1.0f, 0.0f, 2000.0f);
    ImGui::DragFloat2("経験値文字位置", &ui_.upgradeHudExpTextPos_.x, 1.0f);
    ApplyUpgradeHudLayout();
#endif
}

void PlayerHud::DrawUpgradeHudAfterPostEffects()
{
    if (!ui_.arenaUiEnabled_)
        return;
    const bool drawBottomBars = ui_.upgradeHudDrawBottomBars_ && ui_.upgradeHudUseNeonProgressBars_;
    bool drawSegmentBars = ui_.upgradeHudUseSegmentedUpgradeBars_ && ui_.upgradeHudListVisibility_ > 0.01f;
#if defined(USE_IMGUI) && !defined(NDEBUG)
    drawSegmentBars = drawSegmentBars && ui_.upgradeHudSegmentedBarBloomEnabled_;
#endif
    if (!ui_.upgradeHudVisible_ || player_.isChangeMode || player_.isDead_ || !ui_.upgradeHudBarBloomEffect_ ||
        (!drawBottomBars && !drawSegmentBars)) {
        return;
    }

    ui_.upgradeHudBarBloomEffect_->BeginCapture();
    cg2::SpriteCommon::GetInstance()->PreDrawForScene(cg2::kNormal);
    if (drawBottomBars) {
        if (ui_.upgradeHudLevelProgressBar_) {
            ui_.upgradeHudLevelProgressBar_->DrawBloomSource();
        }
        if (ui_.upgradeHudExpProgressBar_) {
            ui_.upgradeHudExpProgressBar_->DrawBloomSource();
        }
    }
    if (drawSegmentBars) {
        for (int i = 0; i < 7; ++i) {
            if (ui_.upgradeHudSegmentBars_[i]) {
                ui_.upgradeHudSegmentBars_[i]->DrawBloomSource();
            }
        }
    }
    ui_.upgradeHudBarBloomEffect_->EndCaptureBloomOnlyToBackBuffer();
}

/// @brief 強化HUDの処理時間と描画件数への読み取り専用参照を返す。
const Player::UiProfileStats& PlayerHud::GetUpgradeHudProfileStats() const
{
    return ui_.upgradeHudProfile_;
}

#if defined(USE_IMGUI) && !defined(NDEBUG)
/// @brief 強化HUD開発表示状態の写しを返す。
Player::UpgradeHudDebugSnapshot PlayerHud::GetUpgradeHudDebugSnapshot() const
{
    return {player_.level_,
            player_.exp_,
            player_.skillPoints_,
            ui_.upgradeHudVisible_,
            ui_.upgradeHudListVisibility_,
            ui_.upgradeHudHideListWithoutPoints_,
            ui_.upgradeHudDrawListPanels_,
            ui_.upgradeHudDrawListText_,
            ui_.upgradeHudDrawBottomBars_,
            ui_.upgradeHudDrawBottomText_,
            ui_.upgradeHudUseRectBatch_,
            ui_.upgradeHudUseNeonProgressBars_,
            ui_.upgradeHudUseSegmentedUpgradeBars_,
            ui_.upgradeHudSegmentedBarBloomEnabled_,
            ui_.upgradeHudListTextBloomEnabled_,
            ui_.upgradeHudVisible_ && !player_.isChangeMode && !player_.isDead_ && ui_.upgradeHudListVisibility_ > 0.01f,
            player_.isChangeMode,
            player_.isDead_,
            player_.maxEnhancePoint};
}
#endif
