#pragma once
#include <array>
#include <string_view>

namespace tankbuild {
enum class Style {
    Shooter,
    Drone,
    Melee
};
inline constexpr unsigned AllStyles = 7;
inline constexpr std::array<const char*, 3> Names{"シューター", "ドローン", "近接"};
inline constexpr std::array<const char*, 3> Ids{"shooter", "drone", "melee"};
inline constexpr std::array<const char*, 5> RarityNames{"コモン", "アンコモン", "レア", "エピック", "レジェンダリー"};
inline constexpr std::array<unsigned, 5> RarityWeights{60, 26, 10, 3, 1};
/// @brief 対象の識別子や値が利用条件を満たすか判定する。
inline constexpr bool Valid(Style style)
{
    return style >= Style::Shooter && style <= Style::Melee;
}
/// @brief 強化項目などの分類をビットマスクとして返す。
inline constexpr unsigned Mask(Style style)
{
    return Valid(style) ? 1u << static_cast<unsigned>(style) : 0u;
}
/// @brief 表示用の名前を返す。
inline constexpr const char* Name(Style style)
{
    return Valid(style) ? Names[static_cast<unsigned>(style)] : "不明";
}
/// @brief 保存と参照に使う識別子を返す。
inline constexpr const char* Id(Style style)
{
    return Valid(style) ? Ids[static_cast<unsigned>(style)] : "invalid";
}
/// @brief 保存された識別子を戦闘系統へ変換する。
inline bool ParseStyle(std::string_view id, Style& output)
{
    for (unsigned i = 0; i < Ids.size(); ++i)
        if (id == Ids[i]) {
            output = static_cast<Style>(i);
            return true;
        }
    return false;
}
/// @brief レア度が利用可能な範囲内か判定する。
inline constexpr bool ValidRarity(int rarity)
{
    return rarity >= 0 && rarity < static_cast<int>(RarityNames.size());
}
/// @brief レア度に対応する表示名を返す。
inline constexpr const char* RarityName(int rarity)
{
    return ValidRarity(rarity) ? RarityNames[static_cast<unsigned>(rarity)] : "不明";
}
} // namespace tankbuild
