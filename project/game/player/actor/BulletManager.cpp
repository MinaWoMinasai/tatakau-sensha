#include "BulletManager.h"
#include "Bullet.h"
#include <algorithm>
#include "Stage.h"

void BulletManager::Initialize(DirectXCommon* dxCommon, Object3dCommon* object3dCommon) {
    trailManager_ = std::make_unique<TrailManager>();
    trailManager_->Initialize(dxCommon, object3dCommon, "resources/white512x512.png");
}

void BulletManager::Add(std::unique_ptr<Bullet> bullet) {
    if (trailManager_) {
        bullet->AttachTrail(trailManager_.get(), &trailSettings_);
    }
    bullets_.push_back(std::move(bullet));
}

void BulletManager::ClearAll()
{
    // Release each pointer before destroying the trail instances it refers to.
    for (auto& bullet : bullets_) if (bullet) bullet->ReleaseTrail();
    bullets_.clear();
    if (trailManager_) trailManager_->ClearInstances();
}

void BulletManager::Update(Stage& stage, float deltaTime) {

    for (auto& bullet : bullets_) {
        bullet->Update(deltaTime);
    }
    if (trailManager_) {
        trailManager_->Update(deltaTime);
    }

    // 弾とブロックの当たり判定
    stage.ResolveBulletsCollision(GetBulletPtrs());

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
