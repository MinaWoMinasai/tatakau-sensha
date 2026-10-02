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

bool Player::LoadEvolutionCircuitTree(const std::string& path)
{
    evolutionCircuitNodes_.clear();
    evolutionCircuitEdges_.clear();
    evolutionCircuitLoaded_ = false;

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
            if (!GetClassConfig(classId) || evolutionCircuitNodes_.size() >= kEvolutionCircuitMaxNodes) {
                continue;
            }
            const float lane = node.value("lane", 0.5f);
            evolutionCircuitNodes_.push_back({classId, (std::clamp)(lane, 0.0f, 1.0f)});
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
                    return std::any_of(evolutionCircuitNodes_.begin(), evolutionCircuitNodes_.end(),
                                       [&](const EvolutionCircuitNodeDefinition& node) {
                                           return node.classId == id;
                                       });
                };
                if (hasNode(from) && hasNode(to)) {
                    evolutionCircuitEdges_.push_back({from, to});
                }
            }
        }
    }
    catch (const std::exception&) {
        evolutionCircuitNodes_.clear();
        evolutionCircuitEdges_.clear();
        return false;
    }
    evolutionCircuitLoaded_ = !evolutionCircuitNodes_.empty();
    return evolutionCircuitLoaded_;
}

bool Player::ShouldUseEvolutionCircuitPrototype() const
{
    return evolutionUiStyle_.enabled && evolutionCircuitLoaded_ && !evolutionCircuitNodes_.empty();
}

void Player::InitializeEvolutionCircuitPrototype()
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
    evolutionCircuitBackdropSprite_ = makeSprite({0.0f, 0.0f});
    evolutionCircuitDetailPanelSprite_ = makeSprite({0.5f, 0.5f});
    for (auto& line : evolutionCircuitLineSprites_) {
        line = makeSprite({0.0f, 0.5f});
    }
    for (size_t i = 0; i < evolutionCircuitNodes_.size(); ++i) {
        evolutionCircuitTankButtons_[i] = std::make_unique<TankButtonUI>();
        evolutionCircuitTankButtons_[i]->Initialize(spriteCommon);
    }
    evolutionCircuitDetailPreview_ = std::make_unique<TankButtonUI>();
    evolutionCircuitDetailPreview_->Initialize(spriteCommon);

    evolutionCircuitSelectedNode_ = 0;
    if (evolutionHistory_.empty() || evolutionHistory_.back() != currentClassId_) {
        evolutionHistory_.clear();
        evolutionHistory_.push_back(currentClassId_);
    }
    for (size_t i = 0; i < evolutionCircuitNodes_.size(); ++i) {
        if (evolutionCircuitNodes_[i].classId == currentClassId_) {
            evolutionCircuitSelectedNode_ = static_cast<int>(i);
            break;
        }
    }
    UpdateEvolutionCircuitPrototype();
}

