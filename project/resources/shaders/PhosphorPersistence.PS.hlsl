Texture2D currentSceneTex : register(t0);
Texture2D persistenceHistoryTex : register(t1);
Texture2D motionVectorTex : register(t7);
SamplerState samp : register(s0);

#include "PostEffectCommon.hlsli"

// Phosphor persistence: bright neon leaves a short, decaying afterglow like a vector CRT.
// History is reprojected with the camera motion so static grid lines do not smear while scrolling;
// moving actors and bullets keep a world-space trail behind them.
float4 main(PSInput input) : SV_TARGET
{
    float2 uv = input.uv;
    float3 currentColor = currentSceneTex.Sample(samp, uv).rgb;
    if (arenaPhosphorDecay <= 0.0f)
    {
        return float4(currentColor, 1.0f);
    }

    float2 historyUV = uv - motionVectorTex.Sample(samp, uv).rg;
    if (any(historyUV < 0.0f) || any(historyUV > 1.0f))
    {
        return float4(currentColor, 1.0f);
    }

    float3 history = persistenceHistoryTex.Sample(samp, historyUV).rgb * saturate(arenaPhosphorDecay);
    // Only emissive energy persists; the dark floor and bodies never accumulate a veil.
    float historyPeak = max(max(history.r, history.g), history.b);
    history *= smoothstep(0.18f, 0.65f, historyPeak);
    return float4(max(currentColor, history), 1.0f);
}
