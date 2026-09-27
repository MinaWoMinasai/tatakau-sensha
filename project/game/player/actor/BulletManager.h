#pragma once
#include <vector>
#include <memory>
#include "TrailManager.h"
#include "Bullet.h"

class Stage;
class Enemy;
class EnemyManager;

class BulletManager {
public:
    enum class BuildEventKind {Chain,Detonation,Burst,Return,Reflection};
    struct BuildEvent {BuildEventKind kind;Vector3 start{},end{};float strength=1;};
    struct MarkVisual {Vector3 position;int stacks=0;};
    struct ShooterStats {uint64_t chainHits=0,detonations=0,burstTriggers=0,burstChildren=0,returns=0,reflections=0;};
    const ShooterStats& GetShooterStats() const {return shooterStats_;}
    std::vector<BuildEvent> ConsumeBuildEvents() {auto out=std::move(buildEvents_);buildEvents_.clear();return out;}
    std::vector<MarkVisual> GetMarkVisuals() const;
    void SetCombatContext(Player* player,Enemy* boss,EnemyManager* enemies) {combatPlayer_=player;combatBoss_=boss;combatEnemies_=enemies;}
    void NotifyPlayerHit(Bullet& bullet,Collider& target,bool killed);
    void QueueArmorReflection(const Bullet& bullet,const Vector3& targetPosition);
    struct BulletCounts {
        size_t player = 0;
        size_t enemy = 0;
        size_t hostileExpEnemy = 0;
    };
    struct GrowthStats {
        uint64_t wallBounces = 0;
        uint64_t actorPierces = 0;
        uint64_t impactSplits = 0;
        uint64_t splitChildrenSpawned = 0;
    };
    // Actual player-owned growth events since ClearAll (enemy shots excluded).
    const GrowthStats& GetGrowthStats() const { return growthStats_; }
	std::vector<Bullet::SpecialImpact> ConsumeSpecialImpacts() {auto events=std::move(specialImpacts_);specialImpacts_.clear();return events;}

    void Initialize(DirectXCommon* dxCommon, Object3dCommon* object3dCommon);
    void Add(std::unique_ptr<Bullet> bullet);
    static constexpr size_t kMaxRunProjectilesPerOwner = 240;
    // Called after a complete collision pass, never from an individual callback.
    void FlushPendingSplits();
    // Call between frames, before moving actors or replacing the stage.
    void ClearAll();

    void Update(Stage& stage, float deltaTime);
    void Draw();
    void DrawTrails(const Matrix4x4& viewProjection);

    // 弾のゲッター
    std::vector<Bullet*> GetBulletPtrs() const;
    size_t GetBulletCount() const { return bullets_.size(); }
    BulletCounts GetBulletCounts() const;
    BulletTrailSettings& GetTrailSettings() { return trailSettings_; }
    size_t GetTrailInstanceCount() const;
    bool HasDrawableTrails() const {
        return trailManager_ && trailManager_->HasDrawableInstances();
    }
    TrailManager::DrawStats GetTrailDrawStats() const {
        return trailManager_ ? trailManager_->GetDrawStats() : TrailManager::DrawStats{};
    }

private:
    struct PendingShot {Vector3 position{},velocity{};uint32_t damage=1;bool reflection=false;};
    std::vector<PendingShot> pendingBuildShots_;
    std::vector<BuildEvent> buildEvents_;
    tankshooter::MarkLedger marks_{};
    ShooterStats shooterStats_{};
    Player* combatPlayer_=nullptr;
    Enemy* combatBoss_=nullptr;
    EnemyManager* combatEnemies_=nullptr;
    Stage* combatStage_=nullptr;
    std::vector<Collider*> LiveCombatTargets() const;
    bool HasClearLink(const Vector3& from,const Vector3& to) const;
    bool DamageBuildTarget(Collider* target,uint32_t damage,const Vector3& source);
    void QueueKillBurst(const Bullet& bullet,const Vector3& position);
    void BuildEventAt(BuildEventKind kind,const Vector3& start,const Vector3& end,float strength=1);
    std::vector<std::unique_ptr<Bullet>> bullets_;
    std::unique_ptr<TrailManager> trailManager_;
    BulletTrailSettings trailSettings_;
    GrowthStats growthStats_{};
	std::vector<Bullet::SpecialImpact> specialImpacts_;
};
