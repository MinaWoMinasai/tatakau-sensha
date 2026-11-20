#pragma once
#include "game/run/SurvivorHeroes.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace survivor {
/// @brief 波の間のショップで買えるパーク（計画書 p4 の K4）。共通の6つと、機体ごとの3つ。挙動が変わる物を中心にする。
enum class PerkId {
    Lifesteal,    // 共通：与えたダメージの value 倍を回復する
    KillCooldown, // 共通：倒すたびに、技の待ち時間を value 秒縮める
    Armor,        // 共通：最大HPを value 増やす
    Speed,        // 共通：移動の速さを value 倍だけ上げる（0.12 なら +12%）
    Magnet,       // 共通：欠片を引き寄せる範囲を value 倍だけ広げる
    UltCharge,    // 共通：Q のゲージの溜まり方を value 倍だけ増やす
    Coolant,      // ソルジャー型：主武器の1発の熱を value 倍にする
    RocketRefund, // ソルジャー型：小型ロケットで倒すたびに、ロケットの待ち時間を value 秒縮める
    FieldMedic,   // ソルジャー型：回復の場で回復した量の value 倍だけ、傷も治る
    BeamKill,     // モイラ型：吸収ビームで倒すたびに、吸収ゲージを value 秒ぶん溜める
    PhaseHeal,    // モイラ型：消える移動の終わりに、最大HPの value 倍を回復する
    OrbAbsorb,    // モイラ型：跳ねる球の与えた威力でも、吸収ビームの value 倍だけ吸収ゲージが溜まる
    ChargeShock,  // タンク型：突進の終わりに、半径 value の小さな衝撃波を出す
    BarrierHeal,  // タンク型：バリアで弾を止めるたびに、HP を value 回復する
    Quake,        // タンク型：衝撃波で押し返した数だけ、衝撃波の待ち時間を value 秒縮める
    Count
};
inline constexpr size_t kPerkCount = static_cast<size_t>(PerkId::Count);

/// @brief パークの名前・説明・使える機体（空なら全機体）と、最初の値段と強さ。値段と強さは設定の shop.perks で変えられる。
struct PerkInfo {
    const char* key;
    const char* name;
    const char* text;
    const char* hero;
    int price;
    float value;
};

inline const PerkInfo& GetPerkInfo(PerkId id)
{
    static const std::array<PerkInfo, kPerkCount> kPerks{{
        {"lifesteal", "吸い取る装甲", "与えたダメージの一部で回復する", "", 250, 0.02f},
        {"killCooldown", "連鎖の冷却", "倒すたびに、技の待ち時間が縮む", "", 200, 0.25f},
        {"armor", "厚い装甲", "最大HPが増える", "", 150, 20.0f},
        {"speed", "軽い足回り", "移動が速くなる", "", 150, 0.12f},
        {"magnet", "欠片の磁石", "欠片を遠くから引き寄せる", "", 150, 1.0f},
        {"ultCharge", "見せ場の予感", "Q のゲージが早く溜まる", "", 200, 0.25f},
        {"coolant", "冷却材", "連射砲の熱が溜まりにくい", "soldier", 300, 0.75f},
        {"rocketRefund", "ロケットの再装填", "小型ロケットで倒すたびに、ロケットの待ち時間が縮む", "soldier", 300, 1.0f},
        {"fieldMedic", "野戦の手当て", "回復の場で回復すると、傷も治る", "soldier", 300, 0.5f},
        {"beamKill", "吸い尽くす", "吸収ビームで倒すと、吸収ゲージが溜まる", "moira", 300, 0.5f},
        {"phaseHeal", "消える癒し", "消える移動の終わりに回復する", "moira", 300, 0.08f},
        {"orbAbsorb", "跳ねる吸収", "跳ねる球の威力でも吸収ゲージが溜まる", "moira", 300, 0.5f},
        {"chargeShock", "突進の衝撃", "突進の終わりに、小さな衝撃波を出す", "tank", 300, 3.5f},
        {"barrierHeal", "盾の手当て", "バリアで弾を止めるたびに、HP が少し戻る", "tank", 300, 1.0f},
        {"quake", "地鳴り", "衝撃波で押し返した数だけ、衝撃波の待ち時間が縮む", "tank", 300, 0.3f},
    }};
    return kPerks[static_cast<size_t>(id)];
}

/// @brief 設定での名前からパークを探す。無ければ Count。
inline PerkId FindPerk(const std::string& key)
{
    for (size_t i = 0; i < kPerkCount; ++i) {
        if (key == GetPerkInfo(static_cast<PerkId>(i)).key) {
            return static_cast<PerkId>(i);
        }
    }
    return PerkId::Count;
}

/// @brief 技の強化の対象：主武器と4つの技の枠（0 が主武器、1〜4 が右クリック・Shift・E・Q）。
inline constexpr size_t kUpgradeTargets = 5;
/// @brief 技の強化の段の上限。3段目で挙動が変わる。
inline constexpr int kMaxUpgradeTier = 3;

