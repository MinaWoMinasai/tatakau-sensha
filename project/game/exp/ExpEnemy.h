#pragma once
#include "Collider.h"
#include "Object3d.h"
#include "AttackController.h"
#include "ExpEnemyCombatCycle.h"
#include "ExpEnemyMagazineCycle.h"
#include "ExpGuardCombat.h"
#include <functional>
#include "game/run/TankExpeditionContent.h"

class Player;
class Stage;
class Enemy;
namespace cg2 {
class NeonGridRenderer;
}

enum class ExpEnemyType {
    Square,
    Triangle,
    Pentagon,
    Shooter,
    Charger,
    Sniper,
    Skirmisher,
    Flanker,
    Suppressor,
    ShieldGuard,
    BladeGuard,
    SummonerCommander,
    EMPJammer,
    ReflectArmor,
};

/// @brief 遠征の敵1体の移動・攻撃・HP・衝突・描画を管理する。
class ExpEnemy : public Collider {
public:
    /// @brief ゲームの性能調整用の数値をまとめる。実行中の状態とは分けて扱う。
    struct BalanceConfig {
        uint32_t contactDamage = 12;
        uint32_t shooterContactDamage = 20;
        uint32_t shooterBulletDamage = 15;
        float shooterDetectionRadius = 18.0f;
        float shooterTurnSpeed = 5.5f;
        float shooterFireInterval = 1.25f;
        float shooterBulletSpeed = 0.20f;
    };
    /// @brief 敵の接触や相互作用の強度・条件を指定する。
    struct EnemyInteractionConfig {
        bool hostileToBoss = false;
    };

    /// @brief 性能調整設定を設定する。
    static void SetBalanceConfig(const BalanceConfig& config);
    /// @brief 敵Interaction設定を設定する。
    static void SetEnemyInteractionConfig(const EnemyInteractionConfig& config);
    /// @brief 敵撃破通知関数を設定する。
    static void SetEnemyKillCallback(std::function<void(uint32_t)> callback);
    /// @brief 自機撃破通知関数を設定する。
    static void SetPlayerDefeatCallback(std::function<void(const cg2::Vector3&)> callback);
    /// @brief 形状ネオンビルボード有効を設定する。
    static void SetShapeNeonBillboardEnabled(bool enabled);
    /// @brief 形状ネオン描画方式を設定する。
    static void SetShapeNeonRenderMode(int mode);
    /// @brief Hostileへのボスであるか判定する。
    static bool IsHostileToBoss()
    {
        return enemyInteractionConfig_.hostileToBoss;
    }

    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(const cg2::Vector3& position, Player* player, ExpEnemyType type = ExpEnemyType::Square);
    /// @brief 制作データ定義を現在の状態へ適用する。
    void ApplyAuthoredDefinition(const tankcontent::Enemy& definition);
    /// @brief 制作データ定義が存在するか判定する。
    bool HasAuthoredDefinition() const
    {
        return hasAuthoredDefinition_;
    }
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(Stage& stage, float deltaTime);
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw(bool drawBody = true);
    /// @brief 機体専用を描画する。
    void DrawBodyOnly();
    /// @brief ネオン塗りつぶし機体専用を描画する。
    void DrawNeonFillBodyOnly();

    /// @brief 衝突の通知を受けて、このオブジェクトの状態を反映する。
    void OnCollision(Collider* other) override;

    // Collider必須関数
    cg2::Vector3 GetWorldPosition() const override
    {
        return worldTransform_.translate;
    }
    /// @brief 半径を返す。
    float GetRadius() const override
    {
        return isRunResource_ ? 1.2f : 0.8f;
    }

