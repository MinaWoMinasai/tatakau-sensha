cbuffer FlameParameters : register(b0)
{
    float4 gColor;
    float gTime;
    float gNoiseScale;
    float gNoiseSpeed;
    float gDistortionStrength;
    float gFieldThreshold;
    float gEdgeSoftness;
    float gIsoBandWidth;
    float gEmissiveIntensity;
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
    const float antiAlias = max(fwidth(field), 0.001f);
    const float maskSoftness = max(gEdgeSoftness, antiAlias);
    const float filledMask = smoothstep(
        gFieldThreshold - maskSoftness,
        gFieldThreshold + maskSoftness,
        field);
    const float distanceToIso = abs(field - gFieldThreshold);
    const float isoBand = 1.0f - smoothstep(
        max(gIsoBandWidth - antiAlias, 0.0f),
        gIsoBandWidth + antiAlias,
        distanceToIso);

    if (gDisplayMode == 1u)
    {
        // A bounded visualization that preserves useful contrast both below
        // and above the selected iso-value.
        const float fieldView = field / (field + max(gFieldThreshold, 0.001f));
        output.color = float4(fieldView.xxx, 1.0f);
    }
    else
    {
        const float selectedMask = gDisplayMode == 3u ? isoBand : filledMask;
        if (selectedMask <= 0.001f)
        {
            discard;
        }

        const float3 displayColor = gDisplayMode == 0u
            ? gColor.rgb * gEmissiveIntensity
            : selectedMask.xxx;
        output.color = float4(displayColor, selectedMask * gColor.a);
    }
    output.normal = float4(0.5f, 0.5f, 1.0f, 0.0f);
    output.material = float4(1.0f, 0.0f, 1.0f, 0.0f);
    return output;
}
