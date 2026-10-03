#include "NeonSkinnedBody.hlsli"

PixelShaderOutput main(NeonSkinnedVertexOutput input, bool isFrontFace : SV_IsFrontFace)
{
    // 微分はclipより前に計算し、Cutout境界でも隣接pixelのLODを安定させる。
    float2 uvDx = ddx(input.texcoord);
    float2 uvDy = ddy(input.texcoord);
    return ShadeNeonBody(input, isFrontFace, uvDx, uvDy, 0.0f);
}
