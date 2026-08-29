cbuffer FlameParameters : register(b0)
{
    float4 gColor;
    float gTime;
    float gNoiseScale;
    float gNoiseSpeed;
    float gDistortionStrength;
    float gContourThreshold;
    float gContourWidth;
    float gContourSoftness;
    float gContourEmissiveIntensity;
    float gFieldGain;
    float gBillboardAspect;
    uint gDisplayMode;
    uint gActiveMetaballCount;
    // xy: normalized billboard position (y grows upward)
    // z: radius, w: lifetime contribution
    float4 gMetaballs[12];
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

float3 HsvToRgb(float3 hsv)
{
    const float3 rgbRamp = saturate(
        abs(frac(hsv.x + float3(0.0f, 2.0f / 3.0f, 1.0f / 3.0f)) * 6.0f - 3.0f) - 1.0f);
    return hsv.z * lerp(1.0f.xxx, rgbRamp, hsv.y);
}

float BuildAuraField(float2 uv)
{
    float2 samplePosition = float2(uv.x, 1.0f - uv.y);

    // Retain the existing upward-flowing fBM domain warp, but apply it to the
    // field sample rather than to a hard-coded silhouette.
    const float2 flowDomain = float2(
        (samplePosition.x - 0.5f) * gNoiseScale,
        samplePosition.y * gNoiseScale * 1.35f - gTime * gNoiseSpeed);
    const float2 warpDomain = flowDomain * 0.62f + float2(13.17f, -7.31f);
    const float warpX = Fbm3(warpDomain) - 0.5f;
    const float warpY = Fbm3(warpDomain + float2(5.23f, 9.41f)) - 0.5f;
    samplePosition += float2(warpX * 0.30f, warpY * 0.10f) * gDistortionStrength;

    float field = 0.0f;
    [unroll]
    for (uint index = 0; index < 12; ++index)
    {
        if (index < gActiveMetaballCount)
        {
            const float4 metaball = gMetaballs[index];
            float2 delta = samplePosition - metaball.xy;
            // Compensate for non-square billboards so a point's radius is
            // approximately circular in world space.
            delta.x *= gBillboardAspect;
            const float radiusSquared = max(metaball.z * metaball.z, 1.0e-6f);
            const float distanceSquared = dot(delta, delta);
            field += metaball.w * radiusSquared /
                (distanceSquared + radiusSquared * 0.08f);
        }
    }
    return field * gFieldGain;
}

PixelShaderOutput main(PixelShaderInput input)
{
    PixelShaderOutput output;
    const float field = BuildAuraField(input.uv);

    // This low-frequency field changes line width gently along the contour
    // and over time. It is deliberately independent of the shape warp so the
    // contour breathes without turning into a jagged noise trace.
    const float vertical = 1.0f - input.uv.y;
    const float contourNoise = Fbm3(float2(
        (input.uv.x - 0.5f) * 1.65f + 31.73f,
        vertical * 1.20f - gTime * 0.075f + 17.19f));
    const float localContourWidth = gContourWidth * lerp(0.78f, 1.22f, contourNoise);

    const float antiAlias = max(fwidth(field), 0.0005f);
    const float maskSoftness = max(gContourSoftness, antiAlias);
    const float filledMask = smoothstep(
        gContourThreshold - maskSoftness,
        gContourThreshold + maskSoftness,
        field);
    const float distanceToIso = abs(field - gContourThreshold);
    const float contourHalfWidth = max(localContourWidth, antiAlias * 0.45f);
    const float contourFeather = max(gContourSoftness, antiAlias * 0.70f);
    const float contourMask = 1.0f - smoothstep(
        contourHalfWidth,
        contourHalfWidth + contourFeather,
        distanceToIso);

    if (gDisplayMode == 1u)
    {
        // A bounded visualization that preserves useful contrast both below
        // and above the selected iso-value.
        const float fieldView = field / (field + max(gContourThreshold, 0.001f));
        output.color = float4(fieldView.xxx, 1.0f);
    }
    else
    {
        const float selectedMask = gDisplayMode == 2u ? filledMask : contourMask;
        if (selectedMask <= 0.001f)
        {
            discard;
        }

        // Spatial position supplies the broad hue travel, while the same
        // low-frequency field gives local variation. Time only drifts the
        // palette, so multiple saturated hues coexist in every frame.
        const float hue = frac(
            0.52f
            + vertical * 1.05f
            + (input.uv.x - 0.5f) * 0.35f
            + (contourNoise - 0.5f) * 0.20f
            - gTime * 0.045f);
        const float3 rainbow = HsvToRgb(float3(hue, 0.92f, 1.0f));
        const float3 displayColor = gDisplayMode == 0u
            ? rainbow * gColor.rgb * gContourEmissiveIntensity
            : selectedMask.xxx;
        output.color = float4(displayColor, selectedMask * gColor.a);
    }
    output.normal = float4(0.5f, 0.5f, 1.0f, 0.0f);
    output.material = float4(1.0f, 0.0f, 1.0f, 0.0f);
    return output;
}
