#include "game/run/session/SurvivorRunController.h"
#include "game/run/session/SurvivorRunShared.h"
#include "Calculation.h"
#include "game/render/neon/NeonGridRenderer.h"
#include "game/session/GameplaySystems.h"
#include <algorithm>
#include <cmath>

// 育つ戦車の試作の、ステージ2から出る新しい敵（計画書 p3 の W9）：ばらばらに飛ぶ三角・味方を回復する敵・ワープする敵・地雷をまく敵。
// 動きの中身は区画の側で持ち、敵には役割と時間だけを持たせる（ほかの遊び方の敵には影響しない）。

namespace gameplay {
namespace {
constexpr int kHealer = static_cast<int>(ExpEnemy::SurvivorRole::Healer);
constexpr int kWarper = static_cast<int>(ExpEnemy::SurvivorRole::Warper);
constexpr int kMineLayer = static_cast<int>(ExpEnemy::SurvivorRole::MineLayer);
constexpr float kWarpFadeSeconds = 0.5f;
const myEngine::Vector4 kHealerColor{0.35f, 1.0f, 0.55f, 1.0f};
const myEngine::Vector4 kWarperColor{0.85f, 0.4f, 1.0f, 1.0f};
const myEngine::Vector4 kMineLayerColor{1.0f, 0.55f, 0.15f, 1.0f};
const myEngine::Vector4 kMineColor{1.0f, 0.3f, 0.12f, 1.0f};
const myEngine::Vector4 kErraticColor{0.45f, 0.85f, 1.0f, 1.0f};

float Distance2D(const myEngine::Vector3& a, const myEngine::Vector3& b)
{
    return myEngine::Length(myEngine::Vector3{a.x - b.x, a.y - b.y, 0.0f});
}
} // namespace

bool SurvivorRunController::ElitesActive() const
{
    return WeaponMode() && stage_ + 1 >= config_.elites.fromStage;
}

void SurvivorRunController::ResetEliteTimers()
{
    const auto& elites = config_.elites;
    nextElite_ = {elites.healerFirst, elites.warperFirst, elites.mineLayerFirst};
    eliteRings_.clear();
}

void SurvivorRunController::MakeErratic(ExpEnemy& enemy)
{
    std::uniform_real_distribution<float> phase(0.0f, 6.2831853f);
    enemy.SetWobble(config_.elites.erraticStrength, config_.elites.erraticFrequency, phase(random_));
    enemy.SetBaseColor(kErraticColor);
    ++eliteStats_.erratic;
}

void SurvivorRunController::SpawnElite(ExpEnemy::SurvivorRole role)
{
    const auto& elites = config_.elites;
    const auto position = FindSpawnPosition(config_.ringMinDistance, config_.ringMaxDistance);
    if (!position) {
        return;
    }
    const bool healer = role == ExpEnemy::SurvivorRole::Healer;
    const bool warper = role == ExpEnemy::SurvivorRole::Warper;
    // 四角は回復する敵、五角はワープする敵（線をディゾルブで消せる形）、三角は地雷をまく敵。
    if (!SpawnShape(healer ? "Square" : warper ? "Pentagon" : "Triangle", *position, false)) {
        return;
    }
    ExpEnemy& enemy = *world_.resources.enemyManager_->GetEnemyPtrs().back();
    const float hpScale = healer ? elites.healerHpScale : warper ? elites.warperHpScale : elites.mineLayerHpScale;
    enemy.SetHp(static_cast<int>(std::lround(static_cast<float>(enemy.GetMaxHp()) * hpScale)));
    enemy.SetSurvivorRole(role);
    enemy.SetSizeScale(1.25f);
    enemy.SetBaseColor(healer ? kHealerColor : warper ? kWarperColor : kMineLayerColor);
    enemy.SetPlayerChase(healer ? elites.healerSpeed * 0.5f : 5.0f, healer ? elites.healerSpeed : warper ? 4.5f : 3.5f);
    // 初めて出たときだけ、何が来たかを知らせる。
    const size_t index = static_cast<size_t>(role) - 1;
    if (!eliteAnnounced_[index]) {
        eliteAnnounced_[index] = true;
        world_.combatFlow->SetEventCallout(healer ? "味方を回復する敵" : warper ? "ワープする敵" : "地雷をまく敵", 1.2f);
    }
    ++eliteStats_.spawned[index];
}

void SurvivorRunController::ExplodeMine(ExpEnemy& mine)
{
    const auto& elites = config_.elites;
    const myEngine::Vector3 position = mine.GetWorldPosition();
    auto& player = *world_.resources.player_;
    if (Distance2D(position, player.GetWorldPosition()) <= elites.mineBlastRadius) {
        // バリアの扇の中で爆発したら、自機には届かない（スタミナが減る）。
        if (barrierOn_ && IsInsideBarrier(position, 0.0f)) {
            barrierStamina_ = (std::max)(0.0f, barrierStamina_ - config_.barrier.hitCost);
            barrierFlash_ = 1.0f;
        } else {
            player.TakeDamage(static_cast<uint32_t>((std::max)(0, elites.mineDamage)));
            ++eliteStats_.mineHits;
        }
    }
    eliteRings_.push_back({position, 0.0f, elites.mineBlastRadius, kMineColor});
    world_.combatEffects->TriggerDeathPostPulse(position, 0.8f);
    mine.DespawnWithoutReward();
    ++eliteStats_.mineExplosions;
}

void SurvivorRunController::UpdateElites(float dt)
{
    for (EliteRing& ring : eliteRings_) {
        ring.age += dt;
    }
    eliteRings_.erase(std::remove_if(eliteRings_.begin(), eliteRings_.end(),
                                     [](const EliteRing& ring) {
                                         return ring.age >= 0.45f;
                                     }),
                      eliteRings_.end());
    if (!ElitesActive()) {
        return;
    }
    const auto& elites = config_.elites;
    const std::array<int, 3> maxAlive{elites.healerMaxAlive, elites.warperMaxAlive, elites.mineLayerMaxAlive};
    const std::array<float, 3> interval{elites.healerInterval, elites.warperInterval, elites.mineLayerInterval};

    // 数えて、足りなければ湧かせる。
    std::array<int, 3> alive{};
    int mines = 0;
    const std::vector<ExpEnemy*> enemies = world_.resources.enemyManager_->GetEnemyPtrs();
    for (ExpEnemy* enemy : enemies) {
        if (!enemy || enemy->IsDead()) {
            continue;
        }
        const int role = static_cast<int>(enemy->GetSurvivorRole());
        if (role >= kHealer && role <= kMineLayer) {
            ++alive[static_cast<size_t>(role) - 1];
        } else if (enemy->GetSurvivorRole() == ExpEnemy::SurvivorRole::Mine) {
            ++mines;
        }
    }
    for (size_t i = 0; i < nextElite_.size(); ++i) {
        if (elapsed_ >= nextElite_[i]) {
            nextElite_[i] = elapsed_ + interval[i];
            if (alive[i] < maxAlive[i]) {
                SpawnElite(static_cast<ExpEnemy::SurvivorRole>(i + 1));
            }
        }
    }

    // 役割ごとの行動。地雷を足すと一覧が伸びるので、番号で回し、足す分は後で湧かせる。
    const myEngine::Vector3 player = world_.resources.player_->GetWorldPosition();
    std::vector<myEngine::Vector3> newMines;
    const size_t count = enemies.size();
    for (size_t i = 0; i < count; ++i) {
        ExpEnemy* enemy = enemies[i];
        if (!enemy || enemy->IsDead() || enemy->GetSurvivorRole() == ExpEnemy::SurvivorRole::None) {
            continue;
        }
        const float timer = enemy->AdvanceRoleTimer(dt);
        const myEngine::Vector3 position = enemy->GetWorldPosition();
        switch (enemy->GetSurvivorRole()) {
        case ExpEnemy::SurvivorRole::Healer:
            if (timer >= elites.healInterval) {
                enemy->SetRoleTimer(0.0f);
                bool healed = false;
                for (ExpEnemy* other : enemies) {
                    if (!other || other == enemy || other->IsDead() || other->GetSurvivorRole() == ExpEnemy::SurvivorRole::Mine ||
                        other->GetHp() >= other->GetMaxHp() || Distance2D(other->GetWorldPosition(), position) > elites.healRadius) {
                        continue;
                    }
                    const int amount = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(other->GetMaxHp()) * elites.healRatio)));
                    eliteStats_.healedHp += other->HealHp(amount);
                    healed = true;
                }
                // 回復した波の輪を見せる（いないときも、回復する敵だと分かるように出す）。
                eliteRings_.push_back({position, 0.0f, elites.healRadius, kHealerColor});
                if (healed) {
                    ++eliteStats_.heals;
                }
            }
            break;
        case ExpEnemy::SurvivorRole::Warper: {
            // 周期の終わりに消えていき、周期の頭で移って現れる。
            const float left = elites.warpInterval - timer;
            if (left <= 0.0f) {
                std::uniform_real_distribution<float> angle(0.0f, 6.2831853f);
                for (int attempt = 0; attempt < 8; ++attempt) {
                    const float a = angle(random_);
                    const myEngine::Vector3 target{player.x + std::cos(a) * elites.warpDistance, player.y + std::sin(a) * elites.warpDistance, position.z};
                    if (target.x > survivorrun::kSpawnMinX && target.x < survivorrun::kSpawnMaxX && target.y > survivorrun::kSpawnMinY &&
                        target.y < survivorrun::kSpawnMaxY && !world_.resources.stage_->IsCollisionWithAnyBlock(target, enemy->GetRadius())) {
                        enemy->SetWorldPosition(target);
                        ++eliteStats_.warps;
                        break;
                    }
                }
                enemy->SetRoleTimer(0.0f);
                enemy->SetWarpDissolve(1.12f);
            } else if (left < kWarpFadeSeconds) {
                enemy->SetWarpDissolve((1.0f - left / kWarpFadeSeconds) * 1.12f);
            } else {
                enemy->SetWarpDissolve((std::max)(0.0f, (1.0f - timer / kWarpFadeSeconds) * 1.12f));
            }
            break;
        }
        case ExpEnemy::SurvivorRole::MineLayer:
            if (timer >= elites.layInterval) {
                enemy->SetRoleTimer(0.0f);
                newMines.push_back(position);
            }
            break;
        case ExpEnemy::SurvivorRole::Mine: {
            // 置かれてすぐは反応しない。時間切れでも爆発する。
            const bool armed = timer >= elites.mineArmSeconds;
            if ((armed && Distance2D(position, player) <= elites.mineTriggerRadius) || timer >= elites.mineLifeSeconds) {
                ExplodeMine(*enemy);
            }
            break;
        }
        default:
            break;
        }
    }
    for (const myEngine::Vector3& position : newMines) {
        if (mines >= elites.mineMaxAlive || !world_.resources.enemyManager_->SpawnLevelEnemy(position, "Pentagon", (std::max)(1, elites.mineHp))) {
            continue;
        }
        ExpEnemy& mine = *world_.resources.enemyManager_->GetEnemyPtrs().back();
        mine.SetSurvivorRole(ExpEnemy::SurvivorRole::Mine);
        mine.SetSizeScale(0.55f);
        mine.SetBaseColor(kMineColor);
        mine.SetExpValue(0);
        ++mines;
        ++eliteStats_.minesLaid;
    }
}

