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
    float outlineBloomIntensity;
    float outlineBloomWidth;
    float2 pad2;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float GetObjectAlpha(float2 uv)
{
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
    {
        return 0.0f;
    }
    return objectTex.Sample(samp, uv).a;
}

float SampleMaxAlpha(float2 uv, float2 offset)
{
    float alpha = 0.0f;
    alpha = max(alpha, GetObjectAlpha(uv + offset * float2(-1.0f, -1.0f)));
    alpha = max(alpha, GetObjectAlpha(uv + offset * float2( 0.0f, -1.0f)));
    alpha = max(alpha, GetObjectAlpha(uv + offset * float2( 1.0f, -1.0f)));
    alpha = max(alpha, GetObjectAlpha(uv + offset * float2(-1.0f,  0.0f)));
    alpha = max(alpha, GetObjectAlpha(uv + offset * float2( 1.0f,  0.0f)));
    alpha = max(alpha, GetObjectAlpha(uv + offset * float2(-1.0f,  1.0f)));
    alpha = max(alpha, GetObjectAlpha(uv + offset * float2( 0.0f,  1.0f)));
    alpha = max(alpha, GetObjectAlpha(uv + offset * float2( 1.0f,  1.0f)));
    return alpha;
}

float GetOutlineMask(float2 uv, float centerAlpha, float2 texelSize)
{
    if (outlineWidth <= 0.0f)
    {
        return 0.0f;
    }

    float neighborAlpha = SampleMaxAlpha(uv, texelSize * outlineWidth);
    float edgeAlpha = saturate(neighborAlpha - centerAlpha);
    return smoothstep(outlineThreshold, 1.0f, edgeAlpha);
}

float GetOutlineBloomMask(float2 uv, float objectAlpha, float outlineMask, float2 texelSize)
{
    if (outlineBloomIntensity <= 0.0f || outlineBloomWidth <= 0.0f)
    {
        return 0.0f;
    }

    float2 baseOffset = texelSize * outlineBloomWidth;
    float glow = 0.0f;
    glow += SampleMaxAlpha(uv, baseOffset * 1.0f) * 0.45f;
    glow += SampleMaxAlpha(uv, baseOffset * 2.0f) * 0.28f;
    glow += SampleMaxAlpha(uv, baseOffset * 3.0f) * 0.18f;
    glow += SampleMaxAlpha(uv, baseOffset * 4.0f) * 0.09f;

    return saturate(glow * (1.0f - objectAlpha) * (1.0f - outlineMask)) * outlineBloomIntensity;
}

float4 main(PSInput input) : SV_TARGET
{
    uint width;
    uint height;
    objectTex.GetDimensions(width, height);
    float2 texelSize = float2(1.0f / width, 1.0f / height);

    float objectAlpha = saturate(GetObjectAlpha(input.uv));
    float outlineMask = GetOutlineMask(input.uv, objectAlpha, texelSize);
    float outlineBloomMask = GetOutlineBloomMask(input.uv, objectAlpha, outlineMask, texelSize);
    float addMask = saturate(outlineMask + outlineBloomMask);

    if (addMask <= 0.001f)
    {
        discard;
    }

    float3 addColor = outlineColor * addMask;
    return float4(addColor, 1.0f);
}
