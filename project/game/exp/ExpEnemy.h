#pragma once
#include "Collider.h"
#include "Object3d.h"
#include "AttackController.h"

class Player;
class Stage;

class ExpEnemy : public Collider {
public:
    void Initialize(const Vector3& position, Player* player);
    void Update(Stage& stage,float deltaTime);
    void Draw();

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

    uint32_t GetExpValue() const { return expValue_; }

private:
    Transform worldTransform_;
    std::unique_ptr<Object3d> object_;

    // 攻撃コントローラ
    AttackController attackController_;

    Player* player_ = nullptr;

    int hp_ = 10;
    float bulletCoolTime = 0.0f;

    // キャラクターの当たり判定サイズ
    static inline const float kWidth = 1.6f;
    static inline const float kHeight = 1.6f;

    bool isDead_ = false;

    uint32_t expValue_ = 10;

    Vector3 velocity_;

    float decel_ = 3.5f; // 減速（ブレーキ）

    // デルタタイム
    float dt_ = 1.0f / 60.0f;

    // 無敵時間
    float invincibleTimer_ = 0.0f;

};