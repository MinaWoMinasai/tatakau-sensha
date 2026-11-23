#include "game/run/session/ExpeditionBossDirector.h"
#include "game/session/GameplaySystems.h"
#include "game/enemy/visual/NeonBossVisual.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <unordered_set>

namespace gameplay {

namespace {
constexpr float kPi = 3.14159265f;
constexpr float kTwoPi = 6.2831853f;
constexpr float kFloorZ = -0.03f;
constexpr float kBossClearance = 2.3f;

const myEngine::Vector4 kBladeColor{2.6f, 0.35f, 0.75f, 1.0f};
const myEngine::Vector4 kWarnColor{2.2f, 0.25f, 0.35f, 1.0f};
const myEngine::Vector4 kGatlingColor{2.4f, 1.3f, 0.3f, 1.0f};
const myEngine::Vector4 kOrbColor{1.5f, 0.45f, 2.3f, 1.0f};
const myEngine::Vector4 kSummonColor{1.2f, 0.35f, 2.1f, 1.0f};

float Distance2D(const myEngine::Vector3& a, const myEngine::Vector3& b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}

float Smooth(float t)
{
    t = (std::clamp)(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float WrapAngle(float angle)
{
    while (angle > kPi) {
        angle -= kTwoPi;
    }
    while (angle < -kPi) {
        angle += kTwoPi;
    }
    return angle;
}

/// @brief current を target へ、最大 maxStep ラジアンだけ回す。
float TurnToward(float current, float target, float maxStep)
{
    const float delta = WrapAngle(target - current);
    return current + (std::clamp)(delta, -maxStep, maxStep);
}

myEngine::Vector3 FromAngle(float angle)
{
    return {std::cos(angle), std::sin(angle), 0.0f};
}

myEngine::Vector4 WithAlpha(myEngine::Vector4 color, float alpha)
{
    color.w = (std::clamp)(alpha, 0.0f, 1.0f);
    return color;
}
} // namespace

void ExpeditionBossDirector::Begin(bool bossRoom)
{
    ClearAll(false);
    summoned_.clear();
    auto* boss = world_.resources.enemy_.get();
    active_ = bossRoom && boss && world_.run.expeditionMapEnabled_ && boss->IsNeonDepthEncounterEnabled();
    if (boss) {
        boss->SetNeonDepthExternalControl(active_);
    }
    if (!active_) {
        return;
    }
    random_.seed(world_.run.expeditionSeed_ * 7919u + 17u);
    phaseTwo_ = false;
    stats_ = {};
    attack_ = lastAttack_ = Attack::None;
    summonCooldown_ = 6.0f;
    serial_ = 1;
    clock_ = 0.0f;
    facing_ = BossFacing();
    BuildPassability();
    EnterStep(Step::Reposition, 1.2f);
}

void ExpeditionBossDirector::ClearAll(bool despawnSummons)
{
    for (const auto& orb : orbs_) {
        myEngine::ParticleManager::GetInstance()->EmitNeonImpactEffect(orb.position, {0, 1, 0}, kOrbColor, 6);
    }
    orbs_.clear();
    summonPoints_.clear();
    if (despawnSummons && world_.resources.enemyManager_) {
        const auto alive = world_.resources.enemyManager_->GetEnemyPtrs();
        for (const auto& summoned : summoned_) {
            if (std::find(alive.begin(), alive.end(), summoned.enemy) != alive.end() && !summoned.enemy->IsDead()) {
                myEngine::ParticleManager::GetInstance()->EmitNeonImpactEffect(summoned.enemy->GetWorldPosition(), {0, 1, 0}, kSummonColor,
                                                                               10);
                summoned.enemy->DespawnWithoutReward();
            }
        }
        summoned_.clear();
    }
}

float ExpeditionBossDirector::BossFacing() const
{
    const auto aim = world_.resources.enemy_->GetAimDirection();
    return std::atan2(aim.y, aim.x);
}

// ---- 床の格子と流れ場 ----

void ExpeditionBossDirector::BuildPassability()
{
    smallGrid_.assign(static_cast<size_t>(kGridW * kGridH), 0);
    bossGrid_.assign(static_cast<size_t>(kGridW * kGridH), 0);
    auto& stage = *world_.resources.stage_;
    for (int y = 0; y < kGridH; ++y) {
        for (int x = 0; x < kGridW; ++x) {
            const myEngine::Vector3 center{kGridMinX + x + 0.5f, kGridMinY + y + 0.5f, 0.0f};
            const size_t index = static_cast<size_t>(y * kGridW + x);
            smallGrid_[index] = stage.IsCollisionWithAnyBlock(center, 0.6f) ? 0 : 1;
            const bool inside = center.x - kBossClearance >= 21.0f && center.x + kBossClearance <= 67.0f &&
                                center.y - kBossClearance >= 15.0f && center.y + kBossClearance <= 43.0f;
            bossGrid_[index] = inside && !stage.IsCollisionWithAnyBlock(center, kBossClearance) ? 1 : 0;
        }
    }
}

bool ExpeditionBossDirector::Passable(const std::vector<uint8_t>& grid, const myEngine::Vector3& position) const
{
    const int x = static_cast<int>(std::floor(position.x - kGridMinX));
    const int y = static_cast<int>(std::floor(position.y - kGridMinY));
    if (x < 0 || y < 0 || x >= kGridW || y >= kGridH || grid.empty()) {
        return false;
    }
    return grid[static_cast<size_t>(y * kGridW + x)] != 0;
}

void ExpeditionBossDirector::BuildFlow(const std::vector<uint8_t>& grid, const myEngine::Vector3& target, std::vector<int>& flow) const
{
    flow.assign(static_cast<size_t>(kGridW * kGridH), -1);
    if (grid.empty()) {
        return;
    }
    int tx = (std::clamp)(static_cast<int>(std::floor(target.x - kGridMinX)), 0, kGridW - 1);
    int ty = (std::clamp)(static_cast<int>(std::floor(target.y - kGridMinY)), 0, kGridH - 1);
    // 目標のマスが通れないとき（壁ぎわ）は、近くの通れるマスから広げる。
    if (!grid[static_cast<size_t>(ty * kGridW + tx)]) {
        bool found = false;
        for (int r = 1; r <= 4 && !found; ++r) {
            for (int dy = -r; dy <= r && !found; ++dy) {
                for (int dx = -r; dx <= r && !found; ++dx) {
                    const int x = tx + dx, y = ty + dy;
                    if (x >= 0 && y >= 0 && x < kGridW && y < kGridH && grid[static_cast<size_t>(y * kGridW + x)]) {
                        tx = x;
                        ty = y;
                        found = true;
                    }
                }
            }
        }
        if (!found) {
            return;
        }
    }
    std::deque<int> open;
    flow[static_cast<size_t>(ty * kGridW + tx)] = 0;
    open.push_back(ty * kGridW + tx);
    static constexpr std::array<std::array<int, 2>, 8> kDirs{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}}};
    while (!open.empty()) {
        const int id = open.front();
        open.pop_front();
        const int cx = id % kGridW, cy = id / kGridW;
        for (const auto& d : kDirs) {
            const int x = cx + d[0], y = cy + d[1];
            if (x < 0 || y < 0 || x >= kGridW || y >= kGridH) {
                continue;
            }
            // 斜めは、縦横の両方が通れるときだけ（角をすり抜けない）。
            if (d[0] && d[1] && (!grid[static_cast<size_t>(cy * kGridW + x)] || !grid[static_cast<size_t>(y * kGridW + cx)])) {
                continue;
            }
            const size_t next = static_cast<size_t>(y * kGridW + x);
            if (!grid[next] || flow[next] >= 0) {
                continue;
            }
            flow[next] = flow[static_cast<size_t>(id)] + 1;
            open.push_back(static_cast<int>(next));
        }
    }
}

bool ExpeditionBossDirector::StraightClear(const std::vector<uint8_t>& grid, const myEngine::Vector3& from,
                                           const myEngine::Vector3& to) const
{
    const float length = Distance2D(from, to);
    const int samples = (std::max)(1, static_cast<int>(length / 0.5f));
    for (int i = 1; i <= samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        if (!Passable(grid, {from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t, 0.0f})) {
            return false;
        }
    }
    return true;
}

myEngine::Vector3 ExpeditionBossDirector::FlowDirection(const std::vector<uint8_t>& grid, const std::vector<int>& flow,
                                                        const myEngine::Vector3& from, const myEngine::Vector3& target) const
{
    myEngine::Vector3 direct{target.x - from.x, target.y - from.y, 0.0f};
    const float length = myEngine::Length(direct);
    if (length < 0.001f) {
        return {};
    }
    direct = direct / length;
    if (flow.empty() || StraightClear(grid, from, target)) {
        return direct;
    }
    const int cx = static_cast<int>(std::floor(from.x - kGridMinX));
    const int cy = static_cast<int>(std::floor(from.y - kGridMinY));
    int best = (std::numeric_limits<int>::max)();
    myEngine::Vector3 bestDirection = direct;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const int x = cx + dx, y = cy + dy;
            if ((!dx && !dy) || x < 0 || y < 0 || x >= kGridW || y >= kGridH) {
                continue;
            }
            const int value = flow[static_cast<size_t>(y * kGridW + x)];
            if (value < 0 || value >= best) {
                continue;
            }
            best = value;
            const myEngine::Vector3 center{kGridMinX + x + 0.5f, kGridMinY + y + 0.5f, 0.0f};
            myEngine::Vector3 toward{center.x - from.x, center.y - from.y, 0.0f};
            if (myEngine::Length(toward) > 0.001f) {
                bestDirection = myEngine::Normalize(toward);
            }
        }
    }
    return bestDirection;
}

