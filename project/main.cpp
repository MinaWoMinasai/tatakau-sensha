#define DIRECTINPUT_VERSION 0x0800
#include "Easing.h"
#include "WinApp.h"
#include <numbers>
#include "DirectXCommon.h"
#include "debugCamera.h"
#include "LogWrite.h"
#include "LoadFile.h"
#include "Dump.h"
#include "Texture.h"
#include "InputDesc.h"
#include "Root.h"
#include "State.h"
#include "Resource.h"
#include "Audio.h"
#include "SpriteCommon.h"
#include "Sprite.h"

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	D3DResouceLeakCheaker leakCheck;

	// COMの初期化を行う
	CoInitializeEx(0, COINIT_MULTITHREADED);

	MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);

	// 誰も捕捉しなかった場合に(Unheadled)、捕捉する関数を登録
	// mein関数始まってすぐに登録する
	Dump dump;
	SetUnhandledExceptionFilter(dump.Export);

	std::unique_ptr<WinApp> winApp;
	winApp = std::make_unique<WinApp>();
	winApp->Initialize();

	std::unique_ptr<DirectXCommon> dxCommon;
	dxCommon = std::make_unique<DirectXCommon>();
	dxCommon->Initialize(winApp.get());

	std::unique_ptr<SpriteCommon> spriteCommon;
	spriteCommon = std::make_unique<SpriteCommon>();
	spriteCommon->Initialize(dxCommon.get());

	std::unique_ptr<Sprite> sprite;
	sprite = std::make_unique<Sprite>();
	sprite->Initialize(spriteCommon.get());

	// キーの初期化
	Input input;
	input.Initialize(winApp->GetWindowClass(), winApp->GetHwnd());

	// リソースを作成する
	Resource resource;
	Texture texture;

	struct Vertex {
		Vector3 position;
		Vector4 color;
	};

	LoadFile loadFile;
	ModelData modelData = loadFile.Obj("resources", "plane.obj");
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	Microsoft::WRL::ComPtr <ID3D12Resource> vertexResource;
	vertexBufferView = resource.CreateVBV(modelData, texture, dxCommon->GetDevice(), vertexResource);

	// 頂点データを1000こ格納できるリソースを作成
	const uint32_t kNumInstance = 10000000;
	// Instancing用のTransformationMatrixリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource =
		texture.CreateBufferResource(dxCommon->GetDevice(), sizeof(ParticleForGPU) * kNumInstance);
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

	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource = texture.CreateBufferResource(dxCommon->GetDevice(), sizeof(Camera));
	Camera* cameraData = nullptr;
	cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));

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
	DirectX::ScratchImage mipImages = texture.Load("resources/uvChecker.png");
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
	
	sprite->SetSrvHandleGPU(textureSrvHandleGPU);

	DebugCamera debugCamera;

	Matrix4x4 billboardMatrix;

	AccelerationField accelerationField;
	accelerationField.acceleration = { 0.5f, 0.0f, 0.0f };
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

			// デバッグカメラ
			debugCamera.Update(input.GetMouseState(), input.GetKey(), leftStick);
			cameraData->worldPosition = debugCamera.GetEyePosition();
			Matrix4x4 viewMatrix = debugCamera.GetViewMatrix();
			Matrix4x4 projectionMatrix = MakePerspectiveForMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f);
			
			if (input.IsPress(input.GetMouseState().rgbButtons[0])) {
				for (uint32_t index = 0; index < 500; ++index) {

					Vector3 worldPos = ScreenToWorld3D(mousePos, debugCamera.GetViewMatrix(), projectionMatrix, WinApp::kClientWidth, WinApp::kClientHeight, debugCamera.GetDistance());
					particles.push_back(MakeParticle(worldPos));
				}
			}

			const float kDeltaTime = 1.0f / 60.0f;
			// パーティクルの移動
			for (auto& particle : particles) {
				// フィールドとパーティクル(球)の当たり判定をとる
				//if (IsCollision(accelerationField.area, Sphere(particle.transform.translate, 0.1f))) {
				//	particle.velocity += accelerationField.acceleration * kDeltaTime;
				//}
				particle.currentTime += kDeltaTime;
				//particle.velocity -= particle.acceleration * kDeltaTime;
				particle.velocity = particle.kVelocity * (1.0f - (particle.currentTime / particle.lifeTime));
				particle.kVelocity += particle.acceleration;
				//particle.velocity = particle.velocity * (1.0f * particle.currentTime);
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
				if (index >= kNumInstance){
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

			sprite->Update();

			// ImGuiの内部コマンドを生成する
			ImGui::Render();

			dxCommon->PreDraw();

			spriteCommon->PreDraw();

			ID3D12DescriptorHeap* heaps[] = { dxCommon->GetSrvHeap().Get() };
			dxCommon->GetList()->SetDescriptorHeaps(1, heaps);

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
			dxCommon->GetList()->DrawInstanced(UINT(modelData.vertices.size()), UINT(particles.size()), 0, 0);

			spriteCommon->PreDraw();

			sprite->Draw();

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