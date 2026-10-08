#include "game/weapon/CombatTypes.h"
#include "ExpEnemy.h"
#include "Enemy.h"
#include "Player.h"
#include "ParticleManager.h"
#include "Stage.h"
#include "NeonGridRenderer.h"
#include "ExpEnemyNavigation.h"
#include <algorithm>
#include <cmath>

namespace {
/// @brief 係数を0〜1に制限し、2つのRGBA色を線形補間して返す。
cg2::Vector4 LerpColor(const cg2::Vector4& a, const cg2::Vector4& b, float t)
{
    t = (std::clamp)(t, 0.0f, 1.0f);
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}
} // namespace

ExpEnemy::BalanceConfig ExpEnemy::balanceConfig_{};
ExpEnemy::EnemyInteractionConfig ExpEnemy::enemyInteractionConfig_{};
std::function<void(uint32_t)> ExpEnemy::enemyKillCallback_{};
std::function<void(const cg2::Vector3&)> ExpEnemy::playerDefeatCallback_{};
bool ExpEnemy::shapeNeonBillboardEnabled_ = false;
int ExpEnemy::shapeNeonRenderMode_ = 0;

void ExpEnemy::SetBalanceConfig(const BalanceConfig& config)
{
    balanceConfig_.contactDamage = (std::max)(1u, config.contactDamage);
    balanceConfig_.shooterContactDamage = (std::max)(1u, config.shooterContactDamage);
    balanceConfig_.shooterBulletDamage = (std::max)(1u, config.shooterBulletDamage);
    balanceConfig_.shooterDetectionRadius = (std::max)(1.0f, config.shooterDetectionRadius);
    balanceConfig_.shooterTurnSpeed = (std::max)(0.1f, config.shooterTurnSpeed);
    balanceConfig_.shooterFireInterval = (std::max)(0.1f, config.shooterFireInterval);
    balanceConfig_.shooterBulletSpeed = (std::max)(0.01f, config.shooterBulletSpeed);
}

void ExpEnemy::SetEnemyInteractionConfig(const EnemyInteractionConfig& config)
{
    enemyInteractionConfig_ = config;
}

void ExpEnemy::SetEnemyKillCallback(std::function<void(uint32_t)> callback)
{
    enemyKillCallback_ = std::move(callback);
}

void ExpEnemy::SetPlayerDefeatCallback(std::function<void(const cg2::Vector3&)> callback)
{
    playerDefeatCallback_ = std::move(callback);
}

void ExpEnemy::SetShapeNeonBillboardEnabled(bool enabled)
{
    shapeNeonBillboardEnabled_ = enabled;
    shapeNeonRenderMode_ = enabled ? 1 : 0;
}

void ExpEnemy::SetShapeNeonRenderMode(int mode)
{
    shapeNeonRenderMode_ = (std::clamp)(mode, 0, 3);
    shapeNeonBillboardEnabled_ = shapeNeonRenderMode_ != 0;
}

bool ExpEnemy::IsShapeNeonBillboardTarget() const
{
    return type_ == ExpEnemyType::Square || type_ == ExpEnemyType::Triangle || type_ == ExpEnemyType::Pentagon ||
           type_ == ExpEnemyType::Shooter || IsExpeditionCombatRole();
}

void ExpEnemy::Initialize(const cg2::Vector3& position, Player* player, ExpEnemyType type)
{
    // 個体の役割・報酬・攻撃履歴を初期化する。速度や一部の旧タイマーはここでは一括消去しない。
    hasAuthoredDefinition_ = false;
    authoredMoveSpeedScale_ = authoredFireIntervalScale_ = 1;
    combatShotsFired_ = combatDashCount_ = 0;
    shieldFlashTimer_ = 0.0f;
    shieldBlockCount_ = 0;
    reflectionCount_ = empPulseCount_ = wallCollisionCount_ = 0;
    summonRequested_ = false;
    summonTotal_ = 0;
    summonerId_ = 0;
    summonLifetime_ = supportFlashTimer_ = wallImpactArmedTimer_ = 0;
    combatRepathTimer_ = dashWarningTimer_ = dashTimer_ = 0;
    isDead_ = false;
    isRunResource_ = false;
    runResourceClaimCallback_ = {};
    object_ = std::make_unique<cg2::Object3d>();
    object_->Initialize();

    worldTransform_ = cg2::InitWorldTransform();
    worldTransform_.translate = position;
    worldTransform_.scale = cg2::Vector3(1.0f, 1.0f, 1.0f);
    baseScale_ = worldTransform_.scale;

    type_ = type;
    ApplyTypeParams();
    if (IsExpeditionCombatRole()) {
        const ExpEnemyCombatCycle::Timing timing{0.36f, 0.23f, 0.30f, 0.70f, 0.23f};
        // 出現位置から初回待ち時間を決め、同じ位置なら同じ攻撃開始のずれを使う。
        combatStagger_ = std::fmod(std::abs(position.x * 0.173f + position.y * 0.319f), 0.85f);
        combatCycle_.Reset(timing, 0.55f + combatStagger_);
        bladeCycle_.Reset(0.35f + combatStagger_);
        supportPulse_.Reset(type_ == ExpEnemyType::EMPJammer ? 6.0f : 5.0f, type_ == ExpEnemyType::EMPJammer ? 1.0f : 0.75f,
                            type_ == ExpEnemyType::EMPJammer ? 2.0f + combatStagger_ : 4.25f);
        orbitSign_ = static_cast<int>(std::abs(position.x + position.y)) % 4 < 2 ? -1.0f : 1.0f;
        dashCooldown_ = 2.3f + combatStagger_;
        ResetMagazine();
        telegraphEnd_ = position;
        object_->SetLighting(false);
    }

    // 衝突属性を設定
    SetCollisionAttribute(kCollisionAttributeExpEnemy);
    RefreshCollisionMask();

    player_ = player;

    object_->SetTransform(worldTransform_);
    object_->Update();
}

void ExpEnemy::ApplyAuthoredDefinition(const tankcontent::Enemy& definition)
{
    hasAuthoredDefinition_ = true;
    hp_ = maxHp_ = (std::clamp)(definition.hp, 1, 9999);
    authoredContactDamage_ = static_cast<uint32_t>((std::clamp)(definition.contactDamage, 0, 999));
    authoredBulletDamage_ = static_cast<uint32_t>((std::clamp)(definition.bulletDamage, 1, 999));
    authoredMoveSpeedScale_ = (std::clamp)(definition.moveSpeedScale, 0.1f, 3.0f);
    authoredFireIntervalScale_ = (std::clamp)(definition.fireIntervalScale, 0.3f, 4.0f);
    if (IsExpeditionCombatRole())
        ResetMagazine(definition.magazineSize, definition.reloadSeconds);
    expValue_ = static_cast<uint32_t>((std::clamp)(definition.creditDrop, 0, 999) * 5);
    baseColor_ = {definition.color[0], definition.color[1], definition.color[2], definition.color[3]};
    visualColor_ = baseColor_;
    object_->SetColor(baseColor_);
    SetDamage(type_ == ExpEnemyType::BladeGuard ? 0u : authoredContactDamage_);
}

void ExpEnemy::RefreshCollisionMask()
{
    uint32_t mask = kCollisionAttributePlayer | kCollisionAttributePlayerBullet | kCollisionAttributePlayerDrone;
    if (enemyInteractionConfig_.hostileToBoss || isRunResource_) {
        mask |= kCollisionAttributeEnemy;
    }
    if (isRunResource_)
        mask |= kCollisionAttributeEnemyBullet;
    SetCollisionMask(mask);
}

void ExpEnemy::ConfigureSummonedUnit(uint64_t commanderId)
{
    summonerId_ = commanderId;
    summonLifetime_ = expguard::kSummonLifetimeSeconds;
    hp_ = maxHp_ = 12;
    expValue_ = 0;
    hasAuthoredDefinition_ = true;
    authoredContactDamage_ = 6;
    authoredMoveSpeedScale_ = 0.85f;
    authoredFireIntervalScale_ = 1.3f;
    baseScale_ = {0.70f, 0.70f, 0.70f};
    baseColor_ = {0.85f, 0.40f, 1.4f, 0.75f};
    visualColor_ = baseColor_;
    SetDamage(authoredContactDamage_);
}

