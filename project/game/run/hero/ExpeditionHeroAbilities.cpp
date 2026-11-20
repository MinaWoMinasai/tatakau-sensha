#include "game/run/hero/ExpeditionHeroAbilities.h"
#include "Calculation.h"
#include "game/enemy/actor/Enemy.h"
#include "game/render/neon/NeonGridRenderer.h"
#include "game/session/GameWorld.h"
#include "game/session/GameplaySystems.h"
#include <algorithm>
#include <cmath>

// 遠征の機体（計画書 p5 の H3）：選んだ機体の技を、戦闘の部屋の中で動かす。技の中身は HeroAbilityCore（育つ戦車の試作と共通）。

namespace gameplay {

using survivor::AbilityKind;
using survivor::AbilitySlot;

void ExpeditionHeroAbilities::Start(const survivor::HeroDefinition& hero)
{
    heroDef_ = hero;
    active_ = true;
    clock_ = 0.0f;
    damageDealt_ = 0;
    ResetHeroState();
    abilityUses_.fill(0);
    barrierStamina_ = barrier_.staminaMax;
    barrierExhausted_ = 0.0f;
    auto& player = *heroWorld_.resources.player_;
    ApplyPrimaryWeapon(hero);
    if (hero.maxHp > 0) {
        player.SetSurvivorMaxHp(hero.maxHp, true);
    }
    // 吸い付く光線が主武器なら、ふつうの弾は撃たない。
    player.SetSurvivorFireLocked(HeroBeamPrimary());
    // 与えたダメージで Q のゲージを溜める（試作と同じ。ボスへのダメージも）。
    ExpEnemy::SetPlayerDamageCallback([this](const ExpEnemy&, uint32_t amount) {
        AddUltimateCharge(static_cast<float>(amount));
        damageDealt_ += amount;
    });
    Enemy::SetDamageCallback([this](const myEngine::Vector3&, uint32_t amount) {
        AddUltimateCharge(static_cast<float>(amount));
        damageDealt_ += amount;
    });
    // 自機のまわりの表示：技の弧と Q のゲージを出す。スタミナの弧はバリアを持つ機体だけ。
    auto& presentation = heroWorld_.presentation;
    presentation.heroUltimateRatio_ = 0.0f;
    presentation.playerStaminaRatio_ = Ability(AbilitySlot::Secondary).kind == AbilityKind::Barrier ? 1.0f : -1.0f;
    presentation.playerStaminaExhausted_ = false;
}

void ExpeditionHeroAbilities::ApplyPrimaryWeapon(const survivor::HeroDefinition& hero)
{
    primaryName_.clear();
    const int index = survivor::FindWeapon(weapons_, hero.primary);
    if (index < 0) {
        return;
    }
    // 試作の SurvivorRunController::SyncSlotWeapons・ApplyFormToPlayer と同じ：機体の熱の倍率で武器の熱を上書きし、形態にして自機に入れる。
    survivor::WeaponDefinition weapon = weapons_[static_cast<size_t>(index)];
    if (hero.primaryHeatScale > 0.0f) {
        weapon.heatScale = hero.primaryHeatScale;
    }
    const survivor::FormDefinition form = survivor::WeaponForm(forms_, weapon);
    auto& player = *heroWorld_.resources.player_;
    const PlayerClassConfig* source = player.FindCatalogClassConfig(form.sourceClass);
    if (!source) {
        source = player.GetActiveClassConfig();
    }
    if (!source) {
        return;
    }
    Player::SurvivorFormTuning tuning{};
    tuning.heatScale = form.heatScale;
    tuning.railPower = form.railPower;
    tuning.railHeatBase = form.railHeatBase;
    tuning.railHeatPerCharge = form.railHeatPerCharge;
    tuning.railWidthScale = form.railWidth;
    tuning.railExtraPierce = form.railPierce;
    tuning.bulletColor = form.color;
    player.ApplySurvivorForm(survivor::BuildFormConfig(*source, form, weapon.form), tuning);
    // 機関砲は、撃ち続けると速くなる加速を最初から持つ（試作の装填機の3段と同じ）。
    Player::SurvivorPartEffects effects{};
    effects.loaderRamp = form.fireRamp;
    player.SetSurvivorPartEffects(effects);
    primaryName_ = weapon.name;
}

void ExpeditionHeroAbilities::OnRoomStart()
{
    if (!active_) {
        return;
    }
    // 前の部屋で効いていた技を消す（場所に置いた場や球が、次の部屋に残らないように）。
    sprintTimer_ = phaseTimer_ = chargeTimer_ = 0.0f;
    healZoneTimer_ = barrageTimer_ = orbTimer_ = lanceTimer_ = fortressTimer_ = 0.0f;
    beamTarget_ = 0;
    beamOn_ = selfHealOn_ = barrierOn_ = false;
    chargeHit_.clear();
    shockAge_ = 10.0f;
    heroWorld_.resources.player_->SetSurvivorMoveScale(1.0f);
}

bool ExpeditionHeroAbilities::HeroBossTargetActive() const
{
    // 遠征のボスは、奥行きの遭遇を有効にした enemy_。ボス戦の間だけ技の狙う先に入れる。
    const Enemy* boss = heroWorld_.resources.enemy_.get();
    return boss && !boss->IsDead() && boss->IsNeonDepthEncounterEnabled();
}

bool ExpeditionHeroAbilities::IsPlaying() const
{
    const auto& run = heroWorld_.run;
    return active_ && run.tankExpedition_.IsCombat() && heroWorld_.combat.combatFlow_.GetState() == GameFlowState::Playing &&
           !run.tankRunPaused_ && !run.expeditionMapPreview_ && !run.tankExpeditionDetailsOpen_ && !run.expeditionTransition_.IsActive() &&
           !heroWorld_.resources.player_->IsDead();
}

void ExpeditionHeroAbilities::Update(float dt)
{
    if (!active_) {
        return;
    }
    if (!IsPlaying()) {
        // 止めている間は押していないことにし、技の時間も進めない。
        primaryHeld_ = secondaryHeld_ = false;
        return;
    }
    clock_ += dt;
    if (bot_) {
        UpdateBot(dt);
    }
    ReadInput();
    UpdateHeroAbilities(dt);
    if (Ability(AbilitySlot::Secondary).kind == AbilityKind::Barrier) {
        const bool wanted = bot_ ? botSecondary_
                                 : heroWorld_.resources.input_->IsPress(heroWorld_.resources.input_->GetMouseState().rgbButtons[1]);
        UpdateBarrier(dt, wanted);
    }
    heroWorld_.resources.player_->SetSurvivorFireLocked(HeroBeamPrimary());
}

void ExpeditionHeroAbilities::ReadInput()
{
    auto& input = *heroWorld_.resources.input_;
    auto pressed = [&](AbilitySlot slot, bool keyTriggered) {
        return bot_ ? botPress_[static_cast<size_t>(slot)] : keyTriggered;
    };
    // 左クリック・右クリックを押しているか（押している間の技：吸い付く光線・自分を回復）。
    primaryHeld_ = bot_ ? botPrimary_ : input.IsPress(input.GetMouseState().rgbButtons[0]);
    secondaryHeld_ = bot_ ? botSecondary_ : input.IsPress(input.GetMouseState().rgbButtons[1]);
    // 右クリック：押している間の技（バリア・自分を回復）は Update と UpdateHeroAbilities で見る。押した瞬間の技（ロケット）はここで使う。
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
    botPress_.fill(false);
}

void ExpeditionHeroAbilities::UpdateBot(float dt)
{
    // 近い敵（いなければボス）を狙って撃ち続け、ゆっくり回りながら、使える技はすぐ使う。自機は無敵にする。
    auto& player = *heroWorld_.resources.player_;
    heroWorld_.presentation.debugPlayerNoDamage_ = true;
    botClock_ += dt;
    const myEngine::Vector3 self = player.GetWorldPosition();
    myEngine::Vector3 aim = self + myEngine::Vector3{6.0f, 0.0f, 0.0f};
    float nearest = 1e9f;
    for (const ExpEnemy* enemy : heroWorld_.resources.enemyManager_->GetEnemyPtrs()) {
        if (!enemy || enemy->IsDead() || enemy->IsRunResource()) {
            continue;
        }
        const myEngine::Vector3 p = enemy->GetWorldPosition();
        const float distance = myEngine::Length(myEngine::Vector3{p.x - self.x, p.y - self.y, 0.0f});
        if (distance < nearest) {
            nearest = distance;
            aim = p;
        }
    }
    if (nearest > 1e8f && HeroBossTargetActive()) {
        aim = heroWorld_.resources.enemy_->GetWorldPosition();
        nearest = myEngine::Length(myEngine::Vector3{aim.x - self.x, aim.y - self.y, 0.0f});
    }
    // 遠ければ近づき、近すぎれば少し離れる。間はゆっくり横へ回る。
    const myEngine::Vector3 toward{aim.x - self.x, aim.y - self.y, 0.0f};
    const float length = (std::max)(0.001f, myEngine::Length(toward));
    const myEngine::Vector2 forward{toward.x / length, toward.y / length};
    const myEngine::Vector2 side{-forward.y, forward.x};
    const float approach = nearest > 9.0f ? 1.0f : nearest < 4.0f ? -0.6f : 0.0f;
    const float sway = std::sin(botClock_ * 0.7f) > 0.0f ? 0.6f : -0.6f;
    myEngine::Vector2 move{forward.x * approach + side.x * sway, forward.y * approach + side.y * sway};
    const float moveLength = std::sqrt(move.x * move.x + move.y * move.y);
    if (moveLength > 1.0f) {
        move = {move.x / moveLength, move.y / moveLength};
    }
    player.SetDemoInput(true, move, aim, true, false);
    botPrimary_ = true;
    // 押している間の右クリック：バリアは1秒ごとに張る・放す、自分を回復は吸収ゲージがあれば押し続ける。
    const AbilityKind secondary = Ability(AbilitySlot::Secondary).kind;
    botSecondary_ = secondary == AbilityKind::Barrier ? std::fmod(botClock_, 2.0f) < 1.0f : secondary == AbilityKind::SelfHeal;
    for (size_t i = 0; i < survivor::kAbilitySlotCount; ++i) {
        botPress_[i] = true;
    }
}

void ExpeditionHeroAbilities::QueueWorldVisuals()
{
    if (!active_ || !heroWorld_.resources.neonGridRenderer_ || !heroWorld_.run.tankExpedition_.IsCombat()) {
        return;
    }
    const myEngine::Matrix4x4 cameraWorld = myEngine::Inverse(heroWorld_.resources.camera->GetViewMatrix());
    const myEngine::Vector3 cameraRight = myEngine::Normalize({cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2]});
    const myEngine::Vector3 cameraUp = myEngine::Normalize({cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2]});
    const myEngine::Vector3 forward = myEngine::Cross(cameraRight, cameraUp);
    QueueBarrier(*heroWorld_.resources.neonGridRenderer_, forward);
    QueueHeroVisuals(*heroWorld_.resources.neonGridRenderer_, forward);
}

