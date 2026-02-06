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
#include "Easing.h"


struct PlayerParticle {
	std::unique_ptr<Object3d> object;
	Vector3 velocity;
	Vector3 rotateSpeed;
	float timer;
	float lifeTime;

	float startAlpha;
};

enum class ClassType {
	
	// 第一段階
	Basic,
	
	// 第二段階
	Twin,       // 2連装
	MachineGun, // バラつき・高速連射
	Overseer,   // ドローン使い

	// 第三段階
	Triple,     // 三連砲
	Assassin,   // ステルスで移動できる
	Bounder,      // 反射弾を使える

	// 第四段階
	Ninja,      // ステルスしながら攻撃
	Smasher,    // 強力な近接攻撃
	Summoner,
};
struct TankSeed {
	ClassType type;
	std::string name;
	int requiredRank;
	std::string texturePath;
};
struct TankData {
	ClassType type;
	std::string name;
	int requiredRank;
	std::string texturePath; // 画像パス
	std::unique_ptr<Sprite> sprite; // 各戦車専用のスプライト
};

class Stage;

/// <summary>
/// 自キャラ
/// </summary>
class Player : public Collider {

public:

	struct PlayerStats {
		float reloadSpeed = 10.0f;    // 連射速度（小さいほど速い）
		float bulletDamage = 1.0f;    // 弾の威力
		float bulletSpeed = 0.3f;     // 弾速
		float moveSpeed = 0.2f;       // 移動速度
		float maxHp = 3.0f;           // 最大HP
		float staminaRecovery = 1.0f; // スタミナ回復速度
		float stamina = 3.0f;         // スタミナ
		float maxStamina = 3.0f;      // スタミナ最大値
		float bodyDamage = 3.0f;      // 直接ダメージ
	};

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
	void Update(Camera* viewProjection, Stage& stage, BulletManager* BulletManager, float deltaTime);

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
	void Attack(BulletManager* BulletManager, float deltaTime);

	/// <summary>
	/// ドローン発射
	/// </summary>
	void DroneShoot(BulletManager* BulletManager);

	/// <summary>
	/// チャージ突進攻撃
	/// </summary>
	void Smash(float deltaTime);

	Sphere GetSphere() const;

	// 経験値を加算する関数
	void AddExp(int amount);
	int GetLevel() const { return level_; }

	bool RequestSlow();

	void InitializeEncyclopedia();

	void UpdateEncyclopedia();

	void DrawEncyclopedia();

	int GetRankFromLevel(int level);

	bool IsChangeMode() { return isChangeMode; }

private:
	// ワールド変換データ
	Transform worldTransform_;

	// モデル
	Object3d* object_ = nullptr;
	//// 砲塔モデル
	//Object3d* object_ = nullptr;
	// テクスチャハンドル
	uint32_t textureHandle_ = 0u;

	// キーボード入力
	Input* input_ = nullptr;

	Vector3 dir_;

	// キャラクターの移動速さ
	float kCharacterSpeed = 0.2f;
	Vector3 move_;

	const int kBulletTime = 10;
	float bulletCoolTime = 0.0f;

	// キャラクターの当たり判定サイズ
	static inline const float kWidth = 1.6f;
	static inline const float kHeight = 1.6f;

	Vector3 velocity_{ 0, 0, 0 };   // 現在速度
	Vector3 inputDir_{ 0, 0, 0 };   // 入力方向

	float maxSpeed_ = 0.15f;        // 最高速度
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


	std::vector<PlayerParticle> ps_;


	// 攻撃コントローラ
	AttackController attackController_;

	// プレイヤーのドローン
	std::vector<std::unique_ptr<PlayerDrone>> drones_;

	// プレイヤーの経験値とレベル
	int exp_ = 0;
	int level_ = 1;
	// 次のレベルまでの必要経験値
	int nextLevelExp_ = 0;
	// 最大レベル
	const int kMaxLevel = 15;
	// 次のレベルに必要な経験値を計算する
	int GetNextLevelExp() const;

	PlayerStats stats_;
	ClassType currentClass_ = ClassType::Basic;

	// 進化させる関数
	void Evolve(ClassType newClass);
	void SpawnCasing();
	int shootBarrelIndex_ = 0; // 次に撃つ砲身の番号

	const int maxEnhancePoint = 5;
	void UpdateStealth(float deltaTime);
	void UpdateSummoner(float deltaTime);
private:
	void SpawnParticles();
	void UpdateParticles(float deltaTime = 1.0f / 60.0f);

	void SpawnAfterimage();
	void UpdateP(float deltaTime);
	void SpawnBuffParticle();
	Vector2 WorldToScreen(const Vector3& worldPos, Camera* camera);

	float angle_ = 0.0f;

	// ダッシュ関連
	bool isDashing_ = false;
	float dashTimer_ = 0.0f;           // ダッシュ持続時間
	float dashCooldown_ = 0.0f;        // 再使用までの時間
	const float kDashDuration = 0.3f; // ダッシュしている時間（秒）
	const float kDashCooldown = 0.3f;  // クールタイム（秒）
	const float kDashSpeed = 0.3f;     // ダッシュの強さ

	// ジャスト回避ができるかのスタミナ
	float justEvadeStamina_ = 3.0f;

	// ジャスト回避判定用
	const float kJustEvadeWindow = 0.2f; // ダッシュ開始から何秒間が「ジャスト」か

	// ジャスト回避したか
	bool isJustEvaded_ = false;

	// スロー要求
	bool requestSlow_ = false;

	// デルタタイム
	float dt_ = 1.0f / 60.0f;

	float buffTimer_ = 0.0f;       // バフの残り時間
	bool isBuffActive_ = false;    // バフ中かどうかのフラグ
	const float kBuffDuration = 2.0f; // バフの持続時間（3秒）

	float smashCharge_ = 0.0f; // スマッシュチャージ時間
	float maxCharge_ = 1.0f;
	bool isSmash_ = false; // スマッシュ中かどうか
	Vector3 smashDir_;

	std::unique_ptr<Sprite> machineGunBtnSprite_ = nullptr; // ボタンの見た目
	Vector2 btnPos_ = { 50.0f, 200.0f };  // ボタンの位置（画面左下あたり）
	Vector2 btnSize_ = { 100.0f, 50.0f }; // ボタンのサイズ

	// 図鑑の並び順（表示したい順番に定義）
	std::vector<TankData> encyclopedia_;

	Vector2 mousePosition_;

	bool isChangeMode = false;

	std::unique_ptr<Sprite> sprite;

	float stealthAlpha_ = 1.0f;      // ステルス時の透明度 (1.0:不透明, 0.0:透明)
	bool isStealth_ = false;         // 現在ステルス中か
	float stealthTimer_ = 0.0f;      // ステルス移行までの待機時間

	// Summoner用：ドローンリスト
	const int kMaxSummonerDrones = 4;

	float summonTimer_;
};

