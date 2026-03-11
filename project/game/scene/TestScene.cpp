#include "TestScene.h"

TestScene::TestScene() {}

TestScene::~TestScene() {}

void TestScene::Initialize() {

	worldTransform_ = InitWorldTransform();

	input_ = Input::GetInstance();

	debugCamera = std::make_unique<DebugCamera>();

	camera = std::make_unique<Camera>();

	camera->SetTranslate(Vector3(17.0f, 21.0f, -80.0f));

	Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());
	Object3dCommon::GetInstance()->SetDebugDefaultCamera(debugCamera.get());


	groundObj_ = std::make_unique<Object3d>();
	groundObj_->Initialize();
	groundObj_->SetModel("ground.obj");
	groundObj_->SetTranslate(Vector3(0.0f, -30.0f, 0.0f));
	groundObj_->SetColor(Vector4(0.5f, 0.5f, 0.5f, 1.0f));
	groundObj_->Update();
	groundObj_->SetLighting(true);

	blockObj_ = std::make_unique<Object3d>();
	blockObj_->Initialize();
	blockObj_->SetTranslate(Vector3(-10.0f, 0.0f, 0.0f));
	blockObj_->Update();
	blockObj_->SetModel("bloomBlock.obj");
	blockObj_->SetColor(Vector4(0.06f, 0.45f, 0.08f, 1.0f));
	blockObj_->SetLighting(true);

	blockObj2_ = std::make_unique<Object3d>();
	blockObj2_->Initialize();
	blockObj2_->SetTranslate(Vector3(10.0f, 0.0f, 0.0f));
	blockObj2_->Update();
	blockObj2_->SetModel("bloomBlock.obj");
	blockObj2_->SetColor(Vector4(0.06f, 0.45f, 0.08f, 1.0f));
	blockObj2_->SetLighting(true);

}

void TestScene::Update() {

	Object3dCommon::GetInstance()->Update();

#ifdef USE_IMGUI

	Vector3 direction = blockObj_->GetPointLightDirection();
	Vector3 position = blockObj_->GetPointLightPosition();

	ImGui::Begin("FPS");
	ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);
	ImGui::Text("deltaTime: %.8f", finalDeltaTime * 60.0f);
	ImGui::End();

	ImGui::Begin("Block");
	ImGui::DragFloat3("position", &blockObj_->GetTranslate().x);
	ImGui::DragFloat3("direction", &direction.x);
	ImGui::DragFloat3("pos", &position.x);
	ImGui::End();

	blockObj_->SetPointLightDirection(direction);
	blockObj_->SetPointLightPosition(position);

#endif // USE_IMGUI

	camera->Update();
	debugCamera->Update(input_->GetMouseState(), input_->GetKey(), input_->GetLeftStick());
	groundObj_->Update();
	blockObj_->Update();
	blockObj2_->Update();

#ifdef USE_IMGUI

	ImGuiIO& io = ImGui::GetIO();

	// アプリ側のクリック処理を行う前にチェック
	if (!io.WantCaptureMouse) {
		// 左クリックしたらパーティクル追加
		if (input_->IsTrigger(input_->GetMouseState().rgbButtons[0], input_->GetPreMouseState().rgbButtons[0])) {
			for (uint32_t index = 0; index < 100; ++index) {

				Vector3 worldPos = ScreenToWorld3D(input_->GetMousePosition(), debugCamera->GetViewMatrix(), MakePerspectiveForMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f), WinApp::kClientWidth, WinApp::kClientHeight, debugCamera->GetDistance());
				ParticleManager::GetInstance()->Emit(MakeParticle(worldPos, Rand()));
			}
		}
	}

#endif // USE_IMGUI

	ParticleManager::GetInstance()->Update(finalDeltaTime, camera.get(), debugCamera.get());

}

void TestScene::Draw() {

	Object3dCommon::GetInstance()->PreDraw(kNone);

	blockObj2_->Draw();

}

void TestScene::DrawPostEffect3D() {

	Object3dCommon::GetInstance()->PreDraw(kNone);

	groundObj_->Draw();

	blockObj_->Draw();

}

void TestScene::DrawShadow() {

	Object3dCommon::GetInstance()->PreDraw(kShadow);

	blockObj_->DrawShadow();
	blockObj2_->DrawShadow();

	// 影を落としたいモデルだけを描画
	//groundObj_->DrawShadow();
}

void TestScene::DrawSprite() {

}