#ifndef NEON_SKINNED_SURFACE_HLSLI
#define NEON_SKINNED_SURFACE_HLSLI

Texture2D<float4> gBaseColorTexture : register(t0);
// LinearData: R=線coverage、G=自動線の置換領域。B/Aは使用しない。
Texture2D<float4> gFeatureMaskTexture : register(t1);
Texture2D<float4> gFeatureDistanceTexture : register(t2);
SamplerState gSurfaceSampler : register(s0);
cbuffer NeonSubmeshConstants : register(b2)
{
    float gSubmeshLineStrength;
    float gAlphaCutoff;
    float gSubmeshGeometryStrength;
    float gSubmeshInternalThresholdScale;
    uint gHasDistanceMask;
    uint3 gSubmeshPadding;
};

float4 SampleNeonSurface(float2 uv, float2 uvDx, float2 uvDy)
{
    return gBaseColorTexture.SampleGrad(gSurfaceSampler, uv, uvDx, uvDy);
}

float4 SampleNeonFeatureMask(float2 uv, float2 uvDx, float2 uvDy)
{
    return saturate(gFeatureMaskTexture.SampleGrad(gSurfaceSampler, uv, uvDx, uvDy));
}

float2 ReconstructNeonLineCoverage(float2 uv, float2 uvDx, float2 uvDy, float4 mask)
{
    float2 coverage = float2(mask.r, gFeatureMaskRenderMode == 0 ? 0.0f : mask.b);
    // These root-constant decisions are uniform across the draw. All sampling and distance
    // derivatives finish before alpha clip or any fragment-dependent branch in the body.
    if (gFeatureMaskRenderMode == 2 && gHasDistanceMask != 0)
    {
        // Average distance MIPs do not preserve thin stroke regions. Reconstruct from level 0
        // and crossfade to separately filtered area coverage as the footprint gets smaller.
        float4 distanceSample = gFeatureDistanceTexture.SampleLevel(gSurfaceSampler, uv, 0);
        float distanceTexels = (distanceSample.r - 0.5f) * (2.0f * gSdfRangeTexels);
        float distancePerPixel = max(length(float2(ddx(distanceTexels), ddy(distanceTexels))), 0.125f);
        float core = smoothstep(-0.5f * distancePerPixel, 0.5f * distancePerPixel, distanceTexels)
            * saturate(distanceSample.b);
        float halo = saturate(1.0f + min(distanceTexels, 0.0f) / max(gSdfHaloWidthTexels, 0.125f));
        halo = gSdfHaloWidthTexels > 0.0f ? halo * halo * saturate(distanceSample.b) : 0.0f;
        uint width, height;
        gFeatureMaskTexture.GetDimensions(width, height);
        float2 textureSize = float2(width, height);
        float footprint = max(length(uvDx * textureSize), length(uvDy * textureSize));
        float lod = log2(max(footprint, 1.0e-6f));
        float minification = smoothstep(gSdfLodBlendStart, max(gSdfLodBlendEnd, gSdfLodBlendStart + 0.01f), lod);
        coverage = lerp(float2(core, halo), coverage, minification);
    }
    if (gFeatureMaskRenderMode == 2 && gSdfHaloWidthTexels <= 0.0f) coverage.y = 0.0f;
    return coverage;
}

struct NeonLineEmission
{
    float3 core;
    float3 halo;
};

NeonLineEmission CompositeNeonInternalEmission(float internalLine, float4 mask, float2 coverage)
{
    float3 automatic = max(gEmissiveColor, 0.0f) * max(gInternalLineIntensity, 0.0f) * internalLine;
    float3 authored = coverage.x * max(gFeatureMaskColor, 0.0f) * max(gFeatureMaskIntensity, 0.0f);
    float3 halo = 0.0f;
    if (gFeatureMaskRenderMode != 0 && gSplitLineEmission != 0)
    {
        authored = coverage.x * max(gLineCoreColor, 0.0f) * max(gLineCoreIntensity, 0.0f);
        halo = coverage.y * max(gLineHaloColor, 0.0f) * max(gLineHaloIntensity, 0.0f);
    }
    float replace = saturate(gFeatureMaskBlend * mask.g);
    NeonLineEmission result;
    result.core = lerp(automatic, authored, replace);
    result.halo = halo * replace;
    return result;
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
