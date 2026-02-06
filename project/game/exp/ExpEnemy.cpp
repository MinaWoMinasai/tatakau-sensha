#include "ExpEnemy.h"
#include "Player.h"
#include "Stage.h"

void ExpEnemy::Initialize(const Vector3& position, Player* player)
{
    object_ = std::make_unique<Object3d>();
    object_->Initialize();

    worldTransform_ = InitWorldTransform();
    worldTransform_.translate = position;
    worldTransform_.scale = Vector3(1.0f, 1.0f, 1.0f);

    object_->SetModel("enemyParticle.obj");

    // 衝突属性を設定
    SetCollisionAttribute(kCollisionAttributeEnemy);
    // 衝突対象をプレイヤーとプレイヤーの弾に設定
    SetCollisionMask(kCollisionAttributePlayer | kCollisionAttributePlayerBullet | kCollisionAttributePlayerDrone);

    player_ = player;

    object_->SetTransform(worldTransform_);
    object_->Update();

}

void ExpEnemy::Update(Stage& stage, float deltaTime) {

    // 座標を移動させる
    //worldTransform_.translate += velocity_ * (deltaTime * 60.0f);

    dt_ = deltaTime;

    invincibleTimer_ -= deltaTime;

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

        bulletCoolTime = 2.0f;
    }
}

void ExpEnemy::Draw() {
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
    if (hp_ <= 0) {
        isDead_ = true;
        player_->AddExp(expValue_);
    }

    if (other->GetCollisionAttribute() == kCollisionAttributePlayer) {
        invincibleTimer_ = 0.5f;
    }
}

AABB ExpEnemy::GetAABB() {
    Vector3 worldPos = GetWorldPosition();

    AABB aabb;

    aabb.min = { worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f };
    aabb.max = { worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f };

    return aabb;
}
