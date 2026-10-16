#include "BloomPyramidCommon.hlsli"

float4 main(PSInput input) : SV_TARGET
{
    uint width, height;
    sourceTex.GetDimensions(width, height);
    float2 texel = radius / float2(width, height);
    float2 uv = input.uv;
    // 正規化した13タップの範囲で、対称かつエネルギーを保存する。フィルタ内では係数を
    // 掛けないため、一定の入力は各レベルでも一定のまま。
    float4 sum = sourceTex.SampleLevel(linearClamp, uv, 0) * .125f;
    sum += (sourceTex.SampleLevel(linearClamp, uv + float2(-2, -2) * texel, 0)
        + sourceTex.SampleLevel(linearClamp, uv + float2(2, -2) * texel, 0)
        + sourceTex.SampleLevel(linearClamp, uv + float2(-2, 2) * texel, 0)
        + sourceTex.SampleLevel(linearClamp, uv + float2(2, 2) * texel, 0)) * .03125f;
    sum += (sourceTex.SampleLevel(linearClamp, uv + float2(0, -2) * texel, 0)
        + sourceTex.SampleLevel(linearClamp, uv + float2(-2, 0) * texel, 0)
        + sourceTex.SampleLevel(linearClamp, uv + float2(2, 0) * texel, 0)
        + sourceTex.SampleLevel(linearClamp, uv + float2(0, 2) * texel, 0)) * .0625f;
    sum += (sourceTex.SampleLevel(linearClamp, uv + float2(-1, -1) * texel, 0)
        + sourceTex.SampleLevel(linearClamp, uv + float2(1, -1) * texel, 0)
        + sourceTex.SampleLevel(linearClamp, uv + float2(-1, 1) * texel, 0)
        + sourceTex.SampleLevel(linearClamp, uv + float2(1, 1) * texel, 0)) * .125f;
    return sum;
}
