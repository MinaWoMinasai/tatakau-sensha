#include "game/run/session/SurvivorRunController.h"
#include "game/run/session/SurvivorRunShared.h"
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
    world_.resources.player_->SetSurvivorMoveScale(1.0f);
}

void SurvivorRunController::UpdateHeroInput()
{
    auto& input = *world_.resources.input_;
    const bool bot = autoTest_ != AutoTest::None;
    auto pressed = [&](AbilitySlot slot, bool keyTriggered) {
        return bot ? autoHeroPress_[static_cast<size_t>(slot)] : keyTriggered;
    };
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
    switch (ability.kind) {
    case AbilityKind::Sprint:
        sprintTimer_ = ability.duration;
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
        for (const survivor::AbilityDefinition& ability : Hero().abilities) {
            if (ability.kind == AbilityKind::Sprint) {
                moveScale = ability.power;
                break;
            }
        }
        if (sprintTimer_ <= 0.0f) {
            moveScale = 1.0f;
        }
    }
    world_.resources.player_->SetSurvivorMoveScale(moveScale);
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
    if (!HeroMode() || amount <= 0.0f) {
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
    std::ostringstream text;
    text << Hero().name;
    for (size_t i = 0; i < survivor::kAbilitySlotCount; ++i) {
        const auto slot = static_cast<AbilitySlot>(i);
        const survivor::AbilityDefinition& ability = Hero().abilities[i];
        if (ability.kind == AbilityKind::None) {
            continue;
        }
        text << "   " << survivor::AbilitySlotButton(slot) << " " << ability.name;
        if (slot == AbilitySlot::Ultimate) {
            const float ratio = ultimateCharge_ / Hero().ultimateCharge;
            text << (ratio >= 1.0f ? " 使える" : " " + std::to_string(static_cast<int>(ratio * 100.0f)) + "%");
        } else if (!survivor::IsHeldAbility(ability.kind) && abilityCooldown_[i] > 0.0f) {
            text << " " << static_cast<int>(std::ceil(abilityCooldown_[i])) << "秒";
        }
    }
    return text.str();
}

} // namespace gameplay
