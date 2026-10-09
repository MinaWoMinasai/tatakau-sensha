#include "game/editor/session/GameplayEditor.h"
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

void GameplayEditor::DrawGameplayDebugUi()
{
#ifdef USE_IMGUI
    if (!world_.presentation.showGameDebugConsole_) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(620.0f, 540.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("ゲームデバッグコンソール", &world_.presentation.showGameDebugConsole_)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("GameDebugTabs")) {
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
        if (ImGui::BeginTabItem("Neon Boss", nullptr,
                                world_.resources.selectNeonBossTab_ ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)) {
            world_.resources.selectNeonBossTab_ = false;
            world_.bossPresentation->DrawNeonBossDeveloperTools();
            ImGui::EndTabItem();
        }
        const ImGuiTabItemFlags previewTabFlags =
            world_.resources.selectNeonSkinnedPreviewTab_ ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        world_.resources.selectNeonSkinnedPreviewTab_ = false;
        if (world_.resources.neonSkinnedPreview_ && ImGui::BeginTabItem("Neon Preview", nullptr, previewTabFlags)) {
            world_.resources.neonSkinnedPreview_->DrawImGui();
            ImGui::EndTabItem();
        }
#endif
        if (ImGui::BeginTabItem("概要")) {
            ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);
            ImGui::Text("デルタタイム: %.8f", world_.combat.finalDeltaTime * 60.0f);
            ImGui::Text("経験値敵の数: %zu", world_.resources.enemyManager_ ? world_.resources.enemyManager_->GetEnemyCount() : 0);
            ImGui::Text("弾軌跡インスタンス数: %zu",
                        world_.resources.bulletManager_ ? world_.resources.bulletManager_->GetTrailInstanceCount() : 0);
            if (world_.resources.player_) {
                const auto& hud = world_.resources.player_->GetUpgradeHudProfileStats();
                const auto& evo = world_.resources.player_->GetEvolutionUiProfileStats();
                ImGui::Separator();
                ImGui::Text("強化HUD: %s total %.3fms / update %.3f / sprite %.3f (%d) / text %.3f (%d)", hud.visible ? "表示" : "非表示",
                            hud.totalMs, hud.updateMs, hud.spriteMs, hud.spriteDraws, hud.textMs, hud.textDraws);
                ImGui::Text("進化UI: %s total %.3fms / update %.3f / sprite %.3f (%d) / text %.3f (%d)", evo.visible ? "表示" : "非表示",
                            evo.totalMs, evo.updateMs, evo.spriteMs, evo.spriteDraws, evo.textMs, evo.textDraws);
            }
            world_.performanceMonitor->DrawPerformanceCaptureImGui();
            ImGui::SeparatorText("処理時間比較");
            world_.performanceMonitor->DrawPerformanceBreakdownImGui();
            ImGui::Separator();
            ImGui::Checkbox("追従HPバーを表示", &world_.combat.showFollowHpBars_);
            ImGui::Checkbox("プレイヤースタミナバーを表示", &world_.combat.showPlayerStaminaBar_);
            ImGui::Checkbox("当たり判定を表示 (F7)", &world_.presentation.showCollisionDebug_);
            ImGui::Checkbox("弾の当たり判定も表示", &world_.presentation.showCollisionDebugBullets_);
            ImGui::Checkbox("弾HP/貫通力ラベルを表示", &world_.presentation.showBulletStatusDebugOverlay_);
            ImGui::Checkbox("弾HP/貫通力テーブルを表示", &world_.presentation.showBulletStatusDebugTable_);
            ImGui::DragInt("弾ラベル最大数", &world_.presentation.bulletStatusDebugMaxLabels_, 1.0f, 1, 200);
            ImGui::Checkbox("ポスト負荷表示を表示 (F8)", &world_.presentation.showPostProfileOverlay_);
            bool gameplayOutlines = world_.combat.screenEffectDirector_.IsOutlineEnabled();
            if (ImGui::Checkbox("ゲーム用 Depth Based Outline", &gameplayOutlines)) {
                world_.combat.screenEffectDirector_.SetOutlineEnabled(gameplayOutlines);
            }
            ImGui::Text("F12: このコンソールを表示/非表示");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("ゲームプレイ")) {
            ImGui::Text("レベル: %d", world_.resources.player_->GetLevel());
            ImGui::Text("経験値: %d / %d", world_.resources.player_->GetExp(), world_.resources.player_->GetNextLevelExpValue());
            ImGui::Text("スキルポイント: %d", world_.resources.player_->GetSkillPoints());
            ImGui::Text("機体クラス: %s", world_.resources.player_->GetCurrentClassName());
            ImGui::Checkbox("デバッグ無敵: プレイヤーHPを減らさない", &world_.presentation.debugPlayerNoDamage_);
            ImGui::Separator();
            ImGui::Text("1 自然回復       Lv.%d", world_.resources.player_->GetUpgradeLevel(0));
            ImGui::Text("2 最大HP         Lv.%d", world_.resources.player_->GetUpgradeLevel(1));
            ImGui::Text("3 体当たりダメージ Lv.%d", world_.resources.player_->GetUpgradeLevel(2));
            ImGui::Text("4 弾速           Lv.%d", world_.resources.player_->GetUpgradeLevel(3));
            ImGui::Text("5 弾ダメージ     Lv.%d", world_.resources.player_->GetUpgradeLevel(4));
            ImGui::Text("6 リロード       Lv.%d", world_.resources.player_->GetUpgradeLevel(5));
            ImGui::Text("7 移動速度       Lv.%d", world_.resources.player_->GetUpgradeLevel(6));
            ImGui::Separator();
            if (ImGui::Button("+ 次レベル分の経験値")) {
                world_.resources.player_->AddExp(world_.resources.player_->GetNextLevelExpValue());
            }
            ImGui::SameLine();
            if (ImGui::Button("+200 経験値")) {
                world_.resources.player_->AddExp(200);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("見た目")) {
            if (ImGui::Button("見た目設定を保存")) {
                if (world_.gameplaySettings->SaveGameVisualConfig()) {
                    world_.presentation.visualConfigStatus_ = "見た目設定を保存しました: resources/configs/gameVisuals.json";
                } else {
                    world_.presentation.visualConfigStatus_ = "見た目設定の保存に失敗しました。";
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("見た目設定を再読み込み")) {
                world_.gameplaySettings->LoadGameVisualConfig();
            }
            if (!world_.presentation.visualConfigStatus_.empty()) {
                ImGui::TextWrapped("%s", world_.presentation.visualConfigStatus_.c_str());
            }
            ImGui::Separator();
            if (ImGui::CollapsingHeader("ゲーム文字 / ネオン", ImGuiTreeNodeFlags_DefaultOpen)) {
                const char* fontModes[] = {"オリジナルへ戻す (Meiryo)", "Zen Maru Gothic Bold"};
                bool appearanceChanged =
                    ImGui::Combo("ゲームフォント", &world_.combat.gameTextFontMode_, fontModes, IM_ARRAYSIZE(fontModes));
                appearanceChanged |= ImGui::Checkbox("文字ネオンを有効化", &world_.combat.gameTextNeonEnabled_);
                appearanceChanged |= ImGui::Checkbox("黒アウトラインを表示", &world_.combat.gameTextOutlineEnabled_);
                appearanceChanged |= ImGui::ColorEdit4("文字アウトライン色", &world_.combat.gameTextOutlineColor_.x);
                appearanceChanged |= ImGui::DragFloat("文字アウトライン幅", &world_.combat.gameTextOutlineThickness_, 0.05f, 0.0f, 8.0f);
                appearanceChanged |= ImGui::ColorEdit4("文字発光色", &world_.combat.gameTextNeonStyle_.glowColor.x);
                appearanceChanged |=
                    ImGui::DragFloat("文字発光源輝度", &world_.combat.gameTextNeonStyle_.sourceBrightness, 0.02f, 0.0f, 8.0f);
                appearanceChanged |=
                    ImGui::DragFloat("文字ブルームしきい値", &world_.combat.gameTextNeonStyle_.threshold, 0.01f, 0.0f, 4.0f);
                appearanceChanged |= ImGui::DragFloat("文字内光強度", &world_.combat.gameTextNeonStyle_.innerIntensity, 0.01f, 0.0f, 4.0f);
                appearanceChanged |= ImGui::DragFloat("文字外光強度", &world_.combat.gameTextNeonStyle_.outerIntensity, 0.01f, 0.0f, 4.0f);
                if (ImGui::Button("文字表示を完全に元へ戻す")) {
                    world_.combat.gameTextFontMode_ = 0;
                    world_.combat.gameTextNeonEnabled_ = false;
                    world_.combat.gameTextOutlineEnabled_ = false;
                    appearanceChanged = true;
                }
                if (appearanceChanged) {
                    world_.gameplayHud->ApplyGameTextAppearance();
                }
                ImGui::TextDisabled("保存・再読込は上の見た目設定ボタンを使用します。 ");
            }
            ImGui::Separator();
            if (world_.resources.player_) {
                world_.resources.player_->DrawUpgradeHudDebugImGui();
                ImGui::Separator();
                world_.resources.player_->DrawEvolutionUiStyleEditor();
                ImGui::Separator();
            }
            if (ImGui::CollapsingHeader("ネオングリッド", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("背景グリッドを表示", &world_.presentation.showNeonGrid_);
                ImGui::Checkbox("キャラ周辺グリッドを表示", &world_.presentation.showActorLocalGrid_);
                ImGui::Checkbox("グリッドにポストエフェクト", &world_.presentation.enableNeonGridPostEffect_);
                ImGui::DragFloat("背景グリッド間隔", &world_.presentation.worldGridSpacing_, 0.05f, 0.25f, 8.0f);
                ImGui::DragFloat("背景グリッド線幅", &world_.presentation.worldGridLineWidth_, 0.005f, 0.005f, 0.5f);
                ImGui::ColorEdit4("背景グリッド色", &world_.presentation.worldGridColor_.x);
                ImGui::DragFloat("キャラ周辺グリッド半径", &world_.presentation.actorGridRadius_, 0.05f, 0.5f, 16.0f);
                ImGui::DragFloat("キャラ周辺グリッド間隔", &world_.presentation.actorGridSpacing_, 0.025f, 0.2f, 3.0f);
                ImGui::DragFloat("キャラ周辺グリッド線幅", &world_.presentation.actorGridLineWidth_, 0.005f, 0.005f, 0.5f);
                ImGui::DragFloat("ネオンライン外縁フェード", &world_.presentation.neonLineSoftEdgeRatio_, 0.01f, 0.0f, 0.95f);
                ImGui::DragFloat("ネオンライン芯の明るさ", &world_.presentation.neonLineCoreIntensity_, 0.01f, 0.0f, 3.0f);
                ImGui::SeparatorText("ネオン三角形パーティクル");
                const char* triangleEffectModes[] = {"枠線ネオン", "旧モデル粒子", "ハイブリッド"};
                if (ImGui::Combo("三角形演出方式", &world_.presentation.neonTriangleEffectMode_, triangleEffectModes,
                                 IM_ARRAYSIZE(triangleEffectModes))) {
                    cg2::ParticleManager::GetInstance()->SetNeonTriangleEffectMode(
                        static_cast<cg2::ParticleManager::NeonTriangleEffectMode>(world_.presentation.neonTriangleEffectMode_));
                }
                bool useGpuParticleUpdate = cg2::ParticleManager::GetInstance()->IsUseGpuUpdate();
                if (ImGui::Checkbox("GPUパーティクル更新", &useGpuParticleUpdate)) {
                    cg2::ParticleManager::GetInstance()->SetUseGpuUpdate(useGpuParticleUpdate);
                }
                ImGui::TextDisabled("OFF: 旧CPU行列生成（枠線比較用）");
                ImGui::DragFloat("三角形 外光幅倍率", &world_.presentation.neonParticleTriangleGlowWidthScale_, 0.05f, 1.0f, 8.0f);
                ImGui::DragFloat("三角形 白芯幅倍率", &world_.presentation.neonParticleTriangleCoreWidthScale_, 0.01f, 0.01f, 1.0f);
                ImGui::DragFloat("三角形 発光強度", &world_.presentation.neonParticleTriangleBrightness_, 0.05f, 0.0f, 5.0f);
                ImGui::DragInt("三角形 残像数", &world_.presentation.neonParticleTriangleTrailCopies_, 1.0f, 0, 8);
                ImGui::DragFloat("三角形 残像間隔", &world_.presentation.neonParticleTriangleTrailSpacing_, 0.01f, 0.0f, 2.0f);
                ImGui::DragFloat("三角形 出現時サイズ", &world_.presentation.neonParticleTriangleBirthScale_, 0.01f, 0.05f, 1.0f);
                ImGui::ColorEdit4("プレイヤーグリッド色", &world_.presentation.playerGridColor_.x);
                ImGui::ColorEdit4("ボスグリッド色", &world_.presentation.enemyGridColor_.x);
                ImGui::ColorEdit4("経験値敵グリッド色", &world_.presentation.expEnemyGridColor_.x);
                ImGui::Checkbox("画面外の周辺グリッドを省略", &world_.presentation.cullActorLocalGrid_);
                ImGui::DragInt("経験値敵グリッド最大数", &world_.presentation.maxExpEnemyLocalGrids_, 1.0f, 0, 60);
                ImGui::SeparatorText("ステージブロック枠線");
                ImGui::Checkbox("通常ブロックに3Dネオン枠線", &world_.presentation.showStageBlockNeonOutlines_);
                ImGui::Checkbox("通常ブロック本体を表示", &world_.presentation.showStageNormalBlockBodies_);
                ImGui::DragFloat("共通ブロック枠線幅", &world_.presentation.stageBlockNeonLineWidth_, 0.005f, 0.005f, 0.5f);
                ImGui::DragFloat("共通ブロック枠線の深度オフセット", &world_.presentation.stageBlockNeonDepthBias_, 0.001f, 0.0f, 0.2f);
                ImGui::ColorEdit4("通常ブロック枠線色", &world_.presentation.stageBlockNeonColor_.x);
                ImGui::Checkbox("ダメージブロックに3Dネオン枠線", &world_.presentation.showStageDamageBlockNeonOutlines_);
                ImGui::ColorEdit4("ダメージブロック枠線色", &world_.presentation.stageDamageBlockNeonColor_.x);
                ImGui::DragFloat("ダメージブロック点滅速度", &world_.presentation.stageDamageBlockPulseSpeed_, 0.1f, 0.0f, 30.0f);
                ImGui::DragFloatRange2("ダメージブロック点滅輝度", &world_.presentation.stageDamageBlockPulseMin_,
                                       &world_.presentation.stageDamageBlockPulseMax_, 0.01f, 0.0f, 4.0f, "最小 %.2f", "最大 %.2f");
                ImGui::SeparatorText("経験値敵ネオン表示");
                const char* expEnemyNeonModes[] = {"3Dモデル", "ビルボード枠線", "3D枠線", "黒塗り+3D枠線"};
                ImGui::Combo("経験値敵の表示", &world_.presentation.expEnemyNeonRenderMode_, expEnemyNeonModes,
                             IM_ARRAYSIZE(expEnemyNeonModes));
                ImGui::DragFloat("四角敵ネオンサイズ", &world_.presentation.expEnemyNeonSquareSize_, 0.025f, 0.2f, 4.0f);
                ImGui::DragFloat("三角敵ネオン半径", &world_.presentation.expEnemyNeonTriangleRadius_, 0.025f, 0.2f, 4.0f);
                ImGui::DragFloat("五角敵ネオン半径", &world_.presentation.expEnemyNeonPentagonRadius_, 0.025f, 0.2f, 4.0f);
                ImGui::DragFloat("射撃敵ネオン半径", &world_.presentation.expEnemyNeonShooterRadius_, 0.025f, 0.2f, 4.0f);
                ImGui::DragFloat("敵ネオン線幅", &world_.presentation.expEnemyNeonLineWidth_, 0.005f, 0.01f, 0.5f);
                ImGui::SeparatorText("プレイヤー / ボス ネオン表示");
                const char* actorNeonModes[] = {"3Dモデル", "ビルボード枠線"};
                ImGui::Combo("プレイヤー表示", &world_.presentation.playerNeonRenderMode_, actorNeonModes, IM_ARRAYSIZE(actorNeonModes));
                ImGui::Combo("ボス表示", &world_.presentation.bossNeonRenderMode_, actorNeonModes, IM_ARRAYSIZE(actorNeonModes));
                ImGui::DragFloat("プレイヤー板ポリ半径", &world_.presentation.playerNeonBillboardRadius_, 0.025f, 0.2f, 4.0f);
                ImGui::DragFloat("ボス板ポリ半径", &world_.presentation.bossNeonBillboardRadius_, 0.025f, 0.2f, 5.0f);
                ImGui::DragFloat("プレイヤー/ボス線幅", &world_.presentation.actorNeonBillboardLineWidth_, 0.005f, 0.01f, 0.5f);
                ImGui::SeparatorText("ボス砲塔位置");
                ImGui::DragFloat("ボス砲塔 前後位置", &world_.presentation.bossNeonBarrelForwardOffset_, 0.01f, -2.0f, 3.0f);
                ImGui::DragFloat("ボス砲塔 横位置", &world_.presentation.bossNeonBarrelSideOffset_, 0.01f, -2.0f, 2.0f);
                ImGui::DragFloat("ボス砲塔 長さ", &world_.presentation.bossNeonBarrelLengthScale_, 0.01f, 0.1f, 4.0f);
                ImGui::DragFloat("ボス砲塔 太さ", &world_.presentation.bossNeonBarrelWidthScale_, 0.01f, 0.05f, 2.0f);
                ImGui::DragFloat("ボス砲塔 角度補正", &world_.presentation.bossNeonBarrelAngleDeg_, 0.25f, -180.0f, 180.0f);
                ImGui::SeparatorText("板ポリ本体の塗り");
                ImGui::Checkbox("プレイヤー/ボス本体を暗く塗る", &world_.presentation.fillActorNeonBodies_);
                ImGui::ColorEdit4("本体の塗り色", &world_.presentation.actorNeonBodyFillColor_.x, ImGuiColorEditFlags_Float);
                ImGui::SliderFloat("ダッシュ中の本体濃度", &world_.presentation.playerDashCurrentAlpha_, 0.05f, 1.0f);
                ImGui::SliderFloat("残像の濃度", &world_.presentation.playerAfterimageAlpha_, 0.05f, 1.0f);
                ImGui::DragFloat("残像の生成間隔", &world_.presentation.playerAfterimageInterval_, 0.005f, 0.01f, 0.25f);
                ImGui::DragFloat("残像の寿命", &world_.presentation.playerAfterimageLifetime_, 0.01f, 0.05f, 1.0f);
                ImGui::SeparatorText("プレイヤー近接剣");
                ImGui::Checkbox("待機中の剣を表示", &world_.presentation.showPlayerIdleMeleeSaber_);
                ImGui::Checkbox("攻撃中のリボン軌跡を表示", &world_.presentation.enablePlayerMeleeRibbonTrail_);
                ImGui::DragFloat("待機剣 横位置", &world_.presentation.playerIdleSaberSideOffset_, 0.01f, -2.0f, 2.0f);
                ImGui::DragFloat("待機剣 前後位置", &world_.presentation.playerIdleSaberForwardOffset_, 0.01f, -2.0f, 2.0f);
                ImGui::DragFloat("待機剣 長さ", &world_.presentation.playerIdleSaberLength_, 0.01f, 0.05f, 4.0f);
                ImGui::DragFloat("待機剣 角度", &world_.presentation.playerIdleSaberAngleDeg_, 0.25f, -180.0f, 180.0f);
                ImGui::DragFloat("待機剣 柄の長さ", &world_.presentation.playerIdleSaberHiltLength_, 0.01f, 0.02f, 1.0f);
                ImGui::DragFloat("待機剣 刃の基準幅", &world_.presentation.playerIdleSaberBladeWidth_, 0.005f, 0.005f, 0.5f);
                ImGui::DragFloat("待機剣 外光幅倍率", &world_.presentation.playerIdleSaberOuterWidthScale_, 0.01f, 0.05f, 8.0f);
                ImGui::DragFloat("待機剣 芯幅倍率", &world_.presentation.playerIdleSaberCoreWidthScale_, 0.01f, 0.01f, 2.0f);
                ImGui::DragFloat("攻撃剣 外光幅倍率", &world_.presentation.playerMeleeBladeOuterWidthScale_, 0.01f, 0.05f, 8.0f);
                ImGui::DragFloat("攻撃剣 発光幅倍率", &world_.presentation.playerMeleeBladeHaloWidthScale_, 0.01f, 0.05f, 4.0f);
                ImGui::DragFloat("攻撃剣 芯幅倍率", &world_.presentation.playerMeleeBladeCoreWidthScale_, 0.01f, 0.01f, 2.0f);
                ImGui::DragFloat("攻撃軌跡 太さ倍率", &world_.presentation.playerMeleeTrailWidthScale_, 0.01f, 0.05f, 5.0f);
                ImGui::DragFloat("攻撃軌跡 濃度倍率", &world_.presentation.playerMeleeTrailAlphaScale_, 0.01f, 0.0f, 5.0f);
                ImGui::DragFloat("攻撃残像 濃度倍率", &world_.presentation.playerMeleeAfterimageAlphaScale_, 0.01f, 0.0f, 5.0f);
                if (ImGui::TreeNode("コンボ段ごとの軌道")) {
                    for (size_t i = 0; i < world_.presentation.playerMeleeComboVisuals_.size(); ++i) {
                        MeleeComboVisualProfile& profile = world_.presentation.playerMeleeComboVisuals_[i];
                        ImGui::PushID(static_cast<int>(i));
                        const std::string label = std::to_string(i + 1) + "段目";
                        if (ImGui::TreeNode(label.c_str())) {
                            ImGui::DragFloat("開始角度", &profile.startAngleDeg, 0.5f, -360.0f, 360.0f);
                            ImGui::DragFloat("終了角度", &profile.endAngleDeg, 0.5f, -360.0f, 360.0f);
                            ImGui::DragFloat("演出時間倍率", &profile.durationScale, 0.01f, 0.05f, 3.0f);
                            ImGui::DragFloat("刃の長さ倍率", &profile.bladeLengthScale, 0.01f, 0.05f, 3.0f);
                            ImGui::DragFloat("刃の太さ倍率", &profile.bladeWidthScale, 0.01f, 0.05f, 3.0f);
                            ImGui::DragFloat("柄の横位置", &profile.hiltSideOffset, 0.005f, -1.0f, 1.0f);
                            ImGui::DragFloat("振りかぶり角度", &profile.windupAngleDeg, 0.5f, -360.0f, 360.0f);
                            ImGui::DragFloat("戻り先角度", &profile.returnAngleDeg, 0.5f, -360.0f, 360.0f);
                            ImGui::ColorEdit4("色倍率", &profile.colorScale.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
                ImGui::SeparatorText("ネオン三角形デモ");
                ImGui::Checkbox("ネオン三角形デモを表示", &world_.presentation.showNeonTriangleDemo_);
                ImGui::DragFloat3("ネオン三角形 位置", &world_.presentation.neonTriangleDemoCenter_.x, 0.05f);
                ImGui::DragFloat("ネオン三角形 半径", &world_.presentation.neonTriangleDemoRadius_, 0.05f, 0.1f, 12.0f);
                ImGui::DragFloat("ネオン三角形 線幅", &world_.validation.neonTriangleDemoLineWidth_, 0.005f, 0.005f, 1.0f);
                ImGui::DragFloat("ネオン三角形 回転速度", &world_.validation.neonTriangleDemoRotateSpeed_, 0.01f, -5.0f, 5.0f);
                ImGui::ColorEdit4("ネオン三角形 色", &world_.validation.neonTriangleDemoColor_.x);
            }
            if (ImGui::CollapsingHeader("弾の軌跡", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("弾軌跡にポストエフェクト", &world_.presentation.enableBulletTrailPostEffect_);
                BulletTrailSettings bulletTrail = world_.resources.bulletManager_->GetTrailSettings();
                ImGui::DragFloat("プレイヤー軌跡半幅", &bulletTrail.playerHalfWidth, 0.01f, 0.01f, 1.5f);
                ImGui::DragFloat("敵軌跡半幅", &bulletTrail.enemyHalfWidth, 0.01f, 0.01f, 1.5f);
                ImGui::DragFloat("軌跡の寿命", &bulletTrail.lifetime, 0.01f, 0.02f, 1.5f);
                ImGui::DragInt("軌跡の最大点数", &bulletTrail.maxPoints, 1.0f, 2, 80);
                ImGui::DragInt("軌跡の補間数", &bulletTrail.interpolationSteps, 1.0f, 1, 12);
                ImGui::DragFloat("先端の太さ倍率", &bulletTrail.headWidthScale, 0.01f, 0.0f, 4.0f);
                ImGui::DragFloat("末端の太さ倍率", &bulletTrail.tailWidthScale, 0.01f, 0.0f, 4.0f);
                ImGui::DragFloat("太さの減衰カーブ", &bulletTrail.widthCurvePower, 0.01f, 0.05f, 6.0f);
                ImGui::DragFloat("色の減衰カーブ", &bulletTrail.colorCurvePower, 0.01f, 0.05f, 6.0f);
                ImGui::Checkbox("弾の色を軌跡に使う", &bulletTrail.useObjectColorForTrail);
                ImGui::ColorEdit4("プレイヤー弾色", &bulletTrail.playerObjectColor.x);
                ImGui::ColorEdit4("敵弾色", &bulletTrail.enemyObjectColor.x);
                ImGui::ColorEdit4("反射弾色", &bulletTrail.reflectableObjectColor.x);
                if (bulletTrail.useObjectColorForTrail) {
                    ImGui::DragFloat("軌跡先端の発光倍率", &bulletTrail.trailHeadIntensity, 0.01f, 0.0f, 5.0f);
                    ImGui::DragFloat("軌跡末端の発光倍率", &bulletTrail.trailTailIntensity, 0.01f, 0.0f, 5.0f);
                    ImGui::DragFloat("軌跡先端の透明度", &bulletTrail.trailHeadAlpha, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat("軌跡末端の透明度", &bulletTrail.trailTailAlpha, 0.01f, 0.0f, 1.0f);
                } else {
                    ImGui::ColorEdit4("軌跡開始色", &bulletTrail.startColor.x);
                    ImGui::ColorEdit4("プレイヤー軌跡終了色", &bulletTrail.playerEndColor.x);
                    ImGui::ColorEdit4("敵軌跡終了色", &bulletTrail.enemyEndColor.x);
                    ImGui::ColorEdit4("反射弾軌跡終了色", &bulletTrail.reflectableEndColor.x);
                }
                world_.resources.bulletManager_->SetTrailSettings(bulletTrail);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("ポスト")) {
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
            const char* bloomModes[] = {"Engine setting", "Bloom OFF", "Legacy Bloom", "Quality Bloom", "Light Bloom"};
            int bloomSelection = world_.presentation.developerBloomComparisonMode_ + 1;
            ImGui::BeginDisabled(cg2::RuntimeProfiler::Get().IsCaptureActive());
            if (ImGui::Combo("Developer Bloom comparison", &bloomSelection, bloomModes, 5))
                world_.presentation.developerBloomComparisonMode_ = bloomSelection - 1;
            const char* toneModes[] = {"Engine tone mapping", "Legacy ACES", "Hue-preserving shoulder"};
            int toneSelection = world_.presentation.developerToneMappingMode_ < 0 ? 0 : world_.presentation.developerToneMappingMode_;
            if (ImGui::Combo("Developer tone mapping", &toneSelection, toneModes, 3))
                world_.presentation.developerToneMappingMode_ = toneSelection == 0 ? -1 : toneSelection;
            ImGui::Checkbox("Freeze game for Bloom comparison", &world_.presentation.developerBloomFreeze_);
            ImGui::EndDisabled();
            ImGui::TextWrapped(
                "Developer-only temporary comparison. OFF/Legacy/Quality/Light affects local object and global Bloom; source cores, camera and gameplay remain unchanged. Text glow uses its own display-safe additive route after tone mapping. Engine setting restores the normal configuration.");
            ImGui::BeginDisabled(world_.presentation.developerGameCapture_.IsBusy() || world_.gameplayQueries->IsNeonShowcaseActive() ||
                                 cg2::RuntimeProfiler::Get().IsCaptureActive());
            ImGui::InputText("Game capture label", world_.presentation.developerGameCaptureLabel_,
                             sizeof(world_.presentation.developerGameCaptureLabel_));
            if (ImGui::Button("Save game clean PNG + metadata")) {
                if (world_.presentation.developerGameCaptureDirectory_.empty()) {
                    world_.presentation.developerGameCaptureDirectory_ =
                        "generated/neon_bloom_presentation/game_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                                                       std::chrono::system_clock::now().time_since_epoch())
                                                                                       .count());
                }
                char name[96]{};
                std::snprintf(name, sizeof(name), "%03u_%s", world_.presentation.developerGameCaptureNumber_++,
                              world_.presentation.developerGameCaptureLabel_);
                world_.presentation.developerGameCapture_.Request(world_.presentation.developerGameCaptureDirectory_, name, {});
            }
            ImGui::EndDisabled();
            ImGui::TextWrapped("%s", world_.presentation.developerGameCapture_.GetStatus().c_str());
            if (!world_.presentation.developerGameCaptureDirectory_.empty())
                ImGui::TextWrapped("Game captures: %s", world_.presentation.developerGameCaptureDirectory_.c_str());
            ImGui::TextWrapped(
                "Manual comparison: freeze once, keep camera/tone/appearance unchanged, and capture OFF/Legacy/Quality individually. Unfrozen captures are labelled live gameplay, not identical-frame A/B. Sharp projectile capture is full-resolution in Quality/OFF; Legacy intentionally retains its old half-resolution source.");
            ImGui::Separator();
#endif
            if (ImGui::Button("ポスト設定を保存")) {
                if (world_.gameplaySettings->SaveGamePostEffectConfig()) {
                    world_.presentation.postEffectConfigStatus_ = "ポスト設定を保存しました: resources/configs/gamePostEffects.json";
                } else {
                    world_.presentation.postEffectConfigStatus_ = "ポスト設定の保存に失敗しました。";
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("ポスト設定を再読み込み")) {
                world_.gameplaySettings->LoadGamePostEffectConfig();
            }
            if (!world_.presentation.postEffectConfigStatus_.empty()) {
                ImGui::TextWrapped("%s", world_.presentation.postEffectConfigStatus_.c_str());
            }
            ImGui::Separator();
            if (ImGui::Button("全ポスト有効")) {
                world_.presentation.enableNeonGridPostEffect_ = true;
                world_.presentation.enableStagePostEffect_ = true;
                world_.presentation.enableBulletTrailPostEffect_ = true;
                world_.presentation.enableParticlePostEffect_ = true;
                world_.combat.enablePlayerPostEffect_ = true;
                world_.combat.enableEnemyPostEffect_ = true;
                world_.presentation.enableExpEnemyPostEffect_ = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("全ポスト無効")) {
                world_.presentation.enableNeonGridPostEffect_ = false;
                world_.presentation.enableStagePostEffect_ = false;
                world_.presentation.enableBulletTrailPostEffect_ = false;
                world_.presentation.enableParticlePostEffect_ = false;
                world_.combat.enablePlayerPostEffect_ = false;
                world_.combat.enableEnemyPostEffect_ = false;
                world_.presentation.enableExpEnemyPostEffect_ = false;
            }
            if (ImGui::CollapsingHeader("プレイヤー", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("プレイヤーポストを有効", &world_.combat.enablePlayerPostEffect_);
                world_.gameplaySettings->DrawPostEffectParamControls("Player", world_.resources.playerPostEffect_->GetParam());
            }
            if (ImGui::CollapsingHeader("ボス敵")) {
                ImGui::Checkbox("ボス敵ポストを有効", &world_.combat.enableEnemyPostEffect_);
                world_.gameplaySettings->DrawPostEffectParamControls("Enemy", world_.resources.enemyPostEffect_->GetParam());
            }
            if (ImGui::CollapsingHeader("経験値敵")) {
                ImGui::Checkbox("経験値敵ポストを有効", &world_.presentation.enableExpEnemyPostEffect_);
                world_.gameplaySettings->DrawPostEffectParamControls("ExpEnemy", world_.resources.expEnemyPostEffect_->GetParam());
                ImGui::DragFloat("ポスト省略範囲 半幅", &world_.presentation.expEnemyPostVisibleHalfWidth_, 0.5f, 10.0f, 80.0f);
                ImGui::DragFloat("ポスト省略範囲 半高さ", &world_.presentation.expEnemyPostVisibleHalfHeight_, 0.5f, 10.0f, 60.0f);
            }
            if (ImGui::CollapsingHeader("ステージ")) {
                ImGui::Checkbox("ステージポストを有効", &world_.presentation.enableStagePostEffect_);
                world_.gameplaySettings->DrawPostEffectParamControls("Stage", world_.resources.stagePostEffect_->GetParam());
                ImGui::Text("ステージは軽量化のためブルーム加算のみを使います。");
            }
            if (ImGui::CollapsingHeader("共有オブジェクト発光")) {
                world_.gameplaySettings->DrawPostEffectParamControls("SharedObjectBloom",
                                                                     world_.resources.sharedObjectBloomPostEffect_->GetParam());
                ImGui::TextWrapped("プレイヤー、敵、経験値敵、ステージなどの単体発光をまとめる共通パスです。");
            }
            if (ImGui::CollapsingHeader("グリッド / 弾軌跡 / パーティクル")) {
                ImGui::Checkbox("グリッドポストを有効", &world_.presentation.enableNeonGridPostEffect_);
                cg2::BloomParam& gridPost = world_.resources.neonGridPostEffect_->GetParam();
                ImGui::DragFloat("グリッド発光強度", &gridPost.intensity, 0.01f, 0.0f, 6.0f);
                ImGui::DragFloat("グリッドQuality Gain", &gridPost.bloomGain, 0.01f, 0.0f, 2.0f);
                ImGui::DragFloat("グリッド発光しきい値", &gridPost.threshold, 0.01f, 0.0f, 2.0f);
                ImGui::Checkbox("弾軌跡ポストを有効", &world_.presentation.enableBulletTrailPostEffect_);
                cg2::BloomParam& bulletTrailPost = world_.resources.bulletTrailPostEffect_->GetParam();
                ImGui::DragFloat("弾軌跡発光強度", &bulletTrailPost.intensity, 0.01f, 0.0f, 8.0f);
                ImGui::DragFloat("弾軌跡Quality Gain", &bulletTrailPost.bloomGain, 0.01f, 0.0f, 2.0f);
                ImGui::DragFloat("弾軌跡発光しきい値", &bulletTrailPost.threshold, 0.01f, 0.0f, 2.0f);
                auto head = world_.resources.neonProjectileRenderer_->GetParams();
                bool changed = ImGui::Checkbox("Neon Projectile Heads", &head.enabled);
                changed |= ImGui::DragFloat("Head Length", &head.headLength, 0.01f, 0.05f, 3.0f);
                changed |= ImGui::DragFloat("Head Width", &head.headWidth, 0.01f, 0.03f, 1.5f);
                changed |= ImGui::DragFloat("Head Emission", &head.headIntensity, 0.05f, 0.0f, 8.0f);
                changed |= ImGui::DragFloat("Pale Core Emission", &head.coreIntensity, 0.05f, 0.0f, 12.0f);
                changed |= ImGui::DragFloat("Head Halo Alpha", &head.haloAlpha, 0.01f, 0.0f, 0.4f);
                if (changed)
                    world_.resources.neonProjectileRenderer_->SetParams(head);
                ImGui::TextWrapped("Head controls are session-only. Bullet movement and collision are unchanged.");
                ImGui::Checkbox("三角パーティクルポストを有効", &world_.presentation.enableParticlePostEffect_);
                cg2::BloomParam& particlePost = world_.resources.particlePostEffect_->GetParam();
                ImGui::DragFloat("三角パーティクル発光強度", &particlePost.intensity, 0.01f, 0.0f, 8.0f);
                ImGui::DragFloat("パーティクルQuality Gain", &particlePost.bloomGain, 0.01f, 0.0f, 2.0f);
                ImGui::DragFloat("三角パーティクル発光しきい値", &particlePost.threshold, 0.01f, 0.0f, 2.0f);
            }
            if (ImGui::CollapsingHeader("死亡チャージ / 衝撃波")) {
                ImGui::Checkbox("死亡時の画面衝撃波を有効", &world_.presentation.enableDeathPostPulse_);
                ImGui::DragFloat("衝撃波の時間", &world_.presentation.deathPostPulseDuration_, 0.01f, 0.1f, 2.0f);
                ImGui::DragFloat("死亡時ブルーム増幅", &world_.presentation.deathPostBloomBoost_, 0.05f, 0.0f, 6.0f);
                ImGui::DragFloat("死亡時色収差", &world_.presentation.deathPostChromAbAmount_, 0.001f, 0.0f, 0.2f);
                ImGui::DragFloat("衝撃波の歪み", &world_.presentation.deathPostShockwaveStrength_, 0.001f, 0.0f, 0.15f);
                ImGui::DragFloat("衝撃波リング幅", &world_.presentation.deathPostShockwaveWidth_, 0.001f, 0.005f, 0.25f);
                ImGui::DragFloat("衝撃波の到達半径", &world_.presentation.deathPostShockwaveMaxRadius_, 0.01f, 0.1f, 1.5f);
            }
            if (ImGui::CollapsingHeader("スローモーション")) {
                ImGui::Text("スロー中: %s", world_.presentation.slowMotionPostActive_ ? "はい" : "いいえ");
                ImGui::Checkbox("スロー中もプレイヤー色を維持", &world_.presentation.keepPlayerColorDuringSlow_);
                ImGui::DragFloat("スロー時 プレイヤー色収差", &world_.presentation.slowPlayerChromAbAmount_, 0.001f, 0.0f, 0.2f);
                ImGui::DragFloat("スロー時 プレイヤー歪み", &world_.presentation.slowPlayerDistortionAmount_, 0.001f, 0.0f, 0.2f);
                ImGui::DragFloat("スロー時 プレイヤーグリッチ", &world_.presentation.slowPlayerGlitchAmount_, 0.001f, 0.0f, 0.2f);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("ツール")) {
            ImGui::Checkbox("パーティクルエディタを開く", &world_.presentation.showParticleEditor_);
            ImGui::Checkbox("プレイヤー機体エディタを開く", &world_.presentation.showPlayerClassEditor_);
            ImGui::Separator();
            ImGui::Text("弾耐久デバッグ");
            ImGui::Checkbox("画面上ラベル", &world_.presentation.showBulletStatusDebugOverlay_);
            ImGui::Checkbox("ライブ表", &world_.presentation.showBulletStatusDebugTable_);
            ImGui::DragInt("画面上ラベル最大数", &world_.presentation.bulletStatusDebugMaxLabels_, 1.0f, 1, 200);
            DrawBulletStatusDebugTable();
            ImGui::Separator();
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
            if (world_.resources.neonSkinnedPreview_)
                world_.resources.neonSkinnedPreview_->DrawImGui();
            ImGui::Separator();
#endif
            if (world_.combat.shotGide) {
                cg2::Vector2 guidePosition = world_.combat.shotGide->GetPosition();
                if (ImGui::SliderFloat2("射撃ガイド位置", &guidePosition.x, 0.0f, 3000.0f, "%.1f"))
                    world_.combat.shotGide->SetPosition(guidePosition);
            }
            if (world_.resources.ball_) {
                cg2::Vector3 ballScale = world_.resources.ball_->GetScale();
                cg2::Vector4 ballColor = world_.resources.ball_->GetColor();
                if (ImGui::DragFloat3("デバッグ球スケール", &ballScale.x))
                    world_.resources.ball_->SetScale(ballScale);
                if (ImGui::ColorEdit4("デバッグ球色", &ballColor.x))
                    world_.resources.ball_->SetColor(ballColor);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("バランス")) {
            DrawLevelAIDitorBalanceLab(true);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
#endif
}

void GameplayEditor::DrawBulletStatusDebugTable()
{
#ifdef USE_IMGUI
    if (!world_.presentation.showBulletStatusDebugTable_ || !world_.resources.bulletManager_) {
        return;
    }

    const std::vector<Bullet*> bullets = world_.resources.bulletManager_->GetBulletPtrs();
    ImGui::Text("現在の弾数: %zu", bullets.size());
    if (!ImGui::BeginTable("BulletStatusDebugTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                           ImVec2(0.0f, 180.0f))) {
        return;
    }

    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 36.0f);
    ImGui::TableSetupColumn("所属", ImGuiTableColumnFlags_WidthFixed, 74.0f);
    ImGui::TableSetupColumn("ダメージ", ImGuiTableColumnFlags_WidthFixed, 78.0f);
    ImGui::TableSetupColumn("弾HP", ImGuiTableColumnFlags_WidthFixed, 82.0f);
    ImGui::TableSetupColumn("貫通力", ImGuiTableColumnFlags_WidthFixed, 74.0f);
    ImGui::TableSetupColumn("位置", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("速度", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    for (size_t i = 0; i < bullets.size(); ++i) {
        Bullet* bullet = bullets[i];
        if (!bullet) {
            continue;
        }
        const cg2::Vector3 pos = bullet->GetWorldPosition();
        const cg2::Vector3 vel = bullet->GetMove();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%zu", i);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextColored(bullet->GetOwner() == kPlayer ? ImVec4(1.0f, 0.92f, 0.28f, 1.0f) : ImVec4(1.0f, 0.36f, 0.48f, 1.0f), "%s",
                           BulletOwnerName(bullet->GetOwner()));
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%u", bullet->GetDamage());
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%.1f", bullet->GetBulletHp());
        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%.1f", bullet->GetBulletPenetration());
        ImGui::TableSetColumnIndex(5);
        ImGui::Text("%.1f, %.1f", pos.x, pos.y);
        ImGui::TableSetColumnIndex(6);
        ImGui::Text("%.2f, %.2f", vel.x, vel.y);
    }

    ImGui::EndTable();
#endif
}

void GameplayEditor::DrawBulletStatusDebugOverlay()
{
#ifdef USE_IMGUI
    if (!world_.presentation.showBulletStatusDebugOverlay_ || !world_.resources.bulletManager_) {
        return;
    }

    const std::vector<Bullet*> bullets = world_.resources.bulletManager_->GetBulletPtrs();
    ImDrawList* drawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
    const int maxLabels = (std::max)(1, world_.presentation.bulletStatusDebugMaxLabels_);
    int drawn = 0;
    for (Bullet* bullet : bullets) {
        if (!bullet || bullet->IsDead()) {
            continue;
        }
        if (drawn >= maxLabels) {
            break;
        }

        const cg2::Vector3 worldPos = bullet->GetWorldPosition();
        const cg2::Vector2 screen = world_.gameplayHud->WorldToScreen(worldPos + cg2::Vector3{0.0f, 0.85f, 0.0f});
        if (screen.x < -80.0f || screen.x > cg2::WinApp::kClientWidth + 80.0f || screen.y < -40.0f ||
            screen.y > cg2::WinApp::kClientHeight + 40.0f) {
            continue;
        }

        char text[64]{};
        std::snprintf(text, sizeof(text), "%c HP %.1f  PEN %.1f",
                      bullet->GetOwner() == kPlayer ? 'P' : (bullet->GetOwner() == kExpEnemyHostile ? 'X' : 'E'), bullet->GetBulletHp(),
                      bullet->GetBulletPenetration());

        const ImVec2 textPos(screen.x + 8.0f, screen.y - 10.0f);
        drawList->AddText(ImVec2(textPos.x + 1.0f, textPos.y + 1.0f), IM_COL32(0, 0, 0, 220), text);
        drawList->AddText(textPos, BulletOwnerDebugColor(bullet->GetOwner()), text);
        ++drawn;
    }
#else
    (void)this;
#endif
}

void GameplayEditor::DrawLevelAIDitorBalanceLab(bool embedded)
{
#ifdef USE_IMGUI
    if (!world_.resources.balanceEditor_.initialized && world_.resources.currentLevelData_.balance.is_object()) {
        world_.gameplaySettings->LoadBalanceEditorFromJson(world_.resources.currentLevelData_.balance);
    }

    if (!embedded) {
        ImGui::Begin("レベルバランス調整ラボ");
    }
    ImGui::Text("実行中のバランスを調整し、必要なら level_test.json に保存します。");
    ImGui::Checkbox("通常ランダムスポーン有効", &world_.resources.balanceEditor_.defaultRandomSpawnEnabled);
    ImGui::Separator();

    if (ImGui::CollapsingHeader("プレイヤー", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragInt("最大HP", &world_.resources.balanceEditor_.playerMaxHp, 10.0f, 1, 9999);
        ImGui::DragFloat("リロード速度 基礎値", &world_.resources.balanceEditor_.playerReloadSpeed, 0.05f, 0.05f, 60.0f);
        ImGui::DragFloat("弾ダメージ 基礎値", &world_.resources.balanceEditor_.playerBulletDamage, 0.1f, 0.1f, 999.0f);
        ImGui::DragFloat("弾速 基礎値", &world_.resources.balanceEditor_.playerBulletSpeed, 0.01f, 0.01f, 5.0f);
        ImGui::DragFloat("移動速度 基礎値", &world_.resources.balanceEditor_.playerMoveSpeed, 0.005f, 0.01f, 5.0f);
        ImGui::DragFloat("自然回復 基礎値", &world_.resources.balanceEditor_.playerStaminaRecovery, 0.01f, 0.0f, 20.0f);
        ImGui::DragFloat("最大スタミナ 基礎値", &world_.resources.balanceEditor_.playerMaxStamina, 0.1f, 0.0f, 20.0f);
        ImGui::DragInt("体当たりダメージ", &world_.resources.balanceEditor_.playerBodyDamage, 1.0f, 1, 999);
        ImGui::Checkbox("適用時に全回復", &world_.resources.balanceEditor_.healToFull);
        if (world_.resources.player_) {
            ImGui::Text("現在HP: %d / %d", world_.resources.player_->GetHp(), world_.resources.player_->GetMaxHp());
            const Player::PlayerStats& stats = world_.resources.player_->GetStats();
            ImGui::Text("現在値: リロード %.2f / 弾ダメ %.2f / 弾速 %.3f / 移動 %.3f / 回復 %.2f", stats.reloadSpeed, stats.bulletDamage,
                        stats.bulletSpeed, stats.moveSpeed, stats.staminaRecovery);
        }
    }

    if (ImGui::CollapsingHeader("プレイヤー強化幅", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped(
            "1回強化したときの倍率です。0.10なら10%%/Lvです。リロードだけは数値が小さいほど速いため、この割合ぶん短縮されます。");
        ImGui::DragFloat("自然回復 倍率/Lv", &world_.resources.balanceEditor_.playerHealthRegenUpgrade, 0.005f, 0.0f, 2.0f);
        ImGui::DragFloat("最大HP 倍率/Lv", &world_.resources.balanceEditor_.maxHpUpgradeAmount, 0.005f, 0.0f, 2.0f);
        ImGui::DragFloat("体当たり倍率/Lv", &world_.resources.balanceEditor_.playerBodyDamageUpgrade, 0.005f, 0.0f, 2.0f);
        ImGui::DragFloat("弾速倍率/Lv", &world_.resources.balanceEditor_.playerBulletSpeedUpgrade, 0.005f, -0.95f, 2.0f);
        ImGui::DragFloat("弾ダメージ倍率/Lv", &world_.resources.balanceEditor_.playerBulletDamageUpgrade, 0.005f, 0.0f, 2.0f);
        ImGui::DragFloat("リロード短縮率/Lv", &world_.resources.balanceEditor_.playerReloadUpgrade, 0.005f, 0.0f, 0.95f);
        ImGui::DragFloat("リロード最小値", &world_.resources.balanceEditor_.playerMinReloadSpeed, 0.05f, 0.05f, 60.0f);
        ImGui::DragFloat("移動速度倍率/Lv", &world_.resources.balanceEditor_.playerMoveSpeedUpgrade, 0.005f, -0.95f, 2.0f);
    }

    if (ImGui::CollapsingHeader("接触/被弾ダメージ", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragInt("ダメージブロック", &world_.resources.balanceEditor_.damageBlock, 1.0f, 1, 999);
        ImGui::DragInt("ボス接触", &world_.resources.balanceEditor_.bossContact, 1.0f, 1, 999);
        ImGui::DragInt("経験値敵接触", &world_.resources.balanceEditor_.expEnemyContact, 1.0f, 1, 999);
        ImGui::DragInt("射撃敵接触", &world_.resources.balanceEditor_.shooterContact, 1.0f, 1, 999);
        ImGui::DragInt("射撃敵の弾", &world_.resources.balanceEditor_.shooterBullet, 1.0f, 1, 999);
        ImGui::DragFloat("射撃敵の探知半径", &world_.resources.balanceEditor_.shooterDetectionRadius, 0.25f, 1.0f, 100.0f);
        ImGui::DragFloat("射撃敵の砲塔旋回速度", &world_.resources.balanceEditor_.shooterTurnSpeed, 0.1f, 0.1f, 30.0f);
        ImGui::DragFloat("射撃敵の発射間隔", &world_.resources.balanceEditor_.shooterFireInterval, 0.05f, 0.1f, 10.0f);
        ImGui::DragFloat("射撃敵の弾速", &world_.resources.balanceEditor_.shooterBulletSpeed, 0.01f, 0.01f, 2.0f);
    }

    if (ImGui::CollapsingHeader("ボス通常攻撃", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* patternNames[] = {"拡散", "リング", "狙撃", "交互射撃"};
        ImGui::Combo("攻撃パターン", &world_.resources.balanceEditor_.bossAttackPattern, patternNames, IM_ARRAYSIZE(patternNames));
        ImGui::DragFloat("弾速", &world_.resources.balanceEditor_.bossBulletSpeed, 0.01f, 0.01f, 2.0f);
        ImGui::DragInt("弾数", &world_.resources.balanceEditor_.bossBulletCount, 1.0f, 1, 64);
        ImGui::DragFloat("拡散角度", &world_.resources.balanceEditor_.bossSpreadAngleDeg, 1.0f, 0.0f, 360.0f);
        ImGui::DragFloat("クールタイム", &world_.resources.balanceEditor_.bossCooldown, 0.01f, 0.05f, 5.0f);
        ImGui::DragInt("弾ダメージ", &world_.resources.balanceEditor_.bossBulletDamage, 1.0f, 1, 999);
        ImGui::DragFloat("弾HP (0=ダメージ値)", &world_.resources.balanceEditor_.bossBulletHp, 0.1f, 0.0f, 999.0f);
        ImGui::DragFloat("弾貫通力 (0=ダメージ値)", &world_.resources.balanceEditor_.bossBulletPenetration, 0.1f, 0.0f, 999.0f);
        ImGui::Checkbox("ランダム拡散", &world_.resources.balanceEditor_.bossRandomSpread);
        if (world_.resources.enemy_) {
            ImGui::Text("ボスレベル: %d", world_.resources.enemy_->GetLevel());
            ImGui::Text("ボス経験値: %u", world_.resources.enemy_->GetEnemyExp());
        }
    }

    if (ImGui::CollapsingHeader("敵RPG / 陣営", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("経験値敵をボスと敵対させる", &world_.resources.balanceEditor_.expEnemyHostileToBoss);
        ImGui::DragInt("ボスから経験値敵へのダメージ", &world_.resources.balanceEditor_.bossExpEnemyDamage, 1.0f, 1, 999);
        ImGui::DragInt("ボスが経験値敵撃破で回復", &world_.resources.balanceEditor_.bossHealOnExpEnemyKill, 1.0f, 0, 999);
        ImGui::DragInt("ボスの必要撃破数/レベル", &world_.resources.balanceEditor_.bossKillsPerLevel, 1.0f, 1, 99);
        ImGui::DragInt("ボス最大HP増加/レベル", &world_.resources.balanceEditor_.bossMaxHpGainPerLevel, 1.0f, 0, 999);
        ImGui::DragInt("ボス攻撃力増加/レベル", &world_.resources.balanceEditor_.bossDamageGainPerLevel, 1.0f, 0, 99);
        ImGui::Checkbox("ボスのレベリング狩りモード", &world_.resources.balanceEditor_.bossLevelingModeEnabled);
        ImGui::DragFloat("狩り開始 プレイヤー距離", &world_.resources.balanceEditor_.bossLevelingEnterDistance, 0.5f, 1.0f, 120.0f);
        ImGui::DragFloat("狩り終了 プレイヤー距離", &world_.resources.balanceEditor_.bossLevelingExitDistance, 0.5f, 0.5f, 120.0f);
        ImGui::DragFloat("狩り探索半径", &world_.resources.balanceEditor_.bossLevelingSearchRadius, 1.0f, 1.0f, 200.0f);
        ImGui::DragFloat("180度旋回にかかる秒数", &world_.resources.balanceEditor_.bossAimTurnHalfSeconds, 0.05f, 0.05f, 5.0f);
        if (world_.resources.enemy_) {
            ImGui::Text("現在のボスモード: %s", world_.resources.enemy_->IsLevelingModeActive() ? "レベリング狩り" : "プレイヤー圧力");
        }
        ImGui::TextWrapped("有効にすると、ボスと経験値敵が敵対陣営として衝突します。ボスは経験値敵を倒すと回復し、レベルアップします。");
    }

    ImGui::Separator();
    if (ImGui::Button("実行中ゲームへ適用")) {
        world_.resources.currentLevelData_.balance = world_.gameplaySettings->BuildBalanceJsonFromEditor();
        world_.resources.enemyManager_->SetDefaultRandomSpawnEnabled(world_.resources.balanceEditor_.defaultRandomSpawnEnabled);
        world_.levelRuntime->ApplyLevelBalance(world_.resources.currentLevelData_.balance);
        world_.resources.balanceEditor_.statusMessage = "Applied balance to running game.";
    }
    ImGui::SameLine();
    if (ImGui::Button("level_test.jsonへ保存")) {
        world_.gameplaySettings->SaveBalanceEditorToLevelFile("resources/levels/level_test.json");
    }
    ImGui::SameLine();
    if (ImGui::Button("AI引き継ぎMD出力")) {
        if (world_.gameplaySettings->WriteBalanceAIHandoff("resources/levels/ai_balance_handoff.md")) {
            world_.resources.balanceEditor_.statusMessage = "Wrote resources/levels/ai_balance_handoff.md";
        } else {
            world_.resources.balanceEditor_.statusMessage = "Failed to write AI handoff file.";
        }
    }
    if (ImGui::Button("JSONから再読み込み")) {
        LevelData levelData;
        if (world_.levelRuntime->LoadLevelFile(levelData)) {
            world_.gameplaySettings->LoadBalanceEditorFromJson(levelData.balance);
            world_.resources.balanceEditor_.statusMessage = "Reloaded balance editor values from JSON.";
        } else {
            world_.resources.balanceEditor_.statusMessage = "Failed to reload level JSON.";
        }
    }

    if (!world_.resources.balanceEditor_.statusMessage.empty()) {
        ImGui::TextWrapped("%s", world_.resources.balanceEditor_.statusMessage.c_str());
    }
    if (!embedded) {
        ImGui::End();
    }
#else
    (void)embedded;
#endif
}

void GameplayEditor::ApplyLevelObject(const LevelObject& levelObject, bool allowBossSpawn)
{
    if (levelObject.type == "PlayerSpawn" || levelObject.type == "BossSpawn") {
        if (!allowBossSpawn) {
            return;
        }
        std::cerr << "[LevelLoader] Spawn markers are ignored after initialization: " << levelObject.name << std::endl;
        return;
    }
    if (levelObject.type == "Enemy") {
        if (world_.gameplayQueries->IsTutorialCombatSuppressed()) {
            return;
        }
        const int hp = ReadCustomInt(levelObject.customProperties, "hp", -1);
        world_.resources.enemyManager_->SpawnLevelEnemy(levelObject.transform.translate, levelObject.prefab, hp);
        return;
    }
    if (levelObject.type == "SpawnArea") {
        if (world_.gameplayQueries->IsTutorialCombatSuppressed()) {
            return;
        }
        AddLevelSpawnAreaFromObject(levelObject);
        return;
    }
    if (levelObject.type == "Obstacle") {
        world_.resources.stage_->AddLevelObstacle(levelObject.transform, levelObject.prefab);
        world_.presentation.stagePostCacheValid_ = false;
        return;
    }
    if (levelObject.type == "Item") {
        world_.levelRuntime->AddLevelItem(levelObject);
        return;
    }
    if (levelObject.type == "MESH") {
        const std::filesystem::path modelPath = std::filesystem::path("resources") / levelObject.prefab;
        if (levelObject.prefab.empty() || !std::filesystem::exists(modelPath)) {
            std::cerr << "[LevelLoader] Skipped MESH with missing model: " << levelObject.prefab << " (" << levelObject.name << ")"
                      << std::endl;
            return;
        }

        LevelVisualObject visual{};
        visual.name = levelObject.name;
        visual.object = std::make_unique<cg2::Object3d>();
        visual.object->Initialize();
        visual.object->SetModel(levelObject.prefab);
        visual.object->SetTransform(levelObject.transform);
        visual.object->Update();
        world_.resources.levelItems_.push_back(std::move(visual));
        return;
    }

    std::cerr << "[LevelLoader] Unsupported object type: " << levelObject.type << " (" << levelObject.name << ")" << std::endl;
}

void GameplayEditor::AddLevelSpawnArea(const LevelSpawnArea& spawnArea)
{
    EnemyManager::SpawnArea area{};
    area.name = spawnArea.name;
    area.prefab = spawnArea.prefab;
    area.center = spawnArea.center;
    area.size = spawnArea.size;
    area.spawnInterval = spawnArea.spawnInterval;
    area.maxAlive = spawnArea.maxAlive;
    area.hp = spawnArea.hp;
    area.enabled = spawnArea.enabled;
    world_.resources.enemyManager_->AddLevelSpawnArea(area);
}

void GameplayEditor::AddLevelSpawnAreaFromObject(const LevelObject& levelObject)
{
    LevelSpawnArea spawnArea{};
    spawnArea.name = levelObject.name;
    spawnArea.prefab = levelObject.prefab;
    spawnArea.center = levelObject.transform.translate;
    spawnArea.size = levelObject.transform.scale;
    spawnArea.spawnInterval = ReadCustomFloat(levelObject.customProperties, "spawnInterval", 2.0f);
    spawnArea.maxAlive = ReadCustomInt(levelObject.customProperties, "maxAlive", 8);
    spawnArea.hp = ReadCustomInt(levelObject.customProperties, "hp", -1);
    spawnArea.enabled = ReadCustomBool(levelObject.customProperties, "enabled", true);
    spawnArea.prefab = ReadCustomString(levelObject.customProperties, "enemyPrefab", spawnArea.prefab);
    AddLevelSpawnArea(spawnArea);
}

void GameplayEditor::UpdateLevelBossPhases()
{
    if (!world_.resources.enemy_ || world_.resources.enemy_->GetMaxHp() <= 0) {
        return;
    }

    // HP比が閾値以下になった未発動の段階を配列順で適用する。同じ更新で複数段階が成立し得る。
    const float hpRate = static_cast<float>(world_.resources.enemy_->GetHp()) / static_cast<float>(world_.resources.enemy_->GetMaxHp());
    for (RuntimeBossPhase& runtimePhase : world_.resources.levelBossPhases_) {
        if (runtimePhase.activated || hpRate > runtimePhase.phase.startHpRate) {
            continue;
        }

        runtimePhase.activated = true;
        world_.combat.screenEffectDirector_.TriggerBossPhaseChange();
        world_.combatFlow->SetEventCallout(GetBossPhaseDisplayName(runtimePhase.phase), 1.10f);
        world_.presentation.cameraShakeTimer_ = (std::max)(world_.presentation.cameraShakeTimer_, 0.30f);
        world_.presentation.cameraShakeDuration_ = 0.30f;
        world_.presentation.cameraShakePower_ = (std::max)(world_.presentation.cameraShakePower_, 0.45f);
        std::cerr << "[Level AI-ditor] Activate boss phase: " << runtimePhase.phase.name << std::endl;
        if (!runtimePhase.phase.message.empty()) {
            std::cerr << "[Level AI-ditor] " << runtimePhase.phase.message << std::endl;
        }
        ApplyBossPhaseTuning(runtimePhase.phase);
        for (const LevelObject& object : runtimePhase.phase.objects) {
            ApplyLevelObject(object, true);
        }
    }
}

void GameplayEditor::ApplyBossPhaseTuning(const LevelBossPhase& phase)
{
    if (!phase.customProperties.is_object()) {
        return;
    }

    if (phase.customProperties.contains("bossAttack") && phase.customProperties["bossAttack"].is_object() && world_.resources.enemy_) {
        const nlohmann::json& attackJson = phase.customProperties["bossAttack"];
        Enemy::BossAttackConfig config = world_.resources.enemy_->GetBossAttackConfig();
        config.bulletSpeed = ReadCustomFloat(attackJson, "bulletSpeed", config.bulletSpeed);
        config.bulletCount = ReadCustomInt(attackJson, "bulletCount", config.bulletCount);
        config.spreadAngleDeg = ReadCustomFloat(attackJson, "spreadAngleDeg", config.spreadAngleDeg);
        config.cooldown = ReadCustomFloat(attackJson, "cooldown", config.cooldown);
        config.damage = static_cast<uint32_t>(ReadCustomInt(attackJson, "damage", static_cast<int>(config.damage)));
        config.bulletHp = ReadCustomFloat(attackJson, "bulletHp", config.bulletHp);
        config.bulletPenetration = ReadCustomFloat(attackJson, "bulletPenetration", config.bulletPenetration);
        config.randomSpread = ReadCustomBool(attackJson, "randomSpread", config.randomSpread);
        config.pattern = static_cast<Enemy::BossAttackConfig::Pattern>(
            (std::clamp)(ReadCustomInt(attackJson, "pattern", static_cast<int>(config.pattern)), 0, 3));
        world_.resources.enemy_->SetBossAttackConfig(config);
    }

    if (phase.customProperties.contains("effectPreset") && phase.customProperties["effectPreset"].is_object()) {
        ApplyLevelEffectPreset(phase.customProperties["effectPreset"]);
    }
}

void GameplayEditor::ApplyLevelEffectPreset(const nlohmann::json& effectJson)
{
    if (effectJson.contains("gridColor") && effectJson["gridColor"].is_object()) {
        world_.presentation.worldGridColor_ = ReadJsonVector4(effectJson["gridColor"], world_.presentation.worldGridColor_);
    }
    if (effectJson.contains("enemyGridColor") && effectJson["enemyGridColor"].is_object()) {
        world_.presentation.enemyGridColor_ = ReadJsonVector4(effectJson["enemyGridColor"], world_.presentation.enemyGridColor_);
    }
    if (effectJson.contains("bossOutlineColor") && effectJson["bossOutlineColor"].is_object() && world_.resources.enemyPostEffect_) {
        cg2::BloomParam& enemyPost = world_.resources.enemyPostEffect_->GetParam();
        enemyPost.outlineColor = ReadJsonVector3(effectJson["bossOutlineColor"], enemyPost.outlineColor);
    }
    if (effectJson.contains("enemyBloomIntensity") && effectJson["enemyBloomIntensity"].is_number() && world_.resources.enemyPostEffect_) {
        world_.resources.enemyPostEffect_->GetParam().intensity = effectJson["enemyBloomIntensity"].get<float>();
    }
    if (effectJson.contains("gridBloomIntensity") && effectJson["gridBloomIntensity"].is_number() && world_.resources.neonGridPostEffect_) {
        world_.resources.neonGridPostEffect_->GetParam().intensity = effectJson["gridBloomIntensity"].get<float>();
    }
    if (effectJson.contains("cameraShakePower") && effectJson["cameraShakePower"].is_number()) {
        world_.presentation.cameraShakePower_ = effectJson["cameraShakePower"].get<float>();
        world_.presentation.cameraShakeTimer_ = world_.presentation.cameraShakeDuration_;
    }
}

void GameplayEditor::QueueLevelEditorPreview()
{
    if (!world_.resources.neonGridRenderer_) {
        return;
    }

    for (const LevelObject& object : world_.resources.currentLevelData_.objects) {
        QueueLevelObjectPreview(object, {0.45f, 0.95f, 1.0f, 0.65f});
    }
    for (const LevelSpawnArea& spawnArea : world_.resources.currentLevelData_.spawnAreas) {
        QueueLevelSpawnAreaPreview(spawnArea, {1.0f, 0.85f, 0.2f, 0.70f});
    }
    for (const RuntimeBossPhase& runtimePhase : world_.resources.levelBossPhases_) {
        const cg2::Vector4 color =
            runtimePhase.activated ? cg2::Vector4{1.0f, 0.25f, 0.25f, 0.85f} : cg2::Vector4{0.85f, 0.35f, 1.0f, 0.45f};
        for (const LevelObject& object : runtimePhase.phase.objects) {
            QueueLevelObjectPreview(object, color);
        }
    }
}

void GameplayEditor::QueueLevelObjectPreview(const LevelObject& levelObject, const cg2::Vector4& color)
{
    if (levelObject.type == "SpawnArea") {
        LevelSpawnArea spawnArea{};
        spawnArea.name = levelObject.name;
        spawnArea.prefab = levelObject.prefab;
        spawnArea.center = levelObject.transform.translate;
        spawnArea.size = levelObject.transform.scale;
        QueueLevelSpawnAreaPreview(spawnArea, color);
        return;
    }

    if (levelObject.type == "Obstacle") {
        world_.resources.neonGridRenderer_->QueueRectangle(
            levelObject.transform.translate,
            {MapChip::kBlockWidth * levelObject.transform.scale.x, MapChip::kBlockHeight * levelObject.transform.scale.y, 1.0f}, 0.08f,
            color);
        return;
    }

    const float radius = levelObject.type == "BossSpawn" ? 3.0f : 1.7f;
    world_.resources.neonGridRenderer_->QueueLocalGrid(levelObject.transform.translate, radius, 0.75f, 0.055f, color);
}

void GameplayEditor::QueueLevelSpawnAreaPreview(const LevelSpawnArea& spawnArea, const cg2::Vector4& color)
{
    world_.resources.neonGridRenderer_->QueueRectangle(spawnArea.center, spawnArea.size, 0.09f, color);
    world_.resources.neonGridRenderer_->QueueLocalGrid(spawnArea.center, (std::min)(spawnArea.size.x, spawnArea.size.y) * 0.22f, 1.0f,
                                                       0.045f, color);
}
} // namespace gameplay
