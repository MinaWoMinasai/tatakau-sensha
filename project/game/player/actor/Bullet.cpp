#include "game/weapon/CombatTypes.h"
#include "Bullet.h"
#include "ParticleManager.h"
#include <cmath>

void Bullet::Initialize(const cg2::Vector3& position, const cg2::Vector3& velocity, uint32_t damage, BulletOwner owner, bool reflectable,
                        float bulletHp, float bulletPenetration)
{
    object_ = std::make_unique<cg2::Object3d>();
    object_->Initialize();

    // 再使用時に前の命中履歴・成長イベント・特殊能力・命中元を持ち越さないよう、戦闘状態を戻す。
    owner_ = owner;
    isDead_ = false;
    deathTimer_ = kLifeTime;
    usesRunProjectileRules_ = false;
    specialKind_ = SpecialKind::None;
    specialImpacts_.clear();
    previousPosition_ = position;
    remainingWallBounces_ = -1;
    remainingActorPierces_ = 0;
    impactSplitCount_ = 0;
    pendingImpactSplitCount_ = 0;
    hitActorIds_.clear();
    growthEvents_ = {};
    canClaimRunResource_ = true;
    shooter_ = {};
    returnFlight_ = {};
    returnTarget_ = position;
    sourcePlayer_ = nullptr;
    sourceDroneIndex_ = -1;
    armorReflected_ = burstChild_ = false;
    visualTrailScale_ = 1;
    isReflectable_ = reflectable;
    bulletHp_ = (std::max)(0.1f, bulletHp);
    bulletPenetration_ = (std::max)(0.1f, bulletPenetration);

    // 所有者に合わせて表示色と衝突属性・マスクを設定する。敵対する経験値敵の弾はボスを対象にする。
    if (owner_ == kPlayer) {
        object_->SetModel("bullet.obj");
        SetCollisionAttribute(kCollisionAttributePlayerBullet);
        SetCollisionMask(kCollisionAttributeEnemy | kCollisionAttributeExpEnemy | kCollisionAttributeEnemyBullet);
        if (isReflectable_) {
            object_->SetColor(cg2::Vector4(1.0f, 1.0f, 0.0f, 1.0f));
        } else {
            object_->SetColor(cg2::Vector4(1.0f, 0.78f, 0.28f, 1.0f));
        }
    } else if (owner_ == kEnemy) {
        object_->SetModel("bullet.obj");
        object_->SetColor(cg2::Vector4(1.0f, 0.22f, 0.38f, 1.0f));
        SetCollisionAttribute(kCollisionAttributeEnemyBullet);
        SetCollisionMask(kCollisionAttributePlayer | kCollisionAttributePlayerDrone | kCollisionAttributeExpEnemy |
                         kCollisionAttributePlayerBullet);
    } else if (owner_ == kExpEnemyHostile) {
        object_->SetModel("bullet.obj");
        object_->SetColor(cg2::Vector4(1.0f, 0.16f, 0.08f, 1.0f));
        SetCollisionAttribute(kCollisionAttributeHostileExpEnemyBullet);
        SetCollisionMask(kCollisionAttributeEnemy);
    }

    worldTransform_ = cg2::InitWorldTransform();
    worldTransform_.translate = position;
    worldTransform_.scale = cg2::Vector3(0.5f, 0.5f, 0.5f);
    velocity_ = velocity;
    if (cg2::Length(velocity_) > 0.001f) {
        worldTransform_.rotate.z = std::atan2(velocity_.y, velocity_.x);
    }

    SetDamage(damage);

    object_->SetTransform(worldTransform_);
    ApplyVisualSettings();
    object_->Update();
}

