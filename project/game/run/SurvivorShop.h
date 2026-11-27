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
    Coolant,      // 標準型：主武器の1発の熱を value 倍にする
    RocketRefund, // 標準型：小型ロケットで倒すたびに、ロケットの待ち時間を value 秒縮める
    FieldMedic,   // 標準型：回復の場で回復した量の value 倍だけ、傷も治る
    BeamKill,     // モイラ型：吸収ビームで倒すたびに、吸収ゲージを value 秒ぶん溜める
    PhaseHeal,    // モイラ型：消える移動の終わりに、最大HPの value 倍を回復する
    OrbAbsorb,    // モイラ型：跳ねる球の与えた威力でも、吸収ビームの value 倍だけ吸収ゲージが溜まる
    ChargeShock,  // タンク型：突進の終わりに、半径 value の小さな衝撃波を出す
    BarrierHeal,  // タンク型：バリアで弾を止めるたびに、HP を value 回復する
    Quake,        // タンク型：衝撃波で押し返した数だけ、衝撃波の待ち時間を value 秒縮める
    // 計画書 p8 の F4 で足した共通のパーク（HoloCure のアイテムに寄せる）。
    Crit,    // 共通：弾・爆発・直接の削りが、value の確率で2倍になる（会心）
    Area,    // 共通：弾・爆発・炎・衝撃の輪の大きさを value 倍だけ広げる
    ExpGain, // 共通：レベルの経験値を value 倍だけ多く入れる（ポイントは増えない）
    Guard,   // 共通：(kGuardBaseCooldown - value) 秒ごとに1回、当たりを防ぐ
    Revive,  // 共通：1回だけ、倒れる当たりで HP を最大の value 倍にして立ち直る
    HealDrop, // 共通：敵がスパナ（回復。計画書 p8 の F5）を落とす確率を value 倍だけ上げる
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
    /// @brief 段の上限（0 なら設定の perkMaxTier。予備の動力のような1回きりの物は1）。
    int maxTier = 0;
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
        {"coolant", "冷却材", "連射砲の熱が溜まりにくい", "standard", 300, 0.75f},
        {"rocketRefund", "ロケットの再装填", "小型ロケットで倒すたびに、ロケットの待ち時間が縮む", "standard", 300, 1.0f},
        {"fieldMedic", "野戦の手当て", "回復の場で回復すると、傷も治る", "standard", 300, 0.5f},
        {"beamKill", "吸い尽くす", "吸収ビームで倒すと、吸収ゲージが溜まる", "moira", 300, 0.5f},
        {"phaseHeal", "消える癒し", "消える移動の終わりに回復する", "moira", 300, 0.08f},
        {"orbAbsorb", "跳ねる吸収", "跳ねる球の威力でも吸収ゲージが溜まる", "moira", 300, 0.5f},
        {"chargeShock", "突進の衝撃", "突進の終わりに、小さな衝撃波を出す", "tank", 300, 3.5f},
        {"barrierHeal", "盾の手当て", "バリアで弾を止めるたびに、HP が少し戻る", "tank", 300, 1.0f},
        {"quake", "地鳴り", "衝撃波で押し返した数だけ、衝撃波の待ち時間が縮む", "tank", 300, 0.3f},
        {"crit", "会心の照準", "弾や爆発が、ときどき2倍の威力になる", "", 200, 0.15f},
        {"area", "大きな弾頭", "弾・爆発・炎・衝撃の輪が大きくなる", "", 200, 0.2f},
        {"expGain", "学習装置", "レベルの経験値が多く入る", "", 150, 0.2f},
        {"guard", "反応装甲", "数秒ごとに1回だけ、当たりを防ぐ", "", 250, 6.0f},
        {"revive", "予備の動力", "倒れたとき1回だけ立ち直り、まわりを押し返す", "", 400, 0.5f, 1},
        {"healDrop", "整備の手配", "敵がスパナ（回復）を落としやすくなる", "", 150, 1.0f},
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

/// @brief 反応装甲（計画書 p8 の F4）の、当たりを防ぐ間隔の土台と下限（秒）。間隔は 土台 - パークの強さ。
inline constexpr float kGuardBaseCooldown = 16.0f;
inline constexpr float kGuardMinCooldown = 3.0f;
inline float GuardCooldown(float value)
{
    return (std::max)(kGuardMinCooldown, kGuardBaseCooldown - value);
}
/// @brief 会心の威力の倍率。
inline constexpr float kCritScale = 2.0f;

/// @brief 技の強化の対象：主武器と4つの技の枠（0 が主武器、1〜4 が右クリック・Shift・E・Q）。
inline constexpr size_t kUpgradeTargets = 5;
/// @brief パークの段（計画書 p8 の F1）での効き目の説明。value は1段ぶんの強さ、tier は上げた後の段（合計の値を出す）。
inline std::string PerkTierText(PerkId id, float value, int tier)
{
    const float total = value * static_cast<float>(tier);
    auto percent = [](float v) {
        return std::to_string(static_cast<int>(std::round(v * 100.0f))) + "%";
    };
    switch (id) {
    case PerkId::Lifesteal:
        return "与えたダメージの " + std::to_string(static_cast<int>(std::round(total * 1000.0f)) / 10) + "." +
               std::to_string(static_cast<int>(std::round(total * 1000.0f)) % 10) + "% で回復";
    case PerkId::KillCooldown:
        return "倒すたびに技の待ち時間 -" + std::to_string(total).substr(0, 4) + "秒";
    case PerkId::Armor:
        return "最大HP +" + std::to_string(static_cast<int>(std::round(total)));
    case PerkId::Speed:
        return "移動の速さ +" + percent(total);
    case PerkId::Magnet:
        return "欠片を引き寄せる範囲 +" + percent(total);
    case PerkId::UltCharge:
        return "Q のゲージの溜まり方 +" + percent(total);
    case PerkId::Crit:
        return percent(total) + " の確率で威力2倍";
    case PerkId::Area:
        return "弾・爆発の大きさ +" + percent(total);
    case PerkId::ExpGain:
        return "経験値 +" + percent(total);
    case PerkId::HealDrop:
        return "スパナの落ちやすさ +" + percent(total);
    case PerkId::Guard:
        return std::to_string(static_cast<int>(std::round(GuardCooldown(total)))) + "秒ごとに1回、当たりを防ぐ";
    default:
        return GetPerkInfo(id).text;
    }
}

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

