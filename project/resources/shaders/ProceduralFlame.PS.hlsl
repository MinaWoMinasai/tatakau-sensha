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
    uint gEnableStarSparks;
    uint gStarSparkCount;
    float gStarSparkSize;
    float gStarSparkIntensity;
    float gStarSparkTwinkleSpeed;
    float gStarSparkGlowStrength;
    float2 gStarSparkPadding;
    // xy: normalized billboard position, z: size scale, w: phase
    float4 gStarSparkData[8];
    float4 gStarSparkColors[8];
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
float LatticeHash(int2 cell)
{
    // Integer arithmetic gives a shared corner exactly the same value from
    // either adjacent cell, including negative coordinates and fused math.
    uint key = asuint(cell.x) * 0x9e3779b9u ^ asuint(cell.y) * 0x85ebca6bu;
    key ^= key >> 16;
    key *= 0x7feb352du;
    key ^= key >> 15;
    key *= 0x846ca68bu;
    key ^= key >> 16;
    return float(key & 0x00ffffffu) / 16777216.0f;
}

float ValueNoise(float2 p)
{
    const int2 cell = int2(floor(p));
    const float2 local = frac(p);
    const float2 blend = local * local * (3.0f - 2.0f * local);
    const float a = LatticeHash(cell);
    const float b = LatticeHash(cell + int2(1, 0));
    const float c = LatticeHash(cell + int2(0, 1));
    const float d = LatticeHash(cell + int2(1, 1));
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

// These distances are in billboard-height units, including aspect correction.
// Give subpixel features analytic coverage instead of forcing every line to a
// full pixel (which used to wash the entire rainbow contour towards white).
float StrokeMask(float distanceToLine, float halfWidth, float aa)
{
    return (1.0f - smoothstep(max(halfWidth - aa, 0.0f), halfWidth + aa, distanceToLine))
        * saturate(halfWidth / max(aa, 1.0e-5f));
}

void BuildStarSparkLayer(float2 p, float pixelSize, out float3 radiance, out float coverage)
{
    radiance = 0.0f.xxx;
    coverage = 0.0f;
    if (gEnableStarSparks == 0u) return;

    [unroll]
    for (uint index = 0; index < 8; ++index)
    {
        if (index < gStarSparkCount)
        {
            const float4 spark = gStarSparkData[index];
            const float clock = gTime * gStarSparkTwinkleSpeed
                * lerp(0.72f, 1.24f, frac(spark.w * 7.13f)) + spark.w;
            const float cycle = frac(clock);
            const float generation = floor(clock);
            const float seed = Hash21(float2(index + 19.0f, generation));
            const float envelope = pow(saturate(sin(cycle * 3.14159265f)), 3.0f);
            // New position at each invisible end of the twinkle. The flash
            // rises a little with the plume, rather than marking fixed slots.
            float2 center = spark.xy;
            center.x = lerp(0.18f, 0.82f, seed);
            center.y = lerp(0.27f, 0.77f, Hash21(float2(generation + 8.0f, index + 3.0f)));
            center += float2(sin(gTime * 1.1f + seed * 12.0f) * 0.025f, cycle * 0.07f);
            float2 delta = p - center;
            delta.x *= gBillboardAspect;
            const float size = max(gStarSparkSize * spark.z * lerp(0.32f, 0.92f, envelope), 0.0001f);
            const float2 q = abs(delta) / size;
            const float rayWidth = max(0.020f, pixelSize * 0.55f / size);
            const float horizontal = exp(-q.x * 3.0f) * exp(-q.y * q.y / (rayWidth * rayWidth));
            const float vertical = exp(-q.y * 2.5f) * exp(-q.x * q.x / (rayWidth * rayWidth));
            const float rays = max(horizontal, vertical);
            const float pointMask = exp(-dot(q, q) * 160.0f);
            const float glow = exp(-dot(q, q) * 8.0f);
            const float3 color = gStarSparkColors[index].rgb;
            const float intensity = gStarSparkIntensity * envelope;
            radiance += color * (rays + glow * 0.18f * gStarSparkGlowStrength) * intensity
                + 1.0f.xxx * pointMask * intensity * 0.9f;
            coverage = max(coverage, saturate((rays + pointMask + glow * 0.12f) * envelope));
        }
    }

    // Small hollow embers and pinpoints give scale to the larger glints. Every
    // slot has its own birth, drift and fade; no textures or particle buffers.
    [loop]
    for (uint ember = 0; ember < 28; ++ember)
    {
        const float slot = float(ember);
        const float clock = gTime * lerp(0.19f, 0.36f, Hash21(float2(slot, 5.0f))) + slot * 0.618034f;
        const float life = frac(clock);
        const float seed = Hash21(float2(slot + 27.0f, floor(clock)));
        const float fade = smoothstep(0.0f, 0.12f, life) * (1.0f - smoothstep(0.58f, 1.0f, life));
        float2 center = float2(0.5f + (seed - 0.5f) * 0.70f, 0.13f + life * 0.82f);
        center.x += sin(life * 6.0f + seed * 20.0f) * 0.045f * life;
        float2 delta = (p - center) * float2(gBillboardAspect, 1.0f);
        const float radius = lerp(0.0016f, 0.0052f, Hash21(float2(slot + 3.0f, floor(clock) + 7.0f)))
            * lerp(1.0f, 0.45f, life);
        const float d = length(delta);
        const bool ring = ember % 3u != 0u;
        const float mask = ring
            ? StrokeMask(abs(d - radius), radius * 0.22f, pixelSize * 0.65f)
            : (1.0f - smoothstep(radius * 0.2f, radius + pixelSize * 0.65f, d));
        const float glow = exp(-dot(delta, delta) / max(radius * radius * 5.0f, 1.0e-7f));
        const float3 emberColor = lerp(float3(0.55f, 0.80f, 1.0f),
            HsvToRgb(float3(frac(seed + life * 0.27f), 0.75f, 1.0f)), 0.55f);
        const float intensity = min(gStarSparkIntensity * 0.16f, 2.4f);
        radiance += (emberColor * mask + emberColor * glow * 0.07f * gStarSparkGlowStrength)
            * intensity * fade;
        coverage = max(coverage, mask * fade * 0.60f);
    }
}

float BuildAuraField(float2 uv)
{
    float2 p = float2(uv.x, 1.0f - uv.y);
    const float height = p.y;
    const float2 flow = float2((p.x - 0.5f) * gNoiseScale,
        height * gNoiseScale * 1.35f - gTime * gNoiseSpeed);
    const float2 warp = flow * 0.62f + float2(13.17f, -7.31f);
    const float2 turbulence = float2(Fbm3(warp), Fbm3(warp + float2(5.23f, 9.41f))) - 0.5f;
    // Preserve a rounded foot; ascending lobes curl, pinch off and tear.
    const float breakup = smoothstep(0.17f, 0.73f, height);
    p += turbulence * float2(0.34f, 0.18f) * gDistortionStrength * lerp(0.48f, 1.7f, breakup);
    p.x += sin(height * 17.0f - gTime * 1.8f) * 0.012f * breakup;
    const float detail = Fbm3(flow * 2.4f + float2(31.8f, 7.3f)) - 0.5f;
    p += float2(detail * 0.08f, detail * 0.045f) * gDistortionStrength * lerp(0.35f, 1.0f, breakup);

    float field = 0.0f;
    [unroll]
    for (uint index = 0; index < 12; ++index)
    {
        if (index < gActiveMetaballCount)
        {
            const float4 ball = gMetaballs[index];
            const float2 delta = (p - ball.xy) * float2(gBillboardAspect, 1.0f);
            const float radius = max(ball.z * gCompactSupportScale, 0.0001f);
            const float compact = saturate(1.0f - dot(delta, delta) / (radius * radius));
            field += ball.w * 1.45f * compact * compact * compact;
        }
    }
    // Multiplicative erosion cannot create disconnected noise outside the
    // compact support. The upper edge has more detail than the quiet base.
    return field * gFieldGain * max(0.25f,
        1.0f + detail * gDistortionStrength * lerp(0.8f, 3.8f, breakup));
}

PixelShaderOutput main(PixelShaderInput input)
{
    PixelShaderOutput output;
    const float vertical = 1.0f - input.uv.y;
    const float2 p = float2(input.uv.x, vertical);
    const float2 metric = float2(input.uv.x * gBillboardAspect, vertical);
    const float pixelSize = max(max(length(ddx(metric)), length(ddy(metric))), 1.0e-5f);
    const float field = BuildAuraField(input.uv);
    const float fieldAa = max(fwidth(field), 0.001f);
    const float gradient = max(length(float2(ddx(field), ddy(field))) / pixelSize, 0.5f);
    const float distanceToIso = abs(field - gContourThreshold) / gradient;
    const float contourNoise = Fbm3(float2((p.x - 0.5f) * 4.1f + 31.73f,
        vertical * 3.4f - gTime * 0.24f));
    const float widthBreath = 1.0f + (contourNoise * 2.0f - 1.0f) * gContourWidthModulation;
    const float aa = max(pixelSize * 0.65f * gContourAaScale, gContourSoftness * 0.016f);
    const float contourWidth = gContourWidth * 0.065f * widthBreath;
    const float contourMask = StrokeMask(distanceToIso, contourWidth, aa);
    const float innerLineMask = StrokeMask(distanceToIso, gInnerLineWidth * 0.055f, aa);
    const float glowWidth = max(gOuterGlowWidth * 0.11f, contourWidth + aa);
    const float outerGlowMask = exp(-distanceToIso * distanceToIso / (glowWidth * glowWidth) * 2.0f)
        * smoothstep(0.02f, gContourThreshold * 0.8f, field);
    const float filledMask = smoothstep(gContourThreshold - fieldAa, gContourThreshold + fieldAa, field);

    // A separate advected erosion field carves the white heat into small
    // connected lobes. The outer contour still comes from the original field.
    const float2 coreDomain = float2((p.x - 0.5f) * gCoreNoiseScale * 1.55f + 47.13f,
        vertical * gCoreNoiseScale * 1.25f - gTime * gNoiseSpeed * 1.35f + 11.71f);
    const float coreNoise = Fbm3(coreDomain);
    const float fineCoreNoise = Fbm3(coreDomain * 2.7f + float2(8.3f, -5.2f));
    float2 coreUv = input.uv;
    coreUv.x += (fineCoreNoise - 0.5f) * gCoreBreakup * 0.045f;
    coreUv.y += (coreNoise - 0.5f) * gCoreBreakup * 0.026f;
    const float coreField = BuildAuraField(coreUv);
    const float upperPenalty = smoothstep(0.17f, 0.49f, vertical) * gCoreVerticalBias * 4.2f;
    const float2 rootDelta = (float2(coreUv.x, 1.0f - coreUv.y) - gMetaballs[0].xy)
        * float2(gBillboardAspect, 1.0f);
    const float rootRadius = max(gMetaballs[0].z * 0.55f, 0.001f);
    // The heat source follows the animated root and is eroded by the same
    // noise. It keeps a small live coal when all of the rising lobes separate.
    const float rootHeat = exp(-dot(rootDelta, rootDelta) / (rootRadius * rootRadius))
        * gMetaballs[0].w * 4.5f * gFieldGain;
    const float shapedCore = max(coreField - upperPenalty, rootHeat) + ((coreNoise - 0.5f) * 2.8f
        + (fineCoreNoise - 0.5f) * 1.5f) * gCoreBreakup;
    const float coreAa = max(fwidth(shapedCore), gCoreSoftness);
    const float coreMask = smoothstep(gCoreThreshold - coreAa, gCoreThreshold + coreAa, shapedCore)
        * filledMask;
    const float hotThreshold = max(gCoreHotThreshold, gCoreThreshold + gCoreSoftness);
    const float coreHotMask = smoothstep(hotThreshold - coreAa, hotThreshold + coreAa, shapedCore)
        * coreMask;

    // Translucent iridescent tongues take over above the white heat. Their
    // palette travels across the shape so several colors coexist each frame.
    // Bias the density, not opacity: a vertical alpha ramp would leave a
    // stationary horizontal band across the moving flame.
    const float ribbonField = field + (coreNoise - 0.50f) * 4.2f + (fineCoreNoise - 0.5f) * 1.6f
        - (1.0f - smoothstep(0.32f, 0.65f, vertical)) * 3.0f;
    const float ribbonThreshold = gContourThreshold + 0.45f;
    const float ribbonAa = max(fwidth(ribbonField), 0.05f);
    const float ribbonMask = smoothstep(ribbonThreshold - ribbonAa, ribbonThreshold + ribbonAa, ribbonField)
        * filledMask;
    const float huePhase = 0.12f + vertical * 1.55f + (p.x - 0.5f) * 0.72f
        + (contourNoise - 0.5f) * 0.32f - gTime * 0.065f;
    const float3 rainbow = HsvToRgb(float3(frac(huePhase), 0.98f, 1.0f));
    const float ribbonHue = 0.13f + (vertical - 0.45f) * 0.70f + (p.x - 0.5f) * 0.32f
        + 0.06f * sin(gTime * 0.37f + coreNoise * 2.0f);
    const float3 ribbonColor = HsvToRgb(float3(frac(ribbonHue), 0.70f, 1.0f));

    float3 sparks;
    float sparkCoverage;
    BuildStarSparkLayer(p, pixelSize, sparks, sparkCoverage);

    if (gDisplayMode == 1u)
    {
        const float fieldView = field / (field + max(gContourThreshold, 0.001f));
        output.color = float4(fieldView.xxx, 1.0f);
    }
    else if (gDisplayMode != 0u)
    {
        float mask = contourMask;
        if (gDisplayMode == 2u) mask = filledMask;
        if (gDisplayMode == 4u) mask = coreMask;
        if (gDisplayMode == 5u) mask = coreHotMask;
        if (mask < 0.001f) discard;
        output.color = float4(1.0f.xxx, mask * gColor.a);
    }
    else
    {
        // Keep the cyan transition narrow: it fringes the white heat instead
        // of filling the whole interior with an opaque pale oval.
        const float heat = smoothstep(0.0f, 0.38f, coreHotMask);
        const float3 heatColor = lerp(gCoreCyanTint.rgb, float3(1.0f, 0.98f, 0.88f), heat);
        const float3 coreRadiance = heatColor * coreMask * gCoreIntensity * lerp(0.20f, 1.0f, heat);
        const float glint = pow(saturate(contourNoise * 1.3f), 4.0f);
        const float3 radiance = (rainbow * contourMask * gContourEmissiveIntensity
            + lerp(rainbow, 1.0f.xxx, 0.22f) * innerLineMask * gInnerLineIntensity * (0.35f + glint)
            + rainbow * outerGlowMask * gOuterGlowIntensity * 0.40f
            + ribbonColor * ribbonMask * min(gContourEmissiveIntensity * 0.30f, 1.6f)
            + coreRadiance + sparks) * gColor.rgb;
        const float coverage = saturate(max(max(contourMask, outerGlowMask * 0.08f),
            max(max(coreMask, ribbonMask * 0.86f), sparkCoverage)));
        if (coverage < 0.001f) discard;
        // Straight alpha PSO: encode accumulated radiance once. Coverage
        // attenuates the background, not the already-masked HDR emission.
        output.color = float4(radiance / max(coverage, 0.001f), coverage * gColor.a);
    }
    output.normal = float4(0.5f, 0.5f, 1.0f, 0.0f);
    output.material = float4(1.0f, 0.0f, 1.0f, 0.0f);
    return output;
}