void SurvivorRunController::QueueEliteRings(myEngine::NeonGridRenderer& renderer, const myEngine::Vector3& cameraForward) const
{
    constexpr int kSegments = 40;
    for (const EliteRing& ring : eliteRings_) {
        const float t = ring.age / 0.45f;
        const float radius = ring.radius * (0.35f + 0.65f * t);
        const myEngine::Vector4 color{ring.color.x * 1.6f, ring.color.y * 1.6f, ring.color.z * 1.6f, (1.0f - t) * 0.85f};
        const myEngine::Vector3 center = ring.position + myEngine::Vector3{0.0f, 0.0f, 0.3f};
        for (int i = 0; i < kSegments; ++i) {
            const float a = 6.2831853f * static_cast<float>(i) / kSegments, b = 6.2831853f * static_cast<float>(i + 1) / kSegments;
            renderer.QueueCameraFacingLine(center + myEngine::Vector3{std::cos(a) * radius, std::sin(a) * radius, 0.0f},
                                           center + myEngine::Vector3{std::cos(b) * radius, std::sin(b) * radius, 0.0f}, 0.1f, color, cameraForward);
        }
    }
    // 地雷は、動かないのに光って脈打つように、外に細い輪を足す。
    const float pulse = 0.5f + 0.5f * std::sin(elapsed_ * 7.0f);
    for (const ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
        if (!enemy || enemy->IsDead() || enemy->GetSurvivorRole() != ExpEnemy::SurvivorRole::Mine) {
            continue;
        }
        const myEngine::Vector3 center = enemy->GetWorldPosition() + myEngine::Vector3{0.0f, 0.0f, 0.3f};
        const float radius = config_.elites.mineTriggerRadius;
        const myEngine::Vector4 color{kMineColor.x * 1.5f, kMineColor.y * 1.5f, kMineColor.z * 1.5f, 0.18f + 0.3f * pulse};
        for (int i = 0; i < 24; i += 2) {
            const float a = 6.2831853f * static_cast<float>(i) / 24.0f, b = 6.2831853f * static_cast<float>(i + 1) / 24.0f;
            renderer.QueueCameraFacingLine(center + myEngine::Vector3{std::cos(a) * radius, std::sin(a) * radius, 0.0f},
                                           center + myEngine::Vector3{std::cos(b) * radius, std::sin(b) * radius, 0.0f}, 0.06f, color, cameraForward);
        }
    }
}

} // namespace gameplay
