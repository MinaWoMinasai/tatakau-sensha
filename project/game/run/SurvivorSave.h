#pragma once
#include "game/run/SurvivorLoot.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace survivor {
/// @brief 拠点にある武器1つ（武器の設定での名前と、レア度）。
struct BaseWeapon {
    std::string key;
    Rarity rarity = Rarity::Common;
    /// @brief 付けているチップ（休憩部屋で付ける。数は ChipSlots まで）。
    std::vector<LootBag::Chip> chips;

    bool operator==(const BaseWeapon&) const = default;
};

/// @brief 拠点の持ち物（計画書 p3 の W5・G5）。拠点に持ち帰った物は残り、出撃中に拾った物は倒れると失う（G4）。
/// @note equipped は出撃に持っていく武器3つ（weapons の番号、-1 は空き）。シールドは今は全周のシールドだけ、補助は空き（後のカード）。
struct BaseInventory {
    int money = 0;
    std::array<int, kMaterialCount> materials{};
    std::vector<LootBag::Chip> chips;
    std::vector<BaseWeapon> weapons;
    std::array<int, kWeaponSlots> equipped{-1, -1, -1};
    /// @brief 出撃した回数と、区画をクリアして持ち帰った回数。
    int sorties = 0;
    int clears = 0;

    bool operator==(const BaseInventory& other) const
    {
        return money == other.money && materials == other.materials && weapons == other.weapons && equipped == other.equipped &&
               sorties == other.sorties && clears == other.clears && chips == other.chips;
    }
};

/// @brief 初めて遊ぶときの拠点：武器の設定にあるコモンの武器を1つずつ持ち、start の3つを装備する。
inline BaseInventory NewBaseInventory(const std::vector<WeaponDefinition>& definitions, const std::array<std::string, kWeaponSlots>& start)
{
    BaseInventory base;
    for (const WeaponDefinition& weapon : definitions) {
        // 始めから持つ武器と、設定の start に書いた武器。
        if (weapon.starter || std::find(start.begin(), start.end(), weapon.key) != start.end()) {
            base.weapons.push_back({weapon.key, Rarity::Common, {}});
        }
    }
    for (size_t slot = 0; slot < kWeaponSlots; ++slot) {
        for (size_t i = 0; i < base.weapons.size(); ++i) {
            if (base.weapons[i].key == start[slot]) {
                base.equipped[slot] = static_cast<int>(i);
                break;
            }
        }
    }
    return base;
}

/// @brief 出撃中の袋の中身を、拠点へ足す（区画をクリアしたとき）。
inline void AddBagToBase(BaseInventory& base, const LootBag& bag, const std::vector<WeaponDefinition>& definitions)
{
    base.money += bag.money;
    for (size_t i = 0; i < kMaterialCount; ++i) {
        base.materials[i] += bag.materials[i];
    }
    base.chips.insert(base.chips.end(), bag.chips.begin(), bag.chips.end());
    for (const LootBag::Weapon& weapon : bag.weapons) {
        if (weapon.weapon >= 0 && static_cast<size_t>(weapon.weapon) < definitions.size()) {
            base.weapons.push_back({definitions[static_cast<size_t>(weapon.weapon)].key, weapon.rarity, {}});
        }
    }
}

