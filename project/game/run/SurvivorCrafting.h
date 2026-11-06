#pragma once
#include "game/run/SurvivorLoot.h"
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace survivor {
/// @brief 作る・進化に要る物（素材4種とお金）。
struct CraftCost {
    std::array<int, kMaterialCount> materials{};
    int money = 0;
};

/// @brief コモンの武器を素材で作るときのレシピ（計画書 p3 の W7）。main はその武器の主な素材（進化で多く使う）。
struct WeaponRecipe {
    std::string key;
    CraftCost cost{};
    MaterialId main = MaterialId::Square;
    /// @brief 作れるか（false なら、進化の主な素材を決めるだけ。作るのではなく派生の木で手に入れる武器）。
    bool craftable = true;
};

/// @brief 派生の木の枝：from の武器が minRarity 以上なら、cost を払って to の武器へ変えられる（レア度とチップはそのまま）。
struct WeaponBranch {
    std::string from;
    std::string to;
    Rarity minRarity = Rarity::Uncommon;
    CraftCost cost{};
};

/// @brief 作る・派生の木（進化）とレア度の効き（survivor_proto.json の crafting で変える。数値は仮）。
/// @note 派生の木は、レア度を1段ずつ上げる道（コモン → … → レジェンド）と、別の武器へ分かれる枝（branches）。
struct CraftingConfig {
    /// @brief レア度ごとの威力と弾倉の倍率（コモン〜レジェンド）。チップ枠はレア度の分が別に足される（ChipSlots）。
    std::array<float, kRarityCount> rarityDamage{1.0f, 1.1f, 1.2f, 1.35f, 1.5f};
    std::array<float, kRarityCount> rarityMagazine{1.0f, 1.1f, 1.2f, 1.35f, 1.5f};
    std::vector<WeaponRecipe> recipes{
        {"cannon", {{10, 5, 0, 0}, 0}, MaterialId::Square},
        {"machineGun", {{0, 15, 0, 0}, 0}, MaterialId::Triangle},
        {"grenade", {{10, 0, 3, 0}, 0}, MaterialId::Square},
        {"beam", {{0, 10, 5, 0}, 0}, MaterialId::Pentagon},
        {"shotgun", {}, MaterialId::Triangle, false},
        {"sniper", {}, MaterialId::Pentagon, false},
        {"homing", {}, MaterialId::Triangle, false},
        {"ricochet", {}, MaterialId::Square, false},
    };
    std::vector<WeaponBranch> branches{
        {"cannon", "ricochet", Rarity::Uncommon, {{30, 0, 0, 0}, 200}},
        {"cannon", "sniper", Rarity::Rare, {{40, 0, 10, 0}, 400}},
        {"machineGun", "shotgun", Rarity::Uncommon, {{0, 30, 0, 0}, 200}},
        {"machineGun", "homing", Rarity::Rare, {{0, 40, 10, 0}, 400}},
    };
    /// @brief そのレア度へ進化するのに要る物（[0] のコモンは使わない）と、そのほかに要る主な素材の数。
    std::array<CraftCost, kRarityCount> evolve{
        CraftCost{},
        CraftCost{{0, 0, 0, 0}, 100},
        CraftCost{{0, 0, 5, 0}, 300},
        CraftCost{{0, 0, 10, 3}, 800},
        CraftCost{{0, 0, 15, 10}, 2000},
    };
    std::array<int, kRarityCount> evolveMain{0, 20, 40, 60, 80};
};

/// @brief 武器のレシピを探す。無ければ nullptr。
inline const WeaponRecipe* FindRecipe(const CraftingConfig& config, const std::string& key)
{
    for (const WeaponRecipe& recipe : config.recipes) {
        if (recipe.key == key) {
            return &recipe;
        }
    }
    return nullptr;
}

/// @brief key の武器を target のレア度へ進化させるのに要る物（主な素材の分を足す）。
inline CraftCost EvolveCost(const CraftingConfig& config, const std::string& key, Rarity target)
{
    CraftCost cost = config.evolve[static_cast<size_t>(target)];
    const WeaponRecipe* recipe = FindRecipe(config, key);
    const MaterialId main = recipe ? recipe->main : MaterialId::Square;
    cost.materials[static_cast<size_t>(main)] += config.evolveMain[static_cast<size_t>(target)];
    return cost;
}

/// @brief 素材とお金が足りるか。
inline bool CanAfford(const std::array<int, kMaterialCount>& materials, int money, const CraftCost& cost)
{
    for (size_t i = 0; i < kMaterialCount; ++i) {
        if (materials[i] < cost.materials[i]) {
            return false;
        }
    }
    return money >= cost.money;
}

/// @brief 素材とお金を払う（足りるかは先に CanAfford で見る）。
inline void Pay(std::array<int, kMaterialCount>& materials, int& money, const CraftCost& cost)
{
    for (size_t i = 0; i < kMaterialCount; ++i) {
        materials[i] -= cost.materials[i];
    }
    money -= cost.money;
}

/// @brief 要る物の一行（「四角 10・三角 5・お金 100」）。
inline std::string DescribeCost(const CraftCost& cost)
{
    static constexpr std::array<const char*, kMaterialCount> kShort{"四角", "三角", "五角", "ボス"};
    std::string text;
    for (size_t i = 0; i < kMaterialCount; ++i) {
        if (cost.materials[i] > 0) {
            text += (text.empty() ? "" : "・") + std::string(kShort[i]) + " " + std::to_string(cost.materials[i]);
        }
    }
    if (cost.money > 0) {
        text += (text.empty() ? "" : "・") + std::string("お金 ") + std::to_string(cost.money);
    }
    return text.empty() ? std::string("なし") : text;
}

/// @brief レア度を効かせた武器の値（威力と弾倉。弾倉の無い武器はそのまま）。
/// @note 弾倉は切り上げる（弾倉の小さい武器も、レア度が上がると少しは増えるように）。
inline WeaponDefinition ApplyRarity(const WeaponDefinition& weapon, Rarity rarity, const CraftingConfig& config)
{
    WeaponDefinition result = weapon;
    result.rarity = rarity;
    result.damageScale *= config.rarityDamage[static_cast<size_t>(rarity)];
    if (result.magazine > 0) {
        const float scaled = static_cast<float>(weapon.magazine) * config.rarityMagazine[static_cast<size_t>(rarity)];
        result.magazine = (std::max)(1, static_cast<int>(std::ceil(scaled - 0.001f)));
    }
    return result;
}
} // namespace survivor
