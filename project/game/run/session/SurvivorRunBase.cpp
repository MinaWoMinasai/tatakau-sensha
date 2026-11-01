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
    // 弾倉は満タン、予備は設定の数から始める。熱は冷えた状態から。
    reserves_ = config_.startReserve;
    for (size_t i = 0; i < weaponSlots_.size(); ++i) {
        magazines_[i] = weaponSlots_[i] >= 0 ? config_.weapons[static_cast<size_t>(weaponSlots_[i])].magazine : 0;
        weaponHeat_[i] = {};
    }
    weaponEquipped_ = false;
    activeSlot_ = first.value_or(0);
    EquipWeapon(activeSlot_);
    world_.resources.player_->RestoreHeat({});
    RefreshWeaponSlots();
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

void SurvivorRunController::UpdateBaseScreenInput()
{
    auto& input = *world_.resources.input_;
    const int count = static_cast<int>(base_.weapons.size());
    bool changed = false;
    if (count > 0 && (input.IsKeyTriggered(DIK_UP) || input.IsKeyTriggered(DIK_W))) {
        baseCursor_ = (baseCursor_ + count - 1) % count;
        changed = true;
    } else if (count > 0 && (input.IsKeyTriggered(DIK_DOWN) || input.IsKeyTriggered(DIK_S))) {
        baseCursor_ = (baseCursor_ + 1) % count;
        changed = true;
    }
    for (size_t i = 0; i < base_.equipped.size(); ++i) {
        if (input.IsKeyTriggered(static_cast<uint8_t>(DIK_1 + i))) {
            EquipFromBase(baseCursor_, i);
            changed = true;
        }
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
    view.help = "↑↓ で選ぶ　／　1〜3 で装備の枠へ入れる　／　Enter で出撃";
    view.bagTitle = baseError_.empty() ? "拠点の持ち物（自動でセーブ）" : "拠点の持ち物（" + baseError_ + "。新しく始めた）";
    // 拠点の画面では、装備の欄は「構えている」を出さず、拠点で選んだ武器を出す。
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
        const survivor::WeaponDefinition& weapon = config_.weapons[static_cast<size_t>(definition)];
        card.empty = false;
        card.color = survivor::RarityColor(owned.rarity);
        card.title = std::to_string(i + 1) + "　" + weapon.name + "　" + survivor::RarityName(owned.rarity);
        const int damage = static_cast<int>((std::max)(1.0f, std::round(world_.resources.player_->GetStats().bulletDamage * weapon.damageScale)));
        std::ostringstream detail;
        detail << survivor::AmmoName(weapon.ammo) << "　" << (weapon.magazine > 0 ? "弾倉 " + std::to_string(weapon.magazine) : std::string("弾倉なし"))
               << "\n威力 " << damage << "　連射 ×" << std::fixed << std::setprecision(1) << 1.0f / (std::max)(0.05f, weapon.reloadScale)
               << "　弾速 ×" << weapon.speedScale << "　熱 ×" << weapon.heatScale << "\n切り替え " << weapon.swapSeconds << " 秒　"
               << DescribeChips(owned, weapon);
        card.detail = detail.str();
    }

    using Line = SurvivorInventoryHud::Line;
    const myEngine::Vector4 plain{0.85f, 0.92f, 1.0f, 1.0f};
    view.bag.clear();
    view.bag.push_back({"お金　" + std::to_string(base_.money) + "　　出撃 " + std::to_string(base_.sorties) + " 回・持ち帰り " +
                            std::to_string(base_.clears) + " 回",
                        plain});
    view.bag.push_back({"素材　四角 " + std::to_string(base_.materials[0]) + "・三角 " + std::to_string(base_.materials[1]) + "・五角 " +
                            std::to_string(base_.materials[2]) + "・ボス " + std::to_string(base_.materials[3]),
                        plain});
    view.bag.push_back({"チップ　" + std::to_string(base_.chips.size()) + " 個（武器に付けるのは休憩部屋で）", plain});
    view.bag.push_back({"拠点の武器（" + std::to_string(base_.weapons.size()) + "）", {1.0f, 0.86f, 0.36f, 1.0f}});
    // 選んでいる行が見えるように、一覧を動かす。
    const int count = static_cast<int>(base_.weapons.size());
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
        view.bag.push_back(Line{text, definition >= 0 ? survivor::RarityColor(owned.rarity) : myEngine::Vector4{0.5f, 0.56f, 0.64f, 1.0f}});
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
