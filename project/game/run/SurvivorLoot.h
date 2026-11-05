#pragma once
#include "game/run/SurvivorParts.h"
#include "game/run/SurvivorWeapons.h"
#include <array>
#include <random>
#include <vector>

namespace survivor {
/// @brief 素材（計画書 p3 の G8）。敵の形ごとに3種類と、ボスの素材。
enum class MaterialId {
    Square,
    Triangle,
    Pentagon,
    Boss,
    Count
};

inline constexpr size_t kMaterialCount = static_cast<size_t>(MaterialId::Count);
inline constexpr size_t kRarityCount = static_cast<size_t>(Rarity::Count);

/// @brief 素材の設定での名前。
inline const char* MaterialKey(MaterialId id)
{
    static constexpr std::array<const char*, kMaterialCount> kKeys{"square", "triangle", "pentagon", "boss"};
    return kKeys[static_cast<size_t>(id)];
}

/// @brief 素材の画面に出す名前。
inline const char* MaterialName(MaterialId id)
{
    static constexpr std::array<const char*, kMaterialCount> kNames{"四角の素材", "三角の素材", "五角の素材", "ボスの素材"};
    return kNames[static_cast<size_t>(id)];
}

/// @brief 床に落ちる物の種類。どれも触れると自動で拾い、弾は予備へ、ほかは出撃中の袋へ入る（お金は欠片のまま拾う）。
enum class LootKind {
    Ammo,
    Material,
    Chip,
    Weapon
};

/// @brief 落とす物の確率と量（計画書 p3 の W3。数値は仮。survivor_proto.json の loot で変える）。
/// @note 確率は敵1体を倒したときのもの。大きな敵（敵戦車・大五角・重い四角）は bigScale 倍になる。
struct LootConfig {
    /// @brief 欠片1つの経験値を、お金に換える倍率。
    float moneyPerExp = 0.1f;
    /// @brief 倒した形の素材を落とす確率（敵戦車は四角・三角・五角のどれか）。
    float materialChance = 0.15f;
    /// @brief 敵戦車がボスの素材を落とす確率（ボスは W8。それまでの仮の入手先）。
    float bossMaterialChance = 0.3f;
    /// @brief 弾を落とす確率と、1つで増える数（ライト・ヘビー・エネルギーの順）。
    std::array<float, kAmmoTypeCount> ammoChance{0.10f, 0.05f, 0.04f};
    std::array<int, kAmmoTypeCount> ammoAmount{30, 4, 15};
    /// @brief 予備の弾の上限。
    std::array<int, kAmmoTypeCount> reserveMax{900, 60, 450};
    float chipChance = 0.01f;
    float weaponChance = 0.004f;
    float bigScale = 5.0f;
    /// @brief チップと武器のレア度の重み（コモン〜レジェンド）。
    std::array<float, kRarityCount> rarityWeights{60.0f, 25.0f, 10.0f, 4.0f, 1.0f};
    /// @brief 落ちている物が寄ってくる距離（ワールド単位）。
    float magnetRadius = 5.0f;
    /// @brief 弾と素材が床に残る秒（チップと武器は消えない）。
    float lifetime = 45.0f;
    /// @brief 休憩部屋で売る値段（W6）。武器とチップはレア度ごと（コモン〜レジェンド）、素材は1つごと（四角・三角・五角・ボス）。
    std::array<int, kRarityCount> weaponPrice{30, 60, 120, 240, 480};
    std::array<int, kRarityCount> chipPrice{10, 20, 40, 80, 160};
    std::array<int, kMaterialCount> materialPrice{2, 2, 3, 20};
};

/// @brief 床に落ちる物1つの中身。id は弾の種類・素材・部品・武器の番号（kind による）。
struct LootItem {
    LootKind kind = LootKind::Ammo;
    int id = 0;
    Rarity rarity = Rarity::Common;
    int amount = 1;
};

/// @brief 出撃中の袋（数の制限なし）。整理・捨てる・売るは休憩部屋で（W6）。死ぬと全部失う（G4）。
struct LootBag {
    struct Chip {
        PartId part = PartId::Warhead;
        Rarity rarity = Rarity::Common;

        bool operator==(const Chip&) const = default;
    };
    struct Weapon {
        /// @brief SpawnConfig::weapons の番号。
        int weapon = 0;
        Rarity rarity = Rarity::Common;
    };
    int money = 0;
    std::array<int, kMaterialCount> materials{};
    std::vector<Chip> chips;
    std::vector<Weapon> weapons;
};

/// @brief 重みからレア度を1つ選ぶ。
inline Rarity RollRarity(const std::array<float, kRarityCount>& weights, std::mt19937& random)
{
    float total = 0.0f;
    for (float weight : weights) {
        total += (std::max)(0.0f, weight);
    }
    if (total <= 0.0f) {
        return Rarity::Common;
    }
    float pick = std::uniform_real_distribution<float>(0.0f, total)(random);
    for (size_t i = 0; i < kRarityCount; ++i) {
        pick -= (std::max)(0.0f, weights[i]);
        if (pick <= 0.0f) {
            return static_cast<Rarity>(i);
        }
    }
    return Rarity::Legendary;
}

/// @brief 武器に付けられるチップの数：武器の chipSlots に、レア度の分を足す（コモン +0・アンコモン +1・レア +1・エピック +2・レジェンド +3）。
inline int ChipSlots(const WeaponDefinition& weapon, Rarity rarity)
{
    static constexpr std::array<int, kRarityCount> kBonus{0, 1, 1, 2, 3};
    return weapon.chipSlots + kBonus[static_cast<size_t>(rarity)];
}

/// @brief チップ1つが部品の何段になるか（コモン 1 〜 レジェンド 5。部品の最大の段まで）。
inline int ChipLevel(Rarity rarity)
{
    return static_cast<int>(rarity) + 1;
}

/// @brief チップになる部品か（G7：今の部品をチップに移す。Q で使う部品と合体の部品は除く）。
inline bool IsChipPart(const PartDefinition& part)
{
    return !part.qSkill && !IsFusion(part);
}
} // namespace survivor