    /// @brief 攻撃制御弾管理を設定する。
    void SetAttackControllerBulletManager(BulletManager* bulletManager)
    {
        attackController_.SetBulletManager(bulletManager);
    }
    /// @brief ボス対象を設定する。
    void SetBossTarget(Enemy* boss)
    {
        boss_ = boss;
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
        return maxHp_;
    }
    // Spawn configuration: damage and death are handled by ApplyDamage.
    /// @brief 現在のHPを設定する。
    void SetHp(int hp)
    {
        hp_ = maxHp_ = (std::max)(1, hp);
    }
    /// @brief 遠征リソースを設定する。
    void SetRunResource(std::function<void(bool playerOwned)> onClaim);
    /// @brief 遠征リソースであるか判定する。
    bool IsRunResource() const
    {
        return isRunResource_;
    }
    /// @brief 戦闘脅威であるか判定する。
    bool IsCombatThreat() const
    {
        return !isDead_ && !isRunResource_ && (type_ == ExpEnemyType::Shooter || IsExpeditionCombatRole());
    }
    /// @brief 遠征戦闘役割であるか判定する。
    bool IsExpeditionCombatRole() const
    {
        return type_ >= ExpEnemyType::Charger && type_ <= ExpEnemyType::ReflectArmor;
    }
    /// @brief Support役割であるか判定する。
    bool IsSupportRole() const
    {
        return type_ == ExpEnemyType::SummonerCommander || type_ == ExpEnemyType::EMPJammer;
    }
    /// @brief 戦闘段階を返す。
    ExpEnemyCombatPhase GetCombatPhase() const
    {
        return IsSupportRole() ? (supportPulse_.IsWarning() ? ExpEnemyCombatPhase::Locked : ExpEnemyCombatPhase::Cooldown)
               : type_ == ExpEnemyType::BladeGuard ? bladeCycle_.GetPhase()
               : type_ == ExpEnemyType::Charger    ? combatCycle_.GetPhase()
                                                   : magazineCycle_.GetPhase();
    }
    /// @brief 攻撃攻撃予告比率を返す。
    float GetAttackTelegraphRatio() const
    {
        return IsSupportRole()                     ? supportPulse_.WarningRatio()
               : type_ == ExpEnemyType::BladeGuard ? bladeCycle_.WarningRatio()
               : type_ == ExpEnemyType::Charger    ? combatCycle_.GetWarningRatio()
                                                   : magazineCycle_.GetWarningRatio();
    }
    /// @brief 攻撃照準固定であるか判定する。
    bool IsAttackAimLocked() const
    {
        return IsSupportRole()                     ? supportPulse_.IsWarning()
               : type_ == ExpEnemyType::BladeGuard ? bladeCycle_.IsCommitted()
               : type_ == ExpEnemyType::Charger    ? combatCycle_.IsAimLocked()
                                                   : magazineCycle_.IsAimLocked();
    }
    /// @brief Reloadingであるか判定する。
    bool IsReloading() const
    {
        return IsExpeditionCombatRole() && !IsSupportRole() && type_ != ExpEnemyType::Charger && type_ != ExpEnemyType::BladeGuard &&
               magazineCycle_.IsReloading();
    }
    /// @brief 残弾数残りを返す。
    int GetAmmoRemaining() const
    {
        return IsSupportRole() || type_ == ExpEnemyType::Charger || type_ == ExpEnemyType::BladeGuard ? 0 : magazineCycle_.GetAmmo();
    }
    /// @brief Dashingであるか判定する。
    bool IsDashing() const
    {
        return dashTimer_ > 0 || (type_ == ExpEnemyType::Charger && combatCycle_.GetPhase() == ExpEnemyCombatPhase::Active);
    }
    /// @brief 戦闘射撃発射済みを返す。
    uint32_t GetCombatShotsFired() const
    {
        return combatShotsFired_;
    }
    /// @brief 戦闘ダッシュ件数を返す。
    uint32_t GetCombatDashCount() const
    {
        return combatDashCount_;
    }
    /// @brief 戦闘再装填件数を返す。
    uint32_t GetCombatReloadCount() const
    {
        return magazineCycle_.GetReloadCount();
    }
    /// @brief Shield地形ブロック件数を返す。
    uint32_t GetShieldBlockCount() const
    {
        return shieldBlockCount_;
    }
    /// @brief 刃Swing件数を返す。
    uint32_t GetBladeSwingCount() const
    {
        return bladeCycle_.SwingCount();
    }
    /// @brief 壁衝突件数を返す。
    uint32_t GetWallCollisionCount() const
    {
        return wallCollisionCount_;
    }
    /// @brief 投射物を条件を確認して反射する。
    bool TryReflectProjectile(const cg2::Vector3& attackSource);
    /// @brief 反射件数を返す。
    uint32_t GetReflectionCount() const
    {
        return reflectionCount_;
    }
    /// @brief EMPパルス件数を返す。
    uint32_t GetEmpPulseCount() const
    {
        return empPulseCount_;
    }
    /// @brief SummonRequestの未処理分を取り出し、内部の保留分を消費済みにする。
    bool ConsumeSummonRequest()
    {
        const bool request = summonRequested_;
        summonRequested_ = false;
        return request;
    }
    /// @brief SummonTotalを返す。
    int GetSummonTotal() const
    {
        return summonTotal_;
    }
    /// @brief 召喚済み単位または敵編成を記録する。
    void RecordSummonedUnit()
    {
        ++summonTotal_;
    }
    /// @brief 召喚済み単位または敵編成を利用条件を設定する。
    void ConfigureSummonedUnit(uint64_t commanderId);
    /// @brief 召喚型識別子を返す。
    uint64_t GetSummonerId() const
    {
        return summonerId_;
    }
    /// @brief 召喚済み単位または敵編成であるか判定する。
    bool IsSummonedUnit() const
    {
        return summonerId_ != 0;
    }
    /// @brief 召喚済み単位または敵編成を解除する。
    void DismissSummonedUnit()
    {
        if (IsSummonedUnit()) {
            isDead_ = true;
            hp_ = 0;
        }
    }
    /// @brief 照準方向を返す。
    const cg2::Vector3& GetAimDirection() const
    {
        return aimDirection_;
    }
    /// @brief 攻撃予告終了を返す。
    const cg2::Vector3& GetTelegraphEnd() const
    {
        return telegraphEnd_;
    }
    // Called from the scene's existing neon pass for all mobile combat roles.
    /// @brief 戦闘表示情報を後で処理するために予約する。
    void QueueCombatVisuals(cg2::NeonGridRenderer& renderer, const cg2::Vector3& cameraRight, const cg2::Vector3& cameraUp,
                            const cg2::Vector3& cameraForward, float lineWidth) const;
    /// @brief ダメージからの自機を受け取って反映する。
    bool TakeDamageFromPlayer(uint32_t amount);
    /// @brief 平行光のダメージを受け取って反映する。
    bool TakeDirectionalDamage(uint32_t amount, const cg2::Vector3& attackSource, bool melee = false);
    /// @brief Knockbackを現在の状態へ適用する。
    void ApplyKnockback(const cg2::Vector3& direction, float power);
    /// @brief ダメージからの敵を受け取って反映する。
    bool TakeDamageFromEnemy(uint32_t amount);
    /// @brief 衝突判定マスクを最新の内容へ更新する。
    void RefreshCollisionMask();

