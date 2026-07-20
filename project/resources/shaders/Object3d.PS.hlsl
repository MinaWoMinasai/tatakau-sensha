#include "Object3d.hlsli"

#ifndef SCENE_NORMAL_TARGET
#define SCENE_NORMAL_TARGET 0
#endif

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    int32_t lightingMode;
    float32_t environmentCoefficient; // 追加：環境マッピング係数 (0.0~1.0)
    float32_t padding; // 16バイトアライメントのための調整
    float32_t4x4 uvTransform;
    float32_t shininess;
    float32_t metallic;
    float32_t roughness;
    float32_t ambientOcclusion;
    float32_t3 emissiveColor;
    float32_t emissiveIntensity;
    float32_t iblDiffuseIntensity;
    float32_t iblSpecularIntensity;
    float32_t iblMaxMipLevel;
    float32_t pbrEnvironmentMode;
    float32_t shadowReceiveStrength;
    float32_t normalDetailStrength;
    float32_t normalDetailScale;
    float32_t normalMapStrength;
    float32_t metallicMapStrength;
    float32_t roughnessMapStrength;
    float32_t occlusionMapStrength;
    float32_t metallicMapChannel;
    float32_t roughnessMapChannel;
    float32_t occlusionMapChannel;
    float32_t shadowDepthBias;
    float32_t shadowSlopeBias;
    float32_t shadowPcfRadius;
    float32_t materialDebugMode;
    float32_t2 materialDebugPadding;
};

struct Camera
{
    float32_t3 worldPosition;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<Camera> gCamera : register(b2);

struct DirectionalLight
{
    float32_t4 color; // ライトの色
    float32_t3 direction; // ライトの方向
    float intensity; // ライトの光度
};

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);
Texture2D<float32_t4> gNormalMap : register(t4);
Texture2D<float32_t4> gMetallicRoughnessMap : register(t5);
Texture2D<float32_t4> gOcclusionMap : register(t6);
Texture2D<float32_t4> gBrdfLut : register(t7);
TextureCube<float32_t4> gIrradianceMap : register(t8);
TextureCube<float32_t4> gPrefilteredEnvironmentMap : register(t9);

// シャドウマップ用
Texture2D<float> gShadowMap : register(t1);
SamplerComparisonState gShadowSampler : register(s1); // 比較用サンプラー

// ポイントライト用
ConstantBuffer<PointLight> gPointLight : register(b3);

// 環境マッピング（キューブマップ）用
TextureCube<float32_t4> gEnvironmentMap : register(t2); // register(t2)に追加

#include "PbrLighting.hlsli"

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
#if SCENE_NORMAL_TARGET
    float32_t4 normal : SV_TARGET1;
    float32_t4 material : SV_TARGET2;
#endif
};

float32_t4 EncodeNormalTarget(float32_t3 normal, float32_t alpha)
{
    return float32_t4(normalize(normal) * 0.5f + 0.5f, alpha);
}

float32_t4 EncodeMaterialTarget(float32_t roughness, float32_t metallic, float32_t ambientOcclusion, float32_t materialClass)
{
    return float32_t4(
        saturate(roughness),
        saturate(metallic),
        saturate(ambientOcclusion),
        saturate(materialClass));
}

float32_t SelectPackedMaterialChannel(float32_t4 sampleValue, float32_t channel)
{
    if (channel < 0.5f) {
        return sampleValue.r;
    }
    if (channel < 1.5f) {
        return sampleValue.g;
    }
    if (channel < 2.5f) {
        return sampleValue.b;
    }
    return sampleValue.a;
}

float32_t ComputeSSRMaterialMask(float32_t metallic, float32_t roughness, float32_t environmentStrength, float32_t alpha)
{
    float32_t smoothness = 1.0f - saturate(roughness);
    float32_t dielectricReflection = smoothness * smoothness * 0.22f;
    float32_t metalReflection = saturate(metallic) * smoothness * 0.95f;
    float32_t environmentReflection = saturate(environmentStrength) * smoothness * 0.45f;
    return saturate(max(max(dielectricReflection, metalReflection), environmentReflection) * alpha);
}