// ---- 行動の流れ ----

void ExpeditionBossDirector::EnterStep(Step step, float duration)
{
    step_ = step;
    timer_ = 0.0f;
    duration_ = duration;
    ++serial_;
    if (step != Step::Reposition) {
        return;
    }
    // 位置取り：自機から少し離れた、ボスの体が収まる場所へ移る。
    const auto boss = world_.resources.enemy_->GetWorldPosition();
    const auto player = world_.resources.player_->GetWorldPosition();
    const float base = std::atan2(boss.y - player.y, boss.x - player.x);
    repositionTarget_ = boss;
    for (float offset : {0.0f, 0.5f, -0.5f, 1.0f, -1.0f, 1.6f, -1.6f, kPi}) {
        for (float distance : {11.0f, 9.0f, 13.0f}) {
            const myEngine::Vector3 candidate = player + FromAngle(base + offset) * distance;
            if (Passable(bossGrid_, candidate)) {
                repositionTarget_ = {candidate.x, candidate.y, 0.0f};
                return;
            }
        }
    }
}

void ExpeditionBossDirector::ChooseAttack()
{
    const auto boss = world_.resources.enemy_->GetWorldPosition();
    const auto player = world_.resources.player_->GetWorldPosition();
    const float distance = Distance2D(boss, player);
    int aliveSummons = 0;
    const auto alive = world_.resources.enemyManager_->GetEnemyPtrs();
    for (const auto& summoned : summoned_) {
        aliveSummons += std::find(alive.begin(), alive.end(), summoned.enemy) != alive.end() ? 1 : 0;
    }
    Attack chosen = Attack::None;
    if (distance < 9.0f && lastAttack_ != Attack::Spin) {
        // 近くにいるなら回転切り。
        chosen = Attack::Spin;
    } else {
        std::vector<std::pair<Attack, float>> pool{{Attack::Gatling, 3.0f}, {Attack::Homing, 3.0f}};
        if (summonCooldown_ <= 0.0f && aliveSummons < 3) {
            pool.push_back({Attack::Summon, 4.0f});
        }
        if (distance < 12.0f) {
            pool.push_back({Attack::Spin, 2.0f});
        }
        if (pool.size() > 1) {
            std::erase_if(pool, [&](const auto& item) {
                return item.first == lastAttack_;
            });
        }
        float total = 0.0f;
        for (const auto& item : pool) {
            total += item.second;
        }
        float pick = std::uniform_real_distribution<float>(0.0f, total)(random_);
        chosen = pool.back().first;
        for (const auto& item : pool) {
            if (pick < item.second) {
                chosen = item.first;
                break;
            }
            pick -= item.second;
        }
    }
    attack_ = chosen;
    lastAttack_ = chosen;
    stats_.spins += chosen == Attack::Spin ? 1 : 0;
    stats_.summons += chosen == Attack::Summon ? 1 : 0;
    stats_.gatlings += chosen == Attack::Gatling ? 1 : 0;
    stats_.homings += chosen == Attack::Homing ? 1 : 0;
    const float windupScale = phaseTwo_ ? 0.7f : 1.0f;
    switch (chosen) {
    case Attack::Spin: {
        bladeLength_ = phaseTwo_ ? 7.6f : 7.0f;
        bladeStart_ = std::atan2(player.y - boss.y, player.x - boss.x);
        bladeAngle_ = bladeStart_;
        spinRevolutions_ = phaseTwo_ ? 2.0f : 1.0f;
        spinHitRevolution_ = -1;
        EnterStep(Step::Windup, 1.1f * windupScale);
        break;
    }
    case Attack::Summon: {
        summonPoints_.clear();
        const int count = 7;
        std::uniform_real_distribution<float> angle(0.0f, kTwoPi), radius(5.0f, 10.0f);
        for (int attempt = 0; attempt < 120 && static_cast<int>(summonPoints_.size()) < count; ++attempt) {
            const myEngine::Vector3 candidate = attempt < 80
                                                    ? player + FromAngle(angle(random_)) * radius(random_)
                                                    : myEngine::Vector3{std::uniform_real_distribution<float>(24.0f, 64.0f)(random_),
                                                                        std::uniform_real_distribution<float>(18.0f, 40.0f)(random_), 0.0f};
            if (!Passable(smallGrid_, candidate) || world_.resources.stage_->IsCollisionWithAnyBlock(candidate, 1.3f) ||
                Distance2D(candidate, player) < 4.5f) {
                continue;
            }
            bool spaced = true;
            for (const auto& point : summonPoints_) {
                spaced &= Distance2D(point, candidate) >= 2.6f;
            }
            if (spaced) {
                summonPoints_.push_back({candidate.x, candidate.y, 0.0f});
            }
        }
        EnterStep(Step::Windup, 1.3f * windupScale);
        break;
    }
    case Attack::Gatling:
        facing_ = BossFacing();
        EnterStep(Step::Windup, 1.0f * windupScale);
        break;
    case Attack::Homing:
        EnterStep(Step::Windup, 0.9f * windupScale);
        break;
    default:
        EnterStep(Step::Reposition, 1.0f);
        break;
    }
}

