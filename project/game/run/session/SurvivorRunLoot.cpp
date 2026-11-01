#include "game/run/session/SurvivorRunController.h"
#include "game/session/GameplaySystems.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

// 育つ戦車の試作の、敵が落とす物と出撃中の袋（計画書 p3 の W3）。

namespace gameplay {

void SurvivorRunController::DropLoot(const ExpEnemy& enemy)
{
    const auto& loot = config_.loot;
    const ExpEnemyType type = enemy.GetType();
    // 敵戦車（遠征の敵）と、大五角・重い四角のような大きな敵は、確率が bigScale 倍。
    const bool tank = type >= ExpEnemyType::Shooter;
    const float scale = tank || enemy.GetSizeScale() > 1.5f ? loot.bigScale : 1.0f;
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    auto roll = [&](float chance) {
        return unit(random_) < (std::min)(1.0f, chance * scale);
    };
    const myEngine::Vector3 position = enemy.GetWorldPosition();
    auto drop = [&](survivor::LootKind kind, int id, int amount, survivor::Rarity rarity = survivor::Rarity::Common) {
        survivor::LootItem item;
        item.kind = kind;
        item.id = id;
        item.amount = amount;
        item.rarity = rarity;
        lootField_.Drop(position, item, random_);
        ++lootDropped_[static_cast<size_t>(kind)];
    };
    // 素材：倒した形の素材。敵戦車は四角・三角・五角のどれかと、ときどきボスの素材。
    if (roll(loot.materialChance)) {
        const int material = tank ? std::uniform_int_distribution<int>(0, 2)(random_)
                                  : (std::clamp)(static_cast<int>(type), 0, 2);
        drop(survivor::LootKind::Material, material, 1);
    }
    if (tank && unit(random_) < loot.bossMaterialChance) {
        drop(survivor::LootKind::Material, static_cast<int>(survivor::MaterialId::Boss), 1);
    }
    for (size_t i = 0; i < survivor::kAmmoTypeCount; ++i) {
        if (roll(loot.ammoChance[i])) {
            drop(survivor::LootKind::Ammo, static_cast<int>(i), loot.ammoAmount[i]);
        }
    }
    // チップ：今の部品（Q の部品と合体の部品は除く）から1つ。
    if (roll(loot.chipChance)) {
        std::vector<int> chips;
        for (size_t i = 0; i < config_.parts.size(); ++i) {
            if (survivor::IsChipPart(config_.parts[i])) {
                chips.push_back(static_cast<int>(i));
            }
        }
        if (!chips.empty()) {
            const int part = chips[std::uniform_int_distribution<size_t>(0, chips.size() - 1)(random_)];
            drop(survivor::LootKind::Chip, part, 1, survivor::RollRarity(loot.rarityWeights, random_));
        }
    }
    if (roll(loot.weaponChance) && !config_.weapons.empty()) {
        const int weapon = static_cast<int>(std::uniform_int_distribution<size_t>(0, config_.weapons.size() - 1)(random_));
        drop(survivor::LootKind::Weapon, weapon, 1, survivor::RollRarity(loot.rarityWeights, random_));
    }
}

void SurvivorRunController::UpdateLoot(float dt)
{
    const auto& loot = config_.loot;
    std::array<bool, survivor::kAmmoTypeCount> accept{};
    for (size_t i = 0; i < accept.size(); ++i) {
        accept[i] = reserves_[i] < loot.reserveMax[i];
    }
    const myEngine::Vector3 player = world_.resources.player_->GetWorldPosition();
    const auto collected = lootField_.Update(dt, player, accept);
    if (std::any_of(collected.begin(), collected.end(), [](int amount) { return amount > 0; })) {
        // エネルギー弾は自機が予備から直接撃っているので、足す前に戻し、足した後に渡し直す。
        SyncAmmoFromPlayer();
        for (size_t i = 0; i < collected.size(); ++i) {
            if (collected[i] <= 0) {
                continue;
            }
            reserves_[i] = (std::min)(loot.reserveMax[i], reserves_[i] + collected[i]);
            ammoPicked_[i] += collected[i];
            lootPicked_[static_cast<size_t>(survivor::LootKind::Ammo)] += (std::max)(1, collected[i] / loot.ammoAmount[i]);
        }
        LoadAmmoToPlayer();
    }

    // 袋の一行と、近くにキーで拾える物があるときの案内。文字が変わったときだけ作り直す。
    const std::string bag = DescribeBag();
    if (lootText_ && bag != lootTextShown_) {
        lootText_->SetText(bag);
        lootTextShown_ = bag;
    }
    const survivor::LootItem* nearest = nullptr;
    const size_t nearCount = lootField_.CountNear(player, &nearest);
    std::string prompt;
    if (nearCount > 0 && nearest) {
        prompt = "F：拾う　" + LootName(*nearest);
        if (nearCount > 1) {
            prompt += "　ほか " + std::to_string(nearCount - 1);
        }
    }
    if (lootPrompt_ && prompt != lootPromptShown_) {
        lootPrompt_->SetText(prompt.empty() ? std::string(" ") : prompt);
        lootPromptShown_ = prompt;
    }
}

int SurvivorRunController::PickUpLoot()
{
    if (!WeaponMode() || !world_.resources.player_) {
        return 0;
    }
    const std::vector<survivor::LootItem> items = lootField_.TakeNear(world_.resources.player_->GetWorldPosition());
    for (const survivor::LootItem& item : items) {
        AddToBag(item);
    }
    return static_cast<int>(items.size());
}

void SurvivorRunController::AddToBag(const survivor::LootItem& item)
{
    ++lootPicked_[static_cast<size_t>(item.kind)];
    switch (item.kind) {
    case survivor::LootKind::Material:
        bag_.materials[static_cast<size_t>((std::clamp)(item.id, 0, static_cast<int>(survivor::kMaterialCount) - 1))] += item.amount;
        break;
    case survivor::LootKind::Chip:
        bag_.chips.push_back({static_cast<survivor::PartId>(item.id), item.rarity});
        world_.combatFlow->SetEventCallout("拾った：" + LootName(item), 1.2f);
        break;
    case survivor::LootKind::Weapon:
        bag_.weapons.push_back({item.id, item.rarity});
        world_.combatFlow->SetEventCallout("拾った：" + LootName(item), 1.6f);
        break;
    case survivor::LootKind::Ammo:
        break;
    }
}

std::string SurvivorRunController::DescribeChips(const survivor::BaseWeapon& owned, const survivor::WeaponDefinition& weapon) const
{
    const int slots = survivor::ChipSlots(weapon, owned.rarity);
    std::string text = "チップ " + std::to_string(owned.chips.size()) + "/" + std::to_string(slots);
    for (size_t i = 0; i < owned.chips.size(); ++i) {
        text += (i ? "・" : "：") + std::string(config_.parts[static_cast<size_t>(owned.chips[i].part)].name) + "（" +
                survivor::RarityName(owned.chips[i].rarity) + "）";
    }
    return text;
}

std::string SurvivorRunController::DescribeBag() const
{
    std::ostringstream text;
    text << "お金 " << bag_.money << "　　素材 四角 " << bag_.materials[0] << "・三角 " << bag_.materials[1] << "・五角 " << bag_.materials[2]
         << "・ボス " << bag_.materials[3] << "　　チップ " << bag_.chips.size() << "　　武器 " << bag_.weapons.size();
    return text.str();
}

std::string SurvivorRunController::LootName(const survivor::LootItem& item) const
{
    switch (item.kind) {
    case survivor::LootKind::Ammo:
        return std::string(survivor::AmmoName(static_cast<survivor::AmmoType>(item.id))) + " ×" + std::to_string(item.amount);
    case survivor::LootKind::Material:
        return survivor::MaterialName(static_cast<survivor::MaterialId>(item.id));
    case survivor::LootKind::Chip:
        return std::string("チップ「") + config_.parts[static_cast<size_t>(item.id)].name + "」（" + survivor::RarityName(item.rarity) + "）";
    case survivor::LootKind::Weapon:
        return config_.weapons[static_cast<size_t>(item.id)].name + "（" + survivor::RarityName(item.rarity) + "）";
    }
    return {};
}

} // namespace gameplay

