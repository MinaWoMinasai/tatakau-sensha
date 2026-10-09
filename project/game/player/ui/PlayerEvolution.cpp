#include "game/player/ui/PlayerEvolution.h"
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

#include "game/weapon/CombatTypes.h"
#include "Player.h"
#include "PlayerUiHelpers.h"
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

// 進化画面専用のJSON読取を、この翻訳単位に閉じ込める。
namespace {
using playerui::ReadVector2Object;
using playerui::SetLabel;
using playerui::Vector4ToJson;
using playerui::WriteVector2Object;
/// @brief JSON配列から4成分を読む。要素数が不足する場合はfallbackを返す。
cg2::Vector4 ReadVector4(const nlohmann::json& json, const cg2::Vector4& fallback)
{
    if (!json.is_array() || json.size() < 4) {
        return fallback;
    }
    return {json[0].get<float>(), json[1].get<float>(), json[2].get<float>(), json[3].get<float>()};
}
} // namespace

void PlayerEvolution::InitializeEncyclopedia()
{
    cg2::StartupTrace::Scope scope("Player.EncyclopediaUi");
    cg2::SpriteCommon* spriteCommon = cg2::SpriteCommon::GetInstance();
    auto makePanel = [spriteCommon](const cg2::Vector2& pos, const cg2::Vector2& size, const cg2::Vector4& color) {
        auto panel = std::make_unique<cg2::Sprite>();
        panel->Initialize(spriteCommon, "resources/white512x512.png");
        panel->SetPosition(pos);
        panel->SetSize(size);
        panel->SetColor(color);
        return panel;
    };

    ui_.evolutionBackdropSprite_ = makePanel({0.0f, 0.0f}, {1280.0f, 720.0f}, {0.02f, 0.03f, 0.07f, 0.78f});
    ui_.evolutionPreviewPanelSprite_ = makePanel({28.0f, 84.0f}, {360.0f, 560.0f}, {0.05f, 0.12f, 0.17f, 0.86f});
    ui_.evolutionStatsPanelSprite_ = makePanel({910.0f, 84.0f}, {342.0f, 560.0f}, {0.07f, 0.08f, 0.12f, 0.88f});
    ui_.evolutionPreviewTankSprite_ = std::make_unique<cg2::Sprite>();
    ui_.evolutionPreviewTankSprite_->Initialize(spriteCommon, "resources/normalTank.png");
    ui_.evolutionPreviewTankSprite_->SetAnchorPoint({0.5f, 0.5f});
    ui_.evolutionPreviewTankSprite_->SetSize({250.0f, 150.0f});
    ui_.evolutionPreviewTankSprite_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
    ui_.evolutionShotSprite_ = makePanel({0.0f, 0.0f}, {170.0f, 9.0f}, {1.0f, 0.88f, 0.28f, 0.0f});
    ui_.evolutionShotSprite_->SetAnchorPoint({0.0f, 0.5f});
    ui_.evolutionChangeButtonSprite_ = makePanel({940.0f, 650.0f}, {290.0f, 48.0f}, {0.24f, 0.86f, 0.44f, 0.92f});

    cg2::TextStyle titleStyle{};
    titleStyle.fontFamily = "Meiryo";
    titleStyle.fontSize = 34.0f;
    titleStyle.color = {0.75f, 1.0f, 0.92f, 1.0f};
    titleStyle.outlineColor = {0.0f, 0.08f, 0.10f, 0.95f};
    titleStyle.outlineThickness = 3.0f;
    titleStyle.padding = 8.0f;
    SetLabel(ui_.evolutionTitleLabel_, spriteCommon, "戦車図鑑 / 進化ツリー", {40.0f, 28.0f}, titleStyle);

    cg2::TextStyle smallStyle = titleStyle;
    smallStyle.fontSize = 20.0f;
    smallStyle.color = {0.86f, 0.92f, 1.0f, 1.0f};
    smallStyle.outlineThickness = 2.0f;
    SetLabel(ui_.evolutionHintLabel_, spriteCommon, "C:閉じる / カード選択:詳細 / ボタン:機体変更", {520.0f, 42.0f}, smallStyle);

    ui_.encyclopedia_.clear();
    const float cardWidth = 154.0f;
    const float cardHeight = 80.0f;
    const float cardGapX = 12.0f;
    const float cardGapY = 12.0f;
    const cg2::Vector2 cardBase = {416.0f, 112.0f};

    for (int i = 0; i < static_cast<int>(player_.classCatalog_.OrderedIds().size()); ++i) {
        const PlayerClassConfig* config = player_.GetClassConfig(player_.classCatalog_.OrderedIds()[i]);
        if (!config) {
            continue;
        }
        TankData data;
        data.type = config->type;
        data.classId = config->id;
        data.name = config->displayName;
        data.requiredRank = config->requiredRank;
        data.texturePath = ClassTexturePath(config->type);

        const int col = i % 3;
        const int row = i / 3;
        const cg2::Vector2 cardPos = {cardBase.x + static_cast<float>(col) * (cardWidth + cardGapX),
                                      cardBase.y + static_cast<float>(row) * (cardHeight + cardGapY)};

        data.cardSprite = makePanel(cardPos, {cardWidth, cardHeight}, {0.10f, 0.15f, 0.22f, 0.82f});
        data.sprite = std::make_unique<cg2::Sprite>();
        data.sprite->Initialize(cg2::SpriteCommon::GetInstance(), data.texturePath);
        data.sprite->SetPosition({cardPos.x + 14.0f, cardPos.y + 8.0f});
        data.sprite->SetSize({126.0f, 38.0f});

        cg2::TextStyle cardNameStyle = smallStyle;
        cardNameStyle.fontSize = 14.0f;
        cardNameStyle.color = {0.92f, 1.0f, 0.95f, 1.0f};
        SetLabel(data.nameLabel, spriteCommon, data.name, {cardPos.x + 10.0f, cardPos.y + 50.0f}, cardNameStyle);

        cg2::TextStyle rankStyle = cardNameStyle;
        rankStyle.fontSize = 12.0f;
        rankStyle.color = {0.72f, 0.86f, 1.0f, 1.0f};
        SetLabel(data.rankLabel, spriteCommon, "R" + std::to_string(data.requiredRank), {cardPos.x + 112.0f, cardPos.y + 54.0f}, rankStyle);

        ui_.encyclopedia_.push_back(std::move(data));
    }
}

void PlayerEvolution::UpdateEncyclopedia(float uiDeltaTime)
{
    if (!ui_.arenaUiEnabled_ || !player_.isChangeMode) {
        return;
    }
    if (ui_.encyclopedia_.size() != player_.classCatalog_.OrderedIds().size()) {
        InitializeEncyclopedia();
    }
    if (ui_.encyclopedia_.empty()) {
        return;
    }
    player_.evolutionUiTimer_ += (std::max)(0.0f, uiDeltaTime);
    if (ShouldUseEvolutionCircuitPrototype()) {
        UpdateEvolutionCircuitPrototype();
        return;
    }
    if (ShouldUseStaticEvolutionPrototype()) {
        UpdateStaticEvolutionPrototype();
        return;
    }
    int currentRank = player_.GetRankFromLevel(player_.level_);

    for (int i = 0; i < static_cast<int>(ui_.encyclopedia_.size()); ++i) {
        auto& tank = ui_.encyclopedia_[i];
        if (!player_.IsEvolutionClassVisible(tank.classId))
            continue;
        bool isAvailable = (currentRank >= tank.requiredRank);
        const bool selected = (i == player_.evolutionSelectedIndex_);
        const bool hovered = tank.cardSprite && tank.cardSprite->IsHovered(player_.mousePosition_);

        if (hovered &&
            player_.input_->IsTrigger(player_.input_->GetMouseState().rgbButtons[0], player_.input_->GetPreMouseState().rgbButtons[0])) {
            player_.evolutionSelectedIndex_ = i;
            ui_.codexSelectedClassIndex_ = i;
        }

        if (tank.cardSprite) {
            if (selected) {
                tank.cardSprite->SetColor({0.18f, 0.48f, 0.42f, 0.94f});
            } else if (hovered) {
                tank.cardSprite->SetColor({0.16f, 0.30f, 0.38f, 0.92f});
            } else if (isAvailable) {
                tank.cardSprite->SetColor({0.10f, 0.15f, 0.22f, 0.82f});
            } else {
                tank.cardSprite->SetColor({0.05f, 0.05f, 0.07f, 0.72f});
            }
            tank.cardSprite->Update();
        }

        if (isAvailable) {
            tank.sprite->SetColor({1.0f, 1.0f, 1.0f, selected ? 1.0f : 0.86f});
        } else {
            tank.sprite->SetColor({0.20f, 0.24f, 0.28f, 0.45f});
        }

        tank.sprite->Update();
    }

    player_.evolutionSelectedIndex_ = (std::clamp)(player_.evolutionSelectedIndex_, 0, static_cast<int>(ui_.encyclopedia_.size()) - 1);
    if (!player_.IsEvolutionClassVisible(ui_.encyclopedia_[player_.evolutionSelectedIndex_].classId)) {
        for (size_t i = 0; i < ui_.encyclopedia_.size(); ++i) {
            if (ui_.encyclopedia_[i].classId == player_.currentClassId_) {
                player_.evolutionSelectedIndex_ = static_cast<int>(i);
                break;
            }
        }
    }
    const TankData& selectedTank = ui_.encyclopedia_[player_.evolutionSelectedIndex_];
    const PlayerClassConfig* selectedConfig = player_.GetClassConfig(selectedTank.classId);
    if (!selectedConfig) {
        return;
    }

    const bool locked = currentRank < selectedTank.requiredRank;
    if (ui_.evolutionPreviewTankSprite_) {
        ui_.evolutionPreviewTankSprite_->SetTexture(selectedTank.texturePath);
        const float bob = std::sin(player_.evolutionUiTimer_ * 2.0f) * 12.0f;
        ui_.evolutionPreviewTankSprite_->SetPosition({210.0f + bob, 300.0f});
        ui_.evolutionPreviewTankSprite_->SetRotation(std::sin(player_.evolutionUiTimer_ * 1.35f) * 0.08f);
        ui_.evolutionPreviewTankSprite_->SetColor(locked ? cg2::Vector4{0.35f, 0.40f, 0.45f, 0.65f} : cg2::Vector4{1.0f, 1.0f, 1.0f, 1.0f});
        ui_.evolutionPreviewTankSprite_->Update();
    }

    if (ui_.evolutionShotSprite_) {
        const float shotPhase = std::fmod(player_.evolutionUiTimer_ * 1.8f, 1.0f);
        ui_.evolutionShotSprite_->SetPosition({300.0f + shotPhase * 52.0f, 296.0f});
        ui_.evolutionShotSprite_->SetRotation(std::sin(player_.evolutionUiTimer_ * 1.2f) * 0.12f);
        ui_.evolutionShotSprite_->SetSize({85.0f + shotPhase * 95.0f, 7.0f});
        ui_.evolutionShotSprite_->SetColor(locked ? cg2::Vector4{0.55f, 0.55f, 0.60f, 0.18f}
                                                  : cg2::Vector4{1.0f, 0.90f, 0.32f, 0.82f * (1.0f - shotPhase * 0.55f)});
        ui_.evolutionShotSprite_->Update();
    }

    if (ui_.evolutionChangeButtonSprite_) {
        const bool buttonHovered = ui_.evolutionChangeButtonSprite_->IsHovered(player_.mousePosition_);
        if (locked) {
            ui_.evolutionChangeButtonSprite_->SetColor({0.16f, 0.16f, 0.18f, 0.82f});
        } else if (buttonHovered) {
            ui_.evolutionChangeButtonSprite_->SetColor({0.36f, 1.0f, 0.58f, 0.96f});
            if (player_.input_->IsTrigger(player_.input_->GetMouseState().rgbButtons[0],
                                          player_.input_->GetPreMouseState().rgbButtons[0])) {
                player_.TryConfirmEvolutionById(selectedTank.classId);
            }
        } else {
            ui_.evolutionChangeButtonSprite_->SetColor({0.24f, 0.86f, 0.44f, 0.92f});
        }
        ui_.evolutionChangeButtonSprite_->Update();
    }
}

void PlayerEvolution::DrawEncyclopedia()
{
    if (!ui_.arenaUiEnabled_)
        return;

    ui_.evolutionUiProfile_ = {};
    if (player_.isChangeMode && ShouldUseEvolutionCircuitPrototype()) {
        DrawEvolutionCircuitPrototype();
        return;
    }
    if (player_.isChangeMode && ShouldUseStaticEvolutionPrototype()) {
        DrawStaticEvolutionPrototype();
        return;
    }
    if (player_.isChangeMode) {
        ui_.evolutionUiProfile_.visible = true;
        const auto totalStart = std::chrono::steady_clock::now();
        if (ui_.encyclopedia_.empty()) {
            return;
        }
        player_.evolutionSelectedIndex_ = (std::clamp)(player_.evolutionSelectedIndex_, 0, static_cast<int>(ui_.encyclopedia_.size()) - 1);
        const TankData& selectedTank = ui_.encyclopedia_[player_.evolutionSelectedIndex_];
        const PlayerClassConfig* selectedConfig = player_.GetClassConfig(selectedTank.classId);
        if (!selectedConfig) {
            return;
        }

        auto roleText = [](const PlayerClassConfig& config) {
            if (config.usesDrone) {
                return std::string("ドローンで周囲を制圧する支援型タンク。");
            }
            if (config.reflect) {
                return std::string("反射弾で壁越しにも圧をかける技巧型タンク。");
            }
            if (config.bulletSpeedScale > 1.2f) {
                return std::string("高速弾で遠距離から狙う狙撃型タンク。");
            }
            if (config.reloadScale < 0.75f) {
                return std::string("連射力で押し切る近中距離向けタンク。");
            }
            if (config.bulletCount > 1 || config.barrels.size() >= 3) {
                return std::string("複数の砲身で広い範囲を抑える制圧型タンク。");
            }
            return std::string("扱いやすい基本性能を持つバランス型タンク。");
        };

        cg2::TextStyle headingStyle{};
        headingStyle.fontFamily = "Meiryo";
        headingStyle.fontSize = 28.0f;
        headingStyle.color = {0.82f, 1.0f, 0.92f, 1.0f};
        headingStyle.outlineColor = {0.0f, 0.05f, 0.08f, 0.95f};
        headingStyle.outlineThickness = 3.0f;
        headingStyle.padding = 8.0f;

        cg2::TextStyle bodyStyle = headingStyle;
        bodyStyle.fontSize = 21.0f;
        bodyStyle.color = {0.92f, 0.96f, 1.0f, 1.0f};
        bodyStyle.outlineThickness = 2.0f;

        cg2::TextStyle smallStyle = bodyStyle;
        smallStyle.fontSize = 18.0f;

        cg2::SpriteCommon* spriteCommon = cg2::SpriteCommon::GetInstance();
        SetLabel(ui_.evolutionPreviewNameLabel_, spriteCommon, selectedConfig->displayName, {58.0f, 112.0f}, headingStyle);
        SetLabel(ui_.evolutionRoleLabel_, spriteCommon, roleText(*selectedConfig), {58.0f, 560.0f}, bodyStyle);

        const int currentRank = player_.GetRankFromLevel(player_.level_);
        const bool locked = currentRank < selectedConfig->requiredRank;
        std::array<std::string, 9> statLines = {
            "必要ランク: " + std::to_string(selectedConfig->requiredRank) + (locked ? "  (未解放)" : "  (使用可能)"),
            "砲塔数: " + std::to_string(selectedConfig->barrels.size()),
            "発射方式: " +
                std::string(selectedConfig->fireAllBarrels ? "全砲門" : (selectedConfig->alternateBarrels ? "交互発射" : "単発")),
            "弾数: " + std::to_string(selectedConfig->bulletCount),
            "リロード倍率: " + std::to_string(selectedConfig->reloadScale).substr(0, 4),
            "弾速倍率: " + std::to_string(selectedConfig->bulletSpeedScale).substr(0, 4),
            "ダメージ倍率: " + std::to_string(selectedConfig->bulletDamageScale).substr(0, 4),
            "拡散角: " + std::to_string(selectedConfig->spreadAngleDeg).substr(0, 4),
            std::string("特殊: ") + (selectedConfig->usesDrone   ? "ドローン"
                                     : selectedConfig->reflect   ? "反射"
                                     : selectedConfig->penetrate ? "貫通"
                                                                 : "なし")};
        for (int i = 0; i < static_cast<int>(statLines.size()); ++i) {
            SetLabel(ui_.evolutionStatLabels_[i], spriteCommon, statLines[i], {932.0f, 136.0f + static_cast<float>(i) * 40.0f}, smallStyle);
        }

        cg2::TextStyle buttonStyle = headingStyle;
        buttonStyle.fontSize = 24.0f;
        buttonStyle.color = locked ? cg2::Vector4{0.68f, 0.68f, 0.72f, 1.0f} : cg2::Vector4{0.02f, 0.09f, 0.04f, 1.0f};
        buttonStyle.outlineColor = locked ? cg2::Vector4{0.0f, 0.0f, 0.0f, 0.70f} : cg2::Vector4{0.78f, 1.0f, 0.82f, 0.65f};
        SetLabel(ui_.evolutionChangeButtonLabel_, spriteCommon, locked ? "ランク不足" : "この戦車に変更", {990.0f, 660.0f}, buttonStyle);

        const auto spriteStart = std::chrono::steady_clock::now();
        cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
        if (ui_.evolutionBackdropSprite_) {
            ui_.evolutionBackdropSprite_->Draw();
            ++ui_.evolutionUiProfile_.spriteDraws;
        }
        if (ui_.evolutionPreviewPanelSprite_) {
            ui_.evolutionPreviewPanelSprite_->Draw();
            ++ui_.evolutionUiProfile_.spriteDraws;
        }
        if (ui_.evolutionStatsPanelSprite_) {
            ui_.evolutionStatsPanelSprite_->Draw();
            ++ui_.evolutionUiProfile_.spriteDraws;
        }
        if (ui_.evolutionShotSprite_) {
            ui_.evolutionShotSprite_->Draw();
            ++ui_.evolutionUiProfile_.spriteDraws;
        }
        if (ui_.evolutionPreviewTankSprite_) {
            ui_.evolutionPreviewTankSprite_->Draw();
            ++ui_.evolutionUiProfile_.spriteDraws;
        }

        for (auto& tank : ui_.encyclopedia_) {
            if (!player_.IsEvolutionClassVisible(tank.classId))
                continue;
            if (tank.cardSprite) {
                tank.cardSprite->Draw();
                ++ui_.evolutionUiProfile_.spriteDraws;
            }
            tank.sprite->Draw(); // 各スプライトが持つ位置で描画
            ++ui_.evolutionUiProfile_.spriteDraws;
        }
        if (ui_.evolutionChangeButtonSprite_) {
            ui_.evolutionChangeButtonSprite_->Draw();
            ++ui_.evolutionUiProfile_.spriteDraws;
        }
        const auto spriteEnd = std::chrono::steady_clock::now();

        const auto textStart = std::chrono::steady_clock::now();
        if (ui_.evolutionTitleLabel_) {
            ui_.evolutionTitleLabel_->Draw();
            ++ui_.evolutionUiProfile_.textDraws;
        }
        if (ui_.evolutionHintLabel_) {
            ui_.evolutionHintLabel_->Draw();
            ++ui_.evolutionUiProfile_.textDraws;
        }
        if (ui_.evolutionPreviewNameLabel_) {
            ui_.evolutionPreviewNameLabel_->Draw();
            ++ui_.evolutionUiProfile_.textDraws;
        }
        if (ui_.evolutionRoleLabel_) {
            ui_.evolutionRoleLabel_->Draw();
            ++ui_.evolutionUiProfile_.textDraws;
        }

        for (auto& tank : ui_.encyclopedia_) {
            if (!player_.IsEvolutionClassVisible(tank.classId))
                continue;
            if (tank.nameLabel) {
                tank.nameLabel->Draw();
                ++ui_.evolutionUiProfile_.textDraws;
            }
            if (tank.rankLabel) {
                tank.rankLabel->Draw();
                ++ui_.evolutionUiProfile_.textDraws;
            }
        }

        for (auto& label : ui_.evolutionStatLabels_) {
            if (label) {
                label->Draw();
                ++ui_.evolutionUiProfile_.textDraws;
            }
        }
        if (ui_.evolutionChangeButtonLabel_) {
            ui_.evolutionChangeButtonLabel_->Draw();
            ++ui_.evolutionUiProfile_.textDraws;
        }
        const auto textEnd = std::chrono::steady_clock::now();
        const auto totalEnd = std::chrono::steady_clock::now();

        ui_.evolutionUiProfile_.spriteMs = std::chrono::duration<float, std::milli>(spriteEnd - spriteStart).count();
        ui_.evolutionUiProfile_.textMs = std::chrono::duration<float, std::milli>(textEnd - textStart).count();
        ui_.evolutionUiProfile_.updateMs = std::chrono::duration<float, std::milli>(spriteStart - totalStart).count();
        ui_.evolutionUiProfile_.totalMs = std::chrono::duration<float, std::milli>(totalEnd - totalStart).count();
    }
}