bool ExpEnemy::TryReflectProjectile(const cg2::Vector3& attackSource)
{
    if (isDead_ || type_ != ExpEnemyType::ReflectArmor)
        return false;
    const cg2::Vector3 source = attackSource - GetWorldPosition();
    if (cg2::Length(source) < 0.0001f ||
        !expguard::InFacingCone(aimDirection_.x, aimDirection_.y, source.x, source.y, expguard::kReflectHalfAngle))
        return false;
    ++reflectionCount_;
    ++shieldBlockCount_;
    if (shieldFlashTimer_ <= 0)
        cg2::ParticleManager::GetInstance()->EmitNeonImpactEffect(GetWorldPosition() + cg2::Normalize(source) * 1.2f,
                                                                  cg2::Normalize(source), {1.7f, 0.20f, 1.3f, 1}, 4);
    shieldFlashTimer_ = 0.13f;
    return true;
}

void ExpEnemy::SetRunResource(std::function<void(bool playerOwned)> onClaim)
{
    isRunResource_ = true;
    runResourceClaimCallback_ = std::move(onClaim);
    velocity_ = {};
    combatMoveVelocity_ = {};
    combatWasDashing_ = false;
    baseScale_ = {1.5f, 1.5f, 1.5f};
    worldTransform_.scale = baseScale_;
    baseColor_ = {1.0f, 0.74f, 0.20f, 1.0f};
    visualColor_ = baseColor_;
    damageFeedbackDuration_ = 0.14f;
    SetDamage(0);
    RefreshCollisionMask();
    object_->SetColor(visualColor_);
    object_->SetTransform(worldTransform_);
    object_->Update();
}

void ExpEnemy::ApplyTypeParams()
{
    switch (type_) {
    case ExpEnemyType::Square:
        object_->SetModel("expBlock.obj");
        baseColor_ = {1.0f, 0.86f, 0.20f, 1.0f};
        hp_ = 6;
        expValue_ = 8;
        shootInterval_ = 0.0f;
        break;
    case ExpEnemyType::Triangle:
        object_->SetModel("expTriangle.obj");
        baseColor_ = {1.0f, 0.30f, 0.35f, 1.0f};
        hp_ = 10;
        expValue_ = 14;
        shootInterval_ = 0.0f;
        break;
    case ExpEnemyType::Pentagon:
        object_->SetModel("expPentagon.obj");
        baseColor_ = {0.35f, 0.48f, 1.0f, 1.0f};
        hp_ = 24;
        expValue_ = 35;
        shootInterval_ = 0.0f;
        break;
    case ExpEnemyType::Shooter:
        object_->SetModel("expEnemy.obj");
        baseColor_ = {0.95f, 0.25f, 0.18f, 1.0f};
        hp_ = 14;
        expValue_ = 22;
        shootInterval_ = balanceConfig_.shooterFireInterval;
        break;
    case ExpEnemyType::Charger:
        object_->SetModel("expTriangle.obj");
        baseColor_ = {1.60f, 0.36f, 0.10f, 1.0f};
        baseScale_ = {1.05f, 1.05f, 1.05f};
        hp_ = 30;
        expValue_ = 28;
        break;
    case ExpEnemyType::Sniper:
        object_->SetModel("expEnemy.obj");
        baseColor_ = {1.40f, 0.16f, 0.72f, 1.0f};
        baseScale_ = {0.90f, 1.10f, 0.90f};
        hp_ = 24;
        expValue_ = 30;
        break;
    case ExpEnemyType::Skirmisher:
        object_->SetModel("expEnemy.obj");
        baseColor_ = {0.20f, 1.45f, 1.75f, 1.0f};
        hp_ = 28;
        expValue_ = 30;
        break;
    case ExpEnemyType::Flanker:
        object_->SetModel("expTriangle.obj");
        baseColor_ = {1.75f, 0.25f, 0.80f, 1.0f};
        hp_ = 32;
        expValue_ = 35;
        break;
    case ExpEnemyType::Suppressor:
        object_->SetModel("expPentagon.obj");
        baseColor_ = {1.60f, 0.95f, 0.16f, 1.0f};
        hp_ = 42;
        expValue_ = 45;
        break;
    case ExpEnemyType::ShieldGuard:
        object_->SetModel("expPentagon.obj");
        baseColor_ = {0.18f, 1.20f, 1.60f, 1.0f};
        baseScale_ = {1.1f, 1.1f, 1.1f};
        hp_ = 80;
        expValue_ = 45;
        break;
    case ExpEnemyType::BladeGuard:
        object_->SetModel("expEnemy.obj");
        baseColor_ = {1.60f, 0.38f, 0.12f, 1.0f};
        hp_ = 65;
        expValue_ = 45;
        break;
    case ExpEnemyType::SummonerCommander:
        object_->SetModel("expPentagon.obj");
        baseColor_ = {0.85f, 0.45f, 1.7f, 1};
        hp_ = 75;
        expValue_ = 60;
        break;
    case ExpEnemyType::EMPJammer:
        object_->SetModel("expBlock.obj");
        baseColor_ = {0.3f, 0.9f, 1.7f, 1};
        hp_ = 60;
        expValue_ = 60;
        break;
    case ExpEnemyType::ReflectArmor:
        object_->SetModel("expPentagon.obj");
        baseColor_ = {1.5f, 0.25f, 0.9f, 1};
        hp_ = 90;
        expValue_ = 70;
        break;
    }
    object_->SetColor(baseColor_);
    visualColor_ = baseColor_;
    maxHp_ = hp_;
    SetDamage(type_ == ExpEnemyType::Shooter ? balanceConfig_.shooterContactDamage : balanceConfig_.contactDamage);
    if (type_ == ExpEnemyType::BladeGuard)
        SetDamage(0);
}

void ExpEnemy::ResetMagazine(int rounds, float reloadSeconds)
{
    ExpEnemyMagazineCycle::Timing timing{};
    if (type_ == ExpEnemyType::Sniper)
        timing = {2, 0.52f, 0.26f, 0.20f, 1.55f};
    else if (type_ == ExpEnemyType::Flanker)
        timing = {2, 0.24f, 0.17f, 0.22f, 1.55f};
    else if (type_ == ExpEnemyType::Suppressor)
        timing = {5, 0.18f, 0.14f, 0.16f, 2.00f};
    else if (type_ == ExpEnemyType::ShieldGuard || type_ == ExpEnemyType::ReflectArmor)
        timing = {2, 0.38f, 0.18f, 0.28f, 1.60f};
    if (rounds > 0)
        timing.rounds = rounds;
    if (reloadSeconds > 0)
        timing.reload = reloadSeconds;
    magazineCycle_.Reset(timing, 0.55f + combatStagger_);
}

