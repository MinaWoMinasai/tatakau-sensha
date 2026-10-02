#pragma once
#include "game/weapon/CombatTypes.h"
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
#include "game/player/PlayerClassCatalog.h"
#include "game/ui/TankButtonUI.h"
#include "game/ui/NeonProgressBar.h"
#include "game/ui/NeonSegmentedBar.h"
#include "game/ui/NeonTextEffect.h"
#include "game/player/TankRunModifiers.h"
#include "game/player/TankSpecialCombat.h"
#include "game/player/TankCombatStyleBalance.h"
#include "game/player/TankExpeditionLoadout.h"
#include "game/run/TankExpeditionContent.h"
#include "game/run/TankBuildStyle.h"

class EnemyManager;
class Enemy;
class ExpEnemy;

/// @brief 図鑑に登録する機体の識別子・名前・解放条件・画像パスを表す。
struct TankSeed {
    ClassType type;
    std::string name;
    int requiredRank;
    std::string texturePath;
};
/// @brief 図鑑の機体データと表示用スプライト・文字を保持する。
struct TankData {
    ClassType type;
    std::string classId;
    std::string name;
    int requiredRank;
    std::string texturePath; // 画像パス
    std::unique_ptr<cg2::Sprite> cardSprite;
    std::unique_ptr<cg2::Sprite> sprite; // 各戦車専用のスプライト
    std::unique_ptr<cg2::TextLabel> nameLabel;
    std::unique_ptr<cg2::TextLabel> rankLabel;
};

/// @brief 遠征中に選べる進化先の識別子と説明を表す。
struct RunEvolutionChoice {
    std::string id;
    std::string name;
    std::string description;
};

class Stage;
namespace cg2 {
class ObjectPostEffect;
}

/// @brief 自機の移動・攻撃・装備と成長状態を管理する。機体の制作データはPlayerClassCatalogから取得する。
class Player : public Collider {

public:
    using BodyShape = PlayerBodyShape;

    /// @brief 自機の基礎性能を保持する。reloadSpeedは60FPS相当の基準フレーム数で、小さいほど連射が速い。
    struct PlayerStats {
        float reloadSpeed = 10.0f;    // 連射速度（小さいほど速い）
        float bulletDamage = 1.0f;    // 弾の威力
        float bulletSpeed = 0.3f;     // 弾速
        float moveSpeed = 0.2f;       // 移動速度
        float maxHp = 10000.0f;       // 最大HP
        float staminaRecovery = 1.0f; // スタミナ回復速度
        float stamina = 3.0f;         // スタミナ
        float maxStamina = 3.0f;      // スタミナ最大値
        float bodyDamage = 3.0f;      // 直接ダメージ
    };

    /// @brief ゲームの性能調整用の数値をまとめる。実行中の状態とは分けて扱う。
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
    /// @brief レーザー発射の位置・方向・性能を戦闘側へ渡す。
    struct LaserShotEvent {
        cg2::Vector3 origin{};
        cg2::Vector3 direction{1.0f, 0.0f, 0.0f};
        float range = 18.0f;
        float width = 0.18f;
        float duration = 0.12f;
        float damageInterval = 0.08f;
        uint32_t damage = 1;
        cg2::Vector4 color{0.25f, 1.0f, 0.95f, 1.0f};
    };
    /// @brief 地雷の配置位置と性能を戦闘側へ渡す。
    struct MineDropEvent {
        cg2::Vector3 position{};
        float radius = 3.2f;
        float fuseTime = 0.45f;
        float lifeTime = 5.0f;
        uint32_t damage = 1;
        cg2::Vector4 color{1.0f, 0.25f, 0.95f, 1.0f};
    };
    /// @brief 近接斬撃の範囲と威力を戦闘側へ渡す。
    struct MeleeSlashEvent {
        cg2::Vector3 origin{};
        cg2::Vector3 direction{1.0f, 0.0f, 0.0f};
        float range = 3.4f;
        float arcDeg = 105.0f;
        float width = 0.20f;
        float duration = 0.18f;
        float windupDuration = 0.08f;
        float recoveryDuration = 0.10f;
        int comboStep = 0;
        float knockback = 0.16f;
        uint32_t damage = 1;
        cg2::Vector4 color{0.55f, 1.25f, 1.0f, 1.0f};
    };
    /// @brief ダッシュ衝突の位置・方向・強度を戦闘と演出へ渡す。
    struct DashImpactEvent {
        cg2::Vector3 origin{}, direction{1.0f, 0.0f, 0.0f};
        bool boss = false;
        bool powered = false;
    };
    /// @brief ドローン間のレーザー接続の端点と威力を表す。
    struct DroneLaserLink {
        cg2::Vector3 start{}, end{};
        bool contact = false;
    };
    enum class SpecialEventKind {
        RailShot,
        Parry,
        PerfectParry,
        LinkHit,
        DroneCharge,
        DroneBomb,
        DroneRebuild,
        TargetLock,
        DashSlash,
        SpinBlade,
        WallSmash
    };
    /// @brief 特殊戦闘の発動結果をシーン側へ渡す。
    struct SpecialCombatEvent {
        SpecialEventKind kind = SpecialEventKind::RailShot;
        cg2::Vector3 origin{}, direction{1, 0, 0};
        float strength = 1;
    };
    /// @brief 特殊戦闘の発動・命中などの件数を集計する。
    struct SpecialCombatStats {
        uint32_t railShots = 0, slashWaves = 0, parries = 0, perfectParries = 0, linkTicks = 0;
        uint32_t droneCharges = 0, droneChargeHits = 0, droneBombs = 0, droneRebuilds = 0, targetLocks = 0, spreadTargets = 0;
        uint32_t dashSlashes = 0, dashSlashHits = 0, spinTicks = 0, wallSmashes = 0;
    };
    /// @brief ドローン能力の状態を表示するための位置・進行度を表す。
    struct DroneAbilityVisual {
        cg2::Vector3 position{}, target{};
        tankspecial::DronePhase phase = tankspecial::DronePhase::Escort;
        float progress = 0;
        bool bomb = false;
    };
    /// @brief 照準固定対象とロック進行度を表示へ渡す。
    struct TargetLockVisual {
        cg2::Vector3 position{};
        int stacks = 0;
        float remaining = 0;
    };
    /// @brief ドローンAbility表示情報を返す。
    std::vector<DroneAbilityVisual> GetDroneAbilityVisuals() const;
    /// @brief 対象Lock表示情報を返す。
    const std::vector<TargetLockVisual>& GetTargetLockVisuals() const
    {
        return targetLockVisuals_;
    }
    /// @brief Spin刃比率を返す。
    float GetSpinBladeRatio() const
    {
        return spinCycle_.remaining / .8f;
    }
    /// @brief EMP比率を返す。
    float GetEmpRatio() const
    {
        return empJammerTimer_ / 2.5f;
    }
    /// @brief EMPJammerを現在の状態へ適用する。
    void ApplyEmpJammer(float seconds = 2.5f)
    {
        if (runModifiers_.enabled)
            empJammerTimer_ = tankspecial::EmpDuration(empJammerTimer_, seconds);
    }
    /// @brief ドローン命中を通知する。
    uint32_t NotifyDroneHit(int drone, Collider* target, uint32_t originalDamage);
    /// @brief ドローン対象ダメージ倍率を返す。
    float GetDroneTargetDamageScale(const Collider* target, bool boss) const;
    /// @brief 壁への体当たりで破壊する候補を記録する。
    void ArmWallSmash(ExpEnemy* target, float strength = 1);
    /// @brief レール砲チャージ比率を返す。
    float GetRailChargeRatio() const
    {
        return railCharge_.held ? railCharge_.seconds : 0.0f;
    }
    /// @brief レール砲チャージ銃口を返す。
    cg2::Vector3 GetRailChargeMuzzle() const;
    /// @brief ドローンレーザーLinksを返す。
    const std::vector<DroneLaserLink>& GetDroneLaserLinks() const
    {
        return droneLaserLinks_;
    }
    /// @brief 特殊戦闘集計値を返す。
    const SpecialCombatStats& GetSpecialCombatStats() const
    {
        return specialCombatStats_;
    }
    /// @brief 特殊戦闘イベントの未処理分を取り出し、内部の保留分を消費済みにする。
    std::vector<SpecialCombatEvent> ConsumeSpecialCombatEvents();
    // Called once after bullet movement and before body/bullet collision resolution.
    /// @brief 特殊戦闘を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateSpecialCombat(Stage& stage, BulletManager* bullets, Enemy* boss, EnemyManager* enemies, float dt);
    /// @brief 衝撃をダッシュによる処理を試みる。
    bool TryDashImpact(Collider* target);
    /// @brief ダッシュ衝撃イベントの未処理分を取り出し、内部の保留分を消費済みにする。
    std::vector<DashImpactEvent> ConsumeDashImpactEvents();
    /// @brief ダッシュ開始済み件数を返す。
    uint32_t GetDashStartedCount() const
    {
        return dashStartedCount_;
    }
    /// @brief ダメージTaken件数を返す。
    uint32_t GetDamageTakenCount() const
    {
        return damageTakenCount_;
    }
    /// @brief 主攻撃攻撃件数を返す。
    uint32_t GetPrimaryAttackCount() const
    {
        return primaryAttackCount_;
    }
    /// @brief 近接攻撃強化ビルドであるか判定する。
    bool IsMeleeBuild() const
    {
        return runModifiers_.enabled &&
               (expeditionCombatStyleSelected_ ? expeditionCombatStyle_ == tankbuild::Style::Melee : runModifiers_.meleeBlade);
    }
    /// @brief ドローン強化ビルドであるか判定する。
    bool IsDroneBuild() const
    {
        return runModifiers_.enabled && expeditionCombatStyleSelected_ && expeditionCombatStyle_ == tankbuild::Style::Drone;
    }
    /// @brief 遠征戦闘外観を設定する。
    bool SetExpeditionCombatStyle(tankbuild::Style style);
    /// @brief 遠征戦闘外観を返す。
    tankbuild::Style GetExpeditionCombatStyle() const
    {
        return expeditionCombatStyle_;
    }
    /// @brief 遠征戦闘外観が存在するか判定する。
    bool HasExpeditionCombatStyle() const
    {
        return expeditionCombatStyleSelected_;
    }

