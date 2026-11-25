#include "game/run/session/SurvivorRunController.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/session/GameplaySystems.h"
#include "game/render/settings/ArenaCameraProfile.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

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
            enemy->SetPlayerChase(heavy.acceleration * EnemySpeedScale(), heavy.maxSpeed * EnemySpeedScale());
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
    // 足し続ける形（計画書 p8 の G1）：間隔ごとに数を足していき、生存数の上限までの分だけ湧かせる（予算は使わず、1回の更新で12体まで）。
    const bool growth = config_.growth.enabled;
    if (growth) {
        const auto point = CurrentGrowth();
        growthTimer_ -= dt;
        while (growthTimer_ <= 0.0f) {
            growthTimer_ += (std::max)(0.05f, point.interval);
            growthRemainder_ += point.count;
            const int add = static_cast<int>(growthRemainder_);
            growthRemainder_ -= static_cast<float>(add);
            growthPending_ += add;
        }
        const int cap = static_cast<int>(std::lround(point.maxAlive));
        growthPending_ = (std::min)(growthPending_, (std::max)(0, cap));
        growthAliveMax_ = (std::max)(growthAliveMax_, alive.shapes);
        missing = (std::min)({growthPending_, (std::max)(0, cap - alive.shapes), 12});
    }
    const auto& weights = config_.ShapeWeightsAt(elapsed_);
    std::discrete_distribution<int> pick(weights.begin(), weights.end());
    static constexpr std::array<const char*, 3> kShapes{"Square", "Triangle", "Pentagon"};
    const auto& lunge = config_.lungePentagon;
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    std::uniform_real_distribution<float> phase(0.0f, (std::max)(0.1f, lunge.interval));
    int chasers = CountChasers();
    while (missing > 0 && (growth || spawnBudget_ >= 1.0f)) {
        const auto position = FindSpawnPosition(config_.ringMinDistance, config_.ringMaxDistance);
        if (!position) {
            break;
        }
        const size_t kind = static_cast<size_t>(pick(random_));
        const bool spawned = SpawnShape(kShapes[kind], *position, false);
        // 寄ってくる図形（計画書 p7 の E5。設定の shapes.chase）：形ごとの速さで自機へ寄る。
        if (spawned && config_.shapeChase[kind].second > 0.0f) {
            world_.resources.enemyManager_->GetEnemyPtrs().back()->SetPlayerChase(config_.shapeChase[kind].first * EnemySpeedScale(),
                                                                                  config_.shapeChase[kind].second * EnemySpeedScale());
        }
        if (spawned && growth) {
            --growthPending_;
            ++growthSpawned_;
        }
        if (spawned && kind == 2 && threatStage_ >= lunge.fromStage && chasers < config_.telegraph.maxChasers &&
            chance(random_) < lunge.chance) {
            // 突進する五角：ゆっくり寄り、周期ごとに溜めてから速く迫る。周期は個体ごとにずらす。
            ExpEnemy* enemy = world_.resources.enemyManager_->GetEnemyPtrs().back();
            enemy->SetPlayerChase(lunge.acceleration, lunge.maxSpeed);
            enemy->SetPlayerLunge(lunge.interval, lunge.windup, lunge.duration, lunge.lungeSpeed, phase(random_));
            ++chasers;
            ++lungeSpawned_;
        }
        if (!growth) {
            spawnBudget_ -= 1.0f;
        }
        --missing;
    }
}

survivor::SpawnConfig::SpawnGrowth::Point SurvivorRunController::CurrentGrowth() const
{
    auto point = config_.growth.At(elapsed_);
    for (const Surge& surge : surges_) {
        if (surge.until <= 0.0f || elapsed_ < surge.until) {
            point.interval *= (std::max)(0.05f, surge.intervalScale);
            point.count *= (std::max)(0.0f, surge.countScale);
            point.maxAlive += static_cast<float>(surge.maxAliveAdd);
        }
    }
    return point;
}

ExpEnemy* SurvivorRunController::SpawnChaser(const std::string& prefab, const myEngine::Vector3& position, float speed)
{
    if (position.x < kSpawnMinX || position.x > kSpawnMaxX || position.y < kSpawnMinY || position.y > kSpawnMaxY ||
        world_.resources.stage_->IsCollisionWithAnyBlock(position, 1.2f) || !SpawnShape(prefab.c_str(), position, false)) {
        return nullptr;
    }
    ExpEnemy* enemy = world_.resources.enemyManager_->GetEnemyPtrs().back();
    enemy->SetPlayerChase(speed * 1.5f * EnemySpeedScale(), speed * EnemySpeedScale());
    return enemy;
}