void Bullet::Update(float deltaTime)
{
    if (isDead_)
        return;
    // 高速弾の接触判定と装甲反射の方向計算に使うため、今回の移動より先に前回位置を保存する。
    previousPosition_ = GetWorldPosition();
    if (shooter_.boomerang) {
        // 能力倍率は帰還開始までの時計へ渡す秒数を調整する。帰還中は保持する帰還先へ同じ速さで向ける。
        const float flightScale = (std::clamp)(.75f + .25f * shooter_.boomerangPower, .60f, 2.0f);
        if (returnFlight_.Step(deltaTime / flightScale))
            BeginReturn();
        if (returnFlight_.returning) {
            const cg2::Vector3 toOwner = returnTarget_ - GetWorldPosition();
            const float speed = cg2::Length(velocity_);
            if (cg2::Length(toOwner) < (std::max)(.9f, speed * deltaTime * 60.0f)) {
                Die();
                return;
            }
            velocity_ = cg2::Normalize(toOwner) * speed;
        }
    }

    // velocity_は60FPS相当の基準1フレームの移動量。deltaTime秒を基準フレーム数へ換算する。
    worldTransform_.translate += velocity_ * (deltaTime * 60.0f);
    if (cg2::Length(velocity_) > 0.001f) {
        worldTransform_.rotate.z = std::atan2(velocity_.y, velocity_.x);
    }

    // 寿命は秒で減らす。死亡状態にして軌跡を停止するが、管理配列からの削除は更新の走査後に行う。
    deathTimer_ -= deltaTime;
    if (deathTimer_ <= 0) {
        Die();
    }

    object_->SetTransform(worldTransform_);
    ApplyVisualSettings();
    object_->Update();
    UpdateTrail(deltaTime);
}

void Bullet::Draw() {}

void Bullet::OnCollision(Collider* other)
{
    if (isDead_) {
        return;
    }
    Bullet* otherBullet = dynamic_cast<Bullet*>(other);
    if (otherBullet) {
        // CollisionManagerがペアの入口で生存を確認する。相手が先の通知で死亡しても、
        // こちらの耐久度へ相手の貫通力を適用するため、相手の死亡状態では打ち切らない。
        if (otherBullet->GetOwner() == owner_) {
            return;
        }
        // 斬撃波は同じ弾との重なりで耐久ダメージ・命中演出を繰り返さないよう、衝突IDを記録する。
        if (specialKind_ == SpecialKind::SlashWave) {
            if (!CanHitActor(otherBullet))
                return;
            hitActorIds_.push_back(otherBullet->GetCollisionId());
            if (specialImpacts_.size() < 8)
                specialImpacts_.push_back({specialKind_, GetWorldPosition(), velocity_, true});
        }
        cg2::Vector3 impactNormal = velocity_ * -1.0f;
        cg2::ParticleManager::GetInstance()->EmitNeonImpactEffect(GetWorldPosition(), impactNormal, GetBulletColor(), 7);
        ApplyBulletDurabilityDamage(otherBullet->GetBulletPenetration());
        return;
    }

    // 同じアクターへの再通知を防ぐ。死亡後も予約・イベントは管理側が回収するので、先に記録しておく。
    if (!CanHitActor(other))
        return;
    hitActorIds_.push_back(other->GetCollisionId());
    if (specialKind_ != SpecialKind::None && specialImpacts_.size() < 8)
        specialImpacts_.push_back({specialKind_, other->GetWorldPosition(), velocity_, false});
    QueueImpactSplit(velocity_);
    cg2::Vector3 impactNormal = velocity_ * -1.0f;
    cg2::ParticleManager::GetInstance()->EmitNeonImpactEffect(GetWorldPosition(), impactNormal, GetBulletColor(), 11);
    if (shooter_.boomerang) {
        // 帰還弾は命中で消費せず、往路・帰路それぞれの履歴で1対象1回にする。帰還開始時に新規分裂を止める。
        ++growthEvents_.actorPierces;
    } else if (remainingActorPierces_ > 0) {
        --remainingActorPierces_;
        ++growthEvents_.actorPierces;
    } else {
        Die();
    }
}

void Bullet::ConfigureSpecial(SpecialKind kind, float radius, float lifetime)
{
    specialKind_ = kind;
    radius_ = (std::clamp)(radius, .1f, 2.5f);
    deathTimer_ = (std::clamp)(lifetime, .05f, kLifeTime);
}

void Bullet::ConfigureShooterAbilities(bool chain, bool mark, bool boomerang, bool killBurst, float chainPower, float markPower,
                                       float boomerangPower, float burstPower)
{
    shooter_ = {chain, mark, boomerang, killBurst, chainPower, markPower, boomerangPower, burstPower};
    if (chain || mark || boomerang || killBurst)
        usesRunProjectileRules_ = true;
}

void Bullet::BeginReturn()
{
    // 帰路では往路の敵へもう一度命中できる。新規の分裂回数は0にするが、既存の保留予約は消去しない。
    returnFlight_.returning = true;
    hitActorIds_.clear();
    impactSplitCount_ = 0;
    if (specialImpacts_.size() < 8)
        specialImpacts_.push_back({specialKind_, GetWorldPosition(), velocity_, false});
}

