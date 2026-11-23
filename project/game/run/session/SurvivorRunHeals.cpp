#include "game/run/session/SurvivorRunController.h"
#include "Calculation.h"
#include "game/render/neon/NeonGridRenderer.h"
#include "game/session/GameplaySystems.h"
#include <algorithm>
#include <cmath>

// 無限の場の試作の、途中の回復（計画書 p8 の F5）。敵がまれに、中ボスは必ず、光るスパナを落とす。
// 拾うと HP と傷が戻る。数が少ないので、欠片と同じく Object3d にはせず、ネオンの格子のパスに線として積む。

namespace gameplay {

namespace {
// スパナを拾う距離・寄ってくる距離（磁石のパークで広がる）と寄る速さ、消える前に点滅する秒。
constexpr float kHealCollectRadius = 1.4f;
constexpr float kHealPullRadius = 2.5f;
constexpr float kHealPullSpeed = 9.0f;
constexpr float kHealBlinkSeconds = 5.0f;
// 色：明るい緑（1 を超える分がブルームで光る）。大きさは敵の四角に負けないくらい（1.7倍、中ボスの物は2.3倍）。
constexpr myEngine::Vector4 kHealColor{0.6f, 3.6f, 1.5f, 1.0f};
} // namespace

void SurvivorRunController::DropHealPickup(const ExpEnemy& enemy)
{
    const auto& heroes = config_.heroes;
    if (!HeroMode() || heroes.healDropHeal <= 0.0f) {
        return;
    }
    const uint64_t id = enemy.GetCollisionId();
    const bool midBoss = std::any_of(midBosses_.begin(), midBosses_.end(), [id](const auto& boss) { return boss.first == id; });
    if (!midBoss) {
        // 整備の手配（パーク）で落ちやすくなる。
        const float chance = heroes.healDropChance * (HasPerk(survivor::PerkId::HealDrop) ? 1.0f + PerkValue(survivor::PerkId::HealDrop) : 1.0f);
        if (chance <= 0.0f || std::uniform_real_distribution<float>(0.0f, 1.0f)(random_) >= chance) {
            return;
        }
    }
    if (heroes.healDropMax > 0 && healPickups_.size() >= static_cast<size_t>(heroes.healDropMax)) {
        healPickups_.erase(healPickups_.begin());
    }
    healPickups_.push_back({enemy.GetWorldPosition(), 0.0f, midBoss});
    ++healsDropped_;
    healsBigDropped_ += midBoss ? 1 : 0;
}

void SurvivorRunController::UpdateHealPickups(float dt)
{
    if (healPickups_.empty() || dt <= 0.0f) {
        return;
    }
    auto& player = *world_.resources.player_;
    const myEngine::Vector3 self = player.GetWorldPosition();
    const float pull = kHealPullRadius * (HasPerk(survivor::PerkId::Magnet) ? 1.0f + PerkValue(survivor::PerkId::Magnet) : 1.0f);
    for (auto it = healPickups_.begin(); it != healPickups_.end();) {
        it->age += dt;
        myEngine::Vector3 to{self.x - it->position.x, self.y - it->position.y, 0.0f};
        const float distance = myEngine::Length(to);
        if (distance <= kHealCollectRadius) {
            const float ratio = config_.heroes.healDropHeal * (it->big ? 1.5f : 1.0f);
            const int amount = (std::max)(1, static_cast<int>(std::round(static_cast<float>(player.GetMaxHp()) * ratio)));
            player.ReduceSurvivorWound(static_cast<float>(amount));
            player.HealRunPlayer(amount);
            healedByPickups_ += amount;
            ++healsCollected_;
            world_.combatFlow->SetEventCallout("修理 +" + std::to_string(amount), 0.7f);
            it = healPickups_.erase(it);
            continue;
        }
        if (distance < pull && distance > 0.001f) {
            it->position = it->position + to * ((std::min)(distance, kHealPullSpeed * dt) / distance);
        }
        if (config_.heroes.healDropLife > 0.0f && it->age >= config_.heroes.healDropLife && !it->big) {
            it = healPickups_.erase(it);
            continue;
        }
        ++it;
    }
}

void SurvivorRunController::QueueHealPickups(myEngine::NeonGridRenderer& renderer) const
{
    for (const HealPickup& heal : healPickups_) {
        // 消える前の数秒は点滅する（中ボスの物は消えない）。
        const float left = config_.heroes.healDropLife - heal.age;
        if (!heal.big && config_.heroes.healDropLife > 0.0f && left < kHealBlinkSeconds && std::fmod(heal.age * 6.0f, 1.0f) < 0.4f) {
            continue;
        }
        const float scale = heal.big ? 2.3f : 1.7f;
        // 落ちた瞬間は小さく、0.25秒でふくらむ。ゆっくり回り、少し脈打つ。
        const float grow = (std::min)(1.0f, heal.age / 0.25f);
        const float pulse = 1.0f + 0.06f * std::sin(heal.age * 5.0f);
        const float s = scale * grow * pulse;
        const float angle = 0.6f + std::sin(heal.age * 1.3f) * 0.5f;
        const myEngine::Vector3 u{std::cos(angle), std::sin(angle), 0.0f};
        const myEngine::Vector3 v{-u.y, u.x, 0.0f};
        const myEngine::Vector3 center{heal.position.x, heal.position.y, -0.35f};
        auto point = [&](float x, float y) { return center + u * (x * s) + v * (y * s); };
        // 柄：まっすぐな太い線。
        renderer.QueueLine(point(-0.5f, 0.0f), point(0.22f, 0.0f), 0.17f * s, kHealColor);
        // 頭：口が +u の方へ開いた輪（スパナの口）。
        constexpr int kHeadSides = 12;
        const float headRadius = 0.27f;
        const float gap = 0.75f;
        for (int n = 0; n < kHeadSides; ++n) {
            const float a = gap + (6.2831853f - 2.0f * gap) * static_cast<float>(n) / kHeadSides;
            const float b = gap + (6.2831853f - 2.0f * gap) * static_cast<float>(n + 1) / kHeadSides;
            renderer.QueueLine(point(0.42f + std::cos(a) * headRadius, std::sin(a) * headRadius),
                               point(0.42f + std::cos(b) * headRadius, std::sin(b) * headRadius), 0.13f * s, kHealColor);
        }
        // 尻：小さな輪。
        constexpr int kTailSides = 8;
        for (int n = 0; n < kTailSides; ++n) {
            const float a = 6.2831853f * static_cast<float>(n) / kTailSides;
            const float b = 6.2831853f * static_cast<float>(n + 1) / kTailSides;
            renderer.QueueLine(point(-0.6f + std::cos(a) * 0.12f, std::sin(a) * 0.12f), point(-0.6f + std::cos(b) * 0.12f, std::sin(b) * 0.12f),
                               0.08f * s, kHealColor);
        }
        // まわりの淡い輪：拾える物だと分かるように、ゆっくり脈打つ。
        constexpr int kHaloSides = 24;
        const float halo = 0.95f * s + 0.1f * std::sin(heal.age * 3.0f);
        const myEngine::Vector4 haloColor{kHealColor.x * 0.6f, kHealColor.y * 0.6f, kHealColor.z * 0.6f, 0.35f};
        for (int n = 0; n < kHaloSides; ++n) {
            const float a = 6.2831853f * static_cast<float>(n) / kHaloSides;
            const float b = 6.2831853f * static_cast<float>(n + 1) / kHaloSides;
            renderer.QueueLine(center + myEngine::Vector3{std::cos(a) * halo, std::sin(a) * halo, 0.0f},
                               center + myEngine::Vector3{std::cos(b) * halo, std::sin(b) * halo, 0.0f}, 0.05f, haloColor);
        }
    }
}

bool SurvivorRunController::FindNearestHeal(const myEngine::Vector3& position, float maxDistance, myEngine::Vector3& out) const
{
    float best = maxDistance;
    bool found = false;
    for (const HealPickup& heal : healPickups_) {
        const float distance = myEngine::Length(myEngine::Vector3{heal.position.x - position.x, heal.position.y - position.y, 0.0f});
        if (distance <= best) {
            best = distance;
            out = heal.position;
            found = true;
        }
    }
    return found;
}

} // namespace gameplay
