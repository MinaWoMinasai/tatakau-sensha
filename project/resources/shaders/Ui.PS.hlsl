#include "Ui.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(UiVertexOutput input) : SV_TARGET
{
    // テクスチャ（文字のアトラスか白い画像）に頂点の色を掛ける。
    return gTexture.Sample(gSampler, input.texcoord) * input.color;
}
