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
    float4 source = sceneTex.Sample(samp, input.uv);
    float3 color = source.rgb;

    float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    float epsilon = 0.0001f;

    float contribution = max(0.0f, luminance - threshold);
    contribution /= max(luminance, epsilon);

    float3 extractColor = color * contribution;
    return float4(extractColor, source.a * saturate(contribution));
}
