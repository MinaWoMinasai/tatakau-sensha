#pragma once
#include "game/player/PlayerClassConfig.h"
#include <array>
#include <cstdio>
#include <string>

namespace survivor {
/// @brief 育つ戦車の試作の、Lv5 で選ぶ形態。
enum class FormId {
    Twin,
    LongBarrel,
    Count
};
inline constexpr size_t kFormCount = static_cast<size_t>(FormId::Count);

/// @brief 育つ戦車の試作の、区画の始めに選ぶ特性（D3）。数値は survivor_proto.json の traits。
enum class TraitId {
    Standard,       // 標準：変化なし
    CoolingFurnace, // 冷却炉：熱の上限が増え、冷える速さが落ちる
    Count
};
inline constexpr size_t kTraitCount = static_cast<size_t>(TraitId::Count);

/// @brief 特性の設定での名前（遊んだ記録にも使う）。
inline const char* TraitKey(TraitId id)
{
    return id == TraitId::CoolingFurnace ? "coolingFurnace" : "standard";
}

/// @brief 特性の画面に出す名前。
inline const char* TraitName(TraitId id)
{
    return id == TraitId::CoolingFurnace ? "冷却炉" : "標準";
}

/// @brief 形態の数値。倍率は元の機体設定（連装は Twin、長砲身は Basic）に対して掛けるのではなく、そのまま使う。
struct FormDefinition {
    const char* key = "";
    const char* name = "";
    /// @brief 元にする playerClasses.json の機体。
    const char* sourceClass = "Basic";
    float reloadScale = 1.0f;
    float damageScale = 1.0f;
    float speedScale = 1.0f;
    /// @brief 主砲1発の熱の倍率。
    float heatScale = 1.0f;
    /// @brief 砲身の長さの倍率。
    float barrelLength = 1.0f;
    /// @brief 右クリックのため撃ちを持つか。持つなら威力の強さと熱を使う。
    bool railCharge = false;
    float railPower = 1.0f;
    float railHeatBase = 10.0f;
    float railHeatPerCharge = 25.0f;
};

/// @brief 形態の既定値。survivor_proto.json の forms で上書きする。
inline std::array<FormDefinition, kFormCount> DefaultForms()
{
    std::array<FormDefinition, kFormCount> forms{};
    auto& twin = forms[static_cast<size_t>(FormId::Twin)];
    twin.key = "twin";
    twin.name = "連装";
    twin.sourceClass = "Twin";
    twin.reloadScale = 0.9f;
    twin.damageScale = 0.7f;
    twin.heatScale = 1.1f;
    auto& longBarrel = forms[static_cast<size_t>(FormId::LongBarrel)];
    longBarrel.key = "longBarrel";
    longBarrel.name = "長砲身";
    longBarrel.reloadScale = 1.35f;
    longBarrel.damageScale = 1.8f;
    longBarrel.speedScale = 1.6f;
    longBarrel.heatScale = 1.2f;
    longBarrel.barrelLength = 1.7f;
    longBarrel.railCharge = true;
    return forms;
}

/// @brief 元の機体設定から、形態の機体設定を作る。
inline PlayerClassConfig BuildFormConfig(const PlayerClassConfig& source, const FormDefinition& form, FormId id)
{
    PlayerClassConfig config = source;
    config.id = id == FormId::Twin ? "SurvivorTwin" : "SurvivorLongBarrel";
    config.displayName = form.name;
    config.requiredRank = 2;
    config.usesDrone = false;
    config.reloadScale = form.reloadScale;
    config.bulletDamageScale = form.damageScale;
    config.bulletSpeedScale = form.speedScale;
    if (id == FormId::LongBarrel) {
        // 1本の長い砲身。まっすぐ飛ばす。
        if (config.barrels.size() > 1) {
            config.barrels.resize(1);
        }
        config.randomSpread = false;
        config.spreadAngleDeg = 0.0f;
        config.bodyShape = PlayerBodyShape::Pentagon;
    } else {
        config.bodyShape = PlayerBodyShape::Box;
    }
    for (auto& barrel : config.barrels) {
        barrel.scale.x *= form.barrelLength;
        barrel.muzzleForward *= form.barrelLength;
        if (id == FormId::LongBarrel) {
            barrel.offset.y = 0.0f;
            barrel.angleDeg = 0.0f;
        }
    }
    config.specialActionId = form.railCharge ? "rail_charge" : "perfect_dodge";
    return config;
}

/// @brief 形態の札の説明文。
inline std::string DescribeForm(const FormDefinition& form, FormId id)
{
    char numbers[160]{};
    std::snprintf(numbers, sizeof(numbers), "威力 x%.2f　発射間隔 x%.2f\n弾速 x%.2f　熱 x%.2f", form.damageScale, form.reloadScale,
                  form.speedScale, form.heatScale);
    std::string text = id == FormId::Twin ? "砲が2本になり、同時に撃つ\n" : "長い砲身で、速く遠くまで届く弾\n";
    text += numbers;
    text += form.railCharge ? "\n右長押し：ため撃ち（貫通・熱）\nダッシュは Shift" : "\n右クリック・Shift：ダッシュ";
    return text;
}
} // namespace survivor
