#include "PlayerLabScene.h"
#include "Object3dCommon.h"
#include "ParticleManager.h"
#include "MapChip.h"
#include <algorithm>
#include <cstdio>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {

Vector3 GetActiveCameraPosition(Camera* camera, DebugCamera* debugCamera) {
	return Object3dCommon::GetInstance()->GetIsDebugCamera() && debugCamera
		? debugCamera->GetEyePosition()
		: camera->GetTranslate();
}

} // namespace

void PlayerLabScene::Initialize()
{
	input_ = Input::GetInstance();

	debugCamera_ = std::make_unique<DebugCamera>();
	camera_ = std::make_unique<Camera>();
	camera_->SetTranslate(Vector3(30.0f, 30.0f, -55.0f));
	Object3dCommon::GetInstance()->SetDefaultCamera(camera_.get());
	Object3dCommon::GetInstance()->SetDebugDefaultCamera(debugCamera_.get());

	playerPostEffect_ = std::make_unique<ObjectPostEffect>();
	playerPostEffect_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(),
		Object3dCommon::GetInstance()->GetSrvManager(),
		nullptr
	);
	{
		BloomParam& param = playerPostEffect_->GetParam();
		param.threshold = 0.0f;
		param.intensity = 1.0f;
		param.outlineWidth = 0.0f;
		param.outlineThreshold = 0.0f;
		param.outlineColor = { 0.12f, 1.0f, 0.32f };
		param.outlineBloomIntensity = 0.27f;
		param.outlineBloomWidth = 2.7f;
	}

	expEnemyPostEffect_ = std::make_unique<ObjectPostEffect>();
	expEnemyPostEffect_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(),
		Object3dCommon::GetInstance()->GetSrvManager(),
		nullptr
	);
	{
		BloomParam& param = expEnemyPostEffect_->GetParam();
		param.threshold = 0.0f;
		param.intensity = 1.0f;
		param.outlineWidth = 0.0f;
		param.outlineThreshold = 0.0f;
		param.outlineColor = { 0.9f, 0.15f, 0.98f };
		param.outlineBloomIntensity = 0.27f;
		param.outlineBloomWidth = 2.7f;
	}

	stagePostEffect_ = std::make_unique<ObjectPostEffect>();
	stagePostEffect_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(),
		Object3dCommon::GetInstance()->GetSrvManager(),
		nullptr,
		0.5f
	);
	{
		BloomParam& param = stagePostEffect_->GetParam();
		param.threshold = 0.0f;
		param.intensity = 0.75f;
		param.outlineWidth = 0.0f;
		param.outlineThreshold = 0.0f;
		param.outlineColor = { 1.0f, 0.55f, 0.58f };
		param.outlineBloomIntensity = 0.0f;
		param.outlineBloomWidth = 0.0f;
	}

	neonGridPostEffect_ = std::make_unique<ObjectPostEffect>();
	neonGridPostEffect_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(),
		Object3dCommon::GetInstance()->GetSrvManager(),
		nullptr
	);
	{
		BloomParam& param = neonGridPostEffect_->GetParam();
		param.threshold = 0.0f;
		param.intensity = 1.5f;
	}

	bulletTrailPostEffect_ = std::make_unique<ObjectPostEffect>();
	bulletTrailPostEffect_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(),
		Object3dCommon::GetInstance()->GetSrvManager(),
		nullptr
	);
	{
		BloomParam& param = bulletTrailPostEffect_->GetParam();
		param.threshold = 0.0f;
		param.intensity = 2.0f;
	}

	stage_ = std::make_unique<Stage>();
	stage_->Initialize();

	bulletManager_ = std::make_unique<BulletManager>();
	bulletManager_->Initialize(Object3dCommon::GetInstance()->GetDxCommon(), Object3dCommon::GetInstance());

	playerObject_ = std::make_unique<Object3d>();
	playerObject_->Initialize();
	playerObject_->SetModel("player3D.obj");
	playerObject_->SetColor(Vector4(0.48f, 0.86f, 0.22f, 1.0f));

	player_ = std::make_unique<Player>();
	player_->Initialize(playerObject_.get(), Vector3(30.0f, 30.0f, 0.0f));
	player_->SetAttackControllerBulletManager(bulletManager_.get());

	collisionManager_ = std::make_unique<CollisionManager>();
	neonGridRenderer_ = std::make_unique<NeonGridRenderer>();
	neonGridRenderer_->Initialize(Object3dCommon::GetInstance()->GetDxCommon(), "resources/white512x512.png");

	targetSpawns_ = {
		{ Vector3(38.0f, 30.0f, 0.0f), ExpEnemyType::Square },
		{ Vector3(44.0f, 32.5f, 0.0f), ExpEnemyType::Triangle },
		{ Vector3(50.0f, 28.0f, 0.0f), ExpEnemyType::Pentagon },
		{ Vector3(56.0f, 33.0f, 0.0f), ExpEnemyType::Square }
	};
	SpawnTargets();

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
}

