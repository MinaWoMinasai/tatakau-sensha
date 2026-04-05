#include "Trail.hlsli"

struct Material
{
    float4 color;
};
ConstantBuffer<Material> gMaterial : register(b0);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    // テクスチャサンプリング
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    // 頂点カラー(寿命によるAlpha減衰) × マテリアル色 × テクスチャ色
    float4 finalColor = input.color * gMaterial.color * textureColor;
    
    // 透明度が低すぎたら破棄（またはブレンドに任せる）
    if (finalColor.a <= 0.0f)
    {
        discard;
    }
    
    return finalColor;
}