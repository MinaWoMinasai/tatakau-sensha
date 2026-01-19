#include "BulletManager.h"
#include "Bullet.h"
#include <algorithm>

void BulletManager::Add(std::unique_ptr<Bullet> bullet) {
    bullets_.push_back(std::move(bullet));
}

void BulletManager::Update() {

    for (auto& bullet : bullets_) {
        bullet->Update();
    }

    bullets_.erase(
        std::remove_if(
            bullets_.begin(),
            bullets_.end(),
            [](const std::unique_ptr<Bullet>& bullet) {
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

std::vector<Bullet*> BulletManager::GetBulletPtrs() const {
    std::vector<Bullet*> result;
    result.reserve(bullets_.size());
    for (const auto& b : bullets_) {
        result.push_back(b.get());
    }
    return result;
}
