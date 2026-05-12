#pragma once
#define NOMINMAX
#include "Audio.h"
#include "debugCamera.h"
#include "Dump.h"
#include "Easing.h"
#include "Resource.h"
#include "Sprite.h"
#include "WinApp.h"
#include "Object3d.h"
#include "Model.h"
#include "ModelManager.h"
#include "SrvManager.h"
#include "CollisionManager.h"
#include "MapChip.h"
#include "Fade.h"
#include "Stage.h"
#include "BulletManager.h"
#include "EnemyManager.h"
#include "IScene.h"
#include "TrailManager.h"
#include "RingManager.h"
#include "Skybox.h"
#include "EffectSequencer.h"
#include "ObjectPostEffect.h"

// ゲームシーン
class TestScene : public IScene {

public:

	/// <summary>
	/// コンストラクタ
	/// </summary>
	TestScene();

	/// <summary>
	/// デストラクタ
	/// </summary>
	~TestScene();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize() override;

	/// <summary>
	/// 更新
	/// </summary>
	void Update() override;

	/// <summary>
	/// 描画
	/// </summary>
	void Draw() override;

	void DrawShadow() override;

	void DrawPostEffect3D() override;

	/// <summary>
	/// 描画
	/// </summary>
	void DrawSprite() override;

	bool IsFinished() const override { return finished_; }

	float GetFinalDeltaTime() const override { return finalDeltaTime; }

private:


	std::unique_ptr<DebugCamera> debugCamera;
	std::unique_ptr<Camera> camera;

	std::unique_ptr<Object3d> groundObj_;
	std::unique_ptr<Object3d> blockObj_;
	std::unique_ptr<Object3d> blockObj2_;
	std::unique_ptr<Object3d> effectStartMarker_;
	std::unique_ptr<Object3d> effectTargetMarker_;

	std::unique_ptr<TrailManager> trailManager_;
	std::unique_ptr<RingManager> ringManager_;
	std::unique_ptr<EffectSequencer> effectSequencer_;
	std::unique_ptr<ObjectPostEffect> objectPostEffect_;
	std::unique_ptr<Object3d> swordObj_;

	std::unique_ptr<Skybox> skybox_;

	// 入力
	Input* input_;

	// ワールドトランスフォーム
	Transform worldTransform_;

	bool finished_ = false;

	float finalDeltaTime = 1.0f / 60.0f;

	EffectProfile transplantTestProfile_;
	Vector3 effectStartPos_ = { -35.0f, 15.0f, 0.0f };
	Vector3 effectTargetPos_ = { 35.0f, 15.0f, 0.0f };
	bool autoFireEffect_ = false;
	bool enableObjectPostEffect_ = false;
	float autoFireTimer_ = 0.0f;
	RingEffectConfig ringConfig_;

};
