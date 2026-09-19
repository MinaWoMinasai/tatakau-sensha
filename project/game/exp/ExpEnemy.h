#pragma once
#include "Collider.h"
#include "Object3d.h"
#include "AttackController.h"
#include "ExpEnemyCombatCycle.h"
#include <functional>

class Player;
class Stage;
class Enemy;
class NeonGridRenderer;

enum class ExpEnemyType {
    Square,
    Triangle,
    Pentagon,
    Shooter,
    Charger,
    Sniper,
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
	static void SetPlayerDefeatCallback(std::function<void(const Vector3&)> callback);
    static void SetShapeNeonBillboardEnabled(bool enabled);
    static void SetShapeNeonRenderMode(int mode);
    static bool IsHostileToBoss() { return enemyInteractionConfig_.hostileToBoss; }

    void Initialize(const Vector3& position, Player* player, ExpEnemyType type = ExpEnemyType::Square);
    void Update(Stage& stage,float deltaTime);
    void Draw(bool drawBody = true);
    void DrawBodyOnly();
    void DrawNeonFillBodyOnly();

    void OnCollision(Collider* other) override;

    // Collider必須関数
    Vector3 GetWorldPosition() const override { return worldTransform_.translate; }
    float GetRadius() const override { return isRunResource_ ? 1.2f : 0.8f; }

    void SetAttackControllerBulletManager(BulletManager* bulletManager) {
        attackController_.SetBulletManager(bulletManager);
    }
	void SetBossTarget(Enemy* boss) { boss_ = boss; }

    // セッター
    void SetWorldPosition(const Vector3& pos) {
        worldTransform_.translate = pos;
        object_->SetTransform(worldTransform_);
        object_->Update();
    }

    AABB GetAABB();

    bool IsDead() const { return isDead_; }
    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }
    void SetHp(int hp) { hp_ = hp; maxHp_ = hp; }
    void SetRunResource(std::function<void(bool playerOwned)> onClaim);
    bool IsRunResource() const { return isRunResource_; }
    bool IsCombatThreat() const {
        return !isDead_ && !isRunResource_ &&
            (type_ == ExpEnemyType::Shooter || IsExpeditionCombatRole());
    }
    bool IsExpeditionCombatRole() const {
        return type_ == ExpEnemyType::Charger || type_ == ExpEnemyType::Sniper;
    }
    ExpEnemyCombatPhase GetCombatPhase() const { return combatCycle_.GetPhase(); }
    float GetAttackTelegraphRatio() const { return combatCycle_.GetWarningRatio(); }
    bool IsAttackAimLocked() const { return combatCycle_.IsAimLocked(); }
    const Vector3& GetAimDirection() const { return aimDirection_; }
    const Vector3& GetTelegraphEnd() const { return telegraphEnd_; }
    // Called from the scene's existing neon pass for the two explicit prefab roles.
    void QueueCombatVisuals(NeonGridRenderer& renderer, const Vector3& cameraRight,
        const Vector3& cameraUp, const Vector3& cameraForward, float lineWidth) const;
    bool TakeDamageFromPlayer(uint32_t amount);
    bool TakeDamageFromEnemy(uint32_t amount);
    void RefreshCollisionMask();

    uint32_t GetExpValue() const { return expValue_; }
    ExpEnemyType GetType() const { return type_; }
    const Vector4& GetVisualColor() const { return visualColor_; }
    float GetVisualRotation() const { return worldTransform_.rotate.z; }
    const Vector3& GetVisualRotate() const { return worldTransform_.rotate; }
    const Vector3& GetVisualScale() const { return worldTransform_.scale; }
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
    bool MoveCombatActor(Stage& stage, const Vector3& displacement);
    Vector3 FindCombatWaypoint(Stage& stage, const Vector3& target) const;
    Vector3 ClipCombatRay(Stage& stage, const Vector3& origin, const Vector3& direction, float distance) const;

    static BalanceConfig balanceConfig_;
    static EnemyInteractionConfig enemyInteractionConfig_;
    static std::function<void(uint32_t)> enemyKillCallback_;
	static std::function<void(const Vector3&)> playerDefeatCallback_;
    static bool shapeNeonBillboardEnabled_;
    static int shapeNeonRenderMode_;

    Transform worldTransform_;
    Vector3 baseScale_{ 1.0f, 1.0f, 1.0f };
    Vector4 baseColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
    Vector4 visualColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
    std::unique_ptr<Object3d> object_;

    // 攻撃コントローラ
    AttackController attackController_;

    Player* player_ = nullptr;
	Enemy* boss_ = nullptr;
	Vector3 aimDirection_{ 0.0f, -1.0f, 0.0f };

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

    Vector3 velocity_;

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
    Vector3 telegraphEnd_{};
    Vector3 combatWaypoint_{};
    float combatRepathTimer_ = 0.0f;

};
