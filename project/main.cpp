#define DIRECTINPUT_VERSION 0x0800
#include "SceneManager.h"
#include "RenderTexture.h"
#include "BloomConstantBuffer.h"
#include "PostEffect.h"

void TransitionResource(DirectXCommon* dxCommon, ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) {
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = resource;
	barrier.Transition.StateBefore = stateBefore;
	barrier.Transition.StateAfter = stateAfter;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	dxCommon->GetList()->ResourceBarrier(1, &barrier);
}

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	D3DResouceLeakCheaker leakCheck;

	// COMの初期化を行う
	CoInitializeEx(0, COINIT_MULTITHREADED);

	MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);

	// 誰も捕捉しなかった場合に(Unheadled)、捕捉する関数を登録
	Dump dump;
	SetUnhandledExceptionFilter(dump.Export);

	WinApp::GetInstance()->Initialize();

	std::unique_ptr<DirectXCommon> dxCommon;
	dxCommon = std::make_unique<DirectXCommon>();
	dxCommon->Initialize(WinApp::GetInstance());

	std::unique_ptr<SrvManager> srvManager;
	srvManager = std::make_unique<SrvManager>();
	srvManager->Initialize(dxCommon.get());

	std::unique_ptr<RtvManager> rtvManager;
	rtvManager = std::make_unique<RtvManager>();
	rtvManager->Initialize(dxCommon.get());

	std::unique_ptr<RenderTexture> sceneRenderTexture;
	sceneRenderTexture = std::make_unique<RenderTexture>();

	sceneRenderTexture->Initialize(
		dxCommon.get(),
		srvManager.get(),
		rtvManager.get(),
		WinApp::kClientWidth,
		WinApp::kClientHeight
	);

	// bloom用CBVの生成
	std::unique_ptr<BloomConstantBuffer> bloomCB = std::make_unique<BloomConstantBuffer>();
	bloomCB->Initialize(dxCommon.get());

	// ポストエフェクトの初期化
	std::unique_ptr<PostEffect> postEffect = std::make_unique<PostEffect>();
	postEffect->Initialize(dxCommon.get(), bloomCB.get());

	std::unique_ptr<RenderTexture> bloomRT_A;
	std::unique_ptr<RenderTexture> bloomRT_B;
	bloomRT_A = std::make_unique<RenderTexture>();
	bloomRT_B = std::make_unique<RenderTexture>();
	uint32_t bloomWidth = WinApp::kClientWidth / 4;
	uint32_t bloomHeight = WinApp::kClientHeight / 4;

	bloomRT_A->Initialize(
		dxCommon.get(),
		srvManager.get(),
		rtvManager.get(),
		bloomWidth,
		bloomHeight
	);

	bloomRT_B->Initialize(
		dxCommon.get(),
		srvManager.get(),
		rtvManager.get(),
		bloomWidth,
		bloomHeight
	);

	// bloomRT_Half を追加
	std::unique_ptr<RenderTexture> bloomRT_Half = std::make_unique<RenderTexture>();
	// サイズは画面の半分
	bloomRT_Half->Initialize(dxCommon.get(), srvManager.get(), rtvManager.get(), WinApp::kClientWidth / 2, WinApp::kClientHeight / 2);

	// ブルームパラメータ
	BloomParam bloomParam{};
	bloomParam.threshold = 0.1f;
	bloomParam.intensity = 0.9f;
	bloomCB->Update(bloomParam);

	// Imguiの初期化
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(WinApp::GetInstance()->GetHwnd());
	ImGui_ImplDX12_Init(dxCommon->GetDevice().Get(),
		dxCommon->GetSwapChainDesc().BufferCount,
		dxCommon->GetRtvDesc().Format,
		srvManager->GetSrvHeap().Get(),
		srvManager->GetSrvHeap()->GetCPUDescriptorHandleForHeapStart(),
		srvManager->GetSrvHeap()->GetGPUDescriptorHandleForHeapStart());

	TextureManager::GetInstance()->Initialize(dxCommon.get(), srvManager.get());

	ModelManager::GetInstance()->Initialize(dxCommon.get());

	//　objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("bloomBlock.obj");
	ModelManager::GetInstance()->LoadModel("ball.obj");
	ModelManager::GetInstance()->LoadModel("cube.obj");
	ModelManager::GetInstance()->LoadModel("cubeDamage.obj");
	ModelManager::GetInstance()->LoadModel("player.obj");
	ModelManager::GetInstance()->LoadModel("playerBullet.obj");
	ModelManager::GetInstance()->LoadModel("enemy.obj");
	ModelManager::GetInstance()->LoadModel("enemyBullet.obj");
	ModelManager::GetInstance()->LoadModel("playerParticle.obj");
	ModelManager::GetInstance()->LoadModel("enemyParticle.obj");
	ModelManager::GetInstance()->LoadModel("playerHPBar.obj");
	ModelManager::GetInstance()->LoadModel("playerHPBarGreen.obj");
	ModelManager::GetInstance()->LoadModel("playerHPBarLong.obj");
	ModelManager::GetInstance()->LoadModel("playerHPBarGreenLong.obj");

	Object3dCommon::GetInstance()->Initialize(dxCommon.get());

	SpriteCommon::GetInstance()->Initialize(dxCommon.get());

	// キーの初期化
	Input::GetInstance()->Initialize(WinApp::GetInstance()->GetWindowClass(), WinApp::GetInstance()->GetHwnd());

	std::unique_ptr<SceneManager> sceneManager;
	sceneManager = std::make_unique<SceneManager>();
	sceneManager->Initialize();

	MSG msg{};
	// ウィンドウの×ボタンが押されるまでループ
	while (msg.message != WM_QUIT) {

		// Windowにメッセージが来てたら最優先で処理させる
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			// インプットインスタンスを取得
			Input* input = Input::GetInstance();

			// 前のフレームのキー状態を保存
			input->BeforeFrameData();
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			if (input->IsPress(input->GetKey()[DIK_LSHIFT]) && input->IsTrigger(input->GetKey()[DIK_D], input->GetPreKey()[DIK_D])) {
				if (Object3dCommon::GetInstance()->GetIsDebugCamera()) {
					Object3dCommon::GetInstance()->SetIsDebugCamera(false);
				} else {
					Object3dCommon::GetInstance()->SetIsDebugCamera(true);
				}
			}

			//ImGui::Begin("BloomBlock");
			//ImGui::DragFloat("Threshold", &bloomParam.threshold, 0.01f, 0.0f, 1.0f);
			//ImGui::DragFloat("Insensity", &bloomParam.intensity, 0.01f);
			//ImGui::End();

			bloomCB->Update(bloomParam);

			sceneManager->Update();

			// ImGuiの内部コマンドを生成する
			ImGui::Render();
			dxCommon->PreDraw(); // バックバッファのバリアはここで行われている
			srvManager->PreDraw();

			// ==========================================
			// 1. シーン描画 (SceneRT)
			// ==========================================
			// 書き込むので SRV -> RenderTarget に変更
			TransitionResource(dxCommon.get(), sceneRenderTexture->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

			// Scene → SceneRT
			dxCommon->SetRenderTarget(sceneRenderTexture->GetRTVHandle());
			dxCommon->ClearRenderTarget(sceneRenderTexture->GetRTVHandle());
			dxCommon->ClearDepthBuffer();

			sceneManager->DrawPostEffect3D(); // 3Dオブジェクト描画

			SpriteCommon::GetInstance()->PreDraw(kNone);
			sceneManager->DrawSprite();

			// 描き終わったので RenderTarget -> SRV (次の工程で読むため) に戻す
			TransitionResource(dxCommon.get(), sceneRenderTexture->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


			// ==========================================
			// 2. 高輝度抽出 (SceneRT -> BloomHalf)
			// ==========================================
			// 書き込む BloomHalf を RT化
			TransitionResource(dxCommon.get(), bloomRT_Half->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

			dxCommon->SetRenderTarget(bloomRT_Half->GetRTVHandle());
			dxCommon->SetViewport(WinApp::kClientWidth / 2, WinApp::kClientHeight / 2);
			dxCommon->ClearRenderTarget(bloomRT_Half->GetRTVHandle());

			// 入力は sceneRenderTexture (SRV状態になっているのでOK)
			postEffect->Draw(
				sceneRenderTexture->GetSrvManager()->GetGPUDescriptorHandle(sceneRenderTexture->GetSrvIndex()),
				kAdd_Bloom_Extract
			);

			// 書き込み完了、BloomHalf を SRV化
			TransitionResource(dxCommon.get(), bloomRT_Half->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


			// ==========================================
			// 3. ダウンサンプリング (BloomHalf -> BloomA)
			// ==========================================
			// 書き込む BloomA を RT化
			TransitionResource(dxCommon.get(), bloomRT_A->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

			dxCommon->SetRenderTarget(bloomRT_A->GetRTVHandle());
			dxCommon->SetViewport(bloomWidth, bloomHeight);
			dxCommon->ClearRenderTarget(bloomRT_A->GetRTVHandle());

			// 入力は BloomHalf (SRV状態なのでOK)
			postEffect->Draw(
				bloomRT_Half->GetSrvManager()->GetGPUDescriptorHandle(bloomRT_Half->GetSrvIndex()),
				kAdd_Bloom_Downsample
			);

			// 書き込み完了、BloomA を SRV化
			TransitionResource(dxCommon.get(), bloomRT_A->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


			// ==========================================
			// 4. ブラー水平 (BloomA -> BloomB)
			// ==========================================
			// 書き込む BloomB を RT化
			TransitionResource(dxCommon.get(), bloomRT_B->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

			dxCommon->SetRenderTarget(bloomRT_B->GetRTVHandle());
			dxCommon->ClearRenderTarget(bloomRT_B->GetRTVHandle());

			// 入力は BloomA (SRV状態なのでOK)
			postEffect->Draw(
				bloomRT_A->GetSrvManager()->GetGPUDescriptorHandle(bloomRT_A->GetSrvIndex()),
				kAdd_Bloom_BlurH
			);

			// 書き込み完了、BloomB を SRV化
			TransitionResource(dxCommon.get(), bloomRT_B->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


			// ==========================================
			// 5. ブラー垂直 (BloomB -> BloomA)
			// ==========================================
			// ★注意: BloomA はさっきSRVにしたばかりだが、また書き込むので RT化
			TransitionResource(dxCommon.get(), bloomRT_A->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

			dxCommon->SetRenderTarget(bloomRT_A->GetRTVHandle());
			dxCommon->ClearRenderTarget(bloomRT_A->GetRTVHandle());

			// 入力は BloomB (SRV状態なのでOK)
			postEffect->Draw(
				bloomRT_B->GetSrvManager()->GetGPUDescriptorHandle(bloomRT_B->GetSrvIndex()),
				kAdd_Bloom_BlurV
			);

			// 書き込み完了、BloomA を SRV化 (これでCompositeで使える)
			TransitionResource(dxCommon.get(), bloomRT_A->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


			// ==========================================
			// 6. 合成 (SceneRT + BloomA -> BackBuffer)
			// ==========================================
			dxCommon->SetBackBuffer(); // BackBufferへのバリアは内部で行われているはず（PreDraw参照）
			dxCommon->SetViewport(WinApp::kClientWidth, WinApp::kClientHeight);

			// SceneRT も BloomA も ここまでの処理で SRV に戻っているので安全に読める
			postEffect->DrawComposite(
				srvManager->GetGPUDescriptorHandle(sceneRenderTexture->GetSrvIndex()),
				srvManager->GetGPUDescriptorHandle(bloomRT_A->GetSrvIndex())
			);
			sceneManager->Draw();

			// 実際のcommandListのImGuiの描画コマンドを組む
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon->GetList().Get());

			dxCommon->PostDraw();

			sceneManager->ChangeScene();
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
	WinApp::GetInstance()->Finalize();

	return 0;
}