float SurvivorRunController::PickRingGap(float radius)
{
    constexpr float kTurn = 6.2831853f;
    const myEngine::Vector3 self = world_.resources.player_->GetWorldPosition();
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const bool moving = myEngine::Length(playerVelocity_) > 1.0f;
    const float heading = std::atan2(playerVelocity_.y, playerVelocity_.x);
    float angle = 0.0f;
    for (int attempt = 0; attempt < 12; ++attempt) {
        // 進む向きの左右60度を除いた、残りの240度から選ぶ。止まっているときは、どこでもよい。
        angle = moving ? heading + kTurn * (1.0f / 6.0f + unit(random_) * 2.0f / 3.0f) : unit(random_) * kTurn;
        const float x = self.x + std::cos(angle) * radius, y = self.y + std::sin(angle) * radius;
        if (x > kSpawnMinX && x < kSpawnMaxX && y > kSpawnMinY && y < kSpawnMaxY) {
            break;
        }
    }
    return angle;
}

void SurvivorRunController::QueueRingWarning(myEngine::NeonGridRenderer& renderer) const
{
    if (ringWarning_.at < 0.0f) {
        return;
    }
    // 自機のまわりの赤い点の輪。切れ目には点が無く、両端に外へ開く短い線を置く。輪は画面に入る大きさにする（湧く輪は画面の外まである）。
    const myEngine::Vector3 center = world_.resources.player_->GetWorldPosition();
    const float radius = (std::min)(ringWarning_.radius, arenacamera::HalfViewHeight() - 3.6f);
    const float left = (std::clamp)((ringWarning_.at - elapsed_) / (std::max)(0.1f, ringWarning_.lead), 0.0f, 1.0f);
    const float blink = 0.5f + 0.5f * std::sin(elapsed_ * (10.0f + 14.0f * (1.0f - left)));
    const myEngine::Vector4 color{1.8f, 0.18f, 0.22f, 0.45f + 0.55f * blink};
    const float half = ringWarning_.gap * 0.5f * 3.1415927f / 180.0f;
    auto at = [&](float angle, float r) {
        return center + myEngine::Vector3{std::cos(angle) * r, std::sin(angle) * r, -0.3f};
    };
    constexpr int kDots = 44;
    for (int n = 0; n < kDots; ++n) {
        const float angle = 6.2831853f * static_cast<float>(n) / kDots;
        if (std::abs(std::remainder(angle - ringWarning_.angle, 6.2831853f)) <= half) {
            continue;
        }
        renderer.QueueLine(at(angle - 0.012f, radius), at(angle + 0.012f, radius), 0.24f, color);
    }
    for (const float side : {-1.0f, 1.0f}) {
        const float edge = ringWarning_.angle + side * half;
        renderer.QueueLine(at(edge, radius), at(edge - side * 0.06f, radius + 1.8f), 0.16f, color);
    }
}

void SurvivorRunController::UpdateTimeline()
{
    // 切れ目のある輪は、warn 秒前に切れ目の向きを決めて知らせる。
    for (size_t i = timelineNext_; i < config_.timeline.size() && ringWarning_.at < 0.0f; ++i) {
        const auto& event = config_.timeline[i];
        if (event.kind == "ring" && event.gap > 0.0f && event.warn > 0.0f && elapsed_ < event.at && elapsed_ >= event.at - event.warn) {
            ringWarning_ = {event.at, event.warn, PickRingGap(event.radius), event.gap, event.radius};
        }
    }
    while (timelineNext_ < config_.timeline.size() && elapsed_ >= config_.timeline[timelineNext_].at) {
        RunTimelineEvent(config_.timeline[timelineNext_++]);
    }
    // 中ボスが倒れたら（いなくなったら）、レベルアップの3択を rewards 回ぶん渡す。
    for (auto it = midBosses_.begin(); it != midBosses_.end();) {
        const auto& enemies = world_.resources.enemyManager_->GetEnemyPtrs();
        const bool alive = std::any_of(enemies.begin(), enemies.end(), [&](const ExpEnemy* enemy) {
            return enemy && !enemy->IsDead() && enemy->GetCollisionId() == it->first;
        });
        if (alive) {
            ++it;
            continue;
        }
        ++midBossKills_;
        world_.combatFlow->SetEventCallout("中ボスを倒した", 1.4f);
        if (HeroMode() && config_.heroes.levelUps && it->second > 0) {
            heroPendingLevelUps_ += it->second;
            if (!heroLevelUpOpen_) {
                RequestHeroLevelUp();
            }
        }
        it = midBosses_.erase(it);
    }
}

