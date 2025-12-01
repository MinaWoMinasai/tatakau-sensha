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

	std::unique_ptr<ModelCommon> modelCommon;
	modelCommon = std::make_unique<ModelCommon>();
	modelCommon->Initialize(dxCommon.get());

	std::unique_ptr<Model> model;
	model = std::make_unique<Model>();
	model->Initialize(modelCommon.get());

	std::unique_ptr<Object3dCommon> object3dCommon;
	object3dCommon = std::make_unique<Object3dCommon>();
	object3dCommon->Initialize(dxCommon.get());

	std::unique_ptr<Object3d> object3d;
	object3d = std::make_unique<Object3d>();
	object3d->Initialize(object3dCommon.get());

	std::unique_ptr<Object3d> object3d2;
	object3d2 = std::make_unique<Object3d>();
	object3d2->Initialize(object3dCommon.get());

	object3d->SetModel(model.get());
	object3d2->SetModel(model.get());

	// キーの初期化
	Input input;
	input.Initialize(winApp->GetWindowClass(), winApp->GetHwnd());

	DebugCamera debugCamera;

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

			// デバッグカメラ
			debugCamera.Update(input.GetMouseState(), input.GetKey(), leftStick);
			Matrix4x4 viewMatrix = debugCamera.GetViewMatrix();
			Matrix4x4 projectionMatrix = MakePerspectiveForMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f);

			// スプライトもスライダーで変えられるようにする
			//ImGui::DragFloat2("scaleSprite", &sprite->GetSize().x);
			//ImGui::DragFloat("rotateSprite", &sprite->GetRotation());
			//ImGui::DragFloat2("translateSprite", &sprite->GetPosition().x);
			//
			//ImGui::DragFloat3("UVTranslate", &sprite->GetUvTransform().translate.x, 0.01f);
			//ImGui::DragFloat3("UVScale", &sprite->GetUvTransform().scale.x, 0.01f);
			//ImGui::SliderAngle("UVRotate", &sprite->GetUvTransform().rotate.z);

			ImGui::DragFloat3("translate", &object3d->GetTranslate().x, 0.01f);

			object3d->Update(debugCamera.GetViewMatrix());
			object3d2->Update(debugCamera.GetViewMatrix());

			sprite->Update();
			sprite2->Update();
			sprite3->Update();

			//if (input.IsTrigger(input.GetMouseState().rgbButtons[0], input.GetPreMouseState().rgbButtons[0])) {
			//	// テクスチャを変更する
			//	sprite->SetTexture("resources/Circle.png");
			//}

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

	dxCommon->Release();
	winApp->Finalize();

	return 0;
}