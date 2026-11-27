#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace survivor {
/// @brief 機体専用スキル（計画書 p8 の F2。自動で効く。各3段）。
/// レベルアップの3択に、技の強化・パーク・副武器と並んで出る。今は標準型の3つ（本人の選び：案A・B・C）。
enum class HeroSkill {
    SteadyAim,   // 狙いすます：止まっているか、ゆっくり歩いていると、主武器の威力が上がる（3段目は弾のばらけも半分）
    AmmoBelt,    // 弾帯：一度の連射で撃つ弾の数が増える
    BattleSense, // 戦場の勘：倒すたびに少しの間、連射が速くなる（重ねられる）
    Count
};
inline constexpr size_t kHeroSkillCount = static_cast<size_t>(HeroSkill::Count);
inline constexpr int kHeroSkillMaxTier = 3;

struct HeroSkillInfo {
    const char* key;
    const char* name;
    /// @brief 使える機体（HeroDefinition::key）。
    const char* hero;
    const char* icon;
    const char* text;
    std::array<const char*, kHeroSkillMaxTier> tierTexts;
};

inline const HeroSkillInfo& GetHeroSkillInfo(HeroSkill skill)
{
    static const std::array<HeroSkillInfo, kHeroSkillCount> kInfos{{
        {"steadyAim", "狙いすます", "standard", "chip_pierce", "止まっているか、ゆっくり歩いている間、主武器の威力が上がる",
         {"0.4秒ほど落ち着くと、主武器の威力 +20%", "威力 +35%", "威力 +50%、弾のばらけが半分になる"}},
        {"ammoBelt", "弾帯", "standard", "chip_loader", "一度の連射で撃つ弾が増える",
         {"弾 +1（5発→6発）", "弾 +2（7発）", "弾 +3（8発）"}},
        {"battleSense", "戦場の勘", "standard", "chip_morale", "倒すたびに2.5秒、連射が速くなる。倒し続けると重なる",
         {"1体ごとに連射 +5%、4つまで（+20%）", "7つまで（+35%）", "10まで（+50%）"}},
    }};
    return kInfos[static_cast<size_t>(skill)];
}

// 段ごとの数値（遊んで調整する）。tier は 0〜3（0 は持っていない）。
/// @brief 狙いすます：落ち着いている（自機の速さが kSteadyAimSpeed 未満）のが kSteadyAimDelay 秒続くと効く。
inline constexpr float kSteadyAimSpeed = 1.2f;
inline constexpr float kSteadyAimDelay = 0.4f;
inline float SteadyAimDamage(int tier)
{
    static constexpr std::array<float, 4> kBonus{0.0f, 0.2f, 0.35f, 0.5f};
    return 1.0f + kBonus[static_cast<size_t>((std::clamp)(tier, 0, 3))];
}
inline int AmmoBeltShots(int tier)
{
    return (std::clamp)(tier, 0, 3);
}
/// @brief 戦場の勘：1つ重なるごとの連射の上がり方・重なる数の上限・続く秒（倒すたびに全部が延びる）。
inline constexpr float kBattleSensePerStack = 0.05f;
inline constexpr float kBattleSenseSeconds = 2.5f;
inline int BattleSenseMaxStacks(int tier)
{
    static constexpr std::array<int, 4> kMax{0, 4, 7, 10};
    return kMax[static_cast<size_t>((std::clamp)(tier, 0, 3))];
}

/// @brief 3択の名前（tier は今の段。上げると tier+1）。
inline std::string HeroSkillOfferName(HeroSkill skill, int tier)
{
    return std::string(GetHeroSkillInfo(skill).name) + " Lv" + std::to_string(tier + 1);
}

/// @brief 3択の説明（tier は今の段）。
inline std::string HeroSkillOfferText(HeroSkill skill, int tier)
{
    const HeroSkillInfo& info = GetHeroSkillInfo(skill);
    const int next = (std::clamp)(tier + 1, 1, kHeroSkillMaxTier);
    return std::string("専用スキル：") + info.text + "\nLv" + std::to_string(next) + "/" + std::to_string(kHeroSkillMaxTier) + "：" +
           info.tierTexts[static_cast<size_t>(next - 1)];
}
} // namespace survivor
