#include "game/run/session/SurvivorRunController.h"
#include "game/debug/MenuFlow.h"
#include "game/settings/UserSettings.h"
#include "game/render/settings/ArenaCameraProfile.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/session/GameplaySystems.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

// 倒れる自動確認と、人が遊んだ記録（計画書 p11 の B2c）。
// MYENGINE_SURVIVOR_AUTOTEST=1 と MYENGINE_SURVIVOR_MORTAL=1 で、ダメージを受ける自機が、敵から離れながら撃ち、経験値を拾う。
// 倒れるか10分まで生きたら autotest.json に書いて終わる。画面は保存しない（何本も並べて回すため）。

namespace gameplay {

using namespace survivorrun;

namespace {
constexpr float kPi = 3.14159265f;
constexpr float kMortalEndSeconds = 600.0f;
constexpr float kSampleSeconds = 30.0f;
// 人の遅れ：0.2秒ごとに見て決め、決めた動きは次に見たときから始める。狙いは少しずれ、向きを変えるのに時間がかかる。
constexpr float kThinkSeconds = 0.2f;
constexpr float kAimErrorRadians = 0.07f;
constexpr float kAimTurnPerSecond = 9.0f;
constexpr int kDirections = 16;
// 先読み：始めの向き × 途中で曲がる向き（-60度・まっすぐ・+60度）の道を、この刻みで進めてくらべる。
constexpr float kStepSeconds = 0.2f;
constexpr int kSteps = 12, kTurnStep = 4;
constexpr float kTurnAngle = kPi / 3.0f;
// 部屋の端に近いと、逃げ場が減るので避ける。
constexpr float kWallMargin = 15.0f;
constexpr float kWallWeight = 1.6f;
constexpr float kShardRange = 11.0f;
constexpr float kHealRange = 16.0f;

/// @brief 2乗ではなく、触れそうな近さだけを強く嫌う。
float Danger(float gap)
{
    const float close = 1.0f / (0.45f + (std::max)(0.0f, gap));
    return close * close;
}
} // namespace

void SurvivorRunController::StartMortalTest(uint32_t seed)
{
    char value[4]{};
    autoMortal_ = autoTest_ == AutoTest::Sector && GetEnvironmentVariableA("MYENGINE_SURVIVOR_MORTAL", value, sizeof(value)) > 0 && value[0] == '1';
    if (!autoMortal_) {
        return;
    }
    world_.presentation.debugPlayerNoDamage_ = false;
    world_.resources.player_->SetDebugNoDamage(false);
    char pick[16]{};
    autoMortalNoMain_ = GetEnvironmentVariableA("MYENGINE_SURVIVOR_MORTAL_PICK", pick, sizeof(pick)) > 0 && std::string(pick) == "nomain";
    mortal_ = {};
    mortal_.seed = seed;
    // 動きの乱数は、湧きの乱数と分ける（動き方を変えても、湧きの並びの始まりは同じになる）。
    mortal_.random.seed(seed * 2654435761u + 17u);
    mortal_.start = std::chrono::steady_clock::now();
}

void SurvivorRunController::UpdateMortalAutoTest(float dt)
{
    auto& player = *world_.resources.player_;
    MortalTest& m = mortal_;
    m.noDamageSeen = m.noDamageSeen || player.IsDebugNoDamage();
    // 当たった回数と量を、相手ごとに足す（同じ更新で2つ以上当たったときは、最後の相手に入れる）。
    if (player.GetSurvivorHits() != m.hits) {
        auto& by = m.by[player.GetSurvivorLastHit()];
        by.first += player.GetSurvivorHits() - m.hits;
        by.second += player.GetSurvivorDamageTaken() - m.damage;
        m.hits = player.GetSurvivorHits();
        m.damage = player.GetSurvivorDamageTaken();
        if (m.firstHitAt < 0.0f) {
            m.firstHitAt = elapsed_;
        }
    }
    const AliveCounts alive = CountAlive();
    m.maxAlive = (std::max)(m.maxAlive, alive.total);
    if (elapsed_ >= m.nextSample) {
        m.samples += (m.samples.empty() ? "" : ",") + ("{\"t\":" + std::to_string(static_cast<int>(std::lround(m.nextSample)))) +
                     ",\"alive\":" + std::to_string(alive.total) + ",\"level\":" + std::to_string(level_) + ",\"hp\":" + std::to_string(player.GetHp()) +
                     ",\"wound\":" + std::to_string(player.GetSurvivorWound()) + ",\"hits\":" + std::to_string(m.hits) +
                     ",\"damage\":" + std::to_string(m.damage) + ",\"main\":" + std::to_string(UpgradeTier(0)) + ",\"kills\":" + std::to_string(world_.combat.defeatedEnemies_) + "}";
        m.nextSample += kSampleSeconds;
    }
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - m.start).count();
    if (player.IsDead() || elapsed_ >= kMortalEndSeconds || wall > 2400.0) {
        FinishMortalTest(player.IsDead() ? "dead" : elapsed_ >= kMortalEndSeconds ? "time" : "timeout", wall);
        return;
    }
    // 強化の画面：少し読んでから選ぶ。主武器 → 副武器 → ほか、の順（今までの自動確認と同じ選び方）。
    if (heroLevelUpOpen_) {
        if (!LevelUpScreenOn() || levelUpScreen_.IsClosing() || levelUpScreen_.GetAge() < 0.5f) {
            return;
        }
        size_t pick = 0;
        int best = -2;
        for (size_t i = 0; i < heroLevelUpOffers_.size(); ++i) {
            const ShopOffer& offer = heroLevelUpOffers_[i];
            const int score = offer.upgrade && offer.index == 0 ? (autoMortalNoMain_ ? -1 : 2) : offer.sub ? 1 : 0;
            if (score > best) {
                best = score;
                pick = i;
            }
        }
        levelUpScreen_.Pick(pick);
        return;
    }
    if (IsPaused()) {
        return;
    }
    const myEngine::Vector3 self = player.GetWorldPosition();
    m.think -= dt;
    if (m.think <= 0.0f) {
        m.think += kThinkSeconds;
        m.move = m.nextMove;
        m.wantedAim = m.nextAim;
        ThinkMortal(self);
    }
    // 照準は、決めた向きへ少しずつ回す。
    const float turn = std::remainder(m.wantedAim - m.aim, 2.0f * kPi);
    m.aim += (std::clamp)(turn, -kAimTurnPerSecond * dt, kAimTurnPerSecond * dt);
    player.SetDemoInput(true, m.move, self + myEngine::Vector3{std::cos(m.aim) * 8.0f, std::sin(m.aim) * 8.0f, 0.0f}, true, false);
    autoBarrierWanted_ = false;
}

