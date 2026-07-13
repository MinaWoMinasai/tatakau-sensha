#include "Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    int32_t lightingMode;
    float32_t environmentCoefficient; // 追加：環境マッピング係数 (0.0~1.0)
    float32_t padding; // 16バイトアライメントのための調整
    float32_t4x4 uvTransform;
    float32_t shininess;
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

// シャドウマップ用
Texture2D<float> gShadowMap : register(t1);
SamplerComparisonState gShadowSampler : register(s1); // 比較用サンプラー

// ポイントライト用
ConstantBuffer<PointLight> gPointLight : register(b3);

// 環境マッピング（キューブマップ）用
TextureCube<float32_t4> gEnvironmentMap : register(t2); // register(t2)に追加

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

// PixelShaderOutput main(VertexShaderOutput input)
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // テクスチャサンプリング
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    // 透明度による破棄
    if (textureColor.a < 0.1f)
    {
        discard;
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
        color += skyTint * glint;

        // PSP-like high readability: keep the water vivid, but add modern layered depth.
        color = saturate(color * gMaterial.color.rgb * 1.05f);
        output.color = float4(color, gMaterial.color.a * textureColor.a);
        return output;
    }

    if (gMaterial.enableLighting == 1)
    {
        // --- 共通ベクトルの準備 ---
        float3 N = normalize(input.normal); // 滑らかな法線を使用
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
        float shadowBias = max(0.00035f, 0.0018f * slope);

        // 3x3 PCF。単一比較より輪郭を柔らかくし、ジャギーを抑える。
        uint shadowWidth, shadowHeight;
        gShadowMap.GetDimensions(shadowWidth, shadowHeight);
        float2 shadowTexel = 1.0f / float2(shadowWidth, shadowHeight);
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
                        gShadowSampler, shadowUV + float2(x, y) * shadowTexel, depth - shadowBias);
                }
            }
            shadow /= 9.0f;
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