void Player::UpdateEvolutionCircuitPrototype()
{
    if (!ShouldUseEvolutionCircuitPrototype()) {
        return;
    }
    if (input_ && input_->IsTrigger(input_->GetKey()[DIK_ESCAPE], input_->GetPreKey()[DIK_ESCAPE])) {
        isChangeMode = false;
        evolutionCancelledEvent_ = true;
        return;
    }
    if (evolutionHistory_.empty() || evolutionHistory_.back() != currentClassId_) {
        evolutionHistory_.clear();
        evolutionHistory_.push_back(currentClassId_);
    }

    constexpr float kTreeLeft = 174.0f;
    constexpr float kTreeRight = 1106.0f;
    constexpr float kTreeTop = 100.0f;
    constexpr float kTreeBottom = 498.0f;
    constexpr cg2::Vector2 kNodeSize{158.0f, 64.0f};
    const float renderScale = GetEvolutionRenderScale();
    const cg2::Vector2 mouseVirtual = EvolutionClientToVirtual(mousePosition_);
    const auto findNodeIndex = [&](const std::string& id) -> int {
        for (size_t i = 0; i < evolutionCircuitNodes_.size(); ++i) {
            if (evolutionCircuitNodes_[i].classId == id && IsEvolutionClassVisible(id))
                return static_cast<int>(i);
        }
        return -1;
    };

    for (size_t i = 0; i < evolutionCircuitNodes_.size(); ++i) {
        const PlayerClassConfig* config = GetClassConfig(evolutionCircuitNodes_[i].classId);
        const int rank = config ? (std::clamp)(config->requiredRank, 1, 4) : 1;
        const float rankRatio = static_cast<float>(rank - 1) / 3.0f;
        evolutionCircuitNodeCentersVirtual_[i] = {kTreeLeft + (kTreeRight - kTreeLeft) * rankRatio,
                                                  kTreeTop + (kTreeBottom - kTreeTop) * evolutionCircuitNodes_[i].lane};
    }

    evolutionCircuitHoveredNode_ = -1;
    for (size_t i = 0; i < evolutionCircuitNodes_.size(); ++i) {
        if (!IsEvolutionClassVisible(evolutionCircuitNodes_[i].classId))
            continue;
        const cg2::Vector2 center = evolutionCircuitNodeCentersVirtual_[i];
        if (mouseVirtual.x >= center.x - kNodeSize.x * 0.5f && mouseVirtual.x <= center.x + kNodeSize.x * 0.5f &&
            mouseVirtual.y >= center.y - kNodeSize.y * 0.5f && mouseVirtual.y <= center.y + kNodeSize.y * 0.5f) {
            evolutionCircuitHoveredNode_ = static_cast<int>(i);
            break;
        }
    }
    const bool primaryTriggered =
        input_ && input_->IsTrigger(input_->GetMouseState().rgbButtons[0], input_->GetPreMouseState().rgbButtons[0]);
    if (primaryTriggered && evolutionCircuitHoveredNode_ >= 0) {
        evolutionCircuitSelectedNode_ = evolutionCircuitHoveredNode_;
    }
    evolutionCircuitSelectedNode_ = (std::clamp)(evolutionCircuitSelectedNode_, 0, static_cast<int>(evolutionCircuitNodes_.size()) - 1);
    if (!IsEvolutionClassVisible(evolutionCircuitNodes_[static_cast<size_t>(evolutionCircuitSelectedNode_)].classId)) {
        const int currentNode = findNodeIndex(currentClassId_);
        if (currentNode >= 0)
            evolutionCircuitSelectedNode_ = currentNode;
    }
    const std::string& selectedClassId = evolutionCircuitNodes_[static_cast<size_t>(evolutionCircuitSelectedNode_)].classId;
    const bool confirmTriggered = input_ && input_->IsTrigger(input_->GetKey()[DIK_RETURN], input_->GetPreKey()[DIK_RETURN]);
    if (confirmTriggered && CanEvolveTo(selectedClassId)) {
        TryConfirmEvolutionById(selectedClassId);
        return;
    }

    std::vector<bool> selectedPath(evolutionCircuitNodes_.size(), false);
    selectedPath[static_cast<size_t>(evolutionCircuitSelectedNode_)] = true;
    for (size_t pass = 0; pass < evolutionCircuitNodes_.size(); ++pass) {
        for (const auto& edge : evolutionCircuitEdges_) {
            const int from = findNodeIndex(edge.from);
            const int to = findNodeIndex(edge.to);
            if (from >= 0 && to >= 0 && selectedPath[static_cast<size_t>(to)])
                selectedPath[static_cast<size_t>(from)] = true;
        }
    }

    evolutionCircuitBackdropSprite_->SetPosition({0.0f, 0.0f});
    evolutionCircuitBackdropSprite_->SetSize({static_cast<float>(cg2::WinApp::GetInstance()->GetClientWidth()),
                                              static_cast<float>(cg2::WinApp::GetInstance()->GetClientHeight())});
    evolutionCircuitBackdropSprite_->SetColor({0.004f, 0.010f, 0.024f, 0.65f});
    evolutionCircuitBackdropSprite_->Update();
    evolutionCircuitDetailPanelSprite_->SetPosition(EvolutionVirtualToRender({640.0f, 611.0f}));
    evolutionCircuitDetailPanelSprite_->SetSize({1160.0f * renderScale, 172.0f * renderScale});
    evolutionCircuitDetailPanelSprite_->SetColor({0.012f, 0.030f, 0.052f, 0.96f});
    evolutionCircuitDetailPanelSprite_->Update();

    const float pulse = 0.82f + std::sin(evolutionUiTimer_ * 3.0f) * 0.18f;
    for (size_t i = 0; i < evolutionCircuitNodes_.size(); ++i) {
        const PlayerClassConfig* config = GetClassConfig(evolutionCircuitNodes_[i].classId);
        if (!config || !evolutionCircuitTankButtons_[i] || !tankButtonUiStyle_)
            continue;
        if (!IsEvolutionClassVisible(config->id))
            continue;
        const bool isCurrent = evolutionCircuitNodes_[i].classId == currentClassId_;
        const bool isSelected = static_cast<int>(i) == evolutionCircuitSelectedNode_;
        const bool isHovered = static_cast<int>(i) == evolutionCircuitHoveredNode_;
        const bool hasDirectEdge = HasEvolutionEdge(currentClassId_, config->id);
        const bool available = CanEvolveTo(config->id);
        const bool rankLocked = hasDirectEdge && GetRankFromLevel(level_) < config->requiredRank;
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
        if (GetTankButtonVisualData(config->id, visualData)) {
            visualData.hiraganaName = config->displayName;
            evolutionCircuitTankButtons_[i]->SetVisualData(visualData);
        }
        TankButtonUiStyle style = *tankButtonUiStyle_;
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
        evolutionCircuitTankButtons_[i]->SetRank(config->requiredRank);
        evolutionCircuitTankButtons_[i]->SetState(isCurrent    ? TankButtonState::Selected
                                                  : isSelected ? TankButtonState::Selected
                                                  : isHovered  ? TankButtonState::Hover
                                                  : rankLocked ? TankButtonState::Locked
                                                               : TankButtonState::Normal);
        evolutionCircuitTankButtons_[i]->Update(EvolutionVirtualToRender(evolutionCircuitNodeCentersVirtual_[i]), style);
    }

    for (auto& line : evolutionCircuitLineSprites_) {
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
        for (size_t layer = 0; layer < widths.size() && lineIndex < evolutionCircuitLineSprites_.size(); ++layer) {
            cg2::Sprite* line = evolutionCircuitLineSprites_[lineIndex++].get();
            line->SetPosition(from);
            line->SetSize({length, widths[layer] * renderScale});
            line->SetRotation(std::atan2(dy, dx));
            cg2::Vector4 layerColor = color;
            layerColor.w *= alphas[layer];
            line->SetColor(layerColor);
            line->Update();
        }
    };
    for (const auto& edge : evolutionCircuitEdges_) {
        const int fromIndex = findNodeIndex(edge.from);
        const int toIndex = findNodeIndex(edge.to);
        if (fromIndex < 0 || toIndex < 0)
            continue;
        cg2::Vector4 color{0.46f, 0.56f, 0.61f, 0.40f};
        bool highlighted = false;
        bool traversed = false;
        for (size_t historyIndex = 1; historyIndex < evolutionHistory_.size(); ++historyIndex) {
            if (evolutionHistory_[historyIndex - 1] == edge.from && evolutionHistory_[historyIndex] == edge.to) {
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
        const cg2::Vector2 start{evolutionCircuitNodeCentersVirtual_[fromIndex].x + kNodeSize.x * 0.5f,
                                 evolutionCircuitNodeCentersVirtual_[fromIndex].y};
        const cg2::Vector2 end{evolutionCircuitNodeCentersVirtual_[toIndex].x - kNodeSize.x * 0.5f,
                               evolutionCircuitNodeCentersVirtual_[toIndex].y};
        const float midX = (start.x + end.x) * 0.5f;
        queueSegment(start, {midX, start.y}, color, highlighted);
        queueSegment({midX, start.y}, {midX, end.y}, color, highlighted);
        queueSegment({midX, end.y}, end, color, highlighted);
    }

    const PlayerClassConfig* selected = GetClassConfig(evolutionCircuitNodes_[static_cast<size_t>(evolutionCircuitSelectedNode_)].classId);
    const PlayerClassConfig* current = GetCurrentClassConfig();
    if (selected && evolutionCircuitDetailPreview_ && tankButtonUiStyle_) {
        TankButtonVisualData visualData{};
        if (GetTankButtonVisualData(selected->id, visualData)) {
            visualData.hiraganaName = selected->displayName;
            evolutionCircuitDetailPreview_->SetVisualData(visualData);
        }
        TankButtonUiStyle style = *tankButtonUiStyle_;
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
        evolutionCircuitDetailPreview_->SetRank(selected->requiredRank);
        evolutionCircuitDetailPreview_->SetState(TankButtonState::Selected);
        evolutionCircuitDetailPreview_->Update(EvolutionVirtualToRender({188.0f, 611.0f}), style);
    }

    cg2::SpriteCommon* spriteCommon = cg2::SpriteCommon::GetInstance();
    auto makeTextStyle = [&](float fontSize, const cg2::Vector4& color) {
        cg2::TextStyle style{};
        style.fontFamily = evolutionUiStyle_.fontFamily;
        style.fontPath = evolutionUiStyle_.fontPath;
        style.fontWeight = evolutionUiStyle_.fontWeight;
        style.fontSize = fontSize * renderScale;
        style.color = color;
        style.outlineColor = evolutionUiStyle_.textOutlineColor;
        style.outlineThickness = 1.0f * renderScale;
        style.padding = 5.0f * renderScale;
        return style;
    };
    cg2::TextStyle titleStyle = makeTextStyle(27.0f, {0.72f, 1.0f, 0.94f, 1.0f});
    SetLabel(evolutionCircuitTitleLabel_, spriteCommon, "EVOLUTION CIRCUIT", EvolutionVirtualToRender({640.0f, 25.0f}), titleStyle);
    evolutionCircuitTitleLabel_->SetAnchorPoint({0.5f, 0.0f});
    for (int rank = 1; rank <= 4; ++rank) {
        const float rankRatio = static_cast<float>(rank - 1) / 3.0f;
        cg2::TextStyle rankStyle = makeTextStyle(19.0f, {0.64f, 0.89f, 0.96f, 0.96f});
        SetLabel(evolutionCircuitRankLabels_[static_cast<size_t>(rank - 1)], spriteCommon, "RANK " + std::to_string(rank),
                 EvolutionVirtualToRender({kTreeLeft + (kTreeRight - kTreeLeft) * rankRatio, 68.0f}), rankStyle);
        evolutionCircuitRankLabels_[static_cast<size_t>(rank - 1)]->SetAnchorPoint({0.5f, 0.5f});
    }
    if (selected) {
        cg2::TextStyle nameStyle = makeTextStyle(22.0f, {0.88f, 1.0f, 0.96f, 1.0f});
        cg2::TextStyle detailStyle = makeTextStyle(14.0f, {0.72f, 0.86f, 0.94f, 0.94f});
        SetLabel(evolutionCircuitDetailNameLabel_, spriteCommon, selected->displayName, EvolutionVirtualToRender({315.0f, 548.0f}),
                 nameStyle);
        SetLabel(evolutionCircuitDetailMetaLabel_, spriteCommon, "REQUIRED RANK " + std::to_string(selected->requiredRank),
                 EvolutionVirtualToRender({1115.0f, 554.0f}), detailStyle);
        evolutionCircuitDetailMetaLabel_->SetAnchorPoint({1.0f, 0.5f});
        SetLabel(evolutionCircuitDetailRoleLabel_, spriteCommon, GetEvolutionShortRole(*selected),
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
            SetLabel(evolutionCircuitDetailStatLabels_[i], spriteCommon, stats[i],
                     EvolutionVirtualToRender({610.0f, 550.0f + static_cast<float>(i) * 39.0f}), detailStyle);
        }
    }
    std::string stateHint = "PREVIEW MODE     ESC  閉じる";
    if (selected && selected->id == currentClassId_) {
        stateHint = "CURRENT CLASS     ESC  閉じる";
    } else if (selected && CanEvolveTo(selected->id)) {
        stateHint = "ENTER  進化     ESC  閉じる";
    } else if (selected && current && HasEvolutionEdge(current->id, selected->id) && selected->requiredRank == current->requiredRank + 1 &&
               GetRankFromLevel(level_) < selected->requiredRank) {
        stateHint = "RANK " + std::to_string(selected->requiredRank) + " REQUIRED     ESC  閉じる";
    }
    cg2::TextStyle hintStyle = makeTextStyle(12.5f, {0.46f, 0.68f, 0.74f, 0.76f});
    SetLabel(evolutionCircuitHintLabel_, spriteCommon, stateHint, EvolutionVirtualToRender({1150.0f, 650.0f}), hintStyle);
    evolutionCircuitHintLabel_->SetAnchorPoint({1.0f, 0.5f});
    PrepareEvolutionCircuitTextTextures();

    if (staticEvolutionButtonBloomEffect_ && tankButtonUiStyle_) {
        cg2::BloomParam bloomParam = staticEvolutionButtonBloomEffect_->GetParam();
        bloomParam.threshold = 0.0f;
        bloomParam.intensity = 0.92f + tankButtonUiStyle_->bloomBoost * 1.8f;
        bloomParam.outlineWidth = 0.0f;
        staticEvolutionButtonBloomEffect_->SetParam(bloomParam);
        staticEvolutionButtonBloomEffect_->Update(0.0f);
    }
}

void Player::PrepareEvolutionCircuitTextTextures()
{
    auto prepare = [](cg2::TextLabel* label) {
        if (label)
            label->PrepareForDraw();
    };
    prepare(evolutionCircuitTitleLabel_.get());
    for (const auto& label : evolutionCircuitRankLabels_)
        prepare(label.get());
    prepare(evolutionCircuitDetailNameLabel_.get());
    prepare(evolutionCircuitDetailMetaLabel_.get());
    prepare(evolutionCircuitDetailRoleLabel_.get());
    for (const auto& label : evolutionCircuitDetailStatLabels_)
        prepare(label.get());
    prepare(evolutionCircuitHintLabel_.get());
    for (size_t i = 0; i < evolutionCircuitNodes_.size(); ++i) {
        if (evolutionCircuitTankButtons_[i])
            prepare(evolutionCircuitTankButtons_[i]->GetLabel());
    }
    if (evolutionCircuitDetailPreview_)
        prepare(evolutionCircuitDetailPreview_->GetLabel());
}

bool Player::ShouldUseStaticEvolutionPrototype() const
{
    if (!evolutionUiStyle_.enabled) {
        return false;
    }
    const PlayerClassConfig* current = GetCurrentClassConfig();
    if (!current || current->requiredRank >= 4) {
        return false;
    }
    const int targetRank = current->requiredRank + 1;
    for (const std::string& id : classCatalog_.OrderedIds()) {
        const PlayerClassConfig* config = GetClassConfig(id);
        if (config && config->requiredRank == targetRank && IsRunCompatibleClass(*config)) {
            return true;
        }
    }
    return false;
}

void Player::RefreshStaticEvolutionCandidates()
{
    staticEvolutionCandidateCount_ = 0;
    for (std::string& id : staticEvolutionCandidateIds_) {
        id.clear();
    }
    const PlayerClassConfig* current = GetCurrentClassConfig();
    if (!current) {
        return;
    }
    const int targetRank = current->requiredRank + 1;
    for (const std::string& id : classCatalog_.OrderedIds()) {
        const PlayerClassConfig* config = GetClassConfig(id);
        if (!config || config->requiredRank != targetRank || config->id == current->id || !IsRunCompatibleClass(*config)) {
            continue;
        }
        if (staticEvolutionCandidateCount_ >= staticEvolutionCandidateIds_.size()) {
            break;
        }
        staticEvolutionCandidateIds_[staticEvolutionCandidateCount_++] = config->id;
    }
    if (staticEvolutionCandidateCount_ == 0) {
        evolutionUiStyle_.fixedSelectedCandidate = 0;
    } else {
        evolutionUiStyle_.fixedSelectedCandidate =
            (std::clamp)(evolutionUiStyle_.fixedSelectedCandidate, 0, static_cast<int>(staticEvolutionCandidateCount_ - 1));
    }
}

std::string Player::GetEvolutionClassName(const std::string& classId) const
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

std::string Player::GetEvolutionShortRole(const PlayerClassConfig& config) const
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

std::string Player::GetEvolutionRole(const PlayerClassConfig& config) const
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

std::array<std::string, 3> Player::GetEvolutionDeltas(const PlayerClassConfig& current, const PlayerClassConfig& target) const
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

std::string Player::GetEvolutionAbility(const PlayerClassConfig& config) const
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

float Player::GetEvolutionRenderScale() const
{
    const float virtualWidth = (std::max)(1.0f, evolutionUiStyle_.virtualResolution.x);
    const float virtualHeight = (std::max)(1.0f, evolutionUiStyle_.virtualResolution.y);
    const float clientWidth = static_cast<float>(cg2::WinApp::GetInstance()->GetClientWidth());
    const float clientHeight = static_cast<float>(cg2::WinApp::GetInstance()->GetClientHeight());
    return (std::min)(clientWidth / virtualWidth, clientHeight / virtualHeight);
}

cg2::Vector2 Player::GetEvolutionRenderOffset() const
{
    const float scale = GetEvolutionRenderScale();
    const float clientWidth = static_cast<float>(cg2::WinApp::GetInstance()->GetClientWidth());
    const float clientHeight = static_cast<float>(cg2::WinApp::GetInstance()->GetClientHeight());
    return {(clientWidth - evolutionUiStyle_.virtualResolution.x * scale) * 0.5f,
            (clientHeight - evolutionUiStyle_.virtualResolution.y * scale) * 0.5f};
}

cg2::Vector2 Player::EvolutionAnchorToVirtual(const cg2::Vector2& normalizedAnchor) const
{
    const float safe = (std::clamp)(evolutionUiStyle_.safeMargin, 0.0f,
                                    (std::min)(evolutionUiStyle_.virtualResolution.x, evolutionUiStyle_.virtualResolution.y) * 0.45f);
    const cg2::Vector2 usable = {(std::max)(1.0f, evolutionUiStyle_.virtualResolution.x - safe * 2.0f),
                                 (std::max)(1.0f, evolutionUiStyle_.virtualResolution.y - safe * 2.0f)};
    return {safe + (std::clamp)(normalizedAnchor.x, 0.0f, 1.0f) * usable.x, safe + (std::clamp)(normalizedAnchor.y, 0.0f, 1.0f) * usable.y};
}

cg2::Vector2 Player::EvolutionVirtualToRender(const cg2::Vector2& virtualPosition) const
{
    const float scale = GetEvolutionRenderScale();
    const cg2::Vector2 offset = GetEvolutionRenderOffset();
    return {offset.x + virtualPosition.x * scale, offset.y + virtualPosition.y * scale};
}

cg2::Vector2 Player::EvolutionClientToVirtual(const cg2::Vector2& clientPosition) const
{
    const float clientWidth = static_cast<float>(cg2::WinApp::GetInstance()->GetClientWidth());
    const float clientHeight = static_cast<float>(cg2::WinApp::GetInstance()->GetClientHeight());
    const float virtualWidth = (std::max)(1.0f, evolutionUiStyle_.virtualResolution.x);
    const float virtualHeight = (std::max)(1.0f, evolutionUiStyle_.virtualResolution.y);
    const float scale = (std::max)(0.0001f, (std::min)(clientWidth / virtualWidth, clientHeight / virtualHeight));
    const cg2::Vector2 offset = {(clientWidth - virtualWidth * scale) * 0.5f, (clientHeight - virtualHeight * scale) * 0.5f};
    return {(clientPosition.x - offset.x) / scale, (clientPosition.y - offset.y) / scale};
}

void Player::InitializeStaticEvolutionPrototype()
{
    cg2::StartupTrace::Scope scope("Player.StaticEvolutionUi");
    cg2::SpriteCommon* spriteCommon = cg2::SpriteCommon::GetInstance();
    auto makeSprite = [spriteCommon](const std::string& texture, const cg2::Vector2& anchor) {
        auto sprite = std::make_unique<cg2::Sprite>();
        sprite->Initialize(spriteCommon, texture);
        sprite->SetAnchorPoint(anchor);
        return sprite;
    };

    staticEvolutionBackdropSprite_ = makeSprite("resources/white512x512.png", {0.0f, 0.0f});
    staticEvolutionDetailPanelSprite_ = makeSprite("resources/white512x512.png", {0.5f, 0.5f});
    staticEvolutionConfirmButtonSprite_ = makeSprite("resources/white512x512.png", {0.5f, 0.5f});
    staticEvolutionBranchGlowSprite_ = makeSprite("resources/white512x512.png", {0.5f, 0.5f});
    staticEvolutionBranchCoreSprite_ = makeSprite("resources/white512x512.png", {0.5f, 0.5f});
    for (auto& line : staticEvolutionConfirmOutlineSprites_) {
        line = makeSprite("resources/white512x512.png", {0.0f, 0.5f});
    }
    for (auto& nodePanels : staticEvolutionNodePanelSprites_) {
        for (auto& panel : nodePanels) {
            panel = makeSprite("resources/white512x512.png", {0.5f, 0.5f});
        }
    }
    for (auto& nodeLines : staticEvolutionNodeFrameSprites_) {
        for (auto& line : nodeLines) {
            line = makeSprite("resources/white512x512.png", {0.0f, 0.5f});
        }
    }
    for (auto& nodeLines : staticEvolutionSilhouetteSprites_) {
        for (auto& line : nodeLines) {
            line = makeSprite("resources/white512x512.png", {0.0f, 0.5f});
        }
    }
    for (auto& line : staticEvolutionCircuitSprites_) {
        line = makeSprite("resources/white512x512.png", {0.0f, 0.5f});
    }

    tankButtonUiStyle_ = std::make_unique<TankButtonUiStyle>();
    LoadTankButtonUiStyle(*tankButtonUiStyle_);
    for (size_t classIndex = 0; classIndex < staticEvolutionTankButtons_.size(); ++classIndex) {
        staticEvolutionTankButtons_[classIndex] = std::make_unique<TankButtonUI>();
        staticEvolutionTankButtons_[classIndex]->Initialize(spriteCommon);
    }
    staticEvolutionButtonBloomEffect_ = std::make_unique<cg2::ObjectPostEffect>();
    staticEvolutionButtonBloomEffect_->Initialize(cg2::Object3dCommon::GetInstance()->GetDxCommon(),
                                                  cg2::Object3dCommon::GetInstance()->GetSrvManager(), nullptr, 1.0f);
    staticEvolutionTextEffect_ = std::make_unique<NeonTextEffect>();
    staticEvolutionTextEffect_->Initialize(cg2::Object3dCommon::GetInstance()->GetDxCommon(),
                                           cg2::Object3dCommon::GetInstance()->GetSrvManager());
    staticEvolutionTextEffect_->SetStyle(evolutionUiStyle_.neonText);

    UpdateStaticEvolutionPrototype();
}

void Player::UpdateStaticEvolutionPrototype()
{
    if (!staticEvolutionBackdropSprite_) {
        return;
    }

    RefreshStaticEvolutionCandidates();
    if (staticEvolutionCandidateCount_ == 0) {
        return;
    }
    if (input_ && input_->IsTrigger(input_->GetKey()[DIK_ESCAPE], input_->GetPreKey()[DIK_ESCAPE])) {
        isChangeMode = false;
        evolutionCancelledEvent_ = true;
        return;
    }

    const size_t activeNodeCount = staticEvolutionCandidateCount_ + 1;
    const float renderScale = GetEvolutionRenderScale();
    const cg2::Vector2 mouseVirtual = EvolutionClientToVirtual(mousePosition_);
    const auto& activeAnchors = evolutionUiStyle_.radialLayout ? evolutionUiStyle_.radialNodeAnchors : evolutionUiStyle_.nodeAnchors;

    staticEvolutionNodeCentersVirtual_[0] = EvolutionAnchorToVirtual(activeAnchors[0]);
    for (size_t candidateIndex = 0; candidateIndex < staticEvolutionCandidateCount_; ++candidateIndex) {
        cg2::Vector2 anchor{};
        if (staticEvolutionCandidateCount_ == 3) {
            anchor = activeAnchors[candidateIndex + 1];
        } else if (evolutionUiStyle_.radialLayout) {
            const float angle =
                -1.5707963268f + static_cast<float>(candidateIndex) * 6.2831853072f / static_cast<float>(staticEvolutionCandidateCount_);
            anchor = {0.50f + std::cos(angle) * 0.30f, 0.40f + std::sin(angle) * 0.27f};
        } else {
            const float y =
                staticEvolutionCandidateCount_ == 1
                    ? 0.43f
                    : 0.13f + static_cast<float>(candidateIndex) * (0.54f / static_cast<float>(staticEvolutionCandidateCount_ - 1));
            anchor = {activeAnchors[1].x, y};
        }
        staticEvolutionNodeCentersVirtual_[candidateIndex + 1] = EvolutionAnchorToVirtual(anchor);
    }

    for (size_t i = 0; i < activeNodeCount; ++i) {
        const cg2::Vector2 baseSize = i == 0 ? evolutionUiStyle_.currentNodeSize : evolutionUiStyle_.candidateNodeSize;
        staticEvolutionNodeHitSizesVirtual_[i] = {baseSize.x + 16.0f, baseSize.y + 16.0f};
    }

    staticEvolutionHoveredNode_ = -1;
    for (size_t i = 1; i < activeNodeCount; ++i) {
        const cg2::Vector2 center = staticEvolutionNodeCentersVirtual_[i];
        const cg2::Vector2 hitSize = staticEvolutionNodeHitSizesVirtual_[i];
        if (mouseVirtual.x >= center.x - hitSize.x * 0.5f && mouseVirtual.x <= center.x + hitSize.x * 0.5f &&
            mouseVirtual.y >= center.y - hitSize.y * 0.5f && mouseVirtual.y <= center.y + hitSize.y * 0.5f) {
            staticEvolutionHoveredNode_ = static_cast<int>(i);
            break;
        }
    }
    const bool primaryTriggered =
        input_ && input_->IsTrigger(input_->GetMouseState().rgbButtons[0], input_->GetPreMouseState().rgbButtons[0]);
    if (primaryTriggered && staticEvolutionHoveredNode_ > 0) {
        evolutionUiStyle_.fixedSelectedCandidate = staticEvolutionHoveredNode_ - 1;
    }

    staticEvolutionBackdropSprite_->SetPosition({0.0f, 0.0f});
    staticEvolutionBackdropSprite_->SetSize({static_cast<float>(cg2::WinApp::GetInstance()->GetClientWidth()),
                                             static_cast<float>(cg2::WinApp::GetInstance()->GetClientHeight())});
    staticEvolutionBackdropSprite_->SetColor({0.005f, 0.012f, 0.025f, evolutionUiStyle_.backgroundDimOpacity});
    staticEvolutionBackdropSprite_->Update();

    const int currentRank = GetRankFromLevel(level_);
    for (size_t i = 0; i < activeNodeCount; ++i) {
        const bool selected = i > 0 && i - 1 == evolutionUiStyle_.fixedSelectedCandidate;
        const bool hovered = i == staticEvolutionHoveredNode_;
        const std::string& nodeClassId = i == 0 ? currentClassId_ : staticEvolutionCandidateIds_[i - 1];
        const PlayerClassConfig* candidateConfig = i > 0 ? GetClassConfig(nodeClassId) : nullptr;
        const PlayerClassConfig* nodeConfig = GetClassConfig(nodeClassId);
        const bool locked = candidateConfig && currentRank < candidateConfig->requiredRank;
        float stateScale = evolutionUiStyle_.normalScale;
        if (selected) {
            stateScale = evolutionUiStyle_.selectedScale;
        } else if (hovered) {
            stateScale = evolutionUiStyle_.hoverScale;
        }
        const cg2::Vector2 baseSize = i == 0 ? evolutionUiStyle_.currentNodeSize : evolutionUiStyle_.candidateNodeSize;
        const cg2::Vector2 drawSize = {(std::max)(1.0f, baseSize.x * stateScale), (std::max)(1.0f, baseSize.y * stateScale)};
        staticEvolutionNodeDrawSizesVirtual_[i] = drawSize;

        const float cut = (std::clamp)(evolutionUiStyle_.nodeCornerCut, 0.0f, drawSize.y * 0.30f);
        cg2::Vector4 panelColor = evolutionUiStyle_.panelColor;
        panelColor.w = locked ? 0.97f : 0.91f;
        const std::array<cg2::Vector2, 3> panelPositions = {
            {staticEvolutionNodeCentersVirtual_[i],
             {staticEvolutionNodeCentersVirtual_[i].x, staticEvolutionNodeCentersVirtual_[i].y - drawSize.y * 0.5f + cut * 0.5f},
             {staticEvolutionNodeCentersVirtual_[i].x, staticEvolutionNodeCentersVirtual_[i].y + drawSize.y * 0.5f - cut * 0.5f}}};
        const std::array<cg2::Vector2, 3> panelSizes = {{{drawSize.x, (std::max)(1.0f, drawSize.y - cut * 2.0f)},
                                                         {(std::max)(1.0f, drawSize.x - cut * 2.0f), cut},
                                                         {(std::max)(1.0f, drawSize.x - cut * 2.0f), cut}}};
        for (int panelIndex = 0; panelIndex < 3; ++panelIndex) {
            cg2::Sprite* panel = staticEvolutionNodePanelSprites_[i][panelIndex].get();
            panel->SetPosition(EvolutionVirtualToRender(panelPositions[panelIndex]));
            panel->SetSize({panelSizes[panelIndex].x * renderScale, panelSizes[panelIndex].y * renderScale});
            panel->SetColor(panelColor);
            panel->Update();
        }

        if (staticEvolutionTankButtons_[i] && tankButtonUiStyle_) {
            TankButtonVisualData visualData{};
            if (GetTankButtonVisualData(nodeClassId, visualData)) {
                staticEvolutionTankButtons_[i]->SetVisualData(visualData);
            }
            TankButtonUiStyle renderedButtonStyle = *tankButtonUiStyle_;
            renderedButtonStyle.buttonWidth = drawSize.x * renderScale;
            renderedButtonStyle.buttonHeight = drawSize.y * renderScale;
            renderedButtonStyle.cornerRadius = evolutionUiStyle_.nodeCornerCut * renderScale;
            renderedButtonStyle.borderWidth = evolutionUiStyle_.nodeOutlineWidth * renderScale;
            renderedButtonStyle.glowWidth = evolutionUiStyle_.nodeOutlineGlowWidth * renderScale;
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
            staticEvolutionTankButtons_[i]->SetRank(nodeConfig ? nodeConfig->requiredRank : 1);
            staticEvolutionTankButtons_[i]->SetState(buttonState);
            staticEvolutionTankButtons_[i]->Update(EvolutionVirtualToRender(staticEvolutionNodeCentersVirtual_[i]), renderedButtonStyle);
        }
    }
    if (staticEvolutionButtonBloomEffect_ && tankButtonUiStyle_) {
        cg2::BloomParam bloomParam = staticEvolutionButtonBloomEffect_->GetParam();
        bloomParam.threshold = 0.0f;
        bloomParam.intensity = 1.10f + tankButtonUiStyle_->bloomBoost * 2.5f;
        bloomParam.outlineWidth = 0.0f;
        staticEvolutionButtonBloomEffect_->SetParam(bloomParam);
        staticEvolutionButtonBloomEffect_->Update(0.0f);
    }
    if (staticEvolutionTextEffect_) {
        staticEvolutionTextEffect_->SetStyle(evolutionUiStyle_.neonText);
    }

    const cg2::Vector2 panelCenterVirtual = EvolutionAnchorToVirtual(evolutionUiStyle_.detailPanelAnchor);
    staticEvolutionDetailPanelSprite_->SetPosition(EvolutionVirtualToRender(panelCenterVirtual));
    staticEvolutionDetailPanelSprite_->SetSize(
        {evolutionUiStyle_.detailPanelSize.x * renderScale, evolutionUiStyle_.detailPanelSize.y * renderScale});
    staticEvolutionDetailPanelSprite_->SetColor(evolutionUiStyle_.panelColor);
    staticEvolutionDetailPanelSprite_->Update();

    const cg2::Vector2 buttonCenterVirtual = {panelCenterVirtual.x + evolutionUiStyle_.detailPanelSize.x * 0.5f -
                                                  evolutionUiStyle_.confirmButtonSize.x * 0.5f - 18.0f,
                                              panelCenterVirtual.y - evolutionUiStyle_.detailPanelSize.y * 0.5f + 32.0f};
    staticEvolutionConfirmHovered_ = mouseVirtual.x >= buttonCenterVirtual.x - evolutionUiStyle_.confirmButtonSize.x * 0.5f &&
                                     mouseVirtual.x <= buttonCenterVirtual.x + evolutionUiStyle_.confirmButtonSize.x * 0.5f &&
                                     mouseVirtual.y >= buttonCenterVirtual.y - evolutionUiStyle_.confirmButtonSize.y * 0.5f &&
                                     mouseVirtual.y <= buttonCenterVirtual.y + evolutionUiStyle_.confirmButtonSize.y * 0.5f;
    const std::string& selectedClassId = staticEvolutionCandidateIds_[static_cast<size_t>(evolutionUiStyle_.fixedSelectedCandidate)];
    const bool canConfirm = CanEvolveTo(selectedClassId);
    staticEvolutionConfirmButtonSprite_->SetPosition(EvolutionVirtualToRender(buttonCenterVirtual));
    staticEvolutionConfirmButtonSprite_->SetSize(
        {evolutionUiStyle_.confirmButtonSize.x * renderScale, evolutionUiStyle_.confirmButtonSize.y * renderScale});
    cg2::Vector4 buttonColor = evolutionUiStyle_.panelColor;
    buttonColor.x *= 0.72f;
    buttonColor.y *= 0.72f;
    buttonColor.z *= 0.72f;
    buttonColor.w = 0.98f;
    if (!canConfirm) {
        buttonColor = {buttonColor.x * evolutionUiStyle_.lockedColor.x, buttonColor.y * evolutionUiStyle_.lockedColor.y,
                       buttonColor.z * evolutionUiStyle_.lockedColor.z, buttonColor.w * evolutionUiStyle_.lockedColor.w};
    }
    staticEvolutionConfirmButtonSprite_->SetColor(buttonColor);
    staticEvolutionConfirmButtonSprite_->Update();

    const float buttonCut = 7.0f;
    const float halfButtonW = evolutionUiStyle_.confirmButtonSize.x * 0.5f;
    const float halfButtonH = evolutionUiStyle_.confirmButtonSize.y * 0.5f;
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
        cg2::Sprite* line = staticEvolutionConfirmOutlineSprites_[i].get();
        line->SetPosition(a);
        line->SetRotation(std::atan2(dy, dx));
        line->SetSize({std::sqrt(dx * dx + dy * dy), (std::max)(1.0f, 1.6f * renderScale)});
        cg2::Vector4 outlineColor = !canConfirm                      ? evolutionUiStyle_.lockedColor
                                    : staticEvolutionConfirmHovered_ ? evolutionUiStyle_.hoverColor
                                                                     : evolutionUiStyle_.availableColor;
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

    const bool confirmTriggered = input_ && input_->IsTrigger(input_->GetKey()[DIK_RETURN], input_->GetPreKey()[DIK_RETURN]);
    if (canConfirm && (confirmTriggered || (primaryTriggered && staticEvolutionConfirmHovered_))) {
        TryConfirmEvolutionById(selectedClassId);
    }
}

void Player::UpdateStaticEvolutionCircuit()
{
    const cg2::Vector2 current = staticEvolutionNodeCentersVirtual_[0];
    for (int& count : staticEvolutionCircuitControlPointCounts_) {
        count = 0;
    }
    int pathCount = 0;
    if (evolutionUiStyle_.radialLayout) {
        for (size_t candidateIndex = 0; candidateIndex < staticEvolutionCandidateCount_; ++candidateIndex) {
            staticEvolutionCircuitControlPoints_[candidateIndex] = {
                {current, staticEvolutionNodeCentersVirtual_[candidateIndex + 1], {}, {}}};
            staticEvolutionCircuitControlPointCounts_[candidateIndex] = 2;
        }
        pathCount = static_cast<int>(staticEvolutionCandidateCount_);
        staticEvolutionBranchGlowSprite_->SetSize({0.0f, 0.0f});
        staticEvolutionBranchCoreSprite_->SetSize({0.0f, 0.0f});
    } else {
        const cg2::Vector2 branch = EvolutionAnchorToVirtual(evolutionUiStyle_.branchPointAnchor);
        staticEvolutionCircuitControlPoints_[0] = {{current, branch, {}, {}}};
        staticEvolutionCircuitControlPointCounts_[0] = 2;
        for (size_t candidateIndex = 0; candidateIndex < staticEvolutionCandidateCount_; ++candidateIndex) {
            staticEvolutionCircuitControlPoints_[candidateIndex + 1] = {
                {branch, staticEvolutionNodeCentersVirtual_[candidateIndex + 1], {}, {}}};
            staticEvolutionCircuitControlPointCounts_[candidateIndex + 1] = 2;
        }
        pathCount = static_cast<int>(staticEvolutionCandidateCount_ + 1);
        const float renderScale = GetEvolutionRenderScale();
        const cg2::Vector2 renderBranch = EvolutionVirtualToRender(branch);
        staticEvolutionBranchGlowSprite_->SetPosition(renderBranch);
        staticEvolutionBranchGlowSprite_->SetRotation(0.785398163f);
        staticEvolutionBranchGlowSprite_->SetSize({20.0f * renderScale, 20.0f * renderScale});
        cg2::Vector4 branchGlow = evolutionUiStyle_.selectedColor;
        branchGlow.w = 0.16f;
        staticEvolutionBranchGlowSprite_->SetColor(branchGlow);
        staticEvolutionBranchGlowSprite_->Update();
        staticEvolutionBranchCoreSprite_->SetPosition(renderBranch);
        staticEvolutionBranchCoreSprite_->SetRotation(0.785398163f);
        staticEvolutionBranchCoreSprite_->SetSize({7.0f * renderScale, 7.0f * renderScale});
        cg2::Vector4 branchCore = evolutionUiStyle_.selectedColor;
        branchCore.w = 0.94f;
        staticEvolutionBranchCoreSprite_->SetColor(branchCore);
        staticEvolutionBranchCoreSprite_->Update();
    }

    const int currentRank = GetRankFromLevel(level_);
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
        const bool trunk = !evolutionUiStyle_.radialLayout && pathIndex == 0;
        const int candidateIndex = evolutionUiStyle_.radialLayout ? pathIndex : pathIndex - 1;
        const bool selected = trunk || candidateIndex == evolutionUiStyle_.fixedSelectedCandidate;
        const bool hovered = !trunk && candidateIndex + 1 == staticEvolutionHoveredNode_;
        const PlayerClassConfig* config = trunk || candidateIndex < 0 || candidateIndex >= static_cast<int>(staticEvolutionCandidateCount_)
                                              ? nullptr
                                              : GetClassConfig(staticEvolutionCandidateIds_[static_cast<size_t>(candidateIndex)]);
        const bool locked = config && currentRank < config->requiredRank;
        cg2::Vector4 routeColor = evolutionUiStyle_.availableColor;
        float brightness = 0.38f;
        if (locked) {
            routeColor = evolutionUiStyle_.lockedColor;
            brightness = 0.14f;
        } else if (selected) {
            routeColor = evolutionUiStyle_.selectedColor;
            brightness = 1.0f;
        } else if (hovered) {
            routeColor = evolutionUiStyle_.hoverColor;
            brightness = 0.84f;
        }

        const int count = staticEvolutionCircuitControlPointCounts_[pathIndex];
        for (int pointIndex = 0; pointIndex + 1 < count; ++pointIndex) {
            const cg2::Vector2 a = staticEvolutionCircuitControlPoints_[pathIndex][pointIndex];
            const cg2::Vector2 b = staticEvolutionCircuitControlPoints_[pathIndex][pointIndex + 1];
            cg2::Vector4 outer = routeColor;
            cg2::Vector4 middle = routeColor;
            cg2::Vector4 core = routeColor;
            const float opacity = evolutionUiStyle_.circuitOpacity * brightness;
            outer.w = opacity * evolutionUiStyle_.circuitOuterAlpha;
            middle.w = opacity * evolutionUiStyle_.circuitMiddleAlpha;
            core.w = opacity * evolutionUiStyle_.circuitCoreAlpha;
            if (spriteIndex + 2 < staticEvolutionCircuitSprites_.size()) {
                setLine(staticEvolutionCircuitSprites_[spriteIndex++].get(), a, b, evolutionUiStyle_.circuitOuterGlowWidth, outer);
                setLine(staticEvolutionCircuitSprites_[spriteIndex++].get(), a, b, evolutionUiStyle_.circuitMiddleGlowWidth, middle);
                setLine(staticEvolutionCircuitSprites_[spriteIndex++].get(), a, b, evolutionUiStyle_.circuitCoreWidth, core);
            }
        }
    }
    while (spriteIndex < staticEvolutionCircuitSprites_.size()) {
        cg2::Sprite* sprite = staticEvolutionCircuitSprites_[spriteIndex++].get();
        sprite->SetSize({0.0f, 0.0f});
        sprite->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
        sprite->Update();
    }
}

void Player::UpdateStaticEvolutionNodeFrames()
{
    const int currentRank = GetRankFromLevel(level_);
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

    const int activeNodeCount = static_cast<int>(staticEvolutionCandidateCount_ + 1);
    for (int nodeIndex = 0; nodeIndex < activeNodeCount; ++nodeIndex) {
        const bool selected = nodeIndex > 0 && nodeIndex - 1 == evolutionUiStyle_.fixedSelectedCandidate;
        const bool hovered = nodeIndex == staticEvolutionHoveredNode_;
        const PlayerClassConfig* config =
            nodeIndex > 0 ? GetClassConfig(staticEvolutionCandidateIds_[static_cast<size_t>(nodeIndex - 1)]) : nullptr;
        const bool locked = config && currentRank < config->requiredRank;
        cg2::Vector4 stateColor = nodeIndex == 0 ? evolutionUiStyle_.normalColor : evolutionUiStyle_.availableColor;
        float glowAlpha = nodeIndex == 0 ? 0.035f : 0.050f;
        float middleAlpha = nodeIndex == 0 ? 0.12f : 0.18f;
        float coreAlpha = nodeIndex == 0 ? 0.58f : 0.72f;
        if (locked) {
            stateColor = evolutionUiStyle_.lockedColor;
            glowAlpha = 0.015f;
            middleAlpha = 0.06f;
            coreAlpha = 0.34f;
        } else if (selected) {
            stateColor = evolutionUiStyle_.selectedColor;
            glowAlpha = 0.18f;
            middleAlpha = 0.42f;
            coreAlpha = 1.0f;
        } else if (hovered) {
            stateColor = evolutionUiStyle_.hoverColor;
            glowAlpha = 0.14f;
            middleAlpha = 0.34f;
            coreAlpha = 0.94f;
        }

        const cg2::Vector2 center = staticEvolutionNodeCentersVirtual_[nodeIndex];
        const cg2::Vector2 size = staticEvolutionNodeDrawSizesVirtual_[nodeIndex];
        const float halfW = size.x * 0.5f;
        const float halfH = size.y * 0.5f;
        const float cut = (std::clamp)(evolutionUiStyle_.nodeCornerCut, 0.0f, halfH * 0.60f);
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
            setLine(staticEvolutionNodeFrameSprites_[nodeIndex][spriteIndex].get(), a, b, evolutionUiStyle_.nodeOutlineGlowWidth, outer);
            setLine(staticEvolutionNodeFrameSprites_[nodeIndex][spriteIndex + 1].get(), a, b, evolutionUiStyle_.nodeOutlineWidth * 2.2f,
                    middle);
            setLine(staticEvolutionNodeFrameSprites_[nodeIndex][spriteIndex + 2].get(), a, b, evolutionUiStyle_.nodeOutlineWidth, core);
        }
    }
}

