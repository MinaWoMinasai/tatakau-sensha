#include "ExpEnemy.h"
#include "Player.h"
#include "ParticleManager.h"
#include "Stage.h"
#include <algorithm>
#include <cmath>

namespace {
Vector4 LerpColor(const Vector4& a, const Vector4& b, float t)
{
    t = (std::clamp)(t, 0.0f, 1.0f);
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    };
}
}

void ExpEnemy::Initialize(const Vector3& position, Player* player, ExpEnemyType type)
{
    object_ = std::make_unique<Object3d>();
    object_->Initialize();

    worldTransform_ = InitWorldTransform();
    worldTransform_.translate = position;
    worldTransform_.scale = Vector3(1.0f, 1.0f, 1.0f);
    baseScale_ = worldTransform_.scale;

    type_ = type;
    ApplyTypeParams();

    // 衝突属性を設定
    SetCollisionAttribute(kCollisionAttributeEnemy);
    // 衝突対象をプレイヤーとプレイヤーの弾に設定
    SetCollisionMask(kCollisionAttributePlayer | kCollisionAttributePlayerBullet | kCollisionAttributePlayerDrone);

    player_ = player;

    object_->SetTransform(worldTransform_);
    object_->Update();

}

void ExpEnemy::ApplyTypeParams()
{
    switch (type_) {
    case ExpEnemyType::Square:
        object_->SetModel("expBlock.obj");
        baseColor_ = { 1.0f, 0.86f, 0.20f, 1.0f };
        hp_ = 6;
        expValue_ = 8;
        shootInterval_ = 0.0f;
        break;
    case ExpEnemyType::Triangle:
        object_->SetModel("expTriangle.obj");
        baseColor_ = { 1.0f, 0.30f, 0.35f, 1.0f };
        hp_ = 10;
        expValue_ = 14;
        shootInterval_ = 0.0f;
        break;
    case ExpEnemyType::Pentagon:
        object_->SetModel("expPentagon.obj");
        baseColor_ = { 0.35f, 0.48f, 1.0f, 1.0f };
        hp_ = 24;
        expValue_ = 35;
        shootInterval_ = 0.0f;
        break;
    case ExpEnemyType::Shooter:
        object_->SetModel("expEnemy.obj");
        baseColor_ = { 0.95f, 0.25f, 0.18f, 1.0f };
        hp_ = 14;
        expValue_ = 22;
        shootInterval_ = 2.4f;
        break;
    }
    object_->SetColor(baseColor_);
    maxHp_ = hp_;
}

void ExpEnemy::Update(Stage& stage, float deltaTime) {

    // 座標を移動させる
    //worldTransform_.translate += velocity_ * (deltaTime * 60.0f);

    dt_ = deltaTime;

    invincibleTimer_ -= deltaTime;
    ApplyDamageFeedback(deltaTime);

    // --- 慣性処理 ---
    velocity_ += (velocity_ * -1.0f) * 0.98f * deltaTime;

    float timeWeight = deltaTime * 60.0f;

    // X移動
    Vector3 pos = GetWorldPosition();
    pos.x += velocity_.x * timeWeight;
    SetWorldPosition(pos);
    stage.ResolveExpEnemyCollision(*this, X);

    // Y移動
    pos = GetWorldPosition();
    pos.y += velocity_.y * timeWeight;
    SetWorldPosition(pos);
    stage.ResolveExpEnemyCollision(*this, Y);

    // 1. その場で回転させる（見た目の動き）
    worldTransform_.rotate.y += 0.2f * deltaTime;
    worldTransform_.rotate.x += 0.2f * deltaTime;
    worldTransform_.rotate.z += 0.2f * deltaTime;
    object_->SetTransform(worldTransform_);
    object_->Update();

    if (shootInterval_ <= 0.0f) {
        return;
    }

    // 2. 一定間隔で弾を撃つ（反撃ギミック）
    bulletCoolTime -= deltaTime;
    if (bulletCoolTime <= 0.0f) { // 2秒に1回

        // 発射位置
        Vector3 origin = GetWorldPosition();

        // 基準方向
        Vector3 baseDir;
        baseDir = Normalize(player_->GetWorldPosition() - origin);
        
        // 攻撃パラメータを設定
        AttackParam param{};
        param.bulletSpeed = 0.2f;
        param.bulletCount = 1;
        param.spreadAngleDeg = 5.0f;
        param.randomSpread = true;

        param.reflect = false;
        param.penetrate = false;
        param.cooldown = 1.0f;
        param.damage = 1;

        attackController_.Fire(origin, baseDir, param, BulletOwner::kEnemy);

        bulletCoolTime = shootInterval_;
    }
}

void ExpEnemy::Draw(bool drawBody) {
    if (!drawBody) {
        return;
    }
    DrawBodyOnly();
}

void ExpEnemy::DrawBodyOnly() {
    object_->Draw();
}

void ExpEnemy::OnCollision(Collider* other)
{
    Vector3 hitDir =
        worldTransform_.translate - other->GetWorldPosition();

    hitDir = Normalize(hitDir);

    const float kKnockBackPower = 0.05f;

    velocity_ += hitDir * kKnockBackPower * other->GetHitPower() * (dt_ * 60.0f);
    
    if (other->GetCollisionAttribute() == kCollisionAttributePlayer && invincibleTimer_ > 0.0f) {
        return;
    }

    hp_ -= other->GetDamage();
    TriggerDamageFeedback();
    if (hp_ <= 0) {
        isDead_ = true;
        ParticleManager::GetInstance()->Emit("EnemyDeathBurst", GetWorldPosition(), 18);
        ParticleManager::GetInstance()->Emit("DeathSmoke", GetWorldPosition(), 5);
        player_->AddExp(expValue_);
    }

    if (other->GetCollisionAttribute() == kCollisionAttributePlayer) {
        invincibleTimer_ = 0.5f;
    }
}

void ExpEnemy::TriggerDamageFeedback()
{
    damageFeedbackTimer_ = damageFeedbackDuration_;
}

void ExpEnemy::ApplyDamageFeedback(float deltaTime)
{
    if (damageFeedbackTimer_ > 0.0f) {
        damageFeedbackTimer_ = (std::max)(0.0f, damageFeedbackTimer_ - deltaTime);
    }

    const float t = damageFeedbackDuration_ > 0.0f ? damageFeedbackTimer_ / damageFeedbackDuration_ : 0.0f;
    const float flash = t * t;
    const float pulse = std::sin(t * 3.14159265f) * 0.16f;
    worldTransform_.scale = baseScale_ * (1.0f + pulse);
    object_->SetColor(LerpColor(baseColor_, { 1.0f, 1.0f, 1.0f, baseColor_.w }, flash * 0.9f));
}

AABB ExpEnemy::GetAABB() {
    Vector3 worldPos = GetWorldPosition();

    AABB aabb;

    aabb.min = { worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f };
    aabb.max = { worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f };

    return aabb;
}