/// @brief 拠点を path から読む。ファイルが無いときは false（error は空）。壊れていれば false と理由。
/// @note 知らない武器・部品の名前は読み飛ばす（設定から消した物）。装備の番号が範囲外なら空きにする。
inline bool LoadBaseInventory(const std::string& path, const std::array<PartDefinition, kPartCount>& parts, BaseInventory& out,
                              std::string& error)
{
    error.clear();
    std::ifstream input{std::filesystem::path(path)};
    if (!input) {
        return false;
    }
    const nlohmann::json data = nlohmann::json::parse(input, nullptr, false);
    if (data.is_discarded() || !data.is_object()) {
        error = "セーブが壊れている";
        return false;
    }
    BaseInventory base;
    base.money = data.value("money", 0);
    base.sorties = data.value("sorties", 0);
    base.clears = data.value("clears", 0);
    if (data.contains("materials") && data.at("materials").is_object()) {
        for (size_t i = 0; i < kMaterialCount; ++i) {
            base.materials[i] = data.at("materials").value(MaterialKey(static_cast<MaterialId>(i)), 0);
        }
    }
    if (data.contains("chips") && data.at("chips").is_array()) {
        for (const auto& item : data.at("chips")) {
            const std::string key = item.value("part", std::string());
            for (const PartDefinition& part : parts) {
                if (key == part.key) {
                    base.chips.push_back({part.id, FindRarity(item.value("rarity", std::string()))});
                    break;
                }
            }
        }
    }
    if (data.contains("weapons") && data.at("weapons").is_array()) {
        for (const auto& item : data.at("weapons")) {
            const std::string key = item.value("key", std::string());
            if (key.empty()) {
                continue;
            }
            BaseWeapon weapon{key, FindRarity(item.value("rarity", std::string())), {}};
            if (item.contains("chips") && item.at("chips").is_array()) {
                for (const auto& chip : item.at("chips")) {
                    const std::string part = chip.value("part", std::string());
                    for (const PartDefinition& definition : parts) {
                        if (part == definition.key) {
                            weapon.chips.push_back({definition.id, FindRarity(chip.value("rarity", std::string()))});
                            break;
                        }
                    }
                }
            }
            base.weapons.push_back(std::move(weapon));
        }
    }
    if (data.contains("equipped") && data.at("equipped").is_array()) {
        for (size_t i = 0; i < kWeaponSlots && i < data.at("equipped").size(); ++i) {
            const int index = data.at("equipped")[i].is_number_integer() ? data.at("equipped")[i].get<int>() : -1;
            base.equipped[i] = index >= 0 && static_cast<size_t>(index) < base.weapons.size() ? index : -1;
        }
    }
    out = std::move(base);
    return true;
}

/// @brief 拠点を path へ書く。途中で落ちても前のセーブが壊れないよう、別のファイルに書いてから置き換える。
inline bool SaveBaseInventory(const std::string& path, const BaseInventory& base, const std::array<PartDefinition, kPartCount>& parts,
                              std::string& error)
{
    nlohmann::json data = nlohmann::json::object();
    data["schemaVersion"] = 1;
    data["money"] = base.money;
    for (size_t i = 0; i < kMaterialCount; ++i) {
        data["materials"][MaterialKey(static_cast<MaterialId>(i))] = base.materials[i];
    }
    data["chips"] = nlohmann::json::array();
    for (const LootBag::Chip& chip : base.chips) {
        data["chips"].push_back({{"part", parts[static_cast<size_t>(chip.part)].key}, {"rarity", RarityKey(chip.rarity)}});
    }
    data["weapons"] = nlohmann::json::array();
    for (const BaseWeapon& weapon : base.weapons) {
        nlohmann::json item = {{"key", weapon.key}, {"rarity", RarityKey(weapon.rarity)}};
        if (!weapon.chips.empty()) {
            item["chips"] = nlohmann::json::array();
            for (const LootBag::Chip& chip : weapon.chips) {
                item["chips"].push_back({{"part", parts[static_cast<size_t>(chip.part)].key}, {"rarity", RarityKey(chip.rarity)}});
            }
        }
        data["weapons"].push_back(std::move(item));
    }
    data["equipped"] = base.equipped;
    data["sorties"] = base.sorties;
    data["clears"] = base.clears;

    const std::filesystem::path target = std::filesystem::path(path);
    std::error_code ignored;
    if (target.has_parent_path()) {
        std::filesystem::create_directories(target.parent_path(), ignored);
    }
    std::filesystem::path temporary = target;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            error = "セーブを書けない";
            return false;
        }
        output << data.dump(2) << "\n";
        if (!output) {
            error = "セーブを書けない";
            return false;
        }
    }
    std::error_code renameError;
    std::filesystem::rename(temporary, target, renameError);
    if (renameError) {
        error = "セーブを置き換えられない";
        return false;
    }
    error.clear();
    return true;
}
} // namespace survivor