/// @brief 拡散連射（主武器の部品 BurstGun。計画書 p7 の E5）の強化の段の上限。HoloCure の武器と同じく Lv1〜7（段0〜6）で、最後が覚醒。
inline constexpr int kBurstGunMaxTier = 6;

/// @brief 強化の段の上限（主武器が拡散連射なら7段、それ以外は3段）。
inline int MaxUpgradeTier(const HeroDefinition& hero, int target)
{
    return target == 0 && hero.primaryAbility.kind == AbilityKind::BurstGun ? kBurstGunMaxTier : kMaxUpgradeTier;
}

/// @brief 拡散連射の、強化の段を効かせた後の値。
struct BurstGunStats {
    int shots = 5;
    float rate = 2.5f;
    float cooldown = 1.0f;
    float damage = 1.0f;
    float spreadDeg = 8.0f;
    float size = 1.0f;
    int pierce = 0;
    float bulletHp = 1.0f;
    float blastRadius = 0.0f;
    float burnDps = 0.0f;
    float burnSeconds = 0.0f;
};

/// @brief 段ごとに足すこと（Lv2〜7）：2 敵と敵の弾を1つ貫く、3 待ち時間 ×0.7、4 連射 ×1.4、5 さらに1つ貫き 威力 ×1.2、
/// 6 当たると小さく爆発、7（覚醒）当たった敵が燃える（3秒、毎秒 主砲の0.6倍）＋ 待ち時間 ×0.7・連射 ×1.2。
inline BurstGunStats BurstGunAt(const AbilityDefinition& ability, int tier)
{
    BurstGunStats stats;
    stats.shots = (std::max)(1, static_cast<int>(std::lround(ability.duration)));
    stats.rate = (std::max)(0.5f, ability.rate);
    stats.cooldown = (std::max)(0.0f, ability.cooldown);
    stats.damage = ability.power;
    stats.spreadDeg = ability.radius;
    stats.size = ability.size > 0.0f ? ability.size : 1.0f;
    if (tier >= 1) {
        stats.pierce += 1;
        stats.bulletHp += 1.0f;
    }
    if (tier >= 2) {
        stats.cooldown *= 0.7f;
    }
    if (tier >= 3) {
        stats.rate *= 1.4f;
    }
    if (tier >= 4) {
        stats.pierce += 1;
        stats.bulletHp += 1.0f;
        stats.damage *= 1.2f;
    }
    if (tier >= 5) {
        stats.blastRadius = 1.4f;
    }
    if (tier >= 6) {
        stats.burnDps = ability.power * 0.6f;
        stats.burnSeconds = 3.0f;
        stats.cooldown *= 0.7f;
        stats.rate *= 1.2f;
    }
    return stats;
}

/// @brief 拡散連射の、次の段で足すこと（next は 1〜6）。
inline const char* BurstGunTierText(int next)
{
    static constexpr std::array<const char*, kBurstGunMaxTier> kTexts{
        "弾が敵を1体・敵の弾を1つ貫く",
        "撃ち終えてからの待ち時間 ×0.7",
        "連射が速くなる ×1.4",
        "さらに1つ貫き、威力 ×1.2",
        "弾が当たった所で小さく爆発する",
        "覚醒：当たった敵が燃えて3秒削られる。待ち時間 ×0.7・連射 ×1.2",
    };
    return kTexts[static_cast<size_t>((std::clamp)(next, 1, kBurstGunMaxTier) - 1)];
}

/// @brief 強化の段を機体の技の数値に効かせる：1段目は威力 ×1.25、2段目は待ち時間 ×0.8（待ち時間の無い技は効く秒 ×1.25）。
/// 主武器が拡散連射なら、主武器は BurstGunAt で別に計算するので、ここでは変えない。
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
    if (hero.primaryAbility.kind != AbilityKind::BurstGun) {
        apply(hero.primaryAbility, tiers[0]);
    }
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
    // 拡散連射は HoloCure と同じく Lv で数える（Lv1 が段0）。
    if (target == 0 && ability.kind == AbilityKind::BurstGun) {
        return name + " Lv" + std::to_string(tier + 2);
    }
    return name + " " + std::to_string(tier + 1) + "段";
}

/// @brief 強化の説明（ボタンと、次の段で変わること）。tier は今の段。
inline std::string UpgradeOfferText(const HeroDefinition& hero, int target, int tier)
{
    const int next = tier + 1;
    const AbilityDefinition& ability = UpgradeTargetAbility(hero, target);
    const std::string button = target == 0 ? std::string("左クリック") : AbilitySlotButton(static_cast<AbilitySlot>(target - 1));
    if (target == 0 && ability.kind == AbilityKind::BurstGun) {
        return std::string("主武器を強くする。\n") + BurstGunTierText(next);
    }
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