/// @brief ショップの設定（計画書 p4 の K4）。
/// @note 並ぶ数は offerCount（修理はいつも別に並ぶ）。引き直しは rerollCost で、引くたびに rerollStep 高くなる（波ごとに戻る）。
/// 技の強化の値段は段ごとに upgradePrices。パークの値段と強さは perks（PerkInfo の最初の値を上書きする）。
struct ShopConfig {
    int offerCount = 5;
    int rerollCost = 50;
    int rerollStep = 25;
    std::array<int, kMaxUpgradeTier> upgradePrices{120, 250, 400};
    std::array<int, kPerkCount> perkPrices = [] {
        std::array<int, kPerkCount> prices{};
        for (size_t i = 0; i < kPerkCount; ++i) {
            prices[i] = GetPerkInfo(static_cast<PerkId>(i)).price;
        }
        return prices;
    }();
    std::array<float, kPerkCount> perkValues = [] {
        std::array<float, kPerkCount> values{};
        for (size_t i = 0; i < kPerkCount; ++i) {
            values[i] = GetPerkInfo(static_cast<PerkId>(i)).value;
        }
        return values;
    }();
};
// 技の強化（計画書 p4 の K4。p5 の後で遠征の工房でも使う）。1段目は威力、2段目は待ち時間（待ち時間の無い技は効く秒）、
// 3段目で挙動が変わる（挙動は HeroAbilityCore が UpgradeTier を見て変える）。試作のショップと遠征の工房で同じ計算を使う。

/// @brief 強化の対象の技（0 は主武器の技、1〜4 は右クリック・Shift・E・Q）。
inline const AbilityDefinition& UpgradeTargetAbility(const HeroDefinition& hero, int target)
{
    return target == 0 ? hero.primaryAbility : hero.abilities[static_cast<size_t>(target - 1)];
}

/// @brief 強化の段を機体の技の数値に効かせる：1段目は威力 ×1.25、2段目は待ち時間 ×0.8（待ち時間の無い技は効く秒 ×1.25）。
inline void ApplyUpgradeTiers(HeroDefinition& hero, const std::array<int, kUpgradeTargets>& tiers)
{
    auto apply = [](AbilityDefinition& ability, int tier) {
        if (tier >= 1) {
            ability.power *= 1.25f;
        }
        if (tier >= 2) {
            if (ability.cooldown > 0.0f) {
                ability.cooldown *= 0.8f;
            } else {
                ability.duration *= 1.25f;
            }
        }
    };
    apply(hero.primaryAbility, tiers[0]);
    for (size_t i = 0; i < kAbilitySlotCount; ++i) {
        apply(hero.abilities[i], tiers[i + 1]);
    }
}

/// @brief 主武器が武器のときの威力の倍率（1段ごとに ×1.15、2段まで）。
inline float PrimaryWeaponDamageScale(int tier)
{
    return std::pow(1.15f, static_cast<float>((std::min)(2, tier)));
}

/// @brief 主武器が武器のときの、3段目の小さな爆発の半径（無ければ0）。
inline float PrimaryWeaponBlastRadius(int tier)
{
    return tier >= 3 ? 1.3f : 0.0f;
}

/// @brief 技の強化の3段目の説明（技の種類ごと）。主武器が武器なら kind は None。
inline std::string TierThreeText(AbilityKind kind)
{
    switch (kind) {
    case AbilityKind::None:
        return "弾が当たった所で小さく爆発する";
    case AbilityKind::DrainBeam:
        return "光線が近くのもう1体へ跳ねる（威力は半分）";
    case AbilityKind::Rocket:
        return "ロケットを2発、少し開いて撃つ";
    case AbilityKind::Sprint:
        return "走り出すと熱が0に戻る";
    case AbilityKind::HealZone:
        return "回復の場が自機について来る";
    case AbilityKind::Barrage:
        return "弾幕の弾が2体を貫く";
    case AbilityKind::SelfHeal:
        return "回復した量の半分だけ、傷も治る";
    case AbilityKind::Phase:
        return "使うと吸収ゲージが1.5秒ぶん溜まる";
    case AbilityKind::BounceOrb:
        return "跳ねるたびに球が大きくなる";
    case AbilityKind::Lance:
        return "太い光線の間、移動が速くなる";
    case AbilityKind::Barrier:
        return "バリアで弾を止めると Q のゲージが溜まる";
    case AbilityKind::Charge:
        return "突進の間は無敵になり、敵や弾をすり抜ける";
    case AbilityKind::Shockwave:
        return "衝撃波が敵の弾も消す";
    case AbilityKind::Fortress:
        return "大きな盾の間、HP が少しずつ戻る";
    default:
        return "挙動が変わる";
    }
}

/// @brief 強化の名前（「ロケット 2段」など）。tier は今の段（買うと tier+1 段になる）。
inline std::string UpgradeOfferName(const HeroDefinition& hero, int target, int tier)
{
    const AbilityDefinition& ability = UpgradeTargetAbility(hero, target);
    const std::string name = target == 0 ? (ability.kind != AbilityKind::None ? ability.name : std::string("主武器")) : ability.name;
    return name + " " + std::to_string(tier + 1) + "段";
}

/// @brief 強化の説明（ボタンと、次の段で変わること）。tier は今の段。
inline std::string UpgradeOfferText(const HeroDefinition& hero, int target, int tier)
{
    const int next = tier + 1;
    const AbilityDefinition& ability = UpgradeTargetAbility(hero, target);
    const std::string button = target == 0 ? std::string("左クリック") : AbilitySlotButton(static_cast<AbilitySlot>(target - 1));
    std::string effect;
    if (next == 1) {
        effect = target == 0 && ability.kind == AbilityKind::None ? "威力 ×1.15" : "威力 ×1.25";
    } else if (next == 2) {
        effect = target == 0 && ability.kind == AbilityKind::None ? "威力 ×1.15"
                 : ability.cooldown > 0.0f                         ? "待ち時間 ×0.8"
                                                                   : "効く時間 ×1.25";
    } else {
        effect = "挙動が変わる：" + TierThreeText(ability.kind);
    }
    return button + " の技を強くする。\n" + effect;
}
} // namespace survivor
