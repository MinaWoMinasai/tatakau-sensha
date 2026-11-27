#include "game/ui/session/SurvivorUpgradePreview.h"
#include <algorithm>
#include <cmath>

namespace gameplay {

namespace {
using Color = myEngine::Vector4;
constexpr float kTau = 6.2831853f;
constexpr Color kTankColor{0.62f, 1.0f, 0.81f, 1.0f};
constexpr Color kShotColor{0.79f, 1.0f, 0.94f, 1.0f};
constexpr Color kSubColor{0.24f, 1.0f, 0.66f, 1.0f};
constexpr Color kPerkColor{0.37f, 0.78f, 1.0f, 1.0f};
constexpr Color kGold{1.0f, 0.83f, 0.30f, 1.0f};
constexpr Color kHurt{1.0f, 0.23f, 0.36f, 1.0f};
constexpr Color kFire{1.0f, 0.55f, 0.18f, 1.0f};
const std::array<Color, 3> kEnemyColors{{{1.0f, 0.88f, 0.40f, 1.0f}, {1.0f, 0.48f, 0.66f, 1.0f}, {0.60f, 0.71f, 1.0f, 1.0f}}};
// 敵の絵の大きさ（画面の幅）と当たりの半径。
constexpr float kEnemySize = 46.0f;
constexpr float kEnemyRadius = 17.0f;

// 画面の色を、光の量で計算する描き方に合わせる（SurvivorLevelUpScreen と同じ。色が濃く出る）。
Color Linear(const Color& color)
{
    auto convert = [](float v) {
        return std::pow((std::max)(0.0f, v), 1.8f);
    };
    return {convert(color.x), convert(color.y), convert(color.z), color.w};
}
// 絵の場所（Texture の順）。
const std::string& TexturePath(size_t index)
{
    static const std::array<std::string, 13> kPaths = [] {
        static constexpr std::array<const char*, 13> kNames{"pv_square", "pv_triangle", "pv_pentagon", "pv_tank", "pv_drone", "pv_ring", "pv_dash",
                                                            "pv_hexagon", "pv_wrench",  "pv_shard",    "pv_dot",  "pv_bolt",  "pv_flame"};
        std::array<std::string, 13> paths;
        for (size_t i = 0; i < kNames.size(); ++i) {
            paths[i] = std::string("resources/endless_ui/") + kNames[i] + ".png";
        }
        return paths;
    }();
    return kPaths[index];
}
const myEngine::TextStyle& NumberStyle()
{
    static const myEngine::TextStyle kStyle = [] {
        myEngine::TextStyle style{};
        style.fontFamily = "Meiryo";
        style.fontWeight = 700;
        style.fontSize = 24.0f;
        style.color = {1.0f, 1.0f, 1.0f, 1.0f};
        style.outlineColor = {0.0f, 0.0f, 0.0f, 0.8f};
        style.outlineThickness = 2.0f;
        style.padding = 3.0f;
        style.preserveOutline = true;
        return style;
    }();
    return kStyle;
}
Color Dim(Color color, float k)
{
    return {color.x * k, color.y * k, color.z * k, 1.0f};
}
} // namespace

void SurvivorUpgradePreview::Preload(SurvivorQuadBatch& batch)
{
    for (size_t i = 0; i < kTextures; ++i) {
        batch.Preload(TexturePath(i));
    }
    for (const char c : std::string("0123456789+-!")) {
        batch.Measure(std::string(1, c), NumberStyle());
    }
    Reset("", 1, false);
}

float SurvivorUpgradePreview::Random(float a, float b)
{
    seed_ ^= seed_ << 13;
    seed_ ^= seed_ >> 17;
    seed_ ^= seed_ << 5;
    return a + (b - a) * static_cast<float>(seed_ % 10000u) / 10000.0f;
}

void SurvivorUpgradePreview::Reset(const std::string& key, int tier, bool awaken)
{
    key_ = key;
    tier_ = (std::max)(1, tier);
    awaken_ = awaken;
    seed_ = 2463534242u;
    for (const char c : key) {
        seed_ = seed_ * 31u + static_cast<uint32_t>(c);
    }
    seed_ |= 1u;
    time_ = shotTimer_ = sideTimer_ = haste_ = gauge_ = flash_ = recoil_ = aim_ = 0.0f;
    actionTimer_ = 0.5f;
    hp_ = 1.0f;
    burstLeft_ = stacks_ = phase_ = 0;
    tankX_ = area_.x + 120.0f;
    tankY_ = area_.y + area_.height * 0.5f;
    enemies_.clear();
    bullets_.clear();
    effects_.clear();
    numbers_.clear();
    for (auto& drone : drones_) {
        drone = {tankX_ - 60.0f, tankY_ - 60.0f};
    }
    // 自機のまわりで効く物（衛星・衝撃の輪など）は、敵が近くから始まる。
    const bool close = Is("sub_orbit") || Is("sub_pulse") || Is("sub_flame");
    if (close) {
        tankX_ = area_.x + area_.width * 0.42f;
    }
    for (int i = 0; i < 11; ++i) {
        SpawnEnemy(false);
    }
}

void SurvivorUpgradePreview::SpawnEnemy(bool outside)
{
    Enemy enemy;
    const float pick = Random(0.0f, 1.0f);
    enemy.type = pick < 0.5f ? 0 : pick < 0.78f ? 1 : 2;
    enemy.hp = enemy.type == 2 ? 36.0f : 24.0f;
    enemy.x = outside ? area_.x + area_.width + Random(0.0f, 40.0f) : Random(tankX_ + 130.0f, area_.x + area_.width - 24.0f);
    enemy.y = Random(area_.y + 36.0f, area_.y + area_.height - 36.0f);
    enemy.rotation = Random(0.0f, kTau);
    enemy.spin = Random(-1.0f, 1.0f);
    enemies_.push_back(enemy);
}

int SurvivorUpgradePreview::Nearest(float x, float y, float maxDistance) const
{
    int best = -1;
    float distance = maxDistance;
    for (size_t i = 0; i < enemies_.size(); ++i) {
        const float d = std::hypot(enemies_[i].x - x, enemies_[i].y - y);
        if (d < distance && enemies_[i].x < area_.x + area_.width) {
            distance = d;
            best = static_cast<int>(i);
        }
    }
    return best;
}

void SurvivorUpgradePreview::Say(float x, float y, const std::string& text, float size, const Color& color)
{
    if (numbers_.size() < 12) {
        numbers_.push_back({x, y, 0.0f, text, size, color});
    }
}

void SurvivorUpgradePreview::Add(int kind, float x, float y, float life, float radius, const Color& color, float damage, float push)
{
    if (effects_.size() < 40) {
        effects_.push_back({kind, x, y, 0.0f, life, radius, damage, push, color});
    }
}

void SurvivorUpgradePreview::Damage(int index, float amount, bool critical)
{
    Enemy& enemy = enemies_[static_cast<size_t>(index)];
    if (enemy.hp <= 0.0f) {
        return;
    }
    enemy.hp -= amount;
    enemy.flash = 0.10f;
    const std::string text = std::to_string(static_cast<int>(amount));
    Say(enemy.x + Random(-8.0f, 8.0f), enemy.y - 24.0f, critical ? text + "!" : text, critical ? 1.5f : 0.8f, critical ? kGold : Color{0.85f, 0.97f, 1.0f, 1.0f});
    if (Is("perk_lifesteal")) {
        Add(2, enemy.x, enemy.y, 0.5f, 0.0f, kSubColor);
    }
    if (Is("perk_ultCharge")) {
        gauge_ += 0.03f;
    }
    if (enemy.hp <= 0.0f) {
        Add(0, enemy.x, enemy.y, 0.3f, 36.0f, kEnemyColors[static_cast<size_t>(enemy.type)]);
        if (Is("perk_expGain") || Is("perk_magnet")) {
            Add(4, enemy.x, enemy.y, 0.9f, 0.0f, kPerkColor);
        }
        if (Is("perk_healDrop") && phase_++ % 2 == 0) {
            Add(3, enemy.x, enemy.y, 1.1f, 0.0f, kSubColor);
        }
        if (Is("skill_battleSense")) {
            stacks_ = (std::min)(tier_ >= 3 ? 10 : tier_ == 2 ? 7 : 4, stacks_ + 1);
            haste_ = 2.5f;
        }
    }
}

void SurvivorUpgradePreview::Explode(float x, float y, float radius, float damage, float push, const Color& color)
{
    Add(0, x, y, 0.35f, radius, color);
    for (size_t i = 0; i < enemies_.size(); ++i) {
        Enemy& enemy = enemies_[i];
        const float dx = enemy.x - x, dy = enemy.y - y, d = std::hypot(dx, dy);
        if (d < radius + kEnemyRadius) {
            if (push > 0.0f && d > 0.1f) {
                enemy.x += dx / d * push;
                enemy.y += dy / d * push;
            }
            Damage(static_cast<int>(i), damage, false);
        }
    }
}

void SurvivorUpgradePreview::Shoot(float x, float y, float angle, float speed, float damage, const Color& color, float size, int pierce)
{
    if (bullets_.size() >= 44) {
        return;
    }
    Bullet bullet;
    bullet.x = x;
    bullet.y = y;
    bullet.vx = std::cos(angle) * speed;
    bullet.vy = std::sin(angle) * speed;
    bullet.damage = damage;
    bullet.color = color;
    bullet.size = size;
    bullet.pierce = pierce;
    bullets_.push_back(bullet);
}

void SurvivorUpgradePreview::Update(float dt)
{
    dt = (std::min)(dt, 0.05f);
    time_ += dt;
    flash_ = (std::max)(0.0f, flash_ - dt);
    recoil_ = (std::max)(0.0f, recoil_ - dt * 8.0f);
    haste_ -= dt;
    if (haste_ <= 0.0f) {
        stacks_ = 0;
    }
    const float baseY = area_.y + area_.height * 0.5f;
    if (Is("perk_speed")) {
        tankY_ = baseY + std::sin(time_ * (1.5f + 0.18f * static_cast<float>(tier_))) * 130.0f;
    } else if (Is("sub_mine")) {
        tankY_ = baseY + std::sin(time_ * 0.9f) * 90.0f;
    } else {
        tankY_ = baseY + std::sin(time_ * 0.7f) * 8.0f;
    }

    // 敵：ゆっくり寄る。近づきすぎた物は右へ戻す（場が詰まらないように）。
    for (Enemy& enemy : enemies_) {
        const float dx = tankX_ - enemy.x, dy = tankY_ - enemy.y, d = (std::max)(1.0f, std::hypot(dx, dy));
        const float speed = enemy.x > area_.x + area_.width ? 60.0f : 24.0f;
        enemy.x += dx / d * speed * dt;
        enemy.y += dy / d * speed * dt;
        enemy.rotation += enemy.spin * dt;
        enemy.flash = (std::max)(0.0f, enemy.flash - dt);
        enemy.touch = (std::max)(0.0f, enemy.touch - dt);
        if (d < 52.0f) {
            enemy.x -= dx / d * 130.0f;
            enemy.y -= dy / d * 40.0f;
        }
        enemy.y = (std::clamp)(enemy.y, area_.y + 24.0f, area_.y + area_.height - 24.0f);
    }
    for (size_t i = 0; i < enemies_.size(); ++i) {
        if (enemies_[i].burn > 0.0f) {
            enemies_[i].burn -= dt;
            enemies_[i].hp -= 7.0f * dt;
            if (enemies_[i].hp <= 0.0f) {
                Add(0, enemies_[i].x, enemies_[i].y, 0.3f, 36.0f, kFire);
            }
        }
    }
    enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(), [](const Enemy& enemy) { return enemy.hp <= 0.0f; }), enemies_.end());
    while (enemies_.size() < 11) {
        SpawnEnemy(true);
    }

    // 主武器：5発を続けて撃ち、少し待つ（拡散連射）。
    const int target = Nearest(tankX_, tankY_);
    if (target >= 0) {
        const float wanted = std::atan2(enemies_[static_cast<size_t>(target)].y - tankY_, enemies_[static_cast<size_t>(target)].x - tankX_);
        aim_ += std::remainder(wanted - aim_, kTau) * (std::min)(1.0f, dt * 9.0f);
    }
    const bool main = Is("main");
    const bool steady = Is("skill_steadyAim");
    shotTimer_ -= dt;
    if (shotTimer_ <= 0.0f) {
        if (burstLeft_ <= 0) {
            burstLeft_ = 5 + (Is("skill_ammoBelt") ? tier_ : 0);
        }
        const float spread = (steady && tier_ >= 3 ? 0.04f : 0.09f);
        const bool big = Is("perk_area") || Is("filler_power") || steady;
        const float size = big ? 1.0f + 0.16f * static_cast<float>(tier_) : 1.0f;
        const float angle = aim_ + Random(-spread, spread);
        Shoot(tankX_ + std::cos(angle) * 30.0f, tankY_ + std::sin(angle) * 30.0f, angle, 780.0f, steady ? 12.0f : 8.0f, steady ? kGold : kShotColor, size,
              main ? (tier_ >= 6 ? 2 : tier_ >= 2 ? 1 : 0) : 0);
        if (!bullets_.empty()) {
            bullets_.back().critical = Is("perk_crit") && Random(0.0f, 1.0f) < 0.12f + 0.06f * static_cast<float>(tier_);
            bullets_.back().explode = (main && tier_ >= 6) || Is("perk_area");
        }
        recoil_ = 1.0f;
        --burstLeft_;
        float interval = burstLeft_ > 0 ? 0.15f : 0.75f;
        interval /= 1.0f + (Is("skill_battleSense") ? 0.12f * static_cast<float>(stacks_) : main ? 0.08f * static_cast<float>(tier_) : 0.0f);
        shotTimer_ = interval;
    }

    actionTimer_ -= dt;
    sideTimer_ -= dt;
    if (Is("sub_orbit")) {
        const int count = awaken_ ? 4 : tier_ >= 3 ? 3 : 2;
        const float radius = awaken_ ? 104.0f : tier_ >= 4 ? 92.0f : 76.0f;
        for (int n = 0; n < count; ++n) {
            const float a = time_ * 2.6f + kTau * static_cast<float>(n) / static_cast<float>(count);
            const float x = tankX_ + std::cos(a) * radius, y = tankY_ + std::sin(a) * radius;
            for (size_t i = 0; i < enemies_.size(); ++i) {
                if (enemies_[i].touch <= 0.0f && std::hypot(enemies_[i].x - x, enemies_[i].y - y) < 30.0f) {
                    enemies_[i].touch = 0.35f;
                    if (awaken_) {
                        enemies_[i].x += std::cos(a) * 46.0f;
                        enemies_[i].y += std::sin(a) * 46.0f;
                    }
                    Damage(static_cast<int>(i), 9.0f, false);
                }
            }
        }
    } else if (Is("sub_turret") && actionTimer_ <= 0.0f) {
        actionTimer_ = awaken_ ? 0.4f : tier_ >= 3 ? 0.5f : 0.75f;
        std::vector<std::pair<float, int>> order;
        for (size_t i = 0; i < enemies_.size(); ++i) {
            order.emplace_back(std::hypot(enemies_[i].x - tankX_, enemies_[i].y - tankY_), static_cast<int>(i));
        }
        std::sort(order.begin(), order.end());
        const size_t shots = awaken_ ? 3u : tier_ >= 4 ? 2u : 1u;
        for (size_t n = 0; n < (std::min)(shots, order.size()); ++n) {
            const Enemy& enemy = enemies_[static_cast<size_t>(order[n].second)];
            Shoot(tankX_, tankY_ - 24.0f, std::atan2(enemy.y - tankY_ + 24.0f, enemy.x - tankX_), 560.0f, 7.0f, kSubColor, 0.75f, awaken_ ? 1 : 0);
        }
    } else if (Is("sub_mine") && actionTimer_ <= 0.0f) {
        actionTimer_ = awaken_ ? 0.8f : 1.2f;
        Add(5, tankX_ + Random(40.0f, 200.0f), tankY_ + Random(-30.0f, 30.0f), 6.0f, awaken_ ? 92.0f : 62.0f, kSubColor, 20.0f);
    } else if (Is("sub_mortar") && actionTimer_ <= 0.0f) {
        actionTimer_ = 1.6f;
        // 敵の一番多い所へ落とす。
        float bx = tankX_ + 250.0f, by = tankY_;
        int most = -1;
        for (const Enemy& enemy : enemies_) {
            const int count = static_cast<int>(std::count_if(enemies_.begin(), enemies_.end(), [&](const Enemy& other) {
                return other.x < area_.x + area_.width && std::hypot(other.x - enemy.x, other.y - enemy.y) < 100.0f;
            }));
            if (count > most && enemy.x < area_.x + area_.width - 120.0f) {
                most = count;
                bx = enemy.x;
                by = enemy.y;
            }
        }
        const int shells = awaken_ ? 3 : tier_ >= 3 ? 2 : 1;
        for (int n = 0; n < shells; ++n) {
            Add(1, bx + (shells > 1 ? Random(-60.0f, 60.0f) : 0.0f), by + (shells > 1 ? Random(-46.0f, 46.0f) : 0.0f), 0.7f + 0.15f * static_cast<float>(n),
                awaken_ ? 78.0f : 58.0f, kHurt, 24.0f);
        }
    } else if (Is("sub_drone")) {
        const int count = awaken_ ? 3 : tier_ >= 3 ? 2 : 1;
        for (int n = 0; n < count; ++n) {
            const float a = time_ * 1.3f + kTau * static_cast<float>(n) / 3.0f;
            auto& drone = drones_[static_cast<size_t>(n)];
            drone[0] += (tankX_ - 20.0f + std::cos(a) * 76.0f - drone[0]) * (std::min)(1.0f, dt * 3.0f);
            drone[1] += (tankY_ + std::sin(a) * 76.0f - drone[1]) * (std::min)(1.0f, dt * 3.0f);
        }
        if (actionTimer_ <= 0.0f) {
            actionTimer_ = (tier_ >= 2 ? 0.42f : 0.6f) / static_cast<float>(count);
            const auto& drone = drones_[static_cast<size_t>(phase_++ % count)];
            const int enemy = Nearest(drone[0], drone[1]);
            if (enemy >= 0) {
                Shoot(drone[0], drone[1], std::atan2(enemies_[static_cast<size_t>(enemy)].y - drone[1], enemies_[static_cast<size_t>(enemy)].x - drone[0]), 620.0f,
                      6.0f, kSubColor, 0.75f, awaken_ ? 2 : tier_ >= 4 ? 1 : 0);
            }
        }
    } else if (Is("sub_flame")) {
        const bool on = awaken_ || std::fmod(time_, 2.4f) < 1.3f;
        phase_ = on ? 1 : 0;
        if (on && sideTimer_ <= 0.0f) {
            sideTimer_ = 0.18f;
            const float reach = awaken_ ? 210.0f : 150.0f + 12.0f * static_cast<float>(tier_), width = awaken_ ? 0.62f : 0.42f;
            for (size_t i = 0; i < enemies_.size(); ++i) {
                const float dx = enemies_[i].x - tankX_, dy = enemies_[i].y - tankY_;
                if (std::hypot(dx, dy) < reach && std::abs(std::remainder(std::atan2(dy, dx) - aim_, kTau)) < width) {
                    enemies_[i].burn = 1.6f;
                    Damage(static_cast<int>(i), 3.0f, false);
                }
            }
        }
    } else if (Is("sub_pulse") && actionTimer_ <= 0.0f) {
        actionTimer_ = awaken_ ? 1.2f : 1.7f;
        Explode(tankX_, tankY_, awaken_ ? 180.0f : 120.0f + 8.0f * static_cast<float>(tier_), 8.0f, 60.0f, kSubColor);
    } else if (Is("ultimate")) {
        // 弾幕：しばらく、近い敵へ続けて撃つ。
        if (std::fmod(time_, 3.2f) < 1.8f && sideTimer_ <= 0.0f && !enemies_.empty()) {
            sideTimer_ = 0.07f;
            const Enemy& enemy = enemies_[static_cast<size_t>(Random(0.0f, static_cast<float>(enemies_.size()) - 0.01f))];
            Shoot(tankX_, tankY_, std::atan2(enemy.y - tankY_, enemy.x - tankX_), 720.0f, 7.0f, kGold, 0.9f, awaken_ ? 1 : 0);
        }
    } else if (Is("perk_guard") && actionTimer_ <= 0.0f) {
        actionTimer_ = 1.5f;
        Bullet bullet;
        bullet.x = area_.x + area_.width - 20.0f;
        bullet.y = tankY_ + Random(-120.0f, 120.0f);
        const float a = std::atan2(tankY_ - bullet.y, tankX_ - bullet.x);
        bullet.vx = std::cos(a) * 380.0f;
        bullet.vy = std::sin(a) * 380.0f;
        bullet.hostile = true;
        bullet.color = kHurt;
        bullets_.push_back(bullet);
    } else if (Is("perk_revive") && actionTimer_ <= 0.0f) {
        if (phase_ == 0) {
            phase_ = 1;
            actionTimer_ = 0.9f;
            hp_ = 0.0f;
            flash_ = 0.3f;
        } else {
            phase_ = 0;
            actionTimer_ = 2.0f;
            hp_ = 0.5f;
            Say(tankX_, tankY_ - 60.0f, "+50", 1.6f, kGold);
            Explode(tankX_, tankY_, 150.0f, 6.0f, 70.0f, kGold);
        }
    } else if (Is("filler_heal") && actionTimer_ <= 0.0f) {
        actionTimer_ = 2.2f;
        hp_ = 1.0f;
        Say(tankX_, tankY_ - 56.0f, "+30", 1.2f, kSubColor);
        Add(0, tankX_, tankY_, 0.5f, 60.0f, kSubColor);
    }
    if (Is("filler_heal") || Is("perk_lifesteal")) {
        hp_ = (std::max)(0.25f, hp_ - dt * (Is("filler_heal") ? 0.28f : 0.10f));
    }
    if (Is("perk_armor") || Is("filler_maxHp")) {
        gauge_ = (std::min)(1.0f, std::fmod(time_, 2.4f) / 0.5f);
    }
    if (gauge_ >= 1.0f && (Is("perk_ultCharge") || Is("perk_expGain"))) {
        gauge_ = 0.0f;
        flash_ = 0.4f;
        Add(0, tankX_, tankY_, 0.5f, 80.0f, Is("perk_ultCharge") ? kGold : kPerkColor);
    }
    if (Is("perk_magnet") && sideTimer_ <= 0.0f) {
        sideTimer_ = 0.5f;
        Add(4, Random(tankX_ + 60.0f, tankX_ + 86.0f + 20.0f * static_cast<float>(tier_)), tankY_ + Random(-150.0f, 150.0f), 0.8f, 0.0f, kPerkColor);
    }

    // 弾
    for (Bullet& bullet : bullets_) {
        bullet.x += bullet.vx * dt;
        bullet.y += bullet.vy * dt;
        bullet.life -= dt;
        if (bullet.hostile) {
            if (std::hypot(bullet.x - tankX_, bullet.y - tankY_) < 46.0f) {
                bullet.life = 0.0f;
                flash_ = 0.45f;
                Add(0, bullet.x, bullet.y, 0.3f, 34.0f, kPerkColor);
            }
        } else {
            for (size_t i = 0; i < enemies_.size() && bullet.life > 0.0f; ++i) {
                if (enemies_[i].hp <= 0.0f || std::find(bullet.hit.begin(), bullet.hit.end(), static_cast<int>(i)) != bullet.hit.end() ||
                    std::hypot(enemies_[i].x - bullet.x, enemies_[i].y - bullet.y) > kEnemyRadius + 7.0f * bullet.size) {
                    continue;
                }
                if (bullet.explode) {
                    Explode(bullet.x, bullet.y, 34.0f + 8.0f * static_cast<float>(tier_), bullet.damage * 0.5f, 0.0f, kShotColor);
                }
                Damage(static_cast<int>(i), bullet.critical ? bullet.damage * 2.0f : bullet.damage, bullet.critical);
                bullet.hit.push_back(static_cast<int>(i));
                if (static_cast<int>(bullet.hit.size()) > bullet.pierce) {
                    bullet.life = 0.0f;
                }
            }
        }
        if (bullet.x > area_.x + area_.width + 10.0f || bullet.x < area_.x - 10.0f || bullet.y < area_.y - 10.0f || bullet.y > area_.y + area_.height + 10.0f) {
            bullet.life = 0.0f;
        }
    }
    bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(), [](const Bullet& bullet) { return bullet.life <= 0.0f; }), bullets_.end());

    // 効果（爆発で増えるので、番号で回す）
    for (size_t i = 0; i < effects_.size(); ++i) {
        effects_[i].age += dt;
        const Effect effect = effects_[i];
        if (effect.kind == 1 && effect.age >= effect.life) {
            Explode(effect.x, effect.y, effect.radius, effect.damage, 0.0f, kGold);
        } else if (effect.kind == 5) {
            const int enemy = Nearest(effect.x, effect.y, 44.0f);
            if (enemy >= 0 && effect.age > 0.3f) {
                effects_[i].age = effect.life;
                Explode(effect.x, effect.y, effect.radius, effect.damage, 0.0f, kFire);
                for (int n = 0; n < (awaken_ ? 3 : 0); ++n) {
                    Explode(effect.x + Random(-70.0f, 70.0f), effect.y + Random(-70.0f, 70.0f), 36.0f, 8.0f, 0.0f, kFire);
                }
            }
        } else if (effect.kind >= 2 && effect.kind <= 4 && effect.age >= effect.life) {
            if (effect.kind == 2) {
                hp_ = (std::min)(1.0f, hp_ + 0.03f);
            } else if (effect.kind == 3) {
                Say(tankX_, tankY_ - 56.0f, "+20", 1.1f, kSubColor);
            } else {
                gauge_ += 0.10f + 0.03f * static_cast<float>(tier_);
                flash_ = (std::max)(flash_, 0.1f);
            }
        }
    }
    effects_.erase(std::remove_if(effects_.begin(), effects_.end(), [](const Effect& effect) { return effect.age >= effect.life; }), effects_.end());
    for (Number& number : numbers_) {
        number.age += dt;
        number.y -= 34.0f * dt;
    }
    numbers_.erase(std::remove_if(numbers_.begin(), numbers_.end(), [](const Number& number) { return number.age >= 0.75f; }), numbers_.end());
}

