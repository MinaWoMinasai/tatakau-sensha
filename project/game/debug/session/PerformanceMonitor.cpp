#include "game/debug/session/PerformanceMonitor.h"
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

// namespace

void PerformanceMonitor::SetRenderProfile(const IScene::RenderProfile& profile)
{
    world_.presentation.renderProfile_ = profile;
#if defined(USE_IMGUI) && !defined(NDEBUG)
    CapturePerformanceFrame();
#endif
}

void PerformanceMonitor::ResetPostProfileEntries()
{
    world_.presentation.postProfileEntryCount_ = 0;
}

void PerformanceMonitor::AddPostProfileEntry(const char* name, float ms, bool active)
{
    if (world_.presentation.postProfileEntryCount_ >= world_.presentation.postProfileEntries_.size()) {
        return;
    }
    world_.presentation.postProfileEntries_[world_.presentation.postProfileEntryCount_] = {name, ms, active};
    world_.presentation.postProfileAccumulatedMs_[world_.presentation.postProfileEntryCount_] += ms;
    ++world_.presentation.postProfileEntryCount_;
}

void PerformanceMonitor::UpdatePostProfileText()
{
#ifdef USE_IMGUI
    ++world_.presentation.postProfileAccumulatedFrames_;
    if (world_.presentation.postProfileAccumulatedFrames_ < 15) {
        return;
    }

    char text[768]{};
    int offset = std::snprintf(text, sizeof(text), "CG2  PostProfile F8 hide F9 mode | %s", GetPostProfileModeName());
    const int frames = (std::max)(1, world_.presentation.postProfileAccumulatedFrames_);
    for (size_t i = 0; i < world_.presentation.postProfileEntryCount_ && offset > 0 && offset < static_cast<int>(sizeof(text)); ++i) {
        const float avgMs = world_.presentation.postProfileAccumulatedMs_[i] / static_cast<float>(frames);
        world_.presentation.postProfileAverageMs_[i] = avgMs;
        offset += std::snprintf(text + offset, sizeof(text) - static_cast<size_t>(offset), " | %s%s %.2fms",
                                world_.presentation.postProfileEntries_[i].active ? "" : "(off) ",
                                world_.presentation.postProfileEntries_[i].name, avgMs);
    }

    SetWindowTextA(cg2::WinApp::GetInstance()->GetHwnd(), text);
    world_.presentation.postProfileAccumulatedMs_.fill(0.0f);
    world_.presentation.postProfileAccumulatedFrames_ = 0;
#endif // USE_IMGUI
}

void PerformanceMonitor::DrawPerformanceBreakdownImGui()
{
#ifdef USE_IMGUI
    const float fps = (std::max)(1.0f, ImGui::GetIO().Framerate);
    const float frameMs =
        world_.presentation.renderProfile_.frameTotalMs > 0.0f ? world_.presentation.renderProfile_.frameTotalMs : 1000.0f / fps;
    auto fpsGainIfRemoved = [frameMs, fps](float ms) -> float {
        if (ms <= 0.0f || ms >= frameMs - 0.01f) {
            return 0.0f;
        }
        return 1000.0f / (frameMs - ms) - fps;
    };
    const float measuredTopLevelMs = world_.presentation.renderProfile_.messagePumpMs +
                                     world_.presentation.renderProfile_.inputImGuiBeginMs +
                                     world_.presentation.renderProfile_.engineUpdateMs + world_.presentation.renderProfile_.sceneUpdateMs +
                                     world_.presentation.renderProfile_.imguiBuildMs + world_.presentation.renderProfile_.drawSetupMs +
                                     world_.presentation.renderProfile_.drawRecordMs + world_.presentation.renderProfile_.imguiDrawMs +
                                     world_.presentation.renderProfile_.postDrawMs;
    const float unmeasuredMs = (std::max)(0.0f, frameMs - measuredTopLevelMs);

    ImGui::Text("現在FPS %.2f / 計測フレーム %.3fms", fps, frameMs);
    ImGui::Text("合計対象 %.3fms + 未計測 %.3fms = %.3fms (100.0%%)", measuredTopLevelMs, unmeasuredMs, frameMs);
    const bool gpuValidationEnabled = cg2::Object3dCommon::GetInstance()->GetDxCommon()->IsGpuBasedValidationEnabled();
    ImGui::TextColored(gpuValidationEnabled ? ImVec4{1.0f, 0.45f, 0.30f, 1.0f} : ImVec4{0.45f, 1.0f, 0.65f, 1.0f},
                       "D3D12 GPU-Based Validation: %s", gpuValidationEnabled ? "ON (計測には非常に重い)" : "OFF");
    ImGui::TextWrapped(
        "[合計] 行だけで1フレームを分割します。[内訳] は親項目の詳細なので、合計には二重加算しません。Fence待ちはGPU処理完了待ちを含みます。");

    if (ImGui::BeginTable("PerformanceBreakdownTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("区分");
        ImGui::TableSetupColumn("項目");
        ImGui::TableSetupColumn("状態");
        ImGui::TableSetupColumn("直近ms");
        ImGui::TableSetupColumn("FPS影響");
        ImGui::TableSetupColumn("比率");
        ImGui::TableHeadersRow();

        auto row = [&](const char* kind, const char* name, const char* state, float ms) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(kind);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(name);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(state);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.3f", ms);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("+%.2f fps", fpsGainIfRemoved(ms));
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%.1f%%", frameMs > 0.0f ? (ms / frameMs) * 100.0f : 0.0f);
        };
        auto totalRow = [&](const char* name, float ms) {
            row("合計", name, "ON", ms);
        };
        auto detailRow = [&](const char* name, bool active, float ms) {
            row("内訳", name, active ? "ON" : "OFF", ms);
        };

        totalRow("Windows Message", world_.presentation.renderProfile_.messagePumpMs);
        totalRow("Input + ImGui Begin", world_.presentation.renderProfile_.inputImGuiBeginMs);
        totalRow("Engine Common Update", world_.presentation.renderProfile_.engineUpdateMs);
        totalRow("Scene Update / UI構築", world_.presentation.renderProfile_.sceneUpdateMs);
        totalRow("ImGui Build", world_.presentation.renderProfile_.imguiBuildMs);
        totalRow("Draw Setup", world_.presentation.renderProfile_.drawSetupMs);
        totalRow("Draw Command Record", world_.presentation.renderProfile_.drawRecordMs);
        totalRow("ImGui Draw Command", world_.presentation.renderProfile_.imguiDrawMs);
        totalRow("Submit / Present / Wait", world_.presentation.renderProfile_.postDrawMs);
        row("合計", "未計測・計測誤差", "-", unmeasuredMs);

        detailRow("  Scene PostEffect3D", true, world_.presentation.renderProfile_.scenePostMs);
        detailRow("  Global Bloom/Post", true, world_.presentation.renderProfile_.globalBloomMs);
        detailRow("  After Object Post", world_.presentation.renderProfile_.afterPostMs > 0.0f,
                  world_.presentation.renderProfile_.afterPostMs);
        detailRow("  Sprite Pass", true, world_.presentation.renderProfile_.spriteMs);
        detailRow("  CommandList Close", true, world_.presentation.renderProfile_.submitCloseMs);
        detailRow("  ExecuteCommandLists", true, world_.presentation.renderProfile_.submitExecuteMs);
        detailRow("  Present", true, world_.presentation.renderProfile_.presentMs);
        detailRow("  GPU Fence Wait", true, world_.presentation.renderProfile_.fenceWaitMs);
        detailRow("  FPS固定待ち", world_.presentation.renderProfile_.fpsLimitMs > 0.01f, world_.presentation.renderProfile_.fpsLimitMs);
        detailRow("  Allocator/List Reset", true, world_.presentation.renderProfile_.submitResetMs);
        for (size_t i = 0; i < world_.presentation.postProfileEntryCount_; ++i) {
            detailRow(world_.presentation.postProfileEntries_[i].name, world_.presentation.postProfileEntries_[i].active,
                      world_.presentation.postProfileAverageMs_[i]);
        }
        if (world_.resources.player_) {
            const auto& hud = world_.resources.player_->GetUpgradeHudProfileStats();
            const auto& evo = world_.resources.player_->GetEvolutionUiProfileStats();
            char hudState[32]{};
            std::snprintf(hudState, sizeof(hudState), "%s %d draws", hud.visible ? "ON" : "OFF", hud.spriteDraws + hud.textDraws);
            row("内訳", "Upgrade HUD CPU", hudState, hud.totalMs);
            detailRow("  HUD Update", hud.visible, hud.updateMs);
            detailRow("  HUD Sprite", hud.visible, hud.spriteMs);
            detailRow("  HUD Text", hud.visible, hud.textMs);
            char evoState[32]{};
            std::snprintf(evoState, sizeof(evoState), "%s %d draws", evo.visible ? "ON" : "OFF", evo.spriteDraws + evo.textDraws);
            row("内訳", "Evolution UI CPU", evoState, evo.totalMs);
            detailRow("  Evolution Update", evo.visible, evo.updateMs);
            detailRow("  Evolution Sprite", evo.visible, evo.spriteMs);
            detailRow("  Evolution Text", evo.visible, evo.textMs);
        }
        ImGui::EndTable();
    }
#endif
}

