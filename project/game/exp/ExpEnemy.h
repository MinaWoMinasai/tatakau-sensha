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
namespace cg2 { class NeonGridRenderer; }

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

class ExpEnemy : public Collider {
public:
    struct BalanceConfig {
        uint32_t contactDamage = 12;
        uint32_t shooterContactDamage = 20;
        uint32_t shooterBulletDamage = 15;
		float shooterDetectionRadius = 18.0f;
		float shooterTurnSpeed = 5.5f;
		float shooterFireInterval = 1.25f;
		float shooterBulletSpeed = 0.20f;
    };
    struct EnemyInteractionConfig {
        bool hostileToBoss = false;
    };

    static void SetBalanceConfig(const BalanceConfig& config);
    static void SetEnemyInteractionConfig(const EnemyInteractionConfig& config);
    static void SetEnemyKillCallback(std::function<void(uint32_t)> callback);
	static void SetPlayerDefeatCallback(std::function<void(const cg2::Vector3&)> callback);
    static void SetShapeNeonBillboardEnabled(bool enabled);
    static void SetShapeNeonRenderMode(int mode);
    static bool IsHostileToBoss() { return enemyInteractionConfig_.hostileToBoss; }

    void Initialize(const cg2::Vector3& position, Player* player, ExpEnemyType type = ExpEnemyType::Square);
    void ApplyAuthoredDefinition(const tankcontent::Enemy& definition);
    bool HasAuthoredDefinition() const { return hasAuthoredDefinition_; }
    void Update(Stage& stage,float deltaTime);
    void Draw(bool drawBody = true);
    void DrawBodyOnly();
    void DrawNeonFillBodyOnly();

    void OnCollision(Collider* other) override;

    // Collider必須関数
    cg2::Vector3 GetWorldPosition() const override { return worldTransform_.translate; }
    float GetRadius() const override { return isRunResource_ ? 1.2f : 0.8f; }

    void SetAttackControllerBulletManager(BulletManager* bulletManager) {
        attackController_.SetBulletManager(bulletManager);
    }
	void SetBossTarget(Enemy* boss) { boss_ = boss; }

    // セッター
    void SetWorldPosition(const cg2::Vector3& pos) {
        worldTransform_.translate = pos;
        object_->SetTransform(worldTransform_);
        object_->Update();
    }

    cg2::AABB GetAABB();

