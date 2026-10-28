#pragma once

namespace gameplay {
/// @brief 1回のプレイの種類。場面の登録時に決まり、プレイ中は変わらない。
enum class RunMode {
    Classic,    // 旧来の戦闘（レベルデータとボスの段階）。今は登録された場面が無い
    Arena,      // コア争奪戦（RunDirector のカードと資材）。今は登録された場面が無い
    Expedition, // 遠征（マップ・部屋・強化工房）。TANK_EXPEDITION とタイトルのデモ
};
} // namespace gameplay
