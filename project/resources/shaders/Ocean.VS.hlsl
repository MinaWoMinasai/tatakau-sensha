#include "OceanCommon.hlsli"

Texture2D<float4> gFFTDisplacementLarge : register(t1);
Texture2D<float4> gFFTDisplacementMedium : register(t3);
Texture2D<float4> gFFTDisplacementSmall : register(t5);
SamplerState gOceanSampler : register(s0);

struct ProjectedOceanPosition
{
    float2 worldXZ;
    float2 ndc;
    float distance;
    float guardBandFade;
    float nearFade;
};

float ProjectedGuardAxisFade(float magnitude, float outerLimit)
{
    float safeLimit = max(outerLimit, 1.0001f);
    float t = saturate(
        (safeLimit - magnitude) / max(safeLimit - 1.0f, 0.0001f));
    return magnitude <= 1.0f ? 1.0f : t * t * (3.0f - 2.0f * t);
}

ProjectedOceanPosition ProjectOceanGridToWorldXZ(float2 gridNdc)
{
    ProjectedOceanPosition result;
    float jitterMargin = max(gProjectedOverscan.w, 0.0f);
    float overscanX = max(gProjectedOverscan.x + jitterMargin, 1.0001f);
    float overscanTop = max(gProjectedOverscan.y + jitterMargin, 1.0001f);
    float overscanBottom = max(gProjectedOverscan.z + jitterMargin, 1.0001f);
    float verticalT = gridNdc.y * 0.5f + 0.5f;
    result.ndc = float2(
        gridNdc.x * overscanX,
        lerp(-overscanBottom, overscanTop, verticalT));

    float fadeX = ProjectedGuardAxisFade(abs(result.ndc.x), overscanX);
    float fadeTop = ProjectedGuardAxisFade(max(result.ndc.y, 0.0f), overscanTop);
    float fadeBottom = ProjectedGuardAxisFade(max(-result.ndc.y, 0.0f), overscanBottom);
    result.guardBandFade = min(fadeX, min(fadeTop, fadeBottom));

    float2 projectionNdc = float2(
        result.ndc.x,
        min(result.ndc.y, gProjectedHorizonNdcY));
    float4 nearPosition = mul(
        float4(projectionNdc, 0.0f, 1.0f),
        gInverseViewProjection);
    float4 farPosition = mul(
        float4(projectionNdc, 1.0f, 1.0f),
        gInverseViewProjection);
    float safeNearW =
        abs(nearPosition.w) > 1.0e-6f ? nearPosition.w : 1.0e-6f;
    float safeFarW =
        abs(farPosition.w) > 1.0e-6f ? farPosition.w : 1.0e-6f;
    nearPosition.xyz /= safeNearW;
    farPosition.xyz /= safeFarW;

    float3 rayVector = farPosition.xyz - nearPosition.xyz;
    float rayLength = length(rayVector);
    float3 rayDirection =
        rayLength > 1.0e-6f
        ? rayVector / rayLength
        : float3(0.0f, -1.0f, 0.0f);
    float horizontalLength = length(rayDirection.xz);
    float safeRayY =
        abs(rayDirection.y) > 1.0e-5f ? rayDirection.y : -1.0e-5f;
    float planeT =
        (gBaseHeight - gCameraPosition.y) / safeRayY;
    float planeDistance =
        planeT > 0.0f && horizontalLength > 1.0e-5f
        ? planeT * horizontalLength
        : gProjectedFarClamp;
    float clampedDistance = clamp(
        planeDistance,
        gProjectedNearClamp,
        gProjectedFarClamp);
    float2 horizontalDirection =
        horizontalLength > 1.0e-5f
        ? rayDirection.xz / horizontalLength
        : float2(0.0f, 1.0f);
    result.worldXZ =
        gCameraPosition.xz + horizontalDirection * clampedDistance;
    result.distance = clampedDistance;
    float nearFadeWidth = max(gProjectedDisplacementGuard.y, 0.001f);
    float minimumSafeDistance = max(
        gProjectedDisplacementGuard.z,
        gProjectedNearClamp);
    float nearFade = smoothstep(
        minimumSafeDistance,
        minimumSafeDistance + nearFadeWidth,
        clampedDistance);
    result.nearFade = lerp(
        1.0f,
        nearFade,
        saturate(gProjectedDisplacementGuard.x));
    return result;
}

