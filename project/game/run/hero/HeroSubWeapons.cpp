#include "game/enemy/actor/Enemy.h"
#include "game/render/neon/NeonGridRenderer.h"
#include "game/run/hero/HeroAbilityCore.h"
#include "game/session/GameWorld.h"
#include "game/session/GameplaySystems.h"
#include <algorithm>
#include <cmath>

// 副武器（計画書 p7 の E4、p8 の F3）：レベルアップの3択で手に入り、勝手に動く武器。段は Lv1〜5 で、Lv5 で覚醒する。
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
constexpr std::array<OrbitTier, 5> kOrbit{{{2, 3.0f, 1.2f}, {2, 3.0f, 1.5f}, {3, 3.0f, 1.6f}, {3, 3.4f, 1.9f}, {4, 4.0f, 2.0f}}};
constexpr float kOrbitSpeed = 2.6f;
constexpr float kOrbitTick = 0.3f;
constexpr float kOrbitBall = 0.65f;

struct TurretTier {
    float interval;
    float damage;
    int targets;
    int pierce;
};
constexpr std::array<TurretTier, 5> kTurret{{{1.0f, 1.0f, 1, 0}, {0.8f, 1.15f, 1, 0}, {0.65f, 1.3f, 1, 0}, {0.65f, 1.3f, 2, 0}, {0.55f, 1.4f, 3, 1}}};
constexpr float kTurretRange = 15.0f;

struct MineTier {
    float interval;
    float damage;
    float radius;
};
constexpr std::array<MineTier, 5> kMine{{{2.5f, 2.5f, 2.4f}, {2.1f, 3.0f, 2.5f}, {1.7f, 3.5f, 2.6f}, {1.5f, 3.8f, 2.9f}, {1.5f, 4.0f, 3.4f}}};
constexpr size_t kMineMax = 6;
constexpr float kMineArm = 0.6f;
constexpr float kMineTrigger = 1.4f;
const myEngine::Vector4 kMineColor{2.2f, 0.9f, 0.25f, 1.0f};

// 迫撃砲（計画書 p8 の F3）：間隔・一度に落ちる数・威力（主砲1発に対する倍率）・爆発の半径。
struct MortarTier {
    float interval;
    int shells;
    float damage;
    float radius;
};
constexpr std::array<MortarTier, 5> kMortar{{{3.0f, 1, 3.0f, 2.6f}, {3.0f, 1, 3.6f, 2.9f}, {2.6f, 2, 3.6f, 2.9f}, {2.2f, 2, 4.2f, 3.0f}, {2.0f, 3, 4.5f, 3.7f}}};
constexpr float kMortarRange = 16.0f;
constexpr float kMortarFall = 0.7f;
const myEngine::Vector4 kMortarColor{2.3f, 0.5f, 0.3f, 1.0f};

// 随伴機：数・撃つ間隔・威力・貫く数。
struct DroneTier {
    int count;
    float interval;
    float damage;
    int pierce;
};
constexpr std::array<DroneTier, 5> kDrone{{{1, 0.9f, 0.9f, 0}, {1, 0.7f, 1.0f, 0}, {2, 0.7f, 1.0f, 0}, {2, 0.55f, 1.2f, 1}, {3, 0.5f, 1.3f, 2}}};
constexpr float kDroneRange = 13.0f;
constexpr float kDroneRing = 2.2f;
const myEngine::Vector4 kDroneColor{2.2f, 1.9f, 0.4f, 1.0f};

// 火炎放射：間隔（0 なら出し続ける）・出す秒・届く長さ・扇の半分の角度（度）・0.25秒ごとの威力・燃える毎秒の威力。
struct FlameTier {
    float interval;
    float duration;
    float range;
    float halfAngle;
    float damage;
    float burn;
};
constexpr std::array<FlameTier, 5> kFlame{{{3.0f, 1.2f, 4.5f, 25.0f, 0.5f, 0.4f},
                                           {2.8f, 1.6f, 5.0f, 25.0f, 0.6f, 0.45f},
                                           {2.6f, 1.6f, 5.0f, 32.0f, 0.7f, 0.6f},
                                           {2.0f, 1.6f, 6.0f, 32.0f, 0.8f, 0.7f},
                                           {0.0f, 0.0f, 6.5f, 42.0f, 0.9f, 0.9f}}};
