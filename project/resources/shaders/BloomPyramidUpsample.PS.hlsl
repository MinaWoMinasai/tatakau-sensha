#include "BloomPyramidCommon.hlsli"

float4 main(PSInput input) : SV_TARGET
{
    uint width, height;
    sourceTex.GetDimensions(width, height);
    float2 texel = radius / float2(width, height);
    float2 uv = input.uv;
    // A normalized separable [1,2,1] tent. Mix scales instead of summing them,
    // preventing an intensity increase simply because Quality has more levels.
    float4 coarse = sourceTex.SampleLevel(linearClamp, uv, 0) * .25f;
    coarse += (sourceTex.SampleLevel(linearClamp, uv + float2(0, -1) * texel, 0)
        + sourceTex.SampleLevel(linearClamp, uv + float2(-1, 0) * texel, 0)
        + sourceTex.SampleLevel(linearClamp, uv + float2(1, 0) * texel, 0)
        + sourceTex.SampleLevel(linearClamp, uv + float2(0, 1) * texel, 0)) * .125f;
    coarse += (sourceTex.SampleLevel(linearClamp, uv + float2(-1, -1) * texel, 0)
        + sourceTex.SampleLevel(linearClamp, uv + float2(1, -1) * texel, 0)
        + sourceTex.SampleLevel(linearClamp, uv + float2(-1, 1) * texel, 0)
        + sourceTex.SampleLevel(linearClamp, uv + float2(1, 1) * texel, 0)) * .0625f;
    float4 fine = auxiliaryTex.SampleLevel(linearClamp, uv, 0);
    return lerp(fine, coarse, scatter);
}
