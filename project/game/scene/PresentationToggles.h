#pragma once
#include <cstdint>

/// @brief シーンをまたいで保つ、遊ぶ人が切り替える表示の設定（Release でも使う）。
namespace PresentationToggles {

/// @brief 真なら、画面全体と物ごとのブルームをすべて切る（F12）。
inline bool bloomOff = false;
/// @brief 切り替えた回数。ゲーム中の画面がこれの変化を見て、切り替えたことを短く表示する。
inline uint32_t bloomSerial = 0;

/// @brief ブルームのオンとオフを切り替える。
inline void ToggleBloom()
{
    bloomOff = !bloomOff;
    ++bloomSerial;
}

} // namespace PresentationToggles
