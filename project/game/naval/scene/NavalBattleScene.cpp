#include "NavalBattleScene.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "Object3dCommon.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void NavalBattleScene::Initialize()
{
	input_ = Input::GetInstance();
	camera_ = std::make_unique<Camera>();
	debugCamera_ = std::make_unique<DebugCamera>();

	ModelManager::GetInstance()->LoadModel("ground.obj");
	ModelManager::GetInstance()->LoadModel("bullet.obj");
	ModelManager::GetInstance()->LoadModel("navalHullBox.obj");

	Object3dCommon::GetInstance()->SetDefaultCamera(camera_.get());
	Object3dCommon::GetInstance()->SetDebugDefaultCamera(debugCamera_.get());
	Object3dCommon::GetInstance()->SetIsDebugCamera(false);
	Object3dCommon::GetInstance()->SetShadowRange(420.0f);

	sea_ = std::make_unique<Object3d>();
	sea_->Initialize();
	InitializeObject(*sea_, "ground.obj", { 0.02f, 0.28f, 0.58f, 1.0f }, false);
	sea_->SetScale({ 0.13f, 1.0f, 0.13f });
	sea_->SetTranslate({ 0.0f, -1.2f, 0.0f });

	playerHull_ = std::make_unique<Object3d>();
	playerHull_->Initialize();
	InitializeObject(*playerHull_, "navalHullBox.obj", { 1.0f, 0.08f, 0.04f, 1.0f }, false);

	playerTurret_ = std::make_unique<Object3d>();
	playerTurret_->Initialize();
	InitializeObject(*playerTurret_, "bullet.obj", { 1.0f, 0.85f, 0.10f, 1.0f }, false);

	playerMarker_ = std::make_unique<Object3d>();
	playerMarker_->Initialize();
	InitializeObject(*playerMarker_, "bullet.obj", { 1.0f, 0.95f, 0.05f, 1.0f }, false);

	ResetBattle();
}

void NavalBattleScene::Update()
{
	battleTimer_ += finalDeltaTime_;

	if (input_->IsTrigger(input_->GetKey()[DIK_ESCAPE], input_->GetPreKey()[DIK_ESCAPE])) {
		finished_ = true;
		nextSceneName_ = "TITLE";
		return;
	}
	if (input_->IsTrigger(input_->GetKey()[DIK_R], input_->GetPreKey()[DIK_R])) {
		ResetBattle();
	}

	UpdatePlayer();
	UpdateEnemies();
	UpdateProjectiles();
	UpdateCamera();

	sea_->Update();
	playerHull_->Update();
	playerTurret_->Update();
	playerMarker_->Update();
	for (auto& enemy : enemies_) {
		if (enemy.alive && enemy.hull) {
			enemy.hull->Update();
		}
	}
	for (auto& projectile : projectiles_) {
		if (projectile.object) {
			projectile.object->Update();
		}
	}

	DrawDebugWindow();
}

void NavalBattleScene::DrawShadow()
{
	Object3dCommon::GetInstance()->PreDraw(kShadow);
	playerHull_->DrawShadow();
	playerTurret_->DrawShadow();
	playerMarker_->DrawShadow();
	for (auto& enemy : enemies_) {
		if (enemy.alive && enemy.hull) {
			enemy.hull->DrawShadow();
		}
	}
}

void NavalBattleScene::DrawPostEffect3D()
{
	Object3dCommon::GetInstance()->PreDraw(kNone);
	sea_->Draw();
	playerHull_->Draw();
	playerTurret_->Draw();
	playerMarker_->Draw();
	for (auto& enemy : enemies_) {
		if (enemy.alive && enemy.hull) {
			enemy.hull->Draw();
		}
	}
	for (auto& projectile : projectiles_) {
		if (projectile.object) {
			projectile.object->Draw();
		}
	}
}