void PlayerEvolution::DrawTankCodex()
{
#ifdef USE_IMGUI
    if (player_.classCatalog_.OrderedIds().empty()) {
        player_.LoadPlayerClassConfigs();
    }
    if (player_.classCatalog_.OrderedIds().empty()) {
        return;
    }

    ui_.codexSelectedClassIndex_ =
        (std::clamp)(ui_.codexSelectedClassIndex_, 0, static_cast<int>(player_.classCatalog_.OrderedIds().size()) - 1);
    const std::string selectedId = player_.classCatalog_.OrderedIds()[ui_.codexSelectedClassIndex_];
    const PlayerClassConfig* selectedConfig = player_.GetClassConfig(selectedId);
    if (!selectedConfig) {
        return;
    }

    if (!ImGui::Begin("Tank Codex / Evolution Tree")) {
        ImGui::End();
        return;
    }

    const int currentRank = player_.GetRankFromLevel(player_.level_);
    ui_.codexPreviewTimer_ += player_.dt_;
    ImGui::Text("Level %d  Rank %d  Current: %s", player_.level_, currentRank, player_.GetCurrentClassName());
    ImGui::Text("Click a tank to inspect it. Locked tanks can be previewed, but not equipped.");
    ImGui::Separator();

    ImGui::Columns(3, "TankCodexColumns", true);
    ImGui::SetColumnWidth(0, 230.0f);
    ImGui::SetColumnWidth(1, 420.0f);

    ImGui::Text("Evolution Tree");
    ImGui::BeginChild("TankCodexTree", ImVec2(0.0f, 430.0f), true);
    for (int rank = 1; rank <= 4; ++rank) {
        ImGui::Text("Rank %d", rank);
        ImGui::Indent(14.0f);
        for (int i = 0; i < static_cast<int>(player_.classCatalog_.OrderedIds().size()); ++i) {
            const PlayerClassConfig* config = player_.GetClassConfig(player_.classCatalog_.OrderedIds()[i]);
            if (!config || config->requiredRank != rank) {
                continue;
            }
            const bool selected = i == ui_.codexSelectedClassIndex_;
            const bool locked = currentRank < config->requiredRank;
            ImGui::PushID(i);
            if (locked) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.62f, 1.0f));
            }
            std::string label = config->displayName;
            if (config->id == player_.currentClassId_) {
                label += "  [Current]";
            } else if (locked) {
                label += "  [Locked]";
            }
            if (ImGui::Selectable(label.c_str(), selected)) {
                ui_.codexSelectedClassIndex_ = i;
            }
            if (locked) {
                ImGui::PopStyleColor();
            }
            ImGui::PopID();
        }
        ImGui::Unindent(14.0f);
        ImGui::Spacing();
        if (rank < 4) {
            ImGui::Text("  v");
        }
    }
    ImGui::EndChild();

    ImGui::NextColumn();

    ImGui::Text("Preview");
    ImGui::BeginChild("TankCodexPreview", ImVec2(0.0f, 430.0f), true);
    ImGui::Checkbox("Auto Move", &ui_.codexPreviewAutoMove_);
    ImGui::SameLine();
    ImGui::Checkbox("Auto Fire", &ui_.codexPreviewAutoFire_);
    ImGui::SliderFloat("Aim Deg", &ui_.codexPreviewAimDeg_, -180.0f, 180.0f);
    if (ImGui::Button("Test Shot")) {
        ui_.codexPreviewTimer_ = 0.0f;
    }

    const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize = ImVec2((std::max)(360.0f, ImGui::GetContentRegionAvail().x), 300.0f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(8, 12, 20, 255));
    drawList->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(90, 160, 190, 150));

    const float grid = 28.0f;
    for (float x = std::fmod(ui_.codexPreviewTimer_ * 18.0f, grid); x < canvasSize.x; x += grid) {
        drawList->AddLine(ImVec2(canvasPos.x + x, canvasPos.y), ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y),
                          IM_COL32(45, 105, 155, 80), 1.0f);
    }
    for (float y = std::fmod(ui_.codexPreviewTimer_ * 10.0f, grid); y < canvasSize.y; y += grid) {
        drawList->AddLine(ImVec2(canvasPos.x, canvasPos.y + y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y),
                          IM_COL32(45, 105, 155, 80), 1.0f);
    }

    const float moveWave = ui_.codexPreviewAutoMove_ ? std::sin(ui_.codexPreviewTimer_ * 1.7f) : 0.0f;
    const ImVec2 center = ImVec2(canvasPos.x + canvasSize.x * 0.48f + moveWave * 42.0f, canvasPos.y + canvasSize.y * 0.55f);
    const float aimRad = ui_.codexPreviewAimDeg_ * 3.1415926535f / 180.0f;
    const ImVec2 forward = ImVec2(std::cos(aimRad), std::sin(aimRad));
    const ImVec2 right = ImVec2(-forward.y, forward.x);
    const float bodyRadius = 38.0f;
    auto toImColor = [](const cg2::Vector4& color) {
        return IM_COL32(
            static_cast<int>(std::clamp(color.x, 0.0f, 1.0f) * 255.0f), static_cast<int>(std::clamp(color.y, 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(color.z, 0.0f, 1.0f) * 255.0f), static_cast<int>(std::clamp(color.w, 0.0f, 1.0f) * 255.0f));
    };
    const ImU32 bodyFill = toImColor(selectedConfig->bodyFillColor);
    const ImU32 bodyOutline = toImColor(selectedConfig->bodyOutlineColor);

    auto addRotatedRect = [&](ImVec2 origin, ImVec2 axisX, ImVec2 axisY, float halfX, float halfY, ImU32 fill, ImU32 outline) {
        const ImVec2 p0 = ImVec2(origin.x - axisX.x * halfX - axisY.x * halfY, origin.y - axisX.y * halfX - axisY.y * halfY);
        const ImVec2 p1 = ImVec2(origin.x + axisX.x * halfX - axisY.x * halfY, origin.y + axisX.y * halfX - axisY.y * halfY);
        const ImVec2 p2 = ImVec2(origin.x + axisX.x * halfX + axisY.x * halfY, origin.y + axisX.y * halfX + axisY.y * halfY);
        const ImVec2 p3 = ImVec2(origin.x - axisX.x * halfX + axisY.x * halfY, origin.y - axisX.y * halfX + axisY.y * halfY);
        drawList->AddQuadFilled(p0, p1, p2, p3, fill);
        drawList->AddQuad(p0, p1, p2, p3, outline, 2.0f);
    };
    auto addPolygonBody = [&](int segments, float rotationRad, ImU32 fill, ImU32 outline) {
        segments = (std::clamp)(segments, 3, 48);
        std::vector<ImVec2> points;
        points.reserve(static_cast<size_t>(segments));
        for (int i = 0; i < segments; ++i) {
            const float angle = rotationRad + static_cast<float>(i) * 6.283185307f / static_cast<float>(segments);
            points.push_back(ImVec2(center.x + std::cos(angle) * bodyRadius * selectedConfig->bodyScale.x,
                                    center.y + std::sin(angle) * bodyRadius * selectedConfig->bodyScale.y));
        }
        drawList->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), fill);
        drawList->AddPolyline(points.data(), static_cast<int>(points.size()), outline, ImDrawFlags_Closed, 3.0f);
    };
    auto groupColor = [](int group) {
        static const ImU32 colors[] = {IM_COL32(160, 255, 120, 230), IM_COL32(255, 220, 80, 230),  IM_COL32(90, 230, 255, 230),
                                       IM_COL32(255, 110, 190, 230), IM_COL32(190, 140, 255, 230), IM_COL32(255, 150, 95, 230)};
        return colors[static_cast<size_t>((std::max)(0, group)) % (sizeof(colors) / sizeof(colors[0]))];
    };

    std::vector<ImVec2> muzzlePoints;
    for (const WeaponMountConfig& barrel : selectedConfig->barrels) {
        const float localAngleRad = (ui_.codexPreviewAimDeg_ + barrel.angleDeg) * 3.1415926535f / 180.0f;
        const ImVec2 barrelForward = ImVec2(std::cos(localAngleRad), std::sin(localAngleRad));
        const ImVec2 barrelRight = ImVec2(-barrelForward.y, barrelForward.x);
        const ImVec2 offset = ImVec2(forward.x * barrel.offset.x * 42.0f + right.x * barrel.offset.y * 42.0f,
                                     forward.y * barrel.offset.x * 42.0f + right.y * barrel.offset.y * 42.0f);
        float length = (std::max)(26.0f, barrel.scale.x * 34.0f);
        float width = (std::max)(8.0f, barrel.scale.y * 38.0f);
        if (barrel.barrelShape == BarrelShape::Heavy) {
            length *= 1.12f;
            width *= 1.45f;
        } else if (barrel.barrelShape == BarrelShape::Short) {
            length *= 0.58f;
            width *= 1.08f;
        } else if (barrel.barrelShape == BarrelShape::Wide) {
            length *= 0.86f;
            width *= 1.80f;
        }
        const ImVec2 base =
            ImVec2(center.x + offset.x + barrelForward.x * length * 0.32f, center.y + offset.y + barrelForward.y * length * 0.32f);
        const ImU32 barrelFill = toImColor(barrel.barrelColor);
        const ImU32 outline = groupColor(barrel.fireGroup);
        if (barrel.barrelShape == BarrelShape::Trapezoid) {
            const float halfBase = width * 0.64f;
            const float halfTip = width * 0.36f;
            const float halfLength = length * 0.5f;
            const ImVec2 p0 = ImVec2(base.x - barrelForward.x * halfLength - barrelRight.x * halfBase,
                                     base.y - barrelForward.y * halfLength - barrelRight.y * halfBase);
            const ImVec2 p1 = ImVec2(base.x + barrelForward.x * halfLength - barrelRight.x * halfTip,
                                     base.y + barrelForward.y * halfLength - barrelRight.y * halfTip);
            const ImVec2 p2 = ImVec2(base.x + barrelForward.x * halfLength + barrelRight.x * halfTip,
                                     base.y + barrelForward.y * halfLength + barrelRight.y * halfTip);
            const ImVec2 p3 = ImVec2(base.x - barrelForward.x * halfLength + barrelRight.x * halfBase,
                                     base.y - barrelForward.y * halfLength + barrelRight.y * halfBase);
            drawList->AddQuadFilled(p0, p1, p2, p3, barrelFill);
            drawList->AddQuad(p0, p1, p2, p3, outline, 2.0f);
        } else {
            addRotatedRect(base, barrelForward, barrelRight, length * 0.5f, width * 0.5f, barrelFill, outline);
        }
        muzzlePoints.push_back(ImVec2(base.x + barrelForward.x * length * 0.56f, base.y + barrelForward.y * length * 0.56f));
    }

    switch (selectedConfig->bodyShape) {
    case BodyShape::Box:
        addPolygonBody(4, aimRad + 6.283185307f * 0.125f, bodyFill, bodyOutline);
        break;
    case BodyShape::Triangle:
        addPolygonBody(3, aimRad - 6.283185307f * 0.25f, bodyFill, bodyOutline);
        break;
    case BodyShape::Pentagon:
        addPolygonBody(5, aimRad - 6.283185307f * 0.25f, bodyFill, bodyOutline);
        break;
    case BodyShape::Circle:
    default:
        drawList->AddCircleFilled(center, bodyRadius + 7.0f, IM_COL32(95, 255, 135, 45), 48);
        drawList->AddCircleFilled(center, bodyRadius, bodyFill, 48);
        drawList->AddCircle(center, bodyRadius, bodyOutline, 48, 3.0f);
        break;
    }

    const bool showShot = ui_.codexPreviewAutoFire_ || std::fmod(ui_.codexPreviewTimer_, 1.0f) < 0.18f;
    if (showShot) {
        const float shotPhase = std::fmod(ui_.codexPreviewTimer_ * 1.8f, 1.0f);
        for (const ImVec2& muzzle : muzzlePoints) {
            const ImVec2 head =
                ImVec2(muzzle.x + forward.x * (50.0f + shotPhase * 130.0f), muzzle.y + forward.y * (50.0f + shotPhase * 130.0f));
            drawList->AddLine(muzzle, head, IM_COL32(255, 245, 175, 190), 8.0f);
            drawList->AddLine(muzzle, head, IM_COL32(255, 105, 130, 210), 3.0f);
            drawList->AddCircleFilled(head, 8.0f, IM_COL32(255, 245, 185, 220), 20);
        }
    }

    if (selectedConfig->usesDrone) {
        for (int i = 0; i < (std::min)(selectedConfig->maxDrones, 7); ++i) {
            const float a = ui_.codexPreviewTimer_ * 1.8f +
                            static_cast<float>(i) * 6.283185307f / (std::max)(1, (std::min)(selectedConfig->maxDrones, 7));
            const ImVec2 drone = ImVec2(center.x + std::cos(a) * 78.0f, center.y + std::sin(a) * 78.0f);
            drawList->AddCircleFilled(drone, 8.0f, IM_COL32(120, 230, 255, 210), 20);
            drawList->AddCircle(drone, 8.0f, IM_COL32(215, 250, 255, 230), 20, 2.0f);
        }
    }

    ImGui::Dummy(canvasSize);
    ImGui::EndChild();

    ImGui::NextColumn();

    ImGui::Text("Tank Data");
    ImGui::BeginChild("TankCodexStats", ImVec2(0.0f, 430.0f), true);
    const bool locked = currentRank < selectedConfig->requiredRank;
    ImGui::Text("Name: %s", selectedConfig->displayName.c_str());
    ImGui::Text("ID: %s", selectedConfig->id.c_str());
    ImGui::Text("Required Rank: %d  %s", selectedConfig->requiredRank, locked ? "(locked)" : "(available)");
    ImGui::Separator();
    ImGui::Text("Barrels: %zu", selectedConfig->barrels.size());
    ImGui::Text("Fire Mode: %s%s",
                selectedConfig->fireAllBarrels ? "All Barrels" : (selectedConfig->alternateBarrels ? "Alternate" : "Single"),
                selectedConfig->usesDrone ? " + Drone" : "");
    ImGui::Text("Bullet Count: %d", selectedConfig->bulletCount);
    ImGui::Text("Reload Scale: %.2f", selectedConfig->reloadScale);
    ImGui::Text("Bullet Speed Scale: %.2f", selectedConfig->bulletSpeedScale);
    ImGui::Text("Bullet Damage Scale: %.2f", selectedConfig->bulletDamageScale);
    ImGui::Text("Spread: %.1f deg  %s", selectedConfig->spreadAngleDeg, selectedConfig->randomSpread ? "random" : "fixed");
    ImGui::Text("Reflect: %s", selectedConfig->reflect ? "yes" : "no");
    ImGui::Text("Penetrate: %s", selectedConfig->penetrate ? "yes" : "no");
    ImGui::Text("Drones: %s  max %d", selectedConfig->usesDrone ? "yes" : "no", selectedConfig->maxDrones);
    ImGui::Separator();
    ImGui::TextWrapped("Role: %s", selectedConfig->usesDrone                 ? "Controls drones and keeps pressure while repositioning."
                                   : selectedConfig->reflect                 ? "Uses bounce shots to fight around cover."
                                   : selectedConfig->bulletSpeedScale > 1.2f ? "Long range, fast projectile style."
                                   : selectedConfig->reloadScale < 0.75f     ? "Rapid fire tank for close and mid range pressure."
                                   : selectedConfig->bulletCount > 1 || selectedConfig->barrels.size() >= 3
                                       ? "Wide multi-shot tank that controls space."
                                       : "Balanced starter tank.");
    ImGui::Spacing();
    if (locked) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
    }
    const bool changeClicked = ImGui::Button(locked ? "Locked" : "Change To This Tank", ImVec2(-1.0f, 32.0f));
    if (locked) {
        ImGui::PopStyleColor(3);
    }
    if (!locked && changeClicked) {
        player_.EvolveById(selectedConfig->id);
    }
    if (locked) {
        ImGui::TextWrapped("Need Rank %d. Use debug exp or play to unlock it.", selectedConfig->requiredRank);
    }
    if (ImGui::Button("Edit This In Class Editor", ImVec2(-1.0f, 28.0f))) {
        ui_.editorSelectedClassIndex_ = ui_.codexSelectedClassIndex_;
    }
    ImGui::EndChild();

    ImGui::Columns(1);
    ImGui::End();
#endif
}

// namespace

bool PlayerEvolution::LoadEvolutionCircuitTree(const std::string& path)
{
    ui_.evolutionCircuitNodes_.clear();
    ui_.evolutionCircuitEdges_.clear();
    ui_.evolutionCircuitLoaded_ = false;

    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    try {
        nlohmann::json json;
        file >> json;
        if (json.value("version", 0) != 1 || !json.contains("nodes") || !json["nodes"].is_array()) {
            return false;
        }
        for (const auto& node : json["nodes"]) {
            if (!node.is_object() || !node.contains("classId") || !node["classId"].is_string()) {
                continue;
            }
            const std::string classId = node["classId"].get<std::string>();
            if (!player_.GetClassConfig(classId) || ui_.evolutionCircuitNodes_.size() >= ui_.kEvolutionCircuitMaxNodes) {
                continue;
            }
            const float lane = node.value("lane", 0.5f);
            ui_.evolutionCircuitNodes_.push_back({classId, (std::clamp)(lane, 0.0f, 1.0f)});
        }
        if (json.contains("edges") && json["edges"].is_array()) {
            for (const auto& edge : json["edges"]) {
                if (!edge.is_object() || !edge.contains("from") || !edge.contains("to") || !edge["from"].is_string() ||
                    !edge["to"].is_string()) {
                    continue;
                }
                const std::string from = edge["from"].get<std::string>();
                const std::string to = edge["to"].get<std::string>();
                const auto hasNode = [&](const std::string& id) {
                    return std::any_of(ui_.evolutionCircuitNodes_.begin(), ui_.evolutionCircuitNodes_.end(),
                                       [&](const PlayerUiState::EvolutionCircuitNodeDefinition& node) {
                                           return node.classId == id;
                                       });
                };
                if (hasNode(from) && hasNode(to)) {
                    ui_.evolutionCircuitEdges_.push_back({from, to});
                }
            }
        }
    }
    catch (const std::exception&) {
        ui_.evolutionCircuitNodes_.clear();
        ui_.evolutionCircuitEdges_.clear();
        return false;
    }
    ui_.evolutionCircuitLoaded_ = !ui_.evolutionCircuitNodes_.empty();
    return ui_.evolutionCircuitLoaded_;
}

bool PlayerEvolution::ShouldUseEvolutionCircuitPrototype() const
{
    return ui_.evolutionUiStyle_.enabled && ui_.evolutionCircuitLoaded_ && !ui_.evolutionCircuitNodes_.empty();
}

void PlayerEvolution::InitializeEvolutionCircuitPrototype()
{
    cg2::StartupTrace::Scope scope("Player.EvolutionCircuitUi");
    if (!LoadEvolutionCircuitTree()) {
        return;
    }
    cg2::SpriteCommon* spriteCommon = cg2::SpriteCommon::GetInstance();
    auto makeSprite = [spriteCommon](const cg2::Vector2& anchor) {
        auto sprite = std::make_unique<cg2::Sprite>();
        sprite->Initialize(spriteCommon, "resources/white512x512.png");
        sprite->SetAnchorPoint(anchor);
        return sprite;
    };
    ui_.evolutionCircuitBackdropSprite_ = makeSprite({0.0f, 0.0f});
    ui_.evolutionCircuitDetailPanelSprite_ = makeSprite({0.5f, 0.5f});
    for (auto& line : ui_.evolutionCircuitLineSprites_) {
        line = makeSprite({0.0f, 0.5f});
    }
    for (size_t i = 0; i < ui_.evolutionCircuitNodes_.size(); ++i) {
        ui_.evolutionCircuitTankButtons_[i] = std::make_unique<TankButtonUI>();
        ui_.evolutionCircuitTankButtons_[i]->Initialize(spriteCommon);
    }
    ui_.evolutionCircuitDetailPreview_ = std::make_unique<TankButtonUI>();
    ui_.evolutionCircuitDetailPreview_->Initialize(spriteCommon);

    ui_.evolutionCircuitSelectedNode_ = 0;
    if (ui_.evolutionHistory_.empty() || ui_.evolutionHistory_.back() != player_.currentClassId_) {
        ui_.evolutionHistory_.clear();
        ui_.evolutionHistory_.push_back(player_.currentClassId_);
    }
    for (size_t i = 0; i < ui_.evolutionCircuitNodes_.size(); ++i) {
        if (ui_.evolutionCircuitNodes_[i].classId == player_.currentClassId_) {
            ui_.evolutionCircuitSelectedNode_ = static_cast<int>(i);
            break;
        }
    }
    UpdateEvolutionCircuitPrototype();
}

