#ifndef CRYSTAL_MATERIAL_HLSLI
#define CRYSTAL_MATERIAL_HLSLI

static const float kCrystalPi = 3.14159265359f;

struct CrystalMaterialResult
{
    float3 surfaceTint;
    float3 reflectionTint;
    float3 emission;
    float fresnel;
    float dielectricF0;
    float edgeWeight;
};

float3 CrystalSafeNormalize(float3 value, float3 fallback)
{
    const float lengthSquared = dot(value, value);
    return lengthSquared > 1.0e-8f
        ? value * rsqrt(lengthSquared)
        : fallback;
}

CrystalMaterialResult EvaluateOpaqueCrystal(
    float3 N,
    float3 V,
    float fresnelPower,
    float iridescenceFactor,
    float iridescenceIor,
    float thicknessMinimumNm,
    float thicknessMaximumNm,
    float edgeEmission,
    float coreEmission,
    float3 coreColor,
    float3 edgeColor)
{
    CrystalMaterialResult result;

    const float3 safeNormal =
        CrystalSafeNormalize(N, float3(0.0f, 1.0f, 0.0f));
    const float3 safeView =
        CrystalSafeNormalize(V, safeNormal);
    const float NdotV = saturate(dot(safeNormal, safeView));
    const float safeIor = clamp(iridescenceIor, 1.001f, 3.0f);
    const float safeFresnelPower = clamp(fresnelPower, 0.25f, 16.0f);
    const float safeThicknessMinimumNm = max(thicknessMinimumNm, 1.0f);
    const float safeThicknessMaximumNm =
        max(thicknessMaximumNm, safeThicknessMinimumNm);

    const float iorDenominator = max(safeIor + 1.0f, 0.0001f);
    const float f0Root = (safeIor - 1.0f) / iorDenominator;
    result.dielectricF0 = saturate(f0Root * f0Root);
    result.fresnel = result.dielectricF0 +
        (1.0f - result.dielectricF0) *
        pow(saturate(1.0f - NdotV), safeFresnelPower);

    // Approximate the refracted film angle with Snell's law. All square-root
    // and divisor inputs are protected so extreme UI values remain finite.
    const float inverseIor = 1.0f / max(safeIor, 0.0001f);
    const float sinThetaTSquared =
        inverseIor * inverseIor * max(1.0f - NdotV * NdotV, 0.0f);
    const float cosThetaT = sqrt(max(1.0f - sinThetaTSquared, 0.0f));

    const float grazing = saturate(1.0f - NdotV);
    const float thicknessBlend = pow(grazing, 1.25f);
    const float thicknessNm = lerp(
        safeThicknessMinimumNm,
        safeThicknessMaximumNm,
        thicknessBlend);
    const float opticalPathNm =
        max(2.0f * safeIor * thicknessNm * cosThetaT, 0.0f);

    // Three representative wavelengths with channel-specific phase offsets
    // give a stable thin-film interference approximation without spectral
    // sampling. The response remains continuous as the view angle changes.
    const float3 wavelengthsNm = float3(650.0f, 510.0f, 475.0f);
    const float3 phaseOffsets = float3(
        0.0f,
        2.0f * kCrystalPi / 3.0f,
        4.0f * kCrystalPi / 3.0f);
    const float3 phase =
        (2.0f * kCrystalPi * opticalPathNm) / wavelengthsNm + phaseOffsets;
    float3 interference = 0.5f + 0.5f * cos(phase);
    const float interferenceAverage =
        dot(interference, float3(0.333333f, 0.333333f, 0.333333f));
    interference = lerp(interferenceAverage.xxx, interference, 0.78f);

    const float3 safeCoreColor = max(coreColor, 0.0f);
    const float3 safeEdgeColor = max(edgeColor, 0.0f);
    result.edgeWeight = saturate(
        smoothstep(0.08f, 0.92f, grazing) *
        (0.58f + result.fresnel * 0.42f));
    result.surfaceTint = lerp(
        safeCoreColor,
        safeEdgeColor,
        result.edgeWeight);

    const float iridescenceWeight =
        saturate(iridescenceFactor) *
        saturate(result.fresnel * 0.90f + result.edgeWeight * 0.48f);
    const float3 filmTint = 0.30f + interference * 1.15f;
    result.reflectionTint = lerp(
        float3(1.0f, 1.0f, 1.0f),
        filmTint,
        iridescenceWeight);

    const float coreMask = 1.0f - result.edgeWeight * 0.82f;
    const float edgeMask =
        result.edgeWeight * (0.35f + result.fresnel * 0.65f);
    result.emission =
        safeCoreColor * max(coreEmission, 0.0f) * coreMask +
        safeEdgeColor * max(edgeEmission, 0.0f) * edgeMask;
    return result;
}

#endif