void Bullet::ConfigureGrowth(int maxWallBounces, int actorPierceCount, int impactSplitCount, float impactSplitDamageScale)
{
    // 壁反射の-1は回数無制限。回数・ダメージ倍率を補正し、遠征の弾数上限を受けるかも更新する。
    remainingWallBounces_ = (std::clamp)(maxWallBounces, -1, 32);
    remainingActorPierces_ = (std::clamp)(actorPierceCount, 0, 8);
    impactSplitCount_ = (std::clamp)(impactSplitCount, 0, 2);
    impactSplitDamageScale_ = std::isfinite(impactSplitDamageScale) ? (std::clamp)(impactSplitDamageScale, 0.1f, 0.95f) : 0.55f;
    usesRunProjectileRules_ = maxWallBounces >= 0 || remainingActorPierces_ > 0 || impactSplitCount_ > 0;
}

bool Bullet::CanHitActor(const Collider* actor) const
{
    return actor && std::find(hitActorIds_.begin(), hitActorIds_.end(), actor->GetCollisionId()) == hitActorIds_.end();
}

void Bullet::QueueImpactSplit(const cg2::Vector3& direction)
{
    // 元弾からの分裂は1回だけ予約する。通知中は生成せず、走査後に残り枠を渡して取り出す。
    if (impactSplitCount_ <= 0 || deathTimer_ <= 0.0f)
        return;
    const float speed = cg2::Length(direction);
    if (!std::isfinite(speed) || speed <= 0.0001f)
        return;
    pendingImpactSplitCount_ = impactSplitCount_;
    impactSplitCount_ = 0;
    pendingImpactDirection_ = direction;
    pendingImpactPosition_ = GetWorldPosition();
    pendingImpactWallNormal_ = {};
}

void Bullet::OnWallImpact(const cg2::Vector3& safePosition, const cg2::Vector3& normal)
{
    if (isDead_)
        return;
    SetWorldPosition(safePosition);
    const float normalLength = cg2::Length(normal);
    if (!std::isfinite(normalLength) || normalLength <= 0.0001f) {
        Die();
        return;
    }
    // 正規化できる法線から反射方向を計算する。壁から離す位置はStageが求めたsafePositionを使う。
    const cg2::Vector3 unitNormal = normal / normalLength;
    const cg2::Vector3 reflected = velocity_ - 2.0f * cg2::Dot(velocity_, unitNormal) * unitNormal;
    if (!std::isfinite(cg2::Length(reflected))) {
        Die();
        return;
    }
    const bool canReflect = isReflectable_ && remainingWallBounces_ != 0;
    if (canReflect && remainingWallBounces_ > 0)
        --remainingWallBounces_;
    if (canReflect && usesRunProjectileRules_)
        ++growthEvents_.wallBounces;
    // 親が反射できない場合も、分裂予約には壁から離れる反射方向と法線を渡す。
    const bool splitAtWall = impactSplitCount_ > 0;
    QueueImpactSplit(reflected);
    if (splitAtWall)
        pendingImpactWallNormal_ = unitNormal;
    if (canReflect) {
        SetVelocity(reflected);
    } else {
        if (shooter_.boomerang && !returnFlight_.returning)
            BeginReturn();
        else
            Die();
    }
}

void Bullet::AppendImpactChildren(std::vector<std::unique_ptr<Bullet>>& children, size_t availableSlots)
{
    const int count = (std::min)(pendingImpactSplitCount_, static_cast<int>((std::min)(availableSlots, size_t{2})));
    pendingImpactSplitCount_ = 0; // 枠不足でも予約を消費する。空きができるまで持ち越して再試行しない。
    if (count <= 0 || deathTimer_ <= 0.0f)
        return;
    const float speed = cg2::Length(pendingImpactDirection_);
    if (!std::isfinite(speed) || speed <= 0.0001f)
        return;
    const cg2::Vector3 base = pendingImpactDirection_ / speed;
    for (int index = 0; index < count; ++index) {
        // 分裂角はラジアン。1発なら中心、2発なら左右へ分ける。
        const float angle = count == 1 ? 0.0f : (index == 0 ? -0.42f : 0.42f);
        const float cosine = std::cos(angle), sine = std::sin(angle);
        cg2::Vector3 direction = {base.x * cosine - base.y * sine, base.x * sine + base.y * cosine, base.z};
        // 壁での分裂では、外向き成分が小さすぎる方向を補正し、壁へ戻る向きで生成しない。
        const float outward = cg2::Dot(direction, pendingImpactWallNormal_);
        if (outward < 0.1f && cg2::Length(pendingImpactWallNormal_) > 0.5f) {
            direction = cg2::Normalize(direction + pendingImpactWallNormal_ * (0.1f - outward));
        }
        auto child = std::make_unique<Bullet>();
        const uint32_t childDamage =
            static_cast<uint32_t>((std::max)(1.0, std::round(static_cast<double>(GetDamage()) * impactSplitDamageScale_)));
        child->Initialize(pendingImpactPosition_ + direction * (radius_ + 0.1f), direction * (speed * 0.9f), childDamage, owner_,
                          isReflectable_, bulletHp_, bulletPenetration_);
        // 残り寿命・所有者・命中履歴・資源取得可否を引き継ぐ。子の分裂回数は0で、連鎖的な分裂を作らない。
        child->ConfigureGrowth(remainingWallBounces_, remainingActorPierces_, 0, impactSplitDamageScale_);
        child->usesRunProjectileRules_ = true;
        child->deathTimer_ = deathTimer_;
        child->canClaimRunResource_ = canClaimRunResource_;
        child->hitActorIds_ = hitActorIds_;
        child->armorReflected_ = armorReflected_;
        child->burstChild_ = true;
        child->sourcePlayer_ = sourcePlayer_;
        child->sourceDroneIndex_ = sourceDroneIndex_;
        children.push_back(std::move(child));
    }
}