void PlayerEvolution::UpdateEvolutionCircuitPrototype()
{
    if (!ShouldUseEvolutionCircuitPrototype()) {
        return;
    }
    if (player_.input_ && player_.input_->IsTrigger(player_.input_->GetKey()[DIK_ESCAPE], player_.input_->GetPreKey()[DIK_ESCAPE])) {
        player_.isChangeMode = false;
        player_.evolutionCancelledEvent_ = true;
        return;
    }
    if (ui_.evolutionHistory_.empty() || ui_.evolutionHistory_.back() != player_.currentClassId_) {
        ui_.evolutionHistory_.clear();
        ui_.evolutionHistory_.push_back(player_.currentClassId_);
    }

    constexpr float kTreeLeft = 174.0f;
    constexpr float kTreeRight = 1106.0f;
    constexpr float kTreeTop = 100.0f;
    constexpr float kTreeBottom = 498.0f;
    constexpr cg2::Vector2 kNodeSize{158.0f, 64.0f};
    const float renderScale = GetEvolutionRenderScale();
    const cg2::Vector2 mouseVirtual = EvolutionClientToVirtual(player_.mousePosition_);
    const auto findNodeIndex = [&](const std::string& id) -> int {
        for (size_t i = 0; i < ui_.evolutionCircuitNodes_.size(); ++i) {
            if (ui_.evolutionCircuitNodes_[i].classId == id && player_.IsEvolutionClassVisible(id))
                return static_cast<int>(i);
        }
        return -1;
    };

    for (size_t i = 0; i < ui_.evolutionCircuitNodes_.size(); ++i) {
        const PlayerClassConfig* config = player_.GetClassConfig(ui_.evolutionCircuitNodes_[i].classId);
        const int rank = config ? (std::clamp)(config->requiredRank, 1, 4) : 1;
        const float rankRatio = static_cast<float>(rank - 1) / 3.0f;
        ui_.evolutionCircuitNodeCentersVirtual_[i] = {kTreeLeft + (kTreeRight - kTreeLeft) * rankRatio,
                                                      kTreeTop + (kTreeBottom - kTreeTop) * ui_.evolutionCircuitNodes_[i].lane};
    }

    ui_.evolutionCircuitHoveredNode_ = -1;
    for (size_t i = 0; i < ui_.evolutionCircuitNodes_.size(); ++i) {
        if (!player_.IsEvolutionClassVisible(ui_.evolutionCircuitNodes_[i].classId))
            continue;
        const cg2::Vector2 center = ui_.evolutionCircuitNodeCentersVirtual_[i];
        if (mouseVirtual.x >= center.x - kNodeSize.x * 0.5f && mouseVirtual.x <= center.x + kNodeSize.x * 0.5f &&
            mouseVirtual.y >= center.y - kNodeSize.y * 0.5f && mouseVirtual.y <= center.y + kNodeSize.y * 0.5f) {
            ui_.evolutionCircuitHoveredNode_ = static_cast<int>(i);
            break;
        }
    }
    const bool primaryTriggered = player_.input_ && player_.input_->IsTrigger(player_.input_->GetMouseState().rgbButtons[0],
                                                                              player_.input_->GetPreMouseState().rgbButtons[0]);
    if (primaryTriggered && ui_.evolutionCircuitHoveredNode_ >= 0) {
        ui_.evolutionCircuitSelectedNode_ = ui_.evolutionCircuitHoveredNode_;
    }
    ui_.evolutionCircuitSelectedNode_ =
        (std::clamp)(ui_.evolutionCircuitSelectedNode_, 0, static_cast<int>(ui_.evolutionCircuitNodes_.size()) - 1);
    if (!player_.IsEvolutionClassVisible(ui_.evolutionCircuitNodes_[static_cast<size_t>(ui_.evolutionCircuitSelectedNode_)].classId)) {
        const int currentNode = findNodeIndex(player_.currentClassId_);
        if (currentNode >= 0)
            ui_.evolutionCircuitSelectedNode_ = currentNode;
    }
    const std::string& selectedClassId = ui_.evolutionCircuitNodes_[static_cast<size_t>(ui_.evolutionCircuitSelectedNode_)].classId;
    const bool confirmTriggered =
        player_.input_ && player_.input_->IsTrigger(player_.input_->GetKey()[DIK_RETURN], player_.input_->GetPreKey()[DIK_RETURN]);
    if (confirmTriggered && player_.CanEvolveTo(selectedClassId)) {
        player_.TryConfirmEvolutionById(selectedClassId);
        return;
    }

    std::vector<bool> selectedPath(ui_.evolutionCircuitNodes_.size(), false);
    selectedPath[static_cast<size_t>(ui_.evolutionCircuitSelectedNode_)] = true;
    for (size_t pass = 0; pass < ui_.evolutionCircuitNodes_.size(); ++pass) {
        for (const auto& edge : ui_.evolutionCircuitEdges_) {
            const int from = findNodeIndex(edge.from);
            const int to = findNodeIndex(edge.to);
            if (from >= 0 && to >= 0 && selectedPath[static_cast<size_t>(to)])
                selectedPath[static_cast<size_t>(from)] = true;
        }
    }

    ui_.evolutionCircuitBackdropSprite_->SetPosition({0.0f, 0.0f});
    ui_.evolutionCircuitBackdropSprite_->SetSize({static_cast<float>(cg2::WinApp::GetInstance()->GetClientWidth()),
                                                  static_cast<float>(cg2::WinApp::GetInstance()->GetClientHeight())});
    ui_.evolutionCircuitBackdropSprite_->SetColor({0.004f, 0.010f, 0.024f, 0.65f});
    ui_.evolutionCircuitBackdropSprite_->Update();
    ui_.evolutionCircuitDetailPanelSprite_->SetPosition(EvolutionVirtualToRender({640.0f, 611.0f}));
    ui_.evolutionCircuitDetailPanelSprite_->SetSize({1160.0f * renderScale, 172.0f * renderScale});
    ui_.evolutionCircuitDetailPanelSprite_->SetColor({0.012f, 0.030f, 0.052f, 0.96f});
    ui_.evolutionCircuitDetailPanelSprite_->Update();

    const float pulse = 0.82f + std::sin(player_.evolutionUiTimer_ * 3.0f) * 0.18f;
    for (size_t i = 0; i < ui_.evolutionCircuitNodes_.size(); ++i) {
        const PlayerClassConfig* config = player_.GetClassConfig(ui_.evolutionCircuitNodes_[i].classId);
        if (!config || !ui_.evolutionCircuitTankButtons_[i] || !ui_.tankButtonUiStyle_)
            continue;
        if (!player_.IsEvolutionClassVisible(config->id))
            continue;
        const bool isCurrent = ui_.evolutionCircuitNodes_[i].classId == player_.currentClassId_;
        const bool isSelected = static_cast<int>(i) == ui_.evolutionCircuitSelectedNode_;
        const bool isHovered = static_cast<int>(i) == ui_.evolutionCircuitHoveredNode_;
        const bool hasDirectEdge = player_.HasEvolutionEdge(player_.currentClassId_, config->id);
        const bool available = player_.CanEvolveTo(config->id);
        const bool rankLocked = hasDirectEdge && player_.GetRankFromLevel(player_.level_) < config->requiredRank;
        cg2::Vector4 nodeColor{0.30f, 0.36f, 0.40f, 0.48f};
        if (available)
            nodeColor = {0.58f, 0.78f, 0.84f, 0.76f};
        if (rankLocked)
            nodeColor = {0.30f, 0.33f, 0.36f, 0.50f};
        if (isHovered)
            nodeColor = {0.34f, 0.82f, 0.94f, 0.88f};
        if (isSelected)
            nodeColor = {0.22f, 0.91f, 1.0f, pulse};
        if (isCurrent)
            nodeColor = {0.35f, 1.0f, 0.54f, 1.0f};

        TankButtonVisualData visualData{};
        if (player_.GetTankButtonVisualData(config->id, visualData)) {
            visualData.hiraganaName = config->displayName;
            ui_.evolutionCircuitTankButtons_[i]->SetVisualData(visualData);
        }
        TankButtonUiStyle style = *ui_.tankButtonUiStyle_;
        style.buttonWidth = kNodeSize.x * renderScale;
        style.buttonHeight = kNodeSize.y * renderScale;
        style.cornerRadius = 8.0f * renderScale;
        style.borderWidth = 1.5f * renderScale;
        style.glowWidth = (isCurrent || isSelected ? 8.0f : isHovered ? 6.0f : 4.0f) * renderScale;
        style.glowIntensity = isCurrent    ? (isHovered || isSelected ? 1.12f : 1.0f)
                              : isSelected ? pulse
                              : isHovered  ? 0.62f
                              : available  ? 0.34f
                              : rankLocked ? 0.12f
                                           : 0.16f;
        style.iconScale = 0.61f * renderScale;
        style.iconOffsetY = -8.0f * renderScale;
        style.labelOffsetY = 22.0f * renderScale;
        style.labelFontSize = 13.5f * renderScale;
        style.labelOutlineWidth *= renderScale;
        style.fillColor = {0.008f, 0.021f, 0.040f, 1.0f};
        style.lockedTint = {0.50f, 0.54f, 0.58f, 0.72f};
        for (cg2::Vector4& color : style.borderColors)
            color = nodeColor;
        for (cg2::Vector4& color : style.glowColors)
            color = nodeColor;
        ui_.evolutionCircuitTankButtons_[i]->SetRank(config->requiredRank);
        ui_.evolutionCircuitTankButtons_[i]->SetState(isCurrent    ? TankButtonState::Selected
                                                      : isSelected ? TankButtonState::Selected
                                                      : isHovered  ? TankButtonState::Hover
                                                      : rankLocked ? TankButtonState::Locked
                                                                   : TankButtonState::Normal);
        ui_.evolutionCircuitTankButtons_[i]->Update(EvolutionVirtualToRender(ui_.evolutionCircuitNodeCentersVirtual_[i]), style);
    }

    for (auto& line : ui_.evolutionCircuitLineSprites_) {
        line->SetSize({0.0f, 0.0f});
        line->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
        line->Update();
    }
    size_t lineIndex = 0;
    auto queueSegment = [&](const cg2::Vector2& fromVirtual, const cg2::Vector2& toVirtual, const cg2::Vector4& color, bool highlighted) {
        const cg2::Vector2 from = EvolutionVirtualToRender(fromVirtual);
        const cg2::Vector2 to = EvolutionVirtualToRender(toVirtual);
        const float dx = to.x - from.x;
        const float dy = to.y - from.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        const std::array<float, 3> widths{9.0f, 4.0f, 1.5f};
        const std::array<float, 3> highlightedAlphas{0.07f, 0.22f, 0.72f};
        const std::array<float, 3> normalAlphas{0.035f, 0.11f, 0.45f};
        const auto& alphas = highlighted ? highlightedAlphas : normalAlphas;
        for (size_t layer = 0; layer < widths.size() && lineIndex < ui_.evolutionCircuitLineSprites_.size(); ++layer) {
            cg2::Sprite* line = ui_.evolutionCircuitLineSprites_[lineIndex++].get();
            line->SetPosition(from);
            line->SetSize({length, widths[layer] * renderScale});
            line->SetRotation(std::atan2(dy, dx));
            cg2::Vector4 layerColor = color;
            layerColor.w *= alphas[layer];
            line->SetColor(layerColor);
            line->Update();
        }
    };
    for (const auto& edge : ui_.evolutionCircuitEdges_) {
        const int fromIndex = findNodeIndex(edge.from);
        const int toIndex = findNodeIndex(edge.to);
        if (fromIndex < 0 || toIndex < 0)
            continue;
        cg2::Vector4 color{0.46f, 0.56f, 0.61f, 0.40f};
        bool highlighted = false;
        bool traversed = false;
        for (size_t historyIndex = 1; historyIndex < ui_.evolutionHistory_.size(); ++historyIndex) {
            if (ui_.evolutionHistory_[historyIndex - 1] == edge.from && ui_.evolutionHistory_[historyIndex] == edge.to) {
                traversed = true;
                break;
            }
        }
        if (traversed) {
            color = {0.30f, 1.0f, 0.50f, 0.80f};
            highlighted = true;
        } else if (selectedPath[static_cast<size_t>(fromIndex)] && selectedPath[static_cast<size_t>(toIndex)]) {
            color = {0.20f, 0.90f, 1.0f, 0.78f};
            highlighted = true;
        }
        const cg2::Vector2 start{ui_.evolutionCircuitNodeCentersVirtual_[fromIndex].x + kNodeSize.x * 0.5f,
                                 ui_.evolutionCircuitNodeCentersVirtual_[fromIndex].y};
        const cg2::Vector2 end{ui_.evolutionCircuitNodeCentersVirtual_[toIndex].x - kNodeSize.x * 0.5f,
                               ui_.evolutionCircuitNodeCentersVirtual_[toIndex].y};
        const float midX = (start.x + end.x) * 0.5f;
        queueSegment(start, {midX, start.y}, color, highlighted);
        queueSegment({midX, start.y}, {midX, end.y}, color, highlighted);
        queueSegment({midX, end.y}, end, color, highlighted);
    }

    const PlayerClassConfig* selected =
        player_.GetClassConfig(ui_.evolutionCircuitNodes_[static_cast<size_t>(ui_.evolutionCircuitSelectedNode_)].classId);
    const PlayerClassConfig* current = player_.GetCurrentClassConfig();
    if (selected && ui_.evolutionCircuitDetailPreview_ && ui_.tankButtonUiStyle_) {
        TankButtonVisualData visualData{};
        if (player_.GetTankButtonVisualData(selected->id, visualData)) {
            visualData.hiraganaName = selected->displayName;
            ui_.evolutionCircuitDetailPreview_->SetVisualData(visualData);
        }
        TankButtonUiStyle style = *ui_.tankButtonUiStyle_;
        style.buttonWidth = 205.0f * renderScale;
        style.buttonHeight = 132.0f * renderScale;
        style.iconScale = 0.78f * renderScale;
        style.iconOffsetY = -18.0f * renderScale;
        style.labelOffsetY = 43.0f * renderScale;
        style.labelFontSize = 15.0f * renderScale;
        style.labelOutlineWidth *= renderScale;
        for (cg2::Vector4& color : style.borderColors)
            color = {0.22f, 0.88f, 1.0f, 0.92f};
        for (cg2::Vector4& color : style.glowColors)
            color = {0.18f, 0.78f, 1.0f, 0.88f};
        ui_.evolutionCircuitDetailPreview_->SetRank(selected->requiredRank);
        ui_.evolutionCircuitDetailPreview_->SetState(TankButtonState::Selected);
        ui_.evolutionCircuitDetailPreview_->Update(EvolutionVirtualToRender({188.0f, 611.0f}), style);
    }

    cg2::SpriteCommon* spriteCommon = cg2::SpriteCommon::GetInstance();
    auto makeTextStyle = [&](float fontSize, const cg2::Vector4& color) {
        cg2::TextStyle style{};
        style.fontFamily = ui_.evolutionUiStyle_.fontFamily;
        style.fontPath = ui_.evolutionUiStyle_.fontPath;
        style.fontWeight = ui_.evolutionUiStyle_.fontWeight;
        style.fontSize = fontSize * renderScale;
        style.color = color;
        style.outlineColor = ui_.evolutionUiStyle_.textOutlineColor;
        style.outlineThickness = 1.0f * renderScale;
        style.padding = 5.0f * renderScale;
        return style;
    };
    cg2::TextStyle titleStyle = makeTextStyle(27.0f, {0.72f, 1.0f, 0.94f, 1.0f});
    SetLabel(ui_.evolutionCircuitTitleLabel_, spriteCommon, "EVOLUTION CIRCUIT", EvolutionVirtualToRender({640.0f, 25.0f}), titleStyle);
    ui_.evolutionCircuitTitleLabel_->SetAnchorPoint({0.5f, 0.0f});
    for (int rank = 1; rank <= 4; ++rank) {
        const float rankRatio = static_cast<float>(rank - 1) / 3.0f;
        cg2::TextStyle rankStyle = makeTextStyle(19.0f, {0.64f, 0.89f, 0.96f, 0.96f});
        SetLabel(ui_.evolutionCircuitRankLabels_[static_cast<size_t>(rank - 1)], spriteCommon, "RANK " + std::to_string(rank),
                 EvolutionVirtualToRender({kTreeLeft + (kTreeRight - kTreeLeft) * rankRatio, 68.0f}), rankStyle);
        ui_.evolutionCircuitRankLabels_[static_cast<size_t>(rank - 1)]->SetAnchorPoint({0.5f, 0.5f});
    }
    if (selected) {
        cg2::TextStyle nameStyle = makeTextStyle(22.0f, {0.88f, 1.0f, 0.96f, 1.0f});
        cg2::TextStyle detailStyle = makeTextStyle(14.0f, {0.72f, 0.86f, 0.94f, 0.94f});
        SetLabel(ui_.evolutionCircuitDetailNameLabel_, spriteCommon, selected->displayName, EvolutionVirtualToRender({315.0f, 548.0f}),
                 nameStyle);
        SetLabel(ui_.evolutionCircuitDetailMetaLabel_, spriteCommon, "REQUIRED RANK " + std::to_string(selected->requiredRank),
                 EvolutionVirtualToRender({1115.0f, 554.0f}), detailStyle);
        ui_.evolutionCircuitDetailMetaLabel_->SetAnchorPoint({1.0f, 0.5f});
        SetLabel(ui_.evolutionCircuitDetailRoleLabel_, spriteCommon, GetEvolutionShortRole(*selected),
                 EvolutionVirtualToRender({315.0f, 587.0f}), detailStyle);
        auto formatFloatStat = [&](const char* name, float currentValue, float selectedValue) {
            char text[96]{};
            if (current && std::abs(currentValue - selectedValue) > 0.001f) {
                std::snprintf(text, sizeof(text), "%s   %.2f  →  %.2f", name, currentValue, selectedValue);
            } else {
                std::snprintf(text, sizeof(text), "%s   %.2f", name, selectedValue);
            }
            return std::string(text);
        };
        const int currentBarrels = current ? static_cast<int>(current->barrels.size()) : static_cast<int>(selected->barrels.size());
        const int selectedBarrels = static_cast<int>(selected->barrels.size());
        std::string barrelText = "砲身数   " + std::to_string(selectedBarrels);
        if (current && currentBarrels != selectedBarrels) {
            barrelText = "砲身数   " + std::to_string(currentBarrels) + "  →  " + std::to_string(selectedBarrels);
        }
        const std::array<std::string, 3> stats = {
            formatFloatStat("発射間隔", current ? current->reloadScale : selected->reloadScale, selected->reloadScale),
            formatFloatStat("拡散角", current ? current->spreadAngleDeg : selected->spreadAngleDeg, selected->spreadAngleDeg), barrelText};
        for (size_t i = 0; i < stats.size(); ++i) {
            SetLabel(ui_.evolutionCircuitDetailStatLabels_[i], spriteCommon, stats[i],
                     EvolutionVirtualToRender({610.0f, 550.0f + static_cast<float>(i) * 39.0f}), detailStyle);
        }
    }
    std::string stateHint = "PREVIEW MODE     ESC  閉じる";
    if (selected && selected->id == player_.currentClassId_) {
        stateHint = "CURRENT CLASS     ESC  閉じる";
    } else if (selected && player_.CanEvolveTo(selected->id)) {
        stateHint = "ENTER  進化     ESC  閉じる";
    } else if (selected && current && player_.HasEvolutionEdge(current->id, selected->id) &&
               selected->requiredRank == current->requiredRank + 1 && player_.GetRankFromLevel(player_.level_) < selected->requiredRank) {
        stateHint = "RANK " + std::to_string(selected->requiredRank) + " REQUIRED     ESC  閉じる";
    }
    cg2::TextStyle hintStyle = makeTextStyle(12.5f, {0.46f, 0.68f, 0.74f, 0.76f});
    SetLabel(ui_.evolutionCircuitHintLabel_, spriteCommon, stateHint, EvolutionVirtualToRender({1150.0f, 650.0f}), hintStyle);
    ui_.evolutionCircuitHintLabel_->SetAnchorPoint({1.0f, 0.5f});
    PrepareEvolutionCircuitTextTextures();

    if (ui_.staticEvolutionButtonBloomEffect_ && ui_.tankButtonUiStyle_) {
        cg2::BloomParam bloomParam = ui_.staticEvolutionButtonBloomEffect_->GetParam();
        bloomParam.threshold = 0.0f;
        bloomParam.intensity = 0.92f + ui_.tankButtonUiStyle_->bloomBoost * 1.8f;
        bloomParam.outlineWidth = 0.0f;
        ui_.staticEvolutionButtonBloomEffect_->SetParam(bloomParam);
        ui_.staticEvolutionButtonBloomEffect_->Update(0.0f);
    }
}

void PlayerEvolution::PrepareEvolutionCircuitTextTextures()
{
    auto prepare = [](cg2::TextLabel* label) {
        if (label)
            label->PrepareForDraw();
    };
    prepare(ui_.evolutionCircuitTitleLabel_.get());
    for (const auto& label : ui_.evolutionCircuitRankLabels_)
        prepare(label.get());
    prepare(ui_.evolutionCircuitDetailNameLabel_.get());
    prepare(ui_.evolutionCircuitDetailMetaLabel_.get());
    prepare(ui_.evolutionCircuitDetailRoleLabel_.get());
    for (const auto& label : ui_.evolutionCircuitDetailStatLabels_)
        prepare(label.get());
    prepare(ui_.evolutionCircuitHintLabel_.get());
    for (size_t i = 0; i < ui_.evolutionCircuitNodes_.size(); ++i) {
        if (ui_.evolutionCircuitTankButtons_[i])
            prepare(ui_.evolutionCircuitTankButtons_[i]->GetLabel());
    }
    if (ui_.evolutionCircuitDetailPreview_)
        prepare(ui_.evolutionCircuitDetailPreview_->GetLabel());
}