void NavalBattleScene::ResetBattle()
{
	player_ = {};
	player_.position = { 0.0f, 1.1f, -36.0f };
	player_.yaw = 0.0f;
	player_.maxHp = 120.0f;
	player_.hp = player_.maxHp;
	player_.reload = 0.0f;
	battleTimer_ = 0.0f;
	cameraYawOffset_ = 0.0f;
	projectiles_.clear();
	enemies_.clear();

	const Vector3 enemyPositions[] = {
		{ -76.0f, 1.0f, 110.0f },
		{ 0.0f, 1.0f, 145.0f },
		{ 82.0f, 1.0f, 115.0f },
	};

	for (const Vector3& position : enemyPositions) {
		EnemyShip enemy{};
		enemy.ship.position = position;
		enemy.ship.maxHp = 50.0f;
		enemy.ship.hp = enemy.ship.maxHp;
		enemy.ship.speed = 0.0f;
		enemy.hull = std::make_unique<Object3d>();
		enemy.hull->Initialize();
		InitializeObject(*enemy.hull, "navalHullBox.obj", { 0.40f, 0.95f, 0.35f, 1.0f }, false);
		enemy.hull->SetScale({ 2.0f, 0.45f, 5.6f });
		enemies_.push_back(std::move(enemy));
	}

	UpdatePlayer();
	UpdateCamera();
	sea_->Update();
	for (auto& enemy : enemies_) {
		if (enemy.hull) {
			enemy.hull->SetTranslate(enemy.ship.position);
			enemy.hull->Update();
		}
	}
}

void NavalBattleScene::InitializeObject(Object3d& object, const std::string& modelPath, const Vector4& color, bool lighting)
{
	object.SetModel(modelPath);
	object.SetColor(color);
	object.SetLighting(lighting);
	object.SetInsensity(0.85f);
}

void NavalBattleScene::UpdatePlayer()
{
	const float dt = finalDeltaTime_;
	const float acceleration = 16.0f;
	const float drag = 4.5f;
	const float maxForwardSpeed = 28.0f;
	const float maxReverseSpeed = -7.0f;
	const float turnRate = 0.72f;

	float throttle = 0.0f;
	if (input_->IsPress(input_->GetKey()[DIK_W])) throttle += 1.0f;
	if (input_->IsPress(input_->GetKey()[DIK_S])) throttle -= 1.0f;
	const Vector2 leftStick = input_->GetLeftStick();
	throttle += leftStick.y;
	throttle = std::clamp(throttle, -1.0f, 1.0f);

	if (std::abs(throttle) > 0.001f) {
		player_.speed += throttle * acceleration * dt;
	} else if (std::abs(player_.speed) > 0.001f) {
		const float brake = (std::min)(std::abs(player_.speed), drag * dt);
		player_.speed -= std::copysign(brake, player_.speed);
	}
	player_.speed = std::clamp(player_.speed, maxReverseSpeed, maxForwardSpeed);

	float rudder = 0.0f;
	if (input_->IsPress(input_->GetKey()[DIK_A])) rudder -= 1.0f;
	if (input_->IsPress(input_->GetKey()[DIK_D])) rudder += 1.0f;
	rudder += leftStick.x;
	rudder = std::clamp(rudder, -1.0f, 1.0f);
	const float speedTurnScale = 0.25f + 0.75f * (std::min)(std::abs(player_.speed) / maxForwardSpeed, 1.0f);
	player_.yaw += rudder * turnRate * speedTurnScale * dt;

	const Vector3 forward = ForwardFromYaw(player_.yaw);
	player_.position.x += forward.x * player_.speed * dt;
	player_.position.z += forward.z * player_.speed * dt;
	player_.position.x = std::clamp(player_.position.x, -230.0f, 230.0f);
	player_.position.z = std::clamp(player_.position.z, -230.0f, 230.0f);

	player_.reload = (std::max)(0.0f, player_.reload - dt);
	const bool firePressed =
		input_->IsTrigger(input_->GetKey()[DIK_SPACE], input_->GetPreKey()[DIK_SPACE]) ||
		input_->IsTrigger(input_->GetMouseState().rgbButtons[0], input_->GetPreMouseState().rgbButtons[0]) ||
		input_->IsGamepadButtonTrigger(XINPUT_GAMEPAD_RIGHT_SHOULDER);
	if (firePressed) {
		FireMainGun();
	}

	float cameraInput = 0.0f;
	if (input_->IsPress(input_->GetKey()[DIK_Q])) cameraInput -= 1.0f;
	if (input_->IsPress(input_->GetKey()[DIK_E])) cameraInput += 1.0f;
	const Vector2 rightStick = input_->GetRightStick();
	cameraInput += rightStick.x;
	cameraYawOffset_ += cameraInput * 1.8f * dt;

	playerHull_->SetTranslate(player_.position);
	playerHull_->SetRotate({ 0.0f, player_.yaw, 0.0f });
	playerHull_->SetScale({ 1.85f, 0.42f, 5.4f });

	const Vector3 turretOffset = ForwardFromYaw(player_.yaw);
	playerTurret_->SetTranslate({
		player_.position.x + turretOffset.x * 0.65f,
		player_.position.y + 0.62f,
		player_.position.z + turretOffset.z * 0.65f,
		});
	playerTurret_->SetRotate({ 0.0f, player_.yaw + std::numbers::pi_v<float> * 0.5f, 0.0f });
	playerTurret_->SetScale({ 0.32f, 0.32f, 0.32f });

	playerMarker_->SetTranslate({
		player_.position.x,
		player_.position.y + 1.55f,
		player_.position.z,
		});
	playerMarker_->SetRotate({ std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f });
	playerMarker_->SetScale({ 0.36f, 0.36f, 0.36f });
}

