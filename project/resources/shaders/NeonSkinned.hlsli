#ifndef NEON_SKINNED_HLSLI
#define NEON_SKINNED_HLSLI

// NeonSkinnedRenderer::GpuConstants (96byte)。Body / OutlineのVS・PSで共有。
cbuffer NeonSkinnedConstants : register(b1)
{
    float4 gBodyColor;
    float3 gEmissiveColor;
    float gEmissiveIntensity;
    float gRimStrength;
    float gRimPower;
    float gOutlineWidthPixels;
    uint gOutlineEnabled;
    uint gInternalLineEnabled;
    float gInternalLineWidthPixels;
    float gInternalLineIntensity;
    float gInternalLineThreshold;
    float3 gCameraWorldPosition;
    float gCameraPadding;
    float2 gViewportSize;
    float2 gViewportPadding;
};

struct NeonSkinnedVertexOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION0;
    float3 worldNormal : NORMAL0;
    float2 texcoord : TEXCOORD0;
};

#endif
