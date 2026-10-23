#pragma once
#include "Struct.h"

namespace tankui {
// HDR の RGB 値は保ち、制作時に指定した不透明度だけに倍率を掛ける。
/// @brief 不透明度に倍率を掛けた色を返す。
inline myEngine::Vector4 ScaleAlpha(const myEngine::Vector4& color, float scale)
{
    return {color.x, color.y, color.z, color.w * scale};
}
} // namespace tankui
