#include "game/run/session/SurvivorRunController.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/render/neon/NeonGridRenderer.h"
#include "game/session/GameplaySystems.h"
#include "game/enemy/actor/Enemy.h"
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
    // 機体の最大HP（満タンにする）と、体力の決まり（H6・H7）。
    world_.resources.player_->SetSurvivorMaxHp(Hero().maxHp);
    ApplyHeroHealthRules();
}

void SurvivorRunController::ApplyHeroHealthRules()
{
    Player::SurvivorDamageRules rules;
    rules.enabled = HeroMode();
    rules.maxHitRatio = config_.health.maxHitRatio;
    rules.maxPerSecondRatio = config_.health.maxPerSecondRatio;
    rules.sameSourceSeconds = config_.health.sameSourceSeconds;
    rules.woundRatio = config_.health.woundRatio;
    world_.resources.player_->ConfigureSurvivorDamageRules(rules);
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
    absorb_ = 0.0f;
    beamTarget_ = 0;
    beamOn_ = false;
    selfHealRemainder_ = 0.0f;
    selfHealOn_ = false;
    phaseTimer_ = 0.0f;
    orbTimer_ = 0.0f;
    lanceTimer_ = 0.0f;
    world_.resources.player_->SetSurvivorMoveScale(1.0f);
}

