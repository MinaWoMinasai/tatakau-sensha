#pragma once
#include "game/run/SurvivorHeroSkills.h"
#include "game/run/SurvivorShop.h"
#include "game/run/SurvivorSubWeapons.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

// 強化の画面（計画書 p9 の L1）の札に出す、1行の効果。変わる所だけを短く、数字は「前 → 後」で書く。
// 詳しい説明は SurvivorSubWeapons.h・SurvivorShop.h・SurvivorHeroSkills.h の文（持ち物の古い画面とショップが使う）。

namespace survivor {

/// @brief 副武器（tier は今の段。0 は手に入れるとき、最後は覚醒）。
inline const char* SubWeaponEffect(SubWeapon weapon, int tier)
{
    static constexpr std::array<std::array<const char*, kMaxSubWeaponTier>, kSubWeaponCount> kTexts{{
        {"球が2つ回り、触れた敵を削る", "威力アップ", "球 2 → 3つ", "輪が広がり、威力アップ", "球 4つ・押し返し・敵の弾も消す"},
        {"1秒ごとに近い敵を撃つ", "間隔 1.0 → 0.8秒・威力アップ", "間隔 0.8 → 0.65秒・威力アップ", "一度に 1 → 2体へ撃つ", "3体へ撃ち、1体を貫く"},
        {"2.5秒ごとに地雷を置く", "間隔 2.5 → 2.1秒・爆発が強い", "間隔 2.1 → 1.7秒・爆発が強い", "間隔 1.7 → 1.5秒・爆発が広い",
         "大きな爆発＋小さな爆発 3つ"},
        {"3秒ごとに敵の多い所へ砲弾", "爆発が強く、広く", "砲弾 1 → 2発・2.6秒ごと", "間隔 2.6 → 2.2秒・爆発が強い", "砲弾 2 → 3発・大きな爆発"},
        {"ついて来て、近い敵を撃つ", "撃つ間隔が縮む", "機体 1 → 2機", "弾が1体を貫く・間隔が縮む", "機体 2 → 3機・2体を貫く"},
        {"3秒ごとに扇の炎・当たると燃える", "炎が長く、少し遠くへ", "扇が広い・燃える威力アップ", "間隔 3 → 2秒・遠くまで", "炎が出続け、大きく広がる"},
        {"4秒ごとに押し返して削る", "間隔が縮み、輪が広がる", "間隔 3秒・威力アップ", "間隔 3 → 2.6秒・輪が広がる", "間隔 2.2秒・敵の弾も消す"},
    }};
    return kTexts[static_cast<size_t>(weapon)][static_cast<size_t>((std::clamp)(tier, 0, kMaxSubWeaponTier - 1))];
}

/// @brief 機体専用スキル（tier は今の段）。
inline const char* HeroSkillEffect(HeroSkill skill, int tier)
{
    static constexpr std::array<std::array<const char*, kHeroSkillMaxTier>, kHeroSkillCount> kTexts{{
        {"落ち着くと主武器の威力 +20%", "威力 +20% → +35%", "威力 +35% → +50%・ばらけ半分"},
        {"一度の連射 5 → 6発", "一度の連射 6 → 7発", "一度の連射 7 → 8発"},
        {"倒すと連射 +5%（4つまで）", "重なり 4 → 7つ", "重なり 7 → 10"},
    }};
    return kTexts[static_cast<size_t>(skill)][static_cast<size_t>((std::clamp)(tier, 0, kHeroSkillMaxTier - 1))];
}

/// @brief 主武器（拡散連射）の強化（tier は今の段）。
inline const char* BurstGunEffect(int tier)
{
    static constexpr std::array<const char*, kBurstGunMaxTier> kTexts{
        "敵を1体・敵の弾を1つ貫く", "撃った後の待ち ×0.7", "連射 ×1.4", "さらに1つ貫く・威力 ×1.2", "当たると小さく爆発", "燃やして3秒削る・連射アップ"};
    return kTexts[static_cast<size_t>((std::clamp)(tier, 0, kBurstGunMaxTier - 1))];
}

/// @brief パーク。value は1段ぶんの強さ、tier は今の段（0 なら後の値だけ）。
inline std::string PerkEffect(PerkId id, float value, int tier)
{
    const float before = value * static_cast<float>(tier), after = value * static_cast<float>(tier + 1);
    auto change = [&](const std::string& head, float a, float b, const std::string& sign, const std::string& unit) {
        auto number = [](float v) {
            const float rounded = std::round(v * 10.0f) / 10.0f;
            const int whole = static_cast<int>(std::floor(rounded + 0.001f));
            const int tenth = static_cast<int>(std::round((rounded - static_cast<float>(whole)) * 10.0f));
            return std::to_string(whole) + (tenth > 0 ? "." + std::to_string(tenth) : std::string());
        };
        return head + " " + (tier > 0 ? sign + number(a) + unit + " → " : std::string()) + sign + number(b) + unit;
    };
    switch (id) {
    case PerkId::Lifesteal:
        return change("与えたダメージの", before * 100.0f, after * 100.0f, "", "%") + " を回復";
    case PerkId::Armor:
        return change("最大HP", before, after, "+", "");
    case PerkId::Speed:
        return change("移動", before * 100.0f, after * 100.0f, "+", "%");
    case PerkId::Magnet:
        return change("引き寄せる範囲", before * 100.0f, after * 100.0f, "+", "%");
    case PerkId::UltCharge:
        return change("Q の溜まり方", before * 100.0f, after * 100.0f, "+", "%");
    case PerkId::Crit:
        return change("会心", before * 100.0f, after * 100.0f, "", "%") + "（威力2倍）";
    case PerkId::Area:
        return change("弾と爆発の大きさ", before * 100.0f, after * 100.0f, "+", "%");
    case PerkId::ExpGain:
        return change("経験値", before * 100.0f, after * 100.0f, "+", "%");
    case PerkId::HealDrop:
        return change("スパナが落ちる率", before * 100.0f, after * 100.0f, "+", "%");
    case PerkId::Guard:
        return change("当たりを防ぐ", GuardCooldown(before), GuardCooldown(after), "", "秒") + "ごと";
    case PerkId::Revive:
        return "倒れても1回、HP " + std::to_string(static_cast<int>(std::round(value * 100.0f))) + "% で立ち直る";
    default:
        return GetPerkInfo(id).text;
    }
}

/// @brief 技の強化（対象 target、tier は今の段）。
inline std::string UpgradeEffect(const HeroDefinition& hero, int target, int tier)
{
    const AbilityDefinition& ability = UpgradeTargetAbility(hero, target);
    if (target == 0 && ability.kind == AbilityKind::BurstGun) {
        return BurstGunEffect(tier);
    }
    const bool weapon = target == 0 && ability.kind == AbilityKind::None;
    if (tier >= 2) {
        return TierThreeText(ability.kind);
    }
    if (tier == 1 && !weapon) {
        return ability.cooldown > 0.0f ? "待ち時間 ×0.8" : "効く時間 ×1.25";
    }
    return weapon ? "威力 ×1.15" : "威力 ×1.25";
}

} // namespace survivor