void ExpEnemy::Update(Stage& stage, float deltaTime)
{

    // 寿命・被弾演出を先に進め、以降の資源/戦闘役/図形/Shooterの経路を選ぶ。
    dt_ = deltaTime;

    invincibleTimer_ -= deltaTime;
    shieldFlashTimer_ = (std::max)(0.0f, shieldFlashTimer_ - deltaTime);
    supportFlashTimer_ = (std::max)(0.0f, supportFlashTimer_ - deltaTime);
    wallImpactArmedTimer_ = (std::max)(0.0f, wallImpactArmedTimer_ - deltaTime);
    if (IsSummonedUnit()) {
        summonLifetime_ -= deltaTime;
        if (summonLifetime_ <= 0) {
            DismissSummonedUnit();
            return;
        }
    }
    ApplyDamageFeedback(deltaTime);

    if (isRunResource_) {
        // 資源は取得地点から移動させず、回転と被弾表示だけを更新する。
        velocity_ = {};
        worldTransform_.rotate.z += 0.35f * deltaTime;
        object_->SetTransform(worldTransform_);
        object_->Update();
        return;
    }
    if (IsExpeditionCombatRole()) {
        UpdateExpeditionCombat(stage, deltaTime);
        object_->SetTransform(worldTransform_);
        object_->Update();
        return;
    }

    // --- 慣性処理 ---
    velocity_ += (velocity_ * -1.0f) * 0.98f * deltaTime;

    float timeWeight = deltaTime * 60.0f;

    // 図形とShooterの外部衝撃速度を60FPS基準から変位へ換算する。
    // 移動戦闘役と同じ細分化した地形補正を通し、壁を飛び越えにくくする。
    MoveCombatActor(stage, velocity_ * timeWeight);

    if (type_ != ExpEnemyType::Shooter) {
        worldTransform_.rotate.y += 0.2f * deltaTime;
        worldTransform_.rotate.x += 0.2f * deltaTime;
        worldTransform_.rotate.z += 0.2f * deltaTime;
        object_->SetTransform(worldTransform_);
        object_->Update();
        return;
    }
    shootInterval_ = balanceConfig_.shooterFireInterval * authoredFireIntervalScale_;
    shooterMuzzleFlashTimer_ = (std::max)(0.0f, shooterMuzzleFlashTimer_ - deltaTime);

    const cg2::Vector3 origin = GetWorldPosition();
    cg2::Vector3 targetPosition{};
    BulletOwner bulletOwner = BulletOwner::kEnemy;
    float nearestDistance = balanceConfig_.shooterDetectionRadius;
    bool hasTarget = false;
    // 生存自機を候補にし、ボスへ敵対する場合は距離がさらに短い有効ボスを選ぶ。
    if (player_ && !player_->IsDead()) {
        const float distance = cg2::Length(player_->GetWorldPosition() - origin);
        if (distance <= nearestDistance) {
            targetPosition = player_->GetWorldPosition();
            nearestDistance = distance;
            hasTarget = true;
        }
    }
    if (IsHostileToBoss() && boss_ && boss_->IsRunEncounterEnabled() && !boss_->IsDead()) {
        const float distance = cg2::Length(boss_->GetWorldPosition() - origin);
        if (distance < nearestDistance) {
            targetPosition = boss_->GetWorldPosition();
            nearestDistance = distance;
            bulletOwner = BulletOwner::kExpEnemyHostile;
            hasTarget = true;
        }
    }

    // 有効ブロックのAABBと線分を検査し、ブロック中心が標的より近い交差を遮蔽とする。
    auto hasLineOfSight = [&](const cg2::Vector3& target) {
        cg2::Segment ray{};
        ray.origin = origin;
        ray.diff = target - origin;
        const float targetDistance = cg2::Length(ray.diff);
        for (const auto& row : stage.GetBlocks()) {
            for (const Block& block : row) {
                if (!block.isActive || !cg2::IsCollision(block.aabb, ray)) {
                    continue;
                }
                const cg2::Vector3 blockCenter = (block.aabb.max + block.aabb.min) / 2.0f;
                if (cg2::Length(blockCenter - origin) < targetDistance) {
                    return false;
                }
            }
        }
        return true;
    };

    const bool canSeeTarget = hasTarget && hasLineOfSight(targetPosition);
    if (hasTarget) {
        const cg2::Vector3 desiredDirection = cg2::Normalize(targetPosition - origin);
        const float turnT = (std::clamp)(balanceConfig_.shooterTurnSpeed * deltaTime, 0.0f, 1.0f);
        aimDirection_ += (desiredDirection - aimDirection_) * turnT;
        if (cg2::Length(aimDirection_) > 0.001f) {
            aimDirection_ = cg2::Normalize(aimDirection_);
        }
        worldTransform_.rotate = {0.0f, 0.0f, std::atan2(aimDirection_.x, -aimDirection_.y)};
    }

    // 視認中だけ射撃待ちを進める。視認を失ったら予告表示を消し、待ち時間を最低0.30秒へ戻す。
    if (canSeeTarget) {
        if (!shooterHadVisibleTarget_ && bulletCoolTime <= 0.0f) {
            bulletCoolTime = kShooterWarningDuration;
        }
        shooterHadVisibleTarget_ = true;
        bulletCoolTime -= deltaTime;
        if (bulletCoolTime > 0.0f) {
            shooterWarningRatio_ = bulletCoolTime <= kShooterWarningDuration
                                       ? (std::clamp)(1.0f - bulletCoolTime / kShooterWarningDuration, 0.0f, 1.0f)
                                       : 0.0f;
        } else {
            AttackParam param{};
            param.bulletSpeed = balanceConfig_.shooterBulletSpeed;
            param.bulletCount = 1;
            param.spreadAngleDeg = 3.0f;
            param.randomSpread = true;
            param.damage = hasAuthoredDefinition_ ? authoredBulletDamage_ : balanceConfig_.shooterBulletDamage;
            param.canClaimRunResource = false;
            attackController_.FireFromMuzzle(origin + aimDirection_ * 1.4f, aimDirection_, param, bulletOwner);
            bulletCoolTime = shootInterval_;
            shooterWarningRatio_ = 0.0f;
            shooterMuzzleFlashTimer_ = kShooterMuzzleFlashDuration;
        }
    } else {
        shooterHadVisibleTarget_ = false;
        shooterWarningRatio_ = 0.0f;
        bulletCoolTime = (std::max)(bulletCoolTime, kShooterWarningDuration);
    }

    object_->SetTransform(worldTransform_);
    object_->Update();
}

cg2::Vector3 ExpEnemy::ClipCombatRay(Stage& stage, const cg2::Vector3& origin, const cg2::Vector3& direction, float distance) const
{
    // XYの各軸で進行パラメーターの区間を絞り、境界接触も交差として受け付ける。
    float nearest = distance;
    for (const auto& row : stage.GetBlocks()) {
        for (const Block& block : row) {
            if (!block.isActive)
                continue;
            float enter = 0.0f;
            float leave = nearest;
            auto clipAxis = [&](float start, float speed, float minimum, float maximum) {
                if (std::abs(speed) < 0.0001f)
                    return start >= minimum && start <= maximum;
                float nearT = (minimum - start) / speed;
                float farT = (maximum - start) / speed;
                if (nearT > farT)
                    std::swap(nearT, farT);
                enter = (std::max)(enter, nearT);
                leave = (std::min)(leave, farT);
                return enter <= leave;
            };
            if (clipAxis(origin.x, direction.x, block.aabb.min.x, block.aabb.max.x) &&
                clipAxis(origin.y, direction.y, block.aabb.min.y, block.aabb.max.y)) {
                nearest = (std::max)(0.0f, enter);
            }
        }
    }
    return origin + direction * nearest;
}

bool ExpEnemy::MoveCombatActor(Stage& stage, const cg2::Vector3& displacement)
{
    // 離散的な重なり補正なので、変位を最大0.35単位相当の小区間に分ける。
    // 各区間でX、Yの順に補正し、ダッシュ中に遮られたら残りの区間を進めない。
    const int steps = (std::max)(1, static_cast<int>(std::ceil(cg2::Length(displacement) / 0.35f)));
    const cg2::Vector3 step = displacement / static_cast<float>(steps);
    bool blocked = false;
    for (int i = 0; i < steps; ++i) {
        const cg2::Vector3 before = GetWorldPosition();
        cg2::Vector3 desired = before;
        desired.x += step.x;
        SetWorldPosition(desired);
        stage.ResolveExpEnemyCollision(*this, cg2::X);
        blocked = blocked || std::abs(GetWorldPosition().x - desired.x) > 0.005f;
        desired = GetWorldPosition();
        desired.y += step.y;
        SetWorldPosition(desired);
        stage.ResolveExpEnemyCollision(*this, cg2::Y);
        blocked = blocked || std::abs(GetWorldPosition().y - desired.y) > 0.005f;
        if (blocked && IsDashing())
            break;
    }
    // 外部衝撃の受付中だけ壁衝撃を記録し、受付を消費して同じ衝撃での重複記録を防ぐ。
    if (blocked && wallImpactArmedTimer_ > 0 && cg2::Length(velocity_) > 0.01f) {
        ++wallCollisionCount_;
        wallImpactArmedTimer_ = 0;
    }
    return blocked;
}

cg2::Vector3 ExpEnemy::FindCombatWaypoint(Stage& stage, const cg2::Vector3& target) const
{
    constexpr int width = MapChip::kNumBlockHorizontal;
    constexpr int height = MapChip::kNumBlockVirtical;
    constexpr float cellSize = 2.0f;
    std::array<bool, width * height> blocked{};
    for (const auto& row : stage.GetBlocks()) {
        for (const Block& block : row) {
            if (!block.isActive)
                continue;
            // 射線と異なり機体の幅を考慮するため、AABBを0.85広げた範囲の格子点を通行不可にする。
            const int minX = (std::max)(0, static_cast<int>(std::ceil((block.aabb.min.x - 0.85f) / cellSize)));
            const int maxX = (std::min)(width - 1, static_cast<int>(std::floor((block.aabb.max.x + 0.85f) / cellSize)));
            const int minY = (std::max)(0, static_cast<int>(std::ceil((block.aabb.min.y - 0.85f) / cellSize)));
            const int maxY = (std::min)(height - 1, static_cast<int>(std::floor((block.aabb.max.y + 0.85f) / cellSize)));
            for (int y = minY; y <= maxY; ++y)
                for (int x = minX; x <= maxX; ++x)
                    blocked[y * width + x] = true;
        }
    }
    // 最も近い2単位間隔の格子点へ四捨五入し、マップ範囲へ制限する。添字はy * width + x。
    auto cell = [&](const cg2::Vector3& position) {
        const int x = (std::clamp)(static_cast<int>(std::round(position.x / cellSize)), 0, width - 1);
        const int y = (std::clamp)(static_cast<int>(std::round(position.y / cellSize)), 0, height - 1);
        return y * width + x;
    };
    const int next = FindExpEnemyNextCell<width, height>(blocked, cell(GetWorldPosition()), cell(target));
    return next >= 0 ? cg2::Vector3{static_cast<float>(next % width) * cellSize, static_cast<float>(next / width) * cellSize, 0.0f}
                     : GetWorldPosition();
}

