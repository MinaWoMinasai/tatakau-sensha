#include "BulletManager.h"
#include "Bullet.h"
#include <algorithm>
#include "Stage.h"
#include "Player.h"
#include "Enemy.h"
#include "EnemyManager.h"
#include <array>

void BulletManager::Initialize(cg2::DirectXCommon* dxCommon, cg2::Object3dCommon* object3dCommon)
{
    trailManager_ = std::make_unique<cg2::TrailManager>();
    trailManager_->Initialize(dxCommon, object3dCommon, "resources/white512x512.png");
}

void BulletManager::SetTrailSettings(const BulletTrailSettings& settings)
{
    trailSettings_ = settings;
    const BulletTrailSettings defaults{};
    const auto bounded = [](float value, float fallback, float lower, float upper) {
        return std::isfinite(value) ? (std::clamp)(value, lower, upper) : fallback;
    };
    auto& trail = trailSettings_;
    trail.playerHalfWidth = bounded(trail.playerHalfWidth, defaults.playerHalfWidth, 0.01f, 1.5f);
    trail.enemyHalfWidth = bounded(trail.enemyHalfWidth, defaults.enemyHalfWidth, 0.01f, 1.5f);
    trail.lifetime = bounded(trail.lifetime, defaults.lifetime, 0.02f, 1.5f);
    trail.maxPoints = (std::clamp)(trail.maxPoints, 2, 80);
    trail.interpolationSteps = (std::clamp)(trail.interpolationSteps, 1, 12);
    trail.headWidthScale = bounded(trail.headWidthScale, defaults.headWidthScale, 0.0f, 4.0f);
    trail.tailWidthScale = bounded(trail.tailWidthScale, defaults.tailWidthScale, 0.0f, 4.0f);
    trail.widthCurvePower = bounded(trail.widthCurvePower, defaults.widthCurvePower, 0.05f, 6.0f);
    trail.colorCurvePower = bounded(trail.colorCurvePower, defaults.colorCurvePower, 0.05f, 6.0f);
    trail.trailHeadIntensity = bounded(trail.trailHeadIntensity, defaults.trailHeadIntensity, 0.0f, 5.0f);
    trail.trailTailIntensity = bounded(trail.trailTailIntensity, defaults.trailTailIntensity, 0.0f, 5.0f);
    trail.trailHeadAlpha = bounded(trail.trailHeadAlpha, defaults.trailHeadAlpha, 0.0f, 1.0f);
    trail.trailTailAlpha = bounded(trail.trailTailAlpha, defaults.trailTailAlpha, 0.0f, 1.0f);
    trail.playerTrailLifetimeScale = bounded(trail.playerTrailLifetimeScale, defaults.playerTrailLifetimeScale, 0.0f, 4.0f);
    trail.playerTrailAlphaScale = bounded(trail.playerTrailAlphaScale, defaults.playerTrailAlphaScale, 0.0f, 1.0f);
}

void BulletManager::Add(std::unique_ptr<Bullet> bullet)
{
    if (!bullet)
        return;
    // 上限判定は追加する弾のルールで決めるが、同じ所有者の生存弾は通常弾も含めて数える。
    if (bullet->UsesRunProjectileRules()) {
        const auto liveCount = std::count_if(bullets_.begin(), bullets_.end(), [&](const auto& existing) {
            return !existing->IsDead() && existing->GetOwner() == bullet->GetOwner();
        });
        if (static_cast<size_t>(liveCount) >= kMaxRunProjectilesPerOwner)
            return;
    }
    // 拒否のreturnでは引数のunique_ptrが破棄する。軌跡は登録する弾だけに取り付ける。
    if (trailManager_) {
        bullet->AttachTrail(trailManager_.get(), &trailSettings_);
    }
    bullets_.push_back(std::move(bullet));
}

