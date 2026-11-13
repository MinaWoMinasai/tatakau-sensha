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
    // 機体の遊び方は弾を数えないので、弾の数は出さない。
    SurvivorItemScreen::View view = BuildScreenBase(bag_.materials, bag_.money, !HeroMode());
    view.title = "持ち物";
    view.help = "マウスを合わせると説明が出る　／　E で閉じる　／　開いている間は止まる　／　付け替え・売るのは休憩部屋で";
    view.gridTitle = "出撃中の袋（倒れると全部失う）";
    // 機体の遊び方（計画書 p4 の H3）：Tab で開く「今の組み立て」。技の段とパークは K4 で足す。
    if (HeroMode()) {
        view.title = "組み立て：" + Hero().name;
        view.help = "Tab で閉じる　／　開いている間は止まる";
        std::string skills;
        for (size_t i = 0; i < survivor::kAbilitySlotCount; ++i) {
            const survivor::AbilityDefinition& ability = Hero().abilities[i];
            if (ability.kind != survivor::AbilityKind::None) {
                skills += std::string(skills.empty() ? "" : "　／　") + survivor::AbilitySlotButton(static_cast<survivor::AbilitySlot>(i)) + " " + ability.name;
            }
        }
        view.gridTitle = "技：" + skills;
    }
    for (size_t i = 0; i < view.cards.size(); ++i) {
        // 機体の遊び方は主武器の枠だけ（空きの枠は名前も出さない）。
        view.cards[i].name = HeroMode() ? std::string(" ") : std::to_string(i + 1) + "　（空き）";
        if (weaponSlots_[i] >= 0) {
            FillWeaponCard(view.cards[i], i, runSlots_[i].weapon, SlotWeapon(i), HeroMode() ? -1 : magazines_[i], runSlots_[i].weapon.chips.size(), "", "");
            view.cards[i].active = i == activeSlot_;
        }
    }
    AddBagToGrid(view, false);
    view.buttons.push_back({HeroMode() ? "閉じる（Tab）" : "閉じる（E）", 0, true});
    itemScreen_.Show(view);
}

void SurvivorRunController::RefreshRestRoom()
{
    // 機体の遊び方：波の間の画面（K4 でショップにする）。今は修理と次の波へだけ。
    if (HeroMode()) {
        const auto& player = *world_.resources.player_;
        SurvivorItemScreen::View view = BuildScreenBase(bag_.materials, bag_.money, false);
        view.title = "波 " + std::to_string(stage_ + 1) + "/" + std::to_string(config_.waves.count) + " おわり";
        view.help = "HP " + std::to_string(player.GetHp()) + "/" + std::to_string(player.GetMaxHp()) + "（傷 " +
                    std::to_string(player.GetSurvivorWound()) + "）　／　レベルポイント " + std::to_string(bag_.money) +
                    "　／　HP は波の間に戻らない。修理で傷を全部と HP を治す";
        view.gridTitle = "";
        view.showEmptyCells = false;
        const bool hurt = player.GetSurvivorWound() > 0 || player.GetHp() < player.GetMaxHp();
        view.buttons.push_back({"出撃を終える（結果へ）", 0, true});
        view.buttons.push_back({"次へ：" + (stage_ + 2 == StageCount() ? std::string("ボスの波") : "波 " + std::to_string(stage_ + 2)), 1, true});
        view.buttons.push_back({"修理 " + std::to_string(config_.health.repairCost) + "：傷を治し HP+" + std::to_string(config_.health.repairHeal), 2,
                                hurt && bag_.money >= config_.health.repairCost});
        itemScreen_.Show(view);
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
            } else {
                StartNextStage();
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

} // namespace gameplay
