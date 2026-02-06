#include "GameScene.h"

GameScene::GameScene() {}

GameScene::~GameScene() {

}

void GameScene::Initialize() {

	worldTransform_ = InitWorldTransform();

	input_ = Input::GetInstance();

	debugCamera = std::make_unique<DebugCamera>();

	camera = std::make_unique<Camera>();

	camera->SetTranslate(Vector3(17.0f, 21.0f, -80.0f));

	Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());
	Object3dCommon::GetInstance()->SetDebugDefaultCamera(debugCamera.get());

	enemyObject_ = std::make_unique<Object3d>();
	enemyObject_->Initialize();

	object3d3 = std::make_unique<Object3d>();
	object3d3->Initialize();

	playerObject_ = std::make_unique<Object3d>();
	playerObject_->Initialize();

	ballObj_ = std::make_unique<Object3d>();
	ballObj_->Initialize();
	ballObj_->SetTranslate(Vector3(20.0f, 0.0f, 0.0f));
	ballObj_->Update();

	ball_ = std::make_unique<Object3d>();
	ball_->Initialize();
	// ライティングを有効か
	ball_->SetLighting(true);
	ball_->SetTranslate(Vector3(-20.0f, 0.0f, 0.0f));
	ball_->Update();


	enemyObject_->SetModel("enemy.obj");
	object3d3->SetModel("plane.obj");
	playerObject_->SetModel("player.obj");
	ballObj_->SetModel("bloomBlock.obj");
	ballObj_->SetColor(Vector4(0.06f, 0.45f, 0.08f, 1.0f));
	
	ball_->SetModel("ball.obj");

	stage_ = std::make_unique<Stage>();
	stage_->Initialize();

	// 弾マネージャの生成
	bulletManager_ = std::make_unique<BulletManager>();

	player_ = std::make_unique<Player>();
	player_->Initialize(playerObject_.get(), Vector3(30.0f, 30.0f, 0.0f));
	player_->SetAttackControllerBulletManager(bulletManager_.get());

	// 敵キャラの生成
	enemy_ = std::make_unique<Enemy>();
	// 敵キャラの初期化
	enemy_->SetPlayer(player_.get());
	enemy_->Initialize(enemyObject_.get(), Vector3(20.0f, 20.0f, 0.0f), stage_.get());
	enemy_->SetAttackControllerBulletManager(bulletManager_.get());

	// 敵マネージャの生成
	//enemyManager_ = std::make_unique<EnemyManager>();
	//enemyManager_->Initialize(player_.get(), bulletManager_.get());

	// 衝突マネージャの生成
	collisionManager_ = std::make_unique<CollisionManager>();

	fade_ = std::make_unique<Fade>();
	fade_->Initialize();
	fade_->Start(Fade::Status::FadeIn, 1.0f);

	shotGide = std::make_unique<Sprite>();
	shotGide->Initialize(SpriteCommon::GetInstance(), "resources/shotGide.png");
	shotGide->SetPosition({ 20.0f, 370.0f });
	shotGide->SetSize({ 200.0f, 50.0f });

	wasdGide = std::make_unique<Sprite>();
	wasdGide->Initialize(SpriteCommon::GetInstance(), "resources/wasd.png");
	wasdGide->SetPosition({ 20.0f, 530.0f });
	wasdGide->SetSize({ 200.0f, 50.0f });

	dashGide = std::make_unique<Sprite>();
	dashGide->Initialize(SpriteCommon::GetInstance(), "resources/dashGide.png");
	dashGide->SetPosition({ 20.0f, 450.0f });
	dashGide->SetSize({ 200.0f, 50.0f });

	toTitleGide = std::make_unique<Sprite>();
	toTitleGide->Initialize(SpriteCommon::GetInstance(), "resources/toTitle.png");
	toTitleGide->SetPosition({ 20.0f, 610.0f });
	toTitleGide->SetSize({ 200.0f, 50.0f });

}

