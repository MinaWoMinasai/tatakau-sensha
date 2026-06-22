#include "GameScene.h"
#include "CollisionConfig.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {

Vector4 GetCollisionDebugColor(uint32_t attribute) {
	if (attribute & kCollisionAttributePlayer) {
		return { 0.15f, 1.0f, 0.95f, 0.85f };
	}
	if (attribute & kCollisionAttributePlayerDrone) {
		return { 0.25f, 0.75f, 1.0f, 0.85f };
	}
	if (attribute & kCollisionAttributeExpEnemy) {
		return { 1.0f, 0.72f, 0.15f, 0.85f };
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
	if (attribute & kCollisionAttributeHostileExpEnemyBullet) {
		return { 1.0f, 0.12f, 0.04f, 0.8f };
	}
	return { 1.0f, 1.0f, 1.0f, 0.7f };
}

bool IsBulletCollider(uint32_t attribute) {
	return (attribute & (kCollisionAttributePlayerBullet | kCollisionAttributeEnemyBullet | kCollisionAttributeHostileExpEnemyBullet)) != 0;
}

const char* BulletOwnerName(BulletOwner owner)
{
	switch (owner) {
	case kPlayer:
		return "プレイヤー";
	case kEnemy:
		return "敵";
	case kExpEnemyHostile:
		return "EXP敵";
	default:
		return "不明";
	}
}

#ifdef USE_IMGUI
ImU32 BulletOwnerDebugColor(BulletOwner owner)
{
	switch (owner) {
	case kPlayer:
		return IM_COL32(255, 238, 80, 255);
	case kEnemy:
		return IM_COL32(255, 94, 120, 255);
	case kExpEnemyHostile:
		return IM_COL32(255, 48, 24, 255);
	default:
		return IM_COL32(255, 255, 255, 255);
	}
}
#endif

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

Vector2 ReadJsonVector2(const nlohmann::json& json, const Vector2& fallback)
{
	if (!json.is_object()) {
		return fallback;
	}

	Vector2 value = fallback;
	if (json.contains("x") && json["x"].is_number()) value.x = json["x"].get<float>();
	if (json.contains("y") && json["y"].is_number()) value.y = json["y"].get<float>();
	return value;
}

nlohmann::json WriteJsonVector2(const Vector2& value)
{
	return {
		{ "x", value.x },
		{ "y", value.y }
	};
}

nlohmann::json WriteJsonVector3(const Vector3& value)
{
	return {
		{ "x", value.x },
		{ "y", value.y },
		{ "z", value.z }
	};
}

nlohmann::json WriteJsonVector4(const Vector4& value)
{
	return {
		{ "x", value.x },
		{ "y", value.y },
		{ "z", value.z },
		{ "w", value.w }
	};
}

nlohmann::json WriteBulletTrailSettingsJson(const BulletTrailSettings& settings)
{
	return {
		{ "playerHalfWidth", settings.playerHalfWidth },
		{ "enemyHalfWidth", settings.enemyHalfWidth },
		{ "lifetime", settings.lifetime },
		{ "maxPoints", settings.maxPoints },
		{ "interpolationSteps", settings.interpolationSteps },
		{ "headWidthScale", settings.headWidthScale },
		{ "tailWidthScale", settings.tailWidthScale },
		{ "widthCurvePower", settings.widthCurvePower },
		{ "colorCurvePower", settings.colorCurvePower },
		{ "useObjectColorForTrail", settings.useObjectColorForTrail },
		{ "trailHeadIntensity", settings.trailHeadIntensity },
		{ "trailTailIntensity", settings.trailTailIntensity },
		{ "trailHeadAlpha", settings.trailHeadAlpha },
		{ "trailTailAlpha", settings.trailTailAlpha },
		{ "playerObjectColor", WriteJsonVector4(settings.playerObjectColor) },
		{ "enemyObjectColor", WriteJsonVector4(settings.enemyObjectColor) },
		{ "reflectableObjectColor", WriteJsonVector4(settings.reflectableObjectColor) },
		{ "startColor", WriteJsonVector4(settings.startColor) },
		{ "playerEndColor", WriteJsonVector4(settings.playerEndColor) },
		{ "enemyEndColor", WriteJsonVector4(settings.enemyEndColor) },
		{ "reflectableEndColor", WriteJsonVector4(settings.reflectableEndColor) }
	};
}

void ReadBulletTrailSettingsJson(const nlohmann::json& json, BulletTrailSettings& settings)
{
	if (!json.is_object()) {
		return;
	}
	settings.playerHalfWidth = ReadCustomFloat(json, "playerHalfWidth", settings.playerHalfWidth);
	settings.enemyHalfWidth = ReadCustomFloat(json, "enemyHalfWidth", settings.enemyHalfWidth);
	settings.lifetime = ReadCustomFloat(json, "lifetime", settings.lifetime);
	settings.maxPoints = ReadCustomInt(json, "maxPoints", settings.maxPoints);
	settings.interpolationSteps = ReadCustomInt(json, "interpolationSteps", settings.interpolationSteps);
	settings.headWidthScale = ReadCustomFloat(json, "headWidthScale", settings.headWidthScale);
	settings.tailWidthScale = ReadCustomFloat(json, "tailWidthScale", settings.tailWidthScale);
	settings.widthCurvePower = ReadCustomFloat(json, "widthCurvePower", settings.widthCurvePower);
	settings.colorCurvePower = ReadCustomFloat(json, "colorCurvePower", settings.colorCurvePower);
	settings.useObjectColorForTrail = ReadCustomBool(json, "useObjectColorForTrail", settings.useObjectColorForTrail);
	settings.trailHeadIntensity = ReadCustomFloat(json, "trailHeadIntensity", settings.trailHeadIntensity);
	settings.trailTailIntensity = ReadCustomFloat(json, "trailTailIntensity", settings.trailTailIntensity);
	settings.trailHeadAlpha = ReadCustomFloat(json, "trailHeadAlpha", settings.trailHeadAlpha);
	settings.trailTailAlpha = ReadCustomFloat(json, "trailTailAlpha", settings.trailTailAlpha);
	if (json.contains("playerObjectColor")) settings.playerObjectColor = ReadJsonVector4(json["playerObjectColor"], settings.playerObjectColor);
	if (json.contains("enemyObjectColor")) settings.enemyObjectColor = ReadJsonVector4(json["enemyObjectColor"], settings.enemyObjectColor);
	if (json.contains("reflectableObjectColor")) settings.reflectableObjectColor = ReadJsonVector4(json["reflectableObjectColor"], settings.reflectableObjectColor);
	if (json.contains("startColor")) settings.startColor = ReadJsonVector4(json["startColor"], settings.startColor);
	if (json.contains("playerEndColor")) settings.playerEndColor = ReadJsonVector4(json["playerEndColor"], settings.playerEndColor);
	if (json.contains("enemyEndColor")) settings.enemyEndColor = ReadJsonVector4(json["enemyEndColor"], settings.enemyEndColor);
	if (json.contains("reflectableEndColor")) settings.reflectableEndColor = ReadJsonVector4(json["reflectableEndColor"], settings.reflectableEndColor);
}

nlohmann::json WriteBloomParamJson(const BloomParam& param)
{
	return {
		{ "threshold", param.threshold },
		{ "intensity", param.intensity },
		{ "vignetteIntensity", param.vignetteIntensity },
		{ "vignetteScale", param.vignetteScale },
		{ "distortionAmount", param.distortionAmount },
		{ "chromAbAmount", param.chromAbAmount },
		{ "noiseIntensity", param.noiseIntensity },
		{ "scanlineIntensity", param.scanlineIntensity },
		{ "scanlineFrequency", param.scanlineFrequency },
		{ "curvature", param.curvature },
		{ "borderSharp", param.borderSharp },
		{ "glitchAmount", param.glitchAmount },
		{ "gaussianIntensity", param.gaussianIntensity },
		{ "dissolveThreshold", param.dissolveThreshold },
		{ "outlineWidth", param.outlineWidth },
		{ "outlineThreshold", param.outlineThreshold },
		{ "boxBlurIntensity", param.boxBlurIntensity },
		{ "outlineColor", WriteJsonVector3(param.outlineColor) },
		{ "outlineBloomIntensity", param.outlineBloomIntensity },
		{ "outlineBloomWidth", param.outlineBloomWidth },
		{ "boxBlurRadius", param.boxBlurRadius },
		{ "fullScreenBoxBlurBlend", param.fullScreenBoxBlurBlend },
		{ "depthOutlineEnabled", param.depthOutlineEnabled },
		{ "depthNearClip", param.depthNearClip },
		{ "depthFarClip", param.depthFarClip },
		{ "depthOutlineScale", param.depthOutlineScale },
		{ "radialBlurCenter", WriteJsonVector2(param.radialBlurCenter) },
		{ "radialBlurWidth", param.radialBlurWidth },
		{ "radialBlurIntensity", param.radialBlurIntensity },
		{ "dissolveEdgeColor", WriteJsonVector3(param.dissolveEdgeColor) },
		{ "dissolveEdgeWidth", param.dissolveEdgeWidth },
		{ "dissolveNoiseScale", param.dissolveNoiseScale },
		{ "dissolveNoiseSpeed", param.dissolveNoiseSpeed }
	};
}

void ReadBloomParamJson(const nlohmann::json& json, BloomParam& param)
{
	if (!json.is_object()) {
		return;
	}
	param.threshold = ReadCustomFloat(json, "threshold", param.threshold);
	param.intensity = ReadCustomFloat(json, "intensity", param.intensity);
	param.vignetteIntensity = ReadCustomFloat(json, "vignetteIntensity", param.vignetteIntensity);
	param.vignetteScale = ReadCustomFloat(json, "vignetteScale", param.vignetteScale);
	param.distortionAmount = ReadCustomFloat(json, "distortionAmount", param.distortionAmount);
	param.chromAbAmount = ReadCustomFloat(json, "chromAbAmount", param.chromAbAmount);
	param.noiseIntensity = ReadCustomFloat(json, "noiseIntensity", param.noiseIntensity);
	param.scanlineIntensity = ReadCustomFloat(json, "scanlineIntensity", param.scanlineIntensity);
	param.scanlineFrequency = ReadCustomFloat(json, "scanlineFrequency", param.scanlineFrequency);
	param.curvature = ReadCustomFloat(json, "curvature", param.curvature);
	param.borderSharp = ReadCustomFloat(json, "borderSharp", param.borderSharp);
	param.glitchAmount = ReadCustomFloat(json, "glitchAmount", param.glitchAmount);
	param.gaussianIntensity = ReadCustomFloat(json, "gaussianIntensity", param.gaussianIntensity);
	param.dissolveThreshold = ReadCustomFloat(json, "dissolveThreshold", param.dissolveThreshold);
	param.outlineWidth = ReadCustomFloat(json, "outlineWidth", param.outlineWidth);
	param.outlineThreshold = ReadCustomFloat(json, "outlineThreshold", param.outlineThreshold);
	param.boxBlurIntensity = ReadCustomFloat(json, "boxBlurIntensity", param.boxBlurIntensity);
	if (json.contains("outlineColor")) {
		param.outlineColor = ReadJsonVector3(json["outlineColor"], param.outlineColor);
	}
	param.outlineBloomIntensity = ReadCustomFloat(json, "outlineBloomIntensity", param.outlineBloomIntensity);
	param.outlineBloomWidth = ReadCustomFloat(json, "outlineBloomWidth", param.outlineBloomWidth);
	param.boxBlurRadius = ReadCustomFloat(json, "boxBlurRadius", param.boxBlurRadius);
	param.fullScreenBoxBlurBlend = ReadCustomFloat(json, "fullScreenBoxBlurBlend", param.fullScreenBoxBlurBlend);
	param.depthOutlineEnabled = ReadCustomFloat(json, "depthOutlineEnabled", param.depthOutlineEnabled);
	param.depthNearClip = ReadCustomFloat(json, "depthNearClip", param.depthNearClip);
	param.depthFarClip = ReadCustomFloat(json, "depthFarClip", param.depthFarClip);
	param.depthOutlineScale = ReadCustomFloat(json, "depthOutlineScale", param.depthOutlineScale);
	if (json.contains("radialBlurCenter")) {
		param.radialBlurCenter = ReadJsonVector2(json["radialBlurCenter"], param.radialBlurCenter);
	}
	param.radialBlurWidth = ReadCustomFloat(json, "radialBlurWidth", param.radialBlurWidth);
	param.radialBlurIntensity = ReadCustomFloat(json, "radialBlurIntensity", param.radialBlurIntensity);
	if (json.contains("dissolveEdgeColor")) {
		param.dissolveEdgeColor = ReadJsonVector3(json["dissolveEdgeColor"], param.dissolveEdgeColor);
	}
	param.dissolveEdgeWidth = ReadCustomFloat(json, "dissolveEdgeWidth", param.dissolveEdgeWidth);
	param.dissolveNoiseScale = ReadCustomFloat(json, "dissolveNoiseScale", param.dissolveNoiseScale);
	param.dissolveNoiseSpeed = ReadCustomFloat(json, "dissolveNoiseSpeed", param.dissolveNoiseSpeed);
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

GameScene::~GameScene()
{
	ExpEnemy::SetEnemyKillCallback(nullptr);
	ExpEnemy::SetShapeNeonRenderMode(0);
}

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
		1.0f
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

	particlePostEffect_ = std::make_unique<ObjectPostEffect>();
	particlePostEffect_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(),
		Object3dCommon::GetInstance()->GetSrvManager(),
		nullptr,
		1.0f
	);
	{
		BloomParam& particlePost = particlePostEffect_->GetParam();
		particlePost.threshold = 0.0f;
		particlePost.intensity = 1.65f;
		particlePost.outlineWidth = 0.0f;
		particlePost.outlineThreshold = 0.0f;
		particlePost.outlineBloomIntensity = 0.0f;
		particlePost.outlineBloomWidth = 0.0f;
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
	LoadGamePostEffectConfig();

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
	enemyManager_->Initialize(player_.get(), bulletManager_.get(), enemy_.get());
	enemy_->SetEnemyManager(enemyManager_.get());
	ExpEnemy::SetEnemyKillCallback([this](uint32_t expValue) {
		if (enemy_) {
			enemy_->RegisterExpEnemyKill(expValue);
		}
	});
	if (hasLevelData) {
		ApplyLevelData(levelData);
	}

	// 衝突マネージャの生成
	collisionManager_ = std::make_unique<CollisionManager>();
	collisionDebugRingManager_ = std::make_unique<RingManager>();
	collisionDebugRingManager_->Initialize(Object3dCommon::GetInstance()->GetDxCommon(), "resources/gradationLine.png");
	neonGridRenderer_ = std::make_unique<NeonGridRenderer>();
	neonGridRenderer_->Initialize(Object3dCommon::GetInstance()->GetDxCommon(), "resources/white512x512.png");
	LoadGameVisualConfig();

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

	TextStyle controlGuideStyle{};
	controlGuideStyle.fontFamily = "Meiryo";
	controlGuideStyle.fontSize = 14.0f;
	controlGuideStyle.color = { 0.88f, 0.94f, 1.0f, 0.78f };
	controlGuideStyle.outlineColor = { 0.0f, 0.02f, 0.04f, 0.88f };
	controlGuideStyle.outlineThickness = 2.0f;
	controlGuideStyle.padding = 5.0f;
	controlGuideText_ = std::make_unique<TextLabel>();
	controlGuideText_->Initialize(
		SpriteCommon::GetInstance(),
		"WASD 移動 / 左クリック 射撃 / 右クリック ダッシュ\nC 進化ツリー / ESC タイトルへ / H ヘルプ切替",
		controlGuideStyle);
	controlGuideText_->SetPosition({ 22.0f, 636.0f });

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
	if (input_->IsTrigger(input_->GetKey()[DIK_H], input_->GetPreKey()[DIK_H])) {
		showControlGuide_ = !showControlGuide_;
		if (controlGuideText_) {
			controlGuideText_->SetText(showControlGuide_
				? "WASD 移動 / 左クリック 射撃 / 右クリック ダッシュ\nC 進化ツリー / ESC タイトルへ / H ヘルプ切替"
				: "H:操作説明ON");
		}
	}
	slowMotionPostActive_ = finalDeltaTime < baseDeltaTime * 0.98f;

	if (player_->IsChangeMode()) {
		finalDeltaTime = 0.0f;
	}

	neonTriangleDemoRotation_ += neonTriangleDemoRotateSpeed_ * baseDeltaTime;
	stageDamageBlockPulseTime_ += baseDeltaTime;
	ExpEnemy::SetShapeNeonRenderMode(expEnemyNeonRenderMode_);

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

	DrawGameSceneDebugImGui();
	if (showPlayerClassEditor_) {
		player_->DrawPlayerClassEditor();
	}

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
	if (input_->IsTrigger(input_->GetKey()[DIK_F12], input_->GetPreKey()[DIK_F12])) {
		showGameDebugConsole_ = !showGameDebugConsole_;
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
	
	player_->SetDebugNoDamage(debugPlayerNoDamage_);
	player_->Update(camera.get(), *stage_, bulletManager_.get(), finalDeltaTime);
	UpdatePlayerNeonAfterimages(baseDeltaTime);
	if (player_->IsDead() && !playerDeathShakeStarted_) {
		playerDeathShakeStarted_ = true;
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
	particlePostEffect_->Update(finalDeltaTime);
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

	if (showParticleEditor_) {
		ParticleManager::GetInstance()->DrawImGuiEditor();
	}

#endif // USE_IMGUI

	UpdateDeathPostPulse(baseDeltaTime);
	ParticleManager::GetInstance()->Update(finalDeltaTime, camera.get(), debugCamera.get());
	for (const ParticleManager::ScreenPulseEvent& event : ParticleManager::GetInstance()->ConsumeScreenPulseEvents()) {
		TriggerDeathPostPulse(event.position, event.strength);
	}

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
	ExpEnemy::SetShapeNeonRenderMode(expEnemyNeonRenderMode_);
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

	if (expEnemyNeonRenderMode_ == 3) {
		profile("Exp Fill", true, [&]() {
			Object3dCommon::GetInstance()->PreDraw(kNormal);
			DrawExpEnemyNeonFillModels();
			DrawExpEnemyNeonDepthLines();
			Object3dCommon::GetInstance()->PreDraw(kNormal);
		});
	}

	if (useGridPost) {
		profile("Grid Post", true, [&]() {
			neonGridPostEffect_->BeginCapture();
			DrawNeonGridPass(false);
			neonGridPostEffect_->EndCaptureAdditiveOnly();
			Object3dCommon::GetInstance()->PreDraw(kNormal);
		});
	} else {
		profile("Grid Draw", false, [&]() {
			DrawNeonGridPass(false);
			Object3dCommon::GetInstance()->PreDraw(kNormal);
		});
	}
	if (fillActorNeonBodies_ && (playerNeonRenderMode_ == 1 || bossNeonRenderMode_ == 1)) {
		profile("Actor Neon Fill", true, [&]() {
			DrawActorNeonBodyFillPass();
			Object3dCommon::GetInstance()->PreDraw(kNormal);
		});
	} else {
		AddPostProfileEntry("Actor Neon Fill", 0.0f, false);
	}

	profile("Base Objects", true, [&]() {
		player_->Draw(playerNeonRenderMode_ == 0);
		enemy_->Draw(bossNeonRenderMode_ == 0);
		enemyManager_->Draw(true);
		bulletManager_->Draw();
		DrawLevelItems();
		stage_->DrawVisible(GetActiveCameraPosition(camera.get(), debugCamera.get()), 38.0f, 24.0f, showStageNormalBlockBodies_);
	});
	AddPostProfileEntry("Stage Glow", 0.0f, useStagePost);

	if (showStageBlockNeonOutlines_ || showStageDamageBlockNeonOutlines_) {
		if (useGridPost) {
			profile("Stage Block Neon", true, [&]() {
				neonGridPostEffect_->BeginCaptureWithCurrentDepth();
				DrawStageBlockNeonPass();
				neonGridPostEffect_->EndCaptureAdditiveOnly();
				Object3dCommon::GetInstance()->PreDraw(kNormal);
			});
		} else {
			profile("Stage Block Neon", false, [&]() {
				DrawStageBlockNeonPass();
				Object3dCommon::GetInstance()->PreDraw(kNormal);
			});
		}
	} else {
		AddPostProfileEntry("Stage Block Neon", 0.0f, false);
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
		useStagePost ||
		(usePlayerPost && !(slowMotionPostActive_ && keepPlayerColorDuringSlow_)) ||
		useEnemyPost ||
		useExpEnemyPost;
	if (useSharedObjectBloom) {
		profile("Shared Glow", true, [&]() {
			const Vector3 currentCameraPos = GetActiveCameraPosition(camera.get(), debugCamera.get());
			sharedObjectBloomPostEffect_->BeginCapture();
			Object3dCommon::GetInstance()->PreDraw(kNormal);
			if (useStagePost) {
				stage_->DrawVisible(currentCameraPos, 38.0f, 24.0f, showStageNormalBlockBodies_);
			}
			if (usePlayerPost && playerNeonRenderMode_ == 0 && !(slowMotionPostActive_ && keepPlayerColorDuringSlow_)) {
				player_->DrawBodyOnly();
			}
			if (useEnemyPost && bossNeonRenderMode_ == 0) {
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
		AddPostProfileEntry("Shared Glow", 0.0f, false);
	}

	if (showCollisionDebug_) {
		Matrix4x4 vp = Object3dCommon::GetInstance()->GetIsDebugCamera()
			? debugCamera->GetViewProjectionMatrix()
			: camera->GetViewProjectionMatrix();
		profile("Collision", true, [&]() {
			collisionDebugRingManager_->DrawAll(vp);
		});
	}

	if (enableParticlePostEffect_) {
		profile("Particle Glow", true, [&]() {
			particlePostEffect_->BeginCaptureWithCurrentDepth();
			ParticleManager::GetInstance()->Draw();
			particlePostEffect_->EndCaptureAdditiveOnly();
			Object3dCommon::GetInstance()->PreDraw(kNormal);
		});
	} else {
		profile("Particles", false, [&]() {
			ParticleManager::GetInstance()->Draw();
		});
	}

}

void GameScene::TriggerDeathPostPulse(const Vector3& worldPosition, float strength) {
	if (!enableDeathPostPulse_) {
		return;
	}

	const Vector2 screen = WorldToScreen(worldPosition);
	deathPostPulse_.center = {
		(std::clamp)(screen.x / static_cast<float>(WinApp::kClientWidth), 0.0f, 1.0f),
		(std::clamp)(screen.y / static_cast<float>(WinApp::kClientHeight), 0.0f, 1.0f)
	};
	deathPostPulseScale_ = (std::clamp)(strength, 0.5f, 1.6f);
	deathPostPulseTimer_ = deathPostPulseDuration_;
	UpdateDeathPostPulse(0.0f);

	cameraShakeTimer_ = cameraShakeDuration_;
	cameraShakePower_ = (std::max)(cameraShakePower_, 0.65f * deathPostPulseScale_);
}

void GameScene::UpdateDeathPostPulse(float deltaTime) {
	if (deathPostPulseTimer_ <= 0.0f || !enableDeathPostPulse_) {
		deathPostPulseTimer_ = 0.0f;
		deathPostPulse_ = {};
		return;
	}

	deathPostPulseTimer_ = (std::max)(0.0f, deathPostPulseTimer_ - deltaTime);
	const float age = 1.0f - deathPostPulseTimer_ / (std::max)(0.001f, deathPostPulseDuration_);
	const float impactEnvelope = std::sin(age * pi) * (1.0f - age * 0.35f);
	const float flashEnvelope = (1.0f - age) * (1.0f - age);
	deathPostPulse_.radius = 0.025f + age * deathPostShockwaveMaxRadius_;
	deathPostPulse_.width = deathPostShockwaveWidth_;
	deathPostPulse_.strength = deathPostShockwaveStrength_ * deathPostPulseScale_ * impactEnvelope;
	deathPostPulse_.bloomBoost = deathPostBloomBoost_ * deathPostPulseScale_ * flashEnvelope;
	deathPostPulse_.chromAbAmount = deathPostChromAbAmount_ * deathPostPulseScale_ * impactEnvelope;
}

void GameScene::DrawAfterPostEffect3D() {
	if (!enablePlayerPostEffect_ || playerNeonRenderMode_ != 0 || !slowMotionPostActive_ || !keepPlayerColorDuringSlow_) {
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

void GameScene::DrawNeonGridPass(bool includeStageBlockOutlines) {
	if (!neonGridRenderer_) {
		return;
	}

	neonGridRenderer_->BeginFrame();
	neonGridRenderer_->SetLineStyle(neonLineSoftEdgeRatio_, neonLineCoreIntensity_);
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
	if (includeStageBlockOutlines && (showStageBlockNeonOutlines_ || showStageDamageBlockNeonOutlines_)) {
		QueueStageBlockNeonOutlines();
	}
	const bool queueExpEnemyNeonInGridPass = expEnemyNeonRenderMode_ == 1 || expEnemyNeonRenderMode_ == 2;
	const bool queueActorBillboards = playerNeonRenderMode_ == 1 || bossNeonRenderMode_ == 1 || !playerNeonAfterimages_.empty();
	if (showNeonTriangleDemo_ || queueExpEnemyNeonInGridPass || queueActorBillboards) {
		Matrix4x4 viewMatrix = Object3dCommon::GetInstance()->GetIsDebugCamera()
			? debugCamera->GetViewMatrix()
			: camera->GetViewMatrix();
		Matrix4x4 cameraWorld = Inverse(viewMatrix);
		Vector3 cameraRight = Normalize({ cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2] });
		Vector3 cameraUp = Normalize({ cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2] });
		Vector3 cameraForward = Normalize({ cameraWorld.m[2][0], cameraWorld.m[2][1], cameraWorld.m[2][2] });
		if (queueExpEnemyNeonInGridPass) {
			QueueExpEnemyNeonShapes(cameraRight, cameraUp, cameraForward);
		}
		if (queueActorBillboards) {
			QueueActorNeonBillboards(cameraRight, cameraUp);
		}
		if (showNeonTriangleDemo_) {
			neonGridRenderer_->QueueBillboardTriangle(
				neonTriangleDemoCenter_,
				neonTriangleDemoRadius_,
				neonTriangleDemoRotation_,
				neonTriangleDemoLineWidth_,
				neonTriangleDemoColor_,
				cameraRight,
				cameraUp,
				cameraForward);
		}
	}

	Matrix4x4 vp = Object3dCommon::GetInstance()->GetIsDebugCamera()
		? debugCamera->GetViewProjectionMatrix()
		: camera->GetViewProjectionMatrix();
	neonGridRenderer_->DrawAll(vp);
}

void GameScene::DrawStageBlockNeonPass() {
	if (!neonGridRenderer_ || (!showStageBlockNeonOutlines_ && !showStageDamageBlockNeonOutlines_)) {
		return;
	}

	const uint32_t startVertex = neonGridRenderer_->GetVertexCount();
	neonGridRenderer_->SetLineStyle(neonLineSoftEdgeRatio_, neonLineCoreIntensity_);
	QueueStageBlockNeonOutlines();
	const uint32_t vertexCount = neonGridRenderer_->GetVertexCount() - startVertex;
	if (vertexCount == 0) {
		return;
	}

	Matrix4x4 vp = Object3dCommon::GetInstance()->GetIsDebugCamera()
		? debugCamera->GetViewProjectionMatrix()
		: camera->GetViewProjectionMatrix();
	neonGridRenderer_->DrawRange(startVertex, vertexCount, vp);
}

void GameScene::QueueStageBlockNeonOutlines() {
	if (!stage_ || !neonGridRenderer_) {
		return;
	}

	Matrix4x4 viewMatrix = Object3dCommon::GetInstance()->GetIsDebugCamera()
		? debugCamera->GetViewMatrix()
		: camera->GetViewMatrix();
	Matrix4x4 cameraWorld = Inverse(viewMatrix);
	Vector3 cameraForward = Normalize({ cameraWorld.m[2][0], cameraWorld.m[2][1], cameraWorld.m[2][2] });
	const Vector3 cameraPos = GetActiveCameraPosition(camera.get(), debugCamera.get());

	auto divisionCount = [](float scale) {
		return (std::max)(1, static_cast<int>(std::round(std::abs(scale))));
	};

	const auto& blocks = stage_->GetBlocks();
	for (const std::vector<Block>& row : blocks) {
		for (const Block& block : row) {
			const bool isNormalBlock = block.type == MapChipType::kBlock;
			const bool isDamageBlock = block.type == MapChipType::kDamageBlock;
			if (!block.isActive || (!isNormalBlock && !isDamageBlock)) {
				continue;
			}
			if ((isNormalBlock && !showStageBlockNeonOutlines_) ||
				(isDamageBlock && !showStageDamageBlockNeonOutlines_)) {
				continue;
			}
			if (cullActorLocalGrid_ && !IsNearCamera2D(block.originalPos, 38.0f, 24.0f, MapChip::kBlockWidth * 1.5f)) {
				continue;
			}

			const Matrix4x4 transform = MakeAffineMatrix(
				block.worldTransform.scale,
				block.worldTransform.rotate,
				block.worldTransform.translate);

				auto localToWorld = [&](const Vector3& local) {
				return TransformMatrix(local, transform);
			};
			Vector4 blockNeonColor = stageBlockNeonColor_;
			if (isDamageBlock) {
				const float wave = 0.5f + 0.5f * std::sin(stageDamageBlockPulseTime_ * stageDamageBlockPulseSpeed_);
				const float pulse = stageDamageBlockPulseMin_ +
					(stageDamageBlockPulseMax_ - stageDamageBlockPulseMin_) * wave;
				blockNeonColor = stageDamageBlockNeonColor_;
				blockNeonColor.x *= pulse;
				blockNeonColor.y *= pulse;
				blockNeonColor.z *= pulse;
				blockNeonColor.w *= 0.75f + wave * 0.25f;
			}
			auto queueLocalLine = [&](const Vector3& start, const Vector3& end, const Vector3& localNormal) {
				Vector3 worldStart = localToWorld(start);
				Vector3 worldEnd = localToWorld(end);
				Vector3 worldNormal = localToWorld(localNormal) - localToWorld({ 0.0f, 0.0f, 0.0f });
				if (Length(worldNormal) > 0.0001f && stageBlockNeonDepthBias_ > 0.0f) {
					worldNormal = Normalize(worldNormal);
					worldStart = worldStart + worldNormal * stageBlockNeonDepthBias_;
					worldEnd = worldEnd + worldNormal * stageBlockNeonDepthBias_;
				}
				neonGridRenderer_->QueueCameraFacingLine(
					worldStart,
					worldEnd,
					stageBlockNeonLineWidth_,
					blockNeonColor,
					cameraForward);
			};
			auto shouldDrawFace = [&](const Vector3& localCenter, const Vector3& localNormal) {
				const Vector3 worldCenter = localToWorld(localCenter);
				const Vector3 worldNormal = localToWorld(localCenter + localNormal) - worldCenter;
				const Vector3 toCamera = cameraPos - worldCenter;
				const float normalLength = Length(worldNormal);
				const float toCameraLength = Length(toCamera);
				if (normalLength <= 0.0001f || toCameraLength <= 0.0001f) {
					return true;
				}
				const float facingDot = Dot(worldNormal / normalLength, toCamera / toCameraLength);
				return facingDot >= -0.08f;
			};
			auto localCoord = [](int index, int divisions) {
				return -1.0f + 2.0f * (static_cast<float>(index) / static_cast<float>(divisions));
			};

			const int divX = divisionCount(block.worldTransform.scale.x);
			const int divY = divisionCount(block.worldTransform.scale.y);
			const int divZ = divisionCount(block.worldTransform.scale.z);

			for (float z : { -1.0f, 1.0f }) {
				const Vector3 faceNormal = { 0.0f, 0.0f, z };
				if (!shouldDrawFace({ 0.0f, 0.0f, z }, { 0.0f, 0.0f, z })) {
					continue;
				}
				for (int i = 0; i <= divX; ++i) {
					const float x = localCoord(i, divX);
					queueLocalLine({ x, -1.0f, z }, { x, 1.0f, z }, faceNormal);
				}
				for (int i = 0; i <= divY; ++i) {
					const float y = localCoord(i, divY);
					queueLocalLine({ -1.0f, y, z }, { 1.0f, y, z }, faceNormal);
				}
			}

			for (float y : { -1.0f, 1.0f }) {
				const Vector3 faceNormal = { 0.0f, y, 0.0f };
				if (!shouldDrawFace({ 0.0f, y, 0.0f }, { 0.0f, y, 0.0f })) {
					continue;
				}
				for (int i = 0; i <= divX; ++i) {
					const float x = localCoord(i, divX);
					queueLocalLine({ x, y, -1.0f }, { x, y, 1.0f }, faceNormal);
				}
				for (int i = 0; i <= divZ; ++i) {
					const float z = localCoord(i, divZ);
					queueLocalLine({ -1.0f, y, z }, { 1.0f, y, z }, faceNormal);
				}
			}

			for (float x : { -1.0f, 1.0f }) {
				const Vector3 faceNormal = { x, 0.0f, 0.0f };
				if (!shouldDrawFace({ x, 0.0f, 0.0f }, { x, 0.0f, 0.0f })) {
					continue;
				}
				for (int i = 0; i <= divY; ++i) {
					const float y = localCoord(i, divY);
					queueLocalLine({ x, y, -1.0f }, { x, y, 1.0f }, faceNormal);
				}
				for (int i = 0; i <= divZ; ++i) {
					const float z = localCoord(i, divZ);
					queueLocalLine({ x, -1.0f, z }, { x, 1.0f, z }, faceNormal);
				}
			}
		}
	}
}

void GameScene::UpdatePlayerNeonAfterimages(float deltaTime) {
	for (PlayerNeonAfterimage& afterimage : playerNeonAfterimages_) {
		afterimage.life -= deltaTime;
	}
	playerNeonAfterimages_.erase(
		std::remove_if(
			playerNeonAfterimages_.begin(),
			playerNeonAfterimages_.end(),
			[](const PlayerNeonAfterimage& afterimage) { return afterimage.life <= 0.0f; }),
		playerNeonAfterimages_.end());

	if (!player_ || player_->IsDead() || !player_->IsDashing()) {
		playerAfterimageSpawnTimer_ = 0.0f;
		return;
	}

	playerAfterimageSpawnTimer_ -= deltaTime;
	if (playerAfterimageSpawnTimer_ <= 0.0f) {
		Vector3 direction = player_->GetDirection();
		if (Length(direction) < 0.001f) {
			direction = { 0.0f, -1.0f, 0.0f };
		}
		playerNeonAfterimages_.push_back({ player_->GetWorldPosition(), Normalize(direction), playerAfterimageLifetime_ });
		playerAfterimageSpawnTimer_ = playerAfterimageInterval_;
	}
}

void GameScene::QueueActorNeonBillboards(const Vector3& cameraRight, const Vector3& cameraUp, bool drawBodies, bool drawBarrels) {
	if (!neonGridRenderer_) {
		return;
	}

	auto directionToRotation = [&](const Vector3& worldDirection) {
		Vector3 direction = worldDirection;
		if (Length(direction) < 0.001f) {
			direction = { 0.0f, -1.0f, 0.0f };
		}
		direction = Normalize(direction);
		return std::atan2(Dot(direction, cameraRight), -Dot(direction, cameraUp));
	};

	auto lerpColor = [](const Vector4& a, const Vector4& b, float t) {
		t = (std::clamp)(t, 0.0f, 1.0f);
		return Vector4{
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t,
			a.w + (b.w - a.w) * t };
	};
	auto rotateDirection = [](const Vector3& direction, float angle) {
		const float c = std::cos(angle);
		const float s = std::sin(angle);
		return Vector3{ direction.x * c - direction.y * s, direction.x * s + direction.y * c, direction.z };
	};

	auto queueTankBillboard = [&](const Vector3& center, const Vector3& direction, float radius, float lineWidth, const Vector4& color, const std::vector<Player::NeonBarrelLayout>* barrelLayouts) {
		constexpr float kTwoPi = 6.28318530718f;
		constexpr int kSegments = 24;
		if (drawBodies) {
			Vector3 previous{};
			for (int i = 0; i <= kSegments; ++i) {
				const float angle = static_cast<float>(i % kSegments) * (kTwoPi / static_cast<float>(kSegments));
				const Vector3 current = center + cameraRight * (std::cos(angle) * radius) + cameraUp * (std::sin(angle) * radius);
				if (i > 0) {
					neonGridRenderer_->QueueLine(previous, current, lineWidth, color);
				}
				previous = current;
			}
		}

		Vector3 mainDirection = direction;
		if (Length(mainDirection) < 0.001f) {
			mainDirection = { 0.0f, -1.0f, 0.0f };
		}
		mainDirection = Normalize(mainDirection);
		const float mainRotation = directionToRotation(mainDirection);
		const Vector3 mainForward = cameraRight * std::sin(mainRotation) + cameraUp * -std::cos(mainRotation);
		const Vector3 mainRight = cameraRight * std::cos(mainRotation) + cameraUp * std::sin(mainRotation);

		auto queueBarrel = [&](const Player::NeonBarrelLayout& layout) {
			const Vector3 barrelDirection = Normalize(rotateDirection(mainDirection, layout.angleRad));
			const float barrelRotation = directionToRotation(barrelDirection);
			const Vector3 barrelForward = cameraRight * std::sin(barrelRotation) + cameraUp * -std::cos(barrelRotation);
			const Vector3 barrelRight = cameraRight * std::cos(barrelRotation) + cameraUp * std::sin(barrelRotation);
			const Vector3 barrelCenter = center +
				mainForward * ((layout.offset.x - layout.recoilOffset) * radius) +
				mainRight * (layout.offset.y * radius) +
				Vector3{ 0.0f, 0.0f, layout.offset.z };
			const float length = (std::max)(radius * 0.28f, layout.scale.x * radius * 0.72f);
			const float half = (std::max)(lineWidth * 1.5f, layout.scale.y * radius * 0.75f);
			const Vector3 base = barrelCenter - barrelForward * (length * 0.20f);
			const Vector3 tip = barrelCenter + barrelForward * (length * 0.80f);
			neonGridRenderer_->QueueLine(base - barrelRight * half, tip - barrelRight * half, lineWidth, color);
			neonGridRenderer_->QueueLine(tip - barrelRight * half, tip + barrelRight * half, lineWidth, color);
			neonGridRenderer_->QueueLine(tip + barrelRight * half, base + barrelRight * half, lineWidth, color);
			neonGridRenderer_->QueueLine(base + barrelRight * half, base - barrelRight * half, lineWidth, color);
		};

		if (!drawBarrels) {
			return;
		}
		if (barrelLayouts && !barrelLayouts->empty()) {
			for (const Player::NeonBarrelLayout& layout : *barrelLayouts) {
				queueBarrel(layout);
			}
		} else {
			queueBarrel({});
		}
	};

	const std::vector<Player::NeonBarrelLayout> playerBarrels = player_ ? player_->GetNeonBarrelLayouts() : std::vector<Player::NeonBarrelLayout>{};

	for (const PlayerNeonAfterimage& afterimage : playerNeonAfterimages_) {
		const float lifeRatio = (std::clamp)(afterimage.life / (std::max)(0.001f, playerAfterimageLifetime_), 0.0f, 1.0f);
		Vector4 color = playerGridColor_;
		color.w *= playerAfterimageAlpha_ * lifeRatio;
		const float radius = playerNeonBillboardRadius_ * (0.88f + lifeRatio * 0.12f);
		queueTankBillboard(afterimage.position + Vector3{ 0.0f, 0.0f, 0.35f }, afterimage.direction, radius, actorNeonBillboardLineWidth_, color, &playerBarrels);
	}

	if (playerNeonRenderMode_ == 1 && player_ && !player_->IsDead()) {
		Vector4 color = playerGridColor_;
		const float feedback = player_->GetDamageFeedbackRatio();
		const float pulse = std::sin(feedback * 3.14159265f) * 0.10f;
		color = lerpColor(color, { 1.8f, 1.8f, 1.8f, color.w }, feedback * feedback * 0.75f);
		if (player_->IsDashing()) {
			color.w *= playerDashCurrentAlpha_;
		}
		queueTankBillboard(player_->GetWorldPosition() + Vector3{ 0.0f, 0.0f, 0.35f }, player_->GetDirection(), playerNeonBillboardRadius_ * (1.0f + pulse), actorNeonBillboardLineWidth_, color, &playerBarrels);
	}
	if (bossNeonRenderMode_ == 1 && enemy_ && !enemy_->IsDead()) {
		const float feedback = enemy_->GetDamageFeedbackRatio();
		const float pulse = std::sin(feedback * 3.14159265f) * 0.08f;
		const Vector4 color = lerpColor(enemyGridColor_, { 1.8f, 1.8f, 1.8f, enemyGridColor_.w }, feedback * feedback * 0.65f);
		Player::NeonBarrelLayout bossBarrel{};
		bossBarrel.offset = { bossNeonBarrelForwardOffset_, bossNeonBarrelSideOffset_, 0.0f };
		bossBarrel.scale = { bossNeonBarrelLengthScale_, bossNeonBarrelWidthScale_, bossNeonBarrelWidthScale_ };
		bossBarrel.angleRad = bossNeonBarrelAngleDeg_ * 3.1415926535f / 180.0f;
		const std::vector<Player::NeonBarrelLayout> bossBarrels = { bossBarrel };
		queueTankBillboard(enemy_->GetWorldPosition() + Vector3{ 0.0f, 0.0f, 0.35f }, enemy_->GetAimDirection(), bossNeonBillboardRadius_ * (1.0f + pulse), actorNeonBillboardLineWidth_, color, &bossBarrels);
	}
}

void GameScene::DrawActorNeonBodyFillPass() {
	if (!neonGridRenderer_ || !fillActorNeonBodies_) {
		return;
	}

	const Matrix4x4 viewMatrix = Object3dCommon::GetInstance()->GetIsDebugCamera()
		? debugCamera->GetViewMatrix()
		: camera->GetViewMatrix();
	const Matrix4x4 cameraWorld = Inverse(viewMatrix);
	const Vector3 cameraRight = Normalize({ cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2] });
	const Vector3 cameraUp = Normalize({ cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2] });
	const Matrix4x4 vp = Object3dCommon::GetInstance()->GetIsDebugCamera()
		? debugCamera->GetViewProjectionMatrix()
		: camera->GetViewProjectionMatrix();

	const uint32_t fillStart = neonGridRenderer_->GetVertexCount();
	if (playerNeonRenderMode_ == 1 && player_ && !player_->IsDead()) {
		const float feedback = player_->GetDamageFeedbackRatio();
		const float pulse = std::sin(feedback * 3.14159265f) * 0.10f;
		neonGridRenderer_->QueueBillboardDisc(
			player_->GetWorldPosition() + Vector3{ 0.0f, 0.0f, 0.345f },
			playerNeonBillboardRadius_ * (1.0f + pulse) * 0.96f,
			actorNeonBodyFillColor_, cameraRight, cameraUp);
	}
	if (bossNeonRenderMode_ == 1 && enemy_ && !enemy_->IsDead()) {
		const float feedback = enemy_->GetDamageFeedbackRatio();
		const float pulse = std::sin(feedback * 3.14159265f) * 0.08f;
		neonGridRenderer_->QueueBillboardDisc(
			enemy_->GetWorldPosition() + Vector3{ 0.0f, 0.0f, 0.345f },
			bossNeonBillboardRadius_ * (1.0f + pulse) * 0.96f,
			actorNeonBodyFillColor_, cameraRight, cameraUp);
	}
	const uint32_t fillCount = neonGridRenderer_->GetVertexCount() - fillStart;
	neonGridRenderer_->DrawRangeSolid(fillStart, fillCount, vp);

	const uint32_t outlineStart = neonGridRenderer_->GetVertexCount();
	QueueActorNeonBillboards(cameraRight, cameraUp, true, false);
	const uint32_t outlineCount = neonGridRenderer_->GetVertexCount() - outlineStart;
	neonGridRenderer_->DrawRange(outlineStart, outlineCount, vp);
}

void GameScene::QueueExpEnemyNeonShapes(const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward) {
	if (!enemyManager_ || !neonGridRenderer_) {
		return;
	}

	auto maxScaleComponent = [](const Vector3& scale) {
		return (std::max)({ scale.x, scale.y, scale.z });
	};

	auto transformLocalPoint = [](const Vector3& local, const Vector3& center, const Vector3& rotate, const Vector3& scale) {
		const Matrix4x4 transform = MakeAffineMatrix(scale, rotate, center);
		return TransformMatrix(local, transform);
	};

	auto queueCube = [&](const Vector3& center, float size, const Vector3& rotate, const Vector3& scale, float lineWidth, const Vector4& color) {
		const float h = size * 0.5f;
		const float z = size * 0.5f;
		Vector3 corners[8] = {
			{ -h, -h, -z }, { h, -h, -z }, { h, h, -z }, { -h, h, -z },
			{ -h, -h, z }, { h, -h, z }, { h, h, z }, { -h, h, z }
		};
		for (Vector3& corner : corners) {
			corner = transformLocalPoint(corner, center, rotate, scale);
		}
		const int edges[12][2] = {
			{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
			{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
			{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
		};
		for (const auto& edge : edges) {
			neonGridRenderer_->QueueLine(corners[edge[0]], corners[edge[1]], lineWidth, color);
		}
	};

	auto queueTriangularPyramid = [&](const Vector3& center, float radius, const Vector3& rotate, const Vector3& scale, float lineWidth, const Vector4& color) {
		constexpr float kTwoPi = 6.28318530718f;
		const float depth = radius * 0.75f;
		Vector3 points[4]{};
		for (int i = 0; i < 3; ++i) {
			const float angle = -kTwoPi * 0.25f + static_cast<float>(i) * (kTwoPi / 3.0f);
			points[i] = transformLocalPoint({ std::cos(angle) * radius, std::sin(angle) * radius, -depth * 0.35f }, center, rotate, scale);
		}
		points[3] = transformLocalPoint({ 0.0f, 0.0f, depth * 0.65f }, center, rotate, scale);
		neonGridRenderer_->QueueLine(points[0], points[1], lineWidth, color);
		neonGridRenderer_->QueueLine(points[1], points[2], lineWidth, color);
		neonGridRenderer_->QueueLine(points[2], points[0], lineWidth, color);
		neonGridRenderer_->QueueLine(points[0], points[3], lineWidth, color);
		neonGridRenderer_->QueueLine(points[1], points[3], lineWidth, color);
		neonGridRenderer_->QueueLine(points[2], points[3], lineWidth, color);
	};

	auto queuePentagonalPrism = [&](const Vector3& center, float radius, const Vector3& rotate, const Vector3& scale, float lineWidth, const Vector4& color) {
		constexpr float kTwoPi = 6.28318530718f;
		const float depth = radius * 0.85f;
		Vector3 points[10]{};
		for (int i = 0; i < 5; ++i) {
			const float angle = -kTwoPi * 0.25f + static_cast<float>(i) * (kTwoPi / 5.0f);
			const Vector3 front = { std::cos(angle) * radius, std::sin(angle) * radius, -depth * 0.5f };
			const Vector3 back = { front.x, front.y, depth * 0.5f };
			points[i] = transformLocalPoint(front, center, rotate, scale);
			points[i + 5] = transformLocalPoint(back, center, rotate, scale);
		}
		for (int i = 0; i < 5; ++i) {
			const int next = (i + 1) % 5;
			neonGridRenderer_->QueueLine(points[i], points[next], lineWidth, color);
			neonGridRenderer_->QueueLine(points[i + 5], points[next + 5], lineWidth, color);
			neonGridRenderer_->QueueLine(points[i], points[i + 5], lineWidth, color);
		}
	};

	auto queueCircle3D = [&](const Vector3& center, float radius, const Vector3& rotate, const Vector3& scale, int axis, float lineWidth, const Vector4& color) {
		constexpr float kTwoPi = 6.28318530718f;
		constexpr int kSegments = 20;
		Vector3 previous{};
		for (int i = 0; i <= kSegments; ++i) {
			const float angle = static_cast<float>(i) * (kTwoPi / static_cast<float>(kSegments));
			Vector3 local{};
			if (axis == 0) {
				local = { 0.0f, std::cos(angle) * radius, std::sin(angle) * radius };
			} else if (axis == 1) {
				local = { std::cos(angle) * radius, 0.0f, std::sin(angle) * radius };
			} else {
				local = { std::cos(angle) * radius, std::sin(angle) * radius, 0.0f };
			}
			const Vector3 current = transformLocalPoint(local, center, rotate, scale);
			if (i > 0) {
				neonGridRenderer_->QueueLine(previous, current, lineWidth, color);
			}
			previous = current;
		}
	};

	auto queueShooter = [&](const Vector3& center, float radius, const Vector3& rotate, const Vector3& scale, float lineWidth, const Vector4& color) {
		queueCircle3D(center, radius, rotate, scale, 0, lineWidth, color);
		queueCircle3D(center, radius, rotate, scale, 1, lineWidth, color);
		queueCircle3D(center, radius, rotate, scale, 2, lineWidth, color);

		const float barrelLength = radius * 0.9f;
		const float barrelHalf = radius * 0.22f;
		const float barrelOffset = radius * 0.72f;
		Vector3 corners[8] = {
			{ -barrelHalf, -barrelOffset, -barrelHalf },
			{ barrelHalf, -barrelOffset, -barrelHalf },
			{ barrelHalf, -barrelOffset - barrelLength, -barrelHalf },
			{ -barrelHalf, -barrelOffset - barrelLength, -barrelHalf },
			{ -barrelHalf, -barrelOffset, barrelHalf },
			{ barrelHalf, -barrelOffset, barrelHalf },
			{ barrelHalf, -barrelOffset - barrelLength, barrelHalf },
			{ -barrelHalf, -barrelOffset - barrelLength, barrelHalf }
		};
		for (Vector3& corner : corners) {
			corner = transformLocalPoint(corner, center, rotate, scale);
		}
		const int edges[12][2] = {
			{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
			{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
			{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
		};
		for (const auto& edge : edges) {
			neonGridRenderer_->QueueLine(corners[edge[0]], corners[edge[1]], lineWidth, color);
		}
	};

	auto queueBillboardPolygon = [&](const Vector3& center, int sides, float radius, float rotation, float lineWidth, const Vector4& color) {
		constexpr float kTwoPi = 6.28318530718f;
		if (sides < 3) {
			return;
		}
		Vector3 previous{};
		for (int i = 0; i <= sides; ++i) {
			const float angle = rotation - kTwoPi * 0.25f + static_cast<float>(i % sides) * (kTwoPi / static_cast<float>(sides));
			const Vector3 current = center + cameraRight * (std::cos(angle) * radius) + cameraUp * (std::sin(angle) * radius);
			if (i > 0) {
				neonGridRenderer_->QueueLine(previous, current, lineWidth, color);
			}
			previous = current;
		}
	};

	auto queueBillboardShooter = [&](const Vector3& center, float radius, float rotation, float lineWidth, const Vector4& color) {
		queueBillboardPolygon(center, 20, radius, rotation, lineWidth, color);
		const float c = std::cos(rotation);
		const float s = std::sin(rotation);
		const Vector3 down = cameraRight * s + cameraUp * -c;
		const Vector3 side = cameraRight * c + cameraUp * s;
		const Vector3 base = center + down * (radius * 0.65f);
		const Vector3 tip = center + down * (radius * 1.45f);
		const float half = radius * 0.18f;
		neonGridRenderer_->QueueLine(base - side * half, tip - side * half, lineWidth, color);
		neonGridRenderer_->QueueLine(tip - side * half, tip + side * half, lineWidth, color);
		neonGridRenderer_->QueueLine(tip + side * half, base + side * half, lineWidth, color);
		neonGridRenderer_->QueueLine(base + side * half, base - side * half, lineWidth, color);
	};

	for (ExpEnemy* expEnemy : enemyManager_->GetEnemyPtrs()) {
		if (!expEnemy || expEnemy->IsDead() || !expEnemy->IsShapeNeonRenderTarget()) {
			continue;
		}
		const Vector3 visualScale = expEnemy->GetVisualScale();
		const float scalePulse = maxScaleComponent(visualScale);
		float shapeRadius = expEnemyNeonSquareSize_ * 0.5f;
		if (expEnemy->GetType() == ExpEnemyType::Triangle) {
			shapeRadius = expEnemyNeonTriangleRadius_;
		} else if (expEnemy->GetType() == ExpEnemyType::Pentagon) {
			shapeRadius = expEnemyNeonPentagonRadius_;
		} else if (expEnemy->GetType() == ExpEnemyType::Shooter) {
			shapeRadius = expEnemyNeonShooterRadius_ * 1.5f;
		}
		if (cullActorLocalGrid_ && !IsNearCamera2D(expEnemy->GetWorldPosition(), 38.0f, 24.0f, shapeRadius * scalePulse + 1.0f)) {
			continue;
		}

		const Vector3 center = expEnemy->GetWorldPosition() + (expEnemyNeonRenderMode_ >= 2 ? Vector3{} : Vector3{ 0.0f, 0.0f, 0.35f });
		const Vector4 color = expEnemy->GetVisualColor();
		if (expEnemyNeonRenderMode_ >= 2) {
			if (expEnemy->GetType() == ExpEnemyType::Triangle) {
				queueTriangularPyramid(center, expEnemyNeonTriangleRadius_, expEnemy->GetVisualRotate(), visualScale, expEnemyNeonLineWidth_, color);
			} else if (expEnemy->GetType() == ExpEnemyType::Pentagon) {
				queuePentagonalPrism(center, expEnemyNeonPentagonRadius_, expEnemy->GetVisualRotate(), visualScale, expEnemyNeonLineWidth_, color);
			} else if (expEnemy->GetType() == ExpEnemyType::Shooter) {
				queueShooter(center, expEnemyNeonShooterRadius_, expEnemy->GetVisualRotate(), visualScale, expEnemyNeonLineWidth_, color);
			} else {
				queueCube(center, expEnemyNeonSquareSize_, expEnemy->GetVisualRotate(), visualScale, expEnemyNeonLineWidth_, color);
			}
		} else if (expEnemy->GetType() == ExpEnemyType::Triangle) {
			neonGridRenderer_->QueueBillboardTriangle(
				center,
				expEnemyNeonTriangleRadius_ * scalePulse,
				expEnemy->GetVisualRotation(),
				expEnemyNeonLineWidth_,
				color,
				cameraRight,
				cameraUp,
				cameraForward);
		} else if (expEnemy->GetType() == ExpEnemyType::Pentagon) {
			queueBillboardPolygon(center, 5, expEnemyNeonPentagonRadius_ * scalePulse, expEnemy->GetVisualRotation(), expEnemyNeonLineWidth_, color);
		} else if (expEnemy->GetType() == ExpEnemyType::Shooter) {
			queueBillboardShooter(center, expEnemyNeonShooterRadius_ * scalePulse, expEnemy->GetVisualRotation(), expEnemyNeonLineWidth_, color);
		} else {
			neonGridRenderer_->QueueBillboardRectangle(
				center,
				{ expEnemyNeonSquareSize_ * scalePulse, expEnemyNeonSquareSize_ * scalePulse },
				expEnemy->GetVisualRotation(),
				expEnemyNeonLineWidth_,
				color,
				cameraRight,
				cameraUp,
				cameraForward);
		}
	}
}

void GameScene::DrawExpEnemyNeonFillModels() {
	if (!enemyManager_) {
		return;
	}

	for (ExpEnemy* expEnemy : enemyManager_->GetEnemyPtrs()) {
		if (!expEnemy || expEnemy->IsDead() || !expEnemy->IsShapeNeonRenderTarget()) {
			continue;
		}
		float shapeRadius = expEnemyNeonSquareSize_ * 0.5f;
		if (expEnemy->GetType() == ExpEnemyType::Triangle) {
			shapeRadius = expEnemyNeonTriangleRadius_;
		} else if (expEnemy->GetType() == ExpEnemyType::Pentagon) {
			shapeRadius = expEnemyNeonPentagonRadius_;
		} else if (expEnemy->GetType() == ExpEnemyType::Shooter) {
			shapeRadius = expEnemyNeonShooterRadius_ * 1.5f;
		}
		const Vector3 visualScale = expEnemy->GetVisualScale();
		const float scalePulse = (std::max)({ visualScale.x, visualScale.y, visualScale.z });
		if (cullActorLocalGrid_ && !IsNearCamera2D(expEnemy->GetWorldPosition(), 38.0f, 24.0f, shapeRadius * scalePulse + 1.0f)) {
			continue;
		}
		expEnemy->DrawNeonFillBodyOnly();
	}
}

void GameScene::DrawExpEnemyNeonDepthLines() {
	if (!neonGridRenderer_) {
		return;
	}

	Matrix4x4 viewMatrix = Object3dCommon::GetInstance()->GetIsDebugCamera()
		? debugCamera->GetViewMatrix()
		: camera->GetViewMatrix();
	Matrix4x4 cameraWorld = Inverse(viewMatrix);
	Vector3 cameraRight = Normalize({ cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2] });
	Vector3 cameraUp = Normalize({ cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2] });
	Vector3 cameraForward = Normalize({ cameraWorld.m[2][0], cameraWorld.m[2][1], cameraWorld.m[2][2] });

	neonGridRenderer_->BeginFrame();
	neonGridRenderer_->SetLineStyle(neonLineSoftEdgeRatio_, neonLineCoreIntensity_);
	QueueExpEnemyNeonShapes(cameraRight, cameraUp, cameraForward);

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
		postProfileAverageMs_[i] = avgMs;
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

void GameScene::DrawPerformanceBreakdownImGui()
{
#ifdef USE_IMGUI
	const float fps = (std::max)(1.0f, ImGui::GetIO().Framerate);
	const float frameMs = renderProfile_.frameTotalMs > 0.0f ? renderProfile_.frameTotalMs : 1000.0f / fps;
	auto fpsGainIfRemoved = [frameMs, fps](float ms) -> float {
		if (ms <= 0.0f || ms >= frameMs - 0.01f) {
			return 0.0f;
		}
		return 1000.0f / (frameMs - ms) - fps;
	};
	const float measuredTopLevelMs =
		renderProfile_.messagePumpMs + renderProfile_.inputImGuiBeginMs +
		renderProfile_.engineUpdateMs + renderProfile_.sceneUpdateMs +
		renderProfile_.imguiBuildMs + renderProfile_.drawSetupMs +
		renderProfile_.drawRecordMs + renderProfile_.imguiDrawMs +
		renderProfile_.postDrawMs;
	const float unmeasuredMs = (std::max)(0.0f, frameMs - measuredTopLevelMs);

	ImGui::Text("現在FPS %.2f / 計測フレーム %.3fms", fps, frameMs);
	ImGui::Text("合計対象 %.3fms + 未計測 %.3fms = %.3fms (100.0%%)", measuredTopLevelMs, unmeasuredMs, frameMs);
	const bool gpuValidationEnabled = Object3dCommon::GetInstance()->GetDxCommon()->IsGpuBasedValidationEnabled();
	ImGui::TextColored(
		gpuValidationEnabled ? ImVec4{ 1.0f, 0.45f, 0.30f, 1.0f } : ImVec4{ 0.45f, 1.0f, 0.65f, 1.0f },
		"D3D12 GPU-Based Validation: %s", gpuValidationEnabled ? "ON (計測には非常に重い)" : "OFF");
	ImGui::TextWrapped("[合計] 行だけで1フレームを分割します。[内訳] は親項目の詳細なので、合計には二重加算しません。Fence待ちはGPU処理完了待ちを含みます。");

	if (ImGui::BeginTable("PerformanceBreakdownTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
		ImGui::TableSetupColumn("区分");
		ImGui::TableSetupColumn("項目");
		ImGui::TableSetupColumn("状態");
		ImGui::TableSetupColumn("直近ms");
		ImGui::TableSetupColumn("FPS影響");
		ImGui::TableSetupColumn("比率");
		ImGui::TableHeadersRow();

		auto row = [&](const char* kind, const char* name, const char* state, float ms) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(kind);
			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted(name);
			ImGui::TableSetColumnIndex(2);
			ImGui::TextUnformatted(state);
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%.3f", ms);
			ImGui::TableSetColumnIndex(4);
			ImGui::Text("+%.2f fps", fpsGainIfRemoved(ms));
			ImGui::TableSetColumnIndex(5);
			ImGui::Text("%.1f%%", frameMs > 0.0f ? (ms / frameMs) * 100.0f : 0.0f);
		};
		auto totalRow = [&](const char* name, float ms) { row("合計", name, "ON", ms); };
		auto detailRow = [&](const char* name, bool active, float ms) { row("内訳", name, active ? "ON" : "OFF", ms); };

		totalRow("Windows Message", renderProfile_.messagePumpMs);
		totalRow("Input + ImGui Begin", renderProfile_.inputImGuiBeginMs);
		totalRow("Engine Common Update", renderProfile_.engineUpdateMs);
		totalRow("Scene Update / UI構築", renderProfile_.sceneUpdateMs);
		totalRow("ImGui Build", renderProfile_.imguiBuildMs);
		totalRow("Draw Setup", renderProfile_.drawSetupMs);
		totalRow("Draw Command Record", renderProfile_.drawRecordMs);
		totalRow("ImGui Draw Command", renderProfile_.imguiDrawMs);
		totalRow("Submit / Present / Wait", renderProfile_.postDrawMs);
		row("合計", "未計測・計測誤差", "-", unmeasuredMs);

		detailRow("  Scene PostEffect3D", true, renderProfile_.scenePostMs);
		detailRow("  Global Bloom/Post", true, renderProfile_.globalBloomMs);
		detailRow("  After Object Post", renderProfile_.afterPostMs > 0.0f, renderProfile_.afterPostMs);
		detailRow("  Sprite Pass", true, renderProfile_.spriteMs);
		detailRow("  CommandList Close", true, renderProfile_.submitCloseMs);
		detailRow("  ExecuteCommandLists", true, renderProfile_.submitExecuteMs);
		detailRow("  Present", true, renderProfile_.presentMs);
		detailRow("  GPU Fence Wait", true, renderProfile_.fenceWaitMs);
		detailRow("  FPS固定待ち", renderProfile_.fpsLimitMs > 0.01f, renderProfile_.fpsLimitMs);
		detailRow("  Allocator/List Reset", true, renderProfile_.submitResetMs);
		for (size_t i = 0; i < postProfileEntryCount_; ++i) {
			detailRow(postProfileEntries_[i].name, postProfileEntries_[i].active, postProfileAverageMs_[i]);
		}
		if (player_) {
			const auto& hud = player_->GetUpgradeHudProfileStats();
			const auto& evo = player_->GetEvolutionUiProfileStats();
			char hudState[32]{};
			std::snprintf(hudState, sizeof(hudState), "%s %d draws", hud.visible ? "ON" : "OFF", hud.spriteDraws + hud.textDraws);
			row("内訳", "Upgrade HUD CPU", hudState, hud.totalMs);
			detailRow("  HUD Update", hud.visible, hud.updateMs);
			detailRow("  HUD Sprite", hud.visible, hud.spriteMs);
			detailRow("  HUD Text", hud.visible, hud.textMs);
			char evoState[32]{};
			std::snprintf(evoState, sizeof(evoState), "%s %d draws", evo.visible ? "ON" : "OFF", evo.spriteDraws + evo.textDraws);
			row("内訳", "Evolution UI CPU", evoState, evo.totalMs);
			detailRow("  Evolution Update", evo.visible, evo.updateMs);
			detailRow("  Evolution Sprite", evo.visible, evo.spriteMs);
			detailRow("  Evolution Text", evo.visible, evo.textMs);
		}
		ImGui::EndTable();
	}
#endif
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
		config.reloadSpeed = ReadCustomFloat(playerJson, "reloadSpeed", config.reloadSpeed);
		config.bulletDamage = ReadCustomFloat(playerJson, "bulletDamage", config.bulletDamage);
		config.bulletSpeed = ReadCustomFloat(playerJson, "bulletSpeed", config.bulletSpeed);
		config.moveSpeed = ReadCustomFloat(playerJson, "moveSpeed", config.moveSpeed);
		config.staminaRecovery = ReadCustomFloat(playerJson, "staminaRecovery", config.staminaRecovery);
		config.maxStamina = ReadCustomFloat(playerJson, "maxStamina", config.maxStamina);
		config.maxHpUpgradeAmount = ReadCustomFloat(playerJson, "maxHpUpgradeAmount", 0.10f);
		config.bodyDamage = static_cast<uint32_t>((std::max)(1, ReadCustomInt(playerJson, "bodyDamage", static_cast<int>(player_->GetDamage()))));
		if (balanceJson.contains("playerUpgrades") && balanceJson["playerUpgrades"].is_object()) {
			const nlohmann::json& upgradeJson = balanceJson["playerUpgrades"];
			config.healthRegenUpgrade = ReadCustomFloat(upgradeJson, "healthRegen", config.healthRegenUpgrade);
			config.maxHpUpgradeAmount = ReadCustomFloat(upgradeJson, "maxHp", config.maxHpUpgradeAmount);
			config.bodyDamageUpgrade = ReadCustomFloat(upgradeJson, "bodyDamage", config.bodyDamageUpgrade);
			config.bulletSpeedUpgrade = ReadCustomFloat(upgradeJson, "bulletSpeed", config.bulletSpeedUpgrade);
			config.bulletDamageUpgrade = ReadCustomFloat(upgradeJson, "bulletDamage", config.bulletDamageUpgrade);
			config.reloadUpgrade = ReadCustomFloat(upgradeJson, "reloadSpeed", config.reloadUpgrade);
			config.moveSpeedUpgrade = ReadCustomFloat(upgradeJson, "moveSpeed", config.moveSpeedUpgrade);
			config.minReloadSpeed = ReadCustomFloat(upgradeJson, "minReloadSpeed", config.minReloadSpeed);
		}
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
		expConfig.shooterDetectionRadius = ReadCustomFloat(damageJson, "shooterDetectionRadius", expConfig.shooterDetectionRadius);
		expConfig.shooterTurnSpeed = ReadCustomFloat(damageJson, "shooterTurnSpeed", expConfig.shooterTurnSpeed);
		expConfig.shooterFireInterval = ReadCustomFloat(damageJson, "shooterFireInterval", expConfig.shooterFireInterval);
		expConfig.shooterBulletSpeed = ReadCustomFloat(damageJson, "shooterBulletSpeed", expConfig.shooterBulletSpeed);
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
		config.bulletHp = ReadCustomFloat(attackJson, "bulletHp", config.bulletHp);
		config.bulletPenetration = ReadCustomFloat(attackJson, "bulletPenetration", config.bulletPenetration);
		config.randomSpread = ReadCustomBool(attackJson, "randomSpread", config.randomSpread);
		config.pattern = static_cast<Enemy::BossAttackConfig::Pattern>((std::clamp)(ReadCustomInt(attackJson, "pattern", static_cast<int>(config.pattern)), 0, 3));
		enemy_->SetBossAttackConfig(config);
	}

	if (enemyManager_ || enemy_) {
		const nlohmann::json emptyEnemySystem = nlohmann::json::object();
		const nlohmann::json& enemySystemJson =
			(balanceJson.contains("enemySystem") && balanceJson["enemySystem"].is_object())
				? balanceJson["enemySystem"]
				: emptyEnemySystem;
		const bool hostile = ReadCustomBool(enemySystemJson, "expEnemyHostileToBoss", false);
		if (enemyManager_) {
			enemyManager_->SetExpEnemyHostileToBoss(hostile);
		}
		if (enemy_) {
			Enemy::EnemyProgressConfig config = enemy_->GetEnemyProgressConfig();
			config.expEnemyHostile = hostile;
			config.expEnemyContactDamage = static_cast<uint32_t>((std::max)(1, ReadCustomInt(enemySystemJson, "bossExpEnemyDamage", static_cast<int>(config.expEnemyContactDamage))));
			config.healOnExpEnemyKill = ReadCustomInt(enemySystemJson, "bossHealOnExpEnemyKill", config.healOnExpEnemyKill);
			config.killsPerLevel = ReadCustomInt(enemySystemJson, "bossKillsPerLevel", config.killsPerLevel);
			config.maxHpGainPerLevel = ReadCustomInt(enemySystemJson, "bossMaxHpGainPerLevel", config.maxHpGainPerLevel);
			config.damageGainPerLevel = static_cast<uint32_t>((std::max)(0, ReadCustomInt(enemySystemJson, "bossDamageGainPerLevel", static_cast<int>(config.damageGainPerLevel))));
			config.levelingModeEnabled = ReadCustomBool(enemySystemJson, "bossLevelingModeEnabled", config.levelingModeEnabled);
			config.levelingEnterPlayerDistance = ReadCustomFloat(enemySystemJson, "bossLevelingEnterDistance", config.levelingEnterPlayerDistance);
			config.levelingExitPlayerDistance = ReadCustomFloat(enemySystemJson, "bossLevelingExitDistance", config.levelingExitPlayerDistance);
			config.levelingSearchRadius = ReadCustomFloat(enemySystemJson, "bossLevelingSearchRadius", config.levelingSearchRadius);
			config.aimTurnHalfSeconds = ReadCustomFloat(enemySystemJson, "bossAimTurnHalfSeconds", config.aimTurnHalfSeconds);
			enemy_->SetEnemyProgressConfig(config);
		}
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
		balanceEditor_.playerReloadSpeed = ReadCustomFloat(playerJson, "reloadSpeed", balanceEditor_.playerReloadSpeed);
		balanceEditor_.playerBulletDamage = ReadCustomFloat(playerJson, "bulletDamage", balanceEditor_.playerBulletDamage);
		balanceEditor_.playerBulletSpeed = ReadCustomFloat(playerJson, "bulletSpeed", balanceEditor_.playerBulletSpeed);
		balanceEditor_.playerMoveSpeed = ReadCustomFloat(playerJson, "moveSpeed", balanceEditor_.playerMoveSpeed);
		balanceEditor_.playerStaminaRecovery = ReadCustomFloat(playerJson, "staminaRecovery", balanceEditor_.playerStaminaRecovery);
		balanceEditor_.playerMaxStamina = ReadCustomFloat(playerJson, "maxStamina", balanceEditor_.playerMaxStamina);
		balanceEditor_.maxHpUpgradeAmount = ReadCustomFloat(playerJson, "maxHpUpgradeAmount", balanceEditor_.maxHpUpgradeAmount);
		balanceEditor_.playerBodyDamage = ReadCustomInt(playerJson, "bodyDamage", balanceEditor_.playerBodyDamage);
		balanceEditor_.healToFull = ReadCustomBool(playerJson, "healToFull", balanceEditor_.healToFull);
	}
	if (balanceJson.contains("playerUpgrades") && balanceJson["playerUpgrades"].is_object()) {
		const nlohmann::json& upgradeJson = balanceJson["playerUpgrades"];
		balanceEditor_.playerHealthRegenUpgrade = ReadCustomFloat(upgradeJson, "healthRegen", balanceEditor_.playerHealthRegenUpgrade);
		balanceEditor_.maxHpUpgradeAmount = ReadCustomFloat(upgradeJson, "maxHp", balanceEditor_.maxHpUpgradeAmount);
		balanceEditor_.playerBodyDamageUpgrade = ReadCustomFloat(upgradeJson, "bodyDamage", balanceEditor_.playerBodyDamageUpgrade);
		balanceEditor_.playerBulletSpeedUpgrade = ReadCustomFloat(upgradeJson, "bulletSpeed", balanceEditor_.playerBulletSpeedUpgrade);
		balanceEditor_.playerBulletDamageUpgrade = ReadCustomFloat(upgradeJson, "bulletDamage", balanceEditor_.playerBulletDamageUpgrade);
		balanceEditor_.playerReloadUpgrade = ReadCustomFloat(upgradeJson, "reloadSpeed", balanceEditor_.playerReloadUpgrade);
		balanceEditor_.playerMoveSpeedUpgrade = ReadCustomFloat(upgradeJson, "moveSpeed", balanceEditor_.playerMoveSpeedUpgrade);
		balanceEditor_.playerMinReloadSpeed = ReadCustomFloat(upgradeJson, "minReloadSpeed", balanceEditor_.playerMinReloadSpeed);
	}
	if (balanceJson.contains("damage") && balanceJson["damage"].is_object()) {
		const nlohmann::json& damageJson = balanceJson["damage"];
		balanceEditor_.damageBlock = ReadCustomInt(damageJson, "damageBlock", balanceEditor_.damageBlock);
		balanceEditor_.bossContact = ReadCustomInt(damageJson, "bossContact", balanceEditor_.bossContact);
		balanceEditor_.expEnemyContact = ReadCustomInt(damageJson, "expEnemyContact", balanceEditor_.expEnemyContact);
		balanceEditor_.shooterContact = ReadCustomInt(damageJson, "shooterContact", balanceEditor_.shooterContact);
		balanceEditor_.shooterBullet = ReadCustomInt(damageJson, "shooterBullet", balanceEditor_.shooterBullet);
		balanceEditor_.shooterDetectionRadius = ReadCustomFloat(damageJson, "shooterDetectionRadius", balanceEditor_.shooterDetectionRadius);
		balanceEditor_.shooterTurnSpeed = ReadCustomFloat(damageJson, "shooterTurnSpeed", balanceEditor_.shooterTurnSpeed);
		balanceEditor_.shooterFireInterval = ReadCustomFloat(damageJson, "shooterFireInterval", balanceEditor_.shooterFireInterval);
		balanceEditor_.shooterBulletSpeed = ReadCustomFloat(damageJson, "shooterBulletSpeed", balanceEditor_.shooterBulletSpeed);
	}
	if (balanceJson.contains("bossAttackDefault") && balanceJson["bossAttackDefault"].is_object()) {
		const nlohmann::json& attackJson = balanceJson["bossAttackDefault"];
		balanceEditor_.bossBulletSpeed = ReadCustomFloat(attackJson, "bulletSpeed", balanceEditor_.bossBulletSpeed);
		balanceEditor_.bossBulletCount = ReadCustomInt(attackJson, "bulletCount", balanceEditor_.bossBulletCount);
		balanceEditor_.bossSpreadAngleDeg = ReadCustomFloat(attackJson, "spreadAngleDeg", balanceEditor_.bossSpreadAngleDeg);
		balanceEditor_.bossCooldown = ReadCustomFloat(attackJson, "cooldown", balanceEditor_.bossCooldown);
		balanceEditor_.bossBulletDamage = ReadCustomInt(attackJson, "damage", balanceEditor_.bossBulletDamage);
		balanceEditor_.bossBulletHp = ReadCustomFloat(attackJson, "bulletHp", balanceEditor_.bossBulletHp);
		balanceEditor_.bossBulletPenetration = ReadCustomFloat(attackJson, "bulletPenetration", balanceEditor_.bossBulletPenetration);
		balanceEditor_.bossRandomSpread = ReadCustomBool(attackJson, "randomSpread", balanceEditor_.bossRandomSpread);
		balanceEditor_.bossAttackPattern = (std::clamp)(ReadCustomInt(attackJson, "pattern", balanceEditor_.bossAttackPattern), 0, 3);
	}
	if (balanceJson.contains("enemySystem") && balanceJson["enemySystem"].is_object()) {
		const nlohmann::json& enemySystemJson = balanceJson["enemySystem"];
		balanceEditor_.expEnemyHostileToBoss = ReadCustomBool(enemySystemJson, "expEnemyHostileToBoss", balanceEditor_.expEnemyHostileToBoss);
		balanceEditor_.bossExpEnemyDamage = ReadCustomInt(enemySystemJson, "bossExpEnemyDamage", balanceEditor_.bossExpEnemyDamage);
		balanceEditor_.bossHealOnExpEnemyKill = ReadCustomInt(enemySystemJson, "bossHealOnExpEnemyKill", balanceEditor_.bossHealOnExpEnemyKill);
		balanceEditor_.bossKillsPerLevel = ReadCustomInt(enemySystemJson, "bossKillsPerLevel", balanceEditor_.bossKillsPerLevel);
		balanceEditor_.bossMaxHpGainPerLevel = ReadCustomInt(enemySystemJson, "bossMaxHpGainPerLevel", balanceEditor_.bossMaxHpGainPerLevel);
		balanceEditor_.bossDamageGainPerLevel = ReadCustomInt(enemySystemJson, "bossDamageGainPerLevel", balanceEditor_.bossDamageGainPerLevel);
		balanceEditor_.bossLevelingModeEnabled = ReadCustomBool(enemySystemJson, "bossLevelingModeEnabled", balanceEditor_.bossLevelingModeEnabled);
		balanceEditor_.bossLevelingEnterDistance = ReadCustomFloat(enemySystemJson, "bossLevelingEnterDistance", balanceEditor_.bossLevelingEnterDistance);
		balanceEditor_.bossLevelingExitDistance = ReadCustomFloat(enemySystemJson, "bossLevelingExitDistance", balanceEditor_.bossLevelingExitDistance);
		balanceEditor_.bossLevelingSearchRadius = ReadCustomFloat(enemySystemJson, "bossLevelingSearchRadius", balanceEditor_.bossLevelingSearchRadius);
		balanceEditor_.bossAimTurnHalfSeconds = ReadCustomFloat(enemySystemJson, "bossAimTurnHalfSeconds", balanceEditor_.bossAimTurnHalfSeconds);
	}
	balanceEditor_.initialized = true;
}

nlohmann::json GameScene::BuildBalanceJsonFromEditor() const
{
	nlohmann::json balance = nlohmann::json::object();
	balance["defaultRandomSpawnEnabled"] = balanceEditor_.defaultRandomSpawnEnabled;
	balance["player"] = {
		{ "maxHp", (std::max)(1, balanceEditor_.playerMaxHp) },
		{ "reloadSpeed", (std::max)(0.05f, balanceEditor_.playerReloadSpeed) },
		{ "bulletDamage", (std::max)(0.1f, balanceEditor_.playerBulletDamage) },
		{ "bulletSpeed", (std::max)(0.01f, balanceEditor_.playerBulletSpeed) },
		{ "moveSpeed", (std::max)(0.01f, balanceEditor_.playerMoveSpeed) },
		{ "staminaRecovery", (std::max)(0.0f, balanceEditor_.playerStaminaRecovery) },
		{ "maxStamina", (std::max)(0.0f, balanceEditor_.playerMaxStamina) },
		{ "bodyDamage", (std::max)(1, balanceEditor_.playerBodyDamage) },
		{ "healToFull", balanceEditor_.healToFull }
	};
	balance["playerUpgrades"] = {
		{ "healthRegen", (std::max)(0.0f, balanceEditor_.playerHealthRegenUpgrade) },
		{ "maxHp", (std::max)(0.0f, balanceEditor_.maxHpUpgradeAmount) },
		{ "bodyDamage", (std::max)(0.0f, balanceEditor_.playerBodyDamageUpgrade) },
		{ "bulletSpeed", balanceEditor_.playerBulletSpeedUpgrade },
		{ "bulletDamage", (std::max)(0.0f, balanceEditor_.playerBulletDamageUpgrade) },
		{ "reloadSpeed", (std::max)(0.0f, balanceEditor_.playerReloadUpgrade) },
		{ "moveSpeed", balanceEditor_.playerMoveSpeedUpgrade },
		{ "minReloadSpeed", (std::max)(0.05f, balanceEditor_.playerMinReloadSpeed) }
	};
	balance["damage"] = {
		{ "damageBlock", (std::max)(1, balanceEditor_.damageBlock) },
		{ "bossContact", (std::max)(1, balanceEditor_.bossContact) },
		{ "expEnemyContact", (std::max)(1, balanceEditor_.expEnemyContact) },
		{ "shooterContact", (std::max)(1, balanceEditor_.shooterContact) },
		{ "shooterBullet", (std::max)(1, balanceEditor_.shooterBullet) },
		{ "shooterDetectionRadius", (std::max)(1.0f, balanceEditor_.shooterDetectionRadius) },
		{ "shooterTurnSpeed", (std::max)(0.1f, balanceEditor_.shooterTurnSpeed) },
		{ "shooterFireInterval", (std::max)(0.1f, balanceEditor_.shooterFireInterval) },
		{ "shooterBulletSpeed", (std::max)(0.01f, balanceEditor_.shooterBulletSpeed) }
	};
	balance["bossAttackDefault"] = {
		{ "bulletSpeed", (std::max)(0.01f, balanceEditor_.bossBulletSpeed) },
		{ "bulletCount", (std::max)(1, balanceEditor_.bossBulletCount) },
		{ "spreadAngleDeg", (std::clamp)(balanceEditor_.bossSpreadAngleDeg, 0.0f, 360.0f) },
		{ "cooldown", (std::max)(0.05f, balanceEditor_.bossCooldown) },
		{ "damage", (std::max)(1, balanceEditor_.bossBulletDamage) },
		{ "bulletHp", (std::max)(0.0f, balanceEditor_.bossBulletHp) },
		{ "bulletPenetration", (std::max)(0.0f, balanceEditor_.bossBulletPenetration) },
		{ "randomSpread", balanceEditor_.bossRandomSpread },
		{ "pattern", (std::clamp)(balanceEditor_.bossAttackPattern, 0, 3) }
	};
	balance["enemySystem"] = {
		{ "expEnemyHostileToBoss", balanceEditor_.expEnemyHostileToBoss },
		{ "bossExpEnemyDamage", (std::max)(1, balanceEditor_.bossExpEnemyDamage) },
		{ "bossHealOnExpEnemyKill", (std::max)(0, balanceEditor_.bossHealOnExpEnemyKill) },
		{ "bossKillsPerLevel", (std::max)(1, balanceEditor_.bossKillsPerLevel) },
		{ "bossMaxHpGainPerLevel", (std::max)(0, balanceEditor_.bossMaxHpGainPerLevel) },
		{ "bossDamageGainPerLevel", (std::max)(0, balanceEditor_.bossDamageGainPerLevel) },
		{ "bossLevelingModeEnabled", balanceEditor_.bossLevelingModeEnabled },
		{ "bossLevelingEnterDistance", (std::max)(1.0f, balanceEditor_.bossLevelingEnterDistance) },
		{ "bossLevelingExitDistance", (std::clamp)(balanceEditor_.bossLevelingExitDistance, 0.5f, balanceEditor_.bossLevelingEnterDistance) },
		{ "bossLevelingSearchRadius", (std::max)(1.0f, balanceEditor_.bossLevelingSearchRadius) },
		{ "bossAimTurnHalfSeconds", (std::max)(0.05f, balanceEditor_.bossAimTurnHalfSeconds) }
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

void GameScene::DrawPostEffectParamControls(const char* labelPrefix, BloomParam& param)
{
#ifdef USE_IMGUI
	ImGui::PushID(labelPrefix);
	ImGui::DragFloat("ブルーム強度", &param.intensity, 0.01f, 0.0f, 8.0f);
	ImGui::DragFloat("ブルームしきい値", &param.threshold, 0.01f, 0.0f, 2.0f);
	ImGui::DragFloat("歪み量", &param.distortionAmount, 0.001f, 0.0f, 0.2f);
	ImGui::DragFloat("色収差", &param.chromAbAmount, 0.001f, 0.0f, 0.2f);
	ImGui::DragFloat("グリッチ", &param.glitchAmount, 0.001f, 0.0f, 0.2f);
	ImGui::DragFloat("ディゾルブ", &param.dissolveThreshold, 0.01f, 0.0f, 1.0f);
	ImGui::ColorEdit3("ディゾルブ境界色", &param.dissolveEdgeColor.x);
	ImGui::DragFloat("ディゾルブ境界幅", &param.dissolveEdgeWidth, 0.001f, 0.0f, 0.25f);
	ImGui::DragFloat("ディゾルブノイズ密度", &param.dissolveNoiseScale, 1.0f, 1.0f, 400.0f);
	ImGui::DragFloat("ディゾルブノイズ速度", &param.dissolveNoiseSpeed, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat("アウトライン幅", &param.outlineWidth, 0.1f, 0.0f, 100.0f);
	ImGui::DragFloat("アウトラインしきい値", &param.outlineThreshold, 0.01f, 0.0f, 1.0f);
	ImGui::ColorEdit3("アウトライン色", &param.outlineColor.x);
	ImGui::DragFloat("アウトライン発光強度", &param.outlineBloomIntensity, 0.01f, 0.0f, 8.0f);
	ImGui::DragFloat("アウトライン発光幅", &param.outlineBloomWidth, 0.1f, 0.0f, 30.0f);
	ImGui::PopID();
#else
	(void)labelPrefix;
	(void)param;
#endif
}

nlohmann::json GameScene::BuildGamePostEffectConfig() const
{
	auto makeEntry = [](bool enabled, const ObjectPostEffect* effect) {
		nlohmann::json entry = nlohmann::json::object();
		entry["enabled"] = enabled;
		if (effect) {
			entry["param"] = WriteBloomParamJson(effect->GetParam());
		}
		return entry;
	};

	nlohmann::json config = nlohmann::json::object();
	config["version"] = 1;
	config["player"] = makeEntry(enablePlayerPostEffect_, playerPostEffect_.get());
	config["bossEnemy"] = makeEntry(enableEnemyPostEffect_, enemyPostEffect_.get());
	config["expEnemy"] = makeEntry(enableExpEnemyPostEffect_, expEnemyPostEffect_.get());
	config["stage"] = makeEntry(enableStagePostEffect_, stagePostEffect_.get());
	config["grid"] = makeEntry(enableNeonGridPostEffect_, neonGridPostEffect_.get());
	config["bulletTrail"] = makeEntry(enableBulletTrailPostEffect_, bulletTrailPostEffect_.get());
	config["particles"] = makeEntry(enableParticlePostEffect_, particlePostEffect_.get());
	config["sharedObjectBloom"] = {
		{ "param", sharedObjectBloomPostEffect_ ? WriteBloomParamJson(sharedObjectBloomPostEffect_->GetParam()) : nlohmann::json::object() }
	};
	config["expEnemyPostCull"] = {
		{ "halfWidth", expEnemyPostVisibleHalfWidth_ },
		{ "halfHeight", expEnemyPostVisibleHalfHeight_ }
	};
	config["slowMotionPlayerPost"] = {
		{ "keepPlayerColor", keepPlayerColorDuringSlow_ },
		{ "chromAbAmount", slowPlayerChromAbAmount_ },
		{ "distortionAmount", slowPlayerDistortionAmount_ },
		{ "glitchAmount", slowPlayerGlitchAmount_ }
	};
	config["deathPulse"] = {
		{ "enabled", enableDeathPostPulse_ },
		{ "duration", deathPostPulseDuration_ },
		{ "bloomBoost", deathPostBloomBoost_ },
		{ "chromAbAmount", deathPostChromAbAmount_ },
		{ "shockwaveStrength", deathPostShockwaveStrength_ },
		{ "shockwaveWidth", deathPostShockwaveWidth_ },
		{ "shockwaveMaxRadius", deathPostShockwaveMaxRadius_ }
	};
	return config;
}

void GameScene::ApplyGamePostEffectConfig(const nlohmann::json& configJson)
{
	if (!configJson.is_object()) {
		return;
	}

	auto applyEntry = [&](const char* key, bool* enabled, ObjectPostEffect* effect) {
		if (!configJson.contains(key) || !configJson[key].is_object()) {
			return;
		}
		const nlohmann::json& entry = configJson[key];
		if (enabled) {
			*enabled = ReadCustomBool(entry, "enabled", *enabled);
		}
		if (effect && entry.contains("param")) {
			BloomParam param = effect->GetParam();
			ReadBloomParamJson(entry["param"], param);
			effect->SetParam(param);
		}
	};

	applyEntry("player", &enablePlayerPostEffect_, playerPostEffect_.get());
	applyEntry("bossEnemy", &enableEnemyPostEffect_, enemyPostEffect_.get());
	applyEntry("expEnemy", &enableExpEnemyPostEffect_, expEnemyPostEffect_.get());
	applyEntry("stage", &enableStagePostEffect_, stagePostEffect_.get());
	applyEntry("grid", &enableNeonGridPostEffect_, neonGridPostEffect_.get());
	applyEntry("bulletTrail", &enableBulletTrailPostEffect_, bulletTrailPostEffect_.get());
	applyEntry("particles", &enableParticlePostEffect_, particlePostEffect_.get());
	applyEntry("sharedObjectBloom", nullptr, sharedObjectBloomPostEffect_.get());

	if (configJson.contains("expEnemyPostCull") && configJson["expEnemyPostCull"].is_object()) {
		const nlohmann::json& cullJson = configJson["expEnemyPostCull"];
		expEnemyPostVisibleHalfWidth_ = ReadCustomFloat(cullJson, "halfWidth", expEnemyPostVisibleHalfWidth_);
		expEnemyPostVisibleHalfHeight_ = ReadCustomFloat(cullJson, "halfHeight", expEnemyPostVisibleHalfHeight_);
	}
	if (configJson.contains("slowMotionPlayerPost") && configJson["slowMotionPlayerPost"].is_object()) {
		const nlohmann::json& slowJson = configJson["slowMotionPlayerPost"];
		keepPlayerColorDuringSlow_ = ReadCustomBool(slowJson, "keepPlayerColor", keepPlayerColorDuringSlow_);
		slowPlayerChromAbAmount_ = ReadCustomFloat(slowJson, "chromAbAmount", slowPlayerChromAbAmount_);
		slowPlayerDistortionAmount_ = ReadCustomFloat(slowJson, "distortionAmount", slowPlayerDistortionAmount_);
		slowPlayerGlitchAmount_ = ReadCustomFloat(slowJson, "glitchAmount", slowPlayerGlitchAmount_);
	}
	if (configJson.contains("deathPulse") && configJson["deathPulse"].is_object()) {
		const nlohmann::json& pulseJson = configJson["deathPulse"];
		enableDeathPostPulse_ = ReadCustomBool(pulseJson, "enabled", enableDeathPostPulse_);
		deathPostPulseDuration_ = ReadCustomFloat(pulseJson, "duration", deathPostPulseDuration_);
		deathPostBloomBoost_ = ReadCustomFloat(pulseJson, "bloomBoost", deathPostBloomBoost_);
		deathPostChromAbAmount_ = ReadCustomFloat(pulseJson, "chromAbAmount", deathPostChromAbAmount_);
		deathPostShockwaveStrength_ = ReadCustomFloat(pulseJson, "shockwaveStrength", deathPostShockwaveStrength_);
		deathPostShockwaveWidth_ = ReadCustomFloat(pulseJson, "shockwaveWidth", deathPostShockwaveWidth_);
		deathPostShockwaveMaxRadius_ = ReadCustomFloat(pulseJson, "shockwaveMaxRadius", deathPostShockwaveMaxRadius_);
	}

	stagePostCacheValid_ = false;
}

bool GameScene::LoadGamePostEffectConfig(const std::string& filePath)
{
	std::ifstream file(filePath);
	if (!file.is_open()) {
		postEffectConfigStatus_ = "ポスト設定ファイルが見つからないため、初期値を使用します。";
		return false;
	}

	nlohmann::json configJson;
	try {
		file >> configJson;
	} catch (...) {
		postEffectConfigStatus_ = "ポスト設定JSONの読み込みに失敗しました。";
		return false;
	}

	ApplyGamePostEffectConfig(configJson);
	postEffectConfigStatus_ = "ポスト設定を読み込みました: " + filePath;
	return true;
}

bool GameScene::SaveGamePostEffectConfig(const std::string& filePath) const
{
	std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());
	std::ofstream file(filePath);
	if (!file.is_open()) {
		return false;
	}
	file << BuildGamePostEffectConfig().dump(2);
	return true;
}

nlohmann::json GameScene::BuildGameVisualConfig() const
{
	nlohmann::json config = nlohmann::json::object();
	config["version"] = 1;
	config["neonGrid"] = {
		{ "showWorldGrid", showNeonGrid_ },
		{ "showActorLocalGrid", showActorLocalGrid_ },
		{ "worldSpacing", worldGridSpacing_ },
		{ "worldLineWidth", worldGridLineWidth_ },
		{ "worldColor", WriteJsonVector4(worldGridColor_) },
		{ "actorRadius", actorGridRadius_ },
		{ "actorSpacing", actorGridSpacing_ },
		{ "actorLineWidth", actorGridLineWidth_ },
		{ "lineSoftEdgeRatio", neonLineSoftEdgeRatio_ },
		{ "lineCoreIntensity", neonLineCoreIntensity_ },
		{ "playerColor", WriteJsonVector4(playerGridColor_) },
		{ "bossEnemyColor", WriteJsonVector4(enemyGridColor_) },
		{ "expEnemyColor", WriteJsonVector4(expEnemyGridColor_) },
		{ "showStageBlockNeonOutlines", showStageBlockNeonOutlines_ },
		{ "showStageNormalBlockBodies", showStageNormalBlockBodies_ },
		{ "stageBlockNeonLineWidth", stageBlockNeonLineWidth_ },
		{ "stageBlockNeonDepthBias", stageBlockNeonDepthBias_ },
		{ "stageBlockNeonColor", WriteJsonVector4(stageBlockNeonColor_) },
		{ "showStageDamageBlockNeonOutlines", showStageDamageBlockNeonOutlines_ },
		{ "stageDamageBlockNeonColor", WriteJsonVector4(stageDamageBlockNeonColor_) },
		{ "stageDamageBlockPulseSpeed", stageDamageBlockPulseSpeed_ },
		{ "stageDamageBlockPulseMin", stageDamageBlockPulseMin_ },
		{ "stageDamageBlockPulseMax", stageDamageBlockPulseMax_ },
		{ "cullActorLocalGrid", cullActorLocalGrid_ },
		{ "maxExpEnemyLocalGrids", maxExpEnemyLocalGrids_ },
		{ "expEnemyNeonRenderMode", expEnemyNeonRenderMode_ },
		{ "expEnemyNeonSquareSize", expEnemyNeonSquareSize_ },
		{ "expEnemyNeonTriangleRadius", expEnemyNeonTriangleRadius_ },
		{ "expEnemyNeonPentagonRadius", expEnemyNeonPentagonRadius_ },
		{ "expEnemyNeonShooterRadius", expEnemyNeonShooterRadius_ },
		{ "expEnemyNeonLineWidth", expEnemyNeonLineWidth_ },
		{ "playerNeonRenderMode", playerNeonRenderMode_ },
		{ "bossNeonRenderMode", bossNeonRenderMode_ },
		{ "playerNeonBillboardRadius", playerNeonBillboardRadius_ },
		{ "bossNeonBillboardRadius", bossNeonBillboardRadius_ },
		{ "actorNeonBillboardLineWidth", actorNeonBillboardLineWidth_ },
		{ "bossNeonBarrelForwardOffset", bossNeonBarrelForwardOffset_ },
		{ "bossNeonBarrelSideOffset", bossNeonBarrelSideOffset_ },
		{ "bossNeonBarrelLengthScale", bossNeonBarrelLengthScale_ },
		{ "bossNeonBarrelWidthScale", bossNeonBarrelWidthScale_ },
		{ "bossNeonBarrelAngleDeg", bossNeonBarrelAngleDeg_ },
		{ "fillActorNeonBodies", fillActorNeonBodies_ },
		{ "actorNeonBodyFillColor", WriteJsonVector4(actorNeonBodyFillColor_) },
		{ "playerDashCurrentAlpha", playerDashCurrentAlpha_ },
		{ "playerAfterimageAlpha", playerAfterimageAlpha_ },
		{ "playerAfterimageInterval", playerAfterimageInterval_ },
		{ "playerAfterimageLifetime", playerAfterimageLifetime_ },
		{ "showTriangleDemo", showNeonTriangleDemo_ },
		{ "triangleCenter", WriteJsonVector3(neonTriangleDemoCenter_) },
		{ "triangleRadius", neonTriangleDemoRadius_ },
		{ "triangleLineWidth", neonTriangleDemoLineWidth_ },
		{ "triangleRotateSpeed", neonTriangleDemoRotateSpeed_ },
		{ "triangleColor", WriteJsonVector4(neonTriangleDemoColor_) }
	};
	if (bulletManager_) {
		config["bulletTrail"] = WriteBulletTrailSettingsJson(bulletManager_->GetTrailSettings());
	}
	return config;
}

void GameScene::ApplyGameVisualConfig(const nlohmann::json& configJson)
{
	if (!configJson.is_object()) {
		return;
	}
	if (configJson.contains("neonGrid") && configJson["neonGrid"].is_object()) {
		const nlohmann::json& gridJson = configJson["neonGrid"];
		showNeonGrid_ = ReadCustomBool(gridJson, "showWorldGrid", showNeonGrid_);
		showActorLocalGrid_ = ReadCustomBool(gridJson, "showActorLocalGrid", showActorLocalGrid_);
		worldGridSpacing_ = ReadCustomFloat(gridJson, "worldSpacing", worldGridSpacing_);
		worldGridLineWidth_ = ReadCustomFloat(gridJson, "worldLineWidth", worldGridLineWidth_);
		if (gridJson.contains("worldColor")) worldGridColor_ = ReadJsonVector4(gridJson["worldColor"], worldGridColor_);
		actorGridRadius_ = ReadCustomFloat(gridJson, "actorRadius", actorGridRadius_);
		actorGridSpacing_ = ReadCustomFloat(gridJson, "actorSpacing", actorGridSpacing_);
		actorGridLineWidth_ = ReadCustomFloat(gridJson, "actorLineWidth", actorGridLineWidth_);
		neonLineSoftEdgeRatio_ = ReadCustomFloat(gridJson, "lineSoftEdgeRatio", neonLineSoftEdgeRatio_);
		neonLineCoreIntensity_ = ReadCustomFloat(gridJson, "lineCoreIntensity", neonLineCoreIntensity_);
		if (gridJson.contains("playerColor")) playerGridColor_ = ReadJsonVector4(gridJson["playerColor"], playerGridColor_);
		if (gridJson.contains("bossEnemyColor")) enemyGridColor_ = ReadJsonVector4(gridJson["bossEnemyColor"], enemyGridColor_);
		if (gridJson.contains("expEnemyColor")) expEnemyGridColor_ = ReadJsonVector4(gridJson["expEnemyColor"], expEnemyGridColor_);
		showStageBlockNeonOutlines_ = ReadCustomBool(gridJson, "showStageBlockNeonOutlines", showStageBlockNeonOutlines_);
		showStageNormalBlockBodies_ = ReadCustomBool(gridJson, "showStageNormalBlockBodies", showStageNormalBlockBodies_);
		stageBlockNeonLineWidth_ = ReadCustomFloat(gridJson, "stageBlockNeonLineWidth", stageBlockNeonLineWidth_);
		stageBlockNeonDepthBias_ = ReadCustomFloat(gridJson, "stageBlockNeonDepthBias", stageBlockNeonDepthBias_);
		if (gridJson.contains("stageBlockNeonColor")) stageBlockNeonColor_ = ReadJsonVector4(gridJson["stageBlockNeonColor"], stageBlockNeonColor_);
		showStageDamageBlockNeonOutlines_ = ReadCustomBool(gridJson, "showStageDamageBlockNeonOutlines", showStageDamageBlockNeonOutlines_);
		if (gridJson.contains("stageDamageBlockNeonColor")) stageDamageBlockNeonColor_ = ReadJsonVector4(gridJson["stageDamageBlockNeonColor"], stageDamageBlockNeonColor_);
		stageDamageBlockPulseSpeed_ = ReadCustomFloat(gridJson, "stageDamageBlockPulseSpeed", stageDamageBlockPulseSpeed_);
		stageDamageBlockPulseMin_ = ReadCustomFloat(gridJson, "stageDamageBlockPulseMin", stageDamageBlockPulseMin_);
		stageDamageBlockPulseMax_ = ReadCustomFloat(gridJson, "stageDamageBlockPulseMax", stageDamageBlockPulseMax_);
		if (stageDamageBlockPulseMin_ > stageDamageBlockPulseMax_) {
			std::swap(stageDamageBlockPulseMin_, stageDamageBlockPulseMax_);
		}
		cullActorLocalGrid_ = ReadCustomBool(gridJson, "cullActorLocalGrid", cullActorLocalGrid_);
		maxExpEnemyLocalGrids_ = ReadCustomInt(gridJson, "maxExpEnemyLocalGrids", maxExpEnemyLocalGrids_);
		expEnemyNeonRenderMode_ = ReadCustomInt(gridJson, "expEnemyNeonRenderMode", expEnemyNeonRenderMode_);
		if (gridJson.contains("useExpEnemyNeonBillboards") && !gridJson.contains("expEnemyNeonRenderMode")) {
			expEnemyNeonRenderMode_ = ReadCustomBool(gridJson, "useExpEnemyNeonBillboards", false) ? 1 : 0;
		}
		expEnemyNeonRenderMode_ = (std::clamp)(expEnemyNeonRenderMode_, 0, 3);
		expEnemyNeonSquareSize_ = ReadCustomFloat(gridJson, "expEnemyNeonSquareSize", expEnemyNeonSquareSize_);
		expEnemyNeonTriangleRadius_ = ReadCustomFloat(gridJson, "expEnemyNeonTriangleRadius", expEnemyNeonTriangleRadius_);
		expEnemyNeonPentagonRadius_ = ReadCustomFloat(gridJson, "expEnemyNeonPentagonRadius", expEnemyNeonPentagonRadius_);
		expEnemyNeonShooterRadius_ = ReadCustomFloat(gridJson, "expEnemyNeonShooterRadius", expEnemyNeonShooterRadius_);
		expEnemyNeonLineWidth_ = ReadCustomFloat(gridJson, "expEnemyNeonLineWidth", expEnemyNeonLineWidth_);
		playerNeonRenderMode_ = (std::clamp)(ReadCustomInt(gridJson, "playerNeonRenderMode", playerNeonRenderMode_), 0, 1);
		bossNeonRenderMode_ = (std::clamp)(ReadCustomInt(gridJson, "bossNeonRenderMode", bossNeonRenderMode_), 0, 1);
		playerNeonBillboardRadius_ = ReadCustomFloat(gridJson, "playerNeonBillboardRadius", playerNeonBillboardRadius_);
		bossNeonBillboardRadius_ = ReadCustomFloat(gridJson, "bossNeonBillboardRadius", bossNeonBillboardRadius_);
		actorNeonBillboardLineWidth_ = ReadCustomFloat(gridJson, "actorNeonBillboardLineWidth", actorNeonBillboardLineWidth_);
		bossNeonBarrelForwardOffset_ = ReadCustomFloat(gridJson, "bossNeonBarrelForwardOffset", bossNeonBarrelForwardOffset_);
		bossNeonBarrelSideOffset_ = ReadCustomFloat(gridJson, "bossNeonBarrelSideOffset", bossNeonBarrelSideOffset_);
		bossNeonBarrelLengthScale_ = ReadCustomFloat(gridJson, "bossNeonBarrelLengthScale", bossNeonBarrelLengthScale_);
		bossNeonBarrelWidthScale_ = ReadCustomFloat(gridJson, "bossNeonBarrelWidthScale", bossNeonBarrelWidthScale_);
		bossNeonBarrelAngleDeg_ = ReadCustomFloat(gridJson, "bossNeonBarrelAngleDeg", bossNeonBarrelAngleDeg_);
		fillActorNeonBodies_ = ReadCustomBool(gridJson, "fillActorNeonBodies", fillActorNeonBodies_);
		if (gridJson.contains("actorNeonBodyFillColor")) actorNeonBodyFillColor_ = ReadJsonVector4(gridJson["actorNeonBodyFillColor"], actorNeonBodyFillColor_);
		playerDashCurrentAlpha_ = ReadCustomFloat(gridJson, "playerDashCurrentAlpha", playerDashCurrentAlpha_);
		playerAfterimageAlpha_ = ReadCustomFloat(gridJson, "playerAfterimageAlpha", playerAfterimageAlpha_);
		playerAfterimageInterval_ = (std::max)(0.01f, ReadCustomFloat(gridJson, "playerAfterimageInterval", playerAfterimageInterval_));
		playerAfterimageLifetime_ = (std::max)(0.05f, ReadCustomFloat(gridJson, "playerAfterimageLifetime", playerAfterimageLifetime_));
		showNeonTriangleDemo_ = ReadCustomBool(gridJson, "showTriangleDemo", showNeonTriangleDemo_);
		if (gridJson.contains("triangleCenter")) neonTriangleDemoCenter_ = ReadJsonVector3(gridJson["triangleCenter"], neonTriangleDemoCenter_);
		neonTriangleDemoRadius_ = ReadCustomFloat(gridJson, "triangleRadius", neonTriangleDemoRadius_);
		neonTriangleDemoLineWidth_ = ReadCustomFloat(gridJson, "triangleLineWidth", neonTriangleDemoLineWidth_);
		neonTriangleDemoRotateSpeed_ = ReadCustomFloat(gridJson, "triangleRotateSpeed", neonTriangleDemoRotateSpeed_);
		if (gridJson.contains("triangleColor")) neonTriangleDemoColor_ = ReadJsonVector4(gridJson["triangleColor"], neonTriangleDemoColor_);
	}
	if (bulletManager_ && configJson.contains("bulletTrail")) {
		ReadBulletTrailSettingsJson(configJson["bulletTrail"], bulletManager_->GetTrailSettings());
	}
}

bool GameScene::LoadGameVisualConfig(const std::string& filePath)
{
	std::ifstream file(filePath);
	if (!file.is_open()) {
		visualConfigStatus_ = "見た目設定ファイルが見つからないため、初期値を使用します。";
		return false;
	}

	nlohmann::json configJson;
	try {
		file >> configJson;
	} catch (...) {
		visualConfigStatus_ = "見た目設定JSONの読み込みに失敗しました。";
		return false;
	}

	ApplyGameVisualConfig(configJson);
	visualConfigStatus_ = "見た目設定を読み込みました: " + filePath;
	return true;
}

bool GameScene::SaveGameVisualConfig(const std::string& filePath) const
{
	std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());
	std::ofstream file(filePath);
	if (!file.is_open()) {
		return false;
	}
	file << BuildGameVisualConfig().dump(2);
	return true;
}

void GameScene::DrawGameSceneDebugImGui()
{
#ifdef USE_IMGUI
	if (!showGameDebugConsole_) {
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(620.0f, 540.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("ゲームデバッグコンソール", &showGameDebugConsole_)) {
		ImGui::End();
		return;
	}

	if (ImGui::BeginTabBar("GameDebugTabs")) {
		if (ImGui::BeginTabItem("概要")) {
			ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);
			ImGui::Text("デルタタイム: %.8f", finalDeltaTime * 60.0f);
			ImGui::Text("経験値敵の数: %zu", enemyManager_ ? enemyManager_->GetEnemyCount() : 0);
			ImGui::Text("弾軌跡インスタンス数: %zu", bulletManager_ ? bulletManager_->GetTrailInstanceCount() : 0);
			if (player_) {
				const auto& hud = player_->GetUpgradeHudProfileStats();
				const auto& evo = player_->GetEvolutionUiProfileStats();
				ImGui::Separator();
				ImGui::Text("強化HUD: %s total %.3fms / update %.3f / sprite %.3f (%d) / text %.3f (%d)",
					hud.visible ? "表示" : "非表示",
					hud.totalMs, hud.updateMs, hud.spriteMs, hud.spriteDraws, hud.textMs, hud.textDraws);
				ImGui::Text("進化UI: %s total %.3fms / update %.3f / sprite %.3f (%d) / text %.3f (%d)",
					evo.visible ? "表示" : "非表示",
					evo.totalMs, evo.updateMs, evo.spriteMs, evo.spriteDraws, evo.textMs, evo.textDraws);
			}
			ImGui::SeparatorText("処理時間比較");
			DrawPerformanceBreakdownImGui();
			ImGui::Separator();
			ImGui::Checkbox("追従HPバーを表示", &showFollowHpBars_);
			ImGui::Checkbox("当たり判定を表示 (F7)", &showCollisionDebug_);
			ImGui::Checkbox("弾の当たり判定も表示", &showCollisionDebugBullets_);
			ImGui::Checkbox("弾HP/貫通力ラベルを表示", &showBulletStatusDebugOverlay_);
			ImGui::Checkbox("弾HP/貫通力テーブルを表示", &showBulletStatusDebugTable_);
			ImGui::DragInt("弾ラベル最大数", &bulletStatusDebugMaxLabels_, 1.0f, 1, 200);
			ImGui::Checkbox("ポスト負荷表示を表示 (F8)", &showPostProfileOverlay_);
			ImGui::Text("F12: このコンソールを表示/非表示");
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("ゲームプレイ")) {
			ImGui::Text("レベル: %d", player_->GetLevel());
			ImGui::Text("経験値: %d / %d", player_->GetExp(), player_->GetNextLevelExpValue());
			ImGui::Text("スキルポイント: %d", player_->GetSkillPoints());
			ImGui::Text("機体クラス: %s", player_->GetCurrentClassName());
			ImGui::Checkbox("デバッグ無敵: プレイヤーHPを減らさない", &debugPlayerNoDamage_);
			ImGui::Separator();
			ImGui::Text("1 自然回復       Lv.%d", player_->GetUpgradeLevel(0));
			ImGui::Text("2 最大HP         Lv.%d", player_->GetUpgradeLevel(1));
			ImGui::Text("3 体当たりダメージ Lv.%d", player_->GetUpgradeLevel(2));
			ImGui::Text("4 弾速           Lv.%d", player_->GetUpgradeLevel(3));
			ImGui::Text("5 弾ダメージ     Lv.%d", player_->GetUpgradeLevel(4));
			ImGui::Text("6 リロード       Lv.%d", player_->GetUpgradeLevel(5));
			ImGui::Text("7 移動速度       Lv.%d", player_->GetUpgradeLevel(6));
			ImGui::Separator();
			if (ImGui::Button("+ 次レベル分の経験値")) {
				player_->AddExp(player_->GetNextLevelExpValue());
			}
			ImGui::SameLine();
			if (ImGui::Button("+200 経験値")) {
				player_->AddExp(200);
			}
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("見た目")) {
			if (ImGui::Button("見た目設定を保存")) {
				if (SaveGameVisualConfig()) {
					visualConfigStatus_ = "見た目設定を保存しました: resources/configs/gameVisuals.json";
				} else {
					visualConfigStatus_ = "見た目設定の保存に失敗しました。";
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("見た目設定を再読み込み")) {
				LoadGameVisualConfig();
			}
			if (!visualConfigStatus_.empty()) {
				ImGui::TextWrapped("%s", visualConfigStatus_.c_str());
			}
			ImGui::Separator();
			if (player_) {
				player_->DrawUpgradeHudDebugImGui();
				ImGui::Separator();
			}
			if (ImGui::CollapsingHeader("ネオングリッド", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Checkbox("背景グリッドを表示", &showNeonGrid_);
				ImGui::Checkbox("キャラ周辺グリッドを表示", &showActorLocalGrid_);
				ImGui::Checkbox("グリッドにポストエフェクト", &enableNeonGridPostEffect_);
				ImGui::DragFloat("背景グリッド間隔", &worldGridSpacing_, 0.05f, 0.25f, 8.0f);
				ImGui::DragFloat("背景グリッド線幅", &worldGridLineWidth_, 0.005f, 0.005f, 0.5f);
				ImGui::ColorEdit4("背景グリッド色", &worldGridColor_.x);
				ImGui::DragFloat("キャラ周辺グリッド半径", &actorGridRadius_, 0.05f, 0.5f, 16.0f);
				ImGui::DragFloat("キャラ周辺グリッド間隔", &actorGridSpacing_, 0.025f, 0.2f, 3.0f);
				ImGui::DragFloat("キャラ周辺グリッド線幅", &actorGridLineWidth_, 0.005f, 0.005f, 0.5f);
				ImGui::DragFloat("ネオンライン外縁フェード", &neonLineSoftEdgeRatio_, 0.01f, 0.0f, 0.95f);
				ImGui::DragFloat("ネオンライン芯の明るさ", &neonLineCoreIntensity_, 0.01f, 0.0f, 3.0f);
				ImGui::ColorEdit4("プレイヤーグリッド色", &playerGridColor_.x);
				ImGui::ColorEdit4("ボスグリッド色", &enemyGridColor_.x);
				ImGui::ColorEdit4("経験値敵グリッド色", &expEnemyGridColor_.x);
				ImGui::Checkbox("画面外の周辺グリッドを省略", &cullActorLocalGrid_);
				ImGui::DragInt("経験値敵グリッド最大数", &maxExpEnemyLocalGrids_, 1.0f, 0, 60);
				ImGui::SeparatorText("ステージブロック枠線");
				ImGui::Checkbox("通常ブロックに3Dネオン枠線", &showStageBlockNeonOutlines_);
				ImGui::Checkbox("通常ブロック本体を表示", &showStageNormalBlockBodies_);
				ImGui::DragFloat("共通ブロック枠線幅", &stageBlockNeonLineWidth_, 0.005f, 0.005f, 0.5f);
				ImGui::DragFloat("共通ブロック枠線の深度オフセット", &stageBlockNeonDepthBias_, 0.001f, 0.0f, 0.2f);
				ImGui::ColorEdit4("通常ブロック枠線色", &stageBlockNeonColor_.x);
				ImGui::Checkbox("ダメージブロックに3Dネオン枠線", &showStageDamageBlockNeonOutlines_);
				ImGui::ColorEdit4("ダメージブロック枠線色", &stageDamageBlockNeonColor_.x);
				ImGui::DragFloat("ダメージブロック点滅速度", &stageDamageBlockPulseSpeed_, 0.1f, 0.0f, 30.0f);
				ImGui::DragFloatRange2(
					"ダメージブロック点滅輝度",
					&stageDamageBlockPulseMin_,
					&stageDamageBlockPulseMax_,
					0.01f,
					0.0f,
					4.0f,
					"最小 %.2f",
					"最大 %.2f");
				ImGui::SeparatorText("経験値敵ネオン表示");
				const char* expEnemyNeonModes[] = { "3Dモデル", "ビルボード枠線", "3D枠線", "黒塗り+3D枠線" };
				ImGui::Combo("経験値敵の表示", &expEnemyNeonRenderMode_, expEnemyNeonModes, IM_ARRAYSIZE(expEnemyNeonModes));
				ImGui::DragFloat("四角敵ネオンサイズ", &expEnemyNeonSquareSize_, 0.025f, 0.2f, 4.0f);
				ImGui::DragFloat("三角敵ネオン半径", &expEnemyNeonTriangleRadius_, 0.025f, 0.2f, 4.0f);
				ImGui::DragFloat("五角敵ネオン半径", &expEnemyNeonPentagonRadius_, 0.025f, 0.2f, 4.0f);
				ImGui::DragFloat("射撃敵ネオン半径", &expEnemyNeonShooterRadius_, 0.025f, 0.2f, 4.0f);
				ImGui::DragFloat("敵ネオン線幅", &expEnemyNeonLineWidth_, 0.005f, 0.01f, 0.5f);
				ImGui::SeparatorText("プレイヤー / ボス ネオン表示");
				const char* actorNeonModes[] = { "3Dモデル", "ビルボード枠線" };
				ImGui::Combo("プレイヤー表示", &playerNeonRenderMode_, actorNeonModes, IM_ARRAYSIZE(actorNeonModes));
				ImGui::Combo("ボス表示", &bossNeonRenderMode_, actorNeonModes, IM_ARRAYSIZE(actorNeonModes));
				ImGui::DragFloat("プレイヤー板ポリ半径", &playerNeonBillboardRadius_, 0.025f, 0.2f, 4.0f);
				ImGui::DragFloat("ボス板ポリ半径", &bossNeonBillboardRadius_, 0.025f, 0.2f, 5.0f);
				ImGui::DragFloat("プレイヤー/ボス線幅", &actorNeonBillboardLineWidth_, 0.005f, 0.01f, 0.5f);
				ImGui::SeparatorText("ボス砲塔位置");
				ImGui::DragFloat("ボス砲塔 前後位置", &bossNeonBarrelForwardOffset_, 0.01f, -2.0f, 3.0f);
				ImGui::DragFloat("ボス砲塔 横位置", &bossNeonBarrelSideOffset_, 0.01f, -2.0f, 2.0f);
				ImGui::DragFloat("ボス砲塔 長さ", &bossNeonBarrelLengthScale_, 0.01f, 0.1f, 4.0f);
				ImGui::DragFloat("ボス砲塔 太さ", &bossNeonBarrelWidthScale_, 0.01f, 0.05f, 2.0f);
				ImGui::DragFloat("ボス砲塔 角度補正", &bossNeonBarrelAngleDeg_, 0.25f, -180.0f, 180.0f);
				ImGui::SeparatorText("板ポリ本体の塗り");
				ImGui::Checkbox("プレイヤー/ボス本体を暗く塗る", &fillActorNeonBodies_);
				ImGui::ColorEdit4("本体の塗り色", &actorNeonBodyFillColor_.x, ImGuiColorEditFlags_Float);
				ImGui::SliderFloat("ダッシュ中の本体濃度", &playerDashCurrentAlpha_, 0.05f, 1.0f);
				ImGui::SliderFloat("残像の濃度", &playerAfterimageAlpha_, 0.05f, 1.0f);
				ImGui::DragFloat("残像の生成間隔", &playerAfterimageInterval_, 0.005f, 0.01f, 0.25f);
				ImGui::DragFloat("残像の寿命", &playerAfterimageLifetime_, 0.01f, 0.05f, 1.0f);
				ImGui::SeparatorText("ネオン三角形デモ");
				ImGui::Checkbox("ネオン三角形デモを表示", &showNeonTriangleDemo_);
				ImGui::DragFloat3("ネオン三角形 位置", &neonTriangleDemoCenter_.x, 0.05f);
				ImGui::DragFloat("ネオン三角形 半径", &neonTriangleDemoRadius_, 0.05f, 0.1f, 12.0f);
				ImGui::DragFloat("ネオン三角形 線幅", &neonTriangleDemoLineWidth_, 0.005f, 0.005f, 1.0f);
				ImGui::DragFloat("ネオン三角形 回転速度", &neonTriangleDemoRotateSpeed_, 0.01f, -5.0f, 5.0f);
				ImGui::ColorEdit4("ネオン三角形 色", &neonTriangleDemoColor_.x);
			}
			if (ImGui::CollapsingHeader("弾の軌跡", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Checkbox("弾軌跡にポストエフェクト", &enableBulletTrailPostEffect_);
				BulletTrailSettings& bulletTrail = bulletManager_->GetTrailSettings();
				ImGui::DragFloat("プレイヤー軌跡半幅", &bulletTrail.playerHalfWidth, 0.01f, 0.01f, 1.5f);
				ImGui::DragFloat("敵軌跡半幅", &bulletTrail.enemyHalfWidth, 0.01f, 0.01f, 1.5f);
				ImGui::DragFloat("軌跡の寿命", &bulletTrail.lifetime, 0.01f, 0.02f, 1.5f);
				ImGui::DragInt("軌跡の最大点数", &bulletTrail.maxPoints, 1.0f, 2, 80);
				ImGui::DragInt("軌跡の補間数", &bulletTrail.interpolationSteps, 1.0f, 1, 12);
				ImGui::DragFloat("先端の太さ倍率", &bulletTrail.headWidthScale, 0.01f, 0.0f, 4.0f);
				ImGui::DragFloat("末端の太さ倍率", &bulletTrail.tailWidthScale, 0.01f, 0.0f, 4.0f);
				ImGui::DragFloat("太さの減衰カーブ", &bulletTrail.widthCurvePower, 0.01f, 0.05f, 6.0f);
				ImGui::DragFloat("色の減衰カーブ", &bulletTrail.colorCurvePower, 0.01f, 0.05f, 6.0f);
				ImGui::Checkbox("弾の色を軌跡に使う", &bulletTrail.useObjectColorForTrail);
				ImGui::ColorEdit4("プレイヤー弾色", &bulletTrail.playerObjectColor.x);
				ImGui::ColorEdit4("敵弾色", &bulletTrail.enemyObjectColor.x);
				ImGui::ColorEdit4("反射弾色", &bulletTrail.reflectableObjectColor.x);
				if (bulletTrail.useObjectColorForTrail) {
					ImGui::DragFloat("軌跡先端の発光倍率", &bulletTrail.trailHeadIntensity, 0.01f, 0.0f, 5.0f);
					ImGui::DragFloat("軌跡末端の発光倍率", &bulletTrail.trailTailIntensity, 0.01f, 0.0f, 5.0f);
					ImGui::DragFloat("軌跡先端の透明度", &bulletTrail.trailHeadAlpha, 0.01f, 0.0f, 1.0f);
					ImGui::DragFloat("軌跡末端の透明度", &bulletTrail.trailTailAlpha, 0.01f, 0.0f, 1.0f);
				} else {
					ImGui::ColorEdit4("軌跡開始色", &bulletTrail.startColor.x);
					ImGui::ColorEdit4("プレイヤー軌跡終了色", &bulletTrail.playerEndColor.x);
					ImGui::ColorEdit4("敵軌跡終了色", &bulletTrail.enemyEndColor.x);
					ImGui::ColorEdit4("反射弾軌跡終了色", &bulletTrail.reflectableEndColor.x);
				}
			}
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("ポスト")) {
			if (ImGui::Button("ポスト設定を保存")) {
				if (SaveGamePostEffectConfig()) {
					postEffectConfigStatus_ = "ポスト設定を保存しました: resources/configs/gamePostEffects.json";
				} else {
					postEffectConfigStatus_ = "ポスト設定の保存に失敗しました。";
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("ポスト設定を再読み込み")) {
				LoadGamePostEffectConfig();
			}
			if (!postEffectConfigStatus_.empty()) {
				ImGui::TextWrapped("%s", postEffectConfigStatus_.c_str());
			}
			ImGui::Separator();
			if (ImGui::Button("全ポスト有効")) {
				enableNeonGridPostEffect_ = true;
				enableStagePostEffect_ = true;
				enableBulletTrailPostEffect_ = true;
				enableParticlePostEffect_ = true;
				enablePlayerPostEffect_ = true;
				enableEnemyPostEffect_ = true;
				enableExpEnemyPostEffect_ = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("全ポスト無効")) {
				enableNeonGridPostEffect_ = false;
				enableStagePostEffect_ = false;
				enableBulletTrailPostEffect_ = false;
				enableParticlePostEffect_ = false;
				enablePlayerPostEffect_ = false;
				enableEnemyPostEffect_ = false;
				enableExpEnemyPostEffect_ = false;
			}
			if (ImGui::CollapsingHeader("プレイヤー", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Checkbox("プレイヤーポストを有効", &enablePlayerPostEffect_);
				DrawPostEffectParamControls("Player", playerPostEffect_->GetParam());
			}
			if (ImGui::CollapsingHeader("ボス敵")) {
				ImGui::Checkbox("ボス敵ポストを有効", &enableEnemyPostEffect_);
				DrawPostEffectParamControls("Enemy", enemyPostEffect_->GetParam());
			}
			if (ImGui::CollapsingHeader("経験値敵")) {
				ImGui::Checkbox("経験値敵ポストを有効", &enableExpEnemyPostEffect_);
				DrawPostEffectParamControls("ExpEnemy", expEnemyPostEffect_->GetParam());
				ImGui::DragFloat("ポスト省略範囲 半幅", &expEnemyPostVisibleHalfWidth_, 0.5f, 10.0f, 80.0f);
				ImGui::DragFloat("ポスト省略範囲 半高さ", &expEnemyPostVisibleHalfHeight_, 0.5f, 10.0f, 60.0f);
			}
			if (ImGui::CollapsingHeader("ステージ")) {
				ImGui::Checkbox("ステージポストを有効", &enableStagePostEffect_);
				DrawPostEffectParamControls("Stage", stagePostEffect_->GetParam());
				ImGui::Text("ステージは軽量化のためブルーム加算のみを使います。");
			}
			if (ImGui::CollapsingHeader("共有オブジェクト発光")) {
				DrawPostEffectParamControls("SharedObjectBloom", sharedObjectBloomPostEffect_->GetParam());
				ImGui::TextWrapped("プレイヤー、敵、経験値敵、ステージなどの単体発光をまとめる共通パスです。");
			}
			if (ImGui::CollapsingHeader("グリッド / 弾軌跡 / パーティクル")) {
				ImGui::Checkbox("グリッドポストを有効", &enableNeonGridPostEffect_);
				BloomParam& gridPost = neonGridPostEffect_->GetParam();
				ImGui::DragFloat("グリッド発光強度", &gridPost.intensity, 0.01f, 0.0f, 6.0f);
				ImGui::DragFloat("グリッド発光しきい値", &gridPost.threshold, 0.01f, 0.0f, 2.0f);
				ImGui::Checkbox("弾軌跡ポストを有効", &enableBulletTrailPostEffect_);
				BloomParam& bulletTrailPost = bulletTrailPostEffect_->GetParam();
				ImGui::DragFloat("弾軌跡発光強度", &bulletTrailPost.intensity, 0.01f, 0.0f, 8.0f);
				ImGui::DragFloat("弾軌跡発光しきい値", &bulletTrailPost.threshold, 0.01f, 0.0f, 2.0f);
				ImGui::Checkbox("三角パーティクルポストを有効", &enableParticlePostEffect_);
				BloomParam& particlePost = particlePostEffect_->GetParam();
				ImGui::DragFloat("三角パーティクル発光強度", &particlePost.intensity, 0.01f, 0.0f, 8.0f);
				ImGui::DragFloat("三角パーティクル発光しきい値", &particlePost.threshold, 0.01f, 0.0f, 2.0f);
			}
			if (ImGui::CollapsingHeader("死亡チャージ / 衝撃波")) {
				ImGui::Checkbox("死亡時の画面衝撃波を有効", &enableDeathPostPulse_);
				ImGui::DragFloat("衝撃波の時間", &deathPostPulseDuration_, 0.01f, 0.1f, 2.0f);
				ImGui::DragFloat("死亡時ブルーム増幅", &deathPostBloomBoost_, 0.05f, 0.0f, 6.0f);
				ImGui::DragFloat("死亡時色収差", &deathPostChromAbAmount_, 0.001f, 0.0f, 0.2f);
				ImGui::DragFloat("衝撃波の歪み", &deathPostShockwaveStrength_, 0.001f, 0.0f, 0.15f);
				ImGui::DragFloat("衝撃波リング幅", &deathPostShockwaveWidth_, 0.001f, 0.005f, 0.25f);
				ImGui::DragFloat("衝撃波の到達半径", &deathPostShockwaveMaxRadius_, 0.01f, 0.1f, 1.5f);
			}
			if (ImGui::CollapsingHeader("スローモーション")) {
				ImGui::Text("スロー中: %s", slowMotionPostActive_ ? "はい" : "いいえ");
				ImGui::Checkbox("スロー中もプレイヤー色を維持", &keepPlayerColorDuringSlow_);
				ImGui::DragFloat("スロー時 プレイヤー色収差", &slowPlayerChromAbAmount_, 0.001f, 0.0f, 0.2f);
				ImGui::DragFloat("スロー時 プレイヤー歪み", &slowPlayerDistortionAmount_, 0.001f, 0.0f, 0.2f);
				ImGui::DragFloat("スロー時 プレイヤーグリッチ", &slowPlayerGlitchAmount_, 0.001f, 0.0f, 0.2f);
			}
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("ツール")) {
			ImGui::Checkbox("パーティクルエディタを開く", &showParticleEditor_);
			ImGui::Checkbox("プレイヤー機体エディタを開く", &showPlayerClassEditor_);
			ImGui::Separator();
			ImGui::Text("弾耐久デバッグ");
			ImGui::Checkbox("画面上ラベル", &showBulletStatusDebugOverlay_);
			ImGui::Checkbox("ライブ表", &showBulletStatusDebugTable_);
			ImGui::DragInt("画面上ラベル最大数", &bulletStatusDebugMaxLabels_, 1.0f, 1, 200);
			DrawBulletStatusDebugTable();
			ImGui::Separator();
			if (shotGide) {
				ImGui::SliderFloat2("射撃ガイド位置", &shotGide->GetPosition().x, 0.0f, 3000.0f, "%.1f");
			}
			if (ball_) {
				ImGui::DragFloat3("デバッグ球スケール", &ball_->GetScale().x);
				ImGui::ColorEdit4("デバッグ球色", &ball_->GetColor().x);
			}
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("バランス")) {
			DrawLevelAIDitorBalanceLab(true);
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
#endif
}

void GameScene::DrawBulletStatusDebugTable()
{
#ifdef USE_IMGUI
	if (!showBulletStatusDebugTable_ || !bulletManager_) {
		return;
	}

	const std::vector<Bullet*> bullets = bulletManager_->GetBulletPtrs();
	ImGui::Text("現在の弾数: %zu", bullets.size());
	if (!ImGui::BeginTable("BulletStatusDebugTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 180.0f))) {
		return;
	}

	ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 36.0f);
	ImGui::TableSetupColumn("所属", ImGuiTableColumnFlags_WidthFixed, 74.0f);
	ImGui::TableSetupColumn("ダメージ", ImGuiTableColumnFlags_WidthFixed, 78.0f);
	ImGui::TableSetupColumn("弾HP", ImGuiTableColumnFlags_WidthFixed, 82.0f);
	ImGui::TableSetupColumn("貫通力", ImGuiTableColumnFlags_WidthFixed, 74.0f);
	ImGui::TableSetupColumn("位置", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("速度", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableHeadersRow();

	for (size_t i = 0; i < bullets.size(); ++i) {
		Bullet* bullet = bullets[i];
		if (!bullet) {
			continue;
		}
		const Vector3 pos = bullet->GetWorldPosition();
		const Vector3 vel = bullet->GetMove();

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("%zu", i);
		ImGui::TableSetColumnIndex(1);
		ImGui::TextColored(
			bullet->GetOwner() == kPlayer ? ImVec4(1.0f, 0.92f, 0.28f, 1.0f) : ImVec4(1.0f, 0.36f, 0.48f, 1.0f),
			"%s",
			BulletOwnerName(bullet->GetOwner()));
		ImGui::TableSetColumnIndex(2);
		ImGui::Text("%u", bullet->GetDamage());
		ImGui::TableSetColumnIndex(3);
		ImGui::Text("%.1f", bullet->GetBulletHp());
		ImGui::TableSetColumnIndex(4);
		ImGui::Text("%.1f", bullet->GetBulletPenetration());
		ImGui::TableSetColumnIndex(5);
		ImGui::Text("%.1f, %.1f", pos.x, pos.y);
		ImGui::TableSetColumnIndex(6);
		ImGui::Text("%.2f, %.2f", vel.x, vel.y);
	}

	ImGui::EndTable();
#endif
}

void GameScene::DrawBulletStatusDebugOverlay()
{
#ifdef USE_IMGUI
	if (!showBulletStatusDebugOverlay_ || !bulletManager_) {
		return;
	}

	const std::vector<Bullet*> bullets = bulletManager_->GetBulletPtrs();
	ImDrawList* drawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
	const int maxLabels = (std::max)(1, bulletStatusDebugMaxLabels_);
	int drawn = 0;
	for (Bullet* bullet : bullets) {
		if (!bullet || bullet->IsDead()) {
			continue;
		}
		if (drawn >= maxLabels) {
			break;
		}

		const Vector3 worldPos = bullet->GetWorldPosition();
		const Vector2 screen = WorldToScreen(worldPos + Vector3{ 0.0f, 0.85f, 0.0f });
		if (screen.x < -80.0f || screen.x > WinApp::kClientWidth + 80.0f ||
			screen.y < -40.0f || screen.y > WinApp::kClientHeight + 40.0f) {
			continue;
		}

		char text[64]{};
		std::snprintf(
			text,
			sizeof(text),
			"%c HP %.1f  PEN %.1f",
			bullet->GetOwner() == kPlayer ? 'P' : (bullet->GetOwner() == kExpEnemyHostile ? 'X' : 'E'),
			bullet->GetBulletHp(),
			bullet->GetBulletPenetration());

		const ImVec2 textPos(screen.x + 8.0f, screen.y - 10.0f);
		drawList->AddText(ImVec2(textPos.x + 1.0f, textPos.y + 1.0f), IM_COL32(0, 0, 0, 220), text);
		drawList->AddText(textPos, BulletOwnerDebugColor(bullet->GetOwner()), text);
		++drawn;
	}
#else
	(void)this;
#endif
}

void GameScene::DrawLevelAIDitorBalanceLab(bool embedded)
{
#ifdef USE_IMGUI
	if (!balanceEditor_.initialized && currentLevelData_.balance.is_object()) {
		LoadBalanceEditorFromJson(currentLevelData_.balance);
	}

	if (!embedded) {
		ImGui::Begin("レベルバランス調整ラボ");
	}
	ImGui::Text("実行中のバランスを調整し、必要なら level_test.json に保存します。");
	ImGui::Checkbox("通常ランダムスポーン有効", &balanceEditor_.defaultRandomSpawnEnabled);
	ImGui::Separator();

	if (ImGui::CollapsingHeader("プレイヤー", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragInt("最大HP", &balanceEditor_.playerMaxHp, 10.0f, 1, 9999);
		ImGui::DragFloat("リロード速度 基礎値", &balanceEditor_.playerReloadSpeed, 0.05f, 0.05f, 60.0f);
		ImGui::DragFloat("弾ダメージ 基礎値", &balanceEditor_.playerBulletDamage, 0.1f, 0.1f, 999.0f);
		ImGui::DragFloat("弾速 基礎値", &balanceEditor_.playerBulletSpeed, 0.01f, 0.01f, 5.0f);
		ImGui::DragFloat("移動速度 基礎値", &balanceEditor_.playerMoveSpeed, 0.005f, 0.01f, 5.0f);
		ImGui::DragFloat("自然回復 基礎値", &balanceEditor_.playerStaminaRecovery, 0.01f, 0.0f, 20.0f);
		ImGui::DragFloat("最大スタミナ 基礎値", &balanceEditor_.playerMaxStamina, 0.1f, 0.0f, 20.0f);
		ImGui::DragInt("体当たりダメージ", &balanceEditor_.playerBodyDamage, 1.0f, 1, 999);
		ImGui::Checkbox("適用時に全回復", &balanceEditor_.healToFull);
		if (player_) {
			ImGui::Text("現在HP: %d / %d", player_->GetHp(), player_->GetMaxHp());
			const Player::PlayerStats& stats = player_->GetStats();
			ImGui::Text("現在値: リロード %.2f / 弾ダメ %.2f / 弾速 %.3f / 移動 %.3f / 回復 %.2f",
				stats.reloadSpeed, stats.bulletDamage, stats.bulletSpeed, stats.moveSpeed, stats.staminaRecovery);
		}
	}

	if (ImGui::CollapsingHeader("プレイヤー強化幅", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::TextWrapped("1回強化したときの倍率です。0.10なら10%%/Lvです。リロードだけは数値が小さいほど速いため、この割合ぶん短縮されます。");
		ImGui::DragFloat("自然回復 倍率/Lv", &balanceEditor_.playerHealthRegenUpgrade, 0.005f, 0.0f, 2.0f);
		ImGui::DragFloat("最大HP 倍率/Lv", &balanceEditor_.maxHpUpgradeAmount, 0.005f, 0.0f, 2.0f);
		ImGui::DragFloat("体当たり倍率/Lv", &balanceEditor_.playerBodyDamageUpgrade, 0.005f, 0.0f, 2.0f);
		ImGui::DragFloat("弾速倍率/Lv", &balanceEditor_.playerBulletSpeedUpgrade, 0.005f, -0.95f, 2.0f);
		ImGui::DragFloat("弾ダメージ倍率/Lv", &balanceEditor_.playerBulletDamageUpgrade, 0.005f, 0.0f, 2.0f);
		ImGui::DragFloat("リロード短縮率/Lv", &balanceEditor_.playerReloadUpgrade, 0.005f, 0.0f, 0.95f);
		ImGui::DragFloat("リロード最小値", &balanceEditor_.playerMinReloadSpeed, 0.05f, 0.05f, 60.0f);
		ImGui::DragFloat("移動速度倍率/Lv", &balanceEditor_.playerMoveSpeedUpgrade, 0.005f, -0.95f, 2.0f);
	}

	if (ImGui::CollapsingHeader("接触/被弾ダメージ", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragInt("ダメージブロック", &balanceEditor_.damageBlock, 1.0f, 1, 999);
		ImGui::DragInt("ボス接触", &balanceEditor_.bossContact, 1.0f, 1, 999);
		ImGui::DragInt("経験値敵接触", &balanceEditor_.expEnemyContact, 1.0f, 1, 999);
		ImGui::DragInt("射撃敵接触", &balanceEditor_.shooterContact, 1.0f, 1, 999);
		ImGui::DragInt("射撃敵の弾", &balanceEditor_.shooterBullet, 1.0f, 1, 999);
		ImGui::DragFloat("射撃敵の探知半径", &balanceEditor_.shooterDetectionRadius, 0.25f, 1.0f, 100.0f);
		ImGui::DragFloat("射撃敵の砲塔旋回速度", &balanceEditor_.shooterTurnSpeed, 0.1f, 0.1f, 30.0f);
		ImGui::DragFloat("射撃敵の発射間隔", &balanceEditor_.shooterFireInterval, 0.05f, 0.1f, 10.0f);
		ImGui::DragFloat("射撃敵の弾速", &balanceEditor_.shooterBulletSpeed, 0.01f, 0.01f, 2.0f);
	}

	if (ImGui::CollapsingHeader("ボス通常攻撃", ImGuiTreeNodeFlags_DefaultOpen)) {
		const char* patternNames[] = { "拡散", "リング", "狙撃", "交互射撃" };
		ImGui::Combo("攻撃パターン", &balanceEditor_.bossAttackPattern, patternNames, IM_ARRAYSIZE(patternNames));
		ImGui::DragFloat("弾速", &balanceEditor_.bossBulletSpeed, 0.01f, 0.01f, 2.0f);
		ImGui::DragInt("弾数", &balanceEditor_.bossBulletCount, 1.0f, 1, 64);
		ImGui::DragFloat("拡散角度", &balanceEditor_.bossSpreadAngleDeg, 1.0f, 0.0f, 360.0f);
		ImGui::DragFloat("クールタイム", &balanceEditor_.bossCooldown, 0.01f, 0.05f, 5.0f);
		ImGui::DragInt("弾ダメージ", &balanceEditor_.bossBulletDamage, 1.0f, 1, 999);
		ImGui::DragFloat("弾HP (0=ダメージ値)", &balanceEditor_.bossBulletHp, 0.1f, 0.0f, 999.0f);
		ImGui::DragFloat("弾貫通力 (0=ダメージ値)", &balanceEditor_.bossBulletPenetration, 0.1f, 0.0f, 999.0f);
		ImGui::Checkbox("ランダム拡散", &balanceEditor_.bossRandomSpread);
		if (enemy_) {
			ImGui::Text("ボスレベル: %d", enemy_->GetLevel());
			ImGui::Text("ボス経験値: %u", enemy_->GetEnemyExp());
		}
	}

	if (ImGui::CollapsingHeader("敵RPG / 陣営", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("経験値敵をボスと敵対させる", &balanceEditor_.expEnemyHostileToBoss);
		ImGui::DragInt("ボスから経験値敵へのダメージ", &balanceEditor_.bossExpEnemyDamage, 1.0f, 1, 999);
		ImGui::DragInt("ボスが経験値敵撃破で回復", &balanceEditor_.bossHealOnExpEnemyKill, 1.0f, 0, 999);
		ImGui::DragInt("ボスの必要撃破数/レベル", &balanceEditor_.bossKillsPerLevel, 1.0f, 1, 99);
		ImGui::DragInt("ボス最大HP増加/レベル", &balanceEditor_.bossMaxHpGainPerLevel, 1.0f, 0, 999);
		ImGui::DragInt("ボス攻撃力増加/レベル", &balanceEditor_.bossDamageGainPerLevel, 1.0f, 0, 99);
		ImGui::Checkbox("ボスのレベリング狩りモード", &balanceEditor_.bossLevelingModeEnabled);
		ImGui::DragFloat("狩り開始 プレイヤー距離", &balanceEditor_.bossLevelingEnterDistance, 0.5f, 1.0f, 120.0f);
		ImGui::DragFloat("狩り終了 プレイヤー距離", &balanceEditor_.bossLevelingExitDistance, 0.5f, 0.5f, 120.0f);
		ImGui::DragFloat("狩り探索半径", &balanceEditor_.bossLevelingSearchRadius, 1.0f, 1.0f, 200.0f);
		ImGui::DragFloat("180度旋回にかかる秒数", &balanceEditor_.bossAimTurnHalfSeconds, 0.05f, 0.05f, 5.0f);
		if (enemy_) {
			ImGui::Text("現在のボスモード: %s", enemy_->IsLevelingModeActive() ? "レベリング狩り" : "プレイヤー圧力");
		}
		ImGui::TextWrapped("有効にすると、ボスと経験値敵が敵対陣営として衝突します。ボスは経験値敵を倒すと回復し、レベルアップします。");
	}

	ImGui::Separator();
	if (ImGui::Button("実行中ゲームへ適用")) {
		currentLevelData_.balance = BuildBalanceJsonFromEditor();
		enemyManager_->SetDefaultRandomSpawnEnabled(balanceEditor_.defaultRandomSpawnEnabled);
		ApplyLevelBalance(currentLevelData_.balance);
		balanceEditor_.statusMessage = "Applied balance to running game.";
	}
	ImGui::SameLine();
	if (ImGui::Button("level_test.jsonへ保存")) {
		SaveBalanceEditorToLevelFile("resources/levels/level_test.json");
	}
	ImGui::SameLine();
	if (ImGui::Button("AI引き継ぎMD出力")) {
		if (WriteBalanceAIHandoff("resources/levels/ai_balance_handoff.md")) {
			balanceEditor_.statusMessage = "Wrote resources/levels/ai_balance_handoff.md";
		} else {
			balanceEditor_.statusMessage = "Failed to write AI handoff file.";
		}
	}
	if (ImGui::Button("JSONから再読み込み")) {
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
	if (!embedded) {
		ImGui::End();
	}
#else
	(void)embedded;
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
		config.bulletHp = ReadCustomFloat(attackJson, "bulletHp", config.bulletHp);
		config.bulletPenetration = ReadCustomFloat(attackJson, "bulletPenetration", config.bulletPenetration);
		config.randomSpread = ReadCustomBool(attackJson, "randomSpread", config.randomSpread);
		config.pattern = static_cast<Enemy::BossAttackConfig::Pattern>((std::clamp)(ReadCustomInt(attackJson, "pattern", static_cast<int>(config.pattern)), 0, 3));
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
	SpriteCommon::GetInstance()->PreDraw(kNormal);
	player_->DrawSprite();
	player_->DrawEncyclopedia();
	//shotGide->Draw();
	if (controlGuideText_) {
		controlGuideText_->SetPosition(showControlGuide_ ? Vector2{ 22.0f, 636.0f } : Vector2{ 22.0f, 690.0f });
		controlGuideText_->Draw();
	}
	if (fpsText_) {
		fpsText_->Draw();
	}
	if (showPostProfileOverlay_ && postProfileText_) {
		postProfileText_->Draw();
	}
	DrawBulletStatusDebugOverlay();
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