bool PlayerEvolution::ShouldUseStaticEvolutionPrototype() const
{
    if (!ui_.evolutionUiStyle_.enabled) {
        return false;
    }
    const PlayerClassConfig* current = player_.GetCurrentClassConfig();
    if (!current || current->requiredRank >= 4) {
        return false;
    }
    const int targetRank = current->requiredRank + 1;
    for (const std::string& id : player_.classCatalog_.OrderedIds()) {
        const PlayerClassConfig* config = player_.GetClassConfig(id);
        if (config && config->requiredRank == targetRank && player_.IsRunCompatibleClass(*config)) {
            return true;
        }
    }
    return false;
}

void PlayerEvolution::RefreshStaticEvolutionCandidates()
{
    ui_.staticEvolutionCandidateCount_ = 0;
    for (std::string& id : ui_.staticEvolutionCandidateIds_) {
        id.clear();
    }
    const PlayerClassConfig* current = player_.GetCurrentClassConfig();
    if (!current) {
        return;
    }
    const int targetRank = current->requiredRank + 1;
    for (const std::string& id : player_.classCatalog_.OrderedIds()) {
        const PlayerClassConfig* config = player_.GetClassConfig(id);
        if (!config || config->requiredRank != targetRank || config->id == current->id || !player_.IsRunCompatibleClass(*config)) {
            continue;
        }
        if (ui_.staticEvolutionCandidateCount_ >= ui_.staticEvolutionCandidateIds_.size()) {
            break;
        }
        ui_.staticEvolutionCandidateIds_[ui_.staticEvolutionCandidateCount_++] = config->id;
    }
    if (ui_.staticEvolutionCandidateCount_ == 0) {
        ui_.evolutionUiStyle_.fixedSelectedCandidate = 0;
    } else {
        ui_.evolutionUiStyle_.fixedSelectedCandidate =
            (std::clamp)(ui_.evolutionUiStyle_.fixedSelectedCandidate, 0, static_cast<int>(ui_.staticEvolutionCandidateCount_ - 1));
    }
}

