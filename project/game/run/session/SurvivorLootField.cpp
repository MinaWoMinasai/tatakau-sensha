#include "game/run/session/SurvivorLootField.h"
#include "Calculation.h"
#include "game/render/neon/NeonGridRenderer.h"
#include <algorithm>
#include <cmath>

namespace gameplay {
namespace {
// 弾が寄ってくる速さと、触れたとみなす距離。
constexpr float kAmmoCollectRadius = 1.2f;
constexpr float kAmmoPullAcceleration = 60.0f;
constexpr float kAmmoPullMaxSpeed = 24.0f;

float Distance2D(const myEngine::Vector3& a, const myEngine::Vector3& b)
{
    return myEngine::Length(myEngine::Vector3{a.x - b.x, a.y - b.y, 0.0f});
}

// 弾の色：ライトは黄、ヘビーは橙、エネルギーは水色。
myEngine::Vector4 AmmoColor(int type)
{
    return type == 1 ? myEngine::Vector4{1.9f, 0.75f, 0.2f, 1.0f} : type == 2 ? myEngine::Vector4{0.35f, 1.5f, 1.9f, 1.0f}
                                                                         : myEngine::Vector4{1.7f, 1.55f, 0.45f, 1.0f};
}
} // namespace

void SurvivorLootField::Drop(const myEngine::Vector3& position, const survivor::LootItem& item, std::mt19937& random)
{
    // 撃破位置から少し散らばる（1体から複数出るときに重ならないように）。
    const float angle = std::uniform_real_distribution<float>(0.0f, 6.2831853f)(random);
    const float speed = std::uniform_real_distribution<float>(2.0f, 5.0f)(random);
    Item dropped;
    dropped.loot = item;
    dropped.position = {position.x, position.y, 0.0f};
    dropped.velocity = {std::cos(angle) * speed, std::sin(angle) * speed, 0.0f};
    items_.push_back(dropped);
}

std::array<int, survivor::kAmmoTypeCount> SurvivorLootField::Update(float dt, const myEngine::Vector3& player,
                                                                    const std::array<bool, survivor::kAmmoTypeCount>& acceptAmmo)
{
    std::array<int, survivor::kAmmoTypeCount> collected{};
    for (Item& item : items_) {
        item.age += dt;
        const bool ammo = item.loot.kind == survivor::LootKind::Ammo;
        const size_t type = static_cast<size_t>((std::clamp)(item.loot.id, 0, static_cast<int>(survivor::kAmmoTypeCount) - 1));
        const myEngine::Vector3 toPlayer{player.x - item.position.x, player.y - item.position.y, 0.0f};
        const float distance = myEngine::Length(toPlayer);
        if (ammo && acceptAmmo[type]) {
            if (distance <= kAmmoCollectRadius) {
                collected[type] += item.loot.amount;
                item.taken = true;
                continue;
            }
            if (distance <= config_.ammoMagnetRadius) {
                item.attracted = true;
            }
        } else if (ammo) {
            // 予備が上限の間は寄ってこない（あとで拾えるように残す）。
            item.attracted = false;
        }
        if (item.attracted && distance > 0.001f) {
            item.velocity += toPlayer * (kAmmoPullAcceleration * dt / distance);
            const float speed = myEngine::Length(item.velocity);
            if (speed > kAmmoPullMaxSpeed) {
                item.velocity = item.velocity * (kAmmoPullMaxSpeed / speed);
            }
        } else {
            // 落ちた直後の散らばりは、すぐに止まる。
            item.velocity = item.velocity * (std::max)(0.0f, 1.0f - 6.0f * dt);
        }
        item.position += item.velocity * dt;
        // 弾と素材は時間がたつと消える。チップと武器は残る。
        const bool expires = item.loot.kind == survivor::LootKind::Ammo || item.loot.kind == survivor::LootKind::Material;
        if (expires && item.age >= config_.lifetime) {
            item.taken = true;
        }
    }
    items_.erase(std::remove_if(items_.begin(), items_.end(),
                                [](const Item& item) {
                                    return item.taken;
                                }),
                 items_.end());
    return collected;
}

std::vector<survivor::LootItem> SurvivorLootField::TakeNear(const myEngine::Vector3& position)
{
    std::vector<survivor::LootItem> taken;
    for (Item& item : items_) {
        if (IsKeyPickup(item.loot) && Distance2D(item.position, position) <= config_.pickupRadius) {
            taken.push_back(item.loot);
            item.taken = true;
        }
    }
    items_.erase(std::remove_if(items_.begin(), items_.end(),
                                [](const Item& item) {
                                    return item.taken;
                                }),
                 items_.end());
    return taken;
}

size_t SurvivorLootField::CountNear(const myEngine::Vector3& position, const survivor::LootItem** nearest) const
{
    size_t count = 0;
    float best = config_.pickupRadius;
    if (nearest) {
        *nearest = nullptr;
    }
    for (const Item& item : items_) {
        if (!IsKeyPickup(item.loot)) {
            continue;
        }
        const float distance = Distance2D(item.position, position);
        if (distance > config_.pickupRadius) {
            continue;
        }
        ++count;
        // 名前に出すのは、武器 → チップ → 素材の順で、同じ種類なら一番近い物。
        if (nearest && (!*nearest || item.loot.kind > (*nearest)->kind || (item.loot.kind == (*nearest)->kind && distance < best))) {
            best = distance;
            *nearest = &item.loot;
        }
    }
    return count;
}

void SurvivorLootField::Queue(myEngine::NeonGridRenderer& renderer, const myEngine::Vector3& cameraRight, const myEngine::Vector3& cameraUp,
                              const myEngine::Vector3& viewCenter, float halfWidth, float halfHeight) const
{
    for (const Item& item : items_) {
        if (std::abs(item.position.x - viewCenter.x) > halfWidth || std::abs(item.position.y - viewCenter.y) > halfHeight) {
            continue;
        }
        const myEngine::Vector3 center{item.position.x, item.position.y, -0.25f};
        // 消える前の3秒は点滅させる。
        const float left = config_.lifetime - item.age;
        const bool expires = item.loot.kind == survivor::LootKind::Ammo || item.loot.kind == survivor::LootKind::Material;
        if (expires && left < 3.0f && std::fmod(item.age, 0.3f) < 0.12f) {
            continue;
        }
        switch (item.loot.kind) {
        case survivor::LootKind::Ammo:
            // 弾：細長い板（弾倉の形）。
            renderer.QueueBillboardRegularPolygonFill(center, 4, 0.42f, 0.785398f, {0.55f, 1.0f}, AmmoColor(item.loot.id), cameraRight, cameraUp);
            break;
        case survivor::LootKind::Material: {
            // 素材：落とした敵の形（ボスの素材は六角）。白っぽい紫でゆっくり回す。
            static constexpr int kSides[survivor::kMaterialCount] = {4, 3, 5, 6};
            const int sides = kSides[(std::clamp)(item.loot.id, 0, static_cast<int>(survivor::kMaterialCount) - 1)];
            const bool boss = item.loot.id == static_cast<int>(survivor::MaterialId::Boss);
            const myEngine::Vector4 color = boss ? myEngine::Vector4{1.9f, 0.3f, 0.45f, 1.0f} : myEngine::Vector4{1.15f, 1.1f, 1.6f, 1.0f};
            renderer.QueueBillboardRegularPolygonFill(center, sides, boss ? 0.5f : 0.36f, item.age * 1.6f, {1.0f, 1.0f}, color, cameraRight,
                                                      cameraUp);
            break;
        }
        case survivor::LootKind::Chip:
        case survivor::LootKind::Weapon: {
            // チップは小さな四角、武器は大きな菱形。レア度の色で塗り、同じ色の輪を脈打たせる。
            const bool weapon = item.loot.kind == survivor::LootKind::Weapon;
            const myEngine::Vector4 rarity = survivor::RarityColor(item.loot.rarity);
            const myEngine::Vector4 color{rarity.x * 1.8f, rarity.y * 1.8f, rarity.z * 1.8f, 1.0f};
            renderer.QueueBillboardRegularPolygonFill(center, 4, weapon ? 0.7f : 0.38f, weapon ? 0.0f : 0.785398f,
                                                      weapon ? myEngine::Vector2{0.7f, 1.0f} : myEngine::Vector2{1.0f, 1.0f}, color, cameraRight,
                                                      cameraUp);
            const float pulse = std::fmod(item.age, 1.2f) / 1.2f;
            const float radius = (weapon ? 0.9f : 0.55f) + pulse * (weapon ? 1.4f : 0.9f);
            const myEngine::Vector4 ring{color.x, color.y, color.z, (1.0f - pulse) * 0.9f};
            constexpr int kSegments = 24;
            for (int i = 0; i < kSegments; ++i) {
                const float a = 6.2831853f * static_cast<float>(i) / kSegments, b = 6.2831853f * static_cast<float>(i + 1) / kSegments;
                renderer.QueueLine(center + (cameraRight * std::cos(a) + cameraUp * std::sin(a)) * radius,
                                   center + (cameraRight * std::cos(b) + cameraUp * std::sin(b)) * radius, 0.07f, ring);
            }
            break;
        }
        }
    }
}

bool SurvivorLootField::FindNearest(const myEngine::Vector3& position, float maxDistance, myEngine::Vector3& out) const
{
    float best = maxDistance;
    bool found = false;
    for (const Item& item : items_) {
        const float distance = Distance2D(item.position, position);
        if (distance < best) {
            best = distance;
            out = item.position;
            found = true;
        }
    }
    return found;
}

} // namespace gameplay
