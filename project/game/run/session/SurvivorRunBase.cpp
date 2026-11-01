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

namespace {
// 拠点の画面の右の欄：上の4行（お金・素材・チップ・見出し）の下に、拠点の武器を並べる。
constexpr int kBaseHeaderLines = 4;
constexpr int kBaseListLines = static_cast<int>(SurvivorInventoryHud::kBagLines) - kBaseHeaderLines;
} // namespace

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
    baseScroll_ = 0;
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

void SurvivorRunController::UpdateBaseScreenInput()
{
    auto& input = *world_.resources.input_;
    bool changed = false;
    // Tab で右の欄を「武器 → 作る → 派生の木」の順に替える。
    if (input.IsKeyTriggered(DIK_TAB)) {
        basePage_ = (basePage_ + 1) % 3;
        changed = true;
    }
    const bool craftPage = basePage_ == 1;
    const int count = craftPage ? static_cast<int>(config_.crafting.recipes.size()) : static_cast<int>(base_.weapons.size());
    int& cursor = craftPage ? craftCursor_ : baseCursor_;
    if (count > 0 && (input.IsKeyTriggered(DIK_UP) || input.IsKeyTriggered(DIK_W))) {
        cursor = (cursor + count - 1) % count;
        changed = true;
    } else if (count > 0 && (input.IsKeyTriggered(DIK_DOWN) || input.IsKeyTriggered(DIK_S))) {
        cursor = (cursor + 1) % count;
        changed = true;
    }
    if (basePage_ == 0) {
        for (size_t i = 0; i < base_.equipped.size(); ++i) {
            if (input.IsKeyTriggered(static_cast<uint8_t>(DIK_1 + i))) {
                EquipFromBase(baseCursor_, i);
                changed = true;
            }
        }
    }
    if (craftPage && input.IsKeyTriggered(DIK_C)) {
        changed = CraftWeapon(static_cast<size_t>(craftCursor_)) || changed;
    }
    if (!craftPage && input.IsKeyTriggered(DIK_U)) {
        changed = EvolveBaseWeapon(baseCursor_) || changed;
    }
    if (input.IsKeyTriggered(DIK_RETURN) || input.IsKeyTriggered(DIK_SPACE)) {
        StartSortie();
        return;
    }
    if (changed) {
        RefreshBaseScreen();
    }
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

void SurvivorRunController::RefreshBaseScreen()
{
    SurvivorInventoryHud::View view = BuildInventoryView();
    view.title = "拠点";
    view.bagTitle = baseError_.empty() ? "拠点の持ち物（自動でセーブ）" : "拠点の持ち物（" + baseError_ + "。新しく始めた）";
    const float bulletDamage = world_.resources.player_->GetStats().bulletDamage;
    auto damageOf = [&](const survivor::WeaponDefinition& weapon) {
        return static_cast<int>((std::max)(1.0f, std::round(bulletDamage * weapon.damageScale)));
    };
    // 拠点の画面では、装備の欄は「構えている」を出さず、拠点で選んだ武器を出す（レア度を効かせた値）。
    for (size_t i = 0; i < view.weapons.size(); ++i) {
        auto& card = view.weapons[i];
        card = {};
        card.title = std::to_string(i + 1) + "　武器";
        const int index = base_.equipped[i];
        if (index < 0 || static_cast<size_t>(index) >= base_.weapons.size()) {
            continue;
        }
        const survivor::BaseWeapon& owned = base_.weapons[static_cast<size_t>(index)];
        const int definition = survivor::FindWeapon(config_.weapons, owned.key);
        if (definition < 0) {
            continue;
        }
        const survivor::WeaponDefinition weapon =
            survivor::ApplyRarity(config_.weapons[static_cast<size_t>(definition)], owned.rarity, config_.crafting);
        card.empty = false;
        card.color = survivor::RarityColor(owned.rarity);
        card.title = std::to_string(i + 1) + "　" + weapon.name + "　" + survivor::RarityName(owned.rarity);
        std::ostringstream detail;
        detail << survivor::AmmoName(weapon.ammo) << "　" << (weapon.magazine > 0 ? "弾倉 " + std::to_string(weapon.magazine) : std::string("弾倉なし"))
               << "\n威力 " << damageOf(weapon) << "　連射 ×" << std::fixed << std::setprecision(1) << 1.0f / (std::max)(0.05f, weapon.reloadScale)
               << "　弾速 ×" << weapon.speedScale << "　熱 ×" << weapon.heatScale << "\n切り替え " << weapon.swapSeconds << " 秒　"
               << DescribeChips(owned, weapon);
        card.detail = detail.str();
    }

    using Line = SurvivorInventoryHud::Line;
    const myEngine::Vector4 plain{0.85f, 0.92f, 1.0f, 1.0f};
    const myEngine::Vector4 header{1.0f, 0.86f, 0.36f, 1.0f};
    const myEngine::Vector4 grey{0.5f, 0.56f, 0.64f, 1.0f};
    view.bag.clear();
    view.bag.push_back({"お金　" + std::to_string(base_.money) + "　　出撃 " + std::to_string(base_.sorties) + " 回・持ち帰り " +
                            std::to_string(base_.clears) + " 回",
                        plain});
    view.bag.push_back({"素材　四角 " + std::to_string(base_.materials[0]) + "・三角 " + std::to_string(base_.materials[1]) + "・五角 " +
                            std::to_string(base_.materials[2]) + "・ボス " + std::to_string(base_.materials[3]),
                        plain});
    view.bag.push_back({"チップ　" + std::to_string(base_.chips.size()) + " 個（武器に付けるのは休憩部屋で）", plain});
    const std::string pages = "　　［Tab：武器・作る・木］";

    if (basePage_ == 1) {
        // 作る：レシピごとに要る物。足りないレシピは灰色。
        view.help = "↑↓ で選ぶ　／　C：作る（コモンを拠点へ）　／　Tab：次の欄　／　Enter で出撃";
        view.bag.push_back({"作る" + pages, header});
        const int count = static_cast<int>(config_.crafting.recipes.size());
        craftCursor_ = (std::clamp)(craftCursor_, 0, (std::max)(0, count - 1));
        for (const survivor::WeaponRecipe& recipe : config_.crafting.recipes) {
            const int definition = survivor::FindWeapon(config_.weapons, recipe.key);
            const std::string name = definition >= 0 ? config_.weapons[static_cast<size_t>(definition)].name : recipe.key;
            const bool affordable = definition >= 0 && survivor::CanAfford(base_.materials, base_.money, recipe.cost);
            view.bag.push_back(Line{name + "　" + survivor::DescribeCost(recipe.cost) + (affordable ? "" : "　（足りない）"), affordable ? plain : grey});
        }
        view.cursorLine = count > 0 ? kBaseHeaderLines + craftCursor_ : -1;
        inventoryHud_.Show(view);
        return;
    }

    const int count = static_cast<int>(base_.weapons.size());
    baseCursor_ = (std::clamp)(baseCursor_, 0, (std::max)(0, count - 1));
    if (basePage_ == 2 && count > 0) {
        // 派生の木：選んでいる武器が、レア度を1段ずつ上げていく道。今の段に印、上の段には要る物を出す。
        const survivor::BaseWeapon& owned = base_.weapons[static_cast<size_t>(baseCursor_)];
        const int definition = survivor::FindWeapon(config_.weapons, owned.key);
        view.help = "U：次の段へ進化　／　Tab：次の欄　／　Enter で出撃";
        view.bag.push_back({"派生の木：" + (definition >= 0 ? config_.weapons[static_cast<size_t>(definition)].name : owned.key) + pages, header});
        if (definition >= 0) {
            for (size_t r = 0; r < survivor::kRarityCount; ++r) {
                const auto rarity = static_cast<survivor::Rarity>(r);
                const survivor::WeaponDefinition weapon =
                    survivor::ApplyRarity(config_.weapons[static_cast<size_t>(definition)], rarity, config_.crafting);
                std::string text = std::string(r ? "　└ " : "　") + survivor::RarityName(rarity) + "　威力 " + std::to_string(damageOf(weapon)) +
                                   (weapon.magazine > 0 ? "・弾倉 " + std::to_string(weapon.magazine) : std::string()) + "・チップ枠 " +
                                   std::to_string(survivor::ChipSlots(weapon, rarity));
                if (rarity == owned.rarity) {
                    view.bag.push_back(Line{text + "　［今］", survivor::RarityColor(rarity)});
                    continue;
                }
                if (rarity < owned.rarity) {
                    view.bag.push_back(Line{text, grey});
                    continue;
                }
                // 今より上の段は、その段へ上げるのに要る物（すぐ次の段だけ、足りるかも出す）。
                const survivor::CraftCost cost = survivor::EvolveCost(config_.crafting, owned.key, rarity);
                const bool next = static_cast<int>(rarity) == static_cast<int>(owned.rarity) + 1;
                const bool affordable = survivor::CanAfford(base_.materials, base_.money, cost);
                view.bag.push_back(Line{text, survivor::RarityColor(rarity)});
                view.bag.push_back(Line{"　　　要る物：" + survivor::DescribeCost(cost) + (next ? (affordable ? "　（U で進化できる）" : "　（足りない）") : ""),
                                        next && affordable ? plain : grey});
            }
        }
        view.cursorLine = -1;
        inventoryHud_.Show(view);
        return;
    }

    // 武器：拠点の武器の一覧。選んでいる行が見えるように、一覧を動かす。
    std::string evolve = "U：進化";
    if (count > 0) {
        const survivor::BaseWeapon& owned = base_.weapons[static_cast<size_t>(baseCursor_)];
        if (owned.rarity != survivor::Rarity::Legendary) {
            const auto next = static_cast<survivor::Rarity>(static_cast<int>(owned.rarity) + 1);
            evolve += "（" + survivor::DescribeCost(survivor::EvolveCost(config_.crafting, owned.key, next)) + "）";
        }
    }
    view.help = "↑↓ で選ぶ　／　1〜3 で装備の枠へ　／　" + evolve + "　／　Tab：作る・木　／　Enter で出撃";
    view.bag.push_back({"拠点の武器（" + std::to_string(base_.weapons.size()) + "）" + pages, header});
    baseScroll_ = (std::clamp)(baseScroll_, (std::max)(0, baseCursor_ - kBaseListLines + 1), (std::max)(0, baseCursor_));
    baseScroll_ = (std::clamp)(baseScroll_, 0, (std::max)(0, count - kBaseListLines));
    for (int i = baseScroll_; i < count && i < baseScroll_ + kBaseListLines; ++i) {
        const survivor::BaseWeapon& owned = base_.weapons[static_cast<size_t>(i)];
        const int definition = survivor::FindWeapon(config_.weapons, owned.key);
        std::string text = definition >= 0 ? config_.weapons[static_cast<size_t>(definition)].name + "（" + survivor::RarityName(owned.rarity) + "）"
                                           : "（使えない武器：" + owned.key + "）";
        for (size_t slot = 0; slot < base_.equipped.size(); ++slot) {
            if (base_.equipped[slot] == i) {
                text += "　［枠 " + std::to_string(slot + 1) + "］";
            }
        }
        view.bag.push_back(Line{text, definition >= 0 ? survivor::RarityColor(owned.rarity) : grey});
    }
    view.cursorLine = count > 0 ? kBaseHeaderLines + baseCursor_ - baseScroll_ : -1;
    inventoryHud_.Show(view);
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
