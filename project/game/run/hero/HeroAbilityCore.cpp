#include "game/run/hero/HeroAbilityCore.h"
#include "game/enemy/actor/Enemy.h"
#include "game/render/neon/NeonGridRenderer.h"
#include "game/session/GameWorld.h"
#include "game/session/GameplaySystems.h"
#include <algorithm>
#include <cmath>
#include <sstream>

// 機体（ヒーロー）の技（計画書 p4 の K0〜K7、p5 の H1 で SurvivorRunHero.cpp から移した）：主武器1つと、右クリック・Shift・E・Q の4つの技。
// 技は部品（survivor::AbilityKind）に数値を付けたもので、機体ごとの組み合わせは設定の heroes に書く。

namespace gameplay {

using survivor::AbilityKind;
using survivor::AbilitySlot;

void HeroAbilityCore::ResetHeroState()
{
    abilityCooldown_.fill(0.0f);
    sprintTimer_ = 0.0f;
    ultimateCharge_ = 0.0f;
    ultimatePulse_ = 0.0f;
    healZoneTimer_ = 0.0f;
    healZoneRemainder_ = 0.0f;
    barrageTimer_ = 0.0f;
    absorb_ = 0.0f;
    beamTarget_ = 0;
    beamOn_ = false;
    selfHealRemainder_ = 0.0f;
    selfHealOn_ = false;
    phaseTimer_ = 0.0f;
    orbTimer_ = 0.0f;
    lanceTimer_ = 0.0f;
    chargeTimer_ = 0.0f;
    fortressTimer_ = 0.0f;
    shockAge_ = 10.0f;
    burstShotsLeft_ = 0;
    burstShotTimer_ = 0.0f;
    burstCooldown_ = 0.0f;
    skillHasLastPosition_ = false;
    steadyTimer_ = 0.0f;
    battleStacks_ = 0;
    battleTimer_ = 0.0f;
    orbitTick_ = 0.0f;
    turretTimer_ = 0.5f;
    mineTimer_ = 1.0f;
    mines_.clear();
    mortarTimer_ = 1.5f;
    mortarShells_.clear();
    dronesPlaced_ = false;
    droneTimer_ = 0.5f;
    flameTimer_ = 0.0f;
    flameWait_ = 0.5f;
    pulseTimer_ = 1.0f;
    // 反応装甲は、取ったらすぐに構える（構え直すまでの秒を先に満たしておく）。
    guardTimer_ = survivor::kGuardBaseCooldown;
    guardSeen_ = heroWorld_.resources.player_->GetSurvivorGuardBlocks();
    guardFlash_ = 0.0f;
    reviveGranted_ = false;
    heroWorld_.resources.player_->SetSurvivorGuardReady(false);
    heroWorld_.resources.player_->SetSurvivorRevives(0, 0.5f);
    heroWorld_.resources.player_->SetSurvivorMoveScale(HeroBaseMoveScale());
}

bool HeroAbilityCore::UseAbility(AbilitySlot slot)
{
    const size_t index = static_cast<size_t>(slot);
    const survivor::AbilityDefinition& ability = Ability(slot);
    if (ability.kind == AbilityKind::None || survivor::IsHeldAbility(ability.kind)) {
        return false;
    }
    if (slot == AbilitySlot::Ultimate) {
        if (ultimateCharge_ < Hero().ultimateCharge) {
            return false;
        }
    } else if (abilityCooldown_[index] > 0.0f) {
        return false;
    }
    auto& player = *heroWorld_.resources.player_;
    switch (ability.kind) {
    case AbilityKind::Sprint:
        sprintTimer_ = ability.duration;
        // 強化の3段目：走り出すと熱が0に戻る。
        if (UpgradeTier(static_cast<int>(index) + 1) >= 3) {
            player.RestoreHeat({});
        }
        break;
    case AbilityKind::Rocket: {
        // 照準の方へ、大きく遅めの橙の弾を1発。当たると爆発する。
        Player::SurvivorShot shot;
        shot.direction = player.GetDirection();
        shot.damageScale = ability.power * HeroPowerScale();
        shot.speedScale = 1.1f;
        shot.size = 1.7f;
        shot.blastRadius = ability.radius;
        shot.blastCenter = 1.0f;
        shot.blastSplash = 0.8f;
        shot.color = {2.2f, 0.95f, 0.25f, 1.0f};
        // 強化の3段目：2発を少し開いて撃つ。
        if (UpgradeTier(static_cast<int>(index) + 1) >= 3) {
            const myEngine::Vector3 aim = myEngine::NormalizeOr(player.GetDirection(), myEngine::Vector3{1.0f, 0.0f, 0.0f}, 0.001f);
            bool fired = false;
            for (float angle : {-0.14f, 0.14f}) {
                shot.direction = {aim.x * std::cos(angle) - aim.y * std::sin(angle), aim.x * std::sin(angle) + aim.y * std::cos(angle), 0.0f};
                fired = player.FireSurvivorShot(shot) || fired;
            }
            if (!fired) {
                return false;
            }
        } else if (!player.FireSurvivorShot(shot)) {
            return false;
        }
        break;
    }
    case AbilityKind::HealZone:
        healZonePosition_ = player.GetWorldPosition();
        healZoneTimer_ = ability.duration;
        healZoneRemainder_ = 0.0f;
        healZoneAge_ = 0.0f;
        break;
    case AbilityKind::Barrage:
        barrageTimer_ = ability.duration;
        barrageShotTimer_ = 0.0f;
        break;
    case AbilityKind::Phase:
        phaseTimer_ = ability.duration;
        player.StartSurvivorPhase(ability.duration);
        // 強化の3段目：使うと吸収ゲージが溜まる。
        if (UpgradeTier(static_cast<int>(index) + 1) >= 3) {
            AddAbsorb(1.5f);
        }
        break;
    case AbilityKind::BounceOrb: {
        // 照準の方へ球を出す。敵に触れるたびに、次の近い敵へ跳ねる。
        const myEngine::Vector3 aim = myEngine::NormalizeOr(player.GetDirection(), myEngine::Vector3{1.0f, 0.0f, 0.0f}, 0.001f);
        orbTimer_ = ability.duration;
        orbPosition_ = player.GetWorldPosition() + aim * 1.2f;
        orbVelocity_ = aim * ability.rate;
        orbTick_ = 0.0f;
        orbLastBounce_ = 0;
        orbScale_ = 1.0f;
        break;
    }
    case AbilityKind::Lance:
        lanceTimer_ = ability.duration;
        lanceTick_ = 0.0f;
        lanceAge_ = 0.0f;
        break;
    case AbilityKind::Charge:
        chargeTimer_ = ability.duration;
        chargeHit_.clear();
        // 強化の3段目：突進の間は無敵（敵や弾をすり抜ける）。
        if (UpgradeTier(static_cast<int>(index) + 1) >= 3) {
            player.StartSurvivorPhase(ability.duration);
        }
        break;
    case AbilityKind::Shockwave: {
        // 強化の3段目：衝撃波が敵の弾も消す。
        const int pushed = TriggerShockwave(player.GetWorldPosition(), ability.radius, HeroDamageUnit(ability.power), ability.rate,
                                            UpgradeTier(static_cast<int>(index) + 1) >= 3);
        shockwavePushes_ += pushed;
        // パーク「地鳴り」：押し返した数だけ、次の衝撃波までが縮む（待ち時間は下で入れるので、入れた後に縮める）。
        if (HasPerk(survivor::PerkId::Quake)) {
            abilityCooldown_[index] = ability.cooldown;
            abilityCooldown_[index] = (std::max)(0.0f, abilityCooldown_[index] - PerkValue(survivor::PerkId::Quake) * static_cast<float>(pushed));
            abilityCooldownSeen_[index] = (std::max)(abilityCooldownSeen_[index], ability.cooldown);
            ++abilityUses_[index];
            return true;
        }
        break;
    }
    case AbilityKind::Fortress:
        fortressTimer_ = ability.duration;
        fortressBlockedNow_ = 0;
        fortressAge_ = 0.0f;
        break;
    default:
        return false;
    }
    if (slot == AbilitySlot::Ultimate) {
        ultimateCharge_ = 0.0f;
    } else {
        abilityCooldown_[index] = ability.cooldown;
        abilityCooldownSeen_[index] = (std::max)(abilityCooldownSeen_[index], ability.cooldown);
    }
    ++abilityUses_[index];
    return true;
}

void HeroAbilityCore::UpdateHeroAbilities(float dt)
{
    for (float& cooldown : abilityCooldown_) {
        cooldown = (std::max)(0.0f, cooldown - dt);
    }
    // 走る・消える移動：効いている間だけ移動の速さを上げる。
    float moveScale = 1.0f;
    if (phaseTimer_ > 0.0f) {
        phaseTimer_ = (std::max)(0.0f, phaseTimer_ - dt);
        if (const survivor::AbilityDefinition* phase = FindAbility(AbilityKind::Phase); phase && phaseTimer_ > 0.0f) {
            moveScale = (std::max)(moveScale, phase->power);
        }
        // パーク「消える癒し」：消える移動の終わりに回復する。
        if (phaseTimer_ <= 0.0f && HasPerk(survivor::PerkId::PhaseHeal)) {
            auto& player = *heroWorld_.resources.player_;
            player.HealRunPlayer(static_cast<int>(std::round(static_cast<float>(player.GetMaxHp()) * PerkValue(survivor::PerkId::PhaseHeal))));
        }
    }
    // 突進：効いている間は速い。
    if (chargeTimer_ > 0.0f) {
        if (const survivor::AbilityDefinition* charge = FindAbility(AbilityKind::Charge)) {
            moveScale = (std::max)(moveScale, charge->power);
        }
    }
    // Q の太い光線の強化の3段目：効いている間は速く動ける。
    if (lanceTimer_ > 0.0f && UpgradeTier(4) >= 3) {
        moveScale = (std::max)(moveScale, 1.3f);
    }
    if (sprintTimer_ > 0.0f) {
        sprintTimer_ = (std::max)(0.0f, sprintTimer_ - dt);
        sprintSeconds_ += dt;
        if (const survivor::AbilityDefinition* sprint = FindAbility(AbilityKind::Sprint)) {
            moveScale = (std::max)(moveScale, sprint->power);
        }
        if (sprintTimer_ <= 0.0f) {
            moveScale = phaseTimer_ > 0.0f ? moveScale : 1.0f;
        }
    }
    // パーク「軽い足回り」：いつも少し速い。
    if (HasPerk(survivor::PerkId::Speed)) {
        moveScale *= 1.0f + PerkValue(survivor::PerkId::Speed);
    }
    heroWorld_.resources.player_->SetSurvivorMoveScale(moveScale * HeroBaseMoveScale());
    if (const survivor::AbilityDefinition* zone = FindAbility(AbilityKind::HealZone)) {
        UpdateHealZone(dt, *zone);
    }
    if (const survivor::AbilityDefinition* barrage = FindAbility(AbilityKind::Barrage)) {
        UpdateBarrage(dt, *barrage);
    }
    if (Hero().primaryAbility.kind == AbilityKind::DrainBeam) {
        UpdateDrainBeam(dt, Hero().primaryAbility);
    }
    if (Hero().primaryAbility.kind == AbilityKind::BurstGun) {
        UpdateBurstGun(dt, Hero().primaryAbility);
    }
    if (const survivor::AbilityDefinition* heal = FindAbility(AbilityKind::SelfHeal)) {
        UpdateSelfHeal(dt, *heal);
    }
    if (const survivor::AbilityDefinition* orb = FindAbility(AbilityKind::BounceOrb)) {
        UpdateBounceOrb(dt, *orb);
    }
    if (const survivor::AbilityDefinition* lance = FindAbility(AbilityKind::Lance)) {
        UpdateLance(dt, *lance);
    }
    if (const survivor::AbilityDefinition* charge = FindAbility(AbilityKind::Charge)) {
        UpdateCharge(dt, *charge);
    }
    if (const survivor::AbilityDefinition* fortress = FindAbility(AbilityKind::Fortress)) {
        UpdateFortress(dt, *fortress);
    }
    shockAge_ += dt;
    UpdateHeroPerks(dt);
    UpdateSubWeapons(dt);
    // 押している間の入力は、次に UpdateHeroInput が見るまで離したことにする（持ち物の画面を開いている間など）。
    primaryHeld_ = false;
    secondaryHeld_ = false;
    // ロケット（弾ごとの爆発）の爆発と倒した数。区画を出ると集計が0に戻るので、差だけを足す。
    const BulletManager::ShooterStats& shooter = heroWorld_.resources.bulletManager_->GetShooterStats();
    lastBulletBlasts_ = (std::min)(lastBulletBlasts_, shooter.bulletBlasts);
    lastBulletBlastKills_ = (std::min)(lastBulletBlastKills_, shooter.bulletBlastKills);
    rocketBlasts_ += shooter.bulletBlasts - lastBulletBlasts_;
    rocketKills_ += shooter.bulletBlastKills - lastBulletBlastKills_;
    // パーク「ロケットの再装填」：爆発で倒した数だけ、ロケットの待ち時間を縮める。
    if (HasPerk(survivor::PerkId::RocketRefund) && shooter.bulletBlastKills > lastBulletBlastKills_) {
        float& cooldown = abilityCooldown_[static_cast<size_t>(AbilitySlot::Secondary)];
        cooldown = (std::max)(0.0f, cooldown - PerkValue(survivor::PerkId::RocketRefund) *
                                                   static_cast<float>(shooter.bulletBlastKills - lastBulletBlastKills_));
    }
    lastBulletBlasts_ = shooter.bulletBlasts;
    lastBulletBlastKills_ = shooter.bulletBlastKills;
    if (ultimateCharge_ >= Hero().ultimateCharge) {
        ultimatePulse_ += dt;
    }
    // 自機のまわりの表示：右クリック・Shift・E は使えるまでの進み（押している間の技はスタミナ）、Q はゲージ。
    auto& presentation = heroWorld_.presentation;
    for (size_t i = 0; i < presentation.heroAbilityReady_.size(); ++i) {
        const survivor::AbilityDefinition& ability = Hero().abilities[i];
        if (ability.kind == AbilityKind::None) {
            presentation.heroAbilityReady_[i] = -1.0f;
        } else if (ability.kind == AbilityKind::SelfHeal) {
            presentation.heroAbilityReady_[i] = absorb_ / (std::max)(0.001f, ability.duration);
        } else if (survivor::IsHeldAbility(ability.kind)) {
            presentation.heroAbilityReady_[i] =
                barrierExhausted_ > 0.0f ? 0.0f : barrierStamina_ / (std::max)(1.0f, HeroBarrierSettings().staminaMax);
        } else {
            presentation.heroAbilityReady_[i] =
                abilityCooldown_[i] > 0.0f && ability.cooldown > 0.0f ? 1.0f - abilityCooldown_[i] / ability.cooldown : 1.0f;
        }
    }
    presentation.heroUltimateRatio_ = (std::clamp)(ultimateCharge_ / Hero().ultimateCharge, 0.0f, 1.0f);
    presentation.heroUltimateGlow_ = 0.75f + 0.25f * std::sin(ultimatePulse_ * 8.0f);
}

void HeroAbilityCore::AddUltimateCharge(float amount)
{
    // アルティメットの間は溜めない（使っている間に次が溜まらないように）。
    if (!HeroActive() || amount <= 0.0f || UltimateActive()) {
        return;
    }
    const float full = Hero().ultimateCharge;
    const bool wasFull = ultimateCharge_ >= full;
    // パーク「見せ場の予感」：溜まり方が増える。
    if (HasPerk(survivor::PerkId::UltCharge)) {
        amount *= 1.0f + PerkValue(survivor::PerkId::UltCharge);
    }
    ultimateCharge_ = (std::min)(full, ultimateCharge_ + amount);
    if (!wasFull && ultimateCharge_ >= full) {
        ++ultimateFills_;
        ultimatePulse_ = 0.0f;
        if (ultimateFullAt_ < 0.0f) {
            ultimateFullAt_ = HeroClock();
        }
    }
}

const survivor::AbilityDefinition* HeroAbilityCore::FindAbility(AbilityKind kind) const
{
    for (const survivor::AbilityDefinition& ability : Hero().abilities) {
        if (ability.kind == kind) {
            return &ability;
        }
    }
    return nullptr;
}

void HeroAbilityCore::UpdateHealZone(float dt, const survivor::AbilityDefinition& ability)
{
    if (healZoneTimer_ <= 0.0f) {
        return;
    }
    healZoneTimer_ = (std::max)(0.0f, healZoneTimer_ - dt);
    healZoneAge_ += dt;
    auto& player = *heroWorld_.resources.player_;
    const myEngine::Vector3 offset = player.GetWorldPosition() - healZonePosition_;
    // 強化の3段目：回復の場が自機について来る。
    if (const survivor::AbilityDefinition* zone = FindAbility(AbilityKind::HealZone); zone == &ability && UpgradeTier(3) >= 3) {
        healZonePosition_ = player.GetWorldPosition();
    }
    if (myEngine::Length(myEngine::Vector3{offset.x, offset.y, 0.0f}) > ability.radius && UpgradeTier(3) < 3 || player.IsDead()) {
        return;
    }
    healZoneInsideSeconds_ += dt;
    // 毎秒 最大HPの power 倍。1未満の端数は貯めて、整数になった分だけ回復する。
    healZoneRemainder_ += static_cast<float>(player.GetMaxHp()) * ability.power * dt;
    const int amount = static_cast<int>(healZoneRemainder_);
    if (amount <= 0) {
        return;
    }
    healZoneRemainder_ -= static_cast<float>(amount);
    const int before = player.GetHp();
    player.HealRunPlayer(amount);
    const int healed = player.GetHp() - before;
    healedByZone_ += healed;
    // パーク「野戦の手当て」：回復した量の一部だけ、傷も治る。
    if (HasPerk(survivor::PerkId::FieldMedic)) {
        player.ReduceSurvivorWound(static_cast<float>(healed) * PerkValue(survivor::PerkId::FieldMedic));
    }
    // 回復した量も Q のゲージになる（H5）。
    AddUltimateCharge(static_cast<float>(healed));
}

void HeroAbilityCore::UpdateBarrage(float dt, const survivor::AbilityDefinition& ability)
{
    if (barrageTimer_ <= 0.0f) {
        return;
    }
    barrageTimer_ = (std::max)(0.0f, barrageTimer_ - dt);
    barrageSeconds_ += dt;
    barrageShotTimer_ -= dt;
    auto& player = *heroWorld_.resources.player_;
    const myEngine::Vector3 origin = player.GetWorldPosition();
    const float interval = 1.0f / (std::max)(1.0f, ability.rate);
    while (barrageShotTimer_ <= 0.0f) {
        barrageShotTimer_ += interval;
        // 狙う範囲の中の近い敵を8体まで集め、順に狙う（同じ敵ばかりに撃たない）。ボスがいれば候補に入れる。
        std::array<std::pair<float, myEngine::Vector3>, 9> targets{};
        size_t count = 0;
        for (const ExpEnemy* enemy : heroWorld_.resources.enemyManager_->GetEnemyPtrs()) {
            if (!enemy || enemy->IsDead()) {
                continue;
            }
            const myEngine::Vector3 position = enemy->GetWorldPosition();
            const float distance = myEngine::Length(myEngine::Vector3{position.x - origin.x, position.y - origin.y, 0.0f});
            if (distance > ability.radius) {
                continue;
            }
            if (count < 8) {
                targets[count++] = {distance, position};
            } else {
                auto farthest = std::max_element(targets.begin(), targets.begin() + 8, [](const auto& a, const auto& b) {
                    return a.first < b.first;
                });
                if (distance < farthest->first) {
                    *farthest = {distance, position};
                }
            }
        }
        if (HeroBossTargetActive()) {
            const myEngine::Vector3 position = heroWorld_.resources.enemy_->GetWorldPosition();
            const float distance = myEngine::Length(myEngine::Vector3{position.x - origin.x, position.y - origin.y, 0.0f});
            if (distance <= ability.radius) {
                targets[count++] = {distance, position};
            }
        }
        if (count == 0) {
            continue;
        }
        std::sort(targets.begin(), targets.begin() + count, [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        const myEngine::Vector3 target = targets[barrageTarget_++ % count].second;
        Player::SurvivorShot shot;
        shot.direction = myEngine::Vector3{target.x - origin.x, target.y - origin.y, 0.0f};
        shot.damageScale = ability.power * HeroPowerScale();
        shot.speedScale = 1.8f;
        shot.size = 0.8f;
        shot.color = {2.2f, 1.7f, 0.35f, 1.0f};
        // 強化の3段目：弾幕の弾が2体を貫く。
        shot.pierce = UpgradeTier(4) >= 3 ? 2 : 0;
        if (player.FireSurvivorShot(shot)) {
            ++barrageShots_;
        }
    }
}

void HeroAbilityCore::QueueHeroVisuals(myEngine::NeonGridRenderer& renderer, const myEngine::Vector3& cameraForward) const
{
    if (!HeroActive()) {
        return;
    }
    auto circle = [&](const myEngine::Vector3& center, float radius, float width, const myEngine::Vector4& color, float phase) {
        constexpr int kSides = 40;
        for (int n = 0; n < kSides; ++n) {
            const float a = phase + 6.2831853f * static_cast<float>(n) / kSides,
                        b = phase + 6.2831853f * static_cast<float>(n + 1) / kSides;
            renderer.QueueLine(center + myEngine::Vector3{std::cos(a) * radius, std::sin(a) * radius, -0.3f},
                               center + myEngine::Vector3{std::cos(b) * radius, std::sin(b) * radius, -0.3f}, width, color);
        }
    };
    // 回復の場：緑の輪と、中へ縮んでいく輪。残りが1秒を切ると点滅する。
    if (const survivor::AbilityDefinition* zone = FindAbility(AbilityKind::HealZone); zone && healZoneTimer_ > 0.0f) {
        const float blink = healZoneTimer_ < 1.0f ? 0.5f + 0.5f * std::sin(healZoneAge_ * 30.0f) : 1.0f;
        circle(healZonePosition_, zone->radius, 0.2f, {0.45f, 1.9f, 0.7f, blink}, 0.0f);
        const float inner = zone->radius * (1.0f - std::fmod(healZoneAge_ * 0.8f, 1.0f));
        circle(healZonePosition_, inner, 0.12f, {0.35f, 1.4f, 0.55f, 0.6f * blink}, 0.0f);
    }
    // 弾幕：自機のまわりを回る金色の輪。
    const myEngine::Vector3 self = heroWorld_.resources.player_->GetWorldPosition();
    if (barrageTimer_ > 0.0f) {
        circle(self, 2.6f, 0.16f, {2.2f, 1.7f, 0.35f, 0.9f}, barrageSeconds_ * 3.0f);
    }
    const myEngine::Vector3 aim =
        myEngine::NormalizeOr(heroWorld_.resources.player_->GetDirection(), myEngine::Vector3{1.0f, 0.0f, 0.0f}, 0.001f);
    const myEngine::Vector3 lift{0.0f, 0.0f, -0.3f};
    // 吸い付く光線：吸い付いている間は濃い紫、外れている間は薄く細い。
    if (beamOn_) {
        const float width = (std::max)(0.05f, Hero().primaryAbility.size);
        const float wobble = 0.85f + 0.15f * std::sin(beamAge_ * 40.0f);
        const myEngine::Vector3 start = self + aim * 1.1f + lift, end = beamEnd_ + lift;
        // 太い線は平らな四角だと見る向きで割れて見えるので、カメラを向いた輪郭の線（芯・肩・にじみ）で描く。
        if (beamTarget_ != 0) {
            renderer.QueueContourLine(start, end, width * wobble, {1.5f, 0.55f, 2.0f, 0.95f}, cameraForward);
            if (beamChainOn_) {
                renderer.QueueContourLine(end, beamChainEnd_ + lift, width * 0.6f, {1.3f, 0.5f, 1.8f, 0.8f}, cameraForward);
            }
        } else {
            renderer.QueueContourLine(start, end, width * 0.5f, {0.8f, 0.35f, 1.1f, 0.4f}, cameraForward);
        }
    }
    // 自分を回復：自機のまわりを回る黄色の輪。
    if (selfHealOn_) {
        circle(self, 1.9f, 0.12f, {1.9f, 1.6f, 0.5f, 0.85f}, beamAge_ * 4.0f);
    }
    // 消える移動：自機のまわりで縮む紫の輪。
    if (const survivor::AbilityDefinition* phase = FindAbility(AbilityKind::Phase); phase && phaseTimer_ > 0.0f) {
        const float t = phaseTimer_ / (std::max)(0.001f, phase->duration);
        circle(self, 1.2f + 1.4f * t, 0.14f, {1.3f, 0.5f, 2.2f, 0.4f + 0.5f * t}, 0.0f);
    }
    // 跳ねる球：紫の玉（輪を重ねる）。
    if (const survivor::AbilityDefinition* orb = FindAbility(AbilityKind::BounceOrb); orb && orbTimer_ > 0.0f) {
        const float radius = (std::max)(0.2f, orb->size) * orbScale_;
        circle(orbPosition_, radius, 0.18f, {1.6f, 0.45f, 2.3f, 0.95f}, orbTimer_ * 6.0f);
        circle(orbPosition_, radius * 0.55f, 0.22f, {2.0f, 0.9f, 2.4f, 0.9f}, -orbTimer_ * 9.0f);
    }
    // 突進：自機のまわりの橙の輪。衝撃波：広がって薄れる輪。大きな盾：自機のまわりの全周の輪（残りが少ないと点滅）。
    if (chargeTimer_ > 0.0f) {
        circle(self, 1.6f, 0.2f, {2.2f, 1.1f, 0.3f, 0.9f}, chargeTimer_ * 20.0f);
    }
    if (shockAge_ < 0.45f) {
        const float t = shockAge_ / 0.45f;
        circle(shockCenter_, shockRadius_ * (0.3f + 0.7f * t), 0.3f * (1.0f - t) + 0.05f, {1.8f, 1.5f, 0.6f, 1.0f - t}, 0.0f);
    }
    if (const survivor::AbilityDefinition* fortress = FindAbility(AbilityKind::Fortress); fortress && fortressTimer_ > 0.0f) {
        const float blink = fortressTimer_ < 1.0f ? 0.5f + 0.5f * std::sin(fortressAge_ * 30.0f) : 1.0f;
        circle(self, fortress->radius, 0.22f, {0.55f, 1.6f, 1.9f, 0.9f * blink}, fortressAge_ * 0.8f);
        circle(self, fortress->radius * 0.94f, 0.08f, {0.4f, 1.2f, 1.5f, 0.5f * blink}, -fortressAge_ * 1.3f);
    }
    // 太い光線：向いている方へ、太い線と外側のにじみ。
    if (const survivor::AbilityDefinition* lance = FindAbility(AbilityKind::Lance); lance && lanceTimer_ > 0.0f) {
        const float width = (std::max)(0.2f, lance->size) * (0.9f + 0.1f * std::sin(lanceAge_ * 30.0f));
        const myEngine::Vector3 start = self + aim * 1.1f + lift, end = self + aim * lance->radius + lift;
        renderer.QueueContourLine(start, end, width, {1.7f, 0.8f, 2.2f, 0.95f}, cameraForward);
    }
    QueueSubWeapons(renderer);
}

namespace {
/// @brief 機体の技で威力を与える間隔（秒）。押している間の光線・跳ねる球・太い光線で使う。
constexpr float kHeroTick = 0.1f;
/// @brief 宿敵に吸い付いているときの id。
constexpr uint64_t kRivalTarget = ~0ull;

/// @brief 点 p と線分 a-b の、平面での距離。
float SegmentDistance(const myEngine::Vector3& a, const myEngine::Vector3& b, const myEngine::Vector3& p)
{
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float lengthSq = dx * dx + dy * dy;
    const float t = lengthSq > 0.0001f ? (std::clamp)(((p.x - a.x) * dx + (p.y - a.y) * dy) / lengthSq, 0.0f, 1.0f) : 0.0f;
    const float x = a.x + dx * t - p.x, y = a.y + dy * t - p.y;
    return std::sqrt(x * x + y * y);
}

float PlanarDistance(const myEngine::Vector3& a, const myEngine::Vector3& b)
{
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}
} // namespace

float HeroAbilityCore::HeroDamageUnit(float scale) const
{
    return heroWorld_.resources.player_->GetStats().bulletDamage * (std::max)(0.0f, scale) * HeroPowerScale();
}

uint32_t HeroAbilityCore::DealHeroDamage(Collider* target, bool rival, uint32_t amount, const myEngine::Vector3& from)
{
    if (!target || amount == 0) {
        return 0;
    }
    if (rival) {
        static_cast<Enemy*>(target)->TakeDamage(amount);
        return amount;
    }
    auto* enemy = static_cast<ExpEnemy*>(target);
    if (enemy->IsDead()) {
        return 0;
    }
    // 会心（計画書 p8 の F4）：衛星・火炎放射・衝撃の輪・光線など、弾を使わない削りにも効く。
    const float crit = heroWorld_.resources.player_->RollSurvivorCrit();
    if (crit > 1.0f) {
        amount = static_cast<uint32_t>(std::round(static_cast<float>(amount) * crit));
    }
    // 会心の数字（計画書 p9 の H2）。
    ExpEnemy::SetNextHitCritical(crit > 1.0f);
    enemy->TakeDirectionalDamage(amount, from);
    ExpEnemy::SetNextHitCritical(false);
    return amount;
}

void HeroAbilityCore::UpdateHeroPerks(float dt)
{
    using survivor::PerkId;
    auto& player = *heroWorld_.resources.player_;
    Player::SurvivorPerkEffects effects;
    effects.critChance = HasPerk(PerkId::Crit) ? (std::min)(0.9f, PerkValue(PerkId::Crit)) : 0.0f;
    effects.critScale = survivor::kCritScale;
    effects.areaScale = HeroAreaScale();
    player.SetSurvivorPerkEffects(effects);
    // 反応装甲：防いだら光らせ、間隔のあいだ待ってから構え直す。
    if (player.GetSurvivorGuardBlocks() != guardSeen_) {
        guardSeen_ = player.GetSurvivorGuardBlocks();
        guardFlash_ = 0.35f;
        guardTimer_ = 0.0f;
    }
    guardFlash_ = (std::max)(0.0f, guardFlash_ - dt);
    if (HasPerk(PerkId::Guard) && !player.IsSurvivorGuardReady()) {
        guardTimer_ += dt;
        if (guardTimer_ >= survivor::GuardCooldown(PerkValue(PerkId::Guard))) {
            guardTimer_ = 0.0f;
            player.SetSurvivorGuardReady(true);
        }
    }
    // 予備の動力：取ったときに1回ぶん渡す。立ち直ったら、まわりを押し返して敵の弾も消す。
    if (HasPerk(PerkId::Revive) && !reviveGranted_) {
        reviveGranted_ = true;
        player.SetSurvivorRevives(1, PerkValue(PerkId::Revive));
    }
    if (player.ConsumeSurvivorRevived()) {
        TriggerShockwave(player.GetWorldPosition(), 6.0f, HeroDamageUnit(3.0f), 1.2f, true);
        heroWorld_.combatFlow->SetEventCallout("予備の動力で立ち直った", 1.5f);
    }
}

void HeroAbilityCore::AddAbsorb(float amount)
{
    const survivor::AbilityDefinition* heal = FindAbility(AbilityKind::SelfHeal);
    if (!heal || amount <= 0.0f) {
        return;
    }
    const float before = absorb_;
    absorb_ = (std::min)(heal->duration, absorb_ + amount);
    absorbGained_ += absorb_ - before;
}

void HeroAbilityCore::UpdateDrainBeam(float dt, const survivor::AbilityDefinition& ability)
{
    auto& player = *heroWorld_.resources.player_;
    beamAge_ += dt;
    beamOn_ = primaryHeld_ && !player.IsDead();
    if (!beamOn_) {
        beamTarget_ = 0;
        beamTick_ = 0.0f;
        return;
    }
    const myEngine::Vector3 origin = player.GetWorldPosition();
    const myEngine::Vector3 aim = myEngine::NormalizeOr(player.GetDirection(), myEngine::Vector3{1.0f, 0.0f, 0.0f}, 0.001f);
    // 照準の近く（今の相手は広め）で、届く距離の中の敵を選ぶ。角度の差を主に、遠さも少し見る。
    Collider* best = nullptr;
    uint64_t bestId = 0;
    float bestScore = 1e9f;
    auto consider = [&](Collider* target, uint64_t id) {
        const myEngine::Vector3 position = target->GetWorldPosition();
        const float distance = PlanarDistance(origin, position);
        const bool current = id == beamTarget_;
        if (distance > ability.radius * (current ? 1.15f : 1.0f) + target->GetRadius()) {
            return;
        }
        const float cosine = distance > 0.001f ? ((position.x - origin.x) * aim.x + (position.y - origin.y) * aim.y) / distance : 1.0f;
        if (cosine < (current ? 0.5f : 0.8f)) {
            return;
        }
        const float score = (1.0f - cosine) + 0.25f * distance / (std::max)(1.0f, ability.radius) - (current ? 0.15f : 0.0f);
        if (score < bestScore) {
            bestScore = score;
            best = target;
            bestId = id;
        }
    };
    for (ExpEnemy* enemy : heroWorld_.resources.enemyManager_->GetEnemyPtrs()) {
        if (enemy && !enemy->IsDead() && !enemy->IsRunResource()) {
            consider(enemy, enemy->GetCollisionId());
        }
    }
    if (HeroBossTargetActive()) {
        consider(heroWorld_.resources.enemy_.get(), kRivalTarget);
    }
    if (!best) {
        // 外れ：照準の方へ届く距離まで薄く出すだけ。
        beamTarget_ = 0;
        beamEnd_ = origin + aim * ability.radius;
        return;
    }
    beamTarget_ = bestId;
    beamEnd_ = best->GetWorldPosition();
    beamLockedSeconds_ += dt;
    beamTick_ -= dt;
    while (beamTick_ <= 0.0f) {
        beamTick_ += kHeroTick;
        const auto amount = static_cast<uint32_t>((std::max)(1.0f, std::round(HeroDamageUnit(ability.power) * kHeroTick)));
        const uint32_t dealt = DealHeroDamage(best, bestId == kRivalTarget, amount, origin);
        drainDamage_ += dealt;
        AddAbsorb(static_cast<float>(dealt) * ability.gain);
        // パーク「吸い尽くす」：吸収ビームで倒すと、吸収ゲージが溜まる。
        if (dealt > 0 && bestId != kRivalTarget && static_cast<ExpEnemy*>(best)->IsDead() && HasPerk(survivor::PerkId::BeamKill)) {
            AddAbsorb(PerkValue(survivor::PerkId::BeamKill));
        }
        // 強化の3段目：先から近くのもう1体へ跳ね、半分の威力を与える。
        beamChainOn_ = false;
        if (UpgradeTier(0) >= 3) {
            const myEngine::Vector3 from = best->GetWorldPosition();
            ExpEnemy* chained = nullptr;
            float nearest = 4.0f;
            for (ExpEnemy* enemy : heroWorld_.resources.enemyManager_->GetEnemyPtrs()) {
                if (!enemy || enemy->IsDead() || enemy->IsRunResource() || enemy == best) {
                    continue;
                }
                const float distance = PlanarDistance(from, enemy->GetWorldPosition());
                if (distance < nearest) {
                    nearest = distance;
                    chained = enemy;
                }
            }
            if (chained) {
                beamChainOn_ = true;
                beamChainEnd_ = chained->GetWorldPosition();
                const uint32_t extra = DealHeroDamage(chained, false, (std::max)(1u, amount / 2), from);
                drainDamage_ += extra;
                AddAbsorb(static_cast<float>(extra) * ability.gain);
            }
        }
        if (dealt == 0) {
            beamTarget_ = 0;
            beamTick_ = 0.0f;
            break;
        }
    }
}

void HeroAbilityCore::UpdateSelfHeal(float dt, const survivor::AbilityDefinition& ability)
{
    auto& player = *heroWorld_.resources.player_;
    selfHealOn_ = secondaryHeld_ && absorb_ > 0.0f && !player.IsDead() && player.GetHp() < player.GetMaxHp();
    if (!selfHealOn_ || dt <= 0.0f) {
        return;
    }
    // 押している間、吸収ゲージを毎秒1秒ぶん使い、毎秒 最大HPの power 倍を回復する（端数は貯める）。
    const float use = (std::min)(absorb_, dt);
    absorb_ -= use;
    selfHealSeconds_ += use;
    selfHealRemainder_ += static_cast<float>(player.GetMaxHp()) * ability.power * use;
    const int amount = static_cast<int>(selfHealRemainder_);
    if (amount <= 0) {
        return;
    }
    selfHealRemainder_ -= static_cast<float>(amount);
    const int before = player.GetHp();
    player.HealRunPlayer(amount);
    const int healed = player.GetHp() - before;
    selfHealed_ += healed;
    // 強化の3段目：回復した量の半分だけ、傷も治る。
    if (UpgradeTier(1) >= 3) {
        player.ReduceSurvivorWound(static_cast<float>(healed) * 0.5f);
    }
    // 回復した量も Q のゲージになる（H5）。
    AddUltimateCharge(static_cast<float>(healed));
}

void HeroAbilityCore::UpdateBounceOrb(float dt, const survivor::AbilityDefinition& ability)
{
    if (orbTimer_ <= 0.0f || dt <= 0.0f) {
        return;
    }
    orbTimer_ = (std::max)(0.0f, orbTimer_ - dt);
    orbPosition_ += orbVelocity_ * dt;
    const myEngine::Vector3 self = heroWorld_.resources.player_->GetWorldPosition();
    // 自機から離れすぎたら、自機の方へ戻す（壁の代わり。画面の外へ行かない）。
    if (PlanarDistance(orbPosition_, self) > 14.0f) {
        const myEngine::Vector3 back = myEngine::NormalizeOr(myEngine::Vector3{self.x - orbPosition_.x, self.y - orbPosition_.y, 0.0f},
                                                             myEngine::Vector3{1.0f, 0.0f, 0.0f}, 0.001f);
        orbVelocity_ = back * ability.rate;
        orbLastBounce_ = 0;
    }
    const float radius = (std::max)(0.2f, ability.size) * orbScale_;
    orbTick_ -= dt;
    const bool tick = orbTick_ <= 0.0f;
    if (tick) {
        orbTick_ += kHeroTick * 2.0f;
    }
    const auto tickDamage = static_cast<uint32_t>((std::max)(1.0f, std::round(HeroDamageUnit(ability.power) * kHeroTick * 2.0f)));
    const auto touchDamage = static_cast<uint32_t>((std::max)(1.0f, std::round(HeroDamageUnit(ability.power) * 0.4f)));
    Collider* bounced = nullptr;
    uint64_t bouncedId = 0;
    auto touch = [&](Collider* target, uint64_t id, bool rival) {
        if (PlanarDistance(orbPosition_, target->GetWorldPosition()) > radius + target->GetRadius()) {
            return;
        }
        // 初めて触れた敵には強めに当てて跳ね、触れ続けている敵には間隔ごとに当てる。
        uint32_t amount = tick ? tickDamage : 0;
        if (!bounced && id != orbLastBounce_) {
            bounced = target;
            bouncedId = id;
            amount += touchDamage;
        }
        const uint32_t dealt = DealHeroDamage(target, rival, amount, orbPosition_);
        orbDamage_ += dealt;
        // パーク「跳ねる吸収」：球の威力でも吸収ゲージが溜まる（吸収ビームの溜まり方の value 倍）。
        if (HasPerk(survivor::PerkId::OrbAbsorb)) {
            AddAbsorb(static_cast<float>(dealt) * Hero().primaryAbility.gain * PerkValue(survivor::PerkId::OrbAbsorb));
        }
    };
    for (ExpEnemy* enemy : heroWorld_.resources.enemyManager_->GetEnemyPtrs()) {
        if (enemy && !enemy->IsDead() && !enemy->IsRunResource()) {
            touch(enemy, enemy->GetCollisionId(), false);
        }
    }
    if (HeroBossTargetActive()) {
        touch(heroWorld_.resources.enemy_.get(), kRivalTarget, true);
    }
    if (!bounced) {
        return;
    }
    // 次に近い敵（今跳ねた敵の他）へ向きを変える。いなければ跳ね返る。
    ++orbBounces_;
    // 強化の3段目：跳ねるたびに球が大きくなる（2倍まで）。
    if (UpgradeTier(3) >= 3) {
        orbScale_ = (std::min)(2.0f, orbScale_ * 1.08f);
    }
    orbLastBounce_ = bouncedId;
    const myEngine::Vector3 from = bounced->GetWorldPosition();
    float nearest = 10.0f;
    myEngine::Vector3 next{};
    bool found = false;
    for (const ExpEnemy* enemy : heroWorld_.resources.enemyManager_->GetEnemyPtrs()) {
        if (!enemy || enemy->IsDead() || enemy->IsRunResource() || enemy->GetCollisionId() == bouncedId) {
            continue;
        }
        const float distance = PlanarDistance(from, enemy->GetWorldPosition());
        if (distance < nearest) {
            nearest = distance;
            next = enemy->GetWorldPosition();
            found = true;
        }
    }
    const myEngine::Vector3 direction = found ? myEngine::Vector3{next.x - orbPosition_.x, next.y - orbPosition_.y, 0.0f}
                                              : myEngine::Vector3{orbPosition_.x - from.x, orbPosition_.y - from.y, 0.0f};
    orbVelocity_ = myEngine::NormalizeOr(direction, myEngine::Vector3{-orbVelocity_.x, -orbVelocity_.y, 0.0f}, 0.001f) * ability.rate;
}

void HeroAbilityCore::UpdateLance(float dt, const survivor::AbilityDefinition& ability)
{
    if (lanceTimer_ <= 0.0f || dt <= 0.0f) {
        return;
    }
    lanceTimer_ = (std::max)(0.0f, lanceTimer_ - dt);
    lanceAge_ += dt;
    lanceTick_ -= dt;
    if (lanceTick_ > 0.0f) {
        return;
    }
    lanceTick_ += kHeroTick;
    auto& player = *heroWorld_.resources.player_;
    const myEngine::Vector3 origin = player.GetWorldPosition();
    const myEngine::Vector3 aim = myEngine::NormalizeOr(player.GetDirection(), myEngine::Vector3{1.0f, 0.0f, 0.0f}, 0.001f);
    const myEngine::Vector3 start = origin + aim * 1.1f, end = origin + aim * ability.radius;
    const auto amount = static_cast<uint32_t>((std::max)(1.0f, std::round(HeroDamageUnit(ability.power) * kHeroTick)));
    int hits = 0;
    auto hit = [&](Collider* target, bool rival) {
        if (SegmentDistance(start, end, target->GetWorldPosition()) > ability.size * 0.5f + target->GetRadius()) {
            return;
        }
        const uint32_t dealt = DealHeroDamage(target, rival, amount, origin);
        lanceDamage_ += dealt;
        AddAbsorb(static_cast<float>(dealt) * ability.gain);
        hits += dealt > 0 ? 1 : 0;
    };
    for (ExpEnemy* enemy : heroWorld_.resources.enemyManager_->GetEnemyPtrs()) {
        if (enemy && !enemy->IsDead() && !enemy->IsRunResource()) {
            hit(enemy, false);
        }
    }
    if (HeroBossTargetActive()) {
        hit(heroWorld_.resources.enemy_.get(), true);
    }
    lanceMaxHits_ = (std::max)(lanceMaxHits_, hits);
}

int HeroAbilityCore::TriggerShockwave(const myEngine::Vector3& origin, float radius, float damage, float strength, bool clearBullets)
{
    radius *= HeroAreaScale();
    shockCenter_ = origin;
    shockRadius_ = radius;
    shockAge_ = 0.0f;
    int pushed = 0;
    const auto amount = static_cast<uint32_t>((std::max)(1.0f, std::round(damage)));
    for (ExpEnemy* enemy : heroWorld_.resources.enemyManager_->GetEnemyPtrs()) {
        if (!enemy || enemy->IsDead() || enemy->IsRunResource()) {
            continue;
        }
        const myEngine::Vector3 position = enemy->GetWorldPosition();
        const myEngine::Vector3 away{position.x - origin.x, position.y - origin.y, 0.0f};
        if (myEngine::Length(away) > radius + enemy->GetRadius()) {
            continue;
        }
        enemy->ApplyKnockback(myEngine::Length(away) > 0.001f ? away : myEngine::Vector3{1.0f, 0.0f, 0.0f}, strength);
        shockwaveDamage_ += DealHeroDamage(enemy, false, amount, origin);
        ++pushed;
    }
    if (HeroBossTargetActive()) {
        Enemy& rival = *heroWorld_.resources.enemy_;
        const myEngine::Vector3 position = rival.GetWorldPosition();
        const myEngine::Vector3 away{position.x - origin.x, position.y - origin.y, 0.0f};
        if (myEngine::Length(away) <= radius + rival.GetRadius()) {
            rival.ApplyKnockback(myEngine::Length(away) > 0.001f ? away : myEngine::Vector3{1.0f, 0.0f, 0.0f}, strength * 0.5f);
            shockwaveDamage_ += DealHeroDamage(&rival, true, amount, origin);
            ++pushed;
        }
    }
    if (clearBullets) {
        for (Bullet* bullet : heroWorld_.resources.bulletManager_->GetBulletPtrs()) {
            if (bullet && !bullet->IsDead() && bullet->GetOwner() != kPlayer &&
                myEngine::Length(myEngine::Vector3{bullet->GetWorldPosition().x - origin.x, bullet->GetWorldPosition().y - origin.y, 0.0f}) <= radius) {
                bullet->ApplyBulletDurabilityDamage(1000000.0f);
            }
        }
    }
    return pushed;
}

void HeroAbilityCore::UpdateCharge(float dt, const survivor::AbilityDefinition& ability)
{
    if (chargeTimer_ <= 0.0f || dt <= 0.0f) {
        return;
    }
    chargeTimer_ = (std::max)(0.0f, chargeTimer_ - dt);
    auto& player = *heroWorld_.resources.player_;
    const myEngine::Vector3 self = player.GetWorldPosition();
    const auto amount = static_cast<uint32_t>((std::max)(1.0f, std::round(HeroDamageUnit(ability.size))));
    // 触れた敵は、1回の突進で1回だけ押し飛ばして傷つける。
    for (ExpEnemy* enemy : heroWorld_.resources.enemyManager_->GetEnemyPtrs()) {
        if (!enemy || enemy->IsDead() || enemy->IsRunResource()) {
            continue;
        }
        const myEngine::Vector3 position = enemy->GetWorldPosition();
        const myEngine::Vector3 away{position.x - self.x, position.y - self.y, 0.0f};
        if (myEngine::Length(away) > player.GetRadius() + enemy->GetRadius() + 0.8f ||
            std::find(chargeHit_.begin(), chargeHit_.end(), enemy->GetCollisionId()) != chargeHit_.end()) {
            continue;
        }
        chargeHit_.push_back(enemy->GetCollisionId());
        enemy->ApplyKnockback(myEngine::Length(away) > 0.001f ? away : player.GetDirection(), ability.rate);
        chargeDamage_ += DealHeroDamage(enemy, false, amount, self);
        ++chargePushes_;
    }
    // パーク「突進の衝撃」：突進の終わりに小さな衝撃波。
    if (chargeTimer_ <= 0.0f && HasPerk(survivor::PerkId::ChargeShock)) {
        chargePushes_ += TriggerShockwave(self, PerkValue(survivor::PerkId::ChargeShock), HeroDamageUnit(ability.size), ability.rate, false);
    }
}

void HeroAbilityCore::UpdateFortress(float dt, const survivor::AbilityDefinition& ability)
{
    if (fortressTimer_ <= 0.0f || dt <= 0.0f) {
        return;
    }
    fortressTimer_ = (std::max)(0.0f, fortressTimer_ - dt);
    fortressAge_ += dt;
    auto& player = *heroWorld_.resources.player_;
    const myEngine::Vector3 center = player.GetWorldPosition();
    // 全周のバリア：入ってきた敵の弾を消して数え、敵は縁まで押し出す。
    for (Bullet* bullet : heroWorld_.resources.bulletManager_->GetBulletPtrs()) {
        if (!bullet || bullet->IsDead() || bullet->GetOwner() == kPlayer) {
            continue;
        }
        const myEngine::Vector3 position = bullet->GetWorldPosition();
        if (myEngine::Length(myEngine::Vector3{position.x - center.x, position.y - center.y, 0.0f}) > ability.radius + bullet->GetRadius()) {
            continue;
        }
        bullet->ApplyBulletDurabilityDamage(1000000.0f);
        ++fortressBlockedNow_;
        ++fortressBlocked_;
    }
    for (ExpEnemy* enemy : heroWorld_.resources.enemyManager_->GetEnemyPtrs()) {
        if (!enemy || enemy->IsDead() || enemy->IsRunResource()) {
            continue;
        }
        const myEngine::Vector3 position = enemy->GetWorldPosition();
        myEngine::Vector3 away{position.x - center.x, position.y - center.y, 0.0f};
        const float distance = myEngine::Length(away);
        if (distance > ability.radius + enemy->GetRadius()) {
            continue;
        }
        away = distance > 0.001f ? away / distance : player.GetDirection();
        const myEngine::Vector3 pushed = center + away * (ability.radius + enemy->GetRadius());
        if (!heroWorld_.resources.stage_->IsCollisionWithAnyBlock(pushed, enemy->GetRadius())) {
            enemy->SetWorldPosition({pushed.x, pushed.y, position.z});
        }
    }
    // 強化の3段目：効いている間、毎秒 最大HPの3% 戻る。
    if (UpgradeTier(4) >= 3) {
        fortressHealRemainder_ += static_cast<float>(player.GetMaxHp()) * 0.03f * dt;
        const int heal = static_cast<int>(fortressHealRemainder_);
        if (heal > 0) {
            fortressHealRemainder_ -= static_cast<float>(heal);
            player.HealRunPlayer(heal);
        }
    }
    // 終わりに、止めた弾の数だけ強く爆発する。
    if (fortressTimer_ <= 0.0f) {
        const float damage = HeroDamageUnit(ability.power + ability.rate * static_cast<float>(fortressBlockedNow_));
        const uint64_t before = fortressDamage_;
        TriggerShockwave(center, ability.radius * 1.6f, damage, 0.6f, false);
        fortressDamage_ = before + static_cast<uint64_t>(damage);
        ++fortressBlasts_;
    }
}

void HeroAbilityCore::OnHeroKill()
{
    const int max = survivor::BattleSenseMaxStacks(HeroSkillTier(survivor::HeroSkill::BattleSense));
    if (max <= 0) {
        return;
    }
    battleStacks_ = (std::min)(max, battleStacks_ + 1);
    battleStacksMax_ = (std::max)(battleStacksMax_, battleStacks_);
    battleTimer_ = survivor::kBattleSenseSeconds;
}

void HeroAbilityCore::UpdateBurstGun(float dt, const survivor::AbilityDefinition& ability)
{
    survivor::BurstGunStats stats = survivor::BurstGunAt(ability, UpgradeTier(0));
    // 専用スキル（計画書 p8 の F2）。
    auto& self = *heroWorld_.resources.player_;
    const myEngine::Vector3 position = self.GetWorldPosition();
    if (skillHasLastPosition_ && dt > 0.0f) {
        const float dx = position.x - skillLastPosition_.x, dy = position.y - skillLastPosition_.y;
        const float speed = std::sqrt(dx * dx + dy * dy) / dt;
        steadyTimer_ = speed < survivor::kSteadyAimSpeed ? steadyTimer_ + dt : 0.0f;
    }
    skillLastPosition_ = position;
    skillHasLastPosition_ = true;
    const int steadyTier = HeroSkillTier(survivor::HeroSkill::SteadyAim);
    const bool steady = steadyTier > 0 && steadyTimer_ >= survivor::kSteadyAimDelay;
    if (steady) {
        stats.damage *= survivor::SteadyAimDamage(steadyTier);
        if (steadyTier >= 3) {
            stats.spreadDeg *= 0.5f;
        }
    }
    stats.shots += survivor::AmmoBeltShots(HeroSkillTier(survivor::HeroSkill::AmmoBelt));
    battleTimer_ -= dt;
    if (battleTimer_ <= 0.0f) {
        battleStacks_ = 0;
    }
    stats.rate *= 1.0f + survivor::kBattleSensePerStack * static_cast<float>(battleStacks_);
    burstCooldown_ = (std::max)(0.0f, burstCooldown_ - dt);
    if (burstShotsLeft_ <= 0) {
        // 待ち時間が明けて、押していれば次の連射を始める（自動で撃つなら押していることになる）。
        if (burstCooldown_ > 0.0f || !primaryHeld_) {
            return;
        }
        burstShotsLeft_ = stats.shots;
        burstShotTimer_ = 0.0f;
        ++burstVolleys_;
        burstShotsPerVolleyMax_ = (std::max)(burstShotsPerVolleyMax_, stats.shots);
    }
    auto& player = *heroWorld_.resources.player_;
    const myEngine::Vector3 aim = myEngine::NormalizeOr(player.GetDirection(), myEngine::Vector3{1.0f, 0.0f, 0.0f}, 0.001f);
    burstShotTimer_ -= dt;
    while (burstShotsLeft_ > 0 && burstShotTimer_ <= 0.0f) {
        burstShotTimer_ += 1.0f / (std::max)(0.5f, stats.rate);
        // 向いている方から ±spreadDeg 度ばらける（xorshift で十分）。
        burstRandom_ ^= burstRandom_ << 13;
        burstRandom_ ^= burstRandom_ >> 17;
        burstRandom_ ^= burstRandom_ << 5;
        const float unit = static_cast<float>(burstRandom_ & 0xFFFFu) / 65535.0f * 2.0f - 1.0f;
        const float angle = unit * stats.spreadDeg * 3.14159265f / 180.0f;
        const float c = std::cos(angle), s = std::sin(angle);
        Player::SurvivorShot shot;
        shot.direction = myEngine::Vector3{aim.x * c - aim.y * s, aim.x * s + aim.y * c, 0.0f};
        shot.damageScale = stats.damage * HeroPowerScale();
        shot.speedScale = 1.3f;
        shot.size = stats.size;
        shot.pierce = stats.pierce;
        shot.bulletHp = stats.bulletHp;
        shot.blastRadius = stats.blastRadius;
        shot.blastCenter = 1.0f;
        shot.blastSplash = 0.5f;
        shot.burnScale = stats.burnDps;
        shot.burnSeconds = stats.burnSeconds;
        if (player.FireSurvivorShot(shot)) {
            ++burstShots_;
            steadyShots_ += steady ? 1 : 0;
        }
        if (--burstShotsLeft_ <= 0) {
            burstCooldown_ = stats.cooldown;
        }
    }
}

std::string HeroAbilityCore::DescribeHeroAbilities() const
{
    // 画面に収まるよう、ボタンと状態だけにする（技の名前は始めの案内と Tab の画面に出す）。
    std::ostringstream text;
    bool first = true;
    for (size_t i = 0; i < survivor::kAbilitySlotCount; ++i) {
        const auto slot = static_cast<AbilitySlot>(i);
        const survivor::AbilityDefinition& ability = Hero().abilities[i];
        if (ability.kind == AbilityKind::None) {
            continue;
        }
        text << (first ? "" : "　／　") << survivor::AbilitySlotButton(slot) << " ";
        first = false;
        if (slot == AbilitySlot::Ultimate) {
            const float ratio = ultimateCharge_ / Hero().ultimateCharge;
            // 5% 刻み（文字が変わるたびに文字の画像が増えるので、細かく変えない）。
            text << (ratio >= 1.0f ? std::string("使える") : std::to_string(static_cast<int>(ratio * 20.0f) * 5) + "%");
        } else if (ability.kind == AbilityKind::SelfHeal) {
            text << "吸収 " << static_cast<int>(absorb_ / (std::max)(0.001f, ability.duration) * 20.0f) * 5 << "%";
        } else if (survivor::IsHeldAbility(ability.kind)) {
            text << (barrierExhausted_ > 0.0f ? "回復中" : "押して張る");
        } else if (abilityCooldown_[i] > 0.0f) {
            text << static_cast<int>(std::ceil(abilityCooldown_[i])) << "秒";
        } else {
            text << "使える";
        }
    }
    return text.str();
}

} // namespace gameplay
