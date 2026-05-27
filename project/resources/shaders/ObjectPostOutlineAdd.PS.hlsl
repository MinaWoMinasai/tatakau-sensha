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
    float boxBlurIntensity;
    float3 outlineColor;
    float outlineBloomIntensity;
    float outlineBloomWidth;
    float boxBlurRadius;
    float fullScreenBoxBlurBlend;
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

float SampleOutlineRing(float2 uv, float2 texelSize, float radius)
{
    float alpha = 0.0f;
    alpha = max(alpha, GetObjectAlpha(uv + texelSize * radius * float2( 1.000f,  0.000f)));
    alpha = max(alpha, GetObjectAlpha(uv + texelSize * radius * float2( 0.866f,  0.500f)));
    alpha = max(alpha, GetObjectAlpha(uv + texelSize * radius * float2( 0.500f,  0.866f)));
    alpha = max(alpha, GetObjectAlpha(uv + texelSize * radius * float2( 0.000f,  1.000f)));
    alpha = max(alpha, GetObjectAlpha(uv + texelSize * radius * float2(-0.500f,  0.866f)));
    alpha = max(alpha, GetObjectAlpha(uv + texelSize * radius * float2(-0.866f,  0.500f)));
    alpha = max(alpha, GetObjectAlpha(uv + texelSize * radius * float2(-1.000f,  0.000f)));
    alpha = max(alpha, GetObjectAlpha(uv + texelSize * radius * float2(-0.866f, -0.500f)));
    alpha = max(alpha, GetObjectAlpha(uv + texelSize * radius * float2(-0.500f, -0.866f)));
    alpha = max(alpha, GetObjectAlpha(uv + texelSize * radius * float2( 0.000f, -1.000f)));
    alpha = max(alpha, GetObjectAlpha(uv + texelSize * radius * float2( 0.500f, -0.866f)));
    alpha = max(alpha, GetObjectAlpha(uv + texelSize * radius * float2( 0.866f, -0.500f)));
    return alpha;
}

float SampleDilatedAlpha(float2 uv, float2 texelSize, float radius)
{
    float alpha = GetObjectAlpha(uv);
    int ringCount = clamp((int)ceil(radius * 0.5f), 1, 6);
    [loop]
    for (int ring = 1; ring <= ringCount; ++ring)
    {
        float ringRadius = radius * (float)ring / (float)ringCount;
        alpha = max(alpha, SampleOutlineRing(uv, texelSize, ringRadius));
    }
    return alpha;
}

float GetOutlineMask(float2 uv, float centerAlpha, float2 texelSize)
{
    if (outlineWidth <= 0.0f)
    {
        return 0.0f;
    }

    float neighborAlpha = SampleDilatedAlpha(uv, texelSize, outlineWidth);
    float edgeAlpha = saturate(neighborAlpha - centerAlpha);
    return smoothstep(outlineThreshold, 1.0f, edgeAlpha);
}

float GetOutlineBloomMask(float2 uv, float objectAlpha, float outlineMask, float2 texelSize)
{
    if (outlineBloomIntensity <= 0.0f || outlineBloomWidth <= 0.0f)
    {
        return 0.0f;
    }

    float glow = 0.0f;
    glow += SampleDilatedAlpha(uv, texelSize, outlineBloomWidth * 1.0f) * 0.45f;
    glow += SampleDilatedAlpha(uv, texelSize, outlineBloomWidth * 2.0f) * 0.28f;
    glow += SampleDilatedAlpha(uv, texelSize, outlineBloomWidth * 3.0f) * 0.18f;
    glow += SampleDilatedAlpha(uv, texelSize, outlineBloomWidth * 4.0f) * 0.09f;

    return saturate(glow * (1.0f - objectAlpha) * (1.0f - outlineMask)) * outlineBloomIntensity;
}

float4 main(PSInput input) : SV_TARGET
{
    uint width;
    uint height;
    objectTex.GetDimensions(width, height);
    float2 texelSize = float2(1.0f / width, 1.0f / height);

    float objectAlpha = saturate(GetObjectAlpha(input.uv));
    float3 objectColor = objectTex.Sample(samp, input.uv).rgb;
    float outlineMask = GetOutlineMask(input.uv, objectAlpha, texelSize);
    float outlineBloomMask = GetOutlineBloomMask(input.uv, objectAlpha, outlineMask, texelSize);
    float addMask = saturate(outlineMask + outlineBloomMask);
    float3 baseAdd = (outlineWidth <= 0.0f && outlineBloomIntensity <= 0.0f) ? objectColor : float3(0.0f, 0.0f, 0.0f);
    float3 bloomColor = bloomTex.Sample(samp, input.uv).rgb * intensity;
    float baseEnergy = max(max(baseAdd.r, baseAdd.g), baseAdd.b);
    float bloomEnergy = max(max(bloomColor.r, bloomColor.g), bloomColor.b);

    if (addMask <= 0.001f && bloomEnergy <= 0.001f && baseEnergy <= 0.001f)
    {
        discard;
    }

    float3 addColor = baseAdd + bloomColor + outlineColor * addMask;
    return float4(addColor, 1.0f);
}
