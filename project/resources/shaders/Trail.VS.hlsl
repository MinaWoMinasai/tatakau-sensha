#include "Trail.hlsli"

struct ViewProjection
{
    float4x4 mat; // ViewProjection行列
};
ConstantBuffer<ViewProjection> gViewProjection : register(b0);

struct VertexShaderInput
{
    float4 position : POSITION0; // すでにワールド座標として計算済みの想定
    float4 color : COLOR0;
    float2 texcoord : TEXCOORD0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    // ワールド座標の頂点に ViewProjection を掛けてスクリーンへ
    output.position = mul(input.position, gViewProjection.mat);
    output.color = input.color;
    output.texcoord = input.texcoord;
    return output;
}