#include "Engine.h"

void Engine::Initialize()
{
}

void Engine::PreDraw()
{

}

void Engine::EndDraw()
{
	ImGui::ShowDemoWindow();

	// ImGuiの内部コマンドを生成する
	ImGui::Render();

	// 実際のcommandListのImGuiの描画コマンドを組む
	renderer.DrawImGui();
	renderer.EndFrame();

}

void Engine::Finalize()
{

	// ImGuiの終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CoUninitialize();

	CloseHandle(command.GetFenceEvent());

	pso.Release();

	CloseWindow(hwnd);

	CoUninitialize();
}