void ExpEnemy::UpdateExpeditionCombat(Stage& stage, float deltaTime)
{
    // この移動戦闘経路は死亡/非有限時間を除外し、1回の戦闘進行を最大0.20秒に制限する。
    // Update冒頭の寿命・演出タイマーは、呼び出し側から来た秒数で先に進んでいる。
    if (isDead_ || !std::isfinite(deltaTime))
        return;
    const float dt = (std::clamp)(deltaTime, 0.0f, 0.20f);
    if (dt <= 0)
        return;
    combatRepathTimer_ -= dt;
    dashCooldown_ -= dt;
    shooterMuzzleFlashTimer_ = (std::max)(0.0f, shooterMuzzleFlashTimer_ - dt);
    const cg2::Vector3 origin = GetWorldPosition();
    const bool hasTarget = player_ && !player_->IsDead();
    cg2::Vector3 toTarget = hasTarget ? player_->GetWorldPosition() - origin : cg2::Vector3{};
    toTarget.z = 0;
    const float distance = cg2::Length(toTarget);
    const cg2::Vector3 desiredDirection = distance > 0.001f ? toTarget / distance : aimDirection_;
    const cg2::Vector3 tangent{-desiredDirection.y * orbitSign_, desiredDirection.x * orbitSign_, 0};
    const bool visible = hasTarget && cg2::Length(ClipCombatRay(stage, origin, desiredDirection, distance) - origin) >= distance - 0.05f;
    const bool charger = type_ == ExpEnemyType::Charger;
    const bool sniper = type_ == ExpEnemyType::Sniper;
    const bool flanker = type_ == ExpEnemyType::Flanker;
    const bool suppressor = type_ == ExpEnemyType::Suppressor;
    const bool shield = type_ == ExpEnemyType::ShieldGuard || type_ == ExpEnemyType::ReflectArmor;
    const bool blade = type_ == ExpEnemyType::BladeGuard;
    if (hasTarget && !IsAttackAimLocked() && dashTimer_ <= 0 && dashWarningTimer_ <= 0) {
        if (shield) {
            // 盾の照準は最短の角度差を1.65ラジアン/秒以内で回す。
            const float oldAngle = std::atan2(aimDirection_.y, aimDirection_.x);
            const float goalAngle = std::atan2(desiredDirection.y, desiredDirection.x);
            const float turn = std::remainder(goalAngle - oldAngle, 6.28318531f);
            const float angle = oldAngle + (std::clamp)(turn, -1.65f * dt, 1.65f * dt);
            aimDirection_ = {std::cos(angle), std::sin(angle), 0};
        } else
            aimDirection_ = desiredDirection;
    }

    // 射線が通っていても機体幅では塞がる場合があるため、前方の重なりも調べる。
    // 追跡移動で塞がれた場合は経由点を使い、0.32秒経過または到着時に経路を再検索する。
    cg2::Vector3 desiredMoveVelocity{};
    bool movingToTarget = false;
    auto navigate = [&](const cg2::Vector3& direction, float speed, bool pathToTarget) {
        cg2::Vector3 desired = direction;
        if (cg2::Length(desired) < 0.01f || !hasTarget)
            return;
        desired = cg2::Normalize(desired);
        if (pathToTarget && (!visible || stage.IsCollisionWithAnyBlock(origin + desired * 1.7f, 0.85f))) {
            if (combatRepathTimer_ <= 0 || cg2::Length(combatWaypoint_ - origin) < 0.35f) {
                combatWaypoint_ = FindCombatWaypoint(stage, player_->GetWorldPosition());
                combatRepathTimer_ = 0.32f;
            }
            const cg2::Vector3 waypoint = combatWaypoint_ - origin;
            desired = cg2::Length(waypoint) > 0.08f ? cg2::Normalize(waypoint) : cg2::Vector3{};
        }
        desiredMoveVelocity = desired * (speed * authoredMoveSpeedScale_);
        movingToTarget = pathToTarget;
    };

    if (IsSupportRole()) {
        const bool jammer = type_ == ExpEnemyType::EMPJammer;
        const bool mayPulse = hasTarget && distance < (jammer ? 15.0f : 32.0f) && (jammer || summonTotal_ < expguard::kSummonLifetimeLimit);
        // 発動通知は予告終了時に1回だけ来る。EMPはこの時点の距離と遮蔽で適用し、召喚は管理側へ予約する。
        if (supportPulse_.Advance(dt, mayPulse)) {
            supportFlashTimer_ = 0.32f;
            if (jammer) {
                ++empPulseCount_;
                if (visible && distance <= expguard::kEmpRadius + player_->GetRadius())
                    player_->ApplyEmpJammer(2.5f);
            } else
                summonRequested_ = true;
        }
        if (hasTarget && !supportPulse_.IsWarning()) {
            const float preferred = jammer ? 6.4f : 15.0f;
            if (!visible || distance > preferred + 2)
                navigate(desiredDirection + tangent * 0.18f, jammer ? 4.0f : 3.1f, true);
            else if (distance < preferred - 2)
                navigate(desiredDirection * -0.75f + tangent * 0.35f, 3.0f, false);
            else
                navigate(tangent, 1.2f, false);
        }
        SetDamage(hasAuthoredDefinition_ ? authoredContactDamage_ : 8u);
    } else if (blade) {
        bladeCycle_.SetIntervalScale(authoredFireIntervalScale_);
        bladeCycle_.Advance(dt, visible && distance <= expguard::kBladeReach + 0.45f);
        const auto phase = bladeCycle_.GetPhase();
        if (hasTarget && phase == ExpEnemyCombatPhase::Cooldown && distance > 2.8f) {
            navigate(desiredDirection + tangent * 0.08f, 6.2f, true);
        }
        // BladeGuardの接触ダメージは0。予告後の斬撃判定が成立したときだけ自機へダメージを要求する。
        SetDamage(0);
        // TryHitが1振りの受付を消費する。自機が無敵でHPを減らせなくても同じ振りで再試行しない。
        if (hasTarget && bladeCycle_.TryHit(aimDirection_.x, aimDirection_.y, toTarget.x, toTarget.y, player_->GetRadius(), visible)) {
            const int previousHp = player_->GetHp();
            const uint32_t damage = hasAuthoredDefinition_ ? static_cast<uint32_t>(std::ceil(authoredContactDamage_ * 1.5f)) : 27u;
            player_->TakeDamage(damage, 0.55f);
            // 実際にHPが減ったときだけ、ノックバック用の速度と命中演出を設定する。
            if (player_->GetHp() < previousHp) {
                player_->SetVelocity(player_->GetMove() * 0.25f + desiredDirection * 0.50f);
                cg2::ParticleManager::GetInstance()->EmitNeonDeathEffect(player_->GetWorldPosition(), {1.8f, 0.55f, 0.15f, 1},
                                                                         {0.8f, 0.15f, 0.05f, 0}, 0.12f);
            }
        }
    } else if (charger) {
        combatCycle_.SetRecoveryScale((std::max)(0.8f, authoredFireIntervalScale_));
        const bool started = combatCycle_.Advance(dt, visible && distance < 13.0f);
        if (started)
            ++combatDashCount_;
        const auto phase = combatCycle_.GetPhase();
        // ChargerのActiveでは固定済みの方向へ突進し、地形補正が生じたら回復段階へ移る。
        if (phase == ExpEnemyCombatPhase::Active) {
            if (MoveCombatActor(stage, aimDirection_ * (30.0f * authoredMoveSpeedScale_ * dt))) {
                combatCycle_.EnterRecovery();
                cg2::ParticleManager::GetInstance()->EmitNeonDeathEffect(GetWorldPosition(), {1.8f, 0.7f, 0.15f, 1}, {0.8f, 0.2f, 0.1f, 0},
                                                                         0.12f);
            }
        } else if (hasTarget && distance > 2.2f && (phase == ExpEnemyCombatPhase::Cooldown || phase == ExpEnemyCombatPhase::Tracking)) {
            navigate(desiredDirection + tangent * 0.18f, phase == ExpEnemyCombatPhase::Tracking ? 3.0f : 6.8f, true);
        }
        SetDamage(phase == ExpEnemyCombatPhase::Recovery
                      ? 0u
                      : (hasAuthoredDefinition_ ? authoredContactDamage_ : balanceConfig_.contactDamage) *
                            (phase == ExpEnemyCombatPhase::Active ? 2u : 1u));
    } else {
        const float preferred = sniper ? 23.0f : flanker ? 6.5f : suppressor ? 17.5f : shield ? 8.5f : 12.5f;
        const float speed = sniper ? 5.5f : flanker ? 8.0f : suppressor ? 4.2f : shield ? 3.6f : 6.8f;
        const float range = sniper ? 64.0f : flanker ? 11.5f : suppressor ? 29.0f : 24.0f;
        const auto previousPhase = magazineCycle_.GetPhase();
        // ダッシュは弾の到来予測ではなく、周期・距離・射線・移動先の地形で開始を決める。
        // 0.18秒の予告後に短い固定方向の移動を行う。
        if (hasTarget && visible && !suppressor && !shield && previousPhase == ExpEnemyCombatPhase::Cooldown && dashCooldown_ <= 0 &&
            dashTimer_ <= 0 && dashWarningTimer_ <= 0 && (!sniper || distance < 15)) {
            dashDirection_ = sniper    ? desiredDirection * -1.0f
                             : flanker ? cg2::Normalize(desiredDirection * (distance > 8 ? 1.0f : -0.65f) + tangent * 0.60f)
                                       : tangent;
            const cg2::Vector3 destination = origin + dashDirection_ * 4.2f;
            if (!stage.IsCollisionWithAnyBlock(destination, 0.9f) &&
                cg2::Length(ClipCombatRay(stage, origin, dashDirection_, 4.2f) - origin) >= 4.1f) {
                dashWarningTimer_ = 0.18f;
            }
            dashCooldown_ = 2.8f + combatStagger_;
        }
        const bool maneuvering = dashWarningTimer_ > 0 || dashTimer_ > 0;
        if (dashWarningTimer_ > 0) {
            dashWarningTimer_ = (std::max)(0.0f, dashWarningTimer_ - dt);
            if (dashWarningTimer_ <= 0) {
                dashTimer_ = 0.18f;
                ++combatDashCount_;
            }
        } else if (dashTimer_ > 0) {
            const float stepTime = (std::min)(dt, dashTimer_);
            if (MoveCombatActor(stage, dashDirection_ * (21.0f * stepTime)))
                dashTimer_ = 0;
            else
                dashTimer_ = (std::max)(0.0f, dashTimer_ - dt);
        }
        magazineCycle_.SetIntervalScale(authoredFireIntervalScale_);
        // ダッシュ予告/実行中は弾倉周期を停止する。発射通知時には周期側の残弾は既に消費されている。
        const bool shot = !maneuvering && magazineCycle_.Advance(dt, visible && distance <= range);
        const auto phase = magazineCycle_.GetPhase();
        if (!maneuvering && hasTarget && phase != ExpEnemyCombatPhase::Locked && phase != ExpEnemyCombatPhase::Active) {
            const float mobility = phase == ExpEnemyCombatPhase::Recovery   ? 0.25f
                                   : phase == ExpEnemyCombatPhase::Tracking ? (sniper ? 0.0f : 0.45f)
                                                                            : 1.0f;
            if (!visible || distance > preferred + 2.5f)
                navigate(desiredDirection + tangent * 0.22f, speed * mobility, true);
            else if (distance < preferred - 2.5f)
                navigate(desiredDirection * -0.85f + tangent * 0.65f, speed * mobility, false);
            else
                navigate(tangent, speed * mobility * (sniper ? 0.6f : shield ? 0.3f : 0.85f), false);
        }
        SetDamage(magazineCycle_.IsReloading() ? 0u
                                               : (hasAuthoredDefinition_ ? authoredContactDamage_ : balanceConfig_.shooterContactDamage));
        if (shot) {
            AttackParam param{};
            param.bulletSpeed = balanceConfig_.shooterBulletSpeed * (sniper ? 1.30f : flanker ? 0.85f : 0.95f);
            param.bulletCount = flanker ? 5 : suppressor ? 3 : 1;
            param.spreadAngleDeg = flanker ? 50.0f : suppressor ? 32.0f : 0.0f;
            param.randomSpread = false;
            param.damage = hasAuthoredDefinition_ ? authoredBulletDamage_ : balanceConfig_.shooterBulletDamage;
            param.bulletHp = sniper ? tankspecial::kArmoredEnemyBulletHp : tankspecial::kOrdinaryEnemyBulletHp;
            param.bulletPenetration = 1.0f;
            param.canClaimRunResource = false;
            const cg2::Vector3 shotOrigin = GetWorldPosition();
            const cg2::Vector3 muzzle = ClipCombatRay(stage, shotOrigin, aimDirection_, 1.35f);
            // 銃口までの地形検査に通った場合だけ発射する。塞がれても消費済み残弾は戻さない。
            if (cg2::Length(muzzle - shotOrigin) >= 1.25f) {
                attackController_.FireFromMuzzle(muzzle, aimDirection_, param, BulletOwner::kEnemy);
                shooterMuzzleFlashTimer_ = 0.12f;
                ++combatShotsFired_;
            }
        }
    }
    // 自律移動速度は秒当たりの距離。突進開始時は残存速度を弱め、目標速度へ徐々に近づける。
    const bool dashing = IsDashing();
    if (dashing && !combatWasDashing_)
        combatMoveVelocity_ = combatMoveVelocity_ * 0.20f;
    const float response =
        ((blade && bladeCycle_.GetPhase() != ExpEnemyCombatPhase::Cooldown) || (IsSupportRole() && supportPulse_.IsWarning())) ? 12.0f
        : cg2::Length(desiredMoveVelocity) > 0.01f                                                                             ? 2.5f
                                                                                                                               : 3.5f;
    combatMoveVelocity_ += (desiredMoveVelocity - combatMoveVelocity_) * (1.0f - std::exp(-response * dt));
    if (MoveCombatActor(stage, combatMoveVelocity_ * dt)) {
        if (!movingToTarget)
            orbitSign_ = -orbitSign_;
        combatRepathTimer_ = 0;
        combatMoveVelocity_ = combatMoveVelocity_ * 0.35f;
    }
    combatWasDashing_ = dashing;
    // 外部衝撃速度は照準固定や再装填中も残す。60FPS基準から変位へ換算し、
    // 自律移動とは別に同じ地形補正を通した後、指数的に減衰させる。
    if (cg2::Length(velocity_) > 0.001f) {
        if (cg2::Length(velocity_) > 0.95f)
            velocity_ = cg2::Normalize(velocity_) * 0.95f;
        MoveCombatActor(stage, velocity_ * (dt * 60.0f));
        velocity_ = velocity_ * std::exp(-5.0f * dt);
    }
    worldTransform_.rotate = {0, 0, std::atan2(aimDirection_.x, -aimDirection_.y)};
    telegraphEnd_ = ClipCombatRay(stage, GetWorldPosition(), aimDirection_,
                                  blade     ? expguard::kBladeReach
                                  : charger ? 9.0f * authoredMoveSpeedScale_
                                  : sniper  ? 64.0f
                                  : flanker ? 11.5f
                                            : 24.0f);
    if (GetCombatPhase() == ExpEnemyCombatPhase::Recovery) {
        visualColor_ = LerpColor(visualColor_, {0.25f, 0.70f, 0.85f, 1}, 0.65f);
        object_->SetColor(visualColor_);
    }
}

