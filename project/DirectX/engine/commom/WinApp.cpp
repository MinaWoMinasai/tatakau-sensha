#include "WinApp.h"

#include <algorithm>

#pragma comment(lib, "winmm.lib")

namespace cg2 {

WinApp* WinApp::GetInstance()
{
	static WinApp instance;
	return &instance;
}

int32_t WinApp::GetClientWidth() const
{
	RECT rect{};
	return hwnd_ && GetClientRect(hwnd_, &rect)
		? (std::max)(1L, rect.right - rect.left)
		: kClientWidth;
}

int32_t WinApp::GetClientHeight() const
{
	RECT rect{};
	return hwnd_ && GetClientRect(hwnd_, &rect)
		? (std::max)(1L, rect.bottom - rect.top)
		: kClientHeight;
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
			// A captured gameplay mouse must never trap the cursor after Alt+Tab.
			if (!wparam) ClipCursor(nullptr);
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

void WinApp::Initialize()
{
	// ウィンドウプロシージャ
	wc_.lpfnWndProc = WindowProc;
	// ウィンドウクラス名
	wc_.lpszClassName = L"CG2WindouClass";
	// インタンスバンドル
	wc_.hInstance = GetModuleHandle(nullptr);
	// カーソル
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);

	// ウィンドウクラスを登録する
	RegisterClass(&wc_);

	// ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0,0,kClientWidth, kClientHeight };

	// クライアント領域を元に実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// ウィンドウの生成
	hwnd_ = CreateWindow(
		wc_.lpszClassName,
		L"たたかうせんしゃ",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		wc_.hInstance,
		nullptr);

	// ウィンドウを表示する
	ShowWindow(hwnd_, SW_SHOW);

	// システムタイマーの分解度をあげる
	timeBeginPeriod(1);

}

void WinApp::Finalize() 
{
	CloseWindow(hwnd_);
	CoUninitialize();
}

} // namespace cg2