    /// @brief デストラクタ
    ~Player();

    /// @brief マウスの方を向く
    void RotateToMouse(cg2::Camera* viewProjection);

    /// @brief 自機の戦闘資源・機体設定・初期位置を用意する。
    /// @param objectBullet 呼び出し側から渡す弾用の描画オブジェクト。
    /// @param position ワールド座標での初期位置。
    /// @param arenaUi 旧アリーナ用の図鑑・HUD資源も準備するか。遠征は専用UIを持つ。
    /// @note 初回生成に使う。機体設定の再読込にはReloadPlayerClassConfigsを使う。
    void Initialize(cg2::Object3d* objectBullet, const cg2::Vector3& position, bool arenaUi = true);

    /// @brief UI入力を処理してから自機の移動・攻撃・装備を更新する。
    /// @param deltaTime 戦闘用の経過秒。スローなどを反映した時間。
    /// @param uiDeltaTime UI用の経過秒。戦闘のスローと分けて渡す。
    /// @note 進化画面を操作したフレームは戦闘入力へ流さない。
    void Update(cg2::Camera* viewProjection, Stage& stage, BulletManager* BulletManager, float deltaTime, float uiDeltaTime);

    /// @brief 描画
    void Draw(bool drawBody = true);
    /// @brief 機体専用を描画する。
    void DrawBodyOnly();

    /// @brief スプライト描画
    void DrawSprite();
    /// @brief 進化後の後処理演出を描画する。
    void DrawEvolutionAfterPostEffects();
    /// @brief 強化HUD後の後処理演出を描画する。
    void DrawUpgradeHudAfterPostEffects();
    /// @brief Gameplayネオン文字文字表示を末尾へ追加する。
    void AppendGameplayNeonTextLabels(std::vector<cg2::TextLabel*>& labels) const;
    /// @brief 強化HUD文字テクスチャを利用前に準備する。
    void PrepareUpgradeHudTextTextures();

    // ドローンのゲッター
    std::vector<PlayerDrone*> GetDronePtrs() const;

    /// @brief 衝突判定
    void OnCollision(Collider* other) override;

    // ワールド座標を取得

    // 半径
    static inline const float kRadius = 0.8f;

    /// @brief ワールド座標での位置を返す。
    cg2::Vector3 GetWorldPosition() const override;

    /// @brief 移動を返す。
    cg2::Vector3 GetMove()
    {
        return velocity_;
    }
    /// @brief 速度を設定する。
    void SetVelocity(const cg2::Vector3& v)
    {
        velocity_ = v;
    }

    // セッター
    void SetWorldPosition(const cg2::Vector3& pos)
    {
        worldTransform_.translate = pos;
        object_->SetTransform(worldTransform_);
        object_->Update();
    }

    /// @brief AABBを返す。
    cg2::AABB GetAABB();

    /// @brief 指定ダメージを戦闘状態へ反映する。
    void Damage(uint32_t amount = kDamageBlockDamage);
    /// @brief 受けたダメージをHPなどの戦闘状態へ反映する。
    void TakeDamage(uint32_t amount, float invincibleTime = 0.45f);
    /// @brief 性能調整設定を現在の状態へ適用する。
    void ApplyBalanceConfig(const BalanceConfig& config);
    /// @brief 戦闘外観性能調整を現在の状態へ適用する。
    void ApplyCombatStyleBalance(const TankCombatStyleBalances& profiles);
    /// @brief 戦闘外観設定を返す。
    const TankCombatStyleProfile& GetCombatStyleProfile(tankbuild::Style style) const
    {
        return combatStyleBalances_[tankbuild::Valid(style) ? static_cast<size_t>(style) : 0];
    }
    /// @brief 遠征Modifiersを設定する。
    void SetRunModifiers(const TankRunModifiers& modifiers);
    /// @brief 遠征中の自機の戦闘性能を表示・検証用にまとめる。
    struct RunCombatSnapshot {
        int barrels = 0, projectilesPerBarrel = 1, activeDrones = 0;
        tankbuild::Style style = tankbuild::Style::Shooter;
        bool hasStyle = false, melee = false;
        int droneLimit = 0;
        std::string classId;
        float classDamageScale = 1.0f, classReloadScale = 1.0f, classBulletSpeedScale = 1.0f;
        bool classReflects = false, classPenetrates = false, isAuthored = false;
        int classDroneCount = 0;
        int baseDroneCount = 3;
        int maxWallBounces = -1, actorPierceCount = 0, impactSplitCount = 0;
        bool reflects = false, homing = false, dashBurst = false, dashExplosion = false;
        float homingTurnRate = 0;
        uint32_t dashExplosionsEmitted = 0;
        float shotDamage = 0, reloadSeconds = 0;
    };
    /// @brief 遠征戦闘状態の写しを返す。
    RunCombatSnapshot GetRunCombatSnapshot() const;
    /// @brief デモ入力有効であるか判定する。
    bool IsDemoInputEnabled() const
    {
        return demoInputEnabled_;
    }
    /// @brief デモ入力を設定する。
    void SetDemoInput(bool enabled, const cg2::Vector2& move, const cg2::Vector3& aimWorld, bool shoot, bool dash)
    {
        demoInputEnabled_ = enabled;
        demoMove_ = move;
        demoAim_ = aimWorld;
        demoShoot_ = shoot;
        demoDash_ = dash;
        if (enabled) {
            isChangeMode = false;
            runRoomAwaitInputRelease_ = false;
        }
    }
    /// @brief 試作装備構成を利用条件を設定する。
    void ConfigurePrototypeLoadout(int archetype); // 0: Twin, 1: MachineGun, 2: Overseer
    /// @brief 遠征中の自機のHPを回復する。
    void HealRunPlayer(int amount);
    /// @brief 遠征中の自機のHPを費用として消費する。
    bool SpendRunHealth(int amount);
    /// @brief 遠征Checkpoint進化を設定する。
    void SetRunCheckpointEvolution(bool enabled)
    {
        runCheckpointEvolution_ = enabled;
    }
    /// @brief 遠征Homing対象を設定する。
    void SetRunHomingTargets(const std::vector<cg2::Vector3>& targets);
    /// @brief 遠征進化候補を返す。
    std::vector<RunEvolutionChoice> GetRunEvolutionChoices() const;
    /// @brief 遠征進化を利用前に準備する。
    void PrepareRunEvolution();
    /// @brief 遠征進化を選ぶ。
    bool ChooseRunEvolution(const std::string& id);
    /// @brief 遠征整備点を付与する。
    bool AwardRunMaintenancePoint(int clearedRoom);
    /// @brief 遠征整備Pointsを返す。
    int GetRunMaintenancePoints() const
    {
        return runMaintenance_.Points();
    }
    /// @brief 遠征整備ランクを返す。
    int GetRunMaintenanceRank(int stat) const
    {
        return runMaintenance_.Rank(stat);
    }
    /// @brief 遠征整備候補を返す。
    std::array<RunMaintenanceChoice, 3> GetRunMaintenanceChoices() const;
    /// @brief 遠征の整備ポイントを消費する。
    bool SpendRunMaintenancePoint(int stat);
    /// @brief 遠征整備点を消費した分を返却する。
    bool RefundRunMaintenancePoint(int stat);
    /// @brief 遠征部屋状態を初期状態へ戻す。
    void ResetRunRoomState(const cg2::Vector3& position);
    /// @brief 開発表示Noダメージを設定する。
    void SetDebugNoDamage(bool enabled)
    {
        debugNoDamage_ = enabled;
    }
    /// @brief 開発表示Noダメージであるか判定する。
    bool IsDebugNoDamage() const
    {
        return debugNoDamage_;
    }