    /// @brief 経験値値を返す。
    uint32_t GetExpValue() const
    {
        return expValue_;
    }
    /// @brief 種類を返す。
    ExpEnemyType GetType() const
    {
        return type_;
    }
    /// @brief 表示色を返す。
    const cg2::Vector4& GetVisualColor() const
    {
        return visualColor_;
    }
    /// @brief 表示回転を返す。
    float GetVisualRotation() const
    {
        return worldTransform_.rotate.z;
    }
    /// @brief 表示回転を返す。
    const cg2::Vector3& GetVisualRotate() const
    {
        return worldTransform_.rotate;
    }
    /// @brief 表示倍率を返す。
    const cg2::Vector3& GetVisualScale() const
    {
        return worldTransform_.scale;
    }
    /// @brief シューター予告比率を返す。
    float GetShooterWarningRatio() const
    {
        return type_ == ExpEnemyType::Shooter ? (std::clamp)(shooterWarningRatio_, 0.0f, 1.0f) : 0.0f;
    }
    /// @brief シューター銃口フラッシュ比率を返す。
    float GetShooterMuzzleFlashRatio() const
    {
        return type_ == ExpEnemyType::Shooter && kShooterMuzzleFlashDuration > 0.0f
                   ? (std::clamp)(shooterMuzzleFlashTimer_ / kShooterMuzzleFlashDuration, 0.0f, 1.0f)
                   : 0.0f;
    }
    /// @brief 形状ネオンビルボード対象であるか判定する。
    bool IsShapeNeonBillboardTarget() const;
    /// @brief 形状ネオン描画対象であるか判定する。
    bool IsShapeNeonRenderTarget() const
    {
        return IsShapeNeonBillboardTarget();
    }

private:
    /// @brief ダメージを現在の状態へ適用する。
    bool ApplyDamage(uint32_t amount, bool playerOwned, bool reportOrdinaryEnemyKill);
    /// @brief 種類パラメーターを現在の状態へ適用する。
    void ApplyTypeParams();
    /// @brief ダメージフィードバックを発動させる。
    void TriggerDamageFeedback();
    /// @brief ダメージフィードバックを現在の状態へ適用する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void ApplyDamageFeedback(float deltaTime);
    /// @brief 遠征戦闘を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateExpeditionCombat(Stage& stage, float deltaTime);
    /// @brief Magazineを初期状態へ戻す。
    void ResetMagazine(int rounds = 0, float reloadSeconds = 0.0f);
    /// @brief 戦闘アクターを移動する。
    bool MoveCombatActor(Stage& stage, const cg2::Vector3& displacement);
    /// @brief 戦闘Waypointを検索する。
    cg2::Vector3 FindCombatWaypoint(Stage& stage, const cg2::Vector3& target) const;
    /// @brief 戦闘射線を範囲内に切り詰める。
    cg2::Vector3 ClipCombatRay(Stage& stage, const cg2::Vector3& origin, const cg2::Vector3& direction, float distance) const;
    /// @brief Shieldダメージを解決する。
    uint32_t ResolveShieldDamage(uint32_t amount, const cg2::Vector3& attackSource, bool melee);

