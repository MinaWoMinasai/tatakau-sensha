cbuffer FlameParameters : register(b0)
{
    float4 gColor;
    float gTime;
    float gNoiseScale;
    float gNoiseSpeed;
    float gDistortionStrength;
    float gFlameWidth;
    float gFlameHeight;
    float gEdgeSoftness;
    float gThreshold;
    float gEmissiveIntensity;
    float gDebugMask;
    float2 gPadding;
};

struct PixelShaderInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 material : SV_TARGET2;
};

float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

// Smooth lattice value noise. This is intentionally compact and authored for
// this renderer; no third-party shader code is embedded here.
float ValueNoise(float2 p)
{
    const float2 cell = floor(p);
    const float2 local = frac(p);
    const float2 blend = local * local * (3.0f - 2.0f * local);
    const float a = Hash21(cell);
    const float b = Hash21(cell + float2(1.0f, 0.0f));
    const float c = Hash21(cell + float2(0.0f, 1.0f));
    const float d = Hash21(cell + 1.0f);
    return lerp(lerp(a, b, blend.x), lerp(c, d, blend.x), blend.y);
}

float Fbm3(float2 p)
{
    float value = 0.0f;
    float amplitude = 0.5714286f;
    [unroll]
    for (int octave = 0; octave < 3; ++octave)
    {
        value += ValueNoise(p) * amplitude;
        p = mul(p, float2x2(1.72f, 1.08f, -1.08f, 1.72f));
        amplitude *= 0.5f;
    }
    return value;
}

float BuildFlameMask(float2 uv)
{
    const float height = max(gFlameHeight, 0.05f);
    const float vertical = 1.0f - uv.y;
    const float y = vertical / height;
    const float alive = 1.0f - smoothstep(0.96f, 1.02f, y);

    // The negative time offset makes equal-noise contours travel upward.
    const float2 flowDomain = float2((uv.x - 0.5f) * gNoiseScale, y * gNoiseScale * 1.35f - gTime * gNoiseSpeed);
    const float2 warpDomain = flowDomain * 0.62f + float2(13.17f, -7.31f);
    const float warpX = Fbm3(warpDomain) - 0.5f;
    const float warpY = Fbm3(warpDomain + float2(5.23f, 9.41f)) - 0.5f;
    const float2 warpedDomain = flowDomain + float2(warpX, warpY * 0.35f) * (gDistortionStrength * 3.0f);
    const float detail = Fbm3(warpedDomain) - 0.5f;

    const float upperMotion = 0.25f + 0.75f * saturate(y);
    const float centerSway = warpX * gDistortionStrength * upperMotion;
    const float localX = uv.x - 0.5f + centerSway;

    // A continuous tapered profile, not a triangle. The exponent keeps a
    // broad base while allowing a fine animated tip.
    const float taper = pow(saturate(1.0f - y), 0.62f);
    float halfWidth = 0.5f * gFlameWidth * taper;
    halfWidth += detail * gDistortionStrength * (0.32f + 0.68f * saturate(y));
    halfWidth = max(halfWidth, 0.002f);

    const float signedInside = halfWidth - abs(localX) - gThreshold;
    float mask = smoothstep(-gEdgeSoftness, gEdgeSoftness, signedInside);

    // Keep the emitter root planted and softly close only the generated tip.
    mask *= alive;
    mask *= smoothstep(0.0f, 0.025f, vertical);
    return saturate(mask);
}

PixelShaderOutput main(PixelShaderInput input)
{
    PixelShaderOutput output;
    const float mask = BuildFlameMask(input.uv);
    const bool showMask = gDebugMask > 0.5f;
    if (!showMask && mask <= 0.001f)
    {
        discard;
    }

    const float3 displayColor = showMask
        ? mask.xxx
        : gColor.rgb * gEmissiveIntensity;
    output.color = float4(displayColor, showMask ? 1.0f : mask * gColor.a);
    output.normal = float4(0.5f, 0.5f, 1.0f, 0.0f);
    output.material = float4(1.0f, 0.0f, 1.0f, 0.0f);
    return output;
}