void ExpeditionBossDirector::MoveBoss(const myEngine::Vector3& direction, float speed, float dt)
{
    auto& boss = *world_.resources.enemy_;
    const myEngine::Vector3 position = boss.GetWorldPosition();
    const myEngine::Vector3 step = direction * (speed * dt);
    if (boss.MoveNeonDepthBossTo({position.x + step.x, position.y + step.y, 0.0f})) {
        return;
    }
    // 壁に当たったら、縦か横だけでも進む。
    if (!boss.MoveNeonDepthBossTo({position.x + step.x, position.y, 0.0f})) {
        boss.MoveNeonDepthBossTo({position.x, position.y + step.y, 0.0f});
    }
}

void ExpeditionBossDirector::Update(float dt)
{
    if (!active_ || dt <= 0.0f) {
        return;
    }
    auto* boss = world_.resources.enemy_.get();
    auto& player = *world_.resources.player_;
    if (!boss || !boss->IsNeonDepthEncounterEnabled() || boss->IsDead() || boss->GetHp() <= 0) {
        ClearAll(true);
        active_ = false;
        if (boss) {
            boss->SetNeonDepthExternalControl(false);
        }
        return;
    }
    clock_ += dt;
    UpdateSummoned(dt);
    UpdateOrbs(dt);
    const auto& snapshot = boss->GetNeonDepthSnapshot();
    if (snapshot.phase == neondepth::Phase::Intro || player.IsDead()) {
        return;
    }
    summonCooldown_ = (std::max)(0.0f, summonCooldown_ - dt);

    // 第2形態：HPが半分を切ったら、速く・多く・2回転になる。
    if (!phaseTwo_ && boss->GetHp() * 2 <= boss->GetMaxHp()) {
        phaseTwo_ = true;
        world_.combat.screenEffectDirector_.TriggerBossPhaseChange();
        myEngine::ParticleManager::GetInstance()->EmitNeonImpactEffect(boss->GetWorldPosition(), {0, 1, 0}, kWarnColor, 40);
        world_.presentation.cameraShakeDuration_ = world_.presentation.cameraShakeTimer_ = 0.4f;
        world_.presentation.cameraShakePower_ = 0.25f;
        attack_ = Attack::None;
        summonPoints_.clear();
        EnterStep(Step::Recovery, 1.0f);
    }

    timer_ += dt;
    const auto bossPosition = boss->GetWorldPosition();
    const auto playerPosition = player.GetWorldPosition();
    const float toPlayer = std::atan2(playerPosition.y - bossPosition.y, playerPosition.x - bossPosition.x);
    switch (step_) {
    case Step::Reposition: {
        BuildFlow(bossGrid_, repositionTarget_, bossFlow_);
        if (Distance2D(bossPosition, repositionTarget_) > 0.8f) {
            MoveBoss(FlowDirection(bossGrid_, bossFlow_, bossPosition, repositionTarget_), phaseTwo_ ? 9.0f : 7.0f, dt);
        }
        facing_ = TurnToward(facing_, toPlayer, 4.0f * dt);
        boss->SetBossFacing(facing_);
        if (timer_ >= duration_) {
            ChooseAttack();
        }
        break;
    }
    case Step::Windup:
    case Step::Attack:
        if (attack_ == Attack::Spin) {
            UpdateSpin(dt);
        } else if (attack_ == Attack::Gatling) {
            UpdateGatling(dt);
        } else if (attack_ == Attack::Homing) {
            UpdateHoming(dt);
        } else if (attack_ == Attack::Summon) {
            UpdateSummon();
        } else {
            EnterStep(Step::Recovery, 0.6f);
        }
        break;
    case Step::Recovery:
        facing_ = TurnToward(facing_, toPlayer, 2.0f * dt);
        boss->SetBossFacing(facing_);
        if (timer_ >= duration_) {
            attack_ = Attack::None;
            EnterStep(Step::Reposition, phaseTwo_ ? 0.8f : 1.1f);
        }
        break;
    }
}

