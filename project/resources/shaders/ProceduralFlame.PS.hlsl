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
    float gBodyRoundness;
    float gNeckWidth;
    float gTongueStrength;
    float3 gPadding;
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

float SmoothMin(float a, float b, float smoothing)
{
    const float h = saturate(0.5f + 0.5f * (b - a) / smoothing);
    return lerp(b, a, h) - smoothing * h * (1.0f - h);
}

// An inexpensive signed-distance approximation for an axis-aligned ellipse.
// Negative values are inside. All blobs use the same UV-space distance unit,
// so their smooth union remains stable enough for a later edge band.
float EllipseDistance(float2 samplePosition, float2 center, float2 radius)
{
    const float2 safeRadius = max(radius, float2(0.001f, 0.001f));
    return (length((samplePosition - center) / safeRadius) - 1.0f) * min(safeRadius.x, safeRadius.y);
}

float AnimatedCenter(float verticalPosition, float seed)
{
    const float phase = gTime * gNoiseSpeed * 0.38f;
    return (Fbm3(float2(verticalPosition * 1.75f - phase, seed)) - 0.5f)
        * gDistortionStrength * (0.45f + verticalPosition * 0.9f);
}

float BuildFlameMask(float2 uv)
{
    const float height = max(gFlameHeight, 0.05f);
    const float vertical = 1.0f - uv.y;
    const float y = vertical / height;
    // The negative time offset makes equal-noise contours travel upward.
    const float2 flowDomain = float2((uv.x - 0.5f) * gNoiseScale, y * gNoiseScale * 1.35f - gTime * gNoiseSpeed);
    const float2 warpDomain = flowDomain * 0.62f + float2(13.17f, -7.31f);
    const float warpX = Fbm3(warpDomain) - 0.5f;
    const float warpY = Fbm3(warpDomain + float2(5.23f, 9.41f)) - 0.5f;
    const float2 warpedDomain = flowDomain + float2(warpX, warpY * 0.35f) * (gDistortionStrength * 3.0f);
    const float leftDetail = Fbm3(warpedDomain + float2(-8.71f, 3.19f)) - 0.5f;
    const float rightDetail = Fbm3(warpedDomain + float2(6.43f, -4.73f)) - 0.5f;

    // Domain warp moves the sampled point itself. Horizontal movement is
    // stronger than vertical movement so the silhouette remains planted.
    float2 shapePoint = float2(uv.x - 0.5f, y);
    shapePoint += float2(warpX * 0.30f, warpY * 0.10f)
        * gDistortionStrength;

    const float width = max(gFlameWidth, 0.05f);
    const float roundness = gBodyRoundness;
    const float neck = gNeckWidth;
    const float tongues = gTongueStrength;

    // Large rounded body and shoulder. Their overlap prevents a triangular
    // falloff and keeps the bottom visually heavy.
    float distance = EllipseDistance(
        shapePoint,
        float2(AnimatedCenter(0.22f, 2.13f) * 0.20f, 0.22f),
        float2(width * 0.45f * roundness, 0.205f * roundness));
    distance = SmoothMin(distance, EllipseDistance(
        shapePoint,
        float2(AnimatedCenter(0.38f, 5.37f) * 0.45f, 0.38f),
        float2(width * 0.34f, 0.14f)), 0.025f);

    // A deliberately narrow bridge produces the neck between lower body and
    // crown while remaining a continuous SDF-like field.
    distance = SmoothMin(distance, EllipseDistance(
        shapePoint,
        float2(AnimatedCenter(0.535f, 8.91f) * 0.75f, 0.535f),
        float2(width * 0.20f * neck, 0.13f)), 0.018f);

    // The upper crown is a separate rounded mass instead of the remainder of
    // a taper. Independent center noise makes it lag behind the lower body.
    distance = SmoothMin(distance, EllipseDistance(
        shapePoint,
        float2(AnimatedCenter(0.69f, 11.43f), 0.69f),
        float2(width * 0.285f, 0.12f)), 0.025f);

    // Two unequal flame tongues. The second can be reduced to zero with the
    // tongue control; both use distinct noise seeds and are never mirrored.
    if (tongues > 0.001f)
    {
        const float primaryCenter = AnimatedCenter(0.84f, 14.77f) * 1.25f - width * 0.075f;
        distance = SmoothMin(distance, EllipseDistance(
            shapePoint,
            float2(primaryCenter, 0.84f),
            float2(width * 0.13f * tongues, 0.16f * tongues)), 0.018f);
        const float secondaryCenter = AnimatedCenter(0.78f, 19.31f) * 1.15f + width * 0.17f;
        distance = SmoothMin(distance, EllipseDistance(
            shapePoint,
            float2(secondaryCenter, 0.78f),
            float2(width * 0.105f * tongues, 0.10f * tongues)), 0.016f);
    }

    // Select a different animated detail field on either side of the moving
    // center. This breaks bilateral symmetry without introducing a seam in
    // the final smoothstep mask.
    const float centerAtY = AnimatedCenter(saturate(y), 23.17f);
    const float sideBlend = smoothstep(-0.025f, 0.025f, shapePoint.x - centerAtY);
    const float asymmetricDetail = lerp(leftDetail, rightDetail, sideBlend) - 0.02f;
    const float signedInside = -distance
        + asymmetricDetail * gDistortionStrength * (0.10f + 0.16f * saturate(y))
        - gThreshold;
    float mask = smoothstep(-gEdgeSoftness, gEdgeSoftness, signedInside);
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