void SurvivorRunController::UpdateHeroInput()
{
    auto& input = *world_.resources.input_;
    const bool bot = autoTest_ != AutoTest::None;
    auto pressed = [&](AbilitySlot slot, bool keyTriggered) {
        return bot ? autoHeroPress_[static_cast<size_t>(slot)] : keyTriggered;
    };
    // 左クリック・右クリックを押しているか（押している間の技：吸い付く光線・自分を回復）。
    primaryHeld_ = bot ? true : input.IsPress(input.GetMouseState().rgbButtons[0]);
    secondaryHeld_ = bot ? autoSecondaryHold_ : input.IsPress(input.GetMouseState().rgbButtons[1]);
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
    case AbilityKind::Phase:
        phaseTimer_ = ability.duration;
        player.StartSurvivorPhase(ability.duration);
        break;
    case AbilityKind::BounceOrb: {
        // 照準の方へ球を出す。敵に触れるたびに、次の近い敵へ跳ねる。
        const myEngine::Vector3 aim = myEngine::NormalizeOr(player.GetDirection(), myEngine::Vector3{1.0f, 0.0f, 0.0f}, 0.001f);
        orbTimer_ = ability.duration;
        orbPosition_ = player.GetWorldPosition() + aim * 1.2f;
        orbVelocity_ = aim * ability.rate;
        orbTick_ = 0.0f;
        orbLastBounce_ = 0;
        break;
    }
    case AbilityKind::Lance:
        lanceTimer_ = ability.duration;
        lanceTick_ = 0.0f;
        lanceAge_ = 0.0f;
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
    // 走る・消える移動：効いている間だけ移動の速さを上げる。
    float moveScale = 1.0f;
    if (phaseTimer_ > 0.0f) {
        phaseTimer_ = (std::max)(0.0f, phaseTimer_ - dt);
        if (const survivor::AbilityDefinition* phase = FindAbility(AbilityKind::Phase); phase && phaseTimer_ > 0.0f) {
            moveScale = (std::max)(moveScale, phase->power);
        }
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
    world_.resources.player_->SetSurvivorMoveScale(moveScale);
    if (const survivor::AbilityDefinition* zone = FindAbility(AbilityKind::HealZone)) {
        UpdateHealZone(dt, *zone);
    }
    if (const survivor::AbilityDefinition* barrage = FindAbility(AbilityKind::Barrage)) {
        UpdateBarrage(dt, *barrage);
    }
    if (Hero().primaryAbility.kind == AbilityKind::DrainBeam) {
        UpdateDrainBeam(dt, Hero().primaryAbility);
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
    // 押している間の入力は、次に UpdateHeroInput が見るまで離したことにする（持ち物の画面を開いている間など）。
    primaryHeld_ = false;
    secondaryHeld_ = false;
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
        } else if (ability.kind == AbilityKind::SelfHeal) {
            presentation.heroAbilityReady_[i] = absorb_ / (std::max)(0.001f, ability.duration);
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
    if (!HeroMode() || amount <= 0.0f || UltimateActive()) {
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

void SurvivorRunController::QueueHeroVisuals(myEngine::NeonGridRenderer& renderer, const myEngine::Vector3& cameraForward) const
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
    const myEngine::Vector3 self = world_.resources.player_->GetWorldPosition();
    if (barrageTimer_ > 0.0f) {
        circle(self, 2.6f, 0.16f, {2.2f, 1.7f, 0.35f, 0.9f}, barrageSeconds_ * 3.0f);
    }
    const myEngine::Vector3 aim =
        myEngine::NormalizeOr(world_.resources.player_->GetDirection(), myEngine::Vector3{1.0f, 0.0f, 0.0f}, 0.001f);
    const myEngine::Vector3 lift{0.0f, 0.0f, -0.3f};
    // 吸い付く光線：吸い付いている間は濃い紫、外れている間は薄く細い。
    if (beamOn_) {
        const float width = (std::max)(0.05f, Hero().primaryAbility.size);
        const float wobble = 0.85f + 0.15f * std::sin(beamAge_ * 40.0f);
        const myEngine::Vector3 start = self + aim * 1.1f + lift, end = beamEnd_ + lift;
        // 太い線は平らな四角だと見る向きで割れて見えるので、カメラを向いた輪郭の線（芯・肩・にじみ）で描く。
        if (beamTarget_ != 0) {
            renderer.QueueContourLine(start, end, width * wobble, {1.5f, 0.55f, 2.0f, 0.95f}, cameraForward);
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
        const float radius = (std::max)(0.2f, orb->size);
        circle(orbPosition_, radius, 0.18f, {1.6f, 0.45f, 2.3f, 0.95f}, orbTimer_ * 6.0f);
        circle(orbPosition_, radius * 0.55f, 0.22f, {2.0f, 0.9f, 2.4f, 0.9f}, -orbTimer_ * 9.0f);
    }
    // 太い光線：向いている方へ、太い線と外側のにじみ。
    if (const survivor::AbilityDefinition* lance = FindAbility(AbilityKind::Lance); lance && lanceTimer_ > 0.0f) {
        const float width = (std::max)(0.2f, lance->size) * (0.9f + 0.1f * std::sin(lanceAge_ * 30.0f));
        const myEngine::Vector3 start = self + aim * 1.1f + lift, end = self + aim * lance->radius + lift;
        renderer.QueueContourLine(start, end, width, {1.7f, 0.8f, 2.2f, 0.95f}, cameraForward);
    }
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

float SurvivorRunController::HeroDamageUnit(float scale) const
{
    return world_.resources.player_->GetStats().bulletDamage * (std::max)(0.0f, scale);
}

uint32_t SurvivorRunController::DealHeroDamage(Collider* target, bool rival, uint32_t amount, const myEngine::Vector3& from)
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
    enemy->TakeDirectionalDamage(amount, from);
    return amount;
}

void SurvivorRunController::AddAbsorb(float amount)
{
    const survivor::AbilityDefinition* heal = FindAbility(AbilityKind::SelfHeal);
    if (!heal || amount <= 0.0f) {
        return;
    }
    const float before = absorb_;
    absorb_ = (std::min)(heal->duration, absorb_ + amount);
    absorbGained_ += absorb_ - before;
}

void SurvivorRunController::UpdateDrainBeam(float dt, const survivor::AbilityDefinition& ability)
{
    auto& player = *world_.resources.player_;
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
    for (ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
        if (enemy && !enemy->IsDead() && !enemy->IsRunResource()) {
            consider(enemy, enemy->GetCollisionId());
        }
    }
    if (IsRivalActive()) {
        consider(world_.resources.enemy_.get(), kRivalTarget);
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
        if (dealt == 0) {
            beamTarget_ = 0;
            beamTick_ = 0.0f;
            break;
        }
    }
}

void SurvivorRunController::UpdateSelfHeal(float dt, const survivor::AbilityDefinition& ability)
{
    auto& player = *world_.resources.player_;
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
    // 回復した量も Q のゲージになる（H5）。
    AddUltimateCharge(static_cast<float>(healed));
}

void SurvivorRunController::UpdateBounceOrb(float dt, const survivor::AbilityDefinition& ability)
{
    if (orbTimer_ <= 0.0f || dt <= 0.0f) {
        return;
    }
    orbTimer_ = (std::max)(0.0f, orbTimer_ - dt);
    orbPosition_ += orbVelocity_ * dt;
    const myEngine::Vector3 self = world_.resources.player_->GetWorldPosition();
    // 自機から離れすぎたら、自機の方へ戻す（壁の代わり。画面の外へ行かない）。
    if (PlanarDistance(orbPosition_, self) > 14.0f) {
        const myEngine::Vector3 back = myEngine::NormalizeOr(myEngine::Vector3{self.x - orbPosition_.x, self.y - orbPosition_.y, 0.0f},
                                                             myEngine::Vector3{1.0f, 0.0f, 0.0f}, 0.001f);
        orbVelocity_ = back * ability.rate;
        orbLastBounce_ = 0;
    }
    const float radius = (std::max)(0.2f, ability.size);
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
        orbDamage_ += DealHeroDamage(target, rival, amount, orbPosition_);
    };
    for (ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
        if (enemy && !enemy->IsDead() && !enemy->IsRunResource()) {
            touch(enemy, enemy->GetCollisionId(), false);
        }
    }
    if (IsRivalActive()) {
        touch(world_.resources.enemy_.get(), kRivalTarget, true);
    }
    if (!bounced) {
        return;
    }
    // 次に近い敵（今跳ねた敵の他）へ向きを変える。いなければ跳ね返る。
    ++orbBounces_;
    orbLastBounce_ = bouncedId;
    const myEngine::Vector3 from = bounced->GetWorldPosition();
    float nearest = 10.0f;
    myEngine::Vector3 next{};
    bool found = false;
    for (const ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
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

void SurvivorRunController::UpdateLance(float dt, const survivor::AbilityDefinition& ability)
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
    auto& player = *world_.resources.player_;
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
    for (ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
        if (enemy && !enemy->IsDead() && !enemy->IsRunResource()) {
            hit(enemy, false);
        }
    }
    if (IsRivalActive()) {
        hit(world_.resources.enemy_.get(), true);
    }
    lanceMaxHits_ = (std::max)(lanceMaxHits_, hits);
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
    std::string text =
        "WASD 移動 ／ 左クリック " + (Hero().primaryAbility.kind != AbilityKind::None ? Hero().primaryAbility.name : std::string("撃つ"));
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
