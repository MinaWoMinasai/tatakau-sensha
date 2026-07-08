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
        float horizon = saturate(viewDistance / 310.0f);

        float longWave =
            sin(worldXZ.x * 0.055f + time * 0.62f) * 0.55f +
            sin(worldXZ.y * 0.071f - time * 0.48f) * 0.45f +
            sin((worldXZ.x + worldXZ.y) * 0.034f + time * 0.35f) * 0.38f;
        float shortWave =
            sin(worldXZ.x * 0.31f + worldXZ.y * 0.13f + time * 1.85f) * 0.16f +
            sin(worldXZ.x * -0.19f + worldXZ.y * 0.27f - time * 1.35f) * 0.13f;
        float wave = longWave + shortWave;

        float crest = smoothstep(0.78f, 1.24f, wave);
        float fineFoam = smoothstep(0.64f, 0.93f, sin(worldXZ.x * 0.83f + worldXZ.y * 0.58f + time * 2.8f));
        float foam = crest * (0.55f + 0.45f * fineFoam);

        float fresnel = pow(1.0f - saturate(dot(float3(0.0f, 1.0f, 0.0f), V)), 3.2f);
        float glintLine = smoothstep(0.86f, 1.0f, sin(worldXZ.x * 0.021f - worldXZ.y * 0.018f + time * 0.28f));
        float glint = fresnel * glintLine * 0.22f;

        float3 nearWater = float3(0.02f, 0.43f, 0.72f);
        float3 farWater = float3(0.05f, 0.23f, 0.48f);
        float3 skyTint = float3(0.45f, 0.70f, 0.92f);
        float3 waveTint = float3(0.04f, 0.63f, 0.88f);
        float3 foamTint = float3(0.74f, 0.92f, 1.0f);

        float3 color = lerp(nearWater, farWater, horizon);
        color = lerp(color, skyTint, fresnel * 0.34f);
        color += waveTint * (wave * 0.055f + 0.055f);
        color += foamTint * foam * 0.34f;
        color += skyTint * glint;

        // PSP-like high readability: keep the water vivid, but add modern layered depth.
        color = saturate(color * gMaterial.color.rgb * 1.18f);
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
