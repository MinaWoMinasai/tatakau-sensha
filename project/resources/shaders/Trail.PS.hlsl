#include "Trail.hlsli"

#ifndef SCENE_NORMAL_TARGET
#define SCENE_NORMAL_TARGET 0
#endif

struct Material
{
    float4 color;
};
ConstantBuffer<Material> gMaterial : register(b0);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
#if SCENE_NORMAL_TARGET
    float4 normal : SV_TARGET1;
#endif
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
#if SCENE_NORMAL_TARGET
    output.normal = float4(0.5f, 0.5f, 1.0f, 0.0f);
#endif
    // テクスチャサンプリング
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    // 頂点カラー(寿命によるAlpha減衰) × マテリアル色 × テクスチャ色
    float4 finalColor = input.color * gMaterial.color * textureColor;
    
    // 透明度が低すぎたら破棄（またはブレンドに任せる）
    if (finalColor.a <= 0.0f)
    {
        discard;
    }
    
    output.color = finalColor;
    return output;
}