    /// @brief 死亡状態にし、以降の攻撃・衝突などの対象から外す。
    void Die(); // ← プレイヤー消滅

    // 演出終了か
    bool isFinished();

    /// @brief 死亡であるか判定する。
    bool IsDead() const
    {
        return isDead_;
    }
    /// @brief 現在のHPを返す。
    int GetHp() const
    {
        return hp_;
    }
    /// @brief 最大値HPを返す。
    int GetMaxHp() const
    {
        return static_cast<int>(stats_.maxHp);
    }
    /// @brief 撃破画面演出を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateDefeatPresentation(float deltaTime);

    /// @brief On地面であるか判定する。
    bool IsOnGround() const
    {
        return isOnGround_;
    }
    /// @brief On地面を設定する。
    void SetOnGround(bool onGround)
    {
        isOnGround_ = onGround;
    }

    /// @brief 角度を返す。
    float GetAngle() const
    {
        return angle_;
    }
    /// @brief 方向を返す。
    const cg2::Vector3& GetDirection() const
    {
        return dir_;
    }
    /// @brief Dashingであるか判定する。
    bool IsDashing() const
    {
        return isDashing_;
    }
    /// @brief 移動入力が存在するか判定する。
    bool HasMovementInput() const
    {
        return cg2::Length(inputDir_) > 0.05f;
    }
    /// @brief 主攻撃攻撃Performedイベントの未処理分を取り出し、内部の保留分を消費済みにする。
    bool ConsumePrimaryAttackPerformedEvent();
    /// @brief ダッシュ開始済みイベントの未処理分を取り出し、内部の保留分を消費済みにする。
    bool ConsumeDashStartedEvent();
    /// @brief ネオン描画する砲塔の位置・方向・形状を表す。
    struct NeonBarrelLayout {
        cg2::Vector3 offset{};
        cg2::Vector3 scale{1.25f, 0.24f, 0.24f};
        float angleRad = 0.0f;
        float recoilOffset = 0.0f;
        float muzzleFlashRatio = 0.0f;
        bool isMelee = false;
        BarrelShape shape = BarrelShape::Box;
        int fireGroup = 0;
        cg2::Vector4 barrelColor{0.25f, 1.0f, 0.95f, 1.0f};
        cg2::Vector4 outlineColor{0.80f, 1.0f, 0.95f, 1.0f};
    };
    /// @brief ネオン描画する機体本体の形状・姿勢・色を表す。
    struct NeonBodyLayout {
        BodyShape shape = BodyShape::Circle;
        cg2::Vector2 scale{1.0f, 1.0f};
        cg2::Vector4 fillColor{0.18f, 0.28f, 0.34f, 0.38f};
        cg2::Vector4 outlineColor{0.50f, 1.0f, 0.35f, 1.0f};
    };
    /// @brief ネオン砲塔Layoutsを返す。
    std::vector<NeonBarrelLayout> GetNeonBarrelLayouts() const;
    /// @brief ネオン機体配置を返す。
    NeonBodyLayout GetNeonBodyLayout() const;
    /// @brief ダメージフィードバック比率を返す。
    float GetDamageFeedbackRatio() const;

    /// @brief 攻撃制御弾管理を設定する。
    void SetAttackControllerBulletManager(BulletManager* bulletManager)
    {
        attackController_.SetBulletManager(bulletManager);
        runBulletManager_ = bulletManager;
        EnsureExpeditionDrones();
    }

    /// @brief 攻撃
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Attack(BulletManager* BulletManager, float deltaTime);

    /// @brief ドローン発射
    void DroneShoot(BulletManager* BulletManager);

    /// @brief チャージ突進攻撃
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Smash(float deltaTime);

    /// @brief 球を返す。
    cg2::Sphere GetSphere() const;

    // 経験値を加算する関数
    void AddExp(int amount);
    // The map owns the wallet. Kills enqueue credits instead of advancing level.
    /// @brief 遠征通貨方式を設定する。
    void SetRunCurrencyMode(bool enabled);
    /// @brief 遠征通貨方式であるか判定する。
    bool IsRunCurrencyMode() const
    {
        return runCurrencyMode_;
    }
    /// @brief 未反映の獲得通貨を取り出し、取り出した分を消費済みにする。
    int TakeRunCurrencyEarned()
    {
        const int earned = runCurrencyEarned_;
        runCurrencyEarned_ = 0;
        return earned;
    }
    /// @brief 制作データから遠征で使う機体設定を登録する。
    void InstallRunAuthoredClasses(const tankcontent::Catalog& catalog);
    /// @brief 遠征制作データ進化候補を返す。
    std::vector<RunEvolutionChoice> GetRunAuthoredEvolutionChoices() const;
    /// @brief 遠征制作データ機体を選ぶ。
    bool ChooseRunAuthoredClass(const std::string& id);
    /// @brief レベルを返す。
    int GetLevel() const
    {
        return level_;
    }
    /// @brief 経験値を返す。
    int GetExp() const
    {
        return exp_;
    }
    /// @brief 次のレベル経験値値を返す。
    int GetNextLevelExpValue() const
    {
        return nextLevelExp_;
    }
    /// @brief 能力Pointsを返す。
    int GetSkillPoints() const
    {
        return skillPoints_;
    }
    /// @brief 現在ランクを返す。
    int GetCurrentRank() const
    {
        return GetRankFromLevel(level_);
    }
    /// @brief 強化レベルを返す。
    int GetUpgradeLevel(int index) const;
    /// @brief 現在機体名前を返す。
    const char* GetCurrentClassName() const;
    /// @brief 性能強化を現在の状態へ適用する。
    bool ApplyStatUpgrade(int index);
    /// @brief 性能強化Performedイベントの未処理分を取り出し、内部の保留分を消費済みにする。
    bool ConsumeStatUpgradePerformedEvent();
    /// @brief 性能強化を消費した分を返却する。
    bool RefundStatUpgrade(int index);
    /// @brief 現在の性能値を返す。
    const PlayerStats& GetStats() const
    {
        return stats_;
    }
    /// @brief 戦車ボタン表示データを返す。
    bool GetTankButtonVisualData(const std::string& classId, TankButtonVisualData& output) const;
    /// @brief 機体設定を再読込し、現在の機体の砲塔と配置に反映する。
    /// @return 現在の機体を保ったまま再読込できた場合true。
    /// @note HPと成長状態を初期化しない。
    bool ReloadPlayerClassConfigs(const std::string& path = "resources/configs/playerClasses.json");

    /// @brief Slowを要求を予約する。
    bool RequestSlow();

    /// @brief 図鑑を初期化する。
    void InitializeEncyclopedia();

    /// @brief 図鑑を更新する。
    /// @param uiDeltaTime UI用の経過秒。戦闘の時間倍率と分けて渡す。
    void UpdateEncyclopedia(float uiDeltaTime);

    /// @brief 図鑑を描画する。
    void DrawEncyclopedia();

    /// @brief 戦車Codexを描画する。
    void DrawTankCodex();