void SurvivorUpgradePreview::Quad(Texture texture, float x, float y, float width, float height, float rotation, const Color& color)
{
    batch_->Quad(SurvivorQuadBatch::PreviewGlow, TexturePath(static_cast<size_t>(texture)), {x, y}, {width, height}, rotation, Linear(Dim(color, fade_)));
}

void SurvivorUpgradePreview::Bar(float x, float y, float width, float height, const Color& color)
{
    batch_->Quad(SurvivorQuadBatch::PreviewTop, "resources/white512x512.png", {x + width * 0.5f, y}, {width, height}, 0.0f, Linear({color.x, color.y, color.z, color.w * fade_}));
}

void SurvivorUpgradePreview::Queue(SurvivorQuadBatch& batch, float fade)
{
    batch_ = &batch;
    fade_ = fade;
    for (const Enemy& enemy : enemies_) {
        Color color = kEnemyColors[static_cast<size_t>(enemy.type)];
        if (enemy.burn > 0.0f) {
            color = kFire;
        }
        if (enemy.flash > 0.0f) {
            color = {1.0f, 1.0f, 1.0f, 1.0f};
        }
        // 場の端では薄くして、枠の外へ急に消えないようにする。
        const float edge = (std::clamp)((area_.x + area_.width + 6.0f - enemy.x) / 60.0f, 0.0f, 1.0f);
        const float size = kEnemySize * (enemy.flash > 0.0f ? 1.12f : 1.0f);
        Quad(enemy.type == 0 ? Square : enemy.type == 1 ? Triangle : Pentagon, enemy.x, enemy.y, size, size, enemy.rotation, Dim(color, edge));
    }
    // 自機
    const bool down = Is("perk_revive") && phase_ == 1;
    const float tank = down ? 0.25f : 1.0f;
    const float kick = recoil_ * 4.0f;
    Quad(Tank, tankX_ - std::cos(aim_) * kick, tankY_ - std::sin(aim_) * kick, 127.0f, 127.0f, aim_, Dim(flash_ > 0.0f && Is("perk_revive") ? kHurt : kTankColor, tank));
    if (Is("perk_speed")) {
        for (int i = 1; i <= 5; ++i) {
            const float past = time_ - 0.05f * static_cast<float>(i);
            const float y = area_.y + area_.height * 0.5f + std::sin(past * (1.5f + 0.18f * static_cast<float>(tier_))) * 130.0f;
            Quad(Dot, tankX_ - 4.0f, y, 34.0f, 34.0f, 0.0f, Dim(kPerkColor, 0.30f - 0.05f * static_cast<float>(i)));
        }
    }
    if (Is("sub_orbit")) {
        const int count = awaken_ ? 4 : tier_ >= 3 ? 3 : 2;
        const float radius = awaken_ ? 104.0f : tier_ >= 4 ? 92.0f : 76.0f;
        Quad(Dash, tankX_, tankY_, radius * 2.29f, radius * 2.29f, time_ * 0.4f, Dim(kSubColor, 0.35f));
        for (int n = 0; n < count; ++n) {
            const float a = time_ * 2.6f + kTau * static_cast<float>(n) / static_cast<float>(count);
            Quad(Dot, tankX_ + std::cos(a) * radius, tankY_ + std::sin(a) * radius, 44.0f, 44.0f, 0.0f, kSubColor);
        }
    } else if (Is("sub_turret")) {
        Quad(Dot, tankX_, tankY_ - 24.0f, 26.0f, 26.0f, 0.0f, kSubColor);
    } else if (Is("sub_drone")) {
        const int count = awaken_ ? 3 : tier_ >= 3 ? 2 : 1;
        for (int n = 0; n < count; ++n) {
            const auto& drone = drones_[static_cast<size_t>(n)];
            const int enemy = Nearest(drone[0], drone[1]);
            const float a = enemy >= 0 ? std::atan2(enemies_[static_cast<size_t>(enemy)].y - drone[1], enemies_[static_cast<size_t>(enemy)].x - drone[0]) : 0.0f;
            Quad(Drone, drone[0], drone[1], 36.0f, 36.0f, a, kSubColor);
        }
    } else if (Is("sub_flame") && phase_ == 1) {
        const float reach = awaken_ ? 220.0f : 160.0f + 12.0f * static_cast<float>(tier_);
        const float width = reach * (awaken_ ? 1.5f : 1.0f) * (0.95f + 0.05f * std::sin(time_ * 40.0f));
        const float start = 26.0f + reach * 0.5f;
        Quad(Flame, tankX_ + std::cos(aim_) * start, tankY_ + std::sin(aim_) * start, reach, width, aim_, kFire);
    } else if (Is("perk_guard")) {
        Quad(Hexagon, tankX_, tankY_, 118.0f, 118.0f, time_ * 0.3f, Dim(kPerkColor, flash_ > 0.0f ? 1.0f : 0.5f));
    } else if (Is("perk_magnet")) {
        const float radius = 92.0f + 20.0f * static_cast<float>(tier_);
        Quad(Dash, tankX_, tankY_, radius * 2.29f, radius * 2.29f, -time_ * 0.3f, Dim(kPerkColor, 0.4f));
    } else if (Is("skill_steadyAim")) {
        const float focus = (std::min)(1.0f, std::fmod(time_, 2.4f) / 0.5f);
        const float radius = 70.0f - 24.0f * focus;
        Quad(Dash, tankX_, tankY_, radius * 2.29f, radius * 2.29f, time_, Dim(kGold, 0.3f + 0.5f * focus));
    } else if (Is("perk_ultCharge") || Is("ultimate")) {
        const float ratio = Is("ultimate") ? (std::fmod(time_, 3.2f) < 1.8f ? 1.0f - std::fmod(time_, 3.2f) / 1.8f : 0.0f) : gauge_;
        for (int i = 0; i < 16; ++i) {
            const float part = (static_cast<float>(i) + 0.5f) / 16.0f, a = -kTau * 0.25f + kTau * part;
            Quad(Dot, tankX_ + std::cos(a) * 56.0f, tankY_ + std::sin(a) * 56.0f, 16.0f, 16.0f, 0.0f, part <= ratio ? kGold : Dim(kGold, 0.12f));
        }
    }
    // 自機の下のゲージ（HP・経験値に関わる強化だけ）
    const float barX = tankX_ - 50.0f, barY = tankY_ + 54.0f;
    if (Is("perk_armor") || Is("filler_maxHp")) {
        const float extra = 10.0f + 7.0f * static_cast<float>(tier_);
        Bar(barX, barY, 100.0f + extra * gauge_, 7.0f, {0.16f, 0.04f, 0.06f, 1.0f});
        Bar(barX, barY, 100.0f, 7.0f, kHurt);
        Bar(barX + 100.0f, barY, extra * gauge_, 7.0f, kSubColor);
    } else if (Is("perk_lifesteal") || Is("filler_heal") || Is("perk_revive") || Is("perk_healDrop")) {
        Bar(barX, barY, 100.0f, 7.0f, {0.16f, 0.04f, 0.06f, 1.0f});
        Bar(barX, barY, 100.0f * (std::clamp)(hp_, 0.0f, 1.0f), 7.0f, kHurt);
    } else if (Is("perk_expGain") || Is("perk_magnet")) {
        Bar(barX, barY, 100.0f, 7.0f, {0.06f, 0.12f, 0.16f, 1.0f});
        Bar(barX, barY, 100.0f * (std::clamp)(gauge_, 0.0f, 1.0f), 7.0f, kPerkColor);
    }
    for (const Bullet& bullet : bullets_) {
        const float a = std::atan2(bullet.vy, bullet.vx);
        Quad(Bolt, bullet.x - std::cos(a) * 16.0f * bullet.size, bullet.y - std::sin(a) * 16.0f * bullet.size, 46.0f * bullet.size, 16.0f * bullet.size, a,
             bullet.color);
    }
    for (const Effect& effect : effects_) {
        const float k = (std::clamp)(effect.age / effect.life, 0.0f, 1.0f);
        if (effect.kind == 0) {
            const float size = effect.radius * 2.29f * (0.35f + 0.65f * std::sqrt(k));
            Quad(Ring, effect.x, effect.y, size, size, 0.0f, Dim(effect.color, 1.0f - k));
        } else if (effect.kind == 1) {
            const float size = effect.radius * 2.29f;
            Quad(Dash, effect.x, effect.y, size, size, 0.0f, Dim(effect.color, 0.6f));
            Quad(Ring, effect.x, effect.y, size * (1.0f - 0.75f * k), size * (1.0f - 0.75f * k), 0.0f, effect.color);
        } else if (effect.kind == 5) {
            const float blink = 0.6f + 0.4f * std::sin(effect.age * 9.0f);
            Quad(Dot, effect.x, effect.y, 22.0f, 22.0f, 0.0f, Dim(effect.color, blink));
            Quad(Ring, effect.x, effect.y, 34.0f, 34.0f, 0.0f, Dim(effect.color, 0.8f));
        } else {
            // 自機へ飛ぶ：始めはその場に少し留まり、後から速くなる。
            const float move = k * k;
            const float x = effect.x + (tankX_ - effect.x) * move, y = effect.y + (tankY_ - effect.y) * move;
            const float size = effect.kind == 3 ? 40.0f : effect.kind == 4 ? 30.0f : 22.0f;
            Quad(effect.kind == 3 ? Wrench : effect.kind == 4 ? Shard : Dot, x, y, size, size, 0.0f, effect.color);
        }
    }
    for (const Number& number : numbers_) {
        const float scale = number.size * (1.0f + (std::max)(0.0f, 0.12f - number.age) * 4.0f);
        const float alpha = (std::clamp)((0.75f - number.age) / 0.3f, 0.0f, 1.0f);
        float width = 0.0f;
        for (const char c : number.text) {
            width += batch.Measure(std::string(1, c), NumberStyle()).x * 0.7f * scale;
        }
        float x = number.x - width * 0.5f;
        for (const char c : number.text) {
            const float step = batch.Measure(std::string(1, c), NumberStyle()).x * 0.7f * scale;
            batch.Label(SurvivorQuadBatch::PreviewTop, std::string(1, c), NumberStyle(), x + step * 0.5f, number.y, 0.5f,
                        Linear({number.color.x, number.color.y, number.color.z, alpha * fade_}), scale);
            x += step;
        }
    }
    batch_ = nullptr;
}

} // namespace gameplay