void BulletManager::FlushPendingSplits()
{
    // 衝突通知で予約した弾を先に退避する。生成後の弾をこの予約列で再帰処理しない。
    auto pending = std::move(pendingBuildShots_);
    pendingBuildShots_.clear();
    for (const auto& spec : pending) {
        auto shot = std::make_unique<Bullet>();
        shot->Initialize(spec.position, spec.velocity, spec.damage, spec.reflection ? kEnemy : kPlayer, false,
                         spec.reflection ? 6.0f : 2.0f, spec.reflection ? 4.0f : 2.0f);
        shot->ConfigureGrowth(0, 0, 0);
        shot->SetArmorReflected(spec.reflection);
        shot->SetBurstChild(!spec.reflection);
        shot->SetCanClaimRunResource(!spec.reflection);
        if (spec.reflection)
            shot->ConfigureSpecial(Bullet::SpecialKind::ArmorReflection, .42f, 2.0f);
        const size_t before = bullets_.size();
        Add(std::move(shot));
        if (!spec.reflection && bullets_.size() > before)
            ++shooterStats_.burstChildren;
    }
    // この後の分裂用に所有者ごとの残り枠を計算する。生成予定の子も加算し、後続の親と枠を共有する。
    size_t liveCounts[3]{};
    for (const auto& bullet : bullets_) {
        if (!bullet->IsDead())
            ++liveCounts[static_cast<size_t>(bullet->GetOwner())];
    }
    // 元の弾を走査しきるまで子弾を別配列へ集め、bullets_の再確保で走査が壊れるのを防ぐ。
    std::vector<std::unique_ptr<Bullet>> children;
    for (const auto& bullet : bullets_) {
        // 死亡して削除を待つ親からも記録を回収する。特殊命中の保留上限を超えた分は持ち越さない。
        const auto events = bullet->ConsumeGrowthEvents();
        for (const auto& impact : bullet->ConsumeSpecialImpacts())
            if (specialImpacts_.size() < 64)
                specialImpacts_.push_back(impact);
        if (bullet->GetOwner() == kPlayer) {
            growthStats_.wallBounces += events.wallBounces;
            growthStats_.actorPierces += events.actorPierces;
        }
        size_t& live = liveCounts[static_cast<size_t>(bullet->GetOwner())];
        const size_t available = live < kMaxRunProjectilesPerOwner ? kMaxRunProjectilesPerOwner - live : 0;
        const size_t before = children.size();
        bullet->AppendImpactChildren(children, available);
        if (children.size() > before && bullet->GetOwner() == kPlayer)
            ++growthStats_.impactSplits;
        live += children.size() - before;
    }
    // 全ての親を走査してから管理配列へ移す。集計はAddによる実際の登録増加を確認する。
    for (auto& child : children) {
        const size_t before = bullets_.size();
        const bool playerOwned = child->GetOwner() == kPlayer;
        Add(std::move(child));
        if (bullets_.size() > before && playerOwned)
            ++growthStats_.splitChildrenSpawned;
    }
}

void BulletManager::ClearAll()
{
    // 弾が保持する軌跡への参照を先に外し、その後に所有側の軌跡を破棄する。
    for (auto& bullet : bullets_)
        if (bullet)
            bullet->ReleaseTrail();
    bullets_.clear();
    growthStats_ = {};
    specialImpacts_.clear();
    pendingBuildShots_.clear();
    buildEvents_.clear();
    marks_ = {};
    shooterStats_ = {};
    // 部屋・敵・ステージを置き換える前に借用先も解除し、古い戦闘対象を次の更新で参照しない。
    combatPlayer_ = nullptr;
    combatBoss_ = nullptr;
    combatEnemies_ = nullptr;
    combatStage_ = nullptr;
    if (trailManager_)
        trailManager_->ClearInstances();
}

void BulletManager::Update(Stage& stage, float deltaTime)
{
    // 地形はこの更新から次の戦闘処理まで借用する。マーキングの寿命も秒で進める。
    combatStage_ = &stage;
    marks_.Update(deltaTime);
    for (auto& bullet : bullets_) {
        if (combatPlayer_ && bullet->IsBoomerang())
            bullet->SetReturnTarget(combatPlayer_->GetWorldPosition());
        // 更新前後の差を見て、帰還開始を1回だけ集計する。既に帰還中の弾は再集計しない。
        const bool returning = bullet->GetIsReturning();
        bullet->Update(deltaTime);
        if (!returning && bullet->GetIsReturning()) {
            ++shooterStats_.returns;
            BuildEventAt(BuildEventKind::Return, bullet->GetWorldPosition(), bullet->GetWorldPosition());
        }
    }
    if (trailManager_) {
        trailManager_->Update(deltaTime);
    }

    // 移動後に地形へ通知し、分裂予約を取り込んでから死亡弾を削除する。
    stage.ResolveBulletsCollision(GetBulletPtrs());

    // 地形との衝突通知が終わった時点で、予約された追加生成を反映する。
    FlushPendingSplits();

    // アクター同士の衝突走査では削除しない。ここが弾の借用ポインターを無効にする境界になる。
    bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(),
                                  [](const std::unique_ptr<Bullet>& bullet) {
                                      if (bullet->IsDead()) {
                                          bullet->ReleaseTrail();
                                      }
                                      return bullet->IsDead();
                                  }),
                   bullets_.end());
}

