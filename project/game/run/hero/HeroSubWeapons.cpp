#include "game/enemy/actor/Enemy.h"
#include "game/render/neon/NeonGridRenderer.h"
#include "game/run/hero/HeroAbilityCore.h"
#include "game/session/GameWorld.h"
#include "game/session/GameplaySystems.h"
#include <algorithm>
#include <cmath>

// 副武器（計画書 p7 の E4）：レベルアップの3択で手に入り、勝手に動く武器。段は1〜3で、3段目で覚醒する。
// 威力は主砲1発（HeroDamageUnit）に対する倍率。数値は遊んで調整する。

namespace gameplay {

using survivor::SubWeapon;

namespace {
/// @brief 段ごとの数値（[段-1]）。
struct OrbitTier {
    int count;
    float ring;
    float damage;
};
// 触れないと当たらないので、1回の威力は砲より高め。
constexpr std::array<OrbitTier, 3> kOrbit{{{2, 3.0f, 1.2f}, {3, 3.0f, 1.6f}, {4, 4.0f, 1.8f}}};
constexpr float kOrbitSpeed = 2.6f;
constexpr float kOrbitTick = 0.3f;
constexpr float kOrbitBall = 0.65f;

struct TurretTier {
    float interval;
    float damage;
    int targets;
    int pierce;
};
constexpr std::array<TurretTier, 3> kTurret{{{1.0f, 1.0f, 1, 0}, {0.65f, 1.3f, 1, 0}, {0.65f, 1.3f, 3, 1}}};
constexpr float kTurretRange = 15.0f;

struct MineTier {
    float interval;
    float damage;
    float radius;
};
constexpr std::array<MineTier, 3> kMine{{{2.5f, 2.5f, 2.4f}, {1.7f, 3.5f, 2.6f}, {1.7f, 3.5f, 3.4f}}};
constexpr size_t kMineMax = 6;
constexpr float kMineArm = 0.6f;
constexpr float kMineTrigger = 1.4f;
const myEngine::Vector4 kMineColor{2.2f, 0.9f, 0.25f, 1.0f};

float Planar(const myEngine::Vector3& a, const myEngine::Vector3& b)
{
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}
} // namespace

size_t HeroAbilityCore::OrbitPositions(std::array<myEngine::Vector3, 4>& out) const
{
    const int tier = SubWeaponTier(SubWeapon::Orbit);
    if (tier <= 0) {
        return 0;
    }
    const OrbitTier& t = kOrbit[static_cast<size_t>(tier - 1)];
    const myEngine::Vector3 self = heroWorld_.resources.player_->GetWorldPosition();
    for (int i = 0; i < t.count; ++i) {
        const float a = orbitAngle_ + 6.2831853f * static_cast<float>(i) / static_cast<float>(t.count);
        out[static_cast<size_t>(i)] = self + myEngine::Vector3{std::cos(a) * t.ring, std::sin(a) * t.ring, 0.0f};
    }
    return static_cast<size_t>(t.count);
}

void HeroAbilityCore::UpdateSubWeapons(float dt)
{
    if (dt <= 0.0f) {
        return;
    }
    auto& player = *heroWorld_.resources.player_;
    const myEngine::Vector3 self = player.GetWorldPosition();

    // まわる衛星：球は回り続け、間隔ごとに触れている敵へ当てる。覚醒（3段目）では押し返し、敵の弾も消す。
    if (const int tier = SubWeaponTier(SubWeapon::Orbit); tier > 0) {
        const OrbitTier& t = kOrbit[static_cast<size_t>(tier - 1)];
        orbitAngle_ = std::fmod(orbitAngle_ + kOrbitSpeed * dt, 6.2831853f);
        std::array<myEngine::Vector3, 4> balls{};
        const size_t count = OrbitPositions(balls);
        orbitTick_ -= dt;
        const bool tick = orbitTick_ <= 0.0f;
        if (tick) {
            orbitTick_ += kOrbitTick;
        }
        const auto amount = static_cast<uint32_t>((std::max)(1.0f, std::round(HeroDamageUnit(t.damage))));
        for (ExpEnemy* enemy : heroWorld_.resources.enemyManager_->GetEnemyPtrs()) {
            if (!tick || !enemy || enemy->IsDead() || enemy->IsRunResource()) {
                continue;
            }
            const myEngine::Vector3 position = enemy->GetWorldPosition();
            for (size_t i = 0; i < count; ++i) {
                if (Planar(balls[i], position) > kOrbitBall + enemy->GetRadius()) {
                    continue;
                }
                if (tier >= 3) {
                    enemy->ApplyKnockback(myEngine::Vector3{position.x - self.x, position.y - self.y, 0.0f}, 6.0f);
                }
                orbitDamage_ += DealHeroDamage(enemy, false, amount, balls[i]);
                break;
            }
        }
        if (tick && HeroBossTargetActive()) {
            Enemy& rival = *heroWorld_.resources.enemy_;
            for (size_t i = 0; i < count; ++i) {
                if (Planar(balls[i], rival.GetWorldPosition()) <= kOrbitBall + rival.GetRadius()) {
                    orbitDamage_ += DealHeroDamage(&rival, true, amount, balls[i]);
                    break;
                }
            }
        }
        if (tier >= 3) {
            for (Bullet* bullet : heroWorld_.resources.bulletManager_->GetBulletPtrs()) {
                if (!bullet || bullet->IsDead() || bullet->GetOwner() == kPlayer) {
                    continue;
                }
                for (size_t i = 0; i < count; ++i) {
                    if (Planar(balls[i], bullet->GetWorldPosition()) <= kOrbitBall + 0.3f) {
                        bullet->ApplyBulletDurabilityDamage(1000000.0f);
                        ++orbitBulletsCleared_;
                        break;
                    }
                }
            }
        }
    }

    // 見張りの砲：間隔ごとに、近い敵（覚醒では近い3体）へ1発ずつ撃つ。届く範囲に敵がいなければ待つ。
    if (const int tier = SubWeaponTier(SubWeapon::Turret); tier > 0) {
        const TurretTier& t = kTurret[static_cast<size_t>(tier - 1)];
        turretTimer_ -= dt;
        if (turretTimer_ <= 0.0f) {
            std::array<std::pair<float, myEngine::Vector3>, 4> targets{};
            size_t found = 0;
            auto consider = [&](const myEngine::Vector3& position) {
                const float distance = Planar(self, position);
                if (distance > kTurretRange) {
                    return;
                }
                if (found < targets.size()) {
                    targets[found++] = {distance, position};
                    return;
                }
                auto farthest = std::max_element(targets.begin(), targets.end(), [](const auto& a, const auto& b) {
                    return a.first < b.first;
                });
                if (distance < farthest->first) {
                    *farthest = {distance, position};
                }
            };
            for (const ExpEnemy* enemy : heroWorld_.resources.enemyManager_->GetEnemyPtrs()) {
                if (enemy && !enemy->IsDead() && !enemy->IsRunResource()) {
                    consider(enemy->GetWorldPosition());
                }
            }
            if (HeroBossTargetActive()) {
                consider(heroWorld_.resources.enemy_->GetWorldPosition());
            }
            if (found == 0) {
                turretTimer_ = 0.1f;
            } else {
                turretTimer_ += t.interval;
                turretTimer_ = (std::max)(turretTimer_, 0.05f);
                std::sort(targets.begin(), targets.begin() + found, [](const auto& a, const auto& b) {
                    return a.first < b.first;
                });
                for (size_t i = 0; i < (std::min)(found, static_cast<size_t>(t.targets)); ++i) {
                    Player::SurvivorShot shot;
                    shot.direction = myEngine::Vector3{targets[i].second.x - self.x, targets[i].second.y - self.y, 0.0f};
                    shot.damageScale = t.damage * HeroPowerScale();
                    shot.speedScale = 1.6f;
                    shot.size = 0.7f;
                    shot.pierce = t.pierce;
                    shot.color = {0.4f, 2.0f, 1.2f, 1.0f};
                    if (player.FireSurvivorShot(shot)) {
                        ++turretShots_;
                    }
                }
            }
        }
    }

    // 置き地雷：間隔ごとに足元へ置く（多すぎたら古い物から消す）。置いて少し経つと、敵が近づいたら爆発する。
    if (const int tier = SubWeaponTier(SubWeapon::Mine); tier > 0) {
        const MineTier& t = kMine[static_cast<size_t>(tier - 1)];
        mineTimer_ -= dt;
        if (mineTimer_ <= 0.0f) {
            mineTimer_ += t.interval;
            if (mines_.size() >= kMineMax) {
                mines_.erase(mines_.begin());
            }
            mines_.push_back({self, 0.0f});
        }
        for (auto it = mines_.begin(); it != mines_.end();) {
            it->age += dt;
            bool trigger = false;
            if (it->age >= kMineArm) {
                for (const ExpEnemy* enemy : heroWorld_.resources.enemyManager_->GetEnemyPtrs()) {
                    if (enemy && !enemy->IsDead() && !enemy->IsRunResource() &&
                        Planar(it->position, enemy->GetWorldPosition()) <= kMineTrigger + enemy->GetRadius()) {
                        trigger = true;
                        break;
                    }
                }
                if (!trigger && HeroBossTargetActive()) {
                    const Enemy& rival = *heroWorld_.resources.enemy_;
                    trigger = Planar(it->position, rival.GetWorldPosition()) <= kMineTrigger + rival.GetRadius();
                }
            }
            if (!trigger) {
                ++it;
                continue;
            }
            player.TriggerBlastAt(it->position, t.radius, t.damage, kMineColor, 0.0f);
            ++mineBlasts_;
            // 覚醒：まわりに小さな爆発が3つ続く。
            if (tier >= 3) {
                for (int i = 0; i < 3; ++i) {
                    const float a = orbitAngle_ + 6.2831853f * static_cast<float>(i) / 3.0f;
                    const myEngine::Vector3 offset{std::cos(a) * t.radius * 0.9f, std::sin(a) * t.radius * 0.9f, 0.0f};
                    player.TriggerBlastAt(it->position + offset, t.radius * 0.6f, t.damage * 0.5f, kMineColor, 0.15f + 0.05f * static_cast<float>(i));
                }
            }
            it = mines_.erase(it);
        }
    }
}

void HeroAbilityCore::QueueSubWeapons(myEngine::NeonGridRenderer& renderer) const
{
    auto circle = [&](const myEngine::Vector3& center, float radius, float width, const myEngine::Vector4& color, float phase, int sides) {
        for (int n = 0; n < sides; ++n) {
            const float a = phase + 6.2831853f * static_cast<float>(n) / static_cast<float>(sides),
                        b = phase + 6.2831853f * static_cast<float>(n + 1) / static_cast<float>(sides);
            renderer.QueueLine(center + myEngine::Vector3{std::cos(a) * radius, std::sin(a) * radius, -0.3f},
                               center + myEngine::Vector3{std::cos(b) * radius, std::sin(b) * radius, -0.3f}, width, color);
        }
    };
    // 衛星：水色の球（輪を2つ重ねる）。覚醒では少し明るい。
    std::array<myEngine::Vector3, 4> balls{};
    const size_t count = OrbitPositions(balls);
    const float glow = SubWeaponTier(SubWeapon::Orbit) >= 3 ? 1.3f : 1.0f;
    for (size_t i = 0; i < count; ++i) {
        circle(balls[i], kOrbitBall, 0.16f, {0.5f * glow, 1.6f * glow, 2.2f * glow, 0.95f}, orbitAngle_ * 3.0f, 12);
        circle(balls[i], kOrbitBall * 0.45f, 0.2f, {1.2f, 2.0f, 2.4f, 0.9f}, -orbitAngle_ * 4.0f, 8);
    }
    // 地雷：橙の小さな四角。動き出したら点滅する。
    for (const SubMine& mine : mines_) {
        const bool armed = mine.age >= kMineArm;
        const float blink = armed ? 0.55f + 0.45f * std::sin(mine.age * 10.0f) : 0.35f;
        circle(mine.position, 0.45f, 0.14f, {kMineColor.x, kMineColor.y, kMineColor.z, blink}, 0.785398f, 4);
    }
}

} // namespace gameplay
