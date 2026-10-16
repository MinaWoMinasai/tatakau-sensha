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
    float32_t waterDiagnosticsEnabled;
    float32_t waterSunPathEnabled;
    float32_t waterAtmosphereEnabled;
    float32_t waterFarFlattenEnabled;
    float32_t waterProceduralCloudReflectionEnabled;
    float32_t waterDebugMode;
    float32_t waterAtmosphereStrength;
    float32_t waterFarFlattenStrength;
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
    float32_t characterLightWrap;
    float32_t characterShadowSoftness;
    float32_t characterShadowStrength;
    float32_t characterRimStrength;
    float32_t characterRimPower;
    float32_t characterSpecularStrength;
    float32_t characterSpecularPower;
    float32_t characterPadding;
    float32_t crystalEnabled;
    float32_t crystalFresnelPower;
    float32_t iridescenceFactor;
    float32_t iridescenceIor;
    float32_t iridescenceThicknessMinimumNm;
    float32_t iridescenceThicknessMaximumNm;
    float32_t crystalEdgeEmission;
    float32_t crystalCoreEmission;
    float32_t3 crystalCoreColor;
    float32_t crystalCorePadding;
    float32_t3 crystalEdgeColor;
    float32_t crystalEdgePadding;
    float32_t enableCaustics;
    float32_t causticsScale;
    float32_t causticsIntensity;
    float32_t causticsPadding;
    float32_t3 causticsColor;
    float32_t causticsColorPadding;
    float32_t causticsAnimationEnabled;
    float32_t causticsPlaybackTime;
    float32_t causticsLoopDuration;
    float32_t causticsFrameCount;
    float32_t causticsAtlasColumns;
    float32_t causticsAtlasRows;
    float32_t2 causticsAnimationPadding;
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
Texture2D<float32_t4> gCausticsMap : register(t10);

// シャドウマップ用
Texture2D<float> gShadowMap : register(t1);
SamplerComparisonState gShadowSampler : register(s1); // 比較用サンプラー

// ポイントライト用
ConstantBuffer<PointLight> gPointLight : register(b3);

// 環境マッピング（キューブマップ）用
TextureCube<float32_t4> gEnvironmentMap : register(t2); // register(t2)に追加

#include "PbrLighting.hlsli"
#include "materials/CrystalMaterial.hlsli"

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
#if SCENE_NORMAL_TARGET
    float32_t4 normal : SV_TARGET1;
    float32_t4 material : SV_TARGET2;
#endif
};

float SampleCausticsAtlasFrame(float2 worldSpaceUV, uint frameIndex)
{
    uint atlasColumns = max((uint)round(gMaterial.causticsAtlasColumns), 1u);
    uint atlasRows = max((uint)round(gMaterial.causticsAtlasRows), 1u);
    uint atlasCellCount = atlasColumns * atlasRows;
    uint safeFrameIndex = min(frameIndex, atlasCellCount - 1u);
    uint frameColumn = safeFrameIndex % atlasColumns;
    uint frameRow = safeFrameIndex / atlasColumns;

    float2 frameUVSize = 1.0f / float2(atlasColumns, atlasRows);
    float2 localFrameUV = frac(worldSpaceUV);
    float2 atlasUV = (float2(frameColumn, frameRow) + localFrameUV) * frameUVSize;
    return gCausticsMap.Sample(gSampler, atlasUV).r;
}

float SampleAnimatedCaustics(float2 worldSpaceUV)
{
    uint atlasColumns = max((uint)round(gMaterial.causticsAtlasColumns), 1u);
    uint atlasRows = max((uint)round(gMaterial.causticsAtlasRows), 1u);
    uint atlasCellCount = atlasColumns * atlasRows;
    uint frameCount = clamp((uint)round(gMaterial.causticsFrameCount), 1u, atlasCellCount);

    if (gMaterial.causticsAnimationEnabled < 0.5f || frameCount <= 1u)
    {
        return SampleCausticsAtlasFrame(worldSpaceUV, 0u);
    }

    float loopDuration = max(gMaterial.causticsLoopDuration, 0.0001f);
    float normalizedTime = frac(max(gMaterial.causticsPlaybackTime, 0.0f) / loopDuration);
    float framePosition = normalizedTime * (float)frameCount;
    uint currentFrameIndex = min((uint)floor(framePosition), frameCount - 1u);
    uint nextFrameIndex = (currentFrameIndex + 1u) % frameCount;
    float frameBlend = frac(framePosition);

    float currentSample = SampleCausticsAtlasFrame(worldSpaceUV, currentFrameIndex);
    float nextSample = SampleCausticsAtlasFrame(worldSpaceUV, nextFrameIndex);
    return lerp(currentSample, nextSample, frameBlend);
}

float32_t3 EvaluateWorldSpaceCaustics(VertexShaderOutput input)
{
    if (gMaterial.enableCaustics < 0.5f || gMaterial.causticsIntensity <= 0.0f)
    {
        return float32_t3(0.0f, 0.0f, 0.0f);
    }

    float2 causticsUV = input.worldPosition.xz * gMaterial.causticsScale;
    float causticsMask = SampleAnimatedCaustics(causticsUV);
    return max(gMaterial.causticsColor, 0.0f) *
        max(gMaterial.causticsIntensity, 0.0f) * causticsMask;
}