constexpr float kFlameTick = 0.25f;
constexpr float kFlameBurnSeconds = 2.0f;

// 衝撃の輪：間隔・半径・威力・押す強さ。
struct PulseTier {
    float interval;
    float radius;
    float damage;
    float push;
};
constexpr std::array<PulseTier, 5> kPulse{{{4.0f, 3.5f, 1.5f, 6.0f}, {3.5f, 3.9f, 1.8f, 7.0f}, {3.0f, 3.9f, 2.2f, 8.0f}, {2.6f, 4.5f, 2.4f, 9.0f}, {2.2f, 5.3f, 2.8f, 10.0f}}};

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
                if (tier >= survivor::kMaxSubWeaponTier) {
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
        if (tier >= survivor::kMaxSubWeaponTier) {
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
            const float mineRadius = t.radius * HeroAreaScale();
            player.TriggerBlastAt(it->position, mineRadius, t.damage * HeroPowerScale(), kMineColor, 0.0f);
            ++mineBlasts_;
            // 覚醒：まわりに小さな爆発が3つ続く。
            if (tier >= survivor::kMaxSubWeaponTier) {
                for (int i = 0; i < 3; ++i) {
                    const float a = orbitAngle_ + 6.2831853f * static_cast<float>(i) / 3.0f;
                    const myEngine::Vector3 offset{std::cos(a) * mineRadius * 0.9f, std::sin(a) * mineRadius * 0.9f, 0.0f};
                    player.TriggerBlastAt(it->position + offset, mineRadius * 0.6f, t.damage * 0.5f * HeroPowerScale(), kMineColor, 0.15f + 0.05f * static_cast<float>(i));
                }
            }
            it = mines_.erase(it);
        }
    }
    UpdateNewSubWeapons(dt);
}

