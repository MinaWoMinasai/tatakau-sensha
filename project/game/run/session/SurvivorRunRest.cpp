#include "game/run/session/SurvivorRunController.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/session/GameplaySystems.h"
#include <algorithm>

// 育つ戦車の試作の休憩部屋（計画書 p3 の W6・G6：メニューの形）。

namespace gameplay {

void SurvivorRunController::OpenRestRoom()
{
    restRoomOpen_ = true;
    SyncAmmoFromPlayer();
    RefreshRestRoom();
}

void SurvivorRunController::ApplyStageSettings(int stage)
{
    stage_ = (std::clamp)(stage, 0, survivor::SpawnConfig::kStageCount - 1);
    // ステージ1・2は時間で出口が開き、ボスステージは exitSeconds でボス（宿敵）が出て、倒すと出口が開く（G3）。
    config_.exitOpenSeconds = config_.stages.exitSeconds[static_cast<size_t>(stage_)];
    config_.rival.enabled = IsBossStage();
}

void SurvivorRunController::StartNextStage()
{
    if (IsBossStage()) {
        return;
    }
    restRoomOpen_ = false;
    ApplyStageSettings(stage_ + 1);
    // 区画の時計・湧き方・脅威・出口・宿敵・床の物を、区画の始めの状態へ戻す（拾った物の袋と枠の武器・弾はそのまま）。
    elapsed_ = 0.0f;
    cleared_ = false;
    exitOpen_ = false;
    rivalSpawned_ = false;
    rivalDefeated_ = false;
    rivalSpawnedAt_ = -1.0f;
    rivalDefeatedAt_ = -1.0f;
    threatStage_ = 1;
    spawnBudget_ = 0.0f;
    nextSwarm_ = config_.swarm.from;
    nextTank_ = config_.tanks.from;
    nextBigPentagon_ = config_.bigPentagon.from;
    nextHeavy_ = 0.0f;
    nextRing_ = 0.0f;
    pendingWaves_.clear();
    ResetEliteTimers();
    pickups_.Clear();
    lootField_.Clear();
    damageNumbers_.Clear();
    world_.resources.enemyManager_->ClearRunActors();
    world_.resources.bulletManager_->ClearAll();
    // 休憩したので、HPは満タンに戻し、部屋の真ん中から始める。
    auto& player = *world_.resources.player_;
    player.ResetRunRoomState(survivorrun::kPlayerStart);
    player.HealRunPlayer(player.GetMaxHp());
    barrierStamina_ = config_.barrier.staminaMax;
    barrierExhausted_ = 0.0f;
    barrierOn_ = false;
    barrierAwaitRelease_ = false;
    world_.resources.camera->SetTranslate({survivorrun::kPlayerStart.x, survivorrun::kPlayerStart.y, world_.resources.camera->GetTranslate().z});
    world_.resources.camera->Update();
    weaponEquipped_ = false;
    EquipWeapon(activeSlot_);
    // 機体の技の待ち時間と走るは戻す（Q のゲージは持ち越す）。
    if (HeroMode()) {
        abilityCooldown_.fill(0.0f);
        sprintTimer_ = 0.0f;
        player.SetSurvivorMoveScale(1.0f);
    }
    const std::string name = IsBossStage() ? std::string("ボスステージ") : "ステージ " + std::to_string(stage_ + 1);
    world_.combatFlow->SetEventCallout(name, 1.6f);
}

void SurvivorRunController::LeaveRestRoom()
{
    restRoomOpen_ = false;
    world_.combatFlow->EnterResultState(true);
}

void SurvivorRunController::RefreshSlotsAfterChange(size_t changedSlot)
{
    SyncAmmoFromPlayer();
    SyncSlotWeapons();
    if (changedSlot < weaponSlots_.size()) {
        magazines_[changedSlot] = weaponSlots_[changedSlot] >= 0 ? SlotWeapon(changedSlot).magazine : 0;
        weaponHeat_[changedSlot] = {};
    }
    // 構えている武器を入れ直して、形・弾・チップを効かせ直す（替えた回数には数えない）。
    weaponEquipped_ = false;
    EquipWeapon(activeSlot_);
    RefreshWeaponSlots();
}

bool SurvivorRunController::RestEquip(int bagWeapon, size_t slot)
{
    if (slot >= runSlots_.size() || bagWeapon < 0 || static_cast<size_t>(bagWeapon) >= bag_.weapons.size()) {
        return false;
    }
    const survivor::LootBag::Weapon picked = bag_.weapons[static_cast<size_t>(bagWeapon)];
    bag_.weapons.erase(bag_.weapons.begin() + bagWeapon);
    // 枠にあった武器から、この出撃で付けたチップを袋へ戻す。袋から付けた武器なら、武器も袋へ戻す。
    RunSlot& current = runSlots_[slot];
    if (!current.weapon.key.empty()) {
        for (size_t i = current.baseChips; i < current.weapon.chips.size(); ++i) {
            bag_.chips.push_back(current.weapon.chips[i]);
        }
        if (current.baseIndex < 0) {
            const int definition = survivor::FindWeapon(config_.weapons, current.weapon.key);
            if (definition >= 0) {
                bag_.weapons.push_back({definition, current.weapon.rarity});
            }
        }
    }
    current = {};
    current.weapon.key = config_.weapons[static_cast<size_t>(picked.weapon)].key;
    current.weapon.rarity = picked.rarity;
    ++restEquips_;
    RefreshSlotsAfterChange(slot);
    return true;
}

bool SurvivorRunController::RestAttach(int bagChip, size_t slot)
{
    if (slot >= runSlots_.size() || weaponSlots_[slot] < 0 || bagChip < 0 || static_cast<size_t>(bagChip) >= bag_.chips.size()) {
        return false;
    }
    RunSlot& target = runSlots_[slot];
    const int capacity = survivor::ChipSlots(SlotWeapon(slot), target.weapon.rarity);
    if (static_cast<int>(target.weapon.chips.size()) >= capacity) {
        world_.combatFlow->SetEventCallout(capacity == 0 ? "コモンの武器にはチップを付けられない" : "チップ枠が空いていない", 1.2f);
        return false;
    }
    target.weapon.chips.push_back(bag_.chips[static_cast<size_t>(bagChip)]);
    bag_.chips.erase(bag_.chips.begin() + bagChip);
    ++restAttaches_;
    RefreshSlotsAfterChange(survivor::kWeaponSlots);
    return true;
}

bool SurvivorRunController::RestDetach(size_t slot, size_t chip)
{
    if (slot >= runSlots_.size()) {
        return false;
    }
    RunSlot& target = runSlots_[slot];
    if (chip < target.baseChips || chip >= target.weapon.chips.size()) {
        return false;
    }
    bag_.chips.push_back(target.weapon.chips[chip]);
    target.weapon.chips.erase(target.weapon.chips.begin() + static_cast<std::ptrdiff_t>(chip));
    ++restDetaches_;
    RefreshSlotsAfterChange(survivor::kWeaponSlots);
    return true;
}

int SurvivorRunController::RestSell(ScreenItem kind, int index)
{
    int price = 0;
    if (kind == ScreenItem::BagWeapon && index >= 0 && static_cast<size_t>(index) < bag_.weapons.size()) {
        price = config_.loot.weaponPrice[static_cast<size_t>(bag_.weapons[static_cast<size_t>(index)].rarity)];
        bag_.weapons.erase(bag_.weapons.begin() + index);
    } else if (kind == ScreenItem::BagChip && index >= 0 && static_cast<size_t>(index) < bag_.chips.size()) {
        price = config_.loot.chipPrice[static_cast<size_t>(bag_.chips[static_cast<size_t>(index)].rarity)];
        bag_.chips.erase(bag_.chips.begin() + index);
    } else if (kind == ScreenItem::Material && index >= 0 && static_cast<size_t>(index) < survivor::kMaterialCount) {
        int& count = bag_.materials[static_cast<size_t>(index)];
        if (count <= 0) {
            return 0;
        }
        price = count * config_.loot.materialPrice[static_cast<size_t>(index)];
        count = 0;
    } else {
        return 0;
    }
    bag_.money += price;
    ++restSales_;
    restSoldMoney_ += price;
    return price;
}

bool SurvivorRunController::RestDiscard(ScreenItem kind, int index)
{
    if (kind == ScreenItem::BagWeapon && index >= 0 && static_cast<size_t>(index) < bag_.weapons.size()) {
        bag_.weapons.erase(bag_.weapons.begin() + index);
    } else if (kind == ScreenItem::BagChip && index >= 0 && static_cast<size_t>(index) < bag_.chips.size()) {
        bag_.chips.erase(bag_.chips.begin() + index);
    } else if (kind == ScreenItem::Material && index >= 0 && static_cast<size_t>(index) < survivor::kMaterialCount &&
               bag_.materials[static_cast<size_t>(index)] > 0) {
        bag_.materials[static_cast<size_t>(index)] = 0;
    } else {
        return false;
    }
    ++restDiscards_;
    return true;
}

} // namespace gameplay