void Player::UpdateStaticEvolutionSilhouettes()
{
    /// @brief 機体シルエットを描く1本の線分を表す。
    struct SilhouetteSegment {
        cg2::Vector2 a{};
        cg2::Vector2 b{};
    };
    const int currentRank = GetRankFromLevel(level_);
    const float renderScale = GetEvolutionRenderScale();
    constexpr float kTwoPi = 6.283185307f;

    const int activeNodeCount = static_cast<int>(staticEvolutionCandidateCount_ + 1);
    for (int nodeIndex = 0; nodeIndex < activeNodeCount; ++nodeIndex) {
        const std::string& nodeClassId =
            nodeIndex == 0 ? currentClassId_ : staticEvolutionCandidateIds_[static_cast<size_t>(nodeIndex - 1)];
        const PlayerClassConfig* config = GetClassConfig(nodeClassId);
        std::array<SilhouetteSegment, kStaticEvolutionSilhouetteSpriteCount> segments{};
        size_t segmentCount = 0;
        auto addSegment = [&](const cg2::Vector2& a, const cg2::Vector2& b) {
            if (segmentCount < segments.size()) {
                segments[segmentCount++] = {a, b};
            }
        };
        if (config) {
            const bool selected = nodeIndex > 0 && nodeIndex - 1 == evolutionUiStyle_.fixedSelectedCandidate;
            const bool hovered = nodeIndex == staticEvolutionHoveredNode_;
            const PlayerClassConfig* candidateConfig = nodeIndex > 0 ? config : nullptr;
            const bool locked = candidateConfig && currentRank < candidateConfig->requiredRank;
            float stateScale = evolutionUiStyle_.normalScale;
            if (selected) {
                stateScale = evolutionUiStyle_.selectedScale;
            } else if (hovered) {
                stateScale = evolutionUiStyle_.hoverScale;
            }
            const cg2::Vector2 nodeSize = staticEvolutionNodeDrawSizesVirtual_[nodeIndex];
            const cg2::Vector2 center = {staticEvolutionNodeCentersVirtual_[nodeIndex].x - nodeSize.x * 0.30f,
                                         staticEvolutionNodeCentersVirtual_[nodeIndex].y - 2.0f};
            const float silhouetteScale = evolutionUiStyle_.silhouetteScale * stateScale;
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

            cg2::Vector4 silhouetteColor = selected ? evolutionUiStyle_.selectedColor : evolutionUiStyle_.classTextColor;
            if (hovered && !selected) {
                silhouetteColor = evolutionUiStyle_.hoverColor;
            }
            silhouetteColor.w = locked ? 0.30f : selected ? 0.94f : 0.72f;
            for (size_t i = 0; i < segmentCount; ++i) {
                const cg2::Vector2 renderA = EvolutionVirtualToRender(segments[i].a);
                const cg2::Vector2 renderB = EvolutionVirtualToRender(segments[i].b);
                const float dx = renderB.x - renderA.x;
                const float dy = renderB.y - renderA.y;
                cg2::Sprite* line = staticEvolutionSilhouetteSprites_[nodeIndex][i].get();
                line->SetPosition(renderA);
                line->SetRotation(std::atan2(dy, dx));
                line->SetSize({std::sqrt(dx * dx + dy * dy), (std::max)(0.75f, 1.55f * renderScale)});
                line->SetColor(silhouetteColor);
                line->Update();
            }
        }
        while (segmentCount < staticEvolutionSilhouetteSprites_[nodeIndex].size()) {
            cg2::Sprite* line = staticEvolutionSilhouetteSprites_[nodeIndex][segmentCount++].get();
            line->SetSize({0.0f, 0.0f});
            line->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
            line->Update();
        }
    }
}

