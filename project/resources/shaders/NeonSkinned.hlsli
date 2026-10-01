#ifndef NEON_SKINNED_HLSLI
#define NEON_SKINNED_HLSLI

struct NeonSkinnedVertexOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION0;
    float3 worldNormal : NORMAL0;
};

#endif