void PlayerLabScene::Update()
{
	finalDeltaTime_ = player_ && player_->IsChangeMode() ? 0.0f : 1.0f / 60.0f;

	if (input_->IsTrigger(input_->GetKey()[DIK_ESCAPE], input_->GetPreKey()[DIK_ESCAPE])) {
		nextSceneName_ = "TITLE";
		finished_ = true;
		return;
	}

	if (input_->IsTrigger(input_->GetKey()[DIK_F5], input_->GetPreKey()[DIK_F5])) {
		player_->AddExp(player_->GetNextLevelExpValue());
	}
	if (input_->IsTrigger(input_->GetKey()[DIK_F6], input_->GetPreKey()[DIK_F6])) {
		player_->AddExp(200);
	}
	if (input_->IsTrigger(input_->GetKey()[DIK_F9], input_->GetPreKey()[DIK_F9])) {
		targets_.clear();
		SpawnTargets();
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

	UpdateCamera();

	stage_->Update();
	player_->Update(camera_.get(), *stage_, bulletManager_.get(), finalDeltaTime_);
	for (auto& target : targets_) {
		target->Update(*stage_, finalDeltaTime_);
	}
	bulletManager_->Update(*stage_, finalDeltaTime_);
	CheckLabCollisions();
	RespawnDeadTargets();
	ParticleManager::GetInstance()->Update(finalDeltaTime_, camera_.get(), debugCamera_.get());
	playerPostEffect_->Update(finalDeltaTime_);
	expEnemyPostEffect_->Update(finalDeltaTime_);
	stagePostEffect_->Update(finalDeltaTime_);
	neonGridPostEffect_->Update(finalDeltaTime_);
	bulletTrailPostEffect_->Update(finalDeltaTime_);

	DrawLabEditor();
}

void PlayerLabScene::Draw()
{
	Object3dCommon::GetInstance()->PreDraw(kNormal);
}

void PlayerLabScene::DrawPostEffect3D()
{
	Object3dCommon::GetInstance()->PreDraw(kNormal);

	if (enableNeonGridPostEffect_) {
		neonGridPostEffect_->BeginCapture();
		DrawNeonGridPass();
		neonGridPostEffect_->EndCaptureAdditiveOnly();
		Object3dCommon::GetInstance()->PreDraw(kNormal);
	} else {
		DrawNeonGridPass();
		Object3dCommon::GetInstance()->PreDraw(kNormal);
	}

	player_->Draw(!enablePlayerPostEffect_);
	if (showTargets_) {
		for (auto& target : targets_) {
			target->Draw(!enableExpEnemyPostEffect_);
		}
	}
	bulletManager_->Draw();
	if (showStage_) {
		stage_->DrawVisible(GetActiveCameraPosition(camera_.get(), debugCamera_.get()), 38.0f, 24.0f);
		if (enableStagePostEffect_) {
			stagePostEffect_->BeginCapture();
			Object3dCommon::GetInstance()->PreDraw(kNormal);
			stage_->DrawVisible(GetActiveCameraPosition(camera_.get(), debugCamera_.get()), 38.0f, 24.0f);
			stagePostEffect_->EndCaptureBloomOnly();
			Object3dCommon::GetInstance()->PreDraw(kNormal);
		}
	}

	const Matrix4x4 vp = Object3dCommon::GetInstance()->GetIsDebugCamera()
		? debugCamera_->GetViewProjectionMatrix()
		: camera_->GetViewProjectionMatrix();

	if (enableBulletTrailPostEffect_) {
		bulletTrailPostEffect_->BeginCapture();
		bulletManager_->DrawTrails(vp);
		Object3dCommon::GetInstance()->PreDraw(kNormal);
		bulletManager_->Draw();
		bulletTrailPostEffect_->EndCaptureAdditiveOnly();
		Object3dCommon::GetInstance()->PreDraw(kNormal);
	} else {
		bulletManager_->DrawTrails(vp);
		Object3dCommon::GetInstance()->PreDraw(kNormal);
	}

	if (enablePlayerPostEffect_) {
		playerPostEffect_->BeginCapture();
		Object3dCommon::GetInstance()->PreDraw(kNormal);
		player_->DrawBodyOnly();
		playerPostEffect_->EndCapture();
		Object3dCommon::GetInstance()->PreDraw(kNormal);
	}

	if (enableExpEnemyPostEffect_ && showTargets_) {
		expEnemyPostEffect_->BeginCapture();
		Object3dCommon::GetInstance()->PreDraw(kNormal);
		for (auto& target : targets_) {
			target->DrawBodyOnly();
		}
		expEnemyPostEffect_->EndCapture();
		Object3dCommon::GetInstance()->PreDraw(kNormal);
	}

	ParticleManager::GetInstance()->Draw();
}

void PlayerLabScene::DrawSprite()
{
	if (fpsText_) {
		fpsText_->Draw();
	}
}

void PlayerLabScene::SpawnTargets()
{
	targets_.clear();
	targets_.reserve(targetSpawns_.size());
	for (const TargetSpawn& spawn : targetSpawns_) {
		auto target = std::make_unique<ExpEnemy>();
		target->Initialize(spawn.position, player_.get(), spawn.type);
		target->SetAttackControllerBulletManager(bulletManager_.get());
		targets_.push_back(std::move(target));
	}
}

void PlayerLabScene::RespawnDeadTargets()
{
	for (size_t i = 0; i < targets_.size(); ++i) {
		if (!targets_[i]->IsDead()) {
			continue;
		}
		auto target = std::make_unique<ExpEnemy>();
		target->Initialize(targetSpawns_[i].position, player_.get(), targetSpawns_[i].type);
		target->SetAttackControllerBulletManager(bulletManager_.get());
		targets_[i] = std::move(target);
	}
}

void PlayerLabScene::UpdateCamera()
{
	const Vector3 playerPos = player_->GetWorldPosition();
	const float kCameraZ = -55.0f;
	const float kMarginX = 18.0f;
	const float kMarginY = 11.0f;
	const float kMaxCameraX = MapChip::kBlockWidth * (MapChip::kNumBlockHorizontal - 1) - kMarginX;
	const float kMaxCameraY = MapChip::kBlockHeight * (MapChip::kNumBlockVirtical - 1) - kMarginY;
	const Vector3 targetCameraPos = {
		(std::clamp)(playerPos.x, kMarginX, kMaxCameraX),
		(std::clamp)(playerPos.y, kMarginY, kMaxCameraY),
		kCameraZ
	};
	const Vector3 currentCameraPos = camera_->GetTranslate();
	camera_->SetTranslate(currentCameraPos + (targetCameraPos - currentCameraPos) * 0.14f);
	camera_->Update();
	debugCamera_->Update(input_->GetMouseState(), input_->GetKey(), input_->GetLeftStick());
}

void PlayerLabScene::CheckLabCollisions()
{
	for (PlayerDrone* drone : player_->GetDronePtrs()) {
		for (auto& target : targets_) {
			collisionManager_->CheckCollisionPair(drone, target.get());
		}
	}

	for (Bullet* bullet : bulletManager_->GetBulletPtrs()) {
		for (auto& target : targets_) {
			collisionManager_->CheckCollisionPair(bullet, target.get());
		}
		collisionManager_->CheckCollisionPair(bullet, player_.get());
	}
}

void PlayerLabScene::DrawNeonGridPass()
{
	if (!neonGridRenderer_) {
		return;
	}

	const Matrix4x4 vp = Object3dCommon::GetInstance()->GetIsDebugCamera()
		? debugCamera_->GetViewProjectionMatrix()
		: camera_->GetViewProjectionMatrix();

	neonGridRenderer_->BeginFrame();
	const float fieldMinX = 0.0f;
	const float fieldMinY = 0.0f;
	const float fieldMaxX = MapChip::kBlockWidth * static_cast<float>(MapChip::kNumBlockHorizontal - 1);
	const float fieldMaxY = MapChip::kBlockHeight * static_cast<float>(MapChip::kNumBlockVirtical - 1);

	if (showWorldGrid_) {
		neonGridRenderer_->QueueWorldGrid(fieldMinX, fieldMaxX, fieldMinY, fieldMaxY, worldGridSpacing_, worldGridLineWidth_, worldGridColor_);
	}

	if (showActorLocalGrid_) {
		neonGridRenderer_->QueueLocalGridClipped(player_->GetWorldPosition(), actorGridRadius_, actorGridSpacing_, actorGridLineWidth_, playerGridColor_, fieldMinX, fieldMaxX, fieldMinY, fieldMaxY);
		for (auto& target : targets_) {
			if (!target->IsDead()) {
				neonGridRenderer_->QueueLocalGridClipped(target->GetWorldPosition(), actorGridRadius_ * 0.6f, actorGridSpacing_, actorGridLineWidth_ * 0.8f, targetGridColor_, fieldMinX, fieldMaxX, fieldMinY, fieldMaxY);
			}
		}
	}
	neonGridRenderer_->DrawAll(vp);
}

void PlayerLabScene::DrawLabEditor()
{
#ifdef USE_IMGUI
	ImGui::Begin("Player Lab");
	ImGui::Text("ESC: Title / F5: Next Level Exp / F6: +200 Exp / F9: Reset Targets");
	ImGui::Checkbox("Show Stage", &showStage_);
	ImGui::Checkbox("Show Targets", &showTargets_);
	ImGui::Checkbox("Enable Player Post", &enablePlayerPostEffect_);
	ImGui::Checkbox("Enable Target Post", &enableExpEnemyPostEffect_);
	ImGui::Checkbox("Enable Stage Post", &enableStagePostEffect_);
	ImGui::Checkbox("Enable Grid Post", &enableNeonGridPostEffect_);
	ImGui::Checkbox("Enable Bullet Trail Post", &enableBulletTrailPostEffect_);
	ImGui::Separator();
	ImGui::Text("Level: %d  Exp: %d / %d  Class: %s",
		player_->GetLevel(),
		player_->GetExp(),
		player_->GetNextLevelExpValue(),
		player_->GetCurrentClassName());
	BulletTrailSettings& trail = bulletManager_->GetTrailSettings();
	ImGui::DragFloat("Player Trail Half Width", &trail.playerHalfWidth, 0.01f, 0.01f, 1.5f);
	ImGui::DragFloat("Trail Lifetime", &trail.lifetime, 0.01f, 0.02f, 1.5f);
	ImGui::DragFloat("Head Width Scale", &trail.headWidthScale, 0.01f, 0.0f, 4.0f);
	ImGui::DragFloat("Tail Width Scale", &trail.tailWidthScale, 0.01f, 0.0f, 4.0f);
	ImGui::End();

	ImGui::Begin("Lab Stage Post");
	BloomParam& stagePost = stagePostEffect_->GetParam();
	ImGui::DragFloat("Stage Intensity", &stagePost.intensity, 0.01f, 0.0f, 5.0f);
	ImGui::Text("Stage uses bloom-only add pass for performance.");
	ImGui::End();

	ImGui::Begin("Lab Neon Grid");
	ImGui::Checkbox("Show World Grid", &showWorldGrid_);
	ImGui::Checkbox("Show Actor Local Grid", &showActorLocalGrid_);
	ImGui::DragFloat("World Spacing", &worldGridSpacing_, 0.05f, 0.25f, 8.0f);
	ImGui::DragFloat("World Line Width", &worldGridLineWidth_, 0.005f, 0.005f, 0.5f);
	ImGui::ColorEdit4("World Color", &worldGridColor_.x);
	ImGui::DragFloat("Actor Radius", &actorGridRadius_, 0.05f, 0.5f, 16.0f);
	ImGui::DragFloat("Actor Spacing", &actorGridSpacing_, 0.025f, 0.2f, 3.0f);
	ImGui::DragFloat("Actor Line Width", &actorGridLineWidth_, 0.005f, 0.005f, 0.5f);
	ImGui::ColorEdit4("Player Grid", &playerGridColor_.x);
	ImGui::ColorEdit4("Target Grid", &targetGridColor_.x);
	ImGui::End();

	player_->DrawPlayerClassEditor();
#endif
}
