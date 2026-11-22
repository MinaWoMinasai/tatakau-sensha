#include "game/run/session/SurvivorRunController.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/render/neon/NeonGridRenderer.h"
#include "game/session/GameplaySystems.h"
#include "game/enemy/actor/Enemy.h"
#include <algorithm>
#include <cmath>

// 育つ戦車の試作の機体（計画書 p4 の K0）：機体を選んで構える・入力（とボット）を読む・体力の決まり。
// 技を使う・進める・当てる・見た目は土台の HeroAbilityCore（game/run/hero/HeroAbilityCore.cpp）。

namespace gameplay {

using survivor::AbilityKind;
using survivor::AbilitySlot;

void SurvivorRunController::StartHero()
{
    hero_ = -1;
    if (!config_.weaponsEnabled || !config_.heroes.enabled || config_.heroes.heroes.empty()) {
        return;
    }
    int found = survivor::FindHero(config_.heroes.heroes, config_.heroes.start);
    // まだ解放していない機体は選べない（K6）。使える最初の機体にする。
    if (found < 0 || !survivor::IsHeroUnlocked(heroSave_, config_.heroes.heroes[static_cast<size_t>(found)])) {
        found = -1;
        for (size_t i = 0; i < config_.heroes.heroes.size() && found < 0; ++i) {
            if (survivor::IsHeroUnlocked(heroSave_, config_.heroes.heroes[i])) {
                found = static_cast<int>(i);
            }
        }
    }
    const size_t index = found >= 0 ? static_cast<size_t>(found) : 0;
    const int weapon = survivor::FindWeapon(config_.weapons, config_.heroes.heroes[index].primary);
    if (weapon < 0) {
        OutputDebugStringA(("[Survivor] hero primary weapon not found: " + config_.heroes.heroes[index].primary + "\n").c_str());
        return;
    }
    hero_ = static_cast<int>(index);
    heroDef_ = config_.heroes.heroes[index];
    heroAutoFire_ = config_.heroes.autoFire;
    upgradeTiers_.fill(0);
    perks_.fill(false);
    subWeaponTiers_.fill(0);
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
    RebuildHero(true);
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

void SurvivorRunController::UpdateHeroInput()
{
    auto& input = *world_.resources.input_;
    const bool bot = autoTest_ != AutoTest::None;
    auto pressed = [&](AbilitySlot slot, bool keyTriggered) {
        return bot ? autoHeroPress_[static_cast<size_t>(slot)] : keyTriggered;
    };
    // F：自動で撃つの切り替え（計画書 p7）。自動で撃つときは、押さなくても向いている方へ撃ち続ける（向きはマウスか右スティック）。
    if (!bot && input.IsKeyTriggered(DIK_F)) {
        heroAutoFire_ = !heroAutoFire_;
        world_.combatFlow->SetEventCallout(heroAutoFire_ ? "自動で撃つ：オン（F で切り替え）" : "自動で撃つ：オフ（F で切り替え）", 1.0f);
    }
    world_.resources.player_->SetAutoFire(heroAutoFire_ && !bot);
    // 左クリック・右クリックを押しているか（押している間の技：吸い付く光線・自分を回復）。
    primaryHeld_ = bot ? true : heroAutoFire_ || input.IsPress(input.GetMouseState().rgbButtons[0]);
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

} // namespace gameplay
