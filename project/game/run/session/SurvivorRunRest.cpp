#include "game/run/session/SurvivorRunController.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/session/GameplaySystems.h"
#include <algorithm>
#include <numeric>

// 育つ戦車の試作の休憩部屋（計画書 p3 の W6・G6：メニューの形）。

namespace gameplay {

namespace {
using Kind = SurvivorRunController::RestRow::Kind;
const myEngine::Vector4 kPlain{0.85f, 0.92f, 1.0f, 1.0f};
const myEngine::Vector4 kHeader{1.0f, 0.86f, 0.36f, 1.0f};
const myEngine::Vector4 kGrey{0.5f, 0.56f, 0.64f, 1.0f};

bool Selectable(Kind kind)
{
    return kind != Kind::Header;
}
} // namespace

void SurvivorRunController::OpenRestRoom()
{
    restRoomOpen_ = true;
    restCursor_ = 0;
    restScroll_ = 0;
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

int SurvivorRunController::RestSell(const RestRow& row)
{
    int price = 0;
    if (row.kind == Kind::BagWeapon && row.index >= 0 && static_cast<size_t>(row.index) < bag_.weapons.size()) {
        price = config_.loot.weaponPrice[static_cast<size_t>(bag_.weapons[static_cast<size_t>(row.index)].rarity)];
        bag_.weapons.erase(bag_.weapons.begin() + row.index);
    } else if (row.kind == Kind::BagChip && row.index >= 0 && static_cast<size_t>(row.index) < bag_.chips.size()) {
        price = config_.loot.chipPrice[static_cast<size_t>(bag_.chips[static_cast<size_t>(row.index)].rarity)];
        bag_.chips.erase(bag_.chips.begin() + row.index);
    } else if (row.kind == Kind::Material && row.index >= 0 && static_cast<size_t>(row.index) < survivor::kMaterialCount) {
        int& count = bag_.materials[static_cast<size_t>(row.index)];
        price = count * config_.loot.materialPrice[static_cast<size_t>(row.index)];
        if (count <= 0) {
            return 0;
        }
        count = 0;
    } else {
        return 0;
    }
    bag_.money += price;
    ++restSales_;
    restSoldMoney_ += price;
    return price;
}

bool SurvivorRunController::RestDiscard(const RestRow& row)
{
    if (row.kind == Kind::BagWeapon && row.index >= 0 && static_cast<size_t>(row.index) < bag_.weapons.size()) {
        bag_.weapons.erase(bag_.weapons.begin() + row.index);
    } else if (row.kind == Kind::BagChip && row.index >= 0 && static_cast<size_t>(row.index) < bag_.chips.size()) {
        bag_.chips.erase(bag_.chips.begin() + row.index);
    } else if (row.kind == Kind::Material && row.index >= 0 && static_cast<size_t>(row.index) < survivor::kMaterialCount &&
               bag_.materials[static_cast<size_t>(row.index)] > 0) {
        bag_.materials[static_cast<size_t>(row.index)] = 0;
    } else {
        return false;
    }
    ++restDiscards_;
    return true;
}

void SurvivorRunController::UpdateRestRoomInput()
{
    auto& input = *world_.resources.input_;
    const int count = static_cast<int>(restRows_.size());
    if (count == 0) {
        return;
    }
    auto move = [&](int step) {
        for (int n = 0; n < count; ++n) {
            restCursor_ = (restCursor_ + step + count) % count;
            if (Selectable(restRows_[static_cast<size_t>(restCursor_)].kind)) {
                break;
            }
        }
        RefreshRestRoom();
    };
    if (input.IsKeyTriggered(DIK_UP) || input.IsKeyTriggered(DIK_W)) {
        move(-1);
        return;
    }
    if (input.IsKeyTriggered(DIK_DOWN) || input.IsKeyTriggered(DIK_S)) {
        move(1);
        return;
    }
    const RestRow row = restRows_[static_cast<size_t>((std::clamp)(restCursor_, 0, count - 1))];
    if (input.IsKeyTriggered(DIK_RETURN) || input.IsKeyTriggered(DIK_SPACE)) {
        if (row.kind == Kind::Leave) {
            LeaveRestRoom();
        } else if (row.kind == Kind::Next && !IsBossStage()) {
            StartNextStage();
        }
        return;
    }
    bool changed = false;
    for (size_t slot = 0; slot < runSlots_.size(); ++slot) {
        if (!input.IsKeyTriggered(static_cast<uint8_t>(DIK_1 + slot))) {
            continue;
        }
        if (row.kind == Kind::BagWeapon) {
            changed = RestEquip(row.index, slot);
        } else if (row.kind == Kind::BagChip) {
            changed = RestAttach(row.index, slot);
        }
    }
    if (input.IsKeyTriggered(DIK_X)) {
        const int price = RestSell(row);
        if (price > 0) {
            world_.combatFlow->SetEventCallout("売った：+" + std::to_string(price), 0.8f);
            changed = true;
        }
    }
    if (input.IsKeyTriggered(DIK_Z)) {
        changed = row.kind == Kind::SlotChip ? RestDetach(static_cast<size_t>(row.slot), static_cast<size_t>(row.index)) : RestDiscard(row);
    }
    if (changed) {
        RefreshRestRoom();
    }
}

void SurvivorRunController::RefreshRestRoom()
{
    SurvivorInventoryHud::View view = BuildInventoryView();
    view.title = "休憩部屋";
    view.bagTitle = "出撃中の袋　お金 " + std::to_string(bag_.money);
    std::vector<SurvivorInventoryHud::Line> lines;
    restRows_.clear();
    auto add = [&](RestRow row, const std::string& text, const myEngine::Vector4& color) {
        restRows_.push_back(row);
        lines.push_back({text, color});
    };
    add({Kind::Leave}, "帰る：枠の武器と袋を持って拠点へ（セーブ）", kPlain);
    const int nextStage = stage_ + 2;
    if (IsBossStage()) {
        add({Kind::Next}, "次へ進む（ボスを倒した。あとは帰るだけ）", kGrey);
    } else {
        add({Kind::Next},
            "次へ進む：" + std::string(nextStage == survivor::SpawnConfig::kStageCount ? "ボスステージ" : "ステージ " + std::to_string(nextStage)) +
                "（HPが戻る。倒れたら拾った物は失う）",
            kPlain);
    }

    // 袋の武器とチップは、レア度の高い順に並べる（番号は袋の中の順のまま持つ）。
    auto order = [](size_t size, auto rarityOf) {
        std::vector<int> indices(size);
        std::iota(indices.begin(), indices.end(), 0);
        std::stable_sort(indices.begin(), indices.end(), [&](int a, int b) {
            return rarityOf(a) > rarityOf(b);
        });
        return indices;
    };
    add({Kind::Header}, "袋の武器（" + std::to_string(bag_.weapons.size()) + "）", kHeader);
    for (int i : order(bag_.weapons.size(), [&](int index) { return bag_.weapons[static_cast<size_t>(index)].rarity; })) {
        const auto& owned = bag_.weapons[static_cast<size_t>(i)];
        const auto& weapon = config_.weapons[static_cast<size_t>(owned.weapon)];
        add({Kind::BagWeapon, i}, "　" + weapon.name + "（" + survivor::RarityName(owned.rarity) + "）　チップ枠 " +
                                      std::to_string(survivor::ChipSlots(weapon, owned.rarity)),
            survivor::RarityColor(owned.rarity));
    }
    add({Kind::Header}, "袋のチップ（" + std::to_string(bag_.chips.size()) + "）", kHeader);
    for (int i : order(bag_.chips.size(), [&](int index) { return bag_.chips[static_cast<size_t>(index)].rarity; })) {
        const auto& chip = bag_.chips[static_cast<size_t>(i)];
        add({Kind::BagChip, i}, "　" + std::string(config_.parts[static_cast<size_t>(chip.part)].name) + "（" + survivor::RarityName(chip.rarity) + "）",
            survivor::RarityColor(chip.rarity));
    }
    add({Kind::Header}, "この出撃で付けたチップ", kHeader);
    for (size_t slot = 0; slot < runSlots_.size(); ++slot) {
        const RunSlot& owned = runSlots_[slot];
        for (size_t c = owned.baseChips; c < owned.weapon.chips.size(); ++c) {
            const auto& chip = owned.weapon.chips[c];
            add({Kind::SlotChip, static_cast<int>(c), static_cast<int>(slot)},
                "　枠 " + std::to_string(slot + 1) + "：" + config_.parts[static_cast<size_t>(chip.part)].name + "（" + survivor::RarityName(chip.rarity) + "）",
                survivor::RarityColor(chip.rarity));
        }
    }
    add({Kind::Header}, "素材", kHeader);
    for (size_t i = 0; i < survivor::kMaterialCount; ++i) {
        add({Kind::Material, static_cast<int>(i)},
            "　" + std::string(survivor::MaterialName(static_cast<survivor::MaterialId>(i))) + " ×" + std::to_string(bag_.materials[i]) + "（1つ " +
                std::to_string(config_.loot.materialPrice[i]) + "）",
            bag_.materials[i] > 0 ? kPlain : kGrey);
    }

    // 選んでいる行を、選べる行に合わせ、見えるように一覧を動かす。
    const int count = static_cast<int>(restRows_.size());
    restCursor_ = (std::clamp)(restCursor_, 0, count - 1);
    while (!Selectable(restRows_[static_cast<size_t>(restCursor_)].kind) && restCursor_ > 0) {
        --restCursor_;
    }
    const int window = static_cast<int>(SurvivorInventoryHud::kBagLines);
    restScroll_ = (std::clamp)(restScroll_, (std::max)(0, restCursor_ - window + 1), restCursor_);
    restScroll_ = (std::clamp)(restScroll_, 0, (std::max)(0, count - window));
    view.bag.assign(lines.begin() + restScroll_, lines.begin() + (std::min)(count, restScroll_ + window));
    view.cursorLine = restCursor_ - restScroll_;

    // 操作の案内は、選んでいる行でできることを出す。
    const RestRow& row = restRows_[static_cast<size_t>(restCursor_)];
    std::string help = "↑↓ で選ぶ　／　";
    switch (row.kind) {
    case Kind::Leave:
        help += "Enter：拠点へ帰る";
        break;
    case Kind::Next:
        help += IsBossStage() ? "最後のステージ" : "Enter：次のステージへ進む";
        break;
    case Kind::BagWeapon:
        help += "1〜3：その枠へ付け替える　／　X：売る（" +
                std::to_string(config_.loot.weaponPrice[static_cast<size_t>(bag_.weapons[static_cast<size_t>(row.index)].rarity)]) +
                "）　／　Z：捨てる";
        break;
    case Kind::BagChip:
        help += "1〜3：その枠の武器に付ける　／　X：売る（" +
                std::to_string(config_.loot.chipPrice[static_cast<size_t>(bag_.chips[static_cast<size_t>(row.index)].rarity)]) + "）　／　Z：捨てる";
        break;
    case Kind::SlotChip:
        help += "Z：外して袋へ戻す";
        break;
    case Kind::Material:
        help += "X：全部売る（" +
                std::to_string(bag_.materials[static_cast<size_t>(row.index)] * config_.loot.materialPrice[static_cast<size_t>(row.index)]) +
                "）　／　Z：全部捨てる";
        break;
    case Kind::Header:
        break;
    }
    view.help = help;
    inventoryHud_.Show(view);
}

} // namespace gameplay
