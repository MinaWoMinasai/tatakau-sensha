#ifndef NEON_SKINNED_SKINNING_HLSLI
#define NEON_SKINNED_SKINNING_HLSLI
#include "NeonSkinned.hlsli"

// 既存Object3dのTransform CB先頭3行列をそのまま使う。
struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};
struct SkinningPaletteEntry
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
StructuredBuffer<SkinningPaletteEntry> gMatrixPalette : register(t3);
struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 tangent : TANGENT0;
    float4 weight : WEIGHT0;
    int4 index : INDEX0;
};

// BodyとOutlineは同じ4 influence / row-major GPU Skinningを共有する。
NeonSkinnedVertexOutput SkinNeonVertex(VertexShaderInput input)
{
    float4 skinnedPosition = 0.0f;
    float3 skinnedNormal = 0.0f;
    [unroll]
    for (uint influence = 0; influence < 4; ++influence)
    {
        skinnedPosition += mul(input.position,
            gMatrixPalette[input.index[influence]].skeletonSpaceMatrix) * input.weight[influence];
        skinnedNormal += mul(input.normal,
            (float3x3)gMatrixPalette[input.index[influence]].skeletonSpaceInverseTransposeMatrix) * input.weight[influence];
    }
    skinnedPosition.w = 1.0f;
    NeonSkinnedVertexOutput output;
    output.position = mul(skinnedPosition, gTransformationMatrix.WVP);
    output.worldPosition = mul(skinnedPosition, gTransformationMatrix.World).xyz;
    float3 normal = mul(skinnedNormal, (float3x3)gTransformationMatrix.WorldInverseTranspose);
    output.worldNormal = normal * rsqrt(max(dot(normal, normal), 1.0e-8f));
    return output;
}
#endif
