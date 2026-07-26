#ifndef OCEAN_COMMON_HLSLI
#define OCEAN_COMMON_HLSLI

cbuffer OceanParameters : register(b0)
{
    float4x4 gViewProjection;
    float4x4 gInverseViewProjection;
    float4 gTint;
    float3 gCameraPosition;
    float gTime;
    float3 gGridOrigin;
    float gBaseHeight;
    float2 gWindDirection;
    float gWindSpeed;
    float gChoppiness;
    float3 gSunDirection;
    float gSunIntensity;
    float3 gSunColor;
    float gSunSpecularStrength;
    float gArtisticSunLaneStrength;
    float gAtmosphereStrength;
    float gFarFlattenStrength;
    float gDebugMode;
    float gMode;
    float gDiagnosticsEnabled;
    float gSunPathEnabled;
    float gAtmosphereEnabled;
    float gFarFlattenEnabled;
    float gProceduralCloudReflectionEnabled;
    float2 gPadding;
    float gWaveSource;
    float gFFTPatchLength;
    float gFFTDebugMode;
    float gFFTDebugScale;
    float4 gCascadePatchLengths;
    float4 gCascadeDisplacementContributions;
    float4 gCascadeSlopeContributions;
    float4 gCascadeEnabled;
    float gCascadeDisplayMode;
    float gFFTDebugPatchLength;
    float gFFTDebugCascade;
    float gCascadePadding;
    float gMeshMode;
    float gProjectedNearClamp;
    float gProjectedFarClamp;
    float gProjectedHorizonNdcY;
    float2 gProjectedGridResolution;
    float gProjectedGridDebug;
    float gProjectedWireframe;
    // x/top/bottom overscan and an NDC margin for TAA jitter.
    float4 gProjectedOverscan;
    // near fade enabled, fade width, minimum safe distance, outer wireframe.
    float4 gProjectedDisplacementGuard;
};

struct OceanVertexInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 tangent : TANGENT0;
};

struct OceanVertexOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
    noperspective float2 projectedGridCoord : TEXCOORD1;
    float projectedDistance : TEXCOORD2;
    noperspective float2 projectedNdc : TEXCOORD3;
    noperspective float guardBandFade : TEXCOORD4;
    noperspective float nearDisplacementFade : TEXCOORD5;
};

float OceanDistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = saturate(dot(N, H));
    float denominator = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(3.14159265f * denominator * denominator, 0.00001f);
}

float OceanGeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) * 0.125f;
    return NdotV / max(NdotV * (1.0f - k) + k, 0.00001f);
}

float OceanGeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    return OceanGeometrySchlickGGX(saturate(dot(N, V)), roughness) *
        OceanGeometrySchlickGGX(saturate(dot(N, L)), roughness);
}

float3 OceanFresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - saturate(cosTheta), 5.0f);
}

float4 EncodeOceanNormal(float3 normal, float ssrMask)
{
    return float4(normalize(normal) * 0.5f + 0.5f, saturate(ssrMask));
}

float4 EncodeOceanMaterial(float roughness, float materialClass)
{
    return float4(saturate(roughness), 0.0f, 1.0f, saturate(materialClass));
}

#endif
