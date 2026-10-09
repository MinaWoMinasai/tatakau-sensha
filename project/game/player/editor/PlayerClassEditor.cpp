#include "game/player/editor/PlayerClassEditor.h"
#include "game/player/ui/PlayerPresentation.h"
#include "Player.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

// 制作画面だけで使う表示定義と説明表示を、この翻訳単位に閉じ込める。
namespace {
/// @brief 編集画面で選択する特殊行動のID・表示名・実装状態を表す。
struct SpecialActionDefinition {
    const char* id;
    const char* displayName;
    bool implemented;
};

/// @brief 編集画面で利用する特殊行動の一覧を返す。表示名と実装状態を同じ表で管理する。
const std::array<SpecialActionDefinition, 4>& SpecialActionDefinitions()
{
    static const std::array<SpecialActionDefinition, 4> definitions = {{{"none", "なし", true},
                                                                        {"perfect_dodge", "ジャスト回避", true},
                                                                        {"saber_counter", "剣カウンター", true},
                                                                        {"charge_beam", "チャージビーム（準備中）", false}}};
    return definitions;
}

#ifdef USE_IMGUI
/// @brief 直前の編集項目にヘルプ印を添え、マウスを重ねたときに説明文を表示する。
void DrawEditorHelp(const char* text)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
#endif

} // namespace

// namespace