#if defined(USE_IMGUI) && !defined(NDEBUG)
void PerformanceMonitor::DrawPerformanceCaptureImGui()
{
    ImGui::SeparatorText("パフォーマンスキャプチャ");
    bool trailAutoFire = world_.resources.player_ && world_.resources.player_->IsDebugAutoFireEnabled();
    if (ImGui::Checkbox("Trailテスト: 自動射撃", &trailAutoFire) && world_.resources.player_) {
        world_.resources.player_->SetDebugAutoFireEnabled(trailAutoFire);
    }
    if (!world_.presentation.performanceCaptureActive_) {
        ImGui::InputText("計測名", world_.presentation.performanceCaptureLabel_.data(),
                         world_.presentation.performanceCaptureLabel_.size());
        ImGui::InputInt("計測フレーム数", &world_.presentation.performanceCaptureFrameCount_);
        world_.presentation.performanceCaptureFrameCount_ = (std::clamp)(world_.presentation.performanceCaptureFrameCount_, 5, 300);
        if (ImGui::Button("計測開始")) {
            StartPerformanceCapture();
        }
    } else {
        ImGui::Text("計測フレーム数: %d", world_.presentation.performanceCaptureFrameCount_);
        ImGui::Text("計測中: %zu / %d frames", world_.presentation.performanceCaptureFrames_.size(),
                    world_.presentation.performanceCaptureFrameCount_);
        ImGui::ProgressBar(static_cast<float>(world_.presentation.performanceCaptureFrames_.size()) /
                               static_cast<float>((std::max)(1, world_.presentation.performanceCaptureFrameCount_)),
                           ImVec2(-1.0f, 0.0f));
    }
    if (!world_.presentation.performanceCaptureLastCsvPath_.empty()) {
        ImGui::TextWrapped("前回保存: %s", world_.presentation.performanceCaptureLastCsvPath_.c_str());
    }
    if (!world_.presentation.performanceCaptureStatus_.empty()) {
        ImGui::TextWrapped("%s", world_.presentation.performanceCaptureStatus_.c_str());
    }
}
#endif

#if defined(USE_IMGUI) && !defined(NDEBUG)
void PerformanceMonitor::StartPerformanceCapture()
{
    world_.presentation.performanceCaptureFrameCount_ = (std::clamp)(world_.presentation.performanceCaptureFrameCount_, 5, 300);
    world_.presentation.performanceCaptureConditions_ = {};
    world_.presentation.performanceCaptureConditions_.label = world_.presentation.performanceCaptureLabel_.data();
    world_.presentation.performanceCaptureConditions_.postProfileMode = world_.presentation.postProfileMode_;
    world_.presentation.performanceCaptureConditions_.postProfileModeName = GetPostProfileModeName();
    world_.presentation.performanceCaptureConditions_.gridPostEnabled =
        world_.presentation.enableNeonGridPostEffect_ && IsPostProfileCategoryEnabled("Grid");
    world_.presentation.performanceCaptureConditions_.stagePostEnabled =
        world_.presentation.enableStagePostEffect_ && IsPostProfileCategoryEnabled("Stage");
    world_.presentation.performanceCaptureConditions_.bulletTrailPostEnabled =
        world_.presentation.enableBulletTrailPostEffect_ && IsPostProfileCategoryEnabled("BulletTrail");
    world_.presentation.performanceCaptureConditions_.playerPostEnabled =
        world_.combat.enablePlayerPostEffect_ && IsPostProfileCategoryEnabled("Player");
    world_.presentation.performanceCaptureConditions_.enemyPostEnabled =
        world_.combat.enableEnemyPostEffect_ && IsPostProfileCategoryEnabled("Enemy");
    world_.presentation.performanceCaptureConditions_.expEnemyPostEnabled =
        world_.presentation.enableExpEnemyPostEffect_ && IsPostProfileCategoryEnabled("ExpEnemy");
    world_.presentation.performanceCaptureConditions_.d3d12DebugLayerEnabled =
        cg2::Object3dCommon::GetInstance()->GetDxCommon()->IsD3D12DebugLayerEnabled();
    if (world_.resources.player_) {
        world_.presentation.performanceCaptureConditions_.upgradeHud = world_.resources.player_->GetUpgradeHudDebugSnapshot();
        world_.presentation.performanceCaptureConditions_.trailAutoFireEnabled = world_.resources.player_->IsDebugAutoFireEnabled();
    }
    world_.presentation.performanceCaptureFrames_.clear();
    world_.presentation.performanceCaptureFrames_.reserve(static_cast<size_t>(world_.presentation.performanceCaptureFrameCount_));
    world_.presentation.performanceCaptureStatus_.clear();
    world_.presentation.performanceCaptureSkipCurrentFrame_ = true;
    world_.presentation.performanceCaptureActive_ = true;
}
#endif