void ExpeditionBossDirector::UpdateSpin(float dt)
{
    (void)dt;
    auto& boss = *world_.resources.enemy_;
    auto& player = *world_.resources.player_;
    const auto bossPosition = boss.GetWorldPosition();
    if (step_ == Step::Windup) {
        // 予備動作：刃を自機へ向けてから、後ろへ引き絞る。
        const auto playerPosition = player.GetWorldPosition();
        const float target = std::atan2(playerPosition.y - bossPosition.y, playerPosition.x - bossPosition.x);
        bladeStart_ = TurnToward(bladeStart_, target, 2.5f * dt);
        bladeAngle_ = bladeStart_ - 0.9f * Smooth(timer_ / duration_);
        boss.SetBossFacing(bladeAngle_);
        // 引き絞りながら自機へ詰め寄る（刃の届く範囲の少し内側まで）。
        if (Distance2D(bossPosition, playerPosition) > bladeLength_ * 0.6f) {
            MoveBoss(FromAngle(target), phaseTwo_ ? 5.0f : 3.5f, dt);
        }
        if (timer_ >= duration_) {
            bladeStart_ = bladeAngle_;
            EnterStep(Step::Attack, spinRevolutions_ * (phaseTwo_ ? 0.85f : 1.0f));
            myEngine::ParticleManager::GetInstance()->EmitNeonImpactEffect(bossPosition, {0, 1, 0}, kBladeColor, 16);
        }
        return;
    }
    // 回転：1秒で1回転（第2形態は少し速く2回転）。刃が通った角度に自機がいれば当たる。
    const float previous = bladeAngle_;
    const float progress = (std::min)(1.0f, timer_ / duration_);
    bladeAngle_ = bladeStart_ + kTwoPi * spinRevolutions_ * progress;
    boss.SetBossFacing(bladeAngle_);
    const auto playerPosition = player.GetWorldPosition();
    const float distance = Distance2D(bossPosition, playerPosition);
    const int revolution = static_cast<int>((bladeAngle_ - bladeStart_) / kTwoPi);
    if (distance <= bladeLength_ + player.GetRadius() && distance > 0.2f && revolution != spinHitRevolution_) {
        const float playerAngle = std::atan2(playerPosition.y - bossPosition.y, playerPosition.x - bossPosition.x);
        float relative = std::fmod(playerAngle - previous, kTwoPi);
        if (relative < 0.0f) {
            relative += kTwoPi;
        }
        // 当たり判定に自機の大きさの分だけ角度の余白を足す。
        const float margin = std::atan2(player.GetRadius(), (std::max)(distance, 0.5f));
        if (relative <= (bladeAngle_ - previous) + margin || relative >= kTwoPi - margin) {
            ++stats_.spinContacts;
            if (player.ReceiveHostileHazard(phaseTwo_ ? 20u : 16u, true) > 0) {
                spinHitRevolution_ = revolution;
                ++stats_.spinHits;
                myEngine::ParticleManager::GetInstance()->EmitNeonImpactEffect(playerPosition, {0, 1, 0}, kBladeColor, 14);
            }
        }
    }
    if (timer_ >= duration_) {
        EnterStep(Step::Recovery, phaseTwo_ ? 0.7f : 1.0f);
    }
}

