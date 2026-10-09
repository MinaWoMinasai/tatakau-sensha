#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief PerformanceMonitorの処理を担当し、同じプレイの共有状態を借用する。
class PerformanceMonitor {
public:
    /// @brief 借用するワールドを設定する。
    explicit PerformanceMonitor(GameWorld& world) : world_(world) {}
    /// @brief 描画区間の計測値を設定する。
    void SetRenderProfile(const IScene::RenderProfile& profile);

    /// @brief 後処理設定Entriesを初期状態へ戻す。
    void ResetPostProfileEntries();

    /// @brief 後処理設定Entryを追加する。
    void AddPostProfileEntry(const char* name, float ms, bool active);

    /// @brief 後処理設定文字を更新する。
    void UpdatePostProfileText();

    /// @brief 性能BreakdownImGUIを描画する。
    void DrawPerformanceBreakdownImGui();

#if defined(USE_IMGUI) && !defined(NDEBUG)
    /// @brief 性能計測ImGUIを描画する。
    void DrawPerformanceCaptureImGui();
#endif

#if defined(USE_IMGUI) && !defined(NDEBUG)
    /// @brief 性能計測を開始する。
    void StartPerformanceCapture();
#endif

#if defined(USE_IMGUI) && !defined(NDEBUG)
    /// @brief 性能フレームを記録する。
    void CapturePerformanceFrame();
#endif

#if defined(USE_IMGUI) && !defined(NDEBUG)
    /// @brief 性能計測ファイルを書き込む。
    bool WritePerformanceCaptureFiles();
#endif

    /// @brief 後処理設定Category有効であるか判定する。
    bool IsPostProfileCategoryEnabled(const char* category) const;

    /// @brief 後処理設定方式名前を返す。
    const char* GetPostProfileModeName() const;

private:
    GameWorld& world_;
};
} // namespace gameplay
