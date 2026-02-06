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

// ゲームシーン
class GameScene {

public:

	/// <summary>
	/// コンストラクタ
	/// </summary>
	GameScene();

	/// <summary>
	/// デストラクタ
	/// </summary>
	~GameScene();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

	void DrawPostEffect3D();

	/// <summary>
	/// 描画
	/// </summary>
	void DrawSprite();

	bool IsFinished() const { return finished_; }

	Object3d* GetBallObj() { return ballObj_.get(); }

private:


	std::unique_ptr<DebugCamera> debugCamera;
	std::unique_ptr<Camera> camera;
	
	//std::unique_ptr<Object3d> object3d;
	std::unique_ptr<Object3d> enemyObject_;
	std::unique_ptr<Object3d> object3d3;

	std::unique_ptr<Object3d> playerObject_;

	std::unique_ptr<Object3d> ballObj_;
	std::unique_ptr<Object3d> ball_;

	// 入力
	Input* input_;

	// ワールドトランスフォーム
	Transform worldTransform_;

	// プレイヤー
	std::unique_ptr<Player> player_;

	// 敵
	std::unique_ptr<Enemy> enemy_;

	// 経験値敵
	//std::unique_ptr<EnemyManager> enemyManager_;

	// ステージ
	std::unique_ptr<Stage> stage_;

	// 弾マネージャ
	std::unique_ptr<BulletManager> bulletManager_;

	// 衝突マネージャ
	std::unique_ptr<CollisionManager> collisionManager_;

	// 終了フラグ
	bool finished_ = false;

	std::unique_ptr<Fade> fade_ = nullptr;
	Phase phase_ = Phase::kFadeIn;

	std::unique_ptr<Sprite> shotGide;
	std::unique_ptr<Sprite> wasdGide;
	std::unique_ptr<Sprite> dashGide;
	std::unique_ptr<Sprite> toTitleGide;

	// カメラ合わせフラグ
	bool cameraFollow_ = true;

	Vector3 direction = { 0.0f, -1.0f, 0.0f };
	float insensity = 1.0f;
	float shininess = 10.0f;

	float timeScale_ = 1.0f; // 1.0 が通常、0.2 なら 5倍スロー

};
