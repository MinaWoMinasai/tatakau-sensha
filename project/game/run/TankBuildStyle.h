#pragma once
#include <array>
#include <string_view>

namespace tankbuild {
enum class Style { Shooter, Drone, Melee };
inline constexpr unsigned AllStyles=7;
inline constexpr std::array<const char*,3> Names{"シューター","ドローン","近接"};
inline constexpr std::array<const char*,3> Ids{"shooter","drone","melee"};
inline constexpr std::array<const char*,5> RarityNames{"コモン","アンコモン","レア","エピック","レジェンダリー"};
inline constexpr std::array<unsigned,5> RarityWeights{60,26,10,3,1};
inline constexpr bool Valid(Style style) {return style>=Style::Shooter&&style<=Style::Melee;}
inline constexpr unsigned Mask(Style style) {return Valid(style)?1u<<static_cast<unsigned>(style):0u;}
inline constexpr const char* Name(Style style) {return Valid(style)?Names[static_cast<unsigned>(style)]:"不明";}
inline constexpr const char* Id(Style style) {return Valid(style)?Ids[static_cast<unsigned>(style)]:"invalid";}
inline bool ParseStyle(std::string_view id,Style& output) {
    for(unsigned i=0;i<Ids.size();++i)if(id==Ids[i]) {output=static_cast<Style>(i);return true;}
    return false;
}
inline constexpr bool ValidRarity(int rarity) {return rarity>=0&&rarity<static_cast<int>(RarityNames.size());}
inline constexpr const char* RarityName(int rarity) {return ValidRarity(rarity)?RarityNames[static_cast<unsigned>(rarity)]:"不明";}
}