void ExpeditionBossDirector::UpdateGatling(float dt)
{
    auto& boss = *world_.resources.enemy_;
    const auto bossPosition = boss.GetWorldPosition();
    const auto playerPosition = world_.resources.player_->GetWorldPosition();
    const float toPlayer = std::atan2(playerPosition.y - bossPosition.y, playerPosition.x - bossPosition.x);
    // 自機の方を向き続ける（回る速さに上限があるので、走れば振り切れる）。
    facing_ = TurnToward(facing_, toPlayer, (step_ == Step::Windup ? 3.0f : (phaseTwo_ ? 2.6f : 1.9f)) * dt);
    boss.SetBossFacing(facing_);
    if (step_ == Step::Windup) {
        if (timer_ >= duration_) {
            fireTimer_ = 0.0f;
            EnterStep(Step::Attack, phaseTwo_ ? 6.0f : 5.0f);
        }
        return;
    }
    // 間に壁があるときは、撃ちながら射線の通る所へゆっくり回り込む。
    if (!boss.HasLineOfSightToPlayer()) {
        BuildFlow(bossGrid_, playerPosition, bossFlow_);
        MoveBoss(FlowDirection(bossGrid_, bossFlow_, bossPosition, playerPosition), 2.5f, dt);
    }
    fireTimer_ -= dt;
    const float interval = phaseTwo_ ? 0.065f : 0.09f;
    std::uniform_real_distribution<float> spread(-0.07f, 0.07f);
    while (fireTimer_ <= 0.0f) {
        fireTimer_ += interval;
        const float angle = facing_ + spread(random_);
        boss.FireBossBullet(FromAngle(angle), phaseTwo_ ? 0.27f : 0.24f, phaseTwo_ ? 4u : 3u);
        ++stats_.bullets;
    }
    if (timer_ >= duration_) {
        EnterStep(Step::Recovery, phaseTwo_ ? 0.7f : 1.0f);
    }
}

void ExpeditionBossDirector::UpdateHoming(float dt)
{
    (void)dt;
    auto& boss = *world_.resources.enemy_;
    const auto bossPosition = boss.GetWorldPosition();
    const auto playerPosition = world_.resources.player_->GetWorldPosition();
    facing_ = TurnToward(facing_, std::atan2(playerPosition.y - bossPosition.y, playerPosition.x - bossPosition.x), 3.0f * dt);
    boss.SetBossFacing(facing_);
    const int total = phaseTwo_ ? 7 : 5;
    const float interval = phaseTwo_ ? 0.7f : 1.0f;
    if (step_ == Step::Windup) {
        if (timer_ >= duration_) {
            orbsFired_ = 0;
            fireTimer_ = 0.0f;
            EnterStep(Step::Attack, interval * static_cast<float>(total));
        }
        return;
    }
    // 動かずに、一定の間隔で追尾弾を放つ。
    fireTimer_ -= dt;
    if (fireTimer_ <= 0.0f && orbsFired_ < total) {
        fireTimer_ += interval;
        FireOrb();
        ++stats_.orbs;
        ++orbsFired_;
        ++serial_;
    }
    if (timer_ >= duration_) {
        EnterStep(Step::Recovery, 0.8f);
    }
}

void ExpeditionBossDirector::FireOrb()
{
    const auto bossPosition = world_.resources.enemy_->GetWorldPosition();
    const float side = (orbsFired_ % 2 == 0 ? 1.0f : -1.0f) * (0.5f + 0.15f * static_cast<float>(orbsFired_ % 3));
    Orb orb;
    const float angle = facing_ + side;
    orb.position = bossPosition + FromAngle(angle) * 2.4f;
    orb.position.z = 0.0f;
    orb.velocity = FromAngle(angle) * (phaseTwo_ ? 5.5f : 4.5f);
    orb.life = 10.0f;
    orb.trail.fill(orb.position);
    orbs_.push_back(orb);
    myEngine::ParticleManager::GetInstance()->EmitNeonImpactEffect(orb.position, {0, 1, 0}, kOrbColor, 8);
}

