#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief PerformanceMonitorの処理を担当し、同じプレイの共有状態を借用する。
class PerformanceMonitor {
public:
    explicit PerformanceMonitor(GameWorld& world) : world_(world) {}

    /// @brief 描画区間の計測値を設定する。
    void SetRenderProfile(const IScene::RenderProfile& profile);

    /// @brief ポストエフェクトの計測項目を空にする。
    void ResetPostProfileEntries();

    /// @brief ポストエフェクトの計測項目を追加する。
    void AddPostProfileEntry(const char* name, float ms, bool active);

    /// @brief 後処理設定文字を更新する。
    void UpdatePostProfileText();

    /// @brief 処理時間の内訳をImGuiで表示する。
    void DrawPerformanceBreakdownImGui();

#if defined(USE_IMGUI) && !defined(NDEBUG)
    /// @brief 性能の記録を操作するImGuiの画面を表示する。
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

    /// @brief ポストエフェクトの計測項目のうち、指定した分類が有効かを返す。
    bool IsPostProfileCategoryEnabled(const char* category) const;

    /// @brief 後処理設定方式名前を返す。
    const char* GetPostProfileModeName() const;

private:
    GameWorld& world_;
};
} // namespace gameplay