void GameScene::Update() {
	
	// 通常は 1/60秒
	float baseDeltaTime = 1.0f / 60.0f;

	// プレイヤーがスローを要求していたら timeScale を下げる
	if (player_->RequestSlow()) {
		timeScale_ = 0.01f; // 100倍スロー
	}

	// 徐々に元の時間（1.0）に戻していく処理（Lerp）
	timeScale_ += (1.0f - timeScale_) * 0.03f;

	// 最終的な deltaTime
	float finalDeltaTime = baseDeltaTime * timeScale_;
	// 一定まで速度が戻ったら完全に戻す
	if (finalDeltaTime * 60.0f > 0.95f) {
		finalDeltaTime = baseDeltaTime;
	}

	if (player_->IsChangeMode()) {
		finalDeltaTime = 0.0f;
	}

	//ImGui::Begin("FPS");
	//ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);
	//ImGui::Text("deltaTime: %.8f", finalDeltaTime * 60.0f);
	//ImGui::End();
	//
	camera->Update();
	debugCamera->Update(input_->GetMouseState(), input_->GetKey(), input_->GetLeftStick());
	//
	//ImGui::Begin("Lighting");
	//ImGui::DragFloat3("direction", &direction.x, 0.01f);
	//ImGui::DragFloat("insensity", &insensity, 0.01f);
	//ImGui::DragFloat("shininess", &shininess, 0.01f);
	//ImGui::ColorEdit4("color", &ballObj_->GetColor().x);
	//ImGui::End();
	direction = Normalize(direction);
	ball_->SetDirectionalLightDirection(direction);
	ball_->SetInsensity(insensity);
	ball_->SetShininess(shininess);

	//worldTransform_.translate.y += 0.01f;


	//ballObj_->SetTranslate(worldTransform_.translate);
	ballObj_->Update();
	ball_->Update();

	stage_->Update();
	
	player_->Update(camera.get(), *stage_, bulletManager_.get(), finalDeltaTime);
	
	enemy_->Update(finalDeltaTime);

	//enemyManager_->Update(*stage_, finalDeltaTime);
	
	bulletManager_->Update(*stage_, finalDeltaTime);
	
	// 衝突マネージャの更新
	//collisionManager_->CheckAllCollisions(player_.get(), enemy_.get(), bulletManager_.get(), enemyManager_.get());
	collisionManager_->CheckAllCollisions(player_.get(), enemy_.get(), bulletManager_.get());
	
	switch (phase_) {
	case Phase::kFadeIn:
		fade_->Update();
	
		if (fade_->IsFinished()) {
			phase_ = Phase::kMain;
		}
		break;
	case Phase::kMain:
		if (input_->IsTrigger(input_->GetKey()[DIK_ESCAPE], input_->GetPreKey()[DIK_ESCAPE])) {
			fade_->Start(Fade::Status::FadeOut, 1.0f);
			phase_ = Phase::kFadeOut;
		}
	
		if (player_->isFinished()) {
			fade_->Start(Fade::Status::FadeOut, 1.0f);
			phase_ = Phase::kFadeOut;
		}
	
		if (enemy_->isFinished()) {
			fade_->Start(Fade::Status::FadeOut, 1.0f);
			phase_ = Phase::kFadeOut;
		}
	
		break;
	case Phase::kFadeOut:
		fade_->Update();
		if (fade_->IsFinished()) {
			finished_ = true;
		}
		break;
	}
	
	shotGide->Update();
	wasdGide->Update();
	dashGide->Update();
	toTitleGide->Update();
	
	if (cameraFollow_) {
	
		// プレイヤーが死んでいる場合カメラをプレイヤーに合わせる
		if (player_->IsDead()) {
			Vector3 playerPos = player_->GetWorldPosition();
			camera->SetTranslate({ playerPos.x, playerPos.y, -50.0f });
			cameraFollow_ = false;
		}
	
		// 敵が死んでいる場合カメラを敵に合わせる
		if (enemy_->IsDead()) {
			Vector3 enemyPos = enemy_->GetWorldPosition();
			camera->SetTranslate({ enemyPos.x, enemyPos.y, -50.0f });
			cameraFollow_ = false;
		}
	}
}

void GameScene::Draw() {

	Object3dCommon::GetInstance()->PreDraw(kNone);

}

void GameScene::DrawPostEffect3D() {

	Object3dCommon::GetInstance()->PreDraw(kNone);

	player_->Draw();
	
	enemy_->Draw();
	
	//enemyManager_->Draw();

	bulletManager_->Draw();
	
	stage_->Draw();

	//ballObj_->Draw();

	enemy_->HPBarDraw();

}

void GameScene::DrawSprite() {

	enemy_->DrawSprite();
	if (!enemy_->IsDead()) {
		player_->DrawSprite();
	}
	shotGide->Draw();
	wasdGide->Draw();
	dashGide->Draw();
	toTitleGide->Draw();
	fade_->Draw();
}