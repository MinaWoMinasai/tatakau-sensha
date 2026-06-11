#include "GameScene.h"
#include "CollisionConfig.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>

namespace {

Vector4 GetCollisionDebugColor(uint32_t attribute) {
	if (attribute & kCollisionAttributePlayer) {
		return { 0.15f, 1.0f, 0.95f, 0.85f };
	}
	if (attribute & kCollisionAttributePlayerDrone) {
		return { 0.25f, 0.75f, 1.0f, 0.85f };
	}
	if (attribute & kCollisionAttributeEnemy) {
		return { 1.0f, 0.18f, 0.35f, 0.85f };
	}
	if (attribute & kCollisionAttributePlayerBullet) {
		return { 1.0f, 0.95f, 0.2f, 0.75f };
	}
	if (attribute & kCollisionAttributeEnemyBullet) {
		return { 1.0f, 0.35f, 0.1f, 0.75f };
	}
	return { 1.0f, 1.0f, 1.0f, 0.7f };
}

bool IsBulletCollider(uint32_t attribute) {
	return (attribute & (kCollisionAttributePlayerBullet | kCollisionAttributeEnemyBullet)) != 0;
}

int ReadCustomInt(const nlohmann::json& customProperties, const char* key, int fallback)
{
	if (!customProperties.is_object() || !customProperties.contains(key) || !customProperties[key].is_number()) {
		return fallback;
	}
	return customProperties[key].get<int>();
}

bool TryGetItemModel(const std::string& prefab, std::string& model)
{
	if (prefab == "Default" || prefab == "Heal") {
		model = "jewelry.obj";
		return true;
	}
	if (prefab == "Power") {
		model = "ball.obj";
		return true;
	}
	if (prefab == "Exp") {
		model = "bloomBall.obj";
		return true;
	}
	return false;
}

bool ReadCustomBool(const nlohmann::json& customProperties, const char* key, bool fallback)
{
	if (!customProperties.is_object() || !customProperties.contains(key) || !customProperties[key].is_boolean()) {
		return fallback;
	}
	return customProperties[key].get<bool>();
}

float ReadCustomFloat(const nlohmann::json& customProperties, const char* key, float fallback)
{
	if (!customProperties.is_object() || !customProperties.contains(key) || !customProperties[key].is_number()) {
		return fallback;
	}
	return customProperties[key].get<float>();
}

std::string ReadCustomString(const nlohmann::json& customProperties, const char* key, const std::string& fallback)
{
	if (!customProperties.is_object() || !customProperties.contains(key) || !customProperties[key].is_string()) {
		return fallback;
	}
	return customProperties[key].get<std::string>();
}

Vector4 ReadJsonVector4(const nlohmann::json& json, const Vector4& fallback)
{
	if (!json.is_object()) {
		return fallback;
	}

	Vector4 value = fallback;
	if (json.contains("x") && json["x"].is_number()) value.x = json["x"].get<float>();
	if (json.contains("y") && json["y"].is_number()) value.y = json["y"].get<float>();
	if (json.contains("z") && json["z"].is_number()) value.z = json["z"].get<float>();
	if (json.contains("w") && json["w"].is_number()) value.w = json["w"].get<float>();
	return value;
}

Vector3 ReadJsonVector3(const nlohmann::json& json, const Vector3& fallback)
{
	if (!json.is_object()) {
		return fallback;
	}

	Vector3 value = fallback;
	if (json.contains("x") && json["x"].is_number()) value.x = json["x"].get<float>();
	if (json.contains("y") && json["y"].is_number()) value.y = json["y"].get<float>();
	if (json.contains("z") && json["z"].is_number()) value.z = json["z"].get<float>();
	return value;
}

RingEffectConfig MakeCollisionRingConfig(float radius, const Vector4& color) {
	RingEffectConfig config{};
	config.lifeTime = 0.045f;
	config.startRadius = radius;
	config.endRadius = radius;
	config.startWidth = 0.045f;
	config.endWidth = 0.045f;
	config.rotate = { 0.0f, 0.0f, 0.0f };
	config.startColor = color;
	config.endColor = color;
	config.divisions = 64;
	return config;
}

void EmitColliderDebugRings(RingManager& ringManager, Collider& collider) {
	const Vector4 color = GetCollisionDebugColor(collider.GetCollisionAttribute());
	if (collider.GetShape() == ColliderShape::Capsule) {
		const Segment& segment = collider.GetSegment();
		const Vector3 start = segment.origin;
		const Vector3 end = segment.origin + segment.diff;
		const Vector3 middle = start + segment.diff * 0.5f;
		RingEffectConfig config = MakeCollisionRingConfig(collider.GetCapsuleRadius(), color);
		ringManager.Emit(start, config);
		ringManager.Emit(middle, config);
		ringManager.Emit(end, config);
		return;
	}

	ringManager.Emit(collider.GetWorldPosition(), MakeCollisionRingConfig(collider.GetRadius(), color));
}

Vector3 GetActiveCameraPosition(Camera* camera, DebugCamera* debugCamera) {
	return Object3dCommon::GetInstance()->GetIsDebugCamera() && debugCamera
		? debugCamera->GetEyePosition()
		: camera->GetTranslate();
}

} // namespace

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
		nullptr,
		0.5f
	);
	{
		BloomParam& playerPost = playerPostEffect_->GetParam();
		playerPost.threshold = 0.0f;
		playerPost.intensity = 1.0f;
		playerPost.outlineWidth = 0.0f;
		playerPost.outlineThreshold = 0.0f;
		playerPost.outlineColor = { 0.12f, 1.0f, 0.32f };
		playerPost.outlineBloomIntensity = 0.27f;
		playerPost.outlineBloomWidth = 2.7f;
	}

	enemyPostEffect_ = std::make_unique<ObjectPostEffect>();
	enemyPostEffect_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(),
		Object3dCommon::GetInstance()->GetSrvManager(),
		nullptr,
		0.5f
	);
	{
		BloomParam& enemyPost = enemyPostEffect_->GetParam();
		enemyPost.intensity = 1.2f;
		enemyPost.outlineWidth = 0.0f;
		enemyPost.outlineThreshold = 0.0f;
		enemyPost.outlineColor = { 1.0f, 0.2f, 0.1f };
		enemyPost.chromAbAmount = 0.0f;
		enemyPost.outlineBloomIntensity = 0.27f;
		enemyPost.outlineBloomWidth = 2.7f;
	}

	expEnemyPostEffect_ = std::make_unique<ObjectPostEffect>();
	expEnemyPostEffect_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(),
		Object3dCommon::GetInstance()->GetSrvManager(),
		nullptr,
		0.5f
	);
	{
		BloomParam& expEnemyPost = expEnemyPostEffect_->GetParam();
		expEnemyPost.intensity = 1.0f;
		expEnemyPost.threshold = 0.0f;
		expEnemyPost.outlineWidth = 0.0f;
		expEnemyPost.outlineThreshold = 0.0f;
		expEnemyPost.outlineColor = { 0.9f, 0.15f, 0.98f };
		expEnemyPost.chromAbAmount = 0.0f;
		expEnemyPost.outlineBloomIntensity = 0.27f;
		expEnemyPost.outlineBloomWidth = 2.7f;
	}

	stagePostEffect_ = std::make_unique<ObjectPostEffect>();
	stagePostEffect_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(),
		Object3dCommon::GetInstance()->GetSrvManager(),
		nullptr,
		0.5f
	);
	{
		BloomParam& stagePost = stagePostEffect_->GetParam();
		stagePost.threshold = 0.0f;
		stagePost.intensity = 0.75f;
		stagePost.outlineWidth = 0.0f;
		stagePost.outlineThreshold = 0.0f;
		stagePost.outlineColor = { 1.0f, 0.55f, 0.58f };
		stagePost.chromAbAmount = 0.0f;
		stagePost.outlineBloomIntensity = 0.0f;
		stagePost.outlineBloomWidth = 0.0f;
	}

	neonGridPostEffect_ = std::make_unique<ObjectPostEffect>();
	neonGridPostEffect_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(),
		Object3dCommon::GetInstance()->GetSrvManager(),
		nullptr,
		0.5f
	);
	{
		BloomParam& gridPost = neonGridPostEffect_->GetParam();
		gridPost.threshold = 0.0f;
		gridPost.intensity = 1.5f;
		gridPost.outlineWidth = 0.0f;
		gridPost.outlineThreshold = 0.0f;
		gridPost.outlineBloomIntensity = 0.0f;
		gridPost.outlineBloomWidth = 0.0f;
	}

	bulletTrailPostEffect_ = std::make_unique<ObjectPostEffect>();
	bulletTrailPostEffect_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(),
		Object3dCommon::GetInstance()->GetSrvManager(),
		nullptr,
		0.5f
	);
	{
		BloomParam& bulletTrailPost = bulletTrailPostEffect_->GetParam();
		bulletTrailPost.threshold = 0.0f;
		bulletTrailPost.intensity = 2.0f;
		bulletTrailPost.outlineWidth = 0.0f;
		bulletTrailPost.outlineThreshold = 0.0f;
		bulletTrailPost.outlineBloomIntensity = 0.0f;
		bulletTrailPost.outlineBloomWidth = 0.0f;
	}

	sharedObjectBloomPostEffect_ = std::make_unique<ObjectPostEffect>();
	sharedObjectBloomPostEffect_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(),
		Object3dCommon::GetInstance()->GetSrvManager(),
		nullptr,
		0.5f
	);
	{
		BloomParam& objectBloomPost = sharedObjectBloomPostEffect_->GetParam();
		objectBloomPost.threshold = 0.0f;
		objectBloomPost.intensity = 1.15f;
		objectBloomPost.outlineWidth = 0.0f;
		objectBloomPost.outlineThreshold = 0.0f;
		objectBloomPost.outlineBloomIntensity = 0.0f;
		objectBloomPost.outlineBloomWidth = 0.0f;
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

	TextureManager::GetInstance()->LoadTexture("resources/skybox.dds");
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize("resources/skybox.dds");
	const uint32_t skyboxTextureIndex = TextureManager::GetInstance()->GetSrvIndex("resources/skybox.dds");
	
	enemyObject_->SetModel("enemy3D.obj");
	enemyObject_->SetLighting(true);
	enemyObject_->SetEnvironmentMap(skyboxTextureIndex);
	enemyObject_->SetEnvironmentCoefficient(0.75f);
	object3d3->SetModel("plane.obj");
	playerObject_->SetModel("player3D.obj");
	playerObject_->SetColor(Vector4(0.48f, 0.86f, 0.22f, 1.0f));
	//playerObject_->SetLighting(true);
	ballObj_->SetModel("bloomBlock.obj");
	ballObj_->SetColor(Vector4(0.06f, 0.45f, 0.08f, 1.0f));
	ballObj_->SetLighting(true);
	
	ball_->SetModel("jewelry.obj");

	stage_ = std::make_unique<Stage>();
	stage_->Initialize();

	// 弾マネージャの生成
	bulletManager_ = std::make_unique<BulletManager>();
	bulletManager_->Initialize(Object3dCommon::GetInstance()->GetDxCommon(), Object3dCommon::GetInstance());

	LevelData levelData;
	const bool hasLevelData = LoadLevelFile(levelData);
	Vector3 playerSpawnPosition = Vector3(30.0f, 30.0f, 0.0f);
	if (hasLevelData) {
		for (const LevelObject& object : levelData.objects) {
			if (object.type == "PlayerSpawn") {
				if (object.prefab != "Default") {
					std::cerr << "[LevelLoader] Unsupported PlayerSpawn prefab: " << object.prefab << std::endl;
				}
				playerSpawnPosition = object.transform.translate;
				break;
			}
		}
	}

	player_ = std::make_unique<Player>();
	player_->Initialize(playerObject_.get(), playerSpawnPosition);
	player_->SetAttackControllerBulletManager(bulletManager_.get());

	// 敵キャラの生成
	enemy_ = std::make_unique<Enemy>();
	// 敵キャラの初期化
	enemy_->SetPlayer(player_.get());
	Vector3 bossSpawnPosition = Vector3(20.0f, 20.0f, 0.0f);
	if (hasLevelData) {
		for (const LevelObject& object : levelData.objects) {
			if (object.type == "BossSpawn") {
				if (object.prefab != "Default") {
					std::cerr << "[LevelLoader] Unsupported BossSpawn prefab: " << object.prefab << std::endl;
				}
				bossSpawnPosition = object.transform.translate;
				break;
			}
		}
	}
	enemy_->Initialize(enemyObject_.get(), bossSpawnPosition, stage_.get());
	enemy_->SetAttackControllerBulletManager(bulletManager_.get());

	// 経験値敵マネージャの生成
	enemyManager_ = std::make_unique<EnemyManager>();
	enemyManager_->Initialize(player_.get(), bulletManager_.get());
	if (hasLevelData) {
		ApplyLevelData(levelData);
	}

	// 衝突マネージャの生成
	collisionManager_ = std::make_unique<CollisionManager>();
	collisionDebugRingManager_ = std::make_unique<RingManager>();
	collisionDebugRingManager_->Initialize(Object3dCommon::GetInstance()->GetDxCommon(), "resources/gradationLine.png");
	neonGridRenderer_ = std::make_unique<NeonGridRenderer>();
	neonGridRenderer_->Initialize(Object3dCommon::GetInstance()->GetDxCommon(), "resources/white512x512.png");

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

	dashGuideText_ = std::make_unique<TextLabel>();
	dashGuideText_->InitializeFromJson(SpriteCommon::GetInstance(), "resources/configs/gameText.json", "dashGuide");

	moveGuideText_ = std::make_unique<TextLabel>();
	moveGuideText_->InitializeFromJson(SpriteCommon::GetInstance(), "resources/configs/gameText.json", "moveGuide");

	titleGuideText_ = std::make_unique<TextLabel>();
	titleGuideText_->InitializeFromJson(SpriteCommon::GetInstance(), "resources/configs/gameText.json", "titleGuide");

	TextStyle fpsStyle{};
	fpsStyle.fontFamily = "Meiryo";
	fpsStyle.fontSize = 24.0f;
	fpsStyle.color = { 0.70f, 1.0f, 0.78f, 1.0f };
	fpsStyle.outlineColor = { 0.0f, 0.0f, 0.0f, 0.88f };
	fpsStyle.outlineThickness = 2.0f;
	fpsStyle.padding = 5.0f;
	fpsText_ = std::make_unique<TextLabel>();
	fpsText_->Initialize(SpriteCommon::GetInstance(), "FPS: --", fpsStyle);
	fpsText_->SetPosition({ 16.0f, 14.0f });
	fpsLastSampleTime_ = std::chrono::steady_clock::now();

	TextStyle profileStyle = fpsStyle;
	profileStyle.fontSize = 18.0f;
	profileStyle.color = { 0.75f, 0.95f, 1.0f, 1.0f };
	profileStyle.outlineThickness = 2.0f;
	postProfileText_ = std::make_unique<TextLabel>();
	postProfileText_->Initialize(SpriteCommon::GetInstance(), "Post Profile  F8:hide  F9:mode", profileStyle);
	postProfileText_->SetPosition({ 16.0f, 46.0f });

	InitializeFollowHpBarBatch();

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

	{
		const auto now = std::chrono::steady_clock::now();
		const float realDeltaTime = std::chrono::duration<float>(now - fpsLastSampleTime_).count();
		fpsLastSampleTime_ = now;
		fpsAccumulatedTime_ += realDeltaTime;
		++fpsFrameCount_;
		if (fpsText_ && fpsAccumulatedTime_ >= 0.25f) {
			const float fps = static_cast<float>(fpsFrameCount_) / fpsAccumulatedTime_;
			char text[32]{};
			std::snprintf(text, sizeof(text), "FPS: %.0f", fps);
			fpsText_->SetText(text);
			fpsAccumulatedTime_ = 0.0f;
			fpsFrameCount_ = 0;
		}
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
	ImGui::Text("Exp Enemies: %zu", enemyManager_ ? enemyManager_->GetEnemyCount() : 0);
	ImGui::Text("Trail Instances: %zu", bulletManager_ ? bulletManager_->GetTrailInstanceCount() : 0);
	ImGui::End();

	ImGui::Begin("Post Effect Toggles");
	if (ImGui::Button("Enable All Posts")) {
		enableNeonGridPostEffect_ = true;
		enableStagePostEffect_ = true;
		enableBulletTrailPostEffect_ = true;
		enablePlayerPostEffect_ = true;
		enableEnemyPostEffect_ = true;
		enableExpEnemyPostEffect_ = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Disable All Posts")) {
		enableNeonGridPostEffect_ = false;
		enableStagePostEffect_ = false;
		enableBulletTrailPostEffect_ = false;
		enablePlayerPostEffect_ = false;
		enableEnemyPostEffect_ = false;
		enableExpEnemyPostEffect_ = false;
	}
	ImGui::Checkbox("Grid Post", &enableNeonGridPostEffect_);
	ImGui::Checkbox("Stage Post", &enableStagePostEffect_);
	ImGui::Checkbox("Bullet Trail Post", &enableBulletTrailPostEffect_);
	ImGui::Checkbox("Player Post", &enablePlayerPostEffect_);
	ImGui::Checkbox("Boss Enemy Post", &enableEnemyPostEffect_);
	ImGui::Checkbox("Exp Enemy Post", &enableExpEnemyPostEffect_);
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

	ImGui::Begin("Exp Enemy Object Post");
	ImGui::Checkbox("Enable Exp Enemy Post", &enableExpEnemyPostEffect_);
	BloomParam& expEnemyPost = expEnemyPostEffect_->GetParam();
	ImGui::DragFloat("Exp Enemy Intensity", &expEnemyPost.intensity, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat("Exp Enemy Distortion", &expEnemyPost.distortionAmount, 0.001f, 0.0f, 0.2f);
	ImGui::DragFloat("Exp Enemy ChromAb", &expEnemyPost.chromAbAmount, 0.001f, 0.0f, 0.2f);
	ImGui::DragFloat("Exp Enemy Glitch", &expEnemyPost.glitchAmount, 0.001f, 0.0f, 0.2f);
	ImGui::DragFloat("Exp Enemy Dissolve", &expEnemyPost.dissolveThreshold, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("Exp Enemy Outline Width", &expEnemyPost.outlineWidth, 0.1f, 0.0f, 100.0f);
	ImGui::DragFloat("Exp Enemy Outline Threshold", &expEnemyPost.outlineThreshold, 0.01f, 0.0f, 1.0f);
	ImGui::ColorEdit3("Exp Enemy Outline Color", &expEnemyPost.outlineColor.x);
	ImGui::DragFloat("Exp Enemy Outline Bloom Intensity", &expEnemyPost.outlineBloomIntensity, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat("Exp Enemy Outline Bloom Width", &expEnemyPost.outlineBloomWidth, 0.1f, 0.0f, 30.0f);
	ImGui::DragFloat("Exp Enemy Post Cull Half Width", &expEnemyPostVisibleHalfWidth_, 0.5f, 10.0f, 80.0f);
	ImGui::DragFloat("Exp Enemy Post Cull Half Height", &expEnemyPostVisibleHalfHeight_, 0.5f, 10.0f, 60.0f);
	ImGui::End();

	ImGui::Begin("Stage Object Post");
	ImGui::Checkbox("Enable Stage Post", &enableStagePostEffect_);
	BloomParam& stagePost = stagePostEffect_->GetParam();
	ImGui::DragFloat("Stage Intensity", &stagePost.intensity, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat("Stage Distortion", &stagePost.distortionAmount, 0.001f, 0.0f, 0.2f);
	ImGui::DragFloat("Stage ChromAb", &stagePost.chromAbAmount, 0.001f, 0.0f, 0.2f);
	ImGui::DragFloat("Stage Glitch", &stagePost.glitchAmount, 0.001f, 0.0f, 0.2f);
	ImGui::Text("Stage uses bloom-only add pass for performance.");
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

	DrawLevelAIDitorBalanceLab();

	player_->DrawPlayerClassEditor();

	ImGui::Begin("Collision Debug");
	ImGui::Checkbox("Show Colliders (F7)", &showCollisionDebug_);
	ImGui::Checkbox("Show Bullet Colliders", &showCollisionDebugBullets_);
	ImGui::Text("Player/Drone: cyan, Enemy: red, Bullets: yellow/orange");
	ImGui::End();

	ImGui::Begin("World HP Bars");
	ImGui::Checkbox("Show Follow HP Bars", &showFollowHpBars_);
	ImGui::Text("White outline / black missing HP / green HP");
	ImGui::End();

	ImGui::Begin("Neon Grid");
	ImGui::Checkbox("Show World Grid", &showNeonGrid_);
	ImGui::Checkbox("Show Actor Local Grid", &showActorLocalGrid_);
	ImGui::Checkbox("Enable Grid Post", &enableNeonGridPostEffect_);
	ImGui::DragFloat("World Spacing", &worldGridSpacing_, 0.05f, 0.25f, 8.0f);
	ImGui::DragFloat("World Line Width", &worldGridLineWidth_, 0.005f, 0.005f, 0.5f);
	ImGui::ColorEdit4("World Color", &worldGridColor_.x);
	ImGui::DragFloat("Actor Radius", &actorGridRadius_, 0.05f, 0.5f, 16.0f);
	ImGui::DragFloat("Actor Spacing", &actorGridSpacing_, 0.025f, 0.2f, 3.0f);
	ImGui::DragFloat("Actor Line Width", &actorGridLineWidth_, 0.005f, 0.005f, 0.5f);
	ImGui::ColorEdit4("Player Grid", &playerGridColor_.x);
	ImGui::ColorEdit4("Enemy Grid", &enemyGridColor_.x);
	ImGui::ColorEdit4("Exp Enemy Grid", &expEnemyGridColor_.x);
	ImGui::Checkbox("Cull Actor Local Grid", &cullActorLocalGrid_);
	ImGui::DragInt("Max Exp Enemy Local Grids", &maxExpEnemyLocalGrids_, 1.0f, 0, 60);
	BloomParam& gridPost = neonGridPostEffect_->GetParam();
	ImGui::DragFloat("Grid Bloom Intensity", &gridPost.intensity, 0.01f, 0.0f, 6.0f);
	ImGui::DragFloat("Grid Bloom Threshold", &gridPost.threshold, 0.01f, 0.0f, 2.0f);
	ImGui::End();

	ImGui::Begin("Bullet Trail");
	ImGui::Checkbox("Enable Bullet Trail Post", &enableBulletTrailPostEffect_);
	BulletTrailSettings& bulletTrail = bulletManager_->GetTrailSettings();
	ImGui::DragFloat("Player Trail Half Width", &bulletTrail.playerHalfWidth, 0.01f, 0.01f, 1.5f);
	ImGui::DragFloat("Enemy Trail Half Width", &bulletTrail.enemyHalfWidth, 0.01f, 0.01f, 1.5f);
	ImGui::DragFloat("Trail Lifetime", &bulletTrail.lifetime, 0.01f, 0.02f, 1.5f);
	ImGui::DragInt("Trail Max Points", &bulletTrail.maxPoints, 1.0f, 2, 80);
	ImGui::DragInt("Trail Interpolation", &bulletTrail.interpolationSteps, 1.0f, 1, 12);
	ImGui::DragFloat("Head Width Scale", &bulletTrail.headWidthScale, 0.01f, 0.0f, 4.0f);
	ImGui::DragFloat("Tail Width Scale", &bulletTrail.tailWidthScale, 0.01f, 0.0f, 4.0f);
	ImGui::Checkbox("Use Object Color For Trail", &bulletTrail.useObjectColorForTrail);
	ImGui::ColorEdit4("Player Bullet Color", &bulletTrail.playerObjectColor.x);
	ImGui::ColorEdit4("Enemy Bullet Color", &bulletTrail.enemyObjectColor.x);
	ImGui::ColorEdit4("Reflect Bullet Color", &bulletTrail.reflectableObjectColor.x);
	if (bulletTrail.useObjectColorForTrail) {
		ImGui::DragFloat("Trail Head Intensity", &bulletTrail.trailHeadIntensity, 0.01f, 0.0f, 5.0f);
		ImGui::DragFloat("Trail Tail Intensity", &bulletTrail.trailTailIntensity, 0.01f, 0.0f, 5.0f);
		ImGui::DragFloat("Trail Head Alpha", &bulletTrail.trailHeadAlpha, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Trail Tail Alpha", &bulletTrail.trailTailAlpha, 0.01f, 0.0f, 1.0f);
	} else {
		ImGui::ColorEdit4("Trail Start Color", &bulletTrail.startColor.x);
		ImGui::ColorEdit4("Player Trail End", &bulletTrail.playerEndColor.x);
		ImGui::ColorEdit4("Enemy Trail End", &bulletTrail.enemyEndColor.x);
		ImGui::ColorEdit4("Reflect Trail End", &bulletTrail.reflectableEndColor.x);
	}
	ImGui::Text("Trail Instances: %zu", bulletManager_->GetTrailInstanceCount());
	BloomParam& bulletTrailPost = bulletTrailPostEffect_->GetParam();
	ImGui::DragFloat("Bullet Trail Bloom Intensity", &bulletTrailPost.intensity, 0.01f, 0.0f, 8.0f);
	ImGui::DragFloat("Bullet Trail Bloom Threshold", &bulletTrailPost.threshold, 0.01f, 0.0f, 2.0f);
	ImGui::End();

#endif // USE_IMGUI

#ifdef USE_IMGUI
	if (input_->IsTrigger(input_->GetKey()[DIK_F7], input_->GetPreKey()[DIK_F7])) {
		showCollisionDebug_ = !showCollisionDebug_;
	}
	if (input_->IsTrigger(input_->GetKey()[DIK_F5], input_->GetPreKey()[DIK_F5])) {
		player_->AddExp(player_->GetNextLevelExpValue());
	}
	if (input_->IsTrigger(input_->GetKey()[DIK_F6], input_->GetPreKey()[DIK_F6])) {
		player_->AddExp(200);
	}
#endif // USE_IMGUI

	if (input_->IsTrigger(input_->GetKey()[DIK_F8], input_->GetPreKey()[DIK_F8])) {
		showPostProfileOverlay_ = !showPostProfileOverlay_;
	}
	if (input_->IsTrigger(input_->GetKey()[DIK_F9], input_->GetPreKey()[DIK_F9])) {
		postProfileMode_ = (postProfileMode_ + 1) % 8;
	}
	if (input_->IsTrigger(input_->GetKey()[DIK_F10], input_->GetPreKey()[DIK_F10])) {
		ReloadLevelData(true);
	}
	if (input_->IsTrigger(input_->GetKey()[DIK_F11], input_->GetPreKey()[DIK_F11])) {
		showLevelAIDitorPreview_ = !showLevelAIDitorPreview_;
	}

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
	if (skybox_) {
		skybox_->Update(camera.get(), debugCamera.get());
	}
	

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
	UpdateLevelItems();
	
	player_->Update(camera.get(), *stage_, bulletManager_.get(), finalDeltaTime);
	if (player_->IsDead() && !playerDeathShakeStarted_) {
		playerDeathShakeStarted_ = true;
		cameraShakeTimer_ = cameraShakeDuration_;
		cameraShakePower_ = 1.15f;
	}
	
	enemy_->Update(finalDeltaTime);
	UpdateLevelBossPhases();

	enemyManager_->Update(*stage_, finalDeltaTime);
	
	bulletManager_->Update(*stage_, finalDeltaTime);
	
	// 衝突マネージャの更新
	collisionManager_->CheckAllCollisions(player_.get(), enemy_.get(), bulletManager_.get(), enemyManager_.get());
	if (showCollisionDebug_) {
		for (Collider* collider : collisionManager_->GetColliders()) {
			if (!collider) {
				continue;
			}
			if (!showCollisionDebugBullets_ && IsBulletCollider(collider->GetCollisionAttribute())) {
				continue;
			}
			EmitColliderDebugRings(*collisionDebugRingManager_, *collider);
		}
	} else {
		collisionDebugRingManager_->Clear();
	}
	collisionDebugRingManager_->Update(finalDeltaTime);
	playerPostEffect_->Update(finalDeltaTime);
	enemyPostEffect_->Update(finalDeltaTime);
	expEnemyPostEffect_->Update(finalDeltaTime);
	stagePostEffect_->Update(finalDeltaTime);
	neonGridPostEffect_->Update(finalDeltaTime);
	bulletTrailPostEffect_->Update(finalDeltaTime);
	sharedObjectBloomPostEffect_->Update(finalDeltaTime);
	UpdatePostProfileText();

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

	ResetPostProfileEntries();
	if (skybox_) {
		//skybox_->Draw();
	}
	Object3dCommon::GetInstance()->PreDraw(kNormal);
	const bool useGridPost = enableNeonGridPostEffect_ && IsPostProfileCategoryEnabled("Grid");
	const bool useStagePost = enableStagePostEffect_ && IsPostProfileCategoryEnabled("Stage");
	const bool useBulletTrailPost = enableBulletTrailPostEffect_ && IsPostProfileCategoryEnabled("BulletTrail");
	const bool usePlayerPost = enablePlayerPostEffect_ && IsPostProfileCategoryEnabled("Player");
	const bool useEnemyPost = enableEnemyPostEffect_ && IsPostProfileCategoryEnabled("Enemy");
	const bool useExpEnemyPost = enableExpEnemyPostEffect_ && IsPostProfileCategoryEnabled("ExpEnemy");
	auto profile = [this](const char* name, bool active, auto&& drawFunc) {
		const auto start = std::chrono::steady_clock::now();
		drawFunc();
		const auto end = std::chrono::steady_clock::now();
		const float ms = std::chrono::duration<float, std::milli>(end - start).count();
		AddPostProfileEntry(name, ms, active);
	};

	if (useGridPost) {
		profile("Grid Post", true, [&]() {
			neonGridPostEffect_->BeginCapture();
			DrawNeonGridPass();
			neonGridPostEffect_->EndCaptureAdditiveOnly();
			Object3dCommon::GetInstance()->PreDraw(kNormal);
		});
	} else {
		profile("Grid Draw", false, [&]() {
			DrawNeonGridPass();
			Object3dCommon::GetInstance()->PreDraw(kNormal);
		});
	}

	profile("Base Objects", true, [&]() {
		player_->Draw(true);
		enemy_->Draw(true);
		enemyManager_->Draw(true);
		bulletManager_->Draw();
		DrawLevelItems();
		stage_->DrawVisible(GetActiveCameraPosition(camera.get(), debugCamera.get()), 38.0f, 24.0f);
	});
	if (useStagePost) {
		profile("Stage Post", true, [&]() {
			const Vector3 currentCameraPos = GetActiveCameraPosition(camera.get(), debugCamera.get());
			Vector2 cacheOffset = stagePostCacheValid_ ? GetStagePostCacheUvOffset(currentCameraPos) : Vector2{ 0.0f, 0.0f };
			const float pixelOffsetX = cacheOffset.x * static_cast<float>(WinApp::kClientWidth);
			const float pixelOffsetY = cacheOffset.y * static_cast<float>(WinApp::kClientHeight);
			const bool shouldRefreshCache =
				!stagePostCacheValid_ ||
				std::fabs(pixelOffsetX) > stagePostCacheRefreshPixels_ ||
				std::fabs(pixelOffsetY) > stagePostCacheRefreshPixels_;

			if (shouldRefreshCache) {
				stagePostEffect_->BeginCapture();
				Object3dCommon::GetInstance()->PreDraw(kNormal);
				stage_->DrawVisible(currentCameraPos, 38.0f, 24.0f);
				stagePostEffect_->EndCaptureBloomOnlyToCache();
				stagePostCacheCameraPos_ = currentCameraPos;
				stagePostCacheValid_ = true;
				cacheOffset = { 0.0f, 0.0f };
			}
			stagePostEffect_->DrawCachedBloom(cacheOffset);
			Object3dCommon::GetInstance()->PreDraw(kNormal);
		});
	} else {
		stagePostCacheValid_ = false;
		AddPostProfileEntry("Stage Post", 0.0f, false);
	}
	
	{
		Matrix4x4 vp = Object3dCommon::GetInstance()->GetIsDebugCamera()
			? debugCamera->GetViewProjectionMatrix()
			: camera->GetViewProjectionMatrix();
		if (useBulletTrailPost) {
			profile("Trail Post", true, [&]() {
				bulletTrailPostEffect_->BeginCapture();
				bulletManager_->DrawTrails(vp);
				Object3dCommon::GetInstance()->PreDraw(kNormal);
				bulletManager_->Draw();
				bulletTrailPostEffect_->EndCaptureAdditiveOnly();
				Object3dCommon::GetInstance()->PreDraw(kNormal);
			});
		} else {
			profile("Trail Draw", false, [&]() {
				bulletManager_->DrawTrails(vp);
				Object3dCommon::GetInstance()->PreDraw(kNormal);
			});
		}
	}

	const bool useSharedObjectBloom =
		(usePlayerPost && !(slowMotionPostActive_ && keepPlayerColorDuringSlow_)) ||
		useEnemyPost ||
		useExpEnemyPost;
	if (useSharedObjectBloom) {
		profile("Object Glow", true, [&]() {
			const Vector3 currentCameraPos = GetActiveCameraPosition(camera.get(), debugCamera.get());
			sharedObjectBloomPostEffect_->BeginCapture();
			Object3dCommon::GetInstance()->PreDraw(kNormal);
			if (usePlayerPost && !(slowMotionPostActive_ && keepPlayerColorDuringSlow_)) {
				player_->DrawBodyOnly();
			}
			if (useEnemyPost) {
				enemy_->DrawBodyOnly();
			}
			if (useExpEnemyPost) {
				enemyManager_->DrawBodyOnlyVisible(
					currentCameraPos,
					expEnemyPostVisibleHalfWidth_,
					expEnemyPostVisibleHalfHeight_);
			}
			sharedObjectBloomPostEffect_->EndCaptureBloomOnly();
			Object3dCommon::GetInstance()->PreDraw(kNormal);
		});
	} else {
		AddPostProfileEntry("Object Glow", 0.0f, false);
	}

	if (showCollisionDebug_) {
		Matrix4x4 vp = Object3dCommon::GetInstance()->GetIsDebugCamera()
			? debugCamera->GetViewProjectionMatrix()
			: camera->GetViewProjectionMatrix();
		profile("Collision", true, [&]() {
			collisionDebugRingManager_->DrawAll(vp);
		});
	}

	profile("Particles", true, [&]() {
		ParticleManager::GetInstance()->Draw();
	});

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

void GameScene::DrawNeonGridPass() {
	if (!neonGridRenderer_) {
		return;
	}

	neonGridRenderer_->BeginFrame();
	if (showNeonGrid_) {
		const float minX = 0.0f;
		const float minY = 0.0f;
		const float maxX = MapChip::kBlockWidth * static_cast<float>(MapChip::kNumBlockHorizontal - 1);
		const float maxY = MapChip::kBlockHeight * static_cast<float>(MapChip::kNumBlockVirtical - 1);
		neonGridRenderer_->QueueWorldGrid(minX, maxX, minY, maxY, worldGridSpacing_, worldGridLineWidth_, worldGridColor_);
	}
	if (showActorLocalGrid_) {
		const float fieldMinX = 0.0f;
		const float fieldMinY = 0.0f;
		const float fieldMaxX = MapChip::kBlockWidth * static_cast<float>(MapChip::kNumBlockHorizontal - 1);
		const float fieldMaxY = MapChip::kBlockHeight * static_cast<float>(MapChip::kNumBlockVirtical - 1);
		if (!player_->IsDead()) {
			neonGridRenderer_->QueueLocalGridClipped(player_->GetWorldPosition(), actorGridRadius_, actorGridSpacing_, actorGridLineWidth_, playerGridColor_, fieldMinX, fieldMaxX, fieldMinY, fieldMaxY);
		}
		if (!enemy_->IsDead()) {
			neonGridRenderer_->QueueLocalGridClipped(enemy_->GetWorldPosition(), actorGridRadius_ * 1.15f, actorGridSpacing_, actorGridLineWidth_, enemyGridColor_, fieldMinX, fieldMaxX, fieldMinY, fieldMaxY);
		}
		int expEnemyLocalGridCount = 0;
		for (ExpEnemy* expEnemy : enemyManager_->GetEnemyPtrs()) {
			if (expEnemy && !expEnemy->IsDead()) {
				if (maxExpEnemyLocalGrids_ >= 0 && expEnemyLocalGridCount >= maxExpEnemyLocalGrids_) {
					break;
				}
				if (cullActorLocalGrid_ && !IsNearCamera2D(expEnemy->GetWorldPosition(), 38.0f, 24.0f, actorGridRadius_)) {
					continue;
				}
				neonGridRenderer_->QueueLocalGridClipped(expEnemy->GetWorldPosition(), actorGridRadius_ * 0.6f, actorGridSpacing_, actorGridLineWidth_ * 0.8f, expEnemyGridColor_, fieldMinX, fieldMaxX, fieldMinY, fieldMaxY);
				++expEnemyLocalGridCount;
			}
		}
	}
	if (showLevelAIDitorPreview_) {
		QueueLevelEditorPreview();
	}

	Matrix4x4 vp = Object3dCommon::GetInstance()->GetIsDebugCamera()
		? debugCamera->GetViewProjectionMatrix()
		: camera->GetViewProjectionMatrix();
	neonGridRenderer_->DrawAll(vp);
}

bool GameScene::IsNearCamera2D(const Vector3& worldPos, float halfWidth, float halfHeight, float margin) const {
	const Vector3 cameraPos = GetActiveCameraPosition(camera.get(), debugCamera.get());
	return worldPos.x >= cameraPos.x - halfWidth - margin &&
		worldPos.x <= cameraPos.x + halfWidth + margin &&
		worldPos.y >= cameraPos.y - halfHeight - margin &&
		worldPos.y <= cameraPos.y + halfHeight + margin;
}

void GameScene::ResetPostProfileEntries() {
	postProfileEntryCount_ = 0;
}

void GameScene::AddPostProfileEntry(const char* name, float ms, bool active) {
	if (postProfileEntryCount_ >= postProfileEntries_.size()) {
		return;
	}
	postProfileEntries_[postProfileEntryCount_] = { name, ms, active };
	postProfileAccumulatedMs_[postProfileEntryCount_] += ms;
	++postProfileEntryCount_;
}

void GameScene::UpdatePostProfileText() {
	++postProfileAccumulatedFrames_;
	if (postProfileAccumulatedFrames_ < 15) {
		return;
	}

	char text[768]{};
	int offset = std::snprintf(
		text,
		sizeof(text),
		"CG2  PostProfile F8 hide F9 mode | %s",
		GetPostProfileModeName()
	);
	const int frames = (std::max)(1, postProfileAccumulatedFrames_);
	for (size_t i = 0; i < postProfileEntryCount_ && offset > 0 && offset < static_cast<int>(sizeof(text)); ++i) {
		const float avgMs = postProfileAccumulatedMs_[i] / static_cast<float>(frames);
		offset += std::snprintf(
			text + offset,
			sizeof(text) - static_cast<size_t>(offset),
			" | %s%s %.2fms",
			postProfileEntries_[i].active ? "" : "(off) ",
			postProfileEntries_[i].name,
			avgMs
		);
	}

	SetWindowTextA(WinApp::GetInstance()->GetHwnd(), text);
	postProfileAccumulatedMs_.fill(0.0f);
	postProfileAccumulatedFrames_ = 0;
}

bool GameScene::IsPostProfileCategoryEnabled(const char* category) const {
	switch (postProfileMode_) {
	case 1:
		return std::strcmp(category, "Grid") != 0;
	case 2:
		return std::strcmp(category, "Stage") != 0;
	case 3:
		return std::strcmp(category, "BulletTrail") != 0;
	case 4:
		return std::strcmp(category, "Player") != 0;
	case 5:
		return std::strcmp(category, "Enemy") != 0;
	case 6:
		return std::strcmp(category, "ExpEnemy") != 0;
	case 7:
		return std::strcmp(category, "Grid") != 0
			&& std::strcmp(category, "Stage") != 0
			&& std::strcmp(category, "BulletTrail") != 0
			&& std::strcmp(category, "Player") != 0
			&& std::strcmp(category, "Enemy") != 0
			&& std::strcmp(category, "ExpEnemy") != 0;
	default:
		return true;
	}
}

const char* GameScene::GetPostProfileModeName() const {
	switch (postProfileMode_) {
	case 1: return "No Grid Post";
	case 2: return "No Stage Post";
	case 3: return "No Bullet Trail Post";
	case 4: return "No Player Post";
	case 5: return "No Enemy Post";
	case 6: return "No Exp Enemy Post";
	case 7: return "No Post Effects";
	default: return "All Enabled";
	}
}

Vector2 GameScene::GetStagePostCacheUvOffset(const Vector3& currentCameraPos) const {
	const Vector2 cacheCenterScreen = WorldToScreen({ stagePostCacheCameraPos_.x, stagePostCacheCameraPos_.y, 0.0f });
	const Vector2 currentCenterScreen = WorldToScreen({ currentCameraPos.x, currentCameraPos.y, 0.0f });
	return {
		(cacheCenterScreen.x - currentCenterScreen.x) / static_cast<float>(WinApp::kClientWidth),
		(cacheCenterScreen.y - currentCenterScreen.y) / static_cast<float>(WinApp::kClientHeight)
	};
}

bool GameScene::LoadLevelFile(LevelData& outLevel) const
{
	return LevelLoader().Load("resources/levels/level_test.json", outLevel);
}

void GameScene::ReloadLevelData(bool resetSpawnPositions)
{
	LevelData levelData;
	if (!LoadLevelFile(levelData)) {
		std::cerr << "[Level AI-ditor] Reload failed." << std::endl;
		return;
	}

	ClearAppliedLevelData();

	if (resetSpawnPositions) {
		for (const LevelObject& object : levelData.objects) {
			if (object.type == "PlayerSpawn" && player_) {
				player_->SetWorldPosition(object.transform.translate);
			} else if (object.type == "BossSpawn" && enemy_) {
				enemy_->SetWorldPosition(object.transform.translate);
			}
		}
	}

	ApplyLevelData(levelData);
	stagePostCacheValid_ = false;
	std::cerr << "[Level AI-ditor] Reloaded: " << levelData.levelName << std::endl;
}

void GameScene::ClearAppliedLevelData()
{
	levelItems_.clear();
	levelBossPhases_.clear();
	if (stage_) {
		stage_->ClearLevelObstacles();
	}
	if (enemyManager_) {
		enemyManager_->ClearLevelData();
	}
	if (enemy_) {
		enemy_->SetBossAttackConfig(Enemy::BossAttackConfig{});
	}
}

void GameScene::ApplyLevelData(const LevelData& levelData)
{
	currentLevelData_ = levelData;
	if (levelData.balance.is_object()) {
		const bool randomSpawnEnabled = ReadCustomBool(levelData.balance, "defaultRandomSpawnEnabled", true);
		enemyManager_->SetDefaultRandomSpawnEnabled(randomSpawnEnabled);
		LoadBalanceEditorFromJson(levelData.balance);
		ApplyLevelBalance(levelData.balance);
	}

	for (const LevelObject& object : levelData.objects) {
		ApplyLevelObject(object, false);
	}
	for (const LevelSpawnArea& spawnArea : levelData.spawnAreas) {
		AddLevelSpawnArea(spawnArea);
	}
	levelBossPhases_.clear();
	levelBossPhases_.reserve(levelData.bossPhases.size());
	for (const LevelBossPhase& phase : levelData.bossPhases) {
		levelBossPhases_.push_back({ phase, false });
	}
}

void GameScene::ApplyLevelBalance(const nlohmann::json& balanceJson)
{
	if (!balanceJson.is_object()) {
		return;
	}

	if (player_ && balanceJson.contains("player") && balanceJson["player"].is_object()) {
		const nlohmann::json& playerJson = balanceJson["player"];
		Player::BalanceConfig config{};
		config.maxHp = ReadCustomInt(playerJson, "maxHp", player_->GetMaxHp());
		config.maxHpUpgradeAmount = ReadCustomInt(playerJson, "maxHpUpgradeAmount", 50);
		config.bodyDamage = static_cast<uint32_t>((std::max)(1, ReadCustomInt(playerJson, "bodyDamage", static_cast<int>(player_->GetDamage()))));
		config.healToFull = ReadCustomBool(playerJson, "healToFull", false);
		player_->ApplyBalanceConfig(config);
	}

	if (balanceJson.contains("damage") && balanceJson["damage"].is_object()) {
		const nlohmann::json& damageJson = balanceJson["damage"];
		if (stage_) {
			stage_->SetDamageBlockDamage(static_cast<uint32_t>((std::max)(1, ReadCustomInt(damageJson, "damageBlock", 75))));
		}
		if (enemy_) {
			enemy_->SetDamage(static_cast<uint32_t>((std::max)(1, ReadCustomInt(damageJson, "bossContact", static_cast<int>(enemy_->GetDamage())))));
		}

		ExpEnemy::BalanceConfig expConfig{};
		expConfig.contactDamage = static_cast<uint32_t>((std::max)(1, ReadCustomInt(damageJson, "expEnemyContact", 12)));
		expConfig.shooterContactDamage = static_cast<uint32_t>((std::max)(1, ReadCustomInt(damageJson, "shooterContact", 20)));
		expConfig.shooterBulletDamage = static_cast<uint32_t>((std::max)(1, ReadCustomInt(damageJson, "shooterBullet", 15)));
		ExpEnemy::SetBalanceConfig(expConfig);
	}

	if (enemy_ && balanceJson.contains("bossAttackDefault") && balanceJson["bossAttackDefault"].is_object()) {
		const nlohmann::json& attackJson = balanceJson["bossAttackDefault"];
		Enemy::BossAttackConfig config = enemy_->GetBossAttackConfig();
		config.bulletSpeed = ReadCustomFloat(attackJson, "bulletSpeed", config.bulletSpeed);
		config.bulletCount = ReadCustomInt(attackJson, "bulletCount", config.bulletCount);
		config.spreadAngleDeg = ReadCustomFloat(attackJson, "spreadAngleDeg", config.spreadAngleDeg);
		config.cooldown = ReadCustomFloat(attackJson, "cooldown", config.cooldown);
		config.damage = static_cast<uint32_t>((std::max)(1, ReadCustomInt(attackJson, "damage", static_cast<int>(config.damage))));
		config.randomSpread = ReadCustomBool(attackJson, "randomSpread", config.randomSpread);
		enemy_->SetBossAttackConfig(config);
	}
}

void GameScene::LoadBalanceEditorFromJson(const nlohmann::json& balanceJson)
{
	if (!balanceJson.is_object()) {
		return;
	}

	balanceEditor_.defaultRandomSpawnEnabled = ReadCustomBool(balanceJson, "defaultRandomSpawnEnabled", balanceEditor_.defaultRandomSpawnEnabled);
	if (balanceJson.contains("player") && balanceJson["player"].is_object()) {
		const nlohmann::json& playerJson = balanceJson["player"];
		balanceEditor_.playerMaxHp = ReadCustomInt(playerJson, "maxHp", balanceEditor_.playerMaxHp);
		balanceEditor_.maxHpUpgradeAmount = ReadCustomInt(playerJson, "maxHpUpgradeAmount", balanceEditor_.maxHpUpgradeAmount);
		balanceEditor_.playerBodyDamage = ReadCustomInt(playerJson, "bodyDamage", balanceEditor_.playerBodyDamage);
		balanceEditor_.healToFull = ReadCustomBool(playerJson, "healToFull", balanceEditor_.healToFull);
	}
	if (balanceJson.contains("damage") && balanceJson["damage"].is_object()) {
		const nlohmann::json& damageJson = balanceJson["damage"];
		balanceEditor_.damageBlock = ReadCustomInt(damageJson, "damageBlock", balanceEditor_.damageBlock);
		balanceEditor_.bossContact = ReadCustomInt(damageJson, "bossContact", balanceEditor_.bossContact);
		balanceEditor_.expEnemyContact = ReadCustomInt(damageJson, "expEnemyContact", balanceEditor_.expEnemyContact);
		balanceEditor_.shooterContact = ReadCustomInt(damageJson, "shooterContact", balanceEditor_.shooterContact);
		balanceEditor_.shooterBullet = ReadCustomInt(damageJson, "shooterBullet", balanceEditor_.shooterBullet);
	}
	if (balanceJson.contains("bossAttackDefault") && balanceJson["bossAttackDefault"].is_object()) {
		const nlohmann::json& attackJson = balanceJson["bossAttackDefault"];
		balanceEditor_.bossBulletSpeed = ReadCustomFloat(attackJson, "bulletSpeed", balanceEditor_.bossBulletSpeed);
		balanceEditor_.bossBulletCount = ReadCustomInt(attackJson, "bulletCount", balanceEditor_.bossBulletCount);
		balanceEditor_.bossSpreadAngleDeg = ReadCustomFloat(attackJson, "spreadAngleDeg", balanceEditor_.bossSpreadAngleDeg);
		balanceEditor_.bossCooldown = ReadCustomFloat(attackJson, "cooldown", balanceEditor_.bossCooldown);
		balanceEditor_.bossBulletDamage = ReadCustomInt(attackJson, "damage", balanceEditor_.bossBulletDamage);
		balanceEditor_.bossRandomSpread = ReadCustomBool(attackJson, "randomSpread", balanceEditor_.bossRandomSpread);
	}
	balanceEditor_.initialized = true;
}

nlohmann::json GameScene::BuildBalanceJsonFromEditor() const
{
	nlohmann::json balance = nlohmann::json::object();
	balance["defaultRandomSpawnEnabled"] = balanceEditor_.defaultRandomSpawnEnabled;
	balance["player"] = {
		{ "maxHp", (std::max)(1, balanceEditor_.playerMaxHp) },
		{ "maxHpUpgradeAmount", (std::max)(1, balanceEditor_.maxHpUpgradeAmount) },
		{ "bodyDamage", (std::max)(1, balanceEditor_.playerBodyDamage) },
		{ "healToFull", balanceEditor_.healToFull }
	};
	balance["damage"] = {
		{ "damageBlock", (std::max)(1, balanceEditor_.damageBlock) },
		{ "bossContact", (std::max)(1, balanceEditor_.bossContact) },
		{ "expEnemyContact", (std::max)(1, balanceEditor_.expEnemyContact) },
		{ "shooterContact", (std::max)(1, balanceEditor_.shooterContact) },
		{ "shooterBullet", (std::max)(1, balanceEditor_.shooterBullet) }
	};
	balance["bossAttackDefault"] = {
		{ "bulletSpeed", (std::max)(0.01f, balanceEditor_.bossBulletSpeed) },
		{ "bulletCount", (std::max)(1, balanceEditor_.bossBulletCount) },
		{ "spreadAngleDeg", (std::clamp)(balanceEditor_.bossSpreadAngleDeg, 0.0f, 180.0f) },
		{ "cooldown", (std::max)(0.05f, balanceEditor_.bossCooldown) },
		{ "damage", (std::max)(1, balanceEditor_.bossBulletDamage) },
		{ "randomSpread", balanceEditor_.bossRandomSpread }
	};
	if (currentLevelData_.balance.is_object()
		&& currentLevelData_.balance.contains("notes")
		&& currentLevelData_.balance["notes"].is_string()) {
		balance["notes"] = currentLevelData_.balance["notes"];
	} else {
		balance["notes"] = "Level AI-ditor Balance Labで調整した値。";
	}
	return balance;
}

bool GameScene::SaveBalanceEditorToLevelFile(const std::string& filePath)
{
	nlohmann::json levelJson = nlohmann::json::object();
	{
		std::ifstream in(filePath);
		if (in.is_open()) {
			try {
				in >> levelJson;
			} catch (const std::exception& e) {
				balanceEditor_.statusMessage = std::string("Failed to parse level JSON: ") + e.what();
				return false;
			}
		}
	}
	if (!levelJson.is_object()) {
		levelJson = nlohmann::json::object();
	}

	levelJson["balance"] = BuildBalanceJsonFromEditor();
	std::ofstream out(filePath);
	if (!out.is_open()) {
		balanceEditor_.statusMessage = "Failed to open level JSON for writing.";
		return false;
	}
	out << levelJson.dump(2) << std::endl;
	currentLevelData_.balance = levelJson["balance"];
	balanceEditor_.statusMessage = "Saved balance to " + filePath;
	return true;
}

bool GameScene::WriteBalanceAIHandoff(const std::string& filePath) const
{
	std::ofstream out(filePath);
	if (!out.is_open()) {
		return false;
	}

	out
		<< "# Level AI-ditor Balance Handoff\n\n"
		<< "このファイルは、ゲーム内のBalance Labで調整した値をAIに渡すためのメモです。\n\n"
		<< "## 依頼例\n\n"
		<< "- 現在のプレイ感:\n"
		<< "- 困っていること:\n"
		<< "- もっと強くしたい要素:\n"
		<< "- もっと弱くしたい要素:\n"
		<< "- 残したい体験:\n\n"
		<< "## 現在のBalance JSON\n\n"
		<< "```json\n"
		<< BuildBalanceJsonFromEditor().dump(2)
		<< "\n```\n\n"
		<< "## AIへのルール\n\n"
		<< "- C++の固定値変更ではなく、まず `resources/levels/level_test.json` の `balance` を調整してください。\n"
		<< "- プレイヤーHP、DamageBlock、Shooter弾、ボス通常弾は `balance` 内で調整してください。\n"
		<< "- ボスHPフェーズ中の弾性能だけは `bossPhases[].customProperties.bossAttack` で調整してください。\n"
		<< "- 変更後の `balance` JSONと、なぜその値にしたかを短く説明してください。\n";
	return true;
}

void GameScene::DrawLevelAIDitorBalanceLab()
{
#ifdef USE_IMGUI
	if (!balanceEditor_.initialized && currentLevelData_.balance.is_object()) {
		LoadBalanceEditorFromJson(currentLevelData_.balance);
	}

	ImGui::Begin("Level AI-ditor Balance Lab");
	ImGui::Text("Edit runtime balance, then save it back to level_test.json.");
	ImGui::Checkbox("Default Random Spawn", &balanceEditor_.defaultRandomSpawnEnabled);
	ImGui::Separator();

	if (ImGui::CollapsingHeader("Player", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragInt("Max HP", &balanceEditor_.playerMaxHp, 10.0f, 1, 9999);
		ImGui::DragInt("Max HP Upgrade", &balanceEditor_.maxHpUpgradeAmount, 1.0f, 1, 999);
		ImGui::DragInt("Body Damage", &balanceEditor_.playerBodyDamage, 1.0f, 1, 999);
		ImGui::Checkbox("Heal To Full On Apply", &balanceEditor_.healToFull);
		if (player_) {
			ImGui::Text("Current HP: %d / %d", player_->GetHp(), player_->GetMaxHp());
		}
	}

	if (ImGui::CollapsingHeader("Damage", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragInt("DamageBlock", &balanceEditor_.damageBlock, 1.0f, 1, 999);
		ImGui::DragInt("Boss Contact", &balanceEditor_.bossContact, 1.0f, 1, 999);
		ImGui::DragInt("Exp Enemy Contact", &balanceEditor_.expEnemyContact, 1.0f, 1, 999);
		ImGui::DragInt("Shooter Contact", &balanceEditor_.shooterContact, 1.0f, 1, 999);
		ImGui::DragInt("Shooter Bullet", &balanceEditor_.shooterBullet, 1.0f, 1, 999);
	}

	if (ImGui::CollapsingHeader("Boss Attack Default", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat("Bullet Speed", &balanceEditor_.bossBulletSpeed, 0.01f, 0.01f, 2.0f);
		ImGui::DragInt("Bullet Count", &balanceEditor_.bossBulletCount, 1.0f, 1, 64);
		ImGui::DragFloat("Spread Angle", &balanceEditor_.bossSpreadAngleDeg, 1.0f, 0.0f, 180.0f);
		ImGui::DragFloat("Cooldown", &balanceEditor_.bossCooldown, 0.01f, 0.05f, 5.0f);
		ImGui::DragInt("Bullet Damage", &balanceEditor_.bossBulletDamage, 1.0f, 1, 999);
		ImGui::Checkbox("Random Spread", &balanceEditor_.bossRandomSpread);
	}

	ImGui::Separator();
	if (ImGui::Button("Apply Runtime")) {
		currentLevelData_.balance = BuildBalanceJsonFromEditor();
		enemyManager_->SetDefaultRandomSpawnEnabled(balanceEditor_.defaultRandomSpawnEnabled);
		ApplyLevelBalance(currentLevelData_.balance);
		balanceEditor_.statusMessage = "Applied balance to running game.";
	}
	ImGui::SameLine();
	if (ImGui::Button("Save level_test.json")) {
		SaveBalanceEditorToLevelFile("resources/levels/level_test.json");
	}
	ImGui::SameLine();
	if (ImGui::Button("AI Handoff MD")) {
		if (WriteBalanceAIHandoff("resources/levels/ai_balance_handoff.md")) {
			balanceEditor_.statusMessage = "Wrote resources/levels/ai_balance_handoff.md";
		} else {
			balanceEditor_.statusMessage = "Failed to write AI handoff file.";
		}
	}
	if (ImGui::Button("Reload From JSON")) {
		LevelData levelData;
		if (LoadLevelFile(levelData)) {
			LoadBalanceEditorFromJson(levelData.balance);
			balanceEditor_.statusMessage = "Reloaded balance editor values from JSON.";
		} else {
			balanceEditor_.statusMessage = "Failed to reload level JSON.";
		}
	}

	if (!balanceEditor_.statusMessage.empty()) {
		ImGui::TextWrapped("%s", balanceEditor_.statusMessage.c_str());
	}
	ImGui::End();
#endif
}

void GameScene::ApplyLevelObject(const LevelObject& levelObject, bool allowBossSpawn)
{
	if (levelObject.type == "PlayerSpawn" || levelObject.type == "BossSpawn") {
		if (!allowBossSpawn) {
			return;
		}
		std::cerr << "[LevelLoader] Spawn markers are ignored after initialization: " << levelObject.name << std::endl;
		return;
	}
	if (levelObject.type == "Enemy") {
		const int hp = ReadCustomInt(levelObject.customProperties, "hp", -1);
		enemyManager_->SpawnLevelEnemy(levelObject.transform.translate, levelObject.prefab, hp);
		return;
	}
	if (levelObject.type == "SpawnArea") {
		AddLevelSpawnAreaFromObject(levelObject);
		return;
	}
	if (levelObject.type == "Obstacle") {
		stage_->AddLevelObstacle(levelObject.transform, levelObject.prefab);
		stagePostCacheValid_ = false;
		return;
	}
	if (levelObject.type == "Item") {
		AddLevelItem(levelObject);
		return;
	}

	std::cerr << "[LevelLoader] Unsupported object type: " << levelObject.type << " (" << levelObject.name << ")" << std::endl;
}

void GameScene::AddLevelSpawnArea(const LevelSpawnArea& spawnArea)
{
	EnemyManager::SpawnArea area{};
	area.name = spawnArea.name;
	area.prefab = spawnArea.prefab;
	area.center = spawnArea.center;
	area.size = spawnArea.size;
	area.spawnInterval = spawnArea.spawnInterval;
	area.maxAlive = spawnArea.maxAlive;
	area.hp = spawnArea.hp;
	area.enabled = spawnArea.enabled;
	enemyManager_->AddLevelSpawnArea(area);
}

void GameScene::AddLevelSpawnAreaFromObject(const LevelObject& levelObject)
{
	LevelSpawnArea spawnArea{};
	spawnArea.name = levelObject.name;
	spawnArea.prefab = levelObject.prefab;
	spawnArea.center = levelObject.transform.translate;
	spawnArea.size = levelObject.transform.scale;
	spawnArea.spawnInterval = ReadCustomFloat(levelObject.customProperties, "spawnInterval", 2.0f);
	spawnArea.maxAlive = ReadCustomInt(levelObject.customProperties, "maxAlive", 8);
	spawnArea.hp = ReadCustomInt(levelObject.customProperties, "hp", -1);
	spawnArea.enabled = ReadCustomBool(levelObject.customProperties, "enabled", true);
	spawnArea.prefab = ReadCustomString(levelObject.customProperties, "enemyPrefab", spawnArea.prefab);
	AddLevelSpawnArea(spawnArea);
}

void GameScene::UpdateLevelBossPhases()
{
	if (!enemy_ || enemy_->GetMaxHp() <= 0) {
		return;
	}

	const float hpRate = static_cast<float>(enemy_->GetHp()) / static_cast<float>(enemy_->GetMaxHp());
	for (RuntimeBossPhase& runtimePhase : levelBossPhases_) {
		if (runtimePhase.activated || hpRate > runtimePhase.phase.startHpRate) {
			continue;
		}

		runtimePhase.activated = true;
		std::cerr << "[Level AI-ditor] Activate boss phase: " << runtimePhase.phase.name << std::endl;
		if (!runtimePhase.phase.message.empty()) {
			std::cerr << "[Level AI-ditor] " << runtimePhase.phase.message << std::endl;
		}
		ApplyBossPhaseTuning(runtimePhase.phase);
		for (const LevelObject& object : runtimePhase.phase.objects) {
			ApplyLevelObject(object, true);
		}
	}
}

void GameScene::ApplyBossPhaseTuning(const LevelBossPhase& phase)
{
	if (!phase.customProperties.is_object()) {
		return;
	}

	if (phase.customProperties.contains("bossAttack") && phase.customProperties["bossAttack"].is_object() && enemy_) {
		const nlohmann::json& attackJson = phase.customProperties["bossAttack"];
		Enemy::BossAttackConfig config = enemy_->GetBossAttackConfig();
		config.bulletSpeed = ReadCustomFloat(attackJson, "bulletSpeed", config.bulletSpeed);
		config.bulletCount = ReadCustomInt(attackJson, "bulletCount", config.bulletCount);
		config.spreadAngleDeg = ReadCustomFloat(attackJson, "spreadAngleDeg", config.spreadAngleDeg);
		config.cooldown = ReadCustomFloat(attackJson, "cooldown", config.cooldown);
		config.damage = static_cast<uint32_t>(ReadCustomInt(attackJson, "damage", static_cast<int>(config.damage)));
		config.randomSpread = ReadCustomBool(attackJson, "randomSpread", config.randomSpread);
		enemy_->SetBossAttackConfig(config);
	}

	if (phase.customProperties.contains("effectPreset") && phase.customProperties["effectPreset"].is_object()) {
		ApplyLevelEffectPreset(phase.customProperties["effectPreset"]);
	}
}

void GameScene::ApplyLevelEffectPreset(const nlohmann::json& effectJson)
{
	if (effectJson.contains("gridColor") && effectJson["gridColor"].is_object()) {
		worldGridColor_ = ReadJsonVector4(effectJson["gridColor"], worldGridColor_);
	}
	if (effectJson.contains("enemyGridColor") && effectJson["enemyGridColor"].is_object()) {
		enemyGridColor_ = ReadJsonVector4(effectJson["enemyGridColor"], enemyGridColor_);
	}
	if (effectJson.contains("bossOutlineColor") && effectJson["bossOutlineColor"].is_object() && enemyPostEffect_) {
		BloomParam& enemyPost = enemyPostEffect_->GetParam();
		enemyPost.outlineColor = ReadJsonVector3(effectJson["bossOutlineColor"], enemyPost.outlineColor);
	}
	if (effectJson.contains("enemyBloomIntensity") && effectJson["enemyBloomIntensity"].is_number() && enemyPostEffect_) {
		enemyPostEffect_->GetParam().intensity = effectJson["enemyBloomIntensity"].get<float>();
	}
	if (effectJson.contains("gridBloomIntensity") && effectJson["gridBloomIntensity"].is_number() && neonGridPostEffect_) {
		neonGridPostEffect_->GetParam().intensity = effectJson["gridBloomIntensity"].get<float>();
	}
	if (effectJson.contains("cameraShakePower") && effectJson["cameraShakePower"].is_number()) {
		cameraShakePower_ = effectJson["cameraShakePower"].get<float>();
		cameraShakeTimer_ = cameraShakeDuration_;
	}
}

void GameScene::QueueLevelEditorPreview()
{
	if (!neonGridRenderer_) {
		return;
	}

	for (const LevelObject& object : currentLevelData_.objects) {
		QueueLevelObjectPreview(object, { 0.45f, 0.95f, 1.0f, 0.65f });
	}
	for (const LevelSpawnArea& spawnArea : currentLevelData_.spawnAreas) {
		QueueLevelSpawnAreaPreview(spawnArea, { 1.0f, 0.85f, 0.2f, 0.70f });
	}
	for (const RuntimeBossPhase& runtimePhase : levelBossPhases_) {
		const Vector4 color = runtimePhase.activated
			? Vector4{ 1.0f, 0.25f, 0.25f, 0.85f }
			: Vector4{ 0.85f, 0.35f, 1.0f, 0.45f };
		for (const LevelObject& object : runtimePhase.phase.objects) {
			QueueLevelObjectPreview(object, color);
		}
	}
}

void GameScene::QueueLevelObjectPreview(const LevelObject& levelObject, const Vector4& color)
{
	if (levelObject.type == "SpawnArea") {
		LevelSpawnArea spawnArea{};
		spawnArea.name = levelObject.name;
		spawnArea.prefab = levelObject.prefab;
		spawnArea.center = levelObject.transform.translate;
		spawnArea.size = levelObject.transform.scale;
		QueueLevelSpawnAreaPreview(spawnArea, color);
		return;
	}

	if (levelObject.type == "Obstacle") {
		neonGridRenderer_->QueueRectangle(levelObject.transform.translate, {
			MapChip::kBlockWidth * levelObject.transform.scale.x,
			MapChip::kBlockHeight * levelObject.transform.scale.y,
			1.0f
		}, 0.08f, color);
		return;
	}

	const float radius = levelObject.type == "BossSpawn" ? 3.0f : 1.7f;
	neonGridRenderer_->QueueLocalGrid(levelObject.transform.translate, radius, 0.75f, 0.055f, color);
}

void GameScene::QueueLevelSpawnAreaPreview(const LevelSpawnArea& spawnArea, const Vector4& color)
{
	neonGridRenderer_->QueueRectangle(spawnArea.center, spawnArea.size, 0.09f, color);
	neonGridRenderer_->QueueLocalGrid(spawnArea.center, (std::min)(spawnArea.size.x, spawnArea.size.y) * 0.22f, 1.0f, 0.045f, color);
}

bool GameScene::AddLevelItem(const LevelObject& levelObject)
{
	std::string model;
	if (!TryGetItemModel(levelObject.prefab, model)) {
		std::cerr << "[LevelLoader] Unsupported Item prefab: " << levelObject.prefab << std::endl;
		return false;
	}

	LevelVisualObject visual{};
	visual.name = levelObject.name;
	visual.object = std::make_unique<Object3d>();
	visual.object->Initialize();
	visual.object->SetModel(model);
	visual.object->SetTransform(levelObject.transform);
	visual.object->SetColor({ 0.35f, 1.0f, 0.75f, 1.0f });
	visual.object->Update();
	levelItems_.push_back(std::move(visual));
	return true;
}

void GameScene::UpdateLevelItems()
{
	for (LevelVisualObject& item : levelItems_) {
		if (item.object) {
			item.object->Update();
		}
	}
}

void GameScene::DrawLevelItems()
{
	for (LevelVisualObject& item : levelItems_) {
		if (item.object) {
			item.object->Draw();
		}
	}
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
	followHpBarIndex_ = 0;
	for (auto& vertices : hpBarBackgroundVertices_) { vertices.clear(); }
	for (auto& vertices : hpBarFillVertices_) { vertices.clear(); }
	for (auto& vertices : hpBarOutlineVertices_) { vertices.clear(); }
	if (showFollowHpBars_) {
		if (!player_->IsDead()) {
			DrawFollowHpBar(player_.get(), player_->GetWorldPosition(), player_->GetHp(), player_->GetMaxHp(), 58.0f, -1.55f);
		}
		for (PlayerDrone* drone : player_->GetDronePtrs()) {
			if (drone && !drone->IsDead()) {
				DrawFollowHpBar(drone, drone->GetWorldPosition(), drone->GetHp(), drone->GetMaxHp(), 42.0f, -1.45f);
			}
		}
		if (!enemy_->IsDead()) {
			DrawFollowHpBar(enemy_.get(), enemy_->GetWorldPosition(), enemy_->GetHp(), enemy_->GetMaxHp(), 92.0f, -3.25f);
		}
		for (ExpEnemy* expEnemy : enemyManager_->GetEnemyPtrs()) {
			if (expEnemy && !expEnemy->IsDead()) {
				DrawFollowHpBar(expEnemy, expEnemy->GetWorldPosition(), expEnemy->GetHp(), expEnemy->GetMaxHp(), 36.0f, -1.05f);
			}
		}
	}
	DrawHpBarBatches();
	player_->DrawSprite();
	player_->DrawEncyclopedia();
	//shotGide->Draw();
	if (dashGuideText_) {
		dashGuideText_->Draw();
	}
	if (moveGuideText_) {
		moveGuideText_->Draw();
	}
	if (titleGuideText_) {
		titleGuideText_->Draw();
	}
	if (fpsText_) {
		fpsText_->Draw();
	}
	if (showPostProfileOverlay_ && postProfileText_) {
		postProfileText_->Draw();
	}
	if (phase_ != Phase::kFadeIn) {
		fade_->Draw();
	}
}

void GameScene::InitializeFollowHpBars(size_t count) {
	followHpBars_.clear();
	followHpBars_.reserve(count);
	for (size_t i = 0; i < count; ++i) {
		FollowHpBar bar{};
		bar.outline = std::make_unique<Sprite>();
		bar.background = std::make_unique<Sprite>();
		bar.fill = std::make_unique<Sprite>();

		bar.outline->Initialize(SpriteCommon::GetInstance(), "resources/hpBarFrame.png");
		bar.background->Initialize(SpriteCommon::GetInstance(), "resources/hpBarMask.png");
		bar.fill->Initialize(SpriteCommon::GetInstance(), "resources/hpBarFillMask.png");

		bar.outline->SetAnchorPoint({ 0.5f, 0.5f });
		bar.background->SetAnchorPoint({ 0.5f, 0.5f });
		bar.fill->SetAnchorPoint({ 0.0f, 0.5f });

		bar.outline->SetColor({ 1.0f, 1.0f, 1.0f, 0.95f });
		bar.background->SetColor({ 0.0f, 0.0f, 0.0f, 0.92f });
		bar.fill->SetColor({ 0.35f, 1.0f, 0.42f, 1.0f });

		followHpBars_.push_back(std::move(bar));
	}
}

void GameScene::InitializeFollowHpBarBatch() {
	TextureManager::GetInstance()->LoadTexture("resources/hpBarOutlineMask.png");
	TextureManager::GetInstance()->LoadTexture("resources/hpBarFillMask.png");

	const uint32_t kMaxHpBarVertices = 4096;
	hpBarVertexResource_ = Object3dCommon::GetInstance()->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * kMaxHpBarVertices);
	hpBarVertexBufferView_.BufferLocation = hpBarVertexResource_->GetGPUVirtualAddress();
	hpBarVertexBufferView_.SizeInBytes = sizeof(VertexData) * kMaxHpBarVertices;
	hpBarVertexBufferView_.StrideInBytes = sizeof(VertexData);
	hpBarVertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&hpBarVertexData_));

	for (HpBarMaterialBuffer& material : hpBarMaterials_) {
		material.resource = Object3dCommon::GetInstance()->GetDxCommon()->CreateBufferResource(sizeof(Material));
		material.resource->Map(0, nullptr, reinterpret_cast<void**>(&material.data));
		material.data->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		material.data->enableLighting = false;
		material.data->lightingMode = false;
		material.data->environmentCoefficient = 0.0f;
		material.data->padding = 0.0f;
		material.data->uvTransform = MakeIdentity4x4();
		material.data->shininess = 1.0f;
	}
	for (size_t i = 0; i < 4; ++i) {
		const float alpha = static_cast<float>(i + 1) / 4.0f;
		hpBarMaterials_[i * 3 + 0].data->color = { 0.0f, 0.0f, 0.0f, 0.94f * alpha };
		hpBarMaterials_[i * 3 + 1].data->color = { 0.46f, 1.0f, 0.56f, alpha };
		hpBarMaterials_[i * 3 + 2].data->color = { 0.92f, 1.0f, 0.94f, 0.98f * alpha };
	}

	hpBarTransformResource_ = Object3dCommon::GetInstance()->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));
	hpBarTransformResource_->Map(0, nullptr, reinterpret_cast<void**>(&hpBarTransformData_));
	hpBarTransformData_->World = MakeIdentity4x4();
	hpBarTransformData_->WVP = MakeOrthographicMatrix(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 100.0f);
}

void GameScene::DrawFollowHpBar(const void* ownerKey, const Vector3& worldPos, int hp, int maxHp, float width, float yOffset) {
	if (maxHp <= 0 || hp <= 0) {
		return;
	}

	HpBarVisibility& visibility = hpBarVisibility_[ownerKey];
	if (visibility.lastHp < 0) {
		visibility.lastHp = hp;
		visibility.lastMaxHp = maxHp;
	}
	const bool hpChanged = hp != visibility.lastHp || maxHp != visibility.lastMaxHp;
	if (hpChanged) {
		visibility.visibleTimer = 1.2f;
	} else if (visibility.lastHp < visibility.lastMaxHp && hp >= maxHp) {
		visibility.visibleTimer = 0.35f;
	}
	visibility.lastHp = hp;
	visibility.lastMaxHp = maxHp;

	const float targetAlpha = hp < maxHp || visibility.visibleTimer > 0.0f ? 1.0f : 0.0f;
	const float fadeSpeed = targetAlpha > visibility.alpha ? 10.0f : 4.0f;
	if (visibility.alpha < targetAlpha) {
		visibility.alpha = (std::min)(targetAlpha, visibility.alpha + finalDeltaTime * fadeSpeed);
	} else if (visibility.alpha > targetAlpha) {
		visibility.alpha = (std::max)(targetAlpha, visibility.alpha - finalDeltaTime * fadeSpeed);
	}
	visibility.visibleTimer = (std::max)(0.0f, visibility.visibleTimer - finalDeltaTime);

	if (visibility.alpha <= 0.0f) {
		return;
	}
	const float alpha = visibility.alpha;

	const float ratio = (std::clamp)(static_cast<float>(hp) / static_cast<float>(maxHp), 0.0f, 1.0f);
	const float height = (std::max)(9.0f, width * 0.18f);
	const Vector2 screenPos = WorldToScreen(worldPos + Vector3{ 0.0f, yOffset, 0.0f });
	const float kCullMargin = 80.0f;
	if (screenPos.x < -kCullMargin || screenPos.x > WinApp::kClientWidth + kCullMargin ||
		screenPos.y < -kCullMargin || screenPos.y > WinApp::kClientHeight + kCullMargin) {
		return;
	}

	const float inset = 3.0f;
	const float innerWidth = (std::max)(1.0f, width - inset * 2.0f);
	const float innerHeight = (std::max)(1.0f, height - inset * 2.0f);
	const float fillWidth = (std::max)(1.0f, innerWidth * ratio);
	const size_t alphaBucket = static_cast<size_t>((std::clamp)(static_cast<int>(std::ceil(alpha * 4.0f)) - 1, 0, 3));

	QueueHpBarQuad(hpBarBackgroundVertices_[alphaBucket], screenPos, { innerWidth, innerHeight });
	QueueHpBarQuad(
		hpBarFillVertices_[alphaBucket],
		{ screenPos.x - innerWidth * 0.5f + fillWidth * 0.5f, screenPos.y },
		{ fillWidth, innerHeight }
	);
	QueueHpBarQuad(hpBarOutlineVertices_[alphaBucket], screenPos, { width, height });
}

void GameScene::QueueHpBarQuad(std::vector<VertexData>& vertices, const Vector2& center, const Vector2& size) {
	const float left = center.x - size.x * 0.5f;
	const float right = center.x + size.x * 0.5f;
	const float top = center.y - size.y * 0.5f;
	const float bottom = center.y + size.y * 0.5f;

	vertices.push_back({ { left, bottom, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } });
	vertices.push_back({ { left, top, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } });
	vertices.push_back({ { right, bottom, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } });
	vertices.push_back({ { left, top, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } });
	vertices.push_back({ { right, top, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } });
	vertices.push_back({ { right, bottom, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } });
}

void GameScene::DrawHpBarBatch(uint32_t startVertex, uint32_t vertexCount, const std::string& textureFilePath, const HpBarMaterialBuffer& material) {
	if (vertexCount == 0 || !hpBarVertexData_) {
		return;
	}

	auto* dxCommon = Object3dCommon::GetInstance()->GetDxCommon();
	auto* commandList = dxCommon->GetList().Get();
	commandList->IASetVertexBuffers(0, 1, &hpBarVertexBufferView_);
	commandList->SetGraphicsRootConstantBufferView(0, material.resource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, hpBarTransformResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath));
	commandList->DrawInstanced(vertexCount, 1, startVertex, 0);
}

void GameScene::DrawHpBarBatches() {
	struct HpBarDrawCommand {
		uint32_t startVertex = 0;
		uint32_t vertexCount = 0;
		const char* textureFilePath = nullptr;
		const HpBarMaterialBuffer* material = nullptr;
	};

	std::array<HpBarDrawCommand, 12> drawCommands{};
	uint32_t currentVertex = 0;
	auto appendVertices = [&](const std::vector<VertexData>& vertices, const char* textureFilePath, const HpBarMaterialBuffer& material, size_t commandIndex) {
		if (vertices.empty() || currentVertex >= 4096u) {
			return;
		}
		const uint32_t count = (std::min)(static_cast<uint32_t>(vertices.size()), 4096u - currentVertex);
		std::copy_n(vertices.data(), count, hpBarVertexData_ + currentVertex);
		drawCommands[commandIndex] = { currentVertex, count, textureFilePath, &material };
		currentVertex += count;
	};

	for (size_t i = 0; i < hpBarBackgroundVertices_.size(); ++i) {
		appendVertices(hpBarBackgroundVertices_[i], "resources/hpBarFillMask.png", hpBarMaterials_[i * 3 + 0], i * 3 + 0);
		appendVertices(hpBarFillVertices_[i], "resources/hpBarFillMask.png", hpBarMaterials_[i * 3 + 1], i * 3 + 1);
		appendVertices(hpBarOutlineVertices_[i], "resources/hpBarOutlineMask.png", hpBarMaterials_[i * 3 + 2], i * 3 + 2);
	}
	if (currentVertex == 0) {
		return;
	}

	hpBarTransformData_->WVP = MakeOrthographicMatrix(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 100.0f);
	SpriteCommon::GetInstance()->PreDraw(kNormal);
	for (const HpBarDrawCommand& command : drawCommands) {
		if (command.vertexCount == 0) {
			continue;
		}
		DrawHpBarBatch(command.startVertex, command.vertexCount, command.textureFilePath, *command.material);
	}
}

Vector2 GameScene::WorldToScreen(const Vector3& worldPos) const {
	Matrix4x4 matVP = Object3dCommon::GetInstance()->GetIsDebugCamera()
		? debugCamera->GetViewMatrix() * debugCamera->GetProjectionMatrix()
		: camera->GetViewMatrix() * camera->GetProjectionMatrix();
	Vector3 ndcPos = TransformMatrix(worldPos, matVP);

	float screenX = (ndcPos.x + 1.0f) * 0.5f * WinApp::kClientWidth;
	float screenY = (1.0f - ndcPos.y) * 0.5f * WinApp::kClientHeight;
	return { screenX, screenY };
}

std::string GameScene::GetNextSceneName() const
{
	return "TITLE";
}
