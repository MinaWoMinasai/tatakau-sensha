#include "BulletManager.h"
#include "Bullet.h"
#include <algorithm>
#include "Stage.h"
#include "Player.h"
#include "Enemy.h"
#include "EnemyManager.h"
#include <array>

void BulletManager::Initialize(DirectXCommon* dxCommon, Object3dCommon* object3dCommon) {
    trailManager_ = std::make_unique<TrailManager>();
    trailManager_->Initialize(dxCommon, object3dCommon, "resources/white512x512.png");
}

void BulletManager::Add(std::unique_ptr<Bullet> bullet) {
    if (!bullet) return;
    if (bullet->UsesRunProjectileRules()) {
        const auto liveCount = std::count_if(bullets_.begin(), bullets_.end(), [&](const auto& existing) {
            return !existing->IsDead() && existing->GetOwner() == bullet->GetOwner();
        });
        if (static_cast<size_t>(liveCount) >= kMaxRunProjectilesPerOwner) return;
    }
    if (trailManager_) {
        bullet->AttachTrail(trailManager_.get(), &trailSettings_);
    }
    bullets_.push_back(std::move(bullet));
}

void BulletManager::FlushPendingSplits()
{
    auto pending=std::move(pendingBuildShots_);pendingBuildShots_.clear();
    for(const auto& spec:pending) {
        auto shot=std::make_unique<Bullet>();
        shot->Initialize(spec.position,spec.velocity,spec.damage,spec.reflection?kEnemy:kPlayer,false,
            spec.reflection?6.0f:2.0f,spec.reflection?4.0f:2.0f);
        shot->ConfigureGrowth(0,0,0);
        shot->SetArmorReflected(spec.reflection);
        shot->SetBurstChild(!spec.reflection);
        shot->SetCanClaimRunResource(!spec.reflection);
        if(spec.reflection)shot->ConfigureSpecial(Bullet::SpecialKind::ArmorReflection,.42f,2.0f);
        const size_t before=bullets_.size();Add(std::move(shot));
        if(!spec.reflection&&bullets_.size()>before)++shooterStats_.burstChildren;
    }
    size_t liveCounts[3]{};
    for (const auto& bullet : bullets_) {
        if (!bullet->IsDead()) ++liveCounts[static_cast<size_t>(bullet->GetOwner())];
    }
    std::vector<std::unique_ptr<Bullet>> children;
    for (const auto& bullet : bullets_) {
        const auto events = bullet->ConsumeGrowthEvents();
		for(const auto& impact:bullet->ConsumeSpecialImpacts())if(specialImpacts_.size()<64)specialImpacts_.push_back(impact);
        if (bullet->GetOwner() == kPlayer) {
            growthStats_.wallBounces += events.wallBounces;
            growthStats_.actorPierces += events.actorPierces;
        }
        size_t& live = liveCounts[static_cast<size_t>(bullet->GetOwner())];
        const size_t available = live < kMaxRunProjectilesPerOwner ? kMaxRunProjectilesPerOwner - live : 0;
        const size_t before = children.size();
        bullet->AppendImpactChildren(children, available);
        if (children.size() > before && bullet->GetOwner() == kPlayer) ++growthStats_.impactSplits;
        live += children.size() - before;
    }
    for (auto& child : children) {
        const size_t before = bullets_.size();
        const bool playerOwned = child->GetOwner() == kPlayer;
        Add(std::move(child));
        if (bullets_.size() > before && playerOwned) ++growthStats_.splitChildrenSpawned;
    }
}

void BulletManager::ClearAll()
{
    // Release each pointer before destroying the trail instances it refers to.
    for (auto& bullet : bullets_) if (bullet) bullet->ReleaseTrail();
    bullets_.clear();
    growthStats_ = {};
	specialImpacts_.clear();
    pendingBuildShots_.clear();buildEvents_.clear();marks_={};shooterStats_={};
    combatPlayer_=nullptr;combatBoss_=nullptr;combatEnemies_=nullptr;combatStage_=nullptr;
    if (trailManager_) trailManager_->ClearInstances();
}