OceanVertexOutput main(OceanVertexInput input)
{
    OceanVertexOutput output;

    bool projectedGrid = gMeshMode >= 0.5f;
    ProjectedOceanPosition projectedPosition;
    projectedPosition.worldXZ = input.position.xz + gGridOrigin.xz;
    projectedPosition.ndc = input.position.xz;
    projectedPosition.distance = length(
        projectedPosition.worldXZ - gCameraPosition.xz);
    projectedPosition.guardBandFade = 1.0f;
    projectedPosition.nearFade = 1.0f;
    if (projectedGrid)
    {
        projectedPosition = ProjectOceanGridToWorldXZ(input.position.xz);
    }
    float2 worldXZ = projectedPosition.worldXZ;
    float projectedDisplacementFade =
        projectedPosition.guardBandFade * projectedPosition.nearFade;
    float2 p = worldXZ;
    float time = gTime * max(gWindSpeed, 0.01f) / 12.0f;
    float arcBlancMode = step(1.5f, gMode);
    float oceanMode = step(0.5f, gMode);

    float2 dirA = normalize(gWindDirection + float2(0.0001f, 0.0001f));
    float2 dirB = normalize(float2(-0.42f, 0.91f));
    float2 dirC = normalize(float2(0.72f, 0.69f));
    float2 dirD = normalize(float2(-0.88f, 0.47f));
    float2 dirE = normalize(float2(0.36f, 0.93f));
    float2 dirF = normalize(float2(0.08f, 0.997f));
    float2 dirG = normalize(float2(-0.31f, 0.95f));

    float flow = dot(p, dirA) * 0.030f + time * 0.86f;
    float cross = dot(p, float2(dirA.y, -dirA.x)) * 0.095f;
    float phaseA = dot(p, dirA) * 0.040f + time * 0.92f;
    float phaseB = dot(p, dirB) * 0.078f - time * 1.34f;
    float phaseC = dot(p, dirC) * 0.150f + time * 2.05f;
    float phaseD = flow + sin(cross + time * 0.35f) * 0.55f;
    float phaseE = dot(p, dirD) * 0.245f - time * 2.90f;
    float phaseF = dot(p, dirE) * 0.360f + time * 3.70f +
        sin(dot(p, dirB) * 0.018f + time * 0.28f) * 0.85f;
    float phaseG = dot(p, dirF) * 0.0155f +
        sin(dot(p, dirG) * 0.0065f + time * 0.055f) * 1.55f + time * 0.30f;
    float phaseH = dot(p, dirG) * 0.0260f -
        sin(dot(p, dirF) * 0.0080f - time * 0.045f) * 1.10f - time * 0.42f;

    float spectrumPatch = 0.72f + 0.28f *
        sin(dot(p, normalize(float2(0.53f, 0.85f))) * 0.010f + time * 0.16f);
    float swellPatch = 0.68f + 0.32f *
        sin(dot(p, normalize(float2(-0.18f, 0.98f))) * 0.0042f + time * 0.035f);

    float displacement =
        sin(phaseA) * lerp(0.46f, 0.34f, arcBlancMode) +
        sin(phaseB) * lerp(0.20f, 0.16f, arcBlancMode) +
        sin(phaseC) * lerp(0.08f, 0.11f, arcBlancMode) +
        sin(phaseD) * lerp(0.12f, 0.09f, arcBlancMode);
    displacement += arcBlancMode * spectrumPatch *
        (sin(phaseE) * 0.070f + sin(phaseF) * 0.044f);
    displacement += arcBlancMode * swellPatch *
        (sin(phaseG) * 0.165f + sin(phaseH) * 0.095f);

    float2 grad;
    grad.x =
        cos(phaseA) * 0.34f * 0.040f * dirA.x +
        cos(phaseB) * 0.16f * 0.078f * dirB.x +
        cos(phaseC) * 0.11f * 0.150f * dirC.x +
        cos(phaseD) * 0.09f * cos(cross + time * 0.35f) * 0.095f * 0.55f;
    grad.y =
        cos(phaseA) * 0.34f * 0.040f * dirA.y +
        cos(phaseB) * 0.16f * 0.078f * dirB.y +
        cos(phaseC) * 0.11f * 0.150f * dirC.y +
        cos(phaseD) * 0.09f * 0.030f;
    grad += arcBlancMode * spectrumPatch *
        (dirD * cos(phaseE) * 0.052f * 0.245f +
         dirE * cos(phaseF) * 0.030f * 0.360f);
    grad += arcBlancMode * swellPatch *
        (dirF * cos(phaseG) * 0.150f * 0.0155f +
         dirG * cos(phaseH) * 0.080f * 0.0260f);
    grad *= lerp(1.0f, 1.52f, arcBlancMode);

    float viewDistance = length(worldXZ - gCameraPosition.xz);
    float flattenControl = saturate(gFarFlattenEnabled) * saturate(gFarFlattenStrength);
    float farFlatten = lerp(
        1.0f,
        1.0f - smoothstep(360.0f, 900.0f, viewDistance) * 0.58f,
        flattenControl * arcBlancMode);
    displacement *= lerp(0.84f, 1.32f, oceanMode) * farFlatten;
    grad *= farFlatten;

    float2 choppyOffset = -grad * gChoppiness * arcBlancMode;
    float3 worldPosition = float3(
        worldXZ.x + choppyOffset.x * projectedDisplacementFade,
        gBaseHeight + displacement * projectedDisplacementFade,
        worldXZ.y + choppyOffset.y * projectedDisplacementFade);
    if (gWaveSource >= 0.5f)
    {
        float3 displayMask = 1.0f.xxx;
        if (gCascadeDisplayMode > 0.5f)
        {
            uint selectedCascade =
                min((uint)(gCascadeDisplayMode - 0.5f), 2u);
            displayMask = float3(
                selectedCascade == 0u,
                selectedCascade == 1u,
                selectedCascade == 2u);
        }
        float3 displacementWeights =
            gCascadeEnabled.xyz *
            gCascadeDisplacementContributions.xyz *
            displayMask;
        float2 uvLarge = frac(
            worldXZ / max(gCascadePatchLengths.x, 0.001f));
        float2 uvMedium = frac(
            worldXZ / max(gCascadePatchLengths.y, 0.001f));
        float2 uvSmall = frac(
            worldXZ / max(gCascadePatchLengths.z, 0.001f));
        float3 fftDisplacement =
            gFFTDisplacementLarge.SampleLevel(
                gOceanSampler,
                uvLarge,
                0.0f).xyz * displacementWeights.x +
            gFFTDisplacementMedium.SampleLevel(
                gOceanSampler,
                uvMedium,
                0.0f).xyz * displacementWeights.y +
            gFFTDisplacementSmall.SampleLevel(
                gOceanSampler,
                uvSmall,
                0.0f).xyz * displacementWeights.z;
        fftDisplacement *= farFlatten * projectedDisplacementFade;
        worldPosition = float3(
            worldXZ.x + fftDisplacement.x,
            gBaseHeight + fftDisplacement.y,
            worldXZ.y + fftDisplacement.z);
        grad = 0.0f.xx;
    }

    output.position = mul(float4(worldPosition, 1.0f), gViewProjection);
    output.texcoord = input.texcoord;
    output.normal = normalize(float3(-grad.x, 1.0f, -grad.y));
    output.worldPosition = worldPosition;
    output.projectedGridCoord = input.texcoord;
    output.projectedDistance =
        projectedGrid ? projectedPosition.distance : viewDistance;
    output.projectedNdc = projectedPosition.ndc;
    output.guardBandFade = projectedPosition.guardBandFade;
    output.nearDisplacementFade = projectedPosition.nearFade;
    output.undisplacedWorldXZ = worldXZ;
    return output;
}