void Bullet::ApplyBulletDurabilityDamage(float amount)
{
    if (isDead_) {
        return;
    }
    bulletHp_ -= (std::max)(0.0f, amount);
    if (bulletHp_ <= 0.0f) {
        Die();
    }
}

cg2::Vector3 Bullet::GetWorldPosition() const
{

    cg2::Vector3 worldPos;
    worldPos.x = worldTransform_.translate.x;
    worldPos.y = worldTransform_.translate.y;
    worldPos.z = worldTransform_.translate.z;

    return worldPos;
}

void Bullet::Die()
{
    isDead_ = true;
    ReleaseTrail();
}

void Bullet::ReleaseTrail()
{
    // 軌跡の実体はTrailManagerが所有する。非アクティブにして借用を解除し、その場では削除しない。
    if (trail_) {
        trail_->SetActive(false);
        trail_ = nullptr;
    }
}

void Bullet::AttachTrail(cg2::TrailManager* trailManager, BulletTrailSettings* trailSettings)
{
    if (!trailManager || trail_) {
        return;
    }

    // 設定も軌跡も借用する。BulletManagerは弾の参照を外してから軌跡の実体を消去する。
    trailSettings_ = trailSettings;
    ApplyVisualSettings();
    trail_ = trailManager->CreateInstance();
    trail_->SetIsPermanent(false);
    trail_->SetActive(true);
    trail_->SetConfig(MakeTrailConfig());
}

cg2::Vector4 Bullet::GetBulletColor() const
{
    // 特殊状態の色を優先し、該当しなければ所有者・反射可否と借用設定から選ぶ。
    if (armorReflected_)
        return {1.5f, .18f, 1.0f, 1};
    if (shooter_.boomerang && returnFlight_.returning)
        return {.55f, 1.6f, .85f, 1};
    if (specialKind_ == SpecialKind::Rail)
        return {.32f, 1.30f, 1.70f, 1};
    if (specialKind_ == SpecialKind::SlashWave)
        return {.30f, 1.50f, 1.15f, .80f};
    if (specialKind_ == SpecialKind::ParryReflection)
        return {1.5f, 1.15f, .25f, 1};
    if (trailSettings_) {
        if (owner_ == kPlayer && isReflectable_) {
            return trailSettings_->reflectableObjectColor;
        }
        if (owner_ == kPlayer) {
            return trailSettings_->playerObjectColor;
        }
        if (owner_ == kExpEnemyHostile) {
            return {1.0f, 0.16f, 0.08f, 1.0f};
        }
        return trailSettings_->enemyObjectColor;
    }
    if (owner_ == kPlayer) {
        if (isReflectable_) {
            return {1.0f, 1.0f, 0.22f, 1.0f};
        }
        return {1.0f, 0.55f, 0.20f, 1.0f};
    }
    if (owner_ == kExpEnemyHostile) {
        return {1.0f, 0.16f, 0.08f, 1.0f};
    }
    return {1.0f, 0.20f, 0.36f, 1.0f};
}

void Bullet::ApplyVisualSettings()
{
    if (!object_) {
        return;
    }
    object_->SetColor(GetBulletColor());
}