namespace gameplay {

void SurvivorRunController::OpenInventory()
{
    if (inventoryOpen_ || !WeaponMode()) {
        return;
    }
    SyncAmmoFromPlayer();
    inventoryHud_.Show(BuildInventoryView());
    inventoryOpen_ = true;
    ++inventoryOpens_;
}

void SurvivorRunController::CloseInventory()
{
    inventoryOpen_ = false;
}

SurvivorInventoryHud::View SurvivorRunController::BuildInventoryView() const
{
    SurvivorInventoryHud::View view;
    const auto& player = *world_.resources.player_;
    auto fixed = [](float value, int digits) {
        std::ostringstream text;
        text << std::fixed << std::setprecision(digits) << value;
        return text.str();
    };
    for (size_t i = 0; i < view.weapons.size() && i < weaponSlots_.size(); ++i) {
        auto& card = view.weapons[i];
        card.title = std::to_string(i + 1) + "　武器";
        if (weaponSlots_[i] < 0) {
            continue;
        }
        const survivor::WeaponDefinition& weapon = SlotWeapon(i);
        card.empty = false;
        card.active = i == activeSlot_;
        card.color = survivor::RarityColor(runSlots_[i].weapon.rarity);
        card.title = std::to_string(i + 1) + "　" + weapon.name + "　" + survivor::RarityName(runSlots_[i].weapon.rarity) + (card.active ? "　（構えている）" : "");
        const int reserve = reserves_[static_cast<size_t>(weapon.ammo)];
        const int damage = static_cast<int>((std::max)(1.0f, std::round(player.GetStats().bulletDamage * weapon.damageScale)));
        std::ostringstream detail;
        detail << survivor::AmmoName(weapon.ammo) << "　";
        if (weapon.magazine > 0) {
            detail << "弾倉 " << magazines_[i] << " / " << weapon.magazine << "　予備 " << reserve;
        } else {
            detail << "弾倉なし（予備 " << reserve << " から直接撃つ）";
        }
        detail << "\n威力 " << damage << "　連射 ×" << fixed(1.0f / (std::max)(0.05f, weapon.reloadScale), 1) << "　弾速 ×"
               << fixed(weapon.speedScale, 1) << "　熱 ×" << fixed(weapon.heatScale, 1);
        if (weapon.blastRadius > 0.0f) {
            detail << "　当たると爆発（半径 " << fixed(weapon.blastRadius, 1) << "）";
        }
        detail << "\n切り替え " << fixed(weapon.swapSeconds, 1) << " 秒";
        if (weapon.magazine > 0) {
            detail << "　リロード " << fixed(weapon.reloadSeconds, 1) << " 秒";
        }
        detail << "　" << DescribeChips(runSlots_[i].weapon, weapon);
        card.detail = detail.str();
    }
    // シールドの枠：今は区画の始めから持っている全周のシールド（計画書 p3 の0章）。
    view.shield.empty = false;
    view.shield.color = {0.35f, 0.85f, 1.0f, 1.0f};
    view.shield.title = "シールド　全周のシールド";
    view.shield.detail = "上限 " + std::to_string(static_cast<int>(player.GetShieldMax())) + "（最大HPの " +
                         std::to_string(static_cast<int>(std::lround(config_.shieldMaxHpRatio * 100.0f))) + "%）　今 " +
                         std::to_string(static_cast<int>(player.GetShield())) + "\n被弾しないと " + fixed(config_.shieldRegenDelay, 0) +
                         " 秒で回復が始まる";
    // 補助の枠（G9：レリックと回復キット）は、休憩部屋で入れられるようになるまで空き。
    view.support.empty = true;
    view.support.title = "補助";
    view.support.color = {0.86f, 0.62f, 1.0f, 1.0f};

    // 袋：お金・素材・予備の弾・チップ・武器。チップと武器は同じ物をまとめ、レア度の高い順に並べる。
    using Line = SurvivorInventoryHud::Line;
    const myEngine::Vector4 plain{0.85f, 0.92f, 1.0f, 1.0f};
    const myEngine::Vector4 header{1.0f, 0.86f, 0.36f, 1.0f};
    view.bag.push_back({"お金　" + std::to_string(bag_.money), plain});
    std::ostringstream materials;
    static constexpr std::array<const char*, survivor::kMaterialCount> kShort{"四角", "三角", "五角", "ボス"};
    for (size_t i = 0; i < survivor::kMaterialCount; ++i) {
        materials << (i ? "・" : "素材　") << kShort[i] << " " << bag_.materials[i];
    }
    view.bag.push_back({materials.str(), plain});
    std::ostringstream ammo;
    static constexpr std::array<const char*, survivor::kAmmoTypeCount> kAmmoShort{"ライト", "ヘビー", "エネルギー"};
    for (size_t i = 0; i < survivor::kAmmoTypeCount; ++i) {
        ammo << (i ? "・" : "予備の弾　") << kAmmoShort[i] << " " << reserves_[i] << "/" << config_.loot.reserveMax[i];
    }
    view.bag.push_back({ammo.str(), plain});
    struct Group {
        std::string name;
        survivor::Rarity rarity;
        int count;
    };
    auto grouped = [](std::vector<Group> groups) {
        std::stable_sort(groups.begin(), groups.end(), [](const Group& a, const Group& b) {
            return a.rarity > b.rarity;
        });
        return groups;
    };
    auto add = [](std::vector<Group>& groups, const std::string& name, survivor::Rarity rarity) {
        for (Group& group : groups) {
            if (group.name == name && group.rarity == rarity) {
                ++group.count;
                return;
            }
        }
        groups.push_back({name, rarity, 1});
    };
    std::vector<Group> weapons, chips;
    for (const auto& weapon : bag_.weapons) {
        add(weapons, config_.weapons[static_cast<size_t>(weapon.weapon)].name, weapon.rarity);
    }
    for (const auto& chip : bag_.chips) {
        add(chips, config_.parts[static_cast<size_t>(chip.part)].name, chip.rarity);
    }
    // 見出しと中身を、欄に収まる行数まで並べる（越える分は「ほか N 種類」）。
    auto list = [&](const std::string& title, size_t total, const std::vector<Group>& groups, size_t room) {
        view.bag.push_back({title + "（" + std::to_string(total) + "）", header});
        if (groups.empty()) {
            view.bag.push_back({"　なし", {0.5f, 0.56f, 0.64f, 1.0f}});
            return;
        }
        for (size_t i = 0; i < groups.size(); ++i) {
            if (i + 1 >= room && groups.size() > room) {
                view.bag.push_back({"　ほか " + std::to_string(groups.size() - i) + " 種類", plain});
                return;
            }
            const myEngine::Vector4 color = survivor::RarityColor(groups[i].rarity);
            view.bag.push_back({"　" + groups[i].name + "（" + survivor::RarityName(groups[i].rarity) + "）" +
                                    (groups[i].count > 1 ? " ×" + std::to_string(groups[i].count) : std::string()),
                                color});
        }
    };
    const size_t room = SurvivorInventoryHud::kBagLines - view.bag.size() - 2;
    const std::vector<Group> weaponGroups = grouped(weapons);
    const std::vector<Group> chipGroups = grouped(chips);
    const size_t weaponRoom = (std::max)(static_cast<size_t>(1), (std::min)(weaponGroups.size(), room / 3));
    list("武器", bag_.weapons.size(), weaponGroups, weaponGroups.empty() ? 1 : weaponRoom);
    list("チップ", bag_.chips.size(), chipGroups, SurvivorInventoryHud::kBagLines - view.bag.size() - 1);
    return view;
}

} // namespace gameplay
