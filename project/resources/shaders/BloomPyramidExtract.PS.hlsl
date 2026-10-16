#include "BloomPyramidCommon.hlsli"

float4 ExtractSample(uint2 coordinate)
{
    coordinate = min(coordinate, uint2(sourceWidth - 1, sourceHeight - 1));
    float4 source = sourceTex.Load(int3(coordinate, 0));
    float3 color = max(source.rgb, 0.0f);
    if (materialEnabled != 0)
    {
        float alpha = auxiliaryTex.Load(int3(coordinate, 0)).a;
        float mask = saturate(smoothstep(.70f, .735f, alpha)
            * (1.0f - smoothstep(.765f, .80f, alpha)) * waterDiagnosticsEnabled);
        color *= lerp(1.0f, waterBloomEnabled, mask);
    }
    // 彩度の高いマゼンタ/シアンは、輝度が低くても明るいネオンの光源として扱う。
    float brightness = max(color.r, max(color.g, color.b));
    float knee = threshold * softKnee;
    float soft = clamp(brightness - threshold + knee, 0.0f, 2.0f * knee);
    soft = soft * soft / max(4.0f * knee, 0.00001f);
    float contribution = max(soft, brightness - threshold) / max(brightness, 0.00001f);
    return float4(color * contribution, source.a * saturate(contribution));
}

float4 main(PSInput input) : SV_TARGET
{
    // 平均を取る前に、元の各ピクセルでしきい値を判定する。これにより、半解像度の
    // ピクセル範囲の4つの位相のどこにあっても、小さく明るい光源が残る。
    uint2 base = uint2(input.position.xy) * 2;
    return (ExtractSample(base) + ExtractSample(base + uint2(1, 0))
        + ExtractSample(base + uint2(0, 1)) + ExtractSample(base + uint2(1, 1))) * .25f;
}
