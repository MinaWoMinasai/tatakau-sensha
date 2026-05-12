Texture2D objectTex : register(t0);
Texture2D bloomTex : register(t1);
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
    float pad2;
};

float Hash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

float2 ApplyGlitch(float2 uv)
{
    if (glitchAmount <= 0.0f)
    {
        return uv;
    }

    float2 gridSize = float2(12.0f, 12.0f);
    float2 gridIndex = floor(uv * gridSize);
    float timeStep = floor(timer * 15.0f);
    float seed = Hash(gridIndex + timeStep);

    if (seed > 0.9f)
    {
        float2 rOffset = float2(Hash(float2(seed, timeStep)), Hash(float2(timeStep, seed))) - 0.5f;
        uv += rOffset * glitchAmount;
    }
    return uv;
}

float2 ApplyWave(float2 uv)
{
    uv.x += sin(uv.y * 10.0f + timer * 2.0f) * distortionAmount;
    uv.y += cos(uv.x * 10.0f + timer * 2.0f) * distortionAmount;
    return uv;
}

float3 ApplyColorModifiers(float3 color)
{
    if (isInverted > 0.5f)
    {
        color = 1.0f - color;
    }
    if (isGrayscale > 0.5f)
    {
        float gray = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
        color = float3(gray, gray, gray);
    }
    return color;
}

float3 ApplyOverlays(float3 color, float2 uv)
{
    float scanline = sin(uv.y * scanlineFrequency + timer * 2.0f);
    color -= scanline * 0.1f * scanlineIntensity;

    float noise = Hash(uv + frac(timer));
    color += (noise - 0.5f) * noiseIntensity;
    return color;
}

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = ApplyWave(ApplyGlitch(input.uv));
    float2 shift = (uv - 0.5f) * chromAbAmount;

    float4 objectColor;
    objectColor.r = objectTex.Sample(samp, uv + shift).r;
    objectColor.g = objectTex.Sample(samp, uv).g;
    objectColor.b = objectTex.Sample(samp, uv - shift).b;
    objectColor.a = objectTex.Sample(samp, uv).a;

    float4 bloomColor;
    bloomColor.r = bloomTex.Sample(samp, uv + shift).r;
    bloomColor.g = bloomTex.Sample(samp, uv).g;
    bloomColor.b = bloomTex.Sample(samp, uv - shift).b;
    bloomColor.a = bloomTex.Sample(samp, uv).a;

    float alpha = saturate(max(objectColor.a, bloomColor.a));
    if (dissolveThreshold > 0.0f)
    {
        float dissolveNoise = Hash(input.uv * 100.0f);
        if (dissolveNoise < dissolveThreshold)
        {
            discard;
        }
    }

    if (alpha <= 0.001f)
    {
        discard;
    }

    float3 result = objectColor.rgb + bloomColor.rgb * intensity;
    result = ApplyColorModifiers(result);
    result = ApplyOverlays(result, input.uv);

    float dist = length((input.uv * 2.0f - 1.0f) * 0.5f);
    result *= pow(saturate(1.0f - dist * vignetteIntensity * vignetteScale), 0.8f);

    return float4(result, alpha);
}