void SurvivorRunController::ThinkMortal(const myEngine::Vector3& self)
{
    MortalTest& m = mortal_;
    auto& player = *world_.resources.player_;
    const float radius = player.GetRadius();
    const float speed = config_.heroes.walkSpeed;
    const float halfHeight = arenacamera::HalfViewHeight();
    const float halfWidth = halfHeight * arenacamera::kAspect;
    const myEngine::Vector3 camera = world_.resources.camera->GetTranslate();
    // 画面に見えている敵だけを見る。速さは、前に見たときからの動きで測る。
    struct Seen {
        float x, y, vx, vy, radius;
    };
    std::vector<Seen> seen;
    std::unordered_map<uint64_t, myEngine::Vector2> positions;
    float nearest = 1.0e9f, bestThreat = 1.0e9f;
    int close = 0;
    for (const ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
        if (!enemy || enemy->IsDead() || enemy->IsRunResource()) {
            continue;
        }
        const myEngine::Vector3 p = enemy->GetWorldPosition();
        positions[enemy->GetCollisionId()] = {p.x, p.y};
        if (std::abs(p.x - camera.x) > halfWidth + 0.5f || std::abs(p.y - camera.y) > halfHeight + 0.5f) {
            continue;
        }
        Seen item{p.x, p.y, 0.0f, 0.0f, enemy->GetRadius()};
        if (const auto last = m.positions.find(enemy->GetCollisionId()); last != m.positions.end()) {
            item.vx = (p.x - last->second.x) / kThinkSeconds;
            item.vy = (p.y - last->second.y) / kThinkSeconds;
        }
        seen.push_back(item);
        const float distance = std::hypot(p.x - self.x, p.y - self.y) - item.radius;
        close += distance < 10.0f ? 1 : 0;
        nearest = (std::min)(nearest, distance);
        // 近い敵を狙う。進む道をふさぐ敵は、少し近いと見る。
        const float along = ((p.x - self.x) * m.nextMove.x + (p.y - self.y) * m.nextMove.y) / (std::max)(0.5f, distance + item.radius);
        const float threat = distance * (1.0f - 0.3f * (std::max)(0.0f, along));
        if (threat < bestThreat) {
            bestThreat = threat;
            m.nextAim = std::atan2(p.y - self.y, p.x - self.x) + std::normal_distribution<float>(0.0f, kAimErrorRadians)(m.random);
        }
    }
    m.positions = std::move(positions);

    const float hpRatio = static_cast<float>(player.GetHp()) / static_cast<float>((std::max)(1, player.GetMaxHp()));
    myEngine::Vector3 heal{}, shard{};
    const bool hasHeal = hpRatio < 0.85f && FindNearestHeal(self, kHealRange, heal);
    const std::vector<myEngine::Vector3> shards = pickups_.GetPositions();
    // 道の悪さ：敵との近さ・部屋の端・拾える物。寄ってくる敵は、道の上の自機を追ってくると考える。
    std::vector<Seen> moved;
    auto cost = [&](const myEngine::Vector2& direction, float turn) {
        moved = seen;
        float x = self.x, y = self.y, total = 0.0f;
        for (int step = 0; step < kSteps; ++step) {
            // 初めの1刻みは、前に決めた向きのまま進む（人の遅れ）。
            myEngine::Vector2 d = step == 0 ? m.nextMove : direction;
            if (step >= kTurnStep) {
                d = {direction.x * std::cos(turn) - direction.y * std::sin(turn), direction.x * std::sin(turn) + direction.y * std::cos(turn)};
            }
            x = (std::clamp)(x + d.x * speed * kStepSeconds, kSpawnMinX, kSpawnMaxX);
            y = (std::clamp)(y + d.y * speed * kStepSeconds, kSpawnMinY, kSpawnMaxY);
            const float weight = 1.0f - 0.06f * static_cast<float>(step);
            for (Seen& e : moved) {
                const float dx = x - e.x, dy = y - e.y;
                const float distance = (std::max)(0.01f, std::hypot(dx, dy));
                const float v = std::hypot(e.vx, e.vy);
                const bool chasing = e.vx * dx + e.vy * dy > 0.4f * v * distance;
                e.x += (chasing ? dx / distance * v : e.vx) * kStepSeconds;
                e.y += (chasing ? dy / distance * v : e.vy) * kStepSeconds;
                const float gap = distance - e.radius - radius;
                if (gap < 4.0f) {
                    // 触れるのは、拾える物より強く嫌う。
                    total += weight * (Danger(gap) + (gap < 0.15f ? 12.0f : 0.0f));
                }
            }
            // 縦と横を別に足す（隅は、行き止まりなので、いちばん嫌う）。
            for (const float wall : {(std::min)(x - kSpawnMinX, kSpawnMaxX - x), (std::min)(y - kSpawnMinY, kSpawnMaxY - y)}) {
                const float in = (std::max)(0.0f, kWallMargin - wall) / kWallMargin;
                total += kWallWeight * in * in;
            }
        }
        // 近くの欠片ほど拾いに行く。回復は、HP が減っているほど強く。
        auto pull = [&](const myEngine::Vector3& goal, float range, float strength) {
            const float now = std::hypot(goal.x - self.x, goal.y - self.y);
            const float next = std::hypot(goal.x - x, goal.y - y);
            return (now - next) / (speed * kStepSeconds * kSteps) * strength * (1.0f - now / range);
        };
        float gain = 0.0f;
        for (const myEngine::Vector3& item : shards) {
            if (std::hypot(item.x - self.x, item.y - self.y) < kShardRange) {
                gain += pull(item, kShardRange, 1.2f);
            }
        }
        total -= (std::min)(3.5f, gain);
        if (hasHeal) {
            total -= pull(heal, kHealRange, 8.0f + 10.0f * (1.0f - hpRatio));
        }
        // 急な切り返しを、少しだけ嫌う。
        total += 0.4f * (1.0f - (direction.x * m.nextMove.x + direction.y * m.nextMove.y));
        return total;
    };
    myEngine::Vector2 bestMove{};
    float best = cost(bestMove, 0.0f) + 0.5f;
    for (int i = 0; i < kDirections; ++i) {
        const float angle = 2.0f * kPi * static_cast<float>(i) / kDirections;
        const myEngine::Vector2 direction{std::cos(angle), std::sin(angle)};
        for (const float turn : {0.0f, -kTurnAngle, kTurnAngle}) {
            const float value = cost(direction, turn);
            if (value < best) {
                best = value;
                bestMove = direction;
            }
        }
    }
    m.nextMove = bestMove;
    // Q：満タンで、近くに敵が8体以上いるか、HP が半分を切って囲まれかけているときに使う。
    if (ultimateCharge_ >= Hero().ultimateCharge && (close >= 8 || (hpRatio < 0.5f && close >= 4))) {
        autoHeroPress_[static_cast<size_t>(survivor::AbilitySlot::Ultimate)] = true;
    }
}