#if defined(USE_IMGUI) && !defined(NDEBUG)
void PerformanceMonitor::CapturePerformanceFrame()
{
    if (!world_.presentation.performanceCaptureActive_) {
        return;
    }
    if (world_.presentation.performanceCaptureSkipCurrentFrame_) {
        world_.presentation.performanceCaptureSkipCurrentFrame_ = false;
        return;
    }

    PerformanceCaptureFrame frame{};
    frame.frameIndex = static_cast<uint32_t>(world_.presentation.performanceCaptureFrames_.size() + 1);
    frame.fps = ImGui::GetIO().Framerate;
    frame.render = world_.presentation.renderProfile_;
    frame.postEntryCount = (std::min)(world_.presentation.postProfileEntryCount_, frame.postEntries.size());
    std::copy_n(world_.presentation.postProfileEntries_.begin(), frame.postEntryCount, frame.postEntries.begin());
    if (world_.resources.player_) {
        frame.upgradeHud = world_.resources.player_->GetUpgradeHudProfileStats();
        frame.evolutionUi = world_.resources.player_->GetEvolutionUiProfileStats();
        const Player::UpgradeHudDebugSnapshot hudSnapshot = world_.resources.player_->GetUpgradeHudDebugSnapshot();
        frame.upgradeHudAfterPlayerUpdate = world_.presentation.upgradeHudAfterPlayerUpdate_;
        frame.upgradeHudAfterCollision = world_.presentation.upgradeHudAfterCollision_;
        frame.upgradeHudAtCapture = hudSnapshot;
        frame.playerLevel = hudSnapshot.playerLevel;
        frame.skillPoints = hudSnapshot.skillPoints;
        frame.upgradeHudListVisible = hudSnapshot.listActuallyVisible;
    }
    frame.enemyCount =
        world_.resources.enemy_ && (world_.gameplayQueries->IsRunRivalActive() && !world_.resources.enemy_->IsDead()) ? 1 : 0;
    frame.expEnemyCount = world_.resources.enemyManager_ ? world_.resources.enemyManager_->GetEnemyCount() : 0;
    if (world_.resources.bulletManager_) {
        const BulletManager::BulletCounts counts = world_.resources.bulletManager_->GetBulletCounts();
        frame.bulletCount = world_.resources.bulletManager_->GetBulletCount();
        frame.playerBulletCount = counts.player;
        frame.hostileExpEnemyBulletCount = counts.hostileExpEnemy;
        frame.enemyBulletCount = counts.enemy + counts.hostileExpEnemy;
        frame.bulletTrailCount = world_.resources.bulletManager_->GetTrailInstanceCount();
        frame.trailDrawStats = world_.resources.bulletManager_->GetTrailDrawStats();
    }
    frame.playerLaserCount = world_.presentation.playerLaserBeams_.size();
    frame.playerMineCount = world_.presentation.playerMines_.size();
    frame.playerMeleeSlashCount = world_.presentation.playerMeleeSlashes_.size();
    frame.neonTriangleParticleCount = world_.presentation.neonTriangleParticles_.size();
    world_.presentation.performanceCaptureFrames_.push_back(frame);

    if (world_.presentation.performanceCaptureFrames_.size() >= static_cast<size_t>(world_.presentation.performanceCaptureFrameCount_)) {
        world_.presentation.performanceCaptureActive_ = false;
        if (WritePerformanceCaptureFiles()) {
            world_.presentation.performanceCaptureStatus_ = "CSVとSummaryを保存しました。";
        } else if (world_.presentation.performanceCaptureStatus_.empty()) {
            world_.presentation.performanceCaptureStatus_ = "パフォーマンス計測結果の保存に失敗しました。";
        }
    }
}
#endif