void ExpeditionBossDirector::UpdateOrbs(float dt)
{
    if (orbs_.empty()) {
        return;
    }
    auto& player = *world_.resources.player_;
    const auto playerPosition = player.GetWorldPosition();
    BuildFlow(smallGrid_, playerPosition, playerFlow_);
    const float speed = phaseTwo_ ? 5.5f : 4.5f;
    const float turn = (phaseTwo_ ? 2.0f : 1.6f) * dt;
    for (auto& orb : orbs_) {
        orb.life -= dt;
        // 壁を避ける向き（流れ場）へ、曲がる速さの上限つきで向きを変える。精度はそこそこ。
        const myEngine::Vector3 desired = FlowDirection(smallGrid_, playerFlow_, orb.position, playerPosition);
        const float current = std::atan2(orb.velocity.y, orb.velocity.x);
        const float wanted = myEngine::Length(desired) > 0.001f ? std::atan2(desired.y, desired.x) : current;
        orb.velocity = FromAngle(TurnToward(current, wanted, turn)) * speed;
        orb.position = orb.position + orb.velocity * dt;
        orb.trailTimer -= dt;
        if (orb.trailTimer <= 0.0f) {
            orb.trailTimer = 0.05f;
            std::rotate(orb.trail.rbegin(), orb.trail.rbegin() + 1, orb.trail.rend());
            orb.trail[0] = orb.position;
        }
    }
    std::erase_if(orbs_, [&](const Orb& orb) {
        if (orb.life <= 0.0f) {
            myEngine::ParticleManager::GetInstance()->EmitNeonImpactEffect(orb.position, {0, 1, 0}, kOrbColor, 6);
            return true;
        }
        if (world_.resources.stage_->IsCollisionWithAnyBlock(orb.position, 0.45f) || orb.position.x < 21.0f || orb.position.x > 67.0f ||
            orb.position.y < 15.0f || orb.position.y > 43.0f) {
            // 壁に当たったら消える。
            ++stats_.orbWalls;
            myEngine::ParticleManager::GetInstance()->EmitNeonImpactEffect(orb.position, {0, 1, 0}, kOrbColor, 10);
            return true;
        }
        if (!player.IsDead() && Distance2D(orb.position, playerPosition) <= 0.6f + player.GetRadius()) {
            player.ReceiveHostileHazard(phaseTwo_ ? 8u : 7u, true);
            ++stats_.orbHits;
            myEngine::ParticleManager::GetInstance()->EmitNeonImpactEffect(orb.position, {0, 1, 0}, kOrbColor, 14);
            return true;
        }
        return false;
    });
}

void ExpeditionBossDirector::UpdateSummon()
{
    if (step_ == Step::Windup) {
        if (timer_ >= duration_) {
            // 召喚：予告の輪の場所に、種類ランダムの雑魚をワープで出す。15秒で自動的に倒れる。
            static const std::array<const char*, 6> kTypes{"Charger", "Skirmisher", "Sniper", "Flanker", "Suppressor", "Triangle"};
            static const std::array<const char*, 8> kTypesTwo{"Charger",    "Skirmisher", "Sniper",      "Flanker",
                                                              "Suppressor", "Triangle",   "ShieldGuard", "BladeGuard"};
            auto& enemies = *world_.resources.enemyManager_;
            for (const auto& point : summonPoints_) {
                const char* type = phaseTwo_ ? kTypesTwo[std::uniform_int_distribution<size_t>(0, kTypesTwo.size() - 1)(random_)]
                                             : kTypes[std::uniform_int_distribution<size_t>(0, kTypes.size() - 1)(random_)];
                if (!enemies.SpawnLevelEnemy(point, type, -1)) {
                    continue;
                }
                ExpEnemy* enemy = enemies.GetEnemyPtrs().back();
                enemy->SetWarpDissolve(1.12f);
                // ボスの手下は、ふつうの同じ敵の半分のHPにする。
                enemy->SetHp((std::max)(1, enemy->GetMaxHp() / 2));
                if (std::string(type) == "Triangle") {
                    enemy->SetHp(9);
                    enemy->SetBaseColor({2.0f, 0.42f, 0.12f, 1.0f});
                    enemy->SetPlayerChase(7.0f, 7.5f);
                    enemy->SetWobble(0.55f, 3.2f, std::uniform_real_distribution<float>(0.0f, kTwoPi)(random_));
                }
                summoned_.push_back({enemy, 15.0f});
                ++stats_.summoned;
                myEngine::ParticleManager::GetInstance()->EmitNeonImpactEffect(point, {0, 1, 0}, kSummonColor, 12);
            }
            summonPoints_.clear();
            summonCooldown_ = 15.0f;
            world_.presentation.cameraShakeDuration_ = world_.presentation.cameraShakeTimer_ = 0.2f;
            world_.presentation.cameraShakePower_ = 0.12f;
            EnterStep(Step::Attack, 0.7f);
        }
        return;
    }
    if (timer_ >= duration_) {
        EnterStep(Step::Recovery, 0.8f);
    }
}

