#include "game/run/session/SurvivorRunController.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/session/GameplaySystems.h"
#include "game/render/settings/ArenaCameraProfile.h"
#include <algorithm>
#include <cmath>

// 育つ戦車の試作の湧き方・脅威の段階・宿敵・出口・敵どうしの押し離し。

namespace gameplay {

using namespace survivorrun;

SurvivorRunController::AliveCounts SurvivorRunController::CountAlive() const
{
    AliveCounts counts;
    for (const ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
        if (!enemy || enemy->IsDead()) {
            continue;
        }
        ++counts.total;
        if (enemy->GetType() == ExpEnemyType::Shooter || enemy->IsExpeditionCombatRole()) {
            ++counts.tanks;
        } else if (enemy->GetType() == ExpEnemyType::Pentagon && enemy->GetRadius() > 1.0f) {
            ++counts.bigPentagons;
        } else {
            ++counts.shapes;
        }
    }
    return counts;
}

void SurvivorRunController::UpdateSpawns(float dt)
{
    spawnBudget_ = (std::min)(spawnBudget_ + config_.maxSpawnsPerSecond * dt, config_.maxSpawnsPerSecond);
    const AliveCounts alive = CountAlive();

    // 三角の群れ：輪の1点から、まとまって自機へ寄ってくる。群れは予算と別に湧かせる。
    // 脅威の段階が上がるほど、群れは大きく、間隔は短くなる。湧く lead 秒前に場所を決めて予告し、時刻が来たら湧かせる。
    const int extraStages = threatStage_ - 1;
    if (config_.swarmCount > 0 && elapsed_ >= nextSwarm_ - config_.telegraph.lead) {
        const float at = nextSwarm_;
        nextSwarm_ += config_.swarm.interval * std::pow(config_.threat.swarmIntervalScalePerStage, static_cast<float>(extraStages));
        const int swarmCount = config_.swarmCount + config_.threat.swarmCountPerStage * extraStages;
        if (const auto center = FindSpawnPosition(config_.ringMinDistance, config_.ringMaxDistance)) {
            pendingWaves_.push_back({at, *center, swarmCount, false});
            QueueHint(Hint::Swarm);
        }
    }
    ReleasePendingWaves();

    // 重い四角：大きく硬く、ゆっくり迫る。脅威の段階が fromStage に達してから、一定の間隔で湧く。
    if (config_.heavySquare.count > 0 && threatStage_ >= config_.heavySquare.fromStage && elapsed_ >= nextHeavy_) {
        nextHeavy_ = elapsed_ + config_.heavySquare.interval;
        const int room = config_.telegraph.maxChasers - CountChasers();
        for (int i = 0; i < (std::min)(config_.heavySquare.count, room); ++i) {
            const auto position = FindSpawnPosition(config_.ringMinDistance, config_.ringMaxDistance);
            if (!position || !SpawnShape("Square", *position, false)) {
                continue;
            }
            const auto& heavy = config_.heavySquare;
            ExpEnemy* enemy = world_.resources.enemyManager_->GetEnemyPtrs().back();
            enemy->SetSizeScale(heavy.scale);
            enemy->SetHp(static_cast<int>(std::lround(static_cast<float>(enemy->GetMaxHp()) * heavy.hpScale)));
            enemy->SetPlayerChase(heavy.acceleration, heavy.maxSpeed);
            ++heavySpawned_;
        }
    }

    // 敵戦車：一定の間隔で、同時に maxAlive 体まで。種類は順番に回す。
    if (!config_.tankTypes.empty() && elapsed_ >= nextTank_ && spawnBudget_ >= 1.0f) {
        nextTank_ = elapsed_ + config_.tanks.interval;
        if (alive.tanks < config_.tanks.maxAlive + config_.threat.tankMaxAlivePerStage * extraStages) {
            if (const auto position = FindSpawnPosition(config_.ringMinDistance, config_.ringMaxDistance)) {
                const std::string& type = config_.tankTypes[nextTankType_ % config_.tankTypes.size()];
                if (world_.resources.enemyManager_->SpawnLevelEnemy(*position, type)) {
                    ScaleLastSpawnHp();
                    ++nextTankType_;
                    spawnBudget_ -= 1.0f;
                }
            }
        }
    }

    // 大五角：五角を大きく硬くしたもの。
    if (elapsed_ >= nextBigPentagon_ && spawnBudget_ >= 1.0f) {
        nextBigPentagon_ = elapsed_ + config_.bigPentagon.interval;
        if (alive.bigPentagons < config_.bigPentagon.maxAlive) {
            if (const auto position = FindSpawnPosition(config_.ringMinDistance, config_.ringMaxDistance)) {
                const int hp = static_cast<int>(std::lround(static_cast<float>(config_.bigPentagonHp) * EnemyHpScale()));
                if (world_.resources.enemyManager_->SpawnLevelEnemy(*position, "Pentagon", hp)) {
                    ExpEnemy* enemy = world_.resources.enemyManager_->GetEnemyPtrs().back();
                    enemy->SetSizeScale(config_.bigPentagonScale);
                    enemy->SetExpValue(static_cast<uint32_t>(config_.bigPentagonExp));
                    spawnBudget_ -= 1.0f;
                }
            }
        }
    }

    // 図形：生存数が目標に届くまで、予算の分だけ湧かせる。
    int missing = static_cast<int>(config_.ShapeTarget(elapsed_)) - alive.shapes;
    const auto& weights = config_.ShapeWeightsAt(elapsed_);
    std::discrete_distribution<int> pick(weights.begin(), weights.end());
    static constexpr std::array<const char*, 3> kShapes{"Square", "Triangle", "Pentagon"};
    const auto& lunge = config_.lungePentagon;
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    std::uniform_real_distribution<float> phase(0.0f, (std::max)(0.1f, lunge.interval));
    int chasers = CountChasers();
    while (missing > 0 && spawnBudget_ >= 1.0f) {
        const auto position = FindSpawnPosition(config_.ringMinDistance, config_.ringMaxDistance);
        if (!position) {
            break;
        }
        const size_t kind = static_cast<size_t>(pick(random_));
        if (SpawnShape(kShapes[kind], *position, false) && kind == 2 && threatStage_ >= lunge.fromStage && chasers < config_.telegraph.maxChasers &&
            chance(random_) < lunge.chance) {
            // 突進する五角：ゆっくり寄り、周期ごとに溜めてから速く迫る。周期は個体ごとにずらす。
            ExpEnemy* enemy = world_.resources.enemyManager_->GetEnemyPtrs().back();
            enemy->SetPlayerChase(lunge.acceleration, lunge.maxSpeed);
            enemy->SetPlayerLunge(lunge.interval, lunge.windup, lunge.duration, lunge.lungeSpeed, phase(random_));
            ++chasers;
            ++lungeSpawned_;
        }
        spawnBudget_ -= 1.0f;
        --missing;
    }
}

int SurvivorRunController::CountChasers() const
{
    int count = 0;
    for (const ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
        if (enemy && !enemy->IsDead() && enemy->IsChasingPlayer()) {
            ++count;
        }
    }
    return count;
}

void SurvivorRunController::ReleasePendingWaves()
{
    // 予告の時刻が来た群れと輪を湧かせる。寄ってくる敵が上限に近ければ、その分だけ数を減らす。
    for (size_t i = 0; i < pendingWaves_.size();) {
        const PendingWave wave = pendingWaves_[i];
        if (elapsed_ < wave.at) {
            ++i;
            continue;
        }
        pendingWaves_.erase(pendingWaves_.begin() + static_cast<std::ptrdiff_t>(i));
        const int count = (std::min)(wave.count, config_.telegraph.maxChasers - CountChasers());
        if (count <= 0) {
            ++wavesSkipped_;
            continue;
        }
        if (wave.ring) {
            SpawnRing(count);
            continue;
        }
        std::uniform_real_distribution<float> offset(-config_.swarmSpread, config_.swarmSpread);
        for (int n = 0; n < count; ++n) {
            myEngine::Vector3 position = wave.center + myEngine::Vector3{offset(random_), offset(random_), 0.0f};
            position.x = (std::clamp)(position.x, kSpawnMinX, kSpawnMaxX);
            position.y = (std::clamp)(position.y, kSpawnMinY, kSpawnMaxY);
            if (!world_.resources.stage_->IsCollisionWithAnyBlock(position, 1.2f)) {
                SpawnShape("Triangle", position, true);
            }
        }
        ++swarmsSpawned_;
    }
}

std::optional<myEngine::Vector3> SurvivorRunController::FindSpawnPosition(float minDistance, float maxDistance)
{
    // 自機の周りの輪から探す。壁際で輪が部屋の外に出るときは、画面の外でありさえすればよい。
    const myEngine::Vector3 player = world_.resources.player_->GetWorldPosition();
    const myEngine::Vector3 camera = world_.resources.camera->GetTranslate();
    const float halfHeight = arenacamera::HalfViewHeight() + 2.0f;
    const float halfWidth = arenacamera::HalfViewHeight() * arenacamera::kAspect + 2.0f;
    std::uniform_real_distribution<float> angle(0.0f, 6.2831853f);
    std::uniform_real_distribution<float> distance(minDistance, maxDistance);
    std::uniform_real_distribution<float> anyX(kSpawnMinX, kSpawnMaxX), anyY(kSpawnMinY, kSpawnMaxY);
    for (int attempt = 0; attempt < 24; ++attempt) {
        myEngine::Vector3 position{};
        if (attempt < 16) {
            const float a = angle(random_), d = distance(random_);
            position = {player.x + std::cos(a) * d, player.y + std::sin(a) * d, 0.0f};
            position.x = (std::clamp)(position.x, kSpawnMinX, kSpawnMaxX);
            position.y = (std::clamp)(position.y, kSpawnMinY, kSpawnMaxY);
        } else {
            position = {anyX(random_), anyY(random_), 0.0f};
        }
        const bool offScreen = std::abs(position.x - camera.x) > halfWidth || std::abs(position.y - camera.y) > halfHeight;
        if (offScreen && !world_.resources.stage_->IsCollisionWithAnyBlock(position, 1.2f)) {
            return position;
        }
    }
    return std::nullopt;
}

bool SurvivorRunController::SpawnShape(const char* prefab, const myEngine::Vector3& position, bool triangleChase)
{
    if (!world_.resources.enemyManager_->SpawnLevelEnemy(position, prefab)) {
        return false;
    }
    ScaleLastSpawnHp();
    if (triangleChase) {
        // 段階が上がるほど速く寄る。加速度は最高速度と同じ割合で上げる。
        const float maxSpeed = config_.swarmMaxSpeed + config_.threat.swarmSpeedPerStage * static_cast<float>(threatStage_ - 1);
        const float acceleration = config_.swarmMaxSpeed > 0.0f ? config_.swarmAcceleration * maxSpeed / config_.swarmMaxSpeed : 0.0f;
        world_.resources.enemyManager_->GetEnemyPtrs().back()->SetPlayerChase(acceleration, maxSpeed);
    }
    return true;
}

float SurvivorRunController::EnemyHpScale() const
{
    return (std::max)(0.1f, 1.0f + config_.threat.enemyHpPerStage * static_cast<float>(threatStage_ - 1));
}

void SurvivorRunController::ScaleLastSpawnHp()
{
    const float scale = EnemyHpScale();
    if (scale == 1.0f || world_.resources.enemyManager_->GetEnemyPtrs().empty()) {
        return;
    }
    ExpEnemy* enemy = world_.resources.enemyManager_->GetEnemyPtrs().back();
    enemy->SetHp(static_cast<int>(std::lround(static_cast<float>(enemy->GetMaxHp()) * scale)));
}

void SurvivorRunController::UpdateThreat()
{
    const int stage = config_.ThreatStage(elapsed_);
    if (stage != threatStage_) {
        threatStage_ = stage;
        std::string text = "脅威 Lv" + std::to_string(stage);
        if (stage == config_.threat.ringFromStage) {
            text += "　─　包囲が来る";
            nextRing_ = elapsed_ + 3.0f;
        }
        world_.combatFlow->SetEventCallout(text, 1.2f);
    }
    // 包囲の輪も lead 秒前に予告する（輪の中心は、湧く時刻の自機の位置）。
    if (config_.threat.ringCount > 0 && threatStage_ >= config_.threat.ringFromStage && elapsed_ >= nextRing_ - config_.telegraph.lead) {
        pendingWaves_.push_back({nextRing_, {}, config_.threat.ringCount, true});
        QueueHint(Hint::Ring);
        nextRing_ += config_.threat.ringInterval;
    }
}

void SurvivorRunController::SpawnRing(int count)
{
    // 輪の上の点のうち、部屋の中で壁と柱に重ならないところにだけ置く。一部が欠けても、残りで囲む。
    const myEngine::Vector3 center = world_.resources.player_->GetWorldPosition();
    std::uniform_real_distribution<float> turn(0.0f, 6.2831853f);
    const float start = turn(random_);
    int placed = 0;
    for (int i = 0; i < count; ++i) {
        const float angle = start + 6.2831853f * static_cast<float>(i) / static_cast<float>(count);
        const myEngine::Vector3 position{center.x + std::cos(angle) * config_.threat.ringRadius,
                                         center.y + std::sin(angle) * config_.threat.ringRadius, 0.0f};
        if (position.x < kSpawnMinX || position.x > kSpawnMaxX || position.y < kSpawnMinY || position.y > kSpawnMaxY ||
            world_.resources.stage_->IsCollisionWithAnyBlock(position, 1.2f)) {
            continue;
        }
        if (SpawnShape("Triangle", position, true)) {
            ++placed;
        }
    }
    if (placed > 0) {
        ++ringsSpawned_;
        world_.combatFlow->SetEventCallout("包囲", 0.8f);
    }
}

void SurvivorRunController::SeparateEnemies()
{
    // 重なりの半分ずつを、1回の更新で押し戻す（重いもの＝大きいものほど動きにくい）。壁や柱に入る押し戻しはしない。
    constexpr float kCell = 3.2f;
    constexpr float kResolve = 0.5f;
    auto cellKey = [](int x, int y) {
        return (static_cast<int64_t>(x) << 32) ^ static_cast<int64_t>(static_cast<uint32_t>(y));
    };
    separationItems_.clear();
    for (ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
        if (!enemy || enemy->IsDead() || enemy->IsRunResource()) {
            continue;
        }
        SeparationItem item;
        item.enemy = enemy;
        item.position = enemy->GetWorldPosition();
        item.radius = enemy->GetRadius();
        item.mass = item.radius * item.radius;
        item.cell = cellKey(static_cast<int>(std::floor(item.position.x / kCell)), static_cast<int>(std::floor(item.position.y / kCell)));
        separationItems_.push_back(item);
    }
    const uint32_t count = static_cast<uint32_t>(separationItems_.size());
    separationOrder_.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        separationOrder_[i] = i;
    }
    std::sort(separationOrder_.begin(), separationOrder_.end(),
              [&](uint32_t a, uint32_t b) { return separationItems_[a].cell < separationItems_[b].cell; });
    auto firstInCell = [&](int64_t cell) {
        return std::lower_bound(separationOrder_.begin(), separationOrder_.end(), cell,
                                [&](uint32_t index, int64_t key) { return separationItems_[index].cell < key; });
    };
    for (uint32_t i = 0; i < count; ++i) {
        SeparationItem& a = separationItems_[i];
        const int cx = static_cast<int>(std::floor(a.position.x / kCell));
        const int cy = static_cast<int>(std::floor(a.position.y / kCell));
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int64_t cell = cellKey(cx + dx, cy + dy);
                for (auto it = firstInCell(cell); it != separationOrder_.end() && separationItems_[*it].cell == cell; ++it) {
                    // 組は1回だけ扱う（番号の小さい側から）。
                    if (*it <= i) {
                        continue;
                    }
                    SeparationItem& b = separationItems_[*it];
                    const float ox = b.position.x - a.position.x, oy = b.position.y - a.position.y;
                    const float limit = a.radius + b.radius;
                    const float distanceSq = ox * ox + oy * oy;
                    if (distanceSq >= limit * limit) {
                        continue;
                    }
                    float distance = std::sqrt(distanceSq);
                    myEngine::Vector3 normal{1.0f, 0.0f, 0.0f};
                    if (distance > 0.0001f) {
                        normal = {ox / distance, oy / distance, 0.0f};
                    } else {
                        // 完全に重なっているときは、番号から決まる向きに離す。
                        const float angle = static_cast<float>((i * 7 + *it * 13) % 360) * 0.0174533f;
                        normal = {std::cos(angle), std::sin(angle), 0.0f};
                        distance = 0.0f;
                    }
                    const float overlap = (limit - distance) * kResolve;
                    const float total = a.mass + b.mass;
                    a.push -= normal * (overlap * b.mass / total);
                    b.push += normal * (overlap * a.mass / total);
                }
            }
        }
    }
    for (SeparationItem& item : separationItems_) {
        if (item.push.x == 0.0f && item.push.y == 0.0f) {
            continue;
        }
        const myEngine::Vector3 moved = item.position + item.push;
        if (!world_.resources.stage_->IsCollisionWithAnyBlock(moved, item.radius)) {
            item.enemy->SetWorldPosition(moved);
        }
    }
}

