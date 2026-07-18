static const float kPbrPi = 3.14159265359f;

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = saturate(dot(N, H));
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    return a2 / max(kPbrPi * denom * denom, 0.00001f);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotV / max(NdotV * (1.0f - k) + k, 0.00001f);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0) - F0) *
        pow(saturate(1.0f - cosTheta), 5.0f);
}

float3 EvaluateCookTorranceLight(
    float3 N,
    float3 V,
    float3 L,
    float3 albedo,
    float3 F0,
    float metallic,
    float roughness,
    float3 radiance)
{
    float3 H = normalize(V + L);
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float HdotV = saturate(dot(H, V));

    float ndf = DistributionGGX(N, H, roughness);
    float geometry = GeometrySmith(N, V, L, roughness);
    float3 fresnel = FresnelSchlick(HdotV, F0);

    float3 numerator = ndf * geometry * fresnel;
    float denominator = max(4.0f * NdotV * NdotL, 0.0001f);
    float3 specular = numerator / denominator;

    float3 kS = fresnel;
    float3 kD = (1.0f - kS) * (1.0f - metallic);
    float3 diffuse = kD * albedo / kPbrPi;
    return (diffuse + specular) * radiance * NdotL;
}

float3 EnvironmentBRDFApprox(float3 F0, float roughness, float NdotV)
{
    const float4 c0 = float4(-1.0f, -0.0275f, -0.572f, 0.022f);
    const float4 c1 = float4(1.0f, 0.0425f, 1.04f, -0.04f);
    float4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28f * NdotV)) * r.x + r.y;
    float2 ab = float2(-1.04f, 1.04f) * a004 + r.zw;
    return max(F0 * ab.x + ab.y, 0.0f);
}

float3 SampleProceduralGraphicsLabEnvironment(float3 direction, float lod)
{
    float3 dir = normalize(direction);
    float horizon = saturate(dir.y * 0.5f + 0.5f);
    float3 sunDir = normalize(float3(0.36f, 0.30f, 0.88f));
    float sun = pow(saturate(dot(dir, sunDir)), 380.0f);
    float sunGlow = pow(saturate(dot(dir, sunDir)), 14.0f);
    float sharpLightFade = 1.0f - smoothstep(0.8f, 4.2f, lod);
    float broadLightFade = 1.0f - smoothstep(2.0f, 7.0f, lod);
    float lowMist = pow(1.0f - horizon, 2.8f);
    float zenithFade = pow(horizon, 1.25f);

    float3 zenith = float3(0.065f, 0.155f, 0.300f);
    float3 midSky = float3(0.170f, 0.430f, 0.620f);
    float3 horizonSky = float3(0.700f, 0.760f, 0.720f);
    float3 waterBelow = float3(0.045f, 0.260f, 0.340f);
    float3 color = lerp(waterBelow, horizonSky, smoothstep(0.0f, 0.42f, horizon));
    color = lerp(color, midSky, smoothstep(0.16f, 0.70f, horizon));
    color = lerp(color, zenith, zenithFade * 0.72f);
    color += float3(1.0f, 0.76f, 0.48f) * sunGlow * 0.42f * broadLightFade;
    color += float3(1.0f, 0.92f, 0.66f) * sun * 5.2f * sharpLightFade;
    color = lerp(color, float3(0.43f, 0.56f, 0.58f), lowMist * 0.13f);
    return max(color, 0.0f);
}

float3 SamplePbrEnvironment(
    TextureCube<float4> environmentMap,
    SamplerState environmentSampler,
    float3 direction,
    float lod,
    float environmentMode)
{
    float3 cubemapColor = environmentMap.SampleLevel(environmentSampler, direction, lod).rgb;
    float3 proceduralColor = SampleProceduralGraphicsLabEnvironment(direction, lod);
    return lerp(cubemapColor, proceduralColor, saturate(environmentMode));
}

void BuildPbrTangentBasis(float3 N, out float3 T, out float3 B)
{
    float3 up = abs(N.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
    T = normalize(cross(up, N));
    B = cross(N, T);
}

float3 SampleDiffuseIrradianceApprox(
    TextureCube<float4> environmentMap,
    SamplerState environmentSampler,
    float3 N,
    float maxMipLevel)
{
    float3 T;
    float3 B;
    BuildPbrTangentBasis(N, T, B);

    float lod = max(maxMipLevel - 1.0f, 0.0f);
    float side = 0.72f;
    float diag = 0.52f;

    float3 color = environmentMap.SampleLevel(environmentSampler, N, lod).rgb * 0.28f;
    color += environmentMap.SampleLevel(environmentSampler, normalize(N + T * side), lod).rgb * 0.12f;
    color += environmentMap.SampleLevel(environmentSampler, normalize(N - T * side), lod).rgb * 0.12f;
    color += environmentMap.SampleLevel(environmentSampler, normalize(N + B * side), lod).rgb * 0.12f;
    color += environmentMap.SampleLevel(environmentSampler, normalize(N - B * side), lod).rgb * 0.12f;
    color += environmentMap.SampleLevel(environmentSampler, normalize(N + (T + B) * diag), lod).rgb * 0.06f;
    color += environmentMap.SampleLevel(environmentSampler, normalize(N + (T - B) * diag), lod).rgb * 0.06f;
    color += environmentMap.SampleLevel(environmentSampler, normalize(N + (-T + B) * diag), lod).rgb * 0.06f;
    color += environmentMap.SampleLevel(environmentSampler, normalize(N + (-T - B) * diag), lod).rgb * 0.06f;
    return max(color, 0.0f);
}

float3 SampleDiffuseIrradiance(
    TextureCube<float4> environmentMap,
    TextureCube<float4> irradianceMap,
    SamplerState environmentSampler,
    float3 N,
    float maxMipLevel,
    float environmentMode)
{
    float3 cubemapIrradiance = SampleDiffuseIrradianceApprox(environmentMap, environmentSampler, N, maxMipLevel);
    float3 proceduralIrradiance = irradianceMap.SampleLevel(environmentSampler, N, 0.0f).rgb;
    return lerp(cubemapIrradiance, proceduralIrradiance, saturate(environmentMode));
}

float3 SampleSpecularIBL(
    TextureCube<float4> environmentMap,
    TextureCube<float4> prefilteredEnvironmentMap,
    SamplerState environmentSampler,
    Texture2D<float4> brdfLut,
    float3 R,
    float3 F0,
    float roughness,
    float NdotV,
    float maxMipLevel,
    float environmentMode)
{
    float lod = roughness * max(maxMipLevel, 0.0f);
    float3 cubemapColor = environmentMap.SampleLevel(environmentSampler, R, lod).rgb;
    float3 proceduralPrefilteredColor = prefilteredEnvironmentMap.SampleLevel(environmentSampler, R, lod).rgb;
    float3 prefilteredColor = lerp(cubemapColor, proceduralPrefilteredColor, saturate(environmentMode));
    float2 brdf = brdfLut.SampleLevel(environmentSampler, float2(saturate(NdotV), saturate(roughness)), 0.0f).rg;
    float3 specularBrdf = max(F0 * brdf.x + brdf.y, 0.0f);
    return max(prefilteredColor * specularBrdf, 0.0f);
}