    /// @brief 機体設定の選択・複製・編集・JSON保存を行う制作画面を表示する。
    /// @note USE_IMGUIが有効な構成で表示する。戦闘の更新と分けて呼ぶ。
    void DrawPlayerClassEditor();
    /// @brief 強化HUD開発表示ImGUIを描画する。
    void DrawUpgradeHudDebugImGui();
    /// @brief 進化UI外観編集画面を描画する。
    void DrawEvolutionUiStyleEditor();
    /// @brief 自機UIの更新・描画の所要時間を記録する。
    struct UiProfileStats {
        float updateMs = 0.0f;
        float spriteMs = 0.0f;
        float textMs = 0.0f;
        float totalMs = 0.0f;
        int spriteDraws = 0;
        int textDraws = 0;
        bool visible = false;
#if defined(USE_IMGUI) && !defined(NDEBUG)
        bool baseTextRefreshed = false;
        bool listTextRefreshed = false;
        float baseTextRefreshMs = 0.0f;
        float expLabelRefreshMs = 0.0f;
        float levelLabelRefreshMs = 0.0f;
        float baseTextSetStyleMs = 0.0f;
        float baseTextSetTextMs = 0.0f;
        float baseTextSetTextRebuildMs = 0.0f;
        float baseTextRebuildTextureMs = 0.0f;
        float baseTextGetOrCreateTextureMs = 0.0f;
        float baseTextSpriteSetTextureMs = 0.0f;
        int baseTextCacheFileExistedCount = 0;
        int baseTextGeneratedPngCount = 0;
        float listTextRefreshMs = 0.0f;
        float listTextSetStyleMs = 0.0f;
        float listTextSetTextMs = 0.0f;
        float listTextSetTextRebuildMs = 0.0f;
        float listTextRebuildTextureMs = 0.0f;
        float listTextGetOrCreateTextureMs = 0.0f;
        float listTextSpriteSetTextureMs = 0.0f;
        int listTextCacheFileExistedCount = 0;
        int listTextGeneratedPngCount = 0;
#endif
    };
    /// @brief 強化HUD設定集計値を返す。
    const UiProfileStats& GetUpgradeHudProfileStats() const
    {
        return upgradeHudProfile_;
    }
    /// @brief 進化UI設定集計値を返す。
    const UiProfileStats& GetEvolutionUiProfileStats() const
    {
        return evolutionUiProfile_;
    }
#if defined(USE_IMGUI) && !defined(NDEBUG)
    /// @brief 強化HUDの状態を開発表示・検証へ渡す。
    struct UpgradeHudDebugSnapshot {
        int playerLevel = 0;
        int exp = 0;
        int skillPoints = 0;
        bool visible = false;
        float listVisibility = 0.0f;
        bool hideListWithoutPoints = false;
        bool drawListPanels = false;
        bool drawListText = false;
        bool drawBottomBars = false;
        bool drawBottomText = false;
        bool useRectBatch = false;
        bool useNeonProgressBars = false;
        bool useSegmentedUpgradeBars = false;
        bool segmentedBarBloomEnabled = true;
        bool listTextBloomEnabled = false;
        bool listActuallyVisible = false;
        bool isChangeMode = false;
        bool playerIsDead = false;
        int maxEnhancePoint = 0;
    };
    /// @brief 強化HUD開発表示状態の写しを返す。
    UpgradeHudDebugSnapshot GetUpgradeHudDebugSnapshot() const
    {
        return {level_,
                exp_,
                skillPoints_,
                upgradeHudVisible_,
                upgradeHudListVisibility_,
                upgradeHudHideListWithoutPoints_,
                upgradeHudDrawListPanels_,
                upgradeHudDrawListText_,
                upgradeHudDrawBottomBars_,
                upgradeHudDrawBottomText_,
                upgradeHudUseRectBatch_,
                upgradeHudUseNeonProgressBars_,
                upgradeHudUseSegmentedUpgradeBars_,
                upgradeHudSegmentedBarBloomEnabled_,
                upgradeHudListTextBloomEnabled_,
                upgradeHudVisible_ && !isChangeMode && !isDead_ && upgradeHudListVisibility_ > 0.01f,
                isChangeMode,
                isDead_,
                maxEnhancePoint};
    }
    /// @brief 開発表示Auto発射有効を設定する。
    void SetDebugAutoFireEnabled(bool enabled)
    {
        debugAutoFireEnabled_ = enabled;
    }
    /// @brief 開発表示Auto発射有効であるか判定する。
    bool IsDebugAutoFireEnabled() const
    {
        return debugAutoFireEnabled_;
    }
#endif
    /// @brief レーザー射撃イベントの未処理分を取り出し、内部の保留分を消費済みにする。
    std::vector<LaserShotEvent> ConsumeLaserShotEvents();
    /// @brief 地雷Dropイベントの未処理分を取り出し、内部の保留分を消費済みにする。
    std::vector<MineDropEvent> ConsumeMineDropEvents();
    /// @brief 近接攻撃斬撃イベントの未処理分を取り出し、内部の保留分を消費済みにする。
    std::vector<MeleeSlashEvent> ConsumeMeleeSlashEvents();

    /// @brief ランクからのレベルを返す。
    int GetRankFromLevel(int level) const;

    /// @brief Change方式であるか判定する。
    bool IsChangeMode()
    {
        return isChangeMode;
    }
    /// @brief チュートリアルの進行に合わせて進化画面を閉じる。
    void CloseEvolutionUiForTutorial();
    /// @brief 進化Confirmedの未処理分を取り出し、内部の保留分を消費済みにする。
    bool ConsumeEvolutionConfirmed();
    /// @brief 進化Cancelledの未処理分を取り出し、内部の保留分を消費済みにする。
    bool ConsumeEvolutionCancelled();

private:
    using PlayerClassConfig = ::PlayerClassConfig;

    // ワールド変換データ
    cg2::Transform worldTransform_;

    // モデル
    cg2::Object3d* object_ = nullptr;
    /// @brief 1砲塔の設定と描画用オブジェクトを保持する。
    struct BarrelModel {
        std::unique_ptr<cg2::Object3d> object;
        cg2::Transform transform;
        cg2::Vector3 localOffset;
        float recoilOffset = 0.0f;
        float muzzleFlashTimer = 0.0f;
    };
    static constexpr float kMuzzleFlashDuration = 0.06f;
    std::vector<BarrelModel> barrels_;
    PlayerClassCatalog classCatalog_;
    std::unordered_map<std::string, PlayerClassConfig> runAuthoredClasses_;
    std::unordered_map<std::string, tankbuild::Style> runAuthoredStyles_;
    std::vector<RunEvolutionChoice> runAuthoredChoices_;
    bool runCurrencyMode_ = false;
    bool runAuthoredEvolutionActive_ = false;
    int runCurrencyEarned_ = 0;
    // テクスチャハンドル
    uint32_t textureHandle_ = 0u;

    // キーボード入力
    cg2::Input* input_ = nullptr;

    cg2::Vector3 dir_;

    // キャラクターの移動速さ
    float kCharacterSpeed = 0.2f;
    cg2::Vector3 move_;

    const int kBulletTime = 10;
    float bulletCoolTime = 0.0f;

    // キャラクターの当たり判定サイズ
    static inline const float kWidth = 1.6f;
    static inline const float kHeight = 1.6f;

    cg2::Vector3 velocity_{0, 0, 0}; // 現在速度
    cg2::Vector3 inputDir_{0, 0, 0}; // 入力方向

    float maxSpeed_ = 0.15f; // 最高速度
    float accel_ = 2.5f;     // 加速
    float decel_ = 3.5f;     // 減速（ブレーキ）

    float gravity_ = -2.0f;
    float jumpPower_ = 1.0f;

    bool isOnGround_ = false;

    cg2::Transform hpTransform_;

    // HPモデル
    std::vector<std::unique_ptr<cg2::Sprite>> hpSprites_;
    std::unique_ptr<cg2::Sprite> hpFont;
    static constexpr uint32_t kDamageBlockDamage = 75;
    int hp_ = 10000;