bool SurvivorRunController::IsRivalActive() const
{
    return rivalSpawned_ && !rivalDefeated_;
}

void SurvivorRunController::SpawnRival()
{
    // 出口の候補のうち、自機から最も遠いところに出す。
    const myEngine::Vector3 player = world_.resources.player_->GetWorldPosition();
    float best = -1.0f;
    myEngine::Vector3 position = kExitCandidates.front();
    for (const auto& candidate : kExitCandidates) {
        const float distance = myEngine::Length(candidate - player);
        if (distance > best) {
            best = distance;
            position = candidate;
        }
    }
    rivalSpawned_ = true;
    rivalSpawnedAt_ = elapsed_;
    auto& rival = *world_.resources.enemy_;
    world_.run.tankExpeditionRivalActive_ = true;
    rival.ResetRunEncounter(position, (std::max)(1, config_.rival.hp), 1, true);
    rival.EnableExpeditionRival(true);
    auto progress = rival.GetEnemyProgressConfig();
    progress.levelingModeEnabled = false;
    rival.SetEnemyProgressConfig(progress);
    world_.combat.previousBossHp_ = rival.GetHp();
    world_.combat.bossDefeatHandled_ = false;
    world_.combat.screenEffectDirector_.TriggerBossEntry();
    // 試作では、立体の宿敵の頭を常に画面の上へ向け、少し大きく置く（照準へ頭を向けると横に寝て見えるため）。
    if (world_.resources.neonBossVisual_) {
        world_.resources.neonBossVisual_->SetUpright(true, 1.5f);
    }
    // 登場の演出：ゲームを止め、カメラを宿敵へ寄せる（UpdateCinematic）。
    cineMode_ = CineMode::RivalIntro;
    cineTime_ = 0.0f;
}

