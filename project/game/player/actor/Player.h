#pragma once
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <array>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "Calculation.h"
#include "Collider.h"
#include "CollisionConfig.h"
#include "Object3d.h"
#include "Sprite.h"
#include "TextLabel.h"
#include "AttackController.h"
#include "PlayerDrone.h"
#include "Easing.h"
#include "ParticleManager.h"
#include "game/weapon/WeaponMount.h"

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
	std::string classId;
	std::string name;
	int requiredRank;
	std::string texturePath; // 画像パス
	std::unique_ptr<Sprite> cardSprite;
	std::unique_ptr<Sprite> sprite; // 各戦車専用のスプライト
	std::unique_ptr<TextLabel> nameLabel;
	std::unique_ptr<TextLabel> rankLabel;
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
		float maxHp = 10000.0f;        // 最大HP
		float staminaRecovery = 1.0f; // スタミナ回復速度
		float stamina = 3.0f;         // スタミナ
		float maxStamina = 3.0f;      // スタミナ最大値
		float bodyDamage = 3.0f;      // 直接ダメージ
	};

	struct BalanceConfig {
		int maxHp = 10000;
		float reloadSpeed = 10.0f;
		float bulletDamage = 1.0f;
		float bulletSpeed = 0.3f;
		float moveSpeed = 0.2f;
		float staminaRecovery = 1.0f;
		float maxStamina = 3.0f;
		uint32_t bodyDamage = 3;
		float healthRegenUpgrade = 0.08f;
		float maxHpUpgradeAmount = 0.10f;
		float bodyDamageUpgrade = 0.10f;
		float bulletSpeedUpgrade = 0.08f;
		float bulletDamageUpgrade = 0.10f;
		float reloadUpgrade = 0.07f;
		float moveSpeedUpgrade = 0.06f;
		float minReloadSpeed = 3.0f;
		bool healToFull = false;
	};
	struct LaserShotEvent {
		Vector3 origin{};
		Vector3 direction{ 1.0f, 0.0f, 0.0f };
		float range = 18.0f;
		float width = 0.18f;
		float duration = 0.12f;
		float damageInterval = 0.08f;
		uint32_t damage = 1;
		Vector4 color{ 0.25f, 1.0f, 0.95f, 1.0f };
	};
	struct MineDropEvent {
		Vector3 position{};
		float radius = 3.2f;
		float fuseTime = 0.45f;
		float lifeTime = 5.0f;
		uint32_t damage = 1;
		Vector4 color{ 1.0f, 0.25f, 0.95f, 1.0f };
	};
	struct MeleeSlashEvent {
		Vector3 origin{};
		Vector3 direction{ 1.0f, 0.0f, 0.0f };
		float range = 3.4f;
		float arcDeg = 105.0f;
		float width = 0.20f;
		float duration = 0.18f;
		uint32_t damage = 1;
		Vector4 color{ 0.55f, 1.25f, 1.0f, 1.0f };
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
	void Draw(bool drawBody = true);
	void DrawBodyOnly();

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

	void Damage(uint32_t amount = kDamageBlockDamage);
	void TakeDamage(uint32_t amount, float invincibleTime = 0.45f);
	void ApplyBalanceConfig(const BalanceConfig& config);
	void SetDebugNoDamage(bool enabled) { debugNoDamage_ = enabled; }
	bool IsDebugNoDamage() const { return debugNoDamage_; }

	void Die(); // ← プレイヤー消滅

	// 演出終了か
	bool isFinished();

	bool IsDead() const { return isDead_; }
	int GetHp() const { return hp_; }
	int GetMaxHp() const { return static_cast<int>(stats_.maxHp); }

	bool IsOnGround() const { return isOnGround_; }
	void SetOnGround(bool onGround) { isOnGround_ = onGround; }

	float GetAngle() const { return angle_; }
	const Vector3& GetDirection() const { return dir_; }
	bool IsDashing() const { return isDashing_; }
	struct NeonBarrelLayout {
		Vector3 offset{};
		Vector3 scale{ 1.25f, 0.24f, 0.24f };
		float angleRad = 0.0f;
		float recoilOffset = 0.0f;
	};
	std::vector<NeonBarrelLayout> GetNeonBarrelLayouts() const;
	float GetDamageFeedbackRatio() const;

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
	int GetExp() const { return exp_; }
	int GetNextLevelExpValue() const { return nextLevelExp_; }
	int GetSkillPoints() const { return skillPoints_; }
	int GetUpgradeLevel(int index) const;
	const char* GetCurrentClassName() const;
	bool ApplyStatUpgrade(int index);
	bool RefundStatUpgrade(int index);
	const PlayerStats& GetStats() const { return stats_; }

	bool RequestSlow();

	void InitializeEncyclopedia();

	void UpdateEncyclopedia();

	void DrawEncyclopedia();

	void DrawTankCodex();

	void DrawPlayerClassEditor();
	void DrawUpgradeHudDebugImGui();
	struct UiProfileStats {
		float updateMs = 0.0f;
		float spriteMs = 0.0f;
		float textMs = 0.0f;
		float totalMs = 0.0f;
		int spriteDraws = 0;
		int textDraws = 0;
		bool visible = false;
	};
	const UiProfileStats& GetUpgradeHudProfileStats() const { return upgradeHudProfile_; }
	const UiProfileStats& GetEvolutionUiProfileStats() const { return evolutionUiProfile_; }
	std::vector<LaserShotEvent> ConsumeLaserShotEvents();
	std::vector<MineDropEvent> ConsumeMineDropEvents();
	std::vector<MeleeSlashEvent> ConsumeMeleeSlashEvents();

	int GetRankFromLevel(int level);

	bool IsChangeMode() { return isChangeMode; }

private:
	struct PlayerClassConfig {
		ClassType type = ClassType::Basic;
		std::string id = "Basic";
		std::string displayName = "Basic";
		int requiredRank = 1;
		bool usesDrone = false;
		int maxDrones = 7;
		float reloadScale = 1.0f;
		float bulletSpeedScale = 1.0f;
		float bulletDamageScale = 1.0f;
		int bulletCount = 1;
		float spreadAngleDeg = 10.0f;
		bool randomSpread = true;
		bool reflect = false;
		bool penetrate = false;
		bool fireAllBarrels = false;
		bool alternateBarrels = false;
		float recoilPower = 0.01f;
		std::vector<WeaponMountConfig> barrels;
	};

	// ワールド変換データ
	Transform worldTransform_;

	// モデル
	Object3d* object_ = nullptr;
	struct BarrelModel {
		std::unique_ptr<Object3d> object;
		Transform transform;
		Vector3 localOffset;
		float recoilOffset = 0.0f;
	};
	std::vector<BarrelModel> barrels_;
	std::unordered_map<std::string, PlayerClassConfig> classConfigs_;
	std::vector<std::string> classOrder_;
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
	static constexpr uint32_t kDamageBlockDamage = 75;
	int hp_ = 10000;

	// 無敵時間
	float invincibleTimer_ = 0.0f;
	bool debugNoDamage_ = false;
	float damageFeedbackTimer_ = 0.0f;
	float damageFeedbackDuration_ = 0.18f;
	Vector4 baseVehicleColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 baseBarrelColor_{ 0.48f, 0.86f, 0.22f, 1.0f };

	bool isDead_ = false;

	bool isExploding_ = false;
	float deathEffectTimer_ = 0.0f;
	float deathChargeTimer_ = 0.0f;
	float deathChargeDuration_ = 0.28f;


	// 攻撃コントローラ
	AttackController attackController_;

	// プレイヤーのドローン
	std::vector<std::unique_ptr<PlayerDrone>> drones_;
	std::vector<LaserShotEvent> pendingLaserShots_;
	std::vector<MineDropEvent> pendingMineDrops_;
	std::vector<MeleeSlashEvent> pendingMeleeSlashes_;

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
	PlayerStats baseStats_;
	float healthRegenUpgradeRate_ = 0.08f;
	float maxHpUpgradeRate_ = 0.10f;
	float bodyDamageUpgradeRate_ = 0.10f;
	float bulletSpeedUpgradeRate_ = 0.08f;
	float bulletDamageUpgradeRate_ = 0.10f;
	float reloadUpgradeRate_ = 0.07f;
	float moveSpeedUpgradeRate_ = 0.06f;
	float minReloadSpeed_ = 3.0f;
	ClassType currentClass_ = ClassType::Basic;
	std::string currentClassId_ = "Basic";

	// 進化させる関数
	void Evolve(ClassType newClass);
	void EvolveById(const std::string& classId);
	void LoadPlayerClassConfigs(const std::string& path = "resources/configs/playerClasses.json");
	void SavePlayerClassConfigs(const std::string& path = "resources/configs/playerClasses.json") const;
	PlayerClassConfig CreateDefaultClassConfig(ClassType type) const;
	const PlayerClassConfig* GetClassConfig(ClassType type) const;
	const PlayerClassConfig* GetClassConfig(const std::string& classId) const;
	const PlayerClassConfig* GetCurrentClassConfig() const;
	PlayerClassConfig* GetMutableClassConfig(const std::string& classId);
	bool FireConfiguredClass(const PlayerClassConfig& config, BulletManager* bulletManager, float baseReload, Vector3& recoilDir, float& recoilPower);
	Vector3 RotateDirection(const Vector3& direction, float angleDeg) const;
	void InitializeBarrels();
	void UpdateBarrelLayout();
	void DrawBarrels();
	void SetVehicleAlpha(float alpha);
	void TriggerDamageFeedback();
	void InitializeUpgradeHud();
	void UpdateUpgradeHud();
	void DrawUpgradeHud();
	void InitializeUpgradeHudBatch();
	void DrawUpgradeHudRectBatch(bool showUpgradeList, float expRatio, float levelRatio, float listAlpha, float listOffsetX);
	void QueueUpgradeHudRect(std::vector<TrailVertex>& vertices, const Vector2& pos, const Vector2& size, const Vector4& color) const;
	void ApplyUpgradeHudLayout();
	bool LoadUpgradeHudConfig(const std::string& path = "resources/configs/playerUpgradeHud.json");
	bool SaveUpgradeHudConfig(const std::string& path = "resources/configs/playerUpgradeHud.json") const;
	void RecalculateStatsFromBase(bool healToFull);
	void SpawnCasing();
	int shootBarrelIndex_ = 0; // 次に撃つ砲身の番号

	const int maxEnhancePoint = 5;
	std::array<int, 7> upgradeLevels_{};
	int skillPoints_ = 0;
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
	float movementParticleTimer_ = 0.0f;
	float movementParticleInterval_ = 0.08f;
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
	std::unique_ptr<Sprite> evolutionBackdropSprite_;
	std::unique_ptr<Sprite> evolutionPreviewPanelSprite_;
	std::unique_ptr<Sprite> evolutionStatsPanelSprite_;
	std::unique_ptr<Sprite> evolutionPreviewTankSprite_;
	std::unique_ptr<Sprite> evolutionShotSprite_;
	std::unique_ptr<Sprite> evolutionChangeButtonSprite_;
	std::unique_ptr<TextLabel> evolutionTitleLabel_;
	std::unique_ptr<TextLabel> evolutionHintLabel_;
	std::unique_ptr<TextLabel> evolutionPreviewNameLabel_;
	std::unique_ptr<TextLabel> evolutionRoleLabel_;
	std::unique_ptr<TextLabel> evolutionChangeButtonLabel_;
	std::array<std::unique_ptr<TextLabel>, 9> evolutionStatLabels_;
	std::unique_ptr<Sprite> upgradeHudBackdropSprite_;
	std::unique_ptr<Sprite> upgradeHudExpBackSprite_;
	std::unique_ptr<Sprite> upgradeHudExpFillSprite_;
	std::unique_ptr<Sprite> upgradeHudLevelBackSprite_;
	std::unique_ptr<Sprite> upgradeHudLevelFillSprite_;
	std::unique_ptr<TextLabel> upgradeHudTitleLabel_;
	std::unique_ptr<TextLabel> upgradeHudPointLabel_;
	std::unique_ptr<TextLabel> upgradeHudExpLabel_;
	std::unique_ptr<TextLabel> upgradeHudLevelLabel_;
	std::unique_ptr<TextLabel> upgradeHudListLabel_;
	std::array<std::unique_ptr<Sprite>, 7> upgradeHudButtonSprites_;
	std::array<std::unique_ptr<Sprite>, 7> upgradeHudPlusSprites_;
	std::array<std::unique_ptr<Sprite>, 7> upgradeHudMinusSprites_;
	std::array<std::unique_ptr<TextLabel>, 7> upgradeHudNameLabels_;
	std::array<std::unique_ptr<TextLabel>, 7> upgradeHudLevelLabels_;
	std::array<std::unique_ptr<TextLabel>, 7> upgradeHudMinusLabels_;
	std::array<std::unique_ptr<TextLabel>, 7> upgradeHudPlusLabels_;
	Microsoft::WRL::ComPtr<ID3D12Resource> upgradeHudBatchVertexResource_;
	D3D12_VERTEX_BUFFER_VIEW upgradeHudBatchVertexBufferView_{};
	TrailVertex* upgradeHudBatchVertexData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> upgradeHudBatchTransformResource_;
	Matrix4x4* upgradeHudBatchTransformData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> upgradeHudBatchMaterialResource_;
	Material* upgradeHudBatchMaterialData_ = nullptr;
	static constexpr uint32_t kUpgradeHudBatchMaxVertices = 256;
	std::array<float, 7> upgradeHudFlashTimers_{};
	std::array<float, 7> upgradeHudRefundFlashTimers_{};
	std::array<float, 7> upgradeHudMissFlashTimers_{};
	float upgradeHudListVisibility_ = 0.0f;
	float upgradeHudListAnimSpeed_ = 10.0f;
	float upgradeHudListSlideDistance_ = 260.0f;
	bool upgradeHudMouseCaptured_ = false;
	bool upgradeHudVisible_ = true;
	bool upgradeHudHideListWithoutPoints_ = true;
	bool upgradeHudDrawListPanels_ = true;
	bool upgradeHudDrawListText_ = true;
	bool upgradeHudDrawBottomBars_ = true;
	bool upgradeHudDrawBottomText_ = true;
	bool upgradeHudUseRectBatch_ = true;
	Vector2 upgradeHudPanelPos_ = { 18.0f, 338.0f };
	Vector2 upgradeHudPanelSize_ = { 340.0f, 260.0f };
	Vector2 upgradeHudRowStart_ = { 30.0f, 384.0f };
	Vector2 upgradeHudButtonSize_ = { 286.0f, 22.0f };
	Vector2 upgradeHudPlusSize_ = { 24.0f, 22.0f };
	float upgradeHudRowGap_ = 29.0f;
	float upgradeHudNameX_ = 44.0f;
	float upgradeHudLevelX_ = 184.0f;
	float upgradeHudMinusX_ = 268.0f;
	float upgradeHudPlusX_ = 304.0f;
	float upgradeHudMinusLabelX_ = 277.0f;
	float upgradeHudPlusLabelX_ = 313.0f;
	float upgradeHudNameTextOffsetY_ = 3.0f;
	float upgradeHudLevelTextOffsetY_ = 3.0f;
	float upgradeHudMinusTextOffsetY_ = 1.0f;
	float upgradeHudPlusTextOffsetY_ = 1.0f;
	Vector2 upgradeHudTitlePos_ = { 32.0f, 350.0f };
	Vector2 upgradeHudPointPos_ = { 286.0f, 352.0f };
	Vector2 upgradeHudLevelBarPos_ = { 415.0f, 656.0f };
	Vector2 upgradeHudLevelBarSize_ = { 450.0f, 16.0f };
	Vector2 upgradeHudLevelTextPos_ = { 565.0f, 653.0f };
	Vector2 upgradeHudExpBarPos_ = { 390.0f, 680.0f };
	Vector2 upgradeHudExpBarSize_ = { 500.0f, 20.0f };
	Vector2 upgradeHudExpTextPos_ = { 560.0f, 677.0f };
	std::string upgradeHudConfigStatus_;
	UiProfileStats upgradeHudProfile_{};
	UiProfileStats evolutionUiProfile_{};
	int cachedUpgradeHudExp_ = -1;
	int cachedUpgradeHudNextExp_ = -1;
	int cachedUpgradeHudLevel_ = -1;
	int cachedUpgradeHudSkillPoints_ = -1;
	std::string cachedUpgradeHudClassName_;
	std::array<int, 7> cachedUpgradeHudLevels_{ -1, -1, -1, -1, -1, -1, -1 };
	bool cachedUpgradeHudListVisible_ = false;

	Vector2 mousePosition_;

	bool isChangeMode = false;
	int evolutionSelectedIndex_ = 0;
	float evolutionUiTimer_ = 0.0f;
	int editorSelectedClassIndex_ = 0;
	int codexSelectedClassIndex_ = 0;
	float codexPreviewTimer_ = 0.0f;
	float codexPreviewAimDeg_ = 0.0f;
	bool codexPreviewAutoMove_ = true;
	bool codexPreviewAutoFire_ = true;

	std::unique_ptr<Sprite> sprite;

	float stealthAlpha_ = 1.0f;      // ステルス時の透明度 (1.0:不透明, 0.0:透明)
	bool isStealth_ = false;         // 現在ステルス中か
	float stealthTimer_ = 0.0f;      // ステルス移行までの待機時間

	// Summoner用：ドローンリスト
	const int kMaxSummonerDrones = 4;

	float summonTimer_;

};

