Texture2D<float> depthTex : register(t2);
SamplerState samp : register(s0);

#include "PostEffectCommon.hlsli"

float SafeInvW(float w)
{
    return abs(w) > 0.0001f ? rcp(w) : 0.0f;
}

float2 ProjectClipToUV(float4 clipPosition)
{
    clipPosition *= SafeInvW(clipPosition.w);
    return float2(
        clipPosition.x * 0.5f + 0.5f,
        0.5f - clipPosition.y * 0.5f);
}

float4 main(PSInput input) : SV_TARGET
{
    if (motionVectorEnabled <= 0.5f)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    if ((renderDebugMode > 14.5f && renderDebugMode < 15.5f) || motionVectorScale < -0.5f)
    {
        float2 diagnosticUV = saturate(input.uv);
        return float4(diagnosticUV.x, diagnosticUV.y, 0.0f, 1.0f);
    }

    float depth = depthTex.Sample(samp, input.uv);
    depth = min(depth, 1.0f);

    float4 currentClip = float4(
        input.uv.x * 2.0f - 1.0f,
        1.0f - input.uv.y * 2.0f,
        depth,
        1.0f);
    float4 worldPosition = mul(currentClip, motionInverseCurrentViewProjection);
    worldPosition *= SafeInvW(worldPosition.w);

    float4 previousClip = mul(float4(worldPosition.xyz, 1.0f), motionPreviousViewProjection);
    float2 previousUV = ProjectClipToUV(previousClip);

    float2 velocity = (input.uv - previousUV) * max(motionVectorScale, 0.0f);
    velocity = clamp(velocity, -1.0f.xx, 1.0f.xx);

    return float4(velocity, 0.0f, 1.0f);
}