void SurvivorRunController::RunTimelineEvent(const survivor::SpawnConfig::TimelineEvent& event)
{
    ++timelineEvents_;
    const myEngine::Vector3 player = world_.resources.player_->GetWorldPosition();
    const myEngine::Vector3 camera = world_.resources.camera->GetTranslate();
    const float halfHeight = arenacamera::HalfViewHeight() + 2.0f;
    const float halfWidth = arenacamera::HalfViewHeight() * arenacamera::kAspect + 2.0f;
    if (event.kind == "wall") {
        // 壁：画面のすぐ外の両側に、縁に沿って等間隔に並べる。両側から挟むように寄ってくるので、どちらかを破って抜ける。
        const bool vertical = event.side == "topBottom";
        const int count = (std::max)(1, event.count);
        for (const float sign : {-1.0f, 1.0f}) {
            for (int i = 0; i < count; ++i) {
                const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(count) * 2.0f - 1.0f;
                const myEngine::Vector3 position = vertical ? myEngine::Vector3{camera.x + t * halfWidth, camera.y + sign * (halfHeight + 1.0f), 0.0f}
                                                            : myEngine::Vector3{camera.x + sign * (halfWidth + 1.0f), camera.y + t * halfHeight, 0.0f};
                SpawnChaser(event.shape, position, event.speed);
            }
        }
    } else if (event.kind == "ring") {
        // 輪：自機のまわりに等間隔に並べ、いっせいに寄らせる。
        const int count = (std::max)(1, event.count);
        std::uniform_real_distribution<float> turn(0.0f, 6.2831853f);
        const float start = turn(random_);
        const bool gap = event.gap > 0.0f;
        const float gapAngle = !gap ? 0.0f : ringWarning_.at >= 0.0f ? ringWarning_.angle : PickRingGap(event.radius);
        ringWarning_.at = -1.0f;
        for (int i = 0; i < count; ++i) {
            const float angle = start + 6.2831853f * static_cast<float>(i) / static_cast<float>(count);
            if (gap && std::abs(std::remainder(angle - gapAngle, 6.2831853f)) <= event.gap * 0.5f * 3.1415927f / 180.0f) {
                continue;
            }
            SpawnChaser(event.shape, {player.x + std::cos(angle) * event.radius, player.y + std::sin(angle) * event.radius, 0.0f}, event.speed);
        }
    } else if (event.kind == "surge") {
        // 大群（計画書 p8 の G1）：しばらく、足し続ける湧きを速く・多くする。
        surges_.push_back({event.duration > 0.0f ? elapsed_ + event.duration : 0.0f, event.intervalScale, event.countScale, event.maxAliveAdd});
        ++surgesStarted_;
        surges_.erase(std::remove_if(surges_.begin(), surges_.end(), [this](const Surge& s) { return s.until > 0.0f && elapsed_ >= s.until; }),
                      surges_.end());
    } else if (event.kind == "midBoss") {
        // 中ボス：画面のすぐ外から、大きく硬い1体。
        const auto position = FindSpawnPosition(config_.ringMinDistance, config_.ringMaxDistance);
        if (position && world_.resources.enemyManager_->SpawnLevelEnemy(*position, event.shape,
                                                                         static_cast<int>(std::lround(static_cast<float>((std::max)(1, event.hp)) * EnemyHpScale())))) {
            ExpEnemy* enemy = world_.resources.enemyManager_->GetEnemyPtrs().back();
            enemy->SetSizeScale(event.scale);
            enemy->SetPlayerChase(event.speed * 1.5f * EnemySpeedScale(), event.speed * EnemySpeedScale());
            if (event.exp > 0) {
                enemy->SetExpValue(static_cast<uint32_t>(event.exp));
            }
            midBosses_.push_back({enemy->GetCollisionId(), event.rewards});
            world_.combat.screenEffectDirector_.TriggerBossEntry();
        }
    }
    if (!event.text.empty()) {
        world_.combatFlow->SetEventCallout(event.text, 1.4f);
    }
}

