#pragma once
#define NOMINMAX
#include "Collider.h"
#include <Windows.h>
#include <algorithm>
#include <optional>
#include "Object3d.h"
#include "Sprite.h"
#include "AttackController.h"

class Player;
class Stage;
class EnemyManager;

class Enemy : public Collider {

public:
	struct BossAttackConfig {
		enum class Pattern {
			Spread = 0,
			Ring = 1,
			Sniper = 2,
			Alternating = 3,
		};
		float bulletSpeed = 0.4f;
		int bulletCount = 2;
		float spreadAngleDeg = 30.0f;
		float cooldown = 0.15f;
		uint32_t damage = 10;
		float bulletHp = 0.0f;
		float bulletPenetration = 0.0f;
		bool randomSpread = true;
		Pattern pattern = Pattern::Spread;
	};
	struct EnemyProgressConfig {
		bool expEnemyHostile = false;
		uint32_t expEnemyContactDamage = 12;
		int healOnExpEnemyKill = 30;
		int killsPerLevel = 3;
		int maxHpGainPerLevel = 20;
		uint32_t damageGainPerLevel = 2;
		bool levelingModeEnabled = true;
		float levelingEnterPlayerDistance = 24.0f;
		float levelingExitPlayerDistance = 16.0f;
		float levelingSearchRadius = 80.0f;
		float aimTurnHalfSeconds = 1.0f;
	};


	enum class AIState {
		Attack, // 攻撃、射撃
		KeepDistance, // 距離をとる
		Evade, // 回避
		Wander, // 徘徊
	};

	/// <summary>
	/// デストラクタ
	/// </summary>
	~Enemy();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(Object3d* object, const Vector3& position, Stage* stage);

	/// <summary>
	/// 更新
	/// </summary>
	void Update(float deltaTime);

	/// <summary>
	/// 描画
	/// </summary>
	void Draw(bool drawBody = true);
	void DrawBodyOnly();

	/// <summary>
	/// スプライト描画
	/// </summary>
	void DrawSprite();

	/// <summary>
	/// 弾発射
	/// </summary>
	void Fire();

	/// <summary>
	/// 散弾発射
	/// </summary>
	/// <param name="spreadAngle"></param>
	/// <param name="canReflect"></param>
	void ShotgunFire();

	/// <summary>
	///
	/// </summary>
	/// <param name="speed"></param>
	void ApproachToPlayer(Vector3& startPos, Vector3& targetPos);

	// 状態クラス用 Getter/Setter
	Transform& GetWorldTransform() { return worldTransform_; }

	Vector3 GetWorldPosition() const override;

	// 自キャラのセッター
	void SetPlayer(Player* player) { player_ = player; }
	void SetEnemyManager(EnemyManager* enemyManager) { enemyManager_ = enemyManager; }
	
	// セッター
	void SetWorldPosition(const Vector3& pos) {
		worldTransform_.translate = pos;
		object_->SetTransform(worldTransform_);
		object_->Update();
	}

	//----------------------
	// 定数

	// 発射間隔
	static inline const int32_t kFireInterval = 60;

	/// <summary>
	/// 衝突判定
	/// </summary>
	void OnCollision(Collider* other) override;

	float GetRadius() const override { return radius_; }

	bool GetIsDead() const { return isDead_; }

	bool IsDead() { return isDead_; }

	/// <summary>
	/// 移動の傾向
	/// </summary>
	void AIStateMovePower();

	/// <summary>
	/// ステートによる移動
	/// </summary>
	void Move(float deltaTime);
	Vector3 RandomDirection();
	
	Vector3 EvadeBullets();
	void UpdateAIState();

	AABB GetAABB();

	Vector3 GetDir() { return dir_; }

	Segment MakeForwardRay(float length) const;

	bool IsBlockNearByRay();

	Vector3 WallAvoidByRay();
	
	float ScoreDir(const Vector3& dir);

	Segment MakeRayToPlayer() const;

	bool HitPlayerByRay(const Segment& ray);

	bool HasLineOfSightToPlayer() const;
	bool HasLineOfSightToTarget(const Vector3& targetPos) const;

	Vector3 GetMove() { return velocity_; }

	void Die(); // ← プレイヤー消滅

	// 演出終了か
	bool isFinished();

	bool IsDead() const { return isDead_; }
	int GetHp() const { return hp_; }
	int GetMaxHp() const { return maxHP_; }
	void SetBossAttackConfig(const BossAttackConfig& config);
	const BossAttackConfig& GetBossAttackConfig() const { return bossAttackConfig_; }
	void SetEnemyProgressConfig(const EnemyProgressConfig& config);
	const EnemyProgressConfig& GetEnemyProgressConfig() const { return enemyProgressConfig_; }
	int GetLevel() const { return enemyLevel_; }
	uint32_t GetEnemyExp() const { return enemyExp_; }
	bool IsLevelingModeActive() const { return levelingModeActive_; }
	void RegisterExpEnemyKill(uint32_t expValue);

