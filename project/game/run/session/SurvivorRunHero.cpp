#include "game/run/session/SurvivorRunController.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/render/neon/NeonGridRenderer.h"
#include "game/session/GameplaySystems.h"
#include <algorithm>
#include <cmath>

// 育つ戦車の試作の機体（計画書 p4 の K0）：主武器1つと、右クリック・Shift・E・Q の4つの技。
// 技は部品（survivor::AbilityKind）に数値を付けたもので、機体ごとの組み合わせは設定の heroes に書く。

namespace gameplay {

using survivor::AbilityKind;
using survivor::AbilitySlot;

void SurvivorRunController::StartHero()
{
    hero_ = -1;
    if (!config_.weaponsEnabled || !config_.heroes.enabled || config_.heroes.heroes.empty()) {
        return;
    }
    const int found = survivor::FindHero(config_.heroes.heroes, config_.heroes.start);
    const size_t index = found >= 0 ? static_cast<size_t>(found) : 0;
    const int weapon = survivor::FindWeapon(config_.weapons, config_.heroes.heroes[index].primary);
    if (weapon < 0) {
        OutputDebugStringA(("[Survivor] hero primary weapon not found: " + config_.heroes.heroes[index].primary + "\n").c_str());
        return;
    }
    hero_ = static_cast<int>(index);
    // 枠は1つ目だけを使う（コモン・チップなし）。弾は数えない（LoadAmmoToPlayer）。
    for (size_t i = 0; i < weaponSlots_.size(); ++i) {
        runSlots_[i] = {};
        magazines_[i] = 0;
        weaponHeat_[i] = {};
    }
    runSlots_[0].weapon.key = config_.weapons[static_cast<size_t>(weapon)].key;
    SyncSlotWeapons();
    reserves_ = config_.startReserve;
    weaponEquipped_ = false;
    activeSlot_ = 0;
    EquipWeapon(0);
    world_.resources.player_->RestoreHeat({});
    RefreshWeaponSlots();
    ResetHeroState();
}

void SurvivorRunController::ResetHeroState()
{
    abilityCooldown_.fill(0.0f);
    sprintTimer_ = 0.0f;
    ultimateCharge_ = 0.0f;
    ultimatePulse_ = 0.0f;
    healZoneTimer_ = 0.0f;
    healZoneRemainder_ = 0.0f;
    barrageTimer_ = 0.0f;
    world_.resources.player_->SetSurvivorMoveScale(1.0f);
}

void SurvivorRunController::UpdateHeroInput()
{
    auto& input = *world_.resources.input_;
    const bool bot = autoTest_ != AutoTest::None;
    auto pressed = [&](AbilitySlot slot, bool keyTriggered) {
        return bot ? autoHeroPress_[static_cast<size_t>(slot)] : keyTriggered;
    };
    // 右クリック：押している間の技（バリア）は UpdateBarrier で見る。押した瞬間の技（ロケット）はここで使う。
    if (!survivor::IsHeldAbility(Ability(AbilitySlot::Secondary).kind) &&
        pressed(AbilitySlot::Secondary, input.IsTrigger(input.GetMouseState().rgbButtons[1], input.GetPreMouseState().rgbButtons[1]))) {
        UseAbility(AbilitySlot::Secondary);
    }
    if (pressed(AbilitySlot::Mobility, input.IsKeyTriggered(DIK_LSHIFT) || input.IsKeyTriggered(DIK_RSHIFT))) {
        UseAbility(AbilitySlot::Mobility);
    }
    if (pressed(AbilitySlot::Skill, input.IsKeyTriggered(DIK_E))) {
        UseAbility(AbilitySlot::Skill);
    }
    if (pressed(AbilitySlot::Ultimate, input.IsKeyTriggered(DIK_Q))) {
        UseAbility(AbilitySlot::Ultimate);
    }
    autoHeroPress_.fill(false);
}

bool SurvivorRunController::UseAbility(AbilitySlot slot)
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
    auto& player = *world_.resources.player_;
    switch (ability.kind) {
    case AbilityKind::Sprint:
        sprintTimer_ = ability.duration;
        break;
    case AbilityKind::Rocket: {
        // 照準の方へ、大きく遅めの橙の弾を1発。当たると爆発する。
        Player::SurvivorShot shot;
        shot.direction = player.GetDirection();
        shot.damageScale = ability.power;
        shot.speedScale = 1.1f;
        shot.size = 1.7f;
        shot.blastRadius = ability.radius;
        shot.blastCenter = 1.0f;
        shot.blastSplash = 0.8f;
        shot.color = {2.2f, 0.95f, 0.25f, 1.0f};
        if (!player.FireSurvivorShot(shot)) {
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

void SurvivorRunController::UpdateHeroAbilities(float dt)
{
    for (float& cooldown : abilityCooldown_) {
        cooldown = (std::max)(0.0f, cooldown - dt);
    }
    // 走る：効いている間だけ移動の速さを上げる。
    float moveScale = 1.0f;
    if (sprintTimer_ > 0.0f) {
        sprintTimer_ = (std::max)(0.0f, sprintTimer_ - dt);
        sprintSeconds_ += dt;
        if (const survivor::AbilityDefinition* sprint = FindAbility(AbilityKind::Sprint)) {
            moveScale = sprint->power;
        }
        if (sprintTimer_ <= 0.0f) {
            moveScale = 1.0f;
        }
    }
    world_.resources.player_->SetSurvivorMoveScale(moveScale);
    if (const survivor::AbilityDefinition* zone = FindAbility(AbilityKind::HealZone)) {
        UpdateHealZone(dt, *zone);
    }
    if (const survivor::AbilityDefinition* barrage = FindAbility(AbilityKind::Barrage)) {
        UpdateBarrage(dt, *barrage);
    }
    // ロケット（弾ごとの爆発）の爆発と倒した数。区画を出ると集計が0に戻るので、差だけを足す。
    const BulletManager::ShooterStats& shooter = world_.resources.bulletManager_->GetShooterStats();
    lastBulletBlasts_ = (std::min)(lastBulletBlasts_, shooter.bulletBlasts);
    lastBulletBlastKills_ = (std::min)(lastBulletBlastKills_, shooter.bulletBlastKills);
    rocketBlasts_ += shooter.bulletBlasts - lastBulletBlasts_;
    rocketKills_ += shooter.bulletBlastKills - lastBulletBlastKills_;
    lastBulletBlasts_ = shooter.bulletBlasts;
    lastBulletBlastKills_ = shooter.bulletBlastKills;
    if (ultimateCharge_ >= Hero().ultimateCharge) {
        ultimatePulse_ += dt;
    }
    // 自機のまわりの表示：右クリック・Shift・E は使えるまでの進み（押している間の技はスタミナ）、Q はゲージ。
    auto& presentation = world_.presentation;
    for (size_t i = 0; i < presentation.heroAbilityReady_.size(); ++i) {
        const survivor::AbilityDefinition& ability = Hero().abilities[i];
        if (ability.kind == AbilityKind::None) {
            presentation.heroAbilityReady_[i] = -1.0f;
        } else if (survivor::IsHeldAbility(ability.kind)) {
            presentation.heroAbilityReady_[i] =
                barrierExhausted_ > 0.0f ? 0.0f : barrierStamina_ / (std::max)(1.0f, config_.barrier.staminaMax);
        } else {
            presentation.heroAbilityReady_[i] =
                abilityCooldown_[i] > 0.0f && ability.cooldown > 0.0f ? 1.0f - abilityCooldown_[i] / ability.cooldown : 1.0f;
        }
    }
    presentation.heroUltimateRatio_ = (std::clamp)(ultimateCharge_ / Hero().ultimateCharge, 0.0f, 1.0f);
    presentation.heroUltimateGlow_ = 0.75f + 0.25f * std::sin(ultimatePulse_ * 8.0f);
}

void SurvivorRunController::AddUltimateCharge(float amount)
{
    // アルティメットの間は溜めない（使っている間に次が溜まらないように）。
    if (!HeroMode() || amount <= 0.0f || barrageTimer_ > 0.0f) {
        return;
    }
    const float full = Hero().ultimateCharge;
    const bool wasFull = ultimateCharge_ >= full;
    ultimateCharge_ = (std::min)(full, ultimateCharge_ + amount);
    if (!wasFull && ultimateCharge_ >= full) {
        ++ultimateFills_;
        ultimatePulse_ = 0.0f;
        if (ultimateFullAt_ < 0.0f) {
            ultimateFullAt_ = elapsed_;
        }
    }
}

const survivor::AbilityDefinition* SurvivorRunController::FindAbility(AbilityKind kind) const
{
    for (const survivor::AbilityDefinition& ability : Hero().abilities) {
        if (ability.kind == kind) {
            return &ability;
        }
    }
    return nullptr;
}

void SurvivorRunController::UpdateHealZone(float dt, const survivor::AbilityDefinition& ability)
{
    if (healZoneTimer_ <= 0.0f) {
        return;
    }
    healZoneTimer_ = (std::max)(0.0f, healZoneTimer_ - dt);
    healZoneAge_ += dt;
    auto& player = *world_.resources.player_;
    const myEngine::Vector3 offset = player.GetWorldPosition() - healZonePosition_;
    if (myEngine::Length(myEngine::Vector3{offset.x, offset.y, 0.0f}) > ability.radius || player.IsDead()) {
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
    // 回復した量も Q のゲージになる（H5）。
    AddUltimateCharge(static_cast<float>(healed));
}

void SurvivorRunController::UpdateBarrage(float dt, const survivor::AbilityDefinition& ability)
{
    if (barrageTimer_ <= 0.0f) {
        return;
    }
    barrageTimer_ = (std::max)(0.0f, barrageTimer_ - dt);
    barrageSeconds_ += dt;
    barrageShotTimer_ -= dt;
    auto& player = *world_.resources.player_;
    const myEngine::Vector3 origin = player.GetWorldPosition();
    const float interval = 1.0f / (std::max)(1.0f, ability.rate);
    while (barrageShotTimer_ <= 0.0f) {
        barrageShotTimer_ += interval;
        // 狙う範囲の中の近い敵を8体まで集め、順に狙う（同じ敵ばかりに撃たない）。ボスがいれば候補に入れる。
        std::array<std::pair<float, myEngine::Vector3>, 9> targets{};
        size_t count = 0;
        for (const ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
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
        if (IsRivalActive()) {
            const myEngine::Vector3 position = world_.resources.enemy_->GetWorldPosition();
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
        shot.damageScale = ability.power;
        shot.speedScale = 1.8f;
        shot.size = 0.8f;
        shot.color = {2.2f, 1.7f, 0.35f, 1.0f};
        if (player.FireSurvivorShot(shot)) {
            ++barrageShots_;
        }
    }
}

void SurvivorRunController::QueueHeroVisuals(myEngine::NeonGridRenderer& renderer) const
{
    if (!HeroMode()) {
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
    if (barrageTimer_ > 0.0f) {
        circle(world_.resources.player_->GetWorldPosition(), 2.6f, 0.16f, {2.2f, 1.7f, 0.35f, 0.9f}, barrageSeconds_ * 3.0f);
    }
}

bool SurvivorRunController::HeroBarrierWanted() const
{
    if (Ability(AbilitySlot::Secondary).kind != AbilityKind::Barrier) {
        return false;
    }
    if (autoTest_ != AutoTest::None) {
        return autoBarrierWanted_;
    }
    return world_.resources.input_->IsPress(world_.resources.input_->GetMouseState().rgbButtons[1]);
}

std::string SurvivorRunController::HeroControlsText() const
{
    std::string text = "WASD 移動 ／ 左クリック 撃つ";
    for (size_t i = 0; i < survivor::kAbilitySlotCount; ++i) {
        const survivor::AbilityDefinition& ability = Hero().abilities[i];
        if (ability.kind != AbilityKind::None) {
            text += std::string(" ／ ") + survivor::AbilitySlotButton(static_cast<AbilitySlot>(i)) + " " + ability.name;
        }
    }
    return text + " ／ Tab 組み立て";
}

std::string SurvivorRunController::DescribeHeroAbilities() const
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
            text << (ratio >= 1.0f ? std::string("使える") : std::to_string(static_cast<int>(ratio * 100.0f)) + "%");
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