void BulletManager::Update(Stage& stage, float deltaTime) {
    combatStage_=&stage;
    marks_.Update(deltaTime);
    for (auto& bullet : bullets_) {
        if(combatPlayer_&&bullet->IsBoomerang())bullet->SetReturnTarget(combatPlayer_->GetWorldPosition());
        const bool returning=bullet->GetIsReturning();
        bullet->Update(deltaTime);
        if(!returning&&bullet->GetIsReturning()) {
            ++shooterStats_.returns;
            BuildEventAt(BuildEventKind::Return,bullet->GetWorldPosition(),bullet->GetWorldPosition());
        }
    }
    if (trailManager_) {
        trailManager_->Update(deltaTime);
    }

    // 弾とブロックの当たり判定
    stage.ResolveBulletsCollision(GetBulletPtrs());

    FlushPendingSplits();

    bullets_.erase(
        std::remove_if(
            bullets_.begin(),
            bullets_.end(),
            [](const std::unique_ptr<Bullet>& bullet) {
                if (bullet->IsDead()) {
                    bullet->ReleaseTrail();
                }
                return bullet->IsDead();
            }
        ),
        bullets_.end()
    );
}

void BulletManager::Draw() {
    for (auto& bullet : bullets_) {
        bullet->Draw();
    }
}

void BulletManager::DrawTrails(const Matrix4x4& viewProjection) {
    if (trailManager_) {
        trailManager_->DrawAll(viewProjection);
    }
}

std::vector<Bullet*> BulletManager::GetBulletPtrs() const {
    std::vector<Bullet*> result;
    result.reserve(bullets_.size());
    for (const auto& b : bullets_) {
        result.push_back(b.get());
    }
    return result;
}

