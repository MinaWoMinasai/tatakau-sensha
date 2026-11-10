#include "game/run/session/SurvivorLootField.h"
#include "Calculation.h"
#include "game/render/neon/NeonGridRenderer.h"
#include <algorithm>
#include <cmath>

namespace gameplay {
namespace {
// 物が寄ってくる速さと、触れたとみなす距離。
constexpr float kCollectRadius = 1.2f;
constexpr float kPullAcceleration = 60.0f;
constexpr float kPullMaxSpeed = 24.0f;

float Distance2D(const myEngine::Vector3& a, const myEngine::Vector3& b)
{
    return myEngine::Length(myEngine::Vector3{a.x - b.x, a.y - b.y, 0.0f});
}

// 弾の色：ライトは白に近いミント、ヘビーは橙、エネルギーは水色。
// ライトは、黄色い四角の敵と見分けやすいように黄色からずらす。
myEngine::Vector4 AmmoColor(int type)
{
    return type == 1 ? myEngine::Vector4{1.9f, 0.75f, 0.2f, 1.0f} : type == 2 ? myEngine::Vector4{0.35f, 1.5f, 1.9f, 1.0f}
                                                                         : myEngine::Vector4{1.1f, 1.9f, 1.45f, 1.0f};
}

// 拾ったときの光の輪は、同時にこれより多くは出さない（まとめて拾ったとき）。
constexpr size_t kMaxFlashes = 24;

void QueueCircle(myEngine::NeonGridRenderer& renderer, const myEngine::Vector3& center, const myEngine::Vector3& right,
                 const myEngine::Vector3& up, float radius, float width, const myEngine::Vector4& color, int segments)
{
    for (int i = 0; i < segments; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / segments, b = 6.2831853f * static_cast<float>(i + 1) / segments;
        renderer.QueueLine(center + (right * std::cos(a) + up * std::sin(a)) * radius, center + (right * std::cos(b) + up * std::sin(b)) * radius,
                           width, color);
    }
}
} // namespace

myEngine::Vector4 SurvivorLootField::ItemColor(const survivor::LootItem& loot)
{
    switch (loot.kind) {
    case survivor::LootKind::Ammo:
        return AmmoColor(loot.id);
    case survivor::LootKind::Material:
        return loot.id == static_cast<int>(survivor::MaterialId::Boss) ? myEngine::Vector4{1.9f, 0.3f, 0.45f, 1.0f}
                                                                         : myEngine::Vector4{1.15f, 1.1f, 1.6f, 1.0f};
    default: {
        const myEngine::Vector4 rarity = survivor::RarityColor(loot.rarity);
        return {rarity.x * 1.8f, rarity.y * 1.8f, rarity.z * 1.8f, 1.0f};
    }
    }
}

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

std::vector<survivor::LootItem> SurvivorLootField::Update(float dt, const myEngine::Vector3& player,
                                                          const std::array<bool, survivor::kAmmoTypeCount>& acceptAmmo)
{
    std::vector<survivor::LootItem> collected;
    for (Flash& flash : flashes_) {
        flash.age += dt;
    }
    const float flashSeconds = glow_.collectFlashSeconds;
    flashes_.erase(std::remove_if(flashes_.begin(), flashes_.end(),
                                  [flashSeconds](const Flash& flash) {
                                      return flash.age >= flashSeconds;
                                  }),
                   flashes_.end());
    for (Item& item : items_) {
        item.age += dt;
        const bool ammo = item.loot.kind == survivor::LootKind::Ammo;
        const size_t type = static_cast<size_t>((std::clamp)(item.loot.id, 0, static_cast<int>(survivor::kAmmoTypeCount) - 1));
        const bool accept = !ammo || acceptAmmo[type];
        const myEngine::Vector3 toPlayer{player.x - item.position.x, player.y - item.position.y, 0.0f};
        const float distance = myEngine::Length(toPlayer);
        if (accept) {
            if (distance <= kCollectRadius) {
                collected.push_back(item.loot);
                item.taken = true;
                if (glow_.collectFlashSeconds > 0.0f && flashes_.size() < kMaxFlashes) {
                    flashes_.push_back({item.position, ItemColor(item.loot), 0.0f});
                }
                continue;
            }
            if (distance <= config_.magnetRadius) {
                item.attracted = true;
            }
        } else {
            // 予備が上限の弾は寄ってこない（あとで拾えるように残す）。
            item.attracted = false;
        }
        if (item.attracted && distance > 0.001f) {
            item.velocity += toPlayer * (kPullAcceleration * dt / distance);
            const float speed = myEngine::Length(item.velocity);
            if (speed > kPullMaxSpeed) {
                item.velocity = item.velocity * (kPullMaxSpeed / speed);
            }
        } else {
            // 落ちた直後の散らばりは、すぐに止まる。
            item.velocity = item.velocity * (std::max)(0.0f, 1.0f - 6.0f * dt);
        }
        // 光の尾：吸い込まれている間だけ、少し前の位置を一定の間隔で覚える（古い順）。
        if (item.attracted && glow_.lootTrailSeconds > 0.0f) {
            const float interval = glow_.lootTrailSeconds / static_cast<float>(kTrailPoints);
            item.trailTimer += dt;
            if (item.trailCount == 0 || item.trailTimer >= interval) {
                item.trailTimer = 0.0f;
                if (item.trailCount == kTrailPoints) {
                    std::rotate(item.trail.begin(), item.trail.begin() + 1, item.trail.end());
                    --item.trailCount;
                }
                item.trail[item.trailCount++] = item.position;
            }
        } else {
            item.trailCount = 0;
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

void SurvivorLootField::Queue(myEngine::NeonGridRenderer& renderer, const myEngine::Vector3& cameraRight, const myEngine::Vector3& cameraUp,
                              const myEngine::Vector3& viewCenter, float halfWidth, float halfHeight) const
{
    for (const Item& item : items_) {
        if (std::abs(item.position.x - viewCenter.x) > halfWidth || std::abs(item.position.y - viewCenter.y) > halfHeight) {
            continue;
        }
        const myEngine::Vector3 center{item.position.x, item.position.y, -0.25f};
        const myEngine::Vector4 itemColor = ItemColor(item.loot);
        // 光の尾：根元（いまの位置）が太く明るく、古い点ほど細く薄くする。
        if (item.trailCount > 0 && glow_.lootTrailWidth > 0.0f) {
            myEngine::Vector3 head = center;
            for (size_t i = item.trailCount; i-- > 0;) {
                const myEngine::Vector3 tail{item.trail[i].x, item.trail[i].y, -0.25f};
                const float t = static_cast<float>(i + 1) / static_cast<float>(item.trailCount + 1);
                const myEngine::Vector4 color{itemColor.x, itemColor.y, itemColor.z, 0.85f * t};
                renderer.QueueLine(tail, head, glow_.lootTrailWidth * (0.25f + 0.75f * t), color);
                head = tail;
            }
        }
        // 消える前の3秒は点滅させる。
        const float left = config_.lifetime - item.age;
        const bool expires = item.loot.kind == survivor::LootKind::Ammo || item.loot.kind == survivor::LootKind::Material;
        if (expires && left < 3.0f && std::fmod(item.age, 0.3f) < 0.12f) {
            continue;
        }
        switch (item.loot.kind) {
        case survivor::LootKind::Ammo:
            // 弾：細長い板（弾倉の形）を、輪で囲む（敵の図形と見分けるため）。
            renderer.QueueBillboardRegularPolygonFill(center, 4, 0.34f, 0.785398f, {0.55f, 1.0f}, itemColor, cameraRight, cameraUp);
            if (glow_.lootRingRadius > 0.0f && glow_.lootRingWidth > 0.0f) {
                QueueCircle(renderer, center, cameraRight, cameraUp, glow_.lootRingRadius, glow_.lootRingWidth,
                            {itemColor.x, itemColor.y, itemColor.z, 0.8f}, 12);
            }
            break;
        case survivor::LootKind::Material: {
            // 素材：落とした敵の形（ボスの素材は六角）。白っぽい紫でゆっくり回す。
            static constexpr int kSides[survivor::kMaterialCount] = {4, 3, 5, 6};
            const int sides = kSides[(std::clamp)(item.loot.id, 0, static_cast<int>(survivor::kMaterialCount) - 1)];
            const bool boss = item.loot.id == static_cast<int>(survivor::MaterialId::Boss);
            renderer.QueueBillboardRegularPolygonFill(center, sides, boss ? 0.5f : 0.3f, item.age * 1.6f, {1.0f, 1.0f}, itemColor, cameraRight,
                                                      cameraUp);
            if (glow_.lootRingRadius > 0.0f && glow_.lootRingWidth > 0.0f) {
                QueueCircle(renderer, center, cameraRight, cameraUp, glow_.lootRingRadius * (boss ? 1.4f : 1.0f), glow_.lootRingWidth,
                            {itemColor.x, itemColor.y, itemColor.z, 0.8f}, 12);
            }
            break;
        }
        case survivor::LootKind::Chip:
        case survivor::LootKind::Weapon: {
            // チップは小さな四角、武器は大きな菱形。レア度の色で塗り、同じ色の輪を脈打たせる。
            const bool weapon = item.loot.kind == survivor::LootKind::Weapon;
            const myEngine::Vector4 color = itemColor;
            renderer.QueueBillboardRegularPolygonFill(center, 4, weapon ? 0.7f : 0.38f, weapon ? 0.0f : 0.785398f,
                                                      weapon ? myEngine::Vector2{0.7f, 1.0f} : myEngine::Vector2{1.0f, 1.0f}, color, cameraRight,
                                                      cameraUp);
            const float pulse = std::fmod(item.age, 1.2f) / 1.2f;
            const float radius = (weapon ? 0.9f : 0.55f) + pulse * (weapon ? 1.4f : 0.9f);
            const myEngine::Vector4 ring{color.x, color.y, color.z, (1.0f - pulse) * 0.9f};
            QueueCircle(renderer, center, cameraRight, cameraUp, radius, 0.07f, ring, 24);
            break;
        }
        }
    }
    // 拾ったところに、物の色の輪を広げて消す。
    if (glow_.collectFlashSeconds > 0.0f) {
        for (const Flash& flash : flashes_) {
            if (std::abs(flash.position.x - viewCenter.x) > halfWidth || std::abs(flash.position.y - viewCenter.y) > halfHeight) {
                continue;
            }
            const float t = (std::clamp)(flash.age / glow_.collectFlashSeconds, 0.0f, 1.0f);
            const float ease = 1.0f - (1.0f - t) * (1.0f - t);
            const myEngine::Vector3 center{flash.position.x, flash.position.y, -0.25f};
            const myEngine::Vector4 color{flash.color.x * 1.3f, flash.color.y * 1.3f, flash.color.z * 1.3f, (1.0f - t) * 0.95f};
            QueueCircle(renderer, center, cameraRight, cameraUp, 0.3f + ease * glow_.collectFlashRadius, 0.09f * (1.0f - t) + 0.02f, color, 16);
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
