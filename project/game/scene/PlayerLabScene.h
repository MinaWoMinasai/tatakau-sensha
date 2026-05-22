#pragma once
#define NOMINMAX
#include <array>
#include <chrono>
#include <memory>
#include <vector>
#include "IScene.h"
#include "Player.h"
#include "debugCamera.h"
#include "Stage.h"
#include "BulletManager.h"
#include "ExpEnemy.h"
#include "CollisionManager.h"
#include "ObjectPostEffect.h"
#include "NeonGridRenderer.h"
#include "RingManager.h"
#include "TextLabel.h"

class PlayerLabScene : public IScene {
public:
	void Initialize() override;
	void Update() override;
	void Draw() override;
	void DrawPostEffect3D() override;
	void DrawSprite() override;

	bool IsFinished() const override { return finished_; }
	std::string GetNextSceneName() const override { return nextSceneName_; }
	float GetFinalDeltaTime() const override { return finalDeltaTime_; }

private:
	struct TargetSpawn {
		Vector3 position;
		ExpEnemyType type;
	};

	void SpawnTargets();
	void RespawnDeadTargets();
	void UpdateCamera();
	void CheckLabCollisions();
	void DrawNeonGridPass();
	void DrawLabEditor();

	Input* input_ = nullptr;
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<DebugCamera> debugCamera_;
	std::unique_ptr<Stage> stage_;
	std::unique_ptr<Object3d> playerObject_;
	std::unique_ptr<Player> player_;
	std::unique_ptr<BulletManager> bulletManager_;
	std::unique_ptr<CollisionManager> collisionManager_;
	std::vector<std::unique_ptr<ExpEnemy>> targets_;
	std::vector<TargetSpawn> targetSpawns_;
	std::unique_ptr<NeonGridRenderer> neonGridRenderer_;
	std::unique_ptr<ObjectPostEffect> playerPostEffect_;
	std::unique_ptr<ObjectPostEffect> expEnemyPostEffect_;
	std::unique_ptr<ObjectPostEffect> neonGridPostEffect_;
	std::unique_ptr<ObjectPostEffect> bulletTrailPostEffect_;
	std::unique_ptr<ObjectPostEffect> stagePostEffect_;
	std::unique_ptr<TextLabel> fpsText_;

	bool finished_ = false;
	std::string nextSceneName_ = "TITLE";
	float finalDeltaTime_ = 1.0f / 60.0f;
	std::chrono::steady_clock::time_point fpsLastSampleTime_{};
	float fpsAccumulatedTime_ = 0.0f;
	int fpsFrameCount_ = 0;
	bool enablePlayerPostEffect_ = true;
	bool enableExpEnemyPostEffect_ = true;
	bool enableNeonGridPostEffect_ = true;
	bool enableBulletTrailPostEffect_ = true;
	bool enableStagePostEffect_ = true;
	bool showStage_ = true;
	bool showTargets_ = true;
	bool showWorldGrid_ = true;
	bool showActorLocalGrid_ = true;
	float worldGridSpacing_ = 2.0f;
	float worldGridLineWidth_ = 0.075f;
	Vector4 worldGridColor_ = { 0.12f, 0.42f, 1.0f, 0.16f };
	float actorGridRadius_ = 5.4f;
	float actorGridSpacing_ = 1.0f;
	float actorGridLineWidth_ = 0.1f;
	Vector4 playerGridColor_ = { 0.50f, 1.0f, 0.35f, 1.0f };
	Vector4 targetGridColor_ = { 1.0f, 0.32f, 0.58f, 1.0f };
};