BulletManager::BulletCounts BulletManager::GetBulletCounts() const {
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

size_t BulletManager::GetTrailInstanceCount() const {
    return trailManager_ ? trailManager_->GetInstanceCount() : 0;
}

void BulletManager::BuildEventAt(BuildEventKind kind,const Vector3& start,const Vector3& end,float strength)
{
    if(buildEvents_.size()<64)buildEvents_.push_back({kind,start,end,strength});
}

std::vector<Collider*> BulletManager::LiveCombatTargets() const
{
    std::vector<Collider*> targets;
    if(combatBoss_&&!combatBoss_->IsDead()&&combatBoss_->GetHp()>0)targets.push_back(combatBoss_);
    if(combatEnemies_)for(auto* target:combatEnemies_->GetEnemyPtrs())
        if(target&&!target->IsDead()&&!target->IsRunResource())targets.push_back(target);
    return targets;
}

bool BulletManager::HasClearLink(const Vector3& from,const Vector3& to) const
{
    if(!combatStage_)return true;
    const float length=Length(to-from);
    const int steps=(std::clamp)(static_cast<int>(std::ceil(length/.45f)),1,128);
    for(int i=1;i<steps;++i)if(combatStage_->IsCollisionWithAnyBlock(from+(to-from)*(static_cast<float>(i)/steps),.08f))return false;
    return true;
}

bool BulletManager::DamageBuildTarget(Collider* target,uint32_t damage,const Vector3& source)
{
    if(auto* enemy=dynamic_cast<ExpEnemy*>(target)) {
        if(enemy->IsDead()||enemy->IsRunResource())return false;
        return enemy->TakeDirectionalDamage(damage,source);
    }
    if(auto* boss=dynamic_cast<Enemy*>(target)) {
        const int hp=boss->GetHp();boss->TakeDamage(damage);return hp>0&&boss->GetHp()<=0;
    }
    return false;
}

void BulletManager::QueueKillBurst(const Bullet& bullet,const Vector3& position)
{
    if(!bullet.GetShooterAbilities().killBurst||bullet.IsBurstChild()||pendingBuildShots_.size()>42)return;
    ++shooterStats_.burstTriggers;
    const auto damage=tankshooter::Damage(bullet.GetDamage(),.35f,bullet.GetShooterAbilities().burstPower);
    for(int i=0;i<tankshooter::kBurstChildren;++i) {
        const float angle=static_cast<float>(i)*6.2831853f/tankshooter::kBurstChildren;
        const Vector3 direction{std::cos(angle),std::sin(angle),0};
        pendingBuildShots_.push_back({position+direction*.35f,direction*.30f,damage,false});
    }
    BuildEventAt(BuildEventKind::Burst,position,position);
}

void BulletManager::NotifyPlayerHit(Bullet& bullet,Collider& target,bool killed)
{
    if(bullet.GetOwner()!=kPlayer||bullet.IsBurstChild()||bullet.GetSourceDroneIndex()>=0)return;
    if(auto* ordinary=dynamic_cast<ExpEnemy*>(&target);ordinary&&ordinary->IsRunResource())return;
    const auto& ability=bullet.GetShooterAbilities();
    if(killed)QueueKillBurst(bullet,target.GetWorldPosition());
    if(ability.chain) {
        Vector3 from=target.GetWorldPosition();
        std::array<uint64_t,3> visited{target.GetCollisionId(),0,0};
        for(int hop=0;hop<tankshooter::kChainTargets;++hop) {
            Collider* nearest=nullptr;float distance=tankshooter::kChainRadius;
            for(auto* candidate:LiveCombatTargets()) {
                if(std::find(visited.begin(),visited.end(),candidate->GetCollisionId())!=visited.end())continue;
                const float d=Length(candidate->GetWorldPosition()-from);
                if(d<distance&&HasClearLink(from,candidate->GetWorldPosition())){nearest=candidate;distance=d;}
            }
            if(!nearest)break;
            visited[hop+1]=nearest->GetCollisionId();
            const auto to=nearest->GetWorldPosition();
            const bool chainKill=DamageBuildTarget(nearest,tankshooter::Damage(bullet.GetDamage(),.48f,ability.chainPower),from);
            ++shooterStats_.chainHits;BuildEventAt(BuildEventKind::Chain,from,to);
            if(chainKill)QueueKillBurst(bullet,to);
            from=to;
        }
    }
    if(ability.mark&&!killed&&marks_.Hit(target.GetCollisionId(),dynamic_cast<Enemy*>(&target)!=nullptr)) {
        const auto center=target.GetWorldPosition();
        ++shooterStats_.detonations;BuildEventAt(BuildEventKind::Detonation,center,center,2.8f);
        for(auto* candidate:LiveCombatTargets()) {
            if(Length(candidate->GetWorldPosition()-center)>2.8f+candidate->GetRadius()||!HasClearLink(center,candidate->GetWorldPosition()))continue;
            const bool boss=dynamic_cast<Enemy*>(candidate)!=nullptr;
            const float scale=1.75f*(candidate==&target?1.0f:.50f)*(boss?.65f:1.0f);
            if(DamageBuildTarget(candidate,tankshooter::Damage(bullet.GetDamage(),scale,ability.markPower),center))QueueKillBurst(bullet,candidate->GetWorldPosition());
        }
    }
}

std::vector<BulletManager::MarkVisual> BulletManager::GetMarkVisuals() const
{
    std::vector<MarkVisual> out;
    if(std::none_of(marks_.entries.begin(),marks_.entries.end(),[](const auto& m){return m.id!=0;}))return out;
    for(auto* target:LiveCombatTargets())for(const auto& mark:marks_.entries)
        if(mark.id==target->GetCollisionId()&&mark.remaining>0)out.push_back({target->GetWorldPosition(),mark.stacks});
    return out;
}

void BulletManager::QueueArmorReflection(const Bullet& bullet,const Vector3& targetPosition)
{
    if(pendingBuildShots_.size()>=48||bullet.WasArmorReflected())return;
    const auto incoming=bullet.GetWorldPosition()-bullet.GetPreviousWorldPosition();
    const auto velocity=Length(incoming)>.0001f?Normalize(incoming)*-Length(bullet.GetMove()):bullet.GetMove()*-1.0f;
    if(Length(velocity)<.0001f)return;
    pendingBuildShots_.push_back({targetPosition+Normalize(velocity)*1.7f,velocity*.85f,tankshooter::Damage(bullet.GetDamage(),.45f),true});
    ++shooterStats_.reflections;BuildEventAt(BuildEventKind::Reflection,targetPosition,targetPosition);
}
