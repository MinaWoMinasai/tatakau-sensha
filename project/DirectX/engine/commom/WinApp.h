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

namespace cg2 {

/// @brief Windowsのウィンドウ生成とメッセージ処理を管理する。
class WinApp {

public:
    // シングルトン
    static WinApp* GetInstance();

    static const int32_t kClientWidth = 1280;
    static const int32_t kClientHeight = 720;

    // ウィンドウプロシージャ
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    /// @brief 初期化
    void Initialize();

    /// @brief 利用終了時の資源と状態を解放する。
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
};

} // namespace cg2
