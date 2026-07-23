Texture2D sceneTex : register(t0);
Texture2D materialTex : register(t5);
SamplerState samp : register(s0);

#include "PostEffectCommon.hlsli"

float4 main(PSInput input) : SV_TARGET
{
    float4 source = sceneTex.Sample(samp, input.uv);
    float3 color = source.rgb;
    float waterMask = GetGraphicsLabWaterMask(materialTex.Sample(samp, input.uv));
    color *= lerp(1.0f, saturate(waterBloomEnabled), waterMask);

    float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    float epsilon = 0.0001f;

    float contribution = max(0.0f, luminance - threshold);
    contribution /= max(luminance, epsilon);

    float3 extractColor = color * contribution;
    return float4(extractColor, source.a * saturate(contribution));
}
