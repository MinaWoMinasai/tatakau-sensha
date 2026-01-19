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

class Stage;

/// <summary>
/// 自キャラ
/// </summary>
class PlayerDrone : public Collider {

public:

	/// <summary>
	/// デストラクタ
	/// </summary>
	~PlayerDrone();

	/// <summary>
	/// 攻撃
	/// </summary>
	void Attack();

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
	void Initialize(const Vector3& position, const Vector3& velocity);

	/// <summary>
	/// 更新
	/// </summary>
	void Update(Camera* viewProjection, Stage& stage, const Vector3& playerPosition);

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

	/// <summary>
	/// スプライト描画
	/// </summary>
	void DrawSprite();

	/// <summary>
	/// 衝突判定
	/// </summary>
	void OnCollision(Collider* other) override;

	// ワールド座標を取得

	// 半径
	static inline const float kRadius = 1.0f;

	Vector3 GetWorldPosition() const override;

	Vector3 GetMove() { return velocity_; }
	void SetVelocity(const Vector3& v) { velocity_ = v; }

	// セッター
	void SetWorldPosition(const Vector3& pos) {
		worldTransform_.translate = pos;
		object_->SetTransform(worldTransform_);
		object_->Update();
	}

	AABB GetAABB();

	void Damage();

	void Die();

	bool IsDead() const { return isDead_; }

	void SetAttackControllerBulletManager(BulletManager* bulletManager) {
		attackController_.SetBulletManager(bulletManager);
	}

private:
	// ワールド変換データ
	Transform worldTransform_;

	// モデル
	std::unique_ptr<Object3d> object_;
	// テクスチャハンドル
	uint32_t textureHandle_ = 0u;

	// キーボード入力
	Input* input_ = nullptr;

	// 弾
	Vector3 dir;

	// キャラクターの移動速さ
	float kCharacterSpeed = 0.2f;
	Vector3 move_;

	const int kBulletTime = 30;
	int bulletCoolTime = 0;

	// キャラクターの当たり判定サイズ
	static inline const float kWidth = 1.6f;
	static inline const float kHeight = 1.6f;

	Vector3 velocity_{ 0, 0, 0 };   // 現在速度
	Vector3 inputDir_{ 0, 0, 0 };   // 入力方向

	float maxSpeed_ = 0.25f;        // 最高速度
	float accel_ = 0.7f;         // 加速
	float decel_ = 3.5f;         // 減速（ブレーキ）

	Transform hpTransform_;

	int hp_ = 10;

	// 無敵時間
	float invincibleTimer_ = 0.0f;

	bool isDead_ = false;

	// 攻撃コントローラ
	AttackController attackController_;

	float angle_ = 0.0f;
};