void ExpEnemy::QueueCombatVisuals(cg2::NeonGridRenderer& renderer, const cg2::Vector3& cameraRight, const cg2::Vector3& cameraUp,
                                  const cg2::Vector3& cameraForward, float lineWidth, bool fillOnly) const
{
    if (!IsExpeditionCombatRole() || isDead_)
        return;
    const cg2::Vector3 center = GetWorldPosition() + cg2::Vector3{0.0f, 0.0f, 0.38f};
    const cg2::Vector3 forward = aimDirection_;
    const cg2::Vector3 side{-forward.y, forward.x, 0.0f};
    const float width = (std::max)(0.045f, lineWidth);
    auto bodyPolygon = [&](const cg2::Vector3* points, uint32_t count, const cg2::Vector4& color) {
        if (fillOnly) {
            cg2::Vector3 fill[6]{};
            for (uint32_t i = 0; i < count; ++i) fill[i] = points[i] - cg2::Vector3{0,0,0.005f};
            renderer.QueueBeveledPolygonFill(fill, count, {0.006f,0.010f,0.016f,0.96f}, color,
                cg2::Normalize(cameraUp - cameraRight * 0.5f));
        } else renderer.QueueContourPolygon(points, count, width, color, cameraForward, cg2::ActorNeonContourStyle());
    };
    auto line = [&](const cg2::Vector3& a, const cg2::Vector3& b, float thickness, const cg2::Vector4& color) {
        renderer.QueueCameraFacingLine(a, b, thickness, color, cameraForward);
    };
    auto contour = [&](const cg2::Vector3& a, const cg2::Vector3& b, float thickness, const cg2::Vector4& color) {
        renderer.QueueContourLine(a, b, thickness, color, cameraForward, cg2::ActorNeonContourStyle());
    };
    auto arcPoint = [&](float angle, float radius) {
        return center + (forward * std::cos(angle) + side * std::sin(angle)) * radius;
    };
    if (IsSupportRole()) {
        const bool emp = type_ == ExpEnemyType::EMPJammer;
        const bool warning = supportPulse_.IsWarning();
        const float charge = supportPulse_.WarningRatio();
        const cg2::Vector4 signal = emp ? cg2::Vector4{0.35f, 1.3f, 2.0f, 0.80f} : cg2::Vector4{1.65f, 0.45f, 2.0f, 0.80f};
        constexpr float tau = 6.283185307f;
        const int sides = emp ? 4 : 6;
        cg2::Vector3 bodyPoints[6]{};
        for (int i = 0; i < sides; ++i) {
            const float a = tau * static_cast<float>(i) / sides;
            bodyPoints[i] = arcPoint(a, 0.90f);
        }
        bodyPolygon(bodyPoints, static_cast<uint32_t>(sides), visualColor_);
        if (fillOnly) return;
        for (int i = 0; i < sides; ++i) {
            const float a = tau * static_cast<float>(i) / sides;
            contour(arcPoint(a, 0.55f), arcPoint(a, 1.20f + charge * 0.20f), width, signal);
        }
        // 支援役の予告は既存の線描画へ積み、ここで専用の演出アクター・メッシュ・テクスチャを生成しない。
        const float radius = emp ? expguard::kEmpRadius : 2.8f;
        if (warning || supportFlashTimer_ > 0.0f) {
            const float flash = (std::clamp)(supportFlashTimer_ / 0.32f, 0.0f, 1.0f);
            const float growing = warning ? 0.25f + 0.75f * charge : 1.0f;
            constexpr int segments = 32;
            for (int i = 0; i < segments; ++i) {
                const float a = tau * static_cast<float>(i) / segments;
                const float b = tau * static_cast<float>(i + 1) / segments;
                line(arcPoint(a, radius), arcPoint(b, radius), width * (0.65f + flash),
                     {signal.x, signal.y, signal.z, warning ? 0.30f + 0.30f * charge : flash * 0.7f});
                if (warning)
                    line(arcPoint(a, radius * growing), arcPoint(b, radius * growing), width * 0.60f,
                         {signal.x, signal.y, signal.z, 0.18f + charge * 0.24f});
            }
            if (!emp)
                for (int i = 0; i < 3; ++i) {
                    const float angle = tau * static_cast<float>(i) / 3.0f;
                    const cg2::Vector3 marker = arcPoint(angle, radius);
                    line(marker - side * 0.30f, marker + side * 0.30f, width, signal);
                    line(marker - forward * 0.30f, marker + forward * 0.30f, width, signal);
                }
        }
        return;
    }
    if (type_ == ExpEnemyType::BladeGuard) {
        const auto phase = bladeCycle_.GetPhase();
        const bool recovery = phase == ExpEnemyCombatPhase::Recovery;
        const bool warning = phase == ExpEnemyCombatPhase::Locked;
        const bool active = phase == ExpEnemyCombatPhase::Active;
        const cg2::Vector4 body = recovery ? cg2::Vector4{0.25f, 0.8f, 0.95f, 0.8f} : visualColor_;
        const cg2::Vector3 tip = center + forward * 0.9f;
        const cg2::Vector3 rear = center - forward * 0.9f;
        const cg2::Vector3 bodyPoints[]{tip, center + side * 0.72f, rear, center - side * 0.72f};
        bodyPolygon(bodyPoints, 4, body);
        if (fillOnly) return;

        // 予告中の引き、攻撃中の扇形の振り、回復中の横構えを攻撃周期に合わせて表示する。
        const float pose = warning    ? -expguard::kBladeHalfAngle - 0.2f * bladeCycle_.WarningRatio()
                           : active   ? -expguard::kBladeHalfAngle + 2.0f * expguard::kBladeHalfAngle * bladeCycle_.SweepRatio()
                           : recovery ? 1.65f
                                      : -0.45f;
        const cg2::Vector4 bladeColor = recovery ? cg2::Vector4{0.2f, 0.8f, 1.0f, 0.55f} : cg2::Vector4{2.0f, 0.65f, 0.24f, 0.95f};
        contour(arcPoint(pose, 0.65f), arcPoint(pose, active ? expguard::kBladeReach : 2.0f), width * 1.6f, bladeColor);
        contour(arcPoint(pose - 0.14f, 0.9f), arcPoint(pose + 0.14f, 0.9f), width, bladeColor);
        if (warning || active) {
            const float intensity = warning ? 0.45f + 0.55f * bladeCycle_.WarningRatio() : 1.0f;
            const cg2::Vector4 danger{1.65f, 0.35f + 0.30f * intensity, 0.10f, warning ? 0.42f : 0.70f};
            constexpr int segments = 16;
            for (int i = 0; i < segments; ++i) {
                const float a = -expguard::kBladeHalfAngle + 2.0f * expguard::kBladeHalfAngle * static_cast<float>(i) / segments;
                const float b = -expguard::kBladeHalfAngle + 2.0f * expguard::kBladeHalfAngle * static_cast<float>(i + 1) / segments;
                line(arcPoint(a, expguard::kBladeReach), arcPoint(b, expguard::kBladeReach), width * (active ? 1.7f : 0.7f), danger);
                if (warning && i % 4 == 0) {
                    line(arcPoint(a, 1.1f), arcPoint(a, expguard::kBladeReach), width * 0.55f, {danger.x, danger.y, danger.z, 0.12f});
                }
                if (active && a < pose) {
                    line(arcPoint(a, 2.6f), arcPoint(b, 2.6f), width * 2.7f, {1.8f, 0.5f, 0.13f, 0.34f});
                }
            }
            line(center, arcPoint(-expguard::kBladeHalfAngle, expguard::kBladeReach), width * 0.6f, danger);
            line(center, arcPoint(expguard::kBladeHalfAngle, expguard::kBladeReach), width * 0.6f, danger);
        } else if (recovery) {
            const float radius = 1.15f + bladeCycle_.RecoveryRatio() * 0.20f;
            line(center - side * radius - forward * 0.3f, center - side * radius + forward * 0.3f, width, body);
            line(center + side * radius - forward * 0.3f, center + side * radius + forward * 0.3f, width, body);
        }
        return;
    }
    if (!fillOnly && (type_ == ExpEnemyType::ShieldGuard || type_ == ExpEnemyType::ReflectArmor)) {
        const bool reflect = type_ == ExpEnemyType::ReflectArmor;
        const float halfAngle = reflect ? expguard::kReflectHalfAngle : expguard::kShieldHalfAngle;
        const float flash = shieldFlashTimer_ / 0.13f;
        const cg2::Vector4 shieldColor =
            reflect ? cg2::Vector4{1.7f + flash, 0.25f + 0.45f * flash, 1.6f + 0.55f * flash, 0.58f + 0.35f * flash}
                    : cg2::Vector4{0.25f + 1.1f * flash, 1.15f + 0.8f * flash, 1.65f + 0.5f * flash, 0.58f + 0.35f * flash};
        constexpr int segments = 8;
        for (int i = 0; i < segments; ++i) {
            const float a = -halfAngle + 2.0f * halfAngle * static_cast<float>(i) / segments;
            const float b = -halfAngle + 2.0f * halfAngle * static_cast<float>(i + 1) / segments;
            line(arcPoint(a, 1.45f), arcPoint(b, 1.45f), width * (1.3f + flash), shieldColor);
            // 盾の内側に低い不透明度の帯を重ね、機体や弾・照準を隠しすぎない面表示にする。
            line(arcPoint(a, 1.33f), arcPoint(b, 1.33f), 0.22f, {shieldColor.x, shieldColor.y, shieldColor.z, 0.12f + 0.16f * flash});
        }
        line(arcPoint(-halfAngle, 1.2f), arcPoint(-halfAngle, 1.55f), width, shieldColor);
        line(arcPoint(halfAngle, 1.2f), arcPoint(halfAngle, 1.55f), width, shieldColor);
    }
    const float pulse = 1.0f + GetAttackTelegraphRatio() * 0.10f;
    if (type_ == ExpEnemyType::Charger) {
        const cg2::Vector3 nose = center + forward * (1.18f * pulse);
        const cg2::Vector3 left = center - forward * (0.72f * pulse) + side * (0.82f * pulse);
        const cg2::Vector3 right = center - forward * (0.72f * pulse) - side * (0.82f * pulse);
        const cg2::Vector3 bodyPoints[]{nose, left, right};
        bodyPolygon(bodyPoints, 3, visualColor_);
        if (fillOnly) return;
        contour(center - side * 0.42f, center + forward * 0.48f, width * 0.80f, visualColor_);
        contour(center + forward * 0.48f, center + side * 0.42f, width * 0.80f, visualColor_);
    } else {
        const int sides = type_ == ExpEnemyType::Flanker      ? 3
                          : type_ == ExpEnemyType::Skirmisher ? 4
                          : type_ == ExpEnemyType::Suppressor ? 5
                                                              : 6;
        cg2::Vector3 bodyPoints[6]{};
        for (int i = 0; i < sides; ++i) {
            const float a = static_cast<float>(i) * 2 * cg2::pi / static_cast<float>(sides);
            bodyPoints[i] = center + (side * std::cos(a) + forward * std::sin(a)) * (0.82f * pulse);
        }
        bodyPolygon(bodyPoints, static_cast<uint32_t>(sides), visualColor_);
        if (fillOnly) return;
        contour(center + side * 0.20f, center + forward * 1.35f + side * 0.12f, width, visualColor_);
        contour(center - side * 0.20f, center + forward * 1.35f - side * 0.12f, width, visualColor_);
        contour(center - side * 0.36f, center + side * 0.36f, width, visualColor_);
        // 後部の短い目盛りで弾倉の容量と残弾を表示する。
        const int ammo = magazineCycle_.GetAmmo(), capacity = magazineCycle_.GetCapacity();
        for (int i = 0; i < capacity; ++i) {
            const float offset = (static_cast<float>(i) - static_cast<float>(capacity - 1) * 0.5f) * 0.25f;
            const cg2::Vector3 tick = center - forward * 1.15f + side * offset;
            line(tick, tick - forward * 0.24f, width * 0.8f,
                 i < ammo ? cg2::Vector4{1.6f, 1.5f, 0.6f, 0.95f} : cg2::Vector4{0.14f, 0.18f, 0.22f, 0.5f});
        }
    }
    const ExpEnemyCombatPhase phase = GetCombatPhase();
    if (phase == ExpEnemyCombatPhase::Tracking || phase == ExpEnemyCombatPhase::Locked) {
        const bool locked = phase == ExpEnemyCombatPhase::Locked;
        const cg2::Vector4 warning = locked                          ? cg2::Vector4{2.1f, 1.45f, 0.75f, 0.95f}
                                     : type_ == ExpEnemyType::Sniper ? cg2::Vector4{1.8f, 0.20f, 0.62f, 0.52f}
                                                                     : cg2::Vector4{1.8f, 0.50f, 0.12f, 0.52f};
        const float distance = cg2::Length(telegraphEnd_ - GetWorldPosition());
        const float start = (std::min)(1.5f, distance);
        if (type_ == ExpEnemyType::Sniper) {
            for (float t = start; t < distance; t += locked ? 1.6f : 2.0f) {
                line(center + forward * t, center + forward * (std::min)(distance, t + (locked ? 1.30f : 0.85f)),
                     locked ? width * 0.85f : width * 0.60f, warning);
            }
        } else if (type_ == ExpEnemyType::Charger) {
            for (float t = start; t < distance; t += 2.0f) {
                const cg2::Vector3 marker = center + forward * t;
                line(marker - forward * 0.50f + side * 0.40f, marker, width * 0.80f, warning);
                line(marker - forward * 0.50f - side * 0.40f, marker, width * 0.80f, warning);
            }
        } else {
            // 射撃役の予告線は種類ごとに短く切り、拡散射撃では左右の線を加える。
            const float reach = (std::min)(distance, type_ == ExpEnemyType::Skirmisher ? 3.5f : 6.0f);
            const float fan = type_ == ExpEnemyType::Flanker ? 0.46f : type_ == ExpEnemyType::Suppressor ? 0.27f : 0.0f;
            line(center + forward * start, center + forward * reach + side * (reach * fan), width * 0.75f, warning);
            if (fan > 0)
                line(center + forward * start, center + forward * reach - side * (reach * fan), width * 0.75f, warning);
        }
        if (type_ == ExpEnemyType::Charger || type_ == ExpEnemyType::Sniper) {
            const cg2::Vector3 end = center + forward * distance;
            line(end - side * 0.55f, end + side * 0.55f, width, warning);
        }
    }
    if (phase == ExpEnemyCombatPhase::Recovery) {
        const cg2::Vector4 recovery{0.25f, 1.0f, 1.2f, 0.75f};
        const float ratio = type_ == ExpEnemyType::Charger ? combatCycle_.GetRecoveryRatio() : 1.0f - magazineCycle_.GetReloadProgress();
        const float radius = 1.15f + ratio * 0.15f;
        line(center + cameraRight * radius - cameraUp * 0.25f, center + cameraRight * radius + cameraUp * 0.25f, width, recovery);
        line(center - cameraRight * radius - cameraUp * 0.25f, center - cameraRight * radius + cameraUp * 0.25f, width, recovery);
        if (type_ != ExpEnemyType::Charger) {
            const cg2::Vector3 start = center - cameraUp * 1.5f - cameraRight * 0.9f;
            line(start, start + cameraRight * 1.8f, width * 0.7f, {0.12f, 0.3f, 0.4f, 0.6f});
            line(start, start + cameraRight * (1.8f * magazineCycle_.GetReloadProgress()), width * 1.5f, recovery);
        }
    }
    if (dashWarningTimer_ > 0 || dashTimer_ > 0) {
        const cg2::Vector3 dashSide{-dashDirection_.y, dashDirection_.x, 0};
        const cg2::Vector4 dashColor{0.4f, 1.6f, 2.0f, 0.75f};
        for (int i = 1; i <= 2; ++i) {
            const cg2::Vector3 marker = center + dashDirection_ * (static_cast<float>(i) * 1.5f);
            line(marker - dashDirection_ * 0.45f + dashSide * 0.35f, marker, width, dashColor);
            line(marker - dashDirection_ * 0.45f - dashSide * 0.35f, marker, width, dashColor);
        }
        if (dashTimer_ > 0) {
            line(center + dashSide * 0.5f, center + dashSide * 0.5f - dashDirection_ * 2.2f, width * 1.5f, dashColor);
            line(center - dashSide * 0.5f, center - dashSide * 0.5f - dashDirection_ * 2.2f, width * 1.5f, dashColor);
        }
    }
    if (shooterMuzzleFlashTimer_ > 0.0f) {
        const cg2::Vector3 muzzle = center + forward * 1.45f;
        const cg2::Vector4 flash{2.4f, 1.6f, 0.9f, 1.0f};
        line(muzzle - side * 0.35f, muzzle + side * 0.35f, width * 1.5f, flash);
        line(muzzle - forward * 0.28f, muzzle + forward * 0.28f, width * 1.5f, flash);
    }
}