void BulletManager::Draw()
{
    for (auto& bullet : bullets_) {
        bullet->Draw();
    }
}

void BulletManager::DrawTrails(const cg2::Matrix4x4& viewProjection)
{
    if (trailManager_) {
        trailManager_->DrawAll(viewProjection);
    }
}

std::vector<Bullet*> BulletManager::GetBulletPtrs() const
{
    std::vector<Bullet*> result;
    result.reserve(bullets_.size());
    for (const auto& b : bullets_) {
        result.push_back(b.get());
    }
    return result;
}

BulletManager::BulletCounts BulletManager::GetBulletCounts() const
{
    BulletCounts counts{};
    for (const auto& bullet : bullets_) {
        if (!bullet) {
            continue;
        }
        switch (bullet->GetOwner()) {
        case kPlayer:
            ++counts.player;
            break;
        case kEnemy:
            ++counts.enemy;
            break;
        case kExpEnemyHostile:
            ++counts.hostileExpEnemy;
            break;
        }
    }
    return counts;
}

size_t BulletManager::GetTrailInstanceCount() const
{
    return trailManager_ ? trailManager_->GetInstanceCount() : 0;
}

void BulletManager::BuildEventAt(BuildEventKind kind, const cg2::Vector3& start, const cg2::Vector3& end, float strength)
{
    if (buildEvents_.size() < 64)
        buildEvents_.push_back({kind, start, end, strength});
}

std::vector<Collider*> BulletManager::LiveCombatTargets() const
{
    std::vector<Collider*> targets;
    if (combatBoss_ && !combatBoss_->IsDead() && combatBoss_->GetHp() > 0)
        targets.push_back(combatBoss_);
    if (combatEnemies_)
        for (auto* target : combatEnemies_->GetEnemyPtrs())
            if (target && !target->IsDead() && !target->IsRunResource())
                targets.push_back(target);
    return targets;
}

bool BulletManager::HasClearLink(const cg2::Vector3& from, const cg2::Vector3& to) const
{
    if (!combatStage_)
        return true;
    // 0.45ワールド単位を目安に内部を分割し、半径0.08の点で遮蔽を近似する。端点はこのループで調べない。
    const float length = cg2::Length(to - from);
    const int steps = (std::clamp)(static_cast<int>(std::ceil(length / .45f)), 1, 128);
    for (int i = 1; i < steps; ++i)
        if (combatStage_->IsCollisionWithAnyBlock(from + (to - from) * (static_cast<float>(i) / steps), .08f))
            return false;
    return true;
}

bool BulletManager::DamageBuildTarget(Collider* target, uint32_t damage, const cg2::Vector3& source)
{
    if (auto* enemy = dynamic_cast<ExpEnemy*>(target)) {
        if (enemy->IsDead() || enemy->IsRunResource())
            return false;
        return enemy->TakeDirectionalDamage(damage, source);
    }
    if (auto* boss = dynamic_cast<Enemy*>(target)) {
        const int hp = boss->GetHp();
        boss->TakeDamage(damage);
        return hp > 0 && boss->GetHp() <= 0;
    }
    return false;
}

void BulletManager::QueueKillBurst(const Bullet& bullet, const cg2::Vector3& position)
{
    // 破裂子弾からは再発動しない。6発分をまとめて予約し、衝突中の管理配列を増やさない。
    if (!bullet.GetShooterAbilities().killBurst || bullet.IsBurstChild() || pendingBuildShots_.size() > 42)
        return;
    ++shooterStats_.burstTriggers;
    const auto damage = tankshooter::Damage(bullet.GetDamage(), .35f, bullet.GetShooterAbilities().burstPower);
    for (int i = 0; i < tankshooter::kBurstChildren; ++i) {
        const float angle = static_cast<float>(i) * 6.2831853f / tankshooter::kBurstChildren;
        const cg2::Vector3 direction{std::cos(angle), std::sin(angle), 0};
        pendingBuildShots_.push_back({position + direction * .35f, direction * .30f, damage, false});
    }
    BuildEventAt(BuildEventKind::Burst, position, position);
}