std::string PlayerEvolution::GetEvolutionClassName(const std::string& classId) const
{
    static const std::unordered_map<std::string, std::string> names = {
        {"Basic", "BASIC"},
        {"Basic_Copy", "SWORD"},
        {"Twin", "TWIN"},
        {"MachineGun", "MACHINE GUN"},
        {"Overseer", "OVERSEER"},
        {"Triple", "TRIPLE"},
        {"Triple_Copy", "TRIPLE GUN"},
        {"Assassin", "ASSASSIN"},
        {"Bounder", "BOUNDER"},
        {"Ninja", "NINJA"},
        {"Smasher", "SMASHER"},
        {"Summoner", "SUMMONER"},
    };
    if (const auto it = names.find(classId); it != names.end()) {
        return it->second;
    }
    std::string result = classId;
    for (char& c : result) {
        if (c == '_') {
            c = ' ';
        } else {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
    }
    return result;
}

std::string PlayerEvolution::GetEvolutionShortRole(const PlayerClassConfig& config) const
{
    if (config.usesDrone || config.id == "Summoner")
        return "DRONE CONTROL";
    if (config.reflect)
        return "RICOCHET";
    if (config.id == "Ninja" || config.id == "Assassin")
        return "PRECISION";
    if (config.id == "Smasher")
        return "IMPACT";
    if (config.id == "Twin")
        return "DUAL FIRE";
    if (config.id == "MachineGun")
        return "SUPPRESSION";
    if (config.barrels.size() >= 3)
        return "MULTI BARREL";
    return "ADVANCED";
}

std::string PlayerEvolution::GetEvolutionRole(const PlayerClassConfig& config) const
{
    if (config.usesDrone || config.id == "Summoner")
        return "役割: ドローンを展開する支援制圧型";
    if (config.reflect)
        return "役割: 反射弾で空間を制圧する技巧型";
    if (config.id == "Ninja" || config.id == "Assassin")
        return "役割: 高速攻撃を狙う精密射撃型";
    if (config.id == "Smasher")
        return "役割: 高い衝撃力で押し切る近距離型";
    if (config.id == "MachineGun")
        return "役割: 弾幕で押す近中距離制圧型";
    if (config.barrels.size() >= 2)
        return "役割: 複数砲身を活かす連続射撃型";
    return "役割: 基礎性能を強化した万能型";
}

std::array<std::string, 3> PlayerEvolution::GetEvolutionDeltas(const PlayerClassConfig& current, const PlayerClassConfig& target) const
{
    char reload[64]{};
    const float currentReload = (std::max)(0.0001f, current.reloadScale);
    const int reloadPercent = static_cast<int>(std::round((target.reloadScale / currentReload - 1.0f) * 100.0f));
    std::snprintf(reload, sizeof(reload), "発射間隔  %+d%%", reloadPercent);
    char spread[64]{};
    std::snprintf(spread, sizeof(spread), "拡散角  %.0f° → %.0f°", current.spreadAngleDeg, target.spreadAngleDeg);
    return {"砲身  " + std::to_string(current.barrels.size()) + " → " + std::to_string(target.barrels.size()), std::string(reload),
            std::string(spread)};
}

std::string PlayerEvolution::GetEvolutionAbility(const PlayerClassConfig& config) const
{
    if (config.usesDrone || config.id == "Summoner") {
        return "固有能力: 最大" + std::to_string(config.maxDrones) + "機のドローンを展開";
    }
    if (config.reflect)
        return "固有能力: 発射した弾が障害物で反射";
    if (config.penetrate)
        return "固有能力: 敵を貫通する弾を発射";
    if (config.fireAllBarrels)
        return "固有能力: 全砲身から同時射撃";
    if (config.alternateBarrels)
        return "固有能力: 複数の砲身から交互に射撃";
    if (config.bulletCount > 1)
        return "固有能力: 1回の射撃で複数弾を発射";
    if (config.barrels.size() >= 2)
        return "固有能力: 複数砲身による多方向射撃";
    return "固有能力: 機体固有の武装構成";
}

float PlayerEvolution::GetEvolutionRenderScale() const
{
    const float virtualWidth = (std::max)(1.0f, ui_.evolutionUiStyle_.virtualResolution.x);
    const float virtualHeight = (std::max)(1.0f, ui_.evolutionUiStyle_.virtualResolution.y);
    const float clientWidth = static_cast<float>(cg2::WinApp::GetInstance()->GetClientWidth());
    const float clientHeight = static_cast<float>(cg2::WinApp::GetInstance()->GetClientHeight());
    return (std::min)(clientWidth / virtualWidth, clientHeight / virtualHeight);
}

cg2::Vector2 PlayerEvolution::GetEvolutionRenderOffset() const
{
    const float scale = GetEvolutionRenderScale();
    const float clientWidth = static_cast<float>(cg2::WinApp::GetInstance()->GetClientWidth());
    const float clientHeight = static_cast<float>(cg2::WinApp::GetInstance()->GetClientHeight());
    return {(clientWidth - ui_.evolutionUiStyle_.virtualResolution.x * scale) * 0.5f,
            (clientHeight - ui_.evolutionUiStyle_.virtualResolution.y * scale) * 0.5f};
}

cg2::Vector2 PlayerEvolution::EvolutionAnchorToVirtual(const cg2::Vector2& normalizedAnchor) const
{
    const float safe =
        (std::clamp)(ui_.evolutionUiStyle_.safeMargin, 0.0f,
                     (std::min)(ui_.evolutionUiStyle_.virtualResolution.x, ui_.evolutionUiStyle_.virtualResolution.y) * 0.45f);
    const cg2::Vector2 usable = {(std::max)(1.0f, ui_.evolutionUiStyle_.virtualResolution.x - safe * 2.0f),
                                 (std::max)(1.0f, ui_.evolutionUiStyle_.virtualResolution.y - safe * 2.0f)};
    return {safe + (std::clamp)(normalizedAnchor.x, 0.0f, 1.0f) * usable.x, safe + (std::clamp)(normalizedAnchor.y, 0.0f, 1.0f) * usable.y};
}

cg2::Vector2 PlayerEvolution::EvolutionVirtualToRender(const cg2::Vector2& virtualPosition) const
{
    const float scale = GetEvolutionRenderScale();
    const cg2::Vector2 offset = GetEvolutionRenderOffset();
    return {offset.x + virtualPosition.x * scale, offset.y + virtualPosition.y * scale};
}

cg2::Vector2 PlayerEvolution::EvolutionClientToVirtual(const cg2::Vector2& clientPosition) const
{
    const float clientWidth = static_cast<float>(cg2::WinApp::GetInstance()->GetClientWidth());
    const float clientHeight = static_cast<float>(cg2::WinApp::GetInstance()->GetClientHeight());
    const float virtualWidth = (std::max)(1.0f, ui_.evolutionUiStyle_.virtualResolution.x);
    const float virtualHeight = (std::max)(1.0f, ui_.evolutionUiStyle_.virtualResolution.y);
    const float scale = (std::max)(0.0001f, (std::min)(clientWidth / virtualWidth, clientHeight / virtualHeight));
    const cg2::Vector2 offset = {(clientWidth - virtualWidth * scale) * 0.5f, (clientHeight - virtualHeight * scale) * 0.5f};
    return {(clientPosition.x - offset.x) / scale, (clientPosition.y - offset.y) / scale};
}

void PlayerEvolution::InitializeStaticEvolutionPrototype()
{
    cg2::StartupTrace::Scope scope("Player.StaticEvolutionUi");
    cg2::SpriteCommon* spriteCommon = cg2::SpriteCommon::GetInstance();
    auto makeSprite = [spriteCommon](const std::string& texture, const cg2::Vector2& anchor) {
        auto sprite = std::make_unique<cg2::Sprite>();
        sprite->Initialize(spriteCommon, texture);
        sprite->SetAnchorPoint(anchor);
        return sprite;
    };

    ui_.staticEvolutionBackdropSprite_ = makeSprite("resources/white512x512.png", {0.0f, 0.0f});
    ui_.staticEvolutionDetailPanelSprite_ = makeSprite("resources/white512x512.png", {0.5f, 0.5f});
    ui_.staticEvolutionConfirmButtonSprite_ = makeSprite("resources/white512x512.png", {0.5f, 0.5f});
    ui_.staticEvolutionBranchGlowSprite_ = makeSprite("resources/white512x512.png", {0.5f, 0.5f});
    ui_.staticEvolutionBranchCoreSprite_ = makeSprite("resources/white512x512.png", {0.5f, 0.5f});
    for (auto& line : ui_.staticEvolutionConfirmOutlineSprites_) {
        line = makeSprite("resources/white512x512.png", {0.0f, 0.5f});
    }
    for (auto& nodePanels : ui_.staticEvolutionNodePanelSprites_) {
        for (auto& panel : nodePanels) {
            panel = makeSprite("resources/white512x512.png", {0.5f, 0.5f});
        }
    }
    for (auto& nodeLines : ui_.staticEvolutionNodeFrameSprites_) {
        for (auto& line : nodeLines) {
            line = makeSprite("resources/white512x512.png", {0.0f, 0.5f});
        }
    }
    for (auto& nodeLines : ui_.staticEvolutionSilhouetteSprites_) {
        for (auto& line : nodeLines) {
            line = makeSprite("resources/white512x512.png", {0.0f, 0.5f});
        }
    }
    for (auto& line : ui_.staticEvolutionCircuitSprites_) {
        line = makeSprite("resources/white512x512.png", {0.0f, 0.5f});
    }

    ui_.tankButtonUiStyle_ = std::make_unique<TankButtonUiStyle>();
    LoadTankButtonUiStyle(*ui_.tankButtonUiStyle_);
    for (size_t classIndex = 0; classIndex < ui_.staticEvolutionTankButtons_.size(); ++classIndex) {
        ui_.staticEvolutionTankButtons_[classIndex] = std::make_unique<TankButtonUI>();
        ui_.staticEvolutionTankButtons_[classIndex]->Initialize(spriteCommon);
    }
    ui_.staticEvolutionButtonBloomEffect_ = std::make_unique<cg2::ObjectPostEffect>();
    ui_.staticEvolutionButtonBloomEffect_->Initialize(cg2::Object3dCommon::GetInstance()->GetDxCommon(),
                                                      cg2::Object3dCommon::GetInstance()->GetSrvManager(), nullptr, 1.0f);
    ui_.staticEvolutionTextEffect_ = std::make_unique<NeonTextEffect>();
    ui_.staticEvolutionTextEffect_->Initialize(cg2::Object3dCommon::GetInstance()->GetDxCommon(),
                                               cg2::Object3dCommon::GetInstance()->GetSrvManager());
    ui_.staticEvolutionTextEffect_->SetStyle(ui_.evolutionUiStyle_.neonText);

    UpdateStaticEvolutionPrototype();
}

void PlayerEvolution::UpdateStaticEvolutionPrototype()
{
    if (!ui_.staticEvolutionBackdropSprite_) {
        return;
    }

    RefreshStaticEvolutionCandidates();
    if (ui_.staticEvolutionCandidateCount_ == 0) {
        return;
    }
    if (player_.input_ && player_.input_->IsTrigger(player_.input_->GetKey()[DIK_ESCAPE], player_.input_->GetPreKey()[DIK_ESCAPE])) {
        player_.isChangeMode = false;
        player_.evolutionCancelledEvent_ = true;
        return;
    }

    const size_t activeNodeCount = ui_.staticEvolutionCandidateCount_ + 1;
    const float renderScale = GetEvolutionRenderScale();
    const cg2::Vector2 mouseVirtual = EvolutionClientToVirtual(player_.mousePosition_);
    const auto& activeAnchors =
        ui_.evolutionUiStyle_.radialLayout ? ui_.evolutionUiStyle_.radialNodeAnchors : ui_.evolutionUiStyle_.nodeAnchors;

    ui_.staticEvolutionNodeCentersVirtual_[0] = EvolutionAnchorToVirtual(activeAnchors[0]);
    for (size_t candidateIndex = 0; candidateIndex < ui_.staticEvolutionCandidateCount_; ++candidateIndex) {
        cg2::Vector2 anchor{};
        if (ui_.staticEvolutionCandidateCount_ == 3) {
            anchor = activeAnchors[candidateIndex + 1];
        } else if (ui_.evolutionUiStyle_.radialLayout) {
            const float angle = -1.5707963268f +
                                static_cast<float>(candidateIndex) * 6.2831853072f / static_cast<float>(ui_.staticEvolutionCandidateCount_);
            anchor = {0.50f + std::cos(angle) * 0.30f, 0.40f + std::sin(angle) * 0.27f};
        } else {
            const float y =
                ui_.staticEvolutionCandidateCount_ == 1
                    ? 0.43f
                    : 0.13f + static_cast<float>(candidateIndex) * (0.54f / static_cast<float>(ui_.staticEvolutionCandidateCount_ - 1));
            anchor = {activeAnchors[1].x, y};
        }
        ui_.staticEvolutionNodeCentersVirtual_[candidateIndex + 1] = EvolutionAnchorToVirtual(anchor);
    }

    for (size_t i = 0; i < activeNodeCount; ++i) {
        const cg2::Vector2 baseSize = i == 0 ? ui_.evolutionUiStyle_.currentNodeSize : ui_.evolutionUiStyle_.candidateNodeSize;
        ui_.staticEvolutionNodeHitSizesVirtual_[i] = {baseSize.x + 16.0f, baseSize.y + 16.0f};
    }

    ui_.staticEvolutionHoveredNode_ = -1;
    for (size_t i = 1; i < activeNodeCount; ++i) {
        const cg2::Vector2 center = ui_.staticEvolutionNodeCentersVirtual_[i];
        const cg2::Vector2 hitSize = ui_.staticEvolutionNodeHitSizesVirtual_[i];
        if (mouseVirtual.x >= center.x - hitSize.x * 0.5f && mouseVirtual.x <= center.x + hitSize.x * 0.5f &&
            mouseVirtual.y >= center.y - hitSize.y * 0.5f && mouseVirtual.y <= center.y + hitSize.y * 0.5f) {
            ui_.staticEvolutionHoveredNode_ = static_cast<int>(i);
            break;
        }
    }
    const bool primaryTriggered = player_.input_ && player_.input_->IsTrigger(player_.input_->GetMouseState().rgbButtons[0],
                                                                              player_.input_->GetPreMouseState().rgbButtons[0]);
    if (primaryTriggered && ui_.staticEvolutionHoveredNode_ > 0) {
        ui_.evolutionUiStyle_.fixedSelectedCandidate = ui_.staticEvolutionHoveredNode_ - 1;
    }

    ui_.staticEvolutionBackdropSprite_->SetPosition({0.0f, 0.0f});
    ui_.staticEvolutionBackdropSprite_->SetSize({static_cast<float>(cg2::WinApp::GetInstance()->GetClientWidth()),
                                                 static_cast<float>(cg2::WinApp::GetInstance()->GetClientHeight())});
    ui_.staticEvolutionBackdropSprite_->SetColor({0.005f, 0.012f, 0.025f, ui_.evolutionUiStyle_.backgroundDimOpacity});
    ui_.staticEvolutionBackdropSprite_->Update();

    const int currentRank = player_.GetRankFromLevel(player_.level_);
    for (size_t i = 0; i < activeNodeCount; ++i) {
        const bool selected = i > 0 && i - 1 == ui_.evolutionUiStyle_.fixedSelectedCandidate;
        const bool hovered = i == ui_.staticEvolutionHoveredNode_;
        const std::string& nodeClassId = i == 0 ? player_.currentClassId_ : ui_.staticEvolutionCandidateIds_[i - 1];
        const PlayerClassConfig* candidateConfig = i > 0 ? player_.GetClassConfig(nodeClassId) : nullptr;
        const PlayerClassConfig* nodeConfig = player_.GetClassConfig(nodeClassId);
        const bool locked = candidateConfig && currentRank < candidateConfig->requiredRank;
        float stateScale = ui_.evolutionUiStyle_.normalScale;
        if (selected) {
            stateScale = ui_.evolutionUiStyle_.selectedScale;
        } else if (hovered) {
            stateScale = ui_.evolutionUiStyle_.hoverScale;
        }
        const cg2::Vector2 baseSize = i == 0 ? ui_.evolutionUiStyle_.currentNodeSize : ui_.evolutionUiStyle_.candidateNodeSize;
        const cg2::Vector2 drawSize = {(std::max)(1.0f, baseSize.x * stateScale), (std::max)(1.0f, baseSize.y * stateScale)};
        ui_.staticEvolutionNodeDrawSizesVirtual_[i] = drawSize;

        const float cut = (std::clamp)(ui_.evolutionUiStyle_.nodeCornerCut, 0.0f, drawSize.y * 0.30f);
        cg2::Vector4 panelColor = ui_.evolutionUiStyle_.panelColor;
        panelColor.w = locked ? 0.97f : 0.91f;
        const std::array<cg2::Vector2, 3> panelPositions = {
            {ui_.staticEvolutionNodeCentersVirtual_[i],
             {ui_.staticEvolutionNodeCentersVirtual_[i].x, ui_.staticEvolutionNodeCentersVirtual_[i].y - drawSize.y * 0.5f + cut * 0.5f},
             {ui_.staticEvolutionNodeCentersVirtual_[i].x, ui_.staticEvolutionNodeCentersVirtual_[i].y + drawSize.y * 0.5f - cut * 0.5f}}};
        const std::array<cg2::Vector2, 3> panelSizes = {{{drawSize.x, (std::max)(1.0f, drawSize.y - cut * 2.0f)},
                                                         {(std::max)(1.0f, drawSize.x - cut * 2.0f), cut},
                                                         {(std::max)(1.0f, drawSize.x - cut * 2.0f), cut}}};
        for (int panelIndex = 0; panelIndex < 3; ++panelIndex) {
            cg2::Sprite* panel = ui_.staticEvolutionNodePanelSprites_[i][panelIndex].get();
            panel->SetPosition(EvolutionVirtualToRender(panelPositions[panelIndex]));
            panel->SetSize({panelSizes[panelIndex].x * renderScale, panelSizes[panelIndex].y * renderScale});
            panel->SetColor(panelColor);
            panel->Update();
        }

        if (ui_.staticEvolutionTankButtons_[i] && ui_.tankButtonUiStyle_) {
            TankButtonVisualData visualData{};
            if (player_.GetTankButtonVisualData(nodeClassId, visualData)) {
                ui_.staticEvolutionTankButtons_[i]->SetVisualData(visualData);
            }
            TankButtonUiStyle renderedButtonStyle = *ui_.tankButtonUiStyle_;
            renderedButtonStyle.buttonWidth = drawSize.x * renderScale;
            renderedButtonStyle.buttonHeight = drawSize.y * renderScale;
            renderedButtonStyle.cornerRadius = ui_.evolutionUiStyle_.nodeCornerCut * renderScale;
            renderedButtonStyle.borderWidth = ui_.evolutionUiStyle_.nodeOutlineWidth * renderScale;
            renderedButtonStyle.glowWidth = ui_.evolutionUiStyle_.nodeOutlineGlowWidth * renderScale;
            renderedButtonStyle.iconOffsetY *= renderScale;
            renderedButtonStyle.labelOffsetY *= renderScale;
            renderedButtonStyle.labelFontSize *= renderScale;
            renderedButtonStyle.labelOutlineWidth *= renderScale;
            TankButtonState buttonState = TankButtonState::Normal;
            if (locked) {
                buttonState = TankButtonState::Locked;
            } else if (selected) {
                buttonState = TankButtonState::Selected;
            } else if (hovered) {
                buttonState = TankButtonState::Hover;
            }
            ui_.staticEvolutionTankButtons_[i]->SetRank(nodeConfig ? nodeConfig->requiredRank : 1);
            ui_.staticEvolutionTankButtons_[i]->SetState(buttonState);
            ui_.staticEvolutionTankButtons_[i]->Update(EvolutionVirtualToRender(ui_.staticEvolutionNodeCentersVirtual_[i]),
                                                       renderedButtonStyle);
        }
    }
    if (ui_.staticEvolutionButtonBloomEffect_ && ui_.tankButtonUiStyle_) {
        cg2::BloomParam bloomParam = ui_.staticEvolutionButtonBloomEffect_->GetParam();
        bloomParam.threshold = 0.0f;
        bloomParam.intensity = 1.10f + ui_.tankButtonUiStyle_->bloomBoost * 2.5f;
        bloomParam.outlineWidth = 0.0f;
        ui_.staticEvolutionButtonBloomEffect_->SetParam(bloomParam);
        ui_.staticEvolutionButtonBloomEffect_->Update(0.0f);
    }
    if (ui_.staticEvolutionTextEffect_) {
        ui_.staticEvolutionTextEffect_->SetStyle(ui_.evolutionUiStyle_.neonText);
    }

    const cg2::Vector2 panelCenterVirtual = EvolutionAnchorToVirtual(ui_.evolutionUiStyle_.detailPanelAnchor);
    ui_.staticEvolutionDetailPanelSprite_->SetPosition(EvolutionVirtualToRender(panelCenterVirtual));
    ui_.staticEvolutionDetailPanelSprite_->SetSize(
        {ui_.evolutionUiStyle_.detailPanelSize.x * renderScale, ui_.evolutionUiStyle_.detailPanelSize.y * renderScale});
    ui_.staticEvolutionDetailPanelSprite_->SetColor(ui_.evolutionUiStyle_.panelColor);
    ui_.staticEvolutionDetailPanelSprite_->Update();

    const cg2::Vector2 buttonCenterVirtual = {panelCenterVirtual.x + ui_.evolutionUiStyle_.detailPanelSize.x * 0.5f -
                                                  ui_.evolutionUiStyle_.confirmButtonSize.x * 0.5f - 18.0f,
                                              panelCenterVirtual.y - ui_.evolutionUiStyle_.detailPanelSize.y * 0.5f + 32.0f};
    ui_.staticEvolutionConfirmHovered_ = mouseVirtual.x >= buttonCenterVirtual.x - ui_.evolutionUiStyle_.confirmButtonSize.x * 0.5f &&
                                         mouseVirtual.x <= buttonCenterVirtual.x + ui_.evolutionUiStyle_.confirmButtonSize.x * 0.5f &&
                                         mouseVirtual.y >= buttonCenterVirtual.y - ui_.evolutionUiStyle_.confirmButtonSize.y * 0.5f &&
                                         mouseVirtual.y <= buttonCenterVirtual.y + ui_.evolutionUiStyle_.confirmButtonSize.y * 0.5f;
    const std::string& selectedClassId =
        ui_.staticEvolutionCandidateIds_[static_cast<size_t>(ui_.evolutionUiStyle_.fixedSelectedCandidate)];
    const bool canConfirm = player_.CanEvolveTo(selectedClassId);
    ui_.staticEvolutionConfirmButtonSprite_->SetPosition(EvolutionVirtualToRender(buttonCenterVirtual));
    ui_.staticEvolutionConfirmButtonSprite_->SetSize(
        {ui_.evolutionUiStyle_.confirmButtonSize.x * renderScale, ui_.evolutionUiStyle_.confirmButtonSize.y * renderScale});
    cg2::Vector4 buttonColor = ui_.evolutionUiStyle_.panelColor;
    buttonColor.x *= 0.72f;
    buttonColor.y *= 0.72f;
    buttonColor.z *= 0.72f;
    buttonColor.w = 0.98f;
    if (!canConfirm) {
        buttonColor = {buttonColor.x * ui_.evolutionUiStyle_.lockedColor.x, buttonColor.y * ui_.evolutionUiStyle_.lockedColor.y,
                       buttonColor.z * ui_.evolutionUiStyle_.lockedColor.z, buttonColor.w * ui_.evolutionUiStyle_.lockedColor.w};
    }
    ui_.staticEvolutionConfirmButtonSprite_->SetColor(buttonColor);
    ui_.staticEvolutionConfirmButtonSprite_->Update();

    const float buttonCut = 7.0f;
    const float halfButtonW = ui_.evolutionUiStyle_.confirmButtonSize.x * 0.5f;
    const float halfButtonH = ui_.evolutionUiStyle_.confirmButtonSize.y * 0.5f;
    const std::array<cg2::Vector2, 8> buttonPoints = {
        {{buttonCenterVirtual.x - halfButtonW + buttonCut, buttonCenterVirtual.y - halfButtonH},
         {buttonCenterVirtual.x + halfButtonW - buttonCut, buttonCenterVirtual.y - halfButtonH},
         {buttonCenterVirtual.x + halfButtonW, buttonCenterVirtual.y - halfButtonH + buttonCut},
         {buttonCenterVirtual.x + halfButtonW, buttonCenterVirtual.y + halfButtonH - buttonCut},
         {buttonCenterVirtual.x + halfButtonW - buttonCut, buttonCenterVirtual.y + halfButtonH},
         {buttonCenterVirtual.x - halfButtonW + buttonCut, buttonCenterVirtual.y + halfButtonH},
         {buttonCenterVirtual.x - halfButtonW, buttonCenterVirtual.y + halfButtonH - buttonCut},
         {buttonCenterVirtual.x - halfButtonW, buttonCenterVirtual.y - halfButtonH + buttonCut}}};
    for (int i = 0; i < 8; ++i) {
        const cg2::Vector2 a = EvolutionVirtualToRender(buttonPoints[i]);
        const cg2::Vector2 b = EvolutionVirtualToRender(buttonPoints[(i + 1) % 8]);
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        cg2::Sprite* line = ui_.staticEvolutionConfirmOutlineSprites_[i].get();
        line->SetPosition(a);
        line->SetRotation(std::atan2(dy, dx));
        line->SetSize({std::sqrt(dx * dx + dy * dy), (std::max)(1.0f, 1.6f * renderScale)});
        cg2::Vector4 outlineColor = !canConfirm                          ? ui_.evolutionUiStyle_.lockedColor
                                    : ui_.staticEvolutionConfirmHovered_ ? ui_.evolutionUiStyle_.hoverColor
                                                                         : ui_.evolutionUiStyle_.availableColor;
        outlineColor.w = 0.90f;
        line->SetColor(outlineColor);
        line->Update();
    }

    UpdateStaticEvolutionNodeFrames();
    UpdateStaticEvolutionSilhouettes();
    UpdateStaticEvolutionCircuit();
    UpdateStaticEvolutionText();
    // TextLabelのフォント差し替えはテクスチャ転送を伴う。描画中に遅延更新すると
    // 転送処理がコマンドリストをResetするため、必ずUpdate段階で同期しておく。
    PrepareStaticEvolutionTextTextures();

    const bool confirmTriggered =
        player_.input_ && player_.input_->IsTrigger(player_.input_->GetKey()[DIK_RETURN], player_.input_->GetPreKey()[DIK_RETURN]);
    if (canConfirm && (confirmTriggered || (primaryTriggered && ui_.staticEvolutionConfirmHovered_))) {
        player_.TryConfirmEvolutionById(selectedClassId);
    }
}

void PlayerEvolution::UpdateStaticEvolutionCircuit()
{
    const cg2::Vector2 current = ui_.staticEvolutionNodeCentersVirtual_[0];
    for (int& count : ui_.staticEvolutionCircuitControlPointCounts_) {
        count = 0;
    }
    int pathCount = 0;
    if (ui_.evolutionUiStyle_.radialLayout) {
        for (size_t candidateIndex = 0; candidateIndex < ui_.staticEvolutionCandidateCount_; ++candidateIndex) {
            ui_.staticEvolutionCircuitControlPoints_[candidateIndex] = {
                {current, ui_.staticEvolutionNodeCentersVirtual_[candidateIndex + 1], {}, {}}};
            ui_.staticEvolutionCircuitControlPointCounts_[candidateIndex] = 2;
        }
        pathCount = static_cast<int>(ui_.staticEvolutionCandidateCount_);
        ui_.staticEvolutionBranchGlowSprite_->SetSize({0.0f, 0.0f});
        ui_.staticEvolutionBranchCoreSprite_->SetSize({0.0f, 0.0f});
    } else {
        const cg2::Vector2 branch = EvolutionAnchorToVirtual(ui_.evolutionUiStyle_.branchPointAnchor);
        ui_.staticEvolutionCircuitControlPoints_[0] = {{current, branch, {}, {}}};
        ui_.staticEvolutionCircuitControlPointCounts_[0] = 2;
        for (size_t candidateIndex = 0; candidateIndex < ui_.staticEvolutionCandidateCount_; ++candidateIndex) {
            ui_.staticEvolutionCircuitControlPoints_[candidateIndex + 1] = {
                {branch, ui_.staticEvolutionNodeCentersVirtual_[candidateIndex + 1], {}, {}}};
            ui_.staticEvolutionCircuitControlPointCounts_[candidateIndex + 1] = 2;
        }
        pathCount = static_cast<int>(ui_.staticEvolutionCandidateCount_ + 1);
        const float renderScale = GetEvolutionRenderScale();
        const cg2::Vector2 renderBranch = EvolutionVirtualToRender(branch);
        ui_.staticEvolutionBranchGlowSprite_->SetPosition(renderBranch);
        ui_.staticEvolutionBranchGlowSprite_->SetRotation(0.785398163f);
        ui_.staticEvolutionBranchGlowSprite_->SetSize({20.0f * renderScale, 20.0f * renderScale});
        cg2::Vector4 branchGlow = ui_.evolutionUiStyle_.selectedColor;
        branchGlow.w = 0.16f;
        ui_.staticEvolutionBranchGlowSprite_->SetColor(branchGlow);
        ui_.staticEvolutionBranchGlowSprite_->Update();
        ui_.staticEvolutionBranchCoreSprite_->SetPosition(renderBranch);
        ui_.staticEvolutionBranchCoreSprite_->SetRotation(0.785398163f);
        ui_.staticEvolutionBranchCoreSprite_->SetSize({7.0f * renderScale, 7.0f * renderScale});
        cg2::Vector4 branchCore = ui_.evolutionUiStyle_.selectedColor;
        branchCore.w = 0.94f;
        ui_.staticEvolutionBranchCoreSprite_->SetColor(branchCore);
        ui_.staticEvolutionBranchCoreSprite_->Update();
    }

    const int currentRank = player_.GetRankFromLevel(player_.level_);
    const float renderScale = GetEvolutionRenderScale();
    size_t spriteIndex = 0;
    auto setLine = [&](cg2::Sprite* sprite, const cg2::Vector2& a, const cg2::Vector2& b, float width, const cg2::Vector4& color) {
        const cg2::Vector2 renderA = EvolutionVirtualToRender(a);
        const cg2::Vector2 renderB = EvolutionVirtualToRender(b);
        const float dx = renderB.x - renderA.x;
        const float dy = renderB.y - renderA.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        sprite->SetPosition(renderA);
        sprite->SetRotation(std::atan2(dy, dx));
        sprite->SetSize({length, (std::max)(0.5f, width * renderScale)});
        sprite->SetColor(color);
        sprite->Update();
    };

    for (int pathIndex = 0; pathIndex < pathCount; ++pathIndex) {
        const bool trunk = !ui_.evolutionUiStyle_.radialLayout && pathIndex == 0;
        const int candidateIndex = ui_.evolutionUiStyle_.radialLayout ? pathIndex : pathIndex - 1;
        const bool selected = trunk || candidateIndex == ui_.evolutionUiStyle_.fixedSelectedCandidate;
        const bool hovered = !trunk && candidateIndex + 1 == ui_.staticEvolutionHoveredNode_;
        const PlayerClassConfig* config =
            trunk || candidateIndex < 0 || candidateIndex >= static_cast<int>(ui_.staticEvolutionCandidateCount_)
                ? nullptr
                : player_.GetClassConfig(ui_.staticEvolutionCandidateIds_[static_cast<size_t>(candidateIndex)]);
        const bool locked = config && currentRank < config->requiredRank;
        cg2::Vector4 routeColor = ui_.evolutionUiStyle_.availableColor;
        float brightness = 0.38f;
        if (locked) {
            routeColor = ui_.evolutionUiStyle_.lockedColor;
            brightness = 0.14f;
        } else if (selected) {
            routeColor = ui_.evolutionUiStyle_.selectedColor;
            brightness = 1.0f;
        } else if (hovered) {
            routeColor = ui_.evolutionUiStyle_.hoverColor;
            brightness = 0.84f;
        }

        const int count = ui_.staticEvolutionCircuitControlPointCounts_[pathIndex];
        for (int pointIndex = 0; pointIndex + 1 < count; ++pointIndex) {
            const cg2::Vector2 a = ui_.staticEvolutionCircuitControlPoints_[pathIndex][pointIndex];
            const cg2::Vector2 b = ui_.staticEvolutionCircuitControlPoints_[pathIndex][pointIndex + 1];
            cg2::Vector4 outer = routeColor;
            cg2::Vector4 middle = routeColor;
            cg2::Vector4 core = routeColor;
            const float opacity = ui_.evolutionUiStyle_.circuitOpacity * brightness;
            outer.w = opacity * ui_.evolutionUiStyle_.circuitOuterAlpha;
            middle.w = opacity * ui_.evolutionUiStyle_.circuitMiddleAlpha;
            core.w = opacity * ui_.evolutionUiStyle_.circuitCoreAlpha;
            if (spriteIndex + 2 < ui_.staticEvolutionCircuitSprites_.size()) {
                setLine(ui_.staticEvolutionCircuitSprites_[spriteIndex++].get(), a, b, ui_.evolutionUiStyle_.circuitOuterGlowWidth, outer);
                setLine(ui_.staticEvolutionCircuitSprites_[spriteIndex++].get(), a, b, ui_.evolutionUiStyle_.circuitMiddleGlowWidth,
                        middle);
                setLine(ui_.staticEvolutionCircuitSprites_[spriteIndex++].get(), a, b, ui_.evolutionUiStyle_.circuitCoreWidth, core);
            }
        }
    }
    while (spriteIndex < ui_.staticEvolutionCircuitSprites_.size()) {
        cg2::Sprite* sprite = ui_.staticEvolutionCircuitSprites_[spriteIndex++].get();
        sprite->SetSize({0.0f, 0.0f});
        sprite->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
        sprite->Update();
    }
}

void PlayerEvolution::UpdateStaticEvolutionNodeFrames()
{
    const int currentRank = player_.GetRankFromLevel(player_.level_);
    const float renderScale = GetEvolutionRenderScale();
    auto setLine = [&](cg2::Sprite* sprite, const cg2::Vector2& a, const cg2::Vector2& b, float width, const cg2::Vector4& color) {
        const cg2::Vector2 renderA = EvolutionVirtualToRender(a);
        const cg2::Vector2 renderB = EvolutionVirtualToRender(b);
        const float dx = renderB.x - renderA.x;
        const float dy = renderB.y - renderA.y;
        sprite->SetPosition(renderA);
        sprite->SetRotation(std::atan2(dy, dx));
        sprite->SetSize({std::sqrt(dx * dx + dy * dy), (std::max)(0.5f, width * renderScale)});
        sprite->SetColor(color);
        sprite->Update();
    };

    const int activeNodeCount = static_cast<int>(ui_.staticEvolutionCandidateCount_ + 1);
    for (int nodeIndex = 0; nodeIndex < activeNodeCount; ++nodeIndex) {
        const bool selected = nodeIndex > 0 && nodeIndex - 1 == ui_.evolutionUiStyle_.fixedSelectedCandidate;
        const bool hovered = nodeIndex == ui_.staticEvolutionHoveredNode_;
        const PlayerClassConfig* config =
            nodeIndex > 0 ? player_.GetClassConfig(ui_.staticEvolutionCandidateIds_[static_cast<size_t>(nodeIndex - 1)]) : nullptr;
        const bool locked = config && currentRank < config->requiredRank;
        cg2::Vector4 stateColor = nodeIndex == 0 ? ui_.evolutionUiStyle_.normalColor : ui_.evolutionUiStyle_.availableColor;
        float glowAlpha = nodeIndex == 0 ? 0.035f : 0.050f;
        float middleAlpha = nodeIndex == 0 ? 0.12f : 0.18f;
        float coreAlpha = nodeIndex == 0 ? 0.58f : 0.72f;
        if (locked) {
            stateColor = ui_.evolutionUiStyle_.lockedColor;
            glowAlpha = 0.015f;
            middleAlpha = 0.06f;
            coreAlpha = 0.34f;
        } else if (selected) {
            stateColor = ui_.evolutionUiStyle_.selectedColor;
            glowAlpha = 0.18f;
            middleAlpha = 0.42f;
            coreAlpha = 1.0f;
        } else if (hovered) {
            stateColor = ui_.evolutionUiStyle_.hoverColor;
            glowAlpha = 0.14f;
            middleAlpha = 0.34f;
            coreAlpha = 0.94f;
        }

        const cg2::Vector2 center = ui_.staticEvolutionNodeCentersVirtual_[nodeIndex];
        const cg2::Vector2 size = ui_.staticEvolutionNodeDrawSizesVirtual_[nodeIndex];
        const float halfW = size.x * 0.5f;
        const float halfH = size.y * 0.5f;
        const float cut = (std::clamp)(ui_.evolutionUiStyle_.nodeCornerCut, 0.0f, halfH * 0.60f);
        const std::array<cg2::Vector2, 8> points = {{{center.x - halfW + cut, center.y - halfH},
                                                     {center.x + halfW - cut, center.y - halfH},
                                                     {center.x + halfW, center.y - halfH + cut},
                                                     {center.x + halfW, center.y + halfH - cut},
                                                     {center.x + halfW - cut, center.y + halfH},
                                                     {center.x - halfW + cut, center.y + halfH},
                                                     {center.x - halfW, center.y + halfH - cut},
                                                     {center.x - halfW, center.y - halfH + cut}}};
        for (int segmentIndex = 0; segmentIndex < 8; ++segmentIndex) {
            cg2::Vector4 outer = stateColor;
            cg2::Vector4 middle = stateColor;
            cg2::Vector4 core = stateColor;
            outer.w = glowAlpha;
            middle.w = middleAlpha;
            core.w = coreAlpha;
            const cg2::Vector2 a = points[segmentIndex];
            const cg2::Vector2 b = points[(segmentIndex + 1) % 8];
            const size_t spriteIndex = static_cast<size_t>(segmentIndex * 3);
            setLine(ui_.staticEvolutionNodeFrameSprites_[nodeIndex][spriteIndex].get(), a, b, ui_.evolutionUiStyle_.nodeOutlineGlowWidth,
                    outer);
            setLine(ui_.staticEvolutionNodeFrameSprites_[nodeIndex][spriteIndex + 1].get(), a, b,
                    ui_.evolutionUiStyle_.nodeOutlineWidth * 2.2f, middle);
            setLine(ui_.staticEvolutionNodeFrameSprites_[nodeIndex][spriteIndex + 2].get(), a, b, ui_.evolutionUiStyle_.nodeOutlineWidth,
                    core);
        }
    }
}

void PlayerEvolution::UpdateStaticEvolutionSilhouettes()
{
    /// @brief 機体シルエットを描く1本の線分を表す。
    struct SilhouetteSegment {
        cg2::Vector2 a{};
        cg2::Vector2 b{};
    };
    const int currentRank = player_.GetRankFromLevel(player_.level_);
    const float renderScale = GetEvolutionRenderScale();
    constexpr float kTwoPi = 6.283185307f;

    const int activeNodeCount = static_cast<int>(ui_.staticEvolutionCandidateCount_ + 1);
    for (int nodeIndex = 0; nodeIndex < activeNodeCount; ++nodeIndex) {
        const std::string& nodeClassId =
            nodeIndex == 0 ? player_.currentClassId_ : ui_.staticEvolutionCandidateIds_[static_cast<size_t>(nodeIndex - 1)];
        const PlayerClassConfig* config = player_.GetClassConfig(nodeClassId);
        std::array<SilhouetteSegment, PlayerUiState::kStaticEvolutionSilhouetteSpriteCount> segments{};
        size_t segmentCount = 0;
        auto addSegment = [&](const cg2::Vector2& a, const cg2::Vector2& b) {
            if (segmentCount < segments.size()) {
                segments[segmentCount++] = {a, b};
            }
        };
        if (config) {
            const bool selected = nodeIndex > 0 && nodeIndex - 1 == ui_.evolutionUiStyle_.fixedSelectedCandidate;
            const bool hovered = nodeIndex == ui_.staticEvolutionHoveredNode_;
            const PlayerClassConfig* candidateConfig = nodeIndex > 0 ? config : nullptr;
            const bool locked = candidateConfig && currentRank < candidateConfig->requiredRank;
            float stateScale = ui_.evolutionUiStyle_.normalScale;
            if (selected) {
                stateScale = ui_.evolutionUiStyle_.selectedScale;
            } else if (hovered) {
                stateScale = ui_.evolutionUiStyle_.hoverScale;
            }
            const cg2::Vector2 nodeSize = ui_.staticEvolutionNodeDrawSizesVirtual_[nodeIndex];
            const cg2::Vector2 center = {ui_.staticEvolutionNodeCentersVirtual_[nodeIndex].x - nodeSize.x * 0.30f,
                                         ui_.staticEvolutionNodeCentersVirtual_[nodeIndex].y - 2.0f};
            const float silhouetteScale = ui_.evolutionUiStyle_.silhouetteScale * stateScale;
            const float radius = 18.0f * silhouetteScale;
            int bodySegments = 14;
            float bodyRotation = 0.0f;
            switch (config->bodyShape) {
            case BodyShape::Box:
                bodySegments = 4;
                bodyRotation = kTwoPi * 0.125f;
                break;
            case BodyShape::Triangle:
                bodySegments = 3;
                bodyRotation = -kTwoPi * 0.25f;
                break;
            case BodyShape::Pentagon:
                bodySegments = 5;
                bodyRotation = -kTwoPi * 0.25f;
                break;
            case BodyShape::Circle:
            default:
                break;
            }
            std::array<cg2::Vector2, 14> bodyPoints{};
            for (int i = 0; i < bodySegments; ++i) {
                const float angle = bodyRotation + static_cast<float>(i) * kTwoPi / static_cast<float>(bodySegments);
                bodyPoints[i] = {center.x + std::cos(angle) * radius * (std::clamp)(config->bodyScale.x, 0.45f, 1.80f),
                                 center.y + std::sin(angle) * radius * (std::clamp)(config->bodyScale.y, 0.45f, 1.80f)};
            }
            for (int i = 0; i < bodySegments; ++i) {
                addSegment(bodyPoints[i], bodyPoints[(i + 1) % bodySegments]);
            }

            for (const WeaponMountConfig& mount : config->barrels) {
                const float angle = mount.angleDeg * 3.1415926535f / 180.0f;
                const cg2::Vector2 forward{std::cos(angle), std::sin(angle)};
                const cg2::Vector2 right{-forward.y, forward.x};
                float length = (std::max)(14.0f, mount.scale.x * 15.0f) * silhouetteScale;
                float halfWidth = (std::max)(2.2f, mount.scale.y * 7.0f) * silhouetteScale;
                if (mount.barrelShape == BarrelShape::Heavy) {
                    length *= 1.12f;
                    halfWidth *= 1.45f;
                } else if (mount.barrelShape == BarrelShape::Short) {
                    length *= 0.58f;
                } else if (mount.barrelShape == BarrelShape::Wide) {
                    length *= 0.86f;
                    halfWidth *= 1.80f;
                }
                const cg2::Vector2 mountCenter = {center.x + mount.offset.x * radius * 0.55f + forward.x * length * 0.30f,
                                                  center.y + mount.offset.y * radius * 0.55f + forward.y * length * 0.30f};
                const float baseWidth = mount.barrelShape == BarrelShape::Trapezoid ? halfWidth * 1.28f : halfWidth;
                const float tipWidth = mount.barrelShape == BarrelShape::Trapezoid ? halfWidth * 0.72f : halfWidth;
                const cg2::Vector2 base = {mountCenter.x - forward.x * length * 0.5f, mountCenter.y - forward.y * length * 0.5f};
                const cg2::Vector2 tip = {mountCenter.x + forward.x * length * 0.5f, mountCenter.y + forward.y * length * 0.5f};
                const cg2::Vector2 p0{base.x - right.x * baseWidth, base.y - right.y * baseWidth};
                const cg2::Vector2 p1{tip.x - right.x * tipWidth, tip.y - right.y * tipWidth};
                const cg2::Vector2 p2{tip.x + right.x * tipWidth, tip.y + right.y * tipWidth};
                const cg2::Vector2 p3{base.x + right.x * baseWidth, base.y + right.y * baseWidth};
                addSegment(p0, p1);
                addSegment(p1, p2);
                addSegment(p2, p3);
                addSegment(p3, p0);
            }

            if (config->usesDrone) {
                for (float side : {-1.0f, 1.0f}) {
                    const cg2::Vector2 droneCenter{center.x - 28.0f * silhouetteScale, center.y + side * 18.0f * silhouetteScale};
                    const float droneRadius = 5.0f * silhouetteScale;
                    const cg2::Vector2 top{droneCenter.x, droneCenter.y - droneRadius};
                    const cg2::Vector2 rightPoint{droneCenter.x + droneRadius, droneCenter.y};
                    const cg2::Vector2 bottom{droneCenter.x, droneCenter.y + droneRadius};
                    const cg2::Vector2 leftPoint{droneCenter.x - droneRadius, droneCenter.y};
                    addSegment(top, rightPoint);
                    addSegment(rightPoint, bottom);
                    addSegment(bottom, leftPoint);
                    addSegment(leftPoint, top);
                }
            }

            cg2::Vector4 silhouetteColor = selected ? ui_.evolutionUiStyle_.selectedColor : ui_.evolutionUiStyle_.classTextColor;
            if (hovered && !selected) {
                silhouetteColor = ui_.evolutionUiStyle_.hoverColor;
            }
            silhouetteColor.w = locked ? 0.30f : selected ? 0.94f : 0.72f;
            for (size_t i = 0; i < segmentCount; ++i) {
                const cg2::Vector2 renderA = EvolutionVirtualToRender(segments[i].a);
                const cg2::Vector2 renderB = EvolutionVirtualToRender(segments[i].b);
                const float dx = renderB.x - renderA.x;
                const float dy = renderB.y - renderA.y;
                cg2::Sprite* line = ui_.staticEvolutionSilhouetteSprites_[nodeIndex][i].get();
                line->SetPosition(renderA);
                line->SetRotation(std::atan2(dy, dx));
                line->SetSize({std::sqrt(dx * dx + dy * dy), (std::max)(0.75f, 1.55f * renderScale)});
                line->SetColor(silhouetteColor);
                line->Update();
            }
        }
        while (segmentCount < ui_.staticEvolutionSilhouetteSprites_[nodeIndex].size()) {
            cg2::Sprite* line = ui_.staticEvolutionSilhouetteSprites_[nodeIndex][segmentCount++].get();
            line->SetSize({0.0f, 0.0f});
            line->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
            line->Update();
        }
    }
}

void PlayerEvolution::UpdateStaticEvolutionText()
{
    const float renderScale = GetEvolutionRenderScale();
    const float safe = ui_.evolutionUiStyle_.safeMargin;
    const auto makeStyle = [&](float size, const cg2::Vector4& color, float outlineWidth) {
        cg2::TextStyle style{};
        style.fontFamily = ui_.evolutionUiStyle_.fontFamily;
        style.fontPath = ui_.evolutionUiStyle_.fontPath;
        style.fontWeight = ui_.evolutionUiStyle_.fontWeight;
        style.fontSize = (std::max)(8.0f, size * renderScale);
        style.color = color;
        style.outlineColor = ui_.evolutionUiStyle_.textOutlineColor;
        style.outlineThickness = (std::max)(0.0f, outlineWidth * renderScale);
        style.padding = 6.0f * renderScale;
        return style;
    };

    const cg2::TextStyle titleStyle =
        makeStyle(ui_.evolutionUiStyle_.titleFontSize, ui_.evolutionUiStyle_.titleTextColor, ui_.evolutionUiStyle_.titleOutlineWidth);
    const cg2::TextStyle classNameStyle = makeStyle(ui_.evolutionUiStyle_.classNameFontSize, ui_.evolutionUiStyle_.classTextColor,
                                                    ui_.evolutionUiStyle_.classNameOutlineWidth);
    const cg2::TextStyle bodyStyle =
        makeStyle(ui_.evolutionUiStyle_.bodyFontSize, ui_.evolutionUiStyle_.bodyTextColor, ui_.evolutionUiStyle_.bodyOutlineWidth);
    const cg2::TextStyle buttonStyle =
        makeStyle(ui_.evolutionUiStyle_.buttonFontSize, ui_.evolutionUiStyle_.buttonTextColor, ui_.evolutionUiStyle_.buttonOutlineWidth);
    cg2::SpriteCommon* spriteCommon = cg2::SpriteCommon::GetInstance();

    SetLabel(ui_.staticEvolutionTitleLabel_, spriteCommon,
             "EVOLUTION CIRCUIT // RANK " +
                 std::to_string(player_.GetCurrentClassConfig() ? player_.GetCurrentClassConfig()->requiredRank : 1) + " TO " +
                 std::to_string(player_.GetCurrentClassConfig() ? player_.GetCurrentClassConfig()->requiredRank + 1 : 2),
             EvolutionVirtualToRender({safe, safe * 0.62f}), titleStyle);
    SetLabel(ui_.staticEvolutionPrototypeLabel_, spriteCommon, "INTERACTION DEBUG",
             EvolutionVirtualToRender({ui_.evolutionUiStyle_.virtualResolution.x - safe, safe * 0.72f}), bodyStyle);
    ui_.staticEvolutionPrototypeLabel_->SetAnchorPoint({1.0f, 0.0f});

    const int currentRank = player_.GetRankFromLevel(player_.level_);
    const int activeNodeCount = static_cast<int>(ui_.staticEvolutionCandidateCount_ + 1);
    for (int i = 0; i < activeNodeCount; ++i) {
        const bool selected = i > 0 && i - 1 == ui_.evolutionUiStyle_.fixedSelectedCandidate;
        const bool hovered = i == ui_.staticEvolutionHoveredNode_;
        const std::string& nodeClassId = i == 0 ? player_.currentClassId_ : ui_.staticEvolutionCandidateIds_[static_cast<size_t>(i - 1)];
        const PlayerClassConfig* config = player_.GetClassConfig(nodeClassId);
        const bool locked = i > 0 && config && currentRank < config->requiredRank;
        cg2::Vector4 textColor = ui_.evolutionUiStyle_.classTextColor;
        if (locked) {
            textColor = ui_.evolutionUiStyle_.lockedColor;
            textColor.w = 1.0f;
        } else if (selected) {
            textColor = ui_.evolutionUiStyle_.selectedColor;
        } else if (hovered) {
            textColor = ui_.evolutionUiStyle_.hoverColor;
        }
        cg2::TextStyle nodeClassStyle =
            makeStyle(ui_.evolutionUiStyle_.classNameFontSize, textColor, ui_.evolutionUiStyle_.classNameOutlineWidth);
        cg2::Vector4 roleColor = ui_.evolutionUiStyle_.bodyTextColor;
        roleColor.w = locked ? 0.42f : 0.72f;
        cg2::TextStyle nodeRoleStyle =
            makeStyle(ui_.evolutionUiStyle_.bodyFontSize * 0.78f, roleColor, ui_.evolutionUiStyle_.bodyOutlineWidth);
        const cg2::Vector2 center = ui_.staticEvolutionNodeCentersVirtual_[i];
        const float textX = center.x + 34.0f;
        SetLabel(ui_.staticEvolutionNodeNameLabels_[i], spriteCommon, GetEvolutionClassName(nodeClassId),
                 EvolutionVirtualToRender({textX, center.y - 12.0f}), nodeClassStyle);
        ui_.staticEvolutionNodeNameLabels_[i]->SetAnchorPoint({0.5f, 0.5f});
        SetLabel(ui_.staticEvolutionNodeRankLabels_[i], spriteCommon,
                 i == 0   ? "CURRENT CLASS"
                 : config ? GetEvolutionShortRole(*config)
                          : "UNKNOWN",
                 EvolutionVirtualToRender({textX, center.y + 18.0f}), nodeRoleStyle);
        ui_.staticEvolutionNodeRankLabels_[i]->SetAnchorPoint({0.5f, 0.5f});
    }

    const std::string& selectedClassId =
        ui_.staticEvolutionCandidateIds_[static_cast<size_t>(ui_.evolutionUiStyle_.fixedSelectedCandidate)];
    const PlayerClassConfig* selected = player_.GetClassConfig(selectedClassId);
    const PlayerClassConfig* current = player_.GetCurrentClassConfig();
    if (!selected || !current) {
        return;
    }
    const std::array<std::string, 3> deltas = GetEvolutionDeltas(*current, *selected);
    const cg2::Vector2 panelCenter = EvolutionAnchorToVirtual(ui_.evolutionUiStyle_.detailPanelAnchor);
    const cg2::Vector2 panelTopLeft = {panelCenter.x - ui_.evolutionUiStyle_.detailPanelSize.x * 0.5f,
                                       panelCenter.y - ui_.evolutionUiStyle_.detailPanelSize.y * 0.5f};
    SetLabel(ui_.staticEvolutionDetailClassLabel_, spriteCommon, GetEvolutionClassName(selectedClassId),
             EvolutionVirtualToRender({panelTopLeft.x + 24.0f, panelTopLeft.y + 13.0f}), classNameStyle);
    SetLabel(ui_.staticEvolutionRoleLabel_, spriteCommon, GetEvolutionRole(*selected),
             EvolutionVirtualToRender({panelTopLeft.x + 150.0f, panelTopLeft.y + 18.0f}), bodyStyle);
    for (int i = 0; i < 3; ++i) {
        SetLabel(ui_.staticEvolutionDeltaLabels_[i], spriteCommon, deltas[i],
                 EvolutionVirtualToRender({panelTopLeft.x + 24.0f + static_cast<float>(i) * 276.0f, panelTopLeft.y + 76.0f}), bodyStyle);
    }
    SetLabel(ui_.staticEvolutionAbilityLabel_, spriteCommon, GetEvolutionAbility(*selected),
             EvolutionVirtualToRender({panelTopLeft.x + 400.0f, panelTopLeft.y + 18.0f}), bodyStyle);
    const cg2::Vector2 buttonCenter = {panelCenter.x + ui_.evolutionUiStyle_.detailPanelSize.x * 0.5f -
                                           ui_.evolutionUiStyle_.confirmButtonSize.x * 0.5f - 18.0f,
                                       panelCenter.y - ui_.evolutionUiStyle_.detailPanelSize.y * 0.5f + 32.0f};
    SetLabel(ui_.staticEvolutionConfirmLabel_, spriteCommon, player_.CanEvolveTo(selectedClassId) ? "ENTER  進化決定" : "RANK不足",
             EvolutionVirtualToRender(buttonCenter), buttonStyle);
    ui_.staticEvolutionConfirmLabel_->SetAnchorPoint({0.5f, 0.5f});
    SetLabel(ui_.staticEvolutionPanelHintLabel_, spriteCommon, "ESC  戻る  /  ENTER  決定",
             EvolutionVirtualToRender({panelTopLeft.x + ui_.evolutionUiStyle_.detailPanelSize.x - 278.0f, panelTopLeft.y + 94.0f}),
             bodyStyle);
}

void PlayerEvolution::PrepareStaticEvolutionTextTextures()
{
    auto prepare = [](cg2::TextLabel* label) {
        if (label) {
            label->PrepareForDraw();
        }
    };

    prepare(ui_.staticEvolutionTitleLabel_.get());
    prepare(ui_.staticEvolutionPrototypeLabel_.get());
    prepare(ui_.staticEvolutionDetailClassLabel_.get());
    prepare(ui_.staticEvolutionRoleLabel_.get());
    for (const auto& label : ui_.staticEvolutionDeltaLabels_) {
        prepare(label.get());
    }
    prepare(ui_.staticEvolutionAbilityLabel_.get());
    prepare(ui_.staticEvolutionConfirmLabel_.get());
    prepare(ui_.staticEvolutionPanelHintLabel_.get());

    for (const auto& label : ui_.staticEvolutionNodeNameLabels_) {
        prepare(label.get());
    }
    for (const auto& label : ui_.staticEvolutionNodeRankLabels_) {
        prepare(label.get());
    }
    for (size_t i = 0; i < ui_.staticEvolutionCandidateCount_ + 1; ++i) {
        if (ui_.staticEvolutionTankButtons_[i]) {
            prepare(ui_.staticEvolutionTankButtons_[i]->GetLabel());
        }
    }
}

bool PlayerEvolution::LoadEvolutionUiStyle(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        ui_.evolutionUiStyleStatus_ = "進化UI設定が見つからないため初期値を使用します。";
        return false;
    }

    nlohmann::json root{};
    try {
        file >> root;
    }
    catch (...) {
        ui_.evolutionUiStyleStatus_ = "進化UI設定JSONの読み込みに失敗しました。";
        return false;
    }
    if (!root.is_object()) {
        return false;
    }

    ui_.evolutionUiStyle_.enabled = root.value("enabled", ui_.evolutionUiStyle_.enabled);
    ui_.evolutionUiStyle_.virtualResolution =
        ReadVector2Object(root.value("virtualResolution", nlohmann::json::object()), ui_.evolutionUiStyle_.virtualResolution);
    ui_.evolutionUiStyle_.safeMargin = root.value("safeMargin", ui_.evolutionUiStyle_.safeMargin);
    if (root.contains("layout") && root["layout"].is_object()) {
        const nlohmann::json& layout = root["layout"];
        ui_.evolutionUiStyle_.radialLayout = layout.value("mode", std::string("leftToRight")) == "radial";
        ui_.evolutionUiStyle_.branchPointAnchor =
            ReadVector2Object(layout.value("branchPoint", nlohmann::json::object()), ui_.evolutionUiStyle_.branchPointAnchor);
        if (layout.contains("radialNodes") && layout["radialNodes"].is_object()) {
            const nlohmann::json& radialNodes = layout["radialNodes"];
            const std::array<const char*, 4> keys = {"Basic", "Twin", "MachineGun", "Overseer"};
            for (int i = 0; i < 4; ++i) {
                ui_.evolutionUiStyle_.radialNodeAnchors[i] =
                    ReadVector2Object(radialNodes.value(keys[i], nlohmann::json::object()), ui_.evolutionUiStyle_.radialNodeAnchors[i]);
            }
        }
    }
    if (root.contains("nodes") && root["nodes"].is_object()) {
        const nlohmann::json& nodes = root["nodes"];
        const std::array<const char*, 4> keys = {"Basic", "Twin", "MachineGun", "Overseer"};
        for (int i = 0; i < 4; ++i) {
            ui_.evolutionUiStyle_.nodeAnchors[i] =
                ReadVector2Object(nodes.value(keys[i], nlohmann::json::object()), ui_.evolutionUiStyle_.nodeAnchors[i]);
        }
        ui_.evolutionUiStyle_.currentNodeSize =
            ReadVector2Object(nodes.value("currentSize", nlohmann::json::object()), ui_.evolutionUiStyle_.currentNodeSize);
        ui_.evolutionUiStyle_.candidateNodeSize =
            ReadVector2Object(nodes.value("candidateSize", nlohmann::json::object()), ui_.evolutionUiStyle_.candidateNodeSize);
        ui_.evolutionUiStyle_.nodeCornerCut = nodes.value("cornerCut", ui_.evolutionUiStyle_.nodeCornerCut);
        ui_.evolutionUiStyle_.nodeOutlineGlowWidth = nodes.value("outlineGlowWidth", ui_.evolutionUiStyle_.nodeOutlineGlowWidth);
        ui_.evolutionUiStyle_.nodeOutlineWidth = nodes.value("outlineWidth", ui_.evolutionUiStyle_.nodeOutlineWidth);
        ui_.evolutionUiStyle_.silhouetteScale = nodes.value("silhouetteScale", ui_.evolutionUiStyle_.silhouetteScale);
        if (nodes.contains("scale") && nodes["scale"].is_object()) {
            const nlohmann::json& scale = nodes["scale"];
            ui_.evolutionUiStyle_.normalScale = scale.value("normal", ui_.evolutionUiStyle_.normalScale);
            ui_.evolutionUiStyle_.hoverScale = scale.value("hover", ui_.evolutionUiStyle_.hoverScale);
            ui_.evolutionUiStyle_.selectedScale = scale.value("selected", ui_.evolutionUiStyle_.selectedScale);
        }
    }
    if (root.contains("circuit") && root["circuit"].is_object()) {
        const nlohmann::json& circuit = root["circuit"];
        ui_.evolutionUiStyle_.circuitOuterGlowWidth = circuit.value("outerGlowWidth", ui_.evolutionUiStyle_.circuitOuterGlowWidth);
        ui_.evolutionUiStyle_.circuitMiddleGlowWidth = circuit.value("middleGlowWidth", ui_.evolutionUiStyle_.circuitMiddleGlowWidth);
        ui_.evolutionUiStyle_.circuitCoreWidth = circuit.value("coreWidth", ui_.evolutionUiStyle_.circuitCoreWidth);
        ui_.evolutionUiStyle_.circuitOpacity = circuit.value("opacity", ui_.evolutionUiStyle_.circuitOpacity);
        ui_.evolutionUiStyle_.circuitOuterAlpha = circuit.value("outerAlpha", ui_.evolutionUiStyle_.circuitOuterAlpha);
        ui_.evolutionUiStyle_.circuitMiddleAlpha = circuit.value("middleAlpha", ui_.evolutionUiStyle_.circuitMiddleAlpha);
        ui_.evolutionUiStyle_.circuitCoreAlpha = circuit.value("coreAlpha", ui_.evolutionUiStyle_.circuitCoreAlpha);
    }
    ui_.evolutionUiStyle_.backgroundDimOpacity = root.value("backgroundDimOpacity", ui_.evolutionUiStyle_.backgroundDimOpacity);
    if (root.contains("detailPanel") && root["detailPanel"].is_object()) {
        const nlohmann::json& panel = root["detailPanel"];
        ui_.evolutionUiStyle_.detailPanelAnchor =
            ReadVector2Object(panel.value("anchor", nlohmann::json::object()), ui_.evolutionUiStyle_.detailPanelAnchor);
        ui_.evolutionUiStyle_.detailPanelSize =
            ReadVector2Object(panel.value("size", nlohmann::json::object()), ui_.evolutionUiStyle_.detailPanelSize);
    }
    if (root.contains("confirmButton") && root["confirmButton"].is_object()) {
        ui_.evolutionUiStyle_.confirmButtonSize =
            ReadVector2Object(root["confirmButton"].value("size", nlohmann::json::object()), ui_.evolutionUiStyle_.confirmButtonSize);
    }
    if (root.contains("text") && root["text"].is_object()) {
        const nlohmann::json& text = root["text"];
        ui_.evolutionUiStyle_.fontFamily = text.value("fontFamily", ui_.evolutionUiStyle_.fontFamily);
        ui_.evolutionUiStyle_.fontPath = text.value("fontPath", ui_.evolutionUiStyle_.fontPath);
        ui_.evolutionUiStyle_.fontWeight = text.value("fontWeight", ui_.evolutionUiStyle_.fontWeight);
        ui_.evolutionUiStyle_.titleFontSize = text.value("titleSize", ui_.evolutionUiStyle_.titleFontSize);
        ui_.evolutionUiStyle_.classNameFontSize = text.value("classNameSize", ui_.evolutionUiStyle_.classNameFontSize);
        ui_.evolutionUiStyle_.bodyFontSize = text.value("bodySize", ui_.evolutionUiStyle_.bodyFontSize);
        ui_.evolutionUiStyle_.buttonFontSize = text.value("buttonSize", ui_.evolutionUiStyle_.buttonFontSize);
        ui_.evolutionUiStyle_.titleTextColor =
            ReadVector4(text.value("titleColor", nlohmann::json::array()), ui_.evolutionUiStyle_.titleTextColor);
        ui_.evolutionUiStyle_.classTextColor =
            ReadVector4(text.value("classNameColor", nlohmann::json::array()), ui_.evolutionUiStyle_.classTextColor);
        ui_.evolutionUiStyle_.bodyTextColor =
            ReadVector4(text.value("bodyColor", nlohmann::json::array()), ui_.evolutionUiStyle_.bodyTextColor);
        ui_.evolutionUiStyle_.buttonTextColor =
            ReadVector4(text.value("buttonColor", nlohmann::json::array()), ui_.evolutionUiStyle_.buttonTextColor);
        ui_.evolutionUiStyle_.textOutlineColor =
            ReadVector4(text.value("outlineColor", nlohmann::json::array()), ui_.evolutionUiStyle_.textOutlineColor);
        const float legacyOutline = text.value("outlineWidth", ui_.evolutionUiStyle_.classNameOutlineWidth);
        ui_.evolutionUiStyle_.titleOutlineWidth = text.value("titleOutlineWidth", legacyOutline);
        ui_.evolutionUiStyle_.classNameOutlineWidth = text.value("classNameOutlineWidth", legacyOutline);
        ui_.evolutionUiStyle_.bodyOutlineWidth = text.value("bodyOutlineWidth", ui_.evolutionUiStyle_.bodyOutlineWidth);
        ui_.evolutionUiStyle_.buttonOutlineWidth = text.value("buttonOutlineWidth", ui_.evolutionUiStyle_.buttonOutlineWidth);
    }
    if (root.contains("neonText") && root["neonText"].is_object()) {
        const nlohmann::json& neon = root["neonText"];
        ui_.evolutionUiStyle_.neonText.enabled = neon.value("enabled", ui_.evolutionUiStyle_.neonText.enabled);
        ui_.evolutionUiStyle_.neonText.glowColor =
            ReadVector4(neon.value("glowColor", nlohmann::json::array()), ui_.evolutionUiStyle_.neonText.glowColor);
        ui_.evolutionUiStyle_.neonText.sourceBrightness = neon.value("sourceBrightness", ui_.evolutionUiStyle_.neonText.sourceBrightness);
        ui_.evolutionUiStyle_.neonText.threshold = neon.value("threshold", ui_.evolutionUiStyle_.neonText.threshold);
        ui_.evolutionUiStyle_.neonText.innerIntensity = neon.value("innerIntensity", ui_.evolutionUiStyle_.neonText.innerIntensity);
        ui_.evolutionUiStyle_.neonText.outerIntensity = neon.value("outerIntensity", ui_.evolutionUiStyle_.neonText.outerIntensity);
    }
    if (root.contains("colors") && root["colors"].is_object()) {
        const nlohmann::json& colors = root["colors"];
        ui_.evolutionUiStyle_.normalColor = ReadVector4(colors.value("normal", nlohmann::json::array()), ui_.evolutionUiStyle_.normalColor);
        ui_.evolutionUiStyle_.availableColor =
            ReadVector4(colors.value("available", nlohmann::json::array()), ui_.evolutionUiStyle_.availableColor);
        ui_.evolutionUiStyle_.hoverColor = ReadVector4(colors.value("hover", nlohmann::json::array()), ui_.evolutionUiStyle_.hoverColor);
        ui_.evolutionUiStyle_.selectedColor =
            ReadVector4(colors.value("selected", nlohmann::json::array()), ui_.evolutionUiStyle_.selectedColor);
        ui_.evolutionUiStyle_.lockedColor = ReadVector4(colors.value("locked", nlohmann::json::array()), ui_.evolutionUiStyle_.lockedColor);
        ui_.evolutionUiStyle_.panelColor = ReadVector4(colors.value("panel", nlohmann::json::array()), ui_.evolutionUiStyle_.panelColor);
    }
    ui_.evolutionUiStyle_.fixedSelectedCandidate =
        (std::clamp)(root.value("fixedSelectedCandidate", ui_.evolutionUiStyle_.fixedSelectedCandidate), 0,
                     static_cast<int>(ui_.kStaticEvolutionMaxCandidates - 1));
    ui_.evolutionUiStyleStatus_ = "進化UI設定を読み込みました: " + path;
    return true;
}

