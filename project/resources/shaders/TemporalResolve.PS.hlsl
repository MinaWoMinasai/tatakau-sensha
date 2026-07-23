Texture2D currentSceneTex : register(t0);
Texture2D historySceneTex : register(t1);
Texture2D<float> depthTex : register(t2);
Texture2D normalTex : register(t3);
Texture2D materialTex : register(t5);
Texture2D motionVectorTex : register(t7);
SamplerState samp : register(s0);

#include "PostEffectCommon.hlsli"

float3 ClampHistoryToCurrentNeighborhood(float3 historyColor, float2 uv)
{
    uint width;
    uint height;
    currentSceneTex.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);

    float3 neighborhoodMin = 100000.0f.xxx;
    float3 neighborhoodMax = -100000.0f.xxx;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 sampleUV = saturate(uv + float2(x, y) * texelSize);
            float3 sampleColor = currentSceneTex.Sample(samp, sampleUV).rgb;
            neighborhoodMin = min(neighborhoodMin, sampleColor);
            neighborhoodMax = max(neighborhoodMax, sampleColor);
        }
    }

    return clamp(historyColor, neighborhoodMin, neighborhoodMax);
}

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = input.uv;
    float3 currentColor = currentSceneTex.Sample(samp, uv).rgb;

    if (temporalEnabled <= 0.5f || temporalHistoryValid <= 0.5f)
    {
        return float4(currentColor, 1.0f);
    }

    float depth = depthTex.Sample(samp, uv);
    if (depth >= 0.99999f)
    {
        return float4(currentColor, 1.0f);
    }

    float2 motionVector = motionVectorTex.Sample(samp, uv).rg;
    float2 historyUV = uv - motionVector;
    if (any(historyUV < 0.0f) || any(historyUV > 1.0f))
    {
        return float4(currentColor, 1.0f);
    }

    float3 historyColor = historySceneTex.Sample(samp, historyUV).rgb;
    historyColor = ClampHistoryToCurrentNeighborhood(historyColor, uv);

    float historyWeight = saturate(temporalBlendFactor);
    historyWeight *= 1.0f - saturate(length(motionVector) * max(temporalMotionRejection, 0.0f));
    float waterMask = GetGraphicsLabWaterMask(materialTex.Sample(samp, uv));
    float waterMaxHistory = waterTaaEnabled > 0.5f
        ? min(saturate(waterHistoryWeight), 0.10f)
        : 0.0f;
    historyWeight = lerp(
        historyWeight,
        min(historyWeight, waterMaxHistory),
        waterMask);

    return float4(lerp(currentColor, historyColor, historyWeight), 1.0f);
}
