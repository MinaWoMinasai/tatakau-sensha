#include "Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    int32_t lightingMode;
    float32_t2 padding;
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

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};


PixelShaderOutput main(VertexShaderOutput input)
{
    
    PixelShaderOutput output;
    
    if (gMaterial.enableLighting == 1)
    {
        // 1. 共通変数の計算 (既存)
        float3 V = normalize(gCamera.worldPosition - input.worldPosition);
        float3 dx = ddx(input.worldPosition);
        float3 dy = ddy(input.worldPosition);
        float3 N = normalize(cross(dx, dy)); // 宝石のフラット法線

    // 2. シャドウマッピング
    // プロジェクション座標をUV空間(0~1)に変換
        float2 shadowUV = input.shadowMapPosition.xy / input.shadowMapPosition.w;
        shadowUV = shadowUV * float2(0.5f, -0.5f) + 0.5f;
        float depth = input.shadowMapPosition.z / input.shadowMapPosition.w;
    // 影の濃さをサンプリング (比較サンプラーを使用)
        float shadow = gShadowMap.SampleCmpLevelZero(gShadowSampler, shadowUV, depth - 0.005f);

    // 3. 平行光源 (Directional Light) の計算
        float3 L_dir = -normalize(gDirectionalLight.direction);
        float3 H_dir = normalize(L_dir + V);
        float NdotL_dir = saturate(dot(N, L_dir));
        float NdotH_dir = saturate(dot(N, H_dir));

    // 4. ポイントライト (Point Light) の計算
        float3 L_point = gPointLight.position - input.worldPosition;
        float dist = length(L_point);
        L_point = normalize(L_point);
        float3 H_point = normalize(L_point + V);
    
    // 距離による減衰
        float attenuation = pow(saturate(1.0f - dist / gPointLight.radius), gPointLight.decay);
        float NdotL_point = saturate(dot(N, L_point)) * attenuation;
        float NdotH_point = saturate(dot(N, H_point)) * attenuation;

    // 5. 宝石エフェクトの合成 (既存のロジックに影と点光源を混ぜる)
    // 影は「拡散反射」と「鋭いスペキュラ」に乗算する
        float3 diffuse = (gDirectionalLight.color.rgb * NdotL_dir + gPointLight.color.rgb * NdotL_point) * shadow;
    
        float3 spec = gDirectionalLight.color.rgb * pow(NdotH_dir, 300.0f) * gDirectionalLight.intensity * shadow;
        spec += gPointLight.color.rgb * pow(NdotH_point, 300.0f) * gPointLight.intensity * shadow;

        float fresnel = pow(1.0f - saturate(dot(N, V)), 4.0f);
        float3 jewelGlow = float3(0.5f, 0.8f, 1.0f) * fresnel * 3.0f; // 自己発光なので影の影響を受けない

        output.color.rgb = (diffuse * 0.3f) + spec + jewelGlow;
        output.color.a = 1.0f; // 既存のアルファ制御をここに適用
    
    }
    else
    {
        
        float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
        float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
        float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

        if (textureColor.a < 0.1f)
        {
            discard;
        }
        
        output.color = gMaterial.color * textureColor;
    }
    
    return output;
}