void ExpeditionBossDirector::UpdateSummoned(float dt)
{
    if (summoned_.empty()) {
        return;
    }
    const auto alive = world_.resources.enemyManager_->GetEnemyPtrs();
    std::erase_if(summoned_, [&](Summoned& summoned) {
        // 倒された（もう一覧にいない）雑魚は外す。生きている間だけ中身に触る。
        if (std::find(alive.begin(), alive.end(), summoned.enemy) == alive.end() || summoned.enemy->IsDead()) {
            return true;
        }
        summoned.life -= dt;
        if (summoned.life > 0.0f) {
            return false;
        }
        myEngine::ParticleManager::GetInstance()->EmitNeonImpactEffect(summoned.enemy->GetWorldPosition(), {0, 1, 0}, kSummonColor, 10);
        summoned.enemy->DespawnWithoutReward();
        ++stats_.summonExpired;
        return true;
    });
}

std::string ExpeditionBossDirector::DebugStateName() const
{
    if (!active_) {
        return {};
    }
    const char* attack = attack_ == Attack::Spin      ? "spin"
                         : attack_ == Attack::Summon  ? "summon"
                         : attack_ == Attack::Gatling ? "gatling"
                         : attack_ == Attack::Homing  ? "homing"
                                                      : "none";
    const char* step = step_ == Step::Reposition ? "move"
                       : step_ == Step::Windup   ? "windup"
                       : step_ == Step::Attack   ? "attack"
                                                 : "recovery";
    return std::string(phaseTwo_ ? "p2_" : "") + attack + "_" + step;
}

// ---- 見た目 ----

bool ExpeditionBossDirector::OverrideVisual(neondepth::Snapshot& depth) const
{
    if (!active_ || depth.phase == neondepth::Phase::Intro || depth.phase == neondepth::Phase::Defeated ||
        depth.phase == neondepth::Phase::Aborted) {
        return false;
    }
    using P = neondepth::Phase;
    using A = neondepth::Attack;
    const float progress = duration_ > 0.0f ? (std::clamp)(timer_ / duration_, 0.0f, 1.0f) : 0.0f;
    depth.plan.phaseTwo = phaseTwo_;
    depth.plan.instance = serial_;
    depth.elapsed = timer_;
    depth.duration = duration_;
    depth.progress = progress;
    const A pose = attack_ == Attack::Spin ? A::Beam : attack_ == Attack::Summon ? A::Dive : A::Volley;
    depth.plan.attack = pose;
    switch (step_) {
    case Step::Reposition:
        depth.phase = P::Reposition;
        break;
    case Step::Windup:
        depth.phase = P::Telegraph;
        break;
    case Step::Attack:
        if (attack_ == Attack::Spin) {
            depth.phase = P::Active;
        } else if (attack_ == Attack::Summon) {
            depth.phase = P::Active;
        } else if (attack_ == Attack::Homing) {
            // 1発ごとに投げる動き。
            const float interval = phaseTwo_ ? 0.7f : 1.0f;
            depth.phase = P::Airborne;
            depth.duration = interval;
            depth.elapsed = std::fmod(timer_, interval);
            depth.progress = depth.elapsed / interval;
        } else {
            depth.phase = P::Locked;
        }
        break;
    case Step::Recovery:
        depth.phase = attack_ == Attack::None ? P::Reposition : P::Recovery;
        break;
    }
    return true;
}

void ExpeditionBossDirector::QueueFloorCircle(const myEngine::Vector3& center, float radius, float width, const myEngine::Vector4& color,
                                              int segments)
{
    auto& renderer = *world_.resources.neonGridRenderer_;
    myEngine::Vector3 previous{center.x + radius, center.y, kFloorZ};
    for (int i = 1; i <= segments; ++i) {
        const float a = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
        const myEngine::Vector3 current{center.x + std::cos(a) * radius, center.y + std::sin(a) * radius, kFloorZ};
        renderer.QueueLine(previous, current, width, color);
        previous = current;
    }
}

