#include "Engine.h"

void Engine::Initialize()
{
	// COMの初期化を行う
	CoInitializeEx(0, COINIT_MULTITHREADED);

	MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);

	// 誰も捕捉しなかった場合に(Unheadled)、捕捉する関数を登録
	// mein関数始まってすぐに登録する
	SetUnhandledExceptionFilter(dump.Export);

	// ログの初期化
	log.Initialize();
	log.Log(log.GetLogStream(), "test");

	window.Initialize();

	wc = window.GetWindowClass();

	hwnd = window.GetHwnd();

	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(hr));

	// アダプターとデバイスを初期化
	useAdapterDevice.Initialize(dxgiFactory);
	useAdapterDevice.Create(device);

	// キーの初期化
	input.Initialize(wc, hwnd);

	// デバッグレイヤー
	DebugLayer::EnableDebugLayer(device.Get());

	// コマンドリスト
	command.Initialize(device);

	swapChain.Initialize(kClientWidth, kClientHeight);
	swapChain.Create(dxgiFactory, command.GetQueue(), hwnd);

	descriptor.Initialize(device);

	view.CreateDSV(texture, kClientWidth, kClientHeight, swapChain, descriptor, device);
	view.CreateSRV(swapChain, descriptor, device);

	pso.Initialize(device, command);
	pso.Graphics();
	pso.Create(device);

	windowScreenSize.Initialize(kClientWidth, kClientHeight);

	// コマンドリストの内容を確定させるすべてのコマンドを積んでからCloseすること
	hr = command.GetList()->Close();
	assert(SUCCEEDED(hr));

	// GPUにコマンドリストの実行を行わせる
	ID3D12CommandList* commandLists[] = { command.GetList().Get() };
	command.GetQueue()->ExecuteCommandLists(1, commandLists);

	//GPUとOSに画面の交換を行うよう通知する
	swapChain.GetList()->Present(1, 0);
	// command.GetFence()の値を更新
	command.SetFenceValue(command.GetFenceValue() + 1);
	// GPUがここまでたどり着いたときに、Fenceの値を指定して値に代入するようにSignalを送る
	command.GetQueue()->Signal(command.GetFence().Get(), command.GetFenceValue());
	// Fenceの値が指定したSignal値にたどり着いているか確認する
	// GetCompletedValueの初期値はFence作成時に渡した初期値
	if (command.GetFence()->GetCompletedValue() < command.GetFenceValue()) {
		// 指定したSignalにたどり着いていないので、たどり着くまで待つようにイベントを設定する
		command.GetFence()->SetEventOnCompletion(command.GetFenceValue(), command.GetFenceEvent());
		// イベント待つ
		WaitForSingleObject(command.GetFenceEvent(), INFINITE);
	}
	// 次のフレーム用のコマンドリストを準備
	hr = command.GetAllocator()->Reset();
	assert(SUCCEEDED(hr));
	hr = command.GetList()->Reset(command.GetAllocator().Get(), nullptr);
	assert(SUCCEEDED(hr));

	// Imguiの初期化
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(device.Get(),
		swapChain.GetDesc().BufferCount,
		view.GetRtvDesc().Format,
		descriptor.GetSrvHeap().Get(),
		descriptor.GetSrvHeap()->GetCPUDescriptorHandleForHeapStart(),
		descriptor.GetSrvHeap()->GetGPUDescriptorHandleForHeapStart());

	// metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	// DescriptorSizeを取得しておく
	const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	descriptor.GetCPUHandle(descriptor.GetRtvHeap(), descriptorSizeRTV, 0);

	// SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = descriptor.GetSrvHeap()->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = descriptor.GetSrvHeap()->GetGPUDescriptorHandleForHeapStart();

	// 先端はImGuiが使っているのでその次を使う
	textureSrvHandleCPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// SRVの作成
	device->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);

	Renderer renderer;
	renderer.SetDSVHandle(descriptor.GetDsvHeap()->GetCPUDescriptorHandleForHeapStart());
	renderer.SetSwapChainResources({ view.GetSwapChainResource()[0], view.GetSwapChainResource()[1] });
	renderer.SetGraphicsPipelineState(pso.GetGraphicsState().Get());
	renderer.SetSRVHeap(descriptor.GetSrvHeap().Get());
	renderer.SetRTVHandles({ view.GetRtvHandles()[0], view.GetRtvHandles()[1] });
	renderer.SetRootSignature(pso.GetRootSignature().Get());
	renderer.SetViewport(windowScreenSize.GetViewport());
	renderer.SetScissorRect(windowScreenSize.GetSissorRect());
	renderer.SetFenceValue(command.GetFenceValue());
	renderer.SetSwapChain(swapChain.GetList());

	// 例：Rendererの初期化（最初に一度だけ）
	renderer.Initialize(
		device.Get(),
		command.GetList().Get(),
		command.GetQueue().Get(),
		command.GetFence().Get(),
		command.GetFenceEvent(),
		command.GetAllocator().Get(),
		2 // バックバッファ数
	);

}

void Engine::PreDraw()
{

	// 前のフレームのキー状態を保存
	input.BeforeFrameData();

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// デバッグカメラ
	//debugCamera.Update(input.GetMouseState(), input.GetKey());

	UINT backBufferIndex = swapChain.GetList()->GetCurrentBackBufferIndex();
	renderer.BeginFrame(backBufferIndex);

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