    // 無敵時間
    float invincibleTimer_ = 0.0f;
    bool debugNoDamage_ = false;
    float damageFeedbackTimer_ = 0.0f;
    float damageFeedbackDuration_ = 0.18f;
    cg2::Vector4 baseVehicleColor_{1.0f, 1.0f, 1.0f, 1.0f};
    cg2::Vector4 baseBarrelColor_{0.48f, 0.86f, 0.22f, 1.0f};

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
    std::vector<DashImpactEvent> pendingDashImpacts_;
    std::vector<uint64_t> dashImpactTargets_;
    uint32_t dashStartedCount_ = 0, damageTakenCount_ = 0;
    uint32_t primaryAttackCount_ = 0;
    int meleeComboStep_ = 0;
    float meleeComboTimer_ = 0.0f;
    float saberCounterTimer_ = 0.0f;
    tankspecial::RailCharge railCharge_{};
    tankspecial::LinkDamageClock linkDamageClock_{};
    std::vector<DroneLaserLink> droneLaserLinks_;
    std::vector<SpecialCombatEvent> pendingSpecialCombatEvents_;
    SpecialCombatStats specialCombatStats_{};
    MeleeSlashEvent specialMeleeSwing_{};
    float specialMeleeElapsed_ = -1;
    bool specialWaveEmitted_ = false, specialPerfectFeedback_ = false;
    std::vector<uint64_t> specialParriedBullets_;
    /// @brief チャージ状態に従ってレール砲を発射する。
    /// @param dt この処理で進める経過時間（秒）。
    void AttackRailCannon(BulletManager* bullets, bool pressed, float dt);
    /// @brief AdditiveArmamentsを最新の内容へ更新する。
    void RefreshAdditiveArmaments();
    /// @brief Additional能力を初期状態へ戻す。
    void ResetAdditionalAbilities();
    /// @brief Additional能力を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateAdditionalAbilities(Stage& stage, BulletManager* bullets, Enemy* boss, EnemyManager* enemies, float dt);
    /// @brief Spin刃を条件を確認して開始する。
    bool TryStartSpinBlade(bool pressed);
    float empJammerTimer_ = 0, droneChargeCooldown_ = 1.5f, droneBombCooldown_ = 4.0f, recentDashTimer_ = 0;
    size_t nextMissionDrone_ = 0;
    std::array<tankspecial::PainterLock, 128> painterLocks_{};
    std::vector<TargetLockVisual> targetLockVisuals_;
    tankspecial::SpinCycle spinCycle_{};
    bool finisherSpinReady_ = false, dashSlashActive_ = false;
    cg2::Vector3 dashSlashPrevious_{}, dashSlashDirection_{1, 0, 0};
    float dashSlashTimer_ = 0;
    uint32_t dashSlashDamage_ = 1;
    std::vector<uint64_t> dashSlashTargets_;
    /// @brief ダッシュで破壊する壁の識別情報と接触条件を保持する。
    struct WallSmashTarget {
        uint64_t id = 0;
        uint32_t collision = 0;
        float seconds = 0, strength = 1;
    };
    std::array<WallSmashTarget, 128> wallSmashTargets_{};

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
    TankRunModifiers runModifiers_{};
    TankCombatStyleBalances combatStyleBalances_ = DefaultTankCombatStyleBalances();
    /// @brief 遠征基準再装填Framesを返す。
    float GetRunBaseReloadFrames() const;
    tankbuild::Style expeditionCombatStyle_ = tankbuild::Style::Shooter;
    bool expeditionCombatStyleSelected_ = false;
    BulletManager* runBulletManager_ = nullptr;
    TankRunGrowth runGrowth_{};
    uint32_t runDashExplosionsEmitted_ = 0;
    std::string runStarterBranch_ = "Twin";
    PlayerClassConfig runStarterConfig_{};
    bool demoInputEnabled_ = false, demoShoot_ = false, demoDash_ = false;
    cg2::Vector2 demoMove_{};
    cg2::Vector3 demoAim_{};
    cg2::Vector3 runAimWorld_{};
    TankExpeditionMaintenance runMaintenance_{};
    PlayerClassConfig runEvolutionConfig_{};
    bool runEvolutionActive_ = false;
    bool runEvolutionPrepared_ = false;
    float runSupportDroneTimer_ = 0.0f;
    float runDashAttackTimer_ = 0.0f;
    float runOverdriveTimer_ = 0.0f;
    float runOverdriveCooldown_ = 0.0f;
    bool runDashBurstPending_ = false;
    bool runRoomAwaitInputRelease_ = false;
    bool runCheckpointEvolution_ = false;
    std::vector<cg2::Vector3> runHomingTargets_;
    /// @brief 遠征ドローンを利用条件を設定する。
    void ConfigureRunDrone(PlayerDrone& drone) const;
    /// @brief 遠征ドローンを必要な状態を用意する。
    void EnsureExpeditionDrones();
    /// @brief 遠征ドローン上限を返す。
    int GetExpeditionDroneLimit() const;
    /// @brief 遠征投射物Rulesを現在の状態へ適用する。
    void ApplyRunProjectileRules(AttackParam& param, bool applyFan = true) const;
    /// @brief 遠征発射間隔倍率を返す。
    float GetRunFireIntervalScale() const;
    /// @brief 遠征投射物を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateRunProjectiles(BulletManager* bulletManager, float deltaTime);
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
    /// @brief 指定IDの機体へ進化し、装備と外観を反映する。
    void EvolveById(const std::string& classId);
    /// @brief 進化By識別子を条件を確認して確定する。
    bool TryConfirmEvolutionById(const std::string& classId);
    /// @brief Evolveへのが可能か判定する。
    bool CanEvolveTo(const std::string& classId) const;
    /// @brief 遠征Compatible機体であるか判定する。
    bool IsRunCompatibleClass(const PlayerClassConfig& config) const;
    /// @brief 進化機体表示中であるか判定する。
    bool IsEvolutionClassVisible(const std::string& classId) const;
    /// @brief 進化接続が存在するか判定する。
    bool HasEvolutionEdge(const std::string& from, const std::string& to) const;
    /// @brief 機体設定を一時Catalogへ読み、現在の機体IDを検証してから置き換える。
    /// @return 読み込みと現在の機体の検証に成功した場合true。
    /// @note 失敗時は現在の設定を保つ。実行中のHPや装備の再初期化は行わない。
    bool LoadPlayerClassConfigs(const std::string& path = "resources/configs/playerClasses.json");
    /// @brief 自機機体Configsを保存する。
    void SavePlayerClassConfigs(const std::string& path = "resources/configs/playerClasses.json") const;
    /// @brief 既定値機体設定を生成する。
    PlayerClassConfig CreateDefaultClassConfig(ClassType type) const;
    /// @brief 機体設定を返す。
    const PlayerClassConfig* GetClassConfig(ClassType type) const;
    /// @brief 機体設定を返す。
    const PlayerClassConfig* GetClassConfig(const std::string& classId) const;
    /// @brief 現在機体設定を返す。
    const PlayerClassConfig* GetCurrentClassConfig() const;
    /// @brief 機体設定を編集用の参照を取得する。
    PlayerClassConfig* GetMutableClassConfig(const std::string& classId);
    /// @brief 現在の機体設定の砲塔と射撃条件に従って弾を発射する。
    bool FireConfiguredClass(const PlayerClassConfig& config, BulletManager* bulletManager, float baseReload, cg2::Vector3& recoilDir,
                             float& recoilPower);
    /// @brief 特殊Actionを条件を確認して有効にする。
    bool TryActivateSpecialAction();
    /// @brief PerfectDodgeを有効にする。
    bool ActivatePerfectDodge(const PlayerClassConfig& config);
    /// @brief 剣カウンターを有効にする。
    bool ActivateSaberCounter(const PlayerClassConfig& config);
    /// @brief 剣カウンターを発動させる。
    void TriggerSaberCounter(const PlayerClassConfig& config);
    /// @brief 方向ベクトルを指定角度だけ回転する。
    cg2::Vector3 RotateDirection(const cg2::Vector3& direction, float angleDeg) const;
    /// @brief 砲塔を初期化する。
    void InitializeBarrels();
    /// @brief 砲塔配置を更新する。
    void UpdateBarrelLayout();
    /// @brief 砲塔を描画する。
    void DrawBarrels();
    /// @brief Vehicle透明度を設定する。
    void SetVehicleAlpha(float alpha);
    /// @brief ダメージフィードバックを発動させる。
    void TriggerDamageFeedback();
    /// @brief 強化HUDを初期化する。
    void InitializeUpgradeHud();
    /// @brief 強化HUD経験値Glyphsを更新する。
    void UpdateUpgradeHudExpGlyphs(const std::string& text, const cg2::TextStyle& style);
    /// @brief 強化HUDの経験値表示の各文字を配置する。
    void PositionUpgradeHudExpGlyphs();
    /// @brief 強化HUDレベル文字表示を更新する。
    void UpdateUpgradeHudLevelLabels(int level, const std::string& className, const cg2::TextStyle& style);
    /// @brief 強化HUDのレベル表示の各文字を配置する。
    void PositionUpgradeHudLevelLabels();
    /// @brief 強化HUD進行度バーStylesを現在の状態へ適用する。
    void ApplyUpgradeHudProgressBarStyles();
    /// @brief 強化HUDを更新する。
    /// @param uiDeltaTime UI用の経過秒。戦闘の時間倍率と分けて渡す。
    void UpdateUpgradeHud(float uiDeltaTime);
    /// @brief 強化HUDを描画する。
    void DrawUpgradeHud();
    /// @brief 強化HUD区切りBarsを利用前に準備する。
    void PrepareUpgradeHudSegmentBars();
    /// @brief 強化HUD一括処理を初期化する。
    void InitializeUpgradeHudBatch();
    /// @brief 強化HUD矩形一括処理を描画する。
    void DrawUpgradeHudRectBatch(bool showUpgradeList, float expRatio, float levelRatio, float listAlpha, float listOffsetX);
    /// @brief 強化HUD矩形を後で処理するために予約する。
    void QueueUpgradeHudRect(std::vector<cg2::TrailVertex>& vertices, const cg2::Vector2& pos, const cg2::Vector2& size,
                             const cg2::Vector4& color) const;
    /// @brief 強化HUD配置を現在の状態へ適用する。
    void ApplyUpgradeHudLayout();
    /// @brief 強化HUD設定を読み込む。
    bool LoadUpgradeHudConfig(const std::string& path = "resources/configs/playerUpgradeHud.json");
    /// @brief 強化HUD設定を保存する。
    bool SaveUpgradeHudConfig(const std::string& path = "resources/configs/playerUpgradeHud.json") const;
    /// @brief 固定進化試作を初期化する。
    void InitializeStaticEvolutionPrototype();
    /// @brief 固定進化試作を更新する。
    void UpdateStaticEvolutionPrototype();
    /// @brief 固定進化試作を描画する。
    void DrawStaticEvolutionPrototype();
    /// @brief 進化経路図Treeを読み込む。
    bool LoadEvolutionCircuitTree(const std::string& path = "resources/configs/evolutionTree.json");
    /// @brief 進化経路図試作を初期化する。
    void InitializeEvolutionCircuitPrototype();
    /// @brief 進化経路図試作を更新する。
    void UpdateEvolutionCircuitPrototype();
    /// @brief 進化経路図試作を描画する。
    void DrawEvolutionCircuitPrototype();
    /// @brief 進化経路図後の後処理演出を描画する。
    void DrawEvolutionCircuitAfterPostEffects();
    /// @brief 進化経路図文字テクスチャを利用前に準備する。
    void PrepareEvolutionCircuitTextTextures();
    /// @brief 使用進化経路図試作が必要か判定する。
    bool ShouldUseEvolutionCircuitPrototype() const;
    /// @brief 進化UI外観を読み込む。
    bool LoadEvolutionUiStyle(const std::string& path = "resources/configs/evolutionUiStyle.json");
    /// @brief 進化UI外観を保存する。
    bool SaveEvolutionUiStyle(const std::string& path = "resources/configs/evolutionUiStyle.json") const;
    /// @brief 使用固定進化試作が必要か判定する。
    bool ShouldUseStaticEvolutionPrototype() const;
    /// @brief 進化画面の配置基準を仮想画面座標へ変換する。
    cg2::Vector2 EvolutionAnchorToVirtual(const cg2::Vector2& normalizedAnchor) const;
    /// @brief 進化画面の仮想座標を描画座標へ変換する。
    cg2::Vector2 EvolutionVirtualToRender(const cg2::Vector2& virtualPosition) const;
    /// @brief ウィンドウ座標を進化画面の仮想座標へ変換する。
    cg2::Vector2 EvolutionClientToVirtual(const cg2::Vector2& clientPosition) const;
    /// @brief 進化描画倍率を返す。
    float GetEvolutionRenderScale() const;
    /// @brief 進化描画差分を返す。
    cg2::Vector2 GetEvolutionRenderOffset() const;
    /// @brief 固定進化経路図を更新する。
    void UpdateStaticEvolutionCircuit();
    /// @brief 固定進化ノードFramesを更新する。
    void UpdateStaticEvolutionNodeFrames();
    /// @brief 固定進化Silhouettesを更新する。
    void UpdateStaticEvolutionSilhouettes();
    /// @brief 固定進化文字を更新する。
    void UpdateStaticEvolutionText();
    /// @brief 固定進化文字テクスチャを利用前に準備する。
    void PrepareStaticEvolutionTextTextures();
    /// @brief 固定進化開発表示重ね表示を描画する。
    void DrawStaticEvolutionDebugOverlay();
    /// @brief 固定進化Candidatesを最新の内容へ更新する。
    void RefreshStaticEvolutionCandidates();
    /// @brief 進化機体名前を返す。
    std::string GetEvolutionClassName(const std::string& classId) const;
    /// @brief 進化Short役割を返す。
    std::string GetEvolutionShortRole(const PlayerClassConfig& config) const;
    /// @brief 進化役割を返す。
    std::string GetEvolutionRole(const PlayerClassConfig& config) const;
    /// @brief 進化Deltasを返す。
    std::array<std::string, 3> GetEvolutionDeltas(const PlayerClassConfig& current, const PlayerClassConfig& target) const;
    /// @brief 進化Abilityを返す。
    std::string GetEvolutionAbility(const PlayerClassConfig& config) const;
    /// @brief 集計値からの基準を現在の条件から再計算する。
    void RecalculateStatsFromBase(bool healToFull);
    /// @brief Casingを出現させる。
    void SpawnCasing();
    int shootBarrelIndex_ = 0; // 次に撃つ砲身の番号
    int shootGroupIndex_ = 0;
    std::vector<float> weaponGroupCooldowns_;

