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
    // 地雷は何も落とさない。
    if (enemy.GetSurvivorRole() == ExpEnemy::SurvivorRole::Mine) {
        return;
    }
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
    const std::vector<survivor::LootItem> collected = lootField_.Update(dt, player, accept);
    if (!collected.empty()) {
        // エネルギー弾は自機が予備から直接撃っているので、足す前に戻し、足した後に渡し直す。
        SyncAmmoFromPlayer();
        for (const survivor::LootItem& item : collected) {
            AddToBag(item);
        }
        LoadAmmoToPlayer();
    }

    // 袋の一行。文字が変わったときだけ作り直す。
    // 機体の遊び方ではお金が欠片ごとに細かく増えるので、0.5秒に1回まで（文字が変わるたびに文字の画像が増えて解放されないため）。
    lootTextWait_ = (std::max)(0.0f, lootTextWait_ - dt);
    const std::string bag = DescribeBag();
    if (lootText_ && bag != lootTextShown_ && (!HeroMode() || lootTextWait_ <= 0.0f)) {
        lootText_->SetText(bag);
        lootTextShown_ = bag;
        lootTextWait_ = 0.5f;
    }
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
    case survivor::LootKind::Ammo: {
        const size_t type = static_cast<size_t>((std::clamp)(item.id, 0, static_cast<int>(survivor::kAmmoTypeCount) - 1));
        reserves_[type] = (std::min)(config_.loot.reserveMax[type], reserves_[type] + item.amount);
        ammoPicked_[type] += item.amount;
        break;
    }
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
