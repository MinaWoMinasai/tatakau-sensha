Texture2D sceneTex : register(t0);
Texture2D bloomTex : register(t1);
SamplerState samp : register(s0);

cbuffer BloomParam : register(b0)
{
    float threshold;
    float intensity;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    float3 scene = sceneTex.Sample(samp, input.uv).rgb;
    float3 bloom = bloomTex.Sample(samp, input.uv).rgb;

    float3 result = scene + bloom * intensity;

    return float4(result, 1.0f);
}