    int maxEnhancePoint = 5;
    std::array<int, 7> upgradeLevels_{};
    int skillPoints_ = 0;
    /// @brief ステルスを更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateStealth(float deltaTime);
    /// @brief 召喚型を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateSummoner(float deltaTime);

private:
    /// @brief 粒子を出現させる。
    void SpawnParticles();
    /// @brief 粒子を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateParticles(float deltaTime = 1.0f / 60.0f);

    /// @brief Afterimageを出現させる。
    void SpawnAfterimage();
    /// @brief Pを更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateP(float deltaTime);
    /// @brief Buff粒子を出現させる。
    void SpawnBuffParticle();
    /// @brief ワールド座標をカメラの画面座標へ変換する。
    cg2::Vector2 WorldToScreen(const cg2::Vector3& worldPos, cg2::Camera* camera);

    float angle_ = 0.0f;

    // ダッシュ関連
    bool isDashing_ = false;
    bool primaryAttackPerformedEvent_ = false;
    bool dashStartedEvent_ = false;
    float dashTimer_ = 0.0f;    // ダッシュ持続時間
    float dashCooldown_ = 0.0f; // 再使用までの時間
    float movementParticleTimer_ = 0.0f;
    float movementParticleInterval_ = 0.08f;
    const float kDashDuration = 0.3f; // ダッシュしている時間（秒）
    const float kDashCooldown = 0.3f; // クールタイム（秒）
    const float kDashSpeed = 0.3f;    // ダッシュの強さ

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

    float buffTimer_ = 0.0f;          // バフの残り時間
    bool isBuffActive_ = false;       // バフ中かどうかのフラグ
    const float kBuffDuration = 2.0f; // バフの持続時間（3秒）

    float smashCharge_ = 0.0f; // スマッシュチャージ時間
    float maxCharge_ = 1.0f;
    bool isSmash_ = false; // スマッシュ中かどうか
    cg2::Vector3 smashDir_;

    std::unique_ptr<cg2::Sprite> machineGunBtnSprite_ = nullptr; // ボタンの見た目
    cg2::Vector2 btnPos_ = {50.0f, 200.0f};                      // ボタンの位置（画面左下あたり）
    cg2::Vector2 btnSize_ = {100.0f, 50.0f};                     // ボタンのサイズ