#if defined(USE_IMGUI) && !defined(NDEBUG)
bool PerformanceMonitor::WritePerformanceCaptureFiles()
{
    if (world_.presentation.performanceCaptureFrames_.empty()) {
        return false;
    }

    const std::filesystem::path outputDirectory = "logs/performance";
    std::error_code directoryError;
    std::filesystem::create_directories(outputDirectory, directoryError);
    if (directoryError) {
        world_.presentation.performanceCaptureStatus_ = "logs/performanceフォルダを作成できませんでした。";
        return false;
    }

    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
    localtime_s(&localTime, &time);
    std::ostringstream timestamp;
    timestamp << std::put_time(&localTime, "%Y%m%d_%H%M%S");
    auto makeSafeFileComponent = [](const std::string& value) {
        std::string result;
        result.reserve(value.size());
        bool previousUnderscore = false;
        for (const unsigned char c : value) {
            if (c >= 0x80 || std::isalnum(c) || c == '-' || c == '_') {
                result.push_back(static_cast<char>(c));
                previousUnderscore = c == '_';
            } else if (!previousUnderscore) {
                result.push_back('_');
                previousUnderscore = true;
            }
        }
        while (!result.empty() && result.front() == '_') {
            result.erase(result.begin());
        }
        while (!result.empty() && result.back() == '_') {
            result.pop_back();
        }
        return result;
    };
    const std::string safeLabel = makeSafeFileComponent(world_.presentation.performanceCaptureConditions_.label);
    std::string baseName = "performance_" + timestamp.str();
    if (!safeLabel.empty()) {
        baseName += "_" + safeLabel;
    }
    std::filesystem::path csvPath = outputDirectory / (baseName + ".csv");
    std::filesystem::path summaryPath = outputDirectory / (baseName + "_summary.txt");
    for (int suffix = 1; std::filesystem::exists(csvPath) || std::filesystem::exists(summaryPath); ++suffix) {
        const std::string suffixedBaseName = baseName + "_" + std::to_string(suffix);
        csvPath = outputDirectory / (suffixedBaseName + ".csv");
        summaryPath = outputDirectory / (suffixedBaseName + "_summary.txt");
    }

    std::vector<const char*> postNames;
    postNames.reserve(world_.presentation.postProfileEntries_.size());
    for (const PerformanceCaptureFrame& frame : world_.presentation.performanceCaptureFrames_) {
        for (size_t i = 0; i < frame.postEntryCount; ++i) {
            const char* name = frame.postEntries[i].name;
            const bool exists = std::any_of(postNames.begin(), postNames.end(), [name](const char* current) {
                return std::strcmp(current, name) == 0;
            });
            if (!exists) {
                postNames.push_back(name);
            }
        }
    }
    auto makeColumnName = [](const char* name) {
        std::string result = "post_";
        bool previousUnderscore = false;
        for (const unsigned char c : std::string(name)) {
            if (std::isalnum(c)) {
                result.push_back(static_cast<char>(std::tolower(c)));
                previousUnderscore = false;
            } else if (!previousUnderscore) {
                result.push_back('_');
                previousUnderscore = true;
            }
        }
        if (!result.empty() && result.back() == '_') {
            result.pop_back();
        }
        return result;
    };
    auto findPostEntry = [](const PerformanceCaptureFrame& frame, const char* name) -> const PostProfileEntry* {
        for (size_t i = 0; i < frame.postEntryCount; ++i) {
            if (std::strcmp(frame.postEntries[i].name, name) == 0) {
                return &frame.postEntries[i];
            }
        }
        return nullptr;
    };
    auto escapeCsv = [](const std::string& value) {
        std::string escaped = "\"";
        for (const char c : value) {
            escaped += c;
            if (c == '\"') {
                escaped += '\"';
            }
        }
        escaped += '\"';
        return escaped;
    };

    std::ofstream csv(csvPath);
    if (!csv.is_open()) {
        world_.presentation.performanceCaptureStatus_ = "CSVファイルを開けませんでした。";
        return false;
    }
    csv << "frame,fps,frame_total_ms,windows_message_ms,input_imgui_begin_ms,engine_common_update_ms,"
           "scene_update_ui_ms,imgui_build_ms,draw_setup_ms,draw_command_record_ms,imgui_draw_command_ms,"
           "submit_present_wait_ms,scene_post_effect_3d_ms,global_bloom_post_ms,after_object_post_ms,sprite_pass_ms,"
           "command_list_close_ms,execute_command_lists_ms,present_ms,gpu_fence_wait_ms,fps_limit_wait_ms,allocator_list_reset_ms,"
           "upgrade_hud_visible,upgrade_hud_total_ms,upgrade_hud_update_ms,upgrade_hud_sprite_ms,upgrade_hud_text_ms,"
           "upgrade_hud_base_text_refreshed,upgrade_hud_base_text_refresh_ms,upgrade_hud_exp_label_refresh_ms,"
           "upgrade_hud_level_label_refresh_ms,upgrade_hud_base_text_set_style_ms,upgrade_hud_base_text_set_text_ms,"
           "upgrade_hud_base_text_set_text_rebuild_ms,upgrade_hud_base_text_rebuild_texture_ms,"
           "upgrade_hud_base_text_get_or_create_texture_ms,upgrade_hud_base_text_sprite_texture_update_ms,"
           "upgrade_hud_base_text_cache_file_existed_count,upgrade_hud_base_text_generated_png_count,"
           "upgrade_hud_list_text_refreshed,upgrade_hud_list_text_refresh_ms,upgrade_hud_list_text_set_style_ms,"
           "upgrade_hud_list_text_set_text_ms,upgrade_hud_list_text_set_text_rebuild_ms,"
           "upgrade_hud_list_text_rebuild_texture_ms,upgrade_hud_list_text_get_or_create_texture_ms,"
           "upgrade_hud_list_text_sprite_texture_update_ms,upgrade_hud_list_text_cache_file_existed_count,"
           "upgrade_hud_list_text_generated_png_count,"
           "upgrade_hud_sprite_draw_count,upgrade_hud_text_draw_count,evolution_ui_visible,evolution_ui_total_ms,"
           "evolution_ui_update_ms,evolution_ui_sprite_ms,evolution_ui_text_ms,evolution_ui_sprite_draw_count,"
           "evolution_ui_text_draw_count,"
           "hud_after_player_update_level,hud_after_player_update_exp,hud_after_player_update_skill_points,"
           "hud_after_player_update_visible,hud_after_player_update_list_visibility,hud_after_player_update_list_actually_visible,"
           "hud_after_player_update_is_change_mode,hud_after_player_update_player_is_dead,"
           "hud_after_collision_level,hud_after_collision_exp,hud_after_collision_skill_points,"
           "hud_after_collision_visible,hud_after_collision_list_visibility,hud_after_collision_list_actually_visible,"
           "hud_after_collision_is_change_mode,hud_after_collision_player_is_dead,"
           "hud_at_capture_level,hud_at_capture_exp,hud_at_capture_skill_points,"
           "hud_at_capture_visible,hud_at_capture_list_visibility,hud_at_capture_list_actually_visible,"
           "hud_at_capture_is_change_mode,hud_at_capture_player_is_dead,"
           "enemy_count,exp_enemy_count,bullet_count,player_bullet_count,enemy_bullet_count,"
           "hostile_exp_enemy_bullet_count,bullet_trail_count,trail_total_instances,trail_active_instances,"
           "trail_drawable_instances,trail_total_points,trail_requested_vertices,trail_generated_vertices,trail_draw_calls,"
           "trail_vertex_capacity,trail_capacity_hit,trail_truncated_vertices,trail_draw_cpu_ms,"
           "trail_vertex_build_cpu_ms,trail_draw_command_cpu_ms,"
           "player_laser_count,player_mine_count,player_melee_slash_count,"
           "neon_triangle_particle_count,player_level,skill_points,upgrade_hud_list_visible,capture_label,"
           "condition_post_profile_mode,condition_post_profile_mode_index,condition_upgrade_hud_visible,"
           "condition_upgrade_hud_hide_list_without_points,condition_upgrade_hud_draw_list_panels,"
           "condition_upgrade_hud_draw_list_text,condition_upgrade_hud_draw_bottom_bars,condition_upgrade_hud_draw_bottom_text,"
           "condition_upgrade_hud_use_rect_batch,condition_upgrade_hud_use_neon_progress_bars,"
           "condition_upgrade_hud_use_segmented_upgrade_bars,condition_segmented_bar_bloom_enabled,"
           "condition_upgrade_list_text_bloom_enabled,condition_upgrade_hud_max_enhance_point,"
           "condition_grid_post_enabled,condition_stage_post_enabled,condition_bullet_trail_post_enabled,"
           "condition_player_post_enabled,condition_enemy_post_enabled,condition_exp_enemy_post_enabled,"
           "condition_trail_auto_fire_enabled";
    for (const char* name : postNames) {
        const std::string column = makeColumnName(name);
        csv << ',' << column << "_ms," << column << "_active";
    }
    csv << '\n' << std::fixed << std::setprecision(4);

    for (const PerformanceCaptureFrame& frame : world_.presentation.performanceCaptureFrames_) {
        const auto& r = frame.render;
        const auto& hud = frame.upgradeHud;
        const auto& evo = frame.evolutionUi;
        const auto& trail = frame.trailDrawStats;
        const auto& hudAfterPlayerUpdate = frame.upgradeHudAfterPlayerUpdate;
        const auto& hudAfterCollision = frame.upgradeHudAfterCollision;
        const auto& hudAtCapture = frame.upgradeHudAtCapture;
        const auto& conditions = world_.presentation.performanceCaptureConditions_;
        const auto& hudConditions = conditions.upgradeHud;
        csv << frame.frameIndex << ',' << frame.fps << ',' << r.frameTotalMs << ',' << r.messagePumpMs << ',' << r.inputImGuiBeginMs << ','
            << r.engineUpdateMs << ',' << r.sceneUpdateMs << ',' << r.imguiBuildMs << ',' << r.drawSetupMs << ',' << r.drawRecordMs << ','
            << r.imguiDrawMs << ',' << r.postDrawMs << ',' << r.scenePostMs << ',' << r.globalBloomMs << ',' << r.afterPostMs << ','
            << r.spriteMs << ',' << r.submitCloseMs << ',' << r.submitExecuteMs << ',' << r.presentMs << ',' << r.fenceWaitMs << ','
            << r.fpsLimitMs << ',' << r.submitResetMs << ',' << (hud.visible ? 1 : 0) << ',' << hud.totalMs << ',' << hud.updateMs << ','
            << hud.spriteMs << ',' << hud.textMs << ',' << (hud.baseTextRefreshed ? 1 : 0) << ',' << hud.baseTextRefreshMs << ','
            << hud.expLabelRefreshMs << ',' << hud.levelLabelRefreshMs << ',' << hud.baseTextSetStyleMs << ',' << hud.baseTextSetTextMs
            << ',' << hud.baseTextSetTextRebuildMs << ',' << hud.baseTextRebuildTextureMs << ',' << hud.baseTextGetOrCreateTextureMs << ','
            << hud.baseTextSpriteSetTextureMs << ',' << hud.baseTextCacheFileExistedCount << ',' << hud.baseTextGeneratedPngCount << ','
            << (hud.listTextRefreshed ? 1 : 0) << ',' << hud.listTextRefreshMs << ',' << hud.listTextSetStyleMs << ','
            << hud.listTextSetTextMs << ',' << hud.listTextSetTextRebuildMs << ',' << hud.listTextRebuildTextureMs << ','
            << hud.listTextGetOrCreateTextureMs << ',' << hud.listTextSpriteSetTextureMs << ',' << hud.listTextCacheFileExistedCount << ','
            << hud.listTextGeneratedPngCount << ',' << hud.spriteDraws << ',' << hud.textDraws << ',' << (evo.visible ? 1 : 0) << ','
            << evo.totalMs << ',' << evo.updateMs << ',' << evo.spriteMs << ',' << evo.textMs << ',' << evo.spriteDraws << ','
            << evo.textDraws << ',' << hudAfterPlayerUpdate.playerLevel << ',' << hudAfterPlayerUpdate.exp << ','
            << hudAfterPlayerUpdate.skillPoints << ',' << (hudAfterPlayerUpdate.visible ? 1 : 0) << ','
            << hudAfterPlayerUpdate.listVisibility << ',' << (hudAfterPlayerUpdate.listActuallyVisible ? 1 : 0) << ','
            << (hudAfterPlayerUpdate.isChangeMode ? 1 : 0) << ',' << (hudAfterPlayerUpdate.playerIsDead ? 1 : 0) << ','
            << hudAfterCollision.playerLevel << ',' << hudAfterCollision.exp << ',' << hudAfterCollision.skillPoints << ','
            << (hudAfterCollision.visible ? 1 : 0) << ',' << hudAfterCollision.listVisibility << ','
            << (hudAfterCollision.listActuallyVisible ? 1 : 0) << ',' << (hudAfterCollision.isChangeMode ? 1 : 0) << ','
            << (hudAfterCollision.playerIsDead ? 1 : 0) << ',' << hudAtCapture.playerLevel << ',' << hudAtCapture.exp << ','
            << hudAtCapture.skillPoints << ',' << (hudAtCapture.visible ? 1 : 0) << ',' << hudAtCapture.listVisibility << ','
            << (hudAtCapture.listActuallyVisible ? 1 : 0) << ',' << (hudAtCapture.isChangeMode ? 1 : 0) << ','
            << (hudAtCapture.playerIsDead ? 1 : 0) << ',' << frame.enemyCount << ',' << frame.expEnemyCount << ',' << frame.bulletCount
            << ',' << frame.playerBulletCount << ',' << frame.enemyBulletCount << ',' << frame.hostileExpEnemyBulletCount << ','
            << frame.bulletTrailCount << ',' << trail.totalInstances << ',' << trail.activeInstances << ',' << trail.drawableInstances
            << ',' << trail.totalPoints << ',' << trail.requestedVertices << ',' << trail.generatedVertices << ',' << trail.drawCalls << ','
            << trail.vertexCapacity << ',' << (trail.capacityHit ? 1 : 0) << ',' << trail.truncatedVertices << ',' << trail.drawCpuMs << ','
            << trail.vertexBuildCpuMs << ',' << trail.drawCommandCpuMs << ',' << frame.playerLaserCount << ',' << frame.playerMineCount
            << ',' << frame.playerMeleeSlashCount << ',' << frame.neonTriangleParticleCount << ',' << frame.playerLevel << ','
            << frame.skillPoints << ',' << (frame.upgradeHudListVisible ? 1 : 0) << ',' << escapeCsv(conditions.label) << ','
            << escapeCsv(conditions.postProfileModeName) << ',' << conditions.postProfileMode << ',' << (hudConditions.visible ? 1 : 0)
            << ',' << (hudConditions.hideListWithoutPoints ? 1 : 0) << ',' << (hudConditions.drawListPanels ? 1 : 0) << ','
            << (hudConditions.drawListText ? 1 : 0) << ',' << (hudConditions.drawBottomBars ? 1 : 0) << ','
            << (hudConditions.drawBottomText ? 1 : 0) << ',' << (hudConditions.useRectBatch ? 1 : 0) << ','
            << (hudConditions.useNeonProgressBars ? 1 : 0) << ',' << (hudConditions.useSegmentedUpgradeBars ? 1 : 0) << ','
            << (hudConditions.segmentedBarBloomEnabled ? 1 : 0) << ',' << (hudConditions.listTextBloomEnabled ? 1 : 0) << ','
            << hudConditions.maxEnhancePoint << ',' << (conditions.gridPostEnabled ? 1 : 0) << ',' << (conditions.stagePostEnabled ? 1 : 0)
            << ',' << (conditions.bulletTrailPostEnabled ? 1 : 0) << ',' << (conditions.playerPostEnabled ? 1 : 0) << ','
            << (conditions.enemyPostEnabled ? 1 : 0) << ',' << (conditions.expEnemyPostEnabled ? 1 : 0) << ','
            << (conditions.trailAutoFireEnabled ? 1 : 0);
        for (const char* name : postNames) {
            const PostProfileEntry* entry = findPostEntry(frame, name);
            csv << ',' << (entry ? entry->ms : 0.0f) << ',' << (entry && entry->active ? 1 : 0);
        }
        csv << '\n';
    }
    csv.close();
    if (!csv) {
        world_.presentation.performanceCaptureStatus_ = "CSVファイルの書き込みに失敗しました。";
        return false;
    }

    std::ofstream summary(summaryPath);
    if (!summary.is_open()) {
        world_.presentation.performanceCaptureStatus_ = "Summaryファイルを開けませんでした。";
        return false;
    }
    const auto& conditions = world_.presentation.performanceCaptureConditions_;
    const auto& hudConditions = conditions.upgradeHud;
    auto boolText = [](bool value) {
        return value ? "true" : "false";
    };
    summary << "=== Performance Capture Summary ===\n\n"
            << "Capture Label: " << conditions.label << "\n"
            << "Frames: " << world_.presentation.performanceCaptureFrames_.size() << "\n\n"
            << "=== Capture Conditions ===\n\n"
            << "Post Profile Mode: " << conditions.postProfileModeName << " (" << conditions.postProfileMode << ")\n\n"
            << "Player Level: " << hudConditions.playerLevel << "\n"
            << "Skill Points: " << hudConditions.skillPoints << "\n\n"
            << "Upgrade HUD Visible: " << boolText(hudConditions.visible) << "\n"
            << "Hide List Without Points: " << boolText(hudConditions.hideListWithoutPoints) << "\n"
            << "Upgrade List Actually Visible: " << boolText(hudConditions.listActuallyVisible) << "\n"
            << "Upgrade List Panels: " << boolText(hudConditions.drawListPanels) << "\n"
            << "Upgrade List Text: " << boolText(hudConditions.drawListText) << "\n"
            << "Segmented Upgrade Bars: " << boolText(hudConditions.useSegmentedUpgradeBars) << "\n"
            << "Segmented Bar Bloom: " << boolText(hudConditions.segmentedBarBloomEnabled) << "\n"
            << "Upgrade List Text Bloom: " << boolText(hudConditions.listTextBloomEnabled) << "\n"
            << "Bottom Bars: " << boolText(hudConditions.drawBottomBars) << "\n"
            << "Bottom Text: " << boolText(hudConditions.drawBottomText) << "\n"
            << "Rect Batch: " << boolText(hudConditions.useRectBatch) << "\n"
            << "Neon Progress Bars: " << boolText(hudConditions.useNeonProgressBars) << "\n"
            << "Max Enhance Point: " << hudConditions.maxEnhancePoint << "\n\n"
            << "Grid Post Enabled: " << boolText(conditions.gridPostEnabled) << "\n"
            << "Stage Post Enabled: " << boolText(conditions.stagePostEnabled) << "\n"
            << "Bullet Trail Post Enabled: " << boolText(conditions.bulletTrailPostEnabled) << "\n"
            << "Player Post Enabled: " << boolText(conditions.playerPostEnabled) << "\n"
            << "Enemy Post Enabled: " << boolText(conditions.enemyPostEnabled) << "\n"
            << "Exp Enemy Post Enabled: " << boolText(conditions.expEnemyPostEnabled) << "\n\n"
            << "D3D12 Debug Layer: " << (conditions.d3d12DebugLayerEnabled ? "ON" : "OFF") << "\n\n"
            << "Trail Test Auto Fire: " << boolText(conditions.trailAutoFireEnabled) << "\n\n"
            << "=== Performance ===\n";
    auto writeStats = [&](const char* label, const char* unit, auto getter) {
        double total = 0.0;
        double minimum = (std::numeric_limits<double>::max)();
        double maximum = (std::numeric_limits<double>::lowest)();
        for (const PerformanceCaptureFrame& frame : world_.presentation.performanceCaptureFrames_) {
            const double value = static_cast<double>(getter(frame));
            total += value;
            minimum = (std::min)(minimum, value);
            maximum = (std::max)(maximum, value);
        }
        summary << "\n"
                << label << "\nAverage: " << std::fixed << std::setprecision(3)
                << total / static_cast<double>(world_.presentation.performanceCaptureFrames_.size()) << unit << "\nMin: " << minimum << unit
                << "\nMax: " << maximum << unit << "\n";
    };

    writeStats("FPS", "", [](const auto& f) {
        return f.fps;
    });
    writeStats("Frame Time", " ms", [](const auto& f) {
        return f.render.frameTotalMs;
    });
    writeStats("Windows Message", " ms", [](const auto& f) {
        return f.render.messagePumpMs;
    });
    writeStats("Input + ImGui Begin", " ms", [](const auto& f) {
        return f.render.inputImGuiBeginMs;
    });
    writeStats("Engine Common Update", " ms", [](const auto& f) {
        return f.render.engineUpdateMs;
    });
    writeStats("Scene Update / UI Build", " ms", [](const auto& f) {
        return f.render.sceneUpdateMs;
    });
    writeStats("ImGui Build", " ms", [](const auto& f) {
        return f.render.imguiBuildMs;
    });
    writeStats("Draw Setup", " ms", [](const auto& f) {
        return f.render.drawSetupMs;
    });
    writeStats("Draw Command Record", " ms", [](const auto& f) {
        return f.render.drawRecordMs;
    });
    writeStats("ImGui Draw Command", " ms", [](const auto& f) {
        return f.render.imguiDrawMs;
    });
    writeStats("Submit / Present / Wait", " ms", [](const auto& f) {
        return f.render.postDrawMs;
    });
    writeStats("Scene PostEffect3D", " ms", [](const auto& f) {
        return f.render.scenePostMs;
    });
    writeStats("Global Bloom/Post", " ms", [](const auto& f) {
        return f.render.globalBloomMs;
    });
    writeStats("After Object Post", " ms", [](const auto& f) {
        return f.render.afterPostMs;
    });
    writeStats("Sprite Pass", " ms", [](const auto& f) {
        return f.render.spriteMs;
    });
    writeStats("CommandList Close", " ms", [](const auto& f) {
        return f.render.submitCloseMs;
    });
    writeStats("ExecuteCommandLists", " ms", [](const auto& f) {
        return f.render.submitExecuteMs;
    });
    writeStats("Present", " ms", [](const auto& f) {
        return f.render.presentMs;
    });
    writeStats("GPU Fence Wait", " ms", [](const auto& f) {
        return f.render.fenceWaitMs;
    });
    writeStats("FPS Limit Wait", " ms", [](const auto& f) {
        return f.render.fpsLimitMs;
    });
    writeStats("Allocator/List Reset", " ms", [](const auto& f) {
        return f.render.submitResetMs;
    });
    writeStats("Upgrade HUD Visible", "", [](const auto& f) {
        return f.upgradeHud.visible ? 1 : 0;
    });
    writeStats("Upgrade HUD", " ms", [](const auto& f) {
        return f.upgradeHud.totalMs;
    });
    writeStats("Upgrade HUD Update", " ms", [](const auto& f) {
        return f.upgradeHud.updateMs;
    });
    writeStats("Upgrade HUD Sprite", " ms", [](const auto& f) {
        return f.upgradeHud.spriteMs;
    });
    writeStats("Upgrade HUD Text", " ms", [](const auto& f) {
        return f.upgradeHud.textMs;
    });
    writeStats("Upgrade HUD Base Text Refresh", " ms", [](const auto& f) {
        return f.upgradeHud.baseTextRefreshMs;
    });
    writeStats("Upgrade HUD EXP Label Refresh", " ms", [](const auto& f) {
        return f.upgradeHud.expLabelRefreshMs;
    });
    writeStats("Upgrade HUD Level Label Refresh", " ms", [](const auto& f) {
        return f.upgradeHud.levelLabelRefreshMs;
    });
    writeStats("Upgrade HUD Base Text SetStyle", " ms", [](const auto& f) {
        return f.upgradeHud.baseTextSetStyleMs;
    });
    writeStats("Upgrade HUD Base Text SetText", " ms", [](const auto& f) {
        return f.upgradeHud.baseTextSetTextMs;
    });
    writeStats("Upgrade HUD Base Text SetText Rebuild", " ms", [](const auto& f) {
        return f.upgradeHud.baseTextSetTextRebuildMs;
    });
    writeStats("Upgrade HUD Base Text RebuildTexture", " ms", [](const auto& f) {
        return f.upgradeHud.baseTextRebuildTextureMs;
    });
    writeStats("Upgrade HUD Base Text GetOrCreateTexture", " ms", [](const auto& f) {
        return f.upgradeHud.baseTextGetOrCreateTextureMs;
    });
    writeStats("Upgrade HUD Base Text Sprite Texture Update", " ms", [](const auto& f) {
        return f.upgradeHud.baseTextSpriteSetTextureMs;
    });
    writeStats("Upgrade HUD Base Text Cache File Existed Count", "", [](const auto& f) {
        return f.upgradeHud.baseTextCacheFileExistedCount;
    });
    writeStats("Upgrade HUD Base Text Generated PNG Count", "", [](const auto& f) {
        return f.upgradeHud.baseTextGeneratedPngCount;
    });
    writeStats("Upgrade HUD List Text Refresh", " ms", [](const auto& f) {
        return f.upgradeHud.listTextRefreshMs;
    });
    writeStats("Upgrade HUD List Text SetStyle", " ms", [](const auto& f) {
        return f.upgradeHud.listTextSetStyleMs;
    });
    writeStats("Upgrade HUD List Text SetText", " ms", [](const auto& f) {
        return f.upgradeHud.listTextSetTextMs;
    });
    writeStats("Upgrade HUD List Text SetText Rebuild", " ms", [](const auto& f) {
        return f.upgradeHud.listTextSetTextRebuildMs;
    });
    writeStats("Upgrade HUD List Text RebuildTexture", " ms", [](const auto& f) {
        return f.upgradeHud.listTextRebuildTextureMs;
    });
    writeStats("Upgrade HUD List Text GetOrCreateTexture", " ms", [](const auto& f) {
        return f.upgradeHud.listTextGetOrCreateTextureMs;
    });
    writeStats("Upgrade HUD List Text Sprite Texture Update", " ms", [](const auto& f) {
        return f.upgradeHud.listTextSpriteSetTextureMs;
    });
    writeStats("Upgrade HUD List Text Cache File Existed Count", "", [](const auto& f) {
        return f.upgradeHud.listTextCacheFileExistedCount;
    });
    writeStats("Upgrade HUD List Text Generated PNG Count", "", [](const auto& f) {
        return f.upgradeHud.listTextGeneratedPngCount;
    });
    writeStats("Upgrade HUD Sprite Draw Count", "", [](const auto& f) {
        return f.upgradeHud.spriteDraws;
    });
    writeStats("Upgrade HUD Text Draw Count", "", [](const auto& f) {
        return f.upgradeHud.textDraws;
    });
    writeStats("Evolution UI Visible", "", [](const auto& f) {
        return f.evolutionUi.visible ? 1 : 0;
    });
    writeStats("Evolution UI", " ms", [](const auto& f) {
        return f.evolutionUi.totalMs;
    });
    writeStats("Evolution UI Update", " ms", [](const auto& f) {
        return f.evolutionUi.updateMs;
    });
    writeStats("Evolution UI Sprite", " ms", [](const auto& f) {
        return f.evolutionUi.spriteMs;
    });
    writeStats("Evolution UI Text", " ms", [](const auto& f) {
        return f.evolutionUi.textMs;
    });
    writeStats("Evolution UI Sprite Draw Count", "", [](const auto& f) {
        return f.evolutionUi.spriteDraws;
    });
    writeStats("Evolution UI Text Draw Count", "", [](const auto& f) {
        return f.evolutionUi.textDraws;
    });
    writeStats("Enemy Count", "", [](const auto& f) {
        return f.enemyCount;
    });
    writeStats("Exp Enemy Count", "", [](const auto& f) {
        return f.expEnemyCount;
    });
    writeStats("Bullet Count", "", [](const auto& f) {
        return f.bulletCount;
    });
    writeStats("Player Bullet Count", "", [](const auto& f) {
        return f.playerBulletCount;
    });
    writeStats("Enemy Bullet Count", "", [](const auto& f) {
        return f.enemyBulletCount;
    });
    writeStats("Hostile Exp Enemy Bullet Count", "", [](const auto& f) {
        return f.hostileExpEnemyBulletCount;
    });
    writeStats("Bullet Trail Count", "", [](const auto& f) {
        return f.bulletTrailCount;
    });
    writeStats("Trail Total Instances", "", [](const auto& f) {
        return f.trailDrawStats.totalInstances;
    });
    writeStats("Trail Active Instances", "", [](const auto& f) {
        return f.trailDrawStats.activeInstances;
    });
    writeStats("Trail Drawable Instances", "", [](const auto& f) {
        return f.trailDrawStats.drawableInstances;
    });
    writeStats("Trail Total Points", "", [](const auto& f) {
        return f.trailDrawStats.totalPoints;
    });
    writeStats("Trail Requested Vertices", "", [](const auto& f) {
        return f.trailDrawStats.requestedVertices;
    });
    writeStats("Trail Generated Vertices", "", [](const auto& f) {
        return f.trailDrawStats.generatedVertices;
    });
    writeStats("Trail Draw Calls", "", [](const auto& f) {
        return f.trailDrawStats.drawCalls;
    });
    writeStats("Trail Vertex Capacity", "", [](const auto& f) {
        return f.trailDrawStats.vertexCapacity;
    });
    writeStats("Trail Capacity Hit", "", [](const auto& f) {
        return f.trailDrawStats.capacityHit ? 1 : 0;
    });
    writeStats("Trail Truncated Vertices", "", [](const auto& f) {
        return f.trailDrawStats.truncatedVertices;
    });
    writeStats("Trail Draw CPU", " ms", [](const auto& f) {
        return f.trailDrawStats.drawCpuMs;
    });
    writeStats("Trail Vertex Build CPU", " ms", [](const auto& f) {
        return f.trailDrawStats.vertexBuildCpuMs;
    });
    writeStats("Trail Draw Command CPU", " ms", [](const auto& f) {
        return f.trailDrawStats.drawCommandCpuMs;
    });
    writeStats("Trail Other CPU", " ms", [](const auto& f) {
        return (std::max)(0.0f, f.trailDrawStats.drawCpuMs - f.trailDrawStats.vertexBuildCpuMs - f.trailDrawStats.drawCommandCpuMs);
    });
    const size_t trailCapacityHitFrames =
        static_cast<size_t>(std::count_if(world_.presentation.performanceCaptureFrames_.begin(),
                                          world_.presentation.performanceCaptureFrames_.end(), [](const PerformanceCaptureFrame& frame) {
                                              return frame.trailDrawStats.capacityHit;
                                          }));
    summary << "\nTrail Capacity Hit Frames\nCount: " << trailCapacityHitFrames << " / "
            << world_.presentation.performanceCaptureFrames_.size() << "\n";
    writeStats("Player Laser Count", "", [](const auto& f) {
        return f.playerLaserCount;
    });
    writeStats("Player Mine Count", "", [](const auto& f) {
        return f.playerMineCount;
    });
    writeStats("Player Melee Slash Count", "", [](const auto& f) {
        return f.playerMeleeSlashCount;
    });
    writeStats("Neon Triangle Particle Count", "", [](const auto& f) {
        return f.neonTriangleParticleCount;
    });
    writeStats("Player Level", "", [](const auto& f) {
        return f.playerLevel;
    });
    writeStats("Skill Points", "", [](const auto& f) {
        return f.skillPoints;
    });
    writeStats("Upgrade HUD List Visible", "", [](const auto& f) {
        return f.upgradeHudListVisible ? 1 : 0;
    });
    for (const char* name : postNames) {
        writeStats(name, " ms", [&](const PerformanceCaptureFrame& frame) {
            const PostProfileEntry* entry = findPostEntry(frame, name);
            return entry ? entry->ms : 0.0f;
        });
    }
    summary.close();
    if (!summary) {
        world_.presentation.performanceCaptureStatus_ = "Summaryファイルの書き込みに失敗しました。";
        return false;
    }

    world_.presentation.performanceCaptureLastCsvPath_ = csvPath.generic_string();
    return true;
}
#endif

