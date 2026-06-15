#pragma once
#include "Collider.h"
#include "Object3d.h"
#include "AttackController.h"

class Player;
class Stage;

enum class ExpEnemyType {
    Square,
    Triangle,
    Pentagon,
    Shooter,
};

class ExpEnemy : public Collider {
public:
    struct BalanceConfig {
        uint32_t contactDamage = 12;
        uint32_t shooterContactDamage = 20;
        uint32_t shooterBulletDamage = 15;
    };
    struct EnemyInteractionConfig {
        bool hostileToBoss = false;
    };

    static void SetBalanceConfig(const BalanceConfig& config);
    static void SetEnemyInteractionConfig(const EnemyInteractionConfig& config);
    static bool IsHostileToBoss() { return enemyInteractionConfig_.hostileToBoss; }

    void Initialize(const Vector3& position, Player* player, ExpEnemyType type = ExpEnemyType::Square);
    void Update(Stage& stage,float deltaTime);
    void Draw(bool drawBody = true);
    void DrawBodyOnly();

    void OnCollision(Collider* other) override;

    // Collider必須関数
    Vector3 GetWorldPosition() const override { return worldTransform_.translate; }
    float GetRadius() const override { return 0.8f; } // 見た目は少し大きくても判定はこのくらい

    void SetAttackControllerBulletManager(BulletManager* bulletManager) {
        attackController_.SetBulletManager(bulletManager);
    }

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
    bool TakeDamageFromEnemy(uint32_t amount);
    void RefreshCollisionMask();

    uint32_t GetExpValue() const { return expValue_; }

private:
    void ApplyTypeParams();
    void TriggerDamageFeedback();
    void ApplyDamageFeedback(float deltaTime);

    static BalanceConfig balanceConfig_;
    static EnemyInteractionConfig enemyInteractionConfig_;

    Transform worldTransform_;
    Vector3 baseScale_{ 1.0f, 1.0f, 1.0f };
    Vector4 baseColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
    std::unique_ptr<Object3d> object_;

    // 攻撃コントローラ
    AttackController attackController_;

    Player* player_ = nullptr;

    int hp_ = 10;
    int maxHp_ = 10;
    float bulletCoolTime = 0.0f;

    // キャラクターの当たり判定サイズ
    static inline const float kWidth = 1.6f;
    static inline const float kHeight = 1.6f;

    bool isDead_ = false;

    uint32_t expValue_ = 10;
    ExpEnemyType type_ = ExpEnemyType::Square;

    Vector3 velocity_;

    float decel_ = 3.5f; // 減速（ブレーキ）

    // デルタタイム
    float dt_ = 1.0f / 60.0f;

    // 無敵時間
    float invincibleTimer_ = 0.0f;
    float shootInterval_ = 0.0f;
    float damageFeedbackTimer_ = 0.0f;
    float damageFeedbackDuration_ = 0.16f;

};