    // 図鑑の並び順（表示したい順番に定義）
    std::vector<TankData> encyclopedia_;
    std::unique_ptr<cg2::Sprite> evolutionBackdropSprite_;
    std::unique_ptr<cg2::Sprite> evolutionPreviewPanelSprite_;
    std::unique_ptr<cg2::Sprite> evolutionStatsPanelSprite_;
    std::unique_ptr<cg2::Sprite> evolutionPreviewTankSprite_;
    std::unique_ptr<cg2::Sprite> evolutionShotSprite_;
    std::unique_ptr<cg2::Sprite> evolutionChangeButtonSprite_;
    std::unique_ptr<cg2::TextLabel> evolutionTitleLabel_;
    std::unique_ptr<cg2::TextLabel> evolutionHintLabel_;
    std::unique_ptr<cg2::TextLabel> evolutionPreviewNameLabel_;
    std::unique_ptr<cg2::TextLabel> evolutionRoleLabel_;
    std::unique_ptr<cg2::TextLabel> evolutionChangeButtonLabel_;
    std::array<std::unique_ptr<cg2::TextLabel>, 9> evolutionStatLabels_;
    static constexpr size_t kStaticEvolutionMaxCandidates = 4;
    static constexpr size_t kStaticEvolutionMaxNodes = kStaticEvolutionMaxCandidates + 1;
    static constexpr size_t kStaticEvolutionMaxPaths = kStaticEvolutionMaxCandidates + 1;
    /// @brief 進化選択画面の配置・色・文字・演出を指定する。
    struct EvolutionUiStyleConfig {
        bool enabled = true;
        bool radialLayout = false;
        cg2::Vector2 virtualResolution{1280.0f, 720.0f};
        float safeMargin = 48.0f;
        std::array<cg2::Vector2, 4> nodeAnchors{{{0.23f, 0.43f}, {0.67f, 0.22f}, {0.67f, 0.43f}, {0.67f, 0.64f}}};
        std::array<cg2::Vector2, 4> radialNodeAnchors{{{0.50f, 0.43f}, {0.33f, 0.20f}, {0.67f, 0.20f}, {0.50f, 0.70f}}};
        cg2::Vector2 branchPointAnchor{0.49f, 0.43f};
        cg2::Vector2 currentNodeSize{160.0f, 112.0f};
        cg2::Vector2 candidateNodeSize{160.0f, 112.0f};
        float normalScale = 1.0f;
        float hoverScale = 1.05f;
        float selectedScale = 1.08f;
        float nodeCornerCut = 12.0f;
        float nodeOutlineGlowWidth = 10.0f;
        float nodeOutlineWidth = 2.0f;
        float silhouetteScale = 1.0f;
        float circuitOuterGlowWidth = 18.0f;
        float circuitMiddleGlowWidth = 8.0f;
        float circuitCoreWidth = 2.5f;
        float circuitOpacity = 0.78f;
        float circuitOuterAlpha = 0.14f;
        float circuitMiddleAlpha = 0.34f;
        float circuitCoreAlpha = 0.90f;
        float backgroundDimOpacity = 0.88f;
        cg2::Vector2 detailPanelAnchor{0.50f, 0.88f};
        cg2::Vector2 detailPanelSize{1088.0f, 134.0f};
        cg2::Vector2 confirmButtonSize{186.0f, 44.0f};
        float titleFontSize = 26.0f;
        float classNameFontSize = 22.0f;
        float bodyFontSize = 16.0f;
        float buttonFontSize = 17.0f;
        std::string fontFamily = "Meiryo";
        std::string fontPath;
        int fontWeight = 400;
        NeonTextEffectStyle neonText{};
        cg2::Vector4 normalColor{0.12f, 0.34f, 0.42f, 0.82f};
        cg2::Vector4 availableColor{0.16f, 0.64f, 0.72f, 0.92f};
        cg2::Vector4 hoverColor{0.30f, 0.94f, 1.00f, 1.0f};
        cg2::Vector4 selectedColor{0.42f, 1.00f, 0.58f, 1.0f};
        cg2::Vector4 lockedColor{0.18f, 0.22f, 0.28f, 0.68f};
        cg2::Vector4 panelColor{0.025f, 0.055f, 0.080f, 0.94f};
        cg2::Vector4 titleTextColor{0.74f, 1.00f, 0.92f, 1.0f};
        cg2::Vector4 classTextColor{0.92f, 1.00f, 0.96f, 1.0f};
        cg2::Vector4 bodyTextColor{0.84f, 0.92f, 1.00f, 1.0f};
        cg2::Vector4 buttonTextColor{0.96f, 1.00f, 0.98f, 1.0f};
        cg2::Vector4 textOutlineColor{0.0f, 0.025f, 0.045f, 0.96f};
        float titleOutlineWidth = 0.9f;
        float classNameOutlineWidth = 1.0f;
        float bodyOutlineWidth = 0.35f;
        float buttonOutlineWidth = 0.5f;
        int fixedSelectedCandidate = 0;
    };
    EvolutionUiStyleConfig evolutionUiStyle_{};
    std::unique_ptr<cg2::Sprite> staticEvolutionBackdropSprite_;
    std::unique_ptr<TankButtonUiStyle> tankButtonUiStyle_;
    std::unique_ptr<cg2::ObjectPostEffect> staticEvolutionButtonBloomEffect_;
    std::unique_ptr<NeonTextEffect> staticEvolutionTextEffect_;
    std::array<std::unique_ptr<TankButtonUI>, kStaticEvolutionMaxNodes> staticEvolutionTankButtons_;
    std::unique_ptr<cg2::Sprite> staticEvolutionDetailPanelSprite_;
    std::unique_ptr<cg2::Sprite> staticEvolutionConfirmButtonSprite_;
    std::array<std::unique_ptr<cg2::Sprite>, 8> staticEvolutionConfirmOutlineSprites_;
    std::unique_ptr<cg2::Sprite> staticEvolutionBranchGlowSprite_;
    std::unique_ptr<cg2::Sprite> staticEvolutionBranchCoreSprite_;
    std::array<std::array<std::unique_ptr<cg2::Sprite>, 3>, kStaticEvolutionMaxNodes> staticEvolutionNodePanelSprites_;
    static constexpr size_t kStaticEvolutionNodeFrameSpriteCount = 24;
    std::array<std::array<std::unique_ptr<cg2::Sprite>, kStaticEvolutionNodeFrameSpriteCount>, kStaticEvolutionMaxNodes>
        staticEvolutionNodeFrameSprites_;
    static constexpr size_t kStaticEvolutionSilhouetteSpriteCount = 32;
    std::array<std::array<std::unique_ptr<cg2::Sprite>, kStaticEvolutionSilhouetteSpriteCount>, kStaticEvolutionMaxNodes>
        staticEvolutionSilhouetteSprites_;
    static constexpr size_t kStaticEvolutionCircuitSpriteCount = 27;
    std::array<std::unique_ptr<cg2::Sprite>, kStaticEvolutionCircuitSpriteCount> staticEvolutionCircuitSprites_;
    std::unique_ptr<cg2::TextLabel> staticEvolutionTitleLabel_;
    std::unique_ptr<cg2::TextLabel> staticEvolutionPrototypeLabel_;
    std::array<std::unique_ptr<cg2::TextLabel>, kStaticEvolutionMaxNodes> staticEvolutionNodeNameLabels_;
    std::array<std::unique_ptr<cg2::TextLabel>, kStaticEvolutionMaxNodes> staticEvolutionNodeRankLabels_;
    std::unique_ptr<cg2::TextLabel> staticEvolutionDetailClassLabel_;
    std::unique_ptr<cg2::TextLabel> staticEvolutionRoleLabel_;
    std::array<std::unique_ptr<cg2::TextLabel>, 3> staticEvolutionDeltaLabels_;
    std::unique_ptr<cg2::TextLabel> staticEvolutionAbilityLabel_;
    std::unique_ptr<cg2::TextLabel> staticEvolutionConfirmLabel_;
    std::unique_ptr<cg2::TextLabel> staticEvolutionPanelHintLabel_;
    std::array<cg2::Vector2, kStaticEvolutionMaxNodes> staticEvolutionNodeCentersVirtual_{};
    std::array<cg2::Vector2, kStaticEvolutionMaxNodes> staticEvolutionNodeDrawSizesVirtual_{};
    std::array<cg2::Vector2, kStaticEvolutionMaxNodes> staticEvolutionNodeHitSizesVirtual_{};
    std::array<std::array<cg2::Vector2, 4>, kStaticEvolutionMaxPaths> staticEvolutionCircuitControlPoints_{};
    std::array<int, kStaticEvolutionMaxPaths> staticEvolutionCircuitControlPointCounts_{};
    std::array<std::string, kStaticEvolutionMaxCandidates> staticEvolutionCandidateIds_{};
    size_t staticEvolutionCandidateCount_ = 0;
    int staticEvolutionHoveredNode_ = -1;
    bool staticEvolutionConfirmHovered_ = false;
    /// @brief 進化経路図の1ノードのID・配置・解放条件を表す。
    struct EvolutionCircuitNodeDefinition {
        std::string classId;
        float lane = 0.5f;
    };
    /// @brief 進化経路図のノード間の接続を表す。
    struct EvolutionCircuitEdgeDefinition {
        std::string from;
        std::string to;
    };
    static constexpr size_t kEvolutionCircuitMaxNodes = 12;
    static constexpr size_t kEvolutionCircuitMaxLineSprites = 108;
    std::vector<EvolutionCircuitNodeDefinition> evolutionCircuitNodes_;
    std::vector<EvolutionCircuitEdgeDefinition> evolutionCircuitEdges_;
    std::vector<std::string> evolutionHistory_;
    std::array<cg2::Vector2, kEvolutionCircuitMaxNodes> evolutionCircuitNodeCentersVirtual_{};
    std::array<std::unique_ptr<TankButtonUI>, kEvolutionCircuitMaxNodes> evolutionCircuitTankButtons_;
    std::unique_ptr<TankButtonUI> evolutionCircuitDetailPreview_;
    std::array<std::unique_ptr<cg2::Sprite>, kEvolutionCircuitMaxLineSprites> evolutionCircuitLineSprites_;
    std::unique_ptr<cg2::Sprite> evolutionCircuitBackdropSprite_;
    std::unique_ptr<cg2::Sprite> evolutionCircuitDetailPanelSprite_;
    std::unique_ptr<cg2::TextLabel> evolutionCircuitTitleLabel_;
    std::array<std::unique_ptr<cg2::TextLabel>, 4> evolutionCircuitRankLabels_;
    std::unique_ptr<cg2::TextLabel> evolutionCircuitDetailNameLabel_;
    std::unique_ptr<cg2::TextLabel> evolutionCircuitDetailMetaLabel_;
    std::unique_ptr<cg2::TextLabel> evolutionCircuitDetailRoleLabel_;
    std::array<std::unique_ptr<cg2::TextLabel>, 3> evolutionCircuitDetailStatLabels_;
    std::unique_ptr<cg2::TextLabel> evolutionCircuitHintLabel_;
    int evolutionCircuitSelectedNode_ = 0;
    int evolutionCircuitHoveredNode_ = -1;
    bool evolutionCircuitLoaded_ = false;
    std::string evolutionUiStyleStatus_;
    bool showEvolutionVirtualBounds_ = false;
    bool showEvolutionSafeArea_ = false;
    bool showEvolutionNodeBounds_ = false;
    bool showEvolutionMouseBounds_ = false;
    bool showEvolutionTextBounds_ = false;
    bool showEvolutionCenterLines_ = false;
    bool showEvolutionCircuitControlPoints_ = false;
    bool showEvolutionResolutionInfo_ = false;
    std::unique_ptr<cg2::Sprite> upgradeHudBackdropSprite_;
    std::unique_ptr<cg2::Sprite> upgradeHudExpBackSprite_;
    std::unique_ptr<cg2::Sprite> upgradeHudExpFillSprite_;
    std::unique_ptr<cg2::Sprite> upgradeHudLevelBackSprite_;
    std::unique_ptr<cg2::Sprite> upgradeHudLevelFillSprite_;
    std::unique_ptr<NeonProgressBar> upgradeHudLevelProgressBar_;
    std::unique_ptr<NeonProgressBar> upgradeHudExpProgressBar_;
    std::array<std::unique_ptr<NeonSegmentedBar>, 7> upgradeHudSegmentBars_;
    std::unique_ptr<cg2::ObjectPostEffect> upgradeHudBarBloomEffect_;
    NeonProgressBarStyle upgradeHudLevelProgressStyle_{};
    NeonProgressBarStyle upgradeHudExpProgressStyle_{};
    std::unique_ptr<cg2::TextLabel> upgradeHudTitleLabel_;
    std::unique_ptr<cg2::TextLabel> upgradeHudPointLabel_;
    std::unique_ptr<cg2::TextLabel> upgradeHudLevelLabel_;
    std::unique_ptr<cg2::TextLabel> upgradeHudLevelClassLabel_;
    std::unique_ptr<cg2::TextLabel> upgradeHudListLabel_;
    static constexpr size_t kUpgradeHudExpGlyphSlotCount = 32;
    std::array<std::unique_ptr<cg2::TextLabel>, kUpgradeHudExpGlyphSlotCount> upgradeHudExpGlyphLabels_;
    size_t upgradeHudExpGlyphCount_ = 0;
    static constexpr size_t kUpgradeHudLevelGlyphSlotCount = 2;
    std::array<std::unique_ptr<cg2::TextLabel>, kUpgradeHudLevelGlyphSlotCount> upgradeHudLevelGlyphLabels_;
    size_t upgradeHudLevelGlyphCount_ = 0;
    unsigned long long upgradeHudTextFontRevision_ = 0;
    bool upgradeHudTextPrepared_ = false;
    bool upgradeHudTextPreparedForSegmentedBars_ = false;
    std::array<std::unique_ptr<cg2::Sprite>, 7> upgradeHudButtonSprites_;
    std::array<std::unique_ptr<cg2::Sprite>, 7> upgradeHudPlusSprites_;
    std::array<std::unique_ptr<cg2::Sprite>, 7> upgradeHudMinusSprites_;
    std::array<std::unique_ptr<cg2::TextLabel>, 7> upgradeHudNameLabels_;
    std::array<std::unique_ptr<cg2::TextLabel>, 7> upgradeHudLevelLabels_;
    std::array<std::unique_ptr<cg2::TextLabel>, 7> upgradeHudMinusLabels_;
    std::array<std::unique_ptr<cg2::TextLabel>, 7> upgradeHudPlusLabels_;
    Microsoft::WRL::ComPtr<ID3D12Resource> upgradeHudBatchVertexResource_;
    D3D12_VERTEX_BUFFER_VIEW upgradeHudBatchVertexBufferView_{};
    cg2::TrailVertex* upgradeHudBatchVertexData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> upgradeHudBatchTransformResource_;
    cg2::Matrix4x4* upgradeHudBatchTransformData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> upgradeHudBatchMaterialResource_;
    cg2::Material* upgradeHudBatchMaterialData_ = nullptr;
    static constexpr uint32_t kUpgradeHudBatchMaxVertices = 256;
    std::array<float, 7> upgradeHudFlashTimers_{};
    std::array<float, 7> upgradeHudRefundFlashTimers_{};
    std::array<float, 7> upgradeHudMissFlashTimers_{};
    float upgradeHudListVisibility_ = 0.0f;
    float upgradeHudListAnimSpeed_ = 10.0f;
    float upgradeHudListSlideDistance_ = 260.0f;
    bool upgradeHudMouseCaptured_ = false;
    bool arenaUiEnabled_ = true;
    bool upgradeHudVisible_ = true;
    bool upgradeHudHideListWithoutPoints_ = true;
    bool upgradeHudDrawListPanels_ = true;
    bool upgradeHudDrawListText_ = true;
    bool upgradeHudDrawBottomBars_ = true;
    bool upgradeHudDrawBottomText_ = true;
    bool upgradeHudUseRectBatch_ = true;
    bool upgradeHudUseNeonProgressBars_ = true;
    bool upgradeHudRoundedProgressBars_ = true;
    bool upgradeHudUseSegmentedUpgradeBars_ = true;
    bool upgradeHudListTextBloomEnabled_ = false;
#if defined(USE_IMGUI) && !defined(NDEBUG)
    bool upgradeHudSegmentedBarBloomEnabled_ = true;
    bool debugAutoFireEnabled_ = false;
#endif
    cg2::Vector2 upgradeHudSegmentBarOffset_ = {0.0f, 0.0f};
    cg2::Vector2 upgradeHudSegmentBarSize_ = {230.0f, 22.0f};
    cg2::Vector2 upgradeHudPanelPos_ = {18.0f, 338.0f};
    cg2::Vector2 upgradeHudPanelSize_ = {340.0f, 260.0f};
    cg2::Vector2 upgradeHudRowStart_ = {30.0f, 384.0f};
    cg2::Vector2 upgradeHudButtonSize_ = {286.0f, 22.0f};
    cg2::Vector2 upgradeHudPlusSize_ = {32.0f, 18.0f};
    float upgradeHudRowGap_ = 29.0f;
    float upgradeHudNameX_ = 44.0f;
    float upgradeHudLevelX_ = 184.0f;
    float upgradeHudMinusX_ = 268.0f;
    float upgradeHudPlusX_ = 306.0f;
    float upgradeHudMinusLabelX_ = 277.0f;
    float upgradeHudPlusLabelX_ = 313.0f;
    float upgradeHudNameTextOffsetY_ = 3.0f;
    float upgradeHudLevelTextOffsetY_ = 3.0f;
    float upgradeHudMinusTextOffsetY_ = 1.0f;
    float upgradeHudPlusTextOffsetY_ = 1.0f;
    cg2::Vector2 upgradeHudTitlePos_ = {32.0f, 350.0f};
    cg2::Vector2 upgradeHudPointPos_ = {286.0f, 352.0f};
    cg2::Vector2 upgradeHudLevelBarPos_ = {415.0f, 656.0f};
    cg2::Vector2 upgradeHudLevelBarSize_ = {450.0f, 16.0f};
    cg2::Vector2 upgradeHudLevelTextPos_ = {565.0f, 653.0f};
    cg2::Vector2 upgradeHudExpBarPos_ = {390.0f, 680.0f};
    cg2::Vector2 upgradeHudExpBarSize_ = {500.0f, 20.0f};
    cg2::Vector2 upgradeHudExpTextPos_ = {560.0f, 677.0f};
    std::string upgradeHudConfigStatus_;
    UiProfileStats upgradeHudProfile_{};
    UiProfileStats evolutionUiProfile_{};
    int cachedUpgradeHudExp_ = -1;
    int cachedUpgradeHudNextExp_ = -1;
    int cachedUpgradeHudLevel_ = -1;
    int upgradeHudAnimatedLevel_ = -1;
    int cachedUpgradeHudSkillPoints_ = -1;
    int cachedUpgradeHudMaxEnhancePoint_ = -1;
    bool cachedUpgradeHudSegmentedBars_ = false;
    std::string cachedUpgradeHudClassName_;
    std::array<int, 7> cachedUpgradeHudLevels_{-1, -1, -1, -1, -1, -1, -1};
    bool cachedUpgradeHudListVisible_ = false;

    cg2::Vector2 mousePosition_;

    bool isChangeMode = false;
    bool evolutionConfirmedEvent_ = false;
    bool evolutionCancelledEvent_ = false;
    bool statUpgradePerformedEvent_ = false;
    int evolutionSelectedIndex_ = 0;
    float evolutionUiTimer_ = 0.0f;
    int editorSelectedClassIndex_ = 0;
    int codexSelectedClassIndex_ = 0;
    float codexPreviewTimer_ = 0.0f;
    float codexPreviewAimDeg_ = 0.0f;
    bool codexPreviewAutoMove_ = true;
    bool codexPreviewAutoFire_ = true;

    std::unique_ptr<cg2::Sprite> sprite;

    float stealthAlpha_ = 1.0f; // ステルス時の透明度 (1.0:不透明, 0.0:透明)
    bool isStealth_ = false;    // 現在ステルス中か
    float stealthTimer_ = 0.0f; // ステルス移行までの待機時間

    // Summoner用：ドローンリスト
    const int kMaxSummonerDrones = 4;

    float summonTimer_;
};
