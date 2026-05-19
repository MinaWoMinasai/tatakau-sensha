#include "GameScene.h"

GameScene::GameScene() {}

GameScene::~GameScene() {}

void GameScene::Initialize() {

	worldTransform_ = InitWorldTransform();

	input_ = Input::GetInstance();

	debugCamera = std::make_unique<DebugCamera>();

	camera = std::make_unique<Camera>();

	camera->SetTranslate(Vector3(17.0f, 21.0f, -80.0f));

	Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());
	Object3dCommon::GetInstance()->SetDebugDefaultCamera(debugCamera.get());

	playerPostEffect_ = std::make_unique<ObjectPostEffect>();
	playerPostEffect_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(),
		Object3dCommon::GetInstance()->GetSrvManager(),
		nullptr
	);
	{
		BloomParam& playerPost = playerPostEffect_->GetParam();
		playerPost.intensity = 2.0f;
		playerPost.outlineWidth = 0.0f;
		playerPost.outlineThreshold = 0.05f;
		playerPost.outlineColor = { 0.1f, 0.85f, 1.0f };
		playerPost.outlineBloomIntensity = 2.0f;
		playerPost.outlineBloomWidth = 1.0f;
	}

	enemyPostEffect_ = std::make_unique<ObjectPostEffect>();
	enemyPostEffect_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(),
		Object3dCommon::GetInstance()->GetSrvManager(),
		nullptr
	);
	{
		BloomParam& enemyPost = enemyPostEffect_->GetParam();
		enemyPost.intensity = 2.0f;
		enemyPost.outlineWidth = 0.0f;
		enemyPost.outlineThreshold = 0.0f;
		enemyPost.outlineColor = { 1.0f, 0.2f, 0.1f };
		enemyPost.chromAbAmount = 0.0f;
		enemyPost.outlineBloomIntensity = 0.0f;
		enemyPost.outlineBloomWidth = 0.0f;
	}

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

	groundObj_ = std::make_unique<Object3d>();
	groundObj_->Initialize();
	groundObj_->SetModel("ground.obj");
	groundObj_->SetTranslate(Vector3(0.0f, -30.0f, 0.0f));
	
	enemyObject_->SetModel("enemy.obj");
	object3d3->SetModel("plane.obj");
	playerObject_->SetModel("player3D.obj");
	//playerObject_->SetColor(Vector4(0.06f, 0.45f, 0.08f, 0.6f));
	playerObject_->SetColor(Vector4(0.06f, 0.45f, 0.08f, 0.2f));
	//playerObject_->SetLighting(true);
	ballObj_->SetModel("bloomBlock.obj");
	ballObj_->SetColor(Vector4(0.06f, 0.45f, 0.08f, 1.0f));
	ballObj_->SetLighting(true);
	
	ball_->SetModel("jewelry.obj");

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

	// 経験値敵マネージャの生成
	enemyManager_ = std::make_unique<EnemyManager>();
	enemyManager_->Initialize(player_.get(), bulletManager_.get());

	// 衝突マネージャの生成
	collisionManager_ = std::make_unique<CollisionManager>();

	fade_ = std::make_unique<Fade>();
	fade_->Initialize();
	fade_->Start(Fade::Status::FadeIn, 1.0f);
	sceneFadeBlurTimer_ = sceneFadeBlurDuration_;
	sceneFadeBlurIntensity_ = 1.0f;

	shotGide = std::make_unique<Sprite>();
	shotGide->Initialize(SpriteCommon::GetInstance(), "resources/LivePhoto.png");
	shotGide->SetPosition({ 100.0f, 100.0f });
	//shotGide->SetSize({ 200.0f, 50.0f });

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
	finalDeltaTime = baseDeltaTime * timeScale_;
	// 一定まで速度が戻ったら完全に戻す
	if (finalDeltaTime * 60.0f > 0.95f) {
		finalDeltaTime = baseDeltaTime;
	}
	slowMotionPostActive_ = finalDeltaTime < baseDeltaTime * 0.98f;

	if (player_->IsChangeMode()) {
		finalDeltaTime = 0.0f;
	}

	if (sceneFadeBlurTimer_ > 0.0f) {
		sceneFadeBlurTimer_ -= baseDeltaTime;
		float t = (std::clamp)(sceneFadeBlurTimer_ / sceneFadeBlurDuration_, 0.0f, 1.0f);
		sceneFadeBlurIntensity_ = t * t;
	} else {
		sceneFadeBlurIntensity_ = 0.0f;
	}

