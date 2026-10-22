#include "Ui.hlsli"

// 画面の大きさ（論理座標。左上が原点、下が +y）。
cbuffer UiScreen : register(b0)
{
    float2 screenSize;
    float2 padding;
};

struct UiVertexInput
{
    float2 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

UiVertexOutput main(UiVertexInput input)
{
    UiVertexOutput output;
    // 論理座標を、正規化デバイス座標（-1〜1、上が +y）へ。
    float2 ndc = input.position / screenSize * 2.0f - 1.0f;
    output.position = float4(ndc.x, -ndc.y, 0.0f, 1.0f);
    output.texcoord = input.texcoord;
    output.color = input.color;
    return output;
}
