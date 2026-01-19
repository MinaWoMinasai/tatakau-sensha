#include "GameScene.h"

GameScene::GameScene() {}

GameScene::~GameScene() {

}

void GameScene::Initialize() {

	input_ = Input::GetInstance();

	debugCamera = std::make_unique<DebugCamera>();

	camera = std::make_unique<Camera>();

	camera->SetTranslate(Vector3(17.0f, 21.0f, -80.0f));

	Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());
	Object3dCommon::GetInstance()->SetDebugDefaultCamera(debugCamera.get());

	object3d = std::make_unique<Object3d>();
	object3d->Initialize();

	enemyObject_ = std::make_unique<Object3d>();
	enemyObject_->Initialize();

	object3d3 = std::make_unique<Object3d>();
	object3d3->Initialize();

	playerObject_ = std::make_unique<Object3d>();
	playerObject_->Initialize();

	object3d->SetModel("teapot.obj");
	enemyObject_->SetModel("enemy.obj");
	object3d3->SetModel("plane.obj");
	playerObject_->SetModel("player.obj");

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

	// 衝突マネージャの生成
	collisionManager_ = std::make_unique<CollisionManager>();

	fade_ = std::make_unique<Fade>();
	fade_->Initialize();
	fade_->Start(Fade::Status::FadeIn, 1.0f);

	shotGide = std::make_unique<Sprite>();
	shotGide->Initialize(SpriteCommon::GetInstance(), "resources/shotGide.png");
	shotGide->SetPosition({ 20.0f, 450.0f });
	shotGide->SetSize({ 200.0f, 50.0f });

	wasdGide = std::make_unique<Sprite>();
	wasdGide->Initialize(SpriteCommon::GetInstance(), "resources/wasd.png");
	wasdGide->SetPosition({ 20.0f, 530.0f });
	wasdGide->SetSize({ 200.0f, 50.0f });

	toTitleGide = std::make_unique<Sprite>();
	toTitleGide->Initialize(SpriteCommon::GetInstance(), "resources/toTitle.png");
	toTitleGide->SetPosition({ 20.0f, 610.0f });
	toTitleGide->SetSize({ 200.0f, 50.0f });
}

void GameScene::Update() {

	ImGui::Begin("FPS");
	ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);
	ImGui::End();

	worldTransform_.translate.y += 0.01f;

	object3d->SetTranslate(worldTransform_.translate);
	object3d->Update();

	camera->Update();
	debugCamera->Update(input_->GetMouseState(), input_->GetKey(), input_->GetLeftStick());

	stage_->Update();

	player_->Update(camera.get(), *stage_, bulletManager_.get());
	
	//enemy_->Update();

	bulletManager_->Update();

	// 弾とブロックの当たり判定
	stage_->ResolveBulletsCollision(bulletManager_->GetBulletPtrs());
    
	// 衝突マネージャの更新
	//collisionManager_->CheckAllCollisions(player_.get(), enemy_.get(), bulletManager_.get());

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

	Object3dCommon::GetInstance()->PreDraw();

	player_->Draw();

	//enemy_->Draw();

	bulletManager_->Draw();

	stage_->Draw();

}

void GameScene::DrawSprite() {

	enemy_->DrawSprite();
	player_->DrawSprite();
	shotGide->Draw();
	wasdGide->Draw();
	toTitleGide->Draw();
	fade_->Draw();
}