void SurvivorRunController::FinishMortalTest(const char* outcome, double wall)
{
    const auto& player = *world_.resources.player_;
    const MortalTest& m = mortal_;
    nlohmann::json by = nlohmann::json::object();
    for (const auto& [name, value] : m.by) {
        by[name] = {{"hits", value.first}, {"damage", value.second}};
    }
    const bool dead = std::string(outcome) == "dead";
    nlohmann::json root{{"mortal", true},
                        {"config", ConfigPath()},
                        {"seed", m.seed},
                        {"pick", autoMortalNoMain_ ? "nomain" : "main"},
                        {"build", BuildRecord()},
                        {"shake", ShakeRecord()},
                        {"mainOfferPity", config_.heroes.mainOfferPity},
                        {"subWeaponPower", config_.heroes.subWeaponPower},
                        {"outcome", outcome},
                        {"seconds", elapsed_},
                        {"killer", dead ? player.GetSurvivorLastHit() : std::string()},
                        {"hits", m.hits},
                        {"damageTaken", m.damage},
                        {"hpLost", player.GetSurvivorHpLost()},
                        {"firstHitAt", m.firstHitAt},
                        {"hitsBy", by},
                        {"level", level_},
                        {"kills", world_.combat.defeatedEnemies_},
                        {"maxAlive", m.maxAlive},
                        {"maxHp", player.GetMaxHp()},
                        {"healed", player.GetSurvivorHealed()},
                        {"healsCollected", healsCollected_},
                        {"ultimates", abilityUses_[3]},
                        {"levelTimes", levelTimes_},
                        {"picks", heroLevelUpLog_},
                        {"noDamageSeen", m.noDamageSeen},
                        {"wallSeconds", wall},
                        {"samples", nlohmann::json::parse("[" + m.samples + "]")}};
    std::ofstream(std::string(kAutoTestDirectory) + "/autotest.json") << root.dump() << "\n";
    autoTest_ = AutoTest::None;
    PostQuitMessage(0);
}

nlohmann::json SurvivorRunController::BuildRecord() const
{
    nlohmann::json subs = nlohmann::json::object(), perks = nlohmann::json::object(), skills = nlohmann::json::object();
    for (size_t i = 0; i < subWeaponTiers_.size(); ++i) {
        if (subWeaponTiers_[i] > 0) {
            subs[survivor::GetSubWeaponInfo(static_cast<survivor::SubWeapon>(i)).name] = subWeaponTiers_[i];
        }
    }
    for (size_t i = 0; i < perks_.size(); ++i) {
        if (perks_[i] > 0) {
            perks[survivor::GetPerkInfo(static_cast<survivor::PerkId>(i)).name] = perks_[i];
        }
    }
    for (size_t i = 0; i < heroSkillTiers_.size(); ++i) {
        if (heroSkillTiers_[i] > 0) {
            skills[survivor::GetHeroSkillInfo(static_cast<survivor::HeroSkill>(i)).name] = heroSkillTiers_[i];
        }
    }
    // 秒は 0.1 秒に丸める。まだなら -1（主武器が6段になった秒、ボスが出た秒・倒した秒。計画書 p11 の B3）。
    const auto at = [](float seconds) {
        return seconds < 0.0f ? -1.0 : std::round(seconds * 10.0) / 10.0;
    };
    return {{"mainTier", UpgradeTier(0)}, {"subs", subs}, {"perks", perks}, {"skills", skills}, {"mainOffers", mainOffers_}, {"levelUps", levelUpsOpened_},
            {"mainMaxedAt", at(mainMaxedAt_)}, {"bossAt", at(rivalSpawnedAt_)}, {"bossDefeatedAt", at(rivalDefeatedAt_)}};
}

