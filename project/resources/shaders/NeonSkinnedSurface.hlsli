#ifndef NEON_SKINNED_SURFACE_HLSLI
#define NEON_SKINNED_SURFACE_HLSLI

Texture2D<float4> gBaseColorTexture : register(t0);
// LinearData: R=線coverage、G=自動線の置換領域。B/Aは使用しない。
Texture2D<float4> gFeatureMaskTexture : register(t1);
SamplerState gSurfaceSampler : register(s0);
cbuffer NeonSubmeshConstants : register(b2)
{
    float gSubmeshLineStrength;
    float gAlphaCutoff;
    float gSubmeshGeometryStrength;
    float gSubmeshInternalThresholdScale;
};

float4 SampleNeonSurface(float2 uv, float2 uvDx, float2 uvDy)
{
    return gBaseColorTexture.SampleGrad(gSurfaceSampler, uv, uvDx, uvDy);
}

float2 SampleNeonFeatureMask(float2 uv, float2 uvDx, float2 uvDy)
{
    return saturate(gFeatureMaskTexture.SampleGrad(gSurfaceSampler, uv, uvDx, uvDy).rg);
}

float3 CompositeNeonInternalEmission(float internalLine, float2 mask)
{
    float3 automatic = max(gEmissiveColor, 0.0f) * max(gInternalLineIntensity, 0.0f) * internalLine;
    float3 authored = mask.r * max(gFeatureMaskColor, 0.0f) * max(gFeatureMaskIntensity, 0.0f);
    return lerp(automatic, authored, saturate(gFeatureMaskBlend * mask.g));
}

void ApplyNeonAlphaCutout(float alpha)
{
    // 任意のCutoutのみ。通常SkinningのMaterialやBLEND描画は変更しない。
    if (gAlphaCutoff > 0.0f) clip(alpha - gAlphaCutoff);
}

float4 FeatureSignal(float4 sampleColor)
{
    // SRGB SRVのlinear値を知覚的な差へ近似。暗い髪の描き込みも拾う。
    // 透明領域のRGBを抑え、Atlasの透明部分にある不要な色差を発光させない。
    return float4(sqrt(max(sampleColor.rgb, 0.0f)) * sampleColor.a, sampleColor.a);
}

float NeonInternalLine(float2 uv, float2 uvDx, float2 uvDy, float4 center)
{
    if (gInternalLineEnabled == 0 || gInternalLineWidthPixels <= 0.0f || gSubmeshLineStrength <= 0.0f)
        return 0.0f;
    float2 x = uvDx * gInternalLineWidthPixels;
    float2 y = uvDy * gInternalLineWidthPixels;
    float4 gradientX = abs(FeatureSignal(SampleNeonSurface(uv + x, uvDx, uvDy))
        - FeatureSignal(SampleNeonSurface(uv - x, uvDx, uvDy)));
    float4 gradientY = abs(FeatureSignal(SampleNeonSurface(uv + y, uvDx, uvDy))
        - FeatureSignal(SampleNeonSurface(uv - y, uvDx, uvDy)));
    float4 gradient = max(gradientX, gradientY);
    float contrast = max(max(gradient.x, gradient.y), max(gradient.z, gradient.w));
    float threshold = max(gInternalLineThreshold * gSubmeshInternalThresholdScale, 0.001f);
    return smoothstep(threshold, threshold * 1.8f, contrast) * saturate(center.a) * gSubmeshLineStrength;
}

#endif