	void SetAttackControllerBulletManager(BulletManager* bulletManager) {
		bulletManager_ = bulletManager;
		attackController_.SetBulletManager(bulletManager);
	}

	void UpdateHPBar();

	void HPBarDraw();

private:
	struct MapIndex {
		int x = 0;
		int y = 0;
	};

	std::optional<Vector3> FindPathDirectionToPlayer();
	std::optional<Vector3> FindPathDirectionToTarget(const Vector3& targetPos);
	std::optional<MapIndex> WorldToMapIndex(const Vector3& pos) const;
	Vector3 MapIndexToWorld(const MapIndex& index) const;
	bool IsPassableCell(int x, int y) const;
	bool IsPathPassableCell(int x, int y) const;
	MapIndex FindNearestPathPassableCell(const MapIndex& base) const;
	bool HasClearMoveRouteToPlayer() const;
	bool HasClearMoveRouteToTarget(const Vector3& targetPos) const;
	Vector3 ApplyHumanLikeSteering(const Vector3& desiredDir, bool usingPath, float deltaTime);
	Vector3 ResolveMoveTargetPosition();
	void RotateTowardTarget(const Vector3& targetPos, float deltaTime);

	// ワールド変換データ
	Transform worldTransform_;
	// モデル
	Object3d* object_ = nullptr;

	// テクスチャハンドル
	uint32_t textureHandle_ = 0u;

	// 発射タイマー
	int32_t fireIntervalTimer = 0;

	// 自キャラ
	Player* player_ = nullptr;
	EnemyManager* enemyManager_ = nullptr;

	// 最初の弾までの時間
	uint32_t time_ = 60;

	float radius_ = 2.0f;

	std::string behaviorName_;
	// 弾のクールダウン
	float bulletCooldown_ = 0.5f;

	AIState aiState_ = AIState::Wander;

	float attackPower = 0.0f;
	float evadePower = 0.0f;
	float wanderPower = 0.0f;

	const float kPower = 0.1f;

	float wanderChangeTimer = 1.0f;
	Vector3 evadeVec = {0.0f, 0.0f, 0.0f};
	Vector3 wanderVec = {0.0f, 0.0f, 0.0f};

	// キャラクターの当たり判定サイズ
	static inline const float kWidth = 3.2f;
	static inline const float kHeight = 3.2f;
	
	Vector3 dir_;

	Stage* stage_ = nullptr;
	
	bool isWallFollowing_ = false;
	float wallFollowTimer_ = 0.0f;
	Vector3 wallFollowDir_;

	// 射撃感覚タイマー
	float kFireTimerMax_ = 0.15f;
	float fireTimer_ = 0.0f;
	BossAttackConfig bossAttackConfig_{};
	EnemyProgressConfig enemyProgressConfig_{};
	int enemyLevel_ = 1;
	int expEnemyKillCount_ = 0;
	uint32_t enemyExp_ = 0;
	int alternatingShotIndex_ = 0;
	bool levelingModeActive_ = false;
	Vector3 currentMoveTargetPosition_{ 0.0f, 0.0f, 0.0f };
	
	Vector3 velocity_{ 0, 0, 0 };

	float maxSpeed_ = 0.06f;
	float accel_ = 0.008f;
	float friction_ = 0.90f;
	Vector3 steeringDir_{ 1.0f, 0.0f, 0.0f };
	Vector3 steeringNoise_{ 0.0f, 0.0f, 0.0f };
	float steeringNoiseTimer_ = 0.0f;
	float hesitationTimer_ = 0.0f;
	float hesitationCooldown_ = 1.0f;
	
	std::unique_ptr<Sprite> bossHpFont;
	std::unique_ptr<Sprite> sprite;
	std::unique_ptr<Sprite> bossHpRed;

	int hp_ = 200;
	int maxHP_ = 200;
	Vector3 baseScale_{ 1.0f, 1.0f, 1.0f };
	Vector4 baseColor_{ 0.0f, 0.0f, 0.0f, 1.0f };
	float damageFeedbackTimer_ = 0.0f;
	float damageFeedbackDuration_ = 0.14f;

	bool isDead_ = false;

	bool isExploding_ = false;
	float deathEffectTimer_ = 0.0f;
	float deathChargeTimer_ = 0.0f;
	float deathChargeDuration_ = 0.28f;

	// 攻撃コントローラ
	AttackController attackController_;

	BulletManager* bulletManager_;

	// HPバーモデル
	Transform hpBarFillTransform_;
	Transform hpBarBGTransform_;

	std::unique_ptr<Object3d> hpBarFill_;
	std::unique_ptr<Object3d> hpBarBG_;


private:
	void TriggerDamageFeedback();
	void ApplyDamageFeedback(float deltaTime);
	void SpawnParticles();
	void UpdateParticles(float deltaTime = 1.0f / 60.0f);
	const float deltaTime = 1.0f / 60.0f;
};
