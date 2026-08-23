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

    void Initialize(DirectXCommon* dxCommon, Object3dCommon* object3dCommon);
    void Add(std::unique_ptr<Bullet> bullet);

    void Update(Stage& stage, float deltaTime);
    void Draw();
    void DrawTrails(const Matrix4x4& viewProjection);

    // 弾のゲッター
    std::vector<Bullet*> GetBulletPtrs() const;
    size_t GetBulletCount() const { return bullets_.size(); }
    BulletCounts GetBulletCounts() const;
    BulletTrailSettings& GetTrailSettings() { return trailSettings_; }
    size_t GetTrailInstanceCount() const;
#if defined(USE_IMGUI) && !defined(NDEBUG)
    TrailManager::DrawStats GetTrailDrawStats() const {
        return trailManager_ ? trailManager_->GetDrawStats() : TrailManager::DrawStats{};
    }
#endif

private:
    std::vector<std::unique_ptr<Bullet>> bullets_;
    std::unique_ptr<TrailManager> trailManager_;
    BulletTrailSettings trailSettings_;
};
