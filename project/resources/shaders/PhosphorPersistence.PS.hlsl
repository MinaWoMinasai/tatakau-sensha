Texture2D currentSceneTex : register(t0);
Texture2D persistenceHistoryTex : register(t1);
Texture2D motionVectorTex : register(t7);
SamplerState samp : register(s0);

#include "PostEffectCommon.hlsli"

// 蛍光体の残光: 明るいネオンがベクターCRTのように短く減衰する残光を残す。
// 履歴はカメラの動きに合わせて再投影するため、スクロール中も静止したグリッド線はにじまず、
// 動くアクターや弾はワールド空間での軌跡を後ろに残す。
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
    // 残るのは発光エネルギーだけで、暗い床や本体に幕が積み重なることはない。
    float historyPeak = max(max(history.r, history.g), history.b);
    history *= smoothstep(0.18f, 0.65f, historyPeak);
    // 指定された円の中は残光を出さない（自機に付いたゲージが、移動中に重なって見えないようにする）。
    if (arenaPersistenceMaskRadius > 0.0f)
    {
        float2 offset = (uv - float2(arenaPersistenceMaskX, arenaPersistenceMaskY)) * float2(arenaPersistenceMaskAspect, 1.0f);
        history *= smoothstep(arenaPersistenceMaskRadius * 0.85f, arenaPersistenceMaskRadius, length(offset));
    }
    return float4(max(currentColor, history), 1.0f);
}