float32_t3 MakeMaterialDebugColor(
    int32_t debugMode,
    float32_t3 worldNormal,
    float32_t3 tangentNormal,
    float32_t3 albedo,
    float32_t roughness,
    float32_t metallic,
    float32_t ambientOcclusion,
    float32_t3 f0,
    float32_t ssrMask,
    float32_t4 packedMaterialSample,
    float32_t2 uv)
{
    if (debugMode == 1) {
        return normalize(worldNormal) * 0.5f + 0.5f;
    }
    if (debugMode == 2) {
        return normalize(tangentNormal) * 0.5f + 0.5f;
    }
    if (debugMode == 3) {
        return saturate(albedo);
    }
    if (debugMode == 4) {
        return float32_t3(roughness, roughness, roughness);
    }
    if (debugMode == 5) {
        return float32_t3(metallic, metallic, metallic);
    }
    if (debugMode == 6) {
        return float32_t3(ambientOcclusion, ambientOcclusion, ambientOcclusion);
    }
    if (debugMode == 7) {
        return saturate(f0);
    }
    if (debugMode == 8) {
        return float32_t3(ssrMask, ssrMask, ssrMask);
    }
    if (debugMode == 9) {
        return saturate(packedMaterialSample.rgb);
    }
    if (debugMode == 10) {
        float2 tiledUv = frac(uv * 4.0f);
        float32_t gridLine = max(
            1.0f - smoothstep(0.0f, 0.025f, min(tiledUv.x, 1.0f - tiledUv.x)),
            1.0f - smoothstep(0.0f, 0.025f, min(tiledUv.y, 1.0f - tiledUv.y)));
        return lerp(float32_t3(tiledUv.x, tiledUv.y, 0.25f), float32_t3(1.0f, 1.0f, 1.0f), gridLine);
    }
    return saturate(albedo);
}

// PixelShaderOutput main(VertexShaderOutput input)
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
#if SCENE_NORMAL_TARGET
    output.normal = EncodeNormalTarget(input.normal, 0.0f);
    output.material = EncodeMaterialTarget(1.0f, 0.0f, 1.0f, 0.0f);
