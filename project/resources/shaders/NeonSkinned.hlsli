#ifndef NEON_SKINNED_HLSLI
#define NEON_SKINNED_HLSLI

// NeonSkinnedRenderer::GpuConstants (320byte, allocated in a 512byte resource).
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
    float3 gGeometryLineColor;
    float gGeometryLineIntensity;
    uint gGeometryLineEnabled;
    float gGeometryLineWidthPixels;
    float gBodyEmissionIntensity;
    float gBodyPadding;
    float3 gFeatureMaskColor;
    float gFeatureMaskIntensity;
    float gFeatureMaskBlend;
    uint gFeatureMaskDebugMode;
    float2 gFeatureMaskPadding;
    float3 gLineCoreColor;
    float gLineCoreIntensity;
    float3 gLineHaloColor;
    float gLineHaloIntensity;
    float3 gOutlineCoreColor;
    float gOutlineCoreIntensity;
    uint gFeatureMaskRenderMode;
    uint gSplitLineEmission;
    uint gLineDiagnosticMode;
    float gSdfRangeTexels;
    float gSdfHaloWidthTexels;
    float gSdfLodBlendStart;
    float gSdfLodBlendEnd;
    float gQualityPadding;
    float3 gDissolveDirection;
    float gDissolveScanMin;
    float gDissolveScanMax;
    float gDissolveProgress;
    float gDissolveNoiseStrength;
    float gDissolveNoiseScale;
    uint gDissolveEnabled;
    uint gDissolveEdgeEnabled;
    uint gDissolveSeed;
    uint gDissolvePadding;
    float3 gDissolveEdgeColor;
    float gDissolveEdgeIntensity;
    float gDissolveEdgeWidth;
    float3 gDissolveEdgePadding;
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
    float3 skinnedModelPosition : TEXCOORD1;
};

#endif
