Texture2D bloomTex : register(t0);
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
    float boxBlurIntensity;
    float3 outlineColor;
    float outlineBloomIntensity;
    float outlineBloomWidth;
    float2 uvOffset;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = input.uv - uvOffset;
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
    {
        discard;
    }

    float4 bloom = bloomTex.Sample(samp, uv);
    float3 color = bloom.rgb * intensity;
    float energy = max(max(color.r, color.g), color.b);
    if (energy <= 0.001f)
    {
        discard;
    }
    // This pass is already an additive light-energy composite.  Reusing the
    // blurred coverage as SrcAlpha attenuates thin emitters a second time and
    // makes one-pixel UI lines effectively disappear.  The energy test above
    // keeps empty pixels out, so surviving bloom must be added at full weight.
    return float4(color, 1.0f);
}
