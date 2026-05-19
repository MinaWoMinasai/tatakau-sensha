Texture2D sceneTex : register(t0);
SamplerState samp : register(s0);

cbuffer BloomParam : register(b0)
{
    float threshold;
    float intensity;
    float vignetteIntensity;
    float vignetteScale;
    float timer;
    float distortionAmount;
    float chromAbAmount;
    float isGrayscale;
    float isInverted;
    float noiseIntensity;
    float scanlineIntensity;
    float scanlineFrequency;
    float curvature;
    float borderSharp;
    float glitchAmount;
    float gaussianIntensity;
    float dissolveThreshold;
    float outlineWidth;
    float outlineThreshold;
    float pad1;
    float3 outlineColor;
    float outlineBloomIntensity;
    float outlineBloomWidth;
    float2 pad2;
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

    float2 texel = 1.0f / float2(width, height);
    float weights[5] = { 0.227f, 0.194f, 0.121f, 0.054f, 0.016f };

    float4 col = sceneTex.Sample(samp, input.uv) * weights[0];

    for (int i = 1; i < 5; i++)
    {
        col += sceneTex.Sample(samp, input.uv + float2(texel.x * i, 0.0f)) * weights[i];
        col += sceneTex.Sample(samp, input.uv - float2(texel.x * i, 0.0f)) * weights[i];
    }

    float blurIntensity = (gaussianIntensity > 0.0f) ? 1.0f : intensity;
    return float4(col.rgb * blurIntensity, col.a);
}
