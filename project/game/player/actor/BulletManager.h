#pragma once
#include <vector>
#include <memory>
#include "TrailManager.h"
#include "Bullet.h"

class Stage;

class BulletManager {
public:
    void Initialize(DirectXCommon* dxCommon, Object3dCommon* object3dCommon);
    void Add(std::unique_ptr<Bullet> bullet);

    void Update(Stage& stage, float deltaTime);
    void Draw();
    void DrawTrails(const Matrix4x4& viewProjection);

    // 弾のゲッター
    std::vector<Bullet*> GetBulletPtrs() const;
    BulletTrailSettings& GetTrailSettings() { return trailSettings_; }
    size_t GetTrailInstanceCount() const;

private:
    std::vector<std::unique_ptr<Bullet>> bullets_;
    std::unique_ptr<TrailManager> trailManager_;
    BulletTrailSettings trailSettings_;
};
