#pragma once
#include "TankExpeditionDirector.h"
#include <array>
#include "TankExpeditionBalance.h"

namespace tankexp {
/// @brief 1つの敵出現グループの種類・数・配置条件を表す。
struct EncounterUnit {
    const char* prefab;
    float x;
    float y;
    int hp;
};
/// @brief 部屋で発生する敵出現グループと進行条件を表す。
struct Encounter {
    std::array<EncounterUnit, 6> units{};
    int count = 0;
    const char* hint = "";
};
/// @brief 基準敵出現を返す。
inline Encounter GetBaseEncounter(RoomKind room)
{
    switch (room) {
    case RoomKind::Skirmish:
        return {{{{"Charger", 38, 22, 24}, {"Charger", 50, 34, 24}}}, 2, "赤い突進は横へ回避 / 金色の照準線が止まったら射線の外へ"};
    case RoomKind::Resource:
        return {{{{"Charger", 37, 35, 30}, {"Sniper", 52, 23, 34}}}, 2, "護衛を倒すとコアが出現 / ライバルより先に確保"};
    case RoomKind::Elite:
        return {
            {{{"Charger", 37, 21, 38}, {"Charger", 45, 37, 38}, {"Charger", 59, 32, 38}, {"Sniper", 52, 22, 42}, {"Sniper", 61, 38, 42}}},
            5,
            "突進と狙撃の混成部隊 / 同じ場所に留まらず各個撃破"};
    case RoomKind::Reflection:
        return {{{{"Sniper", 36, 20, 34}, {"Sniper", 60, 22, 34}, {"Sniper", 54, 38, 34}, {"Charger", 43, 35, 32}}},
                4,
                "遮蔽物で狙撃を切る / 反射弾で曲がり角の敵を狙おう"};
    case RoomKind::Drone:
        return {{{{"Charger", 36, 35, 32}, {"Charger", 46, 23, 32}, {"Charger", 57, 37, 32}, {"Sniper", 60, 22, 36}}},
                4,
                "本体が突進を引きつけ、回り込んで反撃 / ドローンにも指示を"};
    case RoomKind::Guard:
        return {{{{"Charger", 36, 22, 34}, {"Charger", 53, 36, 34}, {"Sniper", 58, 20, 38}, {"Sniper", 38, 38, 38}}},
                4,
                "3つの装置と護衛を撃破 / 狙撃の射線を避けて巡回"};
    case RoomKind::Boss:
        return {{{{"Charger", 40, 21, 32}, {"Charger", 48, 38, 32}}}, 2, "護衛を先に崩す / ボスの予告を見てダッシュで抜けよう"};
    }
    return {};
}
/// @brief 敵出現を返す。
inline Encounter GetEncounter(RoomKind room, int roomIndex = 0)
{
    auto result = GetBaseEncounter(room);
    const auto scale = GetRoomBalance(roomIndex);
    for (int i = 0; i < result.count; ++i)
        result.units[i].hp = (std::max)(4, static_cast<int>(std::round(result.units[i].hp * scale.hpScale)));
    if (roomIndex >= 2 && result.count < 6)
        result.units[result.count++] = {"Shooter", 48, 28, static_cast<int>(28 * scale.hpScale)};
    if (roomIndex >= 3 && room != RoomKind::Boss && result.count < 6)
        result.units[result.count++] = {"Sniper", 62, 34, static_cast<int>(34 * scale.hpScale)};
    return result;
}
// Passive salvage never blocks a combat room. A core objective cannot bypass
// its guards, while the final rival remains the boss-room objective.
/// @brief 部屋Objective完了であるか判定する。
inline bool IsRoomObjectiveComplete(RoomKind room, int threats, int nodes, bool coreClaimed, bool bossDead)
{
    if (room == RoomKind::Boss)
        return bossDead;
    if (room == RoomKind::Resource)
        return coreClaimed;
    if (room == RoomKind::Guard)
        return nodes >= 3 && threats == 0;
    return threats == 0;
}
} // namespace tankexp
