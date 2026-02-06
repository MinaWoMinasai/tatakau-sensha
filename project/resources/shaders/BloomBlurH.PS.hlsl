Texture2D sceneTex : register(t0);
SamplerState samp : register(s0);

cbuffer BloomParam : register(b0)
{
    float threshold;
    float intensity;
    float2 padding;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    uint width, height;
    sceneTex.GetDimensions(width, height);

    float2 texel = 1.0 / float2(width, height);
    
    float weights[5] = { 0.227, 0.194, 0.121, 0.054, 0.016 };

    float3 col = sceneTex.Sample(samp, input.uv).rgb * weights[0];

    for (int i = 1; i < 5; i++)
    {
        col += sceneTex.Sample(samp, input.uv + float2(texel.x * i, 0)).rgb * weights[i];
        col += sceneTex.Sample(samp, input.uv - float2(texel.x * i, 0)).rgb * weights[i];
    }
    
    return float4(col * intensity, 1);
}