nlohmann::json ExpeditionHeroAbilities::MakeReport() const
{
    return {{"active", active_},
            {"key", heroDef_.key},
            {"primary", primaryName_},
            {"className", heroWorld_.resources.player_->GetCurrentClassName()},
            {"bulletDamage", heroWorld_.resources.player_->GetStats().bulletDamage},
            {"primaryShots", heroWorld_.resources.player_->GetPrimaryAttackCount()},
            {"overheats", heroWorld_.resources.player_->GetHeat().GetOverheatCount()},
            {"heatPerShot", heroWorld_.resources.player_->GetHeat().GetConfig().heatPerShot},
            {"maxHp", heroWorld_.resources.player_->GetMaxHp()},
            {"clock", clock_},
            {"uses", abilityUses_},
            {"cooldownSeen", abilityCooldownSeen_},
            {"damageDealt", damageDealt_},
            {"ultimateCharge", ultimateCharge_},
            {"ultimateFills", ultimateFills_},
            {"sprintSeconds", sprintSeconds_},
            {"rocketBlasts", rocketBlasts_},
            {"healedByZone", healedByZone_},
            {"barrageShots", barrageShots_},
            {"drainDamage", drainDamage_},
            {"beamLockedSeconds", beamLockedSeconds_},
            {"selfHealed", selfHealed_},
            {"orbDamage", orbDamage_},
            {"orbBounces", orbBounces_},
            {"lanceDamage", lanceDamage_},
            {"chargePushes", chargePushes_},
            {"chargeDamage", chargeDamage_},
            {"shockwavePushes", shockwavePushes_},
            {"shockwaveDamage", shockwaveDamage_},
            {"fortressBlocked", fortressBlocked_},
            {"fortressDamage", fortressDamage_},
            {"barrierBlocked", barrierBlocked_},
            {"barrierPushes", barrierPushes_}};
}

} // namespace gameplay