void ExpeditionBossDirector::QueueVisuals(const myEngine::Vector3& cameraRight, const myEngine::Vector3& cameraUp)
{
    if (!world_.resources.neonGridRenderer_) {
        return;
    }
    auto& renderer = *world_.resources.neonGridRenderer_;
    // 追尾弾は、ボスがいなくなっても残りを描く。
    for (const auto& orb : orbs_) {
        const float pulse = 0.85f + 0.15f * std::sin(clock_ * 12.0f + orb.life);
        const float fade = (std::min)(1.0f, orb.life / 0.6f);
        constexpr int kSides = 10;
        for (int ring = 0; ring < 2; ++ring) {
            const float radius = (ring == 0 ? 0.6f : 0.32f) * pulse;
            myEngine::Vector3 previous = orb.position + cameraRight * radius;
            for (int i = 1; i <= kSides; ++i) {
                const float a = kTwoPi * static_cast<float>(i) / static_cast<float>(kSides) + clock_ * (ring ? -3.0f : 2.0f);
                const myEngine::Vector3 current = orb.position + (cameraRight * std::cos(a) + cameraUp * std::sin(a)) * radius;
                renderer.QueueLine(previous, current, ring == 0 ? 0.08f : 0.06f, WithAlpha(kOrbColor, fade));
                previous = current;
            }
        }
        for (size_t i = 1; i < orb.trail.size(); ++i) {
            const float alpha = fade * (1.0f - static_cast<float>(i) / static_cast<float>(orb.trail.size())) * 0.7f;
            renderer.QueueLine(orb.trail[i - 1], orb.trail[i], 0.12f, WithAlpha(kOrbColor, alpha));
        }
    }
    if (!active_ || !world_.resources.enemy_ || world_.resources.enemy_->IsDead()) {
        return;
    }
    const auto boss = world_.resources.enemy_->GetWorldPosition();
    const myEngine::Vector3 core{boss.x, boss.y, kFloorZ};
    const float progress = duration_ > 0.0f ? (std::clamp)(timer_ / duration_, 0.0f, 1.0f) : 0.0f;

    if (attack_ == Attack::Spin && (step_ == Step::Windup || step_ == Step::Attack)) {
        const bool windup = step_ == Step::Windup;
        // 届く範囲の輪。予備動作の間に濃くなり、進みの弧が満ちていく。
        QueueFloorCircle(core, bladeLength_, 0.10f, WithAlpha(kWarnColor, windup ? 0.25f + 0.55f * progress : 0.3f));
        if (windup) {
            const int lit = static_cast<int>(48.0f * progress);
            for (int i = 0; i < lit; ++i) {
                const float a0 = bladeStart_ + kTwoPi * static_cast<float>(i) / 48.0f;
                const float a1 = bladeStart_ + kTwoPi * static_cast<float>(i + 1) / 48.0f;
                renderer.QueueLine(core + FromAngle(a0) * (bladeLength_ - 0.35f), core + FromAngle(a1) * (bladeLength_ - 0.35f), 0.18f,
                                   WithAlpha(kWarnColor, 0.9f));
            }
        }
        // 刃（ライトセーバー）：床の上の光る刃と、ボスの手から刃先への光。回転中は残像を引く。
        const float flicker = windup ? 0.55f + 0.45f * std::sin(clock_ * 40.0f) * progress + 0.3f * progress : 1.0f;
        const int trails = windup ? 1 : 12;
        for (int i = trails - 1; i >= 0; --i) {
            const float angle = bladeAngle_ - static_cast<float>(i) * 0.07f;
            const float alpha = (i == 0 ? 1.0f : 0.55f * (1.0f - static_cast<float>(i) / static_cast<float>(trails))) * flicker;
            const myEngine::Vector3 tip = core + FromAngle(angle) * bladeLength_;
            const myEngine::Vector3 root = core + FromAngle(angle) * 1.2f;
            renderer.QueueLine(root, tip, i == 0 ? 0.55f : 0.35f, WithAlpha({1.6f, 0.2f, 0.5f, 1.0f}, alpha * 0.45f));
            renderer.QueueLine(root, tip, i == 0 ? 0.18f : 0.12f, WithAlpha(kBladeColor, alpha));
        }
        myEngine::Vector3 hand{};
        if (world_.resources.neonBossVisual_ && world_.resources.neonBossVisual_->GetDepthSocketWorld(neondepth::Socket::RightHand, hand)) {
            const myEngine::Vector3 tip = core + FromAngle(bladeAngle_) * bladeLength_;
            renderer.QueueLine(hand, tip, 0.22f, WithAlpha(kBladeColor, 0.8f * flicker));
        }
    }
    if (attack_ == Attack::Gatling && (step_ == Step::Windup || step_ == Step::Attack)) {
        // 照準線：予備動作の間に濃くなる点線。撃っている間は薄く残し、銃口が瞬く。
        const myEngine::Vector3 forward = FromAngle(facing_);
        const float alpha = step_ == Step::Windup ? 0.2f + 0.7f * progress : 0.25f;
        for (int i = 0; i < 16; ++i) {
            const float from = 2.0f + static_cast<float>(i) * 1.6f;
            renderer.QueueLine(core + forward * from, core + forward * (from + 0.9f), 0.08f, WithAlpha(kGatlingColor, alpha));
        }
        const float muzzle = step_ == Step::Windup ? 0.4f + 0.8f * progress : 0.6f + 0.4f * std::sin(clock_ * 50.0f);
        QueueFloorCircle(core + forward * 1.9f, muzzle, 0.07f, WithAlpha(kGatlingColor, 0.9f), 12);
    }
    if (attack_ == Attack::Homing && step_ == Step::Windup) {
        // 予備動作：紫の玉がボスのまわりを回りながら育つ。
        for (int i = 0; i < 3; ++i) {
            const float a = clock_ * 3.0f + kTwoPi * static_cast<float>(i) / 3.0f;
            QueueFloorCircle(core + FromAngle(a) * 2.6f, 0.2f + 0.4f * progress, 0.08f, WithAlpha(kOrbColor, 0.5f + 0.5f * progress), 10);
        }
    }
    if (attack_ == Attack::Summon && step_ == Step::Windup) {
        // 予備動作：出てくる場所に、縮んでいく紫の輪。
        for (const auto& point : summonPoints_) {
            const myEngine::Vector3 center{point.x, point.y, kFloorZ};
            QueueFloorCircle(center, 3.0f - 1.6f * progress, 0.08f, WithAlpha(kSummonColor, 0.35f + 0.6f * progress), 24);
            QueueFloorCircle(center, 1.3f, 0.05f, WithAlpha(kSummonColor, 0.5f * progress), 20);
            renderer.QueueLine(core, center, 0.04f, WithAlpha(kSummonColor, 0.25f * progress));
        }
    }
}
} // namespace gameplay