cg2::TrailConfig Bullet::MakeTrailConfig() const
{
    cg2::TrailConfig config{};
    const cg2::Vector4 color = GetBulletColor();
    if (trailSettings_) {
        // 弾色から軌跡色を作る方式と、軌跡専用の色設定を使う方式を分ける。
        if (trailSettings_->useObjectColorForTrail) {
            config.startColor = {color.x * trailSettings_->trailHeadIntensity, color.y * trailSettings_->trailHeadIntensity,
                                 color.z * trailSettings_->trailHeadIntensity, trailSettings_->trailHeadAlpha};
            config.endColor = {color.x * trailSettings_->trailTailIntensity, color.y * trailSettings_->trailTailIntensity,
                               color.z * trailSettings_->trailTailIntensity, trailSettings_->trailTailAlpha};
        } else {
            config.startColor = trailSettings_->startColor;
            if (owner_ == kPlayer && isReflectable_) {
                config.endColor = trailSettings_->reflectableEndColor;
            } else if (owner_ == kPlayer) {
                config.endColor = trailSettings_->playerEndColor;
            } else {
                config.endColor = trailSettings_->enemyEndColor;
            }
        }
        config.interpolationSteps = static_cast<uint32_t>((std::max)(1, trailSettings_->interpolationSteps));
        config.maxPoints = static_cast<uint32_t>((std::max)(2, trailSettings_->maxPoints));
        config.lifetime = (std::max)(0.01f, trailSettings_->lifetime);
        config.startWidthScale = (std::max)(0.0f, trailSettings_->headWidthScale);
        config.endWidthScale = (std::max)(0.0f, trailSettings_->tailWidthScale);
        config.widthCurvePower = (std::max)(0.05f, trailSettings_->widthCurvePower);
        config.colorCurvePower = (std::max)(0.05f, trailSettings_->colorCurvePower);
        if (owner_ == kPlayer) {
            config.lifetime *= (std::clamp)(trailSettings_->playerTrailLifetimeScale, 0.1f, 1.0f);
            config.startColor.w *= (std::clamp)(trailSettings_->playerTrailAlphaScale, 0.1f, 1.0f);
            config.endColor.w *= (std::clamp)(trailSettings_->playerTrailAlphaScale, 0.1f, 1.0f);
        }
    } else {
        config.startColor = {1.0f, 0.98f, 0.78f, 1.0f};
        config.endColor = {color.x, color.y, color.z, 0.0f};
        config.interpolationSteps = 5;
        config.maxPoints = 22;
        config.lifetime = 0.24f;
    }
    if (specialKind_ == SpecialKind::Rail) {
        config.lifetime = (std::max)(config.lifetime, .30f);
        config.maxPoints = (std::max)(config.maxPoints, 24u);
    }
    // 斬撃波本体はネオン描画で表す。太い不透明な軌跡で刃が光の塊に見えないよう、寿命と透明度を抑える。
    if (specialKind_ == SpecialKind::SlashWave) {
        config.lifetime = .10f;
        config.startColor.w *= .16f;
        config.endColor.w *= .10f;
    }
    return config;
}

void Bullet::UpdateTrail(float deltaTime)
{
    if (!trail_ || trail_->IsActive() == false) {
        return;
    }

    // XY平面の移動方向に直交する帯を作る。静止中は新しい点を加えない。
    cg2::Vector3 dir = velocity_;
    dir.z = 0.0f;
    const float speed = cg2::Length(dir);
    if (speed <= 0.001f) {
        return;
    }
    dir = dir / speed;

    cg2::Vector3 side = {-dir.y, dir.x, 0.0f};
    float halfWidth = (owner_ == kPlayer) ? 0.26f : 0.22f;
    if (trailSettings_) {
        halfWidth = (owner_ == kPlayer) ? trailSettings_->playerHalfWidth : trailSettings_->enemyHalfWidth;
    }
    if (specialKind_ == SpecialKind::Rail)
        halfWidth = (std::max)(halfWidth, radius_ * .85f);
    if (specialKind_ == SpecialKind::SlashWave)
        halfWidth = radius_ * .20f;
    halfWidth *= visualTrailScale_;
    const cg2::Vector3 center = {worldTransform_.translate.x, worldTransform_.translate.y, worldTransform_.translate.z - 0.015f};
    const cg2::Vector3 tip = center + side * halfWidth;
    const cg2::Vector3 base = center - side * halfWidth;
    trail_->Update(deltaTime, tip, base, MakeTrailConfig());
}
