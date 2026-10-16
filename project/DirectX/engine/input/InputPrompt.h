#pragma once
#include "Input.h"
#include <string>
#include <utility>

namespace cg2 {

/// @brief 画面の操作説明を、最後に操作された入力機器の表記へ置き換える。
/// キーボード・マウスでは元の文字列のまま。ゲームパッドでは下の表を上から順に置き換える
/// （長い言い回しを先に置き換え、短い語が途中で書き換わらないようにする）。
/// 割り当ては Input::TranslateGamepad と対応させること。
inline std::string ApplyInputPrompts(std::string text)
{
    if (!Input::GetInstance()->IsGamepadActive())
        return text;
    static const std::pair<const char*, const char*> kGamepadPrompts[] = {
        {"数字キー / クリック: 決定    ← → + Enter: 選択", "A: 決定    十字キー: 選択"},
        {"数字キー / クリック: 決定   ← → + Enter: 選択", "A: 決定   十字キー: 選択"},
        {"数字 / クリック: 決定   ← → + Enter: 選択", "A: 決定   十字キー: 選択"},
        {"数字キー / クリック / ← → + Enter", "十字キー + A"},
        {"数字キー / 左クリック: 強化    Shift + 数字 / 右クリック: 1段階戻す", "A: 強化    LB + A: 1段階戻す"},
        {"整備が済んだら E で改造", "整備が済んだら X で改造"},
        {"E / クリック", "X"},
        {"W/S or Arrow Keys : Select   Enter/Click : Confirm", "D-Pad : Select   A : Confirm"},
        {"Esc: 一時停止", "START: 一時停止"},
        {"Esc: ゲームに戻る", "START: ゲームに戻る"},
        {"ESC タイトルへ", "START タイトルへ"},
        {"[Tab / Escで閉じる]", "[VIEW / Bで閉じる]"},
        {"[TAB / ESC で閉じる]", "[VIEW / B で閉じる]"},
        {"TAB 詳細", "VIEW 詳細"},
        {"戦闘中に TAB", "戦闘中に VIEW"},
        {"SELECT + ENTER", "十字キー + A"},
        {"購入せず進む", "Y 購入せず進む"},
        {"左クリックで続ける", "Aで続ける"},
        {"左クリック長押し", "RT長押し"},
        {"左クリック中", "RT中"},
        {"左クリック", "RT"},
        {"右クリック", "LT"},
        {"クリック", "A"},
        {"WASD", "左スティック"},
        {"マウス", "右スティック"},
        {"カーソル", "右スティック"},
        {"Enter", "A"},
        {"ENTER", "A"},
        {"ESC", "B"},
        {"Esc", "B"},
    };
    for (const auto& [from, to] : kGamepadPrompts) {
        const std::string source(from), target(to);
        for (size_t at = text.find(source); at != std::string::npos; at = text.find(source, at + target.size()))
            text.replace(at, source.size(), target);
    }
    return text;
}

} // namespace cg2
