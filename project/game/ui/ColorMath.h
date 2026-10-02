#pragma once
#include "Struct.h"

namespace tankui {
// Preserve HDR RGB values; only scale the authored opacity.
inline cg2::Vector4 ScaleAlpha(const cg2::Vector4& color, float scale) {
    return {color.x, color.y, color.z, color.w * scale};
}
}
