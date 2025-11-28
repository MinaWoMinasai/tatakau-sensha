#define DIRECTINPUT_VERSION 0x0800
#include "Audio.h"
#include "debugCamera.h"
#include "Dump.h"
#include "Easing.h"
#include "LoadFile.h"
#include "Resource.h"
#include "Sprite.h"

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

	// リソースを作成する
	Resource resource;
	Texture texture;

	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource = texture.CreateBufferResource(dxCommon->GetDevice(), sizeof(Camera));
	Camera* cameraData = nullptr;
	cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));

	// キーの初期化
	Input input;
	input.Initialize(winApp->GetWindowClass(), winApp->GetHwnd());

	DebugCamera debugCamera;

	struct Vertex {
		Vector3 position;
		Vector4 color;
	};

	LoadFile loadFile;
	ModelData modelData = loadFile.Obj("resources", "plane.obj");
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	Microsoft::WRL::ComPtr <ID3D12Resource> vertexResource;
	vertexBufferView = resource.CreateVBV(modelData, texture, dxCommon->GetDevice(), vertexResource);

	// 頂点データを10000000こ格納できるリソースを作成
	const uint32_t kNumInstance = 10000000;
	// Instancing用のTransformationMatrixリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource =
		texture.CreateBufferResource(dxCommon->GetDevice(), sizeof(ParticleForGPU) * kNumInstance);
	// 書き込むためのアドレスを取得
	ParticleForGPU* instancingData = nullptr;
	instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&instancingData));
	// 単位行列を書き込んでおく
	std::vector<Particle> particles;

	for (uint32_t index = 0; index < particles.size(); ++index) {
		instancingData[index].WVP = MakeIdentity4x4();
		instancingData[index].World = MakeIdentity4x4();
		instancingData[index].color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	// マテリアル用のリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = resource.CreateMaterial(texture, dxCommon->GetDevice());
	// マテリアルにデータを書き込む
	Material* materialData = nullptr;
	// 書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 赤を書き込む
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	// ライティングを有効にする
	materialData->enableLighting = true;
	materialData->lightingMode = false;
	materialData->uvTransform = MakeIdentity4x4();
	materialData->shininess = 10.0f;

	// 平行光源用のリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource = resource.CreatedirectionalLight(texture, dxCommon->GetDevice());
	// マテリアルにデータを書き込む
	DirectionalLight* directionalLightData = nullptr;
	// 書き込むためのアドレスを取得
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	// デフォルト値
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->direction = Normalize(directionalLightData->direction);
	directionalLightData->intensity = 1.0f;

	// Textureを読んで転送する
	DirectX::ScratchImage mipImages = texture.Load("resources/circle.png");
	const DirectX::TexMetadata metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource = texture.CreateResource(dxCommon->GetDevice(), metadata);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = texture.UploadData(textureResource, mipImages, dxCommon->GetDevice(), dxCommon->GetList());

	dxCommon->CommandListExecuteAndReset();

	// metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};
	instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	instancingSrvDesc.Buffer.FirstElement = 0;
	instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	instancingSrvDesc.Buffer.NumElements = kNumInstance;
	instancingSrvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);
	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU = dxCommon->GetSRVGPUDescriptorHandle(100);
	D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU = dxCommon->GetSRVCPUDescriptorHandle(100);
	dxCommon->GetDevice()->CreateShaderResourceView(instancingResource.Get(), &instancingSrvDesc, instancingSrvHandleCPU);

	// SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = dxCommon->GetSrvHeap()->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = dxCommon->GetSrvHeap()->GetGPUDescriptorHandleForHeapStart();

	// 先端はImGuiが使っているのでその次を使う
	textureSrvHandleCPU.ptr += dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU.ptr += dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// SRVの作成
	dxCommon->GetDevice()->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);

	Matrix4x4 billboardMatrix;

	AccelerationField accelerationField;
	accelerationField.acceleration = { 0.5f, 0.0f, 0.0f };
	accelerationField.area.min = { -1.0f, -1.0f, -1.0f };
	accelerationField.area.max = { 1.0f, 1.0f, 1.0f };
	std::vector<FireworkShell> shells;
	std::vector<TornadoParticle> tornados;

	Vector3 center = { 0.0f, 0.0f, 0.0f };
	for (uint32_t index = 0; index < 100; ++index) {

		tornados.push_back(MakeTornadoParticle(center));
	}

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
			cameraData->worldPosition = debugCamera.GetEyePosition();
			Matrix4x4 viewMatrix = debugCamera.GetViewMatrix();
			Matrix4x4 projectionMatrix = MakePerspectiveForMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f);

			// 花火を打ち上げる
			if (input.IsTrigger(input.GetMouseState().rgbButtons[0], input.GetPreMouseState().rgbButtons[0])) {

				Vector4 baseColor = Rand();
				Vector3 worldPos = ScreenToWorld3D(mousePos, debugCamera.GetViewMatrix(), projectionMatrix, WinApp::kClientWidth, WinApp::kClientHeight, debugCamera.GetDistance());
				FireworkShell shell;
				shell.pos = worldPos;
				shell.velocity = { 0, Rand(0.8f, 1.2f), 0 }; // 上昇速度
				shell.explodeTime = Rand(1.2f, 2.0f);
				shell.state = FireworkState::Rise;
				shell.color = baseColor;
				shells.push_back(shell);
			}

			const float kDeltaTime = 1.0f / 60.0f;

			for (auto& p : tornados) {

				// 回転 & 上昇
				p.angle += p.rotateSpeed * kDeltaTime;
				p.height += p.upSpeed * kDeltaTime;

				// 高さが一定を超えたらループ
				if (p.height > p.maxHeight) {
					p.height -= p.maxHeight;
				}

				// 半径（上昇に応じて広がる）
				float radius = p.baseRadius + p.height * 0.5f;

				// 新しい位置
				p.pos.x = center.x + cos(p.angle) * radius;
				p.pos.z = center.z + sin(p.angle) * radius;
				p.pos.y = center.y + p.height;
			}

			// シェルの更新
			for (auto& shell : shells) {
				shell.timer += kDeltaTime;
				shell.velocity.y -= 0.005f;  // 重力
				shell.pos += shell.velocity;

				if (shell.timer >= shell.explodeTime) {
					shell.state = FireworkState::Explode;
				}

				if (shell.state == FireworkState::Explode && !shell.isRemove) {
					for (int i = 0; i < 500; ++i) {
						particles.push_back(MakeParticle(shell.pos, shell.color));
					}
					// このシェルはもう不要
					shell.isRemove = true;
				}
			}

			// パーティクルの移動
			for (auto& particle : particles) {
				if (particle.currentTime < 0.1f) {
					particle.color.w = 2.0f; // 加算ブレンド前提
				}
				particle.currentTime += kDeltaTime;
				particle.velocity += particle.acceleration;
				particle.transform.translate += particle.velocity;
				float fadeStart = 0.6f; // 60%からフェード開始
				float rate = particle.currentTime / particle.lifeTime;
				particle.color.w = (rate < fadeStart)
					? 1.0f
					: 1.0f - (rate - fadeStart) / (1.0f - fadeStart);
			}

			// パーティクルの削除
			particles.erase(
				std::remove_if(
					particles.begin(), particles.end(),
					[](const Particle& particle) {
						return particle.currentTime >= particle.lifeTime; // ラムダ式でtrueの要素が削除される
					}),
				particles.end());

			// シェルの削除 
			shells.erase(
				std::remove_if(
					shells.begin(), shells.end(),
					[](const FireworkShell& shell) {
						return shell.isRemove; // ラムダ式でtrueの要素が削除される 
					}), shells.end());

			billboardMatrix = Inverse(debugCamera.GetViewMatrix());
			billboardMatrix.m[3][0] = 0.0f;
			billboardMatrix.m[3][1] = 0.0f;
			billboardMatrix.m[3][2] = 0.0f;

			uint32_t index = 0;

			// --- トルネード ---
			for (auto& tornado : tornados) {
				if (index >= kNumInstance) break;

				Matrix4x4 scale = MakeScaleMatrix({ 1.0f, 1.0f, 1.0f });
				Matrix4x4 trans = MakeTranslateMatrix(tornado.pos);
				Matrix4x4 world = scale * billboardMatrix * trans;

				instancingData[index].WVP = Multiply(world, Multiply(viewMatrix, projectionMatrix));
				instancingData[index].World = world;
				instancingData[index].color = tornado.color;

				index++;
			}

			// --- 花火玉 ---
			for (auto& shell : shells) {
				if (index >= kNumInstance) break;

				Matrix4x4 scale = MakeScaleMatrix({ 5.0f, 5.0f, 5.0f });
				Matrix4x4 trans = MakeTranslateMatrix(shell.pos);
				Matrix4x4 world = scale * billboardMatrix * trans;

				instancingData[index].WVP = Multiply(world, Multiply(viewMatrix, projectionMatrix));
				instancingData[index].World = world;
				instancingData[index].color = shell.color;

				index++;
			}

			// --- 花火パーティクル ---
			for (auto& particle : particles) {
				if (index >= kNumInstance) break;

				Matrix4x4 scale = MakeScaleMatrix(particle.transform.scale);
				Matrix4x4 trans = MakeTranslateMatrix(particle.transform.translate);
				Matrix4x4 world = scale * billboardMatrix * trans;

				instancingData[index].WVP = Multiply(world, Multiply(viewMatrix, projectionMatrix));
				instancingData[index].World = world;
				instancingData[index].color = particle.color;

				index++;
			}

			uint32_t instanceCount = index;

			ImGui::Text("--How to operate DebugCamera--\nMiddle mouse button hold + drag : Look around\nShift key hold + middle mouse button hold + drag : translation\nMiddle mouse button + wheel : perspective");
			ImGui::Text("--How to operate fireworks --\nLeft mouse button trigger : start fireworks");
			// ImGuiの内部コマンドを生成する
			ImGui::Render();

			dxCommon->PreDraw();

			dxCommon->GetList()->SetGraphicsRootSignature(dxCommon->GetPSOParticle().root_.GetSignature().Get());
			dxCommon->GetList()->SetPipelineState(dxCommon->GetPSOParticle().graphicsState_.Get()); // PSOを設定
			dxCommon->GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// パーティクルの描画
			dxCommon->GetList()->IASetVertexBuffers(0, 1, &vertexBufferView); //VBVを設定
			dxCommon->GetList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
			dxCommon->GetList()->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU);
			dxCommon->GetList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
			dxCommon->GetList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
			// 描画!(DrawCall/ドローコール) 。3頂点で1つのインスタンス。 インスタンスについては今度
			dxCommon->GetList()->DrawInstanced(UINT(modelData.vertices.size()), instanceCount, 0, 0);

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

	dxCommon->Release();
	winApp->Finalize();

	return 0;
}