bool PerformanceMonitor::IsPostProfileCategoryEnabled(const char* category) const
{
    switch (world_.presentation.postProfileMode_) {
    case 1:
        return std::strcmp(category, "Grid") != 0;
    case 2:
        return std::strcmp(category, "Stage") != 0;
    case 3:
        return std::strcmp(category, "BulletTrail") != 0;
    case 4:
        return std::strcmp(category, "Player") != 0;
    case 5:
        return std::strcmp(category, "Enemy") != 0;
    case 6:
        return std::strcmp(category, "ExpEnemy") != 0;
    case 7:
        return std::strcmp(category, "Grid") != 0 && std::strcmp(category, "Stage") != 0 && std::strcmp(category, "BulletTrail") != 0 &&
               std::strcmp(category, "Player") != 0 && std::strcmp(category, "Enemy") != 0 && std::strcmp(category, "ExpEnemy") != 0;
    default:
        return true;
    }
}

const char* PerformanceMonitor::GetPostProfileModeName() const
{
    switch (world_.presentation.postProfileMode_) {
    case 1:
        return "No Grid Post";
    case 2:
        return "No Stage Post";
    case 3:
        return "No Bullet Trail Post";
    case 4:
        return "No Player Post";
    case 5:
        return "No Enemy Post";
    case 6:
        return "No Exp Enemy Post";
    case 7:
        return "No Post Effects";
    default:
        return "All Enabled";
    }
}
} // namespace gameplay