void NavalBattleScene::UpdateEnemies()
{
	for (auto& enemy : enemies_) {
		if (!enemy.alive) {
			continue;
		}

		const Vector3 toPlayer = {
			player_.position.x - enemy.ship.position.x,
			0.0f,
			player_.position.z - enemy.ship.position.z,
		};
		enemy.ship.yaw = std::atan2(toPlayer.x, toPlayer.z);

		const float distance = DistanceXZ(enemy.ship.position, player_.position);
		if (distance > 72.0f) {
			const Vector3 forward = ForwardFromYaw(enemy.ship.yaw);
			enemy.ship.position.x += forward.x * 4.2f * finalDeltaTime_;
			enemy.ship.position.z += forward.z * 4.2f * finalDeltaTime_;
		} else if (distance < 14.0f) {
			player_.hp = (std::max)(0.0f, player_.hp - 1.5f * finalDeltaTime_);
		}

		enemy.hull->SetTranslate(enemy.ship.position);
		enemy.hull->SetRotate({ 0.0f, enemy.ship.yaw, 0.0f });
		enemy.hull->SetScale({ 2.0f, 0.45f, 5.6f });
	}
}

void NavalBattleScene::UpdateProjectiles()
{
	const float waterY = -0.92f;
	const float gravity = -18.0f;
	for (auto& projectile : projectiles_) {
		projectile.life -= finalDeltaTime_;
		projectile.velocity.y += gravity * finalDeltaTime_;
		projectile.position.x += projectile.velocity.x * finalDeltaTime_;
		projectile.position.y += projectile.velocity.y * finalDeltaTime_;
		projectile.position.z += projectile.velocity.z * finalDeltaTime_;

		if (projectile.object) {
			projectile.object->SetTranslate(projectile.position);
			const float yaw = std::atan2(projectile.velocity.x, projectile.velocity.z) + std::numbers::pi_v<float> * 0.5f;
			projectile.object->SetRotate({ 0.0f, yaw, 0.0f });
		}

		if (projectile.position.y <= waterY) {
			projectile.life = 0.0f;
			continue;
		}

		for (auto& enemy : enemies_) {
			if (!enemy.alive) {
				continue;
			}
			const float verticalDistance = std::abs(projectile.position.y - enemy.ship.position.y);
			if (DistanceXZ(projectile.position, enemy.ship.position) < projectile.radius && verticalDistance < 5.0f) {
				enemy.ship.hp -= projectile.damage;
				projectile.life = 0.0f;
				if (enemy.ship.hp <= 0.0f) {
					enemy.alive = false;
				}
				break;
			}
		}
	}

	projectiles_.erase(
		std::remove_if(projectiles_.begin(), projectiles_.end(), [](const Projectile& projectile) {
			return projectile.life <= 0.0f;
		}),
		projectiles_.end());
}