void AddWorldSpaceCaustics(inout PixelShaderOutput output, VertexShaderOutput input)
{
    output.color.rgb += EvaluateWorldSpaceCaustics(input);
}

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
        AddWorldSpaceCaustics(output, input);
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

        if (gMaterial.lightingMode == 3)
        {
            float3 albedo = max(gMaterial.color.rgb * textureColor.rgb, 0.0f);
            float3 L_dir = -normalize(gDirectionalLight.direction);
            float3 lightColor = gDirectionalLight.color.rgb * max(gDirectionalLight.intensity, 0.0f);
            float NdotL = dot(N, L_dir);
            float wrappedLight = saturate((NdotL + saturate(gMaterial.characterLightWrap)) /
                (1.0f + saturate(gMaterial.characterLightWrap)));
            float softness = max(gMaterial.characterShadowSoftness, 0.001f);
            float ramp = smoothstep(0.50f - softness, 0.50f + softness, wrappedLight);
            float shadowedRamp = ramp * shadow;
            float shadowStrength = saturate(gMaterial.characterShadowStrength);
            float shade = lerp(1.0f - shadowStrength, 1.0f, shadowedRamp);

            float3 ambientTint = lerp(float3(0.50f, 0.54f, 0.60f), float3(0.76f, 0.80f, 0.86f), ramp);
            float3 baseLit = albedo * ambientTint * (0.48f + 0.20f * shade);
            float3 directLit = albedo * lightColor * (0.05f + 0.11f * ramp) * shadowedRamp;

            float NdotV = saturate(dot(N, V));
            float3 H = normalize(L_dir + V);
            float specular = pow(saturate(dot(N, H)), max(gMaterial.characterSpecularPower, 1.0f));
            specular *= saturate(gMaterial.characterSpecularStrength) * ramp * shadow;

            float rimPower = max(gMaterial.characterRimPower, 0.25f);
            float rim = pow(1.0f - NdotV, rimPower) * saturate(gMaterial.characterRimStrength);
            rim *= lerp(0.55f, 1.0f, ramp);

            int materialDebugMode = (int)(gMaterial.materialDebugMode + 0.5f);
            if (materialDebugMode > 0)
            {
                output.color = float4(MakeMaterialDebugColor(
                    materialDebugMode,
                    N,
                    normalTS,
                    albedo,
                    0.78f,
                    0.0f,
                    1.0f,
                    float3(0.04f, 0.04f, 0.04f),
                    0.0f,
                    float4(0.0f, 0.78f, 0.0f, 1.0f),
                    transformedUV.xy), gMaterial.color.a * textureColor.a);
                AddWorldSpaceCaustics(output, input);
                return output;
            }

#if SCENE_NORMAL_TARGET
            output.normal = EncodeNormalTarget(N, 0.08f);
            output.material = EncodeMaterialTarget(0.78f, 0.0f, 1.0f, 0.45f);
#endif
            float3 color = baseLit + directLit + specular * float3(1.0f, 1.0f, 1.0f) + rim * float3(0.74f, 0.88f, 1.0f);
            color += gMaterial.emissiveColor * max(gMaterial.emissiveIntensity, 0.0f);
            output.color.rgb = max(color, 0.0f);
            output.color.a = gMaterial.color.a * textureColor.a;
            AddWorldSpaceCaustics(output, input);
            return output;
        }

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
            const bool crystalEnabled = gMaterial.crystalEnabled > 0.5f;
            CrystalMaterialResult crystalMaterial;
            crystalMaterial.surfaceTint = albedo;
            crystalMaterial.reflectionTint = float3(1.0f, 1.0f, 1.0f);
            crystalMaterial.emission = float3(0.0f, 0.0f, 0.0f);
            crystalMaterial.fresnel = 0.0f;
            crystalMaterial.dielectricF0 = 0.04f;
            crystalMaterial.edgeWeight = 0.0f;
            if (crystalEnabled)
            {
                crystalMaterial = EvaluateOpaqueCrystal(
                    N,
                    V,
                    gMaterial.crystalFresnelPower,
                    gMaterial.iridescenceFactor,
                    gMaterial.iridescenceIor,
                    gMaterial.iridescenceThicknessMinimumNm,
                    gMaterial.iridescenceThicknessMaximumNm,
                    gMaterial.crystalEdgeEmission,
                    gMaterial.crystalCoreEmission,
                    gMaterial.crystalCoreColor,
                    gMaterial.crystalEdgeColor);
                albedo = crystalMaterial.surfaceTint;
            }
            const float dielectricF0 = crystalEnabled
                ? crystalMaterial.dielectricF0
                : 0.04f;
            float3 F0 = lerp(
                float3(dielectricF0, dielectricF0, dielectricF0),
                albedo,
                metallic);
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
                AddWorldSpaceCaustics(output, input);
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
            if (crystalEnabled)
            {
                ambientSpecular *= crystalMaterial.reflectionTint;
                emissive += crystalMaterial.emission;
            }

            output.color.rgb = max(ambientDiffuse + ambientSpecular + Lo + emissive, 0.0f);
            output.color.a = gMaterial.color.a * textureColor.a;
            AddWorldSpaceCaustics(output, input);
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

    AddWorldSpaceCaustics(output, input);
    return output;
}
