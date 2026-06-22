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
	float depthOutlineEnabled;
	float depthNearClip;
	float depthFarClip;
	float depthOutlineScale;
	float2 shockwaveCenter;
	float shockwaveRadius;
	float shockwaveWidth;
	float shockwaveStrength;
	float3 shockwavePadding;
	float2 radialBlurCenter;
	float radialBlurWidth;
	float radialBlurIntensity;
	float3 dissolveEdgeColor;
	float dissolveEdgeWidth;
	float dissolveNoiseScale;
	float dissolveNoiseSpeed;
	float2 postEffectPadding;
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

float GetOutlineBloomMask(float2 uv, float objectAlpha, float outlineMask)
{
    if (outlineBloomIntensity <= 0.0f || outlineBloomWidth <= 0.0f)
    {
        return 0.0f;
    }

    uint width;
    uint height;
    objectTex.GetDimensions(width, height);
    float2 texelSize = float2(1.0f / width, 1.0f / height);
    float glow = 0.0f;
    glow += SampleDilatedAlpha(uv, texelSize, outlineBloomWidth * 1.0f) * 0.45f;
    glow += SampleDilatedAlpha(uv, texelSize, outlineBloomWidth * 2.0f) * 0.28f;
    glow += SampleDilatedAlpha(uv, texelSize, outlineBloomWidth * 3.0f) * 0.18f;
    glow += SampleDilatedAlpha(uv, texelSize, outlineBloomWidth * 4.0f) * 0.09f;

    float outsideObject = 1.0f - objectAlpha;
    float outsideOutline = 1.0f - outlineMask;
    return saturate(glow * outsideObject * outsideOutline) * outlineBloomIntensity;
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

    float objectAlpha = saturate(objectColor.a);
    uint width;
    uint height;
    objectTex.GetDimensions(width, height);
    float2 texelSize = float2(1.0f / width, 1.0f / height);
    float outlineMask = GetOutlineMask(uv, objectAlpha, texelSize);
    float alpha = saturate(max(objectAlpha, outlineMask));
    if (dissolveThreshold > 0.0f)
    {
		float dissolveNoise = Hash(
			input.uv * max(dissolveNoiseScale, 1.0f) + timer * dissolveNoiseSpeed);
        if (dissolveNoise < dissolveThreshold)
        {
            discard;
        }
    }

    if (alpha <= 0.001f)
    {
        discard;
    }

    float3 baseColor = objectColor.rgb / max(objectAlpha, 0.001f);
    float3 result = baseColor;
	if (dissolveThreshold > 0.0f && dissolveEdgeWidth > 0.0f)
	{
		float dissolveNoise = Hash(
			input.uv * max(dissolveNoiseScale, 1.0f) + timer * dissolveNoiseSpeed);
		float dissolveEdge = 1.0f - smoothstep(
			dissolveThreshold,
			dissolveThreshold + dissolveEdgeWidth,
			dissolveNoise);
		result += dissolveEdge * dissolveEdgeColor;
	}
    result = ApplyColorModifiers(result);
    result = ApplyOverlays(result, input.uv);

    result = lerp(result, outlineColor, outlineMask);

    float dist = length((input.uv * 2.0f - 1.0f) * 0.5f);
    result *= pow(saturate(1.0f - dist * vignetteIntensity * vignetteScale), 0.8f);

    return float4(result, alpha);
}
