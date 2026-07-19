Texture2D ssaoTex : register(t0);
Texture2D<float> depthTex : register(t2);
Texture2D normalTex : register(t3);
Texture2D materialTex : register(t5);
SamplerState samp : register(s0);

#include "PostEffectCommon.hlsli"

float SampleAO(float2 uv)
{
    return ssaoTex.Sample(samp, saturate(uv)).r;
}

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = input.uv;
    float centerAO = SampleAO(uv);
    if (ssaoDenoiseEnabled <= 0.5f || ssaoDenoiseRadius <= 0.001f)
    {
        return float4(centerAO, centerAO, centerAO, 1.0f);
    }

    float centerDepth = depthTex.Sample(samp, uv);
    if (centerDepth >= 0.99999f)
    {
        return float4(centerAO, centerAO, centerAO, 1.0f);
    }

    uint width;
    uint height;
    ssaoTex.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);

    float centerViewZ = RestoreViewSpaceZ(centerDepth);
    float3 centerNormal = DecodeNormalTarget(normalTex.Sample(samp, uv));
    float4 centerMaterial = materialTex.Sample(samp, uv);
    float centerRoughness = saturate(centerMaterial.r);
    float centerMetallic = saturate(centerMaterial.g);
    float centerClass = saturate(centerMaterial.a);
    float depthSigma = max(ssaoDenoiseDepthSigma, 0.001f);
    float normalSigma = max(ssaoDenoiseNormalSigma, 0.0f);
    float radius = max(ssaoDenoiseRadius, 0.0f) * lerp(0.75f, 1.25f, centerRoughness);

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

    float weightedAO = 0.0f;
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

        float sampleViewZ = RestoreViewSpaceZ(sampleDepth);
        float depthWeight = exp(-abs(sampleViewZ - centerViewZ) / depthSigma);
        float3 sampleNormal = DecodeNormalTarget(normalTex.Sample(samp, sampleUV));
        float normalWeight = pow(saturate(dot(centerNormal, sampleNormal)), normalSigma);
        float4 sampleMaterial = materialTex.Sample(samp, sampleUV);
        float roughnessWeight = 1.0f - saturate(abs(saturate(sampleMaterial.r) - centerRoughness) * 1.60f);
        float metallicWeight = 1.0f - saturate(abs(saturate(sampleMaterial.g) - centerMetallic) * 1.20f);
        float classWeight = 1.0f - saturate(abs(saturate(sampleMaterial.a) - centerClass) * 2.20f);
        float materialWeight = lerp(1.0f, roughnessWeight * metallicWeight * classWeight, 0.60f);
        float weight = spatialWeights[i] * depthWeight * normalWeight * materialWeight;

        weightedAO += SampleAO(sampleUV) * weight;
        totalWeight += weight;
    }

    if (totalWeight <= 0.0001f)
    {
        return float4(centerAO, centerAO, centerAO, 1.0f);
    }

    float filteredAO = weightedAO / totalWeight;
    float denoiseStrength = 0.85f;
    float ao = lerp(centerAO, filteredAO, denoiseStrength);
    return float4(ao, ao, ao, 1.0f);
}