void SurvivorRunController::UpdateSpeedTest(float dt)
{
    // ボスは出さない。自動で撃つは切る。
    config_.exitOpenSeconds = elapsed_ + 5999.0f;
    if (speedTestClock_ <= 0.0f) {
        heroAutoFire_ = false;
    }
    speedTestClock_ += dt;
    auto find = [&](uint64_t id) -> ExpEnemy* {
        for (ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
            if (enemy && !enemy->IsDead() && enemy->GetCollisionId() == id) {
                return enemy;
            }
        }
        return nullptr;
    };
    static constexpr std::array<const char*, 3> kPrefabs{"Square", "Triangle", "Pentagon"};
    static constexpr std::array<const char*, 3> kNames{"四角", "三角", "五角"};
    // 3体そろっていない（倒した）か、一番前が右の端に着いたら、左から並べ直す。
    bool restart = speedTestIds_[0] == 0;
    float front = 0.0f;
    for (size_t i = 0; i < 3 && !restart; ++i) {
        const ExpEnemy* enemy = find(speedTestIds_[i]);
        if (!enemy) {
            restart = true;
        } else {
            front = (std::max)(front, enemy->GetWorldPosition().x);
        }
    }
    // 走り出しは自機の左10で、高さは自機の 上4・下4・下8（自機と同じ高さには置かない。ぶつかって押し合うので）。40進むか右の端に着いたらやり直す。
    if (restart || front > (std::min)(kSpawnMaxX - 3.0f, speedTestStartX_ + 40.0f)) {
        world_.resources.enemyManager_->ClearRunActors();
        const myEngine::Vector3 self = world_.resources.player_->GetWorldPosition();
        speedTestStartX_ = (std::clamp)(self.x - 10.0f, kSpawnMinX + 2.0f, kSpawnMaxX - 20.0f);
        const float centerY = (std::clamp)(self.y, kSpawnMinY + 6.0f, kSpawnMaxY - 6.0f);
        for (size_t i = 0; i < 3; ++i) {
            const myEngine::Vector3 position{speedTestStartX_, centerY + (i == 0 ? 4.0f : i == 1 ? -4.0f : -8.0f), 0.0f};
            speedTestIds_[i] = 0;
            if (!SpawnShape(kPrefabs[i], position, false)) {
                continue;
            }
            ExpEnemy* enemy = world_.resources.enemyManager_->GetEnemyPtrs().back();
            enemy->SetHp(1000000);
            enemy->SetPlayerChase(config_.shapeChase[i].first, config_.shapeChase[i].second);
            enemy->SetChaseDirection({1.0f, 0.0f, 0.0f});
            speedTestIds_[i] = enemy->GetCollisionId();
            speedTestLast_[i] = position;
        }
        ++speedTestRaces_;
        speedTestMoved_.fill(0.0f);
        speedTestTimer_ = 0.0f;
        speedTestLast_[3] = world_.resources.player_->GetWorldPosition();
        return;
    }
    // 動いた距離を足し、1秒ごとに出す。
    for (size_t i = 0; i < 4; ++i) {
        const ExpEnemy* enemy = i < 3 ? find(speedTestIds_[i]) : nullptr;
        const myEngine::Vector3 position = i < 3 ? (enemy ? enemy->GetWorldPosition() : speedTestLast_[i]) : world_.resources.player_->GetWorldPosition();
        speedTestMoved_[i] += myEngine::Length(myEngine::Vector3{position.x - speedTestLast_[i].x, position.y - speedTestLast_[i].y, 0.0f});
        speedTestLast_[i] = position;
    }
    speedTestTimer_ += dt;
    if (speedTestTimer_ < 1.0f) {
        return;
    }
    std::ostringstream text, json;
    text << std::fixed << std::setprecision(1);
    json << std::fixed << std::setprecision(2) << (speedTestLog_.empty() ? "" : ",") << "{\"t\":" << speedTestClock_ << ",\"race\":" << speedTestRaces_;
    static constexpr std::array<const char*, 4> kKeys{"square", "triangle", "pentagon", "player"};
    for (size_t i = 0; i < 4; ++i) {
        const float speed = speedTestMoved_[i] / speedTestTimer_;
        // 「四角 2.6/2.6」のように、1秒の速さ／設定の最高速度。
        text << (i ? "　" : "") << (i < 3 ? kNames[i] : "自機") << " " << speed;
        if (i < 3) {
            text << "/" << config_.shapeChase[i].second;
        }
        json << ",\"" << kKeys[i] << "\":" << speed;
    }
    // 自機の最高速度の内訳：土台の速さ（毎秒）× 移動の倍率。コントローラーを使っているか（G）。
    const auto& player = *world_.resources.player_;
    const float base = player.GetStats().moveSpeed * myEngine::kFixedStepsPerSecond;
    const float scale = player.GetSurvivorMoveScale();
    const bool pad = world_.resources.input_->IsGamepadActive();
    text << "（" << base << "×" << std::setprecision(2) << scale << (pad ? " G" : "") << "）";
    json << ",\"playerBase\":" << base << ",\"playerScale\":" << scale << ",\"gamepad\":" << (pad ? "true" : "false") << "}";
    speedTestLog_ += json.str();
    world_.combatFlow->SetEventCallout(text.str(), 1.2f);
    std::ofstream(std::string(kAutoTestDirectory) + "/speed_test.json") << "[" << speedTestLog_ << "]\n";
    speedTestMoved_.fill(0.0f);
    speedTestTimer_ = 0.0f;
    if (speedTestAutoQuit_ && speedTestRaces_ == 1 && speedTestClock_ >= 6.0f && speedTestClock_ < 7.1f) {
        RequestCapture("speed_test");
    }
    if (speedTestAutoQuit_ && speedTestClock_ >= 20.0f) {
        PostQuitMessage(0);
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
        // 武器の枠を使う遊び方では、群れと包囲の数を減らす（区切りB の感想）。
        const int waveCount = WeaponMode() ? (std::max)(1, static_cast<int>(std::lround(static_cast<float>(wave.count) * config_.stages.swarmScale))) : wave.count;
        const int count = (std::min)(waveCount, config_.telegraph.maxChasers - CountChasers());
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
            if (!world_.resources.stage_->IsCollisionWithAnyBlock(position, 1.2f) && SpawnShape("Triangle", position, true) && ElitesActive() &&
                std::uniform_real_distribution<float>(0.0f, 1.0f)(random_) < config_.elites.erraticRatio) {
                MakeErratic(*world_.resources.enemyManager_->GetEnemyPtrs().back());
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
    // 画面のすぐ外から（計画書 p7 の E5。設定の spawnRing.screenEdge）：上下左右の縁を、縁の長さに比例して選び、縁に沿ってばらける。
    if (config_.ringScreenEdge) {
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);
        // 進む向きで振り分ける（計画書 p8 の G3）：前（±45度）・横（左右の±45〜135度）・後ろ。止まっているときや割合0は今まで通り。
        const float weightSum = config_.spawnForward + config_.spawnSide + config_.spawnBack;
        const float moveSpeed = myEngine::Length(playerVelocity_);
        if (weightSum > 0.0f && moveSpeed > 1.0f) {
            const float heading = std::atan2(playerVelocity_.y, playerVelocity_.x);
            for (int attempt = 0; attempt < 16; ++attempt) {
                const float pick = unit(random_) * weightSum;
                int sector = 2;
                float offset = 3.1415927f + (unit(random_) - 0.5f) * 1.5707964f;
                if (pick < config_.spawnForward) {
                    sector = 0;
                    offset = (unit(random_) - 0.5f) * 1.5707964f;
                } else if (pick < config_.spawnForward + config_.spawnSide) {
                    sector = 1;
                    offset = (unit(random_) < 0.5f ? 1.0f : -1.0f) * (0.7853982f + unit(random_) * 1.5707964f);
                }
                const float a = heading + offset;
                const float dx = std::cos(a), dy = std::sin(a);
                // カメラの中心から、その向きに画面の縁まで伸ばし、少し外へ出す。
                const float toEdge = (std::min)(std::abs(dx) > 0.0001f ? halfWidth / std::abs(dx) : 1.0e6f,
                                                std::abs(dy) > 0.0001f ? halfHeight / std::abs(dy) : 1.0e6f);
                const float out = toEdge + 1.0f + unit(random_) * 2.0f;
                myEngine::Vector3 position{camera.x + dx * out, camera.y + dy * out, 0.0f};
                position.x = (std::clamp)(position.x, kSpawnMinX, kSpawnMaxX);
                position.y = (std::clamp)(position.y, kSpawnMinY, kSpawnMaxY);
                const bool offScreen = std::abs(position.x - camera.x) > halfWidth || std::abs(position.y - camera.y) > halfHeight;
                if (offScreen && !world_.resources.stage_->IsCollisionWithAnyBlock(position, 1.2f)) {
                    ++spawnSectors_[static_cast<size_t>(sector)];
                    return position;
                }
            }
        }
        ++spawnSectors_[3];
        for (int attempt = 0; attempt < 16; ++attempt) {
            const float along = unit(random_) * (halfWidth + halfHeight) * 2.0f;
            const float sideOut = 1.0f + unit(random_) * 2.0f;
            myEngine::Vector3 position{};
            if (along < halfWidth * 2.0f) {
                const float y = unit(random_) < 0.5f ? -(halfHeight + sideOut) : halfHeight + sideOut;
                position = {camera.x - halfWidth + along, camera.y + y, 0.0f};
            } else {
                const float x = unit(random_) < 0.5f ? -(halfWidth + sideOut) : halfWidth + sideOut;
                position = {camera.x + x, camera.y - halfHeight + (along - halfWidth * 2.0f), 0.0f};
            }
            position.x = (std::clamp)(position.x, kSpawnMinX, kSpawnMaxX);
            position.y = (std::clamp)(position.y, kSpawnMinY, kSpawnMaxY);
            const bool offScreen = std::abs(position.x - camera.x) > halfWidth || std::abs(position.y - camera.y) > halfHeight;
            if (offScreen && !world_.resources.stage_->IsCollisionWithAnyBlock(position, 1.2f)) {
                return position;
            }
        }
    }
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
        world_.resources.enemyManager_->GetEnemyPtrs().back()->SetPlayerChase(acceleration * EnemySpeedScale(), maxSpeed * EnemySpeedScale());
    }
    return true;
}