void ExpEnemy::Draw(bool drawBody)
{
    if (!drawBody) {
        return;
    }
    DrawBodyOnly();
}

void ExpEnemy::DrawBodyOnly()
{
    if (shapeNeonRenderMode_ != 0 && IsShapeNeonRenderTarget()) {
        return;
    }
    object_->Draw();
}

void ExpEnemy::DrawNeonFillBodyOnly()
{
    if (shapeNeonRenderMode_ != 3 || !IsShapeNeonRenderTarget()) {
        return;
    }

    const cg2::Vector4 savedColor = object_->GetColor();
    const bool savedLighting = object_->IsLightingEnabled();
    const float savedEnvironmentCoefficient = object_->GetEnvironmentCoefficient();

    object_->SetLighting(false);
    object_->SetEnvironmentCoefficient(0.0f);
    object_->SetColor({0.0f, 0.0f, 0.0f, 1.0f});
    object_->Draw();

    object_->SetColor(savedColor);
    object_->SetLighting(savedLighting);
    object_->SetEnvironmentCoefficient(savedEnvironmentCoefficient);
}

void ExpEnemy::OnCollision(Collider* other)
{
    // 衝突一覧を作った後でも、前の通知で対象が死亡することがある。
    // 同じフレームの後続通知からHPや撃破報酬をもう一度処理しない。
    if (isDead_) {
        return;
    }
    const uint32_t otherAttribute = other->GetCollisionAttribute();
    if (isRunResource_ && (otherAttribute == kCollisionAttributePlayer || otherAttribute == kCollisionAttributeEnemy ||
                           otherAttribute == kCollisionAttributePlayerDrone)) {
        return;
    }
    if (isRunResource_) {
        const auto* bullet = dynamic_cast<const Bullet*>(other);
        if (bullet && !bullet->CanClaimRunResource())
            return;
    }
    cg2::Vector3 hitDir = worldTransform_.translate - other->GetWorldPosition();

    if (cg2::Length(hitDir) < 0.0001f) {
        hitDir = {1.0f, 0.0f, 0.0f};
    }
    hitDir = cg2::Normalize(hitDir);

    const float kKnockBackPower = 0.05f;

    // 接触による速度加算はダメージ可否や自機接触の無敵判定より先に行う。位置は後のUpdateで進める。
    if (!isRunResource_)
        velocity_ += hitDir * kKnockBackPower * other->GetHitPower() * (dt_ * 60.0f);
    if (type_ == ExpEnemyType::Charger && otherAttribute == kCollisionAttributePlayer &&
        combatCycle_.GetPhase() == ExpEnemyCombatPhase::Active) {
        combatCycle_.EnterRecovery();
        velocity_ = {};
        // 相手への通知で使う、このフレームの突進接触ダメージはここでは変更しない。
    }

    if (other->GetCollisionAttribute() == kCollisionAttributeEnemy) {
        return;
    }

    const bool canTakeDamage = other->GetCollisionAttribute() == kCollisionAttributePlayer ||
                               other->GetCollisionAttribute() == kCollisionAttributePlayerBullet ||
                               other->GetCollisionAttribute() == kCollisionAttributePlayerDrone ||
                               (other->GetCollisionAttribute() == kCollisionAttributeEnemyBullet && (IsHostileToBoss() || isRunResource_));
    if (!canTakeDamage) {
        return;
    }

    if (other->GetCollisionAttribute() == kCollisionAttributePlayer && invincibleTimer_ > 0.0f) {
        return;
    }

    const bool killedByEnemy = other->GetCollisionAttribute() == kCollisionAttributeEnemyBullet && (IsHostileToBoss() || isRunResource_);
    uint32_t damage = other->GetDamage();
    if (!killedByEnemy) {
        cg2::Vector3 source = other->GetWorldPosition();
        bool melee = otherAttribute == kCollisionAttributePlayer;
        if (auto* bullet = dynamic_cast<Bullet*>(other)) {
            // 弾の中心位置ではなく現在の入射側を盾判定に使う。特殊弾は移動前後の変位を優先し、
            // それ以外や変位がほぼ0のときは速度を使う。
            const cg2::Vector3 sweptMovement = bullet->GetWorldPosition() - bullet->GetPreviousWorldPosition();
            const cg2::Vector3 movement = bullet->GetSpecialKind() != Bullet::SpecialKind::None && cg2::Length(sweptMovement) > 0.0001f
                                              ? sweptMovement
                                              : bullet->GetMove();
            if (cg2::Length(movement) > 0.0001f)
                source = GetWorldPosition() - cg2::Normalize(movement);
            melee = bullet->GetSpecialKind() == Bullet::SpecialKind::SlashWave;
        }
        damage = ResolveShieldDamage(damage, source, melee);
    }
    ApplyDamage(damage, !killedByEnemy, true);

    // この接触経路では、ダメージ0でHPを減らせなかった場合も自機接触の再受付を0.5秒抑える。
    if (other->GetCollisionAttribute() == kCollisionAttributePlayer) {
        invincibleTimer_ = 0.5f;
    }
}

