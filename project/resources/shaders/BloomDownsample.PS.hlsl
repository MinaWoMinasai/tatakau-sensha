Texture2D sourceTex : register(t0);
SamplerState samp : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    uint width, height;
    sourceTex.GetDimensions(width, height);
    float2 texel = 1.0f / float2(width, height);
    float2 uv = input.uv;

    float4 c = sourceTex.Sample(samp, uv);
    float4 a = sourceTex.Sample(samp, uv + float2(-2.0f,  2.0f) * texel);
    float4 b = sourceTex.Sample(samp, uv + float2( 2.0f,  2.0f) * texel);
    float4 d = sourceTex.Sample(samp, uv + float2(-2.0f, -2.0f) * texel);
    float4 e = sourceTex.Sample(samp, uv + float2( 2.0f, -2.0f) * texel);
    float4 f = sourceTex.Sample(samp, uv + float2( 0.0f,  1.0f) * texel);
    float4 g = sourceTex.Sample(samp, uv + float2(-1.0f,  0.0f) * texel);
    float4 h = sourceTex.Sample(samp, uv + float2( 1.0f,  0.0f) * texel);
    float4 i = sourceTex.Sample(samp, uv + float2( 0.0f, -1.0f) * texel);

    float4 color = 0.0f;
    color += (f + g + h + i) * 0.125f;
    color += (a + b + d + e) * 0.03125f;
    color += (f + g + h + i) * 0.0625f;
    color += c * 0.125f;

    return color;
}
