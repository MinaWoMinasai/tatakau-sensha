#include "WinApp.h"
#include "StartupTrace.h"

#include <algorithm>
#include <cstdio>

#pragma comment(lib, "winmm.lib")

namespace myEngine {

WinApp* WinApp::GetInstance()
{
    static WinApp instance;
    return &instance;
}

int32_t WinApp::GetClientWidth() const
{
    RECT rect{};
    return hwnd_ && GetClientRect(hwnd_, &rect) ? (std::max)(1L, rect.right - rect.left) : kClientWidth;
}

int32_t WinApp::GetClientHeight() const
{
    RECT rect{};
    return hwnd_ && GetClientRect(hwnd_, &rect) ? (std::max)(1L, rect.bottom - rect.top) : kClientHeight;
}

void WinApp::ClientToLogical(float& x, float& y) const
{
    float u = x / static_cast<float>(GetClientWidth());
    float v = y / static_cast<float>(GetClientHeight());
    if (presentationCurvature_ > 0.0f) {
        // CrtPresent.PS.hlslのDistortと同じ式。画面上の画素が参照する中間画像の位置へ写す。
        const float cx = u * 2.0f - 1.0f;
        const float cy = v * 2.0f - 1.0f;
        const float ox = cy * 0.5f;
        const float oy = cx * 0.5f;
        u = (cx + cx * ox * ox * presentationCurvature_) * 0.5f + 0.5f;
        v = (cy + cy * oy * oy * presentationCurvature_) * 0.5f + 0.5f;
    }
    x = u * static_cast<float>(kClientWidth);
    y = v * static_cast<float>(kClientHeight);
}

LRESULT WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    {
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)

        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
            return true;
        }

#endif // USE_IMGUI

        // メッセージに応じてゲーム固有の処理を行う
        switch (msg) {
        case WM_ACTIVATEAPP:
            // ゲームプレイ中にキャプチャしたマウスが、Alt+Tab後にカーソルを閉じ込めてはならない。
            if (!wparam) {
                ClipCursor(nullptr);
            }
            WinApp::GetInstance()->isActive_ = wparam != FALSE;
            WinApp::GetInstance()->activationChanged_ = true;
            return 0;
            // ウィンドウが破棄された
        case WM_DESTROY:
            // OSに対して、アプリの終了を伝える
            PostQuitMessage(0);
            return 0;
        }

        // 標準のメッセージ処置を行う
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

bool WinApp::ConsumeActivationChanged()
{
    const bool changed = activationChanged_;
    activationChanged_ = false;
    return changed;
}

void WinApp::ShowDeferredWindow()
{
    if (!showDeferred_) {
        return;
    }
    showDeferred_ = false;
    if (!hiddenWindow_) {
        ShowWindow(hwnd_, SW_SHOW);
        SetForegroundWindow(hwnd_);
    }
    myEngine::StartupTrace::Mark("window.shown");
}

void WinApp::Initialize(bool deferShow)
{
    // WIC・Media Foundation・XAudio2などが使うCOMを、メインスレッドでマルチスレッドアパートメントとして初期化する。
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    comInitialized_ = SUCCEEDED(comResult);
    if (!comInitialized_) {
        // 別の方式で初期化済み（RPC_E_CHANGED_MODE）などでも起動は続ける。COMを使う側がそれぞれ失敗を返す。
        char message[96]{};
        std::snprintf(message, sizeof(message), "[WinApp] CoInitializeEx failed: 0x%08lX\n", static_cast<unsigned long>(comResult));
        OutputDebugStringA(message);
    }

    // 表示スケール（125%など）でWindowsがウィンドウ画像を拡大してぼかさないよう、実画素で描く。
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // ウィンドウプロシージャ
    wc_.lpfnWndProc = WindowProc;
    // ウィンドウクラス名
    wc_.lpszClassName = L"MyEngineWindowClass";
    // インタンスバンドル
    wc_.hInstance = GetModuleHandle(nullptr);
    // カーソル
    wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
    // 最初のPresentまでWindowsが塗る背景。既定（ブラシなし）だと白く見えるため黒にする。
    wc_.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

    // ウィンドウクラスを登録する
    RegisterClass(&wc_);

    // 作業領域に収まる最大の16:9クライアント（上限は内部描画解像度）で開く。
    RECT workArea{};
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    int32_t clientWidth = kClientWidth;
    for (const int32_t width : {kRenderWidth, 1600, kClientWidth}) {
        RECT candidate = {0, 0, width, width * 9 / 16};
        AdjustWindowRect(&candidate, WS_OVERLAPPEDWINDOW, false);
        if (candidate.right - candidate.left <= workArea.right - workArea.left &&
            candidate.bottom - candidate.top <= workArea.bottom - workArea.top) {
            clientWidth = width;
            break;
        }
    }
    RECT wrc = {0, 0, clientWidth, clientWidth * 9 / 16};

    // クライアント領域を元に実際のサイズにwrcを変更してもらう
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

    // ウィンドウの生成
    hwnd_ = CreateWindow(wc_.lpszClassName, L"たたかうせんしゃ", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, wrc.right - wrc.left,
                         wrc.bottom - wrc.top, nullptr, nullptr, wc_.hInstance, nullptr);

    // ウィンドウを表示する。自動検証では MYENGINE_HIDDEN_WINDOW=1 で表示せず、作業中の画面やフォーカスを奪わない。
    // 描画とキャプチャ（GPUからの読み戻し）は非表示のままでも同じ結果になる。
    // 非表示の検証でも、遅らせた表示の時刻は window.shown として記録する。
    char hidden[8]{};
    hiddenWindow_ = GetEnvironmentVariableA("MYENGINE_HIDDEN_WINDOW", hidden, sizeof(hidden)) == 1 && hidden[0] == '1';
    if (deferShow) {
        showDeferred_ = true;
    } else {
        if (!hiddenWindow_) {
            ShowWindow(hwnd_, SW_SHOW);
        }
        myEngine::StartupTrace::Mark("window.shown");
    }

    // システムタイマーの分解度をあげる
    timeBeginPeriod(1);
}

void WinApp::Finalize()
{
    CloseWindow(hwnd_);
    if (comInitialized_) {
        CoUninitialize();
        comInitialized_ = false;
    }
}

} // namespace myEngine
