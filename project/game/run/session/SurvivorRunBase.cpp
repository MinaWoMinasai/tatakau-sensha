#include "game/run/session/SurvivorRunController.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/session/GameplaySystems.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>

// 育つ戦車の試作の拠点：持ち物のセーブ、出撃前に武器を選ぶ画面、区画の終わりに持ち帰る（計画書 p3 の W5）。

namespace gameplay {

using namespace survivorrun;

std::string SurvivorRunController::BasePath() const
{
    if (autoTest_ != AutoTest::None) {
        return std::string(kAutoTestDirectory) + "/autotest_base.json";
    }
    return "save/survivor_base.json";
}

void SurvivorRunController::LoadBase()
{
    baseError_.clear();
    // 自動確認は毎回新しい拠点から始める（MYENGINE_SURVIVOR_KEEP_SAVE=1 なら前の回の拠点を読む）。タイトルの見本はセーブを読まない。
    wchar_t keep[4]{};
    const bool keepSave = GetEnvironmentVariableW(L"MYENGINE_SURVIVOR_KEEP_SAVE", keep, 4) > 0 && keep[0] == L'1';
    const bool fresh = world_.demo.titleDemo_ || (autoTest_ != AutoTest::None && !keepSave);
    if (!fresh && survivor::LoadBaseInventory(BasePath(), config_.parts, base_, baseError_)) {
        return;
    }
    if (!baseError_.empty()) {
        // 壊れたセーブは消さずに名前を変えて残し、新しい拠点で始める。
        std::error_code ignored;
        std::filesystem::rename(BasePath(), BasePath() + ".broken", ignored);
    }
    base_ = survivor::NewBaseInventory(config_.weapons, config_.startWeapons);
}

void SurvivorRunController::SaveBase()
{
    if (world_.demo.titleDemo_) {
        return;
    }
    std::string error;
    if (!survivor::SaveBaseInventory(BasePath(), base_, config_.parts, error)) {
        baseError_ = error;
        world_.combatFlow->SetEventCallout(error, 1.6f);
    }
}

void SurvivorRunController::StartLoadout()
{
    std::optional<size_t> first;
    for (size_t i = 0; i < weaponSlots_.size(); ++i) {
        weaponSlots_[i] = -1;
        runSlots_[i] = {};
        const int index = base_.equipped[i];
        if (index >= 0 && static_cast<size_t>(index) < base_.weapons.size()) {
            const survivor::BaseWeapon& owned = base_.weapons[static_cast<size_t>(index)];
            weaponSlots_[i] = survivor::FindWeapon(config_.weapons, owned.key);
            if (weaponSlots_[i] >= 0) {
                runSlots_[i] = {index, owned.chips.size(), owned};
            }
        }
        if (weaponSlots_[i] >= 0 && !first) {
            first = i;
        }
    }
    SyncSlotWeapons();
    // 弾倉は満タン、予備は設定の数から始める。熱は冷えた状態から。
    reserves_ = config_.startReserve;
    for (size_t i = 0; i < weaponSlots_.size(); ++i) {
        magazines_[i] = weaponSlots_[i] >= 0 ? SlotWeapon(i).magazine : 0;
        weaponHeat_[i] = {};
    }
    weaponEquipped_ = false;
    activeSlot_ = first.value_or(0);
    EquipWeapon(activeSlot_);
    world_.resources.player_->RestoreHeat({});
    RefreshWeaponSlots();
}

void SurvivorRunController::SyncSlotWeapons()
{
    for (size_t i = 0; i < weaponSlots_.size(); ++i) {
        weaponSlots_[i] = runSlots_[i].weapon.key.empty() ? -1 : survivor::FindWeapon(config_.weapons, runSlots_[i].weapon.key);
        slotDefs_[i] = weaponSlots_[i] >= 0 ? survivor::ApplyRarity(config_.weapons[static_cast<size_t>(weaponSlots_[i])], runSlots_[i].weapon.rarity,
                                                                  config_.crafting)
                                            : survivor::WeaponDefinition{};
    }
}

void SurvivorRunController::OpenBaseScreen()
{
    baseScreenOpen_ = true;
    baseCursor_ = (std::max)(0, base_.equipped[0]);
    basePage_ = 0;
    RefreshBaseScreen();
}

void SurvivorRunController::EquipFromBase(int weaponIndex, size_t slot)
{
    if (slot >= base_.equipped.size() || weaponIndex < 0 || static_cast<size_t>(weaponIndex) >= base_.weapons.size() ||
        survivor::FindWeapon(config_.weapons, base_.weapons[static_cast<size_t>(weaponIndex)].key) < 0) {
        return;
    }
    // ほかの枠に入っている武器なら、その枠と入れ替える。
    for (size_t i = 0; i < base_.equipped.size(); ++i) {
        if (i != slot && base_.equipped[i] == weaponIndex) {
            base_.equipped[i] = base_.equipped[slot];
        }
    }
    base_.equipped[slot] = weaponIndex;
}

bool SurvivorRunController::CraftWeapon(size_t recipe)
{
    if (recipe >= config_.crafting.recipes.size()) {
        return false;
    }
    const survivor::WeaponRecipe& entry = config_.crafting.recipes[recipe];
    const int definition = survivor::FindWeapon(config_.weapons, entry.key);
    if (definition < 0) {
        return false;
    }
    if (!survivor::CanAfford(base_.materials, base_.money, entry.cost)) {
        world_.combatFlow->SetEventCallout("素材かお金が足りない", 1.0f);
        return false;
    }
    survivor::Pay(base_.materials, base_.money, entry.cost);
    base_.weapons.push_back({entry.key, survivor::Rarity::Common, {}});
    ++crafted_;
    world_.combatFlow->SetEventCallout("作った：" + config_.weapons[static_cast<size_t>(definition)].name, 1.0f);
    SaveBase();
    return true;
}

bool SurvivorRunController::EvolveBaseWeapon(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= base_.weapons.size()) {
        return false;
    }
    survivor::BaseWeapon& owned = base_.weapons[static_cast<size_t>(index)];
    const int definition = survivor::FindWeapon(config_.weapons, owned.key);
    if (definition < 0 || owned.rarity == survivor::Rarity::Legendary) {
        world_.combatFlow->SetEventCallout(definition < 0 ? "この武器は進化できない" : "もう一番上のレア度", 1.0f);
        return false;
    }
    const auto target = static_cast<survivor::Rarity>(static_cast<int>(owned.rarity) + 1);
    const survivor::CraftCost cost = survivor::EvolveCost(config_.crafting, owned.key, target);
    if (!survivor::CanAfford(base_.materials, base_.money, cost)) {
        world_.combatFlow->SetEventCallout("素材かお金が足りない", 1.0f);
        return false;
    }
    survivor::Pay(base_.materials, base_.money, cost);
    owned.rarity = target;
    ++evolved_;
    world_.combatFlow->SetEventCallout("進化した：" + config_.weapons[static_cast<size_t>(definition)].name + "（" + survivor::RarityName(target) + "）",
                                       1.2f);
    SaveBase();
    return true;
}

