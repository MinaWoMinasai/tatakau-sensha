#pragma once
#include "game/run/SurvivorForms.h"
#include <array>
#include <string>
#include <vector>

namespace survivor {
/// @brief 武器が使う弾の種類（計画書 p3 の0章）。ライト弾・ヘビー弾は弾倉と R のリロード、エネルギー弾は弾倉なしで予備から直接使う。
enum class AmmoType {
    Light,
    Heavy,
    Energy,
    Count
};

/// @brief 武器のレア度。コモン（白）→ アンコモン（黄緑）→ レア（青）→ エピック（紫）→ レジェンド（金）。
enum class Rarity {
    Common,
    Uncommon,
    Rare,
    Epic,
    Legendary,
    Count
};

/// @brief 持ち物の武器の枠の数。
inline constexpr size_t kWeaponSlots = 3;
inline constexpr size_t kAmmoTypeCount = static_cast<size_t>(AmmoType::Count);

/// @brief 弾の種類の設定での名前。
inline const char* AmmoKey(AmmoType type)
{
    return type == AmmoType::Heavy ? "heavy" : type == AmmoType::Energy ? "energy" : "light";
}

/// @brief 弾の種類の画面に出す名前。
inline const char* AmmoName(AmmoType type)
{
    return type == AmmoType::Heavy ? "ヘビー弾" : type == AmmoType::Energy ? "エネルギー弾" : "ライト弾";
}

/// @brief 設定での名前から弾の種類を探す。無ければライト弾。
inline AmmoType FindAmmo(const std::string& key)
{
    return key == "heavy" ? AmmoType::Heavy : key == "energy" ? AmmoType::Energy : AmmoType::Light;
}

/// @brief レア度の設定での名前。
inline const char* RarityKey(Rarity rarity)
{
    static constexpr std::array<const char*, static_cast<size_t>(Rarity::Count)> kKeys{"common", "uncommon", "rare", "epic", "legendary"};
    return kKeys[static_cast<size_t>(rarity)];
}

/// @brief レア度の画面に出す名前。
inline const char* RarityName(Rarity rarity)
{
    static constexpr std::array<const char*, static_cast<size_t>(Rarity::Count)> kNames{"コモン", "アンコモン", "レア", "エピック", "レジェンド"};
    return kNames[static_cast<size_t>(rarity)];
}

/// @brief 設定での名前からレア度を探す。無ければコモン。
inline Rarity FindRarity(const std::string& key)
{
    for (size_t i = 0; i < static_cast<size_t>(Rarity::Count); ++i) {
        if (key == RarityKey(static_cast<Rarity>(i))) {
            return static_cast<Rarity>(i);
        }
    }
    return Rarity::Common;
}

/// @brief レア度の色（白・黄緑・青・紫・金）。
inline myEngine::Vector4 RarityColor(Rarity rarity)
{
    static constexpr std::array<myEngine::Vector4, static_cast<size_t>(Rarity::Count)> kColors{
        myEngine::Vector4{0.92f, 0.95f, 1.0f, 1.0f}, myEngine::Vector4{0.62f, 1.0f, 0.35f, 1.0f}, myEngine::Vector4{0.35f, 0.65f, 1.0f, 1.0f},
        myEngine::Vector4{0.78f, 0.45f, 1.0f, 1.0f}, myEngine::Vector4{1.0f, 0.80f, 0.25f, 1.0f}};
    return kColors[static_cast<size_t>(rarity)];
}

/// @brief 武器の数値。砲塔の並びと固有の能力（ばらつき・加速・ため撃ちなど）は form の形態から借り、威力・連射・弾速・熱はここの値で上書きする。
/// @note 熱は武器ごとに持つ（しまってある武器は冷えていく）。chipSlots はチップ（W3 以降）で使う。今はデータとして持つだけ。
struct WeaponDefinition {
    std::string key;
    std::string name;
    /// @brief 砲塔と固有の能力を借りる形態。Count は基本砲（playerClasses.json の Basic）。
    FormId form = FormId::Count;
    AmmoType ammo = AmmoType::Light;
    /// @brief 弾倉の弾の数（エネルギー弾は0＝弾倉なし）。
    int magazine = 12;
    /// @brief この武器に切り替えるのにかかる秒（その間は撃てず、ほかの武器にも変えられない）。
    float swapSeconds = 0.4f;
    /// @brief R のリロードにかかる秒（その間は撃てない）。弾倉なしの武器では使わない。
    float reloadSeconds = 1.4f;
    /// @brief 右クリックのため撃ち1発で使う弾の数（ため撃ちを持つ形態だけ）。
    int railAmmoCost = 1;
    int chipSlots = 0;
    Rarity rarity = Rarity::Common;
    float damageScale = 1.0f;
    /// @brief 発射間隔の倍率（大きいほど遅い）。
    float reloadScale = 1.0f;
    float speedScale = 1.0f;
    /// @brief 主砲1発の熱の倍率（熱の上がり方）。
    float heatScale = 1.0f;
    /// @brief 当たった場所で爆発する半径（0 なら爆発しない）と、当たった敵・まわりの敵への威力の倍率（弾の威力に対する）。
    float blastRadius = 0.0f;
    float blastCenter = 0.0f;
    float blastSplash = 0.0f;
    /// @brief 太い砲身にする。
    bool heavyBarrel = false;
    /// @brief 弾と軌跡の色（A が0なら形態の色のまま）。
    myEngine::Vector4 color{0, 0, 0, 0};
};

/// @brief 武器の既定値。survivor_proto.json の weapons で上書き・追加する（計画書 p3 の2章の案。数値は仮）。
inline std::vector<WeaponDefinition> DefaultWeapons()
{
    std::vector<WeaponDefinition> weapons(4);
    auto& cannon = weapons[0];
    cannon.key = "cannon";
    cannon.name = "単発の砲";
    cannon.heatScale = 0.6f;
    auto& machineGun = weapons[1];
    machineGun.key = "machineGun";
    machineGun.name = "機関砲";
    machineGun.form = FormId::MachineGun;
    machineGun.magazine = 40;
    machineGun.swapSeconds = 0.6f;
    machineGun.reloadSeconds = 2.0f;
    machineGun.damageScale = 0.45f;
    machineGun.reloadScale = 0.55f;
    machineGun.heatScale = 0.35f;
    auto& beam = weapons[2];
    beam.key = "beam";
    beam.name = "光線砲";
    beam.form = FormId::Beam;
    beam.ammo = AmmoType::Energy;
    beam.magazine = 0;
    beam.swapSeconds = 0.8f;
    beam.railAmmoCost = 8;
    beam.damageScale = 1.9f;
    beam.reloadScale = 1.35f;
    beam.speedScale = 1.8f;
    beam.heatScale = 2.2f;
    // 範囲の軸：当たると爆発する。範囲が強い分、1発の威力と連射は控えめ。
    auto& grenade = weapons[3];
    grenade.key = "grenade";
    grenade.name = "榴弾砲";
    grenade.ammo = AmmoType::Heavy;
    grenade.magazine = 4;
    grenade.swapSeconds = 0.8f;
    grenade.reloadSeconds = 2.2f;
    grenade.damageScale = 0.8f;
    grenade.reloadScale = 2.2f;
    grenade.speedScale = 0.7f;
    grenade.heatScale = 1.0f;
    grenade.blastRadius = 3.2f;
    grenade.blastCenter = 1.0f;
    grenade.blastSplash = 0.8f;
    grenade.heavyBarrel = true;
    grenade.color = {1.9f, 0.7f, 0.2f, 1.0f};
    return weapons;
}

/// @brief 武器を設定での名前から探す。無ければ -1。
inline int FindWeapon(const std::vector<WeaponDefinition>& weapons, const std::string& key)
{
    for (size_t i = 0; i < weapons.size(); ++i) {
        if (weapons[i].key == key) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

/// @brief 武器を、自機に入れる形態の数値にする。名前と key は武器のもの（weapon が生きている間だけ使える）。
inline FormDefinition WeaponForm(const std::array<FormDefinition, kFormCount>& forms, const WeaponDefinition& weapon)
{
    FormDefinition form{};
    if (weapon.form != FormId::Count) {
        form = forms[static_cast<size_t>(weapon.form)];
    } else {
        form.stage = 0;
    }
    form.key = weapon.key.c_str();
    form.name = weapon.name.c_str();
    form.damageScale = weapon.damageScale;
    form.reloadScale = weapon.reloadScale;
    form.speedScale = weapon.speedScale;
    form.heatScale = weapon.heatScale;
    form.heavyBarrel = form.heavyBarrel || weapon.heavyBarrel;
    if (weapon.color.w > 0.0f) {
        form.color = weapon.color;
    }
    return form;
}
} // namespace survivor
