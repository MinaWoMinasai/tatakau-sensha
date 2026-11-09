#include "game/run/session/SurvivorPickups.h"
#include "Calculation.h"
#include "game/render/neon/NeonGridRenderer.h"
#include <algorithm>
#include <cmath>

namespace gameplay {

void SurvivorPickups::Drop(const myEngine::Vector3& position, uint32_t value, std::mt19937& random)
{
    if (value == 0) {
        return;
    }
    const int perShard = (std::max)(1, config_.valuePerShard);
    int remaining = static_cast<int>(value);
    const int count = (remaining + perShard - 1) / perShard;
    std::uniform_real_distribution<float> angle(0.0f, 6.2831853f);
    std::uniform_real_distribution<float> speed(2.0f, 6.0f);
    for (int i = 0; i < count; ++i) {
        const int shardValue = (std::min)(perShard, remaining);
        remaining -= shardValue;
        if (shards_.size() >= config_.maxShards && !shards_.empty()) {
            // 上限を超えたら、新しく作らずに既にある欠片へ足す。
            std::uniform_int_distribution<size_t> pick(0, shards_.size() - 1);
            shards_[pick(random)].value += shardValue;
            continue;
        }
        // 撃破位置から少し散らばる（1体から複数出るときに重ならないように）。
        const float a = angle(random), s = count > 1 ? speed(random) : speed(random) * 0.4f;
        Shard shard;
        shard.position = {position.x, position.y, 0.0f};
        shard.velocity = {std::cos(a) * s, std::sin(a) * s, 0.0f};
        shard.value = shardValue;
        shards_.push_back(shard);
    }
}

int SurvivorPickups::Update(float dt, const myEngine::Vector3& player, float magnetScale, bool homing)
{
    int collected = 0;
    const float magnet = config_.magnetRadius * magnetScale;
    for (auto& shard : shards_) {
        shard.age += dt;
        myEngine::Vector3 toPlayer{player.x - shard.position.x, player.y - shard.position.y, 0.0f};
        const float distance = myEngine::Length(toPlayer);
        if (distance <= config_.collectRadius) {
            collected += shard.value;
            shard.value = 0;
            continue;
        }
        if (!shard.attracted && (distance <= magnet || (homing && shard.age >= 0.6f))) {
            shard.attracted = true;
        }
        if (shard.attracted) {
            // 一度吸い寄せ始めたら、範囲の外へ出ても追い続ける。
            shard.velocity += toPlayer * (config_.pullAcceleration * dt / distance);
            const float speed = myEngine::Length(shard.velocity);
            if (speed > config_.pullMaxSpeed) {
                shard.velocity = shard.velocity * (config_.pullMaxSpeed / speed);
            }
        } else {
            // 落ちた直後の散らばりは、すぐに止まる。
            shard.velocity = shard.velocity * (std::max)(0.0f, 1.0f - 6.0f * dt);
        }
        shard.position += shard.velocity * dt;
    }
    shards_.erase(std::remove_if(shards_.begin(), shards_.end(),
                                 [](const Shard& shard) {
                                     return shard.value <= 0;
                                 }),
                  shards_.end());
    return collected;
}

void SurvivorPickups::Queue(myEngine::NeonGridRenderer& renderer, const myEngine::Vector3& cameraRight, const myEngine::Vector3& cameraUp,
                            const myEngine::Vector3& viewCenter, float halfWidth, float halfHeight) const
{
    for (const auto& shard : shards_) {
        if (std::abs(shard.position.x - viewCenter.x) > halfWidth || std::abs(shard.position.y - viewCenter.y) > halfHeight) {
            continue;
        }
        // 量が多い欠片ほど大きく、黄色に寄せる。ゆっくり回して、床の上で目に付くようにする。
        const float weight = (std::min)(1.0f, static_cast<float>(shard.value) / static_cast<float>((std::max)(1, config_.valuePerShard)));
        const float radius = 0.26f + 0.16f * weight;
        const myEngine::Vector4 color{0.35f + 1.25f * weight, 1.55f, 1.25f - 0.75f * weight, 1.0f};
        const myEngine::Vector3 center{shard.position.x, shard.position.y, -0.25f};
        renderer.QueueBillboardRegularPolygonFill(center, 4, radius, shard.age * 2.4f, {0.75f, 1.0f}, color, cameraRight, cameraUp);
    }
}

bool SurvivorPickups::FindNearest(const myEngine::Vector3& position, float maxDistance, myEngine::Vector3& out) const
{
    float best = maxDistance;
    bool found = false;
    for (const auto& shard : shards_) {
        const float distance = myEngine::Length(myEngine::Vector3{shard.position.x - position.x, shard.position.y - position.y, 0.0f});
        if (distance < best) {
            best = distance;
            out = shard.position;
            found = true;
        }
    }
    return found;
}

int SurvivorPickups::GetPendingValue() const
{
    int total = 0;
    for (const auto& shard : shards_) {
        total += shard.value;
    }
    return total;
}

} // namespace gameplay