void NavalBattleScene::UpdateCamera()
{
	const float cameraYaw = player_.yaw + cameraYawOffset_;
	const float cameraPitch = 0.52f;
	const float cameraDistance = 78.0f;
	const float cameraHeight = 16.0f;
	const float cosPitch = std::cos(cameraPitch);
	const Vector3 forward = {
		std::sin(cameraYaw) * cosPitch,
		-std::sin(cameraPitch),
		std::cos(cameraYaw) * cosPitch,
	};
	const Vector3 target = {
		player_.position.x,
		player_.position.y + cameraHeight,
		player_.position.z,
	};
	const Vector3 eye = {
		target.x - forward.x * cameraDistance,
		target.y - forward.y * cameraDistance,
		target.z - forward.z * cameraDistance,
	};

	camera_->SetTranslate(eye);
	camera_->SetRotate({ cameraPitch, cameraYaw, 0.0f });
	camera_->Update();
	Object3dCommon::GetInstance()->SetShadowFocus(player_.position);
}

void NavalBattleScene::FireMainGun()
{
	if (player_.reload > 0.0f) {
		return;
	}

	const Vector3 forward = ForwardFromYaw(player_.yaw);
	Projectile projectile{};
	projectile.position = {
		player_.position.x + forward.x * 6.0f,
		player_.position.y + 1.2f,
		player_.position.z + forward.z * 6.0f,
	};
	projectile.velocity = { forward.x * 92.0f, 15.5f, forward.z * 92.0f };
	projectile.life = 3.6f;
	projectile.damage = 24.0f;
	projectile.radius = 4.6f;
	projectile.object = std::make_unique<Object3d>();
	projectile.object->Initialize();
	InitializeObject(*projectile.object, "bullet.obj", { 1.0f, 0.82f, 0.18f, 1.0f }, false);
	projectile.object->SetRotate({ 0.0f, player_.yaw + std::numbers::pi_v<float> * 0.5f, 0.0f });
	projectile.object->SetScale({ 0.16f, 0.16f, 0.16f });

	projectiles_.push_back(std::move(projectile));
	player_.reload = 0.72f;
}

void NavalBattleScene::DrawDebugWindow()
{
#ifdef USE_IMGUI
	int aliveEnemies = 0;
	for (const auto& enemy : enemies_) {
		if (enemy.alive) {
			++aliveEnemies;
		}
	}

	ImGui::Begin("Naval Battle Prototype");
	ImGui::Text("F4 from title: Naval prototype");
	ImGui::Text("W/S: throttle  A/D: rudder  Space/LClick/RB: fire");
	ImGui::Text("Q/E or RightStick X: camera orbit  R: reset  Esc: title");
	ImGui::Separator();
	ImGui::Text("Player HP: %.0f / %.0f", player_.hp, player_.maxHp);
	ImGui::Text("Player Pos: %.1f, %.1f, %.1f", player_.position.x, player_.position.y, player_.position.z);
	if (camera_) {
		const Vector3& cameraPos = camera_->GetTranslate();
		ImGui::Text("Camera Pos: %.1f, %.1f, %.1f", cameraPos.x, cameraPos.y, cameraPos.z);
	}
	ImGui::Text("Speed: %.1f", player_.speed);
	ImGui::Text("Reload: %.2f", player_.reload);
	ImGui::Text("Enemies: %d", aliveEnemies);
	ImGui::Text("Shells: %zu", projectiles_.size());
	ImGui::Text("Prototype: long-range gunnery spacing");
	ImGui::End();
#endif
}

Vector3 NavalBattleScene::ForwardFromYaw(float yaw) const
{
	return { std::sin(yaw), 0.0f, std::cos(yaw) };
}

float NavalBattleScene::DistanceXZ(const Vector3& a, const Vector3& b) const
{
	const float dx = a.x - b.x;
	const float dz = a.z - b.z;
	return std::sqrt(dx * dx + dz * dz);
}
