#pragma once
#include "game/weapon/CombatTypes.h"
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
#include "game/player/TankSpecialCombat.h"

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
	void Attack(float deltaTime = 1.0f / 60.0f);
	void ConfigureRunAttack(const AttackParam& param, float reloadSeconds);
	void SetRunExactDamage(float amount) {runExactDamage_=(std::max)(0.0f,amount);}
	void SetRunInput(const cg2::Vector3& target, bool attack) {
		runInputOverride_ = true; runAimTarget_ = target; runWantsAttack_ = attack;
	}
	void SetRunFollowOffset(const cg2::Vector3& offset) { runFollowOffset_ = offset; }
	void SetRunFollowTuning(float speed,float catchup,float response) {
		runFollowSpeed_=speed;runCatchupSpeed_=catchup;runFollowResponse_=response;
	}
	void RallyRunAttack() { runShotCooldown_ = 0.0f; runRallyShotPending_ = true; }
	void SetRunOwner(Player* player,int index) {runAttackParam_.sourcePlayer=player;runAttackParam_.sourceDroneIndex=index;}
	bool StartRunMission(const cg2::Vector3& target,bool bomb) {
		if(!mission_.Start(bomb))return false;missionTarget_=target;return true;
	}
	const tankspecial::DroneMission& GetRunMission() const {return mission_;}
	const cg2::Vector3& GetRunMissionTarget() const {return missionTarget_;}
	bool IsRunAvailable() const {return !isDead_&&mission_.Available();}
	bool ConsumeRunMissionImpact() { return mission_.ConsumeImpact(); }
	bool ConsumeRunRebuilt() {const bool value=rebuilt_;rebuilt_=false;return value;}

	/// <summary>
	/// マウスの方を向く
	/// </summary>
	/// <param name="viewProjection"></param>
	void RotateToMouse(cg2::Camera* viewProjection);

	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="model">モデル</param>
	/// <param name="camera">カメラ</param>
	/// <param name="position">初期座標</param>
	void Initialize(const cg2::Vector3& position, const cg2::Vector3& velocity);

	/// <summary>
	/// 更新
	/// </summary>
	void Update(cg2::Camera* viewProjection, Stage& stage, const cg2::Vector3& playerPosition, float deltaTime = 1.0f / 60.0f);

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();
	// The arena renders companions with the same authored neon geometry as tanks.
	void SetNeonVisual(bool enabled) { neonVisual_ = enabled; }
	bool UsesNeonVisual() const { return neonVisual_; }
	bool IsVisualVisible() const;
	const cg2::Vector3& GetAimDirection() const { return dir; }
	float GetNeonMuzzleFlashRatio() const {
		if (!runAttackEnabled_ || runShotCooldown_ <= 0.0f) return 0.0f;
		const float flashSeconds = (std::min)(0.075f, runReloadSeconds_);
		return (std::clamp)((runShotCooldown_ - runReloadSeconds_ + flashSeconds) / flashSeconds, 0.0f, 1.0f);
	}

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

	cg2::Vector3 GetWorldPosition() const override;

	cg2::Vector3 GetMove() { return velocity_; }
	void SetVelocity(const cg2::Vector3& v) { velocity_ = v; }

	// セッター
	void SetWorldPosition(const cg2::Vector3& pos) {
		worldTransform_.translate = pos;
		object_->SetTransform(worldTransform_);
		object_->Update();
	}

	cg2::AABB GetAABB();

	void Damage();

	void Die();

	bool IsDead() const { return isDead_; }
	int GetHp() const { return hp_; }
	int GetMaxHp() const { return kMaxHp; }

	void SetAttackControllerBulletManager(BulletManager* bulletManager) {
		attackController_.SetBulletManager(bulletManager);
		runBulletManager_ = bulletManager;
	}

private:
	static inline const int kMaxHp = 10;

	// ワールド変換データ
	cg2::Transform worldTransform_;

	// モデル
	std::unique_ptr<cg2::Object3d> object_;
	// テクスチャハンドル
	uint32_t textureHandle_ = 0u;

	// キーボード入力
	cg2::Input* input_ = nullptr;

	// 弾
	cg2::Vector3 dir;

	// キャラクターの移動速さ
	float kCharacterSpeed = 0.2f;
	cg2::Vector3 move_;

	const int kBulletTime = 30;
	int bulletCoolTime = 0;
	bool runAttackEnabled_ = false;
	AttackParam runAttackParam_{};
	float runExactDamage_=0,runDamageRemainder_=0;
	float runReloadSeconds_ = 0.5f;
	float runShotCooldown_ = 0.0f;
	bool runRallyShotPending_ = false;
	BulletManager* runBulletManager_ = nullptr;
	bool runInputOverride_ = false, runWantsAttack_ = false;
	cg2::Vector3 runAimTarget_{}, runFollowOffset_{};
	float runFollowSpeed_=0.25f,runCatchupSpeed_=0.62f,runFollowResponse_=5.0f;

	// キャラクターの当たり判定サイズ
	static inline const float kWidth = 1.6f;
	static inline const float kHeight = 1.6f;

	cg2::Vector3 velocity_{ 0, 0, 0 };   // 現在速度
	cg2::Vector3 inputDir_{ 0, 0, 0 };   // 入力方向

	float maxSpeed_ = 0.25f;        // 最高速度
	float accel_ = 0.7f;         // 加速
	float decel_ = 3.5f;         // 減速（ブレーキ）

	cg2::Transform hpTransform_;

	int hp_ = kMaxHp;

	// 無敵時間
	float invincibleTimer_ = 0.0f;

	bool isDead_ = false;
	bool neonVisual_ = false;
	tankspecial::DroneMission mission_{};
	cg2::Vector3 missionTarget_{};
	bool rebuilt_=false;

	// 攻撃コントローラ
	AttackController attackController_;

	float angle_ = 0.0f;
};