void BulletManager::NotifyPlayerHit(Bullet& bullet, Collider& target, bool killed)
{
    if (bullet.GetOwner() != kPlayer || bullet.IsBurstChild() || bullet.GetSourceDroneIndex() >= 0)
        return;
    if (auto* ordinary = dynamic_cast<ExpEnemy*>(&target); ordinary && ordinary->IsRunResource())
        return;
    const auto& ability = bullet.GetShooterAbilities();
    if (killed)
        QueueKillBurst(bullet, target.GetWorldPosition());
    // 1回の連鎖内で同じ衝突IDを選び直さない。各段で生存対象を取り直し、前段の撃破を反映する。
    if (ability.chain) {
        cg2::Vector3 from = target.GetWorldPosition();
        std::array<uint64_t, 3> visited{target.GetCollisionId(), 0, 0};
        for (int hop = 0; hop < tankshooter::kChainTargets; ++hop) {
            Collider* nearest = nullptr;
            float distance = tankshooter::kChainRadius;
            for (auto* candidate : LiveCombatTargets()) {
                if (std::find(visited.begin(), visited.end(), candidate->GetCollisionId()) != visited.end())
                    continue;
                const float d = cg2::Length(candidate->GetWorldPosition() - from);
                if (d < distance && HasClearLink(from, candidate->GetWorldPosition())) {
                    nearest = candidate;
                    distance = d;
                }
            }
            if (!nearest)
                break;
            visited[hop + 1] = nearest->GetCollisionId();
            const auto to = nearest->GetWorldPosition();
            const bool chainKill = DamageBuildTarget(nearest, tankshooter::Damage(bullet.GetDamage(), .48f, ability.chainPower), from);
            ++shooterStats_.chainHits;
            BuildEventAt(BuildEventKind::Chain, from, to);
            if (chainKill)
                QueueKillBurst(bullet, to);
            from = to;
        }
    }
    // 起爆は命中の蓄積が条件を満たしたときだけ。中心・周囲・ボスの倍率を分け、撃破時の破裂も予約する。
    if (ability.mark && !killed && marks_.Hit(target.GetCollisionId(), dynamic_cast<Enemy*>(&target) != nullptr)) {
        const auto center = target.GetWorldPosition();
        ++shooterStats_.detonations;
        BuildEventAt(BuildEventKind::Detonation, center, center, 2.8f);
        for (auto* candidate : LiveCombatTargets()) {
            if (cg2::Length(candidate->GetWorldPosition() - center) > 2.8f + candidate->GetRadius() ||
                !HasClearLink(center, candidate->GetWorldPosition()))
                continue;
            const bool boss = dynamic_cast<Enemy*>(candidate) != nullptr;
            const float scale = 1.75f * (candidate == &target ? 1.0f : .50f) * (boss ? .65f : 1.0f);
            if (DamageBuildTarget(candidate, tankshooter::Damage(bullet.GetDamage(), scale, ability.markPower), center))
                QueueKillBurst(bullet, candidate->GetWorldPosition());
        }
    }
}

std::vector<BulletManager::MarkVisual> BulletManager::GetMarkVisuals() const
{
    std::vector<MarkVisual> out;
    if (std::none_of(marks_.entries.begin(), marks_.entries.end(), [](const auto& m) {
            return m.id != 0;
        }))
        return out;
    for (auto* target : LiveCombatTargets())
        for (const auto& mark : marks_.entries)
            if (mark.id == target->GetCollisionId() && mark.remaining > 0)
                out.push_back({target->GetWorldPosition(), mark.stacks});
    return out;
}

void BulletManager::QueueArmorReflection(const Bullet& bullet, const cg2::Vector3& targetPosition)
{
    if (pendingBuildShots_.size() >= 48 || bullet.WasArmorReflected())
        return;
    // 実際の移動区間を優先して逆向きに返す。区間がほぼ0なら保持する速度を使い、静止弾は予約しない。
    const auto incoming = bullet.GetWorldPosition() - bullet.GetPreviousWorldPosition();
    const auto velocity =
        cg2::Length(incoming) > .0001f ? cg2::Normalize(incoming) * -cg2::Length(bullet.GetMove()) : bullet.GetMove() * -1.0f;
    if (cg2::Length(velocity) < .0001f)
        return;
    pendingBuildShots_.push_back(
        {targetPosition + cg2::Normalize(velocity) * 1.7f, velocity * .85f, tankshooter::Damage(bullet.GetDamage(), .45f), true});
    ++shooterStats_.reflections;
    BuildEventAt(BuildEventKind::Reflection, targetPosition, targetPosition);
}
