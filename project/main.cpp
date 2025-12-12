#define DIRECTINPUT_VERSION 0x0800
#include "Audio.h"
#include "debugCamera.h"
#include "Dump.h"
#include "Easing.h"
#include "Resource.h"
#include "Sprite.h"
#include "WinApp.h"
#include "Object3d.h"
#include "Model.h"
#include "ModelManager.h"

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	D3DResouceLeakCheaker leakCheck;

	// COMの初期化を行う
	CoInitializeEx(0, COINIT_MULTITHREADED);

	MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);

	// 誰も捕捉しなかった場合に(Unheadled)、捕捉する関数を登録
	Dump dump;
	SetUnhandledExceptionFilter(dump.Export);

	std::unique_ptr<WinApp> winApp;
	winApp = std::make_unique<WinApp>();
	winApp->Initialize();

	std::unique_ptr<DirectXCommon> dxCommon;
	dxCommon = std::make_unique<DirectXCommon>();
	dxCommon->Initialize(winApp.get());

	TextureManager::GetInstance()->Initialize(dxCommon.get());

	std::unique_ptr<SpriteCommon> spriteCommon;
	spriteCommon = std::make_unique<SpriteCommon>();
	spriteCommon->Initialize(dxCommon.get());

	std::unique_ptr<Sprite> sprite;
	sprite = std::make_unique<Sprite>();
	sprite->Initialize(spriteCommon.get(), "resources/UvChecker.png");
	
	std::unique_ptr<Sprite> sprite2;
	sprite2 = std::make_unique<Sprite>();
	sprite2->Initialize(spriteCommon.get(), "resources/Circle.png");
	
	std::unique_ptr<Sprite> sprite3;
	sprite3 = std::make_unique<Sprite>();
	sprite3->Initialize(spriteCommon.get(), "resources/CheckerBoard.png");

	ModelManager::GetInstance()->Initialize(dxCommon.get());
	
	//　objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("teapot.obj");
	ModelManager::GetInstance()->LoadModel("bunny.obj");

	std::unique_ptr<DebugCamera> debugCamera;
	debugCamera = std::make_unique<DebugCamera>();

	std::unique_ptr<Camera> camera;
	camera = std::make_unique<Camera>();

	std::unique_ptr<Object3dCommon> object3dCommon;
	object3dCommon = std::make_unique<Object3dCommon>();
	object3dCommon->Initialize(dxCommon.get());
	object3dCommon->SetDefaultCamera(camera.get());
	object3dCommon->SetDebugDefaultCamera(debugCamera.get());

	std::unique_ptr<Object3d> object3d;
	object3d = std::make_unique<Object3d>();
	object3d->Initialize(object3dCommon.get());

	std::unique_ptr<Object3d> object3d2;
	object3d2 = std::make_unique<Object3d>();
	object3d2->Initialize(object3dCommon.get());

	object3d->SetModel("teapot.obj");
	object3d2->SetModel("bunny.obj");

	// キーの初期化
	Input input;
	input.Initialize(winApp->GetWindowClass(), winApp->GetHwnd());

	MSG msg{};
	// ウィンドウの×ボタンが押されるまでループ
	while (msg.message != WM_QUIT) {

		// Windowにメッセージが来てたら最優先で処理させる
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			// ゲームの処理

			// 前のフレームのキー状態を保存
			input.BeforeFrameData();
			Vector2 leftStick = input.GetLeftStick();
			Vector2 rightStick = input.GetRightStick();
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			Vector2 mousePos = input.GetMousePosition();

			ImGui::DragFloat3("translate", &camera->GetTranslate().x, 0.1f);
			ImGui::DragFloat3("rotate", &camera->GetRotate().x, 0.1f);

			if (input.IsPress(input.GetKey()[DIK_LSHIFT]) && input.IsTrigger(input.GetKey()[DIK_D], input.GetPreKey()[DIK_D])) {
				if (object3dCommon->GetIsDebugCamera()) {
					object3dCommon->SetIsDebugCamera(false);
				} else {
					object3dCommon->SetIsDebugCamera(true);
				}
			}
			

			// デバッグカメラ
			debugCamera->Update(input.GetMouseState(), input.GetKey(), leftStick);
			camera->Update();

			object3d->Update();
			object3d2->Update();

			sprite->Update();
			sprite2->Update();
			sprite3->Update();

			// ImGuiの内部コマンドを生成する
			ImGui::Render();

			dxCommon->PreDraw();

			object3dCommon->PreDraw();

			object3d->Draw();
			object3d2->Draw();

			spriteCommon->PreDraw();

			sprite->Draw();
			sprite2->Draw();
			sprite3->Draw();

			// 実際のcommandListのImGuiの描画コマンドを組む
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon->GetList().Get());

			dxCommon->PostDraw();

		}
	}

	// ImGuiの終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CoUninitialize();

	CloseHandle(dxCommon->GetFenceEvent());
	TextureManager::GetInstance()->Finalize();
	ModelManager::GetInstance()->Finalize();

	dxCommon->Release();
	winApp->Finalize();

	return 0;
}