bool PlayerEvolution::SaveEvolutionUiStyle(const std::string& path) const
{
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    nlohmann::json root{};
    root["version"] = 2;
    root["enabled"] = ui_.evolutionUiStyle_.enabled;
    root["virtualResolution"] = WriteVector2Object(ui_.evolutionUiStyle_.virtualResolution);
    root["safeMargin"] = ui_.evolutionUiStyle_.safeMargin;
    root["layout"] = {{"mode", ui_.evolutionUiStyle_.radialLayout ? "radial" : "leftToRight"},
                      {"branchPoint", WriteVector2Object(ui_.evolutionUiStyle_.branchPointAnchor)},
                      {"radialNodes",
                       {{"Basic", WriteVector2Object(ui_.evolutionUiStyle_.radialNodeAnchors[0])},
                        {"Twin", WriteVector2Object(ui_.evolutionUiStyle_.radialNodeAnchors[1])},
                        {"MachineGun", WriteVector2Object(ui_.evolutionUiStyle_.radialNodeAnchors[2])},
                        {"Overseer", WriteVector2Object(ui_.evolutionUiStyle_.radialNodeAnchors[3])}}}};
    root["nodes"] = {{"Basic", WriteVector2Object(ui_.evolutionUiStyle_.nodeAnchors[0])},
                     {"Twin", WriteVector2Object(ui_.evolutionUiStyle_.nodeAnchors[1])},
                     {"MachineGun", WriteVector2Object(ui_.evolutionUiStyle_.nodeAnchors[2])},
                     {"Overseer", WriteVector2Object(ui_.evolutionUiStyle_.nodeAnchors[3])},
                     {"currentSize", WriteVector2Object(ui_.evolutionUiStyle_.currentNodeSize)},
                     {"candidateSize", WriteVector2Object(ui_.evolutionUiStyle_.candidateNodeSize)},
                     {"cornerCut", ui_.evolutionUiStyle_.nodeCornerCut},
                     {"outlineGlowWidth", ui_.evolutionUiStyle_.nodeOutlineGlowWidth},
                     {"outlineWidth", ui_.evolutionUiStyle_.nodeOutlineWidth},
                     {"silhouetteScale", ui_.evolutionUiStyle_.silhouetteScale},
                     {"scale",
                      {{"normal", ui_.evolutionUiStyle_.normalScale},
                       {"hover", ui_.evolutionUiStyle_.hoverScale},
                       {"selected", ui_.evolutionUiStyle_.selectedScale}}}};
    root["circuit"] = {{"outerGlowWidth", ui_.evolutionUiStyle_.circuitOuterGlowWidth},
                       {"middleGlowWidth", ui_.evolutionUiStyle_.circuitMiddleGlowWidth},
                       {"coreWidth", ui_.evolutionUiStyle_.circuitCoreWidth},
                       {"opacity", ui_.evolutionUiStyle_.circuitOpacity},
                       {"outerAlpha", ui_.evolutionUiStyle_.circuitOuterAlpha},
                       {"middleAlpha", ui_.evolutionUiStyle_.circuitMiddleAlpha},
                       {"coreAlpha", ui_.evolutionUiStyle_.circuitCoreAlpha}};
    root["backgroundDimOpacity"] = ui_.evolutionUiStyle_.backgroundDimOpacity;
    root["detailPanel"] = {{"anchor", WriteVector2Object(ui_.evolutionUiStyle_.detailPanelAnchor)},
                           {"size", WriteVector2Object(ui_.evolutionUiStyle_.detailPanelSize)}};
    root["confirmButton"] = {{"size", WriteVector2Object(ui_.evolutionUiStyle_.confirmButtonSize)}};
    root["text"] = {{"fontFamily", ui_.evolutionUiStyle_.fontFamily},
                    {"fontPath", ui_.evolutionUiStyle_.fontPath},
                    {"fontWeight", ui_.evolutionUiStyle_.fontWeight},
                    {"titleSize", ui_.evolutionUiStyle_.titleFontSize},
                    {"classNameSize", ui_.evolutionUiStyle_.classNameFontSize},
                    {"bodySize", ui_.evolutionUiStyle_.bodyFontSize},
                    {"buttonSize", ui_.evolutionUiStyle_.buttonFontSize},
                    {"titleColor", Vector4ToJson(ui_.evolutionUiStyle_.titleTextColor)},
                    {"classNameColor", Vector4ToJson(ui_.evolutionUiStyle_.classTextColor)},
                    {"bodyColor", Vector4ToJson(ui_.evolutionUiStyle_.bodyTextColor)},
                    {"buttonColor", Vector4ToJson(ui_.evolutionUiStyle_.buttonTextColor)},
                    {"outlineColor", Vector4ToJson(ui_.evolutionUiStyle_.textOutlineColor)},
                    {"titleOutlineWidth", ui_.evolutionUiStyle_.titleOutlineWidth},
                    {"classNameOutlineWidth", ui_.evolutionUiStyle_.classNameOutlineWidth},
                    {"bodyOutlineWidth", ui_.evolutionUiStyle_.bodyOutlineWidth},
                    {"buttonOutlineWidth", ui_.evolutionUiStyle_.buttonOutlineWidth}};
    root["neonText"] = {{"enabled", ui_.evolutionUiStyle_.neonText.enabled},
                        {"glowColor", Vector4ToJson(ui_.evolutionUiStyle_.neonText.glowColor)},
                        {"sourceBrightness", ui_.evolutionUiStyle_.neonText.sourceBrightness},
                        {"threshold", ui_.evolutionUiStyle_.neonText.threshold},
                        {"innerIntensity", ui_.evolutionUiStyle_.neonText.innerIntensity},
                        {"outerIntensity", ui_.evolutionUiStyle_.neonText.outerIntensity}};
    root["colors"] = {
        {"normal", Vector4ToJson(ui_.evolutionUiStyle_.normalColor)}, {"available", Vector4ToJson(ui_.evolutionUiStyle_.availableColor)},
        {"hover", Vector4ToJson(ui_.evolutionUiStyle_.hoverColor)},   {"selected", Vector4ToJson(ui_.evolutionUiStyle_.selectedColor)},
        {"locked", Vector4ToJson(ui_.evolutionUiStyle_.lockedColor)}, {"panel", Vector4ToJson(ui_.evolutionUiStyle_.panelColor)}};
    root["fixedSelectedCandidate"] = ui_.evolutionUiStyle_.fixedSelectedCandidate;

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << root.dump(2);
    return true;
}

