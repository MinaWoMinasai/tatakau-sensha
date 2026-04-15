#include "TestScene.h"

TestScene::TestScene() {}

TestScene::~TestScene() {}

void TestScene::Initialize() {

	worldTransform_ = InitWorldTransform();

	input_ = Input::GetInstance();

	debugCamera = std::make_unique<DebugCamera>();

	camera = std::make_unique<Camera>();

	camera->SetTranslate(Vector3(17.0f, 61.0f, -500.0f));

	Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());
	Object3dCommon::GetInstance()->SetDebugDefaultCamera(debugCamera.get());
	
	// 1. TrailManagerの初期化（仮にメンバ変数 std::unique_ptr<TrailManager> trailManager_ を追加）
	//trailManager_ = std::make_unique<TrailManager>();
	// 軌跡用のテクスチャを指定（とりあえず既存のものでもOK）
	//trailManager_->Initialize("resources/gradation.png");

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

	// 2. 剣に見立てた細長いブロックを作る
	swordObj_ = std::make_unique<Object3d>();
	swordObj_->Initialize();
	swordObj_->SetModel("weapon.obj"); // 既存のモデル
	swordObj_->SetScale({ 2.0f, 2.0f, 2.0f }); // 剣っぽく細長く
}

void TestScene::Update() {

#ifdef USE_IMGUI

	ImGui::Begin("FPS");
	ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);
	ImGui::Text("deltaTime: %.8f", finalDeltaTime * 60.0f);
	ImGui::End();

	ImGui::Begin("Block");
	ImGui::DragFloat3("position", &blockObj_->GetTranslate().x);
	ImGui::DragFloat3("scale", &blockObj_->GetScale().x);
	ImGui::End();

	ImGui::Begin("Ground");
	ImGui::DragFloat3("position", &groundObj_->GetTranslate().x);
	ImGui::DragFloat3("rotate", &groundObj_->GetRotate().x, 0.01f);
	ImGui::End();

#endif // USE_IMGUI

	// 1. 剣をぶん回すアニメーション（テスト用）
	static float timer = 0.0f;
	timer += 0.02f;
	swordObj_->SetRotate({ 0.0f, 0.0f, timer * 2.0f }); // Z軸でぐるぐる
	//swordObj_->SetTranslate({ std::sin(timer) * 20.0f, 0.0f, 0.0f }); // 左右に揺らす
	swordObj_->Update();

	// 2. ワールド行列から先端と根元の座標を計算
	// ※Object3dに GetWorldMatrix() がある前提。なければ計算してください
	Matrix4x4 worldMat = MakeAffineMatrix(swordObj_->GetScale(), swordObj_->GetRotate(), swordObj_->GetTranslate());

	Vector3 localBase = { 0.0f, 0.0f, 0.0f };   // 剣の根元
	Vector3 localTip = { 0.0f, 6.0f, 0.0f };  // 剣の先端（Scale.yが10ならこのあたり）

	// ローカル座標をワールド座標へ変換
	Vector3 worldBase = TransformNormal(localBase, worldMat);
	Vector3 worldTip = TransformNormal(localTip, worldMat);

	// 3. 軌跡を更新！
	//trailManager_->Update(worldTip, worldBase);

	camera->Update();
	debugCamera->Update(input_->GetMouseState(), input_->GetKey(), input_->GetLeftStick());
	groundObj_->Update();
	blockObj_->Update();
	//swordObj_->Update();

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

}

void TestScene::DrawPostEffect3D() {

	Object3dCommon::GetInstance()->PreDraw(kNone);

	blockObj_->Draw();

	groundObj_->Draw();

	swordObj_->Draw();

	Matrix4x4 vp = Multiply(debugCamera->GetViewMatrix(), debugCamera->GetProjectionMatrix());
	//trailManager_->Draw(vp);
}

void TestScene::DrawShadow() {

	Object3dCommon::GetInstance()->PreDraw(kShadow);

	blockObj_->DrawShadow();

	swordObj_->DrawShadow();
}

void TestScene::DrawSprite() {

}