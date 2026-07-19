Texture2D ssrTex : register(t0);
Texture2D<float> depthTex : register(t2);
Texture2D normalTex : register(t3);
Texture2D materialTex : register(t5);
SamplerState samp : register(s0);

#include "PostEffectCommon.hlsli"

float4 SampleClampedSSR(float2 uv)
{
    return ssrTex.Sample(samp, saturate(uv));
}

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = input.uv;
    float4 centerSSR = SampleClampedSSR(uv);
    if (ssrDenoiseEnabled <= 0.5f || ssrDenoiseRadius <= 0.001f)
    {
        return centerSSR;
    }

    float centerDepth = depthTex.Sample(samp, uv);
    if (centerDepth >= 0.99999f)
    {
        return centerSSR;
    }

    uint width;
    uint height;
    ssrTex.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);

    float centerViewZ = RestoreViewSpaceZ(centerDepth);
    float3 centerNormal = DecodeNormalTarget(normalTex.Sample(samp, uv));
    float4 centerMaterial = materialTex.Sample(samp, uv);
    float centerRoughness = saturate(centerMaterial.r);
    float centerMetallic = saturate(centerMaterial.g);
    float centerClass = saturate(centerMaterial.a);
    float radius = max(ssrDenoiseRadius, 0.0f) * lerp(0.60f, 1.45f, smoothstep(0.10f, 0.88f, centerRoughness));
    float depthSigma = max(ssrDenoiseDepthSigma, 0.001f);
    float normalSigma = max(ssrDenoiseNormalSigma, 0.0f);

    const float2 offsets[9] =
    {
        float2( 0.0f,  0.0f),
        float2( 1.0f,  0.0f),
        float2(-1.0f,  0.0f),
        float2( 0.0f,  1.0f),
        float2( 0.0f, -1.0f),
        float2( 1.0f,  1.0f),
        float2(-1.0f,  1.0f),
        float2( 1.0f, -1.0f),
        float2(-1.0f, -1.0f)
    };

    const float spatialWeights[9] =
    {
        0.34f,
        0.12f, 0.12f, 0.12f, 0.12f,
        0.045f, 0.045f, 0.045f, 0.045f
    };

    float4 weightedSSR = 0.0f;
    float totalWeight = 0.0f;

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        float2 sampleUV = uv + offsets[i] * texelSize * radius;
        if (any(sampleUV < 0.0f) || any(sampleUV > 1.0f))
        {
            continue;
        }

        float sampleDepth = depthTex.Sample(samp, sampleUV);
        if (sampleDepth >= 0.99999f)
        {
            continue;
        }

        float4 sampleSSR = ssrTex.Sample(samp, sampleUV);
        float sampleConfidence = saturate(sampleSSR.a);
        float sampleViewZ = RestoreViewSpaceZ(sampleDepth);
        float depthWeight = exp(-abs(sampleViewZ - centerViewZ) / depthSigma);
        float3 sampleNormal = DecodeNormalTarget(normalTex.Sample(samp, sampleUV));
        float normalWeight = pow(saturate(dot(centerNormal, sampleNormal)), normalSigma);
        float4 sampleMaterial = materialTex.Sample(samp, sampleUV);
        float roughnessWeight = 1.0f - saturate(abs(saturate(sampleMaterial.r) - centerRoughness) * 1.75f);
        float metallicWeight = 1.0f - saturate(abs(saturate(sampleMaterial.g) - centerMetallic) * 1.40f);
        float classWeight = 1.0f - saturate(abs(saturate(sampleMaterial.a) - centerClass) * 2.50f);
        float materialWeight = roughnessWeight * metallicWeight * classWeight;
        float weight = spatialWeights[i] * depthWeight * normalWeight * materialWeight * max(sampleConfidence, 0.05f);

        weightedSSR += sampleSSR * weight;
        totalWeight += weight;
    }

    if (totalWeight <= 0.0001f)
    {
        return centerSSR;
    }

    float4 filteredSSR = weightedSSR / totalWeight;
    float centerKeep = saturate(centerSSR.a);
    float fillStrength = saturate(1.0f - centerKeep) * 0.35f;
    float denoiseStrength = saturate(0.70f + fillStrength);
    return lerp(centerSSR, filteredSSR, denoiseStrength);
}