void Player::UpdateStaticEvolutionText()
{
    const float renderScale = GetEvolutionRenderScale();
    const float safe = evolutionUiStyle_.safeMargin;
    const auto makeStyle = [&](float size, const cg2::Vector4& color, float outlineWidth) {
        cg2::TextStyle style{};
        style.fontFamily = evolutionUiStyle_.fontFamily;
        style.fontPath = evolutionUiStyle_.fontPath;
        style.fontWeight = evolutionUiStyle_.fontWeight;
        style.fontSize = (std::max)(8.0f, size * renderScale);
        style.color = color;
        style.outlineColor = evolutionUiStyle_.textOutlineColor;
        style.outlineThickness = (std::max)(0.0f, outlineWidth * renderScale);
        style.padding = 6.0f * renderScale;
        return style;
    };

    const cg2::TextStyle titleStyle =
        makeStyle(evolutionUiStyle_.titleFontSize, evolutionUiStyle_.titleTextColor, evolutionUiStyle_.titleOutlineWidth);
    const cg2::TextStyle classNameStyle =
        makeStyle(evolutionUiStyle_.classNameFontSize, evolutionUiStyle_.classTextColor, evolutionUiStyle_.classNameOutlineWidth);
    const cg2::TextStyle bodyStyle =
        makeStyle(evolutionUiStyle_.bodyFontSize, evolutionUiStyle_.bodyTextColor, evolutionUiStyle_.bodyOutlineWidth);
    const cg2::TextStyle buttonStyle =
        makeStyle(evolutionUiStyle_.buttonFontSize, evolutionUiStyle_.buttonTextColor, evolutionUiStyle_.buttonOutlineWidth);
    cg2::SpriteCommon* spriteCommon = cg2::SpriteCommon::GetInstance();

    SetLabel(staticEvolutionTitleLabel_, spriteCommon,
             "EVOLUTION CIRCUIT // RANK " + std::to_string(GetCurrentClassConfig() ? GetCurrentClassConfig()->requiredRank : 1) + " TO " +
                 std::to_string(GetCurrentClassConfig() ? GetCurrentClassConfig()->requiredRank + 1 : 2),
             EvolutionVirtualToRender({safe, safe * 0.62f}), titleStyle);
    SetLabel(staticEvolutionPrototypeLabel_, spriteCommon, "INTERACTION DEBUG",
             EvolutionVirtualToRender({evolutionUiStyle_.virtualResolution.x - safe, safe * 0.72f}), bodyStyle);
    staticEvolutionPrototypeLabel_->SetAnchorPoint({1.0f, 0.0f});

    const int currentRank = GetRankFromLevel(level_);
    const int activeNodeCount = static_cast<int>(staticEvolutionCandidateCount_ + 1);
    for (int i = 0; i < activeNodeCount; ++i) {
        const bool selected = i > 0 && i - 1 == evolutionUiStyle_.fixedSelectedCandidate;
        const bool hovered = i == staticEvolutionHoveredNode_;
        const std::string& nodeClassId = i == 0 ? currentClassId_ : staticEvolutionCandidateIds_[static_cast<size_t>(i - 1)];
        const PlayerClassConfig* config = GetClassConfig(nodeClassId);
        const bool locked = i > 0 && config && currentRank < config->requiredRank;
        cg2::Vector4 textColor = evolutionUiStyle_.classTextColor;
        if (locked) {
            textColor = evolutionUiStyle_.lockedColor;
            textColor.w = 1.0f;
        } else if (selected) {
            textColor = evolutionUiStyle_.selectedColor;
        } else if (hovered) {
            textColor = evolutionUiStyle_.hoverColor;
        }
        cg2::TextStyle nodeClassStyle = makeStyle(evolutionUiStyle_.classNameFontSize, textColor, evolutionUiStyle_.classNameOutlineWidth);
        cg2::Vector4 roleColor = evolutionUiStyle_.bodyTextColor;
        roleColor.w = locked ? 0.42f : 0.72f;
        cg2::TextStyle nodeRoleStyle = makeStyle(evolutionUiStyle_.bodyFontSize * 0.78f, roleColor, evolutionUiStyle_.bodyOutlineWidth);
        const cg2::Vector2 center = staticEvolutionNodeCentersVirtual_[i];
        const float textX = center.x + 34.0f;
        SetLabel(staticEvolutionNodeNameLabels_[i], spriteCommon, GetEvolutionClassName(nodeClassId),
                 EvolutionVirtualToRender({textX, center.y - 12.0f}), nodeClassStyle);
        staticEvolutionNodeNameLabels_[i]->SetAnchorPoint({0.5f, 0.5f});
        SetLabel(staticEvolutionNodeRankLabels_[i], spriteCommon,
                 i == 0   ? "CURRENT CLASS"
                 : config ? GetEvolutionShortRole(*config)
                          : "UNKNOWN",
                 EvolutionVirtualToRender({textX, center.y + 18.0f}), nodeRoleStyle);
        staticEvolutionNodeRankLabels_[i]->SetAnchorPoint({0.5f, 0.5f});
    }

    const std::string& selectedClassId = staticEvolutionCandidateIds_[static_cast<size_t>(evolutionUiStyle_.fixedSelectedCandidate)];
    const PlayerClassConfig* selected = GetClassConfig(selectedClassId);
    const PlayerClassConfig* current = GetCurrentClassConfig();
    if (!selected || !current) {
        return;
    }
    const std::array<std::string, 3> deltas = GetEvolutionDeltas(*current, *selected);
    const cg2::Vector2 panelCenter = EvolutionAnchorToVirtual(evolutionUiStyle_.detailPanelAnchor);
    const cg2::Vector2 panelTopLeft = {panelCenter.x - evolutionUiStyle_.detailPanelSize.x * 0.5f,
                                       panelCenter.y - evolutionUiStyle_.detailPanelSize.y * 0.5f};
    SetLabel(staticEvolutionDetailClassLabel_, spriteCommon, GetEvolutionClassName(selectedClassId),
             EvolutionVirtualToRender({panelTopLeft.x + 24.0f, panelTopLeft.y + 13.0f}), classNameStyle);
    SetLabel(staticEvolutionRoleLabel_, spriteCommon, GetEvolutionRole(*selected),
             EvolutionVirtualToRender({panelTopLeft.x + 150.0f, panelTopLeft.y + 18.0f}), bodyStyle);
    for (int i = 0; i < 3; ++i) {
        SetLabel(staticEvolutionDeltaLabels_[i], spriteCommon, deltas[i],
                 EvolutionVirtualToRender({panelTopLeft.x + 24.0f + static_cast<float>(i) * 276.0f, panelTopLeft.y + 76.0f}), bodyStyle);
    }
    SetLabel(staticEvolutionAbilityLabel_, spriteCommon, GetEvolutionAbility(*selected),
             EvolutionVirtualToRender({panelTopLeft.x + 400.0f, panelTopLeft.y + 18.0f}), bodyStyle);
    const cg2::Vector2 buttonCenter = {panelCenter.x + evolutionUiStyle_.detailPanelSize.x * 0.5f -
                                           evolutionUiStyle_.confirmButtonSize.x * 0.5f - 18.0f,
                                       panelCenter.y - evolutionUiStyle_.detailPanelSize.y * 0.5f + 32.0f};
    SetLabel(staticEvolutionConfirmLabel_, spriteCommon, CanEvolveTo(selectedClassId) ? "ENTER  進化決定" : "RANK不足",
             EvolutionVirtualToRender(buttonCenter), buttonStyle);
    staticEvolutionConfirmLabel_->SetAnchorPoint({0.5f, 0.5f});
    SetLabel(staticEvolutionPanelHintLabel_, spriteCommon, "ESC  戻る  /  ENTER  決定",
             EvolutionVirtualToRender({panelTopLeft.x + evolutionUiStyle_.detailPanelSize.x - 278.0f, panelTopLeft.y + 94.0f}), bodyStyle);
}

