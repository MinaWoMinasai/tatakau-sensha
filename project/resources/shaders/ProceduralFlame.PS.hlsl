cbuffer FlameParameters : register(b0)
{
    float4 gColor;
    float4 gCoreCyanTint;
    float gTime;
    float gNoiseScale;
    float gNoiseSpeed;
    float gDistortionStrength;
    float gContourThreshold;
    float gContourWidth;
    float gContourSoftness;
    float gContourEmissiveIntensity;
    float gInnerLineWidth;
    float gInnerLineIntensity;
    float gOuterGlowWidth;
    float gOuterGlowIntensity;
    float gContourAaScale;
    float gContourWidthModulation;
    float2 gContourPadding;
    float gCoreThreshold;
    float gCoreSoftness;
    float gCoreIntensity;
    float gCoreVerticalBias;
    float gCoreBreakup;
    float gCoreNoiseScale;
    float gCoreHotThreshold;
    float gCorePadding;
    float gFieldGain;
    float gBillboardAspect;
    float gCompactSupportScale;
    uint gDisplayMode;
    uint gActiveMetaballCount;
    float3 gPadding;
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
            const float supportRadius = max(
                metaball.z * gCompactSupportScale,
                1.0e-4f);
            const float supportRadiusSquared = supportRadius * supportRadius;
            const float distanceSquared = dot(delta, delta);
            // Compact C2 falloff: unlike inverse-square influence, this is
            // exactly zero outside the support radius. Nearby points still
            // merge smoothly, but separated satellites cannot bridge the
            // whole billboard with weak long-distance tails.
            const float compactDistance = saturate(
                1.0f - distanceSquared / supportRadiusSquared);
            const float kernel = compactDistance * compactDistance * compactDistance;
            field += metaball.w * 1.45f * kernel;
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
    const float widthBreath = 1.0f
        + (contourNoise * 2.0f - 1.0f) * gContourWidthModulation;
    const float localContourWidth = gContourWidth * widthBreath;

    const float antiAlias = max(fwidth(field), 0.0005f);
    const float maskSoftness = max(gContourSoftness, antiAlias);
    const float filledMask = smoothstep(
        gContourThreshold - maskSoftness,
        gContourThreshold + maskSoftness,
        field);
    const float distanceToIso = abs(field - gContourThreshold);
    const float innerHalfWidth = max(
        gInnerLineWidth * lerp(0.90f, 1.10f, contourNoise),
        antiAlias * 0.45f);
    const float contourHalfWidth = max(
        localContourWidth,
        max(
            innerHalfWidth + antiAlias * 0.35f,
            antiAlias * gContourAaScale));
    const float outerGlowHalfWidth = max(
        gOuterGlowWidth * lerp(0.88f, 1.12f, contourNoise),
        contourHalfWidth + antiAlias);
    const float innerFeather = max(gContourSoftness * 0.38f, antiAlias * 0.55f);
    const float contourFeather = max(gContourSoftness, antiAlias * 0.70f);
    const float outerGlowFeather = max(gContourSoftness * 2.4f, antiAlias * 1.25f);
    const float innerLineMask = 1.0f - smoothstep(
        innerHalfWidth,
        innerHalfWidth + innerFeather,
        distanceToIso);
    const float contourMask = 1.0f - smoothstep(
        contourHalfWidth,
        contourHalfWidth + contourFeather,
        distanceToIso);
    const float outerGlowMask = 1.0f - smoothstep(
        outerGlowHalfWidth,
        outerGlowHalfWidth + outerGlowFeather,
        distanceToIso);

    // Offset the shared aura field very slightly for the core only. The
    // positive UV-y pull samples a lower part of the field and lets occasional
    // noise peaks stretch upward without introducing a fixed flame shape.
    const float2 coreNoiseDomain = float2(
        (input.uv.x - 0.5f) * gCoreNoiseScale + 47.13f,
        vertical * gCoreNoiseScale * 0.82f - gTime * 0.19f + 11.71f);
    const float coreNoise = Fbm3(coreNoiseDomain * 0.72f);
    const float coreSideNoise = Fbm3(coreNoiseDomain * 0.54f + float2(8.37f, -5.19f));
    float2 coreUv = input.uv;
    coreUv.x += (coreSideNoise - 0.5f) * gCoreBreakup * 0.022f;
    coreUv.y += smoothstep(0.58f, 0.90f, coreNoise) * gCoreBreakup * 0.025f;
    const float coreField = BuildAuraField(coreUv);

    // Noise erosion and an upper-field penalty make the core a related but
    // independently broken-up blob rather than a second filled outer mask.
    const float coreErosion = (coreNoise - 0.54f) * 1.35f * gCoreBreakup;
    const float upperCorePenalty = smoothstep(0.40f, 0.82f, vertical)
        * gCoreVerticalBias * 1.55f;
    const float shapedCoreField = coreField + coreErosion - upperCorePenalty;
    const float coreSoftness = max(gCoreSoftness, antiAlias);
    const float coreMask = smoothstep(
        gCoreThreshold - coreSoftness,
        gCoreThreshold + coreSoftness,
        shapedCoreField);
    const float hotThreshold = max(
        gCoreHotThreshold,
        gCoreThreshold + coreSoftness);
    const float coreHotMask = smoothstep(
        hotThreshold - coreSoftness * 0.70f,
        hotThreshold + coreSoftness * 0.70f,
        shapedCoreField) * coreMask;

    if (gDisplayMode == 1u)
    {
        // A bounded visualization that preserves useful contrast both below
        // and above the selected iso-value.
        const float fieldView = field / (field + max(gContourThreshold, 0.001f));
        output.color = float4(fieldView.xxx, 1.0f);
    }
    else
    {
        float selectedMask = contourMask;
        if (gDisplayMode == 2u)
        {
            selectedMask = filledMask;
        }
        else if (gDisplayMode == 4u)
        {
            selectedMask = coreMask;
        }
        else if (gDisplayMode == 5u)
        {
            selectedMask = coreHotMask;
        }
        if (selectedMask <= 0.001f)
        {
            if (gDisplayMode != 0u ||
                (outerGlowMask <= 0.001f && coreMask <= 0.001f))
            {
                discard;
            }
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
        if (gDisplayMode == 0u)
        {
            const float3 tintedRainbow = rainbow * gColor.rgb;
            const float3 hotLineColor = float3(0.82f, 0.97f, 1.0f)
                * lerp(1.0f.xxx, gColor.rgb, 0.12f);
            const float3 coreColor = lerp(
                gCoreCyanTint.rgb,
                float3(1.0f, 0.995f, 1.0f),
                coreHotMask);
            const float cyanFringeMask = coreMask * (1.0f - coreHotMask * 0.58f);
            const float3 coreRadiance =
                gCoreCyanTint.rgb * cyanFringeMask * gCoreIntensity * 0.52f
                + coreColor * coreHotMask * gCoreIntensity;
            const float3 layerRadiance =
                tintedRainbow * outerGlowMask * gOuterGlowIntensity
                + tintedRainbow * contourMask * gContourEmissiveIntensity
                + hotLineColor * innerLineMask * gInnerLineIntensity
                + coreRadiance;

            // The PSO uses straight-alpha blending. Divide by the composed
            // coverage so the intended HDR radiance is not multiplied by the
            // mask a second time when the blend unit applies SrcAlpha.
            const float neonCoverage = saturate(max(innerLineMask, contourMask)
                + outerGlowMask * 0.30f);
            const float coreCoverage = max(coreMask * 0.70f, coreHotMask);
            const float layerCoverage = max(neonCoverage, coreCoverage);
            output.color = float4(
                layerRadiance / max(layerCoverage, 0.001f),
                layerCoverage * gColor.a);
        }
        else
        {
            // Straight-alpha blending applies coverage in the blend unit.
            // Keeping RGB white avoids squaring the mask and falsely turning
            // partially covered antialiased pixels into apparent gaps.
            output.color = float4(1.0f.xxx, selectedMask * gColor.a);
        }
    }
    output.normal = float4(0.5f, 0.5f, 1.0f, 0.0f);
    output.material = float4(1.0f, 0.0f, 1.0f, 0.0f);
    return output;
}
