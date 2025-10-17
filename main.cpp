#define DIRECTINPUT_VERSION 0x0800
#include "Engine.h"
#include "ObjectDraw.h"
#include "Easing.h"
#include "WinApp.h"
#include <numbers>

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	D3DResouceLeakCheaker leakCheck;
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;

	// COMの初期化を行う
	CoInitializeEx(0, COINIT_MULTITHREADED);

	MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);

	// 誰も捕捉しなかった場合に(Unheadled)、捕捉する関数を登録
	// mein関数始まってすぐに登録する
	Dump dump;
	SetUnhandledExceptionFilter(dump.Export);

	// ログの初期化
	LogWrite log;
	log.Initialize();
	log.Log(log.GetLogStream(), "test");

	WinApp winApp;
	winApp.Initialize();

	WNDCLASS wc{};
	wc = winApp.GetWindowClass();

	HWND hwnd;
	hwnd = winApp.GetHwnd();

	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(hr));

	// アダプターとデバイスを初期化
	UseAdapterDevice useAdapterDevice;
	useAdapterDevice.Initialize(dxgiFactory);
	useAdapterDevice.Create(device);

	// キーの初期化
	Input input;
	input.Initialize(wc, hwnd);

	// デバッグレイヤー
	DebugLayer::EnableDebugLayer(device.Get());

	// コマンドリスト
	Command command;
	command.Initialize(device);

	SwapChain swapChain;
	swapChain.Initialize(WinApp::kClientWidth, WinApp::kClientHeight);
	swapChain.Create(dxgiFactory, command.GetQueue(), hwnd);

	Descriptor descriptor;
	descriptor.Initialize(device);

	Texture texture;

	View view;
	view.CreateDSV(texture, WinApp::kClientWidth, WinApp::kClientHeight, swapChain, descriptor, device);
	view.CreateSRV(swapChain, descriptor, device);

	PSO psoObject;
	psoObject.Initialize(device, command, L"Object3d.VS.hlsl", L"Object3d.PS.hlsl", 0);
	psoObject.Graphics();
	psoObject.Create(device);

	PSO psoParticle;
	psoParticle.Initialize(device, command, L"Particle.VS.hlsl", L"Particle.PS.hlsl", 1);
	psoParticle.Graphics();
	psoParticle.Create(device);

	// リソースを作成する
	Resource resource;

	struct Vertex {
		Vector3 position;
		Vector4 color;
	};

	State state;
	state.Initialize();

	LoadFile loadFile;
	ModelData modelData = loadFile.Obj("resources", "plane.obj");
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	Microsoft::WRL::ComPtr <ID3D12Resource> vertexResource;
	vertexBufferView = resource.CreateVBV(modelData, texture, device, vertexResource);

	// 頂点データを10こ格納できるリソースを作成
	const uint32_t kNumInstance = 1000;
	// Instancing用のTransformationMatrixリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource =
		texture.CreateBufferResource(device, sizeof(ParticleForGPU) * kNumInstance);
	// 書き込むためのアドレスを取得
	ParticleForGPU* instancingData = nullptr;
	instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&instancingData));
	// 単位行列を書き込んでおく
	std::vector<Particle> particles;
	for (uint32_t index = 0; index < 10; ++index) {
		particles.push_back(MakeParticle(Vector3(0.0f, 0.0f, 0.0f)));
	}

	for (uint32_t index = 0; index < particles.size(); ++index) {
		instancingData[index].WVP = MakeIdentity4x4();
		instancingData[index].World = MakeIdentity4x4();
		instancingData[index].color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	// Sprite用の頂点リソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite = texture.CreateBufferResource(device, sizeof(VertexData) * 4);

	// 頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	// リソースの先端のアドレスから使う
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 4;
	// 1頂点あたりのサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	VertexData* vertexDataSprite = nullptr;
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));

	// 一枚目の三角形
	vertexDataSprite[0].position = { 0.0f, 360.0f, 0.0f, 1.0f };// 左下
	vertexDataSprite[0].texcoord = { 0.0f, 1.0f };
	vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };// 左上
	vertexDataSprite[1].texcoord = { 0.0f, 0.0f };
	vertexDataSprite[2].position = { 640.0f, 360.0f, 0.0f, 1.0f };// 右下
	vertexDataSprite[2].texcoord = { 1.0f, 1.0f };
	// 二枚目の三角形
	vertexDataSprite[3].position = { 640.0f, 0.0f, 0.0f, 1.0f };// 右上
	vertexDataSprite[3].texcoord = { 1.0f, 0.0f };

	// Index用の頂点リソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite = texture.CreateBufferResource(device, sizeof(uint32_t) * 6);

	// 頂点バッファビューを作成する
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	// リソースの先端のアドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点6つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	// 1頂点あたりのサイズ
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

	// インデックスリソースにデータを書きこむ
	uint32_t* indexDataSprite = nullptr;
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	indexDataSprite[0] = 0; indexDataSprite[1] = 1; indexDataSprite[2] = 2;
	indexDataSprite[3] = 1; indexDataSprite[4] = 3; indexDataSprite[5] = 2;

	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite = texture.CreateBufferResource(device, sizeof(Material));
	// マテリアルにデータを書き込む
	Material* materialDataSprite = nullptr;
	// 書き込むためのアドレスを取得
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	// 白を書き込む
	materialDataSprite->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	// SptiteはLightingを使わないのでfalse
	materialDataSprite->enableLighting = false;
	materialDataSprite->uvTransform = MakeIdentity4x4();

	Transform uvTransformSprite{
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f},
	};

	// Sprite用のTransformationMatrix用のリソースを作る。Matrix4x41つ分のサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSprite = texture.CreateBufferResource(device, sizeof(TransformationMatrix));
	// データを書き込む
	TransformationMatrix* transformationMatrixDataSprite = nullptr;
	// 書き込むためのアドレスを取得
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));
	// 単位行列を書き込んでおく
	transformationMatrixDataSprite->WVP = MakeIdentity4x4();
	transformationMatrixDataSprite->World = MakeIdentity4x4();

	Transform transformSprite{ {0.8f, 0.5f, 1.0f}, {0.0f, 0.0f, 0.0f }, {0.0f, 0.0f, 0.0f } };

	// マテリアル用のリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = resource.CreateMaterial(texture, device);
	// マテリアルにデータを書き込む
	Material* materialData = nullptr;
	// 書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 赤を書き込む
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	// ライティングを有効にする
	materialData->enableLighting = false;
	materialData->lightingMode = false;
	materialData->uvTransform = MakeIdentity4x4();

	// 平行光源用のリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource = resource.CreatedirectionalLight(texture, device);
	// マテリアルにデータを書き込む
	DirectionalLight* directionalLightData = nullptr;
	// 書き込むためのアドレスを取得
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	// デフォルト値
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->direction = Normalize(directionalLightData->direction);
	directionalLightData->intensity = 1.0f;
	WindowScreenSize windowScreenSize;
	windowScreenSize.Initialize(WinApp::kClientWidth, WinApp::kClientHeight);

	// Textureを読んで転送する
	DirectX::ScratchImage mipImages = texture.Load("resources/circle.png");
	const DirectX::TexMetadata metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource = texture.CreateResource(device, metadata);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = texture.UploadData(textureResource, mipImages, device, command.GetList());

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

	D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};
	instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	instancingSrvDesc.Buffer.FirstElement = 0;
	instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	instancingSrvDesc.Buffer.NumElements = kNumInstance;
	instancingSrvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);
	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU = descriptor.GetGPUHandle(descriptor.GetSrvHeap(), descriptorSizeSRV, 100);
	D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU = descriptor.GetCPUHandle(descriptor.GetSrvHeap(), descriptorSizeSRV, 100);
	device->CreateShaderResourceView(instancingResource.Get(), &instancingSrvDesc, instancingSrvHandleCPU);

	descriptor.GetCPUHandle(descriptor.GetRtvHeap(), descriptorSizeRTV, 0);

	// SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = descriptor.GetSrvHeap()->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = descriptor.GetSrvHeap()->GetGPUDescriptorHandleForHeapStart();

	// 先端はImGuiが使っているのでその次を使う
	textureSrvHandleCPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// SRVの作成
	device->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);
	
	DebugCamera debugCamera;

	Renderer renderer;
	renderer.SetDSVHandle(descriptor.GetDsvHeap()->GetCPUDescriptorHandleForHeapStart());
	renderer.SetSwapChainResources({ view.GetSwapChainResource()[0], view.GetSwapChainResource()[1] });
	renderer.SetGraphicsPipelineState(psoObject.GetGraphicsState().Get());
	renderer.SetSRVHeap(descriptor.GetSrvHeap().Get());
	renderer.SetRTVHandles({ view.GetRtvHandles()[0], view.GetRtvHandles()[1] });
	renderer.SetRootSignature(psoObject.GetRootSignature().Get());
	renderer.SetViewport(windowScreenSize.GetViewport());
	renderer.SetScissorRect(windowScreenSize.GetSissorRect());
	renderer.SetFenceValue(command.GetFenceValue());
	renderer.SetSwapChain(swapChain.GetList());

	// Rendererの初期化
	renderer.Initialize(
		device.Get(),
		command.GetList().Get(),
		command.GetQueue().Get(),
		command.GetFence().Get(),
		command.GetFenceEvent(),
		command.GetAllocator().Get(),
		2 // バックバッファ数
	);

	ObjectDraw objectDraw;
	objectDraw.Initialize(device, descriptor, command);

	Matrix4x4 billboardMatrix;

	AccelerationField accelerationField;
	accelerationField.acceleration = { 15.0f, 0.0f, 0.0f };
	accelerationField.area.min = { -1.0f, -1.0f, -1.0f };
	accelerationField.area.max = { 1.0f, 1.0f, 1.0f };

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

			ImGui::Begin("debug");
			ImGui::DragFloat2("a", &mousePos.x);
			ImGui::End();

			// デバッグカメラ
			debugCamera.Update(input.GetMouseState(), input.GetKey(), leftStick);

			Matrix4x4 viewMatrix = debugCamera.GetViewMatrix();
			Matrix4x4 projectionMatrix = MakePerspectiveForMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f);
			
			// ボタンを押したらパーティクル追加
			if (input.IsTrigger(input.GetMouseState().rgbButtons[0], input.GetPreMouseState().rgbButtons[0])) {
				for (uint32_t index = 0; index < 10; ++index) {
					
					Vector3 worldPos = ScreenToWorld3D(mousePos, debugCamera.GetViewMatrix(), projectionMatrix, WinApp::kClientWidth, WinApp::kClientHeight, debugCamera.GetDistance());
					particles.push_back(MakeParticle(worldPos));
				}
			}

			const float kDeltaTime = 1.0f / 60.0f;
			// パーティクルの移動
			for (auto& particle : particles) {
				particle.currentTime += kDeltaTime;
				particle.transform.translate += particle.velocity;
				particle.color.w = 1.0f - (particle.currentTime / particle.lifeTime);
			}

			// パーティクルの削除
			particles.erase(
				std::remove_if(
					particles.begin(), particles.end(),
					[](const Particle& particle) {
						return particle.currentTime >= particle.lifeTime; // ラムダ式でtrueの要素が削除される
					}),
				particles.end());

			billboardMatrix = Inverse(debugCamera.GetViewMatrix());
			billboardMatrix.m[3][0] = 0.0f;
			billboardMatrix.m[3][1] = 0.0f;
			billboardMatrix.m[3][2] = 0.0f;

			for (uint32_t index = 0; index < particles.size(); ++index) {
				// パーティクルが最大数を超えたら追加しない
				if(index >= kNumInstance){
					break;
				}

				auto& particle = particles[index];
				Matrix4x4 scaleMatrix = MakeScaleMatrix(particle.transform.scale);
				Matrix4x4 translateMatrix = MakeTranslateMatrix(particle.transform.translate);
				Matrix4x4 worldMatrixParticle = scaleMatrix * billboardMatrix * translateMatrix;
				Matrix4x4 worldViewProjectionMarrixParticle = Multiply(worldMatrixParticle, Multiply(viewMatrix, projectionMatrix));
				instancingData[index].WVP = worldViewProjectionMarrixParticle;
				instancingData[index].World = worldMatrixParticle;
				instancingData[index].color = particle.color;
			}

			// Sprite用のWorldViewProjectionMatrixを作る
			Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
			Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
			Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 100.0f);
			Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));
			transformationMatrixDataSprite->WVP = worldViewProjectionMatrixSprite;
			transformationMatrixDataSprite->World = worldMatrixSprite;
			// パラメータからUVTransform用の行列を作成する
			Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransformSprite.scale);
			uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZMatrix(uvTransformSprite.rotate.z));
			uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslateMatrix(uvTransformSprite.translate));
			materialDataSprite->uvTransform = uvTransformMatrix;

			objectDraw.Update();

			// ImGuiの内部コマンドを生成する
			ImGui::Render();

			UINT backBufferIndex = swapChain.GetList()->GetCurrentBackBufferIndex();
			renderer.BeginFrame(backBufferIndex);

			objectDraw.Draw(renderer, debugCamera, materialResource, directionalLightResource);

			// スプライトの描画
			//command.GetList()->IASetVertexBuffers(0, 1, &vertexBufferViewSprite); //VBVを設定
			//command.GetList()->IASetIndexBuffer(&indexBufferViewSprite);// IBVを設定
			//command.GetList()->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());
			//command.GetList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite->GetGPUVirtualAddress());
			//command.GetList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
			//// 描画!(DrawCall/ドローコール) 。6個のインデックスを使用しで1つのインスタンスを描画。その他は当面0 
			//command.GetList()->DrawIndexedInstanced(6, 1, 0, 0, 0);

			ID3D12DescriptorHeap* heaps[] = { descriptor.GetSrvHeap().Get() };
			command.GetList()->SetDescriptorHeaps(1, heaps);

			command.GetList()->SetGraphicsRootSignature(psoParticle.GetRootSignature().Get());
			command.GetList()->SetPipelineState(psoParticle.GetGraphicsState().Get()); // PSOを設定
			command.GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// パーティクルの描画
			command.GetList()->IASetVertexBuffers(0, 1, &vertexBufferView); //VBVを設定
			command.GetList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
			command.GetList()->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU);
			command.GetList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
			command.GetList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
			// 描画!(DrawCall/ドローコール) 。3頂点で1つのインスタンス。 インスタンスについては今度
			command.GetList()->DrawInstanced(UINT(modelData.vertices.size()), UINT(particles.size()), 0, 0);

			// 実際のcommandListのImGuiの描画コマンドを組む
			renderer.DrawImGui();

			renderer.EndFrame();

		}
	}

	// ImGuiの終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CoUninitialize();

	CloseHandle(command.GetFenceEvent());

	//psoObject.Release();
	psoParticle.Release();
	winApp.Finalize();
	return 0;
}