void SurvivorRunController::OnRivalDefeated()
{
    if (!IsRivalActive()) {
        return;
    }
    // 倒れた宿敵は、スローとカメラの寄りで撃破の演出を見せてから、欠片と宝箱を落として出口を開く（FinishRivalDefeat）。
    // 遭遇戦はすぐに止める。倒れたまま有効にしておくと、立体の宿敵の資源が描画の途中で解放されて落ちるため。
    auto& rival = *world_.resources.enemy_;
    const myEngine::Vector3 position = rival.GetWorldPosition();
    rival.SetRunEncounterEnabled(false);
    world_.run.tankExpeditionRivalActive_ = false;
    rivalDefeated_ = true;
    rivalDefeatedAt_ = elapsed_;
    cineMode_ = CineMode::RivalDefeat;
    cineTime_ = 0.0f;
    cineBursts_ = 0;
    cineAnchor_ = position;
    world_.combatEffects->TriggerDeathPostPulse(position, 1.55f);
    world_.presentation.cameraShakeDuration_ = 0.5f;
    world_.presentation.cameraShakeTimer_ = world_.presentation.cameraShakeDuration_;
    world_.presentation.cameraShakePower_ = 0.9f;
    world_.combatFlow->SetEventCallout("宿敵を撃破", 1.4f);
}

void SurvivorRunController::FinishRivalDefeat()
{
    const myEngine::Vector3 position = cineAnchor_;
    pickups_.Drop(position, static_cast<uint32_t>((std::max)(0, config_.rival.exp)), random_);
    expDropped_ += (std::max)(0, config_.rival.exp);
    ++pendingChoices_;
    ++pendingPremium_;
    ++premiumTotal_;
    pendingSince_.push_back(elapsed_);
    choiceHud_.SetPending(pendingChoices_, choiceOpen_);
    OpenExit();
    world_.combatFlow->SetEventCallout("出口が開いた", 1.6f);
}

void SurvivorRunController::OpenExit()
{
    exitOpen_ = true;
    const myEngine::Vector3 player = world_.resources.player_->GetWorldPosition();
    float best = -1.0f;
    for (const auto& candidate : kExitCandidates) {
        const float distance = myEngine::Length(candidate - player);
        if (distance > best) {
            best = distance;
            exitPosition_ = candidate;
        }
    }
    world_.combatFlow->SetEventCallout("出口が開いた", 1.4f);
}

} // namespace gameplay