void PlayerEvolution::DrawEvolutionUiStyleEditor()
{
    if (!ui_.arenaUiEnabled_)
        return;
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("進化UI", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    ImGui::Checkbox("新しい進化UIを有効化", &ui_.evolutionUiStyle_.enabled);
    RefreshStaticEvolutionCandidates();
    if (ui_.staticEvolutionCandidateCount_ > 0) {
        ImGui::SliderInt("選択中の候補", &ui_.evolutionUiStyle_.fixedSelectedCandidate, 0,
                         static_cast<int>(ui_.staticEvolutionCandidateCount_ - 1));
        ImGui::SameLine();
        ImGui::TextDisabled(
            "%s", GetEvolutionClassName(ui_.staticEvolutionCandidateIds_[static_cast<size_t>(ui_.evolutionUiStyle_.fixedSelectedCandidate)])
                      .c_str());
    }
    if (ImGui::Button("進化UI設定を保存")) {
        ui_.evolutionUiStyleStatus_ = SaveEvolutionUiStyle() ? "進化UI設定を保存しました。" : "進化UI設定の保存に失敗しました。";
    }
    ImGui::SameLine();
    if (ImGui::Button("進化UI設定を再読み込み")) {
        LoadEvolutionUiStyle();
    }
    if (!ui_.evolutionUiStyleStatus_.empty()) {
        ImGui::TextWrapped("%s", ui_.evolutionUiStyleStatus_.c_str());
    }

    const float clientWidth = static_cast<float>(cg2::WinApp::GetInstance()->GetClientWidth());
    const float clientHeight = static_cast<float>(cg2::WinApp::GetInstance()->GetClientHeight());
    const float actualScale = (std::min)(clientWidth / (std::max)(1.0f, ui_.evolutionUiStyle_.virtualResolution.x),
                                         clientHeight / (std::max)(1.0f, ui_.evolutionUiStyle_.virtualResolution.y));
    ImGui::Text("実解像度: %.0f x %.0f / UI倍率: %.3f", clientWidth, clientHeight, actualScale);

    ImGui::SeparatorText("仮想画面と配置");
    ImGui::DragFloat2("仮想解像度", &ui_.evolutionUiStyle_.virtualResolution.x, 1.0f, 320.0f, 7680.0f);
    ImGui::DragFloat("セーフマージン", &ui_.evolutionUiStyle_.safeMargin, 1.0f, 0.0f, 360.0f);
    int layoutMode = ui_.evolutionUiStyle_.radialLayout ? 1 : 0;
    const char* layoutNames[] = {"左から右へ分岐", "放射型"};
    if (ImGui::Combo("配置モード", &layoutMode, layoutNames, IM_ARRAYSIZE(layoutNames))) {
        ui_.evolutionUiStyle_.radialLayout = layoutMode == 1;
    }
    const char* nodeNames[] = {"Basic", "Twin", "MachineGun", "Overseer"};
    for (int i = 0; i < 4; ++i) {
        ImGui::PushID(i);
        ImGui::DragFloat2(ui_.evolutionUiStyle_.radialLayout ? "放射型アンカー" : nodeNames[i],
                          ui_.evolutionUiStyle_.radialLayout ? &ui_.evolutionUiStyle_.radialNodeAnchors[i].x
                                                             : &ui_.evolutionUiStyle_.nodeAnchors[i].x,
                          0.005f, 0.0f, 1.0f);
        ImGui::PopID();
    }
    if (!ui_.evolutionUiStyle_.radialLayout) {
        ImGui::DragFloat2("分岐点", &ui_.evolutionUiStyle_.branchPointAnchor.x, 0.005f, 0.0f, 1.0f);
    }
    ImGui::DragFloat2("現在ノードサイズ", &ui_.evolutionUiStyle_.currentNodeSize.x, 1.0f, 32.0f, 600.0f);
    ImGui::DragFloat2("候補ノードサイズ", &ui_.evolutionUiStyle_.candidateNodeSize.x, 1.0f, 32.0f, 600.0f);
    ImGui::DragFloat("通常拡大率", &ui_.evolutionUiStyle_.normalScale, 0.005f, 0.5f, 2.0f);
    ImGui::DragFloat("ホバー拡大率", &ui_.evolutionUiStyle_.hoverScale, 0.005f, 0.5f, 2.0f);
    ImGui::DragFloat("選択拡大率", &ui_.evolutionUiStyle_.selectedScale, 0.005f, 0.5f, 2.0f);
    ImGui::DragFloat("角落とし", &ui_.evolutionUiStyle_.nodeCornerCut, 0.25f, 0.0f, 48.0f);
    ImGui::DragFloat("ノード外光幅", &ui_.evolutionUiStyle_.nodeOutlineGlowWidth, 0.25f, 0.5f, 40.0f);
    ImGui::DragFloat("ノード輪郭幅", &ui_.evolutionUiStyle_.nodeOutlineWidth, 0.1f, 0.5f, 12.0f);
    ImGui::DragFloat("戦車シルエット倍率", &ui_.evolutionUiStyle_.silhouetteScale, 0.01f, 0.5f, 2.0f);
    ImGui::DragFloat2("説明パネル位置（正規化）", &ui_.evolutionUiStyle_.detailPanelAnchor.x, 0.005f, 0.0f, 1.0f);
    ImGui::DragFloat2("説明パネルサイズ", &ui_.evolutionUiStyle_.detailPanelSize.x, 1.0f, 64.0f, 2000.0f);
    ImGui::DragFloat2("決定ボタンサイズ", &ui_.evolutionUiStyle_.confirmButtonSize.x, 1.0f, 32.0f, 600.0f);

    ImGui::SeparatorText("ネオン回路");
    ImGui::DragFloat("外光幅", &ui_.evolutionUiStyle_.circuitOuterGlowWidth, 0.25f, 0.5f, 80.0f);
    ImGui::DragFloat("中光幅", &ui_.evolutionUiStyle_.circuitMiddleGlowWidth, 0.25f, 0.5f, 60.0f);
    ImGui::DragFloat("中心線幅", &ui_.evolutionUiStyle_.circuitCoreWidth, 0.1f, 0.25f, 24.0f);
    ImGui::SliderFloat("回路全体透明度", &ui_.evolutionUiStyle_.circuitOpacity, 0.0f, 1.0f);
    ImGui::SliderFloat("外光透明度", &ui_.evolutionUiStyle_.circuitOuterAlpha, 0.0f, 1.0f);
    ImGui::SliderFloat("中光透明度", &ui_.evolutionUiStyle_.circuitMiddleAlpha, 0.0f, 1.0f);
    ImGui::SliderFloat("中心線透明度", &ui_.evolutionUiStyle_.circuitCoreAlpha, 0.0f, 1.0f);
    ImGui::SliderFloat("背景暗転率", &ui_.evolutionUiStyle_.backgroundDimOpacity, 0.0f, 1.0f);

    ImGui::SeparatorText("文字");
    ImGui::Text("Font: %s / weight %d", ui_.evolutionUiStyle_.fontFamily.c_str(), ui_.evolutionUiStyle_.fontWeight);
    ImGui::TextDisabled("%s", ui_.evolutionUiStyle_.fontPath.c_str());
    ImGui::DragFloat("タイトル文字サイズ", &ui_.evolutionUiStyle_.titleFontSize, 0.5f, 8.0f, 96.0f);
    ImGui::DragFloat("クラス名文字サイズ", &ui_.evolutionUiStyle_.classNameFontSize, 0.5f, 8.0f, 72.0f);
    ImGui::DragFloat("本文文字サイズ", &ui_.evolutionUiStyle_.bodyFontSize, 0.5f, 8.0f, 48.0f);
    ImGui::DragFloat("ボタン文字サイズ", &ui_.evolutionUiStyle_.buttonFontSize, 0.5f, 8.0f, 48.0f);
    ImGui::ColorEdit4("タイトル文字色", &ui_.evolutionUiStyle_.titleTextColor.x);
    ImGui::ColorEdit4("クラス名文字色", &ui_.evolutionUiStyle_.classTextColor.x);
    ImGui::ColorEdit4("本文文字色", &ui_.evolutionUiStyle_.bodyTextColor.x);
    ImGui::ColorEdit4("ボタン文字色", &ui_.evolutionUiStyle_.buttonTextColor.x);
    ImGui::ColorEdit4("文字アウトライン色", &ui_.evolutionUiStyle_.textOutlineColor.x);
    ImGui::DragFloat("タイトルアウトライン幅", &ui_.evolutionUiStyle_.titleOutlineWidth, 0.05f, 0.0f, 4.0f);
    ImGui::DragFloat("クラス名アウトライン幅", &ui_.evolutionUiStyle_.classNameOutlineWidth, 0.05f, 0.0f, 4.0f);
    ImGui::DragFloat("本文アウトライン幅", &ui_.evolutionUiStyle_.bodyOutlineWidth, 0.05f, 0.0f, 4.0f);
    ImGui::DragFloat("ボタンアウトライン幅", &ui_.evolutionUiStyle_.buttonOutlineWidth, 0.05f, 0.0f, 4.0f);
    ImGui::SeparatorText("文字ネオン");
    ImGui::Checkbox("文字ネオンを有効化", &ui_.evolutionUiStyle_.neonText.enabled);
    ImGui::ColorEdit4("文字発光色", &ui_.evolutionUiStyle_.neonText.glowColor.x);
    ImGui::DragFloat("発光源輝度", &ui_.evolutionUiStyle_.neonText.sourceBrightness, 0.02f, 0.0f, 8.0f);
    ImGui::DragFloat("抽出しきい値", &ui_.evolutionUiStyle_.neonText.threshold, 0.01f, 0.0f, 4.0f);
    ImGui::DragFloat("内光強度", &ui_.evolutionUiStyle_.neonText.innerIntensity, 0.01f, 0.0f, 4.0f);
    ImGui::DragFloat("外光強度", &ui_.evolutionUiStyle_.neonText.outerIntensity, 0.01f, 0.0f, 4.0f);

    ImGui::SeparatorText("状態色");
    ImGui::ColorEdit4("通常色", &ui_.evolutionUiStyle_.normalColor.x);
    ImGui::ColorEdit4("選択可能色", &ui_.evolutionUiStyle_.availableColor.x);
    ImGui::ColorEdit4("ホバー色", &ui_.evolutionUiStyle_.hoverColor.x);
    ImGui::ColorEdit4("選択色", &ui_.evolutionUiStyle_.selectedColor.x);
    ImGui::ColorEdit4("ロック色", &ui_.evolutionUiStyle_.lockedColor.x);
    ImGui::ColorEdit4("パネル色", &ui_.evolutionUiStyle_.panelColor.x);

    ImGui::SeparatorText("デバッグ表示");
    ImGui::Checkbox("仮想画面領域", &ui_.showEvolutionVirtualBounds_);
    ImGui::Checkbox("セーフエリア", &ui_.showEvolutionSafeArea_);
    ImGui::Checkbox("ノード描画領域", &ui_.showEvolutionNodeBounds_);
    ImGui::Checkbox("マウス判定領域", &ui_.showEvolutionMouseBounds_);
    ImGui::Checkbox("TextLabel領域", &ui_.showEvolutionTextBounds_);
    ImGui::Checkbox("画面中央線", &ui_.showEvolutionCenterLines_);
    ImGui::Checkbox("接続回路の制御点", &ui_.showEvolutionCircuitControlPoints_);
    ImGui::Checkbox("実解像度とUI倍率", &ui_.showEvolutionResolutionInfo_);
#endif
}

void PlayerEvolution::DrawEvolutionCircuitPrototype()
{
    ui_.evolutionUiProfile_.visible = true;
    cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
    for (const auto& line : ui_.evolutionCircuitLineSprites_) {
        if (line && line->GetSize().x > 0.0f && line->GetSize().y > 0.0f && line->GetColor().w > 0.001f) {
            line->Draw();
            ++ui_.evolutionUiProfile_.spriteDraws;
        }
    }
    if (ui_.evolutionCircuitDetailPanelSprite_) {
        ui_.evolutionCircuitDetailPanelSprite_->Draw();
        ++ui_.evolutionUiProfile_.spriteDraws;
    }
    for (size_t i = 0; i < ui_.evolutionCircuitNodes_.size(); ++i) {
        if (player_.IsEvolutionClassVisible(ui_.evolutionCircuitNodes_[i].classId) && ui_.evolutionCircuitTankButtons_[i]) {
            ui_.evolutionCircuitTankButtons_[i]->Draw();
        }
    }
    if (ui_.evolutionCircuitDetailPreview_)
        ui_.evolutionCircuitDetailPreview_->Draw();

    cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
    auto drawLabel = [&](const std::unique_ptr<cg2::TextLabel>& label) {
        if (label) {
            label->Draw();
            ++ui_.evolutionUiProfile_.textDraws;
        }
    };
    drawLabel(ui_.evolutionCircuitTitleLabel_);
    for (const auto& label : ui_.evolutionCircuitRankLabels_)
        drawLabel(label);
    drawLabel(ui_.evolutionCircuitDetailNameLabel_);
    drawLabel(ui_.evolutionCircuitDetailMetaLabel_);
    drawLabel(ui_.evolutionCircuitDetailRoleLabel_);
    for (const auto& label : ui_.evolutionCircuitDetailStatLabels_)
        drawLabel(label);
    drawLabel(ui_.evolutionCircuitHintLabel_);
}

void PlayerEvolution::DrawEvolutionCircuitAfterPostEffects()
{
    cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
    if (ui_.evolutionCircuitBackdropSprite_)
        ui_.evolutionCircuitBackdropSprite_->Draw();
    if (ui_.staticEvolutionButtonBloomEffect_) {
        ui_.staticEvolutionButtonBloomEffect_->BeginCapture();
        cg2::SpriteCommon::GetInstance()->PreDrawForScene(cg2::kNormal);
        for (const auto& line : ui_.evolutionCircuitLineSprites_) {
            if (line && line->GetSize().x > 0.0f && line->GetSize().y > 0.0f && line->GetColor().w > 0.001f) {
                line->Draw();
            }
        }
        for (size_t i = 0; i < ui_.evolutionCircuitNodes_.size(); ++i) {
            if (player_.IsEvolutionClassVisible(ui_.evolutionCircuitNodes_[i].classId) && ui_.evolutionCircuitTankButtons_[i]) {
                ui_.evolutionCircuitTankButtons_[i]->DrawBloomSource();
            }
        }
        if (ui_.evolutionCircuitDetailPreview_)
            ui_.evolutionCircuitDetailPreview_->DrawBloomSource();
        ui_.staticEvolutionButtonBloomEffect_->EndCaptureBloomOnlyToBackBuffer();
    }
    if (ui_.staticEvolutionTextEffect_) {
        std::vector<cg2::TextLabel*> neonLabels;
        neonLabels.reserve(8);
        if (ui_.evolutionCircuitTitleLabel_)
            neonLabels.push_back(ui_.evolutionCircuitTitleLabel_.get());
        for (const auto& rankLabel : ui_.evolutionCircuitRankLabels_) {
            if (rankLabel)
                neonLabels.push_back(rankLabel.get());
        }
        if (ui_.evolutionCircuitDetailNameLabel_)
            neonLabels.push_back(ui_.evolutionCircuitDetailNameLabel_.get());
        if (ui_.evolutionCircuitSelectedNode_ >= 0 &&
            static_cast<size_t>(ui_.evolutionCircuitSelectedNode_) < ui_.evolutionCircuitTankButtons_.size() &&
            ui_.evolutionCircuitTankButtons_[static_cast<size_t>(ui_.evolutionCircuitSelectedNode_)]) {
            neonLabels.push_back(ui_.evolutionCircuitTankButtons_[static_cast<size_t>(ui_.evolutionCircuitSelectedNode_)]->GetLabel());
        }
        ui_.staticEvolutionTextEffect_->DrawBloom(neonLabels);
    }
}

void PlayerEvolution::DrawStaticEvolutionPrototype()
{
    ui_.evolutionUiProfile_.visible = true;
    const auto totalStart = std::chrono::steady_clock::now();
    const auto spriteStart = totalStart;
    cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
    auto drawSprite = [&](const std::unique_ptr<cg2::Sprite>& sprite) {
        if (sprite && sprite->GetColor().w > 0.001f && sprite->GetSize().x > 0.0f && sprite->GetSize().y > 0.0f) {
            sprite->Draw();
            ++ui_.evolutionUiProfile_.spriteDraws;
        }
    };

    for (const auto& line : ui_.staticEvolutionCircuitSprites_) {
        drawSprite(line);
    }
    drawSprite(ui_.staticEvolutionBranchGlowSprite_);
    drawSprite(ui_.staticEvolutionBranchCoreSprite_);
    drawSprite(ui_.staticEvolutionDetailPanelSprite_);
    for (size_t i = 0; i < ui_.staticEvolutionCandidateCount_ + 1; ++i) {
        if (ui_.staticEvolutionTankButtons_[i]) {
            ui_.staticEvolutionTankButtons_[i]->Draw();
        }
    }
    drawSprite(ui_.staticEvolutionConfirmButtonSprite_);
    for (const auto& line : ui_.staticEvolutionConfirmOutlineSprites_) {
        drawSprite(line);
    }
    const auto spriteEnd = std::chrono::steady_clock::now();

    const auto textStart = spriteEnd;
    cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
    auto drawLabel = [&](const std::unique_ptr<cg2::TextLabel>& label) {
        if (label) {
            label->Draw();
            ++ui_.evolutionUiProfile_.textDraws;
        }
    };
    drawLabel(ui_.staticEvolutionTitleLabel_);
    const bool anyDebugOverlay = ui_.showEvolutionVirtualBounds_ || ui_.showEvolutionSafeArea_ || ui_.showEvolutionNodeBounds_ ||
                                 ui_.showEvolutionMouseBounds_ || ui_.showEvolutionTextBounds_ || ui_.showEvolutionCenterLines_ ||
                                 ui_.showEvolutionCircuitControlPoints_ || ui_.showEvolutionResolutionInfo_;
    if (anyDebugOverlay) {
        drawLabel(ui_.staticEvolutionPrototypeLabel_);
    }
    drawLabel(ui_.staticEvolutionDetailClassLabel_);
    drawLabel(ui_.staticEvolutionRoleLabel_);
    for (const auto& label : ui_.staticEvolutionDeltaLabels_) {
        drawLabel(label);
    }
    drawLabel(ui_.staticEvolutionAbilityLabel_);
    drawLabel(ui_.staticEvolutionConfirmLabel_);
    drawLabel(ui_.staticEvolutionPanelHintLabel_);
    const auto textEnd = std::chrono::steady_clock::now();

    DrawStaticEvolutionDebugOverlay();
    const auto totalEnd = std::chrono::steady_clock::now();
    ui_.evolutionUiProfile_.spriteMs = std::chrono::duration<float, std::milli>(spriteEnd - spriteStart).count();
    ui_.evolutionUiProfile_.textMs = std::chrono::duration<float, std::milli>(textEnd - textStart).count();
    ui_.evolutionUiProfile_.updateMs = 0.0f;
    ui_.evolutionUiProfile_.totalMs = std::chrono::duration<float, std::milli>(totalEnd - totalStart).count();
}

void PlayerEvolution::DrawEvolutionAfterPostEffects()
{
    if (!ui_.arenaUiEnabled_ || !player_.isChangeMode) {
        return;
    }
    if (ShouldUseEvolutionCircuitPrototype()) {
        DrawEvolutionCircuitAfterPostEffects();
        return;
    }
    if (!ShouldUseStaticEvolutionPrototype())
        return;
    cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
    if (ui_.staticEvolutionBackdropSprite_) {
        ui_.staticEvolutionBackdropSprite_->Draw();
    }
    if (ui_.staticEvolutionButtonBloomEffect_) {
        ui_.staticEvolutionButtonBloomEffect_->BeginCapture();
        cg2::SpriteCommon::GetInstance()->PreDrawForScene(cg2::kNormal);
        for (const auto& line : ui_.staticEvolutionCircuitSprites_) {
            if (line && line->GetSize().x > 0.0f && line->GetSize().y > 0.0f && line->GetColor().w > 0.001f) {
                line->Draw();
            }
        }
        if (ui_.staticEvolutionBranchGlowSprite_ && ui_.staticEvolutionBranchGlowSprite_->GetSize().x > 0.0f &&
            ui_.staticEvolutionBranchGlowSprite_->GetColor().w > 0.001f) {
            ui_.staticEvolutionBranchGlowSprite_->Draw();
        }
        if (ui_.staticEvolutionBranchCoreSprite_ && ui_.staticEvolutionBranchCoreSprite_->GetSize().x > 0.0f &&
            ui_.staticEvolutionBranchCoreSprite_->GetColor().w > 0.001f) {
            ui_.staticEvolutionBranchCoreSprite_->Draw();
        }
        for (size_t i = 0; i < ui_.staticEvolutionCandidateCount_ + 1; ++i) {
            if (ui_.staticEvolutionTankButtons_[i]) {
                ui_.staticEvolutionTankButtons_[i]->DrawBloomSource();
            }
        }
        ui_.staticEvolutionButtonBloomEffect_->EndCaptureBloomOnlyToBackBuffer();
    }
    if (ui_.staticEvolutionTextEffect_) {
        std::vector<cg2::TextLabel*> neonLabels;
        neonLabels.reserve(4);
        if (ui_.staticEvolutionTitleLabel_) {
            neonLabels.push_back(ui_.staticEvolutionTitleLabel_.get());
        }
        if (ui_.staticEvolutionDetailClassLabel_) {
            neonLabels.push_back(ui_.staticEvolutionDetailClassLabel_.get());
        }
        if (ui_.staticEvolutionCandidateCount_ > 0) {
            const size_t selectedNode = static_cast<size_t>((std::clamp)(ui_.evolutionUiStyle_.fixedSelectedCandidate, 0,
                                                                         static_cast<int>(ui_.staticEvolutionCandidateCount_ - 1))) +
                                        1;
            if (selectedNode < ui_.staticEvolutionTankButtons_.size() && ui_.staticEvolutionTankButtons_[selectedNode]) {
                neonLabels.push_back(ui_.staticEvolutionTankButtons_[selectedNode]->GetLabel());
            }
        }
        ui_.staticEvolutionTextEffect_->DrawBloom(neonLabels);
    }
}

void PlayerEvolution::DrawStaticEvolutionDebugOverlay()
{
#ifdef USE_IMGUI
    if (!ui_.showEvolutionVirtualBounds_ && !ui_.showEvolutionSafeArea_ && !ui_.showEvolutionNodeBounds_ &&
        !ui_.showEvolutionMouseBounds_ && !ui_.showEvolutionTextBounds_ && !ui_.showEvolutionCenterLines_ &&
        !ui_.showEvolutionCircuitControlPoints_ && !ui_.showEvolutionResolutionInfo_) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const float clientWidth = static_cast<float>(cg2::WinApp::GetInstance()->GetClientWidth());
    const float clientHeight = static_cast<float>(cg2::WinApp::GetInstance()->GetClientHeight());
    const float virtualWidth = (std::max)(1.0f, ui_.evolutionUiStyle_.virtualResolution.x);
    const float virtualHeight = (std::max)(1.0f, ui_.evolutionUiStyle_.virtualResolution.y);
    const float scale = (std::max)(0.0001f, (std::min)(clientWidth / virtualWidth, clientHeight / virtualHeight));
    const cg2::Vector2 offset = {(clientWidth - virtualWidth * scale) * 0.5f, (clientHeight - virtualHeight * scale) * 0.5f};
    auto toClient = [&](const cg2::Vector2& point) {
        return ImVec2(offset.x + point.x * scale, offset.y + point.y * scale);
    };
    auto drawCenteredRect = [&](const cg2::Vector2& center, const cg2::Vector2& size, ImU32 color) {
        const cg2::Vector2 minPoint{center.x - size.x * 0.5f, center.y - size.y * 0.5f};
        const cg2::Vector2 maxPoint{center.x + size.x * 0.5f, center.y + size.y * 0.5f};
        drawList->AddRect(toClient(minPoint), toClient(maxPoint), color, 0.0f, 0, 1.5f);
    };

    if (ui_.showEvolutionVirtualBounds_) {
        drawList->AddRect(toClient({0.0f, 0.0f}), toClient(ui_.evolutionUiStyle_.virtualResolution), IM_COL32(255, 210, 70, 230), 0.0f, 0,
                          2.0f);
    }
    if (ui_.showEvolutionSafeArea_) {
        const float safe = ui_.evolutionUiStyle_.safeMargin;
        drawList->AddRect(toClient({safe, safe}), toClient({virtualWidth - safe, virtualHeight - safe}), IM_COL32(80, 255, 160, 230), 0.0f,
                          0, 2.0f);
    }
    if (ui_.showEvolutionCenterLines_) {
        drawList->AddLine(ImVec2(clientWidth * 0.5f, 0.0f), ImVec2(clientWidth * 0.5f, clientHeight), IM_COL32(255, 80, 180, 180), 1.0f);
        drawList->AddLine(ImVec2(0.0f, clientHeight * 0.5f), ImVec2(clientWidth, clientHeight * 0.5f), IM_COL32(255, 80, 180, 180), 1.0f);
    }
    for (size_t i = 0; i < ui_.staticEvolutionCandidateCount_ + 1; ++i) {
        if (ui_.showEvolutionNodeBounds_) {
            drawCenteredRect(ui_.staticEvolutionNodeCentersVirtual_[i], ui_.staticEvolutionNodeDrawSizesVirtual_[i],
                             IM_COL32(75, 190, 255, 230));
        }
        if (ui_.showEvolutionMouseBounds_) {
            drawCenteredRect(ui_.staticEvolutionNodeCentersVirtual_[i], ui_.staticEvolutionNodeHitSizesVirtual_[i],
                             IM_COL32(255, 120, 70, 210));
        }
    }
    if (ui_.showEvolutionCircuitControlPoints_) {
        for (int pathIndex = 0; pathIndex < static_cast<int>(ui_.staticEvolutionCircuitControlPoints_.size()); ++pathIndex) {
            const int count = ui_.staticEvolutionCircuitControlPointCounts_[pathIndex];
            for (int i = 0; i < count; ++i) {
                const ImVec2 p = toClient(ui_.staticEvolutionCircuitControlPoints_[pathIndex][i]);
                drawList->AddCircleFilled(p, 4.0f, IM_COL32(255, 235, 90, 240), 12);
                if (i + 1 < count) {
                    drawList->AddLine(p, toClient(ui_.staticEvolutionCircuitControlPoints_[pathIndex][i + 1]), IM_COL32(255, 235, 90, 150),
                                      1.0f);
                }
            }
        }
    }
    if (ui_.showEvolutionTextBounds_) {
        auto drawTextBounds = [&](const std::unique_ptr<cg2::TextLabel>& label) {
            if (!label || !label->GetSprite()) {
                return;
            }
            cg2::Sprite* sprite = label->GetSprite();
            const float renderScale = GetEvolutionRenderScale();
            const cg2::Vector2 renderOffset = GetEvolutionRenderOffset();
            const cg2::Vector2 renderPosition = sprite->GetPosition();
            const cg2::Vector2 renderSize = sprite->GetSize();
            const cg2::Vector2 anchor = sprite->GetAnchorPoint();
            const cg2::Vector2 virtualPosition = {(renderPosition.x - renderOffset.x) / renderScale,
                                                  (renderPosition.y - renderOffset.y) / renderScale};
            const cg2::Vector2 virtualSize = {renderSize.x / renderScale, renderSize.y / renderScale};
            const cg2::Vector2 minPoint = {virtualPosition.x - virtualSize.x * anchor.x, virtualPosition.y - virtualSize.y * anchor.y};
            drawList->AddRect(toClient(minPoint), toClient({minPoint.x + virtualSize.x, minPoint.y + virtualSize.y}),
                              IM_COL32(190, 110, 255, 220), 0.0f, 0, 1.0f);
        };
        drawTextBounds(ui_.staticEvolutionTitleLabel_);
        drawTextBounds(ui_.staticEvolutionPrototypeLabel_);
        for (size_t i = 0; i < ui_.staticEvolutionCandidateCount_ + 1; ++i) {
            drawTextBounds(ui_.staticEvolutionNodeNameLabels_[i]);
            drawTextBounds(ui_.staticEvolutionNodeRankLabels_[i]);
        }
        drawTextBounds(ui_.staticEvolutionDetailClassLabel_);
        drawTextBounds(ui_.staticEvolutionRoleLabel_);
        for (const auto& label : ui_.staticEvolutionDeltaLabels_) {
            drawTextBounds(label);
        }
        drawTextBounds(ui_.staticEvolutionAbilityLabel_);
        drawTextBounds(ui_.staticEvolutionConfirmLabel_);
        drawTextBounds(ui_.staticEvolutionPanelHintLabel_);
    }
    if (ui_.showEvolutionResolutionInfo_) {
        char buffer[160]{};
        std::snprintf(buffer, sizeof(buffer), "Evolution UI  %.0f x %.0f  scale %.3f  virtual %.0f x %.0f", clientWidth, clientHeight,
                      scale, virtualWidth, virtualHeight);
        drawList->AddText(ImVec2(18.0f, clientHeight - 28.0f), IM_COL32(220, 250, 255, 255), buffer);
    }
#endif
}

/// @brief 進化UIの処理時間と描画件数への読み取り専用参照を返す。
const Player::UiProfileStats& PlayerEvolution::GetEvolutionUiProfileStats() const
{
    return ui_.evolutionUiProfile_;
}
