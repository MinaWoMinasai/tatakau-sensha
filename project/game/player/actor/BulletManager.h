#pragma once
#include <vector>
#include <memory>

class Bullet;

class BulletManager {
public:
    void Add(std::unique_ptr<Bullet> bullet);

    void Update();
    void Draw();

    // 弾のゲッター
    std::vector<Bullet*> GetBulletPtrs() const;

private:
    std::vector<std::unique_ptr<Bullet>> bullets_;
};
