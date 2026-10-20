#pragma once

// flecs.h を読み込む。エンジンでは flecs.h を直接 include せず、このヘッダーを通す。
// Windows.h の min / max マクロが flecs の C++ API（std::numeric_limits<...>::min() など）を壊すので、
// 読み込む間だけ退避する。エンジン全体に NOMINMAX を定義すると既存のコードに影響するため、ここで閉じる。
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "flecs.h"
#pragma pop_macro("max")
#pragma pop_macro("min")
