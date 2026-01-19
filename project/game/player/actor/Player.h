#pragma once
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <list>
#include "Calculation.h"
#include "Collider.h"
#include "CollisionConfig.h"
#include "Object3d.h"
#include "Sprite.h"
#include "AttackController.h"
#include "PlayerDrone.h"

struct PlayerParticle {
	std::unique_ptr<Object3d> object;
	Vector3 velocity;
	Vector3 rotateSpeed;
	float timer;
	float lifeTime;

	float startAlpha;
};

class Stage;

/// <summary>
/// 自キャラ
/// </summary>
class Player : public Collider {

public:

	/// <summary>
	/// デストラクタ
	/// </summary>
	~Player();

	/// <summary>
	/// マウスの方を向く
	/// </summary>
	/// <param name="viewProjection"></param>
	void RotateToMouse(Camera* viewProjection);

	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="model">モデル</param>
	/// <param name="camera">カメラ</param>
	/// <param name="position">初期座標</param>
	void Initialize(Object3d* objectBullet, const Vector3& position);

	/// <summary>
	/// 更新
	/// </summary>
	void Update(Camera* viewProjection, Stage& stage, BulletManager* BulletManager);

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

	/// <summary>
	/// スプライト描画
	/// </summary>
	void DrawSprite();

	// ドローンのゲッター
	std::vector<PlayerDrone*> GetDronePtrs() const;

	/// <summary>
	/// 衝突判定
	/// </summary>
	void OnCollision(Collider* other) override;

	// ワールド座標を取得

	// 半径
	static inline const float kRadius = 0.8f;

	Vector3 GetWorldPosition() const override;

	Vector3 GetMove() { return velocity_; }
	void SetVelocity(const Vector3& v) { velocity_ = v; }

	// セッター
	void SetWorldPosition(const Vector3& pos) {
		worldTransform_.translate = pos;
		object_->SetTransform(worldTransform_);
		object_->Update();
	}

	Vector2 GetMoveInput();

	AABB GetAABB();

	void Damage();

	void Die(); // ← プレイヤー消滅

	// 演出終了か
	bool isFinished();

	bool IsDead() const { return isDead_; }

	bool IsOnGround() const { return isOnGround_; }
	void SetOnGround(bool onGround) { isOnGround_ = onGround; }

	float GetAngle() const { return angle_; }

	void SetAttackControllerBulletManager(BulletManager* bulletManager) {
		attackController_.SetBulletManager(bulletManager);
	}

	/// <summary>
	/// 攻撃
	/// </summary>
	void Attack();

	/// <summary>
	/// ドローン発射
	/// </summary>
	void DroneShoot(BulletManager* BulletManager);

	Sphere GetSphere() const;

private:
	// ワールド変換データ
	Transform worldTransform_;
	
	// モデル
	Object3d* object_ = nullptr;
	// テクスチャハンドル
	uint32_t textureHandle_ = 0u;

	// キーボード入力
	Input* input_ = nullptr;

	// 弾
	Vector3 dir;

	// キャラクターの移動速さ
	float kCharacterSpeed = 0.2f;
	Vector3 move_;
	
	const int kBulletTime = 10;
	int bulletCoolTime = 0;

	// キャラクターの当たり判定サイズ
	static inline const float kWidth = 1.6f;
	static inline const float kHeight = 1.6f;
	
	Vector3 velocity_{ 0, 0, 0 };   // 現在速度
	Vector3 inputDir_{ 0, 0, 0 };   // 入力方向

	float maxSpeed_ = 0.25f;        // 最高速度
	float accel_ = 2.5f;         // 加速
	float decel_ = 3.5f;         // 減速（ブレーキ）

	float gravity_ = -2.0f;
	float jumpPower_ = 1.0f;

	bool isOnGround_ = false;

	Transform hpTransform_;

	// HPモデル
	std::vector<std::unique_ptr<Sprite>> hpSprites_;
	std::unique_ptr<Sprite> hpFont;
	int hp_ = 3;

	// 無敵時間
	float invincibleTimer_ = 0.0f;

	bool isDead_ = false;
	
	bool isExploding_ = false;

	std::vector<PlayerParticle> particles_;

	// 攻撃コントローラ
	AttackController attackController_;

	// プレイヤーのドローン
	std::vector<std::unique_ptr<PlayerDrone>> drones_;

	// プレイヤーの経験値とレベル
	int exp_ = 0;
	int level_ = 1;

private:
	void SpawnParticles();
	void UpdateParticles(float deltaTime = 1.0f / 60.0f);
	const float deltaTime = 1.0f / 60.0f;

	float angle_ = 0.0f;
};

