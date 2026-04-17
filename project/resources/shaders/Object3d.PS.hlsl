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
        // 影の判定 (0.0: 影, 1.0: 日向)
        float shadow = gShadowMap.SampleCmpLevelZero(gShadowSampler, shadowUV, depth - 0.001f);

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