void HeroAbilityCore::UpdateNewSubWeapons(float dt)
{
    auto& player = *heroWorld_.resources.player_;
    const myEngine::Vector3 self = player.GetWorldPosition();
    const auto enemies = heroWorld_.resources.enemyManager_->GetEnemyPtrs();
    auto alive = [](const ExpEnemy* enemy) {
        return enemy && !enemy->IsDead() && !enemy->IsRunResource();
    };

    // 迫撃砲：間隔ごとに、届く範囲の敵から何体かを見て、まわりに敵が一番多い所へ落とす（落ちる所に印が出て、少し後に爆発）。
    if (const int tier = SubWeaponTier(SubWeapon::Mortar); tier > 0) {
        const MortarTier& t = kMortar[static_cast<size_t>(tier - 1)];
        mortarTimer_ -= dt;
        if (mortarTimer_ <= 0.0f) {
            std::vector<myEngine::Vector3> candidates;
            for (const ExpEnemy* enemy : enemies) {
                if (alive(enemy) && Planar(self, enemy->GetWorldPosition()) <= kMortarRange) {
                    candidates.push_back(enemy->GetWorldPosition());
                }
            }
            if (HeroBossTargetActive() && Planar(self, heroWorld_.resources.enemy_->GetWorldPosition()) <= kMortarRange) {
                candidates.push_back(heroWorld_.resources.enemy_->GetWorldPosition());
            }
            if (candidates.empty()) {
                mortarTimer_ = 0.2f;
            } else {
                mortarTimer_ += t.interval;
                mortarTimer_ = (std::max)(mortarTimer_, 0.1f);
                for (int shell = 0; shell < t.shells; ++shell) {
                    // 何体かを選び、まわりの敵の数で比べる（同じ所に落ちないよう、もう印のある所の近くは避ける）。
                    myEngine::Vector3 best = candidates[0];
                    int bestCount = -1;
                    for (int sample = 0; sample < 10; ++sample) {
                        burstRandom_ ^= burstRandom_ << 13;
                        burstRandom_ ^= burstRandom_ >> 17;
                        burstRandom_ ^= burstRandom_ << 5;
                        const myEngine::Vector3& point = candidates[burstRandom_ % candidates.size()];
                        bool taken = false;
                        for (const MortarShell& other : mortarShells_) {
                            taken = taken || Planar(other.position, point) < t.radius;
                        }
                        int count = taken ? -1000 : 0;
                        for (const myEngine::Vector3& other : candidates) {
                            count += Planar(point, other) <= t.radius ? 1 : 0;
                        }
                        if (count > bestCount) {
                            bestCount = count;
                            best = point;
                        }
                    }
                    mortarShells_.push_back({best, kMortarFall + 0.12f * static_cast<float>(shell), t.radius * HeroAreaScale(), t.damage});
                }
            }
        }
    }
    for (auto it = mortarShells_.begin(); it != mortarShells_.end();) {
        it->timer -= dt;
        if (it->timer > 0.0f) {
            ++it;
            continue;
        }
        player.TriggerBlastAt(it->position, it->radius, it->damage * HeroPowerScale(), kMortarColor, 0.0f);
        ++mortarBlasts_;
        it = mortarShells_.erase(it);
    }

    // 随伴機：自機のまわりの輪の上をゆっくり回る位置へ、遅れて付いていく。間隔ごとに、それぞれが近い敵へ1発撃つ。
    if (const int tier = SubWeaponTier(SubWeapon::Drone); tier > 0) {
        const DroneTier& t = kDrone[static_cast<size_t>(tier - 1)];
        droneAngle_ = std::fmod(droneAngle_ + 0.9f * dt, 6.2831853f);
        const float follow = 1.0f - std::exp(-6.0f * dt);
        for (int i = 0; i < t.count; ++i) {
            const float a = droneAngle_ + 6.2831853f * static_cast<float>(i) / static_cast<float>(t.count);
            const myEngine::Vector3 goal = self + myEngine::Vector3{std::cos(a) * kDroneRing, std::sin(a) * kDroneRing, 0.0f};
            myEngine::Vector3& position = dronePositions_[static_cast<size_t>(i)];
            position = dronesPlaced_ ? position + (goal - position) * follow : goal;
        }
        dronesPlaced_ = true;
        droneTimer_ -= dt;
        if (droneTimer_ <= 0.0f) {
            droneTimer_ += t.interval;
            droneTimer_ = (std::max)(droneTimer_, 0.05f);
            for (int i = 0; i < t.count; ++i) {
                const myEngine::Vector3 from = dronePositions_[static_cast<size_t>(i)];
                float nearest = kDroneRange;
                myEngine::Vector3 target{};
                bool found = false;
                for (const ExpEnemy* enemy : enemies) {
                    if (!alive(enemy)) {
                        continue;
                    }
                    const float distance = Planar(from, enemy->GetWorldPosition());
                    if (distance < nearest) {
                        nearest = distance;
                        target = enemy->GetWorldPosition();
                        found = true;
                    }
                }
                if (HeroBossTargetActive() && Planar(from, heroWorld_.resources.enemy_->GetWorldPosition()) < nearest) {
                    target = heroWorld_.resources.enemy_->GetWorldPosition();
                    found = true;
                }
                if (!found) {
                    continue;
                }
                Player::SurvivorShot shot;
                shot.direction = myEngine::Vector3{target.x - from.x, target.y - from.y, 0.0f};
                shot.damageScale = t.damage * HeroPowerScale();
                shot.speedScale = 1.5f;
                shot.size = 0.6f;
                shot.pierce = t.pierce;
                shot.color = kDroneColor;
                shot.useOrigin = true;
                shot.origin = from;
                if (player.FireSurvivorShot(shot)) {
                    ++droneShots_;
                }
            }
        }
    } else {
        dronesPlaced_ = false;
    }

    // 火炎放射：間隔ごとに決まった秒だけ、向いている方の扇に入った敵を削って燃やす（覚醒では出し続ける）。
    if (const int tier = SubWeaponTier(SubWeapon::Flame); tier > 0) {
        const FlameTier& t = kFlame[static_cast<size_t>(tier - 1)];
        const bool always = t.interval <= 0.0f;
        if (!always && flameTimer_ <= 0.0f) {
            flameWait_ -= dt;
            if (flameWait_ <= 0.0f) {
                flameWait_ += t.interval;
                flameTimer_ = t.duration;
            }
        }
        const bool on = always || flameTimer_ > 0.0f;
        flameTimer_ = (std::max)(0.0f, flameTimer_ - dt);
        if (on) {
            flameAge_ += dt;
            flameDirection_ = myEngine::NormalizeOr(player.GetDirection(), flameDirection_, 0.001f);
            flameTick_ -= dt;
            if (flameTick_ <= 0.0f) {
                flameTick_ += kFlameTick;
                const float cosHalf = std::cos(t.halfAngle * 3.14159265f / 180.0f);
                const auto amount = static_cast<uint32_t>((std::max)(1.0f, std::round(HeroDamageUnit(t.damage))));
                const float burn = HeroDamageUnit(t.burn);
                auto inside = [&](const myEngine::Vector3& position, float radius) {
                    const myEngine::Vector3 to{position.x - self.x, position.y - self.y, 0.0f};
                    const float distance = myEngine::Length(to);
                    if (distance > t.range * HeroAreaScale() + radius) {
                        return false;
                    }
                    if (distance < radius + 0.6f) {
                        return true;
                    }
                    return (to.x * flameDirection_.x + to.y * flameDirection_.y) / distance >= cosHalf;
                };
                for (ExpEnemy* enemy : enemies) {
                    if (alive(enemy) && inside(enemy->GetWorldPosition(), enemy->GetRadius())) {
                        flameDamage_ += DealHeroDamage(enemy, false, amount, self);
                        if (!enemy->IsDead()) {
                            enemy->ApplyBurn(burn, kFlameBurnSeconds);
                        }
                    }
                }
                if (HeroBossTargetActive() && inside(heroWorld_.resources.enemy_->GetWorldPosition(), heroWorld_.resources.enemy_->GetRadius())) {
                    flameDamage_ += DealHeroDamage(heroWorld_.resources.enemy_.get(), true, amount, self);
                }
            }
        } else {
            flameTick_ = 0.0f;
        }
    }

    // 衝撃の輪：間隔ごとに、自機のまわりを押し返して削る（覚醒では敵の弾も消す）。
    if (const int tier = SubWeaponTier(SubWeapon::Pulse); tier > 0) {
        const PulseTier& t = kPulse[static_cast<size_t>(tier - 1)];
        pulseTimer_ -= dt;
        if (pulseTimer_ <= 0.0f) {
            pulseTimer_ += t.interval;
            pulseTimer_ = (std::max)(pulseTimer_, 0.1f);
            TriggerShockwave(self, t.radius, HeroDamageUnit(t.damage), t.push, tier >= survivor::kMaxSubWeaponTier);
            ++pulses_;
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
    // 反応装甲（計画書 p8 の F4）：構えている間は自機のまわりに水色の六角、防いだ瞬間は白く広がる。
    {
        const myEngine::Vector3 self = heroWorld_.resources.player_->GetWorldPosition();
        if (heroWorld_.resources.player_->IsSurvivorGuardReady()) {
            circle(self, 1.35f, 0.07f, {0.6f, 1.8f, 2.4f, 0.75f}, orbitAngle_ * 0.5f, 6);
        }
        if (guardFlash_ > 0.0f) {
            const float t = 1.0f - guardFlash_ / 0.35f;
            circle(self, 1.35f + 1.2f * t, 0.12f, {2.4f, 2.4f, 2.4f, 0.9f * (1.0f - t)}, 0.0f, 6);
        }
    }
    // 衛星：水色の球（輪を2つ重ねる）。覚醒では少し明るい。
    std::array<myEngine::Vector3, 4> balls{};
    const size_t count = OrbitPositions(balls);
    const float glow = SubWeaponTier(SubWeapon::Orbit) >= 3 ? 1.3f : 1.0f;
    for (size_t i = 0; i < count; ++i) {
        circle(balls[i], kOrbitBall, 0.16f, {0.5f * glow, 1.6f * glow, 2.2f * glow, 0.95f}, orbitAngle_ * 3.0f, 12);
        circle(balls[i], kOrbitBall * 0.45f, 0.2f, {1.2f, 2.0f, 2.4f, 0.9f}, -orbitAngle_ * 4.0f, 8);
    }
    // 迫撃砲：落ちる所に、赤い輪が縮みながら点滅する。
    for (const MortarShell& shell : mortarShells_) {
        const float t = (std::clamp)(shell.timer / kMortarFall, 0.0f, 1.0f);
        const float blink = 0.55f + 0.45f * std::sin(shell.timer * 40.0f);
        circle(shell.position, shell.radius * (0.35f + 0.65f * t), 0.12f, {kMortarColor.x, kMortarColor.y, kMortarColor.z, 0.85f * blink}, 0.0f, 20);
        circle(shell.position, shell.radius, 0.06f, {kMortarColor.x, kMortarColor.y, kMortarColor.z, 0.4f}, 0.0f, 20);
    }
    // 随伴機：黄色の小さな三角（回りながら）。
    if (const int tier = SubWeaponTier(SubWeapon::Drone); tier > 0 && dronesPlaced_) {
        for (int i = 0; i < kDrone[static_cast<size_t>(tier - 1)].count; ++i) {
            const myEngine::Vector3& p = dronePositions_[static_cast<size_t>(i)];
            circle(p, 0.42f, 0.14f, kDroneColor, droneAngle_ * 4.0f, 3);
            circle(p, 0.16f, 0.16f, {2.4f, 2.2f, 1.2f, 0.9f}, 0.0f, 6);
        }
    }
    // 火炎放射：扇の中に、ゆらめく橙と黄の炎の筋を何本も出す。
    if (const int tier = SubWeaponTier(SubWeapon::Flame); tier > 0) {
        const FlameTier& t = kFlame[static_cast<size_t>(tier - 1)];
        if (t.interval <= 0.0f || flameTimer_ > 0.0f) {
            const myEngine::Vector3 self = heroWorld_.resources.player_->GetWorldPosition();
            const float base = std::atan2(flameDirection_.y, flameDirection_.x);
            const float half = t.halfAngle * 3.14159265f / 180.0f;
            for (int n = 0; n < 14; ++n) {
                // 時間と筋の番号から、ゆらぎを作る。
                const float wobble = std::sin(flameAge_ * 23.0f + static_cast<float>(n) * 2.7f);
                const float a = base + half * (static_cast<float>(n) / 6.5f - 1.0f) + 0.08f * wobble;
                const float length = t.range * HeroAreaScale() * (0.55f + 0.45f * std::fabs(std::sin(flameAge_ * 17.0f + static_cast<float>(n) * 1.3f)));
                const myEngine::Vector3 dir{std::cos(a), std::sin(a), 0.0f};
                const myEngine::Vector3 start = self + dir * 0.9f;
                const bool core = n % 3 == 1;
                const myEngine::Vector4 color = core ? myEngine::Vector4{2.4f, 1.9f, 0.5f, 0.85f} : myEngine::Vector4{2.4f, 0.75f, 0.15f, 0.7f};
                renderer.QueueLine(start + myEngine::Vector3{0.0f, 0.0f, -0.3f}, self + dir * length + myEngine::Vector3{0.0f, 0.0f, -0.3f},
                                   core ? 0.16f : 0.22f, color);
            }
        }
    }
    // 地雷：橙の小さな四角。動き出したら点滅する。
    for (const SubMine& mine : mines_) {
        const bool armed = mine.age >= kMineArm;
        const float blink = armed ? 0.55f + 0.45f * std::sin(mine.age * 10.0f) : 0.35f;
        circle(mine.position, 0.45f, 0.14f, {kMineColor.x, kMineColor.y, kMineColor.z, blink}, 0.785398f, 4);
    }
}

} // namespace gameplay
