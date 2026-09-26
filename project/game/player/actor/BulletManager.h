#pragma once
#include <vector>
#include <memory>
#include "TrailManager.h"
#include "Bullet.h"

class Stage;

class BulletManager {
public:
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
    std::vector<std::unique_ptr<Bullet>> bullets_;
    std::unique_ptr<TrailManager> trailManager_;
    BulletTrailSettings trailSettings_;
    GrowthStats growthStats_{};
	std::vector<Bullet::SpecialImpact> specialImpacts_;
};