void Player::PrepareStaticEvolutionTextTextures()
{
    auto prepare = [](cg2::TextLabel* label) {
        if (label) {
            label->PrepareForDraw();
        }
    };

    prepare(staticEvolutionTitleLabel_.get());
    prepare(staticEvolutionPrototypeLabel_.get());
    prepare(staticEvolutionDetailClassLabel_.get());
    prepare(staticEvolutionRoleLabel_.get());
    for (const auto& label : staticEvolutionDeltaLabels_) {
        prepare(label.get());
    }
    prepare(staticEvolutionAbilityLabel_.get());
    prepare(staticEvolutionConfirmLabel_.get());
    prepare(staticEvolutionPanelHintLabel_.get());

    for (const auto& label : staticEvolutionNodeNameLabels_) {
        prepare(label.get());
    }
    for (const auto& label : staticEvolutionNodeRankLabels_) {
        prepare(label.get());
    }
    for (size_t i = 0; i < staticEvolutionCandidateCount_ + 1; ++i) {
        if (staticEvolutionTankButtons_[i]) {
            prepare(staticEvolutionTankButtons_[i]->GetLabel());
        }
    }
}

bool Player::LoadEvolutionUiStyle(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        evolutionUiStyleStatus_ = "進化UI設定が見つからないため初期値を使用します。";
        return false;
    }

    nlohmann::json root{};
    try {
        file >> root;
    }
    catch (...) {
        evolutionUiStyleStatus_ = "進化UI設定JSONの読み込みに失敗しました。";
        return false;
    }
    if (!root.is_object()) {
        return false;
    }

    evolutionUiStyle_.enabled = root.value("enabled", evolutionUiStyle_.enabled);
    evolutionUiStyle_.virtualResolution =
        ReadVector2Object(root.value("virtualResolution", nlohmann::json::object()), evolutionUiStyle_.virtualResolution);
    evolutionUiStyle_.safeMargin = root.value("safeMargin", evolutionUiStyle_.safeMargin);
    if (root.contains("layout") && root["layout"].is_object()) {
        const nlohmann::json& layout = root["layout"];
        evolutionUiStyle_.radialLayout = layout.value("mode", std::string("leftToRight")) == "radial";
        evolutionUiStyle_.branchPointAnchor =
            ReadVector2Object(layout.value("branchPoint", nlohmann::json::object()), evolutionUiStyle_.branchPointAnchor);
        if (layout.contains("radialNodes") && layout["radialNodes"].is_object()) {
            const nlohmann::json& radialNodes = layout["radialNodes"];
            const std::array<const char*, 4> keys = {"Basic", "Twin", "MachineGun", "Overseer"};
            for (int i = 0; i < 4; ++i) {
                evolutionUiStyle_.radialNodeAnchors[i] =
                    ReadVector2Object(radialNodes.value(keys[i], nlohmann::json::object()), evolutionUiStyle_.radialNodeAnchors[i]);
            }
        }
    }
    if (root.contains("nodes") && root["nodes"].is_object()) {
        const nlohmann::json& nodes = root["nodes"];
        const std::array<const char*, 4> keys = {"Basic", "Twin", "MachineGun", "Overseer"};
        for (int i = 0; i < 4; ++i) {
            evolutionUiStyle_.nodeAnchors[i] =
                ReadVector2Object(nodes.value(keys[i], nlohmann::json::object()), evolutionUiStyle_.nodeAnchors[i]);
        }
        evolutionUiStyle_.currentNodeSize =
            ReadVector2Object(nodes.value("currentSize", nlohmann::json::object()), evolutionUiStyle_.currentNodeSize);
        evolutionUiStyle_.candidateNodeSize =
            ReadVector2Object(nodes.value("candidateSize", nlohmann::json::object()), evolutionUiStyle_.candidateNodeSize);
        evolutionUiStyle_.nodeCornerCut = nodes.value("cornerCut", evolutionUiStyle_.nodeCornerCut);
        evolutionUiStyle_.nodeOutlineGlowWidth = nodes.value("outlineGlowWidth", evolutionUiStyle_.nodeOutlineGlowWidth);
        evolutionUiStyle_.nodeOutlineWidth = nodes.value("outlineWidth", evolutionUiStyle_.nodeOutlineWidth);
        evolutionUiStyle_.silhouetteScale = nodes.value("silhouetteScale", evolutionUiStyle_.silhouetteScale);
        if (nodes.contains("scale") && nodes["scale"].is_object()) {
            const nlohmann::json& scale = nodes["scale"];
            evolutionUiStyle_.normalScale = scale.value("normal", evolutionUiStyle_.normalScale);
            evolutionUiStyle_.hoverScale = scale.value("hover", evolutionUiStyle_.hoverScale);
            evolutionUiStyle_.selectedScale = scale.value("selected", evolutionUiStyle_.selectedScale);
        }
    }
    if (root.contains("circuit") && root["circuit"].is_object()) {
        const nlohmann::json& circuit = root["circuit"];
        evolutionUiStyle_.circuitOuterGlowWidth = circuit.value("outerGlowWidth", evolutionUiStyle_.circuitOuterGlowWidth);
        evolutionUiStyle_.circuitMiddleGlowWidth = circuit.value("middleGlowWidth", evolutionUiStyle_.circuitMiddleGlowWidth);
        evolutionUiStyle_.circuitCoreWidth = circuit.value("coreWidth", evolutionUiStyle_.circuitCoreWidth);
        evolutionUiStyle_.circuitOpacity = circuit.value("opacity", evolutionUiStyle_.circuitOpacity);
        evolutionUiStyle_.circuitOuterAlpha = circuit.value("outerAlpha", evolutionUiStyle_.circuitOuterAlpha);
        evolutionUiStyle_.circuitMiddleAlpha = circuit.value("middleAlpha", evolutionUiStyle_.circuitMiddleAlpha);
        evolutionUiStyle_.circuitCoreAlpha = circuit.value("coreAlpha", evolutionUiStyle_.circuitCoreAlpha);
    }
    evolutionUiStyle_.backgroundDimOpacity = root.value("backgroundDimOpacity", evolutionUiStyle_.backgroundDimOpacity);
    if (root.contains("detailPanel") && root["detailPanel"].is_object()) {
        const nlohmann::json& panel = root["detailPanel"];
        evolutionUiStyle_.detailPanelAnchor =
            ReadVector2Object(panel.value("anchor", nlohmann::json::object()), evolutionUiStyle_.detailPanelAnchor);
        evolutionUiStyle_.detailPanelSize =
            ReadVector2Object(panel.value("size", nlohmann::json::object()), evolutionUiStyle_.detailPanelSize);
    }
    if (root.contains("confirmButton") && root["confirmButton"].is_object()) {
        evolutionUiStyle_.confirmButtonSize =
            ReadVector2Object(root["confirmButton"].value("size", nlohmann::json::object()), evolutionUiStyle_.confirmButtonSize);
    }
    if (root.contains("text") && root["text"].is_object()) {
        const nlohmann::json& text = root["text"];
        evolutionUiStyle_.fontFamily = text.value("fontFamily", evolutionUiStyle_.fontFamily);
        evolutionUiStyle_.fontPath = text.value("fontPath", evolutionUiStyle_.fontPath);
        evolutionUiStyle_.fontWeight = text.value("fontWeight", evolutionUiStyle_.fontWeight);
        evolutionUiStyle_.titleFontSize = text.value("titleSize", evolutionUiStyle_.titleFontSize);
        evolutionUiStyle_.classNameFontSize = text.value("classNameSize", evolutionUiStyle_.classNameFontSize);
        evolutionUiStyle_.bodyFontSize = text.value("bodySize", evolutionUiStyle_.bodyFontSize);
        evolutionUiStyle_.buttonFontSize = text.value("buttonSize", evolutionUiStyle_.buttonFontSize);
        evolutionUiStyle_.titleTextColor = ReadVector4(text.value("titleColor", nlohmann::json::array()), evolutionUiStyle_.titleTextColor);
        evolutionUiStyle_.classTextColor =
            ReadVector4(text.value("classNameColor", nlohmann::json::array()), evolutionUiStyle_.classTextColor);
        evolutionUiStyle_.bodyTextColor = ReadVector4(text.value("bodyColor", nlohmann::json::array()), evolutionUiStyle_.bodyTextColor);
        evolutionUiStyle_.buttonTextColor =
            ReadVector4(text.value("buttonColor", nlohmann::json::array()), evolutionUiStyle_.buttonTextColor);
        evolutionUiStyle_.textOutlineColor =
            ReadVector4(text.value("outlineColor", nlohmann::json::array()), evolutionUiStyle_.textOutlineColor);
        const float legacyOutline = text.value("outlineWidth", evolutionUiStyle_.classNameOutlineWidth);
        evolutionUiStyle_.titleOutlineWidth = text.value("titleOutlineWidth", legacyOutline);
        evolutionUiStyle_.classNameOutlineWidth = text.value("classNameOutlineWidth", legacyOutline);
        evolutionUiStyle_.bodyOutlineWidth = text.value("bodyOutlineWidth", evolutionUiStyle_.bodyOutlineWidth);
        evolutionUiStyle_.buttonOutlineWidth = text.value("buttonOutlineWidth", evolutionUiStyle_.buttonOutlineWidth);
    }
    if (root.contains("neonText") && root["neonText"].is_object()) {
        const nlohmann::json& neon = root["neonText"];
        evolutionUiStyle_.neonText.enabled = neon.value("enabled", evolutionUiStyle_.neonText.enabled);
        evolutionUiStyle_.neonText.glowColor =
            ReadVector4(neon.value("glowColor", nlohmann::json::array()), evolutionUiStyle_.neonText.glowColor);
        evolutionUiStyle_.neonText.sourceBrightness = neon.value("sourceBrightness", evolutionUiStyle_.neonText.sourceBrightness);
        evolutionUiStyle_.neonText.threshold = neon.value("threshold", evolutionUiStyle_.neonText.threshold);
        evolutionUiStyle_.neonText.innerIntensity = neon.value("innerIntensity", evolutionUiStyle_.neonText.innerIntensity);
        evolutionUiStyle_.neonText.outerIntensity = neon.value("outerIntensity", evolutionUiStyle_.neonText.outerIntensity);
    }
    if (root.contains("colors") && root["colors"].is_object()) {
        const nlohmann::json& colors = root["colors"];
        evolutionUiStyle_.normalColor = ReadVector4(colors.value("normal", nlohmann::json::array()), evolutionUiStyle_.normalColor);
        evolutionUiStyle_.availableColor =
            ReadVector4(colors.value("available", nlohmann::json::array()), evolutionUiStyle_.availableColor);
        evolutionUiStyle_.hoverColor = ReadVector4(colors.value("hover", nlohmann::json::array()), evolutionUiStyle_.hoverColor);
        evolutionUiStyle_.selectedColor = ReadVector4(colors.value("selected", nlohmann::json::array()), evolutionUiStyle_.selectedColor);
        evolutionUiStyle_.lockedColor = ReadVector4(colors.value("locked", nlohmann::json::array()), evolutionUiStyle_.lockedColor);
        evolutionUiStyle_.panelColor = ReadVector4(colors.value("panel", nlohmann::json::array()), evolutionUiStyle_.panelColor);
    }
    evolutionUiStyle_.fixedSelectedCandidate = (std::clamp)(root.value("fixedSelectedCandidate", evolutionUiStyle_.fixedSelectedCandidate),
                                                            0, static_cast<int>(kStaticEvolutionMaxCandidates - 1));
    evolutionUiStyleStatus_ = "進化UI設定を読み込みました: " + path;
    return true;
}