    static BalanceConfig balanceConfig_;
    static EnemyInteractionConfig enemyInteractionConfig_;
    static std::function<void(uint32_t)> enemyKillCallback_;
    static std::function<void(const cg2::Vector3&)> playerDefeatCallback_;
    static bool shapeNeonBillboardEnabled_;
    static int shapeNeonRenderMode_;

    cg2::Transform worldTransform_;
    cg2::Vector3 baseScale_{1.0f, 1.0f, 1.0f};
    cg2::Vector4 baseColor_{1.0f, 1.0f, 1.0f, 1.0f};
    cg2::Vector4 visualColor_{1.0f, 1.0f, 1.0f, 1.0f};
    std::unique_ptr<cg2::Object3d> object_;

    // 攻撃コントローラ
    AttackController attackController_;

    Player* player_ = nullptr;
    Enemy* boss_ = nullptr;
    cg2::Vector3 aimDirection_{0.0f, -1.0f, 0.0f};

    int hp_ = 10;
    int maxHp_ = 10;
    float bulletCoolTime = 0.0f;

    // キャラクターの当たり判定サイズ
    static inline const float kWidth = 1.6f;
    static inline const float kHeight = 1.6f;

    bool isDead_ = false;
    bool isRunResource_ = false;
    std::function<void(bool playerOwned)> runResourceClaimCallback_;

    uint32_t expValue_ = 10;
    ExpEnemyType type_ = ExpEnemyType::Square;
    bool hasAuthoredDefinition_ = false;
    uint32_t authoredContactDamage_ = 0, authoredBulletDamage_ = 1;
    float authoredMoveSpeedScale_ = 1, authoredFireIntervalScale_ = 1;

    cg2::Vector3 velocity_;
    cg2::Vector3 combatMoveVelocity_{};
    bool combatWasDashing_ = false;

    float decel_ = 3.5f; // 減速（ブレーキ）

    // デルタタイム
    float dt_ = 1.0f / 60.0f;

    // 無敵時間
    float invincibleTimer_ = 0.0f;
    float shootInterval_ = 0.0f;
    float shooterWarningRatio_ = 0.0f;
    float shooterMuzzleFlashTimer_ = 0.0f;
    bool shooterHadVisibleTarget_ = false;
    static constexpr float kShooterWarningDuration = 0.30f;
    static constexpr float kShooterMuzzleFlashDuration = 0.08f;
    float damageFeedbackTimer_ = 0.0f;
    float damageFeedbackDuration_ = 0.09f;
    ExpEnemyCombatCycle combatCycle_{};
    ExpEnemyMagazineCycle magazineCycle_{};
    expguard::BladeCycle bladeCycle_{};
    float shieldFlashTimer_ = 0.0f;
    uint32_t shieldBlockCount_ = 0;
    expguard::PulseCycle supportPulse_{};
    bool summonRequested_ = false;
    int summonTotal_ = 0;
    uint64_t summonerId_ = 0;
    float summonLifetime_ = 0, supportFlashTimer_ = 0, wallImpactArmedTimer_ = 0;
    uint32_t reflectionCount_ = 0, empPulseCount_ = 0, wallCollisionCount_ = 0;
    float combatStagger_ = 0.0f, orbitSign_ = 1.0f;
    float dashCooldown_ = 0.0f, dashWarningTimer_ = 0.0f, dashTimer_ = 0.0f;
    cg2::Vector3 dashDirection_{};
    uint32_t combatShotsFired_ = 0, combatDashCount_ = 0;
    cg2::Vector3 telegraphEnd_{};
    cg2::Vector3 combatWaypoint_{};
    float combatRepathTimer_ = 0.0f;
};
