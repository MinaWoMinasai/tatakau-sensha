#ifndef POST_EFFECT_COMMON_HLSLI
#define POST_EFFECT_COMMON_HLSLI

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
    float depthBufferPadding;
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
    float randomIntensity;
    float randomScale;
    float randomTimeScale;
    float randomGrayscalePreview;
    float exposure;
    float toneMappingMode;
    float hdrWhitePoint;
    float hdrPadding;
    float renderDebugMode;
    float linearDepthDebugRange;
    float depthNormalScale;
    float depthFogEnabled;
    float3 depthFogColor;
    float depthFogStart;
    float depthFogEnd;
    float depthFogDensity;
    float depthFogMaxOpacity;
    float renderDebugPadding;
    float ssaoEnabled;
    float ssaoRadius;
    float ssaoIntensity;
    float ssaoBias;
    float ssaoPower;
    float ssaoSampleCount;
    float ssaoNormalInfluence;
    float ssaoDistanceFalloff;
    float4x4 ssrViewMatrix;
    float2 ssrProjectionScale;
    float ssrEnabled;
    float ssrIntensity;
    float ssrMaxDistance;
    float ssrThickness;
    float ssrStepCount;
    float ssrStride;
    float ssrFresnelPower;
    float ssrEdgeFade;
    float ssrDepthFade;
    float ssrNormalFade;
    float ssrMaskPower;
    float ssrBlurRadius;
    float ssrDenoiseEnabled;
    float ssrDenoiseRadius;
    float ssrDenoiseDepthSigma;
    float ssrDenoiseNormalSigma;
    float ssaoDenoiseEnabled;
    float ssaoDenoiseRadius;
    float ssaoDenoiseDepthSigma;
    float ssaoDenoiseNormalSigma;
    float2 motionMatrixPadding;
    float4x4 motionCurrentViewProjection;
    float4x4 motionPreviousViewProjection;
    float4x4 motionInverseCurrentViewProjection;
    float motionVectorEnabled;
    float motionVectorScale;
    float motionVectorDebugScale;
    float motionVectorPadding;
    float temporalEnabled;
    float temporalHistoryValid;
    float temporalBlendFactor;
    float temporalMotionRejection;
    float2 temporalJitter;
    float2 temporalPreviousJitter;
    float temporalJitterEnabled;
    float temporalJitterScale;
    float2 temporalJitterPadding;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float Hash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float RestoreViewSpaceZ(float depth)
{
    return depthNearClip * depthFarClip /
        max(depthFarClip - depth * (depthFarClip - depthNearClip), 0.0001f);
}

float3 DecodeNormalTarget(float4 encodedNormal)
{
    return normalize(encodedNormal.xyz * 2.0f - 1.0f);
}

float3 ACESFilm(float3 color)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

float3 ReinhardExtended(float3 color)
{
    float whitePoint = max(hdrWhitePoint, 0.001f);
    float whitePointSq = whitePoint * whitePoint;
    return saturate((color * (1.0f + color / whitePointSq)) / (1.0f + color));
}

float3 ApplyToneMapping(float3 color)
{
    color = max(color, 0.0f) * max(exposure, 0.0f);
    return toneMappingMode > 0.5f ? ACESFilm(color) : ReinhardExtended(color);
}

#endif
