#pragma once
#include <array>
#include <string>

// 副武器（計画書 p7 の E4、p8 の F3）：レベルアップの3択で手に入り、勝手に動く武器。主砲は自分で向けて撃つまま、副武器を集めて組み合わせる。
// 段は Lv1〜5 で、Lv5 で覚醒（挙動が変わる）。動かす処理は HeroSubWeapons.cpp（技の土台 HeroAbilityCore の一部）。

namespace survivor {

enum class SubWeapon {
    Orbit,  ///< まわる衛星：自機のまわりを球が回り、触れた敵を削る。
    Turret, ///< 見張りの砲：近い敵へ、決まった間隔で小さな弾を撃つ。
    Mine,   ///< 置き地雷：足元に地雷を置いていき、敵が近づくと爆発する。
    Mortar, ///< 迫撃砲：ときどき、敵の多い所に砲弾が落ちて爆発する（落ちる所に印が出る）。
    Drone,  ///< 随伴機：自機について来て、近い敵を撃つ。
    Flame,  ///< 火炎放射：向いている方へ短い扇の炎を出し、当たった敵を燃やす。
    Pulse,  ///< 衝撃の輪：一定時間ごとに、自機のまわりを押し返して削る。
    Count,
};

inline constexpr size_t kSubWeaponCount = static_cast<size_t>(SubWeapon::Count);
inline constexpr int kMaxSubWeaponTier = 5;

struct SubWeaponInfo {
    const char* key;
    const char* name;
    /// @brief 札の左の表示で使う短い名前。
    const char* shortName;
    const char* icon;
    /// @brief 段を上げると何が変わるか（[今の段]。0 は手に入れるとき、最後は覚醒）。
    std::array<const char*, kMaxSubWeaponTier> tierText;
};

inline const SubWeaponInfo& GetSubWeaponInfo(SubWeapon weapon)
{
    static const std::array<SubWeaponInfo, kSubWeaponCount> kInfos{{
        {"orbit", "まわる衛星", "衛星", "chip_collector",
         {"自機のまわりを球が2つ回り、触れた敵を削る", "威力が上がる", "球が3つに増える", "輪が広がり、威力が上がる",
          "覚醒：球が4つになって輪が広がり、触れた敵を押し返し、敵の弾も消す"}},
        {"turret", "見張りの砲", "砲", "chip_loader",
         {"1秒ごとに、近い敵へ小さな弾を自動で撃つ", "撃つ間隔が 0.8秒に縮み、威力が上がる", "撃つ間隔が 0.65秒に縮み、威力が上がる",
          "一度に近い2体へ撃つ", "覚醒：一度に近い3体へ撃ち、弾が1体を貫く。間隔 0.55秒"}},
        {"mine", "置き地雷", "地雷", "chip_killBurst",
         {"2.5秒ごとに足元へ地雷を置く。敵が近づくと爆発（6つまで）", "2.1秒ごとに置き、爆発が強くなる", "1.7秒ごとに置き、爆発が強くなる",
          "1.5秒ごとに置き、爆発が広がる", "覚醒：爆発が大きくなり、まわりに小さな爆発が3つ続く"}},
        {"mortar", "迫撃砲", "迫撃", "chip_heavyWarhead",
         {"3秒ごとに、敵の多い所へ砲弾が1発落ちて爆発する（落ちる所に印が出る）", "爆発が強く、広くなる", "一度に2発落ちる。2.6秒ごと",
          "2.2秒ごとに落ち、爆発が強くなる", "覚醒：一度に3発、大きな爆発。2秒ごと"}},
        {"drone", "随伴機", "随伴", "chip_thruster",
         {"自機について来る小さな機体が、近い敵を撃つ", "撃つ間隔が縮む", "随伴機が2機になる", "弾が1体を貫き、間隔が縮む",
          "覚醒：随伴機が3機になり、弾が2体を貫く"}},
        {"flame", "火炎放射", "火炎", "chip_residualHeat",
         {"3秒ごとに1.2秒、向いている方へ短い扇の炎を出す。当たった敵は燃える", "炎が長く出て、少し遠くまで届く", "扇が広がり、燃える威力が上がる",
          "2秒ごとに出て、遠くまで届く", "覚醒：炎が出続け、扇が大きく広がる"}},
        {"pulse", "衝撃の輪", "衝撃", "chip_ram",
         {"4秒ごとに、自機のまわりを押し返して削る", "間隔が縮み、輪が広がる", "間隔 3秒、威力が上がる", "間隔 2.6秒、輪が広がる",
          "覚醒：間隔 2.2秒、大きな輪で敵の弾も消す"}},
    }};
    return kInfos[static_cast<size_t>(weapon)];
}

/// @brief 3択の札の名前（tier は今の段）。Lv で数える。
inline std::string SubWeaponOfferName(SubWeapon weapon, int tier)
{
    const SubWeaponInfo& info = GetSubWeaponInfo(weapon);
    return tier <= 0 ? std::string("副武器：") + info.name : std::string(info.name) + " Lv" + std::to_string(tier + 1);
}

/// @brief 3択の札の説明（tier は今の段）。
inline std::string SubWeaponOfferText(SubWeapon weapon, int tier)
{
    const SubWeaponInfo& info = GetSubWeaponInfo(weapon);
    const int index = tier < 0 ? 0 : tier >= kMaxSubWeaponTier ? kMaxSubWeaponTier - 1 : tier;
    return std::string(info.tierText[static_cast<size_t>(index)]) + (tier <= 0 ? "（勝手に動く）" : "");
}

} // namespace survivor
