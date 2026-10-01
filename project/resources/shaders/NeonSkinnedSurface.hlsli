#ifndef NEON_SKINNED_SURFACE_HLSLI
#define NEON_SKINNED_SURFACE_HLSLI

Texture2D<float4> gBaseColorTexture : register(t0);
SamplerState gSurfaceSampler : register(s0);
cbuffer NeonSubmeshConstants : register(b2)
{
    float gSubmeshLineStrength;
    float gAlphaCutoff;
};

float4 SampleNeonSurface(float2 uv, float2 uvDx, float2 uvDy)
{
    return gBaseColorTexture.SampleGrad(gSurfaceSampler, uv, uvDx, uvDy);
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
    float threshold = max(gInternalLineThreshold, 0.001f);
    return smoothstep(threshold, threshold * 1.8f, contrast) * saturate(center.a) * gSubmeshLineStrength;
}

#endif
