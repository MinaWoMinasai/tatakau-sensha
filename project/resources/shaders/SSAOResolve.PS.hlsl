Texture2D<float> depthTex : register(t2);
Texture2D normalTex : register(t3);
Texture2D materialTex : register(t5);
SamplerState samp : register(s0);

#include "PostEffectCommon.hlsli"

float2 GetSSAOPoissonOffset(int index)
{
    if (index == 0) return float2(-0.94201624f, -0.39906216f);
    if (index == 1) return float2( 0.94558609f, -0.76890725f);
    if (index == 2) return float2(-0.09418410f, -0.92938870f);
    if (index == 3) return float2( 0.34495938f,  0.29387760f);
    if (index == 4) return float2(-0.91588581f,  0.45771432f);
    if (index == 5) return float2(-0.81544232f, -0.87912464f);
    if (index == 6) return float2(-0.38277543f,  0.27676845f);
    if (index == 7) return float2( 0.97484398f,  0.75648379f);
    if (index == 8) return float2( 0.44323325f, -0.97511554f);
    if (index == 9) return float2( 0.53742981f, -0.47373420f);
    if (index == 10) return float2(-0.26496911f, -0.41893023f);
    if (index == 11) return float2( 0.79197514f,  0.19090188f);
    if (index == 12) return float2(-0.24188840f,  0.99706507f);
    if (index == 13) return float2(-0.81409955f,  0.91437590f);
    if (index == 14) return float2( 0.19984126f,  0.78641367f);
    return float2(0.14383161f, -0.14100790f);
}

float GetMaterialAOReceiveScale(float4 materialSample)
{
    float roughness = saturate(materialSample.r);
    float materialClass = saturate(materialSample.a);

    float waterLike = 1.0f - saturate(abs(materialClass - 0.65f) * 8.0f);
    float receiveScale = lerp(1.0f, 0.18f, waterLike);
    receiveScale = lerp(receiveScale, receiveScale * 1.08f, roughness);
    return saturate(receiveScale);
}

float ComputeSSAO(float2 uv)
{
    if (ssaoEnabled <= 0.5f)
    {
        return 1.0f;
    }

    float centerDepth = depthTex.Sample(samp, uv);
    if (centerDepth >= 0.99999f)
    {
        return 1.0f;
    }

    uint width;
    uint height;
    depthTex.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);

    float centerViewZ = RestoreViewSpaceZ(centerDepth);
    float3 centerNormal = DecodeNormalTarget(normalTex.Sample(samp, uv));
    float4 centerMaterial = materialTex.Sample(samp, uv);
    float radiusPixels = max(ssaoRadius, 0.0f);
    float distanceFalloff = max(ssaoDistanceFalloff, 0.001f);
    int sampleCount = clamp((int)ssaoSampleCount, 1, 16);

    float randomAngle = Hash(floor(uv * float2(width, height)) * 0.25f) * 6.2831853f;
    float s = sin(randomAngle);
    float c = cos(randomAngle);
    float2x2 rotation = float2x2(c, -s, s, c);

    float occlusion = 0.0f;
    float validSamples = 0.0f;

    [unroll]
    for (int i = 0; i < 16; ++i)
    {
        if (i >= sampleCount)
        {
            continue;
        }

        float sampleStep = ((float)i + 0.5f) / (float)sampleCount;
        float2 offset = mul(GetSSAOPoissonOffset(i), rotation) * (0.35f + sampleStep * 0.65f);
        float2 sampleUV = uv + offset * texelSize * radiusPixels;
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
        float depthDelta = centerViewZ - sampleViewZ;
        float frontWeight = smoothstep(ssaoBias, ssaoBias + distanceFalloff * 0.18f, depthDelta);
        float rangeWeight = 1.0f - saturate(abs(depthDelta) / distanceFalloff);

        float3 sampleNormal = DecodeNormalTarget(normalTex.Sample(samp, sampleUV));
        float normalDifference = 1.0f - saturate(dot(centerNormal, sampleNormal));
        float normalWeight = lerp(1.0f, normalDifference, saturate(ssaoNormalInfluence));

        float4 sampleMaterial = materialTex.Sample(samp, sampleUV);
        float classWeight = 1.0f - saturate(abs(sampleMaterial.a - centerMaterial.a) * 2.5f);
        float roughnessWeight = 1.0f - saturate(abs(sampleMaterial.r - centerMaterial.r) * 1.5f);
        float materialWeight = lerp(1.0f, roughnessWeight * classWeight, 0.40f);

        occlusion += frontWeight * rangeWeight * normalWeight * materialWeight;
        validSamples += 1.0f;
    }

    if (validSamples <= 0.0f)
    {
        return 1.0f;
    }

    float ao = 1.0f - saturate((occlusion / validSamples) * max(ssaoIntensity, 0.0f));
    ao = pow(saturate(ao), max(ssaoPower, 0.001f));
    return lerp(1.0f, ao, GetMaterialAOReceiveScale(centerMaterial));
}

float4 main(PSInput input) : SV_TARGET
{
    if (any(input.uv < 0.0f) || any(input.uv > 1.0f))
    {
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    float ao = ComputeSSAO(input.uv);
    return float4(ao, ao, ao, 1.0f);
}