bool ExpEnemy::TakeDamageFromEnemy(uint32_t amount)
{
    // ボスの接触処理は、返された撃破結果を使って自分で経験値敵の報酬を加算する。
    return ApplyDamage(amount, false, false);
}

bool ExpEnemy::TakeDamageFromPlayer(uint32_t amount)
{
    return TakeDirectionalDamage(amount, player_ ? player_->GetWorldPosition() : GetWorldPosition());
}

uint32_t ExpEnemy::ResolveShieldDamage(uint32_t amount, const cg2::Vector3& attackSource, bool melee)
{
    if (type_ != ExpEnemyType::ShieldGuard || isDead_ || amount == 0)
        return amount;
    const cg2::Vector3 offset = attackSource - GetWorldPosition();
    const uint32_t result = expguard::ShieldDamage(amount, aimDirection_.x, aimDirection_.y, offset.x, offset.y, melee);
    if (result < amount) {
        ++shieldBlockCount_;
        if (shieldFlashTimer_ <= 0.0f) {
            const cg2::Vector3 normal = cg2::Length(offset) > 0.0001f ? cg2::Normalize(offset) : aimDirection_;
            cg2::ParticleManager::GetInstance()->EmitNeonImpactEffect(GetWorldPosition() + normal * 1.25f, normal,
                                                                      {0.30f, 1.40f, 1.90f, 0.95f}, 4);
        }
        shieldFlashTimer_ = 0.13f;
    }
    return result;
}

