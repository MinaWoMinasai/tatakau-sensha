#include "ModelParticle.hlsli"

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

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // UVトランスフォームの適用
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);

    if (gMaterial.enableLighting != 0)
    {
        // 1. 視線ベクトルの計算
        float3 V = normalize(gCamera.worldPosition - input.worldPosition);
        
        // 2. 奥行き感（擬似屈折）
        float2 distortion = V.xy * 0.05f;
        float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy + distortion);
        
        if (textureColor.a < 0.1f)
        {
            discard;
        }

        // 3. フラットシェーディング法線の計算 (宝石の面を作る)
        float3 dx = ddx(input.worldPosition);
        float3 dy = ddy(input.worldPosition);
        float3 N = normalize(cross(dx, dy));

        // 4. ライティング計算
        float3 L = -normalize(gDirectionalLight.direction);
        float3 H = normalize(L + V);
        float NdotL = saturate(dot(N, L));
        float NdotH = saturate(dot(N, H));

        // A. 鋭いスペキュラ
        float3 jewelSpecular = gDirectionalLight.color.rgb * pow(NdotH, 300.0f) * gDirectionalLight.intensity * 2.0f;
        
        // B. フレネル発光
        float fresnel = pow(1.0f - saturate(dot(N, V)), 4.0f);
        float3 jewelGlow = input.color.rgb * fresnel * 3.0f;

        // C. ベースカラー
        float3 baseColor = textureColor.rgb * gMaterial.color.rgb * input.color.rgb * 0.3f;

        // 最終色合成
        output.color.rgb = baseColor + jewelSpecular + jewelGlow;
        
        // 1. まずベースとなる不透明度を計算（輝き成分を足す）
        float baseAlpha = (0.5f * gMaterial.color.a * textureColor.a) + fresnel + pow(NdotH, 100.0f);
        // 2. その全体に対して input.color.a (フェードアウト係数) を掛ける
        output.color.a = saturate(baseAlpha * input.color.a);
    }
    else
    {
        // ライティングなし
        float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
        if (textureColor.a < 0.1f)
        {
            discard;
        }
        output.color = gMaterial.color * textureColor * input.color;
    }

    return output;
}