void SurvivorRunController::CountShake(float dt)
{
    // 残り時間が前のフレームより増えたら、ゆれが始まったか、強まって延びた（同じフレームに何度起きても1回）。
    const auto& presentation = world_.presentation;
    ShakeCount& c = shakeCount_;
    if (presentation.cameraShakeTimer_ > c.previousTimer + 0.0001f) {
        const float power = presentation.cameraShakePower_ * usersettings::ShakeScale();
        ++c.count;
        c.strong += power > 0.5f ? 1 : 0;
        c.powerSum += power;
        c.powerMax = (std::max)(c.powerMax, power);
    }
    c.activeSeconds += presentation.cameraShakeTimer_ > 0.0f ? dt : 0.0f;
    c.seconds += dt;
    c.previousTimer = presentation.cameraShakeTimer_;
}

nlohmann::json SurvivorRunController::ShakeRecord() const
{
    const ShakeCount& c = shakeCount_;
    const double seconds = (std::max)(0.001f, c.seconds);
    const auto r = [](double value) {
        return std::round(value * 1000.0) / 1000.0;
    };
    return {{"count", c.count},
            {"perSecond", r(c.count / seconds)},
            {"strong", c.strong},
            {"meanPower", r(c.count > 0 ? c.powerSum / c.count : 0.0)},
            {"maxPower", r(c.powerMax)},
            {"powerPerSecond", r(c.powerSum / seconds)},
            {"activeRatio", r(c.activeSeconds / seconds)},
            {"blastShake", config_.hud.blastShake}};
}

void SurvivorRunController::AppendPlayLog(const char* outcome)
{
    // 人が遊んだ回だけ：自動確認・説明・タイトルの後ろ・画面の出入りの確認・隠した窓では書かない。
    if (playLogWritten_ || !HeroMode() || autoTest_ != AutoTest::None || autoMortal_ || speedTest_ || world_.demo.titleDemo_ || tutorialStep_ >= 0 ||
        elapsed_ <= 0.0f) {
        return;
    }
    playLogWritten_ = true;
    // MYENGINE_PLAYLOG_FILE=場所：書く場所を替える（記録の確認用。画面の出入りの確認や隠した窓でも書く）。
    char value[MAX_PATH]{};
    std::string path = "save/playlog.jsonl";
    if (const DWORD length = GetEnvironmentVariableA("MYENGINE_PLAYLOG_FILE", value, MAX_PATH); length > 0 && length < MAX_PATH) {
        path = value;
    } else if (menuflow::IsActive() || GetEnvironmentVariableA("MYENGINE_HIDDEN_WINDOW", nullptr, 0) > 0) {
        return;
    }
    const auto& player = *world_.resources.player_;
    SYSTEMTIME now{};
    GetLocalTime(&now);
    char at[32]{};
    sprintf_s(at, "%04u-%02u-%02uT%02u:%02u:%02u", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    const bool dead = std::string(outcome) == "dead";
    nlohmann::json line{{"at", at},
                              {"outcome", outcome},
                              {"seconds", std::round(elapsed_ * 10.0) / 10.0},
                              {"level", level_},
                              {"kills", world_.combat.defeatedEnemies_},
                              {"hits", player.GetSurvivorHits() - playLogHitsBase_},
                              {"killer", dead ? player.GetSurvivorLastHit() : std::string()}};
    line.update(BuildRecord());
    std::error_code error;
    const std::filesystem::path file(path);
    if (file.has_parent_path()) {
        std::filesystem::create_directories(file.parent_path(), error);
    }
    std::ofstream(file, std::ios::app) << line.dump() << "\n";
}

} // namespace gameplay
