#include "OceanCommon.hlsli"

TextureCube<float4> gEnvironmentMap : register(t0);
Texture2D<float4> gFFTDisplacementLarge : register(t1);
Texture2D<float2> gFFTSlopeLarge : register(t2);
Texture2D<float4> gFFTDisplacementMedium : register(t3);
Texture2D<float2> gFFTSlopeMedium : register(t4);
Texture2D<float4> gFFTDisplacementSmall : register(t5);
Texture2D<float2> gFFTSlopeSmall : register(t6);
Texture2D<float4> gFFTInitialSpectrum : register(t7);
Texture2D<float4> gFFTEvolvedSpectrum : register(t8);
Texture2D<float4> gFFTSpectrumDebug : register(t9);
SamplerState gEnvironmentSampler : register(s0);

struct OceanPixelOutput
{
    float4 color : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 material : SV_TARGET2;
};

OceanPixelOutput main(OceanVertexOutput input)
{
    OceanPixelOutput output;

    float3 V = normalize(gCameraPosition - input.worldPosition);
    float viewDistance = length(gCameraPosition - input.worldPosition);
    float2 worldXZ = input.worldPosition.xz;
    float2 flowDir = normalize(gWindDirection + float2(0.0001f, 0.0001f));
    float2 crossDir = float2(flowDir.y, -flowDir.x);
    float flow = dot(worldXZ, flowDir);
    float cross = dot(worldXZ, crossDir);
    float time = gTime * max(gWindSpeed, 0.01f) / 12.0f;
    float oceanMode = step(0.5f, gMode);
    float arcBlancMode = step(1.5f, gMode);

    float sunPathControl = saturate(gSunPathEnabled);
    float atmosphereControl = saturate(gAtmosphereEnabled) * max(gAtmosphereStrength, 0.0f);
    float farDetailFade = 1.0f - smoothstep(175.0f, 640.0f, viewDistance);
    float foregroundDetail = 1.0f - smoothstep(70.0f, 260.0f, viewDistance);
    float distantCalm = smoothstep(155.0f, 520.0f, viewDistance);

    float warpA = sin(flow * 0.031f + cross * 0.047f + time * 0.42f);
    float warpB = sin(flow * -0.044f + cross * 0.026f - time * 0.31f);
    float warpC = sin(flow * 0.012f - cross * 0.019f +
        sin(cross * 0.006f) * 2.5f + time * 0.11f);
    float warpedFlow = flow + warpA * 8.5f + warpB * 5.0f + warpC * 14.0f;
    float warpedCross = cross + warpA * 4.0f - warpB * 6.5f + warpC * 7.5f;

    float largePatchNoise = frac(sin(dot(floor(worldXZ * 0.032f), float2(12.9898f, 78.233f))) * 43758.5453f);
    float midPatchNoise = frac(sin(dot(floor(worldXZ * 0.115f), float2(39.346f, 11.135f))) * 24634.6345f);
    float finePatchNoise = frac(sin(dot(
        floor(worldXZ * 0.215f + float2(time * 0.07f, -time * 0.04f)),
        float2(17.73f, 61.19f))) * 32145.124f);
    float patchBreakup = saturate(
        (0.44f + largePatchNoise * 0.30f + midPatchNoise * 0.17f + finePatchNoise * 0.14f) *
        0.86f + 0.19f);
    float softPatchGate = smoothstep(0.44f, 0.88f, patchBreakup);

    float longFlow =
        sin(warpedFlow * 0.050f + time * 0.72f) * 0.46f +
        sin(warpedFlow * 0.083f + warpedCross * 0.019f - time * 1.05f) * 0.24f;
    float arcWind = dot(worldXZ, normalize(float2(0.18f, 0.98f)));
    float arcCross = dot(worldXZ, normalize(float2(0.98f, -0.18f)));
    float arcSwell =
        sin(arcWind * 0.115f + sin(arcCross * 0.018f + time * 0.21f) * 1.2f + time * 1.12f) * 0.22f +
        sin(arcWind * 0.185f + arcCross * 0.032f - time * 1.55f) * 0.11f;
    float arcLongSwell =
        sin(arcWind * 0.0155f + sin(arcCross * 0.0065f + time * 0.055f) * 1.55f + time * 0.30f) * 0.24f +
        sin(arcCross * 0.0260f - sin(arcWind * 0.0080f - time * 0.045f) * 1.10f - time * 0.42f) * 0.13f;
    float capillary =
        sin(warpedFlow * 0.33f + warpedCross * 0.13f + time * 1.85f) * 0.040f +
        sin(warpedFlow * 0.58f - warpedCross * 0.19f - time * 2.25f) * 0.028f;
    float curl = sin(warpedFlow * 0.13f +
        sin(warpedCross * 0.075f + time * 0.44f) * 2.8f + time * 0.88f);
    float wave = longFlow + capillary + arcSwell + arcLongSwell * 0.72f;

    float microA = sin(warpedFlow * 0.78f + warpedCross * 0.34f + time * 2.60f);
    float microB = sin(warpedFlow * -0.56f + warpedCross * 0.61f - time * 2.10f);
    float microC = sin(warpedFlow * 1.08f - warpedCross * 0.24f + time * 3.40f);
    float rippleA = sin(dot(worldXZ, float2(0.74f, 0.21f)) * 0.48f + time * 2.15f + warpA * 0.72f);
    float rippleB = sin(dot(worldXZ, float2(-0.33f, 0.91f)) * 0.64f - time * 1.72f + warpB * 0.54f);
    float rippleC = sin(dot(worldXZ, float2(0.94f, -0.42f)) * 0.92f + time * 2.95f + curl * 0.38f);
    float rippleD = sin(dot(worldXZ, float2(-0.68f, -0.37f)) * 1.24f - time * 3.45f + finePatchNoise * 4.0f);
    float2 microSlope = float2(
        microA * 0.035f + microB * -0.026f + microC * 0.014f,
        microA * 0.012f + microB * 0.032f + microC * -0.019f);
    microSlope += float2(
        rippleA * 0.030f + rippleB * -0.024f + rippleC * 0.016f + rippleD * -0.009f,
        rippleA * 0.010f + rippleB * 0.028f + rippleC * -0.018f + rippleD * 0.014f) *
        (0.70f + foregroundDetail * 0.46f) * (0.72f + patchBreakup * 0.28f);

    float crestletA = sin(dot(worldXZ, float2(0.98f, 0.18f)) * 1.18f + time * 4.65f + warpA * 1.25f);
    float crestletB = sin(dot(worldXZ, float2(-0.42f, 0.91f)) * 1.54f - time * 4.05f + warpB * 1.10f);
    float crestletC = sin(dot(worldXZ, float2(0.24f, 0.97f)) * 2.08f + time * 5.70f + curl * 0.55f);
    float crestletD = sin(arcWind * 1.72f + sin(arcCross * 0.26f + time * 0.64f) * 1.9f + time * 4.20f);
    float crestletE = sin(arcWind * 2.35f - arcCross * 0.42f - time * 5.35f + warpC * 0.80f);
    float spectrumA = sin(dot(worldXZ, normalize(float2(0.07f, 0.997f))) * 3.15f + time * 6.2f + warpA * 1.6f);
    float spectrumB = sin(dot(worldXZ, normalize(float2(0.64f, 0.77f))) * 4.45f - time * 7.1f + warpB * 1.2f);
    float spectrumC = sin(dot(worldXZ, normalize(float2(-0.39f, 0.92f))) * 5.80f + time * 8.4f + finePatchNoise * 3.4f);
    float2 arcFineSlope = float2(
        crestletA * 0.020f + crestletB * -0.016f + crestletC * 0.010f + crestletD * 0.018f + crestletE * -0.012f,
        crestletA * 0.007f + crestletB * 0.019f + crestletC * -0.014f + crestletD * 0.020f + crestletE * 0.010f);
    float2 spectrumSlope = float2(
        spectrumA * 0.009f + spectrumB * -0.007f + spectrumC * 0.005f,
        spectrumA * 0.004f + spectrumB * 0.008f + spectrumC * -0.006f);
    microSlope += arcFineSlope * lerp(foregroundDetail, farDetailFade, 0.34f) *
        (0.52f + softPatchGate * 0.72f);
    microSlope += spectrumSlope * (0.72f + softPatchGate * 0.48f) *
        (0.55f + foregroundDetail * 0.45f);
    microSlope *= 1.18f;
    microSlope *= 0.68f + lerp(farDetailFade, foregroundDetail, 0.72f) * 0.32f;
    microSlope *= 1.08f - distantCalm * 0.20f;

    float2 fftDebugUv = frac(
        worldXZ / max(gFFTDebugPatchLength, 0.001f));
    float2 normalPerturbation = microSlope;
    if (gWaveSource >= 0.5f)
    {
        float3 displayMask = 1.0f.xxx;
        if (gCascadeDisplayMode > 0.5f)
        {
            uint selectedCascade =
                min((uint)(gCascadeDisplayMode - 0.5f), 2u);
            displayMask = float3(
                selectedCascade == 0u ? 1.0f : 0.0f,
                selectedCascade == 1u ? 1.0f : 0.0f,
                selectedCascade == 2u ? 1.0f : 0.0f);
        }
        float3 displacementWeights =
            gCascadeEnabled.xyz *
            gCascadeDisplacementContributions.xyz *
            displayMask;
        float3 slopeWeights =
            gCascadeEnabled.xyz *
            gCascadeSlopeContributions.xyz *
            displayMask;
        float2 uvLarge = frac(
            worldXZ / max(gCascadePatchLengths.x, 0.001f));
        float2 uvMedium = frac(
            worldXZ / max(gCascadePatchLengths.y, 0.001f));
        float2 uvSmall = frac(
            worldXZ / max(gCascadePatchLengths.z, 0.001f));
        float4 displacementLarge =
            gFFTDisplacementLarge.SampleLevel(
                gEnvironmentSampler,
                uvLarge,
                0.0f);
        float4 displacementMedium =
            gFFTDisplacementMedium.SampleLevel(
                gEnvironmentSampler,
                uvMedium,
                0.0f);
        float4 displacementSmall =
            gFFTDisplacementSmall.SampleLevel(
                gEnvironmentSampler,
                uvSmall,
                0.0f);
        float2 combinedSlope =
            gFFTSlopeLarge.SampleLevel(
                gEnvironmentSampler,
                uvLarge,
                0.0f) * slopeWeights.x +
            gFFTSlopeMedium.SampleLevel(
                gEnvironmentSampler,
                uvMedium,
                0.0f) * slopeWeights.y +
            gFFTSlopeSmall.SampleLevel(
                gEnvironmentSampler,
                uvSmall,
                0.0f) * slopeWeights.z;
        normalPerturbation = -combinedSlope;
        wave =
            displacementLarge.y * displacementWeights.x +
            displacementMedium.y * displacementWeights.y +
            displacementSmall.y * displacementWeights.z;
    }
    float3 N = normalize(
        input.normal + float3(normalPerturbation.x, 0.0f, normalPerturbation.y));
    float NdotV = saturate(dot(N, V));
    float fresnel = pow(1.0f - NdotV, 4.2f);
    float lowAngleFresnel = pow(1.0f - NdotV, 6.0f);
    float facing = saturate(N.y);
    float roughness = lerp(0.12f, 0.18f, distantCalm);
    output.normal = EncodeOceanNormal(N, saturate(0.82f + fresnel * 0.18f));
    output.material = EncodeOceanMaterial(roughness, 0.75f);

    float3 reflectVector = reflect(-V, N);
    float3 skySharp = gEnvironmentMap.SampleLevel(gEnvironmentSampler, reflectVector, 0.0f).rgb;
    float3 skySoft = gEnvironmentMap.SampleLevel(gEnvironmentSampler, reflectVector, 5.2f).rgb;
    float sharpWeight = saturate(lowAngleFresnel * 0.54f + fresnel * 0.12f);
    sharpWeight *= 0.48f + foregroundDetail * 0.38f;
    float3 skyReflection = lerp(skySoft, skySharp, sharpWeight);
    float reflectionLuma = dot(skyReflection, float3(0.299f, 0.587f, 0.114f));
    skyReflection = lerp(
        skyReflection,
        skyReflection * (0.54f + reflectionLuma * 0.18f),
        smoothstep(0.52f, 0.92f, reflectionLuma));

    float depthFade = saturate(viewDistance / 260.0f);
    float3 shallowBlue = float3(0.060f, 0.170f, 0.240f);
    float3 clearBlue = float3(0.010f, 0.055f, 0.115f);
    float3 deepBlue = float3(0.001f, 0.014f, 0.038f);
    float3 waterColor = lerp(shallowBlue, clearBlue, saturate(depthFade * 1.6f));
    waterColor = lerp(waterColor, deepBlue, depthFade * 0.82f);
    float trough = smoothstep(0.12f, 0.74f, -wave);
    waterColor = lerp(waterColor, deepBlue, trough * 0.08f);
    waterColor += float3(0.002f, 0.026f, 0.058f) * wave * 0.62f;

    float crest = smoothstep(0.86f, 1.18f, longFlow + arcSwell + curl * 0.13f);
    float foamThreads = smoothstep(
        0.988f,
        1.0f,
        sin(warpedFlow * 0.245f - warpedCross * 0.063f + curl * 0.55f + time * 1.18f));
    float arcCrestThread = smoothstep(
        0.982f,
        1.0f,
        crestletA * 0.36f + crestletB * 0.18f + crestletC * 0.12f +
        crestletD * 0.48f + crestletE * 0.28f);
    float foam = saturate(
        (crest * 0.002f + foamThreads * crest * 0.0035f) *
        farDetailFade * (0.35f + facing * 0.65f) * softPatchGate +
        arcCrestThread * crest * farDetailFade *
        (0.38f + foregroundDetail * 0.62f) * 0.052f);
    foam *= 1.0f - step(0.5f, gWaveSource);
    float spectrumSheen = smoothstep(
        0.965f,
        1.0f,
        spectrumA * 0.30f + spectrumB * 0.24f + spectrumC * 0.20f + crestletD * 0.26f);

    float3 L = normalize(-gSunDirection);
    float3 H = normalize(V + L);
    float NdotL = saturate(dot(N, L));
    float D = OceanDistributionGGX(N, H, roughness);
    float G = OceanGeometrySmith(N, V, L, roughness);
    float3 F = OceanFresnelSchlick(saturate(dot(H, V)), float3(0.020f, 0.020f, 0.020f));
    float3 physicalSunSpecular =
        (D * G * F / max(4.0f * NdotV * NdotL, 0.001f)) *
        NdotL * gSunColor * gSunIntensity * gSunSpecularStrength;

    float3 sunReflect = reflect(-L, N);
    float sunAlignment = saturate(dot(sunReflect, V));
    float sunBroad = pow(sunAlignment, 10.0f);
    float sunSpec = pow(sunAlignment, 180.0f);
    float sunLaneCore = pow(sunAlignment, 260.0f);
    float sunNeedle = pow(sunAlignment, 780.0f);
    float sunLaneSpec = pow(sunAlignment, 72.0f);
    float sunAlignedMask = smoothstep(0.24f, 1.0f, sunLaneSpec);
    float streakMask = smoothstep(0.44f, 0.88f, patchBreakup) *
        (0.48f + finePatchNoise * 0.34f) * (0.32f + sunAlignedMask * 0.68f);

    float sunPathNoise =
        0.74f +
        sin(flow * 0.038f + sin(cross * 0.018f) * 1.1f + time * 0.26f) * 0.13f +
        sin(flow * 0.083f - cross * 0.018f + time * 0.51f) * 0.08f;
    float2 sunPlanar = normalize(L.xz + float2(0.0001f, 0.0001f));
    float2 sunSide = float2(-sunPlanar.y, sunPlanar.x);
    float sunAlong = dot(worldXZ, sunPlanar);
    float sunSideDistance = dot(worldXZ, sunSide);
    float laneCenter = sin(sunAlong * 0.010f + time * 0.055f) * 22.0f;
    float laneWidth = exp(-abs(sunSideDistance - laneCenter) * 0.075f);
    laneWidth *= laneWidth;
    float laneBreakup =
        smoothstep(0.76f, 1.0f, sin(sunAlong * 0.072f + warpA * 1.25f + time * 0.22f)) * 0.32f +
        smoothstep(0.84f, 1.0f, sin(sunAlong * 0.165f - sunSideDistance * 0.038f + time * 0.42f)) * 0.38f +
        smoothstep(0.90f, 1.0f, spectrumSheen * 0.58f + crestletD * 0.28f + crestletE * 0.18f) * 0.30f;
    laneBreakup = saturate(laneBreakup);
    float horizonBoost = smoothstep(25.0f, 210.0f, viewDistance);
    float stableSunLane = laneWidth * laneBreakup *
        (0.18f + horizonBoost * 0.56f) * (0.20f + streakMask * 0.46f);
    float artisticSunLane =
        (sunBroad * 0.115f + sunSpec * 0.50f + sunLaneCore * 1.45f + sunNeedle * 3.05f) *
        saturate(sunPathNoise);
    artisticSunLane += stableSunLane * lerp(0.035f, 0.16f, lowAngleFresnel);
    artisticSunLane *= (0.55f + horizonBoost * 0.55f) *
        (0.30f + streakMask * 0.92f) *
        gSunIntensity * gArtisticSunLaneStrength * sunPathControl;

    float3 reflection = skyReflection *
        (0.090f + fresnel * 0.92f + lowAngleFresnel * 0.24f);
    float reflectionMix = 0.085f + fresnel * 0.58f + lowAngleFresnel * 0.24f;
    float3 color = lerp(waterColor, reflection, saturate(reflectionMix));
    color += float3(0.78f, 0.92f, 1.0f) * foam * 0.090f;
    color += float3(0.78f, 0.92f, 1.0f) * spectrumSheen * farDetailFade * 0.018f;
    color += physicalSunSpecular;
    color += gSunColor * artisticSunLane * 2.55f;
    color = lerp(color, float3(0.20f, 0.34f, 0.40f), depthFade * 0.020f);
    color = lerp(color, float3(0.012f, 0.055f, 0.105f), 0.07f);

    float horizonBlend = smoothstep(165.0f, 640.0f, viewDistance) * atmosphereControl;
    float horizonSilhouetteFade = smoothstep(430.0f, 960.0f, viewDistance) * atmosphereControl;
    float3 horizonWater = lerp(
        float3(0.21f, 0.40f, 0.48f),
        skyReflection * 0.72f + float3(0.02f, 0.045f, 0.065f),
        fresnel);
    horizonWater = lerp(
        horizonWater,
        float3(0.42f, 0.61f, 0.70f),
        smoothstep(0.18f, 1.0f, horizonBlend));
    color = lerp(color, horizonWater, horizonBlend * 0.56f);
    float atmosphere = smoothstep(115.0f, 760.0f, viewDistance) *
        lerp(smoothstep(0.10f, 0.82f, lowAngleFresnel), 0.72f, horizonSilhouetteFade) *
        atmosphereControl;
    float3 horizonSky = skySoft * 0.85f + float3(0.090f, 0.135f, 0.165f);
    float3 farBlueAir = lerp(float3(0.19f, 0.31f, 0.39f), horizonSky, 0.76f);
    farBlueAir = lerp(farBlueAir, float3(0.48f, 0.66f, 0.73f), horizonSilhouetteFade * 0.72f);
    color = lerp(color, farBlueAir, atmosphere * 0.74f);
    color += horizonSky * atmosphere * 0.085f;
    color *= gTint.rgb;

    int fftDebugMode = (int)(gFFTDebugMode + 0.5f);
    if (fftDebugMode > 0)
    {
        float debugScale = max(gFFTDebugScale, 0.001f);
        uint debugCascade = min((uint)(gFFTDebugCascade + 0.5f), 2u);
        float4 displacementDebug;
        float2 slopeDebug;
        if (debugCascade == 1u)
        {
            displacementDebug = gFFTDisplacementMedium.SampleLevel(
                gEnvironmentSampler,
                fftDebugUv,
                0.0f);
            slopeDebug = gFFTSlopeMedium.SampleLevel(
                gEnvironmentSampler,
                fftDebugUv,
                0.0f);
        }
        else if (debugCascade == 2u)
        {
            displacementDebug = gFFTDisplacementSmall.SampleLevel(
                gEnvironmentSampler,
                fftDebugUv,
                0.0f);
            slopeDebug = gFFTSlopeSmall.SampleLevel(
                gEnvironmentSampler,
                fftDebugUv,
                0.0f);
        }
        else
        {
            displacementDebug = gFFTDisplacementLarge.SampleLevel(
                gEnvironmentSampler,
                fftDebugUv,
                0.0f);
            slopeDebug = gFFTSlopeLarge.SampleLevel(
                gEnvironmentSampler,
                fftDebugUv,
                0.0f);
        }
        float4 initialDebug =
            gFFTInitialSpectrum.SampleLevel(
                gEnvironmentSampler,
                fftDebugUv,
                0.0f);
        float4 evolvedDebug =
            gFFTEvolvedSpectrum.SampleLevel(
                gEnvironmentSampler,
                fftDebugUv,
                0.0f);
        float4 spectrumDebug =
            gFFTSpectrumDebug.SampleLevel(
                gEnvironmentSampler,
                fftDebugUv,
                0.0f);
        if (fftDebugMode == 1)
        {
            float radial =
                log2(1.0f + max(spectrumDebug.x, 0.0f) * debugScale);
            color = float3(
                saturate(radial * 0.18f),
                saturate(radial * 0.42f),
                saturate(radial));
        }
        else if (fftDebugMode == 2)
        {
            float directional =
                max(spectrumDebug.y, 0.0f) * debugScale * 6.28318530718f;
            color = float3(
                saturate(directional * 0.22f),
                saturate(directional * 0.72f),
                saturate(directional));
        }
        else if (fftDebugMode == 3)
        {
            color = spectrumDebug.z.xxx;
        }
        else if (fftDebugMode == 4)
        {
            float2 initialPhysical = initialDebug.xy / (128.0f * 128.0f);
            float magnitude =
                log2(1.0f + length(initialPhysical) * debugScale);
            color = float3(
                0.5f + initialPhysical.x * debugScale,
                0.5f + initialPhysical.y * debugScale,
                saturate(magnitude));
        }
        else if (fftDebugMode == 5)
        {
            float2 evolvedPhysical = evolvedDebug.xy / (128.0f * 128.0f);
            float magnitude =
                log2(1.0f + length(evolvedPhysical) * debugScale);
            color = float3(
                0.5f + evolvedPhysical.x * debugScale,
                0.5f + evolvedPhysical.y * debugScale,
                saturate(magnitude));
        }
        else if (fftDebugMode == 6)
        {
            color = (0.5f + displacementDebug.y * debugScale).xxx;
        }
        else if (fftDebugMode == 7)
        {
            color = (0.5f + displacementDebug.x * debugScale).xxx;
        }
        else if (fftDebugMode == 8)
        {
            color = (0.5f + displacementDebug.z * debugScale).xxx;
        }
        else if (fftDebugMode == 9)
        {
            color = float3(
                0.5f + slopeDebug.x * debugScale,
                0.5f + slopeDebug.y * debugScale,
                0.5f);
        }
        else
        {
            color = N * 0.5f + 0.5f;
        }
        output.color = float4(saturate(color), 1.0f);
        return output;
    }

    int debugMode = (int)(gDebugMode + 0.5f);
    if (debugMode == 1)
    {
        color = N * 0.5f + 0.5f;
    }
    else if (debugMode == 2)
    {
        color = fresnel.xxx;
    }
    else if (debugMode == 3)
    {
        float physicalSignal = saturate(dot(physicalSunSpecular, 0.3333f.xxx));
        float artisticSignal = saturate(artisticSunLane * 0.12f);
        color = float3(physicalSignal, artisticSignal, artisticSignal);
    }
    else if (debugMode == 4)
    {
        color = foam.xxx;
    }
    else if (debugMode == 5)
    {
        color = 1.0f.xxx;
    }

    output.color = float4(max(color, 0.0f), gTint.a);
    return output;
}
