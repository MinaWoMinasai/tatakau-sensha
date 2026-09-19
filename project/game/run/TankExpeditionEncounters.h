#pragma once
#include "TankExpeditionDirector.h"
#include <array>

namespace tankexp {
struct EncounterUnit { const char* prefab; float x; float y; int hp; };
struct Encounter {
    std::array<EncounterUnit,6> units{};
    int count=0;
    const char* hint="";
};
inline Encounter GetEncounter(RoomKind room) {
    switch(room) {
    case RoomKind::Skirmish:
        return {{{{"Charger",38,22,24},{"Charger",46,36,24},{"Sniper",57,28,28}}},3,
            "赤い突進は横へ回避 / 金色の照準線が止まったら射線の外へ"};
    case RoomKind::Resource:
        return {{{{"Charger",37,35,30},{"Sniper",52,23,34}}},2,
            "護衛を倒すとコアが出現 / ライバルより先に確保"};
    case RoomKind::Elite:
        return {{{{"Charger",37,21,38},{"Charger",45,37,38},{"Charger",59,32,38},
            {"Sniper",52,22,42},{"Sniper",61,38,42}}},5,
            "突進と狙撃の混成部隊 / 同じ場所に留まらず各個撃破"};
    case RoomKind::Reflection:
        return {{{{"Sniper",36,20,34},{"Sniper",60,22,34},{"Sniper",54,38,34},
            {"Charger",43,35,32}}},4,
            "遮蔽物で狙撃を切る / 反射弾で曲がり角の敵を狙おう"};
    case RoomKind::Drone:
        return {{{{"Charger",36,35,32},{"Charger",46,23,32},{"Charger",57,37,32},
            {"Sniper",60,22,36}}},4,
            "本体が突進を引きつけ、回り込んで反撃 / ドローンにも指示を"};
    case RoomKind::Guard:
        return {{{{"Charger",36,22,34},{"Charger",53,36,34},
            {"Sniper",58,20,38},{"Sniper",38,38,38}}},4,
            "3つの装置と護衛を撃破 / 狙撃の射線を避けて巡回"};
    case RoomKind::Boss:
        return {{{{"Charger",40,21,32},{"Charger",48,38,32}}},2,
            "護衛を先に崩す / ボスの予告を見てダッシュで抜けよう"};
    }
    return {};
}
// Passive salvage never blocks a combat room. A core objective cannot bypass
// its guards, while the final rival remains the boss-room objective.
inline bool IsRoomObjectiveComplete(RoomKind room,int threats,int nodes,bool coreClaimed,bool bossDead) {
    if(room==RoomKind::Boss) return bossDead;
    if(room==RoomKind::Resource) return coreClaimed;
    if(room==RoomKind::Guard) return nodes>=3&&threats==0;
    return threats==0;
}
} // namespace tankexp