    bool IsDead() const { return isDead_; }
    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }
    // Spawn configuration: damage and death are handled by ApplyDamage.
    void SetHp(int hp) { hp_ = maxHp_ = (std::max)(1, hp); }
    void SetRunResource(std::function<void(bool playerOwned)> onClaim);
    bool IsRunResource() const { return isRunResource_; }
    bool IsCombatThreat() const {
        return !isDead_ && !isRunResource_ &&
            (type_ == ExpEnemyType::Shooter || IsExpeditionCombatRole());
    }
    bool IsExpeditionCombatRole() const {
        return type_ >= ExpEnemyType::Charger && type_ <= ExpEnemyType::ReflectArmor;
    }
    bool IsSupportRole() const { return type_ == ExpEnemyType::SummonerCommander || type_ == ExpEnemyType::EMPJammer; }
    ExpEnemyCombatPhase GetCombatPhase() const { return IsSupportRole() ? (supportPulse_.IsWarning() ? ExpEnemyCombatPhase::Locked : ExpEnemyCombatPhase::Cooldown) : type_ == ExpEnemyType::BladeGuard ? bladeCycle_.GetPhase() : type_ == ExpEnemyType::Charger ? combatCycle_.GetPhase() : magazineCycle_.GetPhase(); }
    float GetAttackTelegraphRatio() const { return IsSupportRole() ? supportPulse_.WarningRatio() : type_ == ExpEnemyType::BladeGuard ? bladeCycle_.WarningRatio() : type_ == ExpEnemyType::Charger ? combatCycle_.GetWarningRatio() : magazineCycle_.GetWarningRatio(); }
    bool IsAttackAimLocked() const { return IsSupportRole() ? supportPulse_.IsWarning() : type_ == ExpEnemyType::BladeGuard ? bladeCycle_.IsCommitted() : type_ == ExpEnemyType::Charger ? combatCycle_.IsAimLocked() : magazineCycle_.IsAimLocked(); }
    bool IsReloading() const { return IsExpeditionCombatRole() && !IsSupportRole() && type_ != ExpEnemyType::Charger && type_ != ExpEnemyType::BladeGuard && magazineCycle_.IsReloading(); }
    int GetAmmoRemaining() const { return IsSupportRole() || type_ == ExpEnemyType::Charger || type_ == ExpEnemyType::BladeGuard ? 0 : magazineCycle_.GetAmmo(); }
    bool IsDashing() const { return dashTimer_ > 0 || (type_ == ExpEnemyType::Charger && combatCycle_.GetPhase() == ExpEnemyCombatPhase::Active); }
    uint32_t GetCombatShotsFired() const { return combatShotsFired_; }
    uint32_t GetCombatDashCount() const { return combatDashCount_; }
    uint32_t GetCombatReloadCount() const { return magazineCycle_.GetReloadCount(); }
    uint32_t GetShieldBlockCount() const { return shieldBlockCount_; }
    uint32_t GetBladeSwingCount() const { return bladeCycle_.SwingCount(); }
    uint32_t GetWallCollisionCount() const { return wallCollisionCount_; }
    bool TryReflectProjectile(const cg2::Vector3& attackSource);
    uint32_t GetReflectionCount() const { return reflectionCount_; }
    uint32_t GetEmpPulseCount() const { return empPulseCount_; }
    bool ConsumeSummonRequest() { const bool request = summonRequested_; summonRequested_ = false; return request; }
    int GetSummonTotal() const { return summonTotal_; }
    void RecordSummonedUnit() { ++summonTotal_; }
    void ConfigureSummonedUnit(uint64_t commanderId);
    uint64_t GetSummonerId() const { return summonerId_; }
    bool IsSummonedUnit() const { return summonerId_ != 0; }
    void DismissSummonedUnit() { if (IsSummonedUnit()) { isDead_ = true; hp_ = 0; } }
    const cg2::Vector3& GetAimDirection() const { return aimDirection_; }
    const cg2::Vector3& GetTelegraphEnd() const { return telegraphEnd_; }
    // Called from the scene's existing neon pass for all mobile combat roles.
    void QueueCombatVisuals(cg2::NeonGridRenderer& renderer, const cg2::Vector3& cameraRight,
        const cg2::Vector3& cameraUp, const cg2::Vector3& cameraForward, float lineWidth) const;
    bool TakeDamageFromPlayer(uint32_t amount);
    bool TakeDirectionalDamage(uint32_t amount, const cg2::Vector3& attackSource, bool melee = false);
	void ApplyKnockback(const cg2::Vector3& direction, float power);
    bool TakeDamageFromEnemy(uint32_t amount);
    void RefreshCollisionMask();

    uint32_t GetExpValue() const { return expValue_; }
    ExpEnemyType GetType() const { return type_; }
    const cg2::Vector4& GetVisualColor() const { return visualColor_; }
    float GetVisualRotation() const { return worldTransform_.rotate.z; }
    const cg2::Vector3& GetVisualRotate() const { return worldTransform_.rotate; }
    const cg2::Vector3& GetVisualScale() const { return worldTransform_.scale; }
    float GetShooterWarningRatio() const {
        return type_ == ExpEnemyType::Shooter
            ? (std::clamp)(shooterWarningRatio_, 0.0f, 1.0f)
            : 0.0f;
    }
    float GetShooterMuzzleFlashRatio() const {
        return type_ == ExpEnemyType::Shooter && kShooterMuzzleFlashDuration > 0.0f
            ? (std::clamp)(shooterMuzzleFlashTimer_ / kShooterMuzzleFlashDuration, 0.0f, 1.0f)
            : 0.0f;
    }
    bool IsShapeNeonBillboardTarget() const;
    bool IsShapeNeonRenderTarget() const { return IsShapeNeonBillboardTarget(); }

private:
    bool ApplyDamage(uint32_t amount, bool playerOwned, bool reportOrdinaryEnemyKill);
    void ApplyTypeParams();
    void TriggerDamageFeedback();
    void ApplyDamageFeedback(float deltaTime);
    void UpdateExpeditionCombat(Stage& stage, float deltaTime);
    void ResetMagazine(int rounds = 0, float reloadSeconds = 0.0f);
    bool MoveCombatActor(Stage& stage, const cg2::Vector3& displacement);
    cg2::Vector3 FindCombatWaypoint(Stage& stage, const cg2::Vector3& target) const;
    cg2::Vector3 ClipCombatRay(Stage& stage, const cg2::Vector3& origin, const cg2::Vector3& direction, float distance) const;
    uint32_t ResolveShieldDamage(uint32_t amount, const cg2::Vector3& attackSource, bool melee);

    static BalanceConfig balanceConfig_;
    static EnemyInteractionConfig enemyInteractionConfig_;
    static std::function<void(uint32_t)> enemyKillCallback_;
	static std::function<void(const cg2::Vector3&)> playerDefeatCallback_;
    static bool shapeNeonBillboardEnabled_;
    static int shapeNeonRenderMode_;

    cg2::Transform worldTransform_;
    cg2::Vector3 baseScale_{ 1.0f, 1.0f, 1.0f };
    cg2::Vector4 baseColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
    cg2::Vector4 visualColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
    std::unique_ptr<cg2::Object3d> object_;

    // 攻撃コントローラ
    AttackController attackController_;

    Player* player_ = nullptr;
	Enemy* boss_ = nullptr;
	cg2::Vector3 aimDirection_{ 0.0f, -1.0f, 0.0f };

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
    bool hasAuthoredDefinition_=false;
    uint32_t authoredContactDamage_=0,authoredBulletDamage_=1;
    float authoredMoveSpeedScale_=1,authoredFireIntervalScale_=1;

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
