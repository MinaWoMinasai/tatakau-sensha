Texture2D sceneTex : register(t0);
Texture2D<float> depthTex : register(t2);
Texture2D normalTex : register(t3);
Texture2D materialTex : register(t5);
SamplerState samp : register(s0);

#include "PostEffectCommon.hlsli"

float3 ReconstructViewPosition(float2 uv, float viewZ)
{
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    return float3(
        ndc.x * viewZ * ssrProjectionScale.x,
        ndc.y * viewZ * ssrProjectionScale.y,
        viewZ);
}

float2 ProjectViewPositionToUV(float3 viewPosition)
{
    float safeZ = max(viewPosition.z, 0.0001f);
    float2 ndc = float2(
        viewPosition.x / (safeZ * max(ssrProjectionScale.x, 0.0001f)),
        viewPosition.y / (safeZ * max(ssrProjectionScale.y, 0.0001f)));
    return float2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f);
}

float GetScreenEdgeFade(float2 uv)
{
    float edgeDistance = min(min(uv.x, 1.0f - uv.x), min(uv.y, 1.0f - uv.y));
    return saturate(edgeDistance / max(ssrEdgeFade, 0.0001f));
}

float3 SampleSSRReflectionColor(float2 uv, float materialMask, float materialRoughness)
{
    uint width;
    uint height;
    sceneTex.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);
    float roughnessBlur = smoothstep(0.08f, 0.92f, saturate(materialRoughness));
    float maskSharpness = 1.0f - saturate(materialMask) * 0.45f;
    float blurRadius = max(ssrBlurRadius, 0.0f) * (0.18f + roughnessBlur * 0.82f) * maskSharpness;
    if (blurRadius <= 0.001f)
    {
        return sceneTex.Sample(samp, uv).rgb;
    }

    float2 offset = texelSize * blurRadius;
    float3 sum = sceneTex.Sample(samp, uv).rgb * 0.40f;
    sum += sceneTex.Sample(samp, saturate(uv + float2( offset.x,  0.0f))).rgb * 0.15f;
    sum += sceneTex.Sample(samp, saturate(uv + float2(-offset.x,  0.0f))).rgb * 0.15f;
    sum += sceneTex.Sample(samp, saturate(uv + float2( 0.0f,  offset.y))).rgb * 0.15f;
    sum += sceneTex.Sample(samp, saturate(uv + float2( 0.0f, -offset.y))).rgb * 0.15f;
    return sum;
}

float4 TraceSSR(float2 uv)
{
    if (ssrEnabled <= 0.5f || ssrIntensity <= 0.0f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float centerDepth = depthTex.Sample(samp, uv);
    if (centerDepth >= 0.99999f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float4 encodedNormal = normalTex.Sample(samp, uv);
    float4 materialSample = materialTex.Sample(samp, uv);
    float materialRoughness = saturate(materialSample.r);
    float materialMetallic = saturate(materialSample.g);
    float materialClass = saturate(materialSample.a);
    float roughnessReflectionFade = 1.0f - smoothstep(0.62f, 0.98f, materialRoughness);
    float metalBoost = lerp(1.0f, 1.28f, materialMetallic);
    float classWeight = lerp(0.70f, 1.0f, materialClass);
    float materialMask = pow(saturate(encodedNormal.a), max(ssrMaskPower, 0.001f));
    materialMask = saturate(materialMask * roughnessReflectionFade * metalBoost * classWeight);
    if (materialMask <= 0.001f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float centerViewZ = RestoreViewSpaceZ(centerDepth);
    float3 viewPosition = ReconstructViewPosition(uv, centerViewZ);
    float3 worldNormal = DecodeNormalTarget(encodedNormal);
    float3 viewNormal = normalize(mul(worldNormal, (float3x3)ssrViewMatrix));
    float3 viewRay = normalize(viewPosition);
    float3 reflectionDir = normalize(reflect(viewRay, viewNormal));

    if (reflectionDir.z <= 0.001f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    int sampleCount = clamp((int)ssrStepCount, 1, 48);
    float rayStep = max(ssrMaxDistance, 0.001f) / (float)sampleCount;
    rayStep *= max(ssrStride, 0.05f);
    float thickness = max(ssrThickness, 0.001f);

    float3 hitColor = 0.0f;
    float hitWeight = 0.0f;

    [loop]
    for (int i = 0; i < 48; ++i)
    {
        if (i >= sampleCount)
        {
            break;
        }

        float distanceAlongRay = ((float)i + 1.0f) * rayStep;
        float3 rayPosition = viewPosition + reflectionDir * distanceAlongRay;
        float2 sampleUV = ProjectViewPositionToUV(rayPosition);
        if (any(sampleUV < 0.0f) || any(sampleUV > 1.0f))
        {
            break;
        }

        float sampleDepth = depthTex.Sample(samp, sampleUV);
        if (sampleDepth >= 0.99999f)
        {
            continue;
        }

        float sampleViewZ = RestoreViewSpaceZ(sampleDepth);
        float depthDelta = rayPosition.z - sampleViewZ;
        float adaptiveThickness = thickness + rayPosition.z * 0.006f;
        if (depthDelta >= 0.0f && depthDelta <= adaptiveThickness)
        {
            float4 hitMaterialSample = materialTex.Sample(samp, sampleUV);
            float hitRoughness = saturate(hitMaterialSample.r);
            hitColor = SampleSSRReflectionColor(sampleUV, materialMask, hitRoughness);
            float edgeFade = GetScreenEdgeFade(sampleUV);
            float distanceFade = 1.0f - saturate(distanceAlongRay / max(ssrMaxDistance, 0.001f));
            float depthFade = 1.0f - saturate(centerViewZ / max(ssrDepthFade, 0.001f));
            float viewFacing = saturate(dot(viewNormal, normalize(-viewPosition)));
            float fresnel = pow(1.0f - viewFacing, max(ssrFresnelPower, 0.001f));
            float normalFade = lerp(1.0f, fresnel, saturate(ssrNormalFade));
            float hitRoughnessMatch = 1.0f - saturate(abs(hitRoughness - materialRoughness) * 1.35f);
            hitWeight = edgeFade * distanceFade * depthFade * normalFade * hitRoughnessMatch * materialMask;
            break;
        }
    }

    return float4(hitColor, saturate(hitWeight));
}

float4 main(PSInput input) : SV_TARGET
{
    if (any(input.uv < 0.0f) || any(input.uv > 1.0f))
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    return TraceSSR(input.uv);
}
