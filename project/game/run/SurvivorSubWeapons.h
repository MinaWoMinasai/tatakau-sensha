#pragma once
#include <array>
#include <string>

// 副武器（計画書 p7 の E4）：レベルアップの3択で手に入り、勝手に動く武器。主砲は自分で向けて撃つまま、副武器を集めて組み合わせる。
// 段は1〜3で、3段目で覚醒（挙動が変わる）。動かす処理は HeroSubWeapons.cpp（技の土台 HeroAbilityCore の一部）。

namespace survivor {

enum class SubWeapon {
    Orbit,  ///< まわる衛星：自機のまわりを球が回り、触れた敵を削る。
    Turret, ///< 見張りの砲：近い敵へ、決まった間隔で小さな弾を撃つ。
    Mine,   ///< 置き地雷：足元に地雷を置いていき、敵が近づくと爆発する。
    Count,
};

inline constexpr size_t kSubWeaponCount = static_cast<size_t>(SubWeapon::Count);
inline constexpr int kMaxSubWeaponTier = 3;

struct SubWeaponInfo {
    const char* key;
    const char* name;
    /// @brief 札の左の表示で使う短い名前。
    const char* shortName;
    const char* icon;
    /// @brief 段を上げると何が変わるか（[今の段]。0 は手に入れるとき、2 は覚醒）。
    std::array<const char*, kMaxSubWeaponTier> tierText;
};

inline const SubWeaponInfo& GetSubWeaponInfo(SubWeapon weapon)
{
    static const std::array<SubWeaponInfo, kSubWeaponCount> kInfos{{
        {"orbit", "まわる衛星", "衛星", "chip_collector",
         {"自機のまわりを球が2つ回り、触れた敵を削る", "球が3つに増え、威力が上がる", "球が4つになって輪が広がり、触れた敵を押し返し、敵の弾も消す"}},
        {"turret", "見張りの砲", "砲", "chip_loader",
         {"1秒ごとに、近い敵へ小さな弾を自動で撃つ", "撃つ間隔が 0.65秒に縮み、威力が上がる", "一度に近い3体へ撃ち、弾が1体を貫く"}},
        {"mine", "置き地雷", "地雷", "chip_killBurst",
         {"2.5秒ごとに足元へ地雷を置く。敵が近づくと爆発（6つまで）", "1.7秒ごとに置き、爆発が強くなる", "爆発が大きくなり、まわりに小さな爆発が3つ続く"}},
    }};
    return kInfos[static_cast<size_t>(weapon)];
}

/// @brief 3択の札の名前（tier は今の段）。
inline std::string SubWeaponOfferName(SubWeapon weapon, int tier)
{
    const SubWeaponInfo& info = GetSubWeaponInfo(weapon);
    return tier <= 0 ? std::string("副武器：") + info.name : std::string(info.name) + " " + std::to_string(tier + 1) + "段目";
}

/// @brief 3択の札の説明（tier は今の段）。
inline std::string SubWeaponOfferText(SubWeapon weapon, int tier)
{
    const SubWeaponInfo& info = GetSubWeaponInfo(weapon);
    const int index = tier < 0 ? 0 : tier >= kMaxSubWeaponTier ? kMaxSubWeaponTier - 1 : tier;
    return std::string(info.tierText[static_cast<size_t>(index)]) + (tier <= 0 ? "（勝手に動く）" : "");
}

} // namespace survivor