bool Player::SaveEvolutionUiStyle(const std::string& path) const
{
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    nlohmann::json root{};
    root["version"] = 2;
    root["enabled"] = evolutionUiStyle_.enabled;
    root["virtualResolution"] = WriteVector2Object(evolutionUiStyle_.virtualResolution);
    root["safeMargin"] = evolutionUiStyle_.safeMargin;
    root["layout"] = {{"mode", evolutionUiStyle_.radialLayout ? "radial" : "leftToRight"},
                      {"branchPoint", WriteVector2Object(evolutionUiStyle_.branchPointAnchor)},
                      {"radialNodes",
                       {{"Basic", WriteVector2Object(evolutionUiStyle_.radialNodeAnchors[0])},
                        {"Twin", WriteVector2Object(evolutionUiStyle_.radialNodeAnchors[1])},
                        {"MachineGun", WriteVector2Object(evolutionUiStyle_.radialNodeAnchors[2])},
                        {"Overseer", WriteVector2Object(evolutionUiStyle_.radialNodeAnchors[3])}}}};
    root["nodes"] = {{"Basic", WriteVector2Object(evolutionUiStyle_.nodeAnchors[0])},
                     {"Twin", WriteVector2Object(evolutionUiStyle_.nodeAnchors[1])},
                     {"MachineGun", WriteVector2Object(evolutionUiStyle_.nodeAnchors[2])},
                     {"Overseer", WriteVector2Object(evolutionUiStyle_.nodeAnchors[3])},
                     {"currentSize", WriteVector2Object(evolutionUiStyle_.currentNodeSize)},
                     {"candidateSize", WriteVector2Object(evolutionUiStyle_.candidateNodeSize)},
                     {"cornerCut", evolutionUiStyle_.nodeCornerCut},
                     {"outlineGlowWidth", evolutionUiStyle_.nodeOutlineGlowWidth},
                     {"outlineWidth", evolutionUiStyle_.nodeOutlineWidth},
                     {"silhouetteScale", evolutionUiStyle_.silhouetteScale},
                     {"scale",
                      {{"normal", evolutionUiStyle_.normalScale},
                       {"hover", evolutionUiStyle_.hoverScale},
                       {"selected", evolutionUiStyle_.selectedScale}}}};
    root["circuit"] = {{"outerGlowWidth", evolutionUiStyle_.circuitOuterGlowWidth},
                       {"middleGlowWidth", evolutionUiStyle_.circuitMiddleGlowWidth},
                       {"coreWidth", evolutionUiStyle_.circuitCoreWidth},
                       {"opacity", evolutionUiStyle_.circuitOpacity},
                       {"outerAlpha", evolutionUiStyle_.circuitOuterAlpha},
                       {"middleAlpha", evolutionUiStyle_.circuitMiddleAlpha},
                       {"coreAlpha", evolutionUiStyle_.circuitCoreAlpha}};
    root["backgroundDimOpacity"] = evolutionUiStyle_.backgroundDimOpacity;
    root["detailPanel"] = {{"anchor", WriteVector2Object(evolutionUiStyle_.detailPanelAnchor)},
                           {"size", WriteVector2Object(evolutionUiStyle_.detailPanelSize)}};
    root["confirmButton"] = {{"size", WriteVector2Object(evolutionUiStyle_.confirmButtonSize)}};
    root["text"] = {{"fontFamily", evolutionUiStyle_.fontFamily},
                    {"fontPath", evolutionUiStyle_.fontPath},
                    {"fontWeight", evolutionUiStyle_.fontWeight},
                    {"titleSize", evolutionUiStyle_.titleFontSize},
                    {"classNameSize", evolutionUiStyle_.classNameFontSize},
                    {"bodySize", evolutionUiStyle_.bodyFontSize},
                    {"buttonSize", evolutionUiStyle_.buttonFontSize},
                    {"titleColor", Vector4ToJson(evolutionUiStyle_.titleTextColor)},
                    {"classNameColor", Vector4ToJson(evolutionUiStyle_.classTextColor)},
                    {"bodyColor", Vector4ToJson(evolutionUiStyle_.bodyTextColor)},
                    {"buttonColor", Vector4ToJson(evolutionUiStyle_.buttonTextColor)},
                    {"outlineColor", Vector4ToJson(evolutionUiStyle_.textOutlineColor)},
                    {"titleOutlineWidth", evolutionUiStyle_.titleOutlineWidth},
                    {"classNameOutlineWidth", evolutionUiStyle_.classNameOutlineWidth},
                    {"bodyOutlineWidth", evolutionUiStyle_.bodyOutlineWidth},
                    {"buttonOutlineWidth", evolutionUiStyle_.buttonOutlineWidth}};
    root["neonText"] = {{"enabled", evolutionUiStyle_.neonText.enabled},
                        {"glowColor", Vector4ToJson(evolutionUiStyle_.neonText.glowColor)},
                        {"sourceBrightness", evolutionUiStyle_.neonText.sourceBrightness},
                        {"threshold", evolutionUiStyle_.neonText.threshold},
                        {"innerIntensity", evolutionUiStyle_.neonText.innerIntensity},
                        {"outerIntensity", evolutionUiStyle_.neonText.outerIntensity}};
    root["colors"] = {
        {"normal", Vector4ToJson(evolutionUiStyle_.normalColor)}, {"available", Vector4ToJson(evolutionUiStyle_.availableColor)},
        {"hover", Vector4ToJson(evolutionUiStyle_.hoverColor)},   {"selected", Vector4ToJson(evolutionUiStyle_.selectedColor)},
        {"locked", Vector4ToJson(evolutionUiStyle_.lockedColor)}, {"panel", Vector4ToJson(evolutionUiStyle_.panelColor)}};
    root["fixedSelectedCandidate"] = evolutionUiStyle_.fixedSelectedCandidate;

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << root.dump(2);
    return true;
}