#endif
    
    // テクスチャサンプリング
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    // 透明度による破棄
    if (textureColor.a < 0.1f)
    {
        discard;
    }

    if (gMaterial.environmentCoefficient <= -0.5f)
    {
        float2 p = input.worldPosition.xz;
        float large = sin(p.x * 0.030f + sin(p.y * 0.021f) * 2.1f);
        float middle = sin(p.x * -0.115f + p.y * 0.082f) * sin(p.x * 0.071f - p.y * 0.096f);
        float fine = sin(p.x * 0.83f + p.y * 0.37f) * sin(p.x * -0.46f + p.y * 0.74f);
        float grain = saturate(0.54f + large * 0.16f + middle * 0.08f + fine * 0.035f);
        float wet = smoothstep(-90.0f, 115.0f, p.y);
        float offshore = smoothstep(40.0f, 220.0f, p.y);
        float3 drySand = float3(0.84f, 0.80f, 0.66f);
        float3 wetSand = float3(0.52f, 0.60f, 0.58f);
        float3 color = lerp(drySand, wetSand, wet * 0.66f) * (0.82f + grain * 0.34f);
        color += float3(0.040f, 0.095f, 0.110f) * smoothstep(0.72f, 1.0f, grain) * wet;
        color = lerp(color, float3(0.10f, 0.27f, 0.34f), offshore * 0.58f);
#if SCENE_NORMAL_TARGET
        output.normal = EncodeNormalTarget(input.normal, saturate(wet * 0.045f));
        output.material = EncodeMaterialTarget(0.86f - wet * 0.14f, 0.0f, 1.0f, 0.35f);
#endif
        output.color = float4(saturate(color * gMaterial.color.rgb), gMaterial.color.a * textureColor.a);
        return output;
    }

    // Graphics lab water. environmentCoefficient >= 2.5 is reserved for
    // the dedicated high-density river mesh.
    if (gMaterial.environmentCoefficient >= 2.5f)
    {
        float time = gMaterial.shininess;
        float2 worldXZ = input.worldPosition.xz;
        float3 baseN = normalize(input.normal);
        float3 V = normalize(gCamera.worldPosition - input.worldPosition);
        float viewDistance = length(gCamera.worldPosition - input.worldPosition);

        float2 flowDir = normalize(float2(0.10f, 0.995f));
        float2 crossDir = float2(flowDir.y, -flowDir.x);
        float flow = dot(worldXZ, flowDir);
        float cross = dot(worldXZ, crossDir);
        float edgeDistance = abs(cross);
        float oceanMode = step(2.7f, gMaterial.environmentCoefficient);
        float seaDepth = saturate(smoothstep(-60.0f, 205.0f, worldXZ.y));
        float shoreFoamMask = smoothstep(-95.0f, -10.0f, worldXZ.y) * (1.0f - smoothstep(70.0f, 160.0f, worldXZ.y));
        float farDetailFade = 1.0f - smoothstep(95.0f, 285.0f, viewDistance);
        float foregroundDetail = 1.0f - smoothstep(40.0f, 165.0f, viewDistance);
        float riverBank = 1.0f - smoothstep(18.0f, 43.0f, edgeDistance);
        float bank = lerp(riverBank, seaDepth, oceanMode);
        float shallow = lerp(1.0f - riverBank, 1.0f - seaDepth, oceanMode);

        float warpA = sin(flow * 0.031f + cross * 0.047f + time * 0.42f);
        float warpB = sin(flow * -0.044f + cross * 0.026f - time * 0.31f);
        float warpedFlow = flow + warpA * 8.5f + warpB * 5.0f;
        float warpedCross = cross + warpA * 4.0f - warpB * 6.5f;

        float longFlow =
            sin(warpedFlow * 0.050f + time * 0.72f) * 0.46f +
            sin(warpedFlow * 0.083f + warpedCross * 0.019f - time * 1.05f) * 0.24f;
        float capillary =
            sin(warpedFlow * 0.33f + warpedCross * 0.13f + time * 1.85f) * 0.040f +
            sin(warpedFlow * 0.58f - warpedCross * 0.19f - time * 2.25f) * 0.028f;
        float curl =
            sin(warpedFlow * 0.13f + sin(warpedCross * 0.075f + time * 0.44f) * 2.8f + time * 0.88f);
        float wave = longFlow + capillary;

        float microA = sin(warpedFlow * 0.78f + warpedCross * 0.34f + time * 2.60f);
        float microB = sin(warpedFlow * -0.56f + warpedCross * 0.61f - time * 2.10f);
        float microC = sin(warpedFlow * 1.08f - warpedCross * 0.24f + time * 3.40f);
        float2 microSlope = float2(
            microA * 0.035f + microB * -0.026f + microC * 0.014f,
            microA * 0.012f + microB * 0.032f + microC * -0.019f);
        float3 N = normalize(baseN + float3(microSlope.x, 0.0f, microSlope.y));
        float fresnel = pow(1.0f - saturate(dot(N, V)), 4.2f);
        float facing = saturate(dot(N, float3(0.0f, 1.0f, 0.0f)));
#if SCENE_NORMAL_TARGET
        output.normal = EncodeNormalTarget(N, saturate(0.82f + fresnel * 0.18f));
        output.material = EncodeMaterialTarget(0.045f, 0.0f, 1.0f, 0.65f);
#endif

        float3 reflectVector = reflect(-V, N);
        float3 skyReflection = gEnvironmentMap.Sample(gSampler, reflectVector).rgb;
        float3 proceduralSky = lerp(
            float3(0.11f, 0.26f, 0.48f),
            float3(0.62f, 0.66f, 0.60f),
            saturate(reflectVector.y * 0.55f + 0.45f));
        skyReflection = lerp(skyReflection, proceduralSky, oceanMode * 0.72f);

        float2 refractUV = worldXZ * 0.040f + microSlope * 4.2f;
        float bottomMud = sin(refractUV.x * 1.15f + sin(refractUV.y * 0.73f + time * 0.05f) * 1.8f);
        float bottomPebble = sin(refractUV.x * 3.4f + sin(refractUV.y * 1.9f) * 0.7f) * sin(refractUV.y * 2.8f);
        float bottomNoise = saturate(bottomMud * 0.26f + bottomPebble * 0.10f + 0.58f);
        float3 riverBed = lerp(float3(0.34f, 0.39f, 0.34f), float3(0.75f, 0.69f, 0.52f), bottomNoise);
        riverBed = lerp(riverBed, float3(0.09f, 0.23f, 0.28f), bank * 0.36f);

        float depthFade = saturate(viewDistance / 260.0f);
        float3 shallowTint = float3(0.24f, 0.82f, 0.88f);
        float3 clearBlue = float3(0.045f, 0.40f, 0.66f);
        float3 deepBlue = float3(0.006f, 0.075f, 0.185f);
        float3 waterColor = lerp(shallowTint, clearBlue, bank);
        waterColor = lerp(waterColor, deepBlue, depthFade * 0.64f);
        waterColor += float3(0.008f, 0.090f, 0.125f) * wave * (0.65f + seaDepth * 0.35f);
        float oceanDepth = saturate(max(depthFade, seaDepth));
        float riverWaterDominance = 0.36f + bank * 0.47f + fresnel * 0.22f;
        float oceanWaterDominance = 0.68f + oceanDepth * 0.25f + fresnel * 0.14f;
        waterColor = lerp(riverBed, waterColor, saturate(lerp(riverWaterDominance, oceanWaterDominance, oceanMode)));

        float causticA = smoothstep(0.62f, 0.99f, sin(warpedFlow * 0.16f + sin(warpedCross * 0.052f) * 3.0f + time * 0.72f));
        float causticB = smoothstep(0.58f, 0.98f, sin(warpedFlow * -0.12f + warpedCross * 0.082f - time * 0.55f));
        float caustic = causticA * causticB * (0.22f + shallow * 0.44f) * (1.0f - oceanDepth * oceanMode * 0.80f);
        caustic *= lerp(1.0f, foregroundDetail * 0.72f, oceanMode);
        waterColor += float3(0.25f, 0.72f, 0.78f) * caustic * (0.025f + shallow * 0.045f);

        float crest = smoothstep(0.82f, 1.16f, longFlow + curl * 0.18f);
        float foamNoiseA = sin(warpedFlow * 0.034f + warpedCross * 0.021f + sin(warpedCross * 0.029f) * 2.6f + time * 0.32f);
        float foamNoiseB = sin(warpedFlow * -0.027f + warpedCross * 0.048f + sin(warpedFlow * 0.017f) * 2.2f - time * 0.24f);
        float foamNoise = foamNoiseA * 0.62f + foamNoiseB * 0.38f + curl * 0.16f;
        float shoreOnlyMask = shoreFoamMask * (1.0f - smoothstep(115.0f, 240.0f, viewDistance));
        float foamSheet = smoothstep(0.64f, 1.06f, foamNoise) * shoreOnlyMask;
        float foamLace = smoothstep(0.84f, 1.0f, sin(warpedFlow * 0.095f + foamNoiseB * 1.4f + time * 0.42f)) * shoreOnlyMask;
        float openWaterFoam = crest * 0.018f * (1.0f - oceanDepth * oceanMode) * farDetailFade;
        float foam = saturate(openWaterFoam + foamSheet * 0.16f + foamLace * foamSheet * 0.08f);

        float glintLine = smoothstep(0.990f, 1.0f, sin(warpedFlow * 0.17f - warpedCross * 0.046f + sin(warpedCross * 0.020f) * 2.8f + time * 0.86f));
        float sparkle = smoothstep(0.994f, 1.0f, sin(warpedFlow * 0.70f + warpedCross * 0.45f + time * 2.70f));
        float glint = (glintLine * 0.075f + sparkle * 0.045f) * fresnel * facing * (0.42f + oceanDepth * 0.58f) * farDetailFade;

        float3 sunDir = normalize(-gDirectionalLight.direction);
        float3 sunReflect = reflect(-sunDir, N);
        float sunSpec = pow(saturate(dot(sunReflect, V)), 180.0f);
        float sunBroad = pow(saturate(dot(sunReflect, V)), 10.0f);
        float sunPathNoise =
            0.74f +
            sin(flow * 0.038f + sin(cross * 0.018f) * 1.1f + time * 0.26f) * 0.13f +
            sin(flow * 0.083f - cross * 0.018f + time * 0.51f) * 0.08f;
        float sunLaneCenter = sin(flow * 0.007f + time * 0.05f) * 28.0f;
        float sunPathWidth = exp(-abs(cross - sunLaneCenter) * 0.0046f);
        float horizonBoost = smoothstep(25.0f, 210.0f, viewDistance);
        float sunPath = (sunBroad * 0.82f + sunSpec * 2.0f) * saturate(sunPathNoise) * sunPathWidth;
        sunPath *= 0.55f + horizonBoost * 0.55f;
        sunPath *= gDirectionalLight.intensity;
        sunPath *= oceanMode;

        float3 reflection = skyReflection * (0.23f + fresnel * 0.76f + oceanMode * 0.15f);
        float3 color = lerp(waterColor, reflection, 0.20f + fresnel * 0.54f + oceanDepth * oceanMode * 0.14f);
        color += float3(0.78f, 0.95f, 1.0f) * foam * 0.16f;
        color += float3(0.78f, 0.93f, 1.0f) * glint * 2.4f;
        color += float3(1.0f, 0.86f, 0.50f) * sunPath * 3.2f;
        color = lerp(color, float3(0.34f, 0.52f, 0.55f), depthFade * 0.12f);
        color *= gMaterial.color.rgb;

        float edgeAlpha = lerp(1.0f - smoothstep(36.0f, 45.0f, edgeDistance), 1.0f, oceanMode);
        float riverAlpha = 0.70f + fresnel * 0.26f + bank * 0.18f;
        float oceanAlpha = 0.76f + fresnel * 0.18f + oceanDepth * 0.20f;
        float alpha = gMaterial.color.a * textureColor.a * saturate(lerp(riverAlpha, oceanAlpha, oceanMode));
        output.color = float4(max(color, 0.0f), alpha * edgeAlpha);
        return output;
    }

    // Naval water prototype.
    // environmentCoefficient >= 1.5 is reserved as a lightweight water-material flag.
    // shininess is reused as scene time for the water object only.
    if (gMaterial.environmentCoefficient >= 1.5f)
    {
        float time = gMaterial.shininess;
        float2 worldXZ = input.worldPosition.xz;
        float3 viewVector = gCamera.worldPosition - input.worldPosition;
        float viewDistance = length(viewVector);
        float3 V = normalize(viewVector);
        float horizon = saturate(viewDistance / 330.0f);
#if SCENE_NORMAL_TARGET
        float3 waterNormal = normalize(input.normal);
        float waterFresnel = pow(1.0f - saturate(dot(waterNormal, V)), 3.2f);
        output.normal = EncodeNormalTarget(waterNormal, saturate(0.68f + waterFresnel * 0.22f));
        output.material = EncodeMaterialTarget(0.06f, 0.0f, 1.0f, 0.65f);
#endif

        float2 swellA = normalize(float2(0.96f, 0.28f));
        float2 swellB = normalize(float2(-0.34f, 0.94f));
        float2 swellC = normalize(float2(0.68f, 0.74f));
        float longWave =
            sin(dot(worldXZ, swellA) * 0.052f + time * 0.72f) * 0.62f +
            sin(dot(worldXZ, swellB) * 0.067f - time * 0.58f) * 0.49f +
            sin(dot(worldXZ, swellC) * 0.033f + time * 0.34f) * 0.39f;
        float shortWave =
            sin(dot(worldXZ, normalize(float2(0.42f, 0.91f))) * 0.42f + time * 1.70f) * 0.135f +
            sin(dot(worldXZ, normalize(float2(-0.82f, 0.57f))) * 0.55f - time * 1.28f) * 0.115f +
            sin((worldXZ.x * 0.83f + worldXZ.y * -0.38f) + time * 2.45f) * 0.060f;
        float gust = sin(dot(worldXZ, normalize(float2(0.78f, -0.62f))) * 0.012f + time * 0.19f);
        float gustMask = smoothstep(0.26f, 0.78f, gust);
        float wave = (longWave + shortWave) * (1.0f + gustMask * 0.28f);

        float waveSlope = abs(ddx(wave)) + abs(ddy(wave));
        float crest = smoothstep(0.72f, 1.12f, wave);
        float fineFoamA = smoothstep(0.72f, 0.96f, sin(worldXZ.x * 1.26f + worldXZ.y * 0.84f + time * 2.55f));
        float fineFoamB = smoothstep(0.66f, 0.94f, sin(worldXZ.x * -1.05f + worldXZ.y * 1.18f - time * 2.10f));
        float foamStreak = smoothstep(0.88f, 1.0f, sin(dot(worldXZ, swellA) * 0.19f + time * 1.12f));
        float slopeFoam = smoothstep(0.018f, 0.065f, waveSlope);
        float foam = crest * (0.30f + 0.30f * fineFoamA + 0.16f * fineFoamB + 0.18f * foamStreak) + slopeFoam * 0.055f;

        float fresnel = pow(1.0f - saturate(dot(float3(0.0f, 1.0f, 0.0f), V)), 3.2f);
        float glintLineA = smoothstep(0.94f, 1.0f, sin(worldXZ.x * 0.035f - worldXZ.y * 0.030f + time * 0.26f));
        float glintLineB = smoothstep(0.91f, 1.0f, sin(worldXZ.x * -0.027f + worldXZ.y * 0.040f - time * 0.22f));
        float sparkle = smoothstep(0.965f, 1.0f, sin(worldXZ.x * 1.74f + worldXZ.y * 1.31f + time * 3.60f));
        float glint = fresnel * (glintLineA * 0.13f + glintLineB * 0.08f + sparkle * 0.035f) * (1.0f + gustMask * 0.22f);

        float3 nearWater = float3(0.010f, 0.34f, 0.62f);
        float3 midWater = float3(0.018f, 0.43f, 0.70f);
        float3 farWater = float3(0.028f, 0.18f, 0.38f);
        float3 skyTint = float3(0.39f, 0.64f, 0.88f);
        float3 waveTint = float3(0.020f, 0.52f, 0.78f);
        float3 deepTint = float3(0.004f, 0.070f, 0.16f);
        float3 foamTint = float3(0.72f, 0.91f, 0.98f);
        float3 horizonMist = float3(0.32f, 0.60f, 0.80f);

        float3 color = lerp(nearWater, midWater, saturate(0.45f + wave * 0.18f));
        color = lerp(color, farWater, horizon);
        color = lerp(color, deepTint, smoothstep(-1.05f, -0.15f, -wave) * 0.16f);
        color = lerp(color, horizonMist, smoothstep(0.48f, 1.0f, horizon) * 0.18f);
        color = lerp(color, skyTint, fresnel * 0.26f);
        color += waveTint * (wave * 0.050f + 0.034f);
        color += foamTint * foam * (0.18f + gustMask * 0.075f);
        color += skyTint * glint * 2.0f;

        // PSP-like high readability: keep the water vivid, but add modern layered depth.
        color = max(color * gMaterial.color.rgb * 1.05f, 0.0f);
        output.color = float4(color, gMaterial.color.a * textureColor.a);
        return output;
    }

    if (gMaterial.enableLighting == 1)
    {
        // --- 共通ベクトルの準備 ---
        float3 N = normalize(input.normal); // 滑らかな法線を使用
        float3 T = input.tangent.xyz - N * dot(N, input.tangent.xyz);
        T = normalize(T + float3(0.00001f, 0.0f, 0.0f));
        float3 B = normalize(cross(N, T) * input.tangent.w);
        float3 normalTS = float3(0.0f, 0.0f, 1.0f);

        float normalMapStrength = saturate(gMaterial.normalMapStrength);
        if (normalMapStrength > 0.0001f)
        {
            float3 sampledNormalTS = gNormalMap.Sample(gSampler, transformedUV.xy).xyz * 2.0f - 1.0f;
            sampledNormalTS = normalize(float3(
                sampledNormalTS.xy * normalMapStrength,
                lerp(1.0f, max(sampledNormalTS.z, 0.001f), normalMapStrength)));
            normalTS = sampledNormalTS;
        }

        float normalDetailStrength = saturate(gMaterial.normalDetailStrength);
        if (normalDetailStrength > 0.0001f)
        {
            float normalDetailScale = max(gMaterial.normalDetailScale, 0.001f);
            float2 detailUV = input.texcoord * normalDetailScale;
            float waveA = sin(detailUV.x * 6.28318f + sin(detailUV.y * 1.73f) * 0.65f);
            float waveB = sin(detailUV.y * 6.28318f + sin(detailUV.x * 1.31f) * 0.55f);
            float waveC = sin((detailUV.x + detailUV.y) * 3.14159f);
            float2 detailSlope = float2(
                waveA * 0.55f + waveC * 0.24f,
                waveB * 0.55f - waveC * 0.18f) * normalDetailStrength;
            normalTS = normalize(float3(normalTS.xy + detailSlope, max(normalTS.z, 0.001f)));
        }
        N = normalize(T * normalTS.x + B * normalTS.y + N * normalTS.z);
        float3 V = normalize(gCamera.worldPosition - input.worldPosition);
        
        // --- 環境マッピングの追加 ---
        // 反射ベクトルを計算 (反射 = reflect(入射, 法線))
        // 入射ベクトルは視線ベクトルの逆向き (-V)
        float3 reflectVector = reflect(-V, N);
        
        // キューブマップをサンプリング
        float4 environmentColor = gEnvironmentMap.Sample(gSampler, reflectVector);
        
        // --- 2. シャドウマッピング ---
        float2 shadowUV = input.shadowMapPosition.xy / input.shadowMapPosition.w;
        shadowUV = shadowUV * float2(0.5f, -0.5f) + 0.5f;
        float depth = input.shadowMapPosition.z / input.shadowMapPosition.w;
        // 法線の傾斜に応じてbiasを増やし、shadow acneを抑える。
        float3 shadowLightDir = -normalize(gDirectionalLight.direction);
        float slope = 1.0f - saturate(dot(N, shadowLightDir));
        float shadowBias = max(max(gMaterial.shadowDepthBias, 0.0f), max(gMaterial.shadowSlopeBias, 0.0f) * slope);

        // 3x3 PCF。単一比較より輪郭を柔らかくし、ジャギーを抑える。
        uint shadowWidth, shadowHeight;
        gShadowMap.GetDimensions(shadowWidth, shadowHeight);
        float2 shadowTexel = 1.0f / float2(shadowWidth, shadowHeight);
        float shadowPcfRadius = max(gMaterial.shadowPcfRadius, 0.0f);
        float shadow = 1.0f;
        if (all(shadowUV >= 0.0f) && all(shadowUV <= 1.0f) && depth >= 0.0f && depth <= 1.0f)
        {
            shadow = 0.0f;
            [unroll]
            for (int y = -1; y <= 1; ++y)
            {
                [unroll]
                for (int x = -1; x <= 1; ++x)
                {
                    shadow += gShadowMap.SampleCmpLevelZero(
                        gShadowSampler, shadowUV + float2(x, y) * shadowTexel * shadowPcfRadius, depth - shadowBias);
                }
            }
            shadow /= 9.0f;
        }
        shadow = lerp(1.0f, shadow, saturate(gMaterial.shadowReceiveStrength));

        if (gMaterial.lightingMode == 2)
        {
            float3 albedo = max(gMaterial.color.rgb * textureColor.rgb, 0.0f);
            float4 metallicRoughnessSample = gMetallicRoughnessMap.Sample(gSampler, transformedUV.xy);
            float metallic = saturate(lerp(
                gMaterial.metallic,
                SelectPackedMaterialChannel(metallicRoughnessSample, gMaterial.metallicMapChannel),
                saturate(gMaterial.metallicMapStrength)));
            float roughness = clamp(lerp(
                gMaterial.roughness,
                SelectPackedMaterialChannel(metallicRoughnessSample, gMaterial.roughnessMapChannel),
                saturate(gMaterial.roughnessMapStrength)), 0.04f, 1.0f);
#if SCENE_NORMAL_TARGET
            float ssrMask = ComputeSSRMaterialMask(metallic, roughness, max(gMaterial.environmentCoefficient, 0.0f), gMaterial.color.a * textureColor.a);
            output.normal = EncodeNormalTarget(N, ssrMask);
#endif
            float occlusionSample = SelectPackedMaterialChannel(
                gOcclusionMap.Sample(gSampler, transformedUV.xy),
                gMaterial.occlusionMapChannel);
            float ao = saturate(gMaterial.ambientOcclusion * lerp(1.0f, occlusionSample, saturate(gMaterial.occlusionMapStrength)));
#if SCENE_NORMAL_TARGET
            output.material = EncodeMaterialTarget(roughness, metallic, ao, 1.0f);
#endif
            float NdotV = saturate(dot(N, V));
            float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
            int materialDebugMode = (int)(gMaterial.materialDebugMode + 0.5f);
            if (materialDebugMode > 0)
            {
                float debugSSRMask = ComputeSSRMaterialMask(
                    metallic,
                    roughness,
                    max(gMaterial.environmentCoefficient, 0.0f),
                    gMaterial.color.a * textureColor.a);
                output.color = float4(MakeMaterialDebugColor(
                    materialDebugMode,
                    N,
                    normalTS,
                    albedo,
                    roughness,
                    metallic,
                    ao,
                    F0,
                    debugSSRMask,
                    metallicRoughnessSample,
                    transformedUV.xy), gMaterial.color.a * textureColor.a);
                return output;
            }

            float3 Lo = 0.0f;
            float3 L_dir = -normalize(gDirectionalLight.direction);
            float3 directionalRadiance = gDirectionalLight.color.rgb * max(gDirectionalLight.intensity, 0.0f);
            Lo += EvaluateCookTorranceLight(N, V, L_dir, albedo, F0, metallic, roughness, directionalRadiance) * shadow;

            float3 pointVector = gPointLight.position - input.worldPosition;
            float pointDistance = max(length(pointVector), 0.001f);
            float3 L_point = pointVector / pointDistance;
            float pointAttenuation = pow(saturate(1.0f - pointDistance / gPointLight.radius), gPointLight.decay);
            float3 pointRadiance = gPointLight.color.rgb * max(gPointLight.intensity, 0.0f) * pointAttenuation;
            Lo += EvaluateCookTorranceLight(N, V, L_point, albedo, F0, metallic, roughness, pointRadiance);

            float3 kS = FresnelSchlickRoughness(NdotV, F0, roughness);
            float3 kD = (1.0f - kS) * (1.0f - metallic);
            float envStrength = max(gMaterial.environmentCoefficient, 0.0f);
            float maxMipLevel = max(gMaterial.iblMaxMipLevel, 0.0f);
            float environmentMode = gMaterial.pbrEnvironmentMode;
            float3 diffuseIrradiance = SampleDiffuseIrradiance(
                gEnvironmentMap, gIrradianceMap, gSampler, N, maxMipLevel, environmentMode);
            float3 specularIBL = SampleSpecularIBL(
                gEnvironmentMap, gPrefilteredEnvironmentMap, gSampler, gBrdfLut,
                reflectVector, F0, roughness, NdotV, maxMipLevel, environmentMode);
            float3 ambientDiffuse = kD * albedo * diffuseIrradiance * ao * max(gMaterial.iblDiffuseIntensity, 0.0f);
            float3 ambientSpecular = specularIBL * envStrength * max(gMaterial.iblSpecularIntensity, 0.0f);
            float3 emissive = gMaterial.emissiveColor * max(gMaterial.emissiveIntensity, 0.0f);

            output.color.rgb = max(ambientDiffuse + ambientSpecular + Lo + emissive, 0.0f);
            output.color.a = gMaterial.color.a * textureColor.a;
            return output;
        }

        // --- 3. 平行光源 (Directional Light) ---
        float3 L_dir = -normalize(gDirectionalLight.direction);
        float NdotL_dir = saturate(dot(N, L_dir));
        float3 diffuse_dir = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * NdotL_dir * gDirectionalLight.intensity;

        // --- 4. ポイントライト (Point Light) ---
        float3 L_point = gPointLight.position - input.worldPosition;
        float dist = length(L_point);
        L_point = normalize(L_point);
        float NdotL_point = saturate(dot(N, L_point));
        // 距離減衰
        float attenuation = pow(saturate(1.0f - dist / gPointLight.radius), gPointLight.decay);
        float3 diffuse_point = gMaterial.color.rgb * textureColor.rgb * gPointLight.color.rgb * NdotL_point * gPointLight.intensity * attenuation;

        // --- 5. 合成 ---
        float3 ambient = gMaterial.color.rgb * textureColor.rgb * 0.1f;
        // 影（shadow）は平行光源（diffuse_dir）にだけ掛ける
        // ポイントライト（diffuse_point）は、そのライト用のシャドウマップがない限りそのまま足す
        // 反射色を係数に基づいて合成
        
        float3 reflection = environmentColor.rgb * gMaterial.environmentCoefficient;
#if SCENE_NORMAL_TARGET
        float legacySSRMask = ComputeSSRMaterialMask(0.0f, max(1.0f - saturate(gMaterial.environmentCoefficient), 0.12f), max(gMaterial.environmentCoefficient, 0.0f), gMaterial.color.a * textureColor.a);
        output.normal = EncodeNormalTarget(N, legacySSRMask);
        output.material = EncodeMaterialTarget(
            max(1.0f - saturate(gMaterial.environmentCoefficient), 0.12f),
            0.0f,
            1.0f,
            0.25f);
#endif
        
        // 最終出力に反射成分を加算
        output.color.rgb = ambient + (diffuse_dir * shadow) + diffuse_point + reflection;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }

    return output;
}