float SurvivorRunController::EnemyHpScale() const
{
    // 武器の枠を使う遊び方では、ステージごとの倍率もかける（W8）。
    const float stage = WeaponMode() ? StageEnemyHp() : 1.0f;
    // 足し続ける形（計画書 p8 の G1）では、経過分ごとに指数で上がる倍率も掛ける。
    return (std::max)(0.1f, (1.0f + config_.threat.enemyHpPerStage * static_cast<float>(threatStage_ - 1)) * stage *
                                config_.growth.HpScale(elapsed_));
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
    // 武器の枠を使う遊び方では、ステージごとに脅威の段階を上乗せする（W8）。
    const int offset = WeaponMode() ? StageThreatOffset() : 0;
    const int stage = (std::clamp)(config_.ThreatStage(elapsed_) + offset, 1, (std::max)(1, config_.threat.maxStage));
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

void SurvivorRunController::UpdateMoveDirection(float dt)
{
    const myEngine::Vector3 position = world_.resources.player_->GetWorldPosition();
    if (hasLastPlayerPosition_) {
        const myEngine::Vector3 step{(position.x - lastPlayerPosition_.x) / dt, (position.y - lastPlayerPosition_.y) / dt, 0.0f};
        // 区画の移りや押し返しの跳びは無視する。0.25秒ほどでならす。
        if (myEngine::Length(step) < 30.0f) {
            const float blend = 1.0f - std::exp(-dt * 4.0f);
            playerVelocity_ = playerVelocity_ + (step - playerVelocity_) * blend;
        }
    }
    lastPlayerPosition_ = position;
    hasLastPlayerPosition_ = true;
}

void SurvivorRunController::SeparateEnemies()
{
    // 重なりの strength 倍を、1回の更新で押し戻す（重いもの＝大きいものほど動きにくい）。壁や柱に入る押し戻しはしない。
    // 半径は当たりの半径 × radiusScale（計画書 p8 の G3：見た目の大きさに合わせて、塊を隙間のある面にする）。
    const float radiusScale = (std::max)(0.1f, config_.separation.radiusScale);
    const float kResolve = (std::clamp)(config_.separation.strength, 0.0f, 1.0f);
    auto cellKey = [](int x, int y) {
        return (static_cast<int64_t>(x) << 32) ^ static_cast<int64_t>(static_cast<uint32_t>(y));
    };
    separationItems_.clear();
    float maxRadius = 0.0f;
    for (ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
        if (!enemy || enemy->IsDead() || enemy->IsRunResource()) {
            continue;
        }
        SeparationItem item;
        item.enemy = enemy;
        item.position = enemy->GetWorldPosition();
        item.radius = enemy->GetRadius() * radiusScale;
        item.mass = item.radius * item.radius;
        maxRadius = (std::max)(maxRadius, item.radius);
        separationItems_.push_back(item);
    }
    // 升の大きさは、一番大きい敵の直径以上（隣の升までで組を取りこぼさない）。中ボスで広がりすぎないよう上限を置く。
    const float kCell = (std::clamp)(maxRadius * 2.0f, 3.2f, 12.0f);
    for (SeparationItem& item : separationItems_) {
        item.cell = cellKey(static_cast<int>(std::floor(item.position.x / kCell)), static_cast<int>(std::floor(item.position.y / kCell)));
    }
    int deepPairs = 0;
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
                    // 押し合いの前の、当たりの半径（広げる前）でも重なっている組を数える（塊の深さの目安）。
                    deepPairs += distance < limit / radiusScale ? 1 : 0;
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
    separationDeepPairs_ += static_cast<uint64_t>(deepPairs);
    separationDeepPairsMax_ = (std::max)(separationDeepPairsMax_, deepPairs);
    const float maxPush = (std::max)(0.0f, config_.separation.maxPush);
    // 壁の判定は、使っている壁だけを1回集めて使い回す（敵ごとに全部の升を調べると、敵の多い場面で重い）。
    separationWalls_.clear();
    for (const auto& line : world_.resources.stage_->GetBlocks()) {
        for (const Block& block : line) {
            if (block.isActive) {
                separationWalls_.push_back(block.obb);
            }
        }
    }
    auto hitsWall = [&](const myEngine::Vector3& at, float radius) {
        for (const myEngine::OBB& wall : separationWalls_) {
            const float dx = at.x - wall.center.x, dy = at.y - wall.center.y;
            const float reach = radius + wall.halfExtents.x + wall.halfExtents.y + wall.halfExtents.z;
            if (dx * dx + dy * dy <= reach * reach && myEngine::CheckSphereVsOBB({at, radius}, wall).hit) {
                return true;
            }
        }
        return false;
    };
    for (SeparationItem& item : separationItems_) {
        if (item.push.x == 0.0f && item.push.y == 0.0f) {
            continue;
        }
        // 1回で動かす量に上限（密集した瞬間に跳んで見えないように）。
        if (maxPush > 0.0f) {
            const float length = myEngine::Length(item.push);
            const float limit = item.radius * maxPush;
            if (length > limit && length > 0.0f) {
                item.push = item.push * (limit / length);
            }
        }
        const myEngine::Vector3 moved = item.position + item.push;
        if (!hitsWall(moved, item.radius)) {
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
    // 機体の遊び方のボス：レベルポイントを入れて、出撃を終える（K3）。
    if (HeroMode()) {
        AddExperience((std::max)(0, config_.waves.boss));
        FinishWave();
        return;
    }
    // 武器の枠を使う遊び方のボス：3択の代わりに、ボスの素材と武器を1つ落とす（W8）。
    if (WeaponMode()) {
        for (int i = 0; i < config_.stages.bossMaterials; ++i) {
            lootField_.Drop(position, {survivor::LootKind::Material, static_cast<int>(survivor::MaterialId::Boss), survivor::Rarity::Common, 1}, random_);
            ++lootDropped_[static_cast<size_t>(survivor::LootKind::Material)];
        }
        if (!config_.weapons.empty()) {
            const int weapon = static_cast<int>(std::uniform_int_distribution<size_t>(0, config_.weapons.size() - 1)(random_));
            lootField_.Drop(position, {survivor::LootKind::Weapon, weapon, survivor::RollRarity(config_.loot.rarityWeights, random_), 1}, random_);
            ++lootDropped_[static_cast<size_t>(survivor::LootKind::Weapon)];
        }
        OpenExit();
        return;
    }
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