#ifdef USE_IMGUI

	ImGui::Begin("FPS");
	ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);
	ImGui::Text("deltaTime: %.8f", finalDeltaTime * 60.0f);
	ImGui::End();
	
	ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(500, 100), ImGuiCond_FirstUseEver);

	ImGui::Begin("Sprite");
	ImGui::SliderFloat2("position", &shotGide->GetPosition().x, 0.0f, 3000.0f, "%.1f");
	ImGui::End();

	ImGui::Begin("ballScale");
	ImGui::DragFloat3("scale", &ball_->GetScale().x);
	ImGui::End();

	ImGui::Begin("Color");
	ImGui::DragFloat3("scale", &ball_->GetScale().x);
	ImGui::ColorEdit4("color", &ball_->GetColor().x);
	ImGui::End();

	ImGui::Begin("Player Object Post");
	ImGui::Checkbox("Enable Player Post", &enablePlayerPostEffect_);
	BloomParam& playerPost = playerPostEffect_->GetParam();
	ImGui::DragFloat("Player Intensity", &playerPost.intensity, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat("Player Distortion", &playerPost.distortionAmount, 0.001f, 0.0f, 0.2f);
	ImGui::DragFloat("Player ChromAb", &playerPost.chromAbAmount, 0.001f, 0.0f, 0.2f);
	ImGui::DragFloat("Player Glitch", &playerPost.glitchAmount, 0.001f, 0.0f, 0.2f);
	ImGui::DragFloat("Player Dissolve", &playerPost.dissolveThreshold, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("Player Outline Width", &playerPost.outlineWidth, 0.1f, 0.0f, 10.0f);
	ImGui::DragFloat("Player Outline Threshold", &playerPost.outlineThreshold, 0.01f, 0.0f, 1.0f);
	ImGui::ColorEdit3("Player Outline Color", &playerPost.outlineColor.x);
	ImGui::DragFloat("Player Outline Bloom Intensity", &playerPost.outlineBloomIntensity, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat("Player Outline Bloom Width", &playerPost.outlineBloomWidth, 0.1f, 0.0f, 30.0f);
	ImGui::End();

	ImGui::Begin("Slow Motion Post");
	ImGui::Text("Slow Active: %s", slowMotionPostActive_ ? "true" : "false");
	ImGui::Checkbox("Keep Player Color During Slow", &keepPlayerColorDuringSlow_);
	ImGui::DragFloat("Slow Player ChromAb", &slowPlayerChromAbAmount_, 0.001f, 0.0f, 0.2f);
	ImGui::DragFloat("Slow Player Distortion", &slowPlayerDistortionAmount_, 0.001f, 0.0f, 0.2f);
	ImGui::DragFloat("Slow Player Glitch", &slowPlayerGlitchAmount_, 0.001f, 0.0f, 0.2f);
	ImGui::End();

	ImGui::Begin("Enemy Object Post");
	ImGui::Checkbox("Enable Enemy Post", &enableEnemyPostEffect_);
	BloomParam& enemyPost = enemyPostEffect_->GetParam();
	ImGui::DragFloat("Enemy Intensity", &enemyPost.intensity, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat("Enemy Distortion", &enemyPost.distortionAmount, 0.001f, 0.0f, 0.2f);
	ImGui::DragFloat("Enemy ChromAb", &enemyPost.chromAbAmount, 0.001f, 0.0f, 0.2f);
	ImGui::DragFloat("Enemy Glitch", &enemyPost.glitchAmount, 0.001f, 0.0f, 0.2f);
	ImGui::DragFloat("Enemy Dissolve", &enemyPost.dissolveThreshold, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("Enemy Outline Width", &enemyPost.outlineWidth, 0.1f, 0.0f, 10.0f);
	ImGui::DragFloat("Enemy Outline Threshold", &enemyPost.outlineThreshold, 0.01f, 0.0f, 1.0f);
	ImGui::ColorEdit3("Enemy Outline Color", &enemyPost.outlineColor.x);
	ImGui::DragFloat("Enemy Outline Bloom Intensity", &enemyPost.outlineBloomIntensity, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat("Enemy Outline Bloom Width", &enemyPost.outlineBloomWidth, 0.1f, 0.0f, 30.0f);
	ImGui::End();

	ImGui::Begin("RPG Progress");
	ImGui::Text("Level: %d", player_->GetLevel());
	ImGui::Text("Exp: %d / %d", player_->GetExp(), player_->GetNextLevelExpValue());
	ImGui::Text("Skill Points: %d", player_->GetSkillPoints());
	ImGui::Text("Class: %s", player_->GetCurrentClassName());
	ImGui::Separator();
	ImGui::Text("1 Health Regen  Lv.%d", player_->GetUpgradeLevel(0));
	ImGui::Text("2 Max Health    Lv.%d", player_->GetUpgradeLevel(1));
	ImGui::Text("3 Body Damage   Lv.%d", player_->GetUpgradeLevel(2));
	ImGui::Text("4 Bullet Speed  Lv.%d", player_->GetUpgradeLevel(3));
	ImGui::Text("5 Bullet Damage Lv.%d", player_->GetUpgradeLevel(4));
	ImGui::Text("6 Reload        Lv.%d", player_->GetUpgradeLevel(5));
	ImGui::Text("7 Move Speed    Lv.%d", player_->GetUpgradeLevel(6));
	ImGui::Text("Press C to open evolution cards.");
	ImGui::Separator();
	ImGui::Text("Debug Exp: F5 +next level, F6 +200");
	if (ImGui::Button("+ Next Level Exp")) {
		player_->AddExp(player_->GetNextLevelExpValue());
	}
	ImGui::SameLine();
	if (ImGui::Button("+200 Exp")) {
		player_->AddExp(200);
	}
	ImGui::End();

#endif // USE_IMGUI

#ifdef USE_IMGUI
	if (input_->IsTrigger(input_->GetKey()[DIK_F5], input_->GetPreKey()[DIK_F5])) {
		player_->AddExp(player_->GetNextLevelExpValue());
	}
	if (input_->IsTrigger(input_->GetKey()[DIK_F6], input_->GetPreKey()[DIK_F6])) {
		player_->AddExp(200);
	}
#endif // USE_IMGUI

	{
		Vector3 playerPos = player_->GetWorldPosition();
		const float kCameraZ = -55.0f;
		const float kMarginX = 18.0f;
		const float kMarginY = 11.0f;
		const float kMaxCameraX = MapChip::kBlockWidth * (MapChip::kNumBlockHorizontal - 1) - kMarginX;
		const float kMaxCameraY = MapChip::kBlockHeight * (MapChip::kNumBlockVirtical - 1) - kMarginY;
		Vector3 targetCameraPos = {
			(std::clamp)(playerPos.x, kMarginX, kMaxCameraX),
			(std::clamp)(playerPos.y, kMarginY, kMaxCameraY),
			kCameraZ
		};
		Vector3 currentCameraPos = camera->GetTranslate();
		Vector3 nextCameraPos = currentCameraPos + (targetCameraPos - currentCameraPos) * 0.12f;
		if (cameraShakeTimer_ > 0.0f) {
			cameraShakeTimer_ -= baseDeltaTime;
			float t = (std::clamp)(cameraShakeTimer_ / cameraShakeDuration_, 0.0f, 1.0f);
			float power = cameraShakePower_ * t * t;
			nextCameraPos.x += Rand(-power, power);
			nextCameraPos.y += Rand(-power, power);
		}
		camera->SetTranslate(nextCameraPos);
	}

	camera->Update();
	debugCamera->Update(input_->GetMouseState(), input_->GetKey(), input_->GetLeftStick());
	

	direction = Normalize(direction);
	ball_->SetDirectionalLightDirection(direction);
	ball_->SetInsensity(insensity);
	ball_->SetShininess(shininess);

	//worldTransform_.translate.y += 0.01f;


	//ballObj_->SetTranslate(worldTransform_.translate);
	ballObj_->Update();
	ball_->Update();
	groundObj_->Update();

	stage_->Update();
	
	player_->Update(camera.get(), *stage_, bulletManager_.get(), finalDeltaTime);
	if (player_->IsDead() && !playerDeathShakeStarted_) {
		playerDeathShakeStarted_ = true;
		cameraShakeTimer_ = cameraShakeDuration_;
		cameraShakePower_ = 1.15f;
	}
	
	enemy_->Update(finalDeltaTime);

	enemyManager_->Update(*stage_, finalDeltaTime);
	
	bulletManager_->Update(*stage_, finalDeltaTime);
	
	// 衝突マネージャの更新
	collisionManager_->CheckAllCollisions(player_.get(), enemy_.get(), bulletManager_.get(), enemyManager_.get());
	playerPostEffect_->Update(finalDeltaTime);
	enemyPostEffect_->Update(finalDeltaTime);

#ifdef USE_IMGUI

	ImGuiIO& io = ImGui::GetIO();

	// アプリ側のクリック処理を行う前にチェック
	if (!io.WantCaptureMouse) {
		// 左クリックしたらパーティクル追加
		if (input_->IsTrigger(input_->GetMouseState().rgbButtons[0], input_->GetPreMouseState().rgbButtons[0])) {
			Matrix4x4 viewMatrix = Object3dCommon::GetInstance()->GetIsDebugCamera() ? debugCamera->GetViewMatrix() : camera->GetViewMatrix();
			Matrix4x4 projectionMatrix = Object3dCommon::GetInstance()->GetIsDebugCamera() ? debugCamera->GetProjectionMatrix() : camera->GetProjectionMatrix();
			Vector3 worldPos = ScreenToWorldOnZ0(input_->GetMousePosition(), viewMatrix, projectionMatrix, WinApp::kClientWidth, WinApp::kClientHeight);
			ParticleManager::GetInstance()->EmitHitEffect(worldPos);
		}
	}

	ParticleManager::GetInstance()->DrawImGuiEditor();

#endif // USE_IMGUI

	ParticleManager::GetInstance()->Update(finalDeltaTime, camera.get(), debugCamera.get());

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
	
}

void GameScene::Draw() {

	Object3dCommon::GetInstance()->PreDraw(kNormal);

}

void GameScene::DrawPostEffect3D() {

	Object3dCommon::GetInstance()->PreDraw(kNormal);

	player_->Draw(!enablePlayerPostEffect_);
	
	enemy_->Draw(!enableEnemyPostEffect_);
	
	enemyManager_->Draw();
	
	bulletManager_->Draw();
	
	stage_->Draw();
	//
	ballObj_->Draw();
	//
	enemy_->HPBarDraw();
	//
	//groundObj_->Draw();

	ball_->Draw();

	if (enablePlayerPostEffect_ && !(slowMotionPostActive_ && keepPlayerColorDuringSlow_)) {
		playerPostEffect_->BeginCapture();
		Object3dCommon::GetInstance()->PreDraw(kNormal);
		player_->DrawBodyOnly();
		playerPostEffect_->EndCapture();
		Object3dCommon::GetInstance()->PreDraw(kNormal);
	}

	if (enableEnemyPostEffect_) {
		enemyPostEffect_->BeginCapture();
		Object3dCommon::GetInstance()->PreDraw(kNormal);
		enemy_->DrawBodyOnly();
		enemyPostEffect_->EndCapture();
		Object3dCommon::GetInstance()->PreDraw(kNormal);
	}

	
	ParticleManager::GetInstance()->Draw();

}

void GameScene::DrawAfterPostEffect3D() {
	if (!enablePlayerPostEffect_ || !slowMotionPostActive_ || !keepPlayerColorDuringSlow_) {
		return;
	}

	BloomParam savedParam = playerPostEffect_->GetParam();
	BloomParam slowParam = savedParam;
	if (slowParam.chromAbAmount < slowPlayerChromAbAmount_) {
		slowParam.chromAbAmount = slowPlayerChromAbAmount_;
	}
	if (slowParam.distortionAmount < slowPlayerDistortionAmount_) {
		slowParam.distortionAmount = slowPlayerDistortionAmount_;
	}
	if (slowParam.glitchAmount < slowPlayerGlitchAmount_) {
		slowParam.glitchAmount = slowPlayerGlitchAmount_;
	}
	playerPostEffect_->SetParam(slowParam);

	playerPostEffect_->BeginCapture();
	Object3dCommon::GetInstance()->PreDraw(kNormal);
	player_->DrawBodyOnly();
	playerPostEffect_->EndCapture();
	Object3dCommon::GetInstance()->PreDraw(kNormal);

	playerPostEffect_->SetParam(savedParam);
}

void GameScene::DrawShadow() {
	// 影用の共通設定（PSOの切り替えなど）は Object3dCommon 側で行う
	//Object3dCommon::GetInstance()->PreDraw(kShadow);

	// 影を落としたいモデルだけを描画
	//ballObj_->DrawShadow();
	//ball_->DrawShadow();
	//groundObj_->DrawShadow();
}

void GameScene::DrawSprite() {

	enemy_->DrawSprite();
	if (!enemy_->IsDead()) {
		player_->DrawSprite();
	}
	player_->DrawEncyclopedia();
	//shotGide->Draw();
	wasdGide->Draw();
	dashGide->Draw();
	toTitleGide->Draw();
	if (phase_ != Phase::kFadeIn) {
		fade_->Draw();
	}
}

std::string GameScene::GetNextSceneName() const
{
	return "TITLE";
}
