#include "game/run/session/SurvivorRunController.h"
#include "game/session/GameplaySystems.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>

// 育つ戦車の試作の、アイコン中心の画面（持ち物・拠点・休憩部屋）の中身とマウスの操作（計画書 p3 の X4）。

namespace gameplay {
namespace {
using Item = SurvivorItemScreen::Item;
using Zone = SurvivorItemScreen::Zone;
using EventType = SurvivorItemScreen::EventType;
const myEngine::Vector4 kPlain{0.85f, 0.92f, 1.0f, 1.0f};
const std::string kIconDirectory = "resources/survivor_icons/";

std::string Icon(const std::string& name)
{
    return kIconDirectory + name + ".png";
}

std::string WeaponIcon(const std::string& key)
{
    // 絵の無い武器（設定で足した物）は単発の砲の絵にする。
    static const std::array<const char*, 8> kKnown{"cannon", "machineGun", "grenade", "beam", "shotgun", "sniper", "homing", "ricochet"};
    return Icon("weapon_" + (std::find(kKnown.begin(), kKnown.end(), key) != kKnown.end() ? key : std::string("cannon")));
}

std::string ChipIcon(const std::string& key)
{
    static const std::array<const char*, 13> kKnown{"armor", "ram", "warhead", "loader", "thruster", "collector", "cooler",
                                                    "pierce", "chainLightning", "killBurst", "heavyWarhead", "residualHeat", "morale"};
    return Icon("chip_" + (std::find(kKnown.begin(), kKnown.end(), key) != kKnown.end() ? key : std::string("default")));
}

std::string MaterialIcon(size_t material)
{
    return Icon(std::string("material_") + survivor::MaterialKey(static_cast<survivor::MaterialId>(material)));
}

std::string Fixed(float value, int digits)
{
    std::ostringstream text;
    text << std::fixed << std::setprecision(digits) << value;
    return text.str();
}
} // namespace

std::vector<std::string> SurvivorRunController::ScreenIconPaths() const
{
    std::vector<std::string> icons;
    for (const char* name : {"weapon_cannon", "weapon_machineGun", "weapon_grenade", "weapon_beam", "weapon_shotgun", "weapon_sniper", "weapon_homing", "weapon_ricochet", "ammo_light", "ammo_heavy", "ammo_energy", "material_square",
                             "material_triangle", "material_pentagon", "material_boss", "money", "shield_barrier", "support_empty", "chip_armor", "chip_ram",
                             "chip_warhead", "chip_loader", "chip_thruster", "chip_collector", "chip_cooler", "chip_pierce", "chip_chainLightning", "chip_killBurst",
                             "chip_heavyWarhead", "chip_residualHeat", "chip_morale", "chip_default"}) {
        icons.push_back(Icon(name));
    }
    return icons;
}

SurvivorItemScreen::Item SurvivorRunController::MakeChipItem(const survivor::LootBag::Chip& chip, ScreenItem kind, int id, const std::string& actions) const
{
    const survivor::PartDefinition& part = config_.parts[static_cast<size_t>(chip.part)];
    Item item;
    item.icon = ChipIcon(part.key);
    item.color = survivor::RarityColor(chip.rarity);
    item.kind = static_cast<int>(kind);
    item.id = id;
    item.title = std::string(part.name) + "　" + survivor::RarityName(chip.rarity);
    item.body = std::string(part.stat) + "\n部品の段 " + std::to_string((std::min)(part.maxLevel, survivor::ChipLevel(chip.rarity))) + "（構えている武器のチップだけが効く）";
    item.actions = actions;
    return item;
}

void SurvivorRunController::FillWeaponCard(SurvivorItemScreen::WeaponCard& card, size_t slot, const survivor::BaseWeapon& owned, const survivor::WeaponDefinition& weapon,
                                           int magazine, size_t removableFrom, const std::string& chipActions, const std::string& weaponActions) const
{
    const int damage = static_cast<int>((std::max)(1.0f, std::round(baseBulletDamage_ * weapon.damageScale)));
    const int reserve = reserves_[static_cast<size_t>(weapon.ammo)];
    card.empty = false;
    card.name = std::to_string(slot + 1) + "　" + weapon.name + "　" + survivor::RarityName(owned.rarity);
    std::string ammo = survivor::AmmoName(weapon.ammo);
    if (weapon.magazine > 0) {
        ammo += "　弾倉 " + (magazine >= 0 ? std::to_string(magazine) + "/" : std::string()) + std::to_string(weapon.magazine);
    } else {
        ammo += "　弾倉なし";
    }
    card.stats = ammo + (magazine >= 0 ? "　予備 " + std::to_string(reserve) : std::string()) + "\n威力 " + std::to_string(damage) + "　連射 ×" +
                 Fixed(1.0f / (std::max)(0.05f, weapon.reloadScale), 1) + "　熱 ×" + Fixed(weapon.heatScale, 1);
    card.item.icon = WeaponIcon(weapon.key);
    card.item.color = survivor::RarityColor(owned.rarity);
    card.item.title = weapon.name + "　" + survivor::RarityName(owned.rarity);
    std::string body = card.stats + "\n弾速 ×" + Fixed(weapon.speedScale, 1) + "　切り替え " + Fixed(weapon.swapSeconds, 1) + " 秒";
    if (weapon.magazine > 0) {
        body += "　リロード " + Fixed(weapon.reloadSeconds, 1) + " 秒";
    }
    if (weapon.blastRadius > 0.0f) {
        body += "\n当たると爆発（半径 " + Fixed(weapon.blastRadius, 1) + "）";
    }
    body += "\n" + DescribeChips(owned, weapon);
    card.item.body = body;
    card.item.actions = weaponActions;
    card.sockets = (std::min)(survivor::ChipSlots(weapon, owned.rarity), static_cast<int>(SurvivorItemScreen::kMaxSockets));
    for (size_t c = 0; c < owned.chips.size(); ++c) {
        const bool removable = c >= removableFrom;
        card.chips.push_back(MakeChipItem(owned.chips[c], removable ? ScreenItem::SlotChip : ScreenItem::None, static_cast<int>(c),
                                          removable ? chipActions : std::string()));
    }
}

SurvivorItemScreen::View SurvivorRunController::BuildScreenBase(const std::array<int, survivor::kMaterialCount>& materials, int money, bool showAmmo) const
{
    SurvivorItemScreen::View view;
    Item coin;
    coin.icon = Icon("money");
    coin.color = {1.0f, 0.85f, 0.35f, 1.0f};
    coin.count = money;
    coin.title = "お金";
    coin.body = "武器の進化に使う。";
    view.header.push_back(coin);
    for (size_t i = 0; i < survivor::kMaterialCount; ++i) {
        Item item;
        item.icon = MaterialIcon(i);
        item.color = i == static_cast<size_t>(survivor::MaterialId::Boss) ? myEngine::Vector4{1.0f, 0.4f, 0.5f, 1.0f} : myEngine::Vector4{0.82f, 0.8f, 1.0f, 1.0f};
        item.count = materials[i];
        item.title = survivor::MaterialName(static_cast<survivor::MaterialId>(i));
        item.body = "武器を作る・進化させるのに使う。";
        view.header.push_back(item);
    }
    if (showAmmo) {
        static const std::array<myEngine::Vector4, survivor::kAmmoTypeCount> kColors{myEngine::Vector4{1.0f, 0.92f, 0.45f, 1.0f}, myEngine::Vector4{1.0f, 0.55f, 0.2f, 1.0f},
                                                                                    myEngine::Vector4{0.35f, 0.9f, 1.0f, 1.0f}};
        for (size_t i = 0; i < survivor::kAmmoTypeCount; ++i) {
            Item item;
            const auto type = static_cast<survivor::AmmoType>(i);
            item.icon = Icon(std::string("ammo_") + survivor::AmmoKey(type));
            item.color = kColors[i];
            item.count = reserves_[i];
            item.title = survivor::AmmoName(type);
            item.body = "予備 " + std::to_string(reserves_[i]) + " / 上限 " + std::to_string(config_.loot.reserveMax[i]);
            view.header.push_back(item);
        }
    }
    view.shield.icon = Icon("shield_barrier");
    view.shield.color = {0.55f, 1.0f, 0.9f, 1.0f};
    view.shield.title = "網目のバリア";
    view.shield.body = "右クリックを押している間、前に開き " + Fixed(config_.barrier.arcDegrees, 0) + " 度・半径 " + Fixed(config_.barrier.radius, 1) +
                       " の扇を張る。\n敵の弾と敵を止める。張っている間と、弾を止めたときにスタミナが減る。";
    view.support.icon = Icon("support_empty");
    view.support.color = {0.5f, 0.56f, 0.64f, 1.0f};
    view.support.dim = true;
    view.support.title = "補助（空き）";
    view.support.body = "レリックや回復キットを入れる枠。中身は後で入る。";
    return view;
}

void SurvivorRunController::AddBagToGrid(SurvivorItemScreen::View& view, bool actions) const
{
    // 武器 → チップ → 素材の順。武器とチップはレア度の高い順に並べる（id は袋の中の番号のまま）。
    auto order = [](size_t size, auto rarityOf) {
        std::vector<int> indices(size);
        std::iota(indices.begin(), indices.end(), 0);
        std::stable_sort(indices.begin(), indices.end(), [&](int a, int b) {
            return rarityOf(a) > rarityOf(b);
        });
        return indices;
    };
    for (int i : order(bag_.weapons.size(), [&](int index) { return bag_.weapons[static_cast<size_t>(index)].rarity; })) {
        const auto& owned = bag_.weapons[static_cast<size_t>(i)];
        const auto& definition = config_.weapons[static_cast<size_t>(owned.weapon)];
        const survivor::WeaponDefinition weapon = survivor::ApplyRarity(definition, owned.rarity, config_.crafting);
        Item item;
        item.icon = WeaponIcon(definition.key);
        item.color = survivor::RarityColor(owned.rarity);
        item.draggable = actions;
        item.kind = static_cast<int>(ScreenItem::BagWeapon);
        item.id = i;
        item.title = weapon.name + "　" + survivor::RarityName(owned.rarity);
        const int damage = static_cast<int>((std::max)(1.0f, std::round(baseBulletDamage_ * weapon.damageScale)));
        item.body = std::string(survivor::AmmoName(weapon.ammo)) + (weapon.magazine > 0 ? "　弾倉 " + std::to_string(weapon.magazine) : std::string("　弾倉なし")) + "\n威力 " +
                    std::to_string(damage) + "　連射 ×" + Fixed(1.0f / (std::max)(0.05f, weapon.reloadScale), 1) + "　チップ枠 " +
                    std::to_string(survivor::ChipSlots(weapon, owned.rarity));
        if (actions) {
            item.actions = "ドラッグ：武器の枠へ付け替える\n右クリック：売る（" + std::to_string(config_.loot.weaponPrice[static_cast<size_t>(owned.rarity)]) + "）　Z：捨てる";
        }
        view.grid.push_back(item);
    }
    for (int i : order(bag_.chips.size(), [&](int index) { return bag_.chips[static_cast<size_t>(index)].rarity; })) {
        const auto& chip = bag_.chips[static_cast<size_t>(i)];
        Item item = MakeChipItem(chip, ScreenItem::BagChip, i,
                                 actions ? "ドラッグ：武器に付ける\n右クリック：売る（" + std::to_string(config_.loot.chipPrice[static_cast<size_t>(chip.rarity)]) + "）　Z：捨てる" : std::string());
        item.draggable = actions;
        view.grid.push_back(item);
    }
    if (actions) {
        for (size_t i = 0; i < survivor::kMaterialCount; ++i) {
            if (bag_.materials[i] <= 0) {
                continue;
            }
            Item item;
            item.icon = MaterialIcon(i);
            item.color = {0.82f, 0.8f, 1.0f, 1.0f};
            item.count = bag_.materials[i];
            item.kind = static_cast<int>(ScreenItem::Material);
            item.id = static_cast<int>(i);
            item.title = survivor::MaterialName(static_cast<survivor::MaterialId>(i));
            item.body = "1つ " + std::to_string(config_.loot.materialPrice[i]) + "　全部で " + std::to_string(bag_.materials[i] * config_.loot.materialPrice[i]);
            item.actions = "右クリック：全部売る　Z：全部捨てる";
            view.grid.push_back(item);
        }
    }
}

void SurvivorRunController::RefreshInventoryScreen()
{
    // 機体の遊び方（計画書 p4 の H3）：Tab で開く「今の組み立て」（技の段とパーク）。ショップと同じ左側を使う。
    if (HeroMode()) {
        SurvivorItemScreen::View build = HeroBuildView();
        build.title = "組み立て：" + Hero().name;
        build.help = "Tab で閉じる　／　開いている間は止まる　／　技の強化とパークは波の間のショップで買う";
        build.gridTitle = " ";
        build.buttons.push_back({"閉じる（Tab）", 0, true});
        itemScreen_.Show(build);
        return;
    }
    SurvivorItemScreen::View view = BuildScreenBase(bag_.materials, bag_.money, true);
    view.title = "持ち物";
    view.help = "マウスを合わせると説明が出る　／　E で閉じる　／　開いている間は止まる　／　付け替え・売るのは休憩部屋で";
    view.gridTitle = "出撃中の袋（倒れると全部失う）";
    for (size_t i = 0; i < view.cards.size(); ++i) {
        view.cards[i].name = std::to_string(i + 1) + "　（空き）";
        if (weaponSlots_[i] >= 0) {
            FillWeaponCard(view.cards[i], i, runSlots_[i].weapon, SlotWeapon(i), magazines_[i], runSlots_[i].weapon.chips.size(), "", "");
            view.cards[i].active = i == activeSlot_;
        }
    }
    AddBagToGrid(view, false);
    view.buttons.push_back({"閉じる（E）", 0, true});
    itemScreen_.Show(view);
}

void SurvivorRunController::RefreshRestRoom()
{
    // 機体の遊び方：波の間のショップ（K4）。
    if (HeroMode()) {
        RefreshShop();
        return;
    }
    SurvivorItemScreen::View view = BuildScreenBase(bag_.materials, bag_.money, !HeroMode());
    view.title = "休憩部屋";
    view.help = "マウスを合わせると説明が出る　／　ドラッグで付け替え・チップを付ける　／　右クリックで売る　／　Z で捨てる";
    view.gridTitle = "出撃中の袋";
    for (size_t i = 0; i < view.cards.size(); ++i) {
        view.cards[i].name = std::to_string(i + 1) + "　（空き）";
        if (weaponSlots_[i] >= 0) {
            FillWeaponCard(view.cards[i], i, runSlots_[i].weapon, SlotWeapon(i), magazines_[i], runSlots_[i].baseChips, "右クリック：外して袋へ戻す", "");
            view.cards[i].active = i == activeSlot_;
        }
    }
    AddBagToGrid(view, true);
    view.buttons.push_back({"帰る（拠点へ持ち帰る）", 0, true});
    const int nextStage = stage_ + 2;
    view.buttons.push_back({IsBossStage() ? std::string("あとは帰るだけ")
                                          : "次へ進む：" + (nextStage == survivor::SpawnConfig::kStageCount ? std::string("ボス") : "ステージ " + std::to_string(nextStage)) + "（HP回復）",
                            1, !IsBossStage()});
    itemScreen_.Show(view);
}

void SurvivorRunController::RefreshBaseScreen()
{
    SurvivorItemScreen::View view = BuildScreenBase(base_.materials, base_.money, false);
    view.title = "拠点　─　難しさ 段" + std::to_string(config_.stages.tier);
    view.tabs = {{"武器", 0, true}, {"作る", 1, true}, {"派生の木", 2, true}};
    view.activeTab = basePage_;
    view.buttons.push_back({"出撃（Enter）", 0, true});
    for (size_t i = 0; i < view.cards.size(); ++i) {
        view.cards[i].name = std::to_string(i + 1) + "　（空き）";
        const int index = base_.equipped[i];
        if (index < 0 || static_cast<size_t>(index) >= base_.weapons.size()) {
            continue;
        }
        const survivor::BaseWeapon& owned = base_.weapons[static_cast<size_t>(index)];
        const int definition = survivor::FindWeapon(config_.weapons, owned.key);
        if (definition >= 0) {
            FillWeaponCard(view.cards[i], i, owned, survivor::ApplyRarity(config_.weapons[static_cast<size_t>(definition)], owned.rarity, config_.crafting), -1,
                           owned.chips.size(), "", "");
        }
    }
    const int count = static_cast<int>(base_.weapons.size());
    baseCursor_ = (std::clamp)(baseCursor_, 0, (std::max)(0, count - 1));
    if (basePage_ == 1) {
        view.help = "マウスを合わせると要る物が出る　／　クリックで作る（コモンの武器が拠点に増える）";
        view.gridTitle = "作る";
        view.showEmptyCells = false;
        for (size_t i = 0; i < config_.crafting.recipes.size(); ++i) {
            const survivor::WeaponRecipe& recipe = config_.crafting.recipes[i];
            const int definition = survivor::FindWeapon(config_.weapons, recipe.key);
            if (definition < 0 || !recipe.craftable) {
                continue;
            }
            const bool affordable = survivor::CanAfford(base_.materials, base_.money, recipe.cost);
            Item item;
            item.icon = WeaponIcon(recipe.key);
            item.color = survivor::RarityColor(survivor::Rarity::Common);
            item.dim = !affordable;
            item.kind = static_cast<int>(ScreenItem::Recipe);
            item.id = static_cast<int>(i);
            item.title = config_.weapons[static_cast<size_t>(definition)].name;
            item.body = "要る物：" + survivor::DescribeCost(recipe.cost);
            item.actions = affordable ? "クリック：作る" : "素材かお金が足りない";
            view.grid.push_back(item);
        }
    } else if (basePage_ == 2 && count > 0) {
        const survivor::BaseWeapon& owned = base_.weapons[static_cast<size_t>(baseCursor_)];
        const int definition = survivor::FindWeapon(config_.weapons, owned.key);
        view.help = "上の段：レア度を1段ずつ上げる　／　下の段：別の武器へ分かれる　／　クリックで進化　／　合わせると値と要る物が出る";
        view.gridTitle = "派生の木：" + (definition >= 0 ? config_.weapons[static_cast<size_t>(definition)].name : owned.key);
        view.showEmptyCells = false;
        if (definition >= 0) {
            const float bulletDamage = baseBulletDamage_;
            for (size_t r = 0; r < survivor::kRarityCount; ++r) {
                const auto rarity = static_cast<survivor::Rarity>(r);
                const survivor::WeaponDefinition weapon = survivor::ApplyRarity(config_.weapons[static_cast<size_t>(definition)], rarity, config_.crafting);
                Item item;
                item.icon = WeaponIcon(owned.key);
                item.color = survivor::RarityColor(rarity);
                item.dim = rarity < owned.rarity;
                item.kind = static_cast<int>(ScreenItem::TreeNode);
                item.id = static_cast<int>(r);
                item.title = survivor::RarityName(rarity) + std::string(rarity == owned.rarity ? "　［今］" : "");
                item.body = "威力 " + std::to_string(static_cast<int>((std::max)(1.0f, std::round(bulletDamage * weapon.damageScale)))) +
                            (weapon.magazine > 0 ? "　弾倉 " + std::to_string(weapon.magazine) : std::string()) + "　チップ枠 " + std::to_string(survivor::ChipSlots(weapon, rarity));
                if (rarity > owned.rarity) {
                    const survivor::CraftCost cost = survivor::EvolveCost(config_.crafting, owned.key, rarity);
                    item.body += "\n要る物：" + survivor::DescribeCost(cost);
                    if (static_cast<int>(rarity) == static_cast<int>(owned.rarity) + 1) {
                        item.actions = survivor::CanAfford(base_.materials, base_.money, cost) ? "クリック：この段へ進化" : "素材かお金が足りない";
                    } else {
                        item.actions = "先に、ひとつ前の段へ進化する";
                    }
                }
                view.grid.push_back(item);
            }
            // 2段目：別の武器へ分かれる枝。
            view.grid.resize(SurvivorItemScreen::kColumns);
            for (size_t b = 0; b < config_.crafting.branches.size(); ++b) {
                const survivor::WeaponBranch& branch = config_.crafting.branches[b];
                const int target = survivor::FindWeapon(config_.weapons, branch.to);
                if (branch.from != owned.key || target < 0) {
                    continue;
                }
                const survivor::WeaponDefinition weapon = survivor::ApplyRarity(config_.weapons[static_cast<size_t>(target)], owned.rarity, config_.crafting);
                const bool unlocked = owned.rarity >= branch.minRarity;
                const bool affordable = survivor::CanAfford(base_.materials, base_.money, branch.cost);
                Item item;
                item.icon = WeaponIcon(branch.to);
                item.color = survivor::RarityColor(owned.rarity);
                item.dim = !unlocked || !affordable;
                item.kind = static_cast<int>(ScreenItem::TreeBranch);
                item.id = static_cast<int>(b);
                item.title = "分かれる：" + weapon.name;
                item.body = std::string(survivor::AmmoName(weapon.ammo)) + "　威力 " + std::to_string(static_cast<int>((std::max)(1.0f, std::round(bulletDamage * weapon.damageScale)))) +
                            "　連射 ×" + Fixed(1.0f / (std::max)(0.05f, weapon.reloadScale), 1) + "\n要る物：" + survivor::DescribeCost(branch.cost);
                item.actions = !unlocked  ? std::string(survivor::RarityName(branch.minRarity)) + " 以上で分かれられる"
                               : affordable ? std::string("クリック：この武器へ変わる（レア度とチップはそのまま）")
                                            : std::string("素材かお金が足りない");
                view.grid.push_back(item);
            }
        }
    } else {
        view.help = "ドラッグで武器の枠へ入れる　／　クリックで派生の木を見る　／　マウスを合わせると説明が出る";
        view.gridTitle = "拠点の武器（" + std::to_string(count) + "）";
        for (int i = 0; i < count; ++i) {
            const survivor::BaseWeapon& owned = base_.weapons[static_cast<size_t>(i)];
            const int definition = survivor::FindWeapon(config_.weapons, owned.key);
            Item item;
            item.icon = WeaponIcon(owned.key);
            item.color = definition >= 0 ? survivor::RarityColor(owned.rarity) : myEngine::Vector4{0.5f, 0.56f, 0.64f, 1.0f};
            item.draggable = definition >= 0;
            item.kind = static_cast<int>(ScreenItem::BaseWeapon);
            item.id = i;
            if (definition >= 0) {
                const survivor::WeaponDefinition weapon = survivor::ApplyRarity(config_.weapons[static_cast<size_t>(definition)], owned.rarity, config_.crafting);
                item.title = weapon.name + "　" + survivor::RarityName(owned.rarity);
                item.body = std::string(survivor::AmmoName(weapon.ammo)) + "\n" + DescribeChips(owned, weapon);
                for (size_t slot = 0; slot < base_.equipped.size(); ++slot) {
                    if (base_.equipped[slot] == i) {
                        item.body += "\n枠 " + std::to_string(slot + 1) + " に装備中";
                        item.dim = true;
                    }
                }
                item.actions = "ドラッグ：武器の枠へ入れる\nクリック：派生の木を見る";
            } else {
                item.title = "使えない武器：" + owned.key;
            }
            view.grid.push_back(item);
        }
    }
    itemScreen_.Show(view);
}

void SurvivorRunController::HandleScreenEvent(const SurvivorItemScreen::Event& event)
{
    if (event.type == EventType::None) {
        return;
    }
    audio_.Play(event.type == EventType::Drop ? "ui_drop" : "ui_click");
    const Item* source = itemScreen_.ItemAt(event.source);
    const ScreenItem kind = source ? static_cast<ScreenItem>(source->kind) : ScreenItem::None;
    const int id = source ? source->id : -1;
    // ドロップの行き先の枠（札かチップのマス）。
    const bool toCard = event.type == EventType::Drop && (event.target.zone == Zone::Card || event.target.zone == Zone::Socket);
    const size_t targetSlot = toCard ? static_cast<size_t>(event.target.index) : 0;

    // 機体の遊び方の拠点：札をクリックで機体を選び、ボタンで出撃・解放する（K6）。
    if (baseScreenOpen_ && HeroMode()) {
        if (event.type == EventType::Press && event.source.zone == Zone::Button) {
            if (event.source.index == 0) {
                StartSortie();
            } else if (UnlockHeroCard()) {
                RefreshHeroSelect();
            }
        } else if (event.type == EventType::Click && (event.source.zone == Zone::Card || event.source.zone == Zone::Socket)) {
            SelectHeroCard(event.source.index);
            RefreshHeroSelect();
        }
        return;
    }
    if (baseScreenOpen_) {
        if (event.type == EventType::Press) {
            if (event.source.zone == Zone::Button) {
                StartSortie();
            } else {
                basePage_ = event.source.index;
                RefreshBaseScreen();
            }
            return;
        }
        if (toCard && kind == ScreenItem::BaseWeapon) {
            EquipFromBase(id, targetSlot);
            SaveBase();
            RefreshBaseScreen();
        } else if (event.type == EventType::Click && kind == ScreenItem::BaseWeapon) {
            baseCursor_ = id;
            basePage_ = 2;
            RefreshBaseScreen();
        } else if (event.type == EventType::Click && kind == ScreenItem::Recipe) {
            if (CraftWeapon(static_cast<size_t>(id))) {
                RefreshBaseScreen();
            }
        } else if (event.type == EventType::Click && kind == ScreenItem::TreeNode) {
            const int current = static_cast<int>(base_.weapons[static_cast<size_t>(baseCursor_)].rarity);
            if (id == current + 1 && EvolveBaseWeapon(baseCursor_)) {
                RefreshBaseScreen();
            }
        } else if (event.type == EventType::Click && kind == ScreenItem::TreeBranch) {
            if (BranchBaseWeapon(baseCursor_, static_cast<size_t>(id))) {
                RefreshBaseScreen();
            }
        }
        return;
    }
    if (restRoomOpen_) {
        if (event.type == EventType::Press && event.source.zone == Zone::Button) {
            if (event.source.index == 0) {
                LeaveRestRoom();
            } else if (event.source.index == 2) {
                if (RepairHero()) {
                    RefreshRestRoom();
                }
            } else if (event.source.index == 3) {
                if (RerollShop()) {
                    RefreshRestRoom();
                }
            } else {
                StartNextStage();
            }
            return;
        }
        // 機体の遊び方のショップ：並んでいる物をクリックで買う。
        if (HeroMode()) {
            if (event.type == EventType::Click && kind == ScreenItem::ShopOffer && BuyOffer(static_cast<size_t>(id))) {
                RefreshRestRoom();
            }
            return;
        }
        bool changed = false;
        if (toCard && kind == ScreenItem::BagWeapon) {
            changed = RestEquip(id, targetSlot);
        } else if (toCard && kind == ScreenItem::BagChip) {
            changed = RestAttach(id, targetSlot);
        } else if (event.type == EventType::RightClick && kind == ScreenItem::SlotChip) {
            changed = RestDetach(static_cast<size_t>(event.source.index), static_cast<size_t>(id));
        } else if (event.type == EventType::RightClick) {
            const int price = RestSell(kind, id);
            if (price > 0) {
                world_.combatFlow->SetEventCallout("売った：+" + std::to_string(price), 0.8f);
                changed = true;
            }
        }
        if (changed) {
            RefreshRestRoom();
        }
        return;
    }
    if (inventoryOpen_ && event.type == EventType::Press && event.source.zone == Zone::Button) {
        CloseInventory();
    }
}

void SurvivorRunController::UpdateScreenInput()
{
    // 自動確認は DriveScreenPointer でマウスの代わりをする。
    if (autoTest_ != AutoTest::None) {
        return;
    }
    auto& input = *world_.resources.input_;
    if (baseScreenOpen_ && input.IsKeyTriggered(DIK_RETURN)) {
        StartSortie();
        return;
    }
    const bool right = input.IsTrigger(input.GetMouseState().rgbButtons[1], input.GetPreMouseState().rgbButtons[1]);
    DriveScreenPointer(input.GetMousePosition(), input.IsPress(input.GetMouseState().rgbButtons[0]), right, static_cast<int>(input.GetMouseState().lZ));
    if (restRoomOpen_ && input.IsKeyTriggered(DIK_Z)) {
        DiscardHoveredItem();
    }
}

void SurvivorRunController::DriveScreenPointer(const myEngine::Vector2& mouse, bool leftDown, bool rightTrigger, int wheel)
{
    HandleScreenEvent(itemScreen_.UpdatePointer(mouse, leftDown, rightTrigger, wheel));
}

void SurvivorRunController::DiscardHoveredItem()
{
    const Item* item = itemScreen_.ItemAt(itemScreen_.GetHover());
    if (item && RestDiscard(static_cast<ScreenItem>(item->kind), item->id)) {
        RefreshRestRoom();
    }
}

void SurvivorRunController::OpenInventory()
{
    if (inventoryOpen_ || !WeaponMode()) {
        return;
    }
    SyncAmmoFromPlayer();
    RefreshInventoryScreen();
    inventoryOpen_ = true;
    ++inventoryOpens_;
}

void SurvivorRunController::CloseInventory()
{
    inventoryOpen_ = false;
}

SurvivorItemScreen::View SurvivorRunController::HeroBuildView() const
{
    SurvivorItemScreen::View view = BuildScreenBase(bag_.materials, bag_.money, false);
    // 右の上の列は、レベルポイントだけ（素材は使わない）。
    view.header.resize(1);
    view.header[0].title = "レベルポイント";
    view.header[0].body = "敵を倒すと入る。ショップで技の強化・パーク・修理・引き直しに使う。";
    view.showEmptyCells = false;
    // 左の3枚：今の組み立て（技の段とパーク）。
    static constexpr std::array<const char*, 3> kCardTitles{"主武器・右クリック", "Shift・E", "Q・パーク"};
    for (size_t c = 0; c < view.cards.size(); ++c) {
        auto& card = view.cards[c];
        card.empty = false;
        card.name = kCardTitles[c];
        card.item.icon = c == 0 ? WeaponIcon(Hero().primary) : Icon(c == 1 ? "chip_thruster" : "chip_morale");
        card.item.color = {0.7f, 0.95f, 1.0f, 1.0f};
        card.item.title = Hero().name;
        auto line = [&](int target) {
            const survivor::AbilityDefinition& ability = UpgradeAbility(target);
            const std::string name = target == 0 && ability.kind == survivor::AbilityKind::None ? std::string("主武器") : ability.name;
            return name + "　段" + std::to_string(UpgradeTier(target));
        };
        if (c == 0) {
            card.stats = line(0) + "\n" + line(1);
        } else if (c == 1) {
            card.stats = line(2) + "\n" + line(3);
        } else {
            // パークは3つごとに改行する（札の右端からはみ出さないように）。
            std::string perks;
            int owned = 0;
            for (size_t i = 0; i < survivor::kPerkCount; ++i) {
                if (perks_[i]) {
                    perks += (owned == 0 ? "" : owned % 3 == 0 ? "\n" : "・") + std::string(survivor::GetPerkInfo(static_cast<survivor::PerkId>(i)).name);
                    ++owned;
                }
            }
            card.stats = line(4) + "\n" + (perks.empty() ? std::string("パークなし") : perks);
        }
        card.item.body = card.stats;
    }
    // 左下の2つ：HP と傷、Q のゲージ（武器の遊び方のバリアと補助の枠を使う）。
    const auto& player = *world_.resources.player_;
    view.shield.icon = Icon("chip_armor");
    view.shield.color = player.GetSurvivorWound() > 0 ? myEngine::Vector4{1.0f, 0.75f, 0.6f, 1.0f} : myEngine::Vector4{0.6f, 1.0f, 0.7f, 1.0f};
    view.shield.title = "HP " + std::to_string(player.GetHp()) + "/" + std::to_string(player.GetMaxHp()) + "　傷 " + std::to_string(player.GetSurvivorWound());
    view.shield.body = "傷は治すまで HP がそこまでしか戻らない。波の間に HP は戻らず、修理で傷を全部と HP を治す。";
    view.support.icon = Icon("chip_morale");
    view.support.color = {1.0f, 0.85f, 0.35f, 1.0f};
    view.support.dim = false;
    view.support.title = "Q " + std::to_string(static_cast<int>((std::min)(1.0f, ultimateCharge_ / Hero().ultimateCharge) * 100.0f)) + "%";
    view.support.body = "Q のゲージは、与えたダメージと回復した量で溜まる。満タンで " + Hero().abilities[3].name + " を使える。";
    return view;
}

void SurvivorRunController::RefreshShop()
{
    const auto& player = *world_.resources.player_;
    SurvivorItemScreen::View view = HeroBuildView();
    view.title = "波 " + std::to_string(stage_ + 1) + "/" + std::to_string(config_.waves.count) + " おわり　─　ショップ";
    view.help = "HP " + std::to_string(player.GetHp()) + "/" + std::to_string(player.GetMaxHp()) + "（傷 " +
                std::to_string(player.GetSurvivorWound()) + "）　／　クリックで買う　／　HP は波の間に戻らない。修理で傷を全部と HP を治す";
    view.gridTitle = "並んでいる物";
    static constexpr std::array<const char*, survivor::kUpgradeTargets> kUpgradeIcons{"chip_warhead", "chip_heavyWarhead", "chip_thruster",
                                                                                      "chip_collector", "chip_chainLightning"};
    for (size_t i = 0; i < shopOffers_.size(); ++i) {
        const ShopOffer& offer = shopOffers_[i];
        const int price = OfferPrice(offer);
        Item item;
        item.icon = Icon(offer.upgrade ? kUpgradeIcons[static_cast<size_t>(offer.index)] : "chip_default");
        item.color = offer.upgrade ? myEngine::Vector4{0.55f, 0.95f, 1.0f, 1.0f} : myEngine::Vector4{1.0f, 0.75f, 0.35f, 1.0f};
        item.count = price;
        item.dim = bag_.money < price;
        item.kind = static_cast<int>(ScreenItem::ShopOffer);
        item.id = static_cast<int>(i);
        item.title = OfferName(offer) + "　" + std::to_string(price);
        item.body = OfferText(offer);
        item.actions = bag_.money >= price ? "クリック：買う" : "レベルポイントが足りない";
        view.grid.push_back(item);
    }
    const bool hurt = player.GetSurvivorWound() > 0 || player.GetHp() < player.GetMaxHp();
    view.buttons.push_back({"出撃を終える（結果へ）", 0, true});
    view.buttons.push_back({"次へ：" + (stage_ + 2 == StageCount() ? std::string("ボスの波") : "波 " + std::to_string(stage_ + 2)), 1, true});
    view.buttons.push_back({"修理 " + std::to_string(config_.health.repairCost) + "：傷を治し HP+" + std::to_string(config_.health.repairHeal), 2,
                            hurt && bag_.money >= config_.health.repairCost});
    view.buttons.push_back({"引き直す " + std::to_string(RerollPrice()), 3, bag_.money >= RerollPrice()});
    itemScreen_.Show(view);
}

void SurvivorRunController::RefreshHeroSelect()
{
    // 機体の遊び方の拠点（計画書 p4 の K6）：3枚の札に機体を並べ、クリックで選ぶ。まだの機体は解放ポイントで解放する。
    SurvivorItemScreen::View view = BuildScreenBase(bag_.materials, heroSave_.points, false);
    view.header.resize(1);
    view.header[0].title = "解放ポイント";
    view.header[0].body = "出撃のスコアから入る（倒れても入る）。機体の解放に使う。";
    view.title = "拠点　─　機体を選ぶ";
    view.help = "札をクリックで選ぶ　／　Enter で出撃　／　強化は残らない（毎回ゼロから）。残るのは機体の解放だけ";
    view.gridTitle = NextHeroText();
    view.showEmptyCells = false;
    // 左下：これまでの記録（出撃とクリアの回数、ボスを倒した機体の数）。
    view.shield = {};
    view.shield.icon = Icon("chip_morale");
    view.shield.color = {0.7f, 0.95f, 1.0f, 1.0f};
    view.shield.title = "出撃 " + std::to_string(heroSave_.sorties) + "　クリア " + std::to_string(heroSave_.clears);
    view.shield.body = "ボスを倒した機体：" + std::to_string(heroSave_.cleared.size()) + "（初めて倒すと解放ポイントのボーナス）";
    view.support = {};
    const std::vector<int> order = HeroCardOrder();
    const std::vector<int> candidates = survivor::HeroUnlockCandidates(heroSave_, config_.heroes.heroes);
    for (size_t c = 0; c < view.cards.size(); ++c) {
        auto& card = view.cards[c];
        if (c >= order.size()) {
            card.name = " ";
            continue;
        }
        const int index = order[c];
        const survivor::HeroDefinition& hero = config_.heroes.heroes[static_cast<size_t>(index)];
        const bool unlocked = survivor::IsHeroUnlocked(heroSave_, hero);
        const bool candidate = std::find(candidates.begin(), candidates.end(), index) != candidates.end();
        card.empty = false;
        card.active = static_cast<int>(c) == heroCard_;
        card.name = hero.name + (index == hero_ && unlocked ? "　（出撃する）" : "");
        std::string skills;
        for (size_t i = 0; i < survivor::kAbilitySlotCount; ++i) {
            if (hero.abilities[i].kind != survivor::AbilityKind::None) {
                skills += std::string(skills.empty() ? "" : "・") + hero.abilities[i].name;
            }
        }
        card.stats = (unlocked    ? std::string("使える")
                      : candidate ? "解放 " + std::to_string(hero.unlockCost) + " ポイント"
                                  : std::string("まだ解放できない")) +
                     "　　HP " + std::to_string(hero.maxHp) + "\n" + skills;
        card.item.icon = WeaponIcon(hero.primary);
        card.item.color = unlocked ? myEngine::Vector4{0.7f, 0.95f, 1.0f, 1.0f} : myEngine::Vector4{0.45f, 0.5f, 0.58f, 1.0f};
        card.item.dim = !unlocked;
        card.item.title = hero.name;
        card.item.body = card.stats;
        card.item.actions = unlocked ? "クリック：この機体で出撃する" : candidate ? "クリックして「解放する」" : "";
    }
    // 選んでいる札が、解放できる機体なら「解放する」を押せる。
    std::string unlockText = "解放する";
    bool unlockEnabled = false;
    if (heroCard_ >= 0 && static_cast<size_t>(heroCard_) < order.size()) {
        const int index = order[static_cast<size_t>(heroCard_)];
        const survivor::HeroDefinition& hero = config_.heroes.heroes[static_cast<size_t>(index)];
        if (!survivor::IsHeroUnlocked(heroSave_, hero)) {
            unlockText = "解放する：" + hero.name + " " + std::to_string(hero.unlockCost);
            unlockEnabled = std::find(candidates.begin(), candidates.end(), index) != candidates.end() && heroSave_.points >= hero.unlockCost;
        }
    }
    view.buttons.push_back({"出撃：" + Hero().name + "（Enter）", 0, true});
    view.buttons.push_back({unlockText, 1, unlockEnabled});
    itemScreen_.Show(view);
}

} // namespace gameplay