void Player::DrawEvolutionUiStyleEditor()
{
    if (!arenaUiEnabled_)
        return;
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("進化UI", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    ImGui::Checkbox("新しい進化UIを有効化", &evolutionUiStyle_.enabled);
    RefreshStaticEvolutionCandidates();
    if (staticEvolutionCandidateCount_ > 0) {
        ImGui::SliderInt("選択中の候補", &evolutionUiStyle_.fixedSelectedCandidate, 0,
                         static_cast<int>(staticEvolutionCandidateCount_ - 1));
        ImGui::SameLine();
        ImGui::TextDisabled(
            "%s",
            GetEvolutionClassName(staticEvolutionCandidateIds_[static_cast<size_t>(evolutionUiStyle_.fixedSelectedCandidate)]).c_str());
    }
    if (ImGui::Button("進化UI設定を保存")) {
        evolutionUiStyleStatus_ = SaveEvolutionUiStyle() ? "進化UI設定を保存しました。" : "進化UI設定の保存に失敗しました。";
    }
    ImGui::SameLine();
    if (ImGui::Button("進化UI設定を再読み込み")) {
        LoadEvolutionUiStyle();
    }
    if (!evolutionUiStyleStatus_.empty()) {
        ImGui::TextWrapped("%s", evolutionUiStyleStatus_.c_str());
    }

    const float clientWidth = static_cast<float>(cg2::WinApp::GetInstance()->GetClientWidth());
    const float clientHeight = static_cast<float>(cg2::WinApp::GetInstance()->GetClientHeight());
    const float actualScale = (std::min)(clientWidth / (std::max)(1.0f, evolutionUiStyle_.virtualResolution.x),
                                         clientHeight / (std::max)(1.0f, evolutionUiStyle_.virtualResolution.y));
    ImGui::Text("実解像度: %.0f x %.0f / UI倍率: %.3f", clientWidth, clientHeight, actualScale);

    ImGui::SeparatorText("仮想画面と配置");
    ImGui::DragFloat2("仮想解像度", &evolutionUiStyle_.virtualResolution.x, 1.0f, 320.0f, 7680.0f);
    ImGui::DragFloat("セーフマージン", &evolutionUiStyle_.safeMargin, 1.0f, 0.0f, 360.0f);
    int layoutMode = evolutionUiStyle_.radialLayout ? 1 : 0;
    const char* layoutNames[] = {"左から右へ分岐", "放射型"};
    if (ImGui::Combo("配置モード", &layoutMode, layoutNames, IM_ARRAYSIZE(layoutNames))) {
        evolutionUiStyle_.radialLayout = layoutMode == 1;
    }
    const char* nodeNames[] = {"Basic", "Twin", "MachineGun", "Overseer"};
    for (int i = 0; i < 4; ++i) {
        ImGui::PushID(i);
        ImGui::DragFloat2(evolutionUiStyle_.radialLayout ? "放射型アンカー" : nodeNames[i],
                          evolutionUiStyle_.radialLayout ? &evolutionUiStyle_.radialNodeAnchors[i].x : &evolutionUiStyle_.nodeAnchors[i].x,
                          0.005f, 0.0f, 1.0f);
        ImGui::PopID();
    }
    if (!evolutionUiStyle_.radialLayout) {
        ImGui::DragFloat2("分岐点", &evolutionUiStyle_.branchPointAnchor.x, 0.005f, 0.0f, 1.0f);
    }
    ImGui::DragFloat2("現在ノードサイズ", &evolutionUiStyle_.currentNodeSize.x, 1.0f, 32.0f, 600.0f);
    ImGui::DragFloat2("候補ノードサイズ", &evolutionUiStyle_.candidateNodeSize.x, 1.0f, 32.0f, 600.0f);
    ImGui::DragFloat("通常拡大率", &evolutionUiStyle_.normalScale, 0.005f, 0.5f, 2.0f);
    ImGui::DragFloat("ホバー拡大率", &evolutionUiStyle_.hoverScale, 0.005f, 0.5f, 2.0f);
    ImGui::DragFloat("選択拡大率", &evolutionUiStyle_.selectedScale, 0.005f, 0.5f, 2.0f);
    ImGui::DragFloat("角落とし", &evolutionUiStyle_.nodeCornerCut, 0.25f, 0.0f, 48.0f);
    ImGui::DragFloat("ノード外光幅", &evolutionUiStyle_.nodeOutlineGlowWidth, 0.25f, 0.5f, 40.0f);
    ImGui::DragFloat("ノード輪郭幅", &evolutionUiStyle_.nodeOutlineWidth, 0.1f, 0.5f, 12.0f);
    ImGui::DragFloat("戦車シルエット倍率", &evolutionUiStyle_.silhouetteScale, 0.01f, 0.5f, 2.0f);
    ImGui::DragFloat2("説明パネル位置（正規化）", &evolutionUiStyle_.detailPanelAnchor.x, 0.005f, 0.0f, 1.0f);
    ImGui::DragFloat2("説明パネルサイズ", &evolutionUiStyle_.detailPanelSize.x, 1.0f, 64.0f, 2000.0f);
    ImGui::DragFloat2("決定ボタンサイズ", &evolutionUiStyle_.confirmButtonSize.x, 1.0f, 32.0f, 600.0f);

    ImGui::SeparatorText("ネオン回路");
    ImGui::DragFloat("外光幅", &evolutionUiStyle_.circuitOuterGlowWidth, 0.25f, 0.5f, 80.0f);
    ImGui::DragFloat("中光幅", &evolutionUiStyle_.circuitMiddleGlowWidth, 0.25f, 0.5f, 60.0f);
    ImGui::DragFloat("中心線幅", &evolutionUiStyle_.circuitCoreWidth, 0.1f, 0.25f, 24.0f);
    ImGui::SliderFloat("回路全体透明度", &evolutionUiStyle_.circuitOpacity, 0.0f, 1.0f);
    ImGui::SliderFloat("外光透明度", &evolutionUiStyle_.circuitOuterAlpha, 0.0f, 1.0f);
    ImGui::SliderFloat("中光透明度", &evolutionUiStyle_.circuitMiddleAlpha, 0.0f, 1.0f);
    ImGui::SliderFloat("中心線透明度", &evolutionUiStyle_.circuitCoreAlpha, 0.0f, 1.0f);
    ImGui::SliderFloat("背景暗転率", &evolutionUiStyle_.backgroundDimOpacity, 0.0f, 1.0f);

    ImGui::SeparatorText("文字");
    ImGui::Text("Font: %s / weight %d", evolutionUiStyle_.fontFamily.c_str(), evolutionUiStyle_.fontWeight);
    ImGui::TextDisabled("%s", evolutionUiStyle_.fontPath.c_str());
    ImGui::DragFloat("タイトル文字サイズ", &evolutionUiStyle_.titleFontSize, 0.5f, 8.0f, 96.0f);
    ImGui::DragFloat("クラス名文字サイズ", &evolutionUiStyle_.classNameFontSize, 0.5f, 8.0f, 72.0f);
    ImGui::DragFloat("本文文字サイズ", &evolutionUiStyle_.bodyFontSize, 0.5f, 8.0f, 48.0f);
    ImGui::DragFloat("ボタン文字サイズ", &evolutionUiStyle_.buttonFontSize, 0.5f, 8.0f, 48.0f);
    ImGui::ColorEdit4("タイトル文字色", &evolutionUiStyle_.titleTextColor.x);
    ImGui::ColorEdit4("クラス名文字色", &evolutionUiStyle_.classTextColor.x);
    ImGui::ColorEdit4("本文文字色", &evolutionUiStyle_.bodyTextColor.x);
    ImGui::ColorEdit4("ボタン文字色", &evolutionUiStyle_.buttonTextColor.x);
    ImGui::ColorEdit4("文字アウトライン色", &evolutionUiStyle_.textOutlineColor.x);
    ImGui::DragFloat("タイトルアウトライン幅", &evolutionUiStyle_.titleOutlineWidth, 0.05f, 0.0f, 4.0f);
    ImGui::DragFloat("クラス名アウトライン幅", &evolutionUiStyle_.classNameOutlineWidth, 0.05f, 0.0f, 4.0f);
    ImGui::DragFloat("本文アウトライン幅", &evolutionUiStyle_.bodyOutlineWidth, 0.05f, 0.0f, 4.0f);
    ImGui::DragFloat("ボタンアウトライン幅", &evolutionUiStyle_.buttonOutlineWidth, 0.05f, 0.0f, 4.0f);
    ImGui::SeparatorText("文字ネオン");
    ImGui::Checkbox("文字ネオンを有効化", &evolutionUiStyle_.neonText.enabled);
    ImGui::ColorEdit4("文字発光色", &evolutionUiStyle_.neonText.glowColor.x);
    ImGui::DragFloat("発光源輝度", &evolutionUiStyle_.neonText.sourceBrightness, 0.02f, 0.0f, 8.0f);
    ImGui::DragFloat("抽出しきい値", &evolutionUiStyle_.neonText.threshold, 0.01f, 0.0f, 4.0f);
    ImGui::DragFloat("内光強度", &evolutionUiStyle_.neonText.innerIntensity, 0.01f, 0.0f, 4.0f);
    ImGui::DragFloat("外光強度", &evolutionUiStyle_.neonText.outerIntensity, 0.01f, 0.0f, 4.0f);

    ImGui::SeparatorText("状態色");
    ImGui::ColorEdit4("通常色", &evolutionUiStyle_.normalColor.x);
    ImGui::ColorEdit4("選択可能色", &evolutionUiStyle_.availableColor.x);
    ImGui::ColorEdit4("ホバー色", &evolutionUiStyle_.hoverColor.x);
    ImGui::ColorEdit4("選択色", &evolutionUiStyle_.selectedColor.x);
    ImGui::ColorEdit4("ロック色", &evolutionUiStyle_.lockedColor.x);
    ImGui::ColorEdit4("パネル色", &evolutionUiStyle_.panelColor.x);

    ImGui::SeparatorText("デバッグ表示");
    ImGui::Checkbox("仮想画面領域", &showEvolutionVirtualBounds_);
    ImGui::Checkbox("セーフエリア", &showEvolutionSafeArea_);
    ImGui::Checkbox("ノード描画領域", &showEvolutionNodeBounds_);
    ImGui::Checkbox("マウス判定領域", &showEvolutionMouseBounds_);
    ImGui::Checkbox("TextLabel領域", &showEvolutionTextBounds_);
    ImGui::Checkbox("画面中央線", &showEvolutionCenterLines_);
    ImGui::Checkbox("接続回路の制御点", &showEvolutionCircuitControlPoints_);
    ImGui::Checkbox("実解像度とUI倍率", &showEvolutionResolutionInfo_);
#endif
}

void Player::DrawEvolutionCircuitPrototype()
{
    evolutionUiProfile_.visible = true;
    cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
    for (const auto& line : evolutionCircuitLineSprites_) {
        if (line && line->GetSize().x > 0.0f && line->GetSize().y > 0.0f && line->GetColor().w > 0.001f) {
            line->Draw();
            ++evolutionUiProfile_.spriteDraws;
        }
    }
    if (evolutionCircuitDetailPanelSprite_) {
        evolutionCircuitDetailPanelSprite_->Draw();
        ++evolutionUiProfile_.spriteDraws;
    }
    for (size_t i = 0; i < evolutionCircuitNodes_.size(); ++i) {
        if (IsEvolutionClassVisible(evolutionCircuitNodes_[i].classId) && evolutionCircuitTankButtons_[i]) {
            evolutionCircuitTankButtons_[i]->Draw();
        }
    }
    if (evolutionCircuitDetailPreview_)
        evolutionCircuitDetailPreview_->Draw();

    cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
    auto drawLabel = [&](const std::unique_ptr<cg2::TextLabel>& label) {
        if (label) {
            label->Draw();
            ++evolutionUiProfile_.textDraws;
        }
    };
    drawLabel(evolutionCircuitTitleLabel_);
    for (const auto& label : evolutionCircuitRankLabels_)
        drawLabel(label);
    drawLabel(evolutionCircuitDetailNameLabel_);
    drawLabel(evolutionCircuitDetailMetaLabel_);
    drawLabel(evolutionCircuitDetailRoleLabel_);
    for (const auto& label : evolutionCircuitDetailStatLabels_)
        drawLabel(label);
    drawLabel(evolutionCircuitHintLabel_);
}

void Player::DrawEvolutionCircuitAfterPostEffects()
{
    cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
    if (evolutionCircuitBackdropSprite_)
        evolutionCircuitBackdropSprite_->Draw();
    if (staticEvolutionButtonBloomEffect_) {
        staticEvolutionButtonBloomEffect_->BeginCapture();
        cg2::SpriteCommon::GetInstance()->PreDrawForScene(cg2::kNormal);
        for (const auto& line : evolutionCircuitLineSprites_) {
            if (line && line->GetSize().x > 0.0f && line->GetSize().y > 0.0f && line->GetColor().w > 0.001f) {
                line->Draw();
            }
        }
        for (size_t i = 0; i < evolutionCircuitNodes_.size(); ++i) {
            if (IsEvolutionClassVisible(evolutionCircuitNodes_[i].classId) && evolutionCircuitTankButtons_[i]) {
                evolutionCircuitTankButtons_[i]->DrawBloomSource();
            }
        }
        if (evolutionCircuitDetailPreview_)
            evolutionCircuitDetailPreview_->DrawBloomSource();
        staticEvolutionButtonBloomEffect_->EndCaptureBloomOnlyToBackBuffer();
    }
    if (staticEvolutionTextEffect_) {
        std::vector<cg2::TextLabel*> neonLabels;
        neonLabels.reserve(8);
        if (evolutionCircuitTitleLabel_)
            neonLabels.push_back(evolutionCircuitTitleLabel_.get());
        for (const auto& rankLabel : evolutionCircuitRankLabels_) {
            if (rankLabel)
                neonLabels.push_back(rankLabel.get());
        }
        if (evolutionCircuitDetailNameLabel_)
            neonLabels.push_back(evolutionCircuitDetailNameLabel_.get());
        if (evolutionCircuitSelectedNode_ >= 0 &&
            static_cast<size_t>(evolutionCircuitSelectedNode_) < evolutionCircuitTankButtons_.size() &&
            evolutionCircuitTankButtons_[static_cast<size_t>(evolutionCircuitSelectedNode_)]) {
            neonLabels.push_back(evolutionCircuitTankButtons_[static_cast<size_t>(evolutionCircuitSelectedNode_)]->GetLabel());
        }
        staticEvolutionTextEffect_->DrawBloom(neonLabels);
    }
}

void Player::DrawStaticEvolutionPrototype()
{
    evolutionUiProfile_.visible = true;
    const auto totalStart = std::chrono::steady_clock::now();
    const auto spriteStart = totalStart;
    cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
    auto drawSprite = [&](const std::unique_ptr<cg2::Sprite>& sprite) {
        if (sprite && sprite->GetColor().w > 0.001f && sprite->GetSize().x > 0.0f && sprite->GetSize().y > 0.0f) {
            sprite->Draw();
            ++evolutionUiProfile_.spriteDraws;
        }
    };

    for (const auto& line : staticEvolutionCircuitSprites_) {
        drawSprite(line);
    }
    drawSprite(staticEvolutionBranchGlowSprite_);
    drawSprite(staticEvolutionBranchCoreSprite_);
    drawSprite(staticEvolutionDetailPanelSprite_);
    for (size_t i = 0; i < staticEvolutionCandidateCount_ + 1; ++i) {
        if (staticEvolutionTankButtons_[i]) {
            staticEvolutionTankButtons_[i]->Draw();
        }
    }
    drawSprite(staticEvolutionConfirmButtonSprite_);
    for (const auto& line : staticEvolutionConfirmOutlineSprites_) {
        drawSprite(line);
    }
    const auto spriteEnd = std::chrono::steady_clock::now();

    const auto textStart = spriteEnd;
    cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
    auto drawLabel = [&](const std::unique_ptr<cg2::TextLabel>& label) {
        if (label) {
            label->Draw();
            ++evolutionUiProfile_.textDraws;
        }
    };
    drawLabel(staticEvolutionTitleLabel_);
    const bool anyDebugOverlay = showEvolutionVirtualBounds_ || showEvolutionSafeArea_ || showEvolutionNodeBounds_ ||
                                 showEvolutionMouseBounds_ || showEvolutionTextBounds_ || showEvolutionCenterLines_ ||
                                 showEvolutionCircuitControlPoints_ || showEvolutionResolutionInfo_;
    if (anyDebugOverlay) {
        drawLabel(staticEvolutionPrototypeLabel_);
    }
    drawLabel(staticEvolutionDetailClassLabel_);
    drawLabel(staticEvolutionRoleLabel_);
    for (const auto& label : staticEvolutionDeltaLabels_) {
        drawLabel(label);
    }
    drawLabel(staticEvolutionAbilityLabel_);
    drawLabel(staticEvolutionConfirmLabel_);
    drawLabel(staticEvolutionPanelHintLabel_);
    const auto textEnd = std::chrono::steady_clock::now();

    DrawStaticEvolutionDebugOverlay();
    const auto totalEnd = std::chrono::steady_clock::now();
    evolutionUiProfile_.spriteMs = std::chrono::duration<float, std::milli>(spriteEnd - spriteStart).count();
    evolutionUiProfile_.textMs = std::chrono::duration<float, std::milli>(textEnd - textStart).count();
    evolutionUiProfile_.updateMs = 0.0f;
    evolutionUiProfile_.totalMs = std::chrono::duration<float, std::milli>(totalEnd - totalStart).count();
}

void Player::DrawEvolutionAfterPostEffects()
{
    if (!arenaUiEnabled_ || !isChangeMode) {
        return;
    }
    if (ShouldUseEvolutionCircuitPrototype()) {
        DrawEvolutionCircuitAfterPostEffects();
        return;
    }
    if (!ShouldUseStaticEvolutionPrototype())
        return;
    cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
    if (staticEvolutionBackdropSprite_) {
        staticEvolutionBackdropSprite_->Draw();
    }
    if (staticEvolutionButtonBloomEffect_) {
        staticEvolutionButtonBloomEffect_->BeginCapture();
        cg2::SpriteCommon::GetInstance()->PreDrawForScene(cg2::kNormal);
        for (const auto& line : staticEvolutionCircuitSprites_) {
            if (line && line->GetSize().x > 0.0f && line->GetSize().y > 0.0f && line->GetColor().w > 0.001f) {
                line->Draw();
            }
        }
        if (staticEvolutionBranchGlowSprite_ && staticEvolutionBranchGlowSprite_->GetSize().x > 0.0f &&
            staticEvolutionBranchGlowSprite_->GetColor().w > 0.001f) {
            staticEvolutionBranchGlowSprite_->Draw();
        }
        if (staticEvolutionBranchCoreSprite_ && staticEvolutionBranchCoreSprite_->GetSize().x > 0.0f &&
            staticEvolutionBranchCoreSprite_->GetColor().w > 0.001f) {
            staticEvolutionBranchCoreSprite_->Draw();
        }
        for (size_t i = 0; i < staticEvolutionCandidateCount_ + 1; ++i) {
            if (staticEvolutionTankButtons_[i]) {
                staticEvolutionTankButtons_[i]->DrawBloomSource();
            }
        }
        staticEvolutionButtonBloomEffect_->EndCaptureBloomOnlyToBackBuffer();
    }
    if (staticEvolutionTextEffect_) {
        std::vector<cg2::TextLabel*> neonLabels;
        neonLabels.reserve(4);
        if (staticEvolutionTitleLabel_) {
            neonLabels.push_back(staticEvolutionTitleLabel_.get());
        }
        if (staticEvolutionDetailClassLabel_) {
            neonLabels.push_back(staticEvolutionDetailClassLabel_.get());
        }
        if (staticEvolutionCandidateCount_ > 0) {
            const size_t selectedNode = static_cast<size_t>((std::clamp)(evolutionUiStyle_.fixedSelectedCandidate, 0,
                                                                         static_cast<int>(staticEvolutionCandidateCount_ - 1))) +
                                        1;
            if (selectedNode < staticEvolutionTankButtons_.size() && staticEvolutionTankButtons_[selectedNode]) {
                neonLabels.push_back(staticEvolutionTankButtons_[selectedNode]->GetLabel());
            }
        }
        staticEvolutionTextEffect_->DrawBloom(neonLabels);
    }
}

void Player::DrawStaticEvolutionDebugOverlay()
{
#ifdef USE_IMGUI
    if (!showEvolutionVirtualBounds_ && !showEvolutionSafeArea_ && !showEvolutionNodeBounds_ && !showEvolutionMouseBounds_ &&
        !showEvolutionTextBounds_ && !showEvolutionCenterLines_ && !showEvolutionCircuitControlPoints_ && !showEvolutionResolutionInfo_) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const float clientWidth = static_cast<float>(cg2::WinApp::GetInstance()->GetClientWidth());
    const float clientHeight = static_cast<float>(cg2::WinApp::GetInstance()->GetClientHeight());
    const float virtualWidth = (std::max)(1.0f, evolutionUiStyle_.virtualResolution.x);
    const float virtualHeight = (std::max)(1.0f, evolutionUiStyle_.virtualResolution.y);
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

    if (showEvolutionVirtualBounds_) {
        drawList->AddRect(toClient({0.0f, 0.0f}), toClient(evolutionUiStyle_.virtualResolution), IM_COL32(255, 210, 70, 230), 0.0f, 0,
                          2.0f);
    }
    if (showEvolutionSafeArea_) {
        const float safe = evolutionUiStyle_.safeMargin;
        drawList->AddRect(toClient({safe, safe}), toClient({virtualWidth - safe, virtualHeight - safe}), IM_COL32(80, 255, 160, 230), 0.0f,
                          0, 2.0f);
    }
    if (showEvolutionCenterLines_) {
        drawList->AddLine(ImVec2(clientWidth * 0.5f, 0.0f), ImVec2(clientWidth * 0.5f, clientHeight), IM_COL32(255, 80, 180, 180), 1.0f);
        drawList->AddLine(ImVec2(0.0f, clientHeight * 0.5f), ImVec2(clientWidth, clientHeight * 0.5f), IM_COL32(255, 80, 180, 180), 1.0f);
    }
    for (size_t i = 0; i < staticEvolutionCandidateCount_ + 1; ++i) {
        if (showEvolutionNodeBounds_) {
            drawCenteredRect(staticEvolutionNodeCentersVirtual_[i], staticEvolutionNodeDrawSizesVirtual_[i], IM_COL32(75, 190, 255, 230));
        }
        if (showEvolutionMouseBounds_) {
            drawCenteredRect(staticEvolutionNodeCentersVirtual_[i], staticEvolutionNodeHitSizesVirtual_[i], IM_COL32(255, 120, 70, 210));
        }
    }
    if (showEvolutionCircuitControlPoints_) {
        for (int pathIndex = 0; pathIndex < static_cast<int>(staticEvolutionCircuitControlPoints_.size()); ++pathIndex) {
            const int count = staticEvolutionCircuitControlPointCounts_[pathIndex];
            for (int i = 0; i < count; ++i) {
                const ImVec2 p = toClient(staticEvolutionCircuitControlPoints_[pathIndex][i]);
                drawList->AddCircleFilled(p, 4.0f, IM_COL32(255, 235, 90, 240), 12);
                if (i + 1 < count) {
                    drawList->AddLine(p, toClient(staticEvolutionCircuitControlPoints_[pathIndex][i + 1]), IM_COL32(255, 235, 90, 150),
                                      1.0f);
                }
            }
        }
    }
    if (showEvolutionTextBounds_) {
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
        drawTextBounds(staticEvolutionTitleLabel_);
        drawTextBounds(staticEvolutionPrototypeLabel_);
        for (size_t i = 0; i < staticEvolutionCandidateCount_ + 1; ++i) {
            drawTextBounds(staticEvolutionNodeNameLabels_[i]);
            drawTextBounds(staticEvolutionNodeRankLabels_[i]);
        }
        drawTextBounds(staticEvolutionDetailClassLabel_);
        drawTextBounds(staticEvolutionRoleLabel_);
        for (const auto& label : staticEvolutionDeltaLabels_) {
            drawTextBounds(label);
        }
        drawTextBounds(staticEvolutionAbilityLabel_);
        drawTextBounds(staticEvolutionConfirmLabel_);
        drawTextBounds(staticEvolutionPanelHintLabel_);
    }
    if (showEvolutionResolutionInfo_) {
        char buffer[160]{};
        std::snprintf(buffer, sizeof(buffer), "Evolution UI  %.0f x %.0f  scale %.3f  virtual %.0f x %.0f", clientWidth, clientHeight,
                      scale, virtualWidth, virtualHeight);
        drawList->AddText(ImVec2(18.0f, clientHeight - 28.0f), IM_COL32(220, 250, 255, 255), buffer);
    }
#endif
}
