#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"

#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)

/// @brief WindowsメッセージをImGuiへ渡すための外部窓口を宣言する。
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif // USE_IMGUI

namespace myEngine {

/// @brief Windowsのウィンドウ生成とメッセージ処理を管理する。
class WinApp {
public:
    // シングルトン
    static WinApp* GetInstance();

    // UI・照準・HUDが使う論理座標。実際のウィンドウサイズや描画解像度とは独立している。
    static const int32_t kClientWidth = 1280;
    static const int32_t kClientHeight = 720;
    // 3D・ポスト処理・スワップチェーンの内部描画解像度。論理座標と同じ16:9。
    static const int32_t kRenderWidth = 1920;
    static const int32_t kRenderHeight = 1080;

    // ウィンドウプロシージャ
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    /// @brief COMを初期化し、ウィンドウを作って表示する。メインスレッドで1回だけ呼ぶ。
    void Initialize();

    /// @brief ウィンドウを閉じ、Initializeで初期化したCOMを終える。
    void Finalize();

    /// @brief ウィンドウ機体を返す。
    WNDCLASS GetWindowClass()
    {
        return wc_;
    };

    /// @brief Hwndを返す。
    HWND GetHwnd()
    {
        return hwnd_;
    }

    /// @brief Client幅を返す。
    int32_t GetClientWidth() const;
    /// @brief Client高さを返す。
    int32_t GetClientHeight() const;
    /// @brief 実ウィンドウのクライアント座標を、論理座標(kClientWidth x kClientHeight)へ変換する。
    /// 最終提示で画面を樽型に歪ませている場合は、その画素に映っている論理位置を返す。
    void ClientToLogical(float& x, float& y) const;

    /// @brief 最終提示パスの樽型歪みの強さを設定する（0で歪みなし）。
    void SetPresentationCurvature(float curvature)
    {
        presentationCurvature_ = curvature;
    }

    /// @brief 有効であるか判定する。
    bool IsActive() const
    {
        return isActive_;
    }

    /// @brief ActivationChangedの未処理分を取り出し、内部の保留分を消費済みにする。
    bool ConsumeActivationChanged();

private:
    HWND hwnd_ = nullptr;
    WNDCLASS wc_{};
    bool isActive_ = true;
    bool activationChanged_ = false;
    float presentationCurvature_ = 0.0f;
    // CoInitializeExが成功した（S_FALSEを含む）ときだけ、Finalizeで対になるCoUninitializeを呼ぶ。
    bool comInitialized_ = false;
};

} // namespace myEngine