bool ExpEnemy::TakeDirectionalDamage(uint32_t amount, const cg2::Vector3& attackSource, bool melee)
{
    return ApplyDamage(ResolveShieldDamage(amount, attackSource, melee), true, false);
}

void ExpEnemy::ApplyKnockback(const cg2::Vector3& direction, float power)
{
    if (isDead_ || isRunResource_ || !std::isfinite(power) || power <= 0.0f || cg2::Length(direction) < 0.001f)
        return;
    velocity_ += cg2::Normalize(direction) * (std::min)(0.95f, power);
    if (power >= 0.16f)
        wallImpactArmedTimer_ = 1.0f;
    if (cg2::Length(velocity_) > 0.95f)
        velocity_ = cg2::Normalize(velocity_) * 0.95f;
    // 強い衝撃ではChargerの攻撃周期と短いダッシュを中断する。
    // 弾倉の周期・残弾は保持し、衝撃だけで再装填や即時射撃は行わない。
    if (power >= 0.30f) {
        if (type_ == ExpEnemyType::Charger)
            combatCycle_.EnterRecovery();
        dashTimer_ = dashWarningTimer_ = 0.0f;
        dashCooldown_ = (std::max)(dashCooldown_, 0.65f);
    }
}

bool ExpEnemy::ApplyDamage(uint32_t amount, bool playerOwned, bool reportOrdinaryEnemyKill)
{
    if (isDead_ || amount == 0) {
        return false;
    }
    hp_ -= static_cast<int>((std::min)(amount, static_cast<uint32_t>((std::max)(0, hp_))));
    TriggerDamageFeedback();
    if (hp_ <= 0) {
        // 通知先からの再入や同じフレームの後続ダメージに備え、撃破通知より先に死亡を確定する。
        isDead_ = true;
        cg2::ParticleManager::GetInstance()->EmitNeonDeathEffect(
            GetWorldPosition(), isRunResource_ ? cg2::Vector4{1.35f, 0.92f, 0.28f, 1.0f} : cg2::Vector4{visualColor_.x, visualColor_.y, visualColor_.z, 1.0f},
            {0.18f, 1.10f, 1.35f, 0.0f}, isRunResource_ ? 0.46f : 0.32f);
        // 資源は取得側の通知、通常敵は自機側の経験値/撃破通知か敵側の撃破通知へ分ける。
        // 召喚個体の撃破では、これらの報酬通知を行わない。
        if (isRunResource_) {
            if (runResourceClaimCallback_)
                runResourceClaimCallback_(playerOwned);
        } else if (playerOwned && !IsSummonedUnit()) {
            if (player_)
                player_->AddExp(expValue_);
            if (playerDefeatCallback_)
                playerDefeatCallback_(GetWorldPosition());
        } else if (!IsSummonedUnit() && reportOrdinaryEnemyKill && enemyKillCallback_) {
            enemyKillCallback_(expValue_);
        }
        return true;
    }
    return false;
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

    const float impact = t * t;

    worldTransform_.scale = baseScale_ * (1.0f + impact * (isRunResource_ ? 0.22f : 0.10f));

    visualColor_ = LerpColor(baseColor_, {1.0f, 1.0f, 1.0f, baseColor_.w}, (std::min)(1.0f, impact));

    object_->SetColor(visualColor_);
}

cg2::AABB ExpEnemy::GetAABB()
{
    cg2::Vector3 worldPos = GetWorldPosition();

    cg2::AABB aabb;

    aabb.min = {worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f};
    aabb.max = {worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f};

    return aabb;
}