void PlayerClassEditor::DrawPlayerClassEditor()
{
#ifdef USE_IMGUI
    static bool hasUnsavedEditorChanges = false;
    ImGui::SetNextWindowSize(ImVec2(760.0f, 780.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(500.0f, 24.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.96f);
    if (!ImGui::Begin("プレイヤー機体データ編集ツール")) {
        ImGui::End();
        return;
    }

    ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "EDITOR MODE");
    ImGui::SameLine();
    ImGui::TextDisabled("コードを変更せずに、機体性能・砲性能・特殊行動を調整する開発補助ツール");
    ImGui::TextWrapped(
        "目的: C++を直接編集してビルドし直す手間を減らし、JSON化したプレイヤー機体データを画面上で確認・編集・保存できるようにする。");
    ImGui::Separator();

    if (player_.classCatalog_.OrderedIds().empty()) {
        player_.LoadPlayerClassConfigs();
    }
    ui_.editorSelectedClassIndex_ =
        (std::clamp)(ui_.editorSelectedClassIndex_, 0, static_cast<int>(player_.classCatalog_.OrderedIds().size()) - 1);
    std::string selectedId = player_.classCatalog_.OrderedIds()[ui_.editorSelectedClassIndex_];
    PlayerClassConfig* config = player_.GetMutableClassConfig(selectedId);
    if (!config) {
        ImGui::Text("機体設定が見つかりません。");
        ImGui::End();
        return;
    }

    bool rebuildBarrels = false;
    bool relayoutBarrels = false;
    bool editedThisFrame = false;
    static std::unordered_map<std::string, PlayerClassConfig> editorBaselineConfigs;
    static std::unordered_map<std::string, std::string> editorBaselineLabels;
    static bool editorBaselineInitialized = false;
    auto refreshEditorBaselines = [&]() {
        editorBaselineConfigs.clear();
        editorBaselineLabels.clear();
        for (const std::string& id : player_.classCatalog_.OrderedIds()) {
            if (const PlayerClassConfig* baselineConfig = player_.GetClassConfig(id)) {
                editorBaselineConfigs[id] = *baselineConfig;
                editorBaselineLabels[id] = "JSON保存時";
            }
        }
        editorBaselineInitialized = true;
    };
    if (!editorBaselineInitialized) {
        refreshEditorBaselines();
    }
    auto makeUniqueId = [this](const std::string& baseId) {
        const std::string prefix = baseId.empty() ? "CustomTank" : baseId;
        if (!player_.classCatalog_.Find(prefix)) {
            return prefix;
        }
        for (int i = 1; i < 1000; ++i) {
            const std::string candidate = prefix + "_" + std::to_string(i);
            if (!player_.classCatalog_.Find(candidate)) {
                return candidate;
            }
        }
        return prefix + "_Copy";
    };

    ImGui::Text("編集中: %s / %s", config->id.c_str(), config->displayName.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled(hasUnsavedEditorChanges ? "保存状態: 未保存の変更あり" : "保存状態: 保存済み");
    ImGui::Text("現在ゲームに反映中: %s", player_.GetCurrentClassName());
    ImGui::Separator();

    if (ImGui::CollapsingHeader("データ操作", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("JSONの機体データを作成・複製・保存・再読み込みします。新しい機体案を試す入口です。");
        if (ImGui::Button("新規作成")) {
            PlayerClassConfig newConfig = player_.CreateDefaultClassConfig(ClassType::Basic);
            newConfig.id = makeUniqueId("CustomTank");
            newConfig.displayName = newConfig.id;
            player_.classCatalog_.InsertOrAssign(newConfig);
            ui_.editorSelectedClassIndex_ = static_cast<int>(player_.classCatalog_.OrderedIds().size()) - 1;
            selectedId = newConfig.id;
            config = player_.GetMutableClassConfig(selectedId);
            editorBaselineConfigs[newConfig.id] = newConfig;
            editorBaselineLabels[newConfig.id] = "新規作成時";
            hasUnsavedEditorChanges = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("複製")) {
            const PlayerClassConfig sourceConfig = *config;
            PlayerClassConfig newConfig = *config;
            newConfig.id = makeUniqueId(config->id + "_Copy");
            newConfig.displayName = newConfig.id;
            player_.classCatalog_.InsertOrAssign(newConfig);
            ui_.editorSelectedClassIndex_ = static_cast<int>(player_.classCatalog_.OrderedIds().size()) - 1;
            selectedId = newConfig.id;
            config = player_.GetMutableClassConfig(selectedId);
            editorBaselineConfigs[newConfig.id] = sourceConfig;
            editorBaselineLabels[newConfig.id] = "複製元: " + sourceConfig.displayName;
            hasUnsavedEditorChanges = true;
        }
        ImGui::SameLine();
        const bool isLegacyId = config->id == ClassTypeToString(config->type);
        if (!isLegacyId && ImGui::Button("削除")) {
            const std::string deletingId = config->id;
            const bool deletingCurrent = player_.currentClassId_ == deletingId;
            player_.classCatalog_.Erase(deletingId);
            editorBaselineConfigs.erase(deletingId);
            editorBaselineLabels.erase(deletingId);
            ui_.editorSelectedClassIndex_ =
                (std::clamp)(ui_.editorSelectedClassIndex_, 0, static_cast<int>(player_.classCatalog_.OrderedIds().size()) - 1);
            if (deletingCurrent) {
                player_.EvolveById("Basic");
            }
            ImGui::End();
            return;
        }
        ImGui::SameLine();
        if (ImGui::Button("JSON保存")) {
            player_.SavePlayerClassConfigs();
            refreshEditorBaselines();
            hasUnsavedEditorChanges = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("JSON再読み込み")) {
            player_.LoadPlayerClassConfigs();
            // Loading replaces the catalog. Keep the index-based editor selection,
            // but acquire its config again before continuing this frame.
            ui_.editorSelectedClassIndex_ =
                (std::clamp)(ui_.editorSelectedClassIndex_, 0, static_cast<int>(player_.classCatalog_.OrderedIds().size()) - 1);
            selectedId = player_.classCatalog_.OrderedIds()[ui_.editorSelectedClassIndex_];
            config = player_.GetMutableClassConfig(selectedId);
            refreshEditorBaselines();
            rebuildBarrels = true;
            hasUnsavedEditorChanges = false;
        }
    }

    if (ImGui::CollapsingHeader("機体づくり変更サマリー", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("目的: プログラムを増やさず、既存機体を元にして新しい機体バリエーションを作るための差分確認です。");
        const auto baselineIt = editorBaselineConfigs.find(config->id);
        if (baselineIt == editorBaselineConfigs.end()) {
            ImGui::TextDisabled("比較元がありません。JSON保存または再読み込み後に比較できます。");
        } else {
            const PlayerClassConfig& baseline = baselineIt->second;
            const std::string sourceLabel = editorBaselineLabels.count(config->id) > 0 ? editorBaselineLabels[config->id] : "比較元";
            ImGui::Text("比較元: %s", sourceLabel.c_str());

            int changeCount = 0;
            auto floatChanged = [](float a, float b) {
                return std::abs(a - b) > 0.0005f;
            };
            auto boolText = [](bool value) {
                return value ? "ON" : "OFF";
            };
            auto weaponTypeName = [](WeaponType type) {
                switch (type) {
                case WeaponType::Projectile:
                    return "Projectile";
                case WeaponType::Laser:
                    return "Laser";
                case WeaponType::Mine:
                    return "Mine";
                case WeaponType::Drone:
                    return "Drone";
                case WeaponType::Melee:
                    return "Melee";
                }
                return "Unknown";
            };
            auto addChange = [&](const char* label, const std::string& before, const std::string& after) {
                ++changeCount;
                ImGui::BulletText("%s: %s -> %s", label, before.c_str(), after.c_str());
            };
            auto addIntChange = [&](const char* label, int before, int after) {
                if (before != after) {
                    addChange(label, std::to_string(before), std::to_string(after));
                }
            };
            auto addFloatChange = [&](const char* label, float before, float after) {
                if (!floatChanged(before, after)) {
                    return;
                }
                char beforeText[32]{};
                char afterText[32]{};
                std::snprintf(beforeText, sizeof(beforeText), "%.2f", before);
                std::snprintf(afterText, sizeof(afterText), "%.2f", after);
                addChange(label, beforeText, afterText);
            };
            auto addBoolChange = [&](const char* label, bool before, bool after) {
                if (before != after) {
                    addChange(label, boolText(before), boolText(after));
                }
            };
            if (baseline.displayName != config->displayName) {
                addChange("表示名", baseline.displayName, config->displayName);
            }
            addIntChange("必要ランク", baseline.requiredRank, config->requiredRank);
            addBoolChange("ドローン機体", baseline.usesDrone, config->usesDrone);
            addIntChange("最大ドローン数", baseline.maxDrones, config->maxDrones);
            addFloatChange("リロード倍率", baseline.reloadScale, config->reloadScale);
            addFloatChange("弾速倍率", baseline.bulletSpeedScale, config->bulletSpeedScale);
            addFloatChange("弾ダメージ倍率", baseline.bulletDamageScale, config->bulletDamageScale);
            addIntChange("同時発射弾数", baseline.bulletCount, config->bulletCount);
            addFloatChange("拡散角度", baseline.spreadAngleDeg, config->spreadAngleDeg);
            addBoolChange("ランダム拡散", baseline.randomSpread, config->randomSpread);
            addBoolChange("反射弾", baseline.reflect, config->reflect);
            addBoolChange("貫通弾", baseline.penetrate, config->penetrate);
            addBoolChange("全砲塔から発射", baseline.fireAllBarrels, config->fireAllBarrels);
            addBoolChange("砲塔を交互発射", baseline.alternateBarrels, config->alternateBarrels);
            addFloatChange("反動", baseline.recoilPower, config->recoilPower);
            if (baseline.specialActionId != config->specialActionId) {
                addChange("特殊行動", baseline.specialActionId, config->specialActionId);
            }
            addFloatChange("特殊行動クールタイム倍率", baseline.specialActionCooldownScale, config->specialActionCooldownScale);
            addFloatChange("特殊行動スタミナ消費", baseline.specialActionStaminaCost, config->specialActionStaminaCost);
            if (baseline.barrels.size() != config->barrels.size()) {
                addChange("砲塔数", std::to_string(baseline.barrels.size()), std::to_string(config->barrels.size()));
            }
            const size_t comparableBarrels = (std::min)(baseline.barrels.size(), config->barrels.size());
            for (size_t i = 0; i < comparableBarrels && i < 4; ++i) {
                const WeaponMountConfig& before = baseline.barrels[i];
                const WeaponMountConfig& after = config->barrels[i];
                const std::string prefix = "武器マウント" + std::to_string(i) + " ";
                if (before.weaponType != after.weaponType) {
                    addChange((prefix + "武器種").c_str(), weaponTypeName(before.weaponType), weaponTypeName(after.weaponType));
                }
                addFloatChange((prefix + "威力倍率").c_str(), before.damageScale, after.damageScale);
                addFloatChange((prefix + "弾速倍率").c_str(), before.projectileSpeedScale, after.projectileSpeedScale);
            }
            if (changeCount == 0) {
                ImGui::TextDisabled("まだ比較元から変わっていません。複製してから弾数や砲塔配置を変えると、ここに変更内容が出ます。");
            } else {
                ImGui::TextColored(ImVec4(0.55f, 1.0f, 0.70f, 1.0f), "変更項目: %d", changeCount);
            }
        }
    }

    if (ImGui::CollapsingHeader("基本情報", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginCombo("編集する機体", config->displayName.c_str())) {
            for (int i = 0; i < static_cast<int>(player_.classCatalog_.OrderedIds().size()); ++i) {
                const std::string& id = player_.classCatalog_.OrderedIds()[i];
                const PlayerClassConfig* itemConfig = player_.GetClassConfig(id);
                const char* label = itemConfig ? itemConfig->displayName.c_str() : id.c_str();
                const bool selected = i == ui_.editorSelectedClassIndex_;
                if (ImGui::Selectable(label, selected)) {
                    ui_.editorSelectedClassIndex_ = i;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        DrawEditorHelp("編集対象の機体データを選びます。変更内容はJSON保存するまでファイルには反映されません。");
        ImGui::Text("機体ID: %s", config->id.c_str());
        DrawEditorHelp("ゲーム内部とJSONで使う識別子です。既存機体はIDを固定し、表示名だけ変える運用が安全です。");

        char nameBuffer[64]{};
        strncpy_s(nameBuffer, config->displayName.c_str(), _TRUNCATE);
        if (ImGui::InputText("表示名", nameBuffer, sizeof(nameBuffer))) {
            config->displayName = nameBuffer;
            editedThisFrame = true;
        }
        DrawEditorHelp("ゲーム内や選択画面で見える機体名です。");
        editedThisFrame |= ImGui::DragInt("必要ランク", &config->requiredRank, 1.0f, 1, 4);
        DrawEditorHelp("プレイヤーがこの機体を解放できるランクです。");
    }

    if (ImGui::CollapsingHeader("ネオン外観", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* bodyShapeNames[] = {"Circle", "Box", "Triangle", "Pentagon"};
        int bodyShapeIndex = static_cast<int>(config->bodyShape);
        if (ImGui::Combo("ボディ形状", &bodyShapeIndex, bodyShapeNames, IM_ARRAYSIZE(bodyShapeNames))) {
            config->bodyShape = static_cast<BodyShape>((std::clamp)(bodyShapeIndex, 0, 3));
            editedThisFrame = true;
        }
        editedThisFrame |= ImGui::DragFloat2("ボディスケール", &config->bodyScale.x, 0.01f, 0.25f, 3.0f);
        editedThisFrame |= ImGui::ColorEdit4("ボディ塗り色", &config->bodyFillColor.x);
        editedThisFrame |= ImGui::ColorEdit4("ボディ枠線色", &config->bodyOutlineColor.x);
        DrawEditorHelp("モデルを増やさず、ネオン枠線描画側の形と色を変更します。");
    }

    if (ImGui::CollapsingHeader("機体性能", ImGuiTreeNodeFlags_DefaultOpen)) {
        editedThisFrame |= ImGui::Checkbox("ドローン機体", &config->usesDrone);
        DrawEditorHelp("有効にすると、この機体はドローンを使用するタイプとして扱われます。");
        editedThisFrame |= ImGui::DragInt("最大ドローン数", &config->maxDrones, 1.0f, 0, 32);
        DrawEditorHelp("同時に扱えるドローンの上限です。");
        editedThisFrame |= ImGui::DragFloat("リロード倍率", &config->reloadScale, 0.01f, 0.05f, 5.0f);
        DrawEditorHelp("射撃間隔にかかる倍率です。小さいほど連射が速くなります。");
        editedThisFrame |= ImGui::DragFloat("反動", &config->recoilPower, 0.001f, 0.0f, 0.5f);
        DrawEditorHelp("射撃時に機体へ加わる押し戻し量です。");
    }

    if (ImGui::CollapsingHeader("砲性能", ImGuiTreeNodeFlags_DefaultOpen)) {
        config->bulletCount = 1;
        ImGui::TextUnformatted("各砲塔の発射数: 1");
        DrawEditorHelp("1回の射撃で出る弾の数です。");
        editedThisFrame |= ImGui::DragFloat("拡散角度", &config->spreadAngleDeg, 0.1f, 0.0f, 180.0f);
        DrawEditorHelp("複数弾を撃つときの広がり角度です。");
        editedThisFrame |= ImGui::Checkbox("全砲塔から発射", &config->fireAllBarrels);
        DrawEditorHelp("有効にすると、登録されている発射可能な砲塔すべてから撃ちます。");
        editedThisFrame |= ImGui::Checkbox("砲塔を交互発射", &config->alternateBarrels);
        DrawEditorHelp("有効にすると、複数砲塔を順番に切り替えて発射します。");
    }

    if (ImGui::CollapsingHeader("弾性能・特殊効果", ImGuiTreeNodeFlags_DefaultOpen)) {
        editedThisFrame |= ImGui::DragFloat("弾速倍率", &config->bulletSpeedScale, 0.01f, 0.05f, 5.0f);
        DrawEditorHelp("基礎弾速にかかる倍率です。大きいほど弾が速く飛びます。");
        editedThisFrame |= ImGui::DragFloat("弾ダメージ倍率", &config->bulletDamageScale, 0.01f, 0.05f, 20.0f);
        DrawEditorHelp("基礎ダメージにかかる倍率です。");
        editedThisFrame |= ImGui::Checkbox("ランダム拡散", &config->randomSpread);
        DrawEditorHelp("有効にすると弾の散り方にランダム性を持たせます。");
        editedThisFrame |= ImGui::Checkbox("反射弾", &config->reflect);
        DrawEditorHelp("有効にすると弾が壁などで反射するタイプになります。");
        editedThisFrame |= ImGui::Checkbox("貫通弾", &config->penetrate);
        DrawEditorHelp("有効にすると弾が敵を貫通するタイプになります。");
    }

    if (ImGui::CollapsingHeader("特殊行動", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& specialActions = SpecialActionDefinitions();
        const SpecialActionDefinition* selectedSpecialAction = &specialActions.front();
        for (const SpecialActionDefinition& definition : specialActions) {
            if (config->specialActionId == definition.id) {
                selectedSpecialAction = &definition;
                break;
            }
        }
        if (ImGui::BeginCombo("特殊行動", selectedSpecialAction->displayName)) {
            for (const SpecialActionDefinition& definition : specialActions) {
                const bool selected = config->specialActionId == definition.id;
                if (ImGui::Selectable(definition.displayName, selected)) {
                    config->specialActionId = definition.id;
                    editedThisFrame = true;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        DrawEditorHelp("右クリックに割り当てる特殊行動です。未実装のものはデータ上の割り当てだけ行えます。");
        editedThisFrame |= ImGui::DragFloat("特殊行動クールタイム倍率", &config->specialActionCooldownScale, 0.01f, 0.05f, 10.0f);
        DrawEditorHelp("特殊行動の再使用時間にかかる倍率です。小さいほど再使用が早くなります。");
        editedThisFrame |= ImGui::DragFloat("特殊行動スタミナ消費", &config->specialActionStaminaCost, 0.05f, 0.0f, 100.0f);
        DrawEditorHelp("特殊行動を使うときに消費するスタミナ量です。");
        if (config->specialActionId == "saber_counter") {
            editedThisFrame |= ImGui::DragFloat("カウンター受付時間", &config->saberCounterWindow, 0.005f, 0.01f, 2.0f);
            editedThisFrame |= ImGui::DragFloat("カウンター威力倍率", &config->saberCounterDamageScale, 0.05f, 0.0f, 20.0f);
            editedThisFrame |= ImGui::DragFloat("カウンター射程倍率", &config->saberCounterRangeScale, 0.01f, 0.1f, 5.0f);
        }
        if (!selectedSpecialAction->implemented) {
            ImGui::TextDisabled("この特殊行動は割り当てのみ対応しています。");
        }
    }

    ImGui::Separator();
    if (ImGui::Button("この機体に切り替え")) {
        player_.EvolveById(config->id);
        rebuildBarrels = false;
        relayoutBarrels = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("現在の機体へ反映")) {
        rebuildBarrels = true;
    }
    ImGui::Separator();
    if (ImGui::CollapsingHeader("砲配置・武器マウント", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("砲塔数: %zu", config->barrels.size());
        ImGui::TextWrapped("機体に取り付ける砲塔の位置、武器種、レーザー・地雷・近接などの個別性能を編集します。");
        if (ImGui::TreeNodeEx("砲塔の自動配置", ImGuiTreeNodeFlags_DefaultOpen)) {
            static int layoutCount = 2;
            static float layoutForward = 0.72f;
            static float layoutSideSpacing = 0.34f;
            static float layoutAngleSpread = 0.0f;
            static float layoutMuzzleForward = 0.95f;
            static float layoutArcRadius = 0.62f;
            static float layoutArcCenterAngle = 0.0f;
            static float layoutArcSweepAngle = 80.0f;
            static float layoutArcRotationScale = 1.0f;
            static bool layoutSnapAngles = true;
            static float layoutSnapStepDeg = 15.0f;
            static cg2::Vector3 layoutScale = {1.25f, 0.24f, 0.24f};
            ImGui::DragInt("配置数", &layoutCount, 1.0f, 1, 12);
            ImGui::DragFloat("前方向位置", &layoutForward, 0.01f, -2.0f, 5.0f);
            ImGui::DragFloat("横間隔", &layoutSideSpacing, 0.01f, 0.0f, 3.0f);
            ImGui::DragFloat("角度広がり", &layoutAngleSpread, 0.1f, -180.0f, 180.0f);
            ImGui::DragFloat("銃口の前方オフセット", &layoutMuzzleForward, 0.01f, -2.0f, 5.0f);
            ImGui::DragFloat("円弧半径", &layoutArcRadius, 0.01f, 0.0f, 3.0f);
            ImGui::DragFloat("円弧中心角", &layoutArcCenterAngle, 0.1f, -180.0f, 180.0f);
            ImGui::DragFloat("円弧の広がり角", &layoutArcSweepAngle, 0.1f, 0.0f, 360.0f);
            ImGui::DragFloat("円弧回転倍率", &layoutArcRotationScale, 0.01f, -2.0f, 2.0f);
            ImGui::Checkbox("角度をスナップ", &layoutSnapAngles);
            ImGui::SameLine();
            ImGui::DragFloat("スナップ角", &layoutSnapStepDeg, 1.0f, 1.0f, 90.0f);
            ImGui::DragFloat3("配置スケール", &layoutScale.x, 0.01f, 0.01f, 10.0f);
            auto snapAngle = [](float angleDeg) {
                if (!layoutSnapAngles || layoutSnapStepDeg <= 0.0f) {
                    return angleDeg;
                }
                return std::round(angleDeg / layoutSnapStepDeg) * layoutSnapStepDeg;
            };
            auto applyCommonMountStyle = [&](WeaponMountConfig& barrel) {
                if (!config->barrels.empty()) {
                    const WeaponMountConfig& source = config->barrels.front();
                    barrel.model = source.model;
                    barrel.fires = source.fires;
                    barrel.weaponType = source.weaponType;
                    barrel.effectColor = source.effectColor;
                    barrel.damageScale = source.damageScale;
                    barrel.projectileSpeedScale = source.projectileSpeedScale;
                }
                barrel.muzzleForward = layoutMuzzleForward;
            };
            auto generateArcLayout = [&]() {
                layoutCount = (std::clamp)(layoutCount, 1, 12);
                constexpr float kDegToRad = 3.1415926535f / 180.0f;
                config->barrels.clear();
                config->barrels.reserve(static_cast<size_t>(layoutCount));
                for (int i = 0; i < layoutCount; ++i) {
                    const float centerIndex = (static_cast<float>(layoutCount) - 1.0f) * 0.5f;
                    const float normalized = layoutCount <= 1 ? 0.0f : (static_cast<float>(i) - centerIndex) / centerIndex;
                    const float angleDeg = snapAngle(layoutArcCenterAngle + normalized * layoutArcSweepAngle * 0.5f);
                    const float angleRad = angleDeg * kDegToRad;
                    WeaponMountConfig barrel{};
                    applyCommonMountStyle(barrel);
                    barrel.offset = {std::cos(angleRad) * layoutArcRadius, std::sin(angleRad) * layoutArcRadius, 0.0f};
                    barrel.scale = layoutScale;
                    barrel.angleDeg = snapAngle((angleDeg - layoutArcCenterAngle) * layoutArcRotationScale + layoutArcCenterAngle);
                    config->barrels.push_back(barrel);
                }
                config->fireAllBarrels = layoutCount > 1;
                config->alternateBarrels = false;
                rebuildBarrels = true;
                editedThisFrame = true;
            };
            auto generateRadialLayout = [&](int count, float radius, float startAngleDeg, float angleOffsetDeg, const cg2::Vector3& scale,
                                            WeaponType forcedType = WeaponType::Projectile) {
                count = (std::clamp)(count, 1, 12);
                constexpr float kDegToRad = 3.1415926535f / 180.0f;
                config->barrels.clear();
                config->barrels.reserve(static_cast<size_t>(count));
                for (int i = 0; i < count; ++i) {
                    const float angleDeg = snapAngle(startAngleDeg + 360.0f * static_cast<float>(i) / static_cast<float>(count));
                    const float angleRad = angleDeg * kDegToRad;
                    WeaponMountConfig barrel{};
                    applyCommonMountStyle(barrel);
                    barrel.offset = {std::cos(angleRad) * radius, std::sin(angleRad) * radius, 0.0f};
                    barrel.scale = scale;
                    barrel.angleDeg = snapAngle(angleDeg + angleOffsetDeg);
                    barrel.weaponType = forcedType;
                    barrel.fires = true;
                    config->barrels.push_back(barrel);
                }
                config->fireAllBarrels = count > 1;
                config->alternateBarrels = false;
                rebuildBarrels = true;
                editedThisFrame = true;
            };
            auto generateFixedAngleLayout = [&](const std::vector<float>& anglesDeg, float radius, const cg2::Vector3& scale, bool fireAll,
                                                bool alternate, WeaponType forcedType = WeaponType::Projectile) {
                constexpr float kDegToRad = 3.1415926535f / 180.0f;
                config->barrels.clear();
                config->barrels.reserve(anglesDeg.size());
                for (float rawAngleDeg : anglesDeg) {
                    const float angleDeg = snapAngle(rawAngleDeg);
                    const float angleRad = angleDeg * kDegToRad;
                    WeaponMountConfig barrel{};
                    applyCommonMountStyle(barrel);
                    barrel.offset = {std::cos(angleRad) * radius, std::sin(angleRad) * radius, 0.0f};
                    barrel.scale = scale;
                    barrel.angleDeg = angleDeg;
                    barrel.weaponType = forcedType;
                    barrel.fires = true;
                    config->barrels.push_back(barrel);
                }
                config->fireAllBarrels = fireAll;
                config->alternateBarrels = alternate;
                rebuildBarrels = true;
                editedThisFrame = true;
            };
            if (ImGui::Button("左右対称配置を生成")) {
                layoutCount = (std::clamp)(layoutCount, 1, 12);
                config->barrels.clear();
                config->barrels.reserve(static_cast<size_t>(layoutCount));
                for (int i = 0; i < layoutCount; ++i) {
                    const float centerIndex = (static_cast<float>(layoutCount) - 1.0f) * 0.5f;
                    const float normalized = layoutCount <= 1 ? 0.0f : (static_cast<float>(i) - centerIndex) / centerIndex;
                    WeaponMountConfig barrel{};
                    applyCommonMountStyle(barrel);
                    barrel.offset = {layoutForward, (static_cast<float>(i) - centerIndex) * layoutSideSpacing, 0.0f};
                    barrel.scale = layoutScale;
                    barrel.angleDeg = snapAngle(normalized * layoutAngleSpread * 0.5f);
                    config->barrels.push_back(barrel);
                }
                config->fireAllBarrels = layoutCount > 2;
                config->alternateBarrels = layoutCount == 2;
                rebuildBarrels = true;
                editedThisFrame = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("V字配置を生成")) {
                layoutCount = (std::clamp)(layoutCount, 2, 12);
                config->barrels.clear();
                config->barrels.reserve(static_cast<size_t>(layoutCount));
                for (int i = 0; i < layoutCount; ++i) {
                    const float centerIndex = (static_cast<float>(layoutCount) - 1.0f) * 0.5f;
                    const float signedIndex = static_cast<float>(i) - centerIndex;
                    WeaponMountConfig barrel{};
                    applyCommonMountStyle(barrel);
                    barrel.offset = {layoutForward - std::abs(signedIndex) * 0.12f, signedIndex * layoutSideSpacing, 0.0f};
                    barrel.scale = layoutScale;
                    barrel.angleDeg = snapAngle(
                        (layoutCount <= 1 || centerIndex == 0.0f) ? 0.0f : (signedIndex / centerIndex) * layoutAngleSpread * 0.5f);
                    config->barrels.push_back(barrel);
                }
                config->fireAllBarrels = true;
                config->alternateBarrels = false;
                rebuildBarrels = true;
                editedThisFrame = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("円弧ファン配置を生成")) {
                generateArcLayout();
            }
            ImGui::SeparatorText("diep.io風プリセット");
            if (ImGui::Button("単砲")) {
                generateFixedAngleLayout({0.0f}, 0.72f, {1.25f, 0.24f, 0.24f}, false, false);
            }
            ImGui::SameLine();
            if (ImGui::Button("ツイン")) {
                layoutCount = 2;
                layoutForward = 0.72f;
                layoutSideSpacing = 0.34f;
                layoutAngleSpread = 0.0f;
                layoutScale = {1.25f, 0.24f, 0.24f};
                config->barrels.clear();
                for (float side : {-0.5f, 0.5f}) {
                    WeaponMountConfig barrel{};
                    applyCommonMountStyle(barrel);
                    barrel.offset = {layoutForward, side * layoutSideSpacing * 2.0f, 0.0f};
                    barrel.scale = layoutScale;
                    barrel.angleDeg = 0.0f;
                    config->barrels.push_back(barrel);
                }
                config->fireAllBarrels = false;
                config->alternateBarrels = true;
                rebuildBarrels = true;
                editedThisFrame = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("大型砲")) {
                generateFixedAngleLayout({0.0f}, 0.74f, {1.58f, 0.42f, 0.42f}, false, false);
                config->reloadScale = (std::max)(config->reloadScale, 1.35f);
                config->bulletDamageScale = (std::max)(config->bulletDamageScale, 2.0f);
                config->recoilPower = (std::max)(config->recoilPower, 0.035f);
            }
            ImGui::SameLine();
            if (ImGui::Button("トラッパー")) {
                generateFixedAngleLayout({0.0f}, 0.64f, {0.72f, 0.46f, 0.46f}, false, false, WeaponType::Mine);
            }
            if (ImGui::Button("前後砲")) {
                generateFixedAngleLayout({0.0f, 180.0f}, 0.70f, {1.18f, 0.22f, 0.22f}, true, false);
            }
            ImGui::SameLine();
            if (ImGui::Button("左右サイド砲")) {
                generateFixedAngleLayout({90.0f, -90.0f}, 0.70f, {1.18f, 0.22f, 0.22f}, true, false);
            }
            ImGui::SameLine();
            if (ImGui::Button("十字4砲")) {
                generateRadialLayout(4, 0.70f, 0.0f, 0.0f, {1.14f, 0.22f, 0.22f});
            }
            ImGui::SameLine();
            if (ImGui::Button("斜め4砲")) {
                generateRadialLayout(4, 0.70f, 45.0f, 0.0f, {1.14f, 0.22f, 0.22f});
            }
            if (ImGui::Button("オクト8砲")) {
                generateRadialLayout(8, 0.68f, 0.0f, 0.0f, {1.04f, 0.18f, 0.18f});
            }
            ImGui::SameLine();
            if (ImGui::Button("前方3連")) {
                layoutCount = 3;
                layoutArcRadius = 0.66f;
                layoutArcCenterAngle = 0.0f;
                layoutArcSweepAngle = 70.0f;
                layoutArcRotationScale = 1.0f;
                layoutScale = {1.25f, 0.24f, 0.24f};
                generateArcLayout();
            }
            ImGui::SameLine();
            if (ImGui::Button("前方5連")) {
                layoutCount = 5;
                layoutArcRadius = 0.66f;
                layoutArcCenterAngle = 0.0f;
                layoutArcSweepAngle = 120.0f;
                layoutArcRotationScale = 0.9f;
                layoutScale = {1.14f, 0.20f, 0.20f};
                generateArcLayout();
            }
            if (ImGui::Button("ツイン前後")) {
                config->barrels.clear();
                for (float angleDeg : {0.0f, 180.0f}) {
                    const float angleRad = angleDeg * 3.1415926535f / 180.0f;
                    const cg2::Vector3 forwardOffset = {std::cos(angleRad) * 0.72f, std::sin(angleRad) * 0.72f, 0.0f};
                    const cg2::Vector3 sideAxis = {-std::sin(angleRad), std::cos(angleRad), 0.0f};
                    for (float side : {-0.22f, 0.22f}) {
                        WeaponMountConfig barrel{};
                        applyCommonMountStyle(barrel);
                        barrel.offset = forwardOffset + sideAxis * side;
                        barrel.scale = {1.12f, 0.20f, 0.20f};
                        barrel.angleDeg = angleDeg;
                        barrel.fireGroup = angleDeg == 0.0f ? 0 : 1;
                        config->barrels.push_back(barrel);
                    }
                }
                config->fireAllBarrels = false;
                config->alternateBarrels = true;
                rebuildBarrels = true;
                editedThisFrame = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("前方+左右")) {
                generateFixedAngleLayout({0.0f, 90.0f, -90.0f}, 0.70f, {1.14f, 0.21f, 0.21f}, true, false);
            }
            if (ImGui::Button("プリセット 3連ファン")) {
                layoutCount = 3;
                layoutArcRadius = 0.62f;
                layoutArcCenterAngle = 0.0f;
                layoutArcSweepAngle = 70.0f;
                layoutArcRotationScale = 1.0f;
                layoutScale = {1.25f, 0.24f, 0.24f};
                generateArcLayout();
            }
            ImGui::SameLine();
            if (ImGui::Button("プリセット 5連ファン")) {
                layoutCount = 5;
                layoutArcRadius = 0.64f;
                layoutArcCenterAngle = 0.0f;
                layoutArcSweepAngle = 120.0f;
                layoutArcRotationScale = 0.85f;
                layoutScale = {1.18f, 0.22f, 0.22f};
                generateArcLayout();
            }
            ImGui::SameLine();
            if (ImGui::Button("プリセット 側面積み")) {
                layoutCount = 4;
                layoutArcRadius = 0.58f;
                layoutArcCenterAngle = 25.0f;
                layoutArcSweepAngle = 90.0f;
                layoutArcRotationScale = 0.65f;
                layoutScale = {1.18f, 0.22f, 0.22f};
                generateArcLayout();
            }
            ImGui::Separator();
            ImGui::TreePop();
        }
        if (ImGui::Button("武器マウントを追加")) {
            WeaponMountConfig barrel{};
            if (!config->barrels.empty()) {
                barrel = config->barrels.back();
                barrel.offset.y += 0.34f;
            }
            config->barrels.push_back(barrel);
            rebuildBarrels = true;
            editedThisFrame = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("左右ペアをグループ化")) {
            for (WeaponMountConfig& barrel : config->barrels) {
                const int angleBucket = static_cast<int>(std::round(barrel.angleDeg / 15.0f));
                barrel.fireGroup = (angleBucket + 12) * 2 + (barrel.offset.y >= 0.0f ? 1 : 0);
            }
            config->fireAllBarrels = false;
            config->alternateBarrels = true;
            editedThisFrame = true;
        }
        if (ImGui::Button("角度ごとにグループ化")) {
            for (WeaponMountConfig& barrel : config->barrels) {
                barrel.fireGroup = static_cast<int>(std::round((barrel.angleDeg + 180.0f) / 15.0f));
            }
            config->fireAllBarrels = false;
            config->alternateBarrels = true;
            editedThisFrame = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("前後を2グループ化")) {
            for (WeaponMountConfig& barrel : config->barrels) {
                const float angle = std::fmod(barrel.angleDeg + 360.0f, 360.0f);
                barrel.fireGroup = (angle > 90.0f && angle < 270.0f) ? 1 : 0;
            }
            config->fireAllBarrels = false;
            config->alternateBarrels = true;
            editedThisFrame = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("全砲を同じグループへ")) {
            for (WeaponMountConfig& barrel : config->barrels) {
                barrel.fireGroup = 0;
            }
            editedThisFrame = true;
        }
        if (ImGui::Button("グループ交互射撃を有効化")) {
            config->fireAllBarrels = false;
            config->alternateBarrels = true;
            editedThisFrame = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("全グループ同時射撃に戻す")) {
            config->fireAllBarrels = true;
            config->alternateBarrels = false;
            editedThisFrame = true;
        }
        if (ImGui::Button("左右ミラーを生成")) {
            std::vector<WeaponMountConfig> mirrored;
            mirrored.reserve(config->barrels.size() * 2);
            for (const WeaponMountConfig& source : config->barrels) {
                if (source.offset.y < -0.001f) {
                    continue;
                }
                WeaponMountConfig left = source;
                left.offset.y = -std::abs(source.offset.y);
                left.angleDeg = -source.angleDeg;
                WeaponMountConfig right = source;
                right.offset.y = std::abs(source.offset.y);
                right.angleDeg = source.angleDeg;
                if (std::abs(source.offset.y) <= 0.001f) {
                    mirrored.push_back(source);
                } else {
                    mirrored.push_back(left);
                    mirrored.push_back(right);
                }
            }
            if (!mirrored.empty()) {
                config->barrels = mirrored;
                rebuildBarrels = true;
                editedThisFrame = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("選択風: 右側を左へ同期")) {
            for (WeaponMountConfig& right : config->barrels) {
                if (right.offset.y <= 0.001f) {
                    continue;
                }
                for (WeaponMountConfig& left : config->barrels) {
                    if (left.offset.y >= -0.001f) {
                        continue;
                    }
                    if (std::abs(std::abs(left.offset.y) - right.offset.y) < 0.05f && std::abs(left.offset.x - right.offset.x) < 0.05f) {
                        left = right;
                        left.offset.y = -right.offset.y;
                        left.angleDeg = -right.angleDeg;
                        break;
                    }
                }
            }
            rebuildBarrels = true;
            editedThisFrame = true;
        }
        {
            std::vector<int> groups;
            for (const WeaponMountConfig& barrel : config->barrels) {
                if (!barrel.fires) {
                    continue;
                }
                const int group = (std::max)(0, barrel.fireGroup);
                if (std::find(groups.begin(), groups.end(), group) == groups.end()) {
                    groups.push_back(group);
                }
            }
            std::sort(groups.begin(), groups.end());
            ImGui::Text("発射グループ数: %zu / モード: %s", groups.size(),
                        (config->alternateBarrels && !config->fireAllBarrels && groups.size() > 1)
                            ? "グループ交互"
                            : (config->fireAllBarrels ? "全砲同時" : "砲塔交互"));
            if (config->id == player_.currentClassId_ && !player_.weaponGroupCooldowns_.empty()) {
                ImGui::Text("実行中グループCD: ");
                for (size_t groupIndex = 0; groupIndex < groups.size() && groupIndex < player_.weaponGroupCooldowns_.size(); ++groupIndex) {
                    ImGui::SameLine();
                    ImGui::Text("[%d %.2f]", groups[groupIndex], player_.weaponGroupCooldowns_[groupIndex]);
                }
            }
        }

        for (size_t i = 0; i < config->barrels.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            WeaponMountConfig& barrel = config->barrels[i];
            const std::string label = "武器マウント " + std::to_string(i);
            if (ImGui::TreeNode(label.c_str())) {
                char modelBuffer[128]{};
                strncpy_s(modelBuffer, barrel.model.c_str(), _TRUNCATE);
                if (ImGui::InputText("モデル", modelBuffer, sizeof(modelBuffer))) {
                    barrel.model = modelBuffer;
                    rebuildBarrels = true;
                    editedThisFrame = true;
                }
                const char* barrelShapeNames[] = {"Box", "Heavy", "Short", "Wide", "Trapezoid"};
                int barrelShapeIndex = static_cast<int>(barrel.barrelShape);
                if (ImGui::Combo("ネオン砲身形状", &barrelShapeIndex, barrelShapeNames, IM_ARRAYSIZE(barrelShapeNames))) {
                    barrel.barrelShape = static_cast<BarrelShape>((std::clamp)(barrelShapeIndex, 0, 4));
                    editedThisFrame = true;
                }
                relayoutBarrels |= ImGui::DragFloat3("位置 X/Y/Z", &barrel.offset.x, 0.01f, -10.0f, 10.0f);
                relayoutBarrels |= ImGui::DragFloat3("スケール", &barrel.scale.x, 0.01f, 0.0f, 10.0f);
                relayoutBarrels |= ImGui::DragFloat("角度", &barrel.angleDeg, 0.1f, -180.0f, 180.0f);
                editedThisFrame |= ImGui::DragFloat("銃口の前方オフセット", &barrel.muzzleForward, 0.01f, -2.0f, 5.0f);
                editedThisFrame |= ImGui::Checkbox("マウントを有効化", &barrel.fires);
                const char* weaponTypeNames[] = {"Projectile", "Laser", "Mine", "Drone (準備中)", "Melee"};
                int weaponTypeIndex = static_cast<int>(barrel.weaponType);
                if (ImGui::Combo("武器種", &weaponTypeIndex, weaponTypeNames, IM_ARRAYSIZE(weaponTypeNames))) {
                    barrel.weaponType = static_cast<WeaponType>((std::clamp)(weaponTypeIndex, 0, 4));
                    editedThisFrame = true;
                }
                if (barrel.weaponType == WeaponType::Laser) {
                    ImGui::DragFloat("レーザー射程", &barrel.laserRange, 0.1f, 0.5f, 80.0f);
                    ImGui::DragFloat("レーザー太さ", &barrel.laserWidth, 0.005f, 0.02f, 2.0f);
                    ImGui::DragFloat("レーザー表示時間", &barrel.laserDuration, 0.005f, 0.01f, 1.0f);
                    ImGui::DragFloat("レーザーダメージ間隔", &barrel.laserDamageInterval, 0.005f, 0.01f, 1.0f);
                } else if (barrel.weaponType == WeaponType::Mine) {
                    ImGui::DragFloat("地雷爆発半径", &barrel.mineRadius, 0.05f, 0.2f, 20.0f);
                    ImGui::DragFloat("地雷起爆待ち", &barrel.mineFuseTime, 0.01f, 0.0f, 5.0f);
                    ImGui::DragFloat("地雷寿命", &barrel.mineLifeTime, 0.05f, 0.2f, 30.0f);
                } else if (barrel.weaponType == WeaponType::Melee) {
                    ImGui::DragFloat("近接射程", &barrel.meleeRange, 0.05f, 0.3f, 12.0f);
                    ImGui::DragFloat("近接角度", &barrel.meleeArcDeg, 0.5f, 5.0f, 360.0f);
                    ImGui::DragFloat("近接線幅", &barrel.meleeWidth, 0.005f, 0.02f, 1.0f);
                    ImGui::DragFloat("近接表示時間", &barrel.meleeDuration, 0.005f, 0.03f, 1.0f);
                    ImGui::DragFloat("コンボリセット時間", &barrel.meleeComboResetTime, 0.01f, 0.05f, 3.0f);
                    ImGui::DragFloat("1段目 ダメージ倍率", &barrel.meleeCombo1DamageScale, 0.01f, 0.0f, 10.0f);
                    ImGui::DragFloat("2段目 ダメージ倍率", &barrel.meleeCombo2DamageScale, 0.01f, 0.0f, 10.0f);
                    ImGui::DragFloat("3段目 ダメージ倍率", &barrel.meleeCombo3DamageScale, 0.01f, 0.0f, 10.0f);
                    ImGui::DragFloat("1段目 射程倍率", &barrel.meleeCombo1RangeScale, 0.01f, 0.05f, 5.0f);
                    ImGui::DragFloat("2段目 射程倍率", &barrel.meleeCombo2RangeScale, 0.01f, 0.05f, 5.0f);
                    ImGui::DragFloat("3段目 射程倍率", &barrel.meleeCombo3RangeScale, 0.01f, 0.05f, 5.0f);
                    ImGui::DragFloat("1段目 予備動作", &barrel.meleeCombo1Windup, 0.005f, 0.0f, 1.5f);
                    ImGui::DragFloat("2段目 予備動作", &barrel.meleeCombo2Windup, 0.005f, 0.0f, 1.5f);
                    ImGui::DragFloat("3段目 予備動作", &barrel.meleeCombo3Windup, 0.005f, 0.0f, 1.5f);
                    ImGui::DragFloat("1段目 後隙", &barrel.meleeCombo1Recovery, 0.005f, 0.0f, 1.5f);
                    ImGui::DragFloat("2段目 後隙", &barrel.meleeCombo2Recovery, 0.005f, 0.0f, 1.5f);
                    ImGui::DragFloat("3段目 後隙", &barrel.meleeCombo3Recovery, 0.005f, 0.0f, 1.5f);
                } else if (barrel.weaponType != WeaponType::Projectile) {
                    ImGui::TextDisabled("この武器種は次の実装段階まで発射されません。");
                }
                editedThisFrame |= ImGui::ColorEdit4("エフェクト色", &barrel.effectColor.x);
                editedThisFrame |= ImGui::ColorEdit4("砲身塗り色", &barrel.barrelColor.x);
                editedThisFrame |= ImGui::ColorEdit4("砲身枠線色", &barrel.outlineColor.x);
                ImGui::DragFloat("マウント威力倍率", &barrel.damageScale, 0.01f, 0.0f, 20.0f);
                ImGui::DragFloat("マウント弾速倍率", &barrel.projectileSpeedScale, 0.01f, 0.01f, 10.0f);
                editedThisFrame |= ImGui::DragInt("発射グループ", &barrel.fireGroup, 1.0f, 0, 64);
                editedThisFrame |= ImGui::DragFloat("個別リロード倍率", &barrel.reloadScale, 0.01f, 0.05f, 10.0f);
                editedThisFrame |= ImGui::DragFloat("個別反動倍率", &barrel.recoilScale, 0.01f, 0.0f, 10.0f);
                if (ImGui::Button("複製")) {
                    config->barrels.insert(config->barrels.begin() + static_cast<std::ptrdiff_t>(i + 1), barrel);
                    rebuildBarrels = true;
                    ImGui::TreePop();
                    ImGui::PopID();
                    break;
                }
                ImGui::SameLine();
                if (ImGui::Button("削除") && config->barrels.size() > 1) {
                    config->barrels.erase(config->barrels.begin() + static_cast<std::ptrdiff_t>(i));
                    rebuildBarrels = true;
                    ImGui::TreePop();
                    ImGui::PopID();
                    break;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    if (config && config->id == player_.currentClassId_ && (rebuildBarrels || relayoutBarrels)) {
        if (rebuildBarrels) {
            player_.InitializeBarrels();
        }
        player_.UpdateBarrelLayout();
    }

    ImGui::Text("メモ: 位置X=前方向、位置Y=横方向、角度=照準からのずれです。");
    ImGui::TextDisabled("WeaponMount v2: 旧 barrels JSONも自動で読み込めます。");
    if (editedThisFrame || relayoutBarrels) {
        hasUnsavedEditorChanges = true;
    }
    ImGui::End();
#endif
}