bool SurvivorRunController::BranchBaseWeapon(int index, size_t branch)
{
    if (index < 0 || static_cast<size_t>(index) >= base_.weapons.size() || branch >= config_.crafting.branches.size()) {
        return false;
    }
    survivor::BaseWeapon& owned = base_.weapons[static_cast<size_t>(index)];
    const survivor::WeaponBranch& entry = config_.crafting.branches[branch];
    const int target = survivor::FindWeapon(config_.weapons, entry.to);
    if (owned.key != entry.from || target < 0 || owned.rarity < entry.minRarity) {
        return false;
    }
    if (!survivor::CanAfford(base_.materials, base_.money, entry.cost)) {
        world_.combatFlow->SetEventCallout("素材かお金が足りない", 1.0f);
        return false;
    }
    survivor::Pay(base_.materials, base_.money, entry.cost);
    owned.key = entry.to;
    // レア度とチップはそのまま。チップ枠を越える分は拠点へ戻す。
    const size_t capacity = static_cast<size_t>((std::max)(0, survivor::ChipSlots(config_.weapons[static_cast<size_t>(target)], owned.rarity)));
    while (owned.chips.size() > capacity) {
        base_.chips.push_back(owned.chips.back());
        owned.chips.pop_back();
    }
    ++evolved_;
    world_.combatFlow->SetEventCallout("派生した：" + config_.weapons[static_cast<size_t>(target)].name, 1.2f);
    SaveBase();
    return true;
}

void SurvivorRunController::StartSortie()
{
    const bool any = std::any_of(base_.equipped.begin(), base_.equipped.end(), [&](int index) {
        return index >= 0 && static_cast<size_t>(index) < base_.weapons.size() &&
               survivor::FindWeapon(config_.weapons, base_.weapons[static_cast<size_t>(index)].key) >= 0;
    });
    if (!any) {
        world_.combatFlow->SetEventCallout("武器を1つは装備する", 1.2f);
        return;
    }
    baseScreenOpen_ = false;
    StartLoadout();
    ++base_.sorties;
    SaveBase();
}

void SurvivorRunController::CommitRunToBase()
{
    if (baseCommitted_ || !WeaponMode() || world_.demo.titleDemo_) {
        return;
    }
    baseCommitted_ = true;
    // 区画をクリアしたら、枠の武器（付けたチップ・袋から付け替えた武器）と出撃中の袋を拠点へ持ち帰る。倒れたら袋の中身は失う（G4）。
    if (cleared_) {
        for (size_t i = 0; i < runSlots_.size(); ++i) {
            const RunSlot& slot = runSlots_[i];
            if (slot.weapon.key.empty()) {
                base_.equipped[i] = -1;
            } else if (slot.baseIndex >= 0 && static_cast<size_t>(slot.baseIndex) < base_.weapons.size()) {
                base_.weapons[static_cast<size_t>(slot.baseIndex)] = slot.weapon;
                base_.equipped[i] = slot.baseIndex;
            } else {
                base_.weapons.push_back(slot.weapon);
                base_.equipped[i] = static_cast<int>(base_.weapons.size()) - 1;
            }
        }
        survivor::AddBagToBase(base_, bag_, config_.weapons);
        ++base_.clears;
        baseBroughtHome_ = true;
    }
    SaveBase();
}

} // namespace gameplay
