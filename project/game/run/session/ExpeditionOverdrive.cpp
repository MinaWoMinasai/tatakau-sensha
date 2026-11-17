#include "game/run/session/ExpeditionOverdrive.h"
#include "game/session/GameplaySystems.h"
#include "PostEffectSettings.h"
#include <algorithm>
#include <cmath>

namespace gameplay {

namespace {
using C = tankrun::CardId;

// 遠征の熱。射撃型の主砲（0.3秒ごと）を撃ち続けると約9秒でオーバーヒートする。
constexpr float kHeatPerShot = 5.0f;
constexpr float kCoolPerSecond = 55.0f;
constexpr float kCoolWhileFiring = 6.0f;
constexpr float kCoolDelay = 0.4f;
constexpr float kOverheatSeconds = 1.8f;

// 戦える範囲（部屋の外周の内側）。
constexpr float kFieldMinX = 23.0f, kFieldMaxX = 65.0f, kFieldMinY = 17.0f, kFieldMaxY = 41.0f;

// 新しい敵。
constexpr int kWarper = static_cast<int>(ExpEnemy::SurvivorRole::Warper);
constexpr float kWarpInterval = 3.2f;
constexpr float kWarpFadeSeconds = 0.45f;
constexpr float kWarpDistance = 7.0f;
// 出現のディゾルブ（ワープ機が現れるときと同じ見た目。1.12 で全部消えた状態）と、現れきるまでの秒数。
constexpr float kArrivalDissolve = 1.12f;
constexpr float kArrivalSeconds = 0.7f;
const myEngine::Vector4 kHunterColor{2.0f, 0.42f, 0.12f, 1.0f};
const myEngine::Vector4 kWarperColor{1.05f, 0.35f, 2.0f, 1.0f};

// 強化の色。
const myEngine::Vector4 kNovaColor{2.4f, 0.35f, 0.12f, 1.0f};
const myEngine::Vector4 kChainColor{2.1f, 0.75f, 0.12f, 1.0f};
const myEngine::Vector4 kBarrageColor{2.2f, 0.55f, 0.18f, 1.0f};
const myEngine::Vector4 kSingularityColor{1.2f, 0.35f, 2.2f, 1.0f};

float Distance2D(const myEngine::Vector3& a, const myEngine::Vector3& b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}

float SmoothStep(float from, float to, float value)
{
    const float t = (std::clamp)((value - from) / (std::max)(0.0001f, to - from), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

std::unique_ptr<myEngine::Sprite> Rect(myEngine::Vector2 position, myEngine::Vector2 size, const myEngine::Vector4& color)
{
    auto sprite = std::make_unique<myEngine::Sprite>();
    sprite->Initialize(myEngine::SpriteCommon::GetInstance(), "resources/white512x512.png");
    sprite->SetPosition(position);
    sprite->SetSize(size);
    sprite->SetColor(color);
    sprite->Update();
    return sprite;
}

std::unique_ptr<myEngine::TextLabel> Label(float size, myEngine::Vector2 position, const myEngine::Vector4& color)
{
    myEngine::TextStyle style{};
    style.fontFamily = "Meiryo";
    style.fontSize = size;
    style.color = color;
    style.padding = 4;
    style.outlineThickness = 0;
    auto label = std::make_unique<myEngine::TextLabel>();
    label->Initialize(myEngine::SpriteCommon::GetInstance(), " ", style);
    label->SetPosition(position);
    return label;
}

bool IsShape(const std::string& type)
{
    return type == "Square" || type == "Triangle" || type == "Pentagon";
}
} // namespace

bool ExpeditionOverdrive::Has(tankrun::CardId card) const
{
    return world_.run.tankRun_.GetCardCount(card) > 0;
}

void ExpeditionOverdrive::Initialize()
{
    heatTrack_ = Rect({24, 79}, {230, 6}, {0.20f, 0.06f, 0.06f, 0.9f});
    heatFill_ = Rect({24, 79}, {230, 6}, {1.0f, 0.45f, 0.12f, 1.0f});
    warningText_ = Label(30, {640, 96}, {1.0f, 0.18f, 0.30f, 1.0f});
    warningText_->SetAnchorPoint({0.5f, 0.0f});
    warningTextValue_.clear();
    // 縁の光は、外側ほど濃い帯を6段重ねて作る（上・下・左・右）。
    edgeGlow_.clear();
    for (int side = 0; side < 4; ++side) {
        for (int layer = 0; layer < 6; ++layer) {
            edgeGlow_.push_back(Rect({0, 0}, {1, 1}, {1.0f, 0.05f, 0.18f, 0.0f}));
        }
    }
    for (auto& line : frameLines_) {
        line = Rect({0, 0}, {1, 1}, {1.0f, 0.15f, 0.35f, 0.0f});
    }
    for (auto& mark : cornerMarks_) {
        mark = Rect({0, 0}, {1, 1}, {1.0f, 0.35f, 0.45f, 0.0f});
    }
    for (auto& line : scanLines_) {
        line = Rect({0, 0}, {1280, 2}, {1.0f, 0.12f, 0.28f, 0.0f});
    }
}

void ExpeditionOverdrive::BeginRoom(const std::vector<std::string>& spawnTypes, int combatStage, bool elite, bool tutorial, bool boss)
{
    random_.seed(static_cast<uint32_t>(world_.run.expeditionSeed_ * 2654435761u + world_.run.tankExpedition_.GetRoomIndex() * 97u + 13u));
    spawnTypes_.clear();
    for (const auto& type : spawnTypes) {
        if (!IsShape(type)) {
            spawnTypes_.push_back(type);
        }
    }
    stage_ = (std::max)(1, combatStage);
    elite_ = elite;
    // 1区画を長くするため、ふつうの戦闘は2回、精鋭は3回の増援を呼ぶ。操作訓練とボスには呼ばない。
    wavesTotal_ = tutorial || boss ? 0 : elite ? 3 : 2;
    wavesSpawned_ = 0;
    roomTime_ = 0.0f;
    sinceWave_ = 0.0f;
    implosions_.clear();
    barrageCounter_ = 0;
    chainBudget_ = 12.0f;
    selfHitCooldown_ = 0.0f;
    overheatFlash_ = 0.0f;
    waveFlash_ = 0.0f;
    auto& player = *world_.resources.player_;
    ApplyHeatConfig(true);
    player.RestoreHeat({0.0f, 0.0f});
    lastOverheats_ = player.GetHeat().GetOverheatCount();
    lastAttacks_ = player.GetPrimaryAttackCount();
    lastDashes_ = player.GetDashStartedCount();
    // 部屋に置かれた敵も、ワープ機と同じディゾルブで現す（戦闘が始まった最初の更新で光と輪を出す）。
    // 操作訓練は説明の間に敵を見せたいので、そのまま置く。
    for (ExpEnemy* enemy : tutorial ? std::vector<ExpEnemy*>{} : world_.resources.enemyManager_->GetEnemyPtrs()) {
        if (enemy && !enemy->IsDead() && !enemy->IsRunResource()) {
            enemy->SetWarpDissolve(kArrivalDissolve);
        }
    }
    arrivalPending_ = true;
}

void ExpeditionOverdrive::AnnounceArrival(const myEngine::Vector3& position)
{
    myEngine::ParticleManager::GetInstance()->EmitNeonImpactEffect(position, {0, 1, 0}, kWarperColor, 10);
    AddRing(position, 1.8f, 0.35f, kWarperColor);
}

void ExpeditionOverdrive::ApplyHeatConfig(bool force)
{
    PlayerHeat::Config config{};
    config.enabled = true;
    config.maxHeat = 100.0f;
    config.heatPerShot = kHeatPerShot;
    config.coolDelay = kCoolDelay;
    config.coolPerSecond = kCoolPerSecond;
    config.coolPerSecondWhileFiring = kCoolWhileFiring;
    config.overheatSeconds = kOverheatSeconds;
    if (Has(C::ThermalNova)) {
        config.heatPerShot *= 1.5f;
        config.overheatSeconds += 0.8f;
    }
    if (Has(C::Overclock)) {
        config.coolPerSecond *= 0.6f;
        config.coolPerSecondWhileFiring *= 0.6f;
    }
    const bool changed = !heatApplied_ || config.heatPerShot != appliedHeat_.heatPerShot ||
                         config.coolPerSecond != appliedHeat_.coolPerSecond ||
                         config.coolPerSecondWhileFiring != appliedHeat_.coolPerSecondWhileFiring ||
                         config.overheatSeconds != appliedHeat_.overheatSeconds || !world_.resources.player_->GetHeat().IsEnabled();
    if (force || changed) {
        world_.resources.player_->SetHeatConfig(config);
        appliedHeat_ = config;
        heatApplied_ = true;
    }
}

void ExpeditionOverdrive::UpdatePresentation(float dt)
{
    clock_ += dt;
    overheatFlash_ = (std::max)(0.0f, overheatFlash_ - dt);
    waveFlash_ = (std::max)(0.0f, waveFlash_ - dt);
    const auto& player = *world_.resources.player_;
    combatVisible_ = world_.run.expeditionMapEnabled_ && world_.run.tankExpedition_.IsCombat() && !player.IsDead() &&
                     player.GetHeat().IsEnabled() && !world_.run.expeditionMapRun_.IsChoosing() && !world_.run.expeditionMapPreview_;
    float target = 0.0f;
    if (combatVisible_) {
        const auto& heat = player.GetHeat();
        overheated_ = heat.IsOverheated();
        heatRatio_ = heat.GetMaxHeat() > 0.0f ? (std::clamp)(heat.GetHeat() / heat.GetMaxHeat(), 0.0f, 1.0f) : 0.0f;
        target = overheated_ ? 0.88f + 0.12f * std::sin(clock_ * 14.0f) : SmoothStep(0.35f, 1.0f, heatRatio_) * 0.8f;
    } else {
        overheated_ = false;
        heatRatio_ = 0.0f;
    }
    // 警告の文字はここ（更新の段）で作り直す。TextLabel::SetText は新しい文字のテクスチャを読み込み、
    // コマンドリストを実行して作り直すので、描画の途中で呼ぶとルートシグネチャが外れて落ちる。
    if (combatVisible_ && warningText_) {
        const std::string warning = overheated_ ? "!! OVERHEAT !!" : heatRatio_ >= 0.8f ? "HEAT CRITICAL" : "";
        if (warning != warningTextValue_) {
            warningTextValue_ = warning;
            if (!warning.empty()) {
                warningText_->SetText(warning);
            }
        }
    }
    float& tint = world_.presentation.heatTint_;
    const float rate = target > tint ? 5.0f : 2.5f;
    tint += (target - tint) * (std::min)(1.0f, rate * dt);
    if (tint < 0.002f && target <= 0.0f) {
        tint = 0.0f;
    }
}

void ExpeditionOverdrive::UpdateCombat(float dt)
{
    auto& player = *world_.resources.player_;
    if (player.IsDead()) {
        return;
    }
    roomTime_ += dt;
    sinceWave_ += dt;
    selfHitCooldown_ = (std::max)(0.0f, selfHitCooldown_ - dt);
    chainBudget_ = (std::min)(12.0f, chainBudget_ + dt * 12.0f);
    ApplyHeatConfig(false);
    if (arrivalPending_) {
        arrivalPending_ = false;
        for (ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
            if (enemy && !enemy->IsDead() && enemy->GetWarpDissolve() > 0.0f) {
                AnnounceArrival(enemy->GetWorldPosition());
            }
        }
    }

    const int overheats = player.GetHeat().GetOverheatCount();
    if (overheats > lastOverheats_) {
        OnOverheat();
    }
    lastOverheats_ = overheats;

    UpdateUpgrades(dt);

    // 増援：残りが少なくなったか、前の波から時間が経ったら、次の波を呼ぶ。
    if (wavesSpawned_ < wavesTotal_ && roomTime_ >= 4.0f) {
        int threats = 0;
        for (auto* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
            if (enemy && !enemy->IsDead() && !enemy->IsRunResource()) {
                ++threats;
            }
        }
        if (threats <= 2 || sinceWave_ >= 15.0f) {
            SpawnWave();
        }
    }
    UpdateEnemyRoles(dt);
}

void ExpeditionOverdrive::UpdateUpgrades(float dt)
{
    auto& player = *world_.resources.player_;
    const auto& heat = player.GetHeat();
    const bool overheated = heat.IsOverheated();
    const float ratio = heat.GetMaxHeat() > 0.0f ? (std::clamp)(heat.GetHeat() / heat.GetMaxHeat(), 0.0f, 1.0f) : 0.0f;

    // 移動の代償（全方位斉射・臨界駆動の過熱中）。
    float move = 1.0f;
    if (Has(C::NovaBarrage)) {
        move *= 0.85f;
    }
    if (Has(C::Overclock) && overheated) {
        move *= 0.55f;
    }
    player.SetSurvivorMoveScale(move);

    // 臨界駆動：熱が高いほど主砲が重くなる。
    Player::SurvivorPartEffects effects{};
    if (Has(C::Overclock)) {
        effects.shotDamageScale = 1.0f + ratio;
    }
    player.SetSurvivorPartEffects(effects);

    // 全方位斉射：攻撃4回ごとに全周の弾幕。
    const uint32_t attacks = player.GetPrimaryAttackCount();
    if (attacks > lastAttacks_ && Has(C::NovaBarrage) && !overheated) {
        barrageCounter_ += static_cast<int>(attacks - lastAttacks_);
        if (barrageCounter_ >= 4) {
            barrageCounter_ = 0;
            constexpr int kRing = 16;
            const float offset = std::uniform_real_distribution<float>(0.0f, 0.39f)(random_);
            for (int i = 0; i < kRing; ++i) {
                const float angle = offset + 6.2831853f * static_cast<float>(i) / static_cast<float>(kRing);
                Player::SurvivorShot shot{};
                shot.direction = {std::cos(angle), std::sin(angle), 0.0f};
                shot.damageScale = 0.7f;
                shot.speedScale = 0.95f;
                shot.size = 1.15f;
                shot.pierce = 1;
                shot.color = kBarrageColor;
                player.FireSurvivorShot(shot);
            }
            AddRing(player.GetWorldPosition(), 3.0f, 0.22f, kBarrageColor);
            player.AddHeat(12.0f);
            Shake(0.05f, 0.08f);
        }
    }
    lastAttacks_ = attacks;

    // 重力崩壊：ダッシュの始めに吸い寄せ、少し後にその場で爆発。
    const uint32_t dashes = player.GetDashStartedCount();
    if (dashes > lastDashes_ && Has(C::Singularity) && !overheated) {
        const myEngine::Vector3 center = player.GetWorldPosition();
        for (auto* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
            if (!enemy || enemy->IsDead() || enemy->IsRunResource()) {
                continue;
            }
            const myEngine::Vector3 position = enemy->GetWorldPosition();
            const float distance = Distance2D(position, center);
            if (distance < 9.0f && distance > 0.5f) {
                enemy->ApplyKnockback({center.x - position.x, center.y - position.y, 0.0f}, 0.30f + 0.04f * distance);
            }
        }
        AddRing(center, 9.0f, 0.4f, kSingularityColor);
        myEngine::ParticleManager::GetInstance()->EmitNeonImpactEffect(center, {0, 1, 0}, kSingularityColor, 16);
        implosions_.push_back({center, 0.4f});
        player.AddHeat(38.0f);
    }
    lastDashes_ = dashes;
    for (auto& implosion : implosions_) {
        implosion.timer -= dt;
        if (implosion.timer <= 0.0f) {
            player.TriggerBlastAt(implosion.position, 4.8f, 5.0f, kSingularityColor, 0.0f);
            AddRing(implosion.position, 4.8f, 0.35f, kSingularityColor);
            myEngine::ParticleManager::GetInstance()->EmitNeonImpactEffect(implosion.position, {0, 1, 0}, kSingularityColor, 28);
            world_.combat.screenEffectDirector_.TriggerEnemyDefeat(world_.gameplayQueries->WorldToScreenUv(implosion.position), 0.8f);
            Shake(0.16f, 0.18f);
        }
    }
    std::erase_if(implosions_, [](const Implosion& implosion) {
        return implosion.timer <= 0.0f;
    });
}

void ExpeditionOverdrive::OnOverheat()
{
    overheatFlash_ = 0.6f;
    auto& player = *world_.resources.player_;
    const myEngine::Vector3 center = player.GetWorldPosition();
    myEngine::ParticleManager::GetInstance()->EmitNeonImpactEffect(center, {0, 1, 0}, {2.2f, 0.25f, 0.18f, 1.0f}, 14);
    Shake(0.08f, 0.14f);
    if (Has(C::ThermalNova)) {
        // 熱核解放：たまった熱をまわりへ一気に吐き出す。
        player.TriggerBlastAt(center, 7.5f, 10.0f, kNovaColor, 0.03f);
        AddRing(center, 7.5f, 0.45f, kNovaColor);
        AddRing(center, 4.0f, 0.30f, {2.4f, 1.4f, 0.4f, 1.0f});
        myEngine::ParticleManager::GetInstance()->EmitNeonImpactEffect(center, {0, 1, 0}, kNovaColor, 40);
        world_.combat.screenEffectDirector_.TriggerEnemyDefeat(world_.gameplayQueries->WorldToScreenUv(center), 1.0f);
        Shake(0.26f, 0.28f);
    }
}

void ExpeditionOverdrive::OnEnemyDefeated(const myEngine::Vector3& position)
{
    if (!world_.run.expeditionMapEnabled_ || !world_.run.tankExpedition_.IsCombat()) {
        return;
    }
    auto& player = *world_.resources.player_;
    if (player.IsDead()) {
        return;
    }
    const auto& heat = player.GetHeat();
    const float ratio = heat.GetMaxHeat() > 0.0f ? heat.GetHeat() / heat.GetMaxHeat() : 0.0f;
    if (Has(C::ChainDetonation) && chainBudget_ >= 1.0f) {
        // 誘爆：倒した敵がその場で爆発し、巻き込まれた敵がまた爆発する。
        chainBudget_ -= 1.0f;
        player.TriggerBlastAt(position, 3.4f, 2.4f, kChainColor, 0.12f);
        AddRing(position, 3.4f, 0.3f, kChainColor);
        player.AddHeat(4.0f);
        Shake(0.07f, 0.1f);
        if (selfHitCooldown_ <= 0.0f && Distance2D(position, player.GetWorldPosition()) < 3.9f) {
            player.TakeDamage(5);
            selfHitCooldown_ = 0.5f;
        }
    }
    if (Has(C::Overclock) && ratio >= 0.7f) {
        player.TriggerBlastAt(position, 2.6f, 1.5f, {2.3f, 0.4f, 0.15f, 1.0f}, 0.04f);
        AddRing(position, 2.6f, 0.22f, {2.3f, 0.4f, 0.15f, 1.0f});
    }
}

bool ExpeditionOverdrive::FindSpawnPosition(float minDistance, myEngine::Vector3& out)
{
    const myEngine::Vector3 player = world_.resources.player_->GetWorldPosition();
    std::uniform_real_distribution<float> x(kFieldMinX + 1.0f, kFieldMaxX - 1.0f);
    std::uniform_real_distribution<float> y(kFieldMinY + 1.0f, kFieldMaxY - 1.0f);
    for (int attempt = 0; attempt < 40; ++attempt) {
        const myEngine::Vector3 candidate{x(random_), y(random_), 0.0f};
        const float required = attempt < 30 ? minDistance : minDistance * 0.5f;
        if (Distance2D(candidate, player) < required) {
            continue;
        }
        if (world_.resources.stage_->IsCollisionWithAnyBlock(candidate, 1.4f)) {
            continue;
        }
        out = candidate;
        return true;
    }
    return false;
}

void ExpeditionOverdrive::SpawnWave()
{
    ++wavesSpawned_;
    sinceWave_ = 0.0f;
    waveFlash_ = 1.2f;
    auto& enemies = *world_.resources.enemyManager_;
    const float hpScale = 0.85f + 0.18f * static_cast<float>(stage_);
    auto spawn = [&](const std::string& type, int hp) -> ExpEnemy* {
        myEngine::Vector3 position{};
        if (!FindSpawnPosition(9.0f, position) || !enemies.SpawnLevelEnemy(position, type, hp)) {
            return nullptr;
        }
        ExpEnemy* enemy = enemies.GetEnemyPtrs().back();
        // 出現はワープ機と同じディゾルブで現す。
        enemy->SetWarpDissolve(kArrivalDissolve);
        AnnounceArrival(position);
        return enemy;
    };

    // 部屋の顔ぶれを少し足す。
    const int reinforcements = 1 + (stage_ >= 3 ? 1 : 0) + (elite_ ? 1 : 0);
    for (int i = 0; i < reinforcements; ++i) {
        const std::string type = spawnTypes_.empty()
                                     ? (i % 2 ? "Skirmisher" : "Charger")
                                     : spawnTypes_[std::uniform_int_distribution<size_t>(0, spawnTypes_.size() - 1)(random_)];
        if (ExpEnemy* enemy = spawn(type, -1)) {
            enemy->SetHp(static_cast<int>(std::lround(static_cast<float>(enemy->GetMaxHp()) * hpScale)));
        }
    }

    // 追尾機：左右に揺れながら迫り、ときどき飛びかかる三角の群れ。
    const int hunters = (std::min)(8, 3 + stage_ + (elite_ ? 1 : 0));
    std::uniform_real_distribution<float> phase(0.0f, 6.2831853f);
    for (int i = 0; i < hunters; ++i) {
        if (ExpEnemy* enemy = spawn("Triangle", -1)) {
            enemy->SetHp(static_cast<int>(std::lround(14.0f * hpScale)));
            enemy->SetBaseColor(kHunterColor);
            enemy->SetSizeScale(0.95f);
            enemy->SetPlayerChase(6.0f + 0.6f * static_cast<float>(stage_), 6.2f + 0.7f * static_cast<float>(stage_));
            enemy->SetWobble(0.55f, 3.2f, phase(random_));
            enemy->SetPlayerLunge(3.0f, 0.4f, 0.35f, 15.0f, std::uniform_real_distribution<float>(0.0f, 3.0f)(random_));
        }
    }

    // ワープ機：一定の間隔で消え、自機の近くに現れて飛びかかる五角。
    const int warpers = (wavesSpawned_ >= 2 || stage_ >= 2 ? 1 : 0) + (elite_ ? 1 : 0) + (stage_ >= 3 ? 1 : 0);
    for (int i = 0; i < warpers; ++i) {
        if (ExpEnemy* enemy = spawn("Pentagon", -1)) {
            enemy->SetHp(static_cast<int>(std::lround(40.0f * hpScale)));
            enemy->SetSurvivorRole(ExpEnemy::SurvivorRole::Warper);
            enemy->SetRoleTimer(std::uniform_real_distribution<float>(0.0f, 1.5f)(random_));
            enemy->SetBaseColor(kWarperColor);
            enemy->SetSizeScale(1.2f);
            enemy->SetPlayerChase(4.5f, 4.8f);
        }
    }

    Shake(0.06f, 0.12f);
}

void ExpeditionOverdrive::UpdateEnemyRoles(float dt)
{
    const myEngine::Vector3 player = world_.resources.player_->GetWorldPosition();
    for (ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
        if (!enemy || enemy->IsDead()) {
            continue;
        }
        if (static_cast<int>(enemy->GetSurvivorRole()) != kWarper) {
            // 増援の出現のディゾルブを戻していく。
            if (enemy->GetWarpDissolve() > 0.0f) {
                enemy->SetWarpDissolve((std::max)(0.0f, enemy->GetWarpDissolve() - dt * kArrivalDissolve / kArrivalSeconds));
            }
            continue;
        }
        const float timer = enemy->AdvanceRoleTimer(dt);
        const float left = kWarpInterval - timer;
        const myEngine::Vector3 position = enemy->GetWorldPosition();
        if (left <= 0.0f) {
            std::uniform_real_distribution<float> angle(0.0f, 6.2831853f);
            for (int attempt = 0; attempt < 10; ++attempt) {
                const float a = angle(random_);
                const myEngine::Vector3 target{player.x + std::cos(a) * kWarpDistance, player.y + std::sin(a) * kWarpDistance, position.z};
                if (target.x > kFieldMinX && target.x < kFieldMaxX && target.y > kFieldMinY && target.y < kFieldMaxY &&
                    !world_.resources.stage_->IsCollisionWithAnyBlock(target, enemy->GetRadius())) {
                    myEngine::ParticleManager::GetInstance()->EmitNeonImpactEffect(position, {0, 1, 0}, kWarperColor, 6);
                    enemy->SetWorldPosition(target);
                    myEngine::ParticleManager::GetInstance()->EmitNeonImpactEffect(target, {0, 1, 0}, kWarperColor, 10);
                    AddRing(target, 1.8f, 0.3f, kWarperColor);
                    // 現れた直後に自機へ飛びかかる。
                    enemy->ApplyKnockback({player.x - target.x, player.y - target.y, 0.0f}, 0.34f);
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
    }
}

void ExpeditionOverdrive::AddRing(const myEngine::Vector3& position, float radius, float life, const myEngine::Vector4& color)
{
    auto& rings = world_.presentation.playerMineExplosions_;
    if (rings.size() < 64) {
        rings.push_back({position, radius, life, life, color});
    }
}

void ExpeditionOverdrive::Shake(float power, float duration)
{
    auto& presentation = world_.presentation;
    if (presentation.cameraShakeTimer_ > 0.0f && presentation.cameraShakePower_ >= power) {
        return;
    }
    presentation.cameraShakeDuration_ = duration;
    presentation.cameraShakeTimer_ = duration;
    presentation.cameraShakePower_ = power;
}

std::string ExpeditionOverdrive::WaveStatus() const
{
    if (wavesTotal_ <= 0) {
        return {};
    }
    if (wavesSpawned_ >= wavesTotal_) {
        return "   増援 なし";
    }
    return "   増援 あと" + std::to_string(wavesTotal_ - wavesSpawned_) + "波";
}

void ExpeditionOverdrive::ApplyScreenEffect(myEngine::ScreenEffectState& state) const
{
    const float tint = world_.run.expeditionMapEnabled_ ? (std::clamp)(world_.presentation.heatTint_, 0.0f, 1.0f) : 0.0f;
    state.heatTint = tint;
    if (tint <= 0.05f && overheatFlash_ <= 0.0f) {
        return;
    }
    auto& param = state.param;
    if (!state.active) {
        param = {};
        state.active = true;
    }
    // 熱が高いほど、周辺を赤く絞り、にじみと走査線を強める。
    param.vignetteIntensity = (std::max)(param.vignetteIntensity, 0.32f * tint);
    param.vignetteScale = (std::max)(param.vignetteScale, 1.0f + 0.15f * tint);
    param.chromAbAmount += 0.010f * tint;
    param.scanlineIntensity = (std::max)(param.scanlineIntensity, 0.16f * tint);
    param.scanlineFrequency = (std::max)(param.scanlineFrequency, 90.0f);
    param.intensity += 0.18f * tint;
    if (overheatFlash_ > 0.0f || overheated_) {
        // オーバーヒートの間は画面が乱れる。
        const float burst = (std::max)(overheatFlash_ / 0.6f, overheated_ ? 0.35f : 0.0f);
        param.glitchAmount = (std::max)(param.glitchAmount, 0.035f * burst);
        param.noiseIntensity = (std::max)(param.noiseIntensity, 0.10f * burst);
        param.chromAbAmount += 0.02f * burst;
    }
}

void ExpeditionOverdrive::Draw()
{
    if (!heatTrack_ || !combatVisible_) {
        return;
    }
    myEngine::SpriteCommon::GetInstance()->PreDraw(myEngine::kNormal);
    const float tint = (std::clamp)(world_.presentation.heatTint_, 0.0f, 1.0f);

    // 画面の縁の赤い光（熱が高いときだけ）。
    if (tint > 0.02f) {
        constexpr float kWidth = 1280.0f, kHeight = 720.0f;
        const float pulse = 0.85f + 0.15f * std::sin(clock_ * (overheated_ ? 16.0f : 6.0f));
        for (int side = 0; side < 4; ++side) {
            for (int layer = 0; layer < 6; ++layer) {
                auto& sprite = edgeGlow_[static_cast<size_t>(side * 6 + layer)];
                const float depth = 8.0f + static_cast<float>(layer) * 16.0f;
                const float alpha = tint * pulse * 0.10f;
                if (side == 0) {
                    sprite->SetPosition({0, 0});
                    sprite->SetSize({kWidth, depth});
                } else if (side == 1) {
                    sprite->SetPosition({0, kHeight - depth});
                    sprite->SetSize({kWidth, depth});
                } else if (side == 2) {
                    sprite->SetPosition({0, 0});
                    sprite->SetSize({depth, kHeight});
                } else {
                    sprite->SetPosition({kWidth - depth, 0});
                    sprite->SetSize({depth, kHeight});
                }
                sprite->SetColor({1.0f, 0.04f + 0.04f * static_cast<float>(layer), 0.16f, alpha});
                sprite->Update();
                sprite->Draw();
            }
        }
        // 走査線：赤い細い線が上から下へ流れる。
        for (size_t i = 0; i < scanLines_.size(); ++i) {
            const float y = std::fmod(clock_ * (120.0f + 45.0f * static_cast<float>(i)) + 240.0f * static_cast<float>(i), kHeight);
            scanLines_[i]->SetPosition({0, y});
            scanLines_[i]->SetSize({kWidth, i == 0 ? 3.0f : 1.5f});
            scanLines_[i]->SetColor({1.0f, 0.15f, 0.3f, tint * 0.22f});
            scanLines_[i]->Update();
            scanLines_[i]->Draw();
        }
        // ネオンの枠と、サイバーな角の印。
        const float inset = 14.0f;
        const float lineAlpha = tint * pulse * 0.75f;
        const myEngine::Vector4 lineColor{1.0f, 0.18f, 0.36f, lineAlpha};
        frameLines_[0]->SetPosition({inset, inset});
        frameLines_[0]->SetSize({kWidth - inset * 2.0f, 1.5f});
        frameLines_[1]->SetPosition({inset, kHeight - inset - 1.5f});
        frameLines_[1]->SetSize({kWidth - inset * 2.0f, 1.5f});
        frameLines_[2]->SetPosition({inset, inset});
        frameLines_[2]->SetSize({1.5f, kHeight - inset * 2.0f});
        frameLines_[3]->SetPosition({kWidth - inset - 1.5f, inset});
        frameLines_[3]->SetSize({1.5f, kHeight - inset * 2.0f});
        for (auto& line : frameLines_) {
            line->SetColor(lineColor);
            line->Update();
            line->Draw();
        }
        const float arm = 70.0f, thick = 5.0f, o = inset - 4.0f;
        const std::array<myEngine::Vector2, 8> positions{myEngine::Vector2{o, o},
                                                         myEngine::Vector2{o, o},
                                                         myEngine::Vector2{kWidth - o - arm, o},
                                                         myEngine::Vector2{kWidth - o - thick, o},
                                                         myEngine::Vector2{o, kHeight - o - thick},
                                                         myEngine::Vector2{o, kHeight - o - arm},
                                                         myEngine::Vector2{kWidth - o - arm, kHeight - o - thick},
                                                         myEngine::Vector2{kWidth - o - thick, kHeight - o - arm}};
        for (size_t i = 0; i < cornerMarks_.size(); ++i) {
            cornerMarks_[i]->SetPosition(positions[i]);
            cornerMarks_[i]->SetSize(i % 2 == 0 ? myEngine::Vector2{arm, thick} : myEngine::Vector2{thick, arm});
            cornerMarks_[i]->SetColor({1.0f, 0.3f, 0.42f, (std::min)(1.0f, tint * 1.3f) * pulse});
            cornerMarks_[i]->Update();
            cornerMarks_[i]->Draw();
        }
    }

    // 熱のゲージ（HP とスタミナの下）。
    heatTrack_->Draw();
    const float shown = overheated_ ? 1.0f : heatRatio_;
    if (shown > 0.0f) {
        const float blink = overheated_ ? 0.55f + 0.45f * std::sin(clock_ * 18.0f) : 1.0f;
        const myEngine::Vector4 cool{1.0f, 0.75f, 0.2f, 1.0f}, hot{1.0f, 0.12f, 0.22f, 1.0f};
        const float t = overheated_ ? 1.0f : SmoothStep(0.3f, 0.95f, heatRatio_);
        heatFill_->SetSize({230.0f * shown, 6});
        heatFill_->SetColor({cool.x + (hot.x - cool.x) * t, cool.y + (hot.y - cool.y) * t, cool.z + (hot.z - cool.z) * t, blink});
        heatFill_->Update();
        heatFill_->Draw();
    }
    // 警告の文字は UpdatePresentation で作り直してある（描画の途中で文字のテクスチャを作るとコマンドリストが閉じられるため）。
    // 画面上の警告。
    if (!warningTextValue_.empty()) {
        const float jitter = overheated_ ? std::uniform_real_distribution<float>(-4.0f, 4.0f)(random_) : 0.0f;
        warningText_->SetPosition({640.0f + jitter, 96.0f});
        warningText_->SetAlpha(overheated_ ? 0.55f + 0.45f * std::sin(clock_ * 20.0f) : 0.6f + 0.3f * std::sin(clock_ * 8.0f));
        warningText_->Draw();
    }
}
